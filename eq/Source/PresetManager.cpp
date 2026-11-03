#include "PresetManager.h"
#include "PluginProcessor.h"

namespace EQ
{
    namespace
    {
        struct FactoryBand
        {
            FilterType type;
            float freq, gain, q;
            int slope = defaultSlope;
            bool enabled = true;          // false: used but bypassed ("only if needed")
            bool dynOn = false;
            float threshold = defaultThreshold, range = defaultRange;
        };

        struct FactoryPreset
        {
            const char* name;
            std::vector<FactoryBand> bands;
            float outputGain = 0.0f;
            const char* hint = nullptr;   // one-line usage hint, shown as the preset bar tooltip
            bool startingPoint = false;   // listed under "Starting points"
        };

        // Starting-point templates. Based on the EQ advice in Joe Gilder's Home Studio
        // Corner videos (see README): subtractive first, small moves, "only if needed"
        // moves loaded bypassed, and output gain set so the template is loudness-matched
        // on pink noise (K-weighted, bypassed bands off). Q: wide ~0.8, medium ~1.4, narrow 5-8.
        FactoryBand off (FactoryBand b) { b.enabled = false; return b; }

        FactoryBand dynamic (FactoryBand b, float threshold, float range)
        {
            b.dynOn = true;
            b.threshold = threshold;
            b.range = range;
            return b;
        }

