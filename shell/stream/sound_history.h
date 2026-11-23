// НаомиФон — последние доли секунды звука, моно float: эквалайзеру окна звука (SoundEqualizer). Пишет поток захвата
// (AudioCapture), читает поток интерфейса, под замком. Звук стола приставке (StreamService) снимается всё время, пока
// она его играет, — он пишет сюда же, и эквалайзер берёт звук без своего захвата и без ожидания (docs/DESKTOP.md, «Звук»).
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

class SoundHistory
{
public:
    explicit SoundHistory(size_t capacity) : m_ring(capacity) {}

    // кусок вперемешку по каналам → моно (среднее каналов)
    void push(const float *data, size_t frames, int channels)
    {
        std::lock_guard lock(m_m);
        const size_t size = m_ring.size();
        const float k = 1.f / float(channels);
        for (size_t i = 0; i < frames; ++i) {
            float v = 0;
            for (int c = 0; c < channels; ++c)
                v += data[i * size_t(channels) + size_t(c)];
            m_ring[m_at] = v * k;
            if (++m_at == size)
                m_at = 0;
        }
        m_filled = std::min(m_filled + frames, size);
        m_lastUs = nowUs();
    }

    // последние отсчёты, не больше n (сколько есть), — в out по порядку; 0 — звука нет: кусков не было maxAgeUs
    size_t latest(float *out, size_t n, int64_t maxAgeUs) const
    {
        std::lock_guard lock(m_m);
        if (!m_filled || nowUs() - m_lastUs > maxAgeUs)
            return 0;
        const size_t size = m_ring.size();
        n = std::min(n, m_filled);
        const size_t from = (m_at + size - n) % size, first = std::min(n, size - from);   // до конца кольца и с начала
        std::copy_n(m_ring.begin() + long(from), first, out);
        std::copy_n(m_ring.begin(), n - first, out + first);
        return n;
    }

    void clear()
    {
        std::lock_guard lock(m_m);
        m_at = m_filled = 0;
    }

private:
    static int64_t nowUs()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    mutable std::mutex m_m;
    std::vector<float> m_ring;
    size_t m_at = 0;       // куда пишется следующий
    size_t m_filled = 0;
    int64_t m_lastUs = 0;  // когда пришёл последний кусок
};
