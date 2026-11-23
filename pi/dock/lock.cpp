#include "lock.h"

#include "link.h"   // monoUs

extern "C" {
#include <libavutil/frame.h>
}

#include <drm_fourcc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

namespace {

const char *const kWallFile = "/var/lib/naomi-dock/desk.hevc";
constexpr int64_t kOpenUs = 180000, kCloseUs = 140000;   // меню N стола: проявление 180 мс, уход 140 мс (OutCubic)

} // namespace

double LockScreen::Anim::at(int64_t t) const
{
    if (dur <= 0 || t >= start + dur)
        return to;
    if (t <= start)
        return from;
    const double p = double(t - start) / double(dur), q = 1 - p;
    return from + (to - from) * (1 - q * q * q);
}

void LockScreen::Anim::go(double target, int64_t t0, int64_t d)
{
    from = at(t0);   // с того места, где сейчас (передумали посреди)
    to = target;
    start = t0;
    dur = t0 ? d : 0;
}

bool LockScreen::open(Display &display, double pt)
{
    m_display = &display;
    m_pt = pt;
    ClockFace::Buffer b[2];
    for (int i = 0; i < 2; ++i) {
        uint32_t *px = nullptr, pitch = 0;
        b[i].w = std::min(display.width(), 2000);
        b[i].h = std::min(display.height(), 1100);
        m_clockFb[i] = display.dumbFb(b[i].w, b[i].h, DRM_FORMAT_ARGB8888, &px, &pitch);
        if (!m_clockFb[i]) {
            fprintf(stderr, "без дока: буфер часов не создался\n");
            return false;
        }
        b[i].pixels = px;
        b[i].pitch = int(pitch);
    }
    m_face.start(b[0], b[1], pt, display.width(), display.height());
    m_minute = time(nullptr) / 60;
    m_face.draw(m_minute);
    display.layersAt = [this](int64_t vblank, Display::Layer *l, double &cursorAlpha) { layersAt(vblank, l, cursorAlpha); };
    display.layersBusy = [this] { return busy(monoUs()); };
    m_cursor.to = m_cursor.from = 1;
    return true;
}

void LockScreen::setPt(double pt)
{
    if (std::fabs(pt - m_pt) < 1e-6)
        return;
    m_pt = pt;
    m_face.setPt(pt);
    m_face.draw(m_minute);
}

void LockScreen::setClockPos(double fx, double fy)
{
    m_face.setPos(fx, fy);
    m_face.draw(m_minute);
}

void LockScreen::setClockShown(bool shown)
{
    m_clockShown = shown;
    if (m_display)
        m_display->layersChanged();
}

void LockScreen::close()
{
    m_face.stop();
    dropWallpaper();
    m_dec.close();
}

void LockScreen::poll(int64_t)
{
    const int64_t minute = time(nullptr) / 60;
    if (minute != m_minute) {
        m_minute = minute;
        m_face.draw(minute);
    }
    ClockFace::Ready r;
    if (m_face.take(r)) {
        m_shown = r;
        if (m_display)
            m_display->layersChanged();
    }
}

void LockScreen::clockIn(int64_t at)
{
    m_clock.go(1, at, kOpenUs);
    m_display->layersChanged();
}

void LockScreen::clockOut(int64_t at)
{
    m_clock.go(0, at, kCloseUs);
    m_display->layersChanged();
}

void LockScreen::coverIn(int64_t at, int64_t dur)
{
    if (!m_wall && !m_wallTried)
        loadWallpaper();
    m_coverTo = 1;
    m_cover.go(1, at, dur);
    m_display->layersChanged();
}

void LockScreen::coverOut(int64_t at, int64_t dur)
{
    m_coverTo = 0;
    m_cover.go(0, at, dur);
    m_display->layersChanged();
}

void LockScreen::cursorTo(bool shown, int64_t at, int64_t dur)
{
    m_cursor.go(shown ? 1 : 0, at, dur);
    m_display->layersChanged();
}

bool LockScreen::busy(int64_t now) const
{
    for (const Anim *a : {&m_clock, &m_cover, &m_cursor})
        if (a->dur && now < a->start + a->dur + 20000)
            return true;
    return false;
}

