#include "PluginEditor.h"

namespace
{
    constexpr int row1Height = 42, rowHeight = 34;   // three-row top strip
    constexpr int margin       = 12;
    constexpr float ranges[]   = { 12.0f, 24.0f, 30.0f };
    constexpr float tilts[]    = { 0.0f, 3.0f, 4.5f, 6.0f };                 // analyser tilt, dB/oct
    constexpr float tops[]     = { 6.0f, 0.0f, -12.0f }, bottoms[] = { -60.0f, -90.0f, -120.0f };   // analyser range, dBFS
}

SongsmithEQAudioProcessorEditor::SongsmithEQAudioProcessorEditor (SongsmithEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    content.onPaint  = [this] (juce::Graphics& g) { paintContent (g); };
    content.onResize = [this] { layoutContent(); };
    addAndMakeVisible (content);

    content.addAndMakeVisible (display);
    content.addAndMakeVisible (presetBar);

    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y / Ctrl+Shift+Z)");
    undoButton.onClick = [this] { processor.edits.undo(); };
    redoButton.onClick = [this] { processor.edits.redo(); };
    content.addAndMakeVisible (undoButton);
    content.addAndMakeVisible (redoButton);

    for (int i = 0; i < 3; ++i)
        rangeBox.addItem (juce::String::charToString (0x00b1) + juce::String ((int) ranges[i]) + " dB", i + 1);
    rangeBox.setTooltip ("Visible gain range");
    rangeBox.onChange = [this]
    {
        const int i = rangeBox.getSelectedItemIndex();
        if (i >= 0)
        {
            processor.displayRangeDb.store (ranges[i]);
            processor.notifyNonParameterStateChanged();
        }
    };
    content.addAndMakeVisible (rangeBox);

    analyserBox.addItemList ({ "Analyser off", "Analyser: before EQ", "Analyser: after EQ" }, 1);
    analyserBox.setTooltip ("Spectrum analyser: show the input (before EQ) or the output (after EQ)");
    analyserBox.onChange = [this]
    {
        const int i = analyserBox.getSelectedItemIndex();
        if (i >= 0)
        {
            processor.analyser.mode.store (i);
            processor.analyser.clear();
            processor.notifyNonParameterStateChanged();
            display.repaint();
        }
    };
    content.addAndMakeVisible (analyserBox);

    curveViewBox.addItemList ({ "Curve: all bands", "Curve: Left", "Curve: Right", "Curve: Mid", "Curve: Side" }, 1);
    curveViewBox.setTooltip ("Which channel the combined curve shows: Stereo bands plus the bands set to that channel");
    curveViewBox.onChange = [this]
    {
        const int i = curveViewBox.getSelectedItemIndex();
        if (i >= 0)
        {
            processor.curveView.store (i);
            processor.notifyNonParameterStateChanged();
        }
    };
    content.addAndMakeVisible (curveViewBox);

    // Phase mode + linear-phase quality in one selector (two host parameters).
    phaseBox.addItemList ({ "Zero latency", "Linear phase: low", "Linear phase: medium", "Linear phase: high" }, 1);
    phaseBox.setTooltip (EQ::linearPhaseTooltip());
    phaseBox.onChange = [this]
    {
        const int i = phaseBox.getSelectedItemIndex();
        if (i < 0)
            return;

        auto set = [] (juce::RangedAudioParameter* prm, float value)
        {
            if (prm->convertFrom0to1 (prm->getValue()) == value)
                return;
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        };
        if (i > 0)
            set (processor.apvts.getParameter (EQ::lpQualityId), (float) (i - 1));
        set (processor.apvts.getParameter (EQ::phaseModeId), i > 0 ? 1.0f : 0.0f);
    };
    content.addAndMakeVisible (phaseBox);

    oversamplingBox.addItemList ({ "Oversampling: off",
                                   "Oversampling: 2" + juce::String::charToString (0x00d7),    // multiplication sign
                                   "Oversampling: 4" + juce::String::charToString (0x00d7) }, 1);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, EQ::oversamplingId, oversamplingBox);
    oversamplingBox.setTooltip ("Oversampling: keeps high-frequency bells and shelves from 'cramping' near Nyquist "
                                "at 44.1/48 kHz. Adds a few samples of latency in zero-latency mode; in linear-phase "
                                "mode it only changes the design (no extra latency).");
    content.addAndMakeVisible (oversamplingBox);

    analyserSettingsButton.setTooltip ("Settings: analyser (freeze, peak hold, tilt, resolution, speed, range) and interface size");
    analyserSettingsButton.onClick = [this] { showAnalyserMenu(); };
    content.addAndMakeVisible (analyserSettingsButton);

    sidechainBox.addItemList ({ "Side Chain: off", "Side Chain: spectrum", "Side Chain: spectrum and overlap" }, 1);
    sidechainBox.setTooltip ("Show the side chain input's spectrum (orange). \"Spectrum and overlap\" also shades frequencies where "
                             "both tracks are above -60 dBFS and within 6 dB of each other (needs the analyser on)");
    sidechainBox.onChange = [this]
    {
        const int i = sidechainBox.getSelectedItemIndex();
        if (i >= 0)
        {
            processor.sidechainView.store (i);
            processor.sidechainAnalyser.clear();
            processor.notifyNonParameterStateChanged();
            display.repaint();
        }
    };
    content.addChildComponent (sidechainBox);

    // Output gain (bar slider, double-click resets to 0 dB) and global bypass.
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, EQ::outputGainId, outputSlider);
    outputSlider.textFromValueFunction = [] (double db)
    {
        return "Output " + juce::String (db > 0.04 ? "+" : "") + juce::String (std::abs (db) < 0.05 ? 0.0 : db, 1) + " dB";
    };
    outputSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        return parseOutputGainText (t);
    };
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    outputSlider.setTextBoxIsEditable (true);
    outputSlider.setColour (juce::Slider::trackColourId, EQColours::totalCurve);
    outputSlider.setColour (juce::Slider::textBoxTextColourId, EQColours::text);
    outputSlider.setTooltip ("Output gain (double-click: 0 dB)");
    outputSlider.updateText();
    content.addAndMakeVisible (outputSlider);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, EQ::bypassId, bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass the whole EQ (smooth cross-fade)");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

    content.addAndMakeVisible (outputMeter);

    // Scale the whole curve (double-click: 100 %).
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, EQ::gainScaleId, scaleSlider);
    scaleSlider.textFromValueFunction = [] (double pct) { return "Scale " + juce::String (juce::roundToInt (pct)) + "%"; };
    scaleSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        return t.retainCharacters ("-0123456789.").getDoubleValue();
    };
    scaleSlider.setDoubleClickReturnValue (true, 100.0);
    scaleSlider.setTextBoxIsEditable (true);
    scaleSlider.setColour (juce::Slider::trackColourId, EQColours::band (3));
    scaleSlider.setColour (juce::Slider::textBoxTextColourId, EQColours::text);
    scaleSlider.setTooltip ("Scale every band's gain together: 0 % = flat, 100 % = as set, negative inverts "
                            "(cuts and notches are unaffected). Double-click: 100 %");
    scaleSlider.updateText();
    content.addAndMakeVisible (scaleSlider);

    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, EQ::autoGainId, autoGainButton);
    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setTooltip ("Auto gain: match the output loudness to the input (measured like a loudness meter over 2 s, up to 12 dB either way), "
                               "so a boost isn't judged better just because it's louder. Applied before the output gain.");
    autoGainButton.setColour (juce::TextButton::buttonOnColourId, EQColours::totalCurve.withAlpha (0.85f));
    autoGainButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    content.addAndMakeVisible (autoGainButton);

    // Piano strip on/off (task 28). Same setting as "Show piano strip" in the graph's right-click
    // menu; the timer keeps the two in step.
    pianoButton.setClickingTogglesState (true);
    pianoButton.setToggleState (processor.showPianoStrip.load(), juce::dontSendNotification);
    pianoButton.setTooltip ("Piano strip: a keyboard along the bottom of the graph, on the frequency axis, with each octave "
                            "labelled and the key nearest the selected band lit. Hold Ctrl (Cmd on macOS) while dragging "
                            "a band to snap it to semitones.");
    pianoButton.setColour (juce::TextButton::buttonOnColourId, EQColours::totalCurve.withAlpha (0.85f));
    pianoButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    pianoButton.onClick = [this]
    {
        processor.showPianoStrip.store (pianoButton.getToggleState());
        processor.notifyNonParameterStateChanged();
        display.repaint();
    };
    // The button lives at the right end of the footer strip, beside the piano-note strip it shows (EQ-35).

    bandCountLabel.setFont (EQColours::numbers (EQColours::Type::control));
    bandCountLabel.setJustificationType (juce::Justification::centredLeft);
    content.addAndMakeVisible (bandCountLabel);

    hintLabel.setText ("Double-click: add or reset a band (near the edges: a cut; Ctrl: always a Bell).  Drag: frequency and gain (Shift: fine, Ctrl: snap to notes).  "
                       "Mouse wheel: Q.  Alt-click: solo.  Right-click: menu.  Delete key: remove.",
                       juce::dontSendNotification);
    hintLabel.setFont (EQColours::font (EQColours::Type::hint));
    hintLabel.setJustificationType (juce::Justification::centredRight);
    hintLabel.setColour (juce::Label::textColourId, EQColours::label);   // full colour: 4.5:1 contrast
    content.addAndMakeVisible (hintLabel);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (processor, processor.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);
    diagnostics->setTrailingControl (&pianoButton);

    timerCallback();

    // Read the saved size first: setting the limits may call resized(), which overwrites it.
    const int savedWidth = processor.editorWidth.load(), savedHeight = processor.editorHeight.load();
    setConstrainer (&constrainer);
    updateResizeLimits();
    setResizable (true, true);
    const auto size = EQ::EditorSize::constrain (savedWidth, savedHeight, processor.uiScale.load());
    setSize (size.x, size.y);
    startTimerHz (30);
}

