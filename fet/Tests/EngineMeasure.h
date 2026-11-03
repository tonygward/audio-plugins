#pragma once

// Measurement procedures for the engine (definitions in fet/docs/engine.md).
//
// Constant-envelope test signal: samples alternate +A, -A, so the full-wave detector sees a
// true level step (no rectifier ripple) at any sample rate.
//
// Attack time: settle at threshold - 40 dB (no GR), step the level to threshold + `over` dB,
// and report the time until GR first reaches 63.2 % (1 - 1/e) of its settled value, linearly
// interpolated between samples.
// Release time: settle at threshold + `over` dB, step down to threshold - 40 dB, and report the
// time until GR first falls to 36.8 % (1/e) of the value it had at the step.
#include <algorithm>
#include <cmath>
#include <vector>
#include "FETEngine.h"

namespace Measure
{
    inline float amp (float db) { return std::pow (10.0f, db / 20.0f); }

    inline void runConstant (FET::Engine& e, float levelDb, int samples, std::vector<float>* grOut = nullptr)
    {
        const float a = amp (levelDb);
        for (int i = 0; i < samples; ++i)
        {
            float x = (i & 1) ? -a : a;
            float* ch[] = { &x };
            e.process (ch, 1, 1);
            if (grOut != nullptr)
                grOut->push_back (e.getGainReductionDb());
        }
    }

    inline float settledGrDb (FET::Engine& e, double fs, float levelDb)
    {
        e.reset();
        runConstant (e, levelDb, (int) (fs * 0.5));
        return e.getGainReductionDb();
    }

    inline double crossing (const std::vector<float>& gr, float target, bool rising, double fs)
    {
        float prev = rising ? 0.0f : gr.empty() ? 0.0f : gr.front();
        for (size_t i = 0; i < gr.size(); ++i)
        {
            const float g = gr[i];
            if (rising ? g >= target : g <= target)
            {
                // gr[i] is the GR after sample i (time i / fs); gr[-1] is the value before the step.
                const float frac = (g == prev) ? 1.0f : (target - prev) / (g - prev);
                return ((double) i - 1.0 + std::clamp ((double) frac, 0.0, 1.0)) / fs;
            }
            prev = g;
        }
        return -1.0;
    }

    inline void configure (FET::Engine& e, double fs, int ratioIndex, float knob)
    {
        e.prepare (fs);
        e.setRatioIndex (ratioIndex);
        e.setAttackKnob (knob);
        e.setReleaseKnob (knob);
    }

    inline double attackSeconds (double fs, int ratioIndex, float knob, float overDb)
    {
        FET::Engine e;
        configure (e, fs, ratioIndex, knob);
        const float t = FET::Engine::thresholdsDb[(size_t) ratioIndex];
        runConstant (e, t - 40.0f, (int) (fs * 0.1));
        std::vector<float> gr;
        runConstant (e, t + overDb, (int) (fs * 0.05), &gr);
        const float settled = e.staticGainReductionDb (t + overDb);
        return crossing (gr, settled * (1.0f - std::exp (-1.0f)), true, fs);
    }

    inline double releaseSeconds (double fs, int ratioIndex, float knob, float overDb)
    {
        FET::Engine e;
        configure (e, fs, ratioIndex, knob);
        const float t = FET::Engine::thresholdsDb[(size_t) ratioIndex];
        runConstant (e, t + overDb, (int) (fs * 0.2));
        const float start = e.getGainReductionDb();
        std::vector<float> gr;
        runConstant (e, t - 40.0f, (int) (fs * 4.0), &gr);
        // The "previous" value for interpolation before the first sample is the GR at the step.
        gr.insert (gr.begin(), start);
        const double t37 = crossing (gr, start * std::exp (-1.0f), false, fs);
        return t37 < 0.0 ? -1.0 : t37 - 1.0 / fs;
    }

    // Average GR (dB) for a sine at the given RMS level, from the RMS ratio out/in once settled.
    inline float sineGrDb (FET::Engine& e, double fs, double freq, float rmsDb)
    {
        e.reset();
        const float a = amp (rmsDb) * std::sqrt (2.0f);
        const int settle = (int) (fs * 2.0), measure = (int) (fs * 1.0);
        double sumIn = 0.0, sumOut = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            const float x = a * (float) std::sin (2.0 * 3.141592653589793 * freq * i / fs);
            float y = x;
            float* ch[] = { &y };
            e.process (ch, 1, 1);
            if (i >= settle) { sumIn += (double) x * x; sumOut += (double) y * y; }
        }
        return (float) (-10.0 * std::log10 (sumOut / sumIn));
    }
}
