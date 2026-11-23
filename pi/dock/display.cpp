#include "display.h"

#include "link.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
}

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr size_t kLag = 4;     // кадров не по такту в очереди к экрану — не больше стольких (стол сам по себе)
// Кадры по такту приходят раньше своего обновления на опережение (до ~60 мс — до 5 кадров при 75 Гц) и ждут его в
// очереди — их не выбрасывать: было kLag и для них, и при опережении 80 мс Pi выбрасывал больше половины кадров
// перетаскивания окна (прогон стола 19 ноября 2026, docs/PI.md, «Очередь и опережение»). Больше kTimed — зависло
constexpr size_t kTimed = 7;

double rateOf(const drmModeModeInfo &m)
{
    return m.clock * 1000.0 / (double(m.htotal) * m.vtotal);
}

// Имя монитора — из EDID (описатель 0xFC: до 13 знаков, конец — перевод строки); нет — пусто
std::string edidName(const uint8_t *e, size_t n)
{
    for (size_t d = 54; d + 18 <= std::min<size_t>(n, 126); d += 18) {
        if (e[d] || e[d + 1] || e[d + 2] || e[d + 3] != 0xFC)
            continue;
        std::string s;
        for (size_t i = d + 5; i < d + 18 && e[i] != 0x0A; ++i)
            if (e[i] >= 0x20 && e[i] < 0x7F && e[i] != '"' && e[i] != '\\')
                s += char(e[i]);
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        return s;
    }
    return {};
}

} // namespace

uint32_t Display::prop(uint32_t obj, uint32_t type, const char *name, uint64_t *enumValue, const char *enumName)
{
    drmModeObjectProperties *props = drmModeObjectGetProperties(m_fd, obj, type);
    uint32_t id = 0;
    for (uint32_t i = 0; props && i < props->count_props && !id; ++i) {
        drmModePropertyRes *p = drmModeGetProperty(m_fd, props->props[i]);
        if (p && !strcmp(p->name, name)) {
            id = p->prop_id;
            for (int e = 0; enumValue && e < p->count_enums; ++e)
                if (!strcmp(p->enums[e].name, enumName))
                    *enumValue = p->enums[e].value;
        }
        drmModeFreeProperty(p);
    }
    drmModeFreeObjectProperties(props);
    return id;
}

uint32_t Display::dumbFb(int w, int h, uint32_t format, uint32_t **pixels, uint32_t *pitch)
{
    drm_mode_create_dumb c{};
    c.width = uint32_t(w);
    c.height = uint32_t(h);
    c.bpp = 32;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &c))
        return 0;
    drm_mode_map_dumb m{};
    m.handle = c.handle;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &m))
        return 0;
    void *map = mmap(nullptr, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, off_t(m.offset));
    if (map == MAP_FAILED)
        return 0;
    memset(map, 0, c.size);
    *pixels = static_cast<uint32_t *>(map);
    *pitch = c.pitch;
    const uint32_t handles[4] = {c.handle}, pitches[4] = {c.pitch}, offsets[4] = {};
    uint32_t fb = 0;
    drmModeAddFB2(m_fd, uint32_t(w), uint32_t(h), format, handles, pitches, offsets, &fb, 0);
    return fb;
}

// режим родного разрешения с частотой, ближайшей к hz (поровну — выше); hz 0 — самая высокая
size_t Display::pickMode(double hz) const
{
    size_t best = 0;
    for (size_t i = 1; i < m_modes.size(); ++i) {
        const double a = rateOf(m_modes[i]), b = rateOf(m_modes[best]);
        if (hz <= 0 ? a > b + 0.01 : std::fabs(a - hz) < std::fabs(b - hz) - 0.01
                                         || (std::fabs(std::fabs(a - hz) - std::fabs(b - hz)) <= 0.01 && a > b))
            best = i;
    }
    return best;
}

std::vector<double> Display::rates() const
{
    std::vector<double> r;
    for (const drmModeModeInfo &m : m_modes)
        if (std::none_of(r.begin(), r.end(), [&](double v) { return std::fabs(v - rateOf(m)) < 0.01; }))
            r.push_back(rateOf(m));
    std::sort(r.begin(), r.end());
    return r;
}

