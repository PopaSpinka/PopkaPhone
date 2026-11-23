#include "trackpad.h"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace {

constexpr int64_t kMs = 1000;
// касание: палец пробыл не дольше и ушёл не дальше — щелчок (числа libinput: 180 мс и 1,3 мм; срок — с запасом на
// неровные метки отчётов Bluetooth)
constexpr int64_t kTapUs = 200 * kMs;
constexpr double kTapMove = 1.3;
constexpr int64_t kTapPulseUs = 20 * kMs;      // щелчок касания: нажато — отпущено
constexpr int64_t kTapDragUs = 200 * kMs;      // двойное касание: первое держит кнопку столько, ждёт второе
constexpr double kLandMove = 0.4;              // без касания-щелчка: палец поставили — курсор стоит до такого хода
constexpr double kPinMove = 1.5;               // щелчок нажат — курсор стоит, пока палец не уйдёт на столько (libinput)
constexpr double kUnpinMove = 0.6;             // щелчок отпустили
constexpr double kAfterGesture = 2.0;          // прокрутку кончили, один палец остался — курсор стоит до такого хода
constexpr double kJump = 20;                   // за отчёт дальше — не ход, а сбой (libinput)
constexpr int64_t kStillUs = 80 * kMs;         // палец не ушёл за kStillMm за столько — стоит: снова мёртвая зона
constexpr double kStillMm = 0.08;
constexpr int64_t kUndoUs = 80 * kMs;          // щелчок: съезд курсора за столько до него — назад,
constexpr double kUndoMm = 0.8;                // если палец за это время прошёл не больше
constexpr int kPalmMajor = 900;                // пятно шире — ладонь (число libinput для этого трекпада)
constexpr double kRestZone = 0.78;             // ниже этой доли высоты лежит большой палец
constexpr int64_t kRestUs = 300 * kMs;
constexpr double kRestWake = 8;                // лежащий палец ушёл на столько — он не лежит
constexpr int64_t kDragStartUs = 60 * kMs;     // три пальца: тащить — не раньше (четвёртый палец смахивания успеет лечь)
constexpr double kDragStartMm = 1.0;
constexpr int64_t kDragOneUs = 100 * kMs;      // тащили тремя, остался один на столько — отпустить
constexpr int64_t kHoldProbeUs = 120 * kMs;    // кнопка держится, легло меньше трёх пальцев на столько — отпустить
constexpr double kHoldProbeMm = 1.5;
constexpr double kSwipeMm = 8;                 // смахивание: пальцы прошли столько — сработало (без возвратов)
constexpr double kPinchStep = 1.18;            // щипок: расстояние между пальцами изменилось во столько раз — шаг

// Кривые ускорения трекпада Apple — как в macOS (свойства HIDAccelCurves и HIDScrollAccelCurves драйвера трекпада,
// прочитаны с Mac пользователя 22 ноября 2026: ioreg; формула — IOHIDFamily, открытый код Apple). idx — положение
// ползунка скорости; скорость пальца x — в дюймах в секунду, ответ — скорость курсора в дюймах в секунду при 96
// точках на дюйм. До tanLin — многочлен, до tanRoot — касательная, дальше — корень (рост замедляется).
struct Curve { double idx, lin, par, cub, tanLin, tanRoot; };
const Curve kPointerCurves[10] = {
    {0, 1.00, 0, 0, 7.4, 21},        {0.125, 0.99, 0.50, 0.08, 7.3, 20}, {0.5, 0.98, 0.66, 0.10, 7.2, 19},
    {0.6875, 0.96, 0.83, 0.12, 7.1, 18}, {0.875, 0.94, 1.00, 0.15, 7.0, 17}, {1.0, 0.92, 1.15, 0.18, 7.0, 16},
    {1.5, 0.89, 1.30, 0.21, 7.0, 15},    {2.0, 0.86, 1.45, 0.24, 7.0, 14},   {2.5, 0.83, 1.66, 0.28, 7.0, 13},
    {3.0, 1.00, 1.88, 0.36, 7.0, 12}};
