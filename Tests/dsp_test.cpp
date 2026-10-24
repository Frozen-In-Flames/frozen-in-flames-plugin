// dsp_test.cpp - JUCE-free numeric tests for the Frozen in Flames DSP core.
//
// Plain C++17, no framework. Build:
//   g++ -std=c++17 -O2 -I Source Tests/dsp_test.cpp -o dsp_test
//
// Every check prints PASS/FAIL and the process exit code is non-zero if any
// check fails. Tolerances are stated at each check and are part of the test.

#include "../Source/FiDsp.h"
#include "../Source/SpectralFreezer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static constexpr double PI = 3.14159265358979323846;

static int g_pass = 0, g_fail = 0;

static void check (bool ok, const std::string& name, const std::string& detail = "")
{
    if (ok) { ++g_pass; std::printf ("  PASS  %s\n", name.c_str()); }
    else    { ++g_fail; std::printf ("  FAIL  %s  %s\n", name.c_str(), detail.c_str()); }
}

static double rmsOf (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * (double) x[i];
    return std::sqrt (s / (double) n);
}

// Goertzel magnitude of a real signal at an arbitrary frequency.
static double goertzel (const float* x, int n, double sr, double freq)
{
    const double w = 2.0 * PI * freq / sr;
    const double cw = std::cos (w), coeff = 2.0 * cw;
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        s0 = (double) x[i] + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) * 2.0 / (double) n;
}

//==============================================================================
static void testFftRoundTrip()
{
    std::printf ("[FFT round trip]\n");
    const int order = 10, n = 1 << order;
    fif::FFT fft;
    fft.init (order);

    fif::Rng rng (1234);
    std::vector<float> re ((size_t) n), im ((size_t) n), re0 ((size_t) n), im0 ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / (double) n;
        re[(size_t) i] = (float) (0.6 * std::sin (2.0 * PI * 7.0 * t)
                                + 0.3 * std::sin (2.0 * PI * 41.0 * t + 0.7)
                                + 0.2 * rng.bi());
        im[(size_t) i] = 0.0f;
    }
    re0 = re; im0 = im;

    fft.forward (re.data(), im.data());
    fft.inverse (re.data(), im.data());

    double maxErr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double er = std::fabs ((double) re[(size_t) i] / n - re0[(size_t) i]);
        const double ei = std::fabs ((double) im[(size_t) i] / n - im0[(size_t) i]);
        maxErr = std::max (maxErr, std::max (er, ei));
    }
    char buf[128];
    std::snprintf (buf, sizeof buf, "maxErr=%.2e tol=1e-3", maxErr);
    check (maxErr < 1e-3, "forward+inverse reconstructs input (scaled by 1/N)", buf);
}

//==============================================================================
// Runs one freeze scenario. Returns stats about the frozen layer.
struct FreezeRun
{
    double inputRms   = 0.0;   // RMS of the source in its final window pre-freeze
    double rmsAt1s    = 0.0;   // frozen RMS, window starting 1.0 s after engage
    double rmsLate    = 0.0;   // frozen RMS, window starting 2.25 s after engage
    double peakFreq   = 0.0;   // Goertzel peak of the late window
    double peakLate   = 0.0;   // max |sample| in the late window
    bool   allFinite  = true;
};

