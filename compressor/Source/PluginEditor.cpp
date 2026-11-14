#include "PluginEditor.h"

namespace
{
    constexpr int designW = Comp::EditorLayout::designWidth, designH = Comp::EditorLayout::contentHeight;

    juce::String formatMs (double ms)
    {
        return juce::String (ms, ms < 1.0 ? 2 : (ms < 10.0 ? 1 : (ms < 100.0 ? 1 : 0))) + " ms";
    }
}

SongsmithCompressorAudioProcessorEditor::SongsmithCompressorAudioProcessorEditor (SongsmithCompressorAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    // Read the saved size first: setting the resize limits below resizes the window.
    const int savedWidth = Comp::EditorLayout::constrainWidth (proc.editorWidth.load());

    setLookAndFeel (&lookAndFeel);
    content.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.onPaint = [this] (juce::Graphics& g) { paintContent (g); };
    content.onResize = [this] { layoutContent(); };

    auto setupLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (CompColours::font (CompColours::Type::control, CompColours::Weight::Medium)));
        l.setColour (juce::Label::textColourId, CompColours::label);
        l.setInterceptsMouseClicks (false, false);
        content.addAndMakeVisible (l);
    };
    setupLabel (thresholdLabel, "Threshold");
    setupLabel (ratioLabel, "Ratio");
    setupLabel (attackLabel, "Attack");
    setupLabel (releaseLabel, "Release");
    setupLabel (kneeLabel, "Knee");
    setupLabel (makeupLabel, "Makeup Gain");
    setupLabel (mixLabel, "Mix");
    setupLabel (inLabel, "Input");
    setupLabel (grLabel, "Gain Reduction");
    setupLabel (outLabel, "Output");

    auto setupKnob = [this] (juce::Slider& s, const char* id, const juce::String& tip,
                             std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                             std::function<juce::String (double)> text, std::function<double (const juce::String&)> parse)
    {
        s.setColour (juce::Slider::rotarySliderFillColourId, CompColours::accent);
        s.setColour (juce::Slider::textBoxOutlineColourId, CompColours::panelEdge);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 22);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
        auto* param = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        s.textFromValueFunction = std::move (text);
        s.valueFromTextFunction = std::move (parse);
        s.setTooltip (tip);
        s.updateText();
        content.addAndMakeVisible (s);
    };
    auto number = [] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getDoubleValue(); };

    setupKnob (thresholdSlider, Comp::thresholdId,
               "Threshold: the level above which the sound is turned down. Lower it to compress more of the sound",
               thresholdAttachment, [] (double v) { return juce::String (v, 1) + " dB"; }, number);
    setupKnob (ratioSlider, Comp::ratioId,
               "Ratio: how firmly the sound above Threshold is turned down. Near 1 to 1 is very gentle; higher is firmer",
               ratioAttachment, [] (double v) { return juce::String (v, 2) + ":1"; }, number);
    setupKnob (attackSlider, Comp::attackId,
               "Attack: how quickly the turning down starts. A slower attack lets the start of each note through",
               attackAttachment, [] (double v) { return formatMs (v); }, number);
    setupKnob (releaseSlider, Comp::releaseId,
               "Release: how quickly the sound returns to full level. Too short can sound jumpy; too long can dull the beat",
               releaseAttachment, [] (double v) { return formatMs (v); }, number);
    setupKnob (kneeSlider, Comp::kneeId,
               "Knee: how gradually the turning down begins around Threshold. 0 is abrupt; larger is smoother",
               kneeAttachment, [] (double v) { return juce::String (v, 1) + " dB"; }, number);
    setupKnob (makeupSlider, Comp::makeupId,
               "Makeup Gain: raises the level after compression. Switched off while Auto Makeup is on",
               makeupAttachment, [] (double v) { return juce::String (v, 1) + " dB"; }, number);
    setupKnob (mixSlider, Comp::mixId,
               "Mix: blends the compressed sound with the original. Lower it to compress in parallel",
               mixAttachment, [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; }, number);

    autoMakeupButton.setClickingTogglesState (true);
    autoMakeupButton.setTooltip ("Auto Makeup: raises the level to make up for the compression, matched for a typical level of -18 dBFS. "
                                 "Switch it off to set Makeup Gain yourself");
    autoMakeupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Comp::autoMakeupId, autoMakeupButton);
    content.addAndMakeVisible (autoMakeupButton);

    warmthButton.setClickingTogglesState (true);
    warmthButton.setTooltip ("Analog Warmth: adds a little smooth, rounded colour to the compressed sound. Off keeps the sound clean. Quiet and normal levels stay the same loudness; only the loudest peaks are rounded off slightly");
    warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Comp::warmthId, warmthButton);
    content.addAndMakeVisible (warmthButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Comp::bypassId, bypassButton);
    content.addAndMakeVisible (bypassButton);

    // The preset selector sits in the title bar after the product name, as in the other plugins.
    for (size_t i = 0; i < Comp::getPresets().size(); ++i)
        presetBox.addItem (Comp::getPresets()[i].name, (int) i + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.setTitle ("Preset");
    presetBox.setTooltip ("Preset: choose a starting point for a kind of sound, then adjust Threshold by ear. Loading one keeps the window size.");
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index < 0)
            return;
        proc.history.runStep ("preset load", [&] { proc.applyPreset (index); });
        presetBox.setTooltip (Comp::getPresets()[(size_t) index].hint);
        presetBox.setSelectedId (0, juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected (Comp::getPresets()[(size_t) index].name);
        syncFromParameters();
        refreshHistoryButtons();
    };
    content.addAndMakeVisible (presetBox);

    setWantsKeyboardFocus (true);
    undoButton.setTitle ("Undo");
    redoButton.setTitle ("Redo");
    undoButton.onClick = [this] { proc.history.undo(); syncFromParameters(); refreshHistoryButtons(); };
    redoButton.onClick = [this] { proc.history.redo(); syncFromParameters(); refreshHistoryButtons(); };
    content.addAndMakeVisible (undoButton);
    content.addAndMakeVisible (redoButton);
    refreshHistoryButtons();
    presetBox.setExplicitFocusOrder (1);
    undoButton.setExplicitFocusOrder (2);
    redoButton.setExplicitFocusOrder (3);
    bypassButton.setExplicitFocusOrder (4);

    inputMeter.setTooltip ("Input peak (click to reset). The red mark means a sample reached 0 dBFS. This is sample peak, not true peak");
    reductionMeter.setTooltip ("Gain Reduction: how much the compressor is turning the level down, 0 to 24 dB");
    outputMeter.setTooltip ("Output peak (click to reset). The red mark means a sample reached 0 dBFS. This is sample peak, not true peak");
    for (auto* m : { &inputMeter, &reductionMeter, &outputMeter })
        content.addAndMakeVisible (*m);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);
    content.addAndMakeVisible (titleHint);   // the plugin name explains the plugin

    syncFromParameters();

    setResizable (true, true);
    setResizeLimits (Comp::EditorLayout::minWidth, Comp::EditorLayout::heightFor (Comp::EditorLayout::minWidth),
                     Comp::EditorLayout::maxWidth, Comp::EditorLayout::heightFor (Comp::EditorLayout::maxWidth));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) Comp::EditorLayout::designWidth / Comp::EditorLayout::designHeight);

    setSize (savedWidth, Comp::EditorLayout::heightFor (savedWidth));
    constructed = true;

    lastTimerTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (30);
}

