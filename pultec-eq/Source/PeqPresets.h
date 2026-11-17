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
    };

    class Presets
    {
    public:
        static constexpr const char* fileExtension = ".sspreset";
        static constexpr const char* rootTag = "SongsmithPassiveEQPreset";

        static const std::vector<FactoryPreset>& factory();

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
