#include "cursor.h"

#include "cairo_min.h"
#include "stream_protocol.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

// Пути — поле 32 × 32, как в макете (единица — 1/32 стороны рисунка)
const char *const kArrow = "M6 3.5L6 24L11 19.4L14.4 27L17.6 25.6L14.3 18.3L21 18.3Z";
const char *const kHand = "M12 4.4a2.2 2.2 0 0 1 4.4 0V12.6a2.1 2.1 0 0 1 4.2 .4V14a2 2 0 0 1 3.8 .8V21c0 4.2-2.8 7-6.8 7"
                          "h-3.4c-2.6 0-4.2-1.2-5.6-3.4L4.6 19.4a1.9 1.9 0 0 1 2.9-2.4L12 20Z";
const char *const kText = "M11.5 5h3q1.5 0 1.5 1.4q0-1.4 1.5-1.4h3v2h-2.4q-1.1 0-1.1 1.1V23.9q0 1.1 1.1 1.1h2.4v2h-3"
                          "q-1.5 0-1.5-1.4q0 1.4-1.5 1.4h-3v-2h2.4q1.1 0 1.1-1.1V8.1q0-1.1-1.1-1.1h-2.4Z";
const char *const kSize2 = "M3 16L9.5 9.5V13.6H22.5V9.5L29 16L22.5 22.5V18.4H9.5V22.5Z";
const char *const kMove = "M16 2.5L21 7.5H18.2V13.8H24.5V11L29.5 16L24.5 21V18.2H18.2V24.5H21L16 29.5L11 24.5H13.8V18.2"
                          "H7.5V21L2.5 16L7.5 11V13.8H13.8V7.5H11Z";
const char *const kGrab = "M8.2 15.2a2.1 2.1 0 0 1 4.1-.9a2.1 2.1 0 0 1 4.1-.5a2.1 2.1 0 0 1 4.1 .2a2.1 2.1 0 0 1 4 1.2"
                          "V20c0 4.6-3 8-7.6 8h-1.4c-3.6 0-6.1-2-7.1-5.2Z";
const char *const kNoRing = "M16 5a11 11 0 1 1 0 22a11 11 0 1 1 0-22ZM16 8.8a7.2 7.2 0 1 0 0 14.4a7.2 7.2 0 1 0 0-14.4Z";
const char *const kNoSlash = "M10.2 12.4L12.4 10.2L21.8 19.6L19.6 21.8Z";
const char *const kCross = "M15 3h2v9.5h-2ZM15 19.5h2V29h-2ZM3 15h9.5v2H3ZM19.5 15H29v2h-9.5ZM15 15h2v2h-2Z";

struct Part {
    const char *d;
    double rotate = 0;   // градусы, вокруг середины поля
    double scale = 1, tx = 0, ty = 0;
    bool evenOdd = false;
};

// дуга SVG (окружность: rx = ry, без поворота) — к центру и углам (SVG 1.1, F.6.5)
void arcTo(cairo_t *cr, double x1, double y1, double r, int large, int sweep, double x2, double y2)
{
    const double dx = (x1 - x2) / 2, dy = (y1 - y2) / 2, d2 = dx * dx + dy * dy;
    if (d2 <= 0)
        return;
    r = std::max(r, std::sqrt(d2));
    double k = std::sqrt(std::max(0.0, (r * r - d2) / d2));
    if (large == sweep)
        k = -k;
    const double cx = k * dy + (x1 + x2) / 2, cy = -k * dx + (y1 + y2) / 2;
    const double a1 = std::atan2(y1 - cy, x1 - cx), a2 = std::atan2(y2 - cy, x2 - cx);
    if (sweep)
        cairo_arc(cr, cx, cy, r, a1, a2);
    else
        cairo_arc_negative(cr, cx, cy, r, a1, a2);
}

