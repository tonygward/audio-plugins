#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace EQ
{
    // ITU-R BS.1770 K-weighting (pre-filter high shelf + RLB high-pass), designed for any
    // sample rate with the bilinear-transform formulas used by libebur128.
    class KWeighting
    {
    public:
        void prepare (double sampleRate)
        {
            constexpr double kPi = 3.14159265358979323846;

            {   // Stage 1: +4 dB high shelf around 1.7 kHz (head acoustics)
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan (kPi * f0 / sampleRate);
                const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                s1 = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                       2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            {   // Stage 2: RLB high-pass around 38 Hz
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan (kPi * f0 / sampleRate);
                const double a0 = 1.0 + K / Q + K * K;
                s2 = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
            }
            reset();
        }

        void reset() noexcept { z1 = {}; z2 = {}; }

        double process (double x) noexcept
        {
            return run (s2, z2, run (s1, z1, x));
        }

    private:
        struct Biquad { double b0, b1, b2, a1, a2; };

        static double run (const Biquad& c, std::array<double, 2>& z, double x) noexcept
        {
            const double y = c.b0 * x + z[0];
            z[0] = c.b1 * x - c.a1 * y + z[1];
            z[1] = c.b2 * x - c.a2 * y;
            return y;
        }

        Biquad s1 {}, s2 {};
        std::array<double, 2> z1 {}, z2 {};
    };

    // Loudness-matching correction for the EQ (auto gain).
    //
    // Compares K-weighted mean-square energy (BS.1770, without gating) of the EQ's input and
    // output over a 2 s sliding window of 100 ms segments, and returns the gain that makes
    // the output as loud as the input, clamped to +-12 dB. While the input is quieter than
    // -60 dB (K-weighted, ~LUFS) the correction is frozen, so silence and fades never pump.
    //
    // Fixed-size state only: nothing is allocated after construction.
    class AutoGain
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int numSegments = 20;              // x 100 ms = 2 s window
        static constexpr double segmentSeconds = 0.1;
        static constexpr double maxCorrectionDb = 12.0;
        static constexpr double freezeBelowDb = -60.0;

        void prepare (double sampleRate)
        {
            segmentLength = std::max (1, (int) std::lround (segmentSeconds * sampleRate));
            for (auto& k : kIn)  k.prepare (sampleRate);
            for (auto& k : kOut) k.prepare (sampleRate);
            reset();
        }

        // Starts a fresh measurement (e.g. when auto gain is switched on). Keeps the last
        // correction so a re-enable doesn't jump.
        void reset() noexcept
        {
            for (auto& k : kIn)  k.reset();
            for (auto& k : kOut) k.reset();
            segIn.fill (0.0);
            segOut.fill (0.0);
            accIn = accOut = 0.0;
            accSamples = 0;
            filled = 0;
            writePos = 0;
        }

        void measureInput (const float* const* ch, int numChannels, int num) noexcept
        {
            accIn += energy (kIn, ch, numChannels, num);
        }

        void measureOutput (const float* const* ch, int numChannels, int num) noexcept
        {
            accOut += energy (kOut, ch, numChannels, num);
        }

        // Call once per measured chunk (after both measure calls). Returns true when a new
        // segment completed and the correction may have changed.
        bool advance (int num) noexcept
        {
            accSamples += num;
            if (accSamples < segmentLength)
                return false;

            segIn[(size_t) writePos] = accIn;
            segOut[(size_t) writePos] = accOut;
            writePos = (writePos + 1) % numSegments;
            filled = std::min (filled + 1, numSegments);
            accIn = accOut = 0.0;
            const int segSamples = accSamples;
            accSamples = 0;

            double sumIn = 0.0, sumOut = 0.0;
            for (int i = 0; i < filled; ++i)
            {
                sumIn += segIn[(size_t) i];
                sumOut += segOut[(size_t) i];
            }

            const double inputDb = -0.691 + 10.0 * std::log10 (std::max (sumIn / ((double) filled * segSamples), 1.0e-30));
            if (inputDb < freezeBelowDb)
                return false;   // too quiet to judge: keep the current correction

            correctionDb = std::clamp (10.0 * std::log10 (std::max (sumIn, 1.0e-30) / std::max (sumOut, 1.0e-30)),
                                       -maxCorrectionDb, maxCorrectionDb);
            return true;
        }

        double getCorrectionDb() const noexcept { return correctionDb; }
        void setCorrectionDb (double db) noexcept { correctionDb = db; }

    private:
        static double energy (std::array<KWeighting, maxChannels>& k, const float* const* ch, int numChannels, int num) noexcept
        {
            double e = 0.0;
            for (int c = 0; c < std::min (numChannels, maxChannels); ++c)
                for (int i = 0; i < num; ++i)
                {
                    const double y = k[(size_t) c].process ((double) ch[c][i]);
                    e += y * y;
                }
            return e;   // BS.1770: channel energies are summed (L/R weights 1.0)
        }

        std::array<KWeighting, maxChannels> kIn, kOut;
        std::array<double, numSegments> segIn {}, segOut {};
        double accIn = 0.0, accOut = 0.0;
        int accSamples = 0, filled = 0, writePos = 0;
        int segmentLength = 4800;
        double correctionDb = 0.0;
    };
}
