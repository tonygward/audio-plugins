#pragma once

#include <cmath>
#include <complex>

// ITU-R BS.1770-4 K-weighting, shared by Songsmith EQ (Auto Gain) and Songsmith Maximiser (the
// loudness meter), so both measure loudness with exactly the same filter.
//
// BS.1770-4 gives coefficients for 48 kHz only. These are the analogue prototype (stage 1: a high
// shelf of +4 dB around 1.68 kHz, the head's acoustic effect; stage 2: the RLB 38 Hz high-pass),
// mapped to any rate by the bilinear transform with the constants libebur128 uses. At 48 kHz they
// reproduce the standard's published coefficients (MaximiserTests "K-weighting: BS.1770-4
// coefficients at 48 kHz").
namespace Songsmith::KWeighting
{
    // One second-order section, transposed direct form II, in double precision.
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.0; }
    };

    inline void coefficients (double fs, Biquad& shelf, Biquad& highPass) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
            const double k = std::tan (pi * f0 / fs);
            const double vh = std::pow (10.0, gain / 20.0), vb = std::pow (vh, 0.4996667741545416);
            const double a0 = 1.0 + k / q + k * k;
            shelf.b0 = (vh + vb * k / q + k * k) / a0;
            shelf.b1 = 2.0 * (k * k - vh) / a0;
            shelf.b2 = (vh - vb * k / q + k * k) / a0;
            shelf.a1 = 2.0 * (k * k - 1.0) / a0;
            shelf.a2 = (1.0 - k / q + k * k) / a0;
        }
        {
            const double f0 = 38.13547087602444, q = 0.5003270373238773;
            const double k = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + k / q + k * k;
            highPass.b0 = 1.0; highPass.b1 = -2.0; highPass.b2 = 1.0;
            highPass.a1 = 2.0 * (k * k - 1.0) / a0;
            highPass.a2 = (1.0 - k / q + k * k) / a0;
        }
    }

    // Magnitude of the two stages together at 'freq', for response tests.
    inline double magnitudeDb (double fs, double freq) noexcept
    {
        Biquad s, h;
        coefficients (fs, s, h);
        const double w = 2.0 * 3.14159265358979323846 * freq / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        auto mag = [&] (const Biquad& b) { return (b.b0 + b.b1 * z1 + b.b2 * z2) / (1.0 + b.a1 * z1 + b.a2 * z2); };
        return 20.0 * std::log10 (std::abs (mag (s) * mag (h)));
    }

    // A K-weighting filter for one channel.
    class Filter
    {
    public:
        void prepare (double sampleRate) noexcept { coefficients (sampleRate, shelf, highPass); reset(); }
        void reset() noexcept                     { shelf.clear(); highPass.clear(); }
        double process (double x) noexcept        { return highPass.process (shelf.process (x)); }

    private:
        Biquad shelf, highPass;
    };
}
