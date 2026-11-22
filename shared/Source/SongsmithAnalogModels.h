#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

// The Songsmith analogue models, shared by the Songsmith Analog Compressor and the Songsmith Compressor's Analog Warmth
// (songsmithstudio#14), so both run exactly the same code and a fix reaches both. Black, Blue, Anniversary and Lindell.
//
// Two parts, both moved here unchanged from the Analog Compressor (analog-compressor/Source/FETModels.h and FETColour.h, which now
// only forward to this file):
//   1. The model table (Voicing): how each model differs from the original Blackface voicing.
//   2. The colour stages: the FET gain cell's second harmonic (FetCell) and the Class A output stage with its transformer (OutputStage).
// Standard library only, so the engines and the tests can use it without JUCE.
//
// ---- How the models are used in each plugin ----------------------------------------------------------------------------
// Analog Compressor: the voicing moves the FET engine's constants (threshold offset, knee, attack, release) and scales the colour; the
//   FET cell and the output stage colour the sound, as in the hardware order (analog-compressor/docs/models.md).
// Compressor (Analog Warmth): the same voicing scales the compressor's own Threshold, Knee, Attack and Release (so Black leaves them
//   exactly as set, and the others move them the way the Analog Compressor's do), and the same FetCell and OutputStage colour the
//   compressed sound. The Analog Compressor's Input drive does not exist there: the colour depends on what the Analog Compressor's
//   depends on, the level of the signal (0 dBFS = +22 dBm) and how hard the compressor is working (the gain reduction), at the
//   Compressor's own settings. See compressor/docs/engine.md, "Analog Warmth models".
//
namespace Songsmith::AnalogModels
{
    // The `model` parameter is a choice whose INDEX is saved in sessions. Never reorder or reuse an index:
    // append new models at the end. Index 0 is the original Rev D Blackface, so it is also what an older session
    // (which has no `model` value) loads. Presets and the stable text IDs below do not depend on the index.
    enum class Model { blackface = 0, blueStripe = 1, anniversary = 2, lindell = 3 };
    inline constexpr int numModels = 4;
    inline constexpr int modelDefault = 0;   // Rev D · Blackface

    // Stable text IDs: written into preset files. Never change them.
    inline constexpr const char* modelIds[numModels] = { "revD-blackface", "revA-bluestripe", "ae1176-anniversary", "lindell-inspired" };

    inline int modelIndexFromId (const char* id) noexcept
    {
        if (id != nullptr)
            for (int i = 0; i < numModels; ++i)
                if (std::strcmp (id, modelIds[i]) == 0)
                    return i;
        return -1;   // unknown or missing: the caller uses modelDefault (the original Rev D Blackface)
    }

    // How a model differs from the Rev D Blackface reference. Every factor is 1 (and the offset 0) for the original,
    // so the original stays bit-exact.
    struct Voicing
    {
        float thresholdOffsetDb;   // added to every threshold: negative = more gain reduction for the same Input
        float kneeScale;           // >1 = softer, more gradual onset
        float attackScale;         // >1 = slower attack at the same Attack knob position
        float releaseScale;        // >1 = slower release at the same Release knob position
        float colourScale;         // multiplies the FET (second harmonic) colour
        bool  hasGentleRatio;      // offers 2:1 as its own button
        bool  hasSlowAttack;       // offers the Slow Attack switch
    };

    inline constexpr Voicing voicings[numModels] = {
        { 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, false, false },     // Rev D · Blackface: the original Songsmith sound
        { -1.5f, 0.7f, 0.7f, 0.85f, 2.0f, false, false },   // Rev A · Blue Stripe: firmer, faster, grittier
        { 2.0f, 1.3f, 1.0f, 1.15f, 0.6f, true, true },    // 1176AE · Anniversary Edition: gentler, cleaner, 2:1 and Slow Attack
        { -0.5f, 1.5f, 1.6f, 1.3f, 1.0f, true, false },     // Lindell-inspired: soft knee, lets the first hit through, and 2:1 (ACOMP-20)
    };

    // Slow Attack (1176AE only): the attack takes this many times longer. A Songsmith design choice.
    inline constexpr float slowAttackFactor = 4.0f;

    inline const Voicing& voicingFor (int model) noexcept
    {
        return voicings[(model >= 0 && model < numModels) ? model : modelDefault];
    }
}

namespace Songsmith::AnalogModels
{
    // FET gain cell (UG p. 8, Figs. 3-4): the FET is the lower leg of a divider. With no gain
    // reduction it is pinched off and barely conducts, so it adds almost nothing; the more it
    // conducts (deeper GR), the more its drain-source resistance depends on the signal across
    // it, which gives mainly 2nd-harmonic distortion. The LN circuit (UG p. 9) keeps this low.
    struct FetCell
    {
        // Relative 2nd harmonic = k2 * depth * peak / 2, depth = 1 - gain.
        static constexpr float k2 = 0.05f;

        // All Buttons shifts the bias points and distortion "increases radically" (UM p. 9);
        // tuned as eight times the FET term.
        static constexpr float allButtonsFactor = 8.0f;

        // y is the gain-reduced sample, gain the linear gain the engine applied to it, factor
        // 1 normally and allButtonsFactor in All Buttons (smoothed by the caller).
        static float process (float y, float gain, float factor = 1.0f) noexcept
        {
            const float depth = 1.0f - gain;
            return y + factor * k2 * depth * y * y;
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

namespace Songsmith::AnalogModels
{
    // The text shown for the models. The full names are the parameter's choices and the names in preset files and the docs; the short
    // labels are what the buttons and dropdowns show. The descriptions are the tooltips: what each model does in the engine (measured, see
    // analog-compressor/docs/models.md "What is published, what is measured"), as use-case suggestions, not claims about any hardware.
    inline const char* const modelNameTable[4] = { "Blackface", "Blue Stripe", "Anniversary Edition", "Lindell" };
    inline const char* const modelLabelTable[4] = { "Black", "Blue", "Anniversary", "Lindell" };
    inline const char* const modelTooltipTable[4] = {
        "Clean and straightforward. A good place to start for taming vocal, bass or guitar peaks while keeping the sound fairly clean.",
        "Faster and grittier, with more colour. Try it for an upfront vocal, or to make room microphones sound more aggressive.",
        "Gentler and a little cleaner. It adds a 2:1 ratio, and a Slow Attack button that lets more of the first hit through.",
        "Smooth and relaxed, with a slower attack that lets the first hit through. Try it on acoustic guitar to even out strong strums. It also has 2:1." };
}
