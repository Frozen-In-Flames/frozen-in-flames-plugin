#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class FiFLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
/** Band visualiser: live spectrum as flames rising from the bottom, the
    frozen layer as ice hanging from the top. Fed lock-free from the
    processor's BandFeed arrays. */
class Visualiser final : public juce::Component, private juce::Timer
{
public:
    explicit Visualiser (FrozenInFlamesAudioProcessor& p) : processor (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    FrozenInFlamesAudioProcessor& processor;
    std::array<float, fif::SpectralFreezer::kNumBands> smoothLive {}, smoothIce {};
};

//==============================================================================
class FrozenInFlamesAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer
{
public:
    explicit FrozenInFlamesAudioProcessorEditor (FrozenInFlamesAudioProcessor&);
    ~FrozenInFlamesAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshFreezeButton();

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        void init (FrozenInFlamesAudioProcessor&, const char* id, const char* name, juce::Colour accent, juce::Component& parent);
        void place (juce::Rectangle<int> cell);
    };

    FrozenInFlamesAudioProcessor& processor;
    FiFLookAndFeel lookAndFeel;
    Visualiser visualiser;

    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    Knob meltKnob, glassKnob, crystalKnob, pitchKnob, thawKnob;
    Knob heatKnob, flickerKnob, flickerRateKnob, toneKnob;
    Knob mixKnob, outputKnob;

    juce::Label flameLabel, routeLabel, presetLabel;
    juce::ComboBox flameBox, routeBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> flameAttachment, routeAttachment;

    juce::Rectangle<int> icePanel, centrePanel, firePanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrozenInFlamesAudioProcessorEditor)
};
