#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// Songsmith Optical Leveller engine (OPT-01): an LA-2A-inspired optical leveller. The design choices, which
// behaviours are published and which are Songsmith's own, and the test that checks each, are written down in
// optical-leveller/docs/engine.md.
//
//   input -> level detector (smoothed power of the loudest channel, in dB; one detector so the stereo image holds)
//         -> gain computer (Peak Reduction sets the threshold; Compress or Limit sets the ratio and knee)
//         -> optical cell: two light-sensitive parts that both darken in about 10 ms, then let go at their own pace.
//            One lets go quickly (about 60 ms to half), the other slowly (0.5 to 5 seconds). The longer and harder
//            the signal has pushed, the more of the reduction sits in the slow part, which is the optical "memory".
//         -> one gain applied to every channel, then Gain
//
// No latency, no look-ahead. process() does not allocate, lock or log; prepare() and reset() are for the message thread.
namespace SongsmithOptical
{
    struct Params
    {
        float peakReduction = 40.0f;   // 0 to 100: how much of the signal drives the optical cell
        float gainDb        = 0.0f;    // 0 to 40: the make-up Gain after the cell
        bool  limit         = false;   // false = Compress (gentle), true = Limit (firm)
    };

    inline constexpr float minPeakReduction = 0.0f, maxPeakReduction = 100.0f;
    inline constexpr float minGainDb = 0.0f, maxGainDb = 40.0f;

    // The level that ends up at "0 VU" on the meter and at which the factory presets are level-matched.
    inline constexpr float referenceLevelDb = -18.0f;

    // --- Published behaviour (Universal Audio's LA-2A specification), used as targets -----------------------------
    inline constexpr float attackSeconds = 0.010f;            // "attack time 10 ms"
    inline constexpr float halfReleaseSeconds = 0.060f;       // "50 % release in about 0.06 s" for a short signal
    inline constexpr float slowestReleaseSeconds = 5.0f;      // "complete release 0.5 to 5 s", by how long and how hard it was pushed
    inline constexpr float quickestSlowReleaseSeconds = 0.5f;

    // --- Songsmith design choices ----------------------------------------------------------------------------------
    inline constexpr float compressRatio = 3.0f, limitRatio = 12.0f;
    inline constexpr float compressKneeDb = 10.0f, limitKneeDb = 4.0f;
    inline constexpr float cellAttackSeconds = 0.0085f;       // with the detector below, the reduction reaches 63 % in about attackSeconds
    inline constexpr float detectorSeconds = 0.002f;          // how quickly the detector follows the signal's power
    inline constexpr float memoryRiseSeconds = 3.0f, memoryFallSeconds = 8.0f;   // how fast the cell's memory builds and fades
    inline constexpr float memoryFullDb = 10.0f;              // reduction held this long makes the memory "full"
    inline constexpr float fastShareQuiet = 0.90f;            // share of the reduction in the quick part after a short signal...
    inline constexpr float fastShareFull = 0.25f;             // ...and when the memory is full

    // The release time constant of the quick part: gives half the release in halfReleaseSeconds once the slow part is
    // taken into account (see docs/engine.md).
    inline constexpr float quickReleaseSeconds = 0.069f;

    // The signal level (RMS dBFS) at which Peak Reduction starts to reduce the gain. Lower numbers start sooner.
    inline float thresholdDb (float peakReduction) noexcept
    {
        return -4.0f - 0.36f * juce::jlimit (minPeakReduction, maxPeakReduction, peakReduction);
    }

