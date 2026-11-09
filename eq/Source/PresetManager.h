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
        // New saves use fileExtension. Presets saved by TW Pro EQ (<= v0.2) use
        // legacyFileExtension; they are still listed and loaded.
        static constexpr const char* fileExtension = ".sspreset";
        static constexpr const char* legacyFileExtension = ".twpreset";   // legacy: TW Pro EQ

        struct Entry
        {
            juce::String name;       // what the preset bar shows; for a Starting Point, its full label
            int factoryIndex = -1;   // >= 0: factory preset (the stable identity, never reordered)
            juce::File file;         // user preset
            juce::String group;      // Starting Points: the menu group ("Drums"); empty for a user preset

            bool isFactory() const noexcept { return factoryIndex >= 0; }
        };

        explicit PresetManager (SongsmithEQAudioProcessor& p) : processor (p) {}

        // %APPDATA%/Songsmith Studio/Songsmith EQ/Presets on Windows,
        // ~/Library/Songsmith Studio/Songsmith EQ/Presets on macOS.
        static juce::File getDefaultUserFolder();

        // Legacy: where TW Pro EQ (<= v0.2) kept user presets (%APPDATA%/TW Pro EQ/Presets).
        static juce::File getLegacyUserFolder();

        // Tests: redirect both default folders under 'root', so no test ever touches the
        // real user folders. An empty File restores the real locations.
        static void setDefaultFoldersRootForTesting (const juce::File& root);

        // The user folder, after the one-off legacy copy (see migrateLegacyFolder).
        juce::File getUserFolder() const;

        // Tests: use 'f' as the user folder and 'legacy' as the old folder to migrate from
        // (empty = no migration).
        void setUserFolder (const juce::File& f, const juce::File& legacy = {});

        // If 'target' doesn't exist and 'legacy' is a folder, copies (never moves) the
        // legacy folder's contents into 'target'; the legacy folder is left as it is, so an
        // older build still finds its presets. Copies into a temporary sibling first and
        // renames it, so a failed copy leaves no 'target' (an empty preset list) rather
        // than a half-copied one. Returns true if anything was copied. Message thread only.
        static bool migrateLegacyFolder (const juce::File& legacy, const juce::File& target);

        // Factory presets
        static int getNumFactoryPresets();
        static juce::String getFactoryName (int index);
        static PresetData getFactoryPreset (int index);
        static int getFactoryUsedBands (int index);
        static bool isStartingPoint (int index);          // a template: has a usage hint and a Starting Points label
        // EQ-40: the menu is a view over the indexed list. Indices, names and bands never change:
        // old sessions and host programs find a preset by its index. A hidden preset (Flat, Five-zone
        // start, Rack tom) still loads from an old session; it is just not offered in the menu.
        static bool isListed (int index);                 // shown in the Starting Points menu
        static juce::String getMenuGroup (int index);     // "Drums", "Guitars", ... (empty when not listed)
        static juce::String getMenuLabel (int index);     // "Drums - Kick"; the stored name when not listed
        static std::vector<int> getListedIndices();       // in menu order: by group, then by instrument
        static std::vector<juce::String> getMenuGroups(); // in menu order
        static juce::String getFactoryHint (int index);   // empty when the preset has none

        std::vector<Entry> getUserPresets() const;
        std::vector<Entry> getAllPresets() const;   // listed Starting Points (menu order) first, then user (sorted)

        // Loading is one undo transaction. Returns false (state unchanged) with a message on error.
        bool load (const Entry&, juce::String& error);
        bool loadNext (int delta, juce::String& error);

        // Saves the current EQ as a user preset (overwrites a file of the same name).
        bool saveAs (const juce::String& name, juce::String& error);
        // True when saving under this name would replace an existing user preset (same cleaning as saveAs).
        // Save As asks the musician before replacing; "Save" on a loaded user preset is deliberate already.
        bool userPresetExists (const juce::String& name) const;
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
        void migrateIfNeeded() const;

        SongsmithEQAudioProcessor& processor;
        juce::File userFolder { getDefaultUserFolder() };
        juce::File legacyFolder { getLegacyUserFolder() };
        mutable bool migrationChecked = false;
        Entry current;
        std::unique_ptr<PresetData> currentData;   // resolved lazily after a session restore
        bool resolveAttempted = false;
    };
}
