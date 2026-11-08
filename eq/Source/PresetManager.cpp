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
            const char* formerName = nullptr;   // name in older versions: sessions that saved it still show this preset
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
            static const std::vector<FactoryPreset> presets = []
            {
            std::vector<FactoryPreset> list {
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
                // Index 6. Was "Acoustic guitar" (task 08) with a +1.5 dB boost at 2.5 kHz; renamed so
                // it isn't confused with the "Acoustic guitar" template, and the boost is now a cut:
                // Joe ("too much 1-5K makes it sound cheap") and Tony (a dip at 1.75 kHz) both cut there.
                { "Bright acoustic", { { T::LowCut, 80.0f, 0.0f, defaultQ },
                                       { T::Bell, 200.0f, -3.0f, 1.2f },
                                       { T::Bell, 2500.0f, -1.5f, 1.0f },
                                       { T::HighShelf, 9000.0f, 2.0f, defaultQ } },
                  0.0f, nullptr, false, "Acoustic guitar" },
                { "De-mud", { { T::Bell, 300.0f, -4.0f, 1.0f },
                              { T::Bell, 500.0f, -2.0f, 2.0f } } },

                // ---- Starting points (append only: keeps the indices above stable) ----
                { "Lead vocal", { { T::LowCut, 120.0f, 0.0f, defaultQ, 1 },
                                  { T::Bell, 200.0f, -3.0f, 0.9f },
                                  dynamic ({ T::Bell, 2600.0f, -2.0f, 6.0f }, -30.0f, -4.0f),
                                  off ({ T::HighShelf, 10000.0f, 2.0f, defaultQ }),
                                  // Task 26: Joe pulls down a second whistle "at like 4K" (2024-05-29, 12:12; 2023-11-15, 6:43).
                                  off ({ T::Bell, 4000.0f, -2.0f, 6.0f }) },
                  0.7f, "Move the 200 Hz cut to the mud you hear. The air shelf and 4 kHz dip are off: only if needed.", true },
                { "Backing vocals", { { T::LowShelf, 200.0f, -4.0f, defaultQ },
                                      { T::Bell, 500.0f, -3.0f, 0.8f },
                                      { T::HighShelf, 8000.0f, 2.5f, defaultQ } },
                  -0.2f, "For the backing vocal bus: thin the lows and mids, then set how much top end they need.", true },
                { "Spoken word", { { T::LowCut, 100.0f, 0.0f, defaultQ, 1 },
                                   { T::Bell, 200.0f, -3.0f, 0.9f },
                                   { T::Bell, 2600.0f, -2.0f, 6.0f } },
                  0.7f, "Cut rumble and mud; keep the 2.6 kHz dip only if loud words whistle.", true },
                // Joe's "three cuts and a boost" (2021-12-02) with Tony's own settings where they're more
                // specific (fx-chains/songs/acoustic-moods.md, the "Acoustic" chain): see eq/docs/starting-points.md.
                // Band 4's threshold (-27 dB): with pink noise at -20 dBFS RMS (normal playing) it cuts at
                // most 0.13 dB; at -6 dBFS RMS (loud strums) it reaches its 2.5 dB range. EQTests checks both.
                { "Acoustic guitar", { { T::LowCut, 50.0f, 0.0f, defaultQ, 1 },
                                       { T::Bell, 150.0f, -3.0f, 1.4f },
                                       { T::Bell, 500.0f, -1.5f, 2.0f },
                                       dynamic ({ T::Bell, 1800.0f, 0.0f, 4.0f }, -27.0f, -2.5f),
                                       { T::HighShelf, 10000.0f, 1.5f, defaultQ } },
                  0.0f, "Sweep the 150 Hz thump by ear and dip it, don't remove it. The 2 kHz band only cuts loud strums.",
                  true, "Acoustic guitar body" },
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

                // ---- Added by the starting-points review (task 26); append only ----
                // From Tony's own notes rather than Joe's videos (mixing/acoustic-moods.md "Steel guitar
                // rhythm", instruments/tony/gibson-j45-standard.md): a strummed part under bass and a lead.
                { "Acoustic guitar in a full mix", { { T::LowCut, 110.0f, 0.0f, defaultQ, 1 },
                                                     dynamic ({ T::Bell, 250.0f, 0.0f, 1.4f }, -24.0f, -3.0f),
                                                     { T::Bell, 2000.0f, -1.5f, 1.2f },
                                                     { T::HighShelf, 10000.0f, 1.5f, defaultQ } },
                  0.2f, "Strummed rhythm under bass and a lead. The 250 Hz band only dips boom on loud strums.", true },
                // From Tony's guitar notes (instruments/tony/yamaha-apx-3.md, takamine-esf-40c.md): a piezo DI.
                { "Acoustic guitar pickup", { { T::LowCut, 100.0f, 0.0f, defaultQ, 1 },
                                              dynamic ({ T::Bell, 2000.0f, 0.0f, 1.0f }, -26.0f, -3.0f),
                                              { T::HighShelf, 9000.0f, 1.5f, defaultQ } },
                  -0.1f, "For a piezo pickup plugged straight in. The 2 kHz band tames the quack only when it jumps out.", true },

                // ---- Task 30; append only ----
                // Tony's Pro-Q 4 preset "Abbey Road Reverb Trick" (Jul 2026), band for band, as decoded in
                // music-recording/fx-chains/saved-chains.md ("Vocals Reverb (Dual)": Pro-Q 4 -> Pure Plate ->
                // ValhallaRoom -> Pro-C 2). Cuts at Pro-Q's default slope (12 dB/oct here) and neutral Q.
                { "Abbey Road reverb", { { T::LowCut,  377.28f,   0.0f,  defaultQ, 1 },
                                         { T::Bell,    594.40f,   0.54f, 1.923f },
                                         { T::HighCut, 17056.0f,  0.0f,  defaultQ, 1 },
                                         { T::Bell,    10094.0f,  0.41f, 2.158f },
                                         { T::Bell,    2000.0f,  -3.0f,  3.016f } },
                  1.6f, "Put this before the reverb. Raise the low cut towards 600 Hz for a cleaner, thinner tail.", true },

                // ---- Task 31; append only ----
                // Tony's own tom EQ from *Between You and Me* (music-recording/fx-chains/songs/tony-ward.md), not
                // Joe Gilder's advice, so these boost more than the other templates. Waves SSL E-Channel bands
                // modelled one for one: 18 dB/oct high-pass; LF/HF "Type On" = bell, else shelf. Bell Qs the
                // E-Channel doesn't save are assumptions (LF/HF bell 0.7, HMF 1.0); see docs/starting-points.md.
                // Rack tom: Gone from Here High Tom (line 1114), You Want More Mid/High Tom (2295, 2315).
                { "Rack tom", { { T::LowCut,  162.0f,    0.0f, defaultQ, 2 },
                                { T::Bell,    163.0f,    4.6f, 0.7f },
                                { T::Bell,    670.0f,  -11.9f, 1.7f },
                                { T::Bell,    4070.0f,   5.9f, 1.0f },
                                { T::Bell,    10320.0f,  3.3f, 0.7f } },
                  -2.6f, "Tony's rack-tom EQ from Between You and Me. Lower the 670 Hz cut if the tom sounds hollow.", true },
                // Floor tom: Yesterday's Toy Floor Tom (line 2147), the album-era file; You Want More (2304) is a 2023 remix.
                { "Floor tom", { { T::LowShelf,  122.0f,    7.8f, defaultQ },
                                 { T::Bell,      930.0f,   -6.6f, 1.7f },
                                 { T::Bell,      4070.0f,   5.9f, 1.0f },
                                 { T::HighShelf, 10320.0f,  6.8f, defaultQ } },
                  -4.0f, "Tony's floor-tom EQ from Between You and Me. Ease the 10 kHz shelf if the cymbals get harsh.", true },
                // Toms bus: Neutron 3 Equalizer, median of Ethereal Engagement (985), Gone from Here (1122),
                // Nothing Left To Give (1864) and You Want More (2322). The 317 Hz cut is in two of the four.
                { "Toms bus", { { T::LowCut,   55.0f,    0.0f, defaultQ, 5 },
                                { T::Bell,     119.0f,  -4.8f, 2.0f },
                                off ({ T::Bell, 317.0f, -8.0f, 1.4f }),
                                { T::HighCut,  10700.0f, 0.0f, defaultQ, 3 } },
                  1.4f, "Tony's toms-bus EQ from the album. The 317 Hz cut is off: turn it on for a tighter bus.", true },
            };

            // ---- EQ-40; append only ----
            // Mid tom and High tom. Tony's source tracks use ONE saved EQ for both: the Rack tom settings
            // come from "Gone from Here" High Tom and "You Want More" Mid Tom and High Tom, and those
            // settings coincide. So these two are the Rack tom curve under two labels, not two invented
            // curves. Rack tom (index 22) stays for sessions that saved it.
            {
                auto mid = list[22];
                mid.name = "Mid tom";
                mid.hint = "Tony's tom EQ from the album; his high tom uses the same settings. Lower the 670 Hz cut if hollow.";
                mid.formerName = nullptr;
                auto high = list[22];
                high.name = "High tom";
                high.hint = "Tony's tom EQ from the album; his mid tom uses the same settings. Lower the 670 Hz cut if hollow.";
                high.formerName = nullptr;
                list.push_back (mid);
                list.push_back (high);
            }
            return list;
            }();
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

    //==========================================================================
    // EQ-40: the menu. Every entry names a stable index, so the list can be regrouped or relabelled
    // without touching saved sessions. Order here is the menu order.
    namespace
    {
        struct MenuItem { int index; const char* group; const char* label; };

        const std::vector<MenuItem>& menuItems()
        {
            static const std::vector<MenuItem> items {
                { 5,  "Drums",   "Drums - Kick" },                          // was "Kick punch"
                { 16, "Drums",   "Drums - Snare" },
                { 25, "Drums",   "Drums - Mid Tom" },
                { 26, "Drums",   "Drums - High Tom" },
                { 23, "Drums",   "Drums - Floor Tom" },
                { 24, "Drums",   "Drums - Toms Bus" },
                { 15, "Drums",   "Drums - Drum Bus" },

                { 11, "Guitars", "Guitars - Acoustic Guitar" },
                { 19, "Guitars", "Guitars - Acoustic Guitar (Full Mix)" },
                { 20, "Guitars", "Guitars - Acoustic Guitar (Pickup)" },
                { 6,  "Guitars", "Guitars - Acoustic Guitar (Bright)" },    // was "Bright acoustic"
                { 12, "Guitars", "Guitars - Electric Guitar" },
                { 13, "Guitars", "Guitars - Electric Guitar Bus" },
                { 14, "Guitars", "Guitars - Bass Guitar" },

                { 8,  "Vocals",  "Vocals - Lead Vocal" },
                { 9,  "Vocals",  "Vocals - Backing Vocals" },
                { 10, "Vocals",  "Vocals - Spoken Word" },
                { 1,  "Vocals",  "Vocals - Quick Clean-up" },               // was "Vocal clean-up"

                { 17, "Effects", "Effects - Reverb Return" },
                { 21, "Effects", "Effects - Abbey Road Reverb" },
                { 4,  "Effects", "Effects - Telephone" },

                { 2,  "General", "General - Low-End Tidy" },                // was "Low-end tidy"
                { 7,  "General", "General - Reduce Mud" },                  // was "De-mud"
                { 3,  "General", "General - Add Air" },                     // was "Air"
            };
            // Not listed: 0 Flat (a new EQ is already flat), 18 Five-zone start (the Frequency Guide
            // teaches the same zones), 22 Rack tom (replaced by Mid Tom and High Tom, same curve).
            return items;
        }

        const MenuItem* findMenuItem (int index)
        {
            for (const auto& m : menuItems())
                if (m.index == index)
                    return &m;
            return nullptr;
        }
    }

    bool PresetManager::isListed (int index)                  { return findMenuItem (index) != nullptr; }

    juce::String PresetManager::getMenuGroup (int index)
    {
        const auto* m = findMenuItem (index);
        return m != nullptr ? juce::String (m->group) : juce::String();
    }

    juce::String PresetManager::getMenuLabel (int index)
    {
        const auto* m = findMenuItem (index);
        return m != nullptr ? juce::String (m->label) : getFactoryName (index);
    }

    std::vector<int> PresetManager::getListedIndices()
    {
        std::vector<int> list;
        for (const auto& m : menuItems())
            list.push_back (m.index);
        return list;
    }

    std::vector<juce::String> PresetManager::getMenuGroups()
    {
        std::vector<juce::String> groups;
        for (const auto& m : menuItems())
            if (std::find (groups.begin(), groups.end(), juce::String (m.group)) == groups.end())
                groups.push_back (m.group);
        return groups;
    }

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
        for (int i : getListedIndices())
            list.push_back ({ getMenuLabel (i), i, {}, getMenuGroup (i) });
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

    bool PresetManager::load (const Entry& given, juce::String& error)
    {
        PresetData data;
        auto e = given;
        if (e.isFactory() && e.factoryIndex < getNumFactoryPresets())
            e.name = getMenuLabel (e.factoryIndex);   // the index is the identity; the label is what we show

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

        // A factory preset is found by its index, which never changes, so a session saved under any
        // earlier name (task 26's rename, or before EQ-40's labels) shows the current label.
        if (current.isFactory())
            current.name = getMenuLabel (current.factoryIndex);

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
