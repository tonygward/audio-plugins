#include "PluginEditor.h"

namespace
{
    constexpr int headerHeight = 44;
    constexpr int margin       = 12;
    constexpr float ranges[]   = { 12.0f, 24.0f, 30.0f };
}

ThreeBandEQAudioProcessorEditor::ThreeBandEQAudioProcessorEditor (ThreeBandEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (display);

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

    bandCountLabel.setFont (EQColours::font (12.0f));
    bandCountLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bandCountLabel);

    hintLabel.setText ("Dbl-click: add/reset  Drag: freq/gain (Shift fine)  Wheel: Q  Right-click: menu  Del: remove",
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
    startTimerHz (10);
}

ThreeBandEQAudioProcessorEditor::~ThreeBandEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ThreeBandEQAudioProcessorEditor::timerCallback()
{
    auto& um = processor.edits.getUndoManager();
    undoButton.setEnabled (um.canUndo());
    redoButton.setEnabled (um.canRedo());
    undoButton.setTooltip (um.canUndo() ? "Undo " + um.getUndoDescription().toLowerCase() + " (Ctrl+Z)" : "Undo (Ctrl+Z)");
    redoButton.setTooltip (um.canRedo() ? "Redo " + um.getRedoDescription().toLowerCase() + " (Ctrl+Y)" : "Redo (Ctrl+Y)");

    bandCountLabel.setText (juce::String (processor.getNumUsedBands()) + " / " + juce::String (EQ::numBands) + " bands",
                            juce::dontSendNotification);

    const float range = processor.displayRangeDb.load();
    for (int i = 0; i < 3; ++i)
        if (std::abs (ranges[i] - range) < 0.5f && rangeBox.getSelectedItemIndex() != i)
            rangeBox.setSelectedItemIndex (i, juce::dontSendNotification);

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

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);
    g.setColour (EQColours::text);
    g.setFont (EQColours::font (19.0f, true));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(), header, juce::Justification::centredLeft);
}

void ThreeBandEQAudioProcessorEditor::resized()
{
    processor.editorWidth.store (getWidth());
    processor.editorHeight.store (getHeight());

    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (margin, 9);

    header.removeFromLeft (130);   // title
    bandCountLabel.setBounds (header.removeFromLeft (100));
    undoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (6);
    redoButton.setBounds (header.removeFromLeft (56));
    header.removeFromLeft (10);
    rangeBox.setBounds (header.removeFromLeft (92));
    header.removeFromLeft (6);
    analyserBox.setBounds (header.removeFromLeft (132));
    header.removeFromLeft (10);
    hintLabel.setBounds (header);

    display.setBounds (area.reduced (margin, 0).withTrimmedBottom (margin));
}
