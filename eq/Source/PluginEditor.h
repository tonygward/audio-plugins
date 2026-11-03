#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EQDisplay.h"
#include "EQLookAndFeel.h"
#include "OutputMeter.h"
#include "PresetBar.h"

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
    PresetBar presetBar { processor };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::ComboBox rangeBox, analyserBox, curveViewBox, sidechainBox, phaseBox, oversamplingBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    double lastSidechainSignal = -100.0;   // seconds; the sidechain selector shows while a signal is arriving
    juce::Label bandCountLabel, hintLabel;

    // Global output stage
    juce::Slider outputSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    juce::Slider scaleSlider { juce::Slider::LinearBar, juce::Slider::TextBoxLeft };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scaleAttachment;
    juce::TextButton bypassButton { "Bypass" }, autoGainButton { "AG" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    juce::TextButton analyserSettingsButton { "..." };   // analyser settings popup
    void showAnalyserMenu();
    OutputMeterDisplay outputMeter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    double lastMeterTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeBandEQAudioProcessorEditor)
};
