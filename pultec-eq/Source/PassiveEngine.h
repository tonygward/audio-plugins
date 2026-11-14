#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

// Engine for the planned Pultec-style EQ (PEQ-01, slice 1). Four filters in series per channel:
//
//   low shelf boost  (20, 30, 60 or 100 Hz, 0 to 13.5 dB)
//   low shelf cut    (the same switch, 0 to 17.5 dB, with its corner a little higher: see below)
//   high peak boost  (3, 4, 5, 8, 10, 12 or 16 kHz, 0 to 18 dB, adjustable bandwidth)
//   high shelf cut   (5, 10 or 20 kHz, 0 to 16 dB)
//
// The frequency choices and gain ranges are the published EQP-1A panel values. The filter shapes,
// the bandwidth mapping and the corner offset of the low cut are Songsmith design choices, listed
// with their tests in pultec-eq/docs/reference.md. This is an inspired design, not a circuit model.
// Nothing here is latency: the filters are minimum phase biquads.
// process() does not allocate, lock or log.
namespace SongsmithPassive
{
    inline constexpr std::array<float, 4> lowFrequencies  { 20.0f, 30.0f, 60.0f, 100.0f };
    inline constexpr std::array<float, 7> highBoostFrequencies { 3000.0f, 4000.0f, 5000.0f, 8000.0f, 10000.0f, 12000.0f, 16000.0f };
    inline constexpr std::array<float, 3> highCutFrequencies { 5000.0f, 10000.0f, 20000.0f };

    inline constexpr float maxLowBoostDb = 13.5f, maxLowCutDb = 17.5f, maxHighBoostDb = 18.0f, maxHighCutDb = 16.0f;

    // Design choices (see reference.md).
    inline constexpr float lowCutCornerRatio = 3.0f;    // the low cut's corner sits this far above the boost corner
    inline constexpr float minBandwidthQ = 0.35f, maxBandwidthQ = 3.0f;   // broad to narrow high boost
    inline constexpr float shelfQ = 0.5f;               // gentle shelves (Butterworth-like is 0.707)

    struct Params
    {
        int   lowFrequencyIndex = 2;        // 0..3
        float lowBoostDb = 0.0f;            // 0..13.5
        float lowCutDb = 0.0f;              // 0..17.5
        int   highBoostFrequencyIndex = 3;  // 0..6
        float highBoostDb = 0.0f;           // 0..18
        float bandwidth = 0.5f;             // 0 (broad) to 1 (narrow)
        int   highCutFrequencyIndex = 1;    // 0..2
        float highCutDb = 0.0f;             // 0..16
    };

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;   // double: a 20 Hz shelf at 48 kHz needs it

        // Magnitude in dB at a frequency, from the coefficients (for tests and the future response curve).
        float magnitudeDb (double frequency, double sampleRate) const noexcept
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            const auto num = b0 + b1 * z1 + b2 * z2;
            const auto den = 1.0 + a1 * z1 + a2 * z2;
            return (float) (20.0 * std::log10 (std::abs (num) / std::abs (den)));
        }
    };

    // Audio EQ Cookbook (Robert Bristow-Johnson) shelves and peaking filter, with a gain in dB.
    Biquad makeLowShelf (double sampleRate, double frequency, double q, double gainDb) noexcept;
    Biquad makeHighShelf (double sampleRate, double frequency, double q, double gainDb) noexcept;
    Biquad makePeak (double sampleRate, double frequency, double q, double gainDb) noexcept;

    class Engine
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double sampleRate);
        void reset() noexcept;
        void setParams (const Params& p) noexcept;

        // In place, 1 or 2 channels, any block size.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        // The combined response in dB at the settings the engine is heading for (not mid-smoothing).
        float responseDb (double frequency) const noexcept;
        double getSampleRate() const noexcept { return fs; }

    private:
        static constexpr int numSections = 4;
        static constexpr int chunk = 16;   // coefficients are refreshed this often while gains glide

        void updateSections (const std::array<float, 4>& gainsDb) noexcept;

        double fs = 48000.0;
        Params params;
        std::array<juce::SmoothedValue<float>, 4> gain;   // dB: low boost, low cut (as a negative), high boost, high cut (negative)
        std::array<Biquad, numSections> sections;
        std::array<std::array<std::array<double, 2>, numSections>, maxChannels> state {};   // transposed direct form II
        int chunkRemaining = 0;   // samples left before the next coefficient refresh (so block size never matters)
    bool flat = true;
    };
}
