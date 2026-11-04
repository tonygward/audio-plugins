#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Debug editor: plain JUCE controls bound to the parameters, for smoke tests until the
// vector editor (step 4) replaces it.
class SongsmithAnalogCompressorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor&);
    ~SongsmithAnalogCompressorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    SongsmithAnalogCompressorProcessor& proc;

    Knob input, output, attack, release;
    juce::ComboBox ratio;
    juce::Label ratioLabel;
    juce::ToggleButton allButtons { "All Buttons" }, grOff { "Gain Reduction Off" }, bypass { "Bypass" };

    std::unique_ptr<ComboAttachment> ratioAttachment;
    std::unique_ptr<ButtonAttachment> allButtonsAttachment, grOffAttachment, bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogCompressorEditor)
};
