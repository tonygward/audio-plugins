#include "PluginEditor.h"

namespace
{
    // The controls are laid out in the top contentHeight; the diagnostics strip sits below them.
    constexpr int designW = Max::EditorLayout::designWidth, designH = Max::EditorLayout::contentHeight;
}

SongsmithMaximiserAudioProcessorEditor::SongsmithMaximiserAudioProcessorEditor (SongsmithMaximiserAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    // Read the saved size first: setting the resize limits below resizes the window (and resized()
    // would otherwise overwrite the saved width with the minimum).
    const int savedWidth = Max::EditorLayout::constrainWidth (proc.editorWidth.load());

    setLookAndFeel (&lookAndFeel);
    content.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };
    content.onResize = [this] { layoutContent(); };

    auto setupLabel = [this] (juce::Label& l, const juce::String& text, juce::Justification just = juce::Justification::centred)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (just);
        l.setFont (juce::Font (MaxColours::font (MaxColours::Type::control, MaxColours::Weight::Medium)));
        l.setColour (juce::Label::textColourId, MaxColours::label);
        l.setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (l);
    };

    setupLabel (thresholdLabel, "Threshold");
    setupLabel (ceilingLabel, "Output Ceiling");
    setupLabel (releaseLabel, "Release");
    setupLabel (inLabel, "Input");
    setupLabel (grLabel, "Gain Reduction");
    setupLabel (outLabel, "Output");
    setupLabel (peakLabel, "Peak", juce::Justification::centredRight);
    setupLabel (ditherLabel, "Dither", juce::Justification::centredRight);
    setupLabel (gainLabel, {});
    gainLabel.setColour (juce::Label::textColourId, MaxColours::text);
    gainLabel.setFont (juce::Font (MaxColours::numbers (MaxColours::Type::control)));

    // Threshold and Out Ceiling: driven by hand so Link only follows the user (see faderMoved).
    auto setupFader = [this] (juce::Slider& s, juce::Colour colour, const char* id, bool isThreshold)
    {
        auto* param = proc.apvts.getParameter (id);
        s.setRange (param->getNormalisableRange().start, param->getNormalisableRange().end, 0.01);
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        s.setNumDecimalPlacesToDisplay (1);
        s.setTextValueSuffix (" dB");
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
        s.setColour (juce::Slider::trackColourId, colour);
        s.setColour (juce::Slider::textBoxOutlineColourId, MaxColours::panelEdge);
        s.onValueChange = [this, isThreshold] { faderMoved (isThreshold); };
        s.onDragStart = [this, isThreshold]
        {
            if (gestureDepth++ == 0)
            {
                proc.apvts.getParameter (isThreshold ? Max::thresholdId : Max::ceilingId)->beginChangeGesture();
                if (proc.linkThresholdCeiling.load())
                    proc.apvts.getParameter (isThreshold ? Max::ceilingId : Max::thresholdId)->beginChangeGesture();
            }
        };
        s.onDragEnd = [this, isThreshold]
        {
            if (--gestureDepth == 0)
            {
                proc.apvts.getParameter (Max::thresholdId)->endChangeGesture();
                proc.apvts.getParameter (Max::ceilingId)->endChangeGesture();
            }
            gestureDepth = juce::jmax (0, gestureDepth);
            juce::ignoreUnused (isThreshold);
        };
        content.addAndMakeVisible (s);
    };
    setupFader (thresholdSlider, MaxColours::threshold, Max::thresholdId, true);
    setupFader (ceilingSlider, MaxColours::ceiling, Max::ceilingId, false);
    thresholdSlider.setTooltip ("Threshold: how hard the sound is pushed into the limiter. Lower it to make the sound louder; the gain is Output Ceiling minus Threshold");
    ceilingSlider.setTooltip ("Output Ceiling: the highest level the output may reach (sample or true peak, see Peak)");

    linkButton.setClickingTogglesState (true);
    linkButton.setToggleState (proc.linkThresholdCeiling.load(), juce::dontSendNotification);
    linkButton.setTooltip ("Link: moving Threshold or Output Ceiling moves the other by the same amount, keeping the gain the same");
    linkButton.onClick = [this] { proc.linkThresholdCeiling.store (linkButton.getToggleState()); };
    content.addAndMakeVisible (linkButton);

    releaseSlider.setColour (juce::Slider::rotarySliderFillColourId, MaxColours::accent);
    releaseSlider.setColour (juce::Slider::textBoxOutlineColourId, MaxColours::panelEdge);
    releaseSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, Max::releaseId, releaseSlider);
    releaseSlider.setDoubleClickReturnValue (true, (double) Max::defaultReleaseMs);
    releaseSlider.textFromValueFunction = [] (double ms)
    {
        return juce::String (ms, ms < 1.0 ? 3 : (ms < 10.0 ? 2 : (ms < 100.0 ? 1 : 0))) + " ms";
    };
    releaseSlider.valueFromTextFunction = [] (const juce::String& text) { return text.retainCharacters ("0123456789.").getDoubleValue(); };
    releaseSlider.updateText();
    content.addAndMakeVisible (releaseSlider);

    autoReleaseButton.setClickingTogglesState (true);
    autoReleaseButton.setTooltip ("Auto Release: fast recovery after short peaks, slower and steadier on dense material");
    autoReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Max::autoReleaseId, autoReleaseButton);
    content.addAndMakeVisible (autoReleaseButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Max::bypassId, bypassButton);
    content.addAndMakeVisible (bypassButton);

    peakModeBox.addItemList (Max::getPeakModeNames(), 1);
    peakModeBox.setTooltip ("True peak (the default) also limits the peaks between samples, which is what streaming and broadcast checks measure. Sample peak limits only the samples and uses less processing power. Neither adds latency");
    peakModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, Max::peakModeId, peakModeBox);
    content.addAndMakeVisible (peakModeBox);

    ditherBox.addItemList (Max::getDitherNames(), 1);
    ditherBox.setTooltip ("Dither: adds a very quiet noise before the output is rounded to 16 or 24 bits, so quiet fades stay smooth. Use it only for the final export; leave it off otherwise");
    ditherAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, Max::ditherId, ditherBox);
    content.addAndMakeVisible (ditherBox);

    // Factory starting points: loading one sets the parameters (Bypass and Link stay as they are).
    for (size_t i = 0; i < Max::getStartingPoints().size(); ++i)
        startingPointBox.addItem (Max::getStartingPoints()[i].name, (int) i + 1);
    startingPointBox.setTextWhenNothingSelected ("Starting point");
    startingPointBox.setTooltip ("Conservative starting points. Dither stays off in all of them");
    startingPointBox.onChange = [this]
    {
        const int index = startingPointBox.getSelectedItemIndex();
        if (index < 0)
            return;
        proc.applyStartingPoint (index);
        startingPointBox.setTooltip (Max::getStartingPoints()[(size_t) index].hint);
        startingPointBox.setSelectedId (0, juce::dontSendNotification);
        startingPointBox.setTextWhenNothingSelected (juce::String ("Loaded: ") + Max::getStartingPoints()[(size_t) index].name);
        syncFromParameters();
    };
    content.addAndMakeVisible (startingPointBox);

    inputMeter.setTooltip ("Input peak (click to reset)");
    reductionMeter.setTooltip ("Gain Reduction: how much the limiter is turning the level down, 0 to 24 dB");
    outputMeter.setTooltip ("Output peak (click to reset)");
    for (auto* m : { &inputMeter, &reductionMeter, &outputMeter })
        content.addAndMakeVisible (*m);

    // Loudness section. A Songsmith addition (the L1 had none). It only hears audio as it plays through.
    using Just = juce::Justification;
    setupLabel (loudnessTitle, "Loudness", Just::centredLeft);
    loudnessTitle.setFont (juce::Font (MaxColours::font (MaxColours::Type::heading, MaxColours::Weight::SemiBold)));
    loudnessTitle.setColour (juce::Label::textColourId, MaxColours::text);
    setupLabel (loudnessHint, "Measures what plays through the plugin, as it plays.", Just::centredLeft);
    loudnessHint.setFont (juce::Font (MaxColours::font (MaxColours::Type::hint)));

    auto setupReading = [&] (juce::Label& caption, ReadoutLabel& value, juce::Label& unit, const char* name, float valueSize,
                             MaxColours::Weight valueWeight, const juce::String& tip)
    {
        setupLabel (caption, name, Just::centredLeft);
        setupLabel (value, juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")), Just::centredLeft);
        value.setFont (juce::Font (MaxColours::numbers (valueSize, valueWeight)));
        value.setColour (juce::Label::textColourId, MaxColours::text);
        setupLabel (unit, "LUFS", Just::centredLeft);
        unit.setFont (juce::Font (MaxColours::font (MaxColours::Type::hint)));
        value.setInterceptsMouseClicks (true, false);   // click the number to start again
        value.onClick = [this] { resetLoudness(); };
        value.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        for (auto* c : { (juce::Component*) &caption, (juce::Component*) &value, (juce::Component*) &unit })
            if (auto* t = dynamic_cast<juce::SettableTooltipClient*> (c))
                t->setTooltip (tip);
        value.setInterceptsMouseClicks (true, false);
    };
    setupReading (integratedCaption, integratedValue, integratedUnit, "Integrated", MaxColours::Type::title, MaxColours::Weight::SemiBold,
                  "Integrated: the loudness of everything played since the last reset. Play the whole song through for its loudness. Click to reset");
    setupReading (shortTermCaption, shortTermValue, shortTermUnit, "Short Term", MaxColours::Type::heading, MaxColours::Weight::Medium,
                  "Short Term: the loudness over the last 3 seconds. Click the Integrated number or Reset to start again");
    setupReading (momentaryCaption, momentaryValue, momentaryUnit, "Momentary", MaxColours::Type::heading, MaxColours::Weight::Medium,
                  "Momentary: the loudness over the last 0.4 seconds. It follows the music closely. Click the Integrated number or Reset to start again");
    integratedValue.setTooltip ("Integrated: the loudness of everything played since the last reset. Play the whole song through for its loudness. Click to reset");
    loudnessResetButton.setTooltip ("Reset: start measuring again. This also happens each time the host starts playing from a stop");
    loudnessResetButton.onClick = [this] { resetLoudness(); };
    content.addAndMakeVisible (loudnessResetButton);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);

    guidanceButton = std::make_unique<Songsmith::Guidance::Button> (JucePlugin_Name, [this] { return currentGuidance(); });
    content.addAndMakeVisible (*guidanceButton);

    showLoudness();
    syncFromParameters();

    setResizable (true, true);
    setResizeLimits (Max::EditorLayout::minWidth, Max::EditorLayout::heightFor (Max::EditorLayout::minWidth),
                     Max::EditorLayout::maxWidth, Max::EditorLayout::heightFor (Max::EditorLayout::maxWidth));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) Max::EditorLayout::designWidth / Max::EditorLayout::designHeight);

    setSize (savedWidth, Max::EditorLayout::heightFor (savedWidth));
    constructed = true;

    lastTimerTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (30);
}

