#include "BandPanel.h"
#include "EQLookAndFeel.h"
#include "NoteNames.h"

namespace
{
    juce::String formatHz (double hz)
    {
        if (hz >= 1000.0)
            return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";
        return juce::String (hz, hz >= 100.0 ? 0 : 1) + " Hz";
    }

    // "440 Hz · A4 +0¢"
    juce::String formatFreq (double hz)
    {
        return formatHz (hz) + " " + juce::String::charToString (0x00b7) + " " + EQ::Notes::formatNote (hz);
    }

    // Accepts "440", "440 Hz", "1.5k", "2 kHz" and note names ("A2", "C#5", "Eb3").
    double parseFreq (const juce::String& text)
    {
        auto t = text.upToFirstOccurrenceOf (juce::String::charToString (0x00b7), false, false).trim();

        if (auto note = EQ::Notes::parseNote (t))
            return *note;

        t = t.toLowerCase();
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

    stereoBox.addItemList (EQ::getStereoModeNames(), 1);
    stereoBox.setTooltip ("Which channels this band processes: both, left, right, mid or side");
    stereoBox.onChange = [this]
    {
        if (! updating && band >= 0 && stereoBox.getSelectedItemIndex() >= 0)
            processor.edits.setStereoMode (band, stereoBox.getSelectedItemIndex());
    };
    addAndMakeVisible (stereoBox);

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

    soloButton.setClickingTogglesState (true);
    soloButton.setTooltip ("Solo: hear only the region this band affects (Alt-click a node to audition momentarily)");
    soloButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    soloButton.onClick = [this]
    {
        if (updating || band < 0)
            return;
        processor.soloBand.store (soloButton.getToggleState() ? band : -1);   // transient: no undo step
    };
    addAndMakeVisible (soloButton);

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

    // Dynamic EQ: toggle plus a row of controls that is only shown while dynamics are on.
    dynButton.setClickingTogglesState (true);
    dynButton.setTooltip ("Dynamic EQ: the band's gain moves with the level in its own frequency region");
    dynButton.onClick = [this]
    {
        if (! updating && band >= 0)
            processor.edits.setDynamic (band, dynButton.getToggleState());
    };
    addAndMakeVisible (dynButton);

    dynExternalButton.setClickingTogglesState (true);
    dynExternalButton.setTooltip ("Detect from the sidechain input instead of the band's own signal");
    dynExternalButton.onClick = [this]
    {
        if (! updating && band >= 0)
            processor.edits.setValue (band, EQ::Param::DynExternal, dynExternalButton.getToggleState() ? 1.0f : 0.0f);
    };
    addChildComponent (dynExternalButton);

    setupKnob (thresholdKnob, "THRESH", EQ::Param::Threshold);
    setupKnob (rangeKnob,     "RANGE",  EQ::Param::Range);
    setupKnob (attackKnob,    "ATTACK", EQ::Param::Attack);
    setupKnob (releaseKnob,   "RELEASE", EQ::Param::Release);

    auto dbText = [] (double db) { return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB"; };
    auto msText = [] (double ms) { return juce::String (ms, ms < 10.0 ? 1 : 0) + " ms"; };
    auto number = [] (const juce::String& t) { return t.getDoubleValue(); };

    thresholdKnob.slider.setRange (EQ::minThreshold, EQ::maxThreshold, 0.1);
    thresholdKnob.slider.setDoubleClickReturnValue (true, EQ::defaultThreshold);
    thresholdKnob.slider.textFromValueFunction = [] (double db) { return juce::String (db, 1) + " dB"; };
    rangeKnob.slider.setRange (EQ::minRange, EQ::maxRange, 0.1);
    rangeKnob.slider.setDoubleClickReturnValue (true, EQ::defaultRange);
    rangeKnob.slider.textFromValueFunction = dbText;
    attackKnob.slider.setRange (EQ::minAttackMs, EQ::maxAttackMs, 0.01);
    attackKnob.slider.setSkewFactorFromMidPoint (5.0);
    attackKnob.slider.setDoubleClickReturnValue (true, EQ::defaultAttackMs);
    attackKnob.slider.textFromValueFunction = msText;
    releaseKnob.slider.setRange (EQ::minReleaseMs, EQ::maxReleaseMs, 0.1);
    releaseKnob.slider.setSkewFactorFromMidPoint (100.0);
    releaseKnob.slider.setDoubleClickReturnValue (true, EQ::defaultReleaseMs);
    releaseKnob.slider.textFromValueFunction = msText;
    for (auto* k : { &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
    {
        k->slider.valueFromTextFunction = number;
        k->slider.setVisible (false);
        k->label.setVisible (false);
    }

    dynGainLabel.setFont (EQColours::font (11.0f, true));
    dynGainLabel.setJustificationType (juce::Justification::centred);
    dynGainLabel.setTooltip ("Live dynamic gain change");
    addChildComponent (dynGainLabel);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob, &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
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
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 16);
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

    for (auto* k : { &freqKnob, &gainKnob, &qKnob, &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
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

    soloButton.setToggleState (processor.soloBand.load() == band, juce::dontSendNotification);
    soloButton.setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.85f));

    if (typeBox.getSelectedItemIndex() != (int) type)
        typeBox.setSelectedItemIndex ((int) type, juce::dontSendNotification);

    const int slope = (int) std::lround (processor.getBandValue (band, EQ::Param::Slope));
    if (slopeBox.getSelectedItemIndex() != slope)
        slopeBox.setSelectedItemIndex (slope, juce::dontSendNotification);

    const int stereo = (int) std::lround (processor.getBandValue (band, EQ::Param::Stereo));
    if (stereoBox.getSelectedItemIndex() != stereo)
        stereoBox.setSelectedItemIndex (stereo, juce::dontSendNotification);

    // A mono bus treats every mode as Stereo.
    const bool stereoBus = processor.getMainBusNumInputChannels() >= 2;
    stereoBox.setEnabled (stereoBus);
    stereoBox.setAlpha (stereoBus ? 1.0f : 0.35f);

    const bool hasSlope = EQ::typeHasSlope (type);
    slopeBox.setEnabled (hasSlope);
    slopeBox.setAlpha (hasSlope ? 1.0f : 0.35f);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob, &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
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

    // Dynamics: Bell and shelves only; the row is collapsed unless dynamics are on.
    const bool dynParam = processor.getBandValue (band, EQ::Param::DynOn) >= 0.5f;
    dynButton.setToggleState (dynParam, juce::dontSendNotification);
    dynButton.setEnabled (hasGain);
    dynButton.setAlpha (hasGain ? 1.0f : 0.35f);
    dynButton.setColour (juce::TextButton::buttonOnColourId, colour.withAlpha (0.85f));
    dynButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    dynExternalButton.setToggleState (processor.getBandValue (band, EQ::Param::DynExternal) >= 0.5f, juce::dontSendNotification);
    dynExternalButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff9f43));
    dynExternalButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);