const Curve kScrollCurves[10] = {
    {0, 1.00, 0, 0, 6.0, 12},        {0.125, 0.95, 0.6, 0, 6.2, 12}, {0.5, 0.90, 0.9, 0, 6.4, 12},
    {0.6875, 0.85, 1.2, 0, 6.6, 12}, {0.875, 0.80, 1.4, 0, 6.8, 12}, {1.0, 0.75, 1.6, 0, 7.0, 12},
    {1.5, 0.70, 1.8, 0, 7.2, 12},    {2.0, 0.65, 2.0, 0, 7.4, 12},   {2.5, 0.60, 2.2, 0, 7.6, 12},
    {3.0, 0.55, 2.4, 0, 7.8, 12}};

Curve curveAt(const Curve *tab, double idx)
{
    idx = std::clamp(idx, tab[0].idx, tab[9].idx);
    int i = 1;
    while (i < 9 && tab[i].idx < idx)
        ++i;
    const Curve &a = tab[i - 1], &b = tab[i];
    const double k = (idx - a.idx) / (b.idx - a.idx);
    auto mix = [k](double u, double v) { return u + (v - u) * k; };
    return {idx, mix(a.lin, b.lin), mix(a.par, b.par), mix(a.cub, b.cub), mix(a.tanLin, b.tanLin), mix(a.tanRoot, b.tanRoot)};
}

double curveSpeed(const Curve &c, double x, double boost)
{
    const double p = c.par * boost, q = c.cub * boost;
    auto poly = [&](double v) { return c.lin * v + p * p * v * v + q * q * q * v * v * v; };
    if (x <= c.tanLin)
        return poly(x);
    const double y0 = poly(c.tanLin);
    const double m0 = c.lin + 2 * p * p * c.tanLin + 3 * q * q * q * c.tanLin * c.tanLin;
    if (x <= c.tanRoot)
        return y0 + m0 * (x - c.tanLin);
    const double y1 = y0 + m0 * (c.tanRoot - c.tanLin);
    const double m1 = 2 * y1 * m0;
    return std::sqrt(m1 * x + (y1 * y1 - m1 * c.tanRoot));
}

// настройка 0…100 → положение ползунка Mac: 50 — положение пользователя (mid), 0 и 100 — края ползунка
double sliderIndex(const Curve *tab, int percent, double midTick)
{
    const double pos = percent <= 50 ? percent / 50.0 * midTick : midTick + (percent - 50) / 50.0 * (9 - midTick);
    const int i = std::clamp(int(pos), 0, 8);
    return tab[i].idx + (tab[i + 1].idx - tab[i].idx) * (pos - i);
}

double gain(const Curve *tab, double idx, double mmPerS, double boost)
{
    const double x = std::max(mmPerS, 0.5) / 25.4;
    return curveSpeed(curveAt(tab, idx), x, boost) / x * 96 / 25.4;
}

} // namespace

double Trackpad::pointerGain(double mmPerS, int speed, int accel)
{
    const double idx = sliderIndex(kPointerCurves, std::clamp(speed, 0, 100), 3);   // 0,6875 — четвёртое деление
    if (accel == 0)   // без ускорения: всегда как при спокойном ходе (2 дюйма в секунду)
        return gain(kPointerCurves, idx, 2 * 25.4, 1);
    return gain(kPointerCurves, idx, mmPerS, accel == 2 ? 1.35 : 1);
}

double Trackpad::scrollGain(double mmPerS, int speed)
{
    return gain(kScrollCurves, sliderIndex(kScrollCurves, std::clamp(speed, 0, 100), 1.5), mmPerS, 1);   // 0,3125
}

double Trackpad::Rate::add(int64_t t, double mm)
{
    hist.push_back({t, mm});
    double sum = 0;
    int64_t from = t - 11 * kMs;   // после паузы — как за один отчёт (~11 мс у Magic Trackpad)
    for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
        if (t - it->first >= 50 * kMs) {
            from = it->first;
            break;
        }
        sum += it->second;
    }
    while (hist.size() > 1 && t - hist.front().first > 120 * kMs)
        hist.pop_front();
    return sum / std::clamp((t - from) / 1e6, 0.005, 0.1);
}

