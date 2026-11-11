#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter identities for Songsmith Analog Compressor.
//
// NEVER rename or remove an ID: hosts store automation and sessions by ID (and by the
// version hint below). New parameters get a new ID and a version hint above 1.
// AnalogCompressorTests pins every ID, range and default.
//
// Control semantics follow the 1176LN front panel (UA 1176LN manual, part 65-00046,
// "Front Panel" pp. 8-9 and "The Technical Stuff" pp. 20-22; see analog-compressor/docs/references.md):
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
        inline constexpr const char* detectorLowCut = "detectorLowCut";   // version hint 3
        inline constexpr const char* ratioTwoToOne = "ratioTwoToOne";     // version hint 4
    }

    inline constexpr int parameterVersion = 1;

    // Detector Low Cut (ACOMP-07): a Songsmith addition to the 1176-inspired design. It filters only what the
    // level detector hears, never the audio. 12 dB/oct, Off by default. See docs/detector-low-cut.md.
    inline constexpr int numLowCutSteps = 6;
    inline const juce::StringArray& lowCutNames()
    {
        static const juce::StringArray names { "Off", "60 Hz", "80 Hz", "100 Hz", "150 Hz", "200 Hz" };
        return names;
    }
    inline constexpr float lowCutHz[numLowCutSteps] = { 0.0f, 60.0f, 80.0f, 100.0f, 150.0f, 200.0f };   // 0 = Off
    inline constexpr int lowCutDefault = 0;

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

    // ACOMP-11: 2:1, a Songsmith addition (the 1176 has no 2:1 button). It is NOT a fifth item of the `ratio`
    // choice: a choice's host-normalised value is index / (count - 1), so growing it from four to five items
    // would move every legacy value (8:1 = 0.333, 12:1 = 0.667, 20:1 = 1.0 would become 8:1 / 20:1 / 2:1 ...).
    // `ratio` therefore stays exactly as it was, and 2:1 is its own switch (version hint 4, Off by default)
    // that takes over from `ratio` while it is on. See docs/two-to-one-ratio.md.
    inline constexpr float gentleRatioValue = 2.0f;

    // Oversampling for the colour stages and the engine (Off, 2x, 4x). See docs/colour.md.
    inline const juce::StringArray& oversamplingNames()
    {
        static const juce::StringArray names { "Off", "2x", "4x" };
        return names;
    }
    inline constexpr int oversamplingDefault = 0;   // Off: no delay, least processing power. See docs/colour.md "Oversampling"
}

namespace FET
{
    // Interface sizes (editor menu). The editor is drawn in design units and scaled as a whole,
    // so every option is equally sharp.
    inline constexpr float uiScales[] = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    inline constexpr int designWidth = 960, contentHeight = 400;   // the controls sit in the top 400

    // The Ignore Below knob (the detector low cut, ACOMP-07/12): right of "Gain Reduction Off", below the
    // Gain Reduction readout and above the hint line (which starts at 356).
    inline constexpr int lowCutX = 760, lowCutY = 302, lowCutWidth = 176, lowCutHeight = 50;
    inline constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
    inline constexpr int designHeight = contentHeight + footerHeight;

    // Drag-resizing (SUI-10): the window keeps the design proportions. Smallest = the design size, so
    // text stays at least 12 px (11 px grid labels); largest = 200 %.
    inline constexpr int minEditorWidth = designWidth, maxEditorWidth = designWidth * 2;

    inline int constrainEditorWidth (int width) noexcept
    {
        return width < minEditorWidth ? minEditorWidth : width > maxEditorWidth ? maxEditorWidth : width;
    }
    inline int editorHeightFor (int width) noexcept
    {
        return (int) ((double) width * designHeight / designWidth + 0.5);
    }

    inline float snapUiScale (float s)
    {
        if (! std::isfinite (s))
            return 1.0f;
        float best = 1.0f;
        for (auto option : uiScales)
            if (std::abs (option - s) < std::abs (best - s))
                best = option;
        return best;
    }
}