        const std::vector<FactoryPreset>& factoryPresets()
        {
            using T = FilterType;
            static const std::vector<FactoryPreset> presets {
                { "Flat", { { T::Bell, 100.0f, 0.0f, defaultQ }, { T::Bell, 1000.0f, 0.0f, defaultQ }, { T::Bell, 8000.0f, 0.0f, defaultQ } } },
                { "Vocal clean-up", { { T::LowCut, 90.0f, 0.0f, defaultQ, 2 },
                                      { T::Bell, 250.0f, -3.0f, 1.2f },
                                      { T::Bell, 3200.0f, 2.0f, 1.0f },
                                      { T::HighShelf, 10000.0f, 2.5f, defaultQ } } },
                { "Low-end tidy", { { T::LowCut, 28.0f, 0.0f, defaultQ, 3 },
                                    { T::LowShelf, 80.0f, 1.5f, defaultQ },
                                    { T::Bell, 200.0f, -2.5f, 1.0f } } },
                { "Air", { { T::HighShelf, 12000.0f, 4.0f, defaultQ },
                           { T::Bell, 5000.0f, -1.0f, 2.0f } } },
                { "Telephone", { { T::LowCut, 400.0f, 0.0f, defaultQ, 3 },
                                 { T::Bell, 1500.0f, 4.0f, 1.2f },
                                 { T::HighCut, 3200.0f, 0.0f, defaultQ, 3 } }, -3.0f },
                { "Kick punch", { { T::LowCut, 30.0f, 0.0f, defaultQ, 2 },
                                  { T::Bell, 60.0f, 4.0f, 1.4f },
                                  { T::Bell, 350.0f, -5.0f, 1.6f },
                                  { T::Bell, 3500.0f, 3.0f, 1.5f } }, -2.0f },
                { "Acoustic guitar", { { T::LowCut, 80.0f, 0.0f, defaultQ },
                                       { T::Bell, 200.0f, -3.0f, 1.2f },
                                       { T::Bell, 2500.0f, 1.5f, 1.0f },
                                       { T::HighShelf, 9000.0f, 2.0f, defaultQ } } },
                { "De-mud", { { T::Bell, 300.0f, -4.0f, 1.0f },
                              { T::Bell, 500.0f, -2.0f, 2.0f } } },

                // ---- Starting points (append only: keeps the indices above stable) ----
                { "Lead vocal", { { T::LowCut, 120.0f, 0.0f, defaultQ, 1 },
                                  { T::Bell, 200.0f, -3.0f, 0.9f },
                                  dynamic ({ T::Bell, 2600.0f, -2.0f, 6.0f }, -30.0f, -4.0f),
                                  off ({ T::HighShelf, 10000.0f, 2.0f, defaultQ }) },
                  0.7f, "Move the 200 Hz cut to the mud you hear. Air shelf is off: only if still dark.", true },
                { "Backing vocals", { { T::LowShelf, 200.0f, -4.0f, defaultQ },
                                      { T::Bell, 500.0f, -3.0f, 0.8f },
                                      { T::HighShelf, 8000.0f, 2.5f, defaultQ } },
                  -0.2f, "For the BV bus: thin the lows and mids, then set how much top end they need.", true },
                { "Spoken word", { { T::LowCut, 100.0f, 0.0f, defaultQ, 1 },
                                   { T::Bell, 200.0f, -3.0f, 0.9f },
                                   { T::Bell, 2600.0f, -2.0f, 6.0f } },
                  0.7f, "Cut rumble and mud; keep the 2.6 kHz dip only if loud words whistle.", true },
                { "Acoustic guitar body", { off ({ T::LowCut, 45.0f, 0.0f, defaultQ, 1 }),
                                            { T::Bell, 170.0f, -3.0f, 1.4f },
                                            { T::Bell, 1800.0f, -1.5f, 1.4f },
                                            off ({ T::HighShelf, 8000.0f, 1.5f, defaultQ }) },
                  0.4f, "Find the body thump between 150 and 200 Hz by ear. Dip it; don't remove it.", true },
                { "Electric guitar", { { T::LowCut, 80.0f, 0.0f, defaultQ, 1 },
                                       { T::Bell, 175.0f, -2.5f, 1.2f },
                                       { T::Bell, 2500.0f, -2.0f, 0.9f } },
                  0.9f, "Tone comes from the recording. Trim 150-200 Hz mud and 2-3 kHz bite if needed.", true },
                // Breaks the "boosts <= cuts" rule on purpose: the 360 Hz warmth boost is the move.
                { "Electric guitar bus", { { T::Bell, 360.0f, 1.5f, 0.8f },
                                           off ({ T::Bell, 3440.0f, -7.5f, 8.0f }) },
                  -0.2f, "360 Hz adds warmth without mud. Notch is off: sweep it onto a whistle if any.", true },
                { "Bass guitar", { { T::Bell, 130.0f, -3.0f, 1.4f },
                                   { T::Bell, 50.0f, 2.0f, 0.9f },
                                   off ({ T::Bell, 2000.0f, 1.5f, 1.0f }) },
                  0.0f, "Pull down the 120-150 Hz mud first. Click band is off: only if notes get lost.", true },
                { "Drum bus", { { T::Bell, 360.0f, -4.0f, 0.8f },
                                off ({ T::LowShelf, 80.0f, 2.0f, defaultQ }),
                                off ({ T::Bell, 130.0f, -3.0f, 5.0f }),
                                { T::Bell, 3000.0f, 1.5f, 1.0f } },
                  0.0f, "Cut the boxiness around 360-400 Hz. Extra bands are off: use them only if needed.", true },
                { "Snare", { { T::Bell, 190.0f, -3.0f, 1.4f },
                             { T::LowShelf, 200.0f, 2.0f, defaultQ } },
                  0.0f, "Cut the thick spot just below 200 Hz; the shelf then adds a more even low end.", true },
                { "Reverb return", { { T::LowCut, 200.0f, 0.0f, defaultQ, 2 },
                                     off ({ T::Bell, 4800.0f, -3.0f, 1.4f }) },
                  0.6f, "Put it after the reverb. Raise the cut towards 500 Hz if the mix still feels muddy.", true },
                { "Five-zone start", { { T::Bell, 50.0f, 0.0f, 0.9f },
                                       { T::Bell, 160.0f, 0.0f, 0.9f },
                                       { T::Bell, 550.0f, 0.0f, 0.9f },
                                       { T::Bell, 2600.0f, 0.0f, 0.9f },
                                       { T::Bell, 10000.0f, 0.0f, 0.9f } },
                  0.0f, "Lows, low mids, mids, high mids, highs: find the zone that's too loud, turn it down.", true },
            };
            return presets;
        }

