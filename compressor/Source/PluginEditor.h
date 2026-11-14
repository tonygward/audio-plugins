#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "CompLookAndFeel.h"
#include "CompMeter.h"
#include "../../shared/Source/SongsmithGuidance.h"

// Songsmith Compressor editor. Everything sits in one content component laid out at the design size
// (900 x 400) and scaled as a whole, so the window can be resized (75 % to 300 %, fixed aspect)
// without anything getting cramped. Vector drawing and the embedded Inter font throughout.
class SongsmithCompressorAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithCompressorAudioProcessorEditor (SongsmithCompressorAudioProcessor&);
    ~SongsmithCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For the tests.
    juce::Button& getUndoButton() noexcept        { return undoButton; }
    juce::Button& getRedoButton() noexcept        { return redoButton; }
    juce::Button& getBypassButton() noexcept      { return bypassButton; }
    juce::Button& getAutoMakeupButton() noexcept  { return autoMakeupButton; }
    juce::Button& getWarmthButton() noexcept      { return warmthButton; }
    juce::ComboBox& getPresetBox() noexcept       { return presetBox; }
    juce::Slider& getThresholdSlider() noexcept   { return thresholdSlider; }
    juce::Slider& getRatioSlider() noexcept       { return ratioSlider; }
    juce::Slider& getAttackSlider() noexcept      { return attackSlider; }
    juce::Slider& getReleaseSlider() noexcept     { return releaseSlider; }
    juce::Slider& getKneeSlider() noexcept        { return kneeSlider; }
    juce::Slider& getMakeupSlider() noexcept      { return makeupSlider; }
    juce::Slider& getMixSlider() noexcept         { return mixSlider; }
    CompMeter& getInputMeter() noexcept           { return inputMeter; }
    CompMeter& getReductionMeter() noexcept       { return reductionMeter; }
    CompMeter& getOutputMeter() noexcept          { return outputMeter; }
    juce::Component& getContent() noexcept        { return content; }
    juce::Component& getTitleHint() noexcept      { return titleHint; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    float getContentScale() const noexcept        { return contentScale; }

    // What the timer does: pull parameter changes made by the host into the controls, and feed the
    // meters (elapsed = seconds since the last update).
    void syncFromParameters();
    void updateMeters (double elapsedSeconds);

private:
    void timerCallback() override;
    void layoutContent();
    void paintContent (juce::Graphics&);
    void refreshHistoryButtons();

    SongsmithCompressorAudioProcessor& proc;   // not "processor": that would shadow AudioProcessorEditor::processor
    CompLookAndFeel lookAndFeel;

    struct Content final : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void()> onResize;
        void paint (juce::Graphics& g) override  { if (onPaint) onPaint (g); }
        void resized() override                 { if (onResize) onResize(); }
    } content;
    float contentScale = 1.0f;

    juce::TooltipWindow tooltips { &content, 600 };

    juce::Slider thresholdSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider ratioSlider     { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider attackSlider    { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider releaseSlider   { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider kneeSlider      { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider makeupSlider    { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider mixSlider       { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::TextButton autoMakeupButton { "Auto Makeup" }, warmthButton { "Analog Warmth" }, bypassButton { "Bypass" }, undoButton { "Undo" }, redoButton { "Redo" };
    juce::ComboBox presetBox;
    juce::Label thresholdLabel, ratioLabel, attackLabel, releaseLabel, kneeLabel, makeupLabel, mixLabel,
                inLabel, grLabel, outLabel;
    juce::Rectangle<int> titleBounds;   // the product name, painted

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment, ratioAttachment, attackAttachment,
                                                                          releaseAttachment, kneeAttachment, makeupAttachment, mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoMakeupAttachment, warmthAttachment, bypassAttachment;

    CompMeter inputMeter { CompMeter::Kind::level }, reductionMeter { CompMeter::Kind::reduction },
              outputMeter { CompMeter::Kind::level };

    // Latency and the optional Processing Load readout, in the strip below the controls.
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;

    // A transparent hit target over the painted title; hovering it shows what the plugin does.
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };

    double lastTimerTime = 0.0;
    bool constructed = false;   // resized() only saves the size once the saved one is applied

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithCompressorAudioProcessorEditor)
};
