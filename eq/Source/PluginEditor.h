#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "EQDisplay.h"
#include "EQLookAndFeel.h"
#include "OutputMeter.h"
#include "PresetBar.h"

class SongsmithEQAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithEQAudioProcessorEditor (SongsmithEQAudioProcessor&);
    ~SongsmithEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // The analyser settings menu (the "..." button). Public so tests can check its wording.
    juce::PopupMenu createAnalyserMenu();

    // UI size (75 % to 200 %): resizes the window so the content keeps the same layout, and
    // remembers the choice for the session and as this machine's default.
    void setUiScale (float scale);

    // Everything sits in one root component laid out in design units. A single transform on it
    // applies UI size x layout step (the step grows controls and text on large windows). The
    // host's own display scaling (VST3 content scale, Retina) comes on top, separately.
    juce::Component& getContent() noexcept   { return content; }
    EQDisplay& getDisplay() noexcept         { return display; }
    float getContentScale() const noexcept   { return contentScale; }   // design units -> window pixels
    float getLayoutStep() const noexcept     { return layoutStep; }

    // Reads a typed output gain: "Output +1.5 dB", "+1.5", "-3 dB" or "2".
    static double parseOutputGainText (const juce::String&);

private:
    void timerCallback() override;

    SongsmithEQAudioProcessor& processor;
    EQLookAndFeel lookAndFeel;

    struct Content final : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void()> onResize;
        void paint (juce::Graphics& g) override  { if (onPaint) onPaint (g); }
        void resized() override                 { if (onResize) onResize(); }
    } content;
    float contentScale = 1.0f, layoutStep = 1.0f;
    void paintContent (juce::Graphics&);
    void layoutContent();
    void updateResizeLimits();

    // Resize limits plus the width/height range (EditorSize::minAspect..maxAspect).
    struct Constrainer final : juce::ComponentBoundsConstrainer
    {
        float uiScale = 1.0f;
        void checkBounds (juce::Rectangle<int>&, const juce::Rectangle<int>& previous, const juce::Rectangle<int>& limits,
                          bool top, bool left, bool bottom, bool right) override;
    } constrainer;

    juce::TooltipWindow tooltips { &content, 600 };   // inside the scaled content, so tips scale too

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
    juce::TextButton bypassButton { "Bypass" }, autoGainButton { "Auto Gain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    juce::TextButton analyserSettingsButton { "..." };   // analyser settings popup
    void showAnalyserMenu();
    static juce::FontOptions titleFont();
    juce::Rectangle<int> titleBounds;   // the product name, painted
    OutputMeterDisplay outputMeter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    double lastMeterTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithEQAudioProcessorEditor)
};
