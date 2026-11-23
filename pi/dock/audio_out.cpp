#include "audio_out.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>

// alsa-lib 1.2 (Debian 13): объявления по alsa/pcm.h, alsa/error.h
extern "C" {
struct _snd_pcm;
typedef struct _snd_pcm snd_pcm_t;
typedef long snd_pcm_sframes_t;
typedef unsigned long snd_pcm_uframes_t;
int snd_pcm_open(snd_pcm_t **pcm, const char *name, int stream, int mode);
int snd_pcm_set_params(snd_pcm_t *pcm, int format, int access, unsigned int channels, unsigned int rate,
                       int soft_resample, unsigned int latency);
snd_pcm_sframes_t snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
int snd_pcm_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
int snd_pcm_recover(snd_pcm_t *pcm, int err, int silent);
int snd_pcm_close(snd_pcm_t *pcm);
const char *snd_strerror(int errnum);
}

namespace {

constexpr int kPlayback = 0;          // SND_PCM_STREAM_PLAYBACK
constexpr int kS16le = 2;             // SND_PCM_FORMAT_S16_LE
constexpr int kS32le = 10;            // SND_PCM_FORMAT_S32_LE
constexpr int kFloatLe = 14;          // SND_PCM_FORMAT_FLOAT_LE
constexpr int kInterleaved = 3;       // SND_PCM_ACCESS_RW_INTERLEAVED
constexpr unsigned kRate = 48000;
constexpr size_t kPeriod = 240;       // 5 мс: столько пишется за раз
constexpr unsigned kLatencyUs = 20000;   // буфер устройства: столько звука впереди
constexpr double kStep = 1e6 / kRate; // мкс на отсчёт

int64_t monoUs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return int64_t(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

} // namespace

AudioOut::AudioOut(std::string device)
    : m_device(std::move(device))
{
    if (const char *f = getenv("NAOMI_DOCK_AUDIODUMP"))   // что уходит в устройство: float стерео 48 кГц, как есть
        m_dump = fopen(f, "wb");
    m_thread = std::thread([this] { run(); });
}

AudioOut::~AudioOut()
{
    m_quit = true;
    m_cv.notify_all();
    m_thread.join();
}

void AudioOut::push(int64_t first, int64_t playAt, const void *pcm, size_t frames, bool f32)
{
    const int64_t now = monoUs();
    std::lock_guard lock(m_m);
    ++m_stats.chunks;
    // встык к прошлому, если по меткам телефона он — следующий (±2 мс: метки уточняются при запуске захвата), а в
    // часах Pi расходится меньше 50 мс (больше — сверка часов сменилась сильно: заново по метке); пропущенный кусок
    // тишины — 10 мс по меткам телефона: там пауза и должна быть
    double at = double(playAt);
    if (m_haveLast && std::abs(first - m_lastFirstEnd) < 2000 && std::abs(at - m_lastEnd) < 50000) {
        at = m_lastEnd;
        ++m_stats.joined;
    }
    m_lastEnd = at + double(frames) * kStep;
    m_lastFirstEnd = first + int64_t(double(frames) * kStep);
    m_haveLast = true;
    const double lead = (at - double(now)) / 1000.0;
    if (lead < m_stats.minLeadMs) {
        m_stats.secondLeadMs = m_stats.minLeadMs;
        m_stats.minLeadMs = lead;
    } else if (lead < m_stats.secondLeadMs) {
        m_stats.secondLeadMs = lead;
    }
    if (m_synced && m_lastEnd < m_cursor) {   // его время уже прошло
        ++m_stats.late;
        return;
    }
    Chunk c{at, std::vector<float>(frames * 2)};
    if (f32) {
        memcpy(c.pcm.data(), pcm, frames * 8);
    } else {
        const auto *x = static_cast<const uint8_t *>(pcm);
        for (size_t i = 0; i < frames * 2; ++i)
            c.pcm[i] = float(int16_t(x[2 * i] | x[2 * i + 1] << 8)) / 32768.f;
    }
    for (float x : c.pcm)
        m_stats.peak = std::max(m_stats.peak, std::abs(x));
    auto pos = std::upper_bound(m_queue.begin(), m_queue.end(), at, [](double t, const Chunk &x) { return t < x.at; });
    m_queue.insert(pos, std::move(c));
    while (m_queue.size() > 100)   // секунда — больше быть не может (стояли бы)
        m_queue.pop_front();
    m_cv.notify_all();
}

AudioOut::Stats AudioOut::takeStats()
{
    std::lock_guard lock(m_m);
    Stats s = m_stats;
    const bool open = m_stats.open;
    m_stats = Stats{};
    m_stats.open = open;
    return s;
}

// Отсчёты к моменту hearAt (когда прозвучит первый из них): курсор — момент следующего отсчёта по меткам кусков.
// Курсор впереди больше 1 мс — один отсчёт повторить (звук сдвинется позже), позади — один пропустить
void AudioOut::fill(float *out, size_t frames, int64_t hearAt)
{
    if (!m_synced) {
        m_cursor = double(hearAt);
        m_synced = true;
    }
    double err = m_cursor - double(hearAt);
    if (std::abs(err) > 20000) {
        m_cursor = double(hearAt);
        ++m_stats.jumps;
        err = 0;
    }
    const int nudge = err > 1000 ? 1 : err < -1000 ? -1 : 0;
    m_stats.nudges += nudge != 0;
    for (size_t i = 0; i < frames; ++i) {
        if (i == frames / 2 && nudge > 0 && i > 0) {   // повторить прошлый — курсор стоит
            out[2 * i] = out[2 * i - 2];
            out[2 * i + 1] = out[2 * i - 1];
            continue;
        }
        if (i == frames / 2 && nudge < 0)
            m_cursor += kStep;   // пропустить один
        while (!m_queue.empty() && m_queue.front().at + double(m_queue.front().pcm.size() / 2) * kStep <= m_cursor + kStep / 2)
            m_queue.pop_front();
        float l = 0, r = 0;
        if (!m_queue.empty() && m_queue.front().at <= m_cursor + kStep / 2) {
            const Chunk &c = m_queue.front();
            const size_t k = size_t(std::max(0.0, std::round((m_cursor - c.at) / kStep)));
            if (k < c.pcm.size() / 2) {
                l = c.pcm[2 * k];
                r = c.pcm[2 * k + 1];
            }
        }
        out[2 * i] = l;
        out[2 * i + 1] = r;
        m_cursor += kStep;
    }
}

void AudioOut::run()
{
    pthread_setname_np(pthread_self(), "naomi-audio");
    snd_pcm_t *pcm = nullptr;
    std::vector<float> buf(kPeriod * 2);
    std::vector<int32_t> out32(kPeriod * 2);
    std::vector<int16_t> out16(kPeriod * 2);
    int format = kFloatLe;
    int64_t lastSound = 0, retryAt = 0;
    bool warned = false;
    auto closePcm = [&] {
        snd_pcm_close(pcm);
        pcm = nullptr;
        std::lock_guard lock(m_m);
        m_synced = false;
        m_stats.open = false;
    };
    while (!m_quit) {
        if (!pcm) {
            std::unique_lock lock(m_m);
            m_cv.wait_for(lock, std::chrono::milliseconds(200), [&] { return m_quit || !m_queue.empty(); });
            if (m_quit || m_queue.empty() || monoUs() < retryAt)
                continue;
            lock.unlock();
            // формат устройству: float (без потерь), иначе 32 бита, иначе 16 — что примет (HDMI монитора — до 24 бит)
            int err = snd_pcm_open(&pcm, m_device.c_str(), kPlayback, 0);
            if (err >= 0) {
                for (int f : {kFloatLe, kS32le, kS16le})
                    if ((err = snd_pcm_set_params(pcm, f, kInterleaved, 2, kRate, 1, kLatencyUs)) >= 0) {
                        format = f;
                        break;
                    }
                if (err < 0) {
                    snd_pcm_close(pcm);
                    pcm = nullptr;
                }
            }
            if (!pcm) {
                if (!warned)
                    fprintf(stderr, "звук: %s не открылся (%s) — пробую раз в 2 с\n", m_device.c_str(), snd_strerror(err));
                warned = true;
                retryAt = monoUs() + 2000000;
                continue;
            }
            warned = false;
            fprintf(stderr, "звук: играю в %s (48 кГц стерео, %s, буфер %u мс)\n", m_device.c_str(),
                    format == kFloatLe ? "float" : format == kS32le ? "32 бита" : "16 бит", kLatencyUs / 1000);
            lastSound = monoUs();
            std::lock_guard l2(m_m);
            m_synced = false;
            m_stats.open = true;
        }
        snd_pcm_sframes_t delay = 0;
        if (snd_pcm_delay(pcm, &delay) < 0 || delay < 0)
            delay = 0;
        const int64_t now = monoUs();
        {
            std::lock_guard lock(m_m);
            fill(buf.data(), kPeriod, now + int64_t(delay) * 1000000 / kRate);
            if (!m_queue.empty())
                lastSound = now;
        }
        if (now - lastSound > 5000000) {   // 5 с тишины — устройство отпустить
            fprintf(stderr, "звук: тишина 5 с — %s закрыт\n", m_device.c_str());
            closePcm();
            continue;
        }
        if (m_dump)
            fwrite(buf.data(), 8, kPeriod, m_dump);
        const void *data = buf.data();
        if (format == kS32le) {
            for (size_t i = 0; i < buf.size(); ++i)
                out32[i] = int32_t(std::lround(std::clamp(buf[i], -1.f, 1.f) * 2147483520.f));
            data = out32.data();
        } else if (format == kS16le) {
            for (size_t i = 0; i < buf.size(); ++i)
                out16[i] = int16_t(std::lround(std::clamp(buf[i], -1.f, 1.f) * 32767.f));
            data = out16.data();
        }
        snd_pcm_sframes_t n = snd_pcm_writei(pcm, data, kPeriod);
        if (n < 0) {
            {
                std::lock_guard lock(m_m);
                ++m_stats.underruns;
                m_synced = false;
            }
            if (snd_pcm_recover(pcm, int(n), 1) < 0) {
                fprintf(stderr, "звук: %s — сбой (%s), закрыт\n", m_device.c_str(), snd_strerror(int(n)));
                closePcm();
            }
        }
    }
    if (pcm)
        snd_pcm_close(pcm);
    if (m_dump)
        fclose(m_dump);
}
