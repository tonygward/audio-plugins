#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <deque>
#include "PluginProcessor.h"
#include "DeskLookAndFeel.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithMeterColumn.h"
#include "../../shared/Source/SongsmithOutputPanel.h"

// A rotary control with its name above and a readable value below.
class DeskKnob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    // 'name' is what is painted (short, under a heading that completes it); 'title' is the full name for screen readers.
    DeskKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name, const juce::String& title,
              Formatter format);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String getValueText() const { return formatter ((float) slider.getValue()); }
    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); accent = c; }
    juce::Colour getAccent() const noexcept { return accent; }
    const juce::String& getShownName() const noexcept { return shownName; }

private:
    juce::String shownName;
    juce::Colour accent;
    Formatter formatter;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// A switch tied to one parameter.
struct DeskToggle
{
    DeskToggle (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text, const juce::String& title)
        : button (text), attachment (state, paramId, button)
    {
        button.setClickingTogglesState (true);
        button.setTitle (title);
    }

    juce::TextButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

class SongsmithAnalogDeskEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithAnalogDeskEditor (SongsmithAnalogDeskProcessor&);
    ~SongsmithAnalogDeskEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the title bar controls, in reading order.
    juce::Button& getUndoButton() noexcept { return undoButton; }
    juce::Button& getRedoButton() noexcept { return redoButton; }
    juce::Button& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getPresetBox() noexcept { return presetBox; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    float getContentScale() const noexcept { return (float) getWidth() / (float) Desk::EditorLayout::designWidth; }
    juce::Component& getTitleHint() noexcept { return titleHint; }

    // For tests and screenshots.
    Songsmith::UI::MeterColumn& getMeterColumn() noexcept { return meterColumn; }
    Songsmith::UI::OutputPanel& getOutputPanel() noexcept { return outputPanel; }
    // The four modules, left to right: Input and Filters, Dynamics, Equaliser, Output and Meter.
    juce::Rectangle<int> getModuleBounds (int index) const noexcept;
    DeskKnob* findKnob (const juce::String& paramId);
    std::vector<DeskKnob*> getKnobs();
    std::vector<juce::Button*> getToggles();
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    void advanceMeter (float seconds);

    // The headings the editor paints (the wording tests read these).
    static juce::StringArray paintedTexts();

private:
    void syncSizeFromWidth();
    void timerCallback() override { advanceMeter (1.0f / 30.0f); refreshHistoryButtons(); }
    void refreshHistoryButtons();
    void rebuildPresetMenu();
    void presetChosen();
    void savePresetAsync();

    DeskKnob& addKnob (const char* paramId, const juce::String& name, const juce::String& title, const juce::String& tip,
                       DeskKnob::Formatter format, juce::Rectangle<int> bounds);
    DeskToggle& addToggle (const char* paramId, const juce::String& text, const juce::String& title, const juce::String& tip,
                           juce::Rectangle<int> bounds);

    // All controls live here, laid out in design units, then scaled as one.
    struct Content : juce::Component
    {
        explicit Content (SongsmithAnalogDeskEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithAnalogDeskEditor& owner;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();

    SongsmithAnalogDeskProcessor& proc;
    Desk::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;   // until then, resizing does not overwrite the saved window width
    juce::Rectangle<int> titleBounds;   // the product name, painted

    // Deques keep every control where it was created, so the attachments stay valid.
    std::deque<DeskKnob> knobs;
    std::deque<DeskToggle> toggles;
    std::vector<std::pair<juce::String, int>> knobIds;   // parameter ID, index into 'knobs'

    juce::TextButton bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    // songsmithstudio#16: the suite's Output panel at the far right: Gain Reduction and Output meters, a compact Output Trim and Auto Gain.
    Songsmith::UI::OutputPanel outputPanel { true };
    Songsmith::UI::MeterColumn& meterColumn = outputPanel.getMeters();
    juce::AudioProcessorValueTreeState::SliderAttachment outputTrimAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAttachment;
    juce::String lastAutoGainText;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogDeskEditor)
};
