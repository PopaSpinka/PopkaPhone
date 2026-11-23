#include "clock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/eventfd.h>
#include <unistd.h>
#include <vector>

#include "cairo_min.h"

namespace {

// Надпись: шрифт (семья, оси Google Sans Flex / Google Sans), размер — в точках стола, разрядка — в долях кегля
struct Text {
    const char *family;
    double size;
    int wght;
    double spacing;
    double alpha;
};
// Вид — как часы и дата главного экрана телефона (ClockLayer, DateLayer): цифры Google Sans Flex, тонкие (300),
// скруглённые (ROND 100), одной ширины; под ними дата капсом с разрядкой. Выбор пользователя из 10 вариантов,
// 17 ноября 2026: «оставим пока дефолтный 1». Цвет — Theme.text (тёплый белый), под надписями мягкая тень: небо
// обоев светлое
constexpr Text kTime{"Google Sans Flex", 150, 300, 0, 1};
constexpr Text kDate{"Google Sans", 19, 500, 0.1, 0.85};
constexpr uint32_t kColor = 0xece9e3;

const char *const kDaysUp[] = {"ВОСКРЕСЕНЬЕ", "ПОНЕДЕЛЬНИК", "ВТОРНИК", "СРЕДА", "ЧЕТВЕРГ", "ПЯТНИЦА", "СУББОТА"};
const char *const kMonthsUp[] = {"ЯНВАРЯ", "ФЕВРАЛЯ", "МАРТА",    "АПРЕЛЯ",  "МАЯ",    "ИЮНЯ",
                                 "ИЮЛЯ",   "АВГУСТА", "СЕНТЯБРЯ", "ОКТЯБРЯ", "НОЯБРЯ", "ДЕКАБРЯ"};

// разметка pango: шрифт с осями, цифры одной ширины (tnum), разрядка — в 1/1024 пункта (96 точек на дюйм: 3/4 пикселя)
std::string markup(const Text &t, double pt, const std::string &text)
{
    const double px = t.size * pt;
    char head[320];
    snprintf(head, sizeof head,
             "<span font=\"%s %.1fpx @wght=%d,ROND=100,opsz=%d\" font_features=\"tnum=1\" letter_spacing=\"%d\">",
             t.family, px, t.wght, int(std::clamp(px, 6.0, 144.0)), int(t.spacing * px * 1024 * 0.75));
    return head + text + "</span>";
}

struct Piece {
    PangoLayout *layout;
    const Text *text;
    PangoRectangle ink, logical;
    double y = 0;   // где верх чернил (слева — у всех по краю чернил)
};

} // namespace

