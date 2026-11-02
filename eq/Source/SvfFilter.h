#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include "EQParams.h"

// Linear trapezoidal state-variable filter (Andrew Simper / Cytomic, "SvfLinearTrapOptimised2").
//
// It is an exact bilinear transform of the analogue prototype
//     H(s) = m0 + (m1 * s + m2) / (s^2 + k s + 1)
// so its magnitude response is identical to the RBJ cookbook filters, and it can be
// evaluated analytically from the *same* coefficients the DSP uses (see getMagnitudeDb).
//
// Unlike direct-form biquads, its state variables are integrator states, so it stays
// well-behaved when coefficients change while audio is running (parameter smoothing).
namespace EQ
{
    constexpr double pi = 3.14159265358979323846;

    struct SvfCoeffs
    {
        double g = 0.0, k = 1.0;
        double a1 = 1.0, a2 = 0.0, a3 = 0.0;
        double m0 = 1.0, m1 = 0.0, m2 = 0.0;
    };

    struct SvfState
    {
        double ic1 = 0.0, ic2 = 0.0;
        void reset() noexcept { ic1 = ic2 = 0.0; }
        bool isFinite() const noexcept { return std::isfinite (ic1) && std::isfinite (ic2); }
    };

    inline double clampFreqForSampleRate (double freq, double sampleRate)
    {
        return std::clamp (freq, 10.0, sampleRate * 0.48);
    }

    inline SvfCoeffs makeCoeffs (FilterType type, double sampleRate, double freq, double q, double gainDb)
    {
        freq   = clampFreqForSampleRate (freq, sampleRate);
        q      = std::clamp (q, 0.025, 100.0);
        gainDb = std::clamp (gainDb, -48.0, 48.0);

        const double w = std::tan (pi * freq / sampleRate);
        const double A = std::pow (10.0, gainDb / 40.0);

        SvfCoeffs c;
        c.g = w;
        c.k = 1.0 / q;

        switch (type)
        {
            case FilterType::Bell:
                c.k  = 1.0 / (q * A);
                c.m0 = 1.0;  c.m1 = c.k * (A * A - 1.0);  c.m2 = 0.0;
                break;
            case FilterType::LowShelf:
                c.g  = w / std::sqrt (A);
                c.m0 = 1.0;  c.m1 = c.k * (A - 1.0);  c.m2 = A * A - 1.0;
                break;
            case FilterType::HighShelf:
                c.g  = w * std::sqrt (A);
                c.m0 = A * A;  c.m1 = c.k * (1.0 - A) * A;  c.m2 = 1.0 - A * A;
                break;
            case FilterType::LowCut:
                c.m0 = 1.0;  c.m1 = -c.k;  c.m2 = -1.0;
                break;
            case FilterType::HighCut:
                c.m0 = 0.0;  c.m1 = 0.0;  c.m2 = 1.0;
                break;
            case FilterType::Notch:
                c.m0 = 1.0;  c.m1 = -c.k;  c.m2 = 0.0;
                break;
        }

        c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
        c.a2 = c.g * c.a1;
        c.a3 = c.g * c.a2;
        return c;
    }

    inline double processSample (const SvfCoeffs& c, SvfState& s, double v0) noexcept
    {
        const double v3 = v0 - s.ic2;
        const double v1 = c.a1 * s.ic1 + c.a2 * v3;
        const double v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }

    // Exact magnitude response of the digital filter at 'freq'.
    // Bilinear transform: on the unit circle, s = j * tan(pi f / fs) / g.
    inline double getMagnitude (const SvfCoeffs& c, double freq, double sampleRate)
    {
        const double t = std::tan (pi * std::min (freq, sampleRate * 0.4999) / sampleRate) / c.g;
        const std::complex<double> s (0.0, t);
        const auto h = c.m0 + (c.m1 * s + c.m2) / (s * s + c.k * s + 1.0);
        return std::abs (h);
    }

    inline double getMagnitudeDb (const SvfCoeffs& c, double freq, double sampleRate)
    {
        return 20.0 * std::log10 (std::max (getMagnitude (c, freq, sampleRate), 1.0e-12));
    }

    //==========================================================================
    // A band's complete filter: one SVF for every type, or for Low Cut / High Cut a
    // Butterworth cascade of an optional one-pole TPT stage (odd 6 dB part) followed by
    // up to four SVF sections (48 dB/oct = order 8).
    //
    // Q on steep slopes: every section gets its Butterworth Q, and the user's Q scales
    // only the last (highest-Q) section by Q / 0.707. So the default Q gives a maximally
    // flat Butterworth response at every slope, and raising Q adds a resonant peak at
    // the cutoff just as it does at 12 dB/oct. At 12 dB/oct this is exactly the old
    // single-stage behaviour (the user's Q is used as is). The one-pole stage of the
    // 6 dB/oct slope has no resonance, so Q does nothing there.
    constexpr int maxSvfSections = 4;