bool Display::open(double hz)
{
    drmModeRes *res = nullptr;
    for (int i = 0; i < 4 && !res; ++i) {   // карта с выходами (у Pi 5 card0 — v3d без дисплея, card1 — vc4)
        const std::string path = "/dev/dri/card" + std::to_string(i);
        m_fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (m_fd < 0)
            continue;
        res = drmModeGetResources(m_fd);
        if (!res || !res->count_connectors) {
            drmModeFreeResources(res);
            res = nullptr;
            ::close(m_fd);
            m_fd = -1;
        }
    }
    if (!res) {
        fprintf(stderr, "экран: нет карты с выходами\n");
        return false;
    }
    if (drmSetClientCap(m_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) || drmSetClientCap(m_fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
        fprintf(stderr, "экран: atomic KMS нет\n");
        return false;
    }
    drmSetMaster(m_fd);   // первый открывший — уже хозяин; иначе нужен root

    drmModeModeInfo mode{};
    int crtcIndex = -1;
    for (int i = 0; i < res->count_connectors && !m_conn; ++i) {
        drmModeConnector *c = drmModeGetConnector(m_fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
            m_conn = c->connector_id;
            m_name = std::string(drmModeGetConnectorTypeName(c->connector_type)) + "-" + std::to_string(c->connector_type_id);
            // Режим — родное разрешение монитора (предпочтительный режим), частота — ближайшая к hz из его режимов при
            // нём (настройки стола «Экран»; по умолчанию 60 — браузер, Chromium в Qt WebEngine, рисует страницы ровно
            // 60 к/с: при 75 Гц каждое пятое обновление повторяло кадр, docs/PI.md, «60 Гц»). Разрешение — всегда
            // родное: кадр стола растягивает слой дисплея, режим ниже только мутнее. Не «какой остался от консоли»:
            // ядро, возвращая консоль, ставит предпочтительный.
            drmModeModeInfo preferred = c->modes[0];
            for (int m = 0; m < c->count_modes; ++m)
                if (c->modes[m].type & DRM_MODE_TYPE_PREFERRED)
                    preferred = c->modes[m];
            for (int m = 0; m < c->count_modes; ++m) {
                const drmModeModeInfo &cand = c->modes[m];
                if (cand.hdisplay == preferred.hdisplay && cand.vdisplay == preferred.vdisplay
                    && !(cand.flags & DRM_MODE_FLAG_INTERLACE))
                    m_modes.push_back(cand);
            }
            if (m_modes.empty())
                m_modes.push_back(preferred);
            mode = m_modes[pickMode(hz)];
            drmModeObjectProperties *cp = drmModeObjectGetProperties(m_fd, c->connector_id, DRM_MODE_OBJECT_CONNECTOR);
            for (uint32_t k = 0; cp && k < cp->count_props; ++k) {
                drmModePropertyRes *pr = drmModeGetProperty(m_fd, cp->props[k]);
                if (pr && !strcmp(pr->name, "EDID") && cp->prop_values[k])
                    if (drmModePropertyBlobRes *b = drmModeGetPropertyBlob(m_fd, uint32_t(cp->prop_values[k]))) {
                        m_monitor = edidName(static_cast<const uint8_t *>(b->data), b->length);
                        drmModeFreePropertyBlob(b);
                    }
                drmModeFreeProperty(pr);
            }
            drmModeFreeObjectProperties(cp);
            drmModeEncoder *e = c->encoder_id ? drmModeGetEncoder(m_fd, c->encoder_id) : nullptr;
            drmModeCrtc *crtc = e && e->crtc_id ? drmModeGetCrtc(m_fd, e->crtc_id) : nullptr;
            if (crtc) {
                m_crtc = crtc->crtc_id;
            } else {
                for (int k = 0; k < c->count_encoders && !m_crtc; ++k) {
                    drmModeEncoder *pe = drmModeGetEncoder(m_fd, c->encoders[k]);
                    for (int j = 0; pe && j < res->count_crtcs && !m_crtc; ++j)
                        if (pe->possible_crtcs & (1u << j))
                            m_crtc = res->crtcs[j];
                    drmModeFreeEncoder(pe);
                }
            }
            drmModeFreeCrtc(crtc);
            drmModeFreeEncoder(e);
        }
        drmModeFreeConnector(c);
    }
    for (int j = 0; j < res->count_crtcs; ++j)
        if (res->crtcs[j] == m_crtc)
            crtcIndex = j;
    drmModeFreeResources(res);
    if (!m_conn || crtcIndex < 0) {
        fprintf(stderr, "экран: монитор не подключён\n");
        return false;
    }
    m_w = mode.hdisplay;
    m_h = mode.vdisplay;
    m_hz = rateOf(mode);
    m_periodUs = int64_t(1e6 / m_hz);
    if (const char *m = getenv("NAOMI_DOCK_MARGIN_US"))
        m_marginUs = atoi(m);
    m_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

    // слои: основной — кадр; свободные дополнительные: первый — курсор, следующие два — покров и часы (экран без дока)
    std::vector<uint32_t> overlays;
    drmModePlaneRes *planes = drmModeGetPlaneResources(m_fd);
    for (uint32_t i = 0; planes && i < planes->count_planes; ++i) {
        drmModePlane *p = drmModeGetPlane(m_fd, planes->planes[i]);
        if (p && (p->possible_crtcs & (1u << crtcIndex))) {
            uint64_t type = 0;
            drmModeObjectProperties *props = drmModeObjectGetProperties(m_fd, p->plane_id, DRM_MODE_OBJECT_PLANE);
            for (uint32_t k = 0; props && k < props->count_props; ++k) {
                drmModePropertyRes *pr = drmModeGetProperty(m_fd, props->props[k]);
                if (pr && !strcmp(pr->name, "type"))
                    type = props->prop_values[k];
                drmModeFreeProperty(pr);
            }
            drmModeFreeObjectProperties(props);
            if (type == DRM_PLANE_TYPE_PRIMARY && !m_videoPlane)
                m_videoPlane = p->plane_id;
            else if (type == DRM_PLANE_TYPE_OVERLAY && !p->crtc_id && overlays.size() < 1 + kLayers)
                overlays.push_back(p->plane_id);
        }
        drmModeFreePlane(p);
    }
    drmModeFreePlaneResources(planes);
    if (overlays.size() == 1 + kLayers) {
        m_cursorPlane = overlays[0];
        for (int i = 0; i < kLayers; ++i)
            m_layerPlane[i] = overlays[size_t(1 + i)];
    }
    if (!m_videoPlane || !m_cursorPlane) {
        fprintf(stderr, "экран: нет нужных слоёв\n");
        return false;
    }
    auto planeProps = [&](uint32_t plane, auto &pp) {
        pp.fb = prop(plane, DRM_MODE_OBJECT_PLANE, "FB_ID");
        pp.crtc = prop(plane, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
        pp.srcX = prop(plane, DRM_MODE_OBJECT_PLANE, "SRC_X");
        pp.srcY = prop(plane, DRM_MODE_OBJECT_PLANE, "SRC_Y");
        pp.srcW = prop(plane, DRM_MODE_OBJECT_PLANE, "SRC_W");
        pp.srcH = prop(plane, DRM_MODE_OBJECT_PLANE, "SRC_H");
        pp.crtcX = prop(plane, DRM_MODE_OBJECT_PLANE, "CRTC_X");
        pp.crtcY = prop(plane, DRM_MODE_OBJECT_PLANE, "CRTC_Y");
        pp.crtcW = prop(plane, DRM_MODE_OBJECT_PLANE, "CRTC_W");
        pp.crtcH = prop(plane, DRM_MODE_OBJECT_PLANE, "CRTC_H");
        // цвет — как пакует телефон (shell/stream/nv12_pack.cpp): BT.709, узкий диапазон
        pp.encoding = prop(plane, DRM_MODE_OBJECT_PLANE, "COLOR_ENCODING", &m_bt709, "ITU-R BT.709 YCbCr");
        pp.range = prop(plane, DRM_MODE_OBJECT_PLANE, "COLOR_RANGE", &m_limited, "YCbCr limited range");
        pp.alpha = prop(plane, DRM_MODE_OBJECT_PLANE, "alpha");
        pp.zpos = prop(plane, DRM_MODE_OBJECT_PLANE, "zpos");
    };
    planeProps(m_videoPlane, m_vp);
    planeProps(m_cursorPlane, m_cp);
    for (int i = 0; i < kLayers; ++i)
        if (m_layerPlane[i])
            planeProps(m_layerPlane[i], m_lp[i]);

    // чёрный фон до первого кадра (маленький, растягивается) и буферы курсора (пустые: картинку даёт setCursor)
    uint32_t *px, pitch;
    m_blackFb = dumbFb(320, 180, DRM_FORMAT_XRGB8888, &px, &pitch);
    for (int i = 0; i < 3; ++i)
        m_cursorFbs[i] = dumbFb(kCursor, kCursor, DRM_FORMAT_ARGB8888, &m_cursorPx[i], &m_cursorPitch);
    if (!m_blackFb || !m_cursorFbs[0] || !m_cursorFbs[1] || !m_cursorFbs[2]) {
        fprintf(stderr, "экран: буферы не создались\n");
        return false;
    }
    m_cx = m_w / 2;
    m_cy = m_h / 2;

    drmModeAtomicReq *req = drmModeAtomicAlloc();
    drmModeCreatePropertyBlob(m_fd, &mode, sizeof mode, &m_modeBlob);
    drmModeAtomicAddProperty(req, m_conn, prop(m_conn, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID"), m_crtc);
    drmModeAtomicAddProperty(req, m_crtc, prop(m_crtc, DRM_MODE_OBJECT_CRTC, "MODE_ID"), m_modeBlob);
    m_crtcActive = prop(m_crtc, DRM_MODE_OBJECT_CRTC, "ACTIVE");
    drmModeAtomicAddProperty(req, m_crtc, m_crtcActive, 1);
    auto put = [&](uint32_t plane, auto &pp, uint32_t fb, int sw, int sh, Rect to) {
        drmModeAtomicAddProperty(req, plane, pp.fb, fb);
        drmModeAtomicAddProperty(req, plane, pp.crtc, m_crtc);
        drmModeAtomicAddProperty(req, plane, pp.srcX, 0);
        drmModeAtomicAddProperty(req, plane, pp.srcY, 0);
        drmModeAtomicAddProperty(req, plane, pp.srcW, uint64_t(sw) << 16);
        drmModeAtomicAddProperty(req, plane, pp.srcH, uint64_t(sh) << 16);
        drmModeAtomicAddProperty(req, plane, pp.crtcX, uint64_t(to.x));
        drmModeAtomicAddProperty(req, plane, pp.crtcY, uint64_t(to.y));
        drmModeAtomicAddProperty(req, plane, pp.crtcW, uint64_t(to.w));
        drmModeAtomicAddProperty(req, plane, pp.crtcH, uint64_t(to.h));
    };
    put(m_videoPlane, m_vp, m_blackFb, 320, 180, Rect{0, 0, m_w, m_h});
    put(m_cursorPlane, m_cp, m_cursorFbs[0], kCursor, kCursor, Rect{m_cx, m_cy, kCursor, kCursor});
    // порядок: покров над кадром, часы над покровом, курсор выше всех
    for (int i = 0; i < kLayers; ++i)
        if (m_layerPlane[i] && m_lp[i].zpos)
            drmModeAtomicAddProperty(req, m_layerPlane[i], m_lp[i].zpos, uint64_t(1 + i));
    if (m_cp.zpos)
        drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.zpos, uint64_t(1 + kLayers));
    const int r0 = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    if (r0) {
        fprintf(stderr, "экран: не включился (%s)\n", strerror(-r0));
        return false;
    }
    m_video = Rect{0, 0, m_w, m_h};
    std::string all;
    for (double r : rates()) {
        char b[16];
        snprintf(b, sizeof b, "%s%.2f", all.empty() ? "" : ", ", r);
        all += b;
    }
    fprintf(stderr, "экран: %s (%s) %dx%d, %.2f Гц; частоты при нём: %s\n", m_name.c_str(),
            m_monitor.empty() ? "?" : m_monitor.c_str(), m_w, m_h, m_hz, all.c_str());
    return true;
}

// Другая частота на ходу: режим того же разрешения — modeset (монитор заново ловит сигнал). Слои и кадр — как были;
// время обновлений — заново, у ядра. false — не сейчас (прошлое ещё не встало на экран): повторить позже
bool Display::setRate(double hz)
{
    const drmModeModeInfo &mode = m_modes[pickMode(hz)];
    if (std::fabs(rateOf(mode) - m_hz) < 0.01)
        return true;
    if (m_inFlight)
        return false;
    uint32_t blob = 0;
    if (drmModeCreatePropertyBlob(m_fd, &mode, sizeof mode, &blob)) {
        fprintf(stderr, "экран: частота %.2f Гц — режим не создался\n", rateOf(mode));
        return true;
    }
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, m_crtc, prop(m_crtc, DRM_MODE_OBJECT_CRTC, "MODE_ID"), blob);
    drmModeAtomicAddProperty(req, m_crtc, m_crtcActive, m_asleep ? 0 : 1);
    const int64_t t0 = monoUs();
    const int r = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    if (r) {
        drmModeDestroyPropertyBlob(m_fd, blob);
        fprintf(stderr, "экран: частота %.2f Гц не включилась (%s)\n", rateOf(mode), strerror(-r));
        return true;
    }
    drmModeDestroyPropertyBlob(m_fd, m_modeBlob);
    m_modeBlob = blob;
    const double was = m_hz;
    m_hz = rateOf(mode);
    m_periodUs = int64_t(1e6 / m_hz);
    m_lastVblankUs = 0;
    fprintf(stderr, "экран: частота %.2f → %.2f Гц (смена режима %.0f мс)\n", was, m_hz, (monoUs() - t0) / 1000.0);
    schedule();
    return true;
}

void Display::close()
{
    if (m_fd < 0)
        return;
    for (auto &p : m_pending)
        av_frame_free(&p.frame);
    m_pending.clear();
    av_frame_free(&m_flipping.frame);
    av_frame_free(&m_screen);
    for (auto &[h, fb] : m_fbs)
        drmModeRmFB(m_fd, fb.id);
    m_fbs.clear();
    if (m_timer >= 0)
        ::close(m_timer);
    m_timer = -1;
    drmDropMaster(m_fd);
    ::close(m_fd);   // последний хозяин ушёл — ядро вернёт консоль
    m_fd = -1;
}

// буфер слоя для кадра декодера; handles — его хэндлы GEM (по плоскостям)
uint32_t Display::addFb(AVFrame *f, uint32_t handles[4])
{
    const auto *d = reinterpret_cast<const AVDRMFrameDescriptor *>(f->data[0]);
    uint32_t objects[AV_DRM_MAX_PLANES] = {};
    for (int o = 0; o < d->nb_objects; ++o)
        if (drmPrimeFDToHandle(m_fd, d->objects[o].fd, &objects[o]))
            return 0;
    uint32_t pitches[4] = {}, offsets[4] = {}, id = 0;
    uint64_t mods[4] = {};
    const AVDRMLayerDescriptor &layer = d->layers[0];
    for (int p = 0; p < layer.nb_planes && p < 4; ++p) {
        const int o = layer.planes[p].object_index;
        handles[p] = objects[o];
        pitches[p] = uint32_t(layer.planes[p].pitch);
        offsets[p] = uint32_t(layer.planes[p].offset);
        mods[p] = d->objects[o].format_modifier;
    }
    if (drmModeAddFB2WithModifiers(m_fd, uint32_t(f->width), uint32_t(f->height), layer.format, handles, pitches,
                                   offsets, mods, &id, DRM_MODE_FB_MODIFIERS)) {
        fprintf(stderr, "экран: кадр %dx%d (%.4s, %#llx) слой не взял: %s\n", f->width, f->height,
                reinterpret_cast<const char *>(&layer.format), static_cast<unsigned long long>(mods[0]), strerror(errno));
        return 0;
    }
    return id;
}

uint32_t Display::fbFor(AVFrame *f)
{
    const auto *d = reinterpret_cast<const AVDRMFrameDescriptor *>(f->data[0]);
    uint32_t first = 0;
    if (drmPrimeFDToHandle(m_fd, d->objects[0].fd, &first))
        return 0;
    if (auto it = m_fbs.find(first); it != m_fbs.end())
        return it->second.id;
    Fb fb;
    fb.gen = m_gen;
    fb.id = addFb(f, fb.handles);
    if (!fb.id)
        return 0;
    m_fbs[first] = fb;
    return fb.id;
}

uint32_t Display::frameFb(AVFrame *f, uint32_t handles[4])
{
    return addFb(f, handles);
}

void Display::dropFb(uint32_t fb, const uint32_t handles[4])
{
    if (fb)
        drmModeRmFB(m_fd, fb);
    std::vector<uint32_t> closed;
    for (int i = 0; i < 4; ++i)
        if (handles[i] && std::find(closed.begin(), closed.end(), handles[i]) == closed.end()) {
            drmCloseBufferHandle(m_fd, handles[i]);
            closed.push_back(handles[i]);
        }
}

Rect Display::fit(int sw, int sh) const
{
    Rect to{0, 0, m_w, m_h};
    if (sw <= 0 || sh <= 0)
        return to;
    if (int64_t(m_w) * sh > int64_t(m_h) * sw)
        to.w = int(int64_t(m_h) * sw / sh);
    else
        to.h = int(int64_t(m_w) * sh / sw);
    to.x = (m_w - to.w) / 2;
    to.y = (m_h - to.h) / 2;
    return to;
}

// новый поток: кадры прошлого в очереди — прочь (их буферы — у прошлого декодера)
void Display::newStream()
{
    ++m_gen;
    for (auto &p : m_pending)
        av_frame_free(&p.frame);
    m_pending.clear();
}

// буферы прошлых потоков — когда на экране и на подходе уже кадры нового (слой, у которого убрали буфер, гаснет)
void Display::dropStale()
{
    if (m_screenGen != m_gen || (m_flipping.frame && m_flipping.gen != m_gen))
        return;
    for (auto it = m_fbs.begin(); it != m_fbs.end();) {
        if (it->second.gen < m_gen) {
            drmModeRmFB(m_fd, it->second.id);
            std::vector<uint32_t> closed;
            for (uint32_t h : it->second.handles)
                if (h && std::find(closed.begin(), closed.end(), h) == closed.end()) {
                    drmCloseBufferHandle(m_fd, h);
                    closed.push_back(h);
                }
            it = m_fbs.erase(it);
        } else {
            ++it;
        }
    }
}

void Display::show(AVFrame *frame, const FrameInfo &info)
{
    m_pending.push_back({frame, info, m_gen});
    while (m_pending.size() > (info.target ? kTimed : kLag))
        dropFront();
    schedule();
}

// старший кадр очереди — прочь; его метку Away (экран без дока) берёт следующий
void Display::dropFront()
{
    Pending &p = m_pending.front();
    if (onDropped)
        onDropped(p.info);
    if (p.info.away && m_pending.size() > 1 && !m_pending[1].info.away)
        m_pending[1].info.away = p.info.away;
    av_frame_free(&p.frame);
    m_pending.pop_front();
    ++dropped;
}

void Display::cursorChanged()
{
    m_cursorDirty = true;
    schedule();
}

void Display::layersChanged()
{
    m_layersDirty = true;
    schedule();
}

// свои слои и прозрачность курсора — к обновлению монитора, к которому эта отдача
void Display::addLayers(drmModeAtomicReq *req)
{
    m_layersDirty = false;
    if (!layersAt)
        return;
    Layer l[kLayers];
    double cursorAlpha = 1;
    layersAt(m_expectedUs ? m_expectedUs : monoUs(), l, cursorAlpha);
    for (int i = 0; i < kLayers; ++i) {
        const uint32_t plane = m_layerPlane[i];
        const PlaneProps &pp = m_lp[i];
        if (!plane)
            continue;
        if (!l[i].fb || l[i].alpha <= 0 || l[i].to.w <= 0 || l[i].to.h <= 0) {
            if (m_layerOn[i]) {
                drmModeAtomicAddProperty(req, plane, pp.fb, 0);
                drmModeAtomicAddProperty(req, plane, pp.crtc, 0);
                m_layerOn[i] = false;
            }
            continue;
        }
        drmModeAtomicAddProperty(req, plane, pp.fb, l[i].fb);
        drmModeAtomicAddProperty(req, plane, pp.crtc, m_crtc);
        drmModeAtomicAddProperty(req, plane, pp.srcX, uint64_t(l[i].sx) << 16);
        drmModeAtomicAddProperty(req, plane, pp.srcY, uint64_t(l[i].sy) << 16);
        drmModeAtomicAddProperty(req, plane, pp.srcW, uint64_t(l[i].sw) << 16);
        drmModeAtomicAddProperty(req, plane, pp.srcH, uint64_t(l[i].sh) << 16);
        drmModeAtomicAddProperty(req, plane, pp.crtcX, uint64_t(int64_t(l[i].to.x)));
        drmModeAtomicAddProperty(req, plane, pp.crtcY, uint64_t(int64_t(l[i].to.y)));
        drmModeAtomicAddProperty(req, plane, pp.crtcW, uint64_t(l[i].to.w));
        drmModeAtomicAddProperty(req, plane, pp.crtcH, uint64_t(l[i].to.h));
        if (pp.alpha)
            drmModeAtomicAddProperty(req, plane, pp.alpha, uint64_t(std::lround(std::clamp(l[i].alpha, 0.0, 1.0) * 0xffff)));
        if (l[i].yuv && pp.encoding)
            drmModeAtomicAddProperty(req, plane, pp.encoding, m_bt709);
        if (l[i].yuv && pp.range)
            drmModeAtomicAddProperty(req, plane, pp.range, m_limited);
        m_layerOn[i] = true;
    }
    if (m_cp.alpha)
        drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.alpha, uint64_t(std::lround(std::clamp(cursorAlpha, 0.0, 1.0) * 0xffff)));
}

bool Display::setAsleep(bool asleep)
{
    if (asleep == m_asleep)
        return true;
    if (m_inFlight)
        return false;
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, m_crtc, m_crtcActive, asleep ? 0 : 1);
    const int r = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, nullptr);
    drmModeAtomicFree(req);
    if (r) {
        fprintf(stderr, "экран: %s не вышло (%s)\n", asleep ? "усыпить" : "разбудить", strerror(-r));
        return false;
    }
    m_asleep = asleep;
    m_lastVblankUs = 0;   // после сна — время обновлений заново, у ядра
    if (asleep) {
        const itimerspec off{};
        timerfd_settime(m_timer, 0, &off, nullptr);
        m_armed = false;
    } else {
        schedule();
    }
    return true;
}

