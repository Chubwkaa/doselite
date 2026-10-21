#pragma once
// Kleine DSP-Bausteine fuer DoseLite (ohne externe Abhaengigkeiten).
#include <algorithm>
#include <cmath>
#include <vector>

namespace dl
{
constexpr double kPi = 3.14159265358979323846;

// Einpol-Koeffizient fuer Hüllkurven: Zeit in ms -> Schrittweite pro Sample
inline float coefMs (double sr, double ms)
{
    if (ms <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sr)));
}

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }

//==============================================================================
// Biquad nach den RBJ-Audio-EQ-Cookbook-Formeln, ohne Speicherallokation
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setRaw (double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = (float) (nb0 / a0);
        b1 = (float) (nb1 / a0);
        b2 = (float) (nb2 / a0);
        a1 = (float) (na1 / a0);
        a2 = (float) (na2 / a0);
    }

    void highpass (double sr, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void lowpass (double sr, double f, double q = 0.7071)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void peak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        setRaw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void lowShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0);
        const double beta = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1) - (A - 1) * c + beta),
                2 * A * ((A - 1) - (A + 1) * c),
                A * ((A + 1) - (A - 1) * c - beta),
                (A + 1) + (A - 1) * c + beta,
                -2 * ((A - 1) + (A + 1) * c),
                (A + 1) + (A - 1) * c - beta);
    }

    void highShelf (double sr, double f, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0);
        const double beta = 2.0 * std::sqrt (A) * al;
        setRaw (A * ((A + 1) + (A - 1) * c + beta),
                -2 * A * ((A - 1) + (A + 1) * c),
                A * ((A + 1) + (A - 1) * c - beta),
                (A + 1) - (A - 1) * c + beta,
                2 * ((A - 1) - (A + 1) * c),
                (A + 1) - (A - 1) * c - beta);
    }
};

//==============================================================================
// Ringpuffer mit ganzzahligem und gebrochenem Lesezugriff. read(0) = zuletzt geschriebenes Sample.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int sz = 1;
        while (sz < maxSamples + 4) sz <<= 1;
        buf.assign ((size_t) sz, 0.0f);
        mask = sz - 1;
        w = 0;
    }

    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }

    float readFrac (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = read (i), b = read (i + 1);
        return a + (b - a) * f;
    }
};

//==============================================================================
// Einfacher Pitch-Shifter mit zwei ueberblendeten Verzoegerungstaps (Hann-Fenster).
// Qualitaet: "lite". Fuer kleine Korrekturen brauchbar, bei grossen Spruengen hoerbar.
struct PitchShifter
{
    DelayLine dl;
    float phase = 0.0f, grain = 1.0f;

    void prepare (double sr)
    {
        grain = (float) (0.030 * sr);
        dl.prepare ((int) grain + 8);
        phase = 0.0f;
    }

    float process (float x, float ratio)
    {
        dl.push (x);
        phase += (1.0f - ratio) / grain;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float g1 = 0.5f - 0.5f * std::cos ((float) (2.0 * kPi) * phase);
        const float g2 = 0.5f - 0.5f * std::cos ((float) (2.0 * kPi) * p2);
        return dl.readFrac (phase * grain) * g1 + dl.readFrac (p2 * grain) * g2;
    }
};

//==============================================================================
// Tonhoehenerkennung nach dem YIN-Prinzip auf ca. 11 kHz heruntergetaktetem Signal
class PitchDetector
{
public:
    void prepare (double sr)
    {
        dec = std::max (1, (int) std::lround (sr / 11025.0));
        fs = sr / dec;
        minLag = std::max (2, (int) std::floor (fs / 800.0));
        maxLag = (int) std::ceil (fs / 70.0);
        W = (int) (0.035 * fs);
        N = W + maxLag + 2;
        ring.assign ((size_t) N, 0.0f);
        lin.assign ((size_t) N, 0.0f);
        d.assign ((size_t) maxLag + 2, 0.0f);
        wp = 0; acc = 0.0f; cnt = 0;
    }

    void push (float x)
    {
        acc += x;
        if (++cnt >= dec)
        {
            ring[(size_t) wp] = acc / (float) dec;
            wp = (wp + 1) % N;
            acc = 0.0f;
            cnt = 0;
        }
    }

    // liefert Frequenz in Hz oder 0, wenn kein klarer Ton erkannt wurde
    float detect()
    {
        for (int i = 0; i < N; ++i)
            lin[(size_t) i] = ring[(size_t) ((wp + N - 1 - i) % N)]; // 0 = neuestes Sample

        double e = 0.0;
        for (int j = 0; j < W; ++j) e += (double) lin[(size_t) j] * lin[(size_t) j];
        if (e / W < 1.0e-6) return 0.0f; // zu leise

        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < W; ++j)
            {
                const double df = (double) lin[(size_t) j] - lin[(size_t) (j + tau)];
                s += df * df;
            }
            d[(size_t) tau] = (float) s;
        }

        double run = 0.0;
        d[0] = 1.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            run += d[(size_t) tau];
            d[(size_t) tau] = run > 0.0 ? (float) (d[(size_t) tau] * tau / run) : 1.0f;
        }

        int tau = -1;
        for (int t = minLag; t < maxLag; ++t)
        {
            if (d[(size_t) t] < 0.15f)
            {
                while (t + 1 < maxLag && d[(size_t) t + 1] < d[(size_t) t]) ++t;
                tau = t;
                break;
            }
        }
        if (tau < 0) return 0.0f;

        float better = (float) tau;
        if (tau > 1 && tau < maxLag)
        {
            const float s0 = d[(size_t) tau - 1], s1 = d[(size_t) tau], s2 = d[(size_t) tau + 1];
            const float den = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::fabs (den) > 1.0e-9f) better = (float) tau + (s2 - s0) / den;
        }
        return (float) (fs / better);
    }

private:
    int dec = 4, minLag = 14, maxLag = 160, W = 380, N = 600, wp = 0, cnt = 0;
    double fs = 11025.0;
    float acc = 0.0f;
    std::vector<float> ring, lin, d;
};

//==============================================================================
// naechster erlaubter Ton einer Tonart (scale: 0 chromatisch, 1 Dur, 2 Moll), Rueckgabe als MIDI-Note
inline float quantizeMidi (float midi, int key, int scale)
{
    static const bool maj[12]  = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
    static const bool mino[12] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 };

    const int base = (int) std::lround (midi);
    if (scale == 0) return (float) base;

    const bool* allowed = (scale == 1) ? maj : mino;
    float best = (float) base, bestDist = 1.0e9f;
    for (int n = base - 6; n <= base + 6; ++n)
    {
        const int pc = (((n - key) % 12) + 12) % 12;
        if (allowed[pc])
        {
            const float dist = std::fabs ((float) n - midi);
            if (dist < bestDist) { bestDist = dist; best = (float) n; }
        }
    }
    return best;
}
} // namespace dl
