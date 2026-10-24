// FiDsp.h - JUCE-free DSP building blocks for "Frozen in Flames".
// Everything in here is header-only, allocation-free after prepare(), and
// unit-testable without a plugin host (see Tests/dsp_test.cpp).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fif
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept                   { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float v, float lo, float hi) noexcept  { return v < lo ? lo : (v > hi ? hi : v); }
inline float wrapPi (float x) noexcept                      { return x - kTwoPi * std::floor (x * (1.0f / kTwoPi) + 0.5f); }

//==============================================================================
/** Tiny xorshift PRNG - deterministic, lock-free, audio-thread safe. */
struct Rng
{
    explicit Rng (uint32_t seed = 0x9E3779B9u) noexcept : s (seed != 0 ? seed : 1u) {}

    uint32_t nextU32() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept        { return (float) (nextU32() >> 8) * (1.0f / 16777216.0f); }  // [0, 1)
    float bi() noexcept         { return uni() * 2.0f - 1.0f; }                              // [-1, 1)

    uint32_t s;
};

//==============================================================================
/** In-place radix-2 complex FFT. Forward is unscaled, inverse is unscaled
    (divide by N yourself). */
class FFT
{
public:
    void init (int order)
    {
        n = 1 << order;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);

        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * 3.14159265358979323846 * (double) i / (double) n;
            cosT[(size_t) i] = (float) std::cos (a);
            sinT[(size_t) i] = (float) std::sin (a);
        }

        rev.resize ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b))
                    r |= 1 << (order - 1 - b);

            rev[(size_t) i] = r;
        }
    }

    int size() const noexcept { return n; }

    void forward (float* re, float* im) const noexcept  { transform (re, im, false); }
    void inverse (float* re, float* im) const noexcept  { transform (re, im, true); }

private:
    void transform (float* re, float* im, bool inv) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (j > i)
            {
                std::swap (re[i], re[j]);
                std::swap (im[i], im[j]);
            }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1;
            const int step = n / len;

            for (int i = 0; i < n; i += len)
            {
                for (int j = 0, k = 0; j < half; ++j, k += step)
                {
                    const float wr = cosT[(size_t) k];
                    const float wi = inv ? -sinT[(size_t) k] : sinT[(size_t) k];
                    const int a = i + j, b = a + half;

                    const float tr = re[b] * wr - im[b] * wi;
                    const float ti = re[b] * wi + im[b] * wr;

                    re[b] = re[a] - tr;  im[b] = im[a] - ti;
                    re[a] += tr;         im[a] += ti;
                }
            }
        }
    }

    int n = 0;
    std::vector<float> cosT, sinT;
    std::vector<int> rev;
};

//==============================================================================
/** Transposed direct-form II biquad with RBJ shelving designs. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowShelf (double sr, double freq, double gainDb, double q = 0.7071) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * freq / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sq = 2.0 * std::sqrt (A) * al;

        const double nb0 =       A * ((A + 1) - (A - 1) * c + sq);
        const double nb1 =  2.0 * A * ((A - 1) - (A + 1) * c);
        const double nb2 =       A * ((A + 1) - (A - 1) * c - sq);
        const double na0 =            (A + 1) + (A - 1) * c + sq;
        const double na1 =     -2.0 * ((A - 1) + (A + 1) * c);
        const double na2 =            (A + 1) + (A - 1) * c - sq;
        set (nb0, nb1, nb2, na0, na1, na2);
    }

    void setHighShelf (double sr, double freq, double gainDb, double q = 0.7071) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * freq / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), sq = 2.0 * std::sqrt (A) * al;

        const double nb0 =        A * ((A + 1) + (A - 1) * c + sq);
        const double nb1 = -2.0 * A * ((A - 1) + (A + 1) * c);
        const double nb2 =        A * ((A + 1) + (A - 1) * c - sq);
        const double na0 =             (A + 1) - (A - 1) * c + sq;
        const double na1 =       2.0 * ((A - 1) - (A + 1) * c);
        const double na2 =             (A + 1) - (A - 1) * c - sq;
        set (nb0, nb1, nb2, na0, na1, na2);
    }

private:
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = (float) (nb0 * inv);  b1 = (float) (nb1 * inv);  b2 = (float) (nb2 * inv);
        a1 = (float) (na1 * inv);  a2 = (float) (na2 * inv);
    }
};

/** One-pole DC blocker / sub-sonic high-pass. */
struct DcBlocker
{
    void setCutoff (double sr, double fc) noexcept  { R = (float) (1.0 - 2.0 * 3.14159265358979323846 * fc / sr); }
    void reset() noexcept                            { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    float R = 0.995f, x1 = 0.0f, y1 = 0.0f;
};

/** Fixed integer delay (used to latency-compensate the dry path). */
struct FixedDelay
{
    void prepare (int delaySamples)
    {
        d = std::max (0, delaySamples);
        buf.assign ((size_t) std::max (1, d), 0.0f);
        pos = 0;
    }

    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }

