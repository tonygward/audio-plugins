#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "AnalyzerLookAndFeel.h"
#include "../../shared/Source/SongsmithFileAnalysis.h"
#include "../../shared/Source/SongsmithColours.h"
#include "../../shared/Source/SongsmithTube.h"
#include "../../shared/Source/SongsmithType.h"

// ANA-03: the two pictures in the Audio Analyzer's results.
//
//  - TruePeakDisplay: the principal chart. One tall bar per channel (Left and Right, one for Mono) showing the highest
//    true peak in the whole file on a scale from -48 to +6 dBTP, with 0 dBTP marked, with the highest stored
//    sample as a small tick beside it. It is a whole-file result, not a live meter, and is labelled that way.
//  - WaveformDisplay: how big the sound is from the start of the file to the end. Never a loudness chart and never a
//    true peak chart: it holds the largest sample of each slice of the file and says so.
//
// Both only draw what FileAnalysis::Result measured. The numbers they show are the measurements, so a test can compare
// every drawn position with the value it stands for.
namespace Songsmith::Analyzer
{
    // The scale of the True Peak chart: the Output meter's colours and 0 dB line, from -48 to +6 dBTP so that every mark has room.
    // A level below -48 sits on the bottom edge and a level above +6 on the top edge; the number beneath each bar is always the true figure.
    namespace PeakScale
    {
        constexpr float minDb = -48.0f, maxDb = 6.0f;
        constexpr float marks[] = { 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };

        // 0 at -48 dB (the bottom of the bars), 1 at +6 dB (the top), clamped.
        inline float proportion (double db) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, ((float) db - minDb) / (maxDb - minDb));
        }

        // A level at or above full scale is over the top of the scale.
        inline bool isOverFullScale (double db) noexcept { return db >= 0.0; }

        inline const juce::Colour low  { 0xff3ddbc4 };   // the Output meter's colours
        inline const juce::Colour high { 0xfff2c94c };
        inline const juce::Colour over { 0xffff5c5c };
    }

    class TruePeakDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        TruePeakDisplay();

        // Takes the channel peaks of a measured file. A file with more than two channels shows one bar, the highest of all.
        void setResult (const FileAnalysis::Result& r);
        void clear();

        int getNumBars() const noexcept { return (int) bars.size(); }
        juce::String getBarName (int bar) const { return bars[(size_t) bar].name; }
        double getBarTruePeakDb (int bar) const { return bars[(size_t) bar].truePeakDb; }
        double getBarSamplePeakDb (int bar) const { return bars[(size_t) bar].samplePeakDb; }
        juce::String getBarReadout (int bar) const;
        // Where a level is drawn, in this component's own coordinates.
        float yForDb (double db) const noexcept;
        juce::Rectangle<float> getBarBounds (int bar) const;
        juce::Rectangle<float> getScaleBounds() const;
        juce::Label& getHeading() noexcept { return heading; }
        juce::Label& getLegend() noexcept { return legend; }
        bool isSilent() const noexcept { return silent; }
        bool isOverFullScale() const noexcept;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Bar { juce::String name; double truePeakDb, samplePeakDb; };
        std::vector<Bar> bars;
        bool silent = false;
        juce::Label heading, legend;

        void describe();
    };

    class WaveformDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        WaveformDisplay();

        void setResult (const FileAnalysis::Result& r);
        void clear();

        const std::vector<float>& getPeaks() const noexcept { return peaks; }
        double getDurationSeconds() const noexcept { return durationSeconds; }
        bool isSilent() const noexcept { return silent; }
        // The slice with the largest sample, as a fraction of the way through the file (0 to 1).
        double getLoudestPosition() const noexcept;
        // The plot area, and how tall (one side of the centre line) a slice is drawn.
        juce::Rectangle<float> getPlotBounds() const;
        float heightOfSlice (int index) const;
        juce::Label& getHeading() noexcept { return heading; }
        juce::Label& getStart() noexcept { return startLabel; }
        juce::Label& getEnd() noexcept { return endLabel; }
        juce::Label& getNote() noexcept { return note; }

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        std::vector<float> peaks;
        double durationSeconds = 0.0;
        bool silent = false;
        juce::Label heading, startLabel, endLabel, note;

        void describe();
    };
}