static FreezeRun runFreeze (bool noiseSource, float glass, float amp)
{
    const double sr = 48000.0;
    const int preSamples   = (int) (0.75 * sr);
    const int postSamples  = (int) (3.00 * sr);

    fif::SpectralFreezer fz;
    fz.prepare (sr, 7u);

    fif::SpectralFreezer::Params p;
    p.freeze = false; p.glass = glass; p.melt = 0.0f; p.crystal = 0.0f; p.pitch = 0.0f; p.thaw = 0.8f;
    fz.setParams (p);

    fif::Rng rng (99);
    double phase = 0.0;
    const double dphi = 2.0 * PI * 220.0 / sr;

    std::vector<float> tailIn;
    for (int i = 0; i < preSamples; ++i)
    {
        float x;
        if (noiseSource) { x = amp * rng.bi(); }
        else             { x = (float) (amp * std::sin (phase)); phase += dphi; }
        if (i >= preSamples - (int) (0.25 * sr)) tailIn.push_back (x);
        (void) fz.processSample (x);
    }

    FreezeRun r;
    r.inputRms = rmsOf (tailIn.data(), (int) tailIn.size());

    p.freeze = true;
    fz.setParams (p);

    std::vector<float> out ((size_t) postSamples);
    for (int i = 0; i < postSamples; ++i)
    {
        out[(size_t) i] = fz.processSample (0.0f);
        if (! std::isfinite (out[(size_t) i])) r.allFinite = false;
    }

    const int w1 = (int) (1.00 * sr), w2 = (int) (2.25 * sr), wlen = (int) (0.50 * sr);
    r.rmsAt1s = rmsOf (out.data() + w1, wlen);
    r.rmsLate = rmsOf (out.data() + w2, wlen);

    double pk = 0.0;
    for (int i = w2; i < w2 + wlen; ++i) pk = std::max (pk, (double) std::fabs (out[(size_t) i]));
    r.peakLate = pk;

    double best = -1.0;
    for (double fq = 150.0; fq <= 300.0; fq += 1.0)
    {
        const double m = goertzel (out.data() + w2, wlen, sr, fq);
        if (m > best) { best = m; r.peakFreq = fq; }
    }
    return r;
}

static void testFreezeSine()
{
    std::printf ("[Freeze sustain: 220 Hz sine, coherent-leaning glass=0.85]\n");
    const auto r = runFreeze (false, 0.85f, 0.5f);
    char buf[256];

    std::snprintf (buf, sizeof buf, "lateRMS=%.4f inputRMS=%.4f ratio=%.3f (need > 0.35)",
                   r.rmsLate, r.inputRms, r.rmsLate / r.inputRms);
    check (r.rmsLate > 0.35 * r.inputRms, "frozen layer still sustains 2.25 s after capture", buf);

    std::snprintf (buf, sizeof buf, "lateRMS=%.4f ceiling=%.4f", r.rmsLate, 2.0 * r.inputRms);
    check (r.rmsLate < 2.0 * r.inputRms && r.peakLate < 4.0 * 0.5, "frozen layer does not explode", buf);

    std::snprintf (buf, sizeof buf, "peak=%.1f Hz (need within 4 Hz of 220)", r.peakFreq);
    check (std::fabs (r.peakFreq - 220.0) <= 4.0, "frozen dominant frequency stays at 220 Hz", buf);

    const double db = 20.0 * std::log10 (r.rmsAt1s / r.inputRms);
    std::snprintf (buf, sizeof buf, "level at 1 s = %+.2f dB vs capture (need within +/-4 dB)", db);
    check (std::fabs (db) <= 4.0, "AGC holds frozen level near capture level at 1 s", buf);

    check (r.allFinite, "all frozen output samples finite");
}

static void testFreezeDiffuse()
{
    std::printf ("[Freeze sustain: 220 Hz sine, diffuse glass=0.15]\n");
    const auto r = runFreeze (false, 0.15f, 0.5f);
    char buf[256];
    std::snprintf (buf, sizeof buf, "lateRMS=%.4f inputRMS=%.4f ratio=%.3f (need 0.3..2.0)",
                   r.rmsLate, r.inputRms, r.rmsLate / r.inputRms);
    check (r.rmsLate > 0.3 * r.inputRms && r.rmsLate < 2.0 * r.inputRms && r.allFinite,
           "diffuse freeze sustains within level band", buf);
}

static void testFreezeNoise()
{
    std::printf ("[Freeze of white noise]\n");
    const auto r = runFreeze (true, 0.4f, 0.5f);
    char buf[256];
    std::snprintf (buf, sizeof buf, "lateRMS=%.4f peak=%.3f finite=%d", r.rmsLate, r.peakLate, (int) r.allFinite);
    check (r.allFinite && r.rmsLate < 2.0 && r.rmsLate > 0.005 && r.peakLate < 5.0,
           "frozen noise stays finite, bounded and audible", buf);
}

