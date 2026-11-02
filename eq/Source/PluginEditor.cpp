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

    bandCountLabel.setFont (EQColours::font (12.0f));
    bandCountLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bandCountLabel);

    hintLabel.setText ("Dbl-click: add/reset  Drag: freq/gain (Shift fine)  Wheel: Q  Alt-click: solo  Right-click: menu  Del: remove",
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

    bandCountLabel.setText (juce::String (processor.getNumUsedBands()) + " / " + juce::String (EQ::numBands) + " bands",
                            juce::dontSendNotification);

    const float range = processor.displayRangeDb.load();
    for (int i = 0; i < 3; ++i)
        if (std::abs (ranges[i] - range) < 0.5f && rangeBox.getSelectedItemIndex() != i)
            rangeBox.setSelectedItemIndex (i, juce::dontSendNotification);

    if (curveViewBox.getSelectedItemIndex() != processor.curveView.load())
        curveViewBox.setSelectedItemIndex (processor.curveView.load(), juce::dontSendNotification);
    curveViewBox.setVisible (processor.getTotalNumInputChannels() >= 2);

    if (analyserBox.getSelectedItemIndex() != processor.analyser.mode.load())
        analyserBox.setSelectedItemIndex (processor.analyser.mode.load(), juce::dontSendNotification);
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
    g.setFont (EQColours::font (19.0f, true));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), header, juce::Justification::centredLeft);
}

void ThreeBandEQAudioProcessorEditor::resized()
{
    processor.editorWidth.store (getWidth());
    processor.editorHeight.store (getHeight());

    auto area = getLocalBounds();

    // Row 1: title | presets | undo/redo | band count | hint
    {
        auto row = area.removeFromTop (row1Height).reduced (margin, 7);
        row.removeFromLeft (130);   // title (painted)
        presetBar.setBounds (row.removeFromLeft (300));
        row.removeFromLeft (12);
        undoButton.setBounds (row.removeFromLeft (56));
        row.removeFromLeft (6);
        redoButton.setBounds (row.removeFromLeft (56));
        row.removeFromLeft (12);

        bandCountLabel.setVisible (row.getWidth() >= 90);
        bandCountLabel.setBounds (row.removeFromLeft (juce::jmin (100, row.getWidth())));
        row.removeFromLeft (8);
        hintLabel.setVisible (row.getWidth() >= 220);
        hintLabel.setBounds (row);
    }

    // Row 2: display range | analyser ...   output gain | meter | bypass (right-aligned)
    {
        auto row = area.removeFromTop (row2Height).reduced (margin, 4).withTrimmedBottom (2);
        rangeBox.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (6);
        analyserBox.setBounds (row.removeFromLeft (132));
        row.removeFromLeft (6);
        curveViewBox.setBounds (row.removeFromLeft (132));

        bypassButton.setBounds (row.removeFromRight (62));
        row.removeFromRight (6);
        outputMeter.setBounds (row.removeFromRight (64));
        row.removeFromRight (6);
        outputSlider.setBounds (row.removeFromRight (104));
    }

    display.setBounds (area.reduced (margin, 0).withTrimmedBottom (margin));
}
