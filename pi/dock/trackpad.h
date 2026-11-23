// НаомиФон, приставка на Pi — трекпад: из касаний пальцев — курсор, щелчки, прокрутка и жесты (docs/PI.md, «Трекпад»).
// Сам по себе, без Linux: на вход — отчёты трекпада (пальцы в миллиметрах, размер пятна, сила, нажат ли щелчок), на
// выход — ход курсора и прокрутки в точках стола, кнопки, смахивания, щипок. Так его гоняют и на Mac — на записях
// настоящего трекпада и на выдуманных пальцах (pi/tests/trackpad_test.cpp).
//
// Что делает (образец поведения — трекпад Mac; числа — из замеров и из открытых описаний, docs/PI.md):
//  • один палец — курсор: палец встал — курсор стоит (мёртвая зона, пока палец не сдвинется явно), палец поставили,
//    сняли или нажали щелчок — курсор не прыгает; ускорение — кривая трекпада Apple;
//  • касание — щелчок (двумя пальцами — правый), щелчок двумя пальцами или в правом нижнем углу — правый;
//  • два пальца — прокрутка (ось держится) или щипок; лежащий большой палец и ладонь не считаются;
//  • три пальца — перетаскивание: пальцы сняли — кнопка ещё держится, можно переставить руку;
//  • смахивание четырьмя пальцами (тремя, если перетаскивание не тремя) — вбок, вверх, вниз;
//  • пока печатают на клавиатуре — новые касания не считаются.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

// Настройки (сообщение Pad со стола; по умолчанию — как трекпад на Mac пользователя)
struct PadConfig
{
    int speed = 50;          // указатель, 0…100: 50 — ползунок «Скорость» Mac в положении пользователя (0,6875)
    int accel = 1;           // 0 — без ускорения, 1 — кривая Mac, 2 — сильнее
    bool tap = true;         // касание — щелчок
    int right = 1;           // правый щелчок: 0 — нет, 1 — два пальца, 2 — правый нижний угол
    int drag = 1;            // перетаскивание: 0 — только нажатием, 1 — три пальца, 2 — двойное касание
    int holdMs = 700;        // три пальца сняли — кнопка держится ещё столько
    bool natural = true;     // прокрутка: страница за пальцами
    int scrollSpeed = 50;    // 0…100: 50 — как на Mac пользователя (0,3125)
    bool pinch = true;       // щипок — масштаб
    int jitter = 1;          // мёртвая зона стоящего пальца: 0 — 0,12 мм, 1 — 0,25, 2 — 0,4
    bool palm = true;        // лежащий палец и ладонь не считать
    bool typing = true;      // пауза при печати
    bool pin = true;         // курсор замирает на время щелчка
};

class Trackpad
{
public:
    struct Touch { int id = -1; double x = 0, y = 0; int major = 0, minor = 0, pressure = 0; };   // x, y — мм от левого верхнего угла
    enum Swipe { Left, Right, Up, Down };
    enum class Mode { Idle, Pointer, Wait2, Scroll, Pinch, Wait3, Drag, Hold, WaitSwipe, Done };

    PadConfig cfg;
    double width = 160, height = 115;   // мм

    void frame(int64_t t, const std::vector<Touch> &touches, bool click);   // отчёт трекпада; t — мкс
    void tick(int64_t t);       // сроки: отпустить щелчок касания, отпустить перетаскивание
    void typed(int64_t t);      // нажата клавиша с буквой
    void drop(int64_t t);       // трекпад пропал — отпустить всё

    std::function<void(double dx, double dy, int64_t t)> onMove;      // ход курсора, точки стола
    std::function<void(int buttons)> onButtons;                       // что держит трекпад: 1 — левая, 2 — правая
    // ход пальцев при прокрутке, точки стола (вниз и вправо — плюс); fingers false — пальцы сняли: дальше инерция
    std::function<void(double dx, double dy, int64_t t, bool fingers)> onScroll;
    std::function<void(Swipe dir, int fingers)> onSwipe;
    std::function<void(int steps)> onZoom;                            // щипок: +1 — крупнее, −1 — мельче