SongsmithMaximiserAudioProcessorEditor::~SongsmithMaximiserAudioProcessorEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SongsmithMaximiserAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (MaxColours::background);
}

void SongsmithMaximiserAudioProcessorEditor::resized()
{
    contentScale = (float) getWidth() / (float) Max::EditorLayout::designWidth;
    content.setBounds (0, 0, Max::EditorLayout::designWidth, Max::EditorLayout::designHeight);
    content.setTransform (juce::AffineTransform::scale (contentScale));
    if (constructed)
        proc.editorWidth.store (getWidth());
}

//==============================================================================
void SongsmithMaximiserAudioProcessorEditor::layoutContent()
{
    // Top bar.
    bypassButton.setBounds (designW - 16 - 84, 12, 84, 26);
    ditherBox.setBounds (bypassButton.getX() - 12 - 96, 12, 96, 26);
    ditherLabel.setBounds (ditherBox.getX() - 56, 12, 52, 26);
    peakModeBox.setBounds (ditherLabel.getX() - 12 - 116, 12, 116, 26);
    peakLabel.setBounds (peakModeBox.getX() - 44, 12, 40, 26);

    // Main row: Input | Threshold | Link | Output Ceiling | Gain Reduction | Output | Release.
    // The meter and fader captions have room for two lines ("Gain Reduction"), so no word is shortened.
    const int top = 70, labelH = 34, bottom = designH - 24;
    const int meterTop = top + labelH + 4, faderBottom = bottom;

    inLabel.setBounds (8, top, 64, labelH);
    inputMeter.setBounds (28, meterTop, 24, bottom - meterTop - 26);

    thresholdLabel.setBounds (76, top, 100, labelH);
    thresholdSlider.setBounds (86, meterTop, 80, faderBottom - meterTop);

    linkButton.setBounds (184, (meterTop + faderBottom) / 2 - 14, 56, 28);

    ceilingLabel.setBounds (248, top, 100, labelH);
    ceilingSlider.setBounds (258, meterTop, 80, faderBottom - meterTop);

    grLabel.setBounds (346, top, 80, labelH);
    reductionMeter.setBounds (372, meterTop, 28, bottom - meterTop - 26);

    outLabel.setBounds (426, top, 56, labelH);
    outputMeter.setBounds (442, meterTop, 24, bottom - meterTop - 26);

    const int releaseTop = top + 24;   // the Release knob stays where it was
    releaseLabel.setBounds (500, top + (labelH - 20) / 2, 180, 20);   // on the same line as the other captions
    releaseSlider.setBounds (515, releaseTop + 4, 150, 150);
    autoReleaseButton.setBounds (530, releaseSlider.getBottom() + 14, 120, 28);
    gainLabel.setBounds (490, autoReleaseButton.getBottom() + 22, 200, 22);
    startingPointBox.setBounds (500, gainLabel.getBottom() + 18, 180, 26);

    // Loudness section: its own band below the controls, above the diagnostics strip.
    const int ly = designH;
    loudnessTitle.setBounds (16, ly + 8, 90, 22);
    loudnessHint.setBounds (106, ly + 8, 440, 22);
    integratedCaption.setBounds (16, ly + 36, 140, 20);
    integratedValue.setBounds (16, ly + 58, 100, 32);
    integratedUnit.setBounds (118, ly + 66, 48, 20);
    shortTermCaption.setBounds (220, ly + 36, 140, 20);
    shortTermValue.setBounds (220, ly + 62, 80, 24);
    shortTermUnit.setBounds (302, ly + 64, 48, 20);
    momentaryCaption.setBounds (400, ly + 36, 140, 20);
    momentaryValue.setBounds (400, ly + 62, 80, 24);
    momentaryUnit.setBounds (482, ly + 64, 48, 20);
    loudnessResetButton.setBounds (designW - 16 - 84, ly + 60, 84, 26);
    guidanceButton->setBounds (designW - 16 - 84, ly + 8, 84, 26);   // beside the readings it talks about

    diagnostics->setBounds (0, designH + Max::EditorLayout::loudnessHeight, designW, Max::EditorLayout::footerHeight);
}

void SongsmithMaximiserAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    const auto area = juce::Rectangle<float> ((float) designW, (float) Max::EditorLayout::designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, area);
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, area.withHeight (Songsmith::UI::titleBarHeight));

    Songsmith::UI::drawTitle (g, { Songsmith::UI::titleInset, 0.0f, 360.0f, Songsmith::UI::titleBarHeight },
                              JucePlugin_Name, MaxColours::text, MaxColours::accent);

    // A fine line sets the loudness section apart from the controls above it.
    g.setColour (MaxColours::panelEdge);
    g.drawHorizontalLine (designH, 12.0f, (float) designW - 12.0f);

    // The Link bracket between the two faders while linked.
    if (linkButton.getToggleState())
    {
        g.setColour (MaxColours::accent.withAlpha (0.6f));
        const float y = (float) linkButton.getBounds().getCentreY();
        g.drawLine ((float) thresholdSlider.getRight() - 16.0f, y, (float) linkButton.getX(), y, 1.5f);
        g.drawLine ((float) linkButton.getRight(), y, (float) ceilingSlider.getX() + 16.0f, y, 1.5f);
    }
}

//==============================================================================
void SongsmithMaximiserAudioProcessorEditor::setParameter (const char* id, float value)
{
    auto* param = proc.apvts.getParameter (id);
    const bool ownGesture = gestureDepth == 0;
    if (ownGesture) param->beginChangeGesture();
    param->setValueNotifyingHost (param->convertTo0to1 (value));
    if (ownGesture) param->endChangeGesture();
}

