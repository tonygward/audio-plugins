#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "FETLookAndFeel.h"
#include "NeedleMeter.h"

// A rotary control with its name above, a printed scale around it and a readable value below.
class FETKnob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    FETKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name,
             std::vector<std::pair<float, juce::String>> scaleMarks, Formatter format);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String getValueText() const { return formatter ((float) slider.getValue()); }

private:
    juce::String name;
    std::vector<std::pair<float, juce::String>> marks;
    Formatter formatter;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class SongsmithAnalogCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor&);
    ~SongsmithAnalogCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setUiScale (float scale);
    float getUiScale() const noexcept { return uiScale; }

    // For tests and screenshots.
    FET::UI::NeedleMeter& getMeter() noexcept { return meter; }
    FETKnob& getKnob (const juce::String& id);
    juce::Button& getRatioButton (int index) { return *ratioButtons[(size_t) index]; }   // 0..3 = 4, 8, 12, 20
    juce::Button& getAllButton() { return allButton; }
    void advanceMeter (float seconds);

private:
    void timerCallback() override { advanceMeter (1.0f / 30.0f); }
    void refreshButtons();
    void setMeterMode (int mode);

    // All controls live here, laid out in design units, then scaled as one.
    struct Content : juce::Component
    {
        explicit Content (SongsmithAnalogCompressorEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithAnalogCompressorEditor& owner;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();

    SongsmithAnalogCompressorProcessor& proc;
    FET::UI::LookAndFeel lnf;
    Content content { *this };
    float uiScale = 1.0f;

    FETKnob input, output, attack, release;
    std::array<std::unique_ptr<juce::TextButton>, 4> ratioButtons;
    juce::TextButton allButton { "All" };
    juce::ParameterAttachment ratioAttachment, allButtonsAttachment;

    juce::TextButton grOffButton { "GR Off" }, bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment grOffAttachment, bypassAttachment;

    std::array<juce::TextButton, 3> meterButtons { juce::TextButton ("GR"), juce::TextButton ("+4"), juce::TextButton ("+8") };
    FET::UI::NeedleMeter meter;
    float grDisplayDb = 0.0f;

    juce::ComboBox oversampling, sizeBox;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment oversamplingAttachment;

    int ratioIndex = 0;
    bool allButtonsOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogCompressorEditor)
};