        BandSnapshot defaultSnapshot (int band)
        {
            BandSnapshot s;
            s.used = false;
            s.freq = defaultFreq (band);
            return s;
        }
    }

    //==========================================================================
    namespace
    {
        juce::File& testingRoot()
        {
            static juce::File root;
            return root;
        }

        juce::File appDataRoot()
        {
            return testingRoot() != juce::File() ? testingRoot()
                                                 : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        }
    }

    juce::File PresetManager::getDefaultUserFolder()
    {
        // Vendor folder first, so later Songsmith plugins sit alongside.
        return appDataRoot().getChildFile (JucePlugin_Manufacturer)
                            .getChildFile (JucePlugin_Name)
                            .getChildFile ("Presets");
    }

    juce::File PresetManager::getLegacyUserFolder()
    {
        return appDataRoot().getChildFile ("TW Pro EQ").getChildFile ("Presets");   // legacy: TW Pro EQ's folder
    }

    void PresetManager::setDefaultFoldersRootForTesting (const juce::File& root) { testingRoot() = root; }

    void PresetManager::setUserFolder (const juce::File& f, const juce::File& legacy)
    {
        userFolder = f;
        legacyFolder = legacy;
        migrationChecked = false;
    }

    juce::File PresetManager::getUserFolder() const
    {
        migrateIfNeeded();
        return userFolder;
    }

    void PresetManager::migrateIfNeeded() const
    {
        if (migrationChecked)
            return;
        migrationChecked = true;

        if (legacyFolder != juce::File())
            migrateLegacyFolder (legacyFolder, userFolder);
    }

    bool PresetManager::migrateLegacyFolder (const juce::File& legacy, const juce::File& target)
    {
        if (target.exists() || ! legacy.isDirectory())
            return false;

        const auto parent = target.getParentDirectory();
        if (! parent.isDirectory() && ! parent.createDirectory())
            return false;

        // Copy next to the target, then rename: a failure never leaves a half-filled target.
        const auto temp = parent.getNonexistentChildFile (target.getFileName() + "-migrating", {}, false);
        if (legacy.copyDirectoryTo (temp) && temp.moveFileTo (target))
            return true;

        temp.deleteRecursively();
        return false;
    }

    int PresetManager::getNumFactoryPresets()                 { return (int) factoryPresets().size(); }
    juce::String PresetManager::getFactoryName (int index)    { return factoryPresets()[(size_t) index].name; }
    int PresetManager::getFactoryUsedBands (int index)        { return (int) factoryPresets()[(size_t) index].bands.size(); }
    bool PresetManager::isStartingPoint (int index)           { return factoryPresets()[(size_t) index].startingPoint; }

    juce::String PresetManager::getFactoryHint (int index)
    {
        const auto* h = factoryPresets()[(size_t) index].hint;
        return h != nullptr ? juce::String (juce::CharPointer_UTF8 (h)) : juce::String();
    }

    PresetData PresetManager::getFactoryPreset (int index)
    {
        const auto& f = factoryPresets()[(size_t) juce::jlimit (0, getNumFactoryPresets() - 1, index)];

        PresetData d;
        for (int b = 0; b < numBands; ++b)
            d.bands[(size_t) b] = defaultSnapshot (b);

        for (size_t i = 0; i < f.bands.size(); ++i)
        {
            auto& s = d.bands[i];
            s.used = true;
            s.enabled = f.bands[i].enabled;
            s.type = (int) f.bands[i].type;
            s.freq = f.bands[i].freq;
            s.gain = typeHasGain (f.bands[i].type) ? f.bands[i].gain : 0.0f;
            s.q = f.bands[i].q;
            s.slope = f.bands[i].slope;
            s.dynOn = f.bands[i].dynOn;
            s.threshold = f.bands[i].threshold;
            s.range = f.bands[i].range;
        }
        d.outputGain = f.outputGain;
        return d;
    }

