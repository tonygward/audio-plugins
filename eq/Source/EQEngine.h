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
    //  * Filter-type changes cross-fade between the old filter (frozen) and the new one.
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
            Ramp logFreq, gainDb, logQ;
            bool coeffsDirty = true;

            SvfCoeffs coeffs, prevCoeffs;
            std::array<SvfState, maxChannels> state, prevState;

            Ramp wet;                 // 0 = bypassed, 1 = fully processed
            Ramp xfade;               // 0 = old filter, 1 = new filter
            bool crossfading = false;
        };

        void snapBand (Band&);
        void beginBlockForBand (Band&);
        void processBand (Band&, float* const* channels, int numChannels, int start, int num) noexcept;

        double sampleRate = 48000.0;
        int smoothingSamples = 960;
        int fadeSamples = 960;
        std::array<Band, numBands> bands;
    };
}
