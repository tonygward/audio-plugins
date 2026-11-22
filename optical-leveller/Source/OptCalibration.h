#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include "OpticalEngine.h"
#include "OptParams.h"

// Set Levels for the Optical Leveller (songsmithstudio#19): the musician plays a typical part, and the plugin
// suggests the Peak Reduction that gives a chosen average gain reduction on it.
//
// The measuring replays the captured sound through the leveller's own engine (the same code and the same
// parameters that play it), so the figure is real, not a cosmetic estimate. Nothing here touches the audio
// thread: it runs on the message thread once listening has finished.
//
// What the number means: the target is the gain reduction averaged over the sound that was playing (silence
// is left out). The leveller follows the music, so it will turn the level down more on loud parts and less on
// quiet ones. This sets a representative amount, not a fixed amount at every moment.
namespace Opt::Calibration
{
    inline constexpr float defaultTargetDb = 3.0f;           // a gentle, typical amount of levelling, as a positive dB of reduction
    inline constexpr float minTargetDb = 0.0f, maxTargetDb = 24.0f;   // what the Gain Reduction bar can show
    inline constexpr double minSeconds = 3.0;                // shorter than this is too little to judge
    inline constexpr float silenceRmsDb = -80.0f;            // the whole capture quieter than this counts as silence
    inline constexpr float quietBlockDb = -60.0f;            // blocks quieter than this are left out of the average
    inline constexpr double settleSeconds = 0.25;            // the leveller's first moments are not representative
    inline constexpr float tolerableErrorDb = 0.5f;          // closer than this counts as reached

    enum class Status { good, silent, tooShort, tooQuiet, tooLoud };

    struct Settings
    {
        bool limit = false;        // Compress or Limit when listening began
        float peakReduction = Opt::defaultPeakReduction;
        float gainDb = Opt::defaultGainDb;
    };

    struct Result
    {
        Status status = Status::silent;
        float targetDb = defaultTargetDb;       // asked for, as a positive amount of reduction
        float currentReductionDb = 0.0f;        // measured at the Peak Reduction in use now
        float peakReduction = 0.0f;             // suggested
        float reductionDb = 0.0f;               // measured at the suggested Peak Reduction
        bool usable() const noexcept { return status == Status::good || status == Status::tooQuiet || status == Status::tooLoud; }
    };

    inline float toDb (double power) noexcept { return power > 1.0e-15 ? (float) (10.0 * std::log10 (power)) : -150.0f; }

    // The average gain reduction (positive dB) over the loud-enough blocks of the capture, at one Peak Reduction.
    inline float averageReductionDb (const float* const* in, int numCh, int n, double rate, bool limit, float peakReduction)
    {
        SongsmithOptical::Engine engine;
        engine.prepare (rate);
        SongsmithOptical::Params p;
        p.peakReduction = peakReduction;
        p.gainDb = 0.0f;   // Gain follows the cell and never changes the reduction
        p.limit = limit;
        engine.setParams (p);
        engine.reset();

        constexpr int block = 64;
        const int skip = (int) (settleSeconds * rate);
        float buf[2][block] = {};
        double sum = 0.0;
        int counted = 0;
        for (int pos = 0; pos < n; pos += block)
        {
            const int num = std::min (block, n - pos);
            double power = 0.0;
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < num; ++i)
                {
                    buf[c][i] = in[c][pos + i];
                    power += (double) buf[c][i] * buf[c][i];
                }
            float* ptr[2] = { buf[0], buf[1] };
            engine.process (ptr, numCh, num);
            if (pos < skip)
                continue;
            const float blockDb = toDb (power / (double) (num * numCh));
            if (blockDb > quietBlockDb)
            {
                sum += engine.getGainReductionDb();
                ++counted;
            }
        }
        return counted > 0 ? (float) (sum / counted) : 0.0f;
    }

    // Finds the Peak Reduction that gives targetDb (a positive amount of reduction). Message thread only.
    inline Result analyse (const float* const* in, int numCh, int n, double rate, const Settings& s, float targetDb)
    {
        Result r;
        r.targetDb = juce::jlimit (minTargetDb, maxTargetDb, targetDb);
        numCh = juce::jlimit (1, 2, numCh);
        if (n <= 0 || rate <= 0.0)
            return r;

        double power = 0.0;
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < numCh; ++c)
                power += (double) in[c][i] * in[c][i];
        if (toDb (power / ((double) n * numCh)) < silenceRmsDb) { r.status = Status::silent; return r; }
        if ((double) n < minSeconds * rate) { r.status = Status::tooShort; return r; }

        const auto at = [&] (float pr) { return averageReductionDb (in, numCh, n, rate, s.limit, pr); };
        r.currentReductionDb = at (s.peakReduction);

        // More Peak Reduction always means more gain reduction, so bisect.
        float lo = SongsmithOptical::minPeakReduction, hi = SongsmithOptical::maxPeakReduction;
        const float atTop = at (hi), atBottom = at (lo);
        if (atTop < r.targetDb - tolerableErrorDb)
        {
            r.status = Status::tooQuiet;
            r.peakReduction = hi;
            r.reductionDb = atTop;
            return r;
        }
        if (atBottom > r.targetDb + tolerableErrorDb)
        {
            r.status = Status::tooLoud;
            r.peakReduction = lo;
            r.reductionDb = atBottom;
            return r;
        }
        for (int i = 0; i < 14; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            (at (mid) < r.targetDb ? lo : hi) = mid;
        }
        r.peakReduction = std::round (0.5f * (lo + hi) * 10.0f) / 10.0f;   // the knob's own step
        r.reductionDb = at (r.peakReduction);
        r.status = Status::good;
        return r;
    }

    // "-3.0 dB": a gain reduction is written with a minus sign, as the number it takes off the level.
    inline juce::String targetText (double reductionDb)
    {
        return (reductionDb > 0.05 ? "-" : "") + juce::String (std::abs (reductionDb) < 0.05 ? 0.0 : reductionDb, 1) + " dB";
    }

    // Reads "-2", "-2.5 dB", "2" or "2.0": every one means 2 dB of reduction (the sign is the convention, not a boost).
    inline bool parseTarget (const juce::String& text, double& reductionDb)
    {
        const auto digits = text.retainCharacters ("0123456789.");
        if (! digits.containsAnyOf ("0123456789") || digits.indexOfChar ('.') != digits.lastIndexOfChar ('.'))
            return false;
        reductionDb = (double) juce::jlimit (minTargetDb, maxTargetDb, (float) digits.getDoubleValue());
        return true;
    }

    inline juce::String messageFor (const Result& r)
    {
        const auto got = juce::String (r.reductionDb, 1) + " dB";
        const auto now = juce::String (r.currentReductionDb, 1) + " dB";
        const juce::String goal = "The target is an average over the sound you played: loud parts are turned down more and quiet parts less.";
        switch (r.status)
        {
            case Status::silent:   return "No sound was heard. Check that audio is playing, then try again.";
            case Status::tooShort: return "That was too short to judge. Play at least 3 seconds of a typical part, then try again.";
            case Status::good:
                return "Now: " + now + " of gain reduction. Peak Reduction " + juce::String (r.peakReduction, 1) + " gives about " + got + ". " + goal;
            case Status::tooQuiet:
                return "Very quiet: even full Peak Reduction only reaches about " + got + ". Apply anyway, or Cancel.";
            case Status::tooLoud:
                return "Very loud: even no Peak Reduction already gives about " + got + ". Apply anyway, or Cancel.";
        }
        return {};
    }
}