SongsmithEQAudioProcessorEditor::~SongsmithEQAudioProcessorEditor()
{
    processor.soloBand.store (-1);   // solo is an editing aid: never leave it on without a UI
    diagnostics.reset();             // also stops the audio thread timing the blocks

    setLookAndFeel (nullptr);
}

void SongsmithEQAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double elapsed = lastMeterTime > 0.0 ? juce::jlimit (0.0, 0.5, now - lastMeterTime) : 0.0;
    lastMeterTime = now;
    outputMeter.update (processor.outputMeter.readAndReset(),
                        processor.outputMeter.clipped.exchange (false, std::memory_order_relaxed),
                        now, elapsed);

    auto& um = processor.edits.getUndoManager();
    undoButton.setEnabled (um.canUndo());
    redoButton.setEnabled (um.canRedo());
    undoButton.setTooltip (um.canUndo() ? "Undo " + um.getUndoDescription().toLowerCase() + " (Ctrl+Z)" : "Undo (Ctrl+Z)");
    redoButton.setTooltip (um.canRedo() ? "Redo " + um.getRedoDescription().toLowerCase() + " (Ctrl+Y)" : "Redo (Ctrl+Y)");

    presetBar.refresh();

    {
        juce::String agText ("Auto Gain");
        if (processor.isAutoGainOn())
        {
            const float db = processor.autoGainAppliedDb.load();
            agText << " " << (db > 0.05f ? "+" : "") << juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) << " dB";
        }
        if (autoGainButton.getButtonText() != agText)
            autoGainButton.setButtonText (agText);
    }

    bandCountLabel.setText (juce::String (processor.getNumUsedBands()) + " / " + juce::String (EQ::numBands) + " bands",
                            juce::dontSendNotification);

    const float range = processor.displayRangeDb.load();
    for (int i = 0; i < 3; ++i)
        if (std::abs (ranges[i] - range) < 0.5f && rangeBox.getSelectedItemIndex() != i)
            rangeBox.setSelectedItemIndex (i, juce::dontSendNotification);

    {
        const int phaseIndex = processor.isLinearPhaseSelected() ? 1 + processor.getLinearPhaseQuality() : 0;
        if (phaseBox.getSelectedItemIndex() != phaseIndex)
            phaseBox.setSelectedItemIndex (phaseIndex, juce::dontSendNotification);
    }

    if (curveViewBox.getSelectedItemIndex() != processor.curveView.load())
        curveViewBox.setSelectedItemIndex (processor.curveView.load(), juce::dontSendNotification);
    curveViewBox.setVisible (processor.getMainBusNumInputChannels() >= 2);

    // Sidechain selector: only while the sidechain bus is connected and receiving signal
    // (anything above -80 dBFS in the last 3 s).
    if (processor.sidechainMeter.readAndReset() > 1.0e-4f)
        lastSidechainSignal = now;
    processor.sidechainMeter.clipped.store (false);
    sidechainBox.setVisible (processor.isSidechainConnected() && now - lastSidechainSignal < 3.0);
    if (sidechainBox.getSelectedItemIndex() != processor.sidechainView.load())
        sidechainBox.setSelectedItemIndex (processor.sidechainView.load(), juce::dontSendNotification);

    if (analyserBox.getSelectedItemIndex() != processor.analyser.mode.load())
        analyserBox.setSelectedItemIndex (processor.analyser.mode.load(), juce::dontSendNotification);

    if (pianoButton.getToggleState() != processor.showPianoStrip.load())   // changed from the graph's menu or a session
        pianoButton.setToggleState (processor.showPianoStrip.load(), juce::dontSendNotification);
}

