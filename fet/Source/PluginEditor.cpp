#include "PluginEditor.h"
#include "FETEngine.h"

using namespace FET;
using namespace FET::UI;

namespace
{
    juce::String dbText (float v) { return (v > 0.005f ? "+" : "") + juce::String (v, 1) + " dB"; }

    juce::String timeText (double seconds)
    {
        if (seconds < 1.0e-3) return juce::String (juce::roundToInt (seconds * 1.0e6)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb5s"));
        if (seconds < 1.0)    return juce::String (juce::roundToInt (seconds * 1.0e3)) + " ms";
        return juce::String (seconds, 2) + " s";
    }

    std::vector<std::pair<float, juce::String>> knobMarks()
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int i = 1; i <= 7; ++i)
            m.push_back ({ (float) i, juce::String (i) });
        return m;
    }

    std::vector<std::pair<float, juce::String>> dbMarks (std::initializer_list<int> values)
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int v : values)
            m.push_back ({ (float) v, v > 0 ? "+" + juce::String (v) : juce::String (v) });
        return m;
    }

    // Layout, in design units (FET::designWidth x FET::designHeight).
    const juce::Rectangle<int> inputArea   { 24, 64, 170, 270 };
    const juce::Rectangle<int> outputArea  { 200, 64, 170, 270 };
    const juce::Rectangle<int> attackArea  { 382, 56, 112, 138 };
    const juce::Rectangle<int> releaseArea { 382, 200, 112, 138 };
    const juce::Rectangle<int> ratioArea   { 512, 72, 56, 236 };
    const juce::Rectangle<int> meterArea   { 584, 72, 236, 168 };
    const juce::Rectangle<int> meterModes  { 832, 72, 48, 132 };
}

//==============================================================================
FETKnob::FETKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& n,
                  std::vector<std::pair<float, juce::String>> scaleMarks, Formatter format)
    : name (n), marks (std::move (scaleMarks)), formatter (std::move (format)),
      attachment (state, paramId, slider)
{
    auto* p = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                                juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
    slider.setMouseDragSensitivity (220);
    slider.setTitle (name);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void FETKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    r.removeFromBottom (24);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    const int size = side - juce::roundToInt ((float) side * 0.32f);   // room for the printed scale
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (r.getCentre()));
}

void FETKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::text);
    g.setFont (font (14.0f, Weight::SemiBold));
    g.drawText (name, r.removeFromTop (20.0f), juce::Justification::centred);

    g.setFont (numbers (13.0f));
    g.drawText (getValueText(), r.removeFromBottom (20.0f), juce::Justification::centred);

    // Printed scale around the knob.
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float radius = sb.getWidth() * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto range = slider.getNormalisableRange();
    g.setFont (numbers (11.0f, Weight::Medium));
    for (const auto& [value, label] : marks)
    {
        const float pos = (float) range.convertTo0to1 (value);
        const float angle = params.startAngleRadians + pos * (params.endAngleRadians - params.startAngleRadians);
        g.setColour (Colours::textDim);
        g.drawLine ({ c.getPointOnCircumference (radius + 2.0f, angle), c.getPointOnCircumference (radius + 6.0f, angle) }, 1.2f);
        const auto t = c.getPointOnCircumference (radius + 18.0f, angle);
        g.drawText (label, juce::Rectangle<float> (24.0f, 14.0f).withCentre (t), juce::Justification::centred);
    }
}

