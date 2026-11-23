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

    // --- Added in PEQ-04 (ParameterID version 2). Sessions and presets saved before it have none of these, so they
    // load with the mode at Tone Shaper (index 0) and every new amount at zero: the sound they always had.
    inline constexpr const char* outputTrimId         = "output_trim";          // dB, 0 by default (version 4; songsmithstudio#16)
    inline constexpr const char* autoGainId           = "auto_gain";            // on or off, off by default (version 4)
    inline constexpr float minOutputTrimDb = -24.0f, maxOutputTrimDb = 24.0f;
    inline constexpr const char* modeId               = "eq_mode";              // choice: Tone Shaper, Mid Shaper, Frequency Filter
    inline constexpr const char* midLowFrequencyId    = "mid_low_frequency";    // choice: 200, 300, 500, 700 Hz, 1 kHz
    inline constexpr const char* midLowBoostId        = "mid_low_boost";        // 0 to 10 dB
    inline constexpr const char* midDipFrequencyId    = "mid_dip_frequency";    // choice: ten steps, 200 Hz to 7 kHz
    inline constexpr const char* midDipId             = "mid_dip";              // 0 to 10 dB
    inline constexpr const char* midHighFrequencyId   = "mid_high_frequency";   // choice: 1.5, 2, 3, 4.5, 6 kHz
    inline constexpr const char* midHighBoostId       = "mid_high_boost";       // 0 to 10 dB
    inline constexpr const char* highPassFrequencyId  = "high_pass_frequency";  // choice: Off, 30, 50, 80, 120, 150, 200, 500 Hz
    inline constexpr const char* lowPassFrequencyId   = "low_pass_frequency";   // choice: Off, 3, 5, 7, 10, 15 kHz

    // The mode names, as shown (Songsmith names; the classic units they are inspired by are only in the help text).
    // PEQ-05 put new steps in the middle of the filter switches (150 Hz and 500 Hz high-pass, 3 kHz low-pass). A session or
    // a preset file saved before that stores the old position, so it is moved to where the same frequency now sits.
    // Old high-pass: Off, 30, 50, 80, 120, 200.  Old low-pass: Off, 5, 7, 10, 15 kHz.
    inline int migratedHighPassIndex (int oldIndex) noexcept { return oldIndex >= 5 ? oldIndex + 1 : oldIndex; }
    inline int migratedLowPassIndex (int oldIndex) noexcept  { return oldIndex >= 1 ? oldIndex + 1 : oldIndex; }

    inline const juce::StringArray& modeNames()
    {
        static const juce::StringArray names { "Tone Shaper", "Mid Shaper", "Frequency Filter" };
        return names;
    }
    constexpr int defaultMidLowFrequency = 2, defaultMidDipFrequency = 4, defaultMidHighFrequency = 2;   // 500 Hz, 1 kHz, 3 kHz

    // --- Defaults: every gain at zero, so a new instance leaves the sound exactly as it was --------------
    constexpr int defaultLowFrequency = 2, defaultHighBoostFrequency = 3, defaultHighCutFrequency = 1;   // 60 Hz, 8 kHz, 10 kHz
    constexpr float defaultBandwidth = 5.0f, maxBandwidth = 10.0f;

    // "60 Hz", "8 kHz", "1.5 kHz": whole numbers stay whole.
    inline juce::String frequencyName (float hz)
    {
        if (hz < 1000.0f)
            return juce::String ((int) hz) + " Hz";
        const float khz = hz / 1000.0f;
        const bool whole = std::abs (khz - std::round (khz)) < 0.001f;
        return juce::String (khz, whole ? 0 : 1) + " kHz";
    }

    // A choice whose first position is Off: "Off", "30 Hz", ...
    template <typename Array>
    juce::StringArray frequencyNamesWithOff (const Array& frequencies)
    {
        juce::StringArray names;
        for (auto hz : frequencies)
            names.add (hz <= 0.0f ? juce::String ("Off") : frequencyName (hz));
        return names;
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
        // PEQ-04: a configuration row (the EQ Mode dropdown) sits under the title bar, outside the blue panel, so the panel starts
        // lower (plateTop) and the window is taller by the row and by a little more room for the Mid Shaper's two-line switch.
        constexpr int modeRowY = 58, modeRowHeight = 28, plateTop = modeRowY + modeRowHeight + 6;
        constexpr int designWidth = 1144, contentHeight = 500;   // the controls sit above the footer strip; 128 wider than before for the Output panel (songsmithstudio#16)
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 858, maxWidth = 3432;           // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
