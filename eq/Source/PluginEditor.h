#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EQDisplay.h"
#include "EQLookAndFeel.h"

class ThreeBandEQAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ThreeBandEQAudioProcessorEditor (ThreeBandEQAudioProcessor&);
    ~ThreeBandEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;

    ThreeBandEQAudioProcessor& processor;
    EQLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    EQDisplay display;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::ComboBox rangeBox, analyserBox;
    juce::Label bandCountLabel, hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeBandEQAudioProcessorEditor)
};
