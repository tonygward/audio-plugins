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

    // Fixed slots, left to right. At narrow widths the hint, then the band count,
    // give up their space so the output controls always stay visible.
    constexpr int titleW = 130, countW = 100, undoW = 56, rangeW = 92, analyserW = 128;   // fits the 760 px minimum width
    constexpr int outW = 104, meterW = 64, bypassW = 62;
    constexpr int fixedW = titleW + undoW + 6 + undoW + 10 + rangeW + 6 + analyserW + 10 + outW + 6 + meterW + 6 + bypassW;
    const bool showCount = header.getWidth() >= fixedW + countW;

    header.removeFromLeft (titleW);
    bandCountLabel.setVisible (showCount);
    if (showCount)
        bandCountLabel.setBounds (header.removeFromLeft (countW));
    undoButton.setBounds (header.removeFromLeft (undoW));
    header.removeFromLeft (6);
    redoButton.setBounds (header.removeFromLeft (undoW));
    header.removeFromLeft (10);
    rangeBox.setBounds (header.removeFromLeft (rangeW));
    header.removeFromLeft (6);
    analyserBox.setBounds (header.removeFromLeft (analyserW));
    header.removeFromLeft (10);

    outputSlider.setBounds (header.removeFromLeft (outW));
    header.removeFromLeft (6);
    outputMeter.setBounds (header.removeFromLeft (meterW));
    header.removeFromLeft (6);
    bypassButton.setBounds (header.removeFromLeft (bypassW));
    header.removeFromLeft (10);

    hintLabel.setVisible (header.getWidth() >= 220);
    hintLabel.setBounds (header);

    display.setBounds (area.reduced (margin, 0).withTrimmedBottom (margin));
}