// Нарисовать в to (с левого верхнего угла); Ready.x, y — где этот угол на экране
ClockFace::Ready ClockFace::render(const Place &at, int64_t minute, Buffer &to)
{
    const double pt = at.pt;
    const time_t t = time_t(minute * 60);
    tm lt{};
    localtime_r(&t, &lt);
    char hm[16];
    snprintf(hm, sizeof hm, "%02d:%02d", lt.tm_hour, lt.tm_min);
    const std::string date = std::string(kDaysUp[lt.tm_wday]) + " · " + std::to_string(lt.tm_mday) + " " + kMonthsUp[lt.tm_mon];

    // разметка в черновом холсте: размеры надписей
    std::vector<uint32_t> scratch(16);
    cairo_surface_t *ms = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char *>(scratch.data()), kArgb32,
                                                              4, 4, 16);
    cairo_t *mc = cairo_create(ms);
    Piece pieces[2];
    auto add = [&](Piece &p, const Text &tx, const std::string &str) {
        p = Piece{pango_cairo_create_layout(mc), &tx, {}, {}};
        const std::string m = markup(tx, pt, str);
        pango_layout_set_markup(p.layout, m.c_str(), -1);
        pango_layout_get_pixel_extents(p.layout, &p.ink, &p.logical);
    };
    add(pieces[0], kTime, hm);
    add(pieces[1], kDate, date);
    pieces[1].y = pieces[0].ink.height + 16 * pt;   // дата — под цифрами
    // рамка чернил + поля под тень
    double x1 = 0, y1 = 0;
    for (const Piece &p : pieces) {
        x1 = std::max(x1, double(p.ink.width));
        y1 = std::max(y1, p.y + p.ink.height);
    }
    const int pad = int(std::ceil(28 * pt));
    const int w = std::min(to.w, int(std::ceil(x1)) + 2 * pad), h = std::min(to.h, int(std::ceil(y1)) + 2 * pad);

    // чернила — в черновик размером с картинку
    std::vector<uint32_t> fg(size_t(w) * h, 0);
    cairo_surface_t *fs = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char *>(fg.data()), kArgb32, w, h,
                                                              w * 4);
    cairo_t *fc = cairo_create(fs);
    for (Piece &p : pieces) {
        cairo_set_source_rgba(fc, (kColor >> 16 & 255) / 255.0, (kColor >> 8 & 255) / 255.0, (kColor & 255) / 255.0,
                              p.text->alpha);
        cairo_move_to(fc, pad - p.ink.x, pad + p.y - p.ink.y);
        pango_cairo_show_layout(fc, p.layout);
        g_object_unref(p.layout);
    }
    cairo_surface_flush(fs);
    cairo_destroy(fc);
    cairo_surface_destroy(fs);
    cairo_destroy(mc);
    cairo_surface_destroy(ms);

    // мягкая тень: альфа чернил, размытая, — чёрным под надписями, чуть ниже
    std::vector<float> shadow(fg.size());
    for (size_t i = 0; i < fg.size(); ++i)
        shadow[i] = float(fg[i] >> 24) / 255.0f;
    boxBlur(shadow, w, h, std::max(2, int(7 * pt)));
    const int dy = int(std::lround(2 * pt));
    for (int y = 0; y < h; ++y) {
        uint32_t *row = to.pixels + size_t(y) * (to.pitch / 4);
        for (int x = 0; x < w; ++x) {
            const uint32_t c = fg[size_t(y) * w + x];
            const double sa = y - dy >= 0 ? 0.42 * shadow[size_t(y - dy) * w + x] : 0;
            const double a = (c >> 24) / 255.0;
            row[x] = uint32_t(std::lround((a + sa * (1 - a)) * 255)) << 24 | (c & 0xffffff);   // тень под чернилами
        }
        std::fill(row + w, row + to.w, 0u);
    }
    for (int y = h; y < to.h; ++y)
        std::fill(to.pixels + size_t(y) * (to.pitch / 4), to.pixels + size_t(y) * (to.pitch / 4) + to.w, 0u);

    Ready r;
    r.w = w;
    r.h = h;
    // левый верх чернил — куда поставили (по умолчанию слева 44 точки стола, сверху 34 — где панель); картинка с полями
    // тени — целиком на экране
    r.x = std::clamp(int(std::lround(at.fx * at.screenW)) - pad, 0, std::max(0, at.screenW - w));
    r.y = std::clamp(int(std::lround(at.fy * at.screenH)) - pad, 0, std::max(0, at.screenH - h));
    r.minute = minute;
    return r;
}

void ClockFace::start(Buffer a, Buffer b, double pt, int screenW, int screenH)
{
    m_buf[0] = a;
    m_buf[1] = b;
    m_pt = pt;
    m_screenW = screenW;
    m_screenH = screenH;
    m_event = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_thread = std::thread([this] { run(); });
}

void ClockFace::stop()
{
    if (!m_thread.joinable())
        return;
    {
        std::lock_guard lock(m_m);
        m_quit = true;
    }
    m_cv.notify_one();
    m_thread.join();
    ::close(m_event);
    m_event = -1;
}

void ClockFace::draw(int64_t minute)
{
    std::lock_guard lock(m_m);
    m_want = true;
    m_wantMinute = minute;
    m_cv.notify_one();
}

void ClockFace::setPt(double pt)
{
    std::lock_guard lock(m_m);
    m_pt = pt;
}

void ClockFace::setPos(double fx, double fy)
{
    std::lock_guard lock(m_m);
    m_fx = fx;
    m_fy = fy;
}

bool ClockFace::take(Ready &out)
{
    uint64_t n;
    (void)!::read(m_event, &n, sizeof n);
    std::lock_guard lock(m_m);
    if (!m_haveReady)
        return false;
    m_haveReady = false;
    out = m_ready;
    m_shown = out.buffer;
    return true;
}

void ClockFace::run()
{
    for (;;) {
        int buffer;
        int64_t minute;
        Place at;
        {
            std::unique_lock lock(m_m);
            m_cv.wait(lock, [this] { return m_quit || m_want; });
            if (m_quit)
                return;
            m_want = false;
            minute = m_wantMinute;
            at = Place{m_pt, m_fx, m_fy, m_screenW, m_screenH};
            buffer = m_shown == 0 ? 1 : 0;
        }
        Ready r = render(at, minute, m_buf[buffer]);
        r.buffer = buffer;
        std::lock_guard lock(m_m);
        m_ready = r;
        m_haveReady = true;
        const uint64_t one = 1;
        (void)!::write(m_event, &one, sizeof one);
    }
}
