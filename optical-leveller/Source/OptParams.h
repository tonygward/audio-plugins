#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OpticalEngine.h"

// Songsmith Optical Leveller: parameter IDs, ranges, defaults and the editor's layout, shared by the processor,
// the editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace Opt
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* peakReductionId = "peak_reduction";   // 0 to 100
    inline constexpr const char* gainId          = "gain";             // dB
    inline constexpr const char* modeId          = "mode";             // 0 = Compress, 1 = Limit
    inline constexpr const char* bypassId        = "bypass";           // the host's native bypass
    inline constexpr const char* oversamplingId  = "oversampling";     // 0 = Off, 1 = 2x, 2 = 4x (version 2; songsmithstudio#19)

    // --- Added in version 3 (songsmithstudio#16): the suite's Output panel. 0 dB and off, so a session saved before sounds as it did.
    inline constexpr const char* outputTrimId = "output_trim";   // dB, applied last, after Auto Gain; separate from Gain
    inline constexpr const char* autoGainId   = "auto_gain";     // on or off, off by default
    inline constexpr float minOutputTrimDb = -24.0f, maxOutputTrimDb = 24.0f;

    // --- Defaults: a gentle, useful starting point that leaves the level where it was --------------------
    constexpr float defaultPeakReduction = 40.0f, defaultGainDb = 1.0f;
    constexpr int defaultMode = 0;   // Compress

    inline juce::StringArray modeNames() { return { "Compress", "Limit" }; }
    inline juce::StringArray oversamplingNames() { return { "Off", "2x", "4x" }; }
    constexpr int defaultOversampling = 0;   // Off: as every session saved before it existed sounded, and no delay

    // The meter: 0 = Gain Reduction, 1 = Output (0 on the scale at -18 dBFS, like the LA-2A's +4 position).
    inline juce::String meterModeName (int mode) { return mode == 0 ? "Gain Reduction" : "Output"; }

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int designWidth = 960, contentHeight = 372;   // 760 for the controls plus the right-hand meter column (SUI-23); they sit above the footer strip
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 720, maxWidth = 2880;           // 75 % to 300 %; 20 design pixels wider than before for the Output panel (songsmithstudio#16)

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
