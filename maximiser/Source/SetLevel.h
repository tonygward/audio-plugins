#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include "MaxParams.h"
#include "BrickwallLimiter.h"
#include "Dither.h"
#include "Loudness.h"

// Set Level for the Maximiser (songsmithstudio#18): the musician plays part of the song, types a target loudness in LUFS,
// and the plugin finds the Threshold that brings the output's integrated loudness to that target.
//
// Loudness matching, not a promise about every moment. The target is the integrated loudness (ITU-R BS.1770-4 with the
// EBU R 128 gating, the same meter as the Integrated LUFS readout) of the sound that was played, measured after the
// limiter. The level still moves with the music, and a different part of the song can measure differently.
//
// How it measures: the captured input is replayed through the Maximiser's own limiter with the settings in use (Output
// Ceiling, Release, Auto Release, Peak mode, the dither headroom), so the figure is real, not an estimate from a formula.
// The search runs at the host's rate; the chosen Threshold is then measured again through the oversampled path when
// Oversampling is on, and nudged once if that differs. Only Threshold changes. The Output Ceiling and every other control
// stay where the musician put them, so the limiter still keeps the peaks under the ceiling. A target that cannot be
// reached within the Threshold's range is reported, never forced.
//
// Nothing here touches the audio thread: it runs on a background thread once listening has finished.
namespace Max::SetLevel
{
    inline constexpr double defaultTargetLufs = -14.0;
    inline constexpr double minTargetLufs = -40.0, maxTargetLufs = -6.0;   // a realistic range for a finished track
    inline constexpr double minSeconds = 3.0;                // shorter than this is too little to judge
    inline constexpr double listenSeconds = 30.0;            // the longest it listens (it stops on its own)
    inline constexpr double reachedLu = 0.3;                 // closer than this to the target counts as reached
    inline constexpr float heavyLimitingDb = 6.0f;           // more average gain reduction than this: say to listen for distortion
    inline constexpr float quietBlockDb = -60.0f;            // blocks quieter than this are left out of the average reduction

    enum class Status { good, silent, tooShort, louderThanPossible, quieterThanPossible, stopped };

    // The controls the measurement used. Apply only goes ahead if none of them changed since.
    struct Settings
    {
        float thresholdDb = Max::defaultThresholdDb, ceilingDb = Max::defaultCeilingDb, releaseMs = Max::defaultReleaseMs;
        bool truePeak = true, autoRelease = false;
        int dither = 0, oversampling = 0;

        bool operator== (const Settings& o) const noexcept
        {
            return std::abs (thresholdDb - o.thresholdDb) < 0.005f && std::abs (ceilingDb - o.ceilingDb) < 0.005f
                   && std::abs (releaseMs - o.releaseMs) < 1.0e-4f * juce::jmax (1.0f, releaseMs)
                   && truePeak == o.truePeak && autoRelease == o.autoRelease && dither == o.dither && oversampling == o.oversampling;
        }
        bool operator!= (const Settings& o) const noexcept { return ! (*this == o); }
    };

    struct Measurement
    {
        bool valid = false;
        float lufs = Max::dbFloor;              // integrated loudness of the limiter's output
        float averageReductionDb = 0.0f;        // the limiter's gain reduction, averaged over the sound (positive dB)
    };

    struct Result
    {
        Status status = Status::silent;
        double seconds = 0.0;                   // how much sound was measured
        double targetLufs = defaultTargetLufs;
        float inputLufs = Max::dbFloor;         // what came in, before the Maximiser
        float currentLufs = Max::dbFloor;       // what comes out with the Threshold in use now
        float currentThresholdDb = 0.0f;
        float thresholdDb = 0.0f;               // suggested (or the nearest the range allows)
        float resultLufs = Max::dbFloor;        // measured at the suggested Threshold, through the path in use
        float averageReductionDb = 0.0f;        // at the suggested Threshold
        bool usable() const noexcept { return status == Status::good; }
    };