    // The steady gain reduction in dB (zero or positive) for a steady level, before the optical cell's timing.
    // A quadratic soft knee centred on the threshold.
    inline float staticReductionDb (float levelDb, float peakReduction, bool limit) noexcept
    {
        const float ratio = limit ? limitRatio : compressRatio, knee = limit ? limitKneeDb : compressKneeDb;
        const float over = levelDb - thresholdDb (peakReduction);
        const float slope = 1.0f - 1.0f / ratio;
        if (over <= -0.5f * knee)
            return 0.0f;
        if (over >= 0.5f * knee)
            return over * slope;
        const float x = over + 0.5f * knee;
        return slope * x * x / (2.0f * knee);
    }

    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
            detectorCoef = coefficient (detectorSeconds);
            attackCoef = coefficient (cellAttackSeconds);
            quickCoef = coefficient (quickReleaseSeconds);
            controlStep = juce::jmax (1, juce::roundToInt (sampleRate / 3000.0));   // the slow quantities update about 3000 times a second
            gain.reset (sampleRate, 0.02);
            reset();
        }

        void reset()
        {
            power = 0.0f;
            quick = slow = memoryDb = 0.0f;
            slowCoef = coefficient (quickestSlowReleaseSeconds);
            fastShare = fastShareQuiet;
            stepCounter = 0;
            maxReduction = 0.0f;
            gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.gainDb));
        }

        void setParams (const Params& p) noexcept
        {
            params = p;
            params.peakReduction = juce::jlimit (minPeakReduction, maxPeakReduction, p.peakReduction);
            params.gainDb = juce::jlimit (minGainDb, maxGainDb, p.gainDb);
            gain.setTargetValue (juce::Decibels::decibelsToGain (params.gainDb));
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
                        loudest = juce::jmax (loudest, x * x);
                }
                power += (loudest - power) * detectorCoef;
                if (power < 1.0e-20f)
                    power = 0.0f;   // keep the detector clear of denormal numbers in silence

                if (stepCounter == 0)
                    updateControl();
                if (++stepCounter >= controlStep)
                    stepCounter = 0;

                // Both parts of the cell darken at the same quick rate, and let go each at their own.
                const float level = power > 0.0f ? 10.0f * std::log10 (power) : -120.0f;
                const float target = staticReductionDb (level, params.peakReduction, params.limit);
                quick += (target * fastShare - quick) * (target * fastShare > quick ? attackCoef : quickCoef);
                slow  += (target * (1.0f - fastShare) - slow) * (target * (1.0f - fastShare) > slow ? attackCoef : slowCoef);
                if (quick < 1.0e-9f) quick = 0.0f;
                if (slow < 1.0e-9f) slow = 0.0f;

                const float reduction = quick + slow;
                maxReduction = juce::jmax (maxReduction, reduction);
                const float linear = reduction > 0.0f ? std::pow (10.0f, -reduction * 0.05f) : 1.0f;
                const float g = gain.getNextValue() * linear;
                for (int c = 0; c < numChannels; ++c)
                    channels[c][i] *= g;
            }
        }

        // The current total gain reduction in dB (positive), for the meter and the tests.
        float getGainReductionDb() const noexcept { return quick + slow; }

        // The deepest reduction since the last read, in dB (positive).
        float readMaxReductionDb() noexcept
        {
            const float r = maxReduction;
            maxReduction = getGainReductionDb();
            return r;
        }

        // How full the cell's memory is, 0 to 1 (for the tests and the documentation).
        float getMemory() const noexcept { return juce::jlimit (0.0f, 1.0f, memoryDb / memoryFullDb); }

    private:
        float coefficient (float seconds) const noexcept
        {
            return 1.0f - std::exp (-1.0f / (float) (sampleRate * (double) seconds));
        }

        // Runs every few hundred microseconds: the memory builds with the reduction held, and sets how the reduction
        // is shared between the quick and the slow part and how slowly the slow part lets go.
        void updateControl() noexcept
        {
            const float reduction = quick + slow;
            const float stepSeconds = (float) controlStep / (float) sampleRate;
            const float seconds = reduction > memoryDb ? memoryRiseSeconds : memoryFallSeconds;
            memoryDb += (reduction - memoryDb) * (1.0f - std::exp (-stepSeconds / seconds));
            if (memoryDb < 1.0e-6f)
                memoryDb = 0.0f;

            const float full = getMemory();
            fastShare = fastShareQuiet + (fastShareFull - fastShareQuiet) * full;
            const float slowSeconds = quickestSlowReleaseSeconds + (slowestReleaseSeconds - quickestSlowReleaseSeconds) * full;
            slowCoef = 1.0f - std::exp (-1.0f / (float) (sampleRate * (double) slowSeconds));
        }

        Params params;
        double sampleRate = 44100.0;
        float detectorCoef = 0.0f, attackCoef = 0.0f, quickCoef = 0.0f, slowCoef = 0.0f;
        int controlStep = 15, stepCounter = 0;
        float power = 0.0f, quick = 0.0f, slow = 0.0f, memoryDb = 0.0f, fastShare = fastShareQuiet, maxReduction = 0.0f;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain { 1.0f };
    };
}
