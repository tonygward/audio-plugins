#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "FETLookAndFeel.h"
#include "NeedleMeter.h"
#include "../../shared/Source/SongsmithDropdown.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

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

// ACOMP-16: Ignore Below as a horizontal slider. Row one: the name at the left, an editable live readout at the
// right ("127 Hz", or "Off"). Row two: the track with its handle. Row three: "20 Hz" and "500 Hz" under the ends.
// It drives `ignoreBelowHz` (whole Hz, Off or 20 to 500, logarithmic). Double-click returns it to Off.
class IgnoreBelowControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit IgnoreBelowControl (juce::AudioProcessorValueTreeState& state);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Dragging, arrow keys and double-click all go through here.
    struct HorizontalSlider : juce::Slider
    {
        HorizontalSlider() : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox) {}
        bool keyPressed (const juce::KeyPress&) override;
    };

    HorizontalSlider slider;
    juce::Label readout;

    juce::String getValueText() const { return FET::ignoreBelowText ((float) slider.getValue()); }
    float getHz() const { return FET::ignoreBelowSnap ((float) slider.getValue()); }
    void setHzAsGesture (float hz);                 // typed entry and tests: one complete automation gesture
    void commitText (const juce::String& text);     // what the readout does when the musician presses Return

    juce::Rectangle<int> getLabelBounds() const noexcept { return labelBounds; }
    juce::Rectangle<int> getMinLabelBounds() const noexcept { return minBounds; }
    juce::Rectangle<int> getMaxLabelBounds() const noexcept { return maxBounds; }
    static juce::String getMinLabelText() { return "20 Hz"; }
    static juce::String getMaxLabelText() { return "500 Hz"; }

private:
    void refreshReadout();
    juce::RangedAudioParameter& param;
    juce::Rectangle<int> labelBounds, minBounds, maxBounds;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class SongsmithAnalogCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor&);
    ~SongsmithAnalogCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the title bar controls, in reading order (SUI-18).
    juce::Button& getUndoButton() noexcept { return undoButton; }
    juce::Button& getRedoButton() noexcept { return redoButton; }
    juce::Button& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getOversamplingBox() noexcept { return oversampling; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }

    void setUiScale (float scale);
    float getUiScale() const noexcept { return uiScale; }

    // For tests and screenshots.
    FET::UI::NeedleMeter& getMeter() noexcept { return meter; }
    Songsmith::UI::MeterColumn& getMeterColumn() noexcept { return meterColumn; }
    FETKnob& getKnob (const juce::String& id);
    juce::Button& getRatioButton (int index) { return *ratioButtons[(size_t) index]; }   // 0..3 = 4, 8, 12, 20
    juce::Label& getLegacyRatioLabel() noexcept { return legacyRatioLabel; }
    juce::Button& getAllButton() { return allButton; }
    juce::Rectangle<int> getGrOffBounds() const { return grOffButton.getBounds(); }
    IgnoreBelowControl& getIgnoreBelow() noexcept { return ignoreBelow; }
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    juce::ComboBox& getPresetBox() { return presetBox; }

    // ACOMP-23: the Model dropdown in the configuration row (outside the panel), and the two buttons
    // only some models have (for tests).
    juce::TextButton& getModelButton (int index) noexcept { return modelButtons[(size_t) index]; }   // 0..3 = Black, Blue, Anniversary, Lindell
    juce::Rectangle<int> getModelButtonsBounds() const noexcept { return modelButtons[0].getBounds().getUnion (modelButtons[3].getBounds()); }
    juce::Label& getModelLabel() noexcept { return modelLabel; }
    juce::Button& getGentleRatioButton() noexcept { return gentleButton; }
    juce::Button& getSlowAttackButton() noexcept { return slowAttackButton; }
    int getShownModel() const noexcept { return modelShown; }
    void advanceMeter (float seconds);

    // ACOMP-09 Set Levels (for tests). idle -> listening -> proposal -> idle.
    enum class LevelsStage { idle, listening, proposal };
    LevelsStage getLevelsStage() const noexcept { return levelsStage; }
    // ACOMP-21: the faceplate keeps one small entry button; the steps are in a temporary panel above it.
    juce::Button& getSetLevelsButton() noexcept { return setLevelsButton; }          // the entry button on the faceplate
    juce::Button& getSetLevelsActionButton() noexcept { return setLevelsAction; }    // in the panel: Finish Listening, then Apply
    juce::Button& getSetLevelsCancelButton() noexcept { return setLevelsCancel; }    // in the panel: Cancel, or Close after a result
    juce::Label& getSetLevelsMessage() noexcept { return setLevelsMessage; }
    juce::Component& getSetLevelsPanel() noexcept { return levelsPanel; }
    bool isSetLevelsPanelOpen() const noexcept { return levelsPanel.isVisible(); }
    void closeSetLevelsNotice();           // dismisses the panel after a result (nothing is changed)
    const FET::Calibration::Result& getLevelsResult() const noexcept { return levelsResult; }
    void beginListening();                 // what the button does when idle
    void finishListening();                // stops capturing and works out the proposal
    void applyProposal();                  // writes Input and Output as one Undo step
    void cancelLevels();                   // leaves everything as it was
    static juce::String levelsMessageFor (const FET::Calibration::Result& r);
    static juce::String attackTooltip();
    static juce::String releaseTooltip();

    // Text the editor paints (the wording tests read these).
    static juce::String gainReductionText (float db);   // the readout under the meter
    static juce::String meterModeName (int mode);        // 0 Gain Reduction, 1 Output +4, 2 Output +8
    static juce::String oversamplingText (int index);    // "Oversampling: off", "Oversampling: 2x" ...