void Display::setCursor(const uint32_t *argb, int hotX, int hotY)
{
    int i = m_cursorNext;   // ещё не отдан — можно поверх
    for (int k = 0; i < 0 && k < 3; ++k)
        if (k != m_cursorFront && k != m_cursorShown)
            i = k;
    if (i < 0)
        return;
    for (int y = 0; y < kCursor; ++y)
        memcpy(reinterpret_cast<uint8_t *>(m_cursorPx[i]) + size_t(y) * m_cursorPitch, argb + size_t(y) * kCursor,
               kCursor * 4);
    m_cursorNext = i;
    m_nextHotX = hotX;
    m_nextHotY = hotY;
    cursorChanged();
}

void Display::placeCursor(drmModeAtomicReq *req)
{
    if (cursorAt)
        cursorAt(m_expectedUs ? m_expectedUs : monoUs(), m_cx, m_cy);
    if (m_cursorNext >= 0) {   // новая картинка — в этой же отдаче
        m_cursorFront = m_cursorNext;
        m_cursorNext = -1;
        m_hotX = m_nextHotX;
        m_hotY = m_nextHotY;
        drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.fb, m_cursorFbs[m_cursorFront]);
    }
    m_cursorCommitted = m_cursorFront;
    // горячая точка — на месте мыши; у левого и верхнего края картинку срезать (слой не уходит за экран), 1:1
    const int x = m_cx - m_hotX, y = m_cy - m_hotY, sx = std::max(0, -x), sy = std::max(0, -y);
    const int w = kCursor - sx, h = kCursor - sy;
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.srcX, uint64_t(sx) << 16);
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.srcY, uint64_t(sy) << 16);
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.srcW, uint64_t(w) << 16);
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.srcH, uint64_t(h) << 16);
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.crtcX, uint64_t(x + sx));
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.crtcY, uint64_t(y + sy));
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.crtcW, uint64_t(w));
    drmModeAtomicAddProperty(req, m_cursorPlane, m_cp.crtcH, uint64_t(h));
}

