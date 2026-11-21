#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Songsmith Plugin Analyzer: parameter IDs, ranges, defaults and the editor's layout, shared by the processor, the editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace PluginAnalyzer
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* roleId    = "role";      // 0 = Before Plugin (goes before the plugin), 1 = After Plugin (goes after it)
    inline constexpr const char* sessionId = "session";   // 0 to 7: which Before Plugin and After Plugin belong together (found automatically)
    inline constexpr const char* channelId = "channel";   // 0 = Left, 1 = Right, 2 = both combined
    inline constexpr const char* levelId   = "test_level";   // dB, the level of the test sweep
    inline constexpr const char* bypassId  = "bypass";    // the host's native bypass

    // ANALYZER-02: the roles were called Reference and Result. The parameter, its ID and its saved values are unchanged; only the words are.
    inline juce::StringArray roleNames()    { return { "Before Plugin", "After Plugin" }; }
    inline juce::StringArray channelNames() { return { "Left", "Right", "Both Combined" }; }
    inline juce::StringArray sessionNames()
    {
        juce::StringArray names;
        for (int i = 1; i <= 8; ++i)
            names.add ("Session " + juce::String (i));
        return names;
    }

    constexpr float minTestLevelDb = -36.0f, maxTestLevelDb = -6.0f, defaultTestLevelDb = -18.0f;
    // ANALYZER-02: two captures to compare (it used to be four named slots). A session saved with four is brought down to two when it opens.
    constexpr int numSlots = 2;
    constexpr int legacySlotCount = 4;
    inline juce::String defaultSlotName (int index) { return "Capture " + juce::String (index + 1); }
    inline juce::StringArray pairNames()
    {
        juce::StringArray names;
        for (int i = 1; i <= 8; ++i)
            names.add ("Pair " + juce::String (i));
        return names;
    }

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
