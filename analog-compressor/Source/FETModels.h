#pragma once

#include <cstring>

// ACOMP-10: the selectable compressor models. All of them are one plugin and share every control.
// Standard library only, so the engine and the tests can use it without JUCE.
//
// What is published and what is Songsmith's choice is written out in docs/models.md. In short:
// the Rev D Blackface voicing is the original Songsmith sound and must never change (older sessions rely on it);
// the other voicings are Songsmith design choices that move the same few engine constants in the directions the
// references describe. Each difference is pinned by a test in EngineTests.cpp (measured, not claimed).
namespace FET
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
        { 1.0f, 1.3f, 1.0f, 1.15f, 0.6f, true, true },      // 1176AE · Anniversary Edition: gentler, cleaner, 2:1 and Slow Attack
        { -0.5f, 1.5f, 1.6f, 1.3f, 1.0f, false, false },    // Lindell-inspired: soft knee, lets the first hit through
    };

    // Slow Attack (1176AE only): the attack takes this many times longer. A Songsmith design choice.
    inline constexpr float slowAttackFactor = 4.0f;

    inline const Voicing& voicingFor (int model) noexcept
    {
        return voicings[(model >= 0 && model < numModels) ? model : modelDefault];
    }
}