void SongsmithMaximiserAudioProcessorEditor::faderMoved (bool isThreshold)
{
    if (updatingFaders)
        return;

    const juce::ScopedValueSetter<bool> guard (updatingFaders, true);
    const float t = proc.getThresholdDb(), c = proc.getCeilingDb();
    const float newValue = (float) (isThreshold ? thresholdSlider : ceilingSlider).getValue();

    if (proc.linkThresholdCeiling.load())
    {
        const auto moved = Max::moveLinked (t, c, newValue, isThreshold);
        setParameter (Max::thresholdId, moved.threshold);
        setParameter (Max::ceilingId, moved.ceiling);
    }
    else
    {
        setParameter (isThreshold ? Max::thresholdId : Max::ceilingId, newValue);
    }

    // Show what the parameters ended up as (a linked move may have stopped at a range end).
    thresholdSlider.setValue (proc.getThresholdDb(), juce::dontSendNotification);
    ceilingSlider.setValue (proc.getCeilingDb(), juce::dontSendNotification);
}

void SongsmithMaximiserAudioProcessorEditor::syncFromParameters()
{
    const juce::ScopedValueSetter<bool> guard (updatingFaders, true);
    if (! thresholdSlider.isMouseButtonDown())
        thresholdSlider.setValue (proc.getThresholdDb(), juce::dontSendNotification);
    if (! ceilingSlider.isMouseButtonDown())
        ceilingSlider.setValue (proc.getCeilingDb(), juce::dontSendNotification);
    if (linkButton.getToggleState() != proc.linkThresholdCeiling.load())
        linkButton.setToggleState (proc.linkThresholdCeiling.load(), juce::dontSendNotification);

    const float gain = proc.getCeilingDb() - proc.getThresholdDb();
    gainLabel.setText ("Gain below threshold " + juce::String (gain >= 0.0f ? "+" : "") + juce::String (gain, 1) + " dB",
                       juce::dontSendNotification);
}