int64_t Display::nextVblank(int64_t after)
{
    int64_t base = m_lastVblankUs;
    if (!base || after - base > 1000000) {
        uint64_t seq = 0, ns = 0;
        if (!drmCrtcGetSequence(m_fd, m_crtc, &seq, &ns) && ns)
            base = m_lastVblankUs = int64_t(ns / 1000);
    }
    if (!base)
        return after;
    return base + std::max<int64_t>(1, (after - base) / m_periodUs + 1) * m_periodUs;
}

// Есть что показать — завести будильник на «за m_marginUs до ближайшего обновления, к которому ещё успеваем».
// Время обновлений — по событиям экрана (они приходят в обновление); давно не было — спросить у ядра.
void Display::schedule()
{
    if (m_asleep || m_inFlight || m_armed
        || (m_pending.empty() && !m_cursorDirty && !m_layersDirty && !(cursorMoving && cursorMoving())
            && !(layersBusy && layersBusy())))
        return;
    const int64_t now = monoUs();
    m_expectedUs = nextVblank(now + m_marginUs - 1);
    const int64_t at = m_expectedUs - m_marginUs;
    itimerspec it{};
    it.it_value.tv_sec = at / 1000000;
    it.it_value.tv_nsec = (at % 1000000) * 1000;
    timerfd_settime(m_timer, TFD_TIMER_ABSTIME, &it, nullptr);
    m_armed = true;
}

