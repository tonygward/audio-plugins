#pragma once

#include <array>

#include "SvfFilter.h"

namespace EQ
{
    struct BandSettings
    {
        bool active = false;              // in use AND enabled
        FilterType type = FilterType::Bell;
        float freq = 1000.0f;
        float gain = 0.0f;
        float q    = defaultQ;
        int slope  = defaultSlope;        // index into slopeOrders (cut types only)
        StereoMode stereo = StereoMode::Stereo;

        // Dynamic EQ (Bell and shelves only). See Engine::runDetector for the gain computer.
        bool  dynOn = false;
        bool  dynExternal = false;        // detect from the sidechain (falls back to internal)
        float threshold = -30.0f;         // dBFS (sine-peak calibrated)
        float range = -6.0f;              // dB, maximum dynamic gain change
        float attackMs = 10.0f, releaseMs = 150.0f;
    };

    // Real-time EQ processor for up to 24 bands x 2 channels.
    //
    // Click-free update strategy:
    //  * Frequency (log domain), gain (dB) and Q (log domain) are ramped linearly over
    //    'smoothingSeconds'. Coefficients are recomputed from the ramped *parameters*
    //    every 'updateInterval' samples - never by interpolating raw coefficients - so every
    //    intermediate filter is a valid, stable SVF.
    //  * The SVF keeps integrator states, so small coefficient steps don't excite transients
    //    the way direct-form biquads can.
    //  * Filter-type and cut-slope changes cross-fade between the old filter (frozen) and the new one.
    //  * Enabling/disabling a band fades its wet contribution in or out.
    //  * Bands that are inactive and fully faded out cost (almost) nothing.
    //
    // No allocation or locking happens in process().
    class Engine
    {
    public:
        static constexpr int maxChannels    = 2;
        static constexpr int updateInterval = 16;     // samples between coefficient updates while smoothing
        static constexpr double smoothingSeconds = 0.02;
        static constexpr double fadeSeconds      = 0.02;

        void prepare (double sampleRate);
        void reset();

        // Call once per block (before process) for each band.
        void setBand (int band, const BandSettings& settings);

        // Jump straight to the current targets (no smoothing). Used by tests.
        void snapToTargets();

        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        // Solo / audition: -1 = off. Call once per block (before process). While a band is
        // soloed the output cross-fades (fadeSeconds) to the *input* filtered by that band's
        // audition filter (see makeSoloFilter). Switching between soloed bands fades out and in.
        void setSolo (int band) noexcept { soloTarget = band; }

        // EQ-44: matched (high-frequency accurate) bells and shelves. Call once per block (before process).
        // Each audible band cross-fades from its old filter to the new one, as for a type change.
        void setMatched (bool on) noexcept { matched = on; }
        bool isMatched() const noexcept { return matched; }
        bool isBandCrossfading (int band) const noexcept { return bands[(size_t) band].crossfading; }   // for the tests

        // Sidechain for dynamic bands set to "external" (call before process; may be null).
        void setSidechain (const float* const* channels, int numChannels) noexcept { sidechain = channels; numSidechain = numChannels; }

        // Live dynamic gain (dB) of a band (audio thread; the processor publishes it to the UI).
        float getDynamicGainDb (int band) const noexcept { return (float) bands[(size_t) band].dyn.gainDb; }
        bool isSoloActive() const noexcept { return solo.band >= 0 || solo.mix.current > 0.0; }

        double getSampleRate() const noexcept { return sampleRate; }

    private:
        struct Ramp
        {
            double current = 0.0, target = 0.0, step = 0.0;
            int remaining = 0;

            void reset (double v) noexcept { current = target = v; step = 0.0; remaining = 0; }
            void setTarget (double t, int length) noexcept;
            bool isRamping() const noexcept { return remaining > 0; }
            void advance (int n) noexcept;
        };

        struct Band
        {
            BandSettings target;
            bool initialised = false;

            FilterType type = FilterType::Bell;
            int slope = defaultSlope;         // effective slope (always the default for non-cut types)
            bool matched = false;             // the Accurate Highs setting the current coefficients were made with (EQ-44)
            bool designMatched = false;       // whether they actually are the matched design (a shelf falls back outside its limits)
            StereoMode stereo = StereoMode::Stereo;   // changes only while the band is silent (fade out, switch, fade in)
            Ramp logFreq, gainDb, logQ;
            bool coeffsDirty = true;

            FilterCoeffs coeffs, prevCoeffs;
            std::array<FilterState, maxChannels> state, prevState;

            Ramp wet;                 // 0 = bypassed, 1 = fully processed
            Ramp xfade;               // 0 = old filter, 1 = new filter
            bool crossfading = false;

            struct Dynamics
            {
                SvfCoeffs detector;
                bool running = false;          // detector ran last chunk (state is fresh)
                std::array<SvfState, maxChannels> detState {};
                std::array<double, maxChannels> power {}, power2 {};   // two smoothing stages (EQ-43)
                std::array<SvfState, maxChannels> scState {};   // detector state for the sidechain input
                std::array<double, maxChannels> scPower {}, scPower2 {};
                double gainDb = 0.0;           // smoothed dynamic gain added to the static gain
                double appliedDb = 0.0;        // value the current coefficients were built with

                void reset() noexcept
                {
                    for (auto& s : detState) s.reset();
                    for (auto& s : scState)  s.reset();
                    power.fill (0.0);
                    power2.fill (0.0);
                    scPower.fill (0.0);
                    scPower2.fill (0.0);
                    gainDb = appliedDb = 0.0;
                }
            } dyn;
        };

        struct Solo
        {
            int band = -1;                    // band whose filter is currently in use
            Ramp mix;                         // 0 = normal output, 1 = soloed
            Ramp logFreq, logQ;
            FilterType type = FilterType::Bell;
            std::array<SvfCoeffs, numSoloSections> coeffs {};
            std::array<std::array<SvfState, numSoloSections>, maxChannels> state {};

            void resetState() noexcept { for (auto& ch : state) for (auto& s : ch) s.reset(); }
        };

        static void encodeMidSide (float* const* channels, int start, int num) noexcept;
        static void decodeMidSide (float* const* channels, int start, int num) noexcept;

        void updateSolo() noexcept;
        void runDetector (Band&, float* const* channels, int firstChannel, int lastChannel, int start, int num) noexcept;
        void processSolo (float* const* channels, const float (*input)[updateInterval], int numChannels, int start, int num) noexcept;

        void snapBand (Band&);
        void beginBlockForBand (Band&);
        void processBand (Band&, float* const* channels, int firstChannel, int lastChannel, int start, int num) noexcept;

        double sampleRate = 48000.0;
        int smoothingSamples = 960;
        int fadeSamples = 960;
        std::array<Band, numBands> bands;
        Solo solo;
        const float* const* sidechain = nullptr;
        int numSidechain = 0;
        int soloTarget = -1;
        bool matched = false;
    };
}
