#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>

// Engine for the Pultec-style EQ (PEQ-01), with three modes (PEQ-04). Up to four filters in series per channel:
//
//   Tone Shaper (inspired by the EQP-1A, the original and default mode):
//     low shelf boost  (20, 30, 60 or 100 Hz, 0 to 13.5 dB)
//     low shelf cut    (the same switch, 0 to 17.5 dB, with its corner a little higher: see below)
//     high peak boost  (3, 4, 5, 8, 10, 12 or 16 kHz, 0 to 18 dB, adjustable bandwidth)
//     high shelf cut   (5, 10 or 20 kHz, 0 to 16 dB)
//   Mid Shaper (inspired by the MEQ-5):
//     low-mid peak boost (200, 300, 500, 700 Hz or 1 kHz, 0 to 10 dB)
//     mid peak dip       (200 Hz to 7 kHz in ten steps, 0 to 10 dB)
//     high-mid peak boost (1.5, 2, 3, 4.5 or 6 kHz, 0 to 10 dB)
//   Frequency Filter (inspired by the HLF-3C):
//     high-pass filter (Off, 30, 50, 80, 120 or 200 Hz)
//     low-pass filter  (Off, 5, 7, 10 or 15 kHz)
//
// The EQP-1A frequency choices and gain ranges are the published panel values. The Mid Shaper and Frequency Filter
// steps and every filter shape, the bandwidth mapping and the corner offset of the low cut are Songsmith design choices,
// listed with their tests in pultec-eq/docs/reference.md. This is an inspired design, not a circuit model.
// Changing a frequency switch or the mode fades from the old filters to the new ones over 20 ms, so it does not click.
// Nothing here is latency: the filters are minimum phase biquads.
// process() does not allocate, lock or log.
namespace SongsmithPassive
{
    inline constexpr std::array<float, 4> lowFrequencies  { 20.0f, 30.0f, 60.0f, 100.0f };
    inline constexpr std::array<float, 7> highBoostFrequencies { 3000.0f, 4000.0f, 5000.0f, 8000.0f, 10000.0f, 12000.0f, 16000.0f };
    inline constexpr std::array<float, 3> highCutFrequencies { 5000.0f, 10000.0f, 20000.0f };

    // Mid Shaper (design choices).
    inline constexpr std::array<float, 5>  midLowFrequencies  { 200.0f, 300.0f, 500.0f, 700.0f, 1000.0f };
    inline constexpr std::array<float, 10> midDipFrequencies  { 200.0f, 300.0f, 500.0f, 700.0f, 1000.0f, 1500.0f, 2000.0f, 3000.0f, 4500.0f, 7000.0f };
    inline constexpr std::array<float, 5>  midHighFrequencies { 1500.0f, 2000.0f, 3000.0f, 4500.0f, 6000.0f };

    // Frequency Filter (design choices). Position 0 is Off (the frequency 0 means "no filter").
    inline constexpr std::array<float, 6> highPassFrequencies { 0.0f, 30.0f, 50.0f, 80.0f, 120.0f, 200.0f };
    inline constexpr std::array<float, 5> lowPassFrequencies  { 0.0f, 5000.0f, 7000.0f, 10000.0f, 15000.0f };

    inline constexpr float maxLowBoostDb = 13.5f, maxLowCutDb = 17.5f, maxHighBoostDb = 18.0f, maxHighCutDb = 16.0f;
    inline constexpr float maxMidBoostDb = 10.0f, maxMidDipDb = 10.0f;

    // Design choices (see reference.md).
    inline constexpr float lowCutCornerRatio = 3.0f;    // the low cut's corner sits this far above the boost corner
    inline constexpr float minBandwidthQ = 0.35f, maxBandwidthQ = 3.0f;   // broad to narrow high boost
    inline constexpr float shelfQ = 0.5f;               // gentle shelves (Butterworth-like is 0.707)
    inline constexpr float midBoostQ = 0.55f, midDipQ = 0.6f;             // broad mid shapes, wider than the high boost's narrowest
    inline constexpr float filterQ = 0.7071f;           // each filter is one 12 dB per octave section, flat in the pass band