Trackpad::F *Trackpad::find(int id)
{
    for (F &f : m_f)
        if (f.id == id)
            return &f;
    return nullptr;
}

std::vector<Trackpad::F *> Trackpad::counted()
{
    std::vector<F *> out;
    for (F &f : m_f)
        if (f.counted())
            out.push_back(&f);
    std::sort(out.begin(), out.end(), [](const F *a, const F *b) { return a->id < b->id; });
    return out;
}

double Trackpad::restMargin() const
{
    return cfg.jitter <= 0 ? 0.12 : cfg.jitter == 1 ? 0.25 : 0.4;
}

void Trackpad::anchor(F &f, double margin)
{
    f.ax = f.x, f.ay = f.y;
    f.margin = margin;
    f.held = true;
    f.land = false;
}

bool Trackpad::inCorner(const F &f) const
{
    return f.x > 0.72 * width && f.y > 0.70 * height;
}

void Trackpad::buttons()
{
    const int b = m_clickBit | m_tapBit | (m_drag ? 1 : 0);
    if (b == m_sent)
        return;
    m_sent = b;
    if (onButtons)
        onButtons(b);
}

void Trackpad::releaseDrag()
{
    m_drag = false;
    m_holdUntil = m_holdTouchAt = m_dragOneAt = 0;
    buttons();
}

void Trackpad::typed(int64_t t)
{
    // одна клавиша — пауза короткая, печатают подряд — длиннее (сроки libinput: 200 и 500 мс)
    m_typingUntil = t + (t - m_typedAt < 2000 * kMs ? 500 : 200) * kMs;
    m_typedAt = t;
}

void Trackpad::drop(int64_t t)
{
    if (m_mode == Mode::Scroll && onScroll)
        onScroll(0, 0, t, false);
    m_f.clear();
    m_ids.clear();
    m_mode = Mode::Idle;
    m_n = 0;
    m_click = m_drag = m_tapSeq = m_tapAlive = false;
    m_clickBit = m_tapBit = m_tapDrag = 0;
    m_tapReleaseAt = m_holdUntil = m_holdTouchAt = m_dragOneAt = 0;
    buttons();
}

void Trackpad::tick(int64_t t)
{
    if (m_tapReleaseAt && t >= m_tapReleaseAt) {   // щелчок касания — отпустить (двойное касание: второго не дождались)
        m_tapReleaseAt = 0;
        m_tapBit = 0;
        m_tapDrag = 0;
        buttons();
    }
    // три пальца сняли и не вернули — отпустить; пальцы легли и молчат дольше — тоже
    if (m_mode == Mode::Hold && m_holdUntil && t >= m_holdUntil + (m_n ? 300 * kMs : 0)) {
        releaseDrag();
        m_mode = m_n ? Mode::Done : Mode::Idle;
    }
}

// Мёртвая зона: палец стоит — хода нет, пока не уйдёт от якоря дальше радиуса; вышел — ход начинается с края зоны
// (без скачка) и дальше идёт как есть; снова встал — зона снова
bool Trackpad::deadZone(F &f, double &ex, double &ey)
{
    if (f.held) {
        double m = f.margin;
        if (f.land && m_now - f.t0 > kTapUs)   // касанием уже не станет — зона обычная
            m = std::min(m, std::max(restMargin(), kLandMove));
        const double dx = f.x - f.ax, dy = f.y - f.ay, d = std::hypot(dx, dy);
        if (d <= m)
            return false;
        f.held = f.land = false;
        f.stx = f.x, f.sty = f.y, f.stillFrom = m_now;
        ex = dx * (d - m) / d, ey = dy * (d - m) / d;
        return true;
    }
    if (m_now - f.stillFrom >= kStillUs) {
        anchor(f, restMargin());
        return false;
    }
    ex = f.dx, ey = f.dy;
    return ex != 0 || ey != 0;
}

