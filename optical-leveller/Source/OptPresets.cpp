#include "OptPresets.h"
#include "OptParams.h"

namespace Opt
{
    namespace
    {
        juce::File& folderRootOverride()
        {
            static juce::File root;
            return root;
        }
    }

    const char* const Presets::presetParameterIds[] = { peakReductionId, gainId, modeId };
    const int Presets::numPresetParameters = (int) std::size (presetParameterIds);

    // Conservative starting points, named for what they are used for. Gain is set so a steady -18 dBFS signal
    // comes out about as loud as it went in (a test checks it), so switching the plugin in and out compares the
    // evenness, not the loudness. They are Songsmith starting points, not copies of anyone's settings; the research
    // behind them and the test for each are in optical-leveller/docs/presets.md.
    const std::vector<FactoryPreset>& Presets::factory()
    {
        static const std::vector<FactoryPreset> presets {
            { "Lead Vocal Levelling", 55.0f, 4.0f, false,
              "Keeps a lead vocal even without squashing it. The needle should move a few decibels on the loudest words and settle back between phrases. Lower Peak Reduction if it sounds flat." },
            { "Bass Guitar Levelling", 62.0f, 5.5f, false,
              "Evens out the notes of a bass guitar. Listen for each note holding the same weight. If the notes feel held back, lower Peak Reduction." },
            { "Acoustic Guitar Smoothing", 35.0f, 0.5f, false,
              "Very light smoothing for strummed or picked acoustic guitar. The needle should barely move. If it never moves, nothing is being changed." },
            { "Drum Room Control", 50.0f, 3.5f, true,
              "Limit holds the loudest hits of a drum room in check and brings up the room between them. Listen for pumping and use Compress if it is too obvious." },
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
        setParam (peakReductionId, f.peakReduction);
        setParam (gainId, f.gainDb);
        setParam (modeId, f.limit ? 1.0f : 0.0f);
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