    // Integrated loudness of plain audio (no processing), with the same meter as the readout.
    inline Measurement loudnessOf (const float* const* in, int numCh, int n, double rate)
    {
        Max::Loudness::StepMeter meter;
        meter.prepare (rate);
        constexpr int block = 4096;
        for (int pos = 0; pos < n; pos += block)
        {
            const float* ptr[2] = { in[0] + pos, in[juce::jmin (1, numCh - 1)] + pos };
            meter.process (ptr, numCh, juce::jmin (block, n - pos));
        }
        Max::Loudness::LoudnessReadings readings;
        readings.update (meter);
        const auto r = readings.getIntegrated();
        return { r.valid, r.lufs, 0.0f };
    }

    // Replays the capture through the Maximiser's limiter at one Threshold and measures the output, as processBlock runs it.
    // withOversampling false: always at the host's rate (the quick path for the search).
    inline Measurement measure (const float* const* in, int numCh, int n, double rate, const Settings& s, float thresholdDb,
                                bool withOversampling)
    {
        numCh = juce::jlimit (1, Max::BrickwallLimiter::maxChannels, numCh);
        const int os = withOversampling ? juce::jlimit (0, 2, s.oversampling) : 0;
        const float headroom = Max::Dither::headroomFor ((Max::Dither::Depth) juce::jlimit (0, 2, s.dither));
        auto configure = [&] (Max::BrickwallLimiter& l, float threshold, float release, bool autoRelease)
        {
            l.setThresholdDb (threshold);
            l.setCeilingDb (s.ceilingDb);
            l.setTruePeak (s.truePeak);
            l.setAutoRelease (autoRelease);
            l.setReleaseMs (release);
            l.setCeilingHeadroom (headroom);
        };

        constexpr int block = 512;
        Max::BrickwallLimiter limiter, guard;
        configure (limiter, thresholdDb, s.releaseMs, s.autoRelease);
        limiter.prepare (rate * (double) (1 << os));
        limiter.reset();
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        if (os > 0)
        {
            using OS = juce::dsp::Oversampling<float>;
            oversampler = std::make_unique<OS> ((size_t) numCh, (size_t) os, OS::filterHalfBandFIREquiripple, true, true);
            oversampler->initProcessing ((size_t) block);
            oversampler->reset();
            configure (guard, s.ceilingDb, 5.0f, false);   // the processor's guard: a brickwall at the ceiling, nothing below it
            guard.prepare (rate);
            guard.reset();
        }

        Max::Loudness::StepMeter meter;
        meter.prepare (rate);
        juce::AudioBuffer<float> work (numCh, block);
        double reductionSum = 0.0;
        int reductionBlocks = 0;
        for (int pos = 0; pos < n; pos += block)
        {
            const int num = juce::jmin (block, n - pos);
            double power = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                const float* src = in[c] + pos;
                float* dst = work.getWritePointer (c);
                std::copy (src, src + num, dst);
                for (int i = 0; i < num; ++i)
                    power += (double) src[i] * src[i];
            }
            float* ch[Max::BrickwallLimiter::maxChannels] = { work.getWritePointer (0), work.getWritePointer (numCh - 1) };
            if (os == 0)
            {
                limiter.process (ch, numCh, num);
            }
            else
            {
                juce::dsp::AudioBlock<float> audio (ch, (size_t) numCh, (size_t) num);
                auto up = oversampler->processSamplesUp (audio);
                float* upCh[Max::BrickwallLimiter::maxChannels] = { up.getChannelPointer (0), up.getChannelPointer ((size_t) numCh - 1) };
                limiter.process (upCh, numCh, (int) up.getNumSamples());
                oversampler->processSamplesDown (audio);
                guard.process (ch, numCh, num);
            }
            const float reduction = -limiter.readAndResetMaxGainReductionDb();
            if (Max::gainToDb ((float) std::sqrt (power / (double) (num * numCh))) > quietBlockDb)
            {
                reductionSum += reduction;
                ++reductionBlocks;
            }
            const float* out[2] = { ch[0], ch[numCh - 1] };
            meter.process (out, numCh, num);
        }

