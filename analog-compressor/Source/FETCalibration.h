#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include "FETColour.h"
#include "../../shared/Source/SongsmithLevelCapture.h"
#include "FETEngine.h"
#include "FETParams.h"

// ACOMP-09: guided calibration ("Set Levels"). See docs/set-levels.md.
//
// Two halves, kept apart so the audio thread does almost nothing:
//   Capture  - copies the incoming (dry) audio into a buffer that was allocated in prepareToPlay.
//              It never allocates, locks or logs, and it never touches a parameter.
//   analyse  - runs on the message thread afterwards. It replays the capture through a private copy of the
//              engine (current ratio, attack, release, Ignore Below and All Buttons) and the same colour
//              stages, and searches for the Input that gives the wanted gain reduction on the louder peaks.
//              Then it compares dry and compressed loudness to propose an Output.
namespace FET::Calibration
{
    inline constexpr double maxCaptureSeconds = 12.0;
    inline constexpr double minCaptureSeconds = 3.0;       // shorter than this is too little to judge
    inline constexpr float targetReductionDb = 3.0f;       // the middle of 2 to 4 dB
    inline constexpr float acceptableLowDb = 2.0f, acceptableHighDb = 4.0f;
    inline constexpr float silenceRmsDb = -80.0f;          // quieter than this (whole capture) counts as silence;
                                                           // Input can add 48 dB, so quieter sound can still be reached
    inline constexpr float clipPeakDb = -0.1f;

    //==============================================================================
    // The listening buffer is shared by every plugin with a Set Levels panel (songsmithstudio#19).
    using Capture = Songsmith::LevelCapture;

    //==============================================================================
    struct Settings
    {
        int ratioIndex = 0;                 // Engine ratio index (0..3, or Engine::gentleRatioIndex)
        bool allButtons = false;
        float attackKnob = 4.0f, releaseKnob = 4.0f;
        float ignoreBelowHz = 0.0f;
        bool colour = true;
        int model = 0;                      // ACOMP-10: the selected model (0 = Rev D Blackface), so Set Levels measures what plays
        bool slowAttack = false;
        float targetDb = targetReductionDb;
    };

    enum class Status { good, silent, tooShort, inputAtLimit, outputAtLimit };

    struct Result
    {
        Status status = Status::silent;
        float inputDb = 0.0f, outputDb = 0.0f;
        float reductionDb = 0.0f;       // measured on the louder peaks at the proposed Input
        float averageReductionDb = 0.0f;
        float dryPeakDb = -150.0f, dryRmsDb = -150.0f;
        float matchedPeakDb = -150.0f;  // output peak with the proposed Input and Output
        bool clipRisk = false;          // the matched output would reach 0 dBFS
        bool alreadyClipping = false;   // the captured audio itself reached full scale
        bool usable() const noexcept { return status == Status::good || status == Status::inputAtLimit || status == Status::outputAtLimit; }
    };

    struct Replay { float loudReductionDb = 0, averageReductionDb = 0, wetRmsDb = -150, wetPeakDb = -150; };

    inline float toDb (double power) noexcept { return power > 1.0e-15 ? (float) (10.0 * std::log10 (power)) : -150.0f; }

    // Replays the capture at one Input setting, with the Output at 0 dB. Message thread only.
    inline Replay replay (const float* const* in, int numCh, int n, double rate, const Settings& s, float inputDb)
    {
        Engine eng;
        eng.prepare (rate);
        eng.setRatioIndex (s.ratioIndex);
        eng.setAttackKnob (s.attackKnob);
        eng.setReleaseKnob (s.releaseKnob);
        eng.setAllButtons (s.allButtons);
        eng.setVoicing (s.model, s.slowAttack);
        eng.setDetectorLowCutHz (s.ignoreBelowHz);
        eng.reset();
        OutputStage stage;
        stage.prepare (rate);

        const float drive = std::abs (inputDb) < 0.005f ? 1.0f : std::pow (10.0f, inputDb / 20.0f);
        const float factor = (s.allButtons ? FetCell::allButtonsFactor : 1.0f) * voicingFor (s.model).colourScale;
        constexpr int chunk = 512;
        const int window = juce::jmax (1, (int) (0.1 * rate));   // 100 ms
        std::vector<float> gains ((size_t) chunk), buf[2];
        buf[0].resize ((size_t) chunk);
        buf[1].resize ((size_t) chunk);

        std::vector<float> windowGr;
        float windowMax = 0.0f;
        int inWindow = 0;
        double sumGr = 0.0, wetPower = 0.0, peak = 0.0;

        for (int pos = 0; pos < n; pos += chunk)
        {
            const int num = juce::jmin (chunk, n - pos);
            float* ptr[2] = { buf[0].data(), buf[1].data() };
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < num; ++i)
                    ptr[c][i] = in[c][pos + i] * drive;
            eng.process (ptr, numCh, num, gains.data());

            for (int i = 0; i < num; ++i)
            {
                const float gr = gains[(size_t) i] < 1.0e-6f ? 120.0f : -20.0f * std::log10 (gains[(size_t) i]);
                sumGr += gr;
                windowMax = juce::jmax (windowMax, gr);
                if (++inWindow >= window) { windowGr.push_back (windowMax); windowMax = 0.0f; inWindow = 0; }

                double framePower = 0.0;
                for (int c = 0; c < numCh; ++c)
                {
                    float y = ptr[c][i];
                    if (s.colour)
                        y = stage.processSample (c, FetCell::process (y, gains[(size_t) i], factor));
                    framePower = juce::jmax (framePower, (double) y * y);
                    peak = juce::jmax (peak, (double) std::abs (y));
                }
                wetPower += framePower;
            }
        }
        if (inWindow > window / 2)
            windowGr.push_back (windowMax);

