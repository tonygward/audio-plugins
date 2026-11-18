#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "OptLookAndFeel.h"
#include "OptMeter.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

// A rotary control with its name above, a printed scale around it and a readable value below.
class OptKnob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    OptKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name,
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

// The Compress and Limit buttons: the label, plus a small picture of what the mode does to the sound. The picture is a
// concept, not a measurement: input level along the bottom, output level up the side. Compress bends gently above the
// threshold; Limit flattens much more. (Both are the leveller's own smooth optical action: neither is a hard ceiling.)
class OptModeButton : public juce::TextButton
{
public:
    OptModeButton (const juce::String& text, bool isLimit) : juce::TextButton (text), limit (isLimit) {}
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    // The icon's area inside a button of this size, at the left edge. Tests read it.
    static juce::Rectangle<float> iconArea (juce::Rectangle<int> buttonBounds) noexcept;
    // The two slopes of the picture after the bend, as output rise for input rise (Compress shallower than 1, Limit flatter still).
    static float slopeAfterBend (bool isLimit) noexcept { return isLimit ? 0.08f : 0.40f; }

private:
    bool limit;
};

class SongsmithOpticalLevellerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithOpticalLevellerEditor (SongsmithOpticalLevellerProcessor&);
    ~SongsmithOpticalLevellerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the title bar controls, in reading order.
    juce::Button& getUndoButton() noexcept { return undoButton; }
    juce::Button& getRedoButton() noexcept { return redoButton; }
    juce::Button& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getPresetBox() noexcept { return presetBox; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    float getContentScale() const noexcept { return (float) getWidth() / (float) Opt::EditorLayout::designWidth; }
    juce::Component& getTitleHint() noexcept { return titleHint; }

    // For tests and screenshots.
    Opt::UI::NeedleMeter& getMeter() noexcept { return meter; }
    Songsmith::UI::MeterColumn& getMeterColumn() noexcept { return meterColumn; }
    OptKnob& getGainKnob() noexcept { return gainKnob; }
    OptKnob& getPeakReductionKnob() noexcept { return peakReductionKnob; }
    juce::Button& getModeButton (int index) { return modeButtons[(size_t) index]; }   // 0 = Compress, 1 = Limit
    juce::Button& getMeterButton (int index) { return meterButtons[(size_t) index]; }   // 0 = Gain Reduction, 1 = Output
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    void advanceMeter (float seconds);

    // Text the editor paints (the wording tests read these).
    static juce::String gainReductionText (float db);   // the readout under the meter

private:
    void syncSizeFromWidth();
    void timerCallback() override { advanceMeter (1.0f / 30.0f); refreshHistoryButtons(); }
    void refreshHistoryButtons();
    void refreshButtons();
    void setMeterMode (int mode);
    void rebuildPresetMenu();
    void presetChosen();
    void savePresetAsync();

    // All controls live here, laid out in design units, then scaled as one.
    struct Content : juce::Component
    {
        explicit Content (SongsmithOpticalLevellerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithOpticalLevellerEditor& owner;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();

    SongsmithOpticalLevellerProcessor& proc;
    Opt::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;   // until then, resizing does not overwrite the saved window width
    juce::Rectangle<int> titleBounds;   // the product name, painted

    OptKnob gainKnob, peakReductionKnob;
    std::array<OptModeButton, 2> modeButtons { OptModeButton ("Compress", false), OptModeButton ("Limit", true) };
    juce::ParameterAttachment modeAttachment;
    int modeIndex = 0;

    juce::TextButton bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    std::array<juce::TextButton, 2> meterButtons { juce::TextButton (Opt::meterModeName (0)), juce::TextButton (Opt::meterModeName (1)) };
    Opt::UI::NeedleMeter meter;
    float grDisplayDb = 0.0f;
    Songsmith::UI::MeterColumn meterColumn { true };   // SUI-23: Gain Reduction and Output at the far right, like the Songsmith Compressor

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithOpticalLevellerEditor)
};
