#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
struct PresetDef
{
    const char* name;
    bool  freeze;
    float melt, glass, crystal, pitch, thaw;
    float heat; int flame; float flicker, flickerRate, tone;
    int   route; float mix, output;
};

// Presets demonstrate ice alone, fire alone, and both stages together.
const PresetDef kPresets[] =
{
    // name              freeze  melt  glass crystal pitch  thaw   heat flame flick  rate  tone  route  mix   out
    { "Frozen in Flames", true,  0.15f, 0.55f, 0.35f, 0.0f, 1.2f, 0.45f, 0, 0.35f, 2.5f,  1.0f, 0, 0.80f, 0.0f },
    { "Permafrost Pad",   true,  0.05f, 0.25f, 0.50f, 0.0f, 2.5f, 0.00f, 0, 0.10f, 1.2f,  0.0f, 0, 0.90f, 0.0f },
    { "Glass Cathedral",  true,  0.10f, 0.90f, 0.80f, 12.0f, 2.0f, 0.00f, 0, 0.10f, 1.0f, 0.0f, 0, 0.90f, 0.0f },
    { "Ember Drive",      false, 0.15f, 0.50f, 0.25f, 0.0f, 0.8f, 0.55f, 0, 0.25f, 1.8f,  2.0f, 1, 1.00f, 0.0f },
    { "Inferno Melt",     true,  0.60f, 0.30f, 0.20f, 0.0f, 0.6f, 0.80f, 2, 0.50f, 4.5f, -2.0f, 1, 0.85f, 0.0f },
    { "Cinder Thaw",      true,  0.30f, 0.45f, 0.15f, 0.0f, 2.0f, 0.65f, 3, 0.30f, 2.2f,  3.0f, 0, 0.85f, 0.0f },
};

float driveGainForHeat (float heat) noexcept
{
    return 1.0f + std::pow (juce::jlimit (0.0f, 1.0f, heat), 1.5f) * 24.0f;
}

float makeupFor (int flame, float driveGain) noexcept
{
    // Approximate loudness trim so raising Heat mostly changes character,
    // not level. Exponent differs for the sine folder, whose output energy
    // saturates sooner. The Output control remains the final trim.
    const float exponent = (flame == (int) fif::FlameType::Inferno) ? -0.30f : -0.38f;
    return std::pow (driveGain, exponent);
}

float softClip (float x) noexcept
{
    constexpr float t = 0.92f;
    const float a = std::fabs (x);
    if (a <= t)
        return x;
    const float s = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
    return std::copysign (s, x);
}

// Fixed-size channel copy between the pre-allocated scratch buffers.
// (AudioBuffer::makeCopyOf can resize; this never allocates.)
void copyBuf (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& src, int numSamples)
{
    for (int ch = 0; ch < 2; ++ch)
        dst.copyFrom (ch, 0, src, ch, 0, numSamples);
}
} // namespace

//==============================================================================
FrozenInFlamesAudioProcessor::FrozenInFlamesAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    pFreeze    = apvts.getRawParameterValue ("freeze");
    pMelt      = apvts.getRawParameterValue ("melt");
    pGlass     = apvts.getRawParameterValue ("glass");
    pCrystal   = apvts.getRawParameterValue ("crystal");
    pPitch     = apvts.getRawParameterValue ("pitch");
    pThaw      = apvts.getRawParameterValue ("thaw");
    pHeat      = apvts.getRawParameterValue ("heat");
    pFlame     = apvts.getRawParameterValue ("flame");
    pFlicker   = apvts.getRawParameterValue ("flicker");
    pFlickRate = apvts.getRawParameterValue ("flickerRate");
    pTone      = apvts.getRawParameterValue ("tone");
    pRoute     = apvts.getRawParameterValue ("route");
    pMix       = apvts.getRawParameterValue ("mix");
    pOutput    = apvts.getRawParameterValue ("output");
}

