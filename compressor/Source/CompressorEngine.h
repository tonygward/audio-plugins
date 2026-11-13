#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// Songsmith Compressor engine (COMP-01, slice 1): a clean, feed-forward, stereo-linked compressor.
// Design choices are written down in compressor/docs/engine.md, with the test that checks each.
//
//   input -> level detector (peak of the loudest channel, in dB)
//         -> gain computer (threshold, continuous ratio, soft knee)
//         -> smoothing of the gain reduction (attack when it grows, release when it falls)
//         -> one gain applied to every channel (stereo link), then make-up gain
//         -> mix with the untouched input (parallel compression)
//
// No latency, no look-ahead, no oversampling. The Analog Warmth stage is a later slice.
// process() does not allocate, lock or log. prepare() and reset() are for the message thread.
namespace SongsmithComp
{
    struct Params
    {
        float thresholdDb = -24.0f;   // -60 to 0
        float ratio       = 2.0f;     // 1 to 20 (1 = no compression)
        float attackMs    = 10.0f;    // 0.1 to 100
        float releaseMs   = 100.0f;   // 10 to 2000
        float kneeDb      = 6.0f;     // 0 (hard) to 24 (soft)
        float makeupDb    = 0.0f;     // 0 to 24, used when autoMakeup is off
        bool  autoMakeup  = false;    // level-match at autoMakeupReferenceDb instead of makeupDb
        float mix         = 1.0f;     // 0 to 1 (1 = fully compressed)
    };

    inline constexpr float minThresholdDb = -60.0f, maxThresholdDb = 0.0f;
    inline constexpr float minRatio = 1.0f, maxRatio = 20.0f;
    inline constexpr float minAttackMs = 0.1f, maxAttackMs = 100.0f;
    inline constexpr float minReleaseMs = 10.0f, maxReleaseMs = 2000.0f;
    inline constexpr float minKneeDb = 0.0f, maxKneeDb = 24.0f;
    inline constexpr float minMakeupDb = 0.0f, maxMakeupDb = 24.0f;

    // Auto make-up restores the level of a typical programme signal at this level (dBFS), so a
    // gentle low-threshold setting stays level-matched without a fixed 0 dBFS assumption.
    inline constexpr float autoMakeupReferenceDb = -18.0f;

    // The static curve: gain change in dB (zero or negative) for a steady input level in dB.
    // Quadratic soft knee centred on the threshold, as in the standard published formulation.
    inline float staticGainDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const float slope = 1.0f / juce::jmax (1.0f, ratio) - 1.0f;   // 0 at 1:1, towards -1 at infinity
        const float over = inputDb - thresholdDb;
        if (kneeDb <= 0.0f)
            return over > 0.0f ? slope * over : 0.0f;
        if (2.0f * over < -kneeDb)
            return 0.0f;
        if (2.0f * over <= kneeDb)
        {
            const float t = over + kneeDb * 0.5f;
            return slope * t * t / (2.0f * kneeDb);
        }
        return slope * over;
    }

    // The make-up gain that auto mode applies, in dB (never negative).
    inline float autoMakeupDb (const Params& p) noexcept
    {
        return juce::jmax (0.0f, -staticGainDb (autoMakeupReferenceDb, p.thresholdDb, p.ratio, p.kneeDb));
    }

    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double sampleRate);
        void reset() noexcept;
        void setParams (const Params& p) noexcept;

        // In place. Any block size, 1 or 2 channels.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        // Since the last call to readMeters(): the deepest gain reduction (a positive number of dB)
        // and the highest absolute sample at the input and the output (linear).
        struct Meters { float gainReductionDb = 0.0f, inputPeak = 0.0f, outputPeak = 0.0f; };
        Meters readMeters() noexcept;

        // The gain reduction now (a positive number of dB), for tests and meters.
        float getGainReductionDb() const noexcept { return -grDb; }

    private:
        void updateCoefficients() noexcept;

        double fs = 48000.0;
        Params params;
        float attackCoeff = 0.0f, releaseCoeff = 0.0f;
        float grDb = 0.0f;   // smoothed gain reduction, zero or negative
        juce::SmoothedValue<float> makeupGain { 1.0f }, mix { 1.0f };
        float meterGr = 0.0f, meterIn = 0.0f, meterOut = 0.0f;
    };
}
