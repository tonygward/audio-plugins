#include "BusPresets.h"
#include "BusParams.h"

namespace BusComp
{
    namespace
    {
        juce::File& folderRootOverride()
        {
            static juce::File root;
            return root;
        }
    }

    const char* const Presets::presetParameterIds[] = { thresholdId, ratioId, attackId, releaseId, makeupId };
    const int Presets::numPresetParameters = (int) std::size (presetParameterIds);

    // Conservative starting points, named for what they are used for. Make-up Gain is set so a steady peak level of
    // -12 dBFS comes out about as loud as it went in (a test checks it), so switching the plugin in and out compares
    // the control, not the loudness. They are Songsmith starting points, not copies of anyone's settings; the research
    // behind them and the test for each are in bus-compressor/docs/presets.md.
    // Steps: Ratio 0/1/2 = 2:1/4:1/10:1; Attack 0..5 = 0.1/0.3/1/3/10/30 ms; Release 0..4 = 0.1/0.3/0.6/1.2 s/Auto.
    const std::vector<FactoryPreset>& Presets::factory()
    {
        static const std::vector<FactoryPreset> presets {
            { "Mix Bus Glue", 2.0f, 0, 5, 0, 2.0f,
              "Holds a whole mix together. The meter should move about two decibels on the loudest parts. Lower Threshold for more, or raise it if the mix loses its life." },
            { "Finished Mix Gentle Glue", 4.0f, 0, 5, 4, 1.0f,
              "A very light touch for a finished mix. The meter should barely move. If it never moves, nothing is being changed." },
            { "Drum Bus Punch", 0.0f, 1, 5, 1, 4.5f,
              "The slow Attack lets each hit through before the compressor acts, so drums keep their punch. Listen for the room swelling back between hits." },
            { "Drum Bus Heavy Control", -1.0f, 2, 4, 0, 6.0f,
              "Firm control for drums that are too uneven. Listen for pumping, and use a slower Release or a lower Ratio if it is too obvious." },
            { "Vocal Group Glue", 0.0f, 0, 3, 1, 3.0f,
              "Blends a group of backing vocals so they sit together. The meter should move a few decibels. Raise Threshold if they sound flat." },
        };
        return presets;
    }

    juce::File Presets::getDefaultUserFolder()
    {
        const auto base = folderRootOverride() != juce::File()
                              ? folderRootOverride()
                              : juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        return base.getChildFile ("Songsmith Studio").getChildFile (JucePlugin_Name).getChildFile ("Presets");
    }

    void Presets::setDefaultFolderRootForTesting (const juce::File& root)
    {
        folderRootOverride() = root;
    }

    juce::Array<juce::File> Presets::getUserPresetFiles() const
    {
        auto files = userFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
        files.sort();
        return files;
    }

    void Presets::setParam (const char* id, float plainValue)
    {
        auto* p = apvts.getParameter (id);
        const float v = p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (plainValue));
        if (std::abs (p->getValue() - v) < 1.0e-7f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }

    void Presets::loadFactory (int index)
    {
        const auto& list = factory();
        if (index < 0 || index >= (int) list.size())
            return;
        const auto& f = list[(size_t) index];
        setParam (thresholdId, f.threshold);
        setParam (ratioId, (float) f.ratio);
        setParam (attackId, (float) f.attack);
        setParam (releaseId, (float) f.release);
        setParam (makeupId, f.makeupDb);
        currentName = f.name;
        currentHint = f.hint;
    }

    bool Presets::loadFile (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile() || file.getSize() > 1024 * 1024)
        {
            error = "\"" + file.getFileName() + "\" isn't a " + JucePlugin_Name + " preset.";
            return false;
        }
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (rootTag))
        {
            error = "\"" + file.getFileName() + "\" isn't a " + JucePlugin_Name + " preset.";
            return false;
        }
        // Missing values load as defaults, like an old session; out-of-range values are clamped.
        for (int i = 0; i < numPresetParameters; ++i)
        {
            const char* id = presetParameterIds[i];
            auto* p = apvts.getParameter (id);
            const float def = p->convertFrom0to1 (p->getDefaultValue());
            const double v = xml->getDoubleAttribute (id, def);
            setParam (id, std::isfinite (v) ? (float) v : def);
        }
        currentName = file.getFileNameWithoutExtension();
        currentHint = {};
        return true;
    }

    bool Presets::isFactoryName (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        for (const auto& f : factory())
            if (clean.equalsIgnoreCase (f.name))
                return true;
        return false;
    }

    bool Presets::userPresetExists (const juce::String& name) const
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        return clean.isNotEmpty() && userFolder.getChildFile (clean + fileExtension).existsAsFile();
    }

    bool Presets::saveAs (const juce::String& name, juce::String& error)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty())
        {
            error = "Please type a name for the preset.";
            return false;
        }
        if (isFactoryName (clean))
        {
            error = "\"" + clean + "\" is a starting point, and starting points can't be changed. Please choose a different name.";
            return false;
        }
        if (! userFolder.createDirectory())
        {
            error = "Couldn't create the presets folder: " + userFolder.getFullPathName();
            return false;
        }

        juce::XmlElement xml (rootTag);
        xml.setAttribute ("version", 1);
        for (int i = 0; i < numPresetParameters; ++i)
            xml.setAttribute (presetParameterIds[i], (double) apvts.getRawParameterValue (presetParameterIds[i])->load());

        const auto file = userFolder.getChildFile (clean + fileExtension);
        if (! xml.writeTo (file))
        {
            error = "Couldn't write " + file.getFullPathName();
            return false;
        }
        currentName = clean;
        currentHint = {};
        return true;
    }
}
