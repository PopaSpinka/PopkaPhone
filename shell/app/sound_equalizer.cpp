#include "sound_equalizer.h"

#include <QPainter>

#include "stream_service.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr double kEq = 0.5;            // VIZ_EQ: на сколько полосу тянуть к среднему всех
constexpr double kEqSec = 4.0;         // VIZ_EQ_SEC: «своё среднее» полосы — за столько секунд
constexpr double kSigma = 1.5;         // VIZ_SIGMA: сглаживание соседями
constexpr int kFrameMs = 33;           // VIZ_FRAME_MS: ~30 кадров в секунду (подъём 0,6 — на кадр)
constexpr int64_t kStaleUs = 300000;   // кусков звука нет 0,3 с — звука нет

} // namespace

SoundEqualizer::SoundEqualizer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    m_samples.resize(SoundSpectrum::kWin);
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(kFrameMs);
    connect(&m_timer, &QTimer::timeout, this, &SoundEqualizer::tick);
    connect(this, &QQuickItem::widthChanged, this, [this] {
        const int cols = std::clamp(int(width() / 3), 0, 256);
        if (cols != m_cols) {
            m_cols = cols;
            m_spectrum.setBands(cols);
            reset();
            showDots();
        }
    });
    connect(this, &QQuickItem::heightChanged, this, &SoundEqualizer::showDots);
    m_clock.start();
}

SoundEqualizer::~SoundEqualizer()
{
    m_capture.stop();
}

// Открыли — первый кадр сразу (звук трансляции уже есть — столбики в первом же кадре окна), дальше — по таймеру
void SoundEqualizer::setRunning(bool on)
{
    if (on == m_running)
        return;
    m_running = on;
    if (on) {
        reset();
        updateSource(true);
        m_lastMs = m_clock.elapsed();
        tick();
        m_timer.start();
    } else {   // столбики — как были: окно тает с ними (заново — при следующем открытии)
        m_timer.stop();
        m_capture.stop();
    }
    emit runningChanged();
}

void SoundEqualizer::setOutput(const QString &output)
{
    if (output == m_output)
        return;
    m_output = output;
    updateSource(true);
    emit outputChanged();
}

QObject *SoundEqualizer::stream() const
{
    return m_stream;
}

void SoundEqualizer::setStream(QObject *stream)
{
    auto *s = qobject_cast<StreamService *>(stream);
    if (s == m_stream)
        return;
    if (m_stream)
        disconnect(m_stream, nullptr, this, nullptr);
    m_stream = s;
    if (s)   // зритель начал или перестал играть звук стола
        connect(s, &StreamService::changed, this, [this] { updateSource(false); });
    updateSource(false);
    emit streamChanged();
}

void SoundEqualizer::setColor(const QColor &c)
{
    if (c == m_color)
        return;
    m_color = c;
    update();
    emit colorChanged();
}

void SoundEqualizer::setGainDb(double db)
{
    if (db == m_gainDb)
        return;
    m_gainDb = db;
    emit gainDbChanged();
}

// Звук на «Динамиках», и трансляция его снимает — берём у неё, свой захват не нужен. Иначе — свой: монитор выхода по
// умолчанию, 48 кГц моно (без пересчёта частоты у PulseAudio); «@DEFAULT_MONITOR@» PulseAudio называет при открытии —
// сменился выход, открыть заново
void SoundEqualizer::updateSource(bool reopen)
{
    const bool desk = m_output == QLatin1String("desk") && m_stream && m_stream->deskSoundLive();
    m_source = desk ? m_stream->deskSound() : std::shared_ptr<const SoundHistory>(m_own);
    if (!m_running || desk) {
        m_capture.stop();
        return;
    }
    if (!reopen && m_capture.running())
        return;
    m_own->clear();
    m_capture.start("@DEFAULT_MONITOR@", SoundSpectrum::kRate, 1, 20, [h = m_own](int64_t, const void *data, size_t n) {
        h->push(static_cast<const float *>(data), n, 1);
    }, AudioCapture::F32);
}

void SoundEqualizer::reset()
{
    const size_t n = size_t(m_cols);
    m_fresh = m_settle = true;
    m_top = m_floor = 0;
    m_avg.assign(n, 0);
    m_target.assign(n, 0);
    m_level.assign(n, 0);
    m_peak.assign(n, 0);
    m_peakUntil.assign(n, 0);
}

