#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include "EQEditController.h"

class SongsmithEQAudioProcessor;

namespace EQ
{
    // Factory presets (compiled in, read-only) and user presets (XML files).
    //
    // A user preset file is the plugin's own versioned state XML (root <PARAMETERS>,
    // 'version' property, <BANDSLOTS>) without the UI section, so old presets go
    // through exactly the same migration as old sessions. A preset holds every band
    // (used/unused, type, slope, freq, gain, Q, on) and the output gain; never bypass,
    // editor size or analyser settings.
    //
    // Message thread only.
    class PresetManager
    {
    public:
        static constexpr const char* fileExtension = ".twpreset";

        struct Entry
        {
            juce::String name;
            int factoryIndex = -1;   // >= 0: factory preset
            juce::File file;         // user preset

            bool isFactory() const noexcept { return factoryIndex >= 0; }
        };

        explicit PresetManager (SongsmithEQAudioProcessor& p) : processor (p) {}

        // %APPDATA%/TW Pro EQ/Presets on Windows, ~/Library/TW Pro EQ/Presets on macOS.
        static juce::File getDefaultUserFolder();
        juce::File getUserFolder() const { return userFolder; }
        void setUserFolder (const juce::File& f) { userFolder = f; }   // tests

        // Factory presets
        static int getNumFactoryPresets();
        static juce::String getFactoryName (int index);
        static PresetData getFactoryPreset (int index);
        static int getFactoryUsedBands (int index);
        static bool isStartingPoint (int index);          // listed under "Starting points"
        static juce::String getFactoryHint (int index);   // empty when the preset has none

        std::vector<Entry> getUserPresets() const;
        std::vector<Entry> getAllPresets() const;   // factory first, then user (sorted)

        // Loading is one undo transaction. Returns false (state unchanged) with a message on error.
        bool load (const Entry&, juce::String& error);
        bool loadNext (int delta, juce::String& error);

        // Saves the current EQ as a user preset (overwrites a file of the same name).
        bool saveAs (const juce::String& name, juce::String& error);
        bool canSaveInPlace() const;   // current preset is a user preset whose file exists
        bool deleteCurrent (juce::String& error);

        // Reads and migrates a preset file without applying it.
        bool readFile (const juce::File&, PresetData& out, juce::String& error) const;
        static bool decode (const juce::ValueTree& state, PresetData& out, juce::String& error);

        // Current preset (shown in the preset bar).
        bool hasCurrent() const noexcept { return current.name.isNotEmpty(); }
        const Entry& getCurrent() const noexcept { return current; }
        bool isModified();   // current EQ differs from the loaded preset
        void clearCurrent() { current = {}; currentData.reset(); }

        // Persisted with the session's UI state so the preset bar survives reopening.
        void saveToUiState (juce::ValueTree& ui) const;
        void restoreFromUiState (const juce::ValueTree& ui);

    private:
        void setCurrent (const Entry&, const PresetData& applied);

        SongsmithEQAudioProcessor& processor;
        juce::File userFolder { getDefaultUserFolder() };
        Entry current;
        std::unique_ptr<PresetData> currentData;   // resolved lazily after a session restore
        bool resolveAttempted = false;
    };
}
