#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Factory starting points (compiled in) and your own presets (XML files), the same workflow as the other Songsmith
// plugins: your presets are .sspreset files in
//   %APPDATA%\Songsmith Studio\Songsmith Passive EQ\Presets      (Windows)
//   ~/Library/Songsmith Studio/Songsmith Passive EQ/Presets       (macOS)
// a folder of their own, so they never mix with another plugin's. A preset stores the sound (every frequency and
// amount), never Bypass or editor state. Message thread only.
namespace Peq
{
    struct FactoryPreset
    {
        const char* name;
        int lowFrequency;        // index into the low switch
        float lowBoost, lowCut;
        int highBoostFrequency;  // index into the high boost switch
        float highBoost, bandwidth;
        int highCutFrequency;    // index into the high cut switch
        float highCut;
        const char* hint;        // a listening cue, shown on hover

        // PEQ-04: the mode this starting point is in, and the Mid Shaper and Frequency Filter settings. The Tone Shaper
        // starting points above leave these at their defaults (Tone Shaper, nothing set in the other modes).
        int mode = 0;                         // 0 Tone Shaper, 1 Mid Shaper, 2 Frequency Filter
        int midLowFrequency = 2;              // index into the low-mid switch
        float midLowBoost = 0.0f;
        int midDipFrequency = 4;              // index into the mid dip switch
        float midDip = 0.0f;
        int midHighFrequency = 2;             // index into the high-mid switch
        float midHighBoost = 0.0f;
        int highPass = 0, lowPass = 0;        // 0 = Off
    };

    // PEQ-06: the factory starting points grouped for the preset menu. A group is a heading you cannot choose, with the
    // starting points that belong to it under it. 'presets' are indices into Presets::factory(), in the order shown.
    // Grouping changes only how the menu is laid out: a starting point keeps its index, its name and its menu item number,
    // so saved sessions, the host's program list and your own presets are unaffected.
    struct FactoryCategory
    {
        const char* name;
        std::vector<int> presets;
    };

    class Presets
    {
    public:
        static constexpr const char* fileExtension = ".sspreset";
        static constexpr const char* rootTag = "SongsmithPassiveEQPreset";

        static const std::vector<FactoryPreset>& factory();

        // The groups in menu order. Every starting point is in exactly one group, and no group is empty.
        static const std::vector<FactoryCategory>& categories();
        // The group a starting point is in (its name), or an empty string for an index that is not a starting point.
        static juce::String categoryOf (int factoryIndex);
        // The same groups without any that hold nothing, so a menu never shows a heading with nothing under it.
        static std::vector<FactoryCategory> withoutEmptyCategories (const std::vector<FactoryCategory>& groups);

        explicit Presets (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

        static juce::File getDefaultUserFolder();
        // Tests: redirect the default folder under 'root' (an empty File is the real location).
        static void setDefaultFolderRootForTesting (const juce::File& root);

        juce::File getUserFolder() const { return userFolder; }
        void setUserFolder (const juce::File& f) { userFolder = f; }
        juce::Array<juce::File> getUserPresetFiles() const;

        // Each changed parameter is set as one complete gesture (automation-friendly).
        void loadFactory (int index);
        bool loadFile (const juce::File&, juce::String& error);
        // Refuses an empty name and any starting point's name (those stay read-only). Overwrites a file of the same
        // name, so the editor asks first (see userPresetExists).
        bool saveAs (const juce::String& name, juce::String& error);
        bool userPresetExists (const juce::String& name) const;
        static bool isFactoryName (const juce::String& name);

        static const char* const presetParameterIds[];
        static const int numPresetParameters;

        juce::String currentName;   // shown in the editor; saved with the session's editor state
        juce::String currentHint;

    private:
        void setParam (const char* id, float plainValue);

        juce::AudioProcessorValueTreeState& apvts;
        juce::File userFolder { getDefaultUserFolder() };
    };
}
