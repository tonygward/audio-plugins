#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// Analogue colour stages, kept apart from the clean FET::Engine so tests can run either.
// Every constant here is a modelling choice calibrated to the published figures in
// fet/docs/colour.md (UG p. 1: < 0.5 % THD at +22 dBm, ±1 dB 20 Hz-20 kHz).
//
// Digital reference: 0 VU = +4 dBm = -18 dBFS, so +22 dBm = 0 dBFS.
namespace FET
{
    // FET gain cell (UG p. 8, Figs. 3-4): the FET is the lower leg of a divider. With no gain
    // reduction it is pinched off and barely conducts, so it adds almost nothing; the more it
    // conducts (deeper GR), the more its drain-source resistance depends on the signal across
    // it, which gives mainly 2nd-harmonic distortion. The LN circuit (UG p. 9) keeps this low.
    struct FetCell
    {
        // Relative 2nd harmonic = k2 * depth * peak / 2, depth = 1 - gain.
        static constexpr float k2 = 0.05f;

        // y is the gain-reduced sample, gain the linear gain the engine applied to it.
        static float process (float y, float gain) noexcept
        {
            const float depth = 1.0f - gain;
            return y + k2 * depth * y * y;
        }
    };

    // Class A output amplifier (Darlington + 2N3053, UG p. 9) and output transformer.
    // - Single-ended Class A: asymmetric soft saturation (even and odd harmonics), rising with
    //   level; calibrated to ~0.5 % THD at 0 dBFS (= +22 dBm) at 1 kHz.
    // - Transformer core: low-frequency saturation driven by the core flux (the integral of the
    //   voltage), so it rises at low frequencies and high levels.
    // - Coupling: first-order high-pass at 8 Hz (-0.7 dB at 20 Hz, inside ±1 dB) that also
    //   removes the DC the even-order terms create.
    class OutputStage
    {
    public:
        static constexpr float clipLevel = 2.9f;     // soft ceiling, about +9 dBFS peak (+6 dB above 0 dBFS RMS)
        static constexpr float asym = 0.005f;        // Class A asymmetry: 2nd harmonic 0.25 % at 0 dBFS
        static constexpr float coreGain = 0.05f;     // flux^3 coefficient
        static constexpr double fluxCornerHz = 20.0;
        static constexpr double couplingHz = 8.0;
        static constexpr int maxChannels = 2;

        void prepare (double sampleRate) noexcept
        {
            fs = sampleRate;
            fluxCoeff = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * fluxCornerHz / fs));
            hpCoeff = (float) std::exp (-2.0 * 3.141592653589793 * couplingHz / fs);
            reset();
        }

        void reset() noexcept
        {
            flux.fill (0.0f);
            hpX.fill (0.0f);
            hpY.fill (0.0f);
        }

        // Memoryless amplifier curve with unity small-signal gain and f(0) = 0: a small
        // second-order term (single-ended Class A is asymmetric), then a fourth-order soft
        // ceiling u / (1 + (u/c)^4)^(1/4), which stays clean at normal levels and closes in
        // quickly above 0 dBFS, like an amplifier running out of headroom.
        static float amp (float x) noexcept
        {
            const float u = x + asym * x * x;
            const float r = u / clipLevel;
            const float r2 = r * r;
            return u / std::sqrt (std::sqrt (1.0f + r2 * r2));
        }

        float processSample (int ch, float x) noexcept
        {
            auto& f = flux[(size_t) ch];
            f += fluxCoeff * (x - f);                  // flux-like low-passed signal
            float y = amp (x) - coreGain * f * f * f;  // amplifier, then core saturation

            // Coupling high-pass (one-pole, one-zero).
            const float out = hpCoeff * (hpY[(size_t) ch] + y - hpX[(size_t) ch]);
            hpX[(size_t) ch] = y;
            hpY[(size_t) ch] = out;
            return out;
        }

    private:
        double fs = 44100.0;
        float fluxCoeff = 0.0f, hpCoeff = 1.0f;
        std::array<float, maxChannels> flux {}, hpX {}, hpY {};
    };
}
