#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "AnalysisRunner.h"
#include "AnalyzerGraphs.h"
#include "AnalyzerLookAndFeel.h"
#include "AnalyzerText.h"
#include "GuidanceView.h"
#include "../../shared/Source/SongsmithFileAdvice.h"
#include "../../shared/Source/SongsmithFileAnalysis.h"
#include "../../shared/Source/SongsmithType.h"

// REC-02: the Songsmith Audio Analyzer window. One job: open a finished WAV, AIFF, FLAC or MP3 file, show what it
// measures (integrated loudness, true peak, sample rate, bit depth and encoding, duration), and give short,
// optional, plain-English guidance. Then analyse another file.
//
// ANA-03: the results are a small dashboard. The principal picture is the True Peak chart (the highest peak in the whole
// file, per channel). Integrated Loudness is the large number, the waveform shows how big the sound is through the song,
// and the sample rate, encoding, duration and channels sit compactly below. Everything is a whole-file measurement.
namespace Songsmith::Analyzer
{
    namespace Layout
    {
        // Design units; the window scales this as a whole (75 % to 200 %).
        constexpr int designWidth = 760, designHeight = 800;
        constexpr int margin = 24, titleBarHeight = 52;
        constexpr int buttonHeight = 30;

        // The results dashboard. Left column: Integrated Loudness, then the waveform. Right column: the True Peak reading,
        // then its chart. Under both: four compact facts. Then the guidance.
        constexpr int columnGap = 20, rightColumnWidth = 252;
        constexpr int loudnessHeight = 104, peakTileHeight = 86, peakChartHeight = 226, blockGap = 12, smallGap = 8;
        constexpr int factsHeight = 78, factsGap = 12, factsColumns = 4, bitDepthFactWidth = 212;
        // The large Integrated Loudness number. A little over the title size, because it is the figure to read first.
        constexpr float heroValueHeight = 40.0f;
    }

    // A reading box: a caption, the value in large type, and one short note.
    class ReadingTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ReadingTile();
        // The large version, for the figure to read first, and the compact one for the facts under the charts.
        void setProminent (bool shouldBeProminent);
        void setCompact (bool shouldBeCompact);
        void setReading (const Reading& r);
        void paint (juce::Graphics&) override;
        void resized() override;
        const Reading& getReading() const noexcept { return reading; }
        juce::Label& getCaption() noexcept { return caption; }
        juce::Label& getValue() noexcept { return value; }
        juce::Label& getNote() noexcept { return note; }

    private:
        Reading reading;
        juce::Label caption, value, note;
        bool prominent = false, compact = false;
    };

    class AnalyzerComponent : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
    {
    public:
        enum class State { waiting, reading, showing };

        AnalyzerComponent();
        ~AnalyzerComponent() override;

        // Begin analysing this file (cancels any file being read). Message thread.
        void analyse (const juce::File& file);
        // Show a finished analysis without reading a file (the same path the background reader ends in).
        void showResult (const juce::File& file, const FileAnalysis::Result& result);
        // Open the file chooser.
        void chooseFile();
        // Back to the waiting screen (cancels any file being read).
        void reset();

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        bool isInterestedInFileDrag (const juce::StringArray& files) override;
        void filesDropped (const juce::StringArray& files, int x, int y) override;
        void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
        void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }

        // For tests and screenshots.
        State getState() const noexcept { return state; }
        const FileAnalysis::Result& getResult() const noexcept { return result; }
        const FileAdvice::Advice& getAdvice() const noexcept { return advice; }
        juce::File getFile() const { return file; }
        juce::TextButton& getChooseButton() noexcept { return chooseButton; }
        juce::TextButton& getAnotherButton() noexcept { return anotherButton; }
        juce::TextButton& getCancelButton() noexcept { return cancelButton; }
        ReadingTile& getTile (int index) noexcept { return *tiles[(size_t) index]; }
        int getNumTiles() const noexcept { return (int) tiles.size(); }
        GuidanceViewport& getGuidance() noexcept { return guidance; }
        TruePeakDisplay& getPeakDisplay() noexcept { return peakDisplay; }
        WaveformDisplay& getWaveform() noexcept { return waveform; }
        juce::Label& getFileNameLabel() noexcept { return fileNameLabel; }
        juce::Label& getFileDetailLabel() noexcept { return fileDetailLabel; }
        juce::Label& getStatusLabel() noexcept { return statusLabel; }
        juce::ProgressBar& getProgressBar() noexcept { return progressBar; }
        juce::Rectangle<int> getDropPanelBounds() const noexcept { return dropPanel; }
        juce::Rectangle<int> getGuidanceBounds() const noexcept { return guidanceArea; }
        juce::Rectangle<int> getTitleBounds() const noexcept { return titleBounds; }
        bool isReading() const noexcept { return runner.isRunning(); }

    private:
        void timerCallback() override;
        void updateVisibility();
        void finished (const juce::File&, const FileAnalysis::Result&);

        LookAndFeel lnf;
        AnalysisRunner runner;
        std::unique_ptr<juce::FileChooser> chooser;

        State state = State::waiting;
        juce::File file;
        FileAnalysis::Result result;
        FileAdvice::Advice advice;
        double progress = 0.0;
        bool dragOver = false;

        juce::Rectangle<int> titleBounds, dropPanel, guidanceArea;
        juce::String extraNote;   // under the guidance: "Only the first of several dropped files is analysed", or empty
        std::unique_ptr<juce::TooltipWindow> tooltipWindow;

        juce::Label dropLabel, orLabel, privacyLabel, fileNameLabel, fileDetailLabel, statusLabel, footerHint, versionLabel;
        juce::TextButton chooseButton, anotherButton, cancelButton;
        juce::ProgressBar progressBar { progress };
        std::vector<std::unique_ptr<ReadingTile>> tiles;
        TruePeakDisplay peakDisplay;
        WaveformDisplay waveform;
        GuidanceViewport guidance;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyzerComponent)
    };

    // The window's content: the analyzer at its design size, scaled as a whole to the window.
    class AnalyzerRoot : public juce::Component
    {
    public:
        AnalyzerRoot()
        {
            addAndMakeVisible (content);
            setSize (Layout::designWidth, Layout::designHeight);
        }
        void resized() override
        {
            content.setBounds (0, 0, Layout::designWidth, Layout::designHeight);
            content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Layout::designWidth));
        }
        AnalyzerComponent content;
    };
}
