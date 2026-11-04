#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MaxLookAndFeel.h"
#include "Meters.h"

// Songsmith Maximiser editor. Everything sits in one content component laid out at the design size
// (720 x 420) and scaled as a whole, so the window can be resized (75 % to 300 %, fixed aspect)
// without anything getting cramped. Vector drawing and the embedded Inter font throughout.
class SongsmithMaximiserAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithMaximiserAudioProcessorEditor (SongsmithMaximiserAudioProcessor&);
    ~SongsmithMaximiserAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // For the tests.
    juce::Slider& getThresholdSlider() noexcept { return thresholdSlider; }
    juce::Slider& getCeilingSlider() noexcept   { return ceilingSlider; }
    juce::Slider& getReleaseSlider() noexcept   { return releaseSlider; }
    juce::Button& getLinkButton() noexcept      { return linkButton; }
    juce::ComboBox& getPeakModeBox() noexcept   { return peakModeBox; }
    juce::ComboBox& getDitherBox() noexcept     { return ditherBox; }
    VerticalMeter& getReductionMeter() noexcept { return reductionMeter; }
    juce::Component& getContent() noexcept      { return content; }
    float getContentScale() const noexcept      { return contentScale; }

    // What the timer does: pull parameter changes made by the host into the faders, and feed the
    // meters (elapsed = seconds since the last update).
    void syncFromParameters();
    void updateMeters (double elapsedSeconds);

private:
    void timerCallback() override;
    void layoutContent();
    void paintContent (juce::Graphics&);

    // Threshold / Out Ceiling are driven by hand (no attachment) so that Link only reacts to the
    // user, never to host automation.
    void faderMoved (bool isThreshold);
    void setParameter (const char* id, float value);

    SongsmithMaximiserAudioProcessor& proc;   // not "processor": that would shadow AudioProcessorEditor::processor
    MaxLookAndFeel lookAndFeel;

    struct Content final : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void()> onResize;
        void paint (juce::Graphics& g) override  { if (onPaint) onPaint (g); }
        void resized() override                 { if (onResize) onResize(); }
    } content;
    float contentScale = 1.0f;

    juce::TooltipWindow tooltips { &content, 600 };

    juce::Slider thresholdSlider { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::Slider ceilingSlider   { juce::Slider::LinearVertical, juce::Slider::TextBoxBelow };
    juce::Slider releaseSlider   { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::TextButton linkButton { "Link" }, autoReleaseButton { "Auto Release" }, bypassButton { "Bypass" };
    juce::ComboBox peakModeBox, ditherBox;
    juce::Label thresholdLabel, ceilingLabel, releaseLabel, inLabel, grLabel, outLabel, peakLabel, ditherLabel, gainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoReleaseAttachment, bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> peakModeAttachment, ditherAttachment;

    VerticalMeter inputMeter { VerticalMeter::Kind::level }, reductionMeter { VerticalMeter::Kind::reduction },
                  outputMeter { VerticalMeter::Kind::level };

    bool updatingFaders = false;
    int gestureDepth = 0;
    double lastTimerTime = 0.0;
    bool constructed = false;   // resized() only saves the size once the saved one is applied

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithMaximiserAudioProcessorEditor)
};
