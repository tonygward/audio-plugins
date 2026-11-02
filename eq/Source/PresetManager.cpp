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
        };

        struct FactoryPreset
        {
            const char* name;
            std::vector<FactoryBand> bands;
            float outputGain = 0.0f;
        };

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
    juce::File PresetManager::getDefaultUserFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("TW Pro EQ").getChildFile ("Presets");
    }

    int PresetManager::getNumFactoryPresets()                 { return (int) factoryPresets().size(); }
    juce::String PresetManager::getFactoryName (int index)    { return factoryPresets()[(size_t) index].name; }
    int PresetManager::getFactoryUsedBands (int index)        { return (int) factoryPresets()[(size_t) index].bands.size(); }

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
            s.enabled = true;
            s.type = (int) f.bands[i].type;
            s.freq = f.bands[i].freq;
            s.gain = typeHasGain (f.bands[i].type) ? f.bands[i].gain : 0.0f;
            s.q = f.bands[i].q;
            s.slope = f.bands[i].slope;
        }
        d.outputGain = f.outputGain;
        return d;
    }

    //==========================================================================
    std::vector<PresetManager::Entry> PresetManager::getUserPresets() const
    {
        std::vector<Entry> list;
        if (! userFolder.isDirectory())
            return list;

        auto files = userFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
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
            error = "This file is not a TW Pro EQ preset.";
            return false;
        }

        // Exactly the session restore path (defaults for missing parameters, v1 -> v2 band
        // slots, ...), applied to a scratch instance, so presets migrate like sessions.
        ThreeBandEQAudioProcessor scratch;
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

        if (! userFolder.isDirectory() && ! userFolder.createDirectory())
        {
            error = "Could not create the preset folder:\n" + userFolder.getFullPathName();
            return false;
        }

        const auto file = userFolder.getChildFile (name + fileExtension);
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
                ThreeBandEQAudioProcessor scratch;
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
            current.file = juce::File (path);
    }
}
