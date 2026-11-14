#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "TruePeak.h"

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
    //   g[n]   = two cascaded box-car means of m (lengths L1 and L2, L1 + L2 - 2 = L): an S-shaped
    //            attack over the same L samples (MAX-10; a single box-car gave a straight ramp)
    //   out[n] = x[n-L] * min (g[n], r[n-L])
    //
    // Why it is brickwall at the exact delayed sample: g[n] is a weighted mean of m[n-j] for
    // j = 0..L1+L2-2 = 0..L, and every m[n-j] is a minimum over a window that contains n-L, so each
    // is <= e[n-L] <= r[n-L]; their weighted mean g[n] is too. The final
    // min with r[n-L] only removes float rounding in the running mean. So |out[n]| <= ceiling for
    // every sample, with no clipper.
    //
    // Design choices (Songsmith, not from the L1 manual):
    //  - Look-ahead: 1.5 ms (rounded up to whole samples), which is also the latency and the attack
    //    ramp. 67 samples at 44.1 kHz, 72 at 48 kHz, 144 at 96 kHz.
    //  - Release law: the gain recovers exponentially towards unity with time constant = Release
    //    (63 % of the gain reduction is recovered after one Release time), then passes through the
    //    same L-sample smoothing, so very short releases are still at least ~1.5 ms smooth.
    //  - Attack shape (MAX-10): two box-cars instead of one. The gain still moves only inside the
    //    look-ahead and still lands exactly on time, but it starts and finishes its move gently
    //    (a trapezoid-weighted mean, an S-curve) instead of with corners, which puts less
    //    distortion into the signal on hard limiting. Same latency.
    //  - Threshold / ceiling changes glide over 20 ms (baseline gain multiplicatively, the ceiling
    //    linearly). The ceiling that protects a sample is the one current when it was detected.
    //  - Peak domain (step 03): p[n] is either the sample peak or the true-peak estimate of
    //    TruePeakDetector (8x, 24 taps/phase): the largest reconstructed value on both sides of the
    //    sample. The audio always waits the detector's 12 samples, so both modes have the same
    //    latency (look-ahead + 12) and switching never moves the audio in time.
    //  - Auto release (step 03, Songsmith recovery law; Waves ARC was only a behavioural reference):
    //    a sustained-gain estimate s follows e (40 ms going down, 400 ms coming back up). While
    //    e < s the gain is recovering from a transient and uses the manual Release; once e >= s it
    //    recovers with max (Release, 150 ms). Short peaks therefore recover as fast as the manual
    //    setting, while dense material sits at a steady gain instead of pumping with the waveform.
    class BrickwallLimiter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr double lookAheadSeconds = 0.0015;
        static constexpr double parameterSmoothingSeconds = 0.02;
        static constexpr double autoSlowReleaseMs = 150.0, autoSustainAttackMs = 40.0, autoSustainReleaseMs = 400.0;

        static int lookAheadSamples (double sampleRate) noexcept
        {
            return juce::jmax (1, (int) std::ceil (lookAheadSeconds * sampleRate - 1.0e-9));
        }

        // Total latency: the look-ahead plus the true-peak detector delay (12 samples). It is the same
        // in both peak modes, so switching mode never changes the latency the host compensates.
        static int latencySamples (double sampleRate) noexcept
        {
            return lookAheadSamples (sampleRate) + TruePeakDetector::delaySamples;
        }

        // Allocates. Call before processing and whenever the sample rate changes.
        void prepare (double sampleRate);
        void reset() noexcept;   // clears the delay, the envelope and the meters; snaps the parameters

        int getLatencySamples() const noexcept { return lookAhead + TruePeakDetector::delaySamples; }

        // Targets, in dB / ms. Safe to call every block from the audio thread.
        void setThresholdDb (float db) noexcept;
        void setCeilingDb (float db) noexcept;
        void setReleaseMs (float ms) noexcept;
        void setTruePeak (bool on) noexcept    { truePeak = on; }
        void setAutoRelease (bool on) noexcept { autoRelease = on; }
        // Linear amount taken off the ceiling (dither headroom, see Dither.h). 0 when dither is off.
        void setCeilingHeadroom (float linear) noexcept { headroom = juce::jmax (0.0f, linear); }
        bool isTruePeak() const noexcept       { return truePeak; }
        bool isAutoRelease() const noexcept    { return autoRelease; }

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

        // Running mean over a fixed number of samples, re-added exactly now and then so the sum never drifts.
        struct BoxCar
        {
            std::vector<float> ring;
            double sum = 0.0;
            int pos = 0, countdown = 0;

            void prepare (int length)  { ring.assign ((size_t) juce::jmax (1, length), 1.0f); reset(); }
            void reset() noexcept      { std::fill (ring.begin(), ring.end(), 1.0f); sum = (double) ring.size(); pos = 0; countdown = (int) ring.size(); }

            float push (float v) noexcept
            {
                const int n = (int) ring.size();
                sum += (double) v - (double) ring[(size_t) pos];
                ring[(size_t) pos] = v;
                pos = pos + 1 == n ? 0 : pos + 1;
                if (--countdown <= 0)
                {
                    double s = 0.0;
                    for (float x : ring)
                        s += (double) x;
                    sum = s;
                    countdown = n;
                }
                return (float) (sum / (double) n);
            }
        };

        // Two box-cars of the sliding minimum, L1 + L2 - 2 = lookAhead (see the class comment).
        BoxCar box1, box2;

        // True-peak detection (step 03): the audio waits delaySamples for the detector.
        TruePeakDetector detector;
        std::vector<float> preDelay[maxChannels];
        int prePos = 0;
        bool truePeak = false, autoRelease = false;

        float env = 1.0f;          // release envelope e
        float sustained = 1.0f;    // auto release: sustained gain estimate
        float slowCoef = 0.0f, sustainDownCoef = 0.0f, sustainUpCoef = 0.0f;
        float releaseCoef = 0.0f;  // a = exp (-1 / (release * fs))
        float releaseMs = 1.0f;
        double coefRate = 0.0;     // sample rate the release coefficients were built for

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> baseline { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> ceiling { 1.0f };
        float thresholdDb = 0.0f, ceilingDb = 0.0f;
        float headroom = 0.0f;

        float minGain = 1.0f, lastGain = 1.0f;

        void updateBaselineTarget() noexcept;
        float slidingMin (float value) noexcept;
    };
}
