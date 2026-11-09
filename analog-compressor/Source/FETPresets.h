#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Factory starting points (compiled in) and user presets (XML files), in the same workflow as
// Songsmith EQ: user presets are .sspreset files in
//   %APPDATA%\Songsmith Studio\Songsmith Analog Compressor\Presets      (Windows)
//   ~/Library/Songsmith Studio/Songsmith Analog Compressor/Presets       (macOS)
// a folder of their own, so they never mix with the EQ's. A preset stores the sound
// (input, output, attack, release, ratio, All Buttons, GR Off), never bypass, oversampling
// or editor state. Message thread only.
namespace FET
{
    struct FactoryPreset
    {
        const char* name;
        float input, output, attack, release;
        int ratio;           // 0..3 = 4, 8, 12, 20
        bool allButtons;
        const char* hint;    // listening cue, shown in the editor
    };

    class Presets
    {
    public:
        static constexpr const char* fileExtension = ".sspreset";
        static constexpr const char* rootTag = "SongsmithAnalogCompressorPreset";

        static const std::vector<FactoryPreset>& factory();

        explicit Presets (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

        static juce::File getDefaultUserFolder();
        // Tests: redirect the default folder under 'root' (empty File = the real location).
        static void setDefaultFolderRootForTesting (const juce::File& root);

        juce::File getUserFolder() const { return userFolder; }
        void setUserFolder (const juce::File& f) { userFolder = f; }
        juce::Array<juce::File> getUserPresetFiles() const;

        // Each changed parameter is set as one complete gesture (automation-friendly).
        void loadFactory (int index);
        bool loadFile (const juce::File&, juce::String& error);
        // Refuses an empty name and any starting point's name (those stay read-only). Overwrites a
        // file of the same name, so the editor asks first (see userPresetExists).
        bool saveAs (const juce::String& name, juce::String& error);
        // True when a user preset with this name (after the same cleaning saveAs applies) is already saved.
        bool userPresetExists (const juce::String& name) const;
        static bool isFactoryName (const juce::String& name);

        static const char* const presetParameterIds[];
        static const int numPresetParameters;

        juce::String currentName;   // shown in the editor; saved with the session UI state
        juce::String currentHint;

    private:
        void setParam (const char* id, float plainValue);

        juce::AudioProcessorValueTreeState& apvts;
        juce::File userFolder { getDefaultUserFolder() };
    };
}
