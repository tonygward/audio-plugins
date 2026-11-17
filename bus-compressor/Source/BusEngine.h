#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Songsmith Bus Compressor engine (BUS-01): a stereo-linked bus compressor in the style of the compressor on
// the SSL 4000 G series console. Which behaviours are published, which are Songsmith's own design choices, and the
// test that checks each, are written down in bus-compressor/docs/engine.md.
//
//   input -> level detector (the louder of the channels' peaks, in dB; one detector, so the stereo image holds)
//         -> gain computer (Threshold and Ratio, with a short soft knee)
//         -> attack and release, applied to the gain reduction in decibels (Release can be Auto)
//         -> one gain applied to every channel, then Make-up Gain
//
// No latency, no look-ahead. process() does not allocate, lock or log; prepare() and reset() are for the message thread.
namespace SongsmithBus
{
    // The steps of the stepped controls. The values are the published ones for the hardware.
    inline constexpr std::array<float, 3> ratios { 2.0f, 4.0f, 10.0f };                                   // to 1
    inline constexpr std::array<float, 6> attackMilliseconds { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
    inline constexpr std::array<float, 5> releaseSeconds { 0.1f, 0.3f, 0.6f, 1.2f, 0.0f };               // the last step is Auto
    inline constexpr int autoReleaseIndex = 4;

    struct Params
    {
        float thresholdDial = 4.0f;   // -15 to +10, as printed around the Threshold knob
        int   ratioIndex    = 0;      // into ratios
        int   attackIndex   = 4;      // into attackMilliseconds
        int   releaseIndex  = 4;      // into releaseSeconds (the last is Auto)
        float makeupDb      = 0.0f;   // 0 to 15
    };

    inline constexpr float minThresholdDial = -15.0f, maxThresholdDial = 10.0f;
    inline constexpr float minMakeupDb = 0.0f, maxMakeupDb = 15.0f;

    // --- Songsmith design choices --------------------------------------------------------------------------------
    // 0 on the Threshold dial sits at the suite's reference level of -18 dBFS (so +4 is -14 dBFS, and the dial's
    // -15 to +10 span is -33 to -8 dBFS). The hardware's own scale is in volts at its analogue input.
    inline constexpr float dialZeroDbfs = -18.0f;
    inline constexpr float referenceLevelDb = -12.0f;   // the peak level the factory starting points are level-matched at
    inline constexpr float kneeDb = 4.0f;               // width of the soft knee around the threshold
    inline constexpr float detectorReleaseSeconds = 0.010f;   // how quickly the peak detector lets go (it never lags on the way up)
    inline constexpr float autoFastSeconds = 0.1f, autoSlowSeconds = 1.2f;   // Auto release: the quickest and the slowest let-go
    inline constexpr float autoDepthSeconds = 1.5f;     // how slowly Auto notices that the compressor has been working hard for a while
    inline constexpr float autoFullDb = 6.0f;           // a held reduction this deep makes Auto release as slow as it gets

    inline float clampDial (float dial) noexcept { return juce::jlimit (minThresholdDial, maxThresholdDial, dial); }
    inline float thresholdDbfs (float dial) noexcept { return dialZeroDbfs + clampDial (dial); }

    // The steady gain reduction in dB (zero or positive) for a steady peak level, before any attack or release.
    // A quadratic soft knee centred on the threshold.
    inline float staticReductionDb (float levelDb, float thresholdDial, int ratioIndex) noexcept
    {
        const float ratio = ratios[(size_t) juce::jlimit (0, (int) ratios.size() - 1, ratioIndex)];
        const float over = levelDb - thresholdDbfs (thresholdDial);
        const float slope = 1.0f - 1.0f / ratio;
        if (over <= -0.5f * kneeDb)
            return 0.0f;
        if (over >= 0.5f * kneeDb)
            return over * slope;
        const float x = over + 0.5f * kneeDb;
        return slope * x * x / (2.0f * kneeDb);
    }

    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            detectorCoef = coefficient (detectorReleaseSeconds);
            controlStep = juce::jmax (1, juce::roundToInt (sampleRate / 3000.0));   // the slow quantities update about 3000 times a second
            makeup.reset (sampleRate, 0.02);
            updateTimes();
            reset();
        }