void Display::onTimer()
{
    uint64_t n;
    if (::read(m_timer, &n, sizeof n) < 0 && m_armed)
        return;   // будильник ещё не прозвенел
    m_armed = false;
    if (const int64_t late = monoUs() - (m_expectedUs - m_marginUs); late > 1000) {
        ++timerLate;
        timerLateMaxUs = std::max(timerLateMaxUs, late);
    }
    if (m_inFlight || m_asleep)
        return;
    if (!m_pending.empty() && pickReady())
        commitNext();
    else if (m_cursorDirty || m_layersDirty || (cursorMoving && cursorMoving()) || (layersBusy && layersBusy()))
        commitCursor();
    else
        schedule();   // кадры пришли раньше своего обновления — к следующему
}

// Какой кадр показать в обновление m_expectedUs. Кадры по такту знают своё обновление (target): самый свежий из тех,
// чьё обновление — это или прошлые; старшие — прочь (опоздавший не тянет за собой очередь: один рывок, задержка та
// же); пришедшие раньше своего обновления — ждут. Кадры не по такту (стол сам по себе) — сразу, по порядку; копятся
// несколько обновлений подряд — лишний прочь.
bool Display::pickReady()
{
    int pick = -1;
    for (size_t i = 0; i < m_pending.size(); ++i)
        if (m_pending[i].info.target <= m_expectedUs + m_periodUs / 2)
            pick = int(i);
    if (pick < 0)
        return false;
    const bool untimed = m_pending[size_t(pick)].info.target == 0;
    if (untimed) {
        m_backlog = m_pending.size() >= 2 ? m_backlog + 1 : 0;
        pick = m_backlog >= 4 ? 1 : 0;
        if (pick)
            m_backlog = 0;
    }
    for (int i = 0; i < pick; ++i)
        dropFront();
    return true;
}

