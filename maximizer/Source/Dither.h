#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <cstdint>

namespace Max
{
    // Final-stage TPDF dither and quantiser for 16- or 24-bit export (Songsmith design; the legacy
    // L1 manual's IDR section was only a workflow reference: there is no noise shaping here).
    //
    //   y = clamp (round ((x + d) / q) * q),   q = 2^-(bits-1),   d = (u1 - u2) * q, u1, u2 ~ U[0, 1)
    //
    // d is triangular (TPDF) over (-q, q), which makes the quantisation error independent of the
    // signal; with rounding the total noise is white at q^2 / 4 (q^2 / 6 dither + q^2 / 12 rounding).
    // Each channel has its own generator with its own seed, so left and right noise are uncorrelated
    // and a mono source does not turn into a correlated (centre-panned) noise floor.
    //
    // Headroom: |d| < q and rounding adds at most q / 2, so the limiter's ceiling is lowered by
    // headroomLsb = 1.5 LSB while dither is on. Then |x| <= C - 1.5 q gives |y| <= C after
    // quantisation: the exported sample peak never exceeds Out Ceiling. At 16 bits that is
    // 4.6e-5 (0.0004 dB at a -1 dBFS ceiling), at 24 bits 1.8e-7.
    class Dither
    {
    public:
        enum class Depth { off = 0, bits16, bits24 };
        static constexpr int maxChannels = 2;
        static constexpr double headroomLsb = 1.5;

        static int bitsFor (Depth d) noexcept   { return d == Depth::bits16 ? 16 : (d == Depth::bits24 ? 24 : 0); }
        static double lsbFor (Depth d) noexcept { return d == Depth::off ? 0.0 : std::ldexp (1.0, 1 - bitsFor (d)); }
        static float headroomFor (Depth d) noexcept { return (float) (headroomLsb * lsbFor (d)); }

        void setDepth (Depth d) noexcept
        {
            depth = d;
            lsb = lsbFor (d);
        }

        Depth getDepth() const noexcept { return depth; }

        // Fixed seeds, so renders are repeatable; different per channel.
        void reset() noexcept
        {
            for (size_t c = 0; c < state.size(); ++c)
                state[c] = (0x9E3779B97F4A7C15ull * (c + 1)) ^ 0xD1B54A32D192ED03ull;
        }

        void process (float* const* channels, int numChannels, int numSamples) noexcept
        {
            if (depth == Depth::off)
                return;

            const double maxValue = 1.0 - lsb;   // largest code: 32767 / 32768 at 16 bits
            numChannels = juce::jmin (numChannels, maxChannels);

            for (int c = 0; c < numChannels; ++c)
            {
                auto& s = state[(size_t) c];
                for (int i = 0; i < numSamples; ++i)
                {
                    const double d = (uniform (s) - uniform (s)) * lsb;
                    const double q = std::round (((double) channels[c][i] + d) / lsb) * lsb;
                    channels[c][i] = (float) juce::jlimit (-1.0, maxValue, q);
                }
            }
        }

    private:
        Depth depth = Depth::off;
        double lsb = 0.0;
        std::array<std::uint64_t, maxChannels> state {};

        // xorshift64*: fast, allocation-free, good enough for dither noise.
        static double uniform (std::uint64_t& s) noexcept
        {
            s ^= s >> 12;
            s ^= s << 25;
            s ^= s >> 27;
            return (double) ((s * 0x2545F4914F6CDD1Dull) >> 11) * (1.0 / 9007199254740992.0);   // [0, 1)
        }
    };
}