    //==========================================================================
    std::vector<PresetManager::Entry> PresetManager::getUserPresets() const
    {
        migrateIfNeeded();

        std::vector<Entry> list;
        if (! userFolder.isDirectory())
            return list;

        auto files = userFolder.findChildFiles (juce::File::findFiles, false,
                                                juce::String ("*") + fileExtension + ";*" + legacyFileExtension);
        files.sort();
        for (auto& f : files)
            list.push_back ({ f.getFileNameWithoutExtension(), -1, f });
        return list;
    }

    std::vector<PresetManager::Entry> PresetManager::getAllPresets() const
    {
        std::vector<Entry> list;
        for (int i = 0; i < getNumFactoryPresets(); ++i)
            list.push_back ({ getFactoryName (i), i, {} });
        for (auto& e : getUserPresets())
            list.push_back (e);
        return list;
    }

    //==========================================================================
    bool PresetManager::decode (const juce::ValueTree& state, PresetData& out, juce::String& error)
    {
        if (! state.isValid() || ! state.hasType ("PARAMETERS"))
        {
            error = juce::String ("This file is not a ") + JucePlugin_Name + " preset.";
            return false;
        }

        // Exactly the session restore path (defaults for missing parameters, v1 -> v2 band
        // slots, ...), applied to a scratch instance, so presets migrate like sessions.
        SongsmithEQAudioProcessor scratch;
        scratch.restoreFromValueTree (state);
        out = scratch.edits.getPresetData();
        return true;
    }

    bool PresetManager::readFile (const juce::File& file, PresetData& out, juce::String& error) const
    {
        if (! file.existsAsFile())
        {
            error = "The preset file \"" + file.getFileName() + "\" no longer exists.";
            return false;
        }

        if (file.getSize() > 4 * 1024 * 1024)
        {
            error = "\"" + file.getFileName() + "\" is too large to be a preset.";
            return false;
        }

        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
        {
            error = "\"" + file.getFileName() + "\" could not be read (it is damaged or not a preset).";
            return false;
        }

        if (! decode (juce::ValueTree::fromXml (*xml), out, error))
        {
            error = "\"" + file.getFileName() + "\": " + error;
            return false;
        }
        return true;
    }

    //==========================================================================
    void PresetManager::setCurrent (const Entry& e, const PresetData& applied)
    {
        current = e;
        currentData = std::make_unique<PresetData> (applied);
        resolveAttempted = true;
    }

    bool PresetManager::load (const Entry& e, juce::String& error)
    {
        PresetData data;

        if (e.isFactory())
            data = getFactoryPreset (e.factoryIndex);
        else if (! readFile (e.file, data, error))
            return false;

        processor.edits.loadPreset (data, e.name);
        processor.notifyNonParameterStateChanged();   // band slots may have changed

        // Compare later against what the parameters actually hold (after range snapping).
        setCurrent (e, processor.edits.getPresetData());
        return true;
    }

    bool PresetManager::loadNext (int delta, juce::String& error)
    {
        const auto all = getAllPresets();
        if (all.empty())
            return false;

        int index = -1;
        for (size_t i = 0; i < all.size(); ++i)
        {
            const auto& e = all[i];
            if (hasCurrent() && e.factoryIndex == current.factoryIndex
                && (e.isFactory() || e.file == current.file))
                index = (int) i;
        }

        const int n = (int) all.size();
        const int next = index < 0 ? (delta > 0 ? 0 : n - 1) : ((index + delta) % n + n) % n;
        return load (all[(size_t) next], error);
    }