//==============================================================================
static void testSaturators()
{
    std::printf ("[Saturator curves]\n");
    const fif::FlameType types[4] = { fif::FlameType::Ember, fif::FlameType::Blaze,
                                      fif::FlameType::Inferno, fif::FlameType::Cinder };
    const char* names[4] = { "Ember", "Blaze", "Inferno", "Cinder" };

    for (int t = 0; t < 4; ++t)
    {
        bool finite = true;
        for (double x = -4.0; x <= 4.0; x += 0.001)
            for (float bias : { 0.0f, 0.3f, -0.3f })
                if (! std::isfinite (fif::shape (types[t], (float) x, bias)))
                    finite = false;
        check (finite, std::string (names[t]) + " finite on [-4, 4] for bias in {0, +/-0.3}");
    }

    double worst = 0.0;
    for (double x = -4.0; x <= 4.0; x += 0.001)
        worst = std::max (worst, (double) std::fabs (fif::shape (fif::FlameType::Ember, (float) x, 0.0f)
                                                   + fif::shape (fif::FlameType::Ember, (float) -x, 0.0f)));
    char buf[128];
    std::snprintf (buf, sizeof buf, "worst asymmetry=%.2e tol=1e-5", worst);
    check (worst < 1e-5, "Ember at bias 0 is odd-symmetric (pure tanh)", buf);

    double mean = 0.0;
    const int n = 48000;
    for (int i = 0; i < n; ++i)
    {
        const double ph = 2.0 * PI * 100.0 * (double) i / 48000.0;
        mean += fif::shape (fif::FlameType::Cinder, (float) (0.8 * std::sin (ph)), 0.0f);
    }
    mean /= n;
    std::snprintf (buf, sizeof buf, "DC mean=%.4f (need > 0.05)", mean);
    check (mean > 0.05, "Cinder rectifies: sine in, positive DC out", buf);

    double mx = 0.0;
    for (double x = -4.0; x <= 4.0; x += 0.001)
        mx = std::max (mx, (double) std::fabs (fif::shape (fif::FlameType::Inferno, (float) x, 0.0f)));
    std::snprintf (buf, sizeof buf, "max |out|=%.6f (need <= 1)", mx);
    check (mx <= 1.0 + 1e-6, "Inferno output bounded by 1", buf);
}

//==============================================================================
static void testCinderDcFix()
{
    std::printf ("[Cinder DC and the fire-stage DC blocker fix]\n");
    // The Cinder shaper rectifies by design, which puts a large DC offset on
    // its output (measured mean +0.446 on a 0.8-amplitude 100 Hz sine). The
    // plugin's fire stage therefore runs every channel through fif::DcBlocker
    // after the shaper. This check proves the blocker (a FiDsp part, so it is
    // testable here) removes that DC while the raw path keeps it.
    fif::DcBlocker dc;
    dc.setCutoff (48000.0, 12.0);
    const int n = 48000;
    double meanRaw = 0.0, meanFix = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; ++i)
    {
        const double ph = 2.0 * PI * 100.0 * (double) i / 48000.0;
        const float y = fif::shape (fif::FlameType::Cinder, (float) (0.8 * std::sin (ph)), 0.0f);
        const float z = dc.process (y);
        if (i >= n / 2) { meanRaw += y; meanFix += z; ++cnt; }
    }
    meanRaw /= cnt; meanFix /= cnt;
    char buf[160];
    std::snprintf (buf, sizeof buf, "raw mean=%+.4f (need > 0.2, the defect)", meanRaw);
    check (meanRaw > 0.2, "raw Cinder output carries the expected rectification DC", buf);
    std::snprintf (buf, sizeof buf, "blocked mean=%+.5f (need |mean| < 0.005)", meanFix);
    check (std::fabs (meanFix) < 0.005, "DcBlocker at 12 Hz removes the Cinder DC", buf);
}

//==============================================================================
int main()
{
    std::printf ("Frozen in Flames - DSP core tests\n\n");
    testFftRoundTrip();
    testFreezeSine();
    testFreezeDiffuse();
    testFreezeNoise();
    testSaturators();
    testCinderDcFix();
    std::printf ("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
