#pragma once

#include <juce_core/juce_core.h>
#include "SongsmithFileAnalysis.h"

// REC-01: turns a file analysis into plain, optional advice.
//
// Every message answers: what matters, why, and whether the musician needs to do anything. Measured
// facts, listening suggestions and things the tool cannot know are kept apart. No scores, no alarm
// badges, no pass or fail, and no platform targets: streaming services normalise in their own ways
// and listeners can switch normalisation off, so only conditional, general wording is used.
// (No platform-specific claim is made here because no dated official source is cited for one.)
namespace Songsmith::FileAdvice
{
    enum class Kind { noChangeNecessary, tryIfHearingAProblem, forNextRecording };

    inline juce::String heading (Kind k)
    {
        switch (k)
        {
            case Kind::noChangeNecessary:      return "No change necessary";
            case Kind::tryIfHearingAProblem:   return "Try this if you're hearing a problem";
            case Kind::forNextRecording:       return "For your next recording";
        }
        return {};
    }

    struct Item { Kind kind; juce::String text; };

    struct Advice
    {
        juce::String summary;             // one or two sentences, shown first
        juce::StringArray measured;       // facts read from the file
        std::vector<Item> steps;          // optional next steps, grouped by kind when shown
        juce::StringArray cannotKnow;     // honest limits
    };

    inline juce::String formatKHz (double hz)
    {
        return juce::String (hz / 1000.0, hz == std::floor (hz / 1000.0) * 1000.0 ? 0 : 1) + " kHz";
    }

    inline juce::String formatDuration (double seconds)
    {
        const int total = (int) std::lround (seconds);
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }

    // ANA-02: "about 192 kilobits per second", spelled out. Empty when the file's own size and length give no average.
    inline juce::String describeBitrate (const FileAnalysis::Result& r)
    {
        return r.averageBitrateKbps > 0 ? "about " + juce::String (r.averageBitrateKbps) + " kilobits per second" : juce::String();
    }

    inline juce::String describeFormat (const FileAnalysis::Result& r)
    {
        // A lossy file has no bit depth to show: it stores how the sound is compressed, not how finely each sample is kept.
        if (r.lossy)
        {
            const auto rate = describeBitrate (r);
            return r.formatName + ", " + formatKHz (r.sampleRate) + (rate.isNotEmpty() ? ", " + rate : juce::String());
        }
        const auto depth = r.floatingPoint ? juce::String (r.storedBits > 0 ? r.storedBits : 32) + "-bit floating point"
                                           : (r.storedBits > 0 ? juce::String (r.storedBits) + "-bit" : juce::String ("unknown bit depth"));
        return r.formatName + ", " + formatKHz (r.sampleRate) + ", " + depth;
    }