        Max::Loudness::LoudnessReadings readings;
        readings.update (meter);
        const auto r = readings.getIntegrated();
        return { r.valid, r.lufs, reductionBlocks > 0 ? (float) (reductionSum / reductionBlocks) : 0.0f };
    }

    // Finds the Threshold that brings the output to targetLufs. Background thread; shouldStop is checked between passes.
    inline Result analyse (const float* const* in, int numCh, int n, double rate, const Settings& s, double targetLufs,
                           const std::function<bool()>& shouldStop = {})
    {
        Result r;
        r.targetLufs = targetLufs;
        r.currentThresholdDb = s.thresholdDb;
        r.thresholdDb = s.thresholdDb;
        r.seconds = rate > 0.0 ? (double) n / rate : 0.0;
        numCh = juce::jlimit (1, 2, numCh);
        if (n <= 0 || rate <= 0.0)
            return r;

        const auto input = loudnessOf (in, numCh, n, rate);
        if (! input.valid)
            return r;   // silent: nothing above the gate (-70 LUFS)
        r.inputLufs = input.lufs;
        if ((double) n < minSeconds * rate) { r.status = Status::tooShort; return r; }

        const auto stopped = [&] { return shouldStop && shouldStop(); };
        const auto at = [&] (float threshold) { return measure (in, numCh, n, rate, s, threshold, false); };

        r.currentLufs = measure (in, numCh, n, rate, s, s.thresholdDb, true).lufs;

        // A lower Threshold always means more gain, so the output gets louder: bisect.
        float loud = Max::minThresholdDb, quiet = Max::maxThresholdDb;
        const auto loudest = at (loud);
        if (stopped()) { r.status = Status::stopped; return r; }
        if (loudest.lufs < (float) targetLufs - (float) reachedLu)
        {
            r.status = Status::louderThanPossible;
            r.thresholdDb = loud;
            r.resultLufs = loudest.lufs;
            r.averageReductionDb = loudest.averageReductionDb;
            return r;
        }
        const auto quietest = at (quiet);
        if (quietest.lufs > (float) targetLufs + (float) reachedLu)
        {
            r.status = Status::quieterThanPossible;
            r.thresholdDb = quiet;
            r.resultLufs = quietest.lufs;
            r.averageReductionDb = quietest.averageReductionDb;
            return r;
        }
        for (int i = 0; i < 12; ++i)
        {
            if (stopped()) { r.status = Status::stopped; return r; }
            const float mid = 0.5f * (loud + quiet);
            (at (mid).lufs < (float) targetLufs ? quiet : loud) = mid;
        }

        const auto round = [] (float t) { return juce::jlimit (Max::minThresholdDb, Max::maxThresholdDb, std::round (t * 10.0f) / 10.0f); };   // 0.1 dB steps
        float threshold = round (0.5f * (loud + quiet));
        auto check = measure (in, numCh, n, rate, s, threshold, true);   // the path that will play it, oversampling included
        if (s.oversampling > 0 && std::abs (check.lufs - (float) targetLufs) > 0.2f && ! stopped())
        {
            // The oversampled path measures slightly differently: one correction, about a decibel of Threshold per LU.
            threshold = round (threshold - ((float) targetLufs - check.lufs));
            check = measure (in, numCh, n, rate, s, threshold, true);
        }
        if (stopped()) { r.status = Status::stopped; return r; }
        r.thresholdDb = threshold;
        r.resultLufs = check.lufs;
        r.averageReductionDb = check.averageReductionDb;
        r.status = Status::good;
        return r;
    }

    // ---- Wording ---------------------------------------------------------------------------------------------------
    inline juce::String minus() { return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")); }

    // "-14.0 LUFS", with a real minus sign.
    inline juce::String lufsText (double lufs)
    {
        const double v = std::round (lufs * 10.0) / 10.0;
        return (v < 0.0 ? minus() : juce::String()) + juce::String (std::abs (v), 1) + " LUFS";
    }

    inline juce::String dbText (double db, bool sign = false)
    {
        const double v = std::round (db * 10.0) / 10.0;
        return (v < 0.0 ? minus() : (sign && v > 0.0 ? juce::String ("+") : juce::String())) + juce::String (std::abs (v), 1) + " dB";
    }

    // Reads "-14", "-14.0", "-9.5 LUFS", with a hyphen or a real minus sign. Anything else is not a value.
    inline bool parseTarget (const juce::String& text, double& lufs)
    {
        auto t = text.trim().replace (minus(), "-").replace (juce::String::charToString (0x2013), "-");
        if (t.endsWithIgnoreCase ("LUFS"))
            t = t.dropLastCharacters (4).trim();
        if (t.isEmpty() || ! t.containsOnly ("+-0123456789.") || ! t.containsAnyOf ("0123456789")
            || t.indexOfChar ('.') != t.lastIndexOfChar ('.') || t.lastIndexOfAnyOf ("+-") > 0)
            return false;
        lufs = t.getDoubleValue();
        return std::isfinite (lufs);
    }

    // What is wrong with typed text, in words, or empty if it is a target this can use. Nothing is clamped quietly.
    inline juce::String targetProblem (const juce::String& text)
    {
        double v = 0.0;
        if (! parseTarget (text, v))
            return "Type a number, such as " + minus() + "14.";
        if (v > 0.0 || (v == 0.0 && ! text.contains ("-")))
            return "Loudness targets are below zero, such as " + minus() + "14.";
        if (v < minTargetLufs || v > maxTargetLufs)
            return "Choose a target from " + lufsText (minTargetLufs) + " to " + lufsText (maxTargetLufs) + ".";
        return {};
    }

    inline juce::String messageFor (const Result& r)
    {
        const auto measured = "Measured over the " + juce::String (r.seconds, 1) + " seconds you played: "
                              + lufsText (r.inputLufs) + " going in, " + lufsText (r.currentLufs) + " coming out now. ";
        switch (r.status)
        {
            case Status::silent:   return "No sound was heard. Check that audio is playing, then try again.";
            case Status::tooShort: return "That was too short to judge. Play at least 3 seconds of the song, then try again.";
            case Status::stopped:  return {};
            case Status::good:
            {
                const float change = r.currentThresholdDb - r.thresholdDb;   // more gain is a lower Threshold
                auto text = measured + "Threshold " + dbText (r.currentThresholdDb) + " to " + dbText (r.thresholdDb)
                            + " (" + dbText (change, true) + " of gain) gives " + lufsText (r.resultLufs)
                            + ", with peaks kept under the Output Ceiling. ";
                if (r.averageReductionDb > heavyLimitingDb)
                    text << "That is heavy limiting (about " << dbText (r.averageReductionDb) << " on average): listen for distortion before you keep it. ";
                return text + "This matches the average loudness of what you played; the level still moves with the music.";
            }
            case Status::louderThanPossible:
                return measured + "Even the lowest Threshold only reaches " + lufsText (r.resultLufs)
                       + " under this Output Ceiling, so " + lufsText (r.targetLufs)
                       + " cannot be reached without going over it. Choose a lower target, or raise the Output Ceiling.";
            case Status::quieterThanPossible:
                return measured + "Even with Threshold at " + dbText (0.0) + " the output measures " + lufsText (r.resultLufs)
                       + ", louder than " + lufsText (r.targetLufs)
                       + ". Choose a higher target, or lower the Output Ceiling or the level going in.";
        }
        return {};
    }
}
