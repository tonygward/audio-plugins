#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include "SongsmithKWeighting.h"

namespace EQ
{
    // ITU-R BS.1770 K-weighting, shared with Songsmith Maximiser's loudness meter (same coefficients).
    using KWeighting = Songsmith::KWeighting::Filter;

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
