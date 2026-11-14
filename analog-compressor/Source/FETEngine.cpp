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
        // Time constant a third of the glide time, so a change has settled in about lowCutGlideSeconds.
        lowCutGlide = 1.0 - std::exp (-(double) lowCutBlock / (fs * lowCutGlideSeconds / 3.0));
        updateCoefficients();
        reset();
    }

    void Engine::reset() noexcept
    {
        gainReductionDb = 0.0f;
        peakGr.store (0.0f, std::memory_order_relaxed);
        lowCutState = {};
        lowCutMix = lowCutMixTarget;
        lowCutCurrentHz = lowCutTargetHz;
        lowCutCounter = 0;
        updateLowCutCoefficients();
    }

    void Engine::setDetectorLowCutHz (float hz) noexcept
    {
        if (! (hz > 0.0f))
        {
            lowCutHz = 0.0f;
            lowCutMixTarget = 0.0;   // fade the filter out; the cutoff stays where it was
            return;
        }
        if (hz == lowCutHz)
            return;
        lowCutHz = hz;
        lowCutTargetHz = (double) hz;
        lowCutMixTarget = 1.0;
        if (lowCutMix < 1.0e-3)      // filter not audible yet: start at the new cutoff, no glide from a stale one
        {
            lowCutCurrentHz = lowCutTargetHz;
            updateLowCutCoefficients();
        }
    }

    void Engine::updateLowCutCoefficients() noexcept
    {
        const double fc = std::clamp (lowCutCurrentHz, 1.0, fs * 0.45);
        const double K = std::tan (3.14159265358979323846 * fc / fs);
        const double root2 = 1.41421356237309505;
        const double norm = 1.0 / (1.0 + root2 * K + K * K);
        b0 = norm;
        b1 = -2.0 * norm;
        b2 = norm;
        a1 = 2.0 * (K * K - 1.0) * norm;
        a2 = (1.0 - root2 * K + K * K) * norm;
    }

    void Engine::advanceLowCutSmoothing() noexcept
    {
        bool changed = false;
        if (lowCutMix != lowCutMixTarget)
        {
            lowCutMix += (lowCutMixTarget - lowCutMix) * lowCutGlide;
            if (std::abs (lowCutMixTarget - lowCutMix) < 1.0e-4)
                lowCutMix = lowCutMixTarget;
        }
        if (lowCutCurrentHz != lowCutTargetHz)
        {
            lowCutCurrentHz *= std::pow (lowCutTargetHz / lowCutCurrentHz, lowCutGlide);   // glide in octaves
            if (std::abs (lowCutCurrentHz / lowCutTargetHz - 1.0) < 1.0e-4)
                lowCutCurrentHz = lowCutTargetHz;
            changed = true;
        }
        if (changed)
            updateLowCutCoefficients();
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

    void Engine::setAllButtons (bool on) noexcept
    {
        if (on != allButtons) { allButtons = on; updateCoefficients(); }
    }

    void Engine::updateCoefficients() noexcept
    {
        const auto r = (size_t) ratioIndex;
        k = ratios[r] - 1.0f;
        thresholdDb = thresholdsDb[r];
        kneeDb = kneeWidthsDb[r];
        const float ratio = allButtons ? abRatio : ratios[r];
        overSlope = 0.0f;
        overStartDb = 0.0f;
        if (allButtons)
        {
            k = abRatio - 1.0f;
            thresholdDb = abThresholdDb;
            kneeDb = abKneeDb;
            overSlope = abOverSlope;
            overStartDb = abOverStartDb;
        }

        // Above the knee the closed loop speeds the capacitor's time constant up by (1 + k) =
        // ratio, so the internal attack constant is scaled by the ratio to make the MEASURED
        // attack match the knob (docs/engine.md "Timing"). Release runs with the detector below
        // threshold, where the loop is open, so it needs no scaling.
        attackCoeff  = coeffFor (attackSeconds (attackKnob) * ratio, fs);
        releaseCoeff = coeffFor (releaseSeconds (releaseKnob), fs);
        grOffCoeff   = coeffFor (grOffFadeSeconds, fs);
    }

    float Engine::solve (float levelDb, float g0) const noexcept
    {
        // All Buttons over-compression: extra GR from the input overshoot (independent of G).
        const float extra = overSlope * std::max (0.0f, levelDb - thresholdDb - overStartDb);
        const float target0 = k * knee (levelDb - g0 - thresholdDb, kneeDb) + extra;
        float a = target0 > g0 ? attackCoeff : releaseCoeff;

        if (allButtons)
        {
            // Attack lags while GR is small (transients get through), release speeds up with
            // depth: the "constant changes in the attack and release times" (UM p. 9).
            const float mul = target0 > g0 ? 1.0f + abAttackLag * std::exp (-g0 / abAttackLagDb)
                                           : 1.0f / (1.0f + g0 / abReleaseDb);
            a = 1.0f - std::pow (1.0f - a, 1.0f / mul);
        }

        if (target0 <= g0)
            return solveRelease (levelDb, g0, a, extra);

        // Attack, in dB (calibrated to the published attack times, docs/engine.md):
        // f(G) = G - g0 - a (k knee(L - G - T) - g0) is increasing in G, so the root is unique
        // and lies between g0 and the explicit step g0 + a (target0 - g0).
        float lo = g0, hi = g0 + a * (target0 - g0);
        if (lo > hi) std::swap (lo, hi);

        float g = hi;   // start at the explicit step
        for (int it = 0; it < 8; ++it)
        {
            const float over = levelDb - g - thresholdDb;
            const float f = g - g0 - a * (k * knee (over, kneeDb) + extra - g0);
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

    // Release (ACOMP-18), in the gain cell's control-voltage domain. The cell is a divider, R5 over the
    // FET (UG p. 8, Figs. 3-4): gain = 1 / (1 + u), with u = R5 x the FET's conductance, which in its
    // linear region is proportional to the gate voltage on the timing capacitor. That capacitor
    // discharges exponentially through the release pot (UG p. 10, Fig. 5), so u, not GR in dB, decays
    // exponentially:  u = 10^(G/20) - 1,  u[n] = u0 + a (u(target(G[n])) - u0).
    // Near 0 dB, u is proportional to G, so shallow release is exactly as before. Deep GR recovers at
    // an almost steady number of dB per second instead, so heavy compression lets go more slowly:
    // the programme-dependent release the 1176 is known for. No new constants (a modelling choice,
    // docs/engine.md and docs/references.md).
    //   f(G) = u(G) - u0 - a (u(T(G)) - u0) is increasing in G (T falls as G rises), f(g0) >= 0, and
    //   f(G_explicit) <= 0, so the root is unique and lies between the explicit step and g0.
    float Engine::solveRelease (float levelDb, float g0f, float af, float extra) const noexcept
    {
        // Double precision with expm1 / log1p: u = 10^(G/20) - 1 is tiny for small GR, and computing it
        // as exp() - 1 in float cancels most of its digits (rounding noise near -80 dB).
        constexpr double c = 0.11512925464970229;   // ln(10) / 20
        auto u  = [] (double g) { return std::expm1 (g * c); };
        auto du = [] (double g) { return c * std::exp (g * c); };
        auto gOf = [] (double uu) { return std::log1p (std::max (0.0, uu)) / c; };
        auto target = [&] (double g) { return (double) (k * knee (levelDb - (float) g - thresholdDb, kneeDb) + extra); };

        const double g0 = g0f, a = af, u0 = u (g0);
        double lo = gOf (u0 + a * (u (target (g0)) - u0)), hi = g0;
        if (lo > hi) std::swap (lo, hi);

        double x = lo;   // start at the explicit step
        for (int it = 0; it < 8; ++it)
        {
            const double over = (double) levelDb - x - (double) thresholdDb;
            const double tg = target (x);
            const double f = u (x) - u0 - a * (u (tg) - u0);
            if (std::abs (f) < 1.0e-12 * (1.0 + u0))
                break;

            if (f > 0.0) hi = x; else lo = x;

            const double df = du (x) + a * du (tg) * k * kneeSlope ((float) over, kneeDb);
            double next = x - f / df;
            if (! (next > lo && next < hi))
                next = 0.5 * (lo + hi);   // Newton left the bracket: bisect
            x = next;
        }
        return (float) std::max (0.0, x);
    }

    void Engine::process (float* const* channels, int numChannels, int numSamples, float* gainOut) noexcept
    {
        const int nc = std::min (numChannels, maxChannels);
        float blockPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float g0 = gainReductionDb;
            float gr;

            // Detector Low Cut: filter a copy of the input for the detector only. The audio path below
            // multiplies the unfiltered samples. Off and settled: no filtering, no state, bit-exact as before.
            const bool lowCutActive = lowCutMix > 0.0 || lowCutMixTarget > 0.0;
            float detected[maxChannels];
            if (lowCutActive)
            {
                if (++lowCutCounter >= lowCutBlock)
                {
                    lowCutCounter = 0;
                    advanceLowCutSmoothing();
                }
                for (int c = 0; c < nc; ++c)
                {
                    const double x = (double) channels[c][i];
                    auto& s = lowCutState[(size_t) c];
                    const double y = b0 * x + s.z1;
                    s.z1 = b1 * x - a1 * y + s.z2;
                    s.z2 = b2 * x - a2 * y;
                    detected[c] = (float) (x + lowCutMix * (y - x));
                }
            }
            else
            {
                lowCutState = {};
                for (int c = 0; c < nc; ++c)
                    detected[c] = channels[c][i];
            }

            if (grEnabled)
            {
                float peak = 0.0f;
                for (int c = 0; c < nc; ++c)
                    peak = std::max (peak, std::abs (detected[c]));

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
            const float extra = overSlope * std::max (0.0f, levelDb - thresholdDb - overStartDb);
            if (mid - k * knee (levelDb - mid - thresholdDb, kneeDb) - extra > 0.0f) hi = mid; else lo = mid;
        }
        return 0.5f * (lo + hi);
    }
}
