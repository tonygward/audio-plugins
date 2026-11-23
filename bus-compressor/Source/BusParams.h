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

    // --- Added in version 2 (songsmithstudio#16): the suite's Output panel. 0 dB and off, so a session saved before sounds as it did.
    inline constexpr const char* outputTrimId = "output_trim";   // dB, applied last, after Auto Gain; separate from Make-up Gain
    inline constexpr const char* autoGainId   = "auto_gain";     // on or off, off by default
    inline constexpr float minOutputTrimDb = -24.0f, maxOutputTrimDb = 24.0f;

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
        // songsmithstudio#10: two zones, like a hardware console. The control panel at the left holds the five controls in a grid
        // (Threshold and Make-up Gain on top, Attack, Release and Ratio below); the recessed metering zone at the right holds the gain
        // reduction lamps and, at the far edge, the suite's Input, Gain Reduction and Output meters.
        constexpr int designWidth = 1120, contentHeight = 440;   // above the footer strip
        constexpr int panelX = 16, panelY = 58, panelWidth = 556, panelHeight = contentHeight - 58 - 10;   // the control panel
        constexpr int wellX = panelX + panelWidth + 16, wellY = panelY, wellRight = designWidth - 16, wellHeight = panelHeight;   // the metering zone
        constexpr int inputMeterGap = 14;   // between the Input meter and the Gain Reduction / Output column
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 840, maxWidth = 3360;           // 75 % to 300 %; 20 design pixels wider than before for the Output panel (songsmithstudio#16)

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
