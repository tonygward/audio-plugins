#include "BandPanel.h"
#include "EQLookAndFeel.h"

namespace
{
    juce::String formatFreq (double hz)
    {
        if (hz >= 1000.0)
            return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";
        return juce::String (hz, hz >= 100.0 ? 0 : 1) + " Hz";
    }

    double parseFreq (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        const double v = t.getDoubleValue();
        return t.containsChar ('k') ? v * 1000.0 : v;
    }
}

BandPanel::BandPanel (ThreeBandEQAudioProcessor& p) : processor (p)
{
    title.setFont (EQColours::font (14.0f, true));
    title.setJustificationType (juce::Justification::centredLeft);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    powerButton.setClickingTogglesState (true);
    powerButton.setTooltip ("Enable / bypass this band");
    powerButton.onClick = [this]
    {
        if (! updating && band >= 0)
            processor.edits.setEnabled (band, powerButton.getToggleState());
    };
    addAndMakeVisible (powerButton);

    typeBox.addItemList (EQ::getTypeNames(), 1);
    typeBox.onChange = [this]
    {
        if (! updating && band >= 0 && typeBox.getSelectedItemIndex() >= 0)
            processor.edits.setType (band, EQ::typeFromIndex (typeBox.getSelectedItemIndex()));
    };
    addAndMakeVisible (typeBox);

    slopeBox.addItemList (EQ::getSlopeNames(), 1);
    slopeBox.setTooltip ("Cut slope (Low Cut / High Cut only)");
    slopeBox.onChange = [this]
    {
        if (! updating && band >= 0 && slopeBox.getSelectedItemIndex() >= 0)
            processor.edits.setSlope (band, slopeBox.getSelectedItemIndex());
    };
    addAndMakeVisible (slopeBox);

    setupKnob (freqKnob, "FREQ", EQ::Param::Freq);
    setupKnob (gainKnob, "GAIN", EQ::Param::Gain);
    setupKnob (qKnob,    "Q",    EQ::Param::Q);

    freqKnob.slider.setRange (EQ::minFreq, EQ::maxFreq, 0.1);
    freqKnob.slider.setSkewFactorFromMidPoint (1000.0);
    freqKnob.slider.textFromValueFunction = formatFreq;
    freqKnob.slider.valueFromTextFunction = parseFreq;

    gainKnob.slider.setRange (EQ::minGain, EQ::maxGain, 0.1);
    gainKnob.slider.setDoubleClickReturnValue (true, 0.0);
    gainKnob.slider.textFromValueFunction = [] (double db) { return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB"; };
    gainKnob.slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };

    qKnob.slider.setRange (EQ::minQ, EQ::maxQ, 0.01);
    qKnob.slider.setSkewFactorFromMidPoint (1.0);
    qKnob.slider.setDoubleClickReturnValue (true, EQ::defaultQ);
    qKnob.slider.textFromValueFunction = [] (double q) { return juce::String (q, q >= 10.0 ? 1 : 2); };
    qKnob.slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };

    resetButton.setTooltip ("Reset gain and Q");
    resetButton.onClick = [this] { if (band >= 0) processor.edits.resetBand (band); };
    addAndMakeVisible (resetButton);

    deleteButton.setTooltip ("Remove this band (Delete key)");
    deleteButton.onClick = [this]
    {
        if (band < 0)
            return;
        processor.edits.removeBand (band);
        if (onBandRemoved)
            onBandRemoved();
    };
    addAndMakeVisible (deleteButton);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->slider.updateText();

    setSize (panelWidth, panelHeight);
    startTimerHz (30);
}

BandPanel::~BandPanel()
{
    if (processor.edits.isEditing())
        processor.edits.endEdit();
}

