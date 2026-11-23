#include "decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

#include "link.h"   // monoUs

#include <cstdio>
#include <cstring>
#include <utility>
#include <sys/eventfd.h>
#include <unistd.h>

namespace {

// только кадры в видеопамяти: без них дисплей кадр не возьмёт (обычная память — копия, медленно)
AVPixelFormat pickFormat(AVCodecContext *, const AVPixelFormat *fmts)
{
    for (const AVPixelFormat *f = fmts; *f != AV_PIX_FMT_NONE; ++f)
        if (*f == AV_PIX_FMT_DRM_PRIME)
            return *f;
    fprintf(stderr, "декодер: DRM PRIME не предложен — аппаратной распаковки нет\n");
    return AV_PIX_FMT_NONE;
}

} // namespace

bool Decoder::open()
{
    close();
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (!codec)
        return false;
    if (av_hwdevice_ctx_create(&m_hw, AV_HWDEVICE_TYPE_DRM, nullptr, nullptr, 0) < 0) {
        fprintf(stderr, "декодер: устройство DRM не открылось\n");
        return false;
    }
    m_ctx = avcodec_alloc_context3(codec);
    m_ctx->hw_device_ctx = av_buffer_ref(m_hw);
    m_ctx->get_format = pickFormat;
    m_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;   // B-кадров у телефона нет: кадр — сразу, без очереди перестановки
    m_ctx->thread_count = 1;                   // потоки кадров — это задержка на кадр каждый
    // Кадры сверх нужных декодеру: дисплей держит до 9 — на экране, во взлёте и до kTimed (7) в очереди к своим
    // обновлениям (опережение до ~60 мс, display.cpp); ещё 3 — распакованные, но не взятые главным циклом (пачка после
    // провала связи). Буферы кончились — декодер ждёт свободный и картинка стоит, пока кадр не отпустят: при 9 запас
    // был один буфер (проба 20 ноября 2026: удержали лишние — встал, отпустили — пошёл сам, docs/PI.md)
    m_ctx->extra_hw_frames = 12;
    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        fprintf(stderr, "декодер: HEVC не открылся\n");
        close();
        return false;
    }
    m_pkt = av_packet_alloc();
    return true;
}

void Decoder::close()
{
    av_packet_free(&m_pkt);
    avcodec_free_context(&m_ctx);
    av_buffer_unref(&m_hw);
}

bool Decoder::decode(const uint8_t *data, size_t n, int64_t pts, std::vector<AVFrame *> &out)
{
    if (av_new_packet(m_pkt, int(n)) < 0)
        return false;
    memcpy(m_pkt->data, data, n);   // свой буфер: с обязательным хвостом нулей для разбора
    m_pkt->pts = pts;
    int r = avcodec_send_packet(m_ctx, m_pkt);
    av_packet_unref(m_pkt);
    if (r < 0 && r != AVERROR(EAGAIN))
        return false;
    for (;;) {
        AVFrame *f = av_frame_alloc();
        r = avcodec_receive_frame(m_ctx, f);
        if (r < 0) {
            av_frame_free(&f);
            return r == AVERROR(EAGAIN) || r == AVERROR_EOF;
        }
        if (f->format != AV_PIX_FMT_DRM_PRIME) {
            av_frame_free(&f);
            return false;
        }
        out.push_back(f);
    }
}

DecodeThread::DecodeThread()
    : m_event(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
{
    m_thread = std::thread([this] { run(); });
}

DecodeThread::~DecodeThread()
{
    {
        std::lock_guard lock(m_m);
        m_quit = true;
    }
    m_cv.notify_one();
    m_thread.join();
    for (Out &o : m_out)
        av_frame_free(&o.frame);
    ::close(m_event);
}

void DecodeThread::reset()
{
    std::lock_guard lock(m_m);
    m_in.clear();
    m_reset = true;
    m_cv.notify_one();
}

void DecodeThread::push(const uint8_t *data, size_t n, bool key, const FrameInfo &info)
{
    std::lock_guard lock(m_m);
    m_in.push_back({std::vector<uint8_t>(data, data + n), key, info});
    m_cv.notify_one();
}

std::vector<DecodeThread::Out> DecodeThread::take()
{
    uint64_t n;
    (void)!::read(m_event, &n, sizeof n);
    std::lock_guard lock(m_m);
    return std::exchange(m_out, {});
}

void DecodeThread::run()
{
    for (;;) {
        In job;
        {
            std::unique_lock lock(m_m);
            m_cv.wait(lock, [this] { return m_quit || m_reset || !m_in.empty(); });
            if (m_quit)
                return;
            if (m_reset) {
                m_reset = false;
                m_needKey = true;
                lock.unlock();
                m_dec.open();
                continue;
            }
            job = std::move(m_in.front());
            m_in.pop_front();
        }
        if (m_needKey && !job.key)
            continue;   // после сбоя или нового потока — с опорного
        m_needKey = false;
        std::vector<AVFrame *> frames;
        const bool ok = m_dec.isOpen() && m_dec.decode(job.data.data(), job.data.size(), job.info.capture, frames);
        job.info.decoded = monoUs();
        std::lock_guard lock(m_m);
        if (!ok) {
            for (AVFrame *f : frames)
                av_frame_free(&f);
            m_out.push_back({nullptr, job.info, true});
            m_needKey = true;
            m_reset = true;   // декодер — заново
        }
        for (AVFrame *f : frames)
            m_out.push_back({f, job.info, false});
        const uint64_t one = 1;
        (void)!::write(m_event, &one, sizeof one);
    }
}