        Replay r;
        r.averageReductionDb = n > 0 ? (float) (sumGr / n) : 0.0f;
        r.wetRmsDb = toDb (n > 0 ? wetPower / n : 0.0);
        r.wetPeakDb = toDb (peak * peak);
        if (! windowGr.empty())
        {
            // "Louder peaks": the 90th percentile of the 100 ms peak gain reductions, so a few
            // loud hits in a quiet passage count and one stray spike does not.
            std::sort (windowGr.begin(), windowGr.end());
            r.loudReductionDb = windowGr[juce::jmin (windowGr.size() - 1, (size_t) ((double) windowGr.size() * 0.9))];
        }
        return r;
    }

    inline Result analyse (const float* const* in, int numCh, int n, double rate, const Settings& s)
    {
        Result res;
        numCh = juce::jlimit (1, 2, numCh);
        if (n <= 0 || rate <= 0.0)
            return res;

        double dryPower = 0.0, dryPeak = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double p = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                p = juce::jmax (p, (double) in[c][i] * in[c][i]);
                dryPeak = juce::jmax (dryPeak, (double) std::abs (in[c][i]));
            }
            dryPower += p;
        }
        res.dryRmsDb = toDb (dryPower / n);
        res.dryPeakDb = toDb (dryPeak * dryPeak);
        res.alreadyClipping = dryPeak >= 0.999;

        if (res.dryRmsDb < silenceRmsDb)   { res.status = Status::silent; return res; }
        if ((double) n < minCaptureSeconds * rate) { res.status = Status::tooShort; return res; }

        // Gain reduction rises with Input, so bisect for the wanted reduction.
        float lo = inputMinDb, hi = inputMaxDb;
        const auto at = [&] (float db) { return replay (in, numCh, n, rate, s, db); };
        Replay best;
        Status status = Status::good;
        const Replay atMax = at (hi);
        if (atMax.loudReductionDb < acceptableLowDb)
        {
            res.inputDb = hi; best = atMax; status = Status::inputAtLimit;   // too quiet to reach it
        }
        else
        {
            const Replay atMin = at (lo);
            if (atMin.loudReductionDb > acceptableHighDb)
            {
                res.inputDb = lo; best = atMin; status = Status::inputAtLimit;   // too loud even turned right down
            }
            else
            {
                for (int i = 0; i < 14; ++i)
                {
                    const float mid = 0.5f * (lo + hi);
                    (at (mid).loudReductionDb < s.targetDb ? lo : hi) = mid;
                }
                res.inputDb = std::round (0.5f * (lo + hi) * 10.0f) / 10.0f;
                best = at (res.inputDb);
            }
        }

        float out = res.dryRmsDb - best.wetRmsDb;   // Output 0 dB gives wetRmsDb; the difference makes it equal
        if (out < outputMinDb || out > outputMaxDb)
        {
            out = juce::jlimit (outputMinDb, outputMaxDb, out);
            if (status == Status::good)
                status = Status::outputAtLimit;
        }
        res.outputDb = std::round (out * 10.0f) / 10.0f;
        res.reductionDb = best.loudReductionDb;
        res.averageReductionDb = best.averageReductionDb;
        res.matchedPeakDb = best.wetPeakDb + res.outputDb;
        res.clipRisk = res.matchedPeakDb > clipPeakDb;
        res.status = status;
        return res;
    }
}
