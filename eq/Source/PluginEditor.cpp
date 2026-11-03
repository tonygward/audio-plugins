#include "PluginEditor.h"

namespace
{
    constexpr int row1Height = 40, row2Height = 32;   // two-row top strip
    constexpr int margin       = 12;
    constexpr float ranges[]   = { 12.0f, 24.0f, 30.0f };
}

ThreeBandEQAudioProcessorEditor::ThreeBandEQAudioProcessorEditor (ThreeBandEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (display);
    addAndMakeVisible (presetBar);

    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y / Ctrl+Shift+Z)");
    undoButton.onClick = [this] { processor.edits.undo(); };
    redoButton.onClick = [this] { processor.edits.redo(); };
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

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
    addAndMakeVisible (rangeBox);

    analyserBox.addItemList ({ "Analyser off", "Analyser: pre", "Analyser: post" }, 1);
    analyserBox.setTooltip ("Spectrum analyser: input (pre-EQ) or output (post-EQ)");
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
    addAndMakeVisible (analyserBox);

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
    addAndMakeVisible (curveViewBox);

    // Phase mode + linear-phase quality in one selector (two host parameters).
    phaseBox.addItemList ({ "Zero latency", "Linear: low", "Linear: medium", "Linear: high" }, 1);
    phaseBox.setTooltip ("Zero latency (minimum phase) or linear phase. Linear phase adds latency "
                         "(at 44.1/48 kHz: low 43-46 ms, medium 85-93 ms, high 171-186 ms) and "
                         "turns off dynamic EQ and solo.");
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
    addAndMakeVisible (phaseBox);

    analyserSettingsButton.setTooltip ("Analyser settings: freeze, peak hold, tilt, resolution, speed, range");
    analyserSettingsButton.onClick = [this] { showAnalyserMenu(); };
    addAndMakeVisible (analyserSettingsButton);

    sidechainBox.addItemList ({ "Sidechain: off", "Sidechain: spectrum", "Sidechain: + overlap" }, 1);
    sidechainBox.setTooltip ("Show the sidechain input's spectrum (orange). \"+ overlap\" shades frequencies where "
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
    addChildComponent (sidechainBox);

    // Output gain (bar slider, double-click resets to 0 dB) and global bypass.
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, EQ::outputGainId, outputSlider);
    outputSlider.textFromValueFunction = [] (double db)
    {
        return "Out " + juce::String (db > 0.04 ? "+" : "") + juce::String (std::abs (db) < 0.05 ? 0.0 : db, 1) + " dB";
    };
    outputSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        return t.trimCharactersAtStart ("Ou t").getDoubleValue();
    };
    outputSlider.setDoubleClickReturnValue (true, 0.0);
    outputSlider.setTextBoxIsEditable (true);
    outputSlider.setColour (juce::Slider::trackColourId, EQColours::totalCurve);
    outputSlider.setColour (juce::Slider::textBoxTextColourId, EQColours::text);
    outputSlider.setTooltip ("Output gain (double-click: 0 dB)");
    outputSlider.updateText();
    addAndMakeVisible (outputSlider);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, EQ::bypassId, bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass the whole EQ (smooth cross-fade)");
    bypassButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff9f43));
    bypassButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (bypassButton);

    addAndMakeVisible (outputMeter);

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
    addAndMakeVisible (scaleSlider);

    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, EQ::autoGainId, autoGainButton);
    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setTooltip ("Auto gain: match the output loudness to the input (K-weighted, 2 s window, +-12 dB), "
                               "so a boost isn't judged better just because it's louder. Applied before the output gain.");
    autoGainButton.setColour (juce::TextButton::buttonOnColourId, EQColours::totalCurve.withAlpha (0.85f));
    autoGainButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (autoGainButton);

    bandCountLabel.setFont (EQColours::font (12.0f));
    bandCountLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bandCountLabel);

    hintLabel.setText ("Dbl-click: add/reset  Drag: freq/gain (Shift fine, Ctrl note snap)  Wheel: Q  Alt-click: solo  Right-click: menu  Del: remove",
                       juce::dontSendNotification);
    hintLabel.setFont (EQColours::font (11.0f));
    hintLabel.setJustificationType (juce::Justification::centredRight);
    hintLabel.setColour (juce::Label::textColourId, EQColours::label.withAlpha (0.8f));
    addAndMakeVisible (hintLabel);

    timerCallback();

    // Read the saved size first: setResizeLimits() may call resized(), which overwrites it.
    const int savedWidth = processor.editorWidth.load(), savedHeight = processor.editorHeight.load();
    setResizable (true, true);
    setResizeLimits (760, 440, 2000, 1200);
    setSize (savedWidth, savedHeight);
    startTimerHz (30);
}