FrozenInFlamesAudioProcessor::~FrozenInFlamesAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout FrozenInFlamesAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterBool> (ParameterID { "freeze", 1 }, "Freeze", false));

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "melt", 1 }, "Melt",
        NormalisableRange<float> (0.0f, 1.0f), 0.15f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "glass", 1 }, "Glass",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "crystal", 1 }, "Crystal",
        NormalisableRange<float> (0.0f, 1.0f), 0.25f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pitch", 1 }, "Pitch",
        NormalisableRange<float> (-12.0f, 12.0f), 0.0f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "thaw", 1 }, "Thaw",
        NormalisableRange<float> (0.05f, 4.0f, 0.0f, 0.5f), 0.8f));

    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "heat", 1 }, "Heat",
        NormalisableRange<float> (0.0f, 1.0f), 0.35f));
    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "flame", 1 }, "Flame",
        StringArray { "Ember", "Blaze", "Inferno", "Cinder" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "flicker", 1 }, "Flicker",
        NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "flickerRate", 1 }, "Flicker Rate",
        NormalisableRange<float> (0.2f, 12.0f, 0.0f, 0.5f), 2.5f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (-12.0f, 12.0f), 0.0f));

    params.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "route", 1 }, "Route",
        StringArray { "Ice into Fire", "Fire into Ice", "Parallel" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.75f));
    params.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
int FrozenInFlamesAudioProcessor::getNumPresets() const
{
    return (int) std::size (kPresets);
}

juce::String FrozenInFlamesAudioProcessor::getPresetName (int presetIndex) const
{
    if (presetIndex < 0 || presetIndex >= getNumPresets())
        return {};
    return kPresets[presetIndex].name;
}

void FrozenInFlamesAudioProcessor::applyPreset (int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= getNumPresets())
        return;

    const auto& p = kPresets[presetIndex];
    auto setParam = [this] (const char* id, float valueInRange)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (valueInRange));
    };

    setParam ("freeze",     p.freeze ? 1.0f : 0.0f);
    setParam ("melt",       p.melt);
    setParam ("glass",      p.glass);
    setParam ("crystal",    p.crystal);
    setParam ("pitch",      p.pitch);
    setParam ("thaw",       p.thaw);
    setParam ("heat",       p.heat);
    setParam ("flame",      (float) p.flame);
    setParam ("flicker",    p.flicker);
    setParam ("flickerRate", p.flickerRate);
    setParam ("tone",       p.tone);
    setParam ("route",      (float) p.route);
    setParam ("mix",        p.mix);
    setParam ("output",     p.output);
}

//==============================================================================
void FrozenInFlamesAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (int ch = 0; ch < maxChannels; ++ch)
    {
        freezers[(size_t) ch].prepare (sampleRate, (uint32_t) (ch + 1));

        fif::SpectralFreezer::VisualFeed feed;
        feed.live   = bandFeeds[(size_t) ch].live.data();
        feed.ice    = bandFeeds[(size_t) ch].ice.data();
        feed.amount = &bandFeeds[(size_t) ch].amount;
        freezers[(size_t) ch].setVisualFeed (feed);

        fireDc[(size_t) ch].setCutoff (sampleRate, 12.0);
        fireDc[(size_t) ch].reset();
        toneLow[(size_t) ch].reset();
        toneHigh[(size_t) ch].reset();
        flickers[(size_t) ch].prepare (sampleRate, (uint32_t) (1234 + ch * 777));
    }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        maxChannels, osFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing (samplesPerBlock);

    // Reported latency = oversampler latency + one freeze hop (the spectral
    // engine's update quantum). The dry path is delayed by the same total.
    const int osLatency = (int) std::ceil (oversampler->getLatencyInSamples());
    reportedLatency = osLatency + freezers[0].getHopSize();
    setLatencySamples (reportedLatency);

    for (int ch = 0; ch < maxChannels; ++ch)
        dryDelays[(size_t) ch].prepare (reportedLatency);

    chunkCapacity = juce::jmax (1, samplesPerBlock);
    for (auto* b : { &dryBuf, &firedBuf, &iceBuf, &wetBuf, &fireStageBuf })
        b->setSize (maxChannels, chunkCapacity);
    flickerCh[0].assign ((size_t) chunkCapacity, 0.0f);
    flickerCh[1].assign ((size_t) chunkCapacity, 0.0f);
    engageRamp.assign ((size_t) chunkCapacity, 0.0f);
    driveRamp.assign ((size_t) chunkCapacity, 1.0f);

    mixSm.reset (sampleRate, 0.03);
    outGainSm.reset (sampleRate, 0.03);
    toneSm.reset (sampleRate, 0.10);
    driveSm.reset (sampleRate, 0.03);
    engageSm.reset (sampleRate, 0.05);

    mixSm.setCurrentAndTargetValue (pMix->load());
    outGainSm.setCurrentAndTargetValue (fif::dbToGain (pOutput->load()));
    toneSm.setCurrentAndTargetValue (pTone->load());
    driveSm.setCurrentAndTargetValue (driveGainForHeat (pHeat->load()));
    const float h = pHeat->load();
    engageSm.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, h / 0.08f));

    updateFireCoefficients (pTone->load());
}

