// cairo 1.18 и pango 1.56 (Debian 13) для часов без дока (clock.cpp) и курсора (cursor.cpp): библиотеки системные,
// заголовков (-dev) на Pi нет — объявления по cairo.h, pango-layout.h, pangocairo.h; все объекты — непрозрачные
// указатели. И размытие для мягких теней.
#pragma once

#include <algorithm>
#include <vector>

extern "C" {
typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _PangoLayout PangoLayout;
typedef struct _PangoFontDescription PangoFontDescription;
struct PangoRectangle { int x, y, width, height; };
cairo_surface_t *cairo_image_surface_create_for_data(unsigned char *data, int format, int width, int height, int stride);
cairo_t *cairo_create(cairo_surface_t *target);
void cairo_destroy(cairo_t *cr);
void cairo_surface_destroy(cairo_surface_t *surface);
void cairo_surface_flush(cairo_surface_t *surface);
void cairo_set_source_rgba(cairo_t *cr, double r, double g, double b, double a);
void cairo_move_to(cairo_t *cr, double x, double y);
void cairo_line_to(cairo_t *cr, double x, double y);
void cairo_curve_to(cairo_t *cr, double x1, double y1, double x2, double y2, double x3, double y3);
void cairo_arc(cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2);
void cairo_arc_negative(cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2);
void cairo_close_path(cairo_t *cr);
void cairo_new_path(cairo_t *cr);
void cairo_fill(cairo_t *cr);
void cairo_fill_preserve(cairo_t *cr);
void cairo_stroke(cairo_t *cr);
void cairo_set_line_width(cairo_t *cr, double width);
void cairo_set_line_join(cairo_t *cr, int join);
void cairo_set_line_cap(cairo_t *cr, int cap);
void cairo_set_fill_rule(cairo_t *cr, int rule);
void cairo_save(cairo_t *cr);
void cairo_restore(cairo_t *cr);
void cairo_translate(cairo_t *cr, double tx, double ty);
void cairo_scale(cairo_t *cr, double sx, double sy);
void cairo_rotate(cairo_t *cr, double angle);
PangoLayout *pango_cairo_create_layout(cairo_t *cr);
void pango_cairo_show_layout(cairo_t *cr, PangoLayout *layout);
void pango_layout_set_markup(PangoLayout *layout, const char *markup, int length);
void pango_layout_get_pixel_extents(PangoLayout *layout, PangoRectangle *ink, PangoRectangle *logical);
void g_object_unref(void *object);
}

constexpr int kArgb32 = 0;          // CAIRO_FORMAT_ARGB32: умноженная альфа, порядок как DRM ARGB8888
constexpr int kJoinRound = 1;       // CAIRO_LINE_JOIN_ROUND
constexpr int kCapRound = 1;        // CAIRO_LINE_CAP_ROUND
constexpr int kFillEvenOdd = 1;     // CAIRO_FILL_RULE_EVEN_ODD

// размытие — трижды по коробке радиуса r (почти гауссово), строки и столбцы
inline void boxBlur(std::vector<float> &a, int w, int h, int r)
{
    std::vector<float> tmp(a.size());
    const float k = 1.0f / float(2 * r + 1);
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {   // строки
            const float *src = &a[size_t(y) * w];
            float *dst = &tmp[size_t(y) * w];
            float acc = 0;
            for (int x = -r; x <= r; ++x)
                acc += src[std::clamp(x, 0, w - 1)];
            for (int x = 0; x < w; ++x) {
                dst[x] = acc * k;
                acc += src[std::min(x + r + 1, w - 1)] - src[std::max(x - r, 0)];
            }
        }
        for (int x = 0; x < w; ++x) {   // столбцы
            float acc = 0;
            for (int y = -r; y <= r; ++y)
                acc += tmp[size_t(std::clamp(y, 0, h - 1)) * w + x];
            for (int y = 0; y < h; ++y) {
                a[size_t(y) * w + x] = acc * k;
                acc += tmp[size_t(std::min(y + r + 1, h - 1)) * w + x] - tmp[size_t(std::max(y - r, 0)) * w + x];
            }
        }
    }
}
