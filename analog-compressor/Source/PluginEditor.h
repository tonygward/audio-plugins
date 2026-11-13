#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "FETLookAndFeel.h"
#include "NeedleMeter.h"
#include "../../shared/Source/SongsmithGuidance.h"

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
    FETKnob& getKnob (const juce::String& id);
    juce::Button& getRatioButton (int index) { return *ratioButtons[(size_t) index]; }   // 0..3 = 4, 8, 12, 20; 4 = 2:1
    juce::Button& getAllButton() { return allButton; }
    juce::Rectangle<int> getGrOffBounds() const { return grOffButton.getBounds(); }
    IgnoreBelowControl& getIgnoreBelow() noexcept { return ignoreBelow; }
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    juce::ComboBox& getPresetBox() { return presetBox; }
    void advanceMeter (float seconds);

    // Text the editor paints (the wording tests read these).
    static juce::String gainReductionText (float db);   // the readout under the meter
    static juce::String meterModeName (int mode);        // 0 Gain Reduction, 1 Output +4, 2 Output +8
    static juce::String oversamplingText (int index);    // "Oversampling: off", "Oversampling: 2x" ...

private:
    void syncSizeFromWidth();
    void timerCallback() override { advanceMeter (1.0f / 30.0f); refreshHistoryButtons(); }
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
    std::array<std::unique_ptr<juce::TextButton>, 5> ratioButtons;   // 0..3 = 4, 8, 12, 20; 4 = Songsmith 2:1
    juce::TextButton allButton { "All" };
    juce::ParameterAttachment ratioAttachment, allButtonsAttachment, gentleAttachment;

    juce::TextButton grOffButton { "Gain Reduction Off" }, bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment grOffAttachment, bypassAttachment;

    std::array<juce::TextButton, 3> meterButtons { juce::TextButton (meterModeName (0)), juce::TextButton (meterModeName (1)), juce::TextButton (meterModeName (2)) };
    FET::UI::NeedleMeter meter;
    float grDisplayDb = 0.0f;

    juce::ComboBox oversampling;
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    IgnoreBelowControl ignoreBelow;   // detector cutoff slider, beside Gain Reduction Off, under the meter it affects
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
