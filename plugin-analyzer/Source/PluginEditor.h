#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ResponseGraph.h"
#include "../../shared/Source/SongsmithGuidance.h"
#include "../../shared/Source/SongsmithHeader.h"
#include "../../shared/Source/SongsmithLookAndFeel.h"
#include "../../shared/Source/SongsmithMeterColumn.h"

// The look of the Songsmith Plugin Analyzer: the suite's dark panel, bevelled controls and Inter typeface from the shared library,
// with one accent colour (a fresh green). Everything is drawn as vectors in design units, so it scales to any size and display.
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

    // For tests and screenshots.
    juce::ComboBox& getRoleBox() noexcept { return roleBox; }
    juce::ComboBox& getSessionBox() noexcept { return sessionBox; }
    juce::ComboBox& getChannelBox() noexcept { return channelBox; }
    juce::Slider& getLevelSlider() noexcept { return levelSlider; }
    juce::TextButton& getStartButton() noexcept { return startButton; }
    juce::TextButton& getStopButton() noexcept { return stopButton; }
    juce::TextButton& getResetButton() noexcept { return resetButton; }
    juce::TextButton& getExportButton() noexcept { return exportButton; }
    juce::TextButton& getBypassButton() noexcept { return bypassButton; }
    juce::ComboBox& getMeasureIntoBox() noexcept { return measureIntoBox; }
    juce::ComboBox& getShowBox() noexcept { return showBox; }
    juce::ComboBox& getCompareBox() noexcept { return compareBox; }
    juce::ComboBox& getRangeBox() noexcept { return rangeBox; }
    juce::TextEditor& getNameEditor() noexcept { return nameEditor; }
    juce::Label& getStatusLabel() noexcept { return statusLabel; }
    juce::Label& getNotesLabel() noexcept { return notesLabel; }
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
    void rebuildSlotBoxes();
    void exportCsv();

    struct Content : juce::Component
    {
        explicit Content (SongsmithPluginAnalyzerEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        SongsmithPluginAnalyzerEditor& owner;
    };
    void paintContent (juce::Graphics&);

    SongsmithPluginAnalyzerProcessor& proc;
    PluginAnalyzer::UI::LookAndFeel lnf;
    Content content { *this };
    bool constructed = false;
    juce::Rectangle<int> titleBounds;

    juce::Label roleLabel, sessionLabel, channelLabel, levelLabel, levelValue, measureIntoLabel, showLabel, compareLabel, rangeLabel, nameLabel, notesTitle;
    juce::ComboBox roleBox, sessionBox, channelBox;
    juce::Slider levelSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox measureIntoBox, showBox, compareBox, rangeBox;
    juce::TextEditor nameEditor;
    juce::TextButton startButton { "Start" }, stopButton { "Stop" }, resetButton { "Reset" }, exportButton { "Export CSV..." };
    juce::TextButton bypassButton { "Bypass" };
    juce::Label statusLabel, notesLabel, hintLabel;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    PluginAnalyzer::UI::ResponseGraph graph;

    juce::AudioProcessorValueTreeState::ComboBoxAttachment roleAttachment, sessionAttachment, channelAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment levelAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    Songsmith::UI::MeterColumn meterColumn { false };   // SUI-23: the Output meter at the far right (a measuring tool turns nothing down)
    std::unique_ptr<Songsmith::Diagnostics::Bar> diagnostics;
    Songsmith::Guidance::TitleHint titleHint { JucePlugin_Name };
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String shownNames[4];   // the slot names the boxes were last built with
    int lastStatus = -1;
    int lastMeasurementCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPluginAnalyzerEditor)
};