//==============================================================================
SongsmithAnalogCompressorEditor::SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      input (p.apvts, ID::input, "Input", dbMarks ({ -24, 0, 24, 48 }), [] (float v) { return dbText (v); }),
      output (p.apvts, ID::output, "Output", dbMarks ({ -36, -12, 0, 12, 24 }), [] (float v) { return dbText (v); }),
      attack (p.apvts, ID::attack, "Attack", knobMarks(),
              [] (float v) { return juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + timeText (Engine::attackSeconds (v)); }),
      release (p.apvts, ID::release, "Release", knobMarks(),
               [] (float v) { return juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + timeText (Engine::releaseSeconds (v)); }),
      ratioAttachment (*p.apvts.getParameter (ID::ratio), [this] (float v) { ratioIndex = juce::roundToInt (v); refreshButtons(); }),
      allButtonsAttachment (*p.apvts.getParameter (ID::allButtons), [this] (float v) { allButtonsOn = v >= 0.5f; refreshButtons(); }),
      grOffAttachment (p.apvts, ID::grOff, grOffButton),
      bypassAttachment (p.apvts, ID::bypass, bypassButton),
      oversamplingAttachment (p.apvts, ID::oversampling, oversampling)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* k : { &input, &output, &attack, &release })
        content.addAndMakeVisible (*k);

    // Ratio buttons: one press selects that ratio (and leaves All Buttons); "All" presses all
    // four at once (UM p. 9). Each press is one automation gesture.
    const juce::StringArray ratioText { "4", "8", "12", "20" };
    for (int i = 0; i < 4; ++i)
    {
        auto b = std::make_unique<juce::TextButton> (ratioText[i]);
        b->setTooltip (ratioNames()[i]);
        b->onClick = [this, i]
        {
            if (allButtonsOn)
                allButtonsAttachment.setValueAsCompleteGesture (0.0f);
            ratioAttachment.setValueAsCompleteGesture ((float) i);
        };
        content.addAndMakeVisible (*b);
        ratioButtons[(size_t) i] = std::move (b);
    }
    allButton.setTooltip ("All Buttons: all four ratio buttons pressed in");
    allButton.onClick = [this] { allButtonsAttachment.setValueAsCompleteGesture (allButtonsOn ? 0.0f : 1.0f); };
    content.addAndMakeVisible (allButton);

    for (auto* b : { &grOffButton, &bypassButton })
    {
        b->setClickingTogglesState (true);
        content.addAndMakeVisible (*b);
    }
    grOffButton.setTooltip ("Gain reduction off: the signal still runs through the amplifiers (Attack \"OFF\")");
    bypassButton.getProperties().set ("bypassColour", true);

    for (int m = 0; m < 3; ++m)
    {
        meterButtons[(size_t) m].onClick = [this, m] { setMeterMode (m); };
        content.addAndMakeVisible (meterButtons[(size_t) m]);
    }
    content.addAndMakeVisible (meter);

    oversampling.addItemList (oversamplingNames(), 1);
    oversampling.setTooltip ("Oversampling for the analogue stages and the detector");
    content.addAndMakeVisible (oversampling);
    oversampling.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (ID::oversampling)->load(), juce::dontSendNotification);

    for (int i = 0; i < (int) std::size (uiScales); ++i)
        sizeBox.addItem ("Size " + juce::String (juce::roundToInt (uiScales[i] * 100.0f)) + " %", i + 1);
    sizeBox.onChange = [this] { setUiScale (uiScales[(size_t) juce::jmax (0, sizeBox.getSelectedId() - 1)]); };
    content.addAndMakeVisible (sizeBox);

    ratioAttachment.sendInitialUpdate();
    allButtonsAttachment.sendInitialUpdate();
    setMeterMode (proc.meterMode.load());
    meter.ballistics.reset (needleFraction (0.0f));

    layoutContent();
    setUiScale (proc.uiScale.load());
    startTimerHz (30);
}

SongsmithAnalogCompressorEditor::~SongsmithAnalogCompressorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void SongsmithAnalogCompressorEditor::setUiScale (float scale)
{
    uiScale = snapUiScale (scale);
    proc.uiScale.store (uiScale);
    for (int i = 0; i < (int) std::size (uiScales); ++i)
        if (uiScales[i] == uiScale)
            sizeBox.setSelectedId (i + 1, juce::dontSendNotification);
    setSize (juce::roundToInt ((float) designWidth * uiScale), juce::roundToInt ((float) designHeight * uiScale));
}

void SongsmithAnalogCompressorEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
}

void SongsmithAnalogCompressorEditor::layoutContent()
{
    input.setBounds (inputArea);
    output.setBounds (outputArea);
    attack.setBounds (attackArea);
    release.setBounds (releaseArea);

    // Hardware order, top to bottom: 20, 12, 8, 4 (then All).
    auto col = ratioArea;
    const int h = 40, gap = 7;
    for (int i = 3; i >= 0; --i)
    {
        ratioButtons[(size_t) i]->setBounds (col.removeFromTop (h));
        col.removeFromTop (gap);
    }
    allButton.setBounds (col.removeFromTop (h));

    meter.setBounds (meterArea);
    auto modes = meterModes;
    for (auto& b : meterButtons)
    {
        b.setBounds (modes.removeFromTop (40));
        modes.removeFromTop (6);
    }

    grOffButton.setBounds (584, 300, 112, 36);
    bypassButton.setBounds (708, 300, 112, 36);

    oversampling.setBounds (600, 14, 130, 26);
    sizeBox.setBounds (744, 14, 136, 26);
}

void SongsmithAnalogCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithAnalogCompressorEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    juce::ColourGradient bg (Colours::panelLight, 0.0f, 0.0f, Colours::panel, 0.0f, r.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (r);

    // Title bar.
    g.setColour (Colours::recess.withAlpha (0.6f));
    g.fillRect (r.withHeight (52.0f));
    g.setColour (Colours::text);
    g.setFont (font (18.0f, Weight::SemiBold));
    g.drawText ("Songsmith", juce::Rectangle<float> (24.0f, 10.0f, 110.0f, 32.0f), juce::Justification::centredLeft);
    g.setColour (Colours::accent);
    g.setFont (font (18.0f, Weight::Regular));
    g.drawText ("Analog Compressor", juce::Rectangle<float> (128.0f, 10.0f, 260.0f, 32.0f), juce::Justification::centredLeft);

    // Section rules and captions.
    g.setColour (Colours::panelLight.brighter (0.1f));
    for (float x : { 376.0f, 502.0f })
        g.drawLine (x, 70.0f, x, 340.0f, 1.0f);

    g.setColour (Colours::textDim);
    g.setFont (font (12.0f, Weight::Medium));
    g.drawText ("Ratio", juce::Rectangle<float> ((float) ratioArea.getX(), 52.0f, (float) ratioArea.getWidth(), 18.0f), juce::Justification::centred);
    g.drawText ("Meter", juce::Rectangle<float> ((float) meterModes.getX(), 52.0f, (float) meterModes.getWidth(), 18.0f), juce::Justification::centred);

    // Numeric GR under the needle (the needle has VU ballistics; this shows the value).
    g.setColour (Colours::text);
    g.setFont (numbers (14.0f, Weight::Medium));
    g.drawText ("GR " + juce::String (-grDisplayDb, 1) + " dB",
                juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getBottom() + 10.0f, (float) meterArea.getWidth(), 20.0f),
                juce::Justification::centred);

    if (allButtonsOn)
    {
        g.setColour (Colours::accent);
        g.setFont (font (12.0f, Weight::SemiBold));
        g.drawText ("All Buttons", juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getBottom() + 30.0f, (float) meterArea.getWidth(), 16.0f),
                    juce::Justification::centred);
    }

    g.setColour (Colours::textDim);
    g.setFont (font (12.0f));
    g.drawText ("Double-click a knob for its default", juce::Rectangle<float> (24.0f, 368.0f, 360.0f, 18.0f), juce::Justification::centredLeft);
}

void SongsmithAnalogCompressorEditor::refreshButtons()
{
    for (int i = 0; i < 4; ++i)
        ratioButtons[(size_t) i]->setToggleState (allButtonsOn || i == ratioIndex, juce::dontSendNotification);
    allButton.setToggleState (allButtonsOn, juce::dontSendNotification);
    content.repaint();
}

void SongsmithAnalogCompressorEditor::setMeterMode (int mode)
{
    mode = juce::jlimit (0, 2, mode);
    proc.meterMode.store (mode);
    for (int m = 0; m < 3; ++m)
        meterButtons[(size_t) m].setToggleState (m == mode, juce::dontSendNotification);
    meter.setModeLabel (mode == 0 ? "GR" : mode == 1 ? "+4" : "+8");
}

void SongsmithAnalogCompressorEditor::advanceMeter (float seconds)
{
    const float gr = proc.takeGainReductionDb();
    const float outDb = proc.takeOutputRmsDb();
    const auto mode = (MeterMode) proc.meterMode.load();
    meter.setReading (meterReadingVu (mode, gr, outDb, allButtonsOn), seconds);

    // The numeric readout: instant rise, ~300 ms fall.
    grDisplayDb = gr > grDisplayDb ? gr : grDisplayDb * 0.9f + gr * 0.1f;
    content.repaint (meterArea.getX(), meterArea.getBottom(), meterArea.getWidth(), 50);
}

FETKnob& SongsmithAnalogCompressorEditor::getKnob (const juce::String& id)
{
    if (id == ID::input)  return input;
    if (id == ID::output) return output;
    if (id == ID::attack) return attack;
    return release;
}
