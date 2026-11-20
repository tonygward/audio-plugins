#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include "../../shared/Source/SongsmithFileAdvice.h"

// REC-02: the words and numbers the Audio Analyzer window shows for one file's measurements.
//
// Kept apart from the window so a test can check every reading without drawing anything. A measurement that
// is missing is said to be missing, in plain words. A sample peak is never shown as a true peak.
namespace Songsmith::Analyzer
{
    struct Reading
    {
        juce::String caption;   // "Integrated Loudness"
        juce::String value;     // "-14.2 LUFS", or "Not available"
        juce::String note;      // one short line under the value, or empty
        juce::String tooltip;   // what the reading means, in a sentence or two
    };

    inline juce::String oneDecimal (double v)
    {
        const double rounded = std::round (v * 10.0) / 10.0;
        return juce::String (rounded == 0.0 ? 0.0 : rounded, 1);   // never "-0.0"
    }

    inline juce::String formatLoudness (double lufs) { return oneDecimal (lufs) + " LUFS"; }

    inline juce::String formatPeak (double db)
    {
        return std::isfinite (db) ? oneDecimal (db) + " dBTP" : juce::String ("Silent");
    }

    // Minutes and seconds to a tenth of a second; hours when there are some: 0:02.5, 3:25.4, 1:02:03.
    inline juce::String formatLength (double seconds)
    {
        if (seconds >= 3600.0)
        {
            const int total = (int) std::lround (seconds);
            return juce::String (total / 3600) + ":" + juce::String ((total % 3600) / 60).paddedLeft ('0', 2) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
        }
        const int tenths = (int) std::lround (seconds * 10.0);
        return juce::String (tenths / 600) + ":" + juce::String ((tenths % 600) / 10).paddedLeft ('0', 2) + "." + juce::String (tenths % 10);
    }

    inline juce::String describeChannels (int channels)
    {
        return channels == 1 ? "Mono" : channels == 2 ? "Stereo" : juce::String (channels) + " channels";
    }

    inline juce::String describeBitDepth (const FileAnalysis::Result& r)
    {
        if (r.lossy)
            return "Not applicable";   // a compressed file has no bit depth to show, and none is made up
        const int bits = r.storedBits > 0 ? r.storedBits : (r.floatingPoint ? 32 : 0);
        return bits > 0 ? juce::String (bits) + "-bit" : juce::String ("Unknown");
    }

    inline juce::String describeEncoding (const FileAnalysis::Result& r)
    {
        // Short enough for its box. The average bitrate is in the line under the file's name and in the measured facts.
        if (r.lossy)
            return r.formatName + ", lossy";
        if (r.floatingPoint)
            return r.formatName == "FLAC" ? "FLAC, floating point" : r.formatName + ", floating point";
        if (r.formatName == "FLAC")
            return "FLAC, lossless";
        return r.formatName + ", uncompressed";
    }

    // The readings, in reading order. Empty when the file could not be read.
    inline std::vector<Reading> readingsFor (const FileAnalysis::Result& r)
    {
        std::vector<Reading> out;
        if (! r.opened)
            return out;

        Reading loud { "Integrated Loudness", {}, {},
                       "How loud the whole file is on average, measured the way broadcasters and streaming services measure it. "
                       "Lower numbers are quieter. There is no single number a track has to reach." };
        if (r.silent)
        {
            loud.value = "Not available";
            loud.note = "The file is silent";
        }
        else if (r.hasIntegratedLoudness)
            loud.value = formatLoudness (r.integratedLufs);
        else
        {
            loud.value = "Not available";
            loud.note = "Too short or too quiet to measure";
        }
        out.push_back (loud);

        Reading peak { "True Peak", {}, {},
                       "The highest level the sound reaches once the stored samples are turned back into a smooth wave. It can be a little above "
                       "the highest stored sample. 0 dBTP is the top of the scale." };
        peak.value = formatPeak (r.truePeakDb);
        peak.note = r.silent ? "The file is silent" : (r.lossy ? "Highest sample after decoding: " : "Highest stored sample: ") + oneDecimal (r.samplePeakDb) + " dBFS";
        out.push_back (peak);

        out.push_back ({ "Sample Rate", FileAdvice::formatKHz (r.sampleRate), {},
                         "How many times a second the sound is stored. 44.1 kHz and 48 kHz are the usual choices for music." });

        out.push_back ({ "Bit Depth and Encoding", describeBitDepth (r), describeEncoding (r),
                         "How finely each sample is stored, and how the file is stored. It shows how this file is kept, which can differ from how "
                         "the sound was first recorded. A compressed file such as an MP3 has no bit depth, so none is shown." });

        out.push_back ({ "Duration", formatLength (r.durationSeconds), {}, "How long the file plays." });

        out.push_back ({ "Channels", describeChannels (r.channels), {}, "How many separate channels of sound the file holds." });
        return out;
    }

    // The one line under the file name: what the file is.
    inline juce::String describeFile (const FileAnalysis::Result& r)
    {
        return r.opened ? FileAdvice::describeFormat (r) + ", " + describeChannels (r.channels).toLowerCase() : juce::String();
    }

    // Measured facts that the reading boxes do not already show (clipping, loudness range, bits in use...).
    inline juce::StringArray extraMeasurements (const FileAdvice::Advice& a)
    {
        juce::StringArray out;
        for (auto& line : a.measured)
            if (! line.startsWith ("Format:") && ! line.startsWith ("Channels:") && ! line.startsWith ("Length:")
                && ! line.startsWith ("Maximum true peak:") && ! line.startsWith ("Integrated loudness:"))
                out.add (line);
        return out;
    }
}
