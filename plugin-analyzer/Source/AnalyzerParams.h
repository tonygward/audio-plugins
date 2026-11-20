#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Songsmith Plugin Analyzer: parameter IDs, ranges, defaults and the editor's layout, shared by the processor, the editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace PluginAnalyzer
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* roleId    = "role";      // 0 = Reference (goes before the plugin), 1 = Result (goes after it)
    inline constexpr const char* sessionId = "session";   // 0 to 7: which Reference and Result belong together
    inline constexpr const char* channelId = "channel";   // 0 = Left, 1 = Right, 2 = both combined
    inline constexpr const char* levelId   = "test_level";   // dB, the level of the test sweep
    inline constexpr const char* bypassId  = "bypass";    // the host's native bypass

    inline juce::StringArray roleNames()    { return { "Reference", "Result" }; }
    inline juce::StringArray channelNames() { return { "Left", "Right", "Both Combined" }; }
    inline juce::StringArray sessionNames()
    {
        juce::StringArray names;
        for (int i = 1; i <= 8; ++i)
            names.add ("Session " + juce::String (i));
        return names;
    }

    constexpr float minTestLevelDb = -36.0f, maxTestLevelDb = -6.0f, defaultTestLevelDb = -18.0f;
    constexpr int numSlots = 4;
    inline juce::String defaultSlotName (int index) { return "Measurement " + juce::String (index + 1); }

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int designWidth = 1100, contentHeight = 560;   // 1100 includes the far-right Output meter column
        constexpr int footerHeight = 28;                          // the latency and Processing Load strip below
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 825, maxWidth = 3300;            // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