    struct OnePoleCoeffs
    {
        double G = 0.0;          // g / (1 + g), g = tan(pi f / fs)
        bool highpass = true;
    };

    struct OnePoleState
    {
        double s = 0.0;
    };

    struct FilterCoeffs
    {
        std::array<SvfCoeffs, maxSvfSections> svf {};
        int numSvf = 1;
        bool hasOnePole = false;
        OnePoleCoeffs onePole;

        bool isSingleSvf() const noexcept { return numSvf == 1 && ! hasOnePole; }
    };

    struct FilterState
    {
        std::array<SvfState, maxSvfSections> svf {};
        OnePoleState onePole;

        void reset() noexcept
        {
            for (auto& s : svf) s.reset();
            onePole.s = 0.0;
        }

        // Checks only the stages 'c' uses (unused stages stay at zero).
        bool isFinite (const FilterCoeffs& c) const noexcept;
    };

    inline bool FilterState::isFinite (const FilterCoeffs& c) const noexcept
    {
        // Sum first: a single isfinite per stage set (NaN/inf propagate through the sum).
        double sum = c.hasOnePole ? onePole.s : 0.0;
        for (int i = 0; i < c.numSvf; ++i)
            sum += svf[(size_t) i].ic1 + svf[(size_t) i].ic2;
        return std::isfinite (sum);
    }

    // Butterworth section Q values for an order-N cascade (sections sorted by
    // increasing Q; the one-pole stage, if N is odd, is not included).
    inline int butterworthQs (int order, double* qs)
    {
        const int sections = order / 2;
        for (int k = 0; k < sections; ++k)
        {
            const double theta = (order % 2 == 0) ? pi * (2.0 * k + 1.0) / (2.0 * order)
                                                   : pi * (k + 1.0) / order;
            qs[k] = 1.0 / (2.0 * std::cos (theta));   // theta grows with k, so Q increases
        }
        return sections;
    }

    inline FilterCoeffs makeFilter (FilterType type, double sampleRate, double freq, double q, double gainDb, int slopeIndex)
    {
        FilterCoeffs f;
        const int order = typeHasSlope (type) ? slopeOrder (slopeIndex) : 2;

        if (order == 2)
        {
            f.svf[0] = makeCoeffs (type, sampleRate, freq, q, gainDb);   // unchanged single-stage path
            return f;
        }

        double qs[maxSvfSections] {};
        f.numSvf = butterworthQs (order, qs);

        const double userQ = std::clamp (q, 0.025, 100.0);
        constexpr double butterworthQ2 = 0.70710678118654752;
        for (int i = 0; i < f.numSvf; ++i)
        {
            const double sectionQ = (i == f.numSvf - 1) ? qs[i] * userQ / butterworthQ2 : qs[i];
            f.svf[(size_t) i] = makeCoeffs (type, sampleRate, freq, sectionQ, 0.0);
        }

        f.hasOnePole = (order % 2) != 0;
        if (f.hasOnePole)
        {
            const double g = std::tan (pi * clampFreqForSampleRate (freq, sampleRate) / sampleRate);
            f.onePole.G = g / (1.0 + g);
            f.onePole.highpass = (type == FilterType::LowCut);
        }
        return f;
    }

    inline double processOnePole (const OnePoleCoeffs& c, OnePoleState& s, double x) noexcept
    {
        const double v = (x - s.s) * c.G;
        const double lp = v + s.s;
        s.s = lp + v;
        return c.highpass ? x - lp : lp;
    }

    inline double processFilter (const FilterCoeffs& c, FilterState& s, double x) noexcept
    {
        if (c.hasOnePole)
            x = processOnePole (c.onePole, s.onePole, x);
        for (int i = 0; i < c.numSvf; ++i)
            x = processSample (c.svf[(size_t) i], s.svf[(size_t) i], x);
        return x;
    }

    inline double getMagnitude (const FilterCoeffs& c, double freq, double sampleRate)
    {
        double m = 1.0;
        for (int i = 0; i < c.numSvf; ++i)
            m *= getMagnitude (c.svf[(size_t) i], freq, sampleRate);

        if (c.hasOnePole)
        {
            // Bilinear transform of 1/(s+1) or s/(s+1), with s = j tan(pi f / fs) / g.
            const double g = c.onePole.G / (1.0 - c.onePole.G);
            const double t = std::tan (pi * std::min (freq, sampleRate * 0.4999) / sampleRate) / g;
            m *= c.onePole.highpass ? t / std::sqrt (1.0 + t * t) : 1.0 / std::sqrt (1.0 + t * t);
        }
        return m;
    }

    inline double getMagnitudeDb (const FilterCoeffs& c, double freq, double sampleRate)
    {
        return 20.0 * std::log10 (std::max (getMagnitude (c, freq, sampleRate), 1.0e-12));
    }
}
