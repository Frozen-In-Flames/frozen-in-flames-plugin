#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::Colour kBackground  { 0xff0a0d13 };
const juce::Colour kPanel       { 0x14ffffff };
const juce::Colour kPanelBorder { 0x30ffffff };
const juce::Colour kIce         { 0xff6fd7ff };
const juce::Colour kIceDim      { 0xff2a7ba6 };
const juce::Colour kFire        { 0xffff8b3d };
const juce::Colour kFireDeep    { 0xffd84e1e };
const juce::Colour kText        { 0xffe8edf4 };
const juce::Colour kTextDim     { 0xff93a1b3 };

juce::Font makeFont (float height, bool bold = false)
{
    auto options = juce::FontOptions().withHeight (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}
} // namespace

//==============================================================================
void FiFLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    auto centre = bounds.getCentre();
    auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    // body
    g.setColour (juce::Colour (0xff161c26));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (kPanelBorder);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    // value arc
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius + 1.5f, radius + 1.5f, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    const float pl = radius - 4.0f;
    pointer.addLineSegment (juce::Line<float> (0.0f, -radius + 3.0f, 0.0f, -radius + 3.0f + pl * 0.32f), 2.5f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
void Visualiser::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1420), bounds.getTopLeft(),
                                             juce::Colour (0xff130c08), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (kPanelBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    constexpr int bands = fif::SpectralFreezer::kNumBands;
    float liveAvg[bands] = {}, iceAvg[bands] = {};
    float amount = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& feed = processor.bandFeeds[(size_t) ch];
        amount = juce::jmax (amount, feed.amount.load (std::memory_order_relaxed));
        for (int b = 0; b < bands; ++b)
        {
            liveAvg[b] += 0.5f * feed.live[(size_t) b].load (std::memory_order_relaxed);
            iceAvg[b]  += 0.5f * feed.ice[(size_t) b].load (std::memory_order_relaxed);
        }
    }

    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float barW = w / (float) bands;

    for (int b = 0; b < bands; ++b)
    {
        const float x = bounds.getX() + barW * (float) b + 1.0f;
        const float bw = juce::jmax (1.0f, barW - 2.0f);

        smoothLive[(size_t) b] = juce::jmax (liveAvg[b], smoothLive[(size_t) b] * 0.82f);
        smoothIce[(size_t) b]  = juce::jmax (iceAvg[b],  smoothIce[(size_t) b] * 0.90f);

        // flames: live spectrum rising from the bottom
        const float flameH = smoothLive[(size_t) b] * h * 0.62f;
        if (flameH > 1.0f)
        {
            g.setGradientFill (juce::ColourGradient (kFireDeep, x, bounds.getBottom(),
                                                     kFire, x, bounds.getBottom() - flameH, false));
            g.fillRoundedRectangle (x, bounds.getBottom() - flameH, bw, flameH, 2.0f);
        }

        // ice: frozen spectrum hanging from the top
        const float iceH = smoothIce[(size_t) b] * h * 0.62f;
        if (iceH > 1.0f)
        {
            g.setGradientFill (juce::ColourGradient (kIce, x, bounds.getY(),
                                                     kIceDim, x, bounds.getY() + iceH, false));
            g.fillRoundedRectangle (x, bounds.getY(), bw, iceH, 2.0f);
        }
    }

    if (amount > 0.02f)
    {
        g.setColour (kIce.withAlpha (0.9f));
        g.setFont (makeFont (11.0f, true));
        g.drawText ("FROZEN", bounds.reduced (10.0f, 6.0f), juce::Justification::topRight, false);
    }
}

//==============================================================================
void FrozenInFlamesAudioProcessorEditor::Knob::init (FrozenInFlamesAudioProcessor& p, const char* id,
                                                     const char* name, juce::Colour accent, juce::Component& parent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxTextColourId, kText);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    parent.addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setFont (makeFont (11.0f, true));
    label.setColour (juce::Label::textColourId, kTextDim);
    label.setJustificationType (juce::Justification::centred);
    parent.addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
}

void FrozenInFlamesAudioProcessorEditor::Knob::place (juce::Rectangle<int> cell)
{
    label.setBounds (cell.removeFromTop (16));
    slider.setBounds (cell);
}

//==============================================================================
FrozenInFlamesAudioProcessorEditor::FrozenInFlamesAudioProcessorEditor (FrozenInFlamesAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), visualiser (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (visualiser);

    meltKnob.init     (p, "melt",       "MELT",    kIce,  *this);
    glassKnob.init    (p, "glass",      "GLASS",   kIce,  *this);
    crystalKnob.init  (p, "crystal",    "CRYSTAL", kIce,  *this);
    pitchKnob.init    (p, "pitch",      "PITCH",   kIce,  *this);
    thawKnob.init     (p, "thaw",       "THAW",    kIce,  *this);
    heatKnob.init     (p, "heat",       "HEAT",    kFire, *this);
    flickerKnob.init  (p, "flicker",    "FLICKER", kFire, *this);
    flickerRateKnob.init (p, "flickerRate", "FLICK RATE", kFire, *this);
    toneKnob.init     (p, "tone",       "TONE",    kFire, *this);
    mixKnob.init      (p, "mix",        "MIX",     kText, *this);
    outputKnob.init   (p, "output",     "OUTPUT",  kText, *this);

    freezeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff12222f));
    freezeButton.setColour (juce::TextButton::buttonOnColourId, kIceDim);
    freezeButton.setColour (juce::TextButton::textColourOffId, kIce);
    freezeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "freeze", freezeButton);

    auto setupLabel = [this] (juce::Label& l, const char* text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (makeFont (11.0f, true));
        l.setColour (juce::Label::textColourId, kTextDim);
        addAndMakeVisible (l);
    };
    auto setupBox = [this] (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff161c26));
        box.setColour (juce::ComboBox::textColourId, kText);
        box.setColour (juce::ComboBox::outlineColourId, kPanelBorder);
        box.setColour (juce::ComboBox::arrowColourId, kTextDim);
        addAndMakeVisible (box);
    };

    setupLabel (flameLabel, "FLAME");
    setupBox (flameBox);
    flameBox.addItemList ({ "Ember", "Blaze", "Inferno", "Cinder" }, 1);
    flameAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "flame", flameBox);

    setupLabel (routeLabel, "ROUTE");
    setupBox (routeBox);
    routeBox.addItemList ({ "Ice into Fire", "Fire into Ice", "Parallel" }, 1);
    routeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "route", routeBox);

    setupLabel (presetLabel, "PRESET");
    setupBox (presetBox);
    for (int i = 0; i < p.getNumPresets(); ++i)
        presetBox.addItem (p.getPresetName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Choose a preset");
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) processor.applyPreset (presetBox.getSelectedId() - 1); };

    setSize (920, 620);
    startTimerHz (8);
    refreshFreezeButton();
}

