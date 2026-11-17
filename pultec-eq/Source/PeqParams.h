#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PassiveEngine.h"

// Songsmith Passive EQ: parameter IDs, ranges, defaults and the editor's layout, shared by the processor, the
// editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace Peq
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* lowFrequencyId       = "low_frequency";        // choice: 20, 30, 60, 100 Hz
    inline constexpr const char* lowBoostId           = "low_boost";            // 0 to 13.5 dB
    inline constexpr const char* lowCutId             = "low_cut";              // 0 to 17.5 dB
    inline constexpr const char* highBoostFrequencyId = "high_boost_frequency"; // choice: 3, 4, 5, 8, 10, 12, 16 kHz
    inline constexpr const char* highBoostId          = "high_boost";           // 0 to 18 dB
    inline constexpr const char* highBandwidthId      = "high_bandwidth";       // 0 (broad) to 10 (narrow)
    inline constexpr const char* highCutFrequencyId   = "high_cut_frequency";   // choice: 5, 10, 20 kHz
    inline constexpr const char* highCutId            = "high_cut";             // 0 to 16 dB
    inline constexpr const char* bypassId             = "bypass";               // the host's native bypass

    // --- Defaults: every gain at zero, so a new instance leaves the sound exactly as it was --------------
    constexpr int defaultLowFrequency = 2, defaultHighBoostFrequency = 3, defaultHighCutFrequency = 1;   // 60 Hz, 8 kHz, 10 kHz
    constexpr float defaultBandwidth = 5.0f, maxBandwidth = 10.0f;

    inline juce::String frequencyName (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + " kHz" : juce::String ((int) hz) + " Hz";
    }

    template <typename Array>
    juce::StringArray frequencyNames (const Array& frequencies)
    {
        juce::StringArray names;
        for (auto hz : frequencies)
            names.add (frequencyName (hz));
        return names;
    }

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int designWidth = 1016, contentHeight = 448;   // the controls sit above the footer strip
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 762, maxWidth = 3048;           // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