juce::PopupMenu SongsmithEQAudioProcessorEditor::createAnalyserMenu()
{
    auto& an = processor.analyser;
    juce::PopupMenu menu;

    menu.addItem (1, "Freeze", true, an.isFrozen());
    menu.addItem (2, "Peak hold", true, an.isPeakHold());
    menu.addItem (3, "Reset peaks (or click the graph)", an.isPeakHold());

    juce::PopupMenu tilt, resolution, speed, top, bottom;
    for (int i = 0; i < 4; ++i)
        tilt.addItem (10 + i, juce::String (tilts[i], i == 2 ? 1 : 0) + " dB/oct", true,
                      std::abs (processor.analyserTilt.load() - tilts[i]) < 0.01f);

    for (int order = SpectrumAnalyser::minFftOrder; order <= SpectrumAnalyser::fftOrder; ++order)
        resolution.addItem (20 + order, juce::StringArray { "Low", "Medium", "High" }[order - SpectrumAnalyser::minFftOrder]
                                            + " (" + juce::String (1 << order) + " points)", true, an.getFftOrder() == order);

    const char* speeds[] = { "Slow", "Medium", "Fast" };
    for (int i = 0; i < 3; ++i)
        speed.addItem (40 + i, speeds[i], true, an.getSpeed() == i);

    for (int i = 0; i < 3; ++i)
    {
        top.addItem (50 + i, juce::String ((int) tops[i]) + " dBFS", true, processor.analyserTopDb.load() == tops[i]);
        bottom.addItem (60 + i, juce::String ((int) bottoms[i]) + " dBFS", true, processor.analyserBottomDb.load() == bottoms[i]);
    }

    menu.addSeparator();
    menu.addSubMenu ("Tilt", tilt);
    menu.addSubMenu ("Resolution", resolution);
    menu.addSubMenu ("Speed", speed);
    menu.addSubMenu ("Range top", top);
    menu.addSubMenu ("Range floor", bottom);

    juce::PopupMenu uiSize;
    for (int i = 0; i < EQ::EditorSize::numUiScales; ++i)
        uiSize.addItem (70 + i, juce::String (juce::roundToInt (EQ::EditorSize::uiScales[i] * 100.0f)) + "%", true,
                        std::abs (processor.uiScale.load() - EQ::EditorSize::uiScales[i]) < 0.01f);
    menu.addSeparator();
    menu.addSubMenu ("Interface size", uiSize);   // "UI size" in the task; spelled out per task 24

    return menu;
}

