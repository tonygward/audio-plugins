#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CapturePanels.h"
#include "PluginProcessor.h"
#include "ResponseGraph.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithHeader.h"
#include "../../shared/Source/SongsmithLookAndFeel.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

// The look of the Songsmith Plugin Analyzer: the suite's dark panel, bevelled controls and Inter typeface from the shared library,
// with one accent colour (a fresh green). Everything is drawn as vectors in design units, so it scales to any size and display.
//
// ANALYZER-02: Start drives everything. On the After Plugin, Start makes the first capture. Pressing it again asks whether to replace
// a capture or add a second one to compare, and both curves are drawn on the same graph. Under the graph, each capture has a Show
// switch and a name you can change. The two plugins find each other on their own, so there is no session or slot to choose; a Pair With
// box appears only when they cannot tell which pair they belong to.
namespace PluginAnalyzer::UI
{
    class LookAndFeel : public Songsmith::UI::LookAndFeelBase
    {
    public:
        LookAndFeel() : LookAndFeelBase (Colours::accent)
        {
            setColour (juce::Label::textColourId, Songsmith::UI::Colours::text);
            setColour (juce::ProgressBar::backgroundColourId, Songsmith::UI::Colours::recess);
            setColour (juce::ProgressBar::foregroundColourId, Colours::accent);
        }
    };
}

class SongsmithPluginAnalyzerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SongsmithPluginAnalyzerEditor (SongsmithPluginAnalyzerProcessor&);
    ~SongsmithPluginAnalyzerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // What pressing Start would do (the same as clicking the button). Shows the choice panel when a capture would be affected.
    void pressStart();
    // Starts a measurement into a capture, with an optional name, without asking (what the choice panel does when you choose).
    void startMeasurementInto (int captureIndex, const juce::String& name);
    // True when pressing Start has to ask first, because a capture would otherwise be replaced or added to.
    bool startNeedsChoice() const;

    // For tests and screenshots.
    juce::ComboBox& getRoleBox() noexcept { return roleBox; }
    juce::ComboBox& getPairBox() noexcept { return pairBox; }
    juce::Label& getPairLabel() noexcept { return pairLabel; }
    juce::ComboBox& getChannelBox() noexcept { return channelBox; }
    juce::Slider& getLevelSlider() noexcept { return levelSlider; }
    juce::TextButton& getStartButton() noexcept { return startButton; }
    juce::TextButton& getStopButton() noexcept { return stopButton; }
    juce::TextButton& getClearButton() noexcept { return clearButton; }
    juce::TextButton& getExportButton() noexcept { return exportButton; }
    juce::TextButton& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getRangeBox() noexcept { return rangeBox; }
    PluginAnalyzer::UI::CaptureRow& getCaptureRow (int index) noexcept { return *captureRows[(size_t) index]; }
    PluginAnalyzer::UI::ChoicePanel& getChoicePanel() noexcept { return choicePanel; }
    juce::Label& getStatusLabel() noexcept { return statusLabel; }
    juce::Label& getNotesLabel() noexcept { return notesLabel; }
    juce::Label& getHintLabel() noexcept { return hintLabel; }
    PluginAnalyzer::UI::ResponseGraph& getGraph() noexcept { return graph; }
    Songsmith::UI::MeterColumn& getMeterColumn() noexcept { return meterColumn; }
    juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
    juce::Rectangle<int> getDiagnosticsBounds() const { return diagnostics->getBounds(); }
    float getContentScale() const noexcept { return (float) getWidth() / (float) PluginAnalyzer::EditorLayout::designWidth; }

    // Brings every control up to date with the processor (the timer does this about thirty times a second).
    void refresh();

private:
    void timerCallback() override { refresh(); }
    void syncSizeFromWidth();
    void layoutContent();
    void exportCsv();
    void openChoice();
    void openClearQuestion();
    void applyChoice (int id, const juce::String& typedName, bool nameEdited);

    struct Content : juce::Component
    {
        explicit Content (SongsmithPluginAnalyzerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithPluginAnalyzerEditor& owner;
    };
    void paintContent (juce::Graphics&);

    // The ids the choice panel hands back.
    static constexpr int replaceFirstId = 100, replaceSecondId = 101, addSecondId = 200, clearAllId = 300;

    SongsmithPluginAnalyzerProcessor& proc;
    PluginAnalyzer::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;
    juce::Rectangle<int> titleBounds;

    juce::Label roleLabel, pairLabel, channelLabel, levelLabel, levelValue, rangeLabel, notesTitle;
    juce::ComboBox roleBox, pairBox, channelBox;
    juce::Slider levelSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox rangeBox;
    juce::TextButton startButton { "Start" }, stopButton { "Stop" }, clearButton { "Clear All" }, exportButton { "Export CSV..." };
    juce::TextButton bypassButton { "Bypass" };
    juce::Label statusLabel, notesLabel, hintLabel;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    PluginAnalyzer::UI::ResponseGraph graph;
    std::array<std::unique_ptr<PluginAnalyzer::UI::CaptureRow>, (size_t) PluginAnalyzer::numSlots> captureRows;
    PluginAnalyzer::UI::ChoicePanel choicePanel;

    // Made after the boxes have their items, or the saved choice would find nothing to select.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment, pairAttachment, channelAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    Songsmith::UI::MeterColumn meterColumn { false };   // SUI-23: the Output meter at the far right (a measuring tool turns nothing down)
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    int pendingAddSlot = -1;   // the empty capture an "Add to Compare" choice will fill

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPluginAnalyzerEditor)
};
