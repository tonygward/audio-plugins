#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BusEngine.h"

// Songsmith Bus Compressor: parameter IDs, ranges, defaults and the editor's layout, shared by the processor,
// the editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace BusComp
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* thresholdId = "threshold";    // the dial, -15 to +10
    inline constexpr const char* ratioId     = "ratio";        // 0 = 2:1, 1 = 4:1, 2 = 10:1
    inline constexpr const char* attackId    = "attack";       // 0 to 5, 0.1 to 30 ms
    inline constexpr const char* releaseId   = "release";      // 0 to 4, 0.1 s to 1.2 s, then Auto
    inline constexpr const char* makeupId    = "makeup_gain";  // dB
    inline constexpr const char* bypassId    = "bypass";       // the host's native bypass

    // --- Defaults: a gentle, useful starting point -----------------------------------------
    constexpr float defaultThreshold = 4.0f, defaultMakeupDb = 0.0f;
    constexpr int defaultRatio = 0, defaultAttack = 4, defaultRelease = SongsmithBus::autoReleaseIndex;   // 2:1, 10 ms, Auto

    inline juce::StringArray ratioNames()   { return { "2:1", "4:1", "10:1" }; }
    inline juce::StringArray attackNames()  { return { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }; }
    inline juce::StringArray releaseNames() { return { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }; }

    // The meter reads gain reduction, 0 to 20 dB.
    inline constexpr float meterMaxDb = 20.0f;

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int designWidth = 800, contentHeight = 392;   // the controls sit above the footer strip
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 600, maxWidth = 2400;           // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
