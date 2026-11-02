#pragma once

#include <algorithm>
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
}