    const bool show = dynParam && hasGain;
    if (show != showDynamics)
    {
        showDynamics = show;
        for (auto* k : { &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
        {
            k->slider.setVisible (show);
            k->label.setVisible (show);
        }
        dynExternalButton.setVisible (show);
        dynGainLabel.setVisible (show);
        setSize (panelWidth, getPreferredHeight());
    }

    // Linear-phase mode has no dynamics or solo (they need the zero-latency engine).
    const bool linearPhase = processor.isLinearPhaseSelected();
    dynButton.setEnabled (hasGain && ! linearPhase);
    dynButton.setTooltip (linearPhase ? "Dynamic EQ is not available in linear-phase mode"
                                      : "Dynamic EQ: the band's gain moves with the level in its own frequency region");
    soloButton.setEnabled (! linearPhase);
    soloButton.setAlpha (linearPhase ? 0.35f : 1.0f);
    for (auto* k : { &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
    {
        k->slider.setEnabled (! linearPhase);
        k->slider.setAlpha (linearPhase ? 0.35f : 1.0f);
    }
    dynExternalButton.setEnabled (! linearPhase);

    if (show && linearPhase)
    {
        dynGainLabel.setText ("off (linear phase)", juce::dontSendNotification);
        dynGainLabel.setColour (juce::Label::textColourId, EQColours::label);
    }
    else if (show)
    {
        const float db = processor.dynamicGainDb[(size_t) band].load();
        dynGainLabel.setText (juce::String (db > 0.05f ? "+" : "") + juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) + " dB",
                              juce::dontSendNotification);
        dynGainLabel.setColour (juce::Label::textColourId, std::abs (db) > 0.05f ? colour : EQColours::label);
        dynExternalButton.setAlpha (processor.isSidechainConnected() ? 1.0f : 0.5f);
    }
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
    auto all = getLocalBounds().reduced (12, 8);
    auto area = all.removeFromTop (panelHeight - 16);   // main row; the dynamics row is below

    auto left = area.removeFromLeft (124);
    auto top = left.removeFromTop (24);
    powerButton.setBounds (top.removeFromRight (50));
    title.setBounds (top);
    left.removeFromTop (5);
    typeBox.setBounds (left.removeFromTop (24));
    left.removeFromTop (5);
    slopeBox.setBounds (left.removeFromTop (24));
    left.removeFromTop (5);
    stereoBox.setBounds (left.removeFromTop (24));

    auto right = area.removeFromRight (64);
    right = right.withSizeKeepingCentre (right.getWidth(), 4 * 22 + 3 * 4);
    for (auto* b : { &dynButton, &soloButton, &resetButton, &deleteButton })
    {
        b->setBounds (right.removeFromTop (22));
        right.removeFromTop (4);
    }

    area.reduce (8, 0);
    const int kw = area.getWidth() / 3;
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        auto col = area.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (14));
        k->slider.setBounds (col);
    }

    // Dynamics row: live gain + sidechain toggle on the left, then four knobs.
    if (showDynamics)
    {
        auto row = all.withTrimmedTop (4);
        auto info = row.removeFromLeft (124);
        dynGainLabel.setBounds (info.removeFromTop (24));
        info.removeFromTop (6);
        dynExternalButton.setBounds (info.removeFromTop (24).removeFromLeft (50));

        row.reduce (8, 0);
        const int w = row.getWidth() / 4;
        for (auto* k : { &thresholdKnob, &rangeKnob, &attackKnob, &releaseKnob })
        {
            auto col = row.removeFromLeft (w);
            k->label.setBounds (col.removeFromTop (14));
            k->slider.setBounds (col);
        }
    }
}
