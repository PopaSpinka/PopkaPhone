// НаомиФон, приставка на Pi — яркость монитора по DDC/CI (docs/PI.md, «Приглушение»): без дока и без дела монитор не
// гаснет, а приглушается до доли яркости из настроек стола; клавиша, щелчок, стол вернулся — прежняя яркость. Монитору
// — команды VCP 0x10 (яркость) по шине DDC его разъёма HDMI (I2C, адрес 0x37; /dev/i2c-N — модуль i2c-dev).
// Переход плавный: запись раз в 20 мс по кривой «на глаз», приглушить — 0,7 с, вернуть — 0,4 с; посреди перехода
// просят обратное — разворот с того места. Шина медленная (40–50 мс на чтение) — всё своим потоком, главный цикл
// только просит. Прежняя яркость — и на диске (/var/lib/naomi-dock/brightness): служба упала приглушённой — вернёт
// при следующем запуске. Пока приглушено, яркость кнопками монитора сделали ярче прежней — она и остаётся.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class Brightness
{
public:
    ~Brightness() { close(); }
    void open(const std::string &connector);   // выход монитора (HDMI-A-1); шина — его ddc в /sys/class/drm
    void close();                              // приглушено — вернуть яркость (ждёт)
    void dim(int percent);                     // приглушить до percent % наибольшей яркости (ярче не делает)
    void restore();                            // вернуть прежнюю
    // клавиши яркости: на столько десятых наибольшей яркости ярче (+) или темнее (−), по сетке 0, 10 … 100 %;
    // приглушено — не трогает. Какая стала (%) — takeLevel: −1 — нового нет
    void step(int tenths);
    int takeLevel() { return m_level.exchange(-1); }
    int supported() const { return m_supported; }   // монитор отвечает: 1 — да, 0 — нет, -1 — ещё не знаем

private:
    void run();
    bool get(int &cur, int &max, int tries = 3);
    bool put(int value);   // записать яркость, без паузы (переход)
    bool set(int value);   // записать и выждать паузу спецификации
    bool command(const unsigned char *msg, int n);
    void doDim(int percent);
    void doRestore();
    void doStep(int tenths);
    int fade(int from, int to, int max, int ms, int want);
    bool settle(int target, int want);

    std::string m_dev;
    int m_fd = -1;
    std::thread m_thread;
    std::mutex m_m;
    std::condition_variable m_cv;
    int m_steps = 0;                // клавиши яркости: шагов ждёт
    std::atomic<int> m_level{-1};   // …и какая стала, % (главному циклу)
    int m_stepAt = -1;              // поток: что записали по клавише последним и когда (монитор после записи
    std::chrono::steady_clock::time_point m_stepTime;   // отвечает не сразу и не тем)
    int m_want = -1, m_have = -1;   // просят / сделано: -1 — обычная яркость, иначе — приглушить до стольких %
    bool m_quit = false;
    std::atomic<int> m_supported{-1};
    // поток: прежняя яркость; что записали сами последним (-1 — не приглушено); наибольшая у монитора
    int m_orig = -1, m_dimmed = -1, m_max = 0;
};
