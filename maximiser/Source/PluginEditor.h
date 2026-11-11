#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MaxLookAndFeel.h"
#include "Meters.h"
#include "../../shared/Source/SongsmithGuidance.h"

// Songsmith Maximiser editor. Everything sits in one content component laid out at the design size
// (860 x 448) and scaled as a whole, so the window can be resized (75 % to 300 %, fixed aspect)
// without anything getting cramped. Vector drawing and the embedded Inter font throughout.
class SongsmithMaximiserAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithMaximiserAudioProcessorEditor (SongsmithMaximiserAudioProcessor&);
    ~SongsmithMaximiserAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For the tests: the title bar (SUI-18).
    juce::Button& getUndoButton() noexcept      { return undoButton; }
    juce::Button& getRedoButton() noexcept      { return redoButton; }
    juce::Button& getBypassButton() noexcept    { return bypassButton; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }

    // For the tests.
    juce::Slider& getThresholdSlider() noexcept { return thresholdSlider; }
    juce::Slider& getCeilingSlider() noexcept   { return ceilingSlider; }
    juce::Slider& getReleaseSlider() noexcept   { return releaseSlider; }
    juce::Button& getLinkButton() noexcept      { return linkButton; }
    juce::ComboBox& getPeakModeBox() noexcept   { return peakModeBox; }
    juce::ComboBox& getDitherBox() noexcept     { return ditherBox; }
    juce::ComboBox& getStartingPointBox() noexcept { return startingPointBox; }
    VerticalMeter& getReductionMeter() noexcept { return reductionMeter; }
    juce::Component& getContent() noexcept      { return content; }
    juce::Button& getLoudnessResetButton() noexcept { return loudnessResetButton; }
    juce::Label& getIntegratedLabel() noexcept  { return integratedValue; }
    juce::Label& getIntegratedCaption() noexcept { return integratedCaption; }
    juce::Label& getIntegratedUnit() noexcept   { return integratedUnit; }
    juce::Rectangle<int> getReleaseDialBounds() const noexcept { return releaseSlider.getBounds(); }

    void clickIntegratedReadout() { if (integratedValue.onClick) integratedValue.onClick(); }

    // Starts the loudness measurement again (the Reset button and a click on the Integrated number).
    void resetLoudness();
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
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::Rectangle<int> titleBounds;   // the product name, painted
    void refreshHistoryButtons();
    juce::ComboBox peakModeBox, ditherBox, startingPointBox;
    juce::Label thresholdLabel, ceilingLabel, releaseLabel, inLabel, grLabel, outLabel, peakLabel, ditherLabel, gainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoReleaseAttachment, bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> peakModeAttachment, ditherAttachment;

    VerticalMeter inputMeter { VerticalMeter::Kind::level }, reductionMeter { VerticalMeter::Kind::reduction },
                  outputMeter { VerticalMeter::Kind::level };

    // Loudness section below the controls: readings in LUFS (kept by the processor, shown here).
    struct ReadoutLabel final : juce::Label
    {
        std::function<void()> onClick;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };
    juce::Label integratedCaption, integratedUnit;
    ReadoutLabel integratedValue;
    juce::TextButton loudnessResetButton { "Reset" };
    void showLoudness();

    // Latency and the optional Processing Load readout, in the strip below the controls.
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;

    // SUI-17: a transparent hit target over the painted title; hovering it shows what the plugin does.
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };

public:
    juce::Component& getTitleHint() noexcept { return titleHint; }
private:

    bool updatingFaders = false;
    int gestureDepth = 0;
    double lastTimerTime = 0.0;
    bool constructed = false;   // resized() only saves the size once the saved one is applied

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithMaximiserAudioProcessorEditor)
};
