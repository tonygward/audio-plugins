#pragma once

#include <array>
#include <atomic>
#include <cmath>

// Clean feedback gain-reduction engine (no colour), testable without the plugin.
//
// Signal flow (UG p. 7 Fig. 1, p. 10; see fet/docs/references.md and fet/docs/engine.md):
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
        static constexpr int numRatios = 4;

        // Gain computer constants per ratio button (4, 8, 12, 20). Modelling choices, see
        // docs/engine.md: thresholds rise with ratio (UM p. 9, p. 21), soft knee (UM p. 21),
        // 20:1 calibrated like the Q-bias procedure (UG p. 5).
        static constexpr std::array<float, numRatios> ratios       { 4.0f, 8.0f, 12.0f, 20.0f };
        static constexpr std::array<float, numRatios> thresholdsDb { -19.5f, -17.0f, -15.5f, -13.8f };   // peak dBFS
        static constexpr std::array<float, numRatios> kneeWidthsDb { 10.0f, 8.0f, 7.0f, 6.0f };

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

        // In place. Channels beyond maxChannels are left untouched. The gain is linked:
        // every channel gets the same gain, computed from the loudest channel.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

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

        float k = 3.0f, thresholdDb = -19.5f, kneeDb = 10.0f;   // k = ratio - 1 (feedback gain)
        float attackCoeff = 1.0f, releaseCoeff = 1.0f, grOffCoeff = 1.0f;

        float gainReductionDb = 0.0f;
        std::atomic<float> peakGr { 0.0f };
    };
}