void BandPanel::setupKnob (Knob& k, const juce::String& name, EQ::Param param)
{
    k.param = param;

    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 16);
    k.slider.setScrollWheelEnabled (false);   // the graph's wheel handles Q; avoids undo spam
    k.slider.setVelocityBasedMode (false);

    k.slider.onDragStart = [this, &k]
    {
        if (band < 0) return;
        k.dragging = true;
        processor.edits.beginEdit (band, "Edit " + k.label.getText().toLowerCase());
    };
    k.slider.onValueChange = [this, &k]
    {
        if (updating || band < 0)
            return;

        if (k.dragging)
            processor.edits.setParam (k.param, (float) k.slider.getValue());
        else
            processor.edits.setValue (band, k.param, (float) k.slider.getValue());   // typed value / reset
    };
    k.slider.onDragEnd = [this, &k]
    {
        if (! k.dragging) return;
        k.dragging = false;
        processor.edits.endEdit();
    };
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (EQColours::font (11.0f, true));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);
}

void BandPanel::setBand (int newBand)
{
    if (newBand == band)
        return;

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        if (k->dragging)
        {
            k->dragging = false;
            processor.edits.endEdit();
        }

    band = newBand;
    refreshFromParams();
    repaint();
}

void BandPanel::refreshFromParams()
{
    if (band < 0)
        return;

    const juce::ScopedValueSetter<bool> svs (updating, true);
    const auto colour = EQColours::band (band);
    const auto type = EQ::typeFromIndex ((int) std::lround (processor.getBandValue (band, EQ::Param::Type)));
    const bool enabled = processor.getBandValue (band, EQ::Param::Enabled) >= 0.5f;

    title.setText ("BAND " + juce::String (band + 1), juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, colour);

    powerButton.setToggleState (enabled, juce::dontSendNotification);
    powerButton.setButtonText (enabled ? "ON" : "OFF");
    powerButton.setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.85f));
    powerButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    if (typeBox.getSelectedItemIndex() != (int) type)
        typeBox.setSelectedItemIndex ((int) type, juce::dontSendNotification);

    const int slope = (int) std::lround (processor.getBandValue (band, EQ::Param::Slope));
    if (slopeBox.getSelectedItemIndex() != slope)
        slopeBox.setSelectedItemIndex (slope, juce::dontSendNotification);

    const bool hasSlope = EQ::typeHasSlope (type);
    slopeBox.setEnabled (hasSlope);
    slopeBox.setAlpha (hasSlope ? 1.0f : 0.35f);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, enabled ? colour : colour.withSaturation (0.1f));
        if (! k->dragging)
            k->slider.setValue (processor.getBandValue (band, k->param), juce::dontSendNotification);
    }

    const double defaultFreq = EQ::defaultFreq (band);
    freqKnob.slider.setDoubleClickReturnValue (true, defaultFreq);

    const bool hasGain = EQ::typeHasGain (type);
    gainKnob.slider.setEnabled (hasGain);
    gainKnob.slider.setAlpha (hasGain ? 1.0f : 0.35f);
    gainKnob.label.setAlpha (hasGain ? 1.0f : 0.35f);
}

void BandPanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (EQColours::panel.withAlpha (0.96f));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (band >= 0 ? EQColours::band (band).withAlpha (0.7f) : EQColours::panelEdge);
    g.drawRoundedRectangle (r, 9.0f, 1.2f);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);

    auto left = area.removeFromLeft (124);
    auto top = left.removeFromTop (24);
    powerButton.setBounds (top.removeFromRight (50));
    title.setBounds (top);
    left.removeFromTop (5);
    typeBox.setBounds (left.removeFromTop (24));
    left.removeFromTop (5);
    slopeBox.setBounds (left.removeFromTop (24));

    auto right = area.removeFromRight (64);
    right = right.withSizeKeepingCentre (right.getWidth(), 62);
    resetButton.setBounds (right.removeFromTop (28));
    right.removeFromTop (6);
    deleteButton.setBounds (right.removeFromTop (28));

    area.reduce (8, 0);
    const int kw = area.getWidth() / 3;
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        auto col = area.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }
}