// viz_targets: полоса — к среднему всех на половину отличия своего долгого среднего; сглаживание соседями (Гаусс);
// пол — 15 % полос, потолок — 90 % (вверх сразу, вниз 6 дБ/с), размах не меньше 16 дБ
void SoundEqualizer::targets(const std::vector<double> &db, double dt)
{
    static const auto gauss = [] {
        std::array<double, 9> g{};
        for (int k = -4; k <= 4; ++k)
            g[size_t(k + 4)] = std::exp(-double(k * k) / (2 * kSigma * kSigma));
        return g;
    }();
    const size_t n = db.size();
    double mean = 0;
    for (size_t i = 0; i < n; ++i) {
        m_avg[i] = m_fresh ? db[i] : m_avg[i] + (db[i] - m_avg[i]) * (1 - std::exp(-dt / kEqSec));
        mean += m_avg[i];
    }
    mean /= double(n);
    m_flat.resize(n);
    m_smooth.resize(n);
    for (size_t i = 0; i < n; ++i)
        m_flat[i] = db[i] - kEq * (m_avg[i] - mean);
    for (size_t i = 0; i < n; ++i) {
        double sum = 0, weight = 0;
        for (int k = -4; k <= 4; ++k) {
            const long j = long(i) + k;
            if (j < 0 || j >= long(n))
                continue;
            sum += gauss[size_t(k + 4)] * m_flat[size_t(j)];
            weight += gauss[size_t(k + 4)];
        }
        m_smooth[i] = sum / weight;
    }
    m_sorted = m_smooth;
    std::sort(m_sorted.begin(), m_sorted.end());
    const double floorDb = m_sorted[size_t(double(n) * 0.15)], topDb = m_sorted[size_t(double(n) * 0.90)];
    if (topDb < -70) {   // тишина
        std::fill(m_target.begin(), m_target.end(), 0);
        m_fresh = true;
        return;
    }
    m_top = (m_fresh || topDb > m_top) ? topDb : m_top - 6.0 * dt;
    m_floor = m_fresh ? floorDb : m_floor + (floorDb - m_floor) * (1 - std::exp(-dt / 0.6));
    m_fresh = false;
    const double lo = m_floor, hi = std::max(m_top, lo + 16);
    for (size_t i = 0; i < n; ++i)
        m_target[i] = std::pow(std::clamp((m_smooth[i] - lo) / (hi - lo), 0.0, 1.0), 1.2);
}

void SoundEqualizer::tick()
{
    if (m_cols <= 0)
        return;
    const qint64 now = m_clock.elapsed();
    const double dt = std::min((now - m_lastMs) / 1000.0, 0.1);
    m_lastMs = now;
    const size_t got = m_source->latest(m_samples.data(), m_samples.size(), kStaleUs);
    const bool heard = m_spectrum.compute(m_samples.data(), got, m_gainDb, m_db);
    if (heard) {
        targets(m_db, dt);
    } else {
        std::fill(m_target.begin(), m_target.end(), 0);
        m_fresh = true;
    }
    // первый звук после открытия (у трансляции — уже в первом кадре, свой захват — через ~30 мс) — столбики сразу на
    // месте, пики — на них; дальше — как обычно
    if (m_settle && heard) {
        m_settle = false;
        m_level = m_target;
        m_peak = m_target;
        std::fill(m_peakUntil.begin(), m_peakUntil.end(), now + 350);
    }
    // viz_advance: вверх — 0,6 пути за кадр, вниз — 1,6 в секунду; пик держится 0,35 с, потом 0,7 в секунду
    for (size_t i = 0; i < m_level.size(); ++i) {
        if (m_target[i] > m_level[i])
            m_level[i] += (m_target[i] - m_level[i]) * 0.6;
        else
            m_level[i] = std::max(m_target[i], m_level[i] - 1.6 * dt);
        if (m_level[i] >= m_peak[i]) {
            m_peak[i] = m_level[i];
            m_peakUntil[i] = now + 350;
        } else if (now > m_peakUntil[i]) {
            m_peak[i] = std::max(m_level[i], m_peak[i] - 0.7 * dt);
        }
    }
    showDots();
}

// Точки по столбикам: горящих (нижний ряд — всегда) и до пика. Кадр — только когда они сменились: в тишине и на ровном
// звуке открытое окно звука не заставляет стол рисовать и сжимать кадры-повторы (у НаомиОС рисовалось каждые 33 мс)
void SoundEqualizer::showDots()
{
    const int rows = std::max(int(height() / 3), 0);
    const size_t cols = size_t(m_cols);
    bool same = m_dotsOn.size() == cols && m_rows == rows;
    m_dotsOn.resize(cols);
    m_dotsTop.resize(cols);
    m_rows = rows;
    for (size_t i = 0; i < cols && rows > 0; ++i) {
        const int on = std::clamp(int(std::lround(m_level[i] * rows)), 1, rows);
        const int top = std::clamp(int(std::lround(m_peak[i] * rows)), on, rows);
        same = same && m_dotsOn[i] == on && m_dotsTop[i] == top;
        m_dotsOn[i] = uint8_t(on);
        m_dotsTop[i] = uint8_t(top);
    }
    if (!same)
        update();
}

// viz_dots: столбики прижаты к правому краю, точки 2 × 2 с шагом 3; горящие — .95, от них до пика — .26
void SoundEqualizer::paint(QPainter *painter)
{
    if (m_rows <= 0)
        return;
    const int cols = int(m_dotsOn.size());
    const int x0 = int(width()) - cols * 3 + 1, bottom = int(height());
    QColor lit = m_color, dim = m_color;
    lit.setAlphaF(0.95f);
    dim.setAlphaF(0.26f);
    for (int i = 0; i < cols; ++i)
        for (int r = 0; r < m_dotsTop[size_t(i)]; ++r)
            painter->fillRect(x0 + i * 3, bottom - (r + 1) * 3 + 1, 2, 2, r < m_dotsOn[size_t(i)] ? lit : dim);
}
