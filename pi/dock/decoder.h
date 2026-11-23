// НаомиФон, приставка на Pi — распаковка HEVC аппаратным декодером Pi 5 (rpi-hevc-dec, /dev/video19) через ffmpeg
// Raspberry Pi OS (v4l2-request): кадр остаётся в видеопамяти (DRM PRIME) — его берёт слой дисплея без копий.
#pragma once

#include "display.h"   // FrameInfo

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

struct AVBufferRef;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

class Decoder
{
public:
    ~Decoder() { close(); }
    bool open();
    void close();
    bool isOpen() const { return m_ctx != nullptr; }
    // кадр Annex B (с параметрами перед опорным) → распакованные (обычно один); pts — метка захвата.
    // Кадры (AV_PIX_FMT_DRM_PRIME) — вызывающему: av_frame_free. false — ошибка (нужен опорный кадр).
    bool decode(const uint8_t *data, size_t n, int64_t pts, std::vector<AVFrame *> &out);

private:
    AVBufferRef *m_hw = nullptr;
    AVCodecContext *m_ctx = nullptr;
    AVPacket *m_pkt = nullptr;
};

// Распаковка — своим потоком: главный цикл (такты, отдача экрану за 4 мс до обновления, мышь) не ждёт декодер
// (~3,7 мс на кадр 2400 × 1350). Было в главном цикле: отдача экрану, совпавшая с распаковкой, промахивалась мимо
// обновления — рывок «встал позже + следующий выброшен» (замер 15 ноября 2026, docs/PI.md, «Плавность до конца»).
class DecodeThread
{
public:
    struct Out { AVFrame *frame; FrameInfo info; bool failed; };   // failed — декодер сбился: нужен опорный кадр
    DecodeThread();
    ~DecodeThread();
    int fd() const { return m_event; }   // готово (читать take)
    void reset();                        // новый поток (Hello): декодер заново, очередь — прочь
    void push(const uint8_t *data, size_t n, bool key, const FrameInfo &info);
    std::vector<Out> take();

private:
    struct In { std::vector<uint8_t> data; bool key; FrameInfo info; };
    void run();
    Decoder m_dec;
    std::thread m_thread;
    std::mutex m_m;
    std::condition_variable m_cv;
    std::deque<In> m_in;
    std::vector<Out> m_out;
    bool m_reset = false, m_quit = false, m_needKey = true;
    int m_event = -1;
};
