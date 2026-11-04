#include "FETEngine.h"
#include <algorithm>

namespace FET
{
    namespace
    {
        constexpr float levelFloorDb = -150.0f;

        // Quadratic soft knee centred on the threshold: 0 below, identity above.
        inline float knee (float over, float width) noexcept
        {
            const float h = 0.5f * width;
            if (over <= -h) return 0.0f;
            if (over >= h)  return over;
            const float t = over + h;
            return t * t / (2.0f * width);
        }

        inline float kneeSlope (float over, float width) noexcept
        {
            const float h = 0.5f * width;
            if (over <= -h) return 0.0f;
            if (over >= h)  return 1.0f;
            return (over + h) / width;
        }

        inline double logInterp (double from, double to, float knob) noexcept
        {
            const double t = std::clamp ((knob - 1.0) / 6.0, 0.0, 1.0);
            return from * std::pow (to / from, t);
        }

        inline float coeffFor (double seconds, double fs) noexcept
        {
            return (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
        }
    }

    double Engine::attackSeconds (float knob) noexcept  { return logInterp (attackSlowest, attackFastest, knob); }
    double Engine::releaseSeconds (float knob) noexcept { return logInterp (releaseSlowest, releaseFastest, knob); }

    void Engine::prepare (double sampleRate) noexcept
    {
        fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        updateCoefficients();
        reset();
    }

    void Engine::reset() noexcept
    {
        gainReductionDb = 0.0f;
        peakGr.store (0.0f, std::memory_order_relaxed);
    }

    void Engine::setRatioIndex (int index) noexcept
    {
        ratioIndex = std::clamp (index, 0, numRatios - 1);
        updateCoefficients();
    }

    void Engine::setAttackKnob (float knob) noexcept
    {
        if (knob != attackKnob) { attackKnob = knob; updateCoefficients(); }
    }

    void Engine::setReleaseKnob (float knob) noexcept
    {
        if (knob != releaseKnob) { releaseKnob = knob; updateCoefficients(); }
    }

    void Engine::updateCoefficients() noexcept
    {
        const auto r = (size_t) ratioIndex;
        k = ratios[r] - 1.0f;
        thresholdDb = thresholdsDb[r];
        kneeDb = kneeWidthsDb[r];

        // Above the knee the closed loop speeds the capacitor's time constant up by (1 + k) =
        // ratio, so the internal attack constant is scaled by the ratio to make the MEASURED
        // attack match the knob (docs/engine.md "Timing"). Release runs with the detector below
        // threshold, where the loop is open, so it needs no scaling.
        attackCoeff  = coeffFor (attackSeconds (attackKnob) * ratios[r], fs);
        releaseCoeff = coeffFor (releaseSeconds (releaseKnob), fs);
        grOffCoeff   = coeffFor (grOffFadeSeconds, fs);
    }

    float Engine::solve (float levelDb, float g0) const noexcept
    {
        const float target0 = k * knee (levelDb - g0 - thresholdDb, kneeDb);
        const float a = target0 > g0 ? attackCoeff : releaseCoeff;

        // f(G) = G - g0 - a (k knee(L - G - T) - g0) is increasing in G, so the root is unique
        // and lies between g0 and the explicit step g0 + a (target0 - g0).
        float lo = g0, hi = g0 + a * (target0 - g0);
        if (lo > hi) std::swap (lo, hi);

        float g = hi;   // start at the explicit step
        for (int it = 0; it < 8; ++it)
        {
            const float over = levelDb - g - thresholdDb;
            const float f = g - g0 - a * (k * knee (over, kneeDb) - g0);
            if (std::abs (f) < 1.0e-6f)
                break;

            if (f > 0.0f) hi = g; else lo = g;

            const float df = 1.0f + a * k * kneeSlope (over, kneeDb);
            float next = g - f / df;
            if (! (next > lo && next < hi))
                next = 0.5f * (lo + hi);   // Newton left the bracket: bisect
            g = next;
        }
        return std::max (0.0f, g);
    }

    void Engine::process (float* const* channels, int numChannels, int numSamples, float* gainOut) noexcept
    {
        const int nc = std::min (numChannels, maxChannels);
        float blockPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float g0 = gainReductionDb;
            float gr;

            if (grEnabled)
            {
                float peak = 0.0f;
                for (int c = 0; c < nc; ++c)
                    peak = std::max (peak, std::abs (channels[c][i]));

                const float levelDb = peak > 0.0f ? std::max (levelFloorDb, 20.0f * std::log10 (peak)) : levelFloorDb;
                gr = solve (levelDb, g0);
            }
            else
            {
                gr = g0 - grOffCoeff * g0;   // fade GR out (click-free "Attack OFF")
                if (gr < 1.0e-3f) gr = 0.0f;   // 0.001 dB: inaudible, and lets the bypass-exact path start
            }

            gainReductionDb = gr;
            blockPeak = std::max (blockPeak, gr);

            float gain = 1.0f;
            if (gr > 0.0f)
            {
                gain = std::exp (-gr * 0.11512925464970229f);   // ln(10) / 20
                for (int c = 0; c < nc; ++c)
                    channels[c][i] *= gain;
            }
            if (gainOut != nullptr)
                gainOut[i] = gain;
        }

        float prev = peakGr.load (std::memory_order_relaxed);
        while (blockPeak > prev && ! peakGr.compare_exchange_weak (prev, blockPeak, std::memory_order_relaxed)) {}
    }

    float Engine::staticGainReductionDb (float levelDb) const noexcept
    {
        // Steady state of the loop: G = k knee(L - G - T). Solve by bisection (G in [0, L+150]).
        float lo = 0.0f, hi = std::max (0.0f, levelDb - levelFloorDb);
        for (int it = 0; it < 60; ++it)
        {
            const float mid = 0.5f * (lo + hi);
            if (mid - k * knee (levelDb - mid - thresholdDb, kneeDb) > 0.0f) hi = mid; else lo = mid;
        }
        return 0.5f * (lo + hi);
    }
}