SongsmithCompressorAudioProcessorEditor::~SongsmithCompressorAudioProcessorEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SongsmithCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (CompColours::background);
}

void SongsmithCompressorAudioProcessorEditor::resized()
{
    contentScale = (float) getWidth() / (float) Comp::EditorLayout::designWidth;
    content.setBounds (0, 0, Comp::EditorLayout::designWidth, Comp::EditorLayout::designHeight);
    content.setTransform (juce::AffineTransform::scale (contentScale));
    if (constructed)
        proc.editorWidth.store (getWidth());
}

//==============================================================================
void SongsmithCompressorAudioProcessorEditor::layoutContent()
{
    // Title bar, the same in every plugin: name | preset | Undo | Redo ... Bypass (far right).
    {
        const auto header = Songsmith::UI::Header::layout (
            Songsmith::UI::Header::rowIn ((int) Songsmith::UI::titleInset, designW - 16 - (int) Songsmith::UI::titleInset,
                                          (int) Songsmith::UI::titleBarHeight),
            JucePlugin_Name);
        titleBounds = header.title;
        titleHint.setBounds (header.title);
        presetBox.setBounds (header.preset);
        undoButton.setBounds (header.undo);
        redoButton.setBounds (header.redo);
        bypassButton.setBounds (header.bypass);
    }

    // Two rows of knobs on the left; the three meters on the right. The knob that shapes the sound
    // most (Threshold) comes first, and Auto Makeup sits beside Makeup Gain.
    constexpr int colW = 156, left = 16, knobW = 128, knobH = 118;
    auto column = [&] (int c) { return left + c * colW + (colW - knobW) / 2; };
    const int row1Label = 64, row1Knob = row1Label + 22, row2Label = row1Knob + knobH + 8, row2Knob = row2Label + 22;

    auto place = [&] (juce::Label& label, juce::Slider& knob, int c, int labelY, int knobY)
    {
        label.setBounds (column (c), labelY, knobW, 20);
        knob.setBounds (column (c), knobY, knobW, knobH);
    };
    place (thresholdLabel, thresholdSlider, 0, row1Label, row1Knob);
    place (ratioLabel, ratioSlider, 1, row1Label, row1Knob);
    place (attackLabel, attackSlider, 2, row1Label, row1Knob);
    place (releaseLabel, releaseSlider, 3, row1Label, row1Knob);
    place (kneeLabel, kneeSlider, 0, row2Label, row2Knob);
    place (makeupLabel, makeupSlider, 1, row2Label, row2Knob);
    autoMakeupButton.setBounds (column (2) + 4, row2Knob + 16, knobW - 8, 28);   // beside Makeup Gain
    warmthButton.setBounds (column (2) + 4, row2Knob + 64, knobW - 8, 28);
    place (mixLabel, mixSlider, 3, row2Label, row2Knob);

    const int meterLeft = left + 4 * colW + 28, meterTop = row1Label + 36, meterBottom = designH - 24;
    inLabel.setBounds (meterLeft, row1Label, 60, 34);
    inputMeter.setBounds (meterLeft + 18, meterTop, 24, meterBottom - meterTop);
    grLabel.setBounds (meterLeft + 64, row1Label, 84, 34);
    reductionMeter.setBounds (meterLeft + 94, meterTop, 24, meterBottom - meterTop);
    outLabel.setBounds (meterLeft + 152, row1Label, 60, 34);
    outputMeter.setBounds (meterLeft + 170, meterTop, 24, meterBottom - meterTop);

    diagnostics->setBounds (0, designH, designW, Comp::EditorLayout::footerHeight);
}

void SongsmithCompressorAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    const auto area = juce::Rectangle<float> ((float) designW, (float) Comp::EditorLayout::designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, area);
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, area.withHeight (Songsmith::UI::titleBarHeight));
    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, CompColours::text, CompColours::accent);

    // A fine vertical line sets the meters apart from the controls.
    g.setColour (CompColours::panelEdge);
    g.drawVerticalLine (inLabel.getX() - 14, (float) Songsmith::UI::titleBarHeight + 16.0f, (float) designH - 16.0f);
}

//==============================================================================
void SongsmithCompressorAudioProcessorEditor::syncFromParameters()
{
    // Makeup Gain does nothing while Auto Makeup is on, so it is greyed out (its tooltip says why).
    makeupSlider.setEnabled (! autoMakeupButton.getToggleState());
}

void SongsmithCompressorAudioProcessorEditor::updateMeters (double elapsedSeconds)
{
    inputMeter.push (Comp::gainToDb (SongsmithCompressorAudioProcessor::readAndReset (proc.inputPeak)), elapsedSeconds);
    outputMeter.push (Comp::gainToDb (SongsmithCompressorAudioProcessor::readAndReset (proc.outputPeak)), elapsedSeconds);
    reductionMeter.push (SongsmithCompressorAudioProcessor::readAndReset (proc.gainReductionDb), elapsedSeconds);
}

void SongsmithCompressorAudioProcessorEditor::refreshHistoryButtons()
{
    Songsmith::UI::Header::refreshHistoryButtons (undoButton, redoButton, proc.history);
}

bool SongsmithCompressorAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const bool used = Songsmith::UI::Header::handleHistoryKey (key, proc.history);
    if (used)
    {
        syncFromParameters();
        refreshHistoryButtons();
    }
    return used;
}

void SongsmithCompressorAudioProcessorEditor::timerCallback()
{
    refreshHistoryButtons();
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    updateMeters (juce::jlimit (0.0, 0.5, now - lastTimerTime));
    lastTimerTime = now;
    syncFromParameters();
}
