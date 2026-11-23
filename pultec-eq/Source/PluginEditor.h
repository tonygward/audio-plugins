#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PeqLookAndFeel.h"
#include "PeqCurve.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithOutputMeter.h"
#include "../../shared/Source/SongsmithOutputPanel.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

// A rotary control with its name above, a printed scale around it and a readable value below.
class PeqKnob : public juce::Component
{
public:
    using Formatter = std::function<juce::String (float)>;

    // 'label' is what is painted above the knob; 'title' is the full name a screen reader announces.
    PeqKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& label, const juce::String& title,
             std::vector<std::pair<float, juce::String>> scaleMarks, Formatter format);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::String getValueText() const { return formatter ((float) slider.getValue()); }

private:
    juce::String label;
    std::vector<std::pair<float, juce::String>> marks;
    Formatter formatter;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

// A row of buttons, one per frequency; the lit one is the choice. A press is one automation gesture.
class PeqSwitch : public juce::Component
{
public:
    PeqSwitch (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::StringArray& names,
               const juce::String& title, const juce::String& tooltip);

    void resized() override;

    int getNumButtons() const noexcept { return buttons.size(); }
    juce::TextButton& getButton (int index) { return *buttons[index]; }
    int getSelected() const noexcept { return selected; }
    // PEQ-04: a long row of choices can wrap onto more lines (the buttons fill the rows left to right).
    void setRows (int numRows) { rows = juce::jmax (1, numRows); resized(); }
    int getRows() const noexcept { return rows; }

private:
    void refresh();

    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
    int selected = 0;
    int rows = 1;
};

class SongsmithPassiveEQEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithPassiveEQEditor (SongsmithPassiveEQProcessor&);
    ~SongsmithPassiveEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // For tests: the title bar controls, in reading order.
    juce::Button& getUndoButton() noexcept { return undoButton; }
    juce::Button& getRedoButton() noexcept { return redoButton; }
    juce::Button& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getPresetBox() noexcept { return presetBox; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    float getContentScale() const noexcept { return (float) getWidth() / (float) Peq::EditorLayout::designWidth; }
    juce::Component& getTitleHint() noexcept { return titleHint; }

    // For tests and screenshots.
    Peq::UI::ResponseCurve& getCurve() noexcept { return curve; }
    Songsmith::UI::OutputMeterDisplay& getOutputMeter() noexcept { return outputMeter; }
    Songsmith::UI::OutputPanel& getOutputPanel() noexcept { return outputPanel; }
    // Reads the audio thread's output peaks into the meter, as the timer does 30 times a second.
    void advanceMeter (double elapsedSeconds);
    PeqKnob& getLowBoostKnob() noexcept { return lowBoostKnob; }
    PeqKnob& getLowCutKnob() noexcept { return lowCutKnob; }
    PeqKnob& getHighBoostKnob() noexcept { return highBoostKnob; }
    PeqKnob& getBandwidthKnob() noexcept { return bandwidthKnob; }
    PeqKnob& getHighCutKnob() noexcept { return highCutKnob; }
    // PEQ-04: the EQ Mode dropdown (in its own row under the title bar, outside the blue panel) and the controls of each mode.
    juce::ComboBox& getModeBox() noexcept { return modeBox; }
    juce::Label& getModeLabel() noexcept { return modeLabel; }
    int getShownMode() const noexcept { return shownMode; }
    PeqKnob& getMidLowKnob() noexcept { return midLowKnob; }
    PeqKnob& getMidDipKnob() noexcept { return midDipKnob; }
    PeqKnob& getMidHighKnob() noexcept { return midHighKnob; }
    PeqSwitch& getMidLowSwitch() noexcept { return midLowSwitch; }
    PeqSwitch& getMidDipSwitch() noexcept { return midDipSwitch; }
    PeqSwitch& getMidHighSwitch() noexcept { return midHighSwitch; }
    PeqSwitch& getHighPassSwitch() noexcept { return highPassSwitch; }
    PeqSwitch& getLowPassSwitch() noexcept { return lowPassSwitch; }
    juce::Rectangle<int> getMidLowGroup() const noexcept;
    juce::Rectangle<int> getMidDipGroup() const noexcept;
    juce::Rectangle<int> getMidHighGroup() const noexcept;
    juce::Rectangle<int> getHighPassGroup() const noexcept;
    juce::Rectangle<int> getLowPassGroup() const noexcept;
    juce::Rectangle<int> getMeterBounds() const noexcept { return outputPanel.getBounds(); }   // the Output panel holds the meter
    juce::Rectangle<int> getCurveBounds() const noexcept { return curve.getBounds(); }
    PeqSwitch& getLowSwitch() noexcept { return lowSwitch; }
    PeqSwitch& getHighBoostSwitch() noexcept { return highBoostSwitch; }
    PeqSwitch& getHighCutSwitch() noexcept { return highCutSwitch; }
    juce::Rectangle<int> getLowGroup() const noexcept;
    juce::Rectangle<int> getHighBoostGroup() const noexcept;
    juce::Rectangle<int> getHighCutGroup() const noexcept;
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    void refreshCurve();

    // Text the editor paints (the wording tests read these).
    static juce::String bandwidthText (float value);
    static juce::String boostText (float db);
    static juce::String cutText (float db);

private:
    void syncSizeFromWidth();
    void timerCallback() override { refreshCurve(); refreshHistoryButtons(); advanceMeter (1.0 / 30.0); }
    void refreshHistoryButtons();
    void rebuildPresetMenu();
    void refreshMode();   // the mode changed (the musician, a preset, automation or a loaded session): show its controls
    void presetChosen();
    void savePresetAsync();

    // All controls live here, laid out in design units, then scaled as one.
    struct Content : juce::Component
    {
        explicit Content (SongsmithPassiveEQEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithPassiveEQEditor& owner;
    };
    void paintContent (juce::Graphics&);
    void layoutContent();

    SongsmithPassiveEQProcessor& proc;
    Peq::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;   // until then, resizing does not overwrite the saved window width
    juce::Rectangle<int> titleBounds;   // the product name, painted

    Peq::UI::ResponseCurve curve;
    // songsmithstudio#16: the suite's Output panel on the right: the meter, a compact Output Trim and Auto Gain.
    Songsmith::UI::OutputPanel outputPanel { false };
    Songsmith::UI::OutputMeterDisplay& outputMeter = outputPanel.getMeters().getOutput();
    juce::AudioProcessorValueTreeState::SliderAttachment outputTrimAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAttachment;
    juce::String lastAutoGainText;

    PeqSwitch lowSwitch, highBoostSwitch, highCutSwitch;
    PeqKnob lowBoostKnob, lowCutKnob, highBoostKnob, bandwidthKnob, highCutKnob;

    // PEQ-04: Mid Shaper and Frequency Filter controls (shown only in their mode), and the mode selector.
    PeqSwitch midLowSwitch, midDipSwitch, midHighSwitch, highPassSwitch, lowPassSwitch;
    PeqKnob midLowKnob, midDipKnob, midHighKnob;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::ParameterAttachment modeAttachment;
    int shownMode = 0;
    int filterKey = -1;   // the Frequency Filter group paints the chosen frequencies, so it repaints when they change

    juce::TextButton bypassButton { "Bypass" };
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPassiveEQEditor)
};
