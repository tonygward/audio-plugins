#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace Max
{
    // Look-ahead sample-peak brickwall limiter with a shared (linked) gain for all channels.
    //
    // Signal flow per sample n (all channels share one gain):
    //   x[n]   = input[n] * baseline            baseline = Out Ceiling - Threshold (dB), smoothed
    //   p[n]   = max over channels |x[n]|
    //   r[n]   = min (1, ceiling / p[n])        the gain sample n needs to land on the ceiling
    //   e[n]   = min (r[n], e[n-1] + (1 - e[n-1]) * (1 - a))   instant attack, exponential release
    //   m[n]   = min of e over [n-L, n]         sliding minimum, L + 1 samples
    //   g[n]   = mean of m over [n-L, n]        box-car smoothing: a linear L-sample attack ramp
    //   out[n] = x[n-L] * min (g[n], r[n-L])
    //
    // Why it is brickwall at the exact delayed sample: every m[n-j] (j = 0..L) is a minimum over
    // a window that contains n-L, so each is <= e[n-L] <= r[n-L]; their mean g[n] is too. The final
    // min with r[n-L] only removes float rounding in the running mean. So |out[n]| <= ceiling for
    // every sample, with no clipper.
    //
    // Design choices (Songsmith, not from the L1 manual):
    //  - Look-ahead: 1.5 ms (rounded up to whole samples), which is also the latency and the attack
    //    ramp. 67 samples at 44.1 kHz, 72 at 48 kHz, 144 at 96 kHz.
    //  - Release law: the gain recovers exponentially towards unity with time constant = Release
    //    (63 % of the gain reduction is recovered after one Release time), then passes through the
    //    same L-sample box-car, so very short releases are still at least ~1.5 ms smooth.
    //  - Threshold / ceiling changes glide over 20 ms (baseline gain multiplicatively, the ceiling
    //    linearly). The ceiling that protects a sample is the one current when it was detected.
    class BrickwallLimiter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr double lookAheadSeconds = 0.0015;
        static constexpr double parameterSmoothingSeconds = 0.02;

        static int lookAheadSamples (double sampleRate) noexcept
        {
            return juce::jmax (1, (int) std::ceil (lookAheadSeconds * sampleRate - 1.0e-9));
        }

        // Allocates. Call before processing and whenever the sample rate changes.
        void prepare (double sampleRate);
        void reset() noexcept;   // clears the delay, the envelope and the meters; snaps the parameters

        int getLatencySamples() const noexcept { return lookAhead; }

        // Targets, in dB / ms. Safe to call every block from the audio thread.
        void setThresholdDb (float db) noexcept;
        void setCeilingDb (float db) noexcept;
        void setReleaseMs (float ms) noexcept;

        // Processes in place: 'channels' holds numChannels pointers (1 or 2) of numSamples each.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        // Lowest limiter gain (excluding the baseline gain) applied since the last call, in dB (<= 0).
        float readAndResetMaxGainReductionDb() noexcept;
        float getCurrentGain() const noexcept { return lastGain; }

    private:
        double fs = 48000.0;
        int lookAhead = 72;
        int window = 73;   // lookAhead + 1

        // Delay line (per channel) and the needed gain r, both lookAhead + 1 long.
        std::vector<float> delay[maxChannels];
        std::vector<float> needRing;
        int writePos = 0;

        // Monotonic deque for the sliding minimum (values and their sample indices).
        std::vector<float> dqVal;
        std::vector<long long> dqIdx;
        int dqHead = 0, dqSize = 0;
        long long sampleIndex = 0;

        // Box-car of the sliding minimum.
        std::vector<float> boxRing;
        double boxSum = 0.0;
        int boxPos = 0, resumCountdown = 0;

        float env = 1.0f;          // release envelope e
        float releaseCoef = 0.0f;  // a = exp (-1 / (release * fs))
        float releaseMs = 1.0f;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> baseline { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ceiling { 1.0f };
        float thresholdDb = 0.0f, ceilingDb = 0.0f;

        float minGain = 1.0f, lastGain = 1.0f;

        void updateBaselineTarget() noexcept;
        float slidingMin (float value) noexcept;
    };
}