    float process (float x) noexcept
    {
        if (d == 0)
            return x;

        const float y = buf[(size_t) pos];
        buf[(size_t) pos] = x;
        if (++pos >= d) pos = 0;
        return y;
    }

    int d = 0, pos = 0;
    std::vector<float> buf;
};

//==============================================================================
/** The four "flame" models. All take a pre-driven sample and return roughly +/-1. */
enum class FlameType : int { Ember = 0, Blaze = 1, Inferno = 2, Cinder = 3 };

inline float shape (FlameType type, float x, float bias) noexcept
{
    switch (type)
    {
        case FlameType::Ember:    // warm, asymmetric tube-style tanh (even + odd harmonics)
            return std::tanh (x + bias) - std::tanh (bias);

        case FlameType::Blaze:    // hard-knee superellipse clipper (bright transistor bite)
        {
            const float a = std::fabs (x);
            return x / std::cbrt (1.0f + a * a * a);
        }

        case FlameType::Inferno:  // sine wavefolder (metallic, harmonically dense)
            return std::sin (x);

        case FlameType::Cinder:   // rectifying octave-fuzz (strong 2nd harmonic)
        {
            const float s = std::tanh (1.5f * x);
            return 0.55f * s + 0.75f * std::fabs (s);
        }
    }

    return x;
}

//==============================================================================
/** Organic "flame flicker": three smoothed random layers at related rates. Output in [-1, 1]. */
class FlameFlicker
{
public:
    void prepare (double sampleRate, uint32_t seed)
    {
        sr  = (float) sampleRate;
        rng = Rng (seed);

        for (auto& l : layers)
        {
            l.phase  = rng.uni();
            l.value  = 0.0f;
            l.target = rng.bi();
        }

        setRate (3.0f);
    }

    void setRate (float hz) noexcept
    {
        static constexpr float ratio[3] = { 1.0f, 2.37f, 5.19f };

        for (int i = 0; i < 3; ++i)
        {
            const float r = hz * ratio[i];
            layers[(size_t) i].inc   = r / sr;
            layers[(size_t) i].coeff = 1.0f - std::exp (-kTwoPi * r * 1.2f / sr);
        }
    }

    float next() noexcept
    {
        static constexpr float amp[3] = { 0.62f, 0.28f, 0.10f };
        float sum = 0.0f;

        for (int i = 0; i < 3; ++i)
        {
            auto& l = layers[(size_t) i];
            l.phase += l.inc;

            if (l.phase >= 1.0f)
            {
                l.phase -= 1.0f;
                l.target = rng.bi();
            }

            l.value += (l.target - l.value) * l.coeff;
            sum += l.value * amp[i];
        }

        return clampf (sum * 1.6f, -1.0f, 1.0f);
    }

private:
    struct Layer { float phase = 0, value = 0, target = 0, inc = 0, coeff = 0; };

    std::array<Layer, 3> layers {};
    float sr = 44100.0f;
    Rng rng;
};

} // namespace fif