ThreeBandEQAudioProcessorEditor::~ThreeBandEQAudioProcessorEditor()
{
    processor.soloBand.store (-1);   // solo is an editing aid: never leave it on without a UI

    setLookAndFeel (nullptr);
}

void ThreeBandEQAudioProcessorEditor::timerCallback()
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
        juce::String agText ("AG");
        if (processor.isAutoGainOn())
        {
            const float db = processor.autoGainAppliedDb.load();
            agText << " " << (db > 0.05f ? "+" : "") << juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1);
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
}

void ThreeBandEQAudioProcessorEditor::showAnalyserMenu()
{
    auto& an = processor.analyser;
    juce::PopupMenu menu;

    menu.addItem (1, "Freeze", true, an.isFrozen());
    menu.addItem (2, "Peak hold", true, an.isPeakHold());
    menu.addItem (3, "Reset peaks (or click the graph)", an.isPeakHold());

    juce::PopupMenu tilt, resolution, speed, top, bottom;
    const float tilts[] = { 0.0f, 3.0f, 4.5f, 6.0f };
    for (int i = 0; i < 4; ++i)
        tilt.addItem (10 + i, juce::String (tilts[i], i == 2 ? 1 : 0) + " dB/oct", true,
                      std::abs (processor.analyserTilt.load() - tilts[i]) < 0.01f);

    for (int order = SpectrumAnalyser::minFftOrder; order <= SpectrumAnalyser::fftOrder; ++order)
        resolution.addItem (20 + order, juce::String (1 << order) + "-point FFT", true, an.getFftOrder() == order);

    const char* speeds[] = { "Slow", "Medium", "Fast" };
    for (int i = 0; i < 3; ++i)
        speed.addItem (40 + i, speeds[i], true, an.getSpeed() == i);

    const float tops[] = { 6.0f, 0.0f, -12.0f }, bottoms[] = { -60.0f, -90.0f, -120.0f };
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

    juce::Component::SafePointer<ThreeBandEQAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&analyserSettingsButton),
                        [safe, tilts, tops, bottoms] (int r)
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

                            if (r != 1)   // freeze is transient and not saved
                                p.notifyNonParameterStateChanged();
                            safe->display.repaint();
                        });
}

bool ThreeBandEQAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
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

void ThreeBandEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (EQColours::background);

    auto header = getLocalBounds().removeFromTop (row1Height).reduced (margin, 0);
    g.setColour (EQColours::text);
    g.setFont (EQColours::font (17.0f, true));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), header, juce::Justification::centredLeft);
}

void ThreeBandEQAudioProcessorEditor::resized()
{
    processor.editorWidth.store (getWidth());
    processor.editorHeight.store (getHeight());

    auto area = getLocalBounds();

    // Row 1: title | presets | undo/redo | curve scale | phase mode | band count | hint
    {
        auto row = area.removeFromTop (row1Height).reduced (margin, 7);
        row.removeFromLeft (112);   // title (painted)
        presetBar.setBounds (row.removeFromLeft (240));
        row.removeFromLeft (12);
        undoButton.setBounds (row.removeFromLeft (56));
        row.removeFromLeft (6);
        redoButton.setBounds (row.removeFromLeft (56));
        row.removeFromLeft (12);
        scaleSlider.setBounds (row.removeFromLeft (100));
        row.removeFromLeft (8);
        phaseBox.setBounds (row.removeFromLeft (118));
        row.removeFromLeft (12);

        bandCountLabel.setVisible (row.getWidth() >= 90);
        bandCountLabel.setBounds (row.removeFromLeft (juce::jmin (100, row.getWidth())));
        row.removeFromLeft (8);
        hintLabel.setVisible (row.getWidth() >= 220);
        hintLabel.setBounds (row);
    }

    // Row 2: display range | analyser | settings | curve view | sidechain ...
    //        ... auto gain | output gain | meter | bypass (right-aligned). Fits 760 px.
    {
        auto row = area.removeFromTop (row2Height).reduced (margin, 4).withTrimmedBottom (2);
        rangeBox.setBounds (row.removeFromLeft (76));
        row.removeFromLeft (6);
        analyserBox.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (4);
        analyserSettingsButton.setBounds (row.removeFromLeft (28));
        row.removeFromLeft (6);
        curveViewBox.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (6);
        sidechainBox.setBounds (row.removeFromLeft (116));

        bypassButton.setBounds (row.removeFromRight (56));
        row.removeFromRight (6);
        outputMeter.setBounds (row.removeFromRight (52));
        row.removeFromRight (6);
        outputSlider.setBounds (row.removeFromRight (92));
        row.removeFromRight (6);
        autoGainButton.setBounds (row.removeFromRight (64));
    }

    display.setBounds (area.reduced (margin, 0).withTrimmedBottom (margin));
}
