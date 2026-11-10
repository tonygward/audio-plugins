#pragma once

#include <array>
#include <atomic>
#include <cmath>

// Clean feedback gain-reduction engine (no colour), testable without the plugin.
//
// Signal flow (UG p. 7 Fig. 1, p. 10; see analog-compressor/docs/references.md and analog-compressor/docs/engine.md):
//
//   x (after Input drive) --> [gain cell g = 10^(-G/20)] --> y --> (Output trim, outside)
//                                      ^                      |
//                                      +--- G <- detector <---+   (feedback: detect AFTER GR)
//
// The loop is solved per sample with zero delay: G[n] satisfies
//     G[n] = G[n-1] + a * (target(level(x[n]) - G[n]) - G[n-1])
// so y[n] uses the gain that y[n] itself implies. See docs/engine.md "Loop evaluation".
namespace FET
{
    class Engine
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int numHardwareRatios = 4;   // the four 1176 buttons: indexes 0..3
        static constexpr int gentleRatioIndex = 4;    // ACOMP-11 Songsmith 2:1, appended so 0..3 keep their meaning
        static constexpr int numRatios = 5;

        // Gain computer constants per ratio (4, 8, 12, 20, then Songsmith 2:1). Modelling choices, see
        // docs/engine.md: thresholds rise with ratio (UM p. 9, p. 21), soft knee (UM p. 21),
        // 20:1 calibrated like the Q-bias procedure (UG p. 5). 2:1 is a Songsmith design choice: the lowest
        // threshold and a wide soft knee, so it eases in gently. Above about -14.5 dBFS it takes off less gain
        // than 4:1 (docs/two-to-one-ratio.md).
        static constexpr std::array<float, numRatios> ratios       { 4.0f, 8.0f, 12.0f, 20.0f, 2.0f };
        static constexpr std::array<float, numRatios> thresholdsDb { -19.5f, -17.0f, -15.5f, -13.8f, -22.0f };   // peak dBFS
        static constexpr std::array<float, numRatios> kneeWidthsDb { 10.0f, 8.0f, 7.0f, 6.0f, 10.0f };

        // Published ranges (UG p. 1, UM p. 38), clockwise = faster (UM p. 8).
        static constexpr double attackSlowest = 800.0e-6, attackFastest = 20.0e-6;
        static constexpr double releaseSlowest = 1.1, releaseFastest = 0.05;
        static constexpr double grOffFadeSeconds = 0.01;

        // Knob position 1..7 -> nominal time in seconds (log taper; modelling choice).
        static double attackSeconds (float knob) noexcept;
        static double releaseSeconds (float knob) noexcept;

        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        void setRatioIndex (int index) noexcept;
        void setAttackKnob (float knob) noexcept;
        void setReleaseKnob (float knob) noexcept;
        void setGainReductionEnabled (bool enabled) noexcept { grEnabled = enabled; }
        void setAllButtons (bool on) noexcept;
        bool isAllButtons() const noexcept { return allButtons; }

        // ACOMP-07 detector Low Cut (a Songsmith addition; the 1176 has none). A 12 dB/oct Butterworth
        // high-pass that only the level detector listens through: the audio path is never filtered.
        // 0 Hz = Off. Changes glide (cutoff and the Off/On blend are smoothed), so there are no clicks.
        // Zero latency (an IIR filter), at the running (possibly oversampled) rate.
        void setDetectorLowCutHz (float hz) noexcept;
        float getDetectorLowCutHz() const noexcept { return lowCutHz; }

        // All Buttons ("British mode"): all four ratio buttons pressed. Sourced (UM p. 9, p. 22):
        // the ratio "goes to somewhere between 12:1 and 20:1", the bias points change, attack
        // lags on initial transients, attack and release keep changing, distortion rises
        // radically. The numbers are tuned (docs/all-buttons.md).
        static constexpr float abRatio = 18.0f;          // nominal; measures 12-20 with the knee
        static constexpr float abThresholdDb = -21.0f;   // bias shift: lower than any single button
        static constexpr float abKneeDb = 4.0f;
        static constexpr float abOverStartDb = 24.0f;    // input overshoot where over-compression starts
        static constexpr float abOverSlope = 1.2f;       // extra dB of GR per dB above that
        static constexpr float abAttackLag = 20.0f;      // attack up to 21x slower while GR is small ...
        static constexpr float abAttackLagDb = 4.0f;     // ... fading out over the first few dB of GR
        static constexpr float abReleaseDb = 12.0f;      // release speeds up with depth: x1 / (1 + GR / 12)

        // In place. Channels beyond maxChannels are left untouched. The gain is linked:
        // every channel gets the same gain, computed from the loudest channel.
        // gainOut (optional, numSamples long) receives the linear gain applied to each sample,
        // for the FET colour stage.
        void process (float* const* channels, int numChannels, int numSamples, float* gainOut = nullptr) noexcept;

        // Steady-state gain reduction for a constant-envelope input at levelDb (peak dBFS),
        // from the static curve (for tests, metering and docs).
        float staticGainReductionDb (float levelDb) const noexcept;

        float getGainReductionDb() const noexcept { return gainReductionDb; }
        // Largest GR since the last call (for the meter); safe to call from another thread.
        float takePeakGainReductionDb() noexcept { return peakGr.exchange (0.0f, std::memory_order_relaxed); }

    private:
        float solve (float levelDb, float previousGr) const noexcept;
        void updateCoefficients() noexcept;

        double fs = 44100.0;
        int ratioIndex = 0;
        float attackKnob = 4.0f, releaseKnob = 4.0f;
        bool grEnabled = true;
        bool allButtons = false;
        float overSlope = 0.0f, overStartDb = 0.0f;

        float k = 3.0f, thresholdDb = -19.5f, kneeDb = 10.0f;   // k = ratio - 1 (feedback gain)
        float attackCoeff = 1.0f, releaseCoeff = 1.0f, grOffCoeff = 1.0f;

        // Detector Low Cut: one Butterworth biquad per channel (double precision: at 4x oversampling the
        // cutoff is a tiny fraction of the sample rate). Transposed direct form II.
        struct Biquad { double z1 = 0.0, z2 = 0.0; };
        void updateLowCutCoefficients() noexcept;
        void advanceLowCutSmoothing() noexcept;

        static constexpr double lowCutGlideSeconds = 0.02;
        static constexpr int lowCutBlock = 16;          // samples between coefficient updates while gliding
        float lowCutHz = 0.0f;                          // requested; 0 = Off
        double lowCutTargetHz = 100.0;                  // the cutoff asked for (kept while Off, so switching on is instant)
        double lowCutCurrentHz = 100.0;                 // the cutoff in use (glides to the target)
        int lowCutCounter = 0;
        double lowCutMix = 0.0, lowCutMixTarget = 0.0;  // 0 = detector hears everything, 1 = fully filtered
        double lowCutGlide = 1.0;                       // per-block smoothing coefficient
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
        std::array<Biquad, maxChannels> lowCutState {};

        float gainReductionDb = 0.0f;
        std::atomic<float> peakGr { 0.0f };
    };
}