void Trackpad::move(double ex, double ey, double mm, int64_t t)
{
    m_lastSpeed = m_rate.add(t, mm);
    const double g = pointerGain(m_lastSpeed, cfg.speed, cfg.accel);
    m_moved.emplace_back(t, ex * g, ey * g, mm);
    while (!m_moved.empty() && t - std::get<0>(m_moved.front()) > 250 * kMs)
        m_moved.pop_front();
    if (onMove)
        onMove(ex * g, ey * g, t);
}

// Кто не считается: ладонь (широкое пятно), касание во время печати, лежащий палец
void Trackpad::classify(int64_t t, bool landed)
{
    for (F &f : m_f)
        if (cfg.palm && f.major >= kPalmMajor)
            f.palm = true;
    if (landed) {
        std::vector<F *> old;
        for (F &f : m_f)
            if (!f.fresh && f.counted())
                old.push_back(&f);
        for (F &f : m_f) {
            if (!f.fresh)
                continue;
            if (cfg.typing && old.empty() && t < m_typingUntil)
                f.typing = true;
            // один палец ведёт курсор, новый лёг далеко от него — случайный (правило libinput: 35 × 25 мм)
            if (cfg.palm && old.size() == 1 && m_mode == Mode::Pointer && !old[0]->held && m_lastSpeed > 20
                && (std::fabs(f.x - old[0]->x) > 35 || std::fabs(f.y - old[0]->y) > 25))
                f.rest = true, f.rx = f.x, f.ry = f.y;
        }
        // лежал один внизу и не двигался — и тут поставили другой: лежащий не считается
        if (cfg.palm && old.size() == 1 && old[0]->y > kRestZone * height && t - old[0]->calmFrom >= kRestUs)
            old[0]->rest = true, old[0]->rx = old[0]->x, old[0]->ry = old[0]->y;
    }
    // лежащий поехал — обычный палец: один на трекпаде — с 1,5 мм, при других пальцах — с kRestWake
    bool others = false;
    for (const F &f : m_f)
        others = others || f.counted();
    for (F &f : m_f)
        if (f.rest && std::hypot(f.x - f.rx, f.y - f.ry) > (others ? kRestWake : 1.5)) {
            f.rest = false;
            anchor(f, restMargin());
            others = true;
        }
}

// Число пальцев сменилось — жест заново
void Trackpad::regroup(int64_t t, const std::vector<F *> &act, Mode was)
{
    const int n = int(act.size());
    if (was == Mode::Scroll && onScroll)
        onScroll(0, 0, t, false);
    m_rate.clear();
    m_lastSpeed = 0;
    m_modeAt = t;
    m_skip = 1;
    m_axis = 0;
    m_accX = m_accY = m_cross = 0;
    const bool after = was == Mode::Scroll || was == Mode::Pinch || was == Mode::Drag;
    for (F *f : act) {
        f->gx = f->x, f->gy = f->y;
        if (f->fresh) {
            anchor(*f, cfg.tap ? kTapMove : kLandMove);
            f->land = true;
        } else if (was != Mode::Pointer) {   // жест кончился — курсор с места, где палец сейчас (после прокрутки — зона шире)
            anchor(*f, n == 1 && after ? kAfterGesture : restMargin());
        }
    }
    Mode m;
    if (n == 0) {
        m = Mode::Idle;
        if (m_drag) {   // три пальца сняли — кнопка ещё держится
            m = Mode::Hold;
            if (was == Mode::Drag)
                m_holdUntil = t + int64_t(cfg.holdMs) * kMs;
            m_holdTouchAt = 0;
            if (cfg.holdMs <= 0)
                releaseDrag(), m = Mode::Idle;
        }
    } else if (m_drag && was == Mode::Hold) {
        if (n == 3) {
            m = Mode::Drag;   // вернули три пальца — тащим дальше
        } else if (n > 3) {
            releaseDrag();
            m = Mode::WaitSwipe;
        } else {
            m = Mode::Hold;   // легло меньше трёх: ждём остальных (kHoldProbeUs)
            if (!m_holdTouchAt)
                m_holdTouchAt = t;
        }
    } else if (m_drag) {      // тащим: двумя — тоже, одним — недолго
        if (n > 3) {
            releaseDrag();
            m = Mode::WaitSwipe;
        } else {
            m = Mode::Drag;
            m_dragOneAt = n == 1 ? t : 0;
        }
    } else if (was == Mode::WaitSwipe || was == Mode::Done) {
        m = Mode::Done;       // смахивание: пальцы снимают по одному — ничего, пока не снимут все
    } else if (n == 1) {
        m = Mode::Pointer;
    } else if (n == 2) {
        m = Mode::Wait2;
    } else if (n == 3) {
        m = cfg.drag == 1 ? Mode::Wait3 : Mode::WaitSwipe;
    } else {
        m = n == 4 ? Mode::WaitSwipe : Mode::Done;
    }
    m_mode = m;
    m_n = n;
}