// путь SVG: M L H V C Q A Z, большие и малые буквы, числа слитно («1.4q0-1.4»)
void svgPath(cairo_t *cr, const char *d)
{
    double cx = 0, cy = 0, sx = 0, sy = 0;
    char cmd = 0;
    const char *p = d;
    auto skip = [&] {
        while (*p == ' ' || *p == ',')
            ++p;
    };
    bool ok = true;
    auto num = [&]() -> double {
        skip();
        char *end = nullptr;
        const double v = strtod(p, &end);
        ok = end != p;
        p = end;
        return v;
    };
    auto flag = [&]() -> int {
        skip();
        const int v = *p == '1';
        if (*p)
            ++p;
        return v;
    };
    for (;;) {
        skip();
        if (!*p)
            return;
        if (std::isalpha(static_cast<unsigned char>(*p)))
            cmd = *p++;
        const bool rel = std::islower(static_cast<unsigned char>(cmd));
        switch (std::tolower(static_cast<unsigned char>(cmd))) {
        case 'm': {
            double x = num(), y = num();
            if (rel)
                x += cx, y += cy;
            cairo_move_to(cr, x, y);
            cx = sx = x, cy = sy = y;
            cmd = rel ? 'l' : 'L';   // дальше — линии
            break;
        }
        case 'l': {
            double x = num(), y = num();
            if (rel)
                x += cx, y += cy;
            cairo_line_to(cr, x, y);
            cx = x, cy = y;
            break;
        }
        case 'h': {
            double x = num();
            if (rel)
                x += cx;
            cairo_line_to(cr, x, cy);
            cx = x;
            break;
        }
        case 'v': {
            double y = num();
            if (rel)
                y += cy;
            cairo_line_to(cr, cx, y);
            cy = y;
            break;
        }
        case 'c': {
            double v[6];
            for (double &a : v)
                a = num();
            if (rel)
                for (int i = 0; i < 6; i += 2)
                    v[i] += cx, v[i + 1] += cy;
            cairo_curve_to(cr, v[0], v[1], v[2], v[3], v[4], v[5]);
            cx = v[4], cy = v[5];
            break;
        }
        case 'q': {   // квадратичная — как кубическая
            double qx = num(), qy = num(), x = num(), y = num();
            if (rel)
                qx += cx, qy += cy, x += cx, y += cy;
            cairo_curve_to(cr, cx + 2.0 / 3 * (qx - cx), cy + 2.0 / 3 * (qy - cy), x + 2.0 / 3 * (qx - x),
                           y + 2.0 / 3 * (qy - y), x, y);
            cx = x, cy = y;
            break;
        }
        case 'a': {
            const double r = num();
            num();   // ry — то же
            num();   // поворот — 0
            const int large = flag(), sweep = flag();
            double x = num(), y = num();
            if (rel)
                x += cx, y += cy;
            arcTo(cr, cx, cy, r, large, sweep, x, y);
            cx = x, cy = y;
            break;
        }
        case 'z':
            cairo_close_path(cr);
            cx = sx, cy = sy;
            break;
        default:
            return;
        }
        if (!ok)
            return;
    }
}

std::vector<Part> partsOf(int shape)
{
    switch (shape) {
    case stream::CursorArrow: return {{kArrow}};
    case stream::CursorBusy: return {{kArrow, 0, 0.8, 1.2, 0.7}};
    case stream::CursorHand: return {{kHand}};
    case stream::CursorText: return {{kText}};
    case stream::CursorSizeH: return {{kSize2}};
    case stream::CursorSizeV: return {{kSize2, 90}};
    case stream::CursorSizeFDiag: return {{kSize2, 45}};
    case stream::CursorSizeBDiag: return {{kSize2, -45}};
    case stream::CursorMove: return {{kMove}};
    case stream::CursorGrab: return {{kGrab}};
    case stream::CursorNo: return {{kNoRing, 0, 1, 0, 0, true}, {kNoSlash}};
    case stream::CursorCross: return {{kCross}};
    default: return {};
    }
}

void drawParts(cairo_t *cr, const std::vector<Part> &parts, bool fill, bool strokeToo)
{
    for (const Part &p : parts) {
        cairo_save(cr);
        if (p.rotate) {
            cairo_translate(cr, 16, 16);
            cairo_rotate(cr, p.rotate * M_PI / 180);
            cairo_translate(cr, -16, -16);
        }
        cairo_translate(cr, p.tx, p.ty);
        cairo_scale(cr, p.scale, p.scale);
        cairo_new_path(cr);
        svgPath(cr, p.d);
        cairo_restore(cr);   // путь остаётся; линия — в единицах поля, без масштаба части
        cairo_set_fill_rule(cr, p.evenOdd ? kFillEvenOdd : 0);
        if (fill && strokeToo) {
            cairo_fill_preserve(cr);
            cairo_stroke(cr);
        } else if (fill) {
            cairo_fill(cr);
        } else {
            cairo_stroke(cr);
        }
    }
}

// «Морфинг»: квадрат со своим радиусом у каждого угла (доли стороны: левый верх, правый верх, правый низ, левый низ)
void morphPath(cairo_t *cr, double x, double y, double s, const double r[4])
{
    constexpr double K = 0.5523;
    const double a = r[0] * s, b = r[1] * s, c = r[2] * s, d = r[3] * s;
    cairo_new_path(cr);
    cairo_move_to(cr, x + a, y);
    cairo_line_to(cr, x + s - b, y);
    cairo_curve_to(cr, x + s - b + b * K, y, x + s, y + b - b * K, x + s, y + b);
    cairo_line_to(cr, x + s, y + s - c);
    cairo_curve_to(cr, x + s, y + s - c + c * K, x + s - c + c * K, y + s, x + s - c, y + s);
    cairo_line_to(cr, x + d, y + s);
    cairo_curve_to(cr, x + d - d * K, y + s, x, y + s - d + d * K, x, y + s - d);
    cairo_line_to(cr, x, y + a);
    cairo_curve_to(cr, x, y + a - a * K, x + a - a * K, y, x + a, y);
    cairo_close_path(cr);
}

