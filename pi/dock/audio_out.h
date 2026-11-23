// НаомиФон, приставка — звук стола на мониторе (docs/PI.md, «Звук на мониторе»): HDMI монитора через ALSA, свой
// поток. Телефон шлёт куски по 10 мс с меткой первого отсчёта (его часы); приставка играет звук в свой момент —
// метка в часах Pi плюс задержка (delay): столько же, сколько идёт картинка, — звук вровень с ней. Куски приходят
// копиями (кабель и Wi-Fi) — берётся первый. Кусок, который по меткам телефона идёт следом за прошлым (через ровно
// столько, сколько тот звучал, ±2 мс), встаёт ровно встык, даже если сверка часов сдвинулась (по меткам в часах Pi на
// каждом стыке терялся или вставлялся кусочек звука — хрип); пропуск в метках (тишину телефон не шлёт) — пауза, новый
// отрезок. Расхождение «когда отсчёт прозвучит» и «когда должен» (часы
// телефона и монитора идут чуть по-разному) — по одному отсчёту на 5 мс: вставить или пропустить, не слышно; больше
// 20 мс (связь стояла, задержку сменили) — сразу на место. Внутри — float; устройству — float, иначе 32 или 16 бит.
// Звука нет 5 с — устройство закрыто.
// Заголовков ALSA на Pi нет (libasound2-dev не ставили) — восемь функций объявлены в audio_out.cpp по alsa-lib 1.2,
// библиотека — системная libasound.so.2.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class AudioOut
{
public:
    explicit AudioOut(std::string device);
    ~AudioOut();

    // кусок: когда звучать его первому отсчёту (мкс, CLOCK_MONOTONIC Pi), стерео 48 кГц, f32 или s16
    // first — метка телефона (по ней видно, что кусок — следующий за прошлым: ровно на 10 мс позже)
    void push(int64_t first, int64_t playAt, const void *pcm, size_t frames, bool f32);

    // за секунду (поток вывода считает, main печатает): опоздавших кусков, сбоев устройства, поправок отсчётами,
    // перескоков; насколько раньше своего момента приходили куски — наименьшее и второе (одиночный выброс — смена
    // выхода, перенос потока в PulseAudio — задержку не раздувает), мс
    struct Stats {
        int late = 0, underruns = 0, nudges = 0, jumps = 0, chunks = 0, joined = 0;   // joined — поставлено встык
        float peak = 0;   // громче всего, 1 — полная шкала
        double minLeadMs = 1e9, secondLeadMs = 1e9;
        bool open = false;
    };
    Stats takeStats();

private:
    struct Chunk { double at; std::vector<float> pcm; };
    void run();
    void fill(float *out, size_t frames, int64_t hearAt);

    std::string m_device;
    std::thread m_thread;
    std::atomic<bool> m_quit{false};
    std::mutex m_m;
    std::condition_variable m_cv;
    std::deque<Chunk> m_queue;
    Stats m_stats;
    FILE *m_dump = nullptr;   // NAOMI_DOCK_AUDIODUMP: всё, что ушло в устройство
    double m_cursor = 0;      // момент (мкс Pi), которому принадлежит следующий выводимый отсчёт
    bool m_synced = false;
    double m_lastEnd = 0;     // где кончился последний принятый кусок (мкс Pi) — следующий встык
    int64_t m_lastFirstEnd = 0;   // и его конец по меткам телефона
    bool m_haveLast = false;
};
