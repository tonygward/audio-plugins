#include "FETPresets.h"
#include "FETParams.h"

namespace FET
{
    namespace
    {
        juce::File& folderRootOverride()
        {
            static juce::File root;
            return root;
        }
    }

    const char* const Presets::presetParameterIds[] = { ID::input, ID::output, ID::attack, ID::release,
                                                        ID::ratio, ID::allButtons, ID::grOff };
    const int Presets::numPresetParameters = (int) std::size (presetParameterIds);

    // Conservative starting points. Output is set so a -18 dBFS RMS signal comes out at about
    // the level it went in (tested), so switching the plugin in and out compares tone, not
    // loudness. The control directions follow the UA manual: Input sets the amount of
    // compression into a fixed threshold, Attack and Release get faster clockwise, Output only
    // matches the level (UM pp. 8-9, p. 20).
    const std::vector<FactoryPreset>& Presets::factory()
    {
        static const std::vector<FactoryPreset> presets {
            { "Lead vocal", 1.0f, 3.7f, 3.0f, 5.0f, 0, false,
              "4:1, about 3-5 dB on loud lines. Raise Input for more control; turn Attack down (counter-clockwise) if consonants dull." },
            { "Acoustic guitar", 2.0f, 3.3f, 2.0f, 6.0f, 0, false,
              "4:1 with a slower attack so each pick comes through. Listen for the strum to sit steady without the attack going soft." },
            { "Bass", 3.0f, 2.4f, 3.0f, 3.0f, 1, false,
              "8:1 with a moderate release so low notes don't distort. Each note should come out even; if it growls, slow the Release." },
            { "Drum room", 6.0f, 1.4f, 2.0f, 7.0f, 1, false,
              "8:1, slow attack, fastest release: hits punch through and the room swells between them. For more, try All." },
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
        setParam (ID::input, f.input);
        setParam (ID::output, f.output);
        setParam (ID::attack, f.attack);
        setParam (ID::release, f.release);
        setParam (ID::ratio, (float) f.ratio);
        setParam (ID::allButtons, f.allButtons ? 1.0f : 0.0f);
        setParam (ID::grOff, 0.0f);
        currentName = f.name;
        currentHint = f.hint;
    }

    bool Presets::loadFile (const juce::File& file, juce::String& error)
    {
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
            setParam (id, (float) xml->getDoubleAttribute (id, def));
        }
        currentName = file.getFileNameWithoutExtension();
        currentHint = {};
        return true;
    }

    bool Presets::saveAs (const juce::String& name, juce::String& error)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty())
        {
            error = "Please type a name for the preset.";
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
