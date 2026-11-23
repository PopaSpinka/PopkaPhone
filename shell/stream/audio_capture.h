// НаомиФон — захват звука PulseAudio своим потоком: звук стола зрителю (stream_service, Audio) и эквалайзер окна
// звука (SoundEqualizer). Источник — монитор выхода: что этот выход сейчас играет. Запись не переезжает на другой
// источник (PA_STREAM_DONT_MOVE): иначе пропавший монитор подменялся микрофоном — и драйвер звука вешался.
// Заголовков libpulse в среде сборки нет (пакет -dev не ставили) — нужные функции и структуры объявлены в
// audio_capture.cpp по документации PulseAudio 16 (ABI не менялся с 0.9), библиотека — та же libpulse.so.0.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class AudioCapture
{
public:
    // first — метка первого отсчёта куска, мкс CLOCK_MONOTONIC (когда он был сыгран выходом): по счётчику отсчётов от
    // начала потока — куски идут ровно через chunkMs (по времени чтения метки дрожали на 1–3 мс, и приставка на
    // каждом стыке теряла или вставляла кусочек звука — хрип); data — отсчёты вперемешку по каналам (s16 или f32),
    // frames — отсчётов на канал
    using Chunk = std::function<void(int64_t first, const void *data, size_t frames)>;
    // запись на паузе (true: на выход gateSink никто не играет) или снова идёт (false) — из потока захвата
    using Idle = std::function<void(bool idle)>;
    enum Format { S16 = 3, F32 = 5 };   // PA_SAMPLE_S16LE, PA_SAMPLE_FLOAT32LE

    AudioCapture() = default;
    ~AudioCapture() { stop(); }
    AudioCapture(const AudioCapture &) = delete;
    AudioCapture &operator=(const AudioCapture &) = delete;

    // device — источник PulseAudio («naomi.desk.monitor», «@DEFAULT_MONITOR@»); кусок — chunkMs. Не открылся или
    // оборвался — снова через 0,5 с, пока не stop(). gateSink — выход, монитор которого пишем: пока на него никто не
    // играет (потоков PulseAudio на нём нет), запись на паузе (cork) — кусков нет, выход PulseAudio усыпляет
    void start(const std::string &device, uint32_t rate, uint8_t channels, int chunkMs, Chunk onChunk,
               Format format = S16, const std::string &gateSink = {}, Idle onIdle = {});
    void stop();
    bool running() const { return m_state != nullptr; }

private:
    struct State {
        std::atomic<bool> quit{false};
        std::atomic<bool> done{false};
        std::mutex m;         // бережёт loop: stop() будит цикл PulseAudio, пока поток захвата его не разобрал
        void *loop = nullptr; // pa_threaded_mainloop потока захвата, пока он жив
    };
    std::shared_ptr<State> m_state;
    std::thread m_thread;
};