void LockScreen::layersAt(int64_t vblank, Display::Layer *l, double &cursorAlpha)
{
    cursorAlpha = m_cursor.at(vblank);
    // покров: обои во весь кадр стола (как кадр — в своих пропорциях), нет обоев — чёрный во весь экран
    Display::Layer &c = l[Display::Cover];
    c.alpha = m_cover.at(vblank);
    if (c.alpha > 0 && m_wall) {
        c.fb = m_wallFb;
        c.sx = int(m_wall->crop_left);
        c.sy = int(m_wall->crop_top);
        c.sw = m_wall->width - int(m_wall->crop_left + m_wall->crop_right);
        c.sh = m_wall->height - int(m_wall->crop_top + m_wall->crop_bottom);
        c.to = m_display->fit(c.sw, c.sh);
        c.yuv = true;
    } else if (c.alpha > 0) {
        c.fb = m_display->blackFb();
        c.sw = 320;
        c.sh = 180;
        c.to = Rect{0, 0, m_display->width(), m_display->height()};
    }
    // Часы: проявляются, съезжая на место сверху на 4 % своей высоты, уходят — вверх и тают (как меню N стола, но без
    // масштаба). Масштаб был (0,96 → 1): пока видеочип тянет слой часов, он делит с кадром стола (тоже растянутым)
    // память строк, и кадр на строках часов слева терял цвет — зелёный блок у N при каждом возвращении в док (фото
    // пользователя, 17 ноября 2026). Слой — всегда 1:1, движется только место
    Display::Layer &k = l[Display::Clock];
    k.alpha = m_clockShown ? m_clock.at(vblank) : 0;
    if (k.alpha > 0 && m_shown.buffer >= 0) {
        k.fb = m_clockFb[m_shown.buffer];
        k.sw = m_shown.w;
        k.sh = m_shown.h;
        k.to.w = m_shown.w;
        k.to.h = m_shown.h;
        k.to.x = m_shown.x;
        k.to.y = m_shown.y - int(std::lround(0.04 * m_shown.h * (1 - k.alpha)));
        if (k.to.y < 0) {   // у верхнего края — срезать сверху, а не за экран
            k.sy = -k.to.y;
            k.sh -= k.sy;
            k.to.h = k.sh;
            k.to.y = 0;
        }
    }
}

void LockScreen::saveWallpaper(const uint8_t *hevc, size_t n)
{
    mkdir("/var/lib/naomi-dock", 0755);
    const std::string tmp = std::string(kWallFile) + ".new";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f)
        return;
    const bool ok = fwrite(hevc, 1, n, f) == n;
    fclose(f);
    if (ok && rename(tmp.c_str(), kWallFile) == 0) {
        fprintf(stderr, "без дока: обои запомнены (%zu КБ)\n", n / 1024);
        if (!covered()) {   // на экране не они — прочесть заново, когда понадобятся
            dropWallpaper();
            m_wallTried = false;
        }
    }
}

// обои с диска: свой декодер (тот же аппаратный HEVC) — кадр в видеопамяти, слою — как кадр потока
bool LockScreen::loadWallpaper()
{
    m_wallTried = true;
    FILE *f = fopen(kWallFile, "rb");
    if (!f)
        return false;
    std::vector<uint8_t> data;
    uint8_t buf[65536];
    for (size_t n; (n = fread(buf, 1, sizeof buf, f)) > 0;)
        data.insert(data.end(), buf, buf + n);
    fclose(f);
    if (!m_dec.isOpen() && !m_dec.open())
        return false;
    std::vector<AVFrame *> frames;
    for (int i = 0; i < 3 && frames.empty(); ++i)   // декодер отдаёт кадр сразу; придержал — тот же кадр ещё раз
        m_dec.decode(data.data(), data.size(), i, frames);
    if (frames.empty()) {
        fprintf(stderr, "без дока: обои (%zu КБ) не распаковались\n", data.size() / 1024);
        return false;
    }
    dropWallpaper();
    m_wall = frames.front();
    for (size_t i = 1; i < frames.size(); ++i)
        av_frame_free(&frames[i]);
    m_wallFb = m_display->frameFb(m_wall, m_wallHandles);
    fprintf(stderr, "без дока: обои %dx%d с диска\n", m_wall->width, m_wall->height);
    return m_wallFb != 0;
}

void LockScreen::dropWallpaper()
{
    if (m_display && (m_wallFb || m_wallHandles[0]))
        m_display->dropFb(m_wallFb, m_wallHandles);
    m_wallFb = 0;
    std::fill(std::begin(m_wallHandles), std::end(m_wallHandles), 0u);
    av_frame_free(&m_wall);
}