FrozenInFlamesAudioProcessorEditor::~FrozenInFlamesAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FrozenInFlamesAudioProcessorEditor::timerCallback() { refreshFreezeButton(); }

void FrozenInFlamesAudioProcessorEditor::refreshFreezeButton()
{
    const bool on = processor.apvts.getRawParameterValue ("freeze")->load() > 0.5f;
    freezeButton.setButtonText (on ? "FROZEN" : "FREEZE");
}

//==============================================================================
void FrozenInFlamesAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    // Title: ice half, then fire half.
    auto title = getLocalBounds().removeFromTop (52).reduced (16, 8);
    auto half = title.getWidth() / 2;
    g.setFont (makeFont (24.0f, true));
    g.setColour (kIce);
    g.drawText ("FROZEN IN ", title.removeFromLeft (half), juce::Justification::centredRight, false);
    g.setColour (kFire);
    g.drawText ("FLAMES", title, juce::Justification::centredLeft, false);

    for (auto* panel : { &icePanel, &centrePanel, &firePanel })
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (panel->toFloat(), 10.0f);
        g.setColour (kPanelBorder);
        g.drawRoundedRectangle (panel->toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setFont (makeFont (13.0f, true));
    g.setColour (kIce);
    g.drawText ("ICE", icePanel.getX(), icePanel.getY() + 8, icePanel.getWidth(), 18, juce::Justification::centred, false);
    g.setColour (kTextDim);
    g.drawText ("SESSION", centrePanel.getX(), centrePanel.getY() + 8, centrePanel.getWidth(), 18, juce::Justification::centred, false);
    g.setColour (kFire);
    g.drawText ("FIRE", firePanel.getX(), firePanel.getY() + 8, firePanel.getWidth(), 18, juce::Justification::centred, false);
}

void FrozenInFlamesAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (52);                       // title
    visualiser.setBounds (area.removeFromTop (128));
    area.removeFromTop (12);

    const int colW = (area.getWidth() - 24) / 3;
    icePanel    = juce::Rectangle<int> (area.getX(),                area.getY(), colW, area.getHeight());
    centrePanel = juce::Rectangle<int> (area.getX() + colW + 12,     area.getY(), colW, area.getHeight());
    firePanel   = juce::Rectangle<int> (area.getX() + 2 * colW + 24, area.getY(), colW, area.getHeight());

    auto layoutPanel = [] (juce::Rectangle<int> panel, int headerH = 34)
    {
        return panel.reduced (12).withTrimmedTop (headerH);
    };

    // ICE column: freeze button, then knobs in two columns.
    {
        auto col = layoutPanel (icePanel);
        freezeButton.setBounds (col.removeFromTop (44));
        col.removeFromTop (10);
        const int cellW = col.getWidth() / 2;
        auto row = [&col, cellW] (int h) {
            auto r = col.removeFromTop (h);
            return std::array<juce::Rectangle<int>, 2> { r.removeFromLeft (cellW), r };
        };
        auto r1 = row (118);
        meltKnob.place (r1[0]);  glassKnob.place (r1[1]);
        auto r2 = row (118);
        crystalKnob.place (r2[0]); pitchKnob.place (r2[1]);
        auto r3 = row (118);
        thawKnob.place (r3[0]);
    }

    // Centre column: route, preset, mix, output.
    {
        auto col = layoutPanel (centrePanel);
        routeLabel.setBounds (col.removeFromTop (16));
        routeBox.setBounds (col.removeFromTop (28));
        col.removeFromTop (12);
        presetLabel.setBounds (col.removeFromTop (16));
        presetBox.setBounds (col.removeFromTop (28));
        col.removeFromTop (14);
        const int cellW = col.getWidth() / 2;
        auto r = col.removeFromTop (118);
        mixKnob.place (r.removeFromLeft (cellW));
        outputKnob.place (r);
    }

    // FIRE column: knobs, then the flame model selector.
    {
        auto col = layoutPanel (firePanel);
        const int cellW = col.getWidth() / 2;
        auto r1 = col.removeFromTop (118);
        heatKnob.place (r1.removeFromLeft (cellW)); flickerKnob.place (r1);
        auto r2 = col.removeFromTop (118);
        flickerRateKnob.place (r2.removeFromLeft (cellW)); toneKnob.place (r2);
        col.removeFromTop (10);
        flameLabel.setBounds (col.removeFromTop (16));
        flameBox.setBounds (col.removeFromTop (28));
    }
}