void SongsmithEQAudioProcessorEditor::showAnalyserMenu()
{
    auto menu = createAnalyserMenu();
    juce::Component::SafePointer<SongsmithEQAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&analyserSettingsButton),
                        [safe] (int r)
                        {
                            if (safe == nullptr || r == 0)
                                return;

                            auto& p = safe->processor;
                            auto& a = p.analyser;

                            if (r == 1)                 { a.setFrozen (! a.isFrozen()); p.sidechainAnalyser.setFrozen (a.isFrozen()); }
                            else if (r == 2)            a.setPeakHold (! a.isPeakHold());
                            else if (r == 3)            a.resetPeaks();
                            else if (r >= 10 && r < 14) p.analyserTilt.store (tilts[r - 10]);
                            else if (r >= 20 && r < 40) p.setAnalyserFftOrder (r - 20);
                            else if (r >= 40 && r < 43) p.setAnalyserSpeed (r - 40);
                            else if (r >= 50 && r < 53) p.analyserTopDb.store (tops[r - 50]);
                            else if (r >= 60 && r < 63) p.analyserBottomDb.store (bottoms[r - 60]);
                            else if (r >= 70 && r < 70 + EQ::EditorSize::numUiScales)
                            {
                                safe->setUiScale (EQ::EditorSize::uiScales[r - 70]);   // saves and notifies itself
                                return;
                            }

                            if (r != 1)   // freeze is transient and not saved
                                p.notifyNonParameterStateChanged();
                            safe->display.repaint();
                        });
}