    Mode mode() const { return m_mode; }
    int fingers() const { return m_n; }
    int held() const { return m_sent; }                  // кнопки, что трекпад держит сейчас
    struct Seen { double x, y; bool counted; };          // палец на трекпаде: где и считается ли (не лежит, не ладонь)
    std::vector<Seen> seen() const
    {
        std::vector<Seen> out;
        for (const F &f : m_f)
            out.push_back({f.x, f.y, f.counted()});
        return out;
    }
    // точек стола на миллиметр пальца при такой его скорости (кривые трекпада Apple, docs/PI.md)
    static double pointerGain(double mmPerS, int speed, int accel);
    static double scrollGain(double mmPerS, int speed);

private:
    struct F
    {
        int id = -1;
        double x = 0, y = 0, px = 0, py = 0;     // сейчас и в прошлом отчёте
        double sx = 0, sy = 0;                   // где поставили
        double gx = 0, gy = 0;                   // где был, когда начался жест (сменилось число пальцев)
        double ax = 0, ay = 0, margin = 0;       // мёртвая зона: якорь и радиус
        bool held = true;                        // в мёртвой зоне
        bool land = true;                        // зона — от касания (пока оно может стать щелчком — шире)
        double travel = 0;                       // наибольшее удаление от места касания
        double dx = 0, dy = 0;                   // ход за отчёт как есть (после отсева скачков)
        int64_t t0 = 0, stillFrom = 0;
        double stx = 0, sty = 0;                 // откуда считается «стоит» (kStillMm)
        int64_t calmFrom = 0;                    // с каких пор не уходил дальше 1,5 мм (cx, cy) — «лежит»
        double cx = 0, cy = 0;
        int major = 0, minor = 0, pressure = 0, pmajor = 0, ppressure = 0, frames = 0;
        bool palm = false, typing = false, rest = false, seen = false, fresh = true;
        double rx = 0, ry = 0;                   // где признан лежащим
        bool counted() const { return !palm && !typing && !rest; }
    };
    struct Rate   // скорость пальца, мм/с, за последние ~50 мс
    {
        std::deque<std::pair<int64_t, double>> hist;
        double add(int64_t t, double mm);
        void clear() { hist.clear(); }
    };

    F *find(int id);
    std::vector<F *> counted();
    void classify(int64_t t, bool landed);
    void regroup(int64_t t, const std::vector<F *> &act, Mode was);
    bool deadZone(F &f, double &ex, double &ey);
    double restMargin() const;
    void anchor(F &f, double margin);
    void move(double ex, double ey, double mm, int64_t t);
    void buttons();
    void clickChanged(int64_t t, bool down, const std::vector<F *> &act);
    void tapEnd(int64_t t);
    void releaseDrag();
    bool inCorner(const F &f) const;

    std::vector<F> m_f;
    std::vector<int> m_ids;        // пальцы, что считались в прошлом отчёте
    Mode m_mode = Mode::Idle;
    int m_n = 0;
    int64_t m_modeAt = 0;          // когда начался жест (сменилось число пальцев)
    int m_skip = 0;                // отчётов без хода после смены числа пальцев
    int64_t m_now = 0;
    Rate m_rate;
    double m_lastSpeed = 0;        // скорость пальца, что ведёт курсор, мм/с
    // щелчок
    bool m_click = false;
    int m_clickBit = 0, m_tapBit = 0, m_sent = 0;
    bool m_drag = false;           // перетаскивание тремя пальцами держит левую
    int64_t m_tapReleaseAt = 0, m_holdUntil = 0, m_holdTouchAt = 0, m_dragOneAt = 0;
    // касание
    bool m_tapAlive = false, m_tapSeq = false;
    int m_tapFingers = 0;
    int64_t m_tapAt = 0;
    bool m_tapCorner = false;
    int m_tapDrag = 0;             // двойное касание: 0 — нет, 1 — ждём второе, 2 — второе на трекпаде, 3 — тащим
    // недавний ход курсора (для отката съезда при щелчке): время, точки
    std::deque<std::tuple<int64_t, double, double, double>> m_moved;
    // прокрутка
    int m_axis = 0;                // 0 — не решено, 1 — по вертикали, 2 — по горизонтали, 3 — свободно
    double m_accX = 0, m_accY = 0, m_cross = 0;
    // щипок
    double m_pinchRef = 0;
    // печать
    int64_t m_typingUntil = 0, m_typedAt = 0;
};
