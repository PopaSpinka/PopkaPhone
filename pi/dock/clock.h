// НаомиФон, приставка на Pi — часы с датой для экрана без дока (lock.h, docs/PI.md, «Без дока»): рисуются своим
// потоком (cairo + pango Raspberry Pi OS, шрифт Google Sans Flex — как часы телефона) в буфер ARGB с умноженной
// альфой — его берёт слой дисплея. Раз в минуту — заново (два буфера: пока один на экране, рисуется другой). Вид — как
// часы и дата главного экрана телефона (выбор пользователя, 17 ноября 2026).
#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class ClockFace
{
public:
    struct Buffer { uint32_t *pixels = nullptr; int pitch = 0, w = 0, h = 0; };   // ARGB8888 (умноженная альфа)
    // pt — пикселей монитора в точке стола (размеры — в точках стола, как у панели); screenW × screenH — монитор
    void start(Buffer a, Buffer b, double pt, int screenW, int screenH);
    void stop();
    ~ClockFace() { stop(); }
    void draw(int64_t minute);   // нарисовать (своим потоком); minute — минута по местным часам (time()/60)
    void setPt(double pt);       // стол сменил размер — со следующей картинки
    // где часы (настройки «Без дока»): левый верх чернил — доля ширины и высоты монитора; часы с тенью — целиком на
    // экране (ближе к краю, чем поле тени, — не подходят). По умолчанию — слева 44 точки стола 1600, сверху 34
    void setPos(double fx, double fy);
    int fd() const { return m_event; }        // готово: take()
    // готовая картинка: какой буфер, сколько в нём занято, где её левый верхний угол на экране (с полями тени)
    struct Ready { int buffer = -1, w = 0, h = 0, x = 0, y = 0; int64_t minute = 0; };
    bool take(Ready &out);

private:
    void run();
    struct Place { double pt, fx, fy; int screenW, screenH; };
    static Ready render(const Place &at, int64_t minute, Buffer &to);
    Buffer m_buf[2];
    double m_pt = 1, m_fx = 44.0 / 1600, m_fy = 34.0 / 900;
    int m_screenW = 0, m_screenH = 0;
    std::thread m_thread;
    std::mutex m_m;
    std::condition_variable m_cv;
    bool m_quit = false, m_want = false;
    int m_shown = -1;   // буфер на экране: рисовать в другой
    int64_t m_wantMinute = 0;
    Ready m_ready;
    bool m_haveReady = false;
    int m_event = -1;
};