bool SongsmithEQAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (mods.isCommandDown() && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown()) processor.edits.redo();
        else                    processor.edits.undo();
        return true;
    }

    if (mods.isCommandDown() && (code == 'Y' || code == 'y'))
    {
        processor.edits.redo();
        return true;
    }

    return false;
}

void SongsmithEQAudioProcessorEditor::paintContent (juce::Graphics& g)
{
    const auto area = content.getLocalBounds().toFloat();
    Songsmith::UI::LookAndFeelBase::drawPanel (g, area);
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, area.withHeight ((float) row1Height));

    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, EQColours::text, EQColours::accent);
}

double SongsmithEQAudioProcessorEditor::parseOutputGainText (const juce::String& text)
{
    // "Output +1.5 dB", "+1.5", "-3 dB" or "2": the number, whatever words surround it.
    return text.retainCharacters ("+-0123456789.").getDoubleValue();
}

namespace
{
    float textWidth (const juce::String& text, const juce::FontOptions& font)
    {
        return juce::GlyphArrangement::getStringWidth (juce::Font (font), text);
    }

    // Wide enough for the longest item, plus the arrow and the label's insets.
    int comboWidth (const juce::ComboBox& box)
    {
        float w = 0.0f;
        for (int i = 0; i < box.getNumItems(); ++i)
            w = juce::jmax (w, textWidth (box.getItemText (i), EQColours::font (EQColours::Type::heading)));
        return (int) std::ceil (w) + 44;
    }

    int buttonWidth (const juce::StringArray& texts)
    {
        float w = 0.0f;
        for (auto& t : texts)
            w = juce::jmax (w, textWidth (t, EQColours::font (EQColours::Type::heading, EQColours::Weight::Medium)));
        return (int) std::ceil (w) + 24;
    }