private:
    void syncSizeFromWidth();
    void timerCallback() override
    {
        advanceMeter (1.0f / 30.0f);
        refreshHistoryButtons();
        updateLevelsListening();
        if (levelsNoticeTicks > 0 && --levelsNoticeTicks == 0)
            closeSetLevelsNotice();
    }
    void updateLevelsListening();
    void showLevelsStage (LevelsStage stage);
    bool levelsSettingsUnchanged() const;
    LevelsStage levelsStage = LevelsStage::idle;

    // The temporary Set Levels panel. It floats over the faceplate (never rearranging it), sits inside the editor and
    // closes on Escape. After Apply or a notice it shows the result for a few seconds, then goes.
    struct LevelsPanel : juce::Component
    {
        void paint (juce::Graphics&) override;
        bool keyPressed (const juce::KeyPress&) override;
        std::function<void()> onEscape;
        juce::Label heading;
    };
    void layoutLevelsPanel();
    void showLevelsNotice (const juce::String& text);
    LevelsPanel levelsPanel;
    juce::TextButton setLevelsButton { "Set Levels" }, setLevelsAction { "Finish Listening" }, setLevelsCancel { "Cancel" };
    juce::Label setLevelsMessage;
    int levelsNoticeTicks = 0;   // counts down (in timer ticks) while a result is shown with the stage idle
    FET::Calibration::Result levelsResult;
    FET::Calibration::Settings levelsSettings;   // the ratio, attack, release and Ignore Below the proposal was measured with
    float levelsInput = 0.0f, levelsOutput = 0.0f;   // Input and Output when listening began
    void refreshHistoryButtons();
    juce::Rectangle<int> titleBounds;   // the product name, painted (SUI-18)
    void refreshButtons();
    void setMeterMode (int mode);
    void rebuildPresetMenu();
    void presetChosen();
    void savePresetAsync();

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
    bool constructed = false;   // until then, resizing does not overwrite the saved window width

    FETKnob input, output, attack, release;
    std::array<std::unique_ptr<juce::TextButton>, 4> ratioButtons;   // 0..3 = 4, 8, 12, 20 (2:1 is no longer offered, ACOMP-19)
    juce::TextButton allButton { "All" };
    juce::Label legacyRatioLabel;   // "Saved 2:1": shown only when an older session still uses 2:1
    juce::ParameterAttachment ratioAttachment, allButtonsAttachment, gentleAttachment;

    // ACOMP-10. The box is driven by modelAttachment (so a preset, automation or a loaded session updates it without
    // being mistaken for a choice by the musician); onChange is therefore only the musician choosing a model.
    void refreshModel();
    void modelChosenByUser (int model);
    std::array<juce::TextButton, 4> modelButtons;   // Black, Blue, Anniversary, Lindell (songsmithstudio#12): one lit at a time
    juce::Label modelLabel;       // "Model", its caption in the configuration row
    juce::TextButton gentleButton { "2:1" }, slowAttackButton { "Slow Attack" };
    juce::ParameterAttachment modelAttachment, slowAttackAttachment;
    int modelShown = FET::modelDefault;
    bool slowAttackOn = false;

    juce::TextButton grOffButton { "Gain Reduction Off" }, bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment grOffAttachment, bypassAttachment;

    std::array<juce::TextButton, 3> meterButtons { juce::TextButton (meterModeName (0)), juce::TextButton (meterModeName (1)), juce::TextButton (meterModeName (2)) };
    FET::UI::NeedleMeter meter;
    Songsmith::UI::MeterColumn meterColumn { true };   // SUI-23: Gain Reduction and Output at the far right, like the Songsmith Compressor
    float grDisplayDb = 0.0f;

    juce::ComboBox oversampling;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    IgnoreBelowControl ignoreBelow;   // detector cutoff slider, directly under the Input knob (ACOMP-19)
    juce::ComboBox presetBox;
    juce::TooltipWindow tooltips { this, 600 };

    // Latency and the optional Processing Load readout, in the strip below the controls.
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;

    // SUI-17: a transparent hit target over the painted title; hovering it shows what the plugin does.
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };
public:
    juce::Component& getTitleHint() noexcept { return titleHint; }
private:
    std::unique_ptr<juce::AlertWindow> saveDialog;
    static constexpr int savePresetId = 1000, openFolderId = 1001, firstUserId = 100;
    juce::Array<juce::File> userPresetFiles;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment oversamplingAttachment;

    int ratioIndex = 0;
    bool allButtonsOn = false;
    bool gentleOn = false;   // the Songsmith 2:1 switch

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogCompressorEditor)
};