void Display::commitNext()
{
    while (!m_pending.empty()) {
        Pending next = m_pending.front();
        m_pending.pop_front();
        AVFrame *f = next.frame;
        const uint32_t fb = fbFor(f);
        if (!fb) {
            av_frame_free(&f);
            continue;
        }
        const int sw = f->width - int(f->crop_left + f->crop_right), sh = f->height - int(f->crop_top + f->crop_bottom);
        const Rect to = fit(sw, sh);   // в пропорциях кадра, по центру
        drmModeAtomicReq *req = drmModeAtomicAlloc();
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.fb, fb);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.crtc, m_crtc);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.srcX, uint64_t(f->crop_left) << 16);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.srcY, uint64_t(f->crop_top) << 16);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.srcW, uint64_t(sw) << 16);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.srcH, uint64_t(sh) << 16);
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.crtcX, uint64_t(to.x));
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.crtcY, uint64_t(to.y));
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.crtcW, uint64_t(to.w));
        drmModeAtomicAddProperty(req, m_videoPlane, m_vp.crtcH, uint64_t(to.h));
        if (m_vp.encoding)
            drmModeAtomicAddProperty(req, m_videoPlane, m_vp.encoding, m_bt709);
        if (m_vp.range)
            drmModeAtomicAddProperty(req, m_videoPlane, m_vp.range, m_limited);
        if (next.info.away && onCommit)   // метка экрана без дока: его анимации — с этого обновления
            onCommit(next.info, m_expectedUs ? m_expectedUs : monoUs());
        placeCursor(req);
        addLayers(req);
        const int r = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, this);
        drmModeAtomicFree(req);
        if (r == -EBUSY) {   // прошлое ещё не встало — подождать события
            ++busy;
            m_pending.push_front(next);
            return;
        }
        if (r) {
            fprintf(stderr, "экран: кадр не показан: %s\n", strerror(-r));
            av_frame_free(&f);
            continue;
        }
        m_video = to;
        ++commitsFrame;
        m_committedUs = monoUs();
        m_inFlight = true;
        m_cursorDirty = false;
        m_flipping = next;
        return;
    }
}