    inline Advice adviseOn (const FileAnalysis::Result& r)
    {
        Advice a;
        if (! r.opened)
        {
            a.summary = r.error.isNotEmpty() ? r.error : "This file could not be analysed.";
            a.steps.push_back ({ Kind::noChangeNecessary, "Your original file has not been touched. If you expected it to open, try exporting it again as a WAV, AIFF, FLAC or MP3 file." });
            return a;
        }

        // Measured facts.
        a.measured.add ("Format: " + describeFormat (r));
        a.measured.add ("Channels: " + juce::String (r.channels) + (r.channels == 1 ? " (mono)" : r.channels == 2 ? " (stereo)" : ""));
        a.measured.add ("Length: " + formatDuration (r.durationSeconds));
        if (r.lossy && r.averageBitrateKbps > 0)
            a.measured.add ("Average bitrate: " + describeBitrate (r) + " (worked out from the file's size and length)");
        if (r.silent)
        {
            a.summary = "This file is completely silent, so there is nothing to measure.";
            a.steps.push_back ({ Kind::noChangeNecessary, "Nothing to do unless you expected sound. If you did, check that the file exported or recorded properly." });
            return a;
        }
        a.measured.add ("Sample peak: " + juce::String (r.samplePeakDb, 1) + " dBFS");
        a.measured.add ("Maximum true peak: " + juce::String (r.truePeakDb, 1) + " dBTP");
        if (r.hasIntegratedLoudness)
            a.measured.add ("Integrated loudness: " + juce::String (r.integratedLufs, 1) + " LUFS");
        else
            a.measured.add ("Integrated loudness: not available, because the file is too short or too quiet to measure.");
        if (r.hasLoudnessRange)
            a.measured.add ("Loudness range: " + juce::String (r.loudnessRange, 1) + " dB");

        // Recording settings: only what the file really says.
        // A lossy file (MP3) is advised on separately: it has no bit depth, and what it measures is the decoded sound.
        const bool cdQuality = ! r.lossy && ! r.floatingPoint && r.storedBits == 16 && std::abs (r.sampleRate - 44100.0) < 1.0;
        const bool goodRecording = ! r.lossy && ! r.floatingPoint && r.storedBits >= 24 && r.sampleRate >= 44100.0;
        if (r.lossy)
        {
            a.summary = "This is an MP3, a compressed (lossy) file. It is fine for sharing and listening, and the readings here are for the sound as it plays back from this file.";
            a.steps.push_back ({ Kind::noChangeNecessary, "An MP3 does not have a bit depth the way a WAV does, so none is shown. Nothing needs changing." });
            a.steps.push_back ({ Kind::noChangeNecessary, "Loudness and true peak are measured on the sound decoded from the MP3, so they can differ a little from the WAV it was made from. Compression can also push the loudest peaks slightly higher." });
            a.steps.push_back ({ Kind::forNextRecording, "Keep a WAV, AIFF or FLAC copy of your finished mix. An MP3 is best made last, because it cannot be turned back into the original." });
        }
        else if (cdQuality)
        {
            a.summary = "Your recording is CD quality and fine for mixing or streaming. There's no need to record it again.";
            a.steps.push_back ({ Kind::noChangeNecessary, "44.1 kHz and 16-bit are well-established settings for music. Leave this file as it is." });
            a.steps.push_back ({ Kind::forNextRecording, "24-bit recording gives you more flexibility with recording levels, so you can leave more room below the loudest peaks. It does not change this file." });
        }
        else if (goodRecording)
        {
            a.summary = "These are good recording settings. You can focus on the performance and the mix.";
            a.steps.push_back ({ Kind::noChangeNecessary, "Nothing needs changing for the sake of the file format." });
        }
        else if (r.floatingPoint)
        {
            a.summary = "This is a 32-bit floating point file, which is usually what a music program exports for further editing. Nothing needs changing.";
        }
        else
        {
            a.summary = "These settings are fine for music. Nothing needs changing for the sake of the file format.";
        }

        // Stored versus original depth.
        if (! r.lossy && r.storedBits >= 24 && r.bitsInUse > 0 && r.bitsInUse <= 16)
        {
            a.measured.add ("Bits in use: the file is stored as " + juce::String (r.storedBits) + "-bit, but every value fits in " + juce::String (r.bitsInUse) + " bits.");
            a.steps.push_back ({ Kind::noChangeNecessary, "This file is stored at a higher bit depth than it appears to have been recorded at. That is harmless, and it does not make the recording worse." });
        }
        if (r.lossy)
        {
            a.cannotKnow.add ("How the MP3 was made: the settings of the original recording, and the loudness and true peak it had before it was turned into an MP3.");
            a.cannotKnow.add ("The start and end of an MP3 can hold a very short stretch of silence added when it was made. It counts towards the length and has almost no effect on the loudness reading.");
        }
        else
        {
            a.cannotKnow.add ("The settings the sound was first recorded at. A file's format shows how it is stored, which can differ from how it was captured, and your music program may process audio at 32-bit floating point internally.");
        }

        // Level and clipping, cautiously.
        const bool overFullScale = r.samplesAboveFullScale > 0;
        if (r.clippedRuns > 0 || overFullScale)
        {
            a.measured.add (overFullScale ? "Samples above full scale: " + juce::String (r.samplesAboveFullScale)
                                          : "Runs of three or more samples at the highest possible level: " + juce::String (r.clippedRuns));
            a.steps.push_back ({ Kind::noChangeNecessary, "Some stretches reach the top of the scale. That is sometimes a deliberate choice in loud music, and it is not proof that the original recording was spoiled." });
            a.steps.push_back ({ Kind::tryIfHearingAProblem, "If you hear harshness or crackle on the loudest moments, check the loudest parts in your music program and turn that part down a little before exporting again." });
            a.steps.push_back ({ Kind::forNextRecording, "Leave a little more room below the top when recording, for example peaks around -12 to -6 dBFS." });
            a.cannotKnow.add ("Whether the original recording clipped. A flat top on a waveform can come from recording, mixing or mastering, and this tool only sees the finished file.");
        }
        else if (r.truePeakDb > 0.0)
        {
            a.steps.push_back ({ Kind::noChangeNecessary, "The true peak is slightly above full scale. The stored samples are not clipped, but some players and file converters may add a little distortion on the loudest moments." });
            a.steps.push_back ({ Kind::tryIfHearingAProblem, "If you hear distortion after converting, lower the final level by about 1 dB and export again." });
        }

        // Loudness and streaming, generally and conditionally.
        if (r.hasIntegratedLoudness)
        {
            a.steps.push_back ({ Kind::noChangeNecessary, "Streaming services may adjust playback loudness, and listeners can switch that off, so there is no single loudness your track has to reach. This tool does not know which services or settings you will be heard on." });
            if (r.integratedLufs > -9.0)
                a.steps.push_back ({ Kind::tryIfHearingAProblem, "This is a very loud track. Where playback is levelled, a loud track is turned down, so extra loudness may not make it play louder. If it sounds squashed to you, a little less limiting can help." });
            else if (r.integratedLufs < -24.0)
                a.steps.push_back ({ Kind::noChangeNecessary, "This is a quiet track. That suits a mix in progress, and you can simply turn it up to listen." });
        }
        return a;
    }
}