// Щелчок трекпада: какая кнопка — по пальцам; курсор на время щелчка замирает
void Trackpad::clickChanged(int64_t t, bool down, const std::vector<F *> &act)
{
    m_click = down;
    if (!down) {
        m_clickBit = 0;
        buttons();
        for (F &f : m_f)
            anchor(f, cfg.pin ? kUnpinMove : restMargin());
        m_ids.clear();      // жест — заново по числу пальцев
        m_mode = Mode::Idle;
        return;
    }
    m_tapAlive = false;
    if (m_tapBit) {         // щелчок касания ещё держится — отпустить
        m_tapBit = 0;
        m_tapReleaseAt = 0;
        m_tapDrag = 0;
    }
    if (m_drag)
        releaseDrag();
    if (m_mode == Mode::Scroll && onScroll)
        onScroll(0, 0, t, false);
    m_mode = Mode::Idle;
    int n = int(act.size());
    bool corner = false;
    for (const F *f : act)
        corner = corner || inCorner(*f);
    if (n == 2) {
        // большой палец жмёт снизу, указательный лежит выше — это щелчок одним пальцем, а не двумя
        const F &lo = act[0]->y > act[1]->y ? *act[0] : *act[1], &hi = act[0]->y > act[1]->y ? *act[1] : *act[0];
        if (std::hypot(lo.x - hi.x, lo.y - hi.y) > 45 || (lo.y > kRestZone * height && lo.y - hi.y > 20))
            n = 1;
    }
    m_clickBit = (cfg.right == 1 && n >= 2) || (cfg.right == 2 && corner) ? 2 : 1;
    if (cfg.pin) {
        // палец давит — пятно ползёт, курсор съезжает: что съехало перед щелчком — назад; дальше стоит, пока палец
        // не уйдёт явно
        double ux = 0, uy = 0, mm = 0;
        for (const auto &[mt, dx, dy, d] : m_moved)
            if (t - mt <= kUndoUs)
                ux += dx, uy += dy, mm += d;
        if (mm > 0 && mm <= kUndoMm && onMove)
            onMove(-ux, -uy, t);
        for (F &f : m_f)
            anchor(f, kPinMove);
    }
    m_moved.clear();
    buttons();
}

// Все пальцы сняты: было ли это касание
void Trackpad::tapEnd(int64_t t)
{
    if (!m_tapSeq)
        return;
    m_tapSeq = false;
    const bool tap = m_tapAlive && t - m_tapAt <= kTapUs + 30 * kMs;
    if (!tap) {
        if (m_tapDrag >= 2) {   // двойное касание: тащили и отпустили
            m_tapBit = 0;
            m_tapDrag = 0;
            buttons();
        }
        return;
    }
    if (m_drag) {               // кнопка трёх пальцев держалась — касание её отпускает
        releaseDrag();
        m_mode = Mode::Idle;
    }
    int bit = 0;
    if (m_tapFingers == 1)
        bit = cfg.right == 2 && m_tapCorner ? 2 : 1;
    else if (m_tapFingers == 2 && cfg.right == 1)
        bit = 2;
    if (!bit)
        return;
    if (m_tapBit) {             // второе касание двойного: первый щелчок отпустить, второй — следом
        m_tapBit = 0;
        buttons();
    }
    m_tapBit = bit;
    buttons();
    m_tapDrag = cfg.drag == 2 && bit == 1 ? 1 : 0;
    m_tapReleaseAt = t + (m_tapDrag ? kTapDragUs : kTapPulseUs);
}

