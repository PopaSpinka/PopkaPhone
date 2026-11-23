// НаомиФон, приставка на Pi — экран без дока (docs/PI.md, «Без дока»): часы с датой на обоях стола. Вид — свои слои
// дисплея поверх кадра (display.h): часы (clock.h) и покров — чистые обои, когда телефона нет (кадр с панелью и
// окнами под ними). Обои — последний кадр стола при выходе из дока (опорный HEVC, только обои: панель и окна телефон
// уже убрал), на диске — /var/lib/naomi-dock/desk.hevc: с ним экран без дока есть и после перезагрузки Pi, и когда
// телефон пропал сам. Всё движется по времени обновлений монитора — кадр в кадр с анимацией стола телефона.
#pragma once

#include "clock.h"
#include "decoder.h"
#include "display.h"

#include <cstdint>
#include <string>
#include <vector>

struct AVFrame;

class LockScreen
{
public:
    ~LockScreen() { close(); }
    bool open(Display &display, double pt);   // pt — пикселей монитора в точке стола
    void close();
    void setPt(double pt);                    // стол сменил размер: часы — в точках нового
    void setClockShown(bool shown);           // часы на обоях (настройки стола «Без дока»); нет — одни обои
    void setClockPos(double fx, double fy);   // где часы: левый верх надписи — доля монитора (clock.h)
    int fd() const { return m_face.fd(); }    // картинка часов готова: poll()
    void poll(int64_t now);                    // главный цикл: новая минута — перерисовать, готовое — на экран

    // Часы: проявиться (180 мс, как меню N) / уйти (140 мс) с обновления монитора at; at 0 — сразу, без анимации.
    // Только прозрачностью и сдвигом на 4 % своей высоты, без масштаба (lock.cpp, layersAt)
    void clockIn(int64_t at);
    void clockOut(int64_t at);
    // Покров — обои поверх кадра за dur с обновления at (пропал телефон); убрать — растаять (под ним уже живой стол).
    // Обоев нет — чёрный
    void coverIn(int64_t at, int64_t dur);
    void coverOut(int64_t at, int64_t dur);
    bool covered() const { return m_coverTo > 0; }
    void cursorTo(bool shown, int64_t at, int64_t dur);   // курсор Pi: проявить / растаять
    bool busy(int64_t now) const;                        // идёт анимация (или начнётся)

    void saveWallpaper(const uint8_t *hevc, size_t n);   // чистый кадр стола (опорный, Annex B) — на диск

private:
    struct Anim {
        int64_t start = 0, dur = 0;
        double from = 0, to = 0;
        double at(int64_t t) const;     // OutCubic, как анимации стола
        void go(double target, int64_t t0, int64_t d);
    };
    void layersAt(int64_t vblank, Display::Layer *layers, double &cursorAlpha);
    bool loadWallpaper();
    void dropWallpaper();

    Display *m_display = nullptr;
    double m_pt = 1;
    ClockFace m_face;
    uint32_t m_clockFb[2] = {};
    ClockFace::Ready m_shown;              // картинка часов на экране (buffer −1 — ещё нет)
    int64_t m_minute = 0;
    Anim m_clock, m_cover, m_cursor;
    double m_coverTo = 0;                  // куда идёт покров (0 — скрыт)
    bool m_clockShown = true;

    // обои
    Decoder m_dec;
    AVFrame *m_wall = nullptr;
    uint32_t m_wallFb = 0, m_wallHandles[4] = {};
    bool m_wallTried = false;
};
