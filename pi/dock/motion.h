// НаомиФон, приставка на Pi — движение в такт монитору (docs/PI.md, «Плавность», «Мышь»).
//  • Pointer: мышь шлёт сдвиги своей частотой (у мыши пользователя — ровно 62,5 в секунду), монитор обновляется 75
//    раз — «последний сдвиг» в каждое обновление идёт рывками (каждое пятое обновление — без сдвига). Положение
//    к нужному моменту — между двумя уже пришедшими отчётами мыши, на 1,25 её шага назад (у 62,5 Гц — 20 мс), без
//    досчёта вперёд. Досчёт вперёд по скорости (было до 15 ноября) на живой мыши шумел — отсчёты в отчётах неровные
//    (7, 11, 8, 12…), — и проскакивал на остановках (в среднем 32 пикселя, до 134): курсор и перетаскиваемое окно
//    улетали и возвращались. Проигрывание записи мыши пользователя: неровность хода курсора 18 % → 8 %, рывков
//    16 % → 4 % обновлений, проскока нет; цена — курсор на ~11 мс позже (меньше одного обновления монитора).
//  • Wheel: щелчок колеса — не прыжок, а плавная прокрутка: остаток тает за такт на долю k — к «мягкости» из настроек
//    стола страница доходит на 95 % (130 мс по умолчанию: 32 % за такт 60 Гц, как было до настроек; без плавности —
//    k = 1, весь щелчок за такт), шаги — целые точки стола (телефон округляет до точки — дробное пропало бы).
//  • WheelRate: разгон колеса (настройка стола) — крутят быстро, щелчок проходит больше, до 3 раз.
//  • Прокрутка трекпадом идёт тем же Wheel: пока пальцы на трекпаде — весь ход за такт (k = 1), сняли — инерция:
//    остаток = скорость пальцев × 0,33 с, тает за такт на долю с той же постоянной (начальная скорость — как у пальцев).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

class Pointer
{
public:
    void add(int64_t t, double x, double y)   // новое положение от мыши (время — метка события ядра)
    {
        if (!m_hist.empty() && t <= m_hist.back().t)
            t = m_hist.back().t + 1;
        m_hist.push_back({t, x, y});
        while (m_hist.size() > 12)
            m_hist.pop_front();
    }
    void jump(int64_t t, double x, double y)   // поставили в точку (абсолютное устройство) — сразу там
    {
        m_hist.clear();
        m_hist.push_back({t - 1000000, x, y});
        m_hist.push_back({t, x, y});
    }
    bool empty() const { return m_hist.empty(); }
    // шаг мыши — медиана промежутков между её отчётами в движении (16 мс у мыши 62,5 Гц, 8 — у 125 Гц)
    int64_t step() const
    {
        std::vector<int64_t> d;
        for (size_t i = 1; i < m_hist.size(); ++i)
            if (m_hist[i].t - m_hist[i - 1].t < 50000)   // паузы — не шаг
                d.push_back(m_hist[i].t - m_hist[i - 1].t);
        if (d.empty())
            return 16000;
        std::nth_element(d.begin(), d.begin() + long(d.size() / 2), d.end());
        return std::clamp<int64_t>(d[d.size() / 2], 1000, 33000);
    }
    int64_t delay() const { return std::clamp<int64_t>(step() * 5 / 4, 4000, 25000); }
    bool moving(int64_t now) const   // положение ещё догоняет последний отчёт
    {
        return !m_hist.empty() && now <= m_hist.back().t + delay() + 2000;
    }
    void at(int64_t t, double &x, double &y) const
    {
        const int64_t st = step(), T = t - delay();
        const Sample &last = m_hist.back();
        x = last.x;
        y = last.y;
        if (T >= last.t)
            return;
        for (size_t i = m_hist.size() - 1; i > 0; --i) {
            const Sample &a = m_hist[i - 1], &b = m_hist[i];
            if (a.t > T)
                continue;
            // после паузы мышь «трогается» за один шаг до первого отчёта, а не тянется через всю паузу
            const int64_t from = std::max(a.t, b.t - st);
            const double k = T <= from ? 0.0 : double(T - from) / double(b.t - from);
            x = a.x + (b.x - a.x) * k;
            y = a.y + (b.y - a.y) * k;
            return;
        }
        x = m_hist.front().x;
        y = m_hist.front().y;
    }

private:
    struct Sample { int64_t t; double x, y; };
    std::deque<Sample> m_hist;
};

class Wheel
{
public:
    void add(double dx, double dy) { m_tx += dx, m_ty += dy; }   // точек стола
    void stop() { m_tx = m_sx, m_ty = m_sy; }                      // остаток — прочь (пальцы легли на трекпад)
    bool busy() const { return std::fabs(m_tx - m_sx) >= 0.5 || std::fabs(m_ty - m_sy) >= 0.5; }
    // шаг этого такта — целые точки; k — доля остатка за такт (1 — весь сразу); false — шагать нечего
    bool step(int &dx, int &dy, double k)
    {
        const double wantX = m_sx + (m_tx - m_sx) * k, wantY = m_sy + (m_ty - m_sy) * k;
        // хвост — не тоньше точки за такт, чтобы не тянулся
        auto toward = [](double want, double sent, double target) {
            double d = std::round(want) - sent;
            if (d == 0 && std::fabs(target - sent) >= 0.5)
                d = target > sent ? 1 : -1;
            return d;
        };
        dx = int(toward(wantX, m_sx, m_tx));
        dy = int(toward(wantY, m_sy, m_ty));
        m_sx += dx;
        m_sy += dy;
        return dx || dy;
    }

private:
    double m_tx = 0, m_ty = 0;   // куда надо (накоплено от щелчков)
    double m_sx = 0, m_sy = 0;   // сколько уже послано
};

// Разгон колеса: щелчков в секунду за последние 0,2 с → множитель щелчка. Спокойно (до 10 в секунду — одиночные и
// обычная прокрутка) — 1, быстро (30 и больше — колесо раскрутили) — 3, между ними — плавно.
class WheelRate
{
public:
    double add(int64_t t, double notches)
    {
        m_recent.push_back({t, std::fabs(notches)});
        while (t - m_recent.front().first > 200000)
            m_recent.pop_front();
        double sum = 0;
        for (const auto &r : m_recent)
            sum += r.second;
        const double s = std::clamp((sum / 0.2 - 10) / 20, 0.0, 1.0);
        return 1 + 2 * s * s * (3 - 2 * s);
    }

private:
    std::deque<std::pair<int64_t, double>> m_recent;
};