void drawMorph(cairo_t *cr, double cx, double cy, double size, double width, double seconds)
{
    static const double keys[4][4] = {{.5, .5, .5, .5}, {.26, .26, .26, .26}, {.5, .07, .5, .07}, {.5, .5, .5, .5}};
    const double p = std::fmod(seconds, 1.8) / 1.8;
    const int seg = std::min(2, int(p * 3));
    double u = p * 3 - seg;
    u = u * u * (3 - 2 * u);   // разгон и торможение на каждый переход
    double r[4];
    for (int i = 0; i < 4; ++i)
        r[i] = keys[seg][i] + (keys[seg + 1][i] - keys[seg][i]) * u;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, p * 2 * M_PI);
    morphPath(cr, -size / 2, -size / 2, size, r);
    cairo_set_line_join(cr, kJoinRound);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_set_line_width(cr, width + 1.8);
    cairo_stroke(cr);
    morphPath(cr, -size / 2, -size / 2, size, r);
    cairo_set_source_rgba(cr, 0xf4 / 255.0, 0xef / 255.0, 0xe6 / 255.0, 1);
    cairo_set_line_width(cr, width);
    cairo_stroke(cr);
    cairo_restore(cr);
}

} // namespace

CursorArt::CursorArt(int monitorH)
    : m_unit(19.2 / 23.5 * std::max(1.0, monitorH / 1080.0))   // стрелка — как прежняя: 19,2 точки на 1080 строк
{
}

CursorArt::Image CursorArt::render(int shape, double seconds) const
{
    Image img;
    if (shape == stream::CursorHidden)
        return img;
    const std::vector<Part> parts = partsOf(shape);
    const int n = kSize;
    const double o = (n - 32 * m_unit) / 2;   // поле 32 × 32 — посередине картинки, вокруг — место для тени

    // «Мягкая»: обводка (вместе с заливкой — общий контур без швов), заливка, кайма заливки поверх (скругляет углы)
    std::vector<uint32_t> fg(size_t(n) * n, 0), sil(size_t(n) * n, 0);
    auto paint = [&](std::vector<uint32_t> &buf, bool silhouette) {
        cairo_surface_t *s = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char *>(buf.data()), kArgb32,
                                                                 n, n, n * 4);
        cairo_t *cr = cairo_create(s);
        cairo_translate(cr, o, o + (silhouette ? 1.3 * m_unit : 0));   // тень — ниже на 1,3
        cairo_scale(cr, m_unit, m_unit);
        cairo_set_line_join(cr, kJoinRound);
        cairo_set_line_cap(cr, kCapRound);
        cairo_set_line_width(cr, 2 * 1.05);
        if (silhouette)
            cairo_set_source_rgba(cr, 0, 0, 0, 1);
        else
            cairo_set_source_rgba(cr, 28 / 255.0, 24 / 255.0, 18 / 255.0, 0.88);
        drawParts(cr, parts, true, true);
        if (!silhouette) {
            cairo_set_source_rgba(cr, 0xf6 / 255.0, 0xf2 / 255.0, 0xea / 255.0, 1);
            drawParts(cr, parts, true, false);
            cairo_set_line_width(cr, 0.9);
            drawParts(cr, parts, false, false);
            if (shape == stream::CursorBusy)
                drawMorph(cr, 23, 23, 9, 1.4, seconds);
        } else if (shape == stream::CursorBusy) {
            cairo_set_line_width(cr, 3.2);
            cairo_arc(cr, 23, 23, 5, 0, 2 * M_PI);
            cairo_stroke(cr);
        }
        cairo_surface_flush(s);
        cairo_destroy(cr);
        cairo_surface_destroy(s);
    };
    paint(fg, false);
    paint(sil, true);

    // мягкая тень: силуэт, размытый (~1,5 единицы), чёрным .38 — под рисунком
    std::vector<float> a(sil.size());
    for (size_t i = 0; i < sil.size(); ++i)
        a[i] = float(sil[i] >> 24) / 255.0f;
    boxBlur(a, n, n, std::max(1, int(std::lround(1.5 * m_unit * 0.9))));
    for (size_t i = 0; i < fg.size(); ++i) {
        const double fa = (fg[i] >> 24) / 255.0, sa = 0.38 * a[i] * (1 - fa);
        const uint32_t alpha = uint32_t(std::lround(std::min(1.0, fa + sa) * 255));
        img.px[i] = alpha << 24 | (fg[i] & 0xffffff);   // цвет тени — чёрный: в умноженной альфе добавляет только альфу
    }
    double hx = 16, hy = 16;
    if (shape == stream::CursorArrow || shape == stream::CursorBusy)
        hx = 6, hy = 3.5;
    else if (shape == stream::CursorHand)
        hx = 14.2, hy = 3.3;
    img.hotX = int(std::lround(o + hx * m_unit));
    img.hotY = int(std::lround(o + hy * m_unit));
    return img;
}
