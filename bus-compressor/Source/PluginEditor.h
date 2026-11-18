#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "BusLookAndFeel.h"
#include "BusMeter.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

// A rotary control with its name above, a printed scale around it and a readable value below. It works for the
// stepped controls too: their scale marks are the steps, and their value text is the step's name.
class BusKnob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    BusKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name,
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

class SongsmithBusCompressorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithBusCompressorEditor (SongsmithBusCompressorProcessor&);
    ~SongsmithBusCompressorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the title bar controls, in reading order.
    juce::Button& getUndoButton() noexcept { return undoButton; }
    juce::Button& getRedoButton() noexcept { return redoButton; }
    juce::Button& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getPresetBox() noexcept { return presetBox; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    float getContentScale() const noexcept { return (float) getWidth() / (float) BusComp::EditorLayout::designWidth; }
    juce::Component& getTitleHint() noexcept { return titleHint; }

    // For tests and screenshots.
    BusComp::UI::LampMeter& getMeter() noexcept { return meter; }
    Songsmith::UI::MeterColumn& getMeterColumn() noexcept { return meterColumn; }
    BusKnob& getThresholdKnob() noexcept { return thresholdKnob; }
    BusKnob& getRatioKnob() noexcept { return ratioKnob; }
    BusKnob& getAttackKnob() noexcept { return attackKnob; }
    BusKnob& getReleaseKnob() noexcept { return releaseKnob; }
    BusKnob& getMakeupKnob() noexcept { return makeupKnob; }
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    void advanceMeter (float seconds);

    // Text the editor paints (the wording tests read these).
    static juce::String gainReductionText (float db);   // the readout under the meter

private:
    void syncSizeFromWidth();
    void timerCallback() override { advanceMeter (1.0f / 30.0f); refreshHistoryButtons(); }
    void refreshHistoryButtons();
    void rebuildPresetMenu();
    void presetChosen();
    void savePresetAsync();

    // All controls live here, laid out in design units, then scaled as one.
    struct Content : juce::Component
    {
        explicit Content (SongsmithBusCompressorEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithBusCompressorEditor& owner;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();

    SongsmithBusCompressorProcessor& proc;
    BusComp::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;   // until then, resizing does not overwrite the saved window width
    juce::Rectangle<int> titleBounds;   // the product name, painted

    BusKnob thresholdKnob, ratioKnob, attackKnob, releaseKnob, makeupKnob;

    juce::TextButton bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    BusComp::UI::LampMeter meter;
    Songsmith::UI::MeterColumn meterColumn { true };   // SUI-23: Gain Reduction and Output at the far right, like the Songsmith Compressor
    float grDisplayDb = 0.0f;

    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };
    juce::ComboBox presetBox;
    juce::TooltipWindow tooltips { this, 600 };

    // Latency and the optional Processing Load readout, in the strip below the controls.
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;

    // A transparent hit target over the painted title; hovering it shows what the plugin does.
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };

    std::unique_ptr<juce::AlertWindow> saveDialog;
    static constexpr int savePresetId = 1000, openFolderId = 1001, firstUserId = 100;
    juce::Array<juce::File> userPresetFiles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithBusCompressorEditor)
};
