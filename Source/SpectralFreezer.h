// SpectralFreezer.h - the "ice" half of Frozen in Flames.
//
// A mono STFT freeze engine (one instance per channel):
//   * Always analyses the incoming signal (4096-pt @ 44.1/48k, 75% overlap, Hann).
//   * When Freeze engages it captures the current magnitude spectrum plus each
//     bin's instantaneous frequency (phase-vocoder style) and keeps re-synthesising
//     it, so the held sound sustains indefinitely.
//   * GLASS   blends between coherent partials (phase-locked, crystalline) and
//             diffuse random phase (smooth, Paulstretch-style pad).
//   * MELT    lets the held spectrum slowly drift toward whatever is live.
//   * CRYSTAL adds octave + octave-and-a-fifth shimmer voices (always diffuse).
//   * PITCH   transposes the frozen layer (diffuse when not 0).
//   * A feed-forward level normaliser keeps the frozen layer as loud as the
//     audio that was captured, whatever the phase mode.
//
// Header-only, JUCE-free, allocation-free after prepare().
#pragma once

#include "FiDsp.h"
#include <atomic>

namespace fif
{
class SpectralFreezer
{
public:
    static constexpr int kNumVoices = 3;
    static constexpr int kNumBands  = 48;   // log-spaced bands published for the UI
    static constexpr int kOverlap   = 4;    // hop = N / 4

    struct Params
    {
        bool  freeze  = false;
        float melt    = 0.0f;   // 0..1   (0 = permafrost)
        float glass   = 0.4f;   // 0..1   (0 = diffuse frost, 1 = coherent glass)
        float crystal = 0.0f;   // 0..1   shimmer amount
        float pitch   = 0.0f;   // semitones
        float thaw    = 0.8f;   // seconds: time to fade 60 dB after un-freezing
    };

    /** Optional lock-free feed for the editor's visualiser. Arrays hold kNumBands atomics. */
    struct VisualFeed
    {
        std::atomic<float>* live   = nullptr;
        std::atomic<float>* ice    = nullptr;
        std::atomic<float>* amount = nullptr;
    };

    //==========================================================================
    void prepare (double sampleRate, uint32_t seed = 1u)
    {
        sr = sampleRate;

        const int order = std::clamp ((int) std::lround (std::log2 (0.085 * sr)), 11, 13);
        N     = 1 << order;
        hop   = N / kOverlap;
        nBins = N / 2 + 1;
        mask  = N - 1;
        fft.init (order);

        win.assign ((size_t) N, 0.0f);
        double sumSq = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) n / (double) N);
            win[(size_t) n] = (float) w;
            sumSq += w * w;
        }
        meanSqWin = (float) (sumSq / (double) N);

        inRing.assign ((size_t) N, 0.0f);
        outRing.assign ((size_t) N, 0.0f);
        re.assign ((size_t) N, 0.0f);
        im.assign ((size_t) N, 0.0f);
        frame.assign ((size_t) N, 0.0f);

        for (auto* v : { &liveMag, &livePhase, &prevPhase, &liveDev, &avgMag, &heldMag, &heldDev, &phiCoh })
            v->assign ((size_t) nBins, 0.0f);

        for (int v = 0; v < kNumVoices; ++v)
        {
            vMag[(size_t) v].assign ((size_t) nBins, 0.0f);
            phiRand[(size_t) v].assign ((size_t) nBins, 0.0f);
        }

        // log-spaced visual bands
        const double fLo = 30.0, fHi = std::min (18000.0, 0.45 * sr), binHz = sr / (double) N;
        for (int b = 0; b < kNumBands; ++b)
        {
            const double f0 = fLo * std::pow (fHi / fLo, (double) b / (double) kNumBands);
            const double f1 = fLo * std::pow (fHi / fLo, (double) (b + 1) / (double) kNumBands);
            int lo = (int) std::floor (f0 / binHz);
            int hi = (int) std::ceil (f1 / binHz) - 1;
            lo = std::clamp (lo, 1, nBins - 2);
            hi = std::clamp (std::max (hi, lo), lo, nBins - 2);
            bandLo[(size_t) b]   = lo;
            bandHi[(size_t) b]   = hi;
            bandTilt[(size_t) b] = (float) (3.0 * std::log2 (std::sqrt (f0 * f1) / 500.0));
        }

        attackCoeff = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
        rng = Rng (seed * 2654435761u + 12345u);

        setParams (params);
        reset();
    }

    void reset() noexcept
    {
        for (auto* v : { &inRing, &outRing, &liveMag, &livePhase, &prevPhase, &liveDev, &avgMag, &heldMag, &heldDev, &phiCoh })
            std::fill (v->begin(), v->end(), 0.0f);

        writePos = readPos = hopCounter = 0;
        framesAnalysed = framesSinceCapture = 0;
        frozen = active = preRollPending = false;
        layerGain = layerTarget = 0.0f;
        agcCur = 1.0f;  agcInc = 0.0f;  agcPower = 0.0f;  targetPower = 0.0f;  hopPower = 0.0f;
        pitchSm = 0.0f;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        const float thaw = std::max (0.02f, p.thaw);
        releaseCoeff = 1.0f - std::exp (-6.9f / (thaw * (float) sr));   // -60 dB after 'thaw' seconds
    }

    void setVisualFeed (const VisualFeed& f) noexcept   { feed = f; }

    //==========================================================================
    float processSample (float in) noexcept
    {
        inRing[(size_t) writePos] = in;
        writePos = (writePos + 1) & mask;

        const float raw = active ? outRing[(size_t) readPos] : 0.0f;
        outRing[(size_t) readPos] = 0.0f;
        readPos = (readPos + 1) & mask;

        hopPower += raw * raw;
        agcCur   += agcInc;
        layerGain += (layerTarget - layerGain) * (layerTarget > layerGain ? attackCoeff : releaseCoeff);

        const float out = raw * agcCur * layerGain;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame();
        }

        return out;
    }

    void process (const float* in, float* out, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            out[i] = processSample (in[i]);
    }

    bool isActive() const noexcept      { return active; }
    bool isFrozen() const noexcept      { return frozen; }
    int  getFftSize() const noexcept    { return N; }
    int  getHopSize() const noexcept    { return hop; }
    float getLayerGain() const noexcept { return layerGain; }