    int barWidth (const juce::StringArray& texts)
    {
        float w = 0.0f;
        for (auto& t : texts)
            w = juce::jmax (w, textWidth (t, EQColours::numbers (EQColours::Type::heading)));
        return (int) std::ceil (w) + 20;
    }
}

void SongsmithEQAudioProcessorEditor::layoutContent()
{

    // Three rows, so every label fits spelled out at the minimum width (760 px):
    //   1: title | presets | undo | redo | scale ............ band count | bypass
    //   2: phase mode | oversampling ............... auto gain | output gain
    //   graph ..................................................... | output meter
    //   3: display range | analyser | settings | curve view | side chain ...... hint
    //   footer: Processing Load | CPU and latency (when on) ...... piano
    // Widths come from the text in the embedded font, so nothing is cut short. Optional
    // items (band count, hint) are hidden when there's no room for them.
    auto area = content.getLocalBounds();
    constexpr int gap = 8;

    const auto strip = area.removeFromBottom (EQ::EditorSize::diagnosticsHeight);
    pianoButton.setSize (buttonWidth ({ pianoButton.getButtonText() }), 22);
    if (diagnostics != nullptr)
    {
        diagnostics->setBounds (strip);
        diagnostics->resized();   // places the Piano button at the right end
    }

    {
        auto row = area.removeFromTop (row1Height).reduced (margin, 7);
        titleBounds = row.removeFromLeft ((int) std::ceil (Songsmith::UI::titleWidth (JucePlugin_Name)) + 4);
        row.removeFromLeft (16);
        presetBar.setBounds (row.removeFromLeft (220));
        row.removeFromLeft (gap + 4);
        undoButton.setBounds (row.removeFromLeft (buttonWidth ({ "Undo", "Redo" })));
        row.removeFromLeft (6);
        redoButton.setBounds (row.removeFromLeft (buttonWidth ({ "Undo", "Redo" })));
        row.removeFromLeft (gap + 4);
        scaleSlider.setBounds (row.removeFromLeft (barWidth ({ "Scale -100%", "Scale 200%" })));

        bypassButton.setBounds (row.removeFromRight (juce::jmax (84, buttonWidth ({ "Bypass" }))));   // the same size in every plugin (SUI-13)
        row.removeFromRight (gap + 4);
        const int countWidth = (int) std::ceil (textWidth ("24 / 24 bands", EQColours::numbers (EQColours::Type::control))) + 8;
        bandCountLabel.setVisible (row.getWidth() >= countWidth + gap);
        bandCountLabel.setBounds (row.removeFromRight (juce::jmin (countWidth, row.getWidth())));
    }

    {
        auto row = area.removeFromTop (rowHeight).reduced (margin, 3);
        phaseBox.setBounds (row.removeFromLeft (comboWidth (phaseBox)));
        row.removeFromLeft (gap);
        oversamplingBox.setBounds (row.removeFromLeft (comboWidth (oversamplingBox)));

        outputSlider.setBounds (row.removeFromRight (barWidth ({ "Output -24.0 dB", "Output +24.0 dB" })));
        row.removeFromRight (gap);
        autoGainButton.setBounds (row.removeFromRight (buttonWidth ({ "Auto Gain -12.0 dB", "Auto Gain +12.0 dB" })));
    }

    {
        auto row = area.removeFromTop (rowHeight).reduced (margin, 3).withTrimmedBottom (2);
        rangeBox.setBounds (row.removeFromLeft (comboWidth (rangeBox)));
        row.removeFromLeft (gap);
        analyserBox.setBounds (row.removeFromLeft (comboWidth (analyserBox)));
        row.removeFromLeft (4);
        analyserSettingsButton.setBounds (row.removeFromLeft (34));
        row.removeFromLeft (gap);
        curveViewBox.setBounds (row.removeFromLeft (comboWidth (curveViewBox)));
        row.removeFromLeft (gap);
        sidechainBox.setBounds (row.removeFromLeft (comboWidth (sidechainBox)));
        row.removeFromLeft (gap);
        row.removeFromLeft (gap);

        const int hintWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (hintLabel.getFont(), hintLabel.getText()))
                              + hintLabel.getBorderSize().getLeftAndRight() + 2;
        hintLabel.setVisible (row.getWidth() >= hintWidth);
        hintLabel.setBounds (row.withLeft (juce::jmax (row.getX(), row.getRight() - hintWidth)));
    }

    // The graph, with the output meter in a full-height strip on its right (task 27).
    auto graph = area.reduced (margin, 0).withTrimmedBottom (margin);
    outputMeter.setBounds (graph.removeFromRight (OutputMeterDisplay::preferredWidth));
    graph.removeFromRight (6);
    display.setBounds (graph);
}

