// НаомиФон — спектр для эквалайзера окна звука (SoundEqualizer, docs/DESKTOP.md «Звук»), числа — audio.c ring_spectrum
// НаомиОС: последние 64 мс звука с окном Ханна → БПФ → мощность по столбикам. Полосы — по логарифму 50 Гц…7 кГц; полоса
// уже шага БПФ — мощность в её середине по соседним отсчётам, иначе — наибольшая в ней. Уровень — дБ от полной синусоиды,
// +4,5 дБ на октаву вокруг 1 кГц.
// Звук — 48 кГц, как его играет стол (у НаомиОС 16 кГц, 1024 отсчёта): 3072 отсчёта — те же 64 мс, то же разрешение по
// частоте (лепесток окна 62,5 Гц), БПФ 4096 — дополнено нулями; вещественный звук — комплексным БПФ 2048. Пересчёт
// частоты не нужен — и нет наложения частот выше 8 кГц. Окно, повороты БПФ и полосы считаются один раз, на кадр — только
// БПФ до 7 кГц. Звука меньше 64 мс (свой захват только что открыт) — окно Ханна по тому, что есть.
// БПФ — во float, вещественная и мнимая части отдельными рядами (умножение std::complex GCC без -ffast-math зовёт
// функцией __muldc3), повороты каждого шага — подряд, первые два шага — четвёрками без умножений: на малом ядре
// телефона (Cortex-A55) БПФ 2048 — 141 мкс вместо 262 у простого (замер 17 ноября 2026). Точности float хватает с
// запасом: шум БПФ ниже −120 дБ, столбики — в пределах 16–70 дБ.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

class SoundSpectrum
{
public:
    static constexpr int kRate = 48000;
    static constexpr int kWin = 3072;   // 64 мс
    static constexpr int kN = 4096;
    static constexpr int kMin = 480;    // 10 мс — меньше не считать

    SoundSpectrum()
    {
        m_hann.resize(kWin);
        for (int i = 0; i < kWin; ++i)
            m_hann[size_t(i)] = float(0.5 - 0.5 * std::cos(2 * M_PI * i / (kWin - 1)));
        m_rev.resize(kM);
        for (size_t i = 1, j = 0; i < kM; ++i) {
            size_t bit = kM >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            m_rev[i] = j;
        }
        // повороты шага с длиной len — с отступа len / 2 − 1, len / 2 штук подряд (шаги 2 и 4 их не берут)
        m_twRe.resize(kM);
        m_twIm.resize(kM);
        for (size_t len = 2; len <= kM; len <<= 1)
            for (size_t j = 0; j < len / 2; ++j) {
                m_twRe[len / 2 - 1 + j] = float(std::cos(2 * M_PI * double(j) / double(len)));
                m_twIm[len / 2 - 1 + j] = float(-std::sin(2 * M_PI * double(j) / double(len)));
            }
        m_postRe.resize(kM + 1);
        m_postIm.resize(kM + 1);
        for (size_t k = 0; k <= kM; ++k) {   // e^(−2πik/N) / (2i)
            m_postRe[k] = -0.5 * std::sin(2 * M_PI * double(k) / kN);
            m_postIm[k] = -0.5 * std::cos(2 * M_PI * double(k) / kN);
        }
        m_re.resize(kM);
        m_im.resize(kM);
        m_power.resize(kM + 1);
    }

    void setBands(int count)
    {
        m_bands.assign(size_t(std::max(count, 0)), {});
        const double bin = double(kRate) / kN;
        m_top = 0;
        for (int i = 0; i < count; ++i) {
            Band &b = m_bands[size_t(i)];
            const double f0 = 50 * std::pow(7000.0 / 50, double(i) / count);
            const double f1 = 50 * std::pow(7000.0 / 50, double(i + 1) / count);
            const double k0 = f0 / bin, k1 = f1 / bin;
            b.from = int(std::ceil(k0));
            b.to = int(std::floor(k1));
            if (b.to < b.from) {   // уже шага БПФ — середина полосы между соседними отсчётами
                const double kc = (k0 + k1) / 2;
                b.from = int(std::floor(kc));
                b.to = b.from + 1;
                b.frac = kc - b.from;
            }
            b.tilt = 4.5 * std::log2(std::sqrt(f0 * f1) / 1000);
            m_top = std::max(m_top, size_t(std::min(b.to, int(kM))));
        }
    }
    int bands() const { return int(m_bands.size()); }

