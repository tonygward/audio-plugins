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

    // The SVF for prototype frequency w (= tan (pi f / fs) for the bilinear transform).
    inline SvfCoeffs makeCoeffsForW (FilterType type, double w, double q, double gainDb)
    {
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

    inline bool makeMatchedCoeffs (FilterType type, double sampleRate, double freq, double q, double gainDb, SvfCoeffs& out);

    // 'matched' (EQ-44, "High Frequency Accuracy"): bells and shelves use a design matched to the analog
    // prototype up to Nyquist instead of the bilinear transform, which squeezes them near the top of
    // the range. Other types, and any case the matched design cannot handle, use the bilinear transform.
    inline SvfCoeffs makeCoeffs (FilterType type, double sampleRate, double freq, double q, double gainDb, bool matched = false)
    {
        freq   = clampFreqForSampleRate (freq, sampleRate);
        q      = std::clamp (q, 0.025, 100.0);
        gainDb = std::clamp (gainDb, -48.0, 48.0);

        if (matched)
        {
            SvfCoeffs m;
            if (makeMatchedCoeffs (type, sampleRate, freq, q, gainDb, m))
                return m;
        }
        return makeCoeffsForW (type, std::tan (pi * freq / sampleRate), q, gainDb);
    }

    // Band-pass normalised to unity gain at 'freq': H(s) = k s / (s^2 + k s + 1).
    inline SvfCoeffs makeBandPass (double sampleRate, double freq, double q)
    {
        auto c = makeCoeffs (FilterType::HighCut, sampleRate, freq, q, 0.0);   // same g, k, a1..a3
        c.m0 = 0.0;  c.m1 = c.k;  c.m2 = 0.0;
        return c;
    }

    // Solo / audition filter for a band: two SVF sections (24 dB/oct skirts).
    //  * Bell, Notch .......... band-pass at the band frequency, each section's Q = the band's
    //                           Q clamped to [0.75, 10]. Even the widest setting is >= 18 dB
    //                           down two octaves away, so a wide bell doesn't just play everything.
    //  * Low Shelf, Low Cut ... 4th-order Butterworth low-pass at the corner: the region the
    //                           shelf changes / the content the cut removes.
    //  * High Shelf, High Cut . 4th-order Butterworth high-pass at the corner.
    constexpr int numSoloSections = 2;

    inline void makeSoloFilter (FilterType type, double sampleRate, double freq, double q, SvfCoeffs* out)
    {
        constexpr double bwQ[] = { 0.54119610014619698, 1.3065629648763766 };

        switch (type)
        {
            case FilterType::LowShelf:
            case FilterType::LowCut:
                for (int i = 0; i < numSoloSections; ++i)
                    out[i] = makeCoeffs (FilterType::HighCut, sampleRate, freq, bwQ[i], 0.0);
                break;
            case FilterType::HighShelf:
            case FilterType::HighCut:
                for (int i = 0; i < numSoloSections; ++i)
                    out[i] = makeCoeffs (FilterType::LowCut, sampleRate, freq, bwQ[i], 0.0);
                break;
            case FilterType::Bell:
            case FilterType::Notch:
            default:
                for (int i = 0; i < numSoloSections; ++i)
                    out[i] = makeBandPass (sampleRate, freq, std::clamp (q, 0.75, 10.0));
                break;
        }
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

    inline FilterCoeffs makeFilter (FilterType type, double sampleRate, double freq, double q, double gainDb, int slopeIndex, bool matched = false)
    {
        FilterCoeffs f;
        const int order = typeHasSlope (type) ? slopeOrder (slopeIndex) : 2;

        if (order == 2)
        {
            f.svf[0] = makeCoeffs (type, sampleRate, freq, q, gainDb, matched);   // single-stage path
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

    //==========================================================================
    // EQ-44: matched second-order designs (Vicanek, "Matched Second Order Digital Filters", 2016).
    //
    // Poles: impulse invariance of the analog prototype's poles (eq. 12), so a high band is not squeezed
    // towards Nyquist. Numerator: matched in |H|^2 = (B0 phi0 + B1 phi1 + B2 phi2) / (A0 phi0 + A1 phi1 + A2 phi2)
    // (eqs. 25-27), then the minimum-phase b0, b1, b2 from B0, B1, B2 (eq. 29).
    //  * Bell: Vicanek's matched peaking EQ (eqs. 42-45): unity at DC, the prototype's gain G at the
    //    centre, and a maximum or minimum there.
    //  * Shelves: the paper has none, so the same scheme matches three points (a Songsmith choice):
    //    DC, the corner frequency (half the shelf's gain in dB) and Nyquist, all to the analog prototype.
    // The digital biquad is then written as the SVF whose bilinear form it equals (svfFromBiquad), so
    // processing, smoothing and the drawn curve (getMagnitude) work exactly as for every other band.

    // Magnitude of the unwarped analog prototype of makeCoeffs at 'freq'.
    inline double analogMagnitude (FilterType type, double sampleRate, double f0, double q, double gainDb, double freq)
    {
        const auto c = makeCoeffsForW (type, pi * f0 / sampleRate, q, gainDb);   // w = pi f0 / fs: no prewarping
        const std::complex<double> s (0.0, (pi * freq / sampleRate) / c.g);
        return std::abs (c.m0 + (c.m1 * s + c.m2) / (s * s + c.k * s + 1.0));
    }

    // The SVF equal to the digital biquad (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2), by inverting
    // the bilinear map of H(s) = m0 + (m1 s + m2) / (s^2 + k s + 1). False if the poles are not stable.
    inline bool svfFromBiquad (double b0, double b1, double b2, double a1, double a2, SvfCoeffs& c)
    {
        const double p = 1.0 + a1 + a2, n = 1.0 - a1 + a2;
        if (! (p > 0.0 && n > 0.0))
            return false;

        const double g2 = p / n, g = std::sqrt (g2);
        const double d = 4.0 / n;                     // 1 + g k + g^2
        const double k = (d - 1.0 - g2) / g;
        if (! (k > 0.0) || ! std::isfinite (k))
            return false;

        // Numerator of the bilinear form: n0, n1, n2 = b * d, with
        //   n0 = N2 + N1 g + N0 g^2,  n1 = 2 (N0 g^2 - N2),  n2 = N2 - N1 g + N0 g^2,
        // where H(s) has numerator N2 s^2 + N1 s + N0.
        const double n0 = b0 * d, n1 = b1 * d, n2 = b2 * d;
        const double N2 = 0.25 * (n0 - n1 + n2), N1 = (n0 - n2) / (2.0 * g), N0 = (n0 + n1 + n2) / (4.0 * g2);

        c.g = g;  c.k = k;
        c.m0 = N2;  c.m1 = N1 - N2 * k;  c.m2 = N0 - N2;
        c.a1 = 1.0 / (1.0 + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
        return std::isfinite (c.m0) && std::isfinite (c.m1) && std::isfinite (c.m2);
    }

    inline bool makeMatchedCoeffs (FilterType type, double sampleRate, double freq, double q, double gainDb, SvfCoeffs& out)
    {
        if (gainDb == 0.0 || ! (type == FilterType::Bell || type == FilterType::LowShelf || type == FilterType::HighShelf))
            return false;

        // The prototype's denominator s'^2 + k s' + 1 in s' = s / wp: poles at wp (radians per sample) with
        // damping k / 2 (Vicanek's q).
        const auto proto = makeCoeffsForW (type, pi * freq / sampleRate, q, gainDb);
        const double wp = 2.0 * proto.g, z = 0.5 * proto.k;

        // A shelf's poles sit at f0 / sqrt (A) or f0 x sqrt (A). Once they reach about 0.45 fs, impulse
        // invariance folds them and the matched shelf is no better than the bilinear one (measured:
        // a 16 kHz, 12 dB shelf at 48 kHz has its poles at 0.47 fs and matches worse), so keep the
        // bilinear design there. Bells were better in every case measured. A cheap rule instead of
        // comparing both designs each time: coefficients are rebuilt every 16 samples while gains move.
        if (type != FilterType::Bell && wp > 0.9 * pi)
            return false;
        double a1;
        if (z <= 1.0) a1 = -2.0 * std::exp (-z * wp) * std::cos (std::sqrt (1.0 - z * z) * wp);
        else          a1 = -2.0 * std::exp (-z * wp) * std::cosh (std::sqrt (z * z - 1.0) * wp);
        const double a2 = std::exp (-2.0 * z * wp);

        const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2), A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2), A2 = -4.0 * a2;
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double phi1 = std::sin (0.5 * w0) * std::sin (0.5 * w0), phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
        const double den0 = A0 * phi0 + A1 * phi1 + A2 * phi2;   // |denominator|^2 at w0

        double B0, B1, B2;
        if (type == FilterType::Bell)
        {
            const double G2 = std::pow (10.0, gainDb / 10.0);    // |H(w0)|^2
            B0 = A0;
            const double R1 = den0 * G2;
            const double R2 = (-A0 + A1 + 4.0 * (phi0 - phi1) * A2) * G2;
            B2 = (R1 - R2 * phi1 - B0) / (4.0 * phi1 * phi1);
            B1 = R2 + B0 + 4.0 * (phi1 - phi0) * B2;
        }
        else
        {
            auto mag2 = [&] (double fr) { const double m = analogMagnitude (type, sampleRate, freq, q, gainDb, fr); return m * m; };
            B0 = A0 * mag2 (0.0);
            B1 = A1 * mag2 (0.5 * sampleRate);
            B2 = (den0 * mag2 (freq) - B0 * phi0 - B1 * phi1) / phi2;
        }

        if (! (B0 >= 0.0 && B1 >= 0.0) || ! std::isfinite (B2))
            return false;
        const double sB0 = std::sqrt (B0), sB1 = std::sqrt (B1);
        const double W = 0.5 * (sB0 + sB1), disc = W * W + B2;
        if (! (disc >= 0.0))
            return false;
        const double b0 = 0.5 * (W + std::sqrt (disc));
        if (! (b0 > 0.0))
            return false;
        const double b1 = 0.5 * (sB0 - sB1), b2 = -B2 / (4.0 * b0);

        SvfCoeffs c;
        if (! svfFromBiquad (b0, b1, b2, a1, a2, c))
            return false;
        out = c;
        return true;
    }

}