    bool PresetManager::saveAs (const juce::String& rawName, juce::String& error)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty())
        {
            error = "Please enter a name for the preset.";
            return false;
        }

        migrateIfNeeded();   // copy old presets in before creating the folder, or they'd never be

        if (! userFolder.isDirectory() && ! userFolder.createDirectory())
        {
            error = "Could not create the preset folder:\n" + userFolder.getFullPathName();
            return false;
        }

        // Overwriting a preset saved by TW Pro EQ keeps its .twpreset file, so "Save" updates
        // that preset in place instead of adding a second preset with the same name.
        auto file = userFolder.getChildFile (name + fileExtension);
        if (const auto legacy = userFolder.getChildFile (name + legacyFileExtension);
            ! file.existsAsFile() && legacy.existsAsFile())
            file = legacy;
        auto state = processor.createPresetState();
        state.removeChild (state.getChildWithProperty ("id", bypassId), nullptr);   // a preset never bypasses
        const auto xml = state.createXml();

        if (xml == nullptr || ! xml->writeTo (file))
        {
            error = "Could not write the preset file:\n" + file.getFullPathName();
            return false;
        }

        setCurrent ({ name, -1, file }, processor.edits.getPresetData());
        processor.notifyNonParameterStateChanged();
        return true;
    }

    bool PresetManager::canSaveInPlace() const
    {
        return hasCurrent() && ! current.isFactory() && current.file.existsAsFile();
    }

    bool PresetManager::deleteCurrent (juce::String& error)
    {
        if (! hasCurrent() || current.isFactory())
        {
            error = "Factory presets can't be deleted.";
            return false;
        }

        if (current.file.existsAsFile() && ! current.file.deleteFile())
        {
            error = "Could not delete:\n" + current.file.getFullPathName();
            return false;
        }

        clearCurrent();
        processor.notifyNonParameterStateChanged();
        return true;
    }

    bool PresetManager::isModified()
    {
        if (! hasCurrent())
            return false;

        if (currentData == nullptr && ! resolveAttempted)
        {
            resolveAttempted = true;
            PresetData d;
            juce::String ignored;

            if (current.isFactory())
            {
                // Round-trip through the parameters so the comparison sees snapped values.
                SongsmithEQAudioProcessor scratch;
                scratch.edits.applyPresetData (getFactoryPreset (current.factoryIndex));
                currentData = std::make_unique<PresetData> (scratch.edits.getPresetData());
            }
            else if (readFile (current.file, d, ignored))
            {
                currentData = std::make_unique<PresetData> (d);
            }
        }

        return currentData != nullptr && ! processor.edits.getPresetData().sameSound (*currentData);
    }

    //==========================================================================
    void PresetManager::saveToUiState (juce::ValueTree& ui) const
    {
        if (! hasCurrent())
            return;

        ui.setProperty ("presetName", current.name, nullptr);
        ui.setProperty ("presetFactory", current.factoryIndex, nullptr);
        if (! current.isFactory())
            ui.setProperty ("presetFile", current.file.getFullPathName(), nullptr);
    }

    void PresetManager::restoreFromUiState (const juce::ValueTree& ui)
    {
        clearCurrent();
        resolveAttempted = false;

        const juce::String name = ui.getProperty ("presetName", juce::String());
        if (name.isEmpty())
            return;

        const int factory = ui.getProperty ("presetFactory", -1);
        current.name = name;
        current.factoryIndex = (factory >= 0 && factory < getNumFactoryPresets()) ? factory : -1;

        const juce::String path = ui.getProperty ("presetFile", juce::String());
        if (! current.isFactory() && juce::File::isAbsolutePath (path))
        {
            current.file = juce::File (path);

            // Legacy: a session saved by TW Pro EQ points into the old presets folder. Use the
            // migrated copy if there is one (read-only check: this can run on any thread).
            if (legacyFolder != juce::File() && current.file.isAChildOf (legacyFolder))
                if (const auto copy = userFolder.getChildFile (current.file.getRelativePathFrom (legacyFolder)); copy.existsAsFile())
                    current.file = copy;
        }
    }
}
