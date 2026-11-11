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
             std::vector<std::pair<float, juce::String>> scaleMarks, Formatter format, bool compactLayout = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String getValueText() const { return formatter ((float) slider.getValue()); }
    bool isCompact() const noexcept { return compact; }

private:
    bool compact;   // knob at the left, name and value at its right (used by Ignore Below)
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
    FETKnob& getIgnoreBelowKnob() noexcept { return ignoreBelow; }
    static juce::String lowCutText (int index);          // "Off", "60 Hz", "80 Hz" ... (the Ignore Below readout)
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
    FETKnob ignoreBelow;   // detector low cut as a stepped rotary, beside Gain Reduction Off, under the meter it affects
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