        void reset()
        {
            peak = 0.0f;
            reduction = depthDb = 0.0f;
            stepCounter = 0;
            maxReduction = 0.0f;
            makeup.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.makeupDb));
        }

        void setParams (const Params& p) noexcept
        {
            params.thresholdDial = clampDial (p.thresholdDial);
            params.ratioIndex = juce::jlimit (0, (int) ratios.size() - 1, p.ratioIndex);
            params.attackIndex = juce::jlimit (0, (int) attackMilliseconds.size() - 1, p.attackIndex);
            params.releaseIndex = juce::jlimit (0, (int) releaseSeconds.size() - 1, p.releaseIndex);
            params.makeupDb = juce::jlimit (minMakeupDb, maxMakeupDb, p.makeupDb);
            makeup.setTargetValue (juce::Decibels::decibelsToGain (params.makeupDb));
            updateTimes();
        }

        const Params& getParams() const noexcept { return params; }

        void process (float* const* channels, int numChannels, int numSamples) noexcept
        {
            numChannels = juce::jlimit (0, maxChannels, numChannels);
            for (int i = 0; i < numSamples; ++i)
            {
                float loudest = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                {
                    const float x = channels[c][i];
                    if (std::isfinite (x))
                        loudest = juce::jmax (loudest, std::abs (x));
                }

                // The peak detector rises at once and lets go over a few milliseconds.
                if (loudest > peak)
                    peak = loudest;
                else
                    peak += (loudest - peak) * detectorCoef;
                if (peak < 1.0e-12f)
                    peak = 0.0f;

                if (stepCounter == 0)
                    updateControl();
                if (++stepCounter >= controlStep)
                    stepCounter = 0;

                const float level = peak > 1.0e-9f ? 20.0f * std::log10 (peak) : -180.0f;
                const float target = staticReductionDb (level, params.thresholdDial, params.ratioIndex);
                reduction += (target - reduction) * (target > reduction ? attackCoef : releaseCoef);
                if (reduction < 1.0e-9f)
                    reduction = 0.0f;

                maxReduction = juce::jmax (maxReduction, reduction);
                const float linear = reduction > 0.0f ? std::exp (-reduction * 0.115129255f) : 1.0f;
                const float g = makeup.getNextValue() * linear;
                for (int c = 0; c < numChannels; ++c)
                    channels[c][i] *= g;
            }
        }

        // The current gain reduction in dB (positive), for the meter and the tests.
        float getGainReductionDb() const noexcept { return reduction; }

        // The deepest reduction since the last read, in dB (positive).
        float readMaxReductionDb() noexcept
        {
            const float r = maxReduction;
            maxReduction = reduction;
            return r;
        }

        // The release time constant in seconds now in use (for Auto it moves with how hard the compressor has been working).
        float getReleaseSeconds() const noexcept
        {
            return params.releaseIndex == autoReleaseIndex ? autoSecondsFor (depthDb) : releaseSeconds[(size_t) params.releaseIndex];
        }

        float getAutoDepthDb() const noexcept { return depthDb; }

    private:
        float coefficient (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (float) (sampleRate * (double) seconds));
        }

        static float autoSecondsFor (float depth) noexcept
        {
            const float held = juce::jlimit (0.0f, 1.0f, depth / autoFullDb);
            return autoFastSeconds + (autoSlowSeconds - autoFastSeconds) * held;
        }

        void updateTimes() noexcept
        {
            attackCoef = coefficient (attackMilliseconds[(size_t) params.attackIndex] * 0.001f);
            if (params.releaseIndex != autoReleaseIndex)
                releaseCoef = coefficient (releaseSeconds[(size_t) params.releaseIndex]);
        }

        // Runs every few hundred microseconds: Auto release notices how hard the compressor has been working and
        // chooses how quickly to let go (a short hit lets go quickly, a long hard push lets go slowly).
        void updateControl() noexcept
        {
            const float stepSeconds = (float) controlStep / (float) sampleRate;
            depthDb += (reduction - depthDb) * (1.0f - std::exp (-stepSeconds / autoDepthSeconds));
            if (depthDb < 1.0e-6f)
                depthDb = 0.0f;
            if (params.releaseIndex == autoReleaseIndex)
                releaseCoef = coefficient (autoSecondsFor (depthDb));
        }

        Params params;
        double sampleRate = 44100.0;
        float detectorCoef = 0.0f, attackCoef = 0.0f, releaseCoef = 0.0f;
        int controlStep = 15, stepCounter = 0;
        float peak = 0.0f, reduction = 0.0f, depthDb = 0.0f, maxReduction = 0.0f;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> makeup { 1.0f };
    };
}
