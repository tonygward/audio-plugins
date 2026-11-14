#include "PluginEditor.h"

namespace
{
    constexpr int designW = Comp::EditorLayout::designWidth, designH = Comp::EditorLayout::contentHeight;

    // The floating panel: seven knobs in a row, then the two buttons.
    constexpr int panelPad = 14, knobColumn = 86, knobCount = 7, labelH = 18, knobH = 96, buttonH = 28, panelGap = 8;
    constexpr int panelW = panelPad * 2 + knobColumn * knobCount;
    constexpr int panelH = 12 + labelH + knobH + panelGap + buttonH + 12;

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

    // The graph fills the window; the controls float over it.
    content.addAndMakeVisible (graph);
    graph.setThresholdDb (proc.getEngineParams().thresholdDb);
    graph.setTitle ("Level graph");

    pauseButton.setClickingTogglesState (true);
    pauseButton.setTooltip ("Pause: hold the graph as it is so you can look at it. Sound and meters carry on. Click again to continue");
    pauseButton.onClick = [this] { graph.setPaused (pauseButton.getToggleState()); };
    content.addAndMakeVisible (pauseButton);

    content.addAndMakeVisible (panel);
    panel.setAccent (CompColours::accent);
    panel.setTitle ("Compressor controls");
    panel.onUserMoved = [this]
    {
        const auto at = panel.getRelativePosition();
        proc.panelX.store (at.x);
        proc.panelY.store (at.y);
    };

    auto setupLabel = [this] (juce::Component& parent, juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (CompColours::font (CompColours::Type::control, CompColours::Weight::Medium)));
        l.setColour (juce::Label::textColourId, CompColours::label);
        l.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (l);
    };
    setupLabel (panel, thresholdLabel, "Threshold");
    setupLabel (panel, ratioLabel, "Ratio");
    setupLabel (panel, attackLabel, "Attack");
    setupLabel (panel, releaseLabel, "Release");
    setupLabel (panel, kneeLabel, "Knee");
    setupLabel (panel, makeupLabel, "Makeup Gain");
    setupLabel (panel, mixLabel, "Mix");
    setupLabel (content, grLabel, "Gain Reduction");
    setupLabel (content, grValue, "0.0 dB");
    grValue.setFont (juce::Font (CompColours::numbers (CompColours::Type::control)));
    grValue.setColour (juce::Label::textColourId, CompColours::text);

    auto setupKnob = [this] (juce::Slider& s, const char* id, const juce::String& tip,
                             std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                             std::function<juce::String (double)> text, std::function<double (const juce::String&)> parse)
    {
        s.setColour (juce::Slider::rotarySliderFillColourId, CompColours::accent);
        s.setColour (juce::Slider::textBoxOutlineColourId, CompColours::panelEdge);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 22);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s);
        auto* param = proc.apvts.getParameter (id);
        s.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        s.textFromValueFunction = std::move (text);
        s.valueFromTextFunction = std::move (parse);
        s.setTooltip (tip);
        s.updateText();
        panel.addAndMakeVisible (s);
    };
    auto number = [] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getDoubleValue(); };

    setupKnob (thresholdSlider, Comp::thresholdId,
               "Threshold: the level above which the sound is turned down. Lower it to compress more of the sound. The dashed line on the graph shows it",
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
    panel.addAndMakeVisible (autoMakeupButton);

    warmthButton.setClickingTogglesState (true);
    warmthButton.setTooltip ("Analog Warmth: adds a little smooth, rounded colour to the compressed sound. Off keeps the sound clean. Quiet and normal levels stay the same loudness; only the loudest peaks are rounded off slightly");
    warmthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, Comp::warmthId, warmthButton);
    panel.addAndMakeVisible (warmthButton);

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

    reductionMeter.setTooltip ("Gain Reduction: how much the compressor is turning the level down right now, 0 to 24 dB. "
                               "This is the reduction alone; Makeup Gain is separate and is not subtracted");
    outputMeter.setTooltip ("Output level for each channel, after compression, Makeup Gain, Analog Warmth, Mix and Bypass. "
                            "The number under the clip warning is the highest sample level held for 1.5 seconds: negative means below 0 dBFS, "
                            "positive means over it and is red. The warning lights when a sample reaches 0 dBFS and stays lit while "
                            "sound continues. This is sample peak, not true peak. Click to clear the held number");
    content.addAndMakeVisible (reductionMeter);
    content.addAndMakeVisible (outputMeter);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);
    content.addAndMakeVisible (titleHint);   // the plugin name explains the plugin

    // What the audio thread has already measured is old: start the graph from now.
    proc.graphFifo.clear();
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

    // Right edge: the output meter, the same size and place as the Songsmith EQ's (full height, at the far right),
    // with the Gain Reduction bar beside it, its bar starting level with the meter's bars.
    const int top = (int) Songsmith::UI::titleBarHeight + 8, bottom = designH - 12, height = bottom - top;
    const int meterX = designW - 16 - Songsmith::UI::OutputMeterDisplay::preferredWidth;
    outputMeter.setBounds (meterX, top, Songsmith::UI::OutputMeterDisplay::preferredWidth, height);

    const int barsTop = top + 46, barsBottom = top + height - (int) Songsmith::UI::OutputMeterDisplay::bottomRow;
    constexpr int grWidth = 88, grBarWidth = 26;
    const int grX = meterX - 14 - grWidth;
    grLabel.setBounds (grX, top + 4, grWidth, 36);
    reductionMeter.setBounds (grX + (grWidth - grBarWidth) / 2, barsTop, grBarWidth, barsBottom - barsTop);
    grValue.setBounds (grX, barsBottom + 2, grWidth, (int) Songsmith::UI::OutputMeterDisplay::bottomRow - 4);

    // The graph fills the rest.
    const juce::Rectangle<int> graphArea (16, top, grX - 12 - 16, height);
    graph.setBounds (graphArea);
    pauseButton.setBounds (graphArea.getRight() - 70, graphArea.getY() + 6, 64, 24);

    // The floating panel: fixed size, kept whole inside the graph, and back where the musician left it.
    panel.setSize (panelW, panelH);
    panel.setLimits (graphArea.reduced (8));
    panel.setRelativePosition ({ proc.panelX.load(), proc.panelY.load() });

    const int knobTop = 12 + labelH;
    int x = panelPad;
    auto place = [&] (juce::Label& label, juce::Slider& knob)
    {
        label.setBounds (x, 12, knobColumn, labelH);
        knob.setBounds (x + 3, knobTop, knobColumn - 6, knobH);
        x += knobColumn;
    };
    place (thresholdLabel, thresholdSlider);
    place (ratioLabel, ratioSlider);
    place (attackLabel, attackSlider);
    place (releaseLabel, releaseSlider);
    place (kneeLabel, kneeSlider);
    place (makeupLabel, makeupSlider);
    place (mixLabel, mixSlider);

    // Auto Makeup sits under Makeup Gain, the knob it switches off.
    const int buttonTop = knobTop + knobH + panelGap;
    autoMakeupButton.setBounds (panelPad + knobColumn * 5 - 10, buttonTop, 112, buttonH);
    warmthButton.setBounds (panelPad, buttonTop, 130, buttonH);

    diagnostics->setBounds (0, designH, designW, Comp::EditorLayout::footerHeight);
}

void SongsmithCompressorAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    const auto area = juce::Rectangle<float> ((float) designW, (float) Comp::EditorLayout::designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, area);
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, area.withHeight (Songsmith::UI::titleBarHeight));
    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, CompColours::text, CompColours::accent);
}

//==============================================================================
void SongsmithCompressorAudioProcessorEditor::syncFromParameters()
{
    // Makeup Gain does nothing while Auto Makeup is on, so it is greyed out (its tooltip says why).
    makeupSlider.setEnabled (! autoMakeupButton.getToggleState());
    graph.setThresholdDb (proc.getEngineParams().thresholdDb);
}

void SongsmithCompressorAudioProcessorEditor::updateMeters (double elapsedSeconds)
{
    // The graph: every column the audio thread has finished since the last call, oldest first.
    Comp::GraphColumn columns[256];
    for (int n; (n = proc.graphFifo.popAll (columns, 256)) > 0;)
        for (int i = 0; i < n; ++i)
            graph.push (columns[i]);
    graph.refreshIfChanged();

    // The Gain Reduction bar and its number (a positive amount: how much the level is turned down).
    reductionMeter.push (SongsmithCompressorAudioProcessor::readAndReset (proc.gainReductionDb), elapsedSeconds);
    grValue.setText (juce::String (reductionMeter.getBallistics().getLevel(), 1) + " dB", juce::dontSendNotification);

    // The output meter, exactly as the Songsmith EQ feeds it.
    {
        auto& m = proc.outputMeter;
        m.readAndReset();   // the all-channel peak isn't shown; the per-channel ones are
        m.clipped.store (false, std::memory_order_relaxed);
        outputMeter.update (m.readAndResetChannel (0), m.readAndResetChannel (1),
                            m.channels.load (std::memory_order_relaxed),
                            m.clipLatched.load (std::memory_order_relaxed),
                            juce::Time::getMillisecondCounterHiRes() * 0.001, elapsedSeconds);
    }
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