    enum class Mode { toneShaper = 0, midShaper = 1, frequencyFilter = 2 };
    inline constexpr int numModes = 3;

    struct Params
    {
        Mode  mode = Mode::toneShaper;

        // Tone Shaper
        int   lowFrequencyIndex = 2;        // 0..3
        float lowBoostDb = 0.0f;            // 0..13.5
        float lowCutDb = 0.0f;              // 0..17.5
        int   highBoostFrequencyIndex = 3;  // 0..6
        float highBoostDb = 0.0f;           // 0..18
        float bandwidth = 0.5f;             // 0 (broad) to 1 (narrow)
        int   highCutFrequencyIndex = 1;    // 0..2
        float highCutDb = 0.0f;             // 0..16

        // Mid Shaper
        int   midLowFrequencyIndex = 2;     // 0..4 (500 Hz)
        float midLowBoostDb = 0.0f;         // 0..10
        int   midDipFrequencyIndex = 4;     // 0..9 (1 kHz)
        float midDipDb = 0.0f;              // 0..10
        int   midHighFrequencyIndex = 2;    // 0..4 (3 kHz)
        float midHighBoostDb = 0.0f;        // 0..10

        // Frequency Filter
        int   highPassIndex = 0;            // 0..5, 0 = Off
        int   lowPassIndex = 0;             // 0..4, 0 = Off
    };

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;   // double: a 20 Hz shelf at 48 kHz needs it

        // Magnitude in dB at a frequency, from the coefficients (for tests and the response curve).
        float magnitudeDb (double frequency, double sampleRate) const noexcept
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            const auto num = b0 + b1 * z1 + b2 * z2;
            const auto den = 1.0 + a1 * z1 + a2 * z2;
            return (float) (20.0 * std::log10 (std::abs (num) / std::abs (den)));
        }
    };

    // Audio EQ Cookbook (Robert Bristow-Johnson) shelves, peaking, high-pass and low-pass filters, with a gain in dB.
    Biquad makeLowShelf (double sampleRate, double frequency, double q, double gainDb) noexcept;
    Biquad makeHighShelf (double sampleRate, double frequency, double q, double gainDb) noexcept;
    Biquad makePeak (double sampleRate, double frequency, double q, double gainDb) noexcept;
    Biquad makeHighPass (double sampleRate, double frequency, double q) noexcept;
    Biquad makeLowPass (double sampleRate, double frequency, double q) noexcept;

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

        // The four gains (dB) the current mode works with: what each slot's filter is asked to do. Unused slots are 0.
        static std::array<float, 4> targetGains (const Params& p) noexcept;
        // The filters for a mode at a given set of gains.
        static std::array<Biquad, numSections> buildSections (const Params& p, const std::array<float, 4>& gainsDb, double sampleRate) noexcept;
        static bool isNeutral (const Params& p) noexcept;   // nothing would change the sound

        void updateSections (const std::array<float, 4>& gainsDb) noexcept;

        double fs = 48000.0;
        Params params;
        std::array<juce::SmoothedValue<float>, 4> gain;   // dB, per slot (see targetGains)
        std::array<Biquad, numSections> sections;
        std::array<std::array<std::array<double, 2>, numSections>, maxChannels> state {};   // transposed direct form II
        int chunkRemaining = 0;   // samples left before the next coefficient refresh (so block size never matters)
        bool flat = true;

        // Changing a frequency switch or the mode moves a filter's corner or type at once, which would click. The engine
        // keeps the filters as they were, with their memory, and fades from them to the new ones over fadeSeconds.
        static constexpr double fadeSeconds = 0.02;
        std::array<Biquad, numSections> fadeSections;
        std::array<std::array<std::array<double, 2>, numSections>, maxChannels> fadeState {};
        int fadeLength = 960, fadeRemaining = 0;
    };
}