void Display::commitCursor()
{
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    placeCursor(req);
    addLayers(req);
    const int r = drmModeAtomicCommit(m_fd, req, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, this);
    drmModeAtomicFree(req);
    if (!r) {
        ++commitsCursor;
        m_committedUs = monoUs();
        m_inFlight = true;
        m_cursorDirty = false;
    } else if (r == -EBUSY) {
        ++busy;
    }
}

void Display::flipHandler(int, unsigned, unsigned sec, unsigned usec, unsigned, void *self)
{
    static_cast<Display *>(self)->onFlip(int64_t(sec) * 1000000 + usec);
}

void Display::handleEvents()
{
    drmEventContext ev{};
    ev.version = 3;
    ev.page_flip_handler2 = flipHandler;
    drmHandleEvent(m_fd, &ev);
}

void Display::onFlip(int64_t us)
{
    m_inFlight = false;
    m_cursorShown = m_cursorCommitted;
    ++flips;
    flipWaitUs += monoUs() - m_committedUs;
    if (m_expectedUs && us > m_expectedUs + m_periodUs / 2)
        ++late;
    m_lastVblankUs = us;
    if (m_flipping.frame) {
        av_frame_free(&m_screen);
        m_screen = m_flipping.frame;
        m_screenGen = m_flipping.gen;
        const FrameInfo info = m_flipping.info;
        m_flipping = {nullptr, {}, 0};
        if (info.target && us > info.target + m_periodUs / 2)
            ++framesLate;
        if (onShown)
            onShown(info, us);
        dropStale();
    }
    schedule();
}