void Trackpad::frame(int64_t t, const std::vector<Touch> &in, bool click)
{
    m_now = t;
    // ---------- пальцы: новые, прежние, снятые ----------
    for (F &f : m_f)
        f.seen = false;
    bool landed = false;
    for (const Touch &c : in) {
        F *f = find(c.id);
        if (!f) {
            m_f.emplace_back();
            f = &m_f.back();
            f->id = c.id;
            f->x = f->px = f->sx = f->gx = f->ax = f->stx = f->cx = c.x;
            f->y = f->py = f->sy = f->gy = f->ay = f->sty = f->cy = c.y;
            f->t0 = f->stillFrom = f->calmFrom = t;
            f->major = c.major, f->pressure = c.pressure;
            f->margin = cfg.tap ? kTapMove : kLandMove;
            f->land = true;
            landed = true;
        } else {
            f->px = f->x, f->py = f->y;
            f->x = c.x, f->y = c.y;
            f->fresh = false;
        }
        f->pmajor = f->major, f->ppressure = f->pressure;
        f->major = c.major, f->minor = c.minor, f->pressure = c.pressure;
        f->seen = true;
        ++f->frames;
        f->travel = std::max(f->travel, std::hypot(f->x - f->sx, f->y - f->sy));
        f->dx = f->x - f->px, f->dy = f->y - f->py;
        // не ход: скачок; пятно резко сжалось или сила упала — палец снимают, пятно ползёт
        const bool lifting = (f->pmajor > 0 && f->major < f->pmajor * 0.75) || (f->ppressure > 24 && f->pressure < f->ppressure * 0.6);
        if (std::hypot(f->dx, f->dy) > kJump || lifting) {
            if (f->held)
                f->ax += f->dx, f->ay += f->dy;
            f->dx = f->dy = 0;
        }
        if (std::hypot(f->x - f->stx, f->y - f->sty) > kStillMm)
            f->stx = f->x, f->sty = f->y, f->stillFrom = t;
        if (std::hypot(f->x - f->cx, f->y - f->cy) > 1.5)
            f->cx = f->x, f->cy = f->y, f->calmFrom = t;
    }
    m_f.erase(std::remove_if(m_f.begin(), m_f.end(), [](const F &f) { return !f.seen; }), m_f.end());
    classify(t, landed);
    std::vector<F *> act = counted();
    const int n = int(act.size());
    std::vector<int> ids;
    for (const F *f : act)
        ids.push_back(f->id);

    // ---------- касание: от первого пальца до пустого трекпада ----------
    bool allFresh = n > 0;   // все, кто считается, только что легли (прежние — лежащий палец, ладонь) — касание заново
    for (const F *f : act)
        allFresh = allFresh && f->fresh;
    if (allFresh && m_tapDrag != 2 && m_tapDrag != 3)
        m_tapSeq = false;
    if (n > 0 && !m_tapSeq) {
        m_tapSeq = true;
        m_tapAlive = cfg.tap && !m_click && !click;
        m_tapFingers = 0;
        m_tapAt = t;
        m_tapCorner = false;
        if (m_tapDrag == 1) {   // двойное касание: второе легло, пока первое держит кнопку
            m_tapDrag = 2;
            m_tapReleaseAt = 0;
        }
    }
    if (m_tapSeq && n > 0) {
        if (n > m_tapFingers)   // ещё палец — срок заново
            m_tapFingers = n, m_tapAt = t;
        for (const F *f : act)
            if (f->travel > kTapMove)
                m_tapAlive = false;
        if (t - m_tapAt > kTapUs)
            m_tapAlive = false;
        if (m_tapFingers == 1)
            m_tapCorner = inCorner(*act[0]);
        if (m_tapDrag == 2 && n > 1) {          // два пальца — это не перетаскивание
            m_tapBit = 0;
            m_tapDrag = 0;
            buttons();
        } else if (m_tapDrag == 2 && !m_tapAlive) {
            m_tapDrag = 3;                      // повели или держат — тащим
        }
    }

    // ---------- щелчок ----------
    if (click != m_click)
        clickChanged(t, click, act);
    if (m_click) {   // щелчок держат: курсор ведёт тот палец, что движется (перетаскивание нажатием)
        if (ids != m_ids) {
            m_ids = ids;
            m_n = n;
            for (F *f : act)
                if (f->fresh)
                    anchor(*f, cfg.pin ? kPinMove : kLandMove);
            if (n == 0)
                tapEnd(t);
            return;
        }
        double bx = 0, by = 0, best = 0, mm = 0;
        for (F *f : act) {
            double ex = 0, ey = 0;
            if (deadZone(*f, ex, ey) && std::hypot(ex, ey) > best)
                best = std::hypot(ex, ey), bx = ex, by = ey, mm = std::hypot(f->dx, f->dy);
        }
        if (best > 0)
            move(bx, by, mm, t);
        return;
    }

    // ---------- число пальцев сменилось ----------
    if (ids != m_ids) {
        m_ids = ids;
        regroup(t, act, m_mode);
        if (n == 0)
            tapEnd(t);
        return;
    }
    if (n == 0) {
        tapEnd(t);
        return;
    }
    if (m_skip > 0) {   // отчёт после смены — без хода: оставшиеся пальцы от перестановки сдвигаются рывком
        --m_skip;
        return;
    }
    auto average = [&](bool fromStart, double &ax, double &ay) {
        ax = ay = 0;
        for (const F *f : act)
            ax += fromStart ? f->x - f->gx : f->dx, ay += fromStart ? f->y - f->gy : f->dy;
        ax /= n, ay /= n;
    };

    switch (m_mode) {
    case Mode::Idle:
    case Mode::Done:
        break;
    case Mode::Pointer: {
        double ex = 0, ey = 0;
        F &f = *act[0];
        if (deadZone(f, ex, ey))
            move(ex, ey, std::hypot(f.dx, f.dy), t);
        else
            m_lastSpeed = 0;
        break;
    }
    case Mode::Wait2: {   // два пальца: прокрутка, щипок или один из них просто лежит
        F &a = *act[0], &b = *act[1];
        const double ax = a.x - a.gx, ay = a.y - a.gy, bx = b.x - b.gx, by = b.y - b.gy;
        const double la = std::hypot(ax, ay), lb = std::hypot(bx, by);
        Mode next = Mode::Wait2;
        if (std::min(la, lb) >= 1.0) {
            const double dot = (ax * bx + ay * by) / (la * lb);
            const double d0 = std::hypot(a.gx - b.gx, a.gy - b.gy), d = std::hypot(a.x - b.x, a.y - b.y);
            if (dot < -0.2 && std::fabs(d - d0) >= 2.0) {
                next = cfg.pinch ? Mode::Pinch : Mode::Done;
                m_pinchRef = d0;
            } else if (dot > 0.3) {
                next = Mode::Scroll;
            }
        } else if (std::max(la, lb) >= 1.5) {   // движется один
            F &still = la < lb ? a : b, &mover = la < lb ? b : a;
            if (cfg.palm && still.y > mover.y && (still.y > kRestZone * height || still.y - mover.y > 20)) {
                still.rest = true, still.rx = still.x, still.ry = still.y;   // нижний лежит — курсор ведёт верхний
                break;
            }
            if (std::max(la, lb) >= 4 || t - m_modeAt > 150 * kMs)
                next = Mode::Scroll;
        }
        if (next != Mode::Wait2) {
            m_mode = next;
            m_tapAlive = false;
            m_rate.clear();
        }
        break;
    }
    case Mode::Scroll: {
        double ax, ay;
        average(false, ax, ay);
        m_accX += ax, m_accY += ay;
        // ось: первые 2,5 мм решают — круче 60° — по вертикали, положе 30° — по горизонтали, иначе свободно; ведут
        // вбок упорно (5 мм) — ось отпускается
        if (m_axis == 0 && std::hypot(m_accX, m_accY) >= 2.5) {
            const double s = std::fabs(m_accY) / std::max(std::fabs(m_accX), 1e-6);
            m_axis = s >= 1.73 ? 1 : s <= 0.57 ? 2 : 3;
        }
        double ox = ax, oy = ay;
        if (m_axis == 0) {
            (std::fabs(m_accY) >= std::fabs(m_accX) ? ox : oy) = 0;
        } else if (m_axis != 3) {
            m_cross = m_cross * 0.92 + (m_axis == 1 ? ax : ay);
            (m_axis == 1 ? ox : oy) = 0;
            if (std::fabs(m_cross) > 5)
                m_axis = 3;
        }
        const double g = scrollGain(m_rate.add(t, std::hypot(ax, ay)), cfg.scrollSpeed) * (cfg.natural ? 1 : -1);
        if (onScroll && (ox != 0 || oy != 0))
            onScroll(ox * g, oy * g, t, true);
        break;
    }
    case Mode::Pinch: {
        const double d = std::hypot(act[0]->x - act[1]->x, act[0]->y - act[1]->y);
        while (d > m_pinchRef * kPinchStep) {
            m_pinchRef *= kPinchStep;
            if (onZoom)
                onZoom(1);
        }
        while (d < m_pinchRef / kPinchStep) {
            m_pinchRef /= kPinchStep;
            if (onZoom)
                onZoom(-1);
        }
        break;
    }
    case Mode::Wait3: {   // три пальца повели — левая кнопка нажата, тащим
        // двое ведут, а нижний стоит внизу или далеко под ними — он лежит (большой палец): это прокрутка двумя
        if (cfg.palm) {
            F *low = act[0];
            for (F *f : act)
                if (f->y > low->y)
                    low = f;
            double others = 1e9, top = 0;
            for (const F *f : act)
                if (f != low)
                    others = std::min(others, std::hypot(f->x - f->gx, f->y - f->gy)), top = std::max(top, f->y);
            if (std::hypot(low->x - low->gx, low->y - low->gy) < 1.0 && others >= 3
                && (low->y > kRestZone * height || low->y - top > 20)) {
                low->rest = true, low->rx = low->x, low->ry = low->y;
                break;
            }
        }
        double ax, ay;
        average(true, ax, ay);
        if (std::hypot(ax, ay) >= kDragStartMm && t - m_modeAt >= kDragStartUs) {
            m_tapAlive = false;
            m_drag = true;
            m_mode = Mode::Drag;
            buttons();
        }
        break;
    }
    case Mode::Drag: {
        if (n == 1 && m_dragOneAt && t - m_dragOneAt > kDragOneUs) {   // остался один палец — отпустить, дальше курсор
            releaseDrag();
            regroup(t, act, Mode::Drag);
            break;
        }
        double ax, ay;
        average(false, ax, ay);
        if (ax != 0 || ay != 0)
            move(ax, ay, std::hypot(ax, ay), t);
        break;
    }
    case Mode::Hold: {    // кнопка держится, а пальцев легло меньше трёх: повели или держат — это уже не перетаскивание
        bool moved = false;
        for (const F *f : act)
            moved = moved || std::hypot(f->x - f->sx, f->y - f->sy) >= kHoldProbeMm;
        if (moved || t - m_holdTouchAt > kHoldProbeUs) {
            releaseDrag();
            regroup(t, act, Mode::Idle);
        }
        break;
    }
    case Mode::WaitSwipe: {
        double ax, ay;
        average(true, ax, ay);
        if (std::hypot(ax, ay) >= kSwipeMm) {
            m_tapAlive = false;
            m_mode = Mode::Done;
            if (onSwipe)
                onSwipe(std::fabs(ax) > std::fabs(ay) ? (ax > 0 ? Right : Left) : (ay > 0 ? Down : Up), n);
        }
        break;
    }
    }
}
