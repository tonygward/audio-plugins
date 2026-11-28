#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "MaxLookAndFeel.h"
#include "Meters.h"
#include "../../shared/Source/SongsmithDropdown.h"
#include "../../shared/Source/SongsmithOutputMeter.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithSetLevels.h"

// Songsmith Maximiser editor. Everything sits in one content component laid out at the design size
// (888 x 448) and scaled as a whole, so the window can be resized (75 % to 300 %, fixed aspect)
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
    juce::ComboBox& getDitherBox() noexcept     { return ditherBox; }
    juce::ComboBox& getOversamplingBox() noexcept { return oversamplingBox; }
    juce::ComboBox& getPeakModeBox() noexcept   { return peakModeBox; }
    juce::ComboBox& getStartingPointBox() noexcept { return startingPointBox; }
    VerticalMeter& getReductionMeter() noexcept { return reductionMeter; }
    Songsmith::UI::OutputMeterDisplay& getOutputMeter() noexcept { return outputMeter; }
    juce::Label& getReductionValue() noexcept   { return grValue; }
    juce::Label& getReductionLabel() noexcept   { return grLabel; }
    juce::Component& getContent() noexcept      { return content; }
    juce::Button& getLoudnessResetButton() noexcept { return loudnessResetButton; }
    juce::Label& getIntegratedLabel() noexcept  { return integratedValue; }
    juce::Label& getIntegratedCaption() noexcept { return integratedCaption; }
    juce::Label& getIntegratedUnit() noexcept   { return integratedUnit; }
    juce::Rectangle<int> getReleaseDialBounds() const noexcept { return releaseSlider.getBounds(); }
    juce::Label& getDitherLabel() noexcept      { return ditherLabel; }
    juce::Label& getReleaseLabel() noexcept     { return releaseLabel; }

    // songsmithstudio#18: Set Level.
    juce::Button& getSetLevelButton() noexcept  { return setLevelButton; }
    Songsmith::UI::SetLevelsDialog& getSetLevelDialog() noexcept { return levelsDialog; }
    const Max::SetLevel::Result& getSetLevelResult() const noexcept { return levelsResult; }
    void beginListening();
    void finishListening();
    void applyProposal();
    void cancelLevels();
    // For the tests: waits for the measuring to finish and shows its result, as the timer would.
    void waitForSetLevelAnalysis();
    bool isSetLevelMeasuring() const noexcept { return levelsJob != nullptr; }

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
    juce::ComboBox peakModeBox, ditherBox, oversamplingBox, startingPointBox;

    // songsmithstudio#18: Set Level. The button sits under the Integrated LUFS readout, the reading it sets; the shared
    // dialog floats over the editor while it is open. The measuring runs on its own thread so the window never stalls.
    juce::TextButton setLevelButton { "Set Level" };
    Songsmith::UI::SetLevelsDialog levelsDialog;
    Max::SetLevel::Result levelsResult;
    Max::SetLevel::Settings levelsSettings;      // what the proposal was measured with
    std::vector<float> levelsAudio[2];           // the sound that was played, copied out of the capture
    int levelsChannels = 0, levelsSamples = 0;
    double levelsRate = 0.0;

    struct AnalysisJob final : juce::Thread
    {
        AnalysisJob (const SongsmithMaximiserAudioProcessorEditor& e, const Max::SetLevel::Settings& s, double target)
            : juce::Thread ("Songsmith Set Level"), editor (e), settings (s), targetLufs (target) {}
        void run() override
        {
            const float* in[2] = { editor.levelsAudio[0].data(), editor.levelsAudio[editor.levelsChannels > 1 ? 1 : 0].data() };
            result = Max::SetLevel::analyse (in, editor.levelsChannels, editor.levelsSamples, editor.levelsRate, settings, targetLufs,
                                             [this] { return threadShouldExit(); });
            done.store (true);
        }
        const SongsmithMaximiserAudioProcessorEditor& editor;
        const Max::SetLevel::Settings settings;
        const double targetLufs;
        Max::SetLevel::Result result;
        std::atomic<bool> done { false };
    };
    std::unique_ptr<AnalysisJob> levelsJob;
    void startAnalysis();
    void stopAnalysis();
    void takeAnalysisResult();
    void updateSetLevel();   // the timer's part: the ear, the listening time, a finished measurement
    juce::Label thresholdLabel, ceilingLabel, releaseLabel, inLabel, grLabel, peakLabel, ditherLabel, gainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoReleaseAttachment, bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> peakModeAttachment, ditherAttachment, oversamplingAttachment;

    // SUI-23: the far-right column is the Songsmith Compressor's: the shared Output meter at the very edge and the
    // Gain Reduction bar (with its number) beside it.
    VerticalMeter inputMeter { VerticalMeter::Kind::level }, reductionMeter { VerticalMeter::Kind::reduction };
    Songsmith::UI::OutputMeterDisplay outputMeter;
    juce::Label grValue;

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