    // x — последние n отсчётов (по порядку); db — по полосе (размер — bands()); false — звука меньше kMin
    bool compute(const float *x, size_t n, double gainDb, std::vector<double> &db)
    {
        n = std::min(n, size_t(kWin));
        if (n < size_t(kMin) || m_bands.empty())
            return false;
        // окно → z: чётные отсчёты — вещественная часть, нечётные — мнимая; дальше — нули
        const float *w = m_hann.data();
        if (n < size_t(kWin)) {   // короткое — своё окно Ханна (первые кадры своего захвата)
            m_short.resize(n);
            for (size_t i = 0; i < n; ++i)
                m_short[i] = float(0.5 - 0.5 * std::cos(2 * M_PI * double(i) / double(n - 1)));
            w = m_short.data();
        }
        const size_t pairs = n / 2;
        for (size_t j = 0; j < pairs; ++j) {
            m_re[j] = x[2 * j] * w[2 * j];
            m_im[j] = x[2 * j + 1] * w[2 * j + 1];
        }
        size_t used = pairs;
        if (n & 1) {
            m_re[pairs] = x[n - 1] * w[n - 1];
            m_im[pairs] = 0;
            ++used;
        }
        std::fill(m_re.begin() + long(used), m_re.end(), 0.f);
        std::fill(m_im.begin() + long(used), m_im.end(), 0.f);
        fft();
        // спектр вещественного звука из комплексного Z: a = Z[k], b = Z*[M − k];
        // X[k] = (a + b) / 2 + e^(−2πik/N) · (a − b) / (2i) — только до 7 кГц
        const double norm = 1.0 / ((double(n) / 4) * (double(n) / 4));
        for (size_t k = 0; k <= m_top; ++k) {   // m_top < kM
            const size_t j = k ? kM - k : 0;
            const double ar = m_re[k], ai = m_im[k], br = m_re[j], bi = -m_im[j];
            const double dr = ar - br, di = ai - bi;
            const double xr = 0.5 * (ar + br) + m_postRe[k] * dr - m_postIm[k] * di;
            const double xi = 0.5 * (ai + bi) + m_postRe[k] * di + m_postIm[k] * dr;
            m_power[k] = (xr * xr + xi * xi) * norm;
        }
        db.resize(m_bands.size());
        for (size_t i = 0; i < m_bands.size(); ++i) {
            const Band &b = m_bands[i];
            double p = 0;
            if (b.frac >= 0)
                p = m_power[size_t(b.from)] * (1 - b.frac) + m_power[size_t(b.to)] * b.frac;
            else
                for (int k = b.from; k <= b.to; ++k)
                    p = std::max(p, m_power[size_t(k)]);
            db[i] = 10 * std::log10(p + 1e-12) + b.tilt + gainDb;
        }
        return true;
    }

private:
    static constexpr size_t kM = kN / 2;   // комплексное БПФ

    void fft()
    {
        float *__restrict re = m_re.data(), *__restrict im = m_im.data();
        for (size_t i = 1; i < kM; ++i)
            if (i < m_rev[i]) {
                std::swap(re[i], re[m_rev[i]]);
                std::swap(im[i], im[m_rev[i]]);
            }
        // шаги длиной 2 и 4 — четвёрками: повороты 1 и −i
        for (size_t i = 0; i < kM; i += 4) {
            const float r0 = re[i] + re[i + 1], i0 = im[i] + im[i + 1], r1 = re[i] - re[i + 1], i1 = im[i] - im[i + 1];
            const float r2 = re[i + 2] + re[i + 3], i2 = im[i + 2] + im[i + 3];
            const float r3 = re[i + 2] - re[i + 3], i3 = im[i + 2] - im[i + 3];
            re[i] = r0 + r2;
            im[i] = i0 + i2;
            re[i + 2] = r0 - r2;
            im[i + 2] = i0 - i2;
            re[i + 1] = r1 + i3;   // (r3 + i·i3) · (−i) = i3 − i·r3
            im[i + 1] = i1 - r3;
            re[i + 3] = r1 - i3;
            im[i + 3] = i1 + r3;
        }
        for (size_t len = 8; len <= kM; len <<= 1) {
            const size_t half = len / 2;
            const float *__restrict twr = m_twRe.data() + half - 1, *__restrict twi = m_twIm.data() + half - 1;
            for (size_t i = 0; i < kM; i += len) {
                float *__restrict pr = re + i, *__restrict pi = im + i;
                float *__restrict qr = re + i + half, *__restrict qi = im + i + half;
                for (size_t j = 0; j < half; ++j) {
                    const float a = qr[j], b = qi[j], c = pr[j], d = pi[j];
                    const float vr = a * twr[j] - b * twi[j], vi = a * twi[j] + b * twr[j];
                    qr[j] = c - vr;
                    qi[j] = d - vi;
                    pr[j] = c + vr;
                    pi[j] = d + vi;
                }
            }
        }
    }

    struct Band {
        int from = 0, to = 0;   // отсчёты БПФ: наибольший из них или (frac ≥ 0) между from и to
        double frac = -1;
        double tilt = 0;        // +4,5 дБ на октаву от 1 кГц
    };
    std::vector<float> m_hann, m_short;
    std::vector<size_t> m_rev;
    std::vector<float> m_twRe, m_twIm, m_re, m_im;
    std::vector<double> m_postRe, m_postIm;
    std::vector<double> m_power;
    std::vector<Band> m_bands;
    size_t m_top = 0;   // последний нужный отсчёт БПФ
};