void FrozenInFlamesAudioProcessor::releaseResources()
{
    oversampler.reset();
}

bool FrozenInFlamesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

double FrozenInFlamesAudioProcessor::getTailLengthSeconds() const
{
    return (double) pThaw->load() + 1.0;
}

//==============================================================================
void FrozenInFlamesAudioProcessor::updateFireCoefficients (float toneDb)
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        toneLow[(size_t) ch].setLowShelf  (currentSampleRate, 180.0,  (double) toneDb);
        toneHigh[(size_t) ch].setHighShelf (currentSampleRate, 4200.0, -(double) toneDb);
    }
}

void FrozenInFlamesAudioProcessor::processFire (juce::AudioBuffer<float>& buf, int numSamples)
{
    // Per-base-sample control ramps, computed once per chunk: flicker values
    // per channel, the Heat engage crossfade, and the smoothed drive. Inside
    // the oversampled loop these are read at base-rate resolution (index >> 2
    // at 4x), which is far below audibility for control signals and keeps the
    // expensive domain free of everything except the shaper itself.
    const float flickerAmount = pFlicker->load();
    const float heat = pHeat->load();
    driveSm.setTargetValue (driveGainForHeat (heat));
    const float eTarget = juce::jlimit (0.0f, 1.0f, heat / 0.08f);
    engageSm.setTargetValue (eTarget * eTarget * (3.0f - 2.0f * eTarget));

    for (int i = 0; i < numSamples; ++i)
    {
        flickerCh[0][(size_t) i] = flickers[0].next();
        flickerCh[1][(size_t) i] = flickers[1].next();
        engageRamp[(size_t) i]  = engageSm.getNextValue();
        driveRamp[(size_t) i]   = driveSm.getNextValue();
    }

    const auto flameType = (fif::FlameType) (int) pFlame->load();
    const float makeup = makeupFor ((int) flameType, driveSm.getCurrentValue());

    juce::dsp::AudioBlock<float> block (buf);
    auto sub = block.getSubBlock (0, (size_t) numSamples);
    auto up = oversampler->processSamplesUp (sub);

    const int osN = (int) up.getNumSamples();
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto* d = up.getChannelPointer ((size_t) ch);
        const auto& flick = flickerCh[(size_t) ch];
        for (int i = 0; i < osN; ++i)
        {
            const int base = i >> osFactorLog2;
            const float drive  = driveRamp[(size_t) base];
            const float bias   = flick[(size_t) base] * flickerAmount * 0.4f;
            const float engage = engageRamp[(size_t) base];
            const float x = d[i];
            const float shaped = fif::shape (flameType, x * drive, bias) * makeup;
            d[i] = x + engage * (shaped - x);
        }
    }

    oversampler->processSamplesDown (sub);

    // The Cinder model rectifies and therefore generates DC (measured mean
    // +0.446 on a full-scale sine before this fix). Block it on every model,
    // at base rate, after downsampling.
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = fireDc[(size_t) ch].process (d[i]);
    }
}

void FrozenInFlamesAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), maxChannels);

    // Pristine copy of this chunk (internal stereo even for mono input).
    for (int ch = 0; ch < numCh; ++ch)
        dryBuf.copyFrom (ch, 0, buffer, ch, startSample, numSamples);
    if (numCh == 1)
        dryBuf.copyFrom (1, 0, buffer, 0, startSample, numSamples);

    const int route = (int) pRoute->load();

    if (route == 1) // Fire into Ice: saturate first, the freezer analyses the fired signal
    {
        copyBuf (fireStageBuf, dryBuf, numSamples);
        processFire (fireStageBuf, numSamples);
        for (int ch = 0; ch < maxChannels; ++ch)
            freezers[(size_t) ch].process (fireStageBuf.getReadPointer (ch), iceBuf.getWritePointer (ch), numSamples);
        copyBuf (wetBuf, iceBuf, numSamples);
        // Wet also carries the fired live signal, so fire alone still sounds with Freeze off.
        for (int ch = 0; ch < maxChannels; ++ch)
            wetBuf.addFrom (ch, 0, fireStageBuf, ch, 0, numSamples);
    }
    else // Ice into Fire (0) and Parallel (2): the freezer analyses the clean input
    {
        for (int ch = 0; ch < maxChannels; ++ch)
            freezers[(size_t) ch].process (dryBuf.getReadPointer (ch), iceBuf.getWritePointer (ch), numSamples);

        if (route == 0)
        {
            // The live signal and its frozen layer flow into the fire together.
            copyBuf (fireStageBuf, dryBuf, numSamples);
            for (int ch = 0; ch < maxChannels; ++ch)
                fireStageBuf.addFrom (ch, 0, iceBuf, ch, 0, numSamples);
            processFire (fireStageBuf, numSamples);
            copyBuf (wetBuf, fireStageBuf, numSamples);
        }
        else
        {
            copyBuf (fireStageBuf, iceBuf, numSamples);
            processFire (fireStageBuf, numSamples);
            copyBuf (wetBuf, dryBuf, numSamples);
            for (int ch = 0; ch < maxChannels; ++ch)
            {
                wetBuf.addFrom (ch, 0, iceBuf, ch, 0, numSamples, 0.5f);
                wetBuf.addFrom (ch, 0, fireStageBuf, ch, 0, numSamples, 0.5f);
            }
        }
    }

    // Tone shelves on the wet path, coefficients refreshed in 64-sample
    // chunks from the smoothed Tone value.
    mixSm.setTargetValue (pMix->load());
    outGainSm.setTargetValue (fif::dbToGain (pOutput->load()));
    toneSm.setTargetValue (pTone->load());

    float* dryPtrs[2] = { dryBuf.getWritePointer (0), dryBuf.getWritePointer (1) };
    float* wetPtrs[2] = { wetBuf.getWritePointer (0), wetBuf.getWritePointer (1) };
    float* outPtrs[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    int toneCountdown = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        if (toneCountdown-- <= 0)
        {
            const float step = (float) juce::jmin (64, numSamples - i);
            updateFireCoefficients (toneSm.skip ((int) step));
            toneCountdown = 63;
        }

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            wetPtrs[ch][i] = toneHigh[(size_t) ch].process (toneLow[(size_t) ch].process (wetPtrs[ch][i]));
            dryPtrs[ch][i] = dryDelays[(size_t) ch].process (dryPtrs[ch][i]);
        }

        const float m = mixSm.getNextValue();
        const float g = outGainSm.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
            outPtrs[ch][startSample + i] = softClip ((dryPtrs[ch][i] * (1.0f - m) + wetPtrs[ch][i] * m) * g);
    }
}

void FrozenInFlamesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (oversampler == nullptr || chunkCapacity <= 0)
        return;

    // Freezer parameters are read once per block and pushed to both engines.
    fif::SpectralFreezer::Params fp;
    fp.freeze  = pFreeze->load() > 0.5f;
    fp.melt    = pMelt->load();
    fp.glass   = pGlass->load();
    fp.crystal = pCrystal->load();
    fp.pitch   = pPitch->load();
    fp.thaw    = pThaw->load();
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        freezers[(size_t) ch].setParams (fp);
        flickers[(size_t) ch].setRate (pFlickRate->load());
    }

    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += chunkCapacity)
        processChunk (buffer, start, juce::jmin (chunkCapacity, total - start));
}

//==============================================================================
void FrozenInFlamesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void FrozenInFlamesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* FrozenInFlamesAudioProcessor::createEditor()
{
    return new FrozenInFlamesAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrozenInFlamesAudioProcessor();
}