void SongsmithMaximiserAudioProcessorEditor::updateMeters (double elapsedSeconds)
{
    const float inputPeakDb = Max::gainToDb (SongsmithMaximiserAudioProcessor::readAndReset (proc.inputPeak));
    inputMeter.push (inputPeakDb, elapsedSeconds);
    outputMeter.push (Max::gainToDb (SongsmithMaximiserAudioProcessor::readAndReset (proc.outputPeak)), elapsedSeconds);
    const float reduction = SongsmithMaximiserAudioProcessor::readAndReset (proc.gainReductionDb);
    reductionMeter.push (reduction, elapsedSeconds);

    // Guidance only needs to know whether audio is playing and how much the limiter has been working lately.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (inputPeakDb > -60.0f)
        lastSignalSeconds = now;
    recentReductionDb = reduction > recentReductionDb ? reduction : recentReductionDb * (float) std::exp (-elapsedSeconds / 2.0);

    proc.updateLoudness();
    showLoudness();
}

Songsmith::Guidance::Message SongsmithMaximiserAudioProcessorEditor::currentGuidance() const
{
    Songsmith::Guidance::MaximiserInput in;
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    in.hasSignal = (now - lastSignalSeconds) < 3.0;   // audio played in the last three seconds
    in.gainReductionDb = recentReductionDb;
    in.ceilingDb = proc.getCeilingDb();
    const auto shortTerm = proc.getLoudnessReadings().getShortTerm();
    if (shortTerm.valid)
        in.shortTermLufs = shortTerm.lufs;
    return Songsmith::Guidance::maximiserMessage (in);
}

void SongsmithMaximiserAudioProcessorEditor::showLoudness()
{
    const auto& readings = proc.getLoudnessReadings();
    integratedValue.setText (Max::Loudness::format (readings.getIntegrated()), juce::dontSendNotification);
    shortTermValue.setText (Max::Loudness::format (readings.getShortTerm()), juce::dontSendNotification);
    momentaryValue.setText (Max::Loudness::format (readings.getMomentary()), juce::dontSendNotification);
}

void SongsmithMaximiserAudioProcessorEditor::resetLoudness()
{
    proc.resetLoudness();   // a step finished a moment later belongs to the new pass
    showLoudness();
}

void SongsmithMaximiserAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    updateMeters (juce::jlimit (0.0, 0.5, now - lastTimerTime));
    lastTimerTime = now;
    syncFromParameters();
    content.repaint (linkButton.getBounds().expanded (80, 0));
}