private:
    static constexpr float expTab[4] = { 0.0f, kPi * 0.5f, kPi, kPi * 1.5f };   // 2*pi*k*hop/N  (mod 2*pi) = k*pi/2

    //==========================================================================
    void analyse() noexcept
    {
        const int start = writePos;   // oldest sample in the ring
        for (int n = 0; n < N; ++n)
        {
            re[(size_t) n] = inRing[(size_t) ((start + n) & mask)] * win[(size_t) n];
            im[(size_t) n] = 0.0f;
        }

        fft.forward (re.data(), im.data());

        const bool havePrev = framesAnalysed > 0;

        for (int k = 0; k < nBins; ++k)
        {
            const float a = re[(size_t) k], b = im[(size_t) k];
            const float mag = std::sqrt (a * a + b * b);
            const float ph  = std::atan2 (b, a);

            liveMag[(size_t) k]   = mag;
            livePhase[(size_t) k] = ph;
            liveDev[(size_t) k]   = havePrev ? wrapPi (ph - prevPhase[(size_t) k] - expTab[k & 3]) : 0.0f;
            prevPhase[(size_t) k] = ph;
            avgMag[(size_t) k]   += 0.5f * (mag - avgMag[(size_t) k]);
        }

        if (framesAnalysed < 1000000000) ++framesAnalysed;
    }

    void capture() noexcept
    {
        std::copy (avgMag.begin(),  avgMag.end(),  heldMag.begin());
        std::copy (liveDev.begin(), liveDev.end(), heldDev.begin());

        const bool audible = layerGain >= 0.02f;
        preRollPending = ! audible;                 // instant, full-level start when nothing is ringing
        const float back = preRollPending ? (float) (kOverlap - 1) : 0.0f;

        for (int k = 1; k < nBins - 1; ++k)
        {
            const float dev = liveDev[(size_t) k];
            const float D   = expTab[k & 3] + dev;                          // per-hop phase advance
            const float phase0 = livePhase[(size_t) k] + (float) kOverlap * dev;   // continue from where the live frame ended
            phiCoh[(size_t) k] = wrapPi (phase0 - back * D);

            for (int v = 0; v < kNumVoices; ++v)
                phiRand[(size_t) v][(size_t) k] = kPi * rng.bi();
        }

        pitchSm = params.pitch;
        frozen = active = true;
        layerTarget = 1.0f;
        framesSinceCapture = 0;
        agcStartFrame = preRollPending ? 2 : 6;

        if (! audible)
        {
            agcCur = 1.3333f - 0.6667f * params.glass;   // sensible start before the measurement locks in
            agcInc = 0.0f;
        }
    }

    void updateHeld() noexcept
    {
        if (params.melt <= 0.004f)
            return;

        const float tau   = 0.12f * std::pow (250.0f, 1.0f - params.melt);     // 0.12 s .. 30 s
        const float alpha = 1.0f - std::exp (-(float) hop / ((float) sr * tau));

        for (int k = 1; k < nBins - 1; ++k)
        {
            heldMag[(size_t) k] += alpha * (avgMag[(size_t) k] - heldMag[(size_t) k]);
            heldDev[(size_t) k] += alpha * wrapPi (liveDev[(size_t) k] - heldDev[(size_t) k]);
        }
    }

    void prepareVoices() noexcept
    {
        const float ratio0 = std::exp2 (pitchSm * (1.0f / 12.0f));
        baseCoherent = std::fabs (pitchSm) < 0.02f;

        float eRef = 0.0f;
        for (int k = 1; k < nBins - 1; ++k)
            eRef += heldMag[(size_t) k] * heldMag[(size_t) k];
        eRef *= 2.0f;

        const float pRef = eRef / ((float) N * (float) N * meanSqWin);   // mean-square of the captured audio

        const float c = clampf (params.crystal, 0.0f, 1.0f);
        vGain[0] = 1.0f;  vGain[1] = c;  vGain[2] = 0.5f * c * c;
        targetPower = 0.0f;

        for (int v = 0; v < kNumVoices; ++v)
        {
            voiceOn[(size_t) v] = (v == 0) || vGain[(size_t) v] > 0.003f;
            if (! voiceOn[(size_t) v])
                continue;

            if (v == 0 && baseCoherent)
            {
                voiceMag[0] = heldMag.data();
                vScale[0]   = 1.0f;
                targetPower += pRef;
                continue;
            }

            const float inv = 1.0f / (ratio0 * (float) (v + 1));
            float* vm = vMag[(size_t) v].data();
            float e = 0.0f;

            for (int k = 0; k < nBins; ++k)
            {
                const float p = (float) k * inv;
                const int i0 = (int) p;

                if (i0 >= nBins - 1) { vm[k] = 0.0f; continue; }

                const float f = p - (float) i0;
                vm[k] = heldMag[(size_t) i0] + f * (heldMag[(size_t) i0 + 1] - heldMag[(size_t) i0]);

                if (k >= 1 && k < nBins - 1)
                    e += vm[k] * vm[k];
            }

            e *= 2.0f;
            voiceMag[(size_t) v] = vm;

            if (e > 1.0e-20f)
            {
                vScale[(size_t) v] = std::sqrt (eRef / e) * vGain[(size_t) v];
                targetPower += pRef * vGain[(size_t) v] * vGain[(size_t) v];
            }
            else
                vScale[(size_t) v] = 0.0f;
        }
    }

    /** Builds one windowed synthesis frame into 'frame' and advances all phase state by one hop. */
    void buildFrame() noexcept
    {
        std::fill (re.begin(), re.end(), 0.0f);
        std::fill (im.begin(), im.end(), 0.0f);

        for (int v = 0; v < kNumVoices; ++v)
        {
            if (! voiceOn[(size_t) v] || vScale[(size_t) v] <= 0.0f)
                continue;

            const float* vm = voiceMag[(size_t) v];
            const float sc  = vScale[(size_t) v];
            const float* ph = (v == 0 && baseCoherent) ? phiCoh.data() : phiRand[(size_t) v].data();

            for (int k = 1; k < nBins - 1; ++k)
            {
                const float m = vm[k] * sc;
                if (m > 1.0e-12f)
                {
                    re[(size_t) k] += m * std::cos (ph[k]);
                    im[(size_t) k] += m * std::sin (ph[k]);
                }
            }
        }

        // Hermitian symmetry -> real output. DC and Nyquist stay empty.
        for (int k = 1; k < N / 2; ++k)
        {
            re[(size_t) (N - k)] =  re[(size_t) k];
            im[(size_t) (N - k)] = -im[(size_t) k];
        }

        fft.inverse (re.data(), im.data());

        const float invN = 1.0f / (float) N;
        for (int n = 0; n < N; ++n)
            frame[(size_t) n] = re[(size_t) n] * invN * win[(size_t) n];

        // advance phases
        const float J0 = kPi * std::pow (1.0f - clampf (params.glass, 0.0f, 1.0f), 1.5f);

        for (int k = 1; k < nBins - 1; ++k)
        {
            float adv = expTab[k & 3] + heldDev[(size_t) k];
            if (baseCoherent && J0 > 1.0e-4f)
                adv += J0 * rng.bi();

            phiCoh[(size_t) k] = wrapPi (phiCoh[(size_t) k] + adv);

            for (int v = 0; v < kNumVoices; ++v)
                phiRand[(size_t) v][(size_t) k] = kPi * rng.bi();
        }
    }

    void addFrame (int startOffset) noexcept
    {
        for (int n = std::max (0, -startOffset); n < N; ++n)
            outRing[(size_t) ((readPos + startOffset + n) & mask)] += frame[(size_t) n];
    }

    float bandLevel (const float* mag, int b) const noexcept
    {
        float m = 0.0f;
        for (int k = bandLo[(size_t) b]; k <= bandHi[(size_t) b]; ++k)
            m = std::max (m, mag[k]);

        const float amp = m * 4.0f / (float) N;   // 0 dB = full-scale sine
        const float db  = 20.0f * std::log10 (amp + 1.0e-9f) + bandTilt[(size_t) b];
        return clampf ((db + 78.0f) / 60.0f, 0.0f, 1.0f);
    }

    void publishVisuals() noexcept
    {
        if (feed.amount != nullptr)
            feed.amount->store (layerGain, std::memory_order_relaxed);

        if (feed.live != nullptr)
            for (int b = 0; b < kNumBands; ++b)
                feed.live[b].store (bandLevel (avgMag.data(), b), std::memory_order_relaxed);

        if (feed.ice != nullptr)
            for (int b = 0; b < kNumBands; ++b)
                feed.ice[b].store (active ? bandLevel (heldMag.data(), b) * layerGain : 0.0f, std::memory_order_relaxed);
    }

    void processFrame() noexcept
    {
        analyse();

        if (params.freeze && ! frozen)
            capture();
        else if (! params.freeze && frozen)
        {
            frozen = false;
            layerTarget = 0.0f;
        }

        if (active)
        {
            ++framesSinceCapture;
            const float measured = hopPower / (float) hop;

            if (frozen)
                updateHeld();

            pitchSm += 0.35f * (params.pitch - pitchSm);
            prepareVoices();

            if (preRollPending)
            {
                preRollPending = false;
                for (int j = kOverlap - 1; j >= 0; --j)
                {
                    buildFrame();
                    addFrame (-j * hop);
                }
            }
            else
            {
                buildFrame();
                addFrame (0);
            }

            // feed-forward level normalisation (measured pre-gain output vs. captured level)
            float gTarget = agcCur;

            if (framesSinceCapture >= agcStartFrame)
            {
                if (framesSinceCapture == agcStartFrame)
                    agcPower = measured;
                else
                    agcPower += 0.10f * (measured - agcPower);

                if (agcPower > 1.0e-14f)
                    gTarget = clampf (std::sqrt (targetPower / agcPower), 0.05f, 12.0f);
            }

            agcInc = (gTarget - agcCur) / (float) hop;

            if (! frozen && layerGain < 1.0e-4f)
            {
                active = false;
                layerGain = 0.0f;
                std::fill (outRing.begin(), outRing.end(), 0.0f);
            }
        }

        hopPower = 0.0f;
        publishVisuals();
    }

    //==========================================================================
    Params params;
    VisualFeed feed;

    double sr = 44100.0;
    int N = 0, hop = 0, nBins = 0, mask = 0;
    float meanSqWin = 0.375f;
    FFT fft;

    std::vector<float> win, inRing, outRing, re, im, frame;
    std::vector<float> liveMag, livePhase, prevPhase, liveDev, avgMag, heldMag, heldDev, phiCoh;
    std::array<std::vector<float>, kNumVoices> vMag, phiRand;
    std::array<const float*, kNumVoices> voiceMag {};
    std::array<float, kNumVoices> vScale {}, vGain {};
    std::array<bool, kNumVoices> voiceOn {};

    std::array<int, kNumBands>   bandLo {}, bandHi {};
    std::array<float, kNumBands> bandTilt {};

    int writePos = 0, readPos = 0, hopCounter = 0;
    int framesAnalysed = 0, framesSinceCapture = 0, agcStartFrame = 2;

    bool frozen = false, active = false, preRollPending = false, baseCoherent = true;
    float layerGain = 0.0f, layerTarget = 0.0f, attackCoeff = 0.01f, releaseCoeff = 0.0001f;
    float agcCur = 1.0f, agcInc = 0.0f, agcPower = 0.0f, targetPower = 0.0f, hopPower = 0.0f;
    float pitchSm = 0.0f;

    Rng rng;
};

} // namespace fif
