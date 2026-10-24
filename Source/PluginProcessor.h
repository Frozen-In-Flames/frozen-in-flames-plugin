#pragma once

#include <JuceHeader.h>
#include "FiDsp.h"
#include "SpectralFreezer.h"

#include <array>
#include <atomic>

//==============================================================================
/** Frozen in Flames: a spectral freeze layer (ice) and a multi-model flame
    saturator (fire) in one stereo effect. All DSP lives in the JUCE-free
    headers FiDsp.h and SpectralFreezer.h; this class is the host-facing
    shell: parameters, routing, oversampling, latency and state. */
class FrozenInFlamesAudioProcessor final : public juce::AudioProcessor
{
public:
    FrozenInFlamesAudioProcessor();
    ~FrozenInFlamesAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void applyPreset (int presetIndex);
    int getNumPresets() const;
    juce::String getPresetName (int presetIndex) const;

    juce::AudioProcessorValueTreeState apvts;

    /** Lock-free band data for the editor visualiser (48 log bands per feed). */
    struct BandFeed
    {
        std::array<std::atomic<float>, fif::SpectralFreezer::kNumBands> live {};
        std::array<std::atomic<float>, fif::SpectralFreezer::kNumBands> ice {};
        std::atomic<float> amount { 0.0f };
    };
    std::array<BandFeed, 2> bandFeeds;

    int getReportedLatency() const noexcept { return reportedLatency; }

private:
    //==========================================================================
    static constexpr int maxChannels = 2;
    static constexpr int osFactorLog2 = 2;                 // 4x oversampling

    void processChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processFire (juce::AudioBuffer<float>& buf, int numSamples);
    void updateFireCoefficients (float toneDb);

    std::array<fif::SpectralFreezer, maxChannels> freezers;
    std::array<fif::FixedDelay, maxChannels> dryDelays;
    std::array<fif::Biquad, maxChannels> toneLow, toneHigh;
    std::array<fif::DcBlocker, maxChannels> fireDc;
    std::array<fif::FlameFlicker, maxChannels> flickers;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::AudioBuffer<float> dryBuf, firedBuf, iceBuf, wetBuf, fireStageBuf;
    std::vector<float> flickerCh[2], engageRamp, driveRamp;
    int chunkCapacity = 0;
    int reportedLatency = 0;
    double currentSampleRate = 48000.0;

    juce::SmoothedValue<float> mixSm, outGainSm, toneSm;
    juce::SmoothedValue<float> driveSm;                    // advances on the oversampled domain
    juce::SmoothedValue<float> engageSm;

    std::atomic<float>* pFreeze   = nullptr;
    std::atomic<float>* pMelt     = nullptr;
    std::atomic<float>* pGlass    = nullptr;
    std::atomic<float>* pCrystal  = nullptr;
    std::atomic<float>* pPitch    = nullptr;
    std::atomic<float>* pThaw     = nullptr;
    std::atomic<float>* pHeat     = nullptr;
    std::atomic<float>* pFlame    = nullptr;
    std::atomic<float>* pFlicker  = nullptr;
    std::atomic<float>* pFlickRate= nullptr;
    std::atomic<float>* pTone     = nullptr;
    std::atomic<float>* pRoute    = nullptr;
    std::atomic<float>* pMix      = nullptr;
    std::atomic<float>* pOutput   = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrozenInFlamesAudioProcessor)
};