//==============================================================================
void SongsmithEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (EQColours::background);   // under the content (rounding can leave a pixel)
}

void SongsmithEQAudioProcessorEditor::resized()
{
    processor.editorWidth.store (getWidth());
    processor.editorHeight.store (getHeight());

    // Layout step: on large windows, controls and text step up in size too (in 1/8 steps, so
    // they don't creep while dragging), but more slowly than the window, so the graph gets
    // most of the extra room. 1 up to 1.25x the default window, at most 1.75 (a 4K window
    // at 100 %).
    const float uiScale = processor.uiScale.load();
    const float ratio = juce::jmin ((float) getWidth()  / ((float) EQ::EditorSize::defaultWidth  * uiScale),
                                    (float) getHeight() / ((float) EQ::EditorSize::defaultHeight * uiScale));
    layoutStep = juce::jlimit (1.0f, 1.75f, std::floor ((1.0f + (ratio - 1.0f) * 0.5f) * 8.0f) / 8.0f);
    contentScale = uiScale * layoutStep;

    content.setTransform (juce::AffineTransform::scale (contentScale));
    content.setBounds (0, 0, juce::roundToInt ((float) getWidth() / contentScale),
                             juce::roundToInt ((float) getHeight() / contentScale));
}

void SongsmithEQAudioProcessorEditor::updateResizeLimits()
{
    const float s = processor.uiScale.load();
    constrainer.uiScale = s;
    constrainer.setSizeLimits (juce::jmin (EQ::EditorSize::maxWidth,  juce::roundToInt ((float) EQ::EditorSize::minWidth  * s)),
                               juce::jmin (EQ::EditorSize::maxHeight, juce::roundToInt ((float) EQ::EditorSize::minHeight * s)),
                               EQ::EditorSize::maxWidth, EQ::EditorSize::maxHeight);
}

void SongsmithEQAudioProcessorEditor::Constrainer::checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous,
                                                                const juce::Rectangle<int>& limits,
                                                                bool top, bool left, bool bottom, bool right)
{
    ComponentBoundsConstrainer::checkBounds (bounds, previous, limits, top, left, bottom, right);

    // Then the aspect-ratio range, keeping the edge that isn't being dragged where it is.
    const auto size = EQ::EditorSize::constrain (bounds.getWidth(), bounds.getHeight(), uiScale);
    if (left) bounds.setLeft (bounds.getRight() - size.x); else bounds.setWidth (size.x);
    if (top)  bounds.setTop (bounds.getBottom() - size.y); else bounds.setHeight (size.y);
}

void SongsmithEQAudioProcessorEditor::setUiScale (float scale)
{
    const float before = processor.uiScale.load();
    processor.setUiScale (scale);
    const float after = processor.uiScale.load();

    updateResizeLimits();

    // Same design size (same layout), so the window grows or shrinks with the UI size.
    const auto size = EQ::EditorSize::constrain (juce::roundToInt ((float) getWidth()  * after / before),
                                                 juce::roundToInt ((float) getHeight() * after / before), after);
    if (size.x != getWidth() || size.y != getHeight())
        setSize (size.x, size.y);
    else
        resized();
}
