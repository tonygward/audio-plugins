#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter identities for Songsmith Analog Compressor.
//
// NEVER rename or remove an ID: hosts store automation and sessions by ID (and by the
// version hint below). New parameters get a new ID and a version hint above 1.
// FETTests pins every ID, range and default.
//
// Control semantics follow the 1176LN front panel (UA 1176LN manual, part 65-00046,
// "Front Panel" pp. 8-9 and "The Technical Stuff" pp. 20-22; see fet/docs/references.md):
//   - Input sets the drive into a FIXED threshold, so it sets the amount of compression.
//   - Output is the make-up gain after gain reduction; it does not change compression.
//   - Attack and Release are marked 1-7 and get FASTER clockwise (7 = fastest).
//   - Attack fully counter-clockwise ("OFF") disables gain reduction but the signal still
//     passes through the amplifiers. Here that is its own switch, grOff, so the knob keeps its
//     position and the switch is automatable on its own.
//   - Four ratio buttons: 4, 8, 12, 20. Pressing all four is a separate mode ("All Buttons").
namespace FET
{
    namespace ID
    {
        inline constexpr const char* input      = "input";
        inline constexpr const char* output     = "output";
        inline constexpr const char* attack     = "attack";
        inline constexpr const char* release    = "release";
        inline constexpr const char* ratio      = "ratio";
        inline constexpr const char* allButtons = "allButtons";
        inline constexpr const char* grOff      = "grOff";
        inline constexpr const char* bypass     = "bypass";
        inline constexpr const char* oversampling = "oversampling";   // version hint 2
    }

    inline constexpr int parameterVersion = 1;

    // Input: drive in dB. Modelling choice: the hardware Input is a passive attenuator ahead of
    // a fixed threshold; a plugin needs gain above unity too, so the knob spans -24..+48 dB.
    inline constexpr float inputMinDb = -24.0f, inputMaxDb = 48.0f, inputDefaultDb = 0.0f;
    // Output: make-up trim in dB (modelling choice; the hardware knob is an attenuator into the
    // output amplifier).
    inline constexpr float outputMinDb = -36.0f, outputMaxDb = 24.0f, outputDefaultDb = 0.0f;
    // Knob positions as printed on the hardware dials.
    inline constexpr float knobMin = 1.0f, knobMax = 7.0f;
    inline constexpr float attackDefault = 4.0f, releaseDefault = 4.0f;

    inline constexpr int numRatios = 4;
    inline const juce::StringArray& ratioNames()
    {
        static const juce::StringArray names { "4:1", "8:1", "12:1", "20:1" };
        return names;
    }
    inline constexpr float ratioValues[numRatios] = { 4.0f, 8.0f, 12.0f, 20.0f };
    inline constexpr int ratioDefault = 0;

    // Oversampling for the colour stages and the engine (Off, 2x, 4x). See docs/colour.md.
    inline const juce::StringArray& oversamplingNames()
    {
        static const juce::StringArray names { "Off", "2x", "4x" };
        return names;
    }
    inline constexpr int oversamplingDefault = 2;   // 4x: see docs/colour.md "Oversampling"
}
