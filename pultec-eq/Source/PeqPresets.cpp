#include "PeqPresets.h"
#include "PeqParams.h"

namespace Peq
{
    namespace
    {
        juce::File& folderRootOverride()
        {
            static juce::File root;
            return root;
        }
    }

    // The sound: every frequency and amount in every mode, and the mode. (Bypass and editor state are never part of a preset.)
    // A preset saved before the modes existed has none of the PEQ-04 entries, so it loads as Tone Shaper.
    const char* const Presets::presetParameterIds[] = { lowFrequencyId, lowBoostId, lowCutId, highBoostFrequencyId,
                                                        highBoostId, highBandwidthId, highCutFrequencyId, highCutId,
                                                        modeId, midLowFrequencyId, midLowBoostId, midDipFrequencyId, midDipId,
                                                        midHighFrequencyId, midHighBoostId, highPassFrequencyId, lowPassFrequencyId };
    const int Presets::numPresetParameters = (int) std::size (presetParameterIds);

    // Conservative starting points, named for what they are used for. They are Songsmith starting points, not copies of
    // anyone's settings: the frequencies follow widely published practice for this kind of equaliser (60 Hz for kick
    // drum weight, 20 or 30 Hz for bass weight, a gentle cut at 100 Hz to clear the low end, a broad lift around 8 to
    // 12 kHz for air) and the amounts are small. The research and the test for each are in pultec-eq/docs/presets.md.
    // There is no output level control, so every boost also makes the sound louder; the hints say so.
    const std::vector<FactoryPreset>& Presets::factory()
    {
        static const std::vector<FactoryPreset> presets {
            //  name                          low frequency, boost, cut   high boost frequency, boost, bandwidth   high cut frequency, cut
            { "Bass Weight",                  1, 5.0f, 2.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Adds weight to a bass guitar or synth bass, and the cut keeps the low middle tidy. Listen for fuller low notes that stay clear. "
              "If the bass turns loose or boomy, lower Low Boost. It also makes the sound louder, so compare at the same level." },
            { "Kick Drum Weight",             2, 4.0f, 3.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Adds thump to a kick drum at 60 Hz without making it boomy. Listen for more punch in the low notes. "
              "If the kick sounds hollow, lower Low Cut. It also makes the sound louder, so compare at the same level." },
            { "Lead Vocal Air",               3, 0.0f, 1.5f,   5, 3.5f, 3.0f,   1, 0.0f,
              "A broad, gentle lift of the top of a lead vocal, with a little less low rumble. Listen for the voice sounding more open. "
              "If breaths or hissing sounds become sharp, lower High Boost. It also makes the sound louder, so compare at the same level." },
            { "Acoustic Guitar Brightness",   3, 0.0f, 2.5f,   3, 3.0f, 4.0f,   1, 0.0f,
              "Brightens strummed or picked acoustic guitar and reduces boom from the body. Listen for clearer strings and a less boomy low end. "
              "If it sounds thin, lower Low Cut. It also makes the sound louder, so compare at the same level." },
            { "Mix Bus Polish",               1, 2.0f, 0.0f,   6, 2.5f, 2.0f,   1, 0.0f,
              "A very gentle widening of a full mix: a little more depth and a little more sparkle. Make small changes and listen to the whole mix. "
              "If you cannot hear a difference at the same level, nothing needs to change." },
            { "Soften Harsh Highs",           3, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 3.0f,
              "Turns down the very top of a bright or harsh recording from 10 kHz upwards. Listen for the edge becoming smoother. "
              "If the sound turns dull, lower High Cut." },

            // PEQ-04: starting points for the Mid Shaper and the Frequency Filter. Songsmith starting points, named for their use.
            { "Lead Vocal Presence",          2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Brings a lead vocal forward with a broad lift around 3 kHz and a little less boxiness around 500 Hz. Listen for the words becoming easier to understand. "
              "If it sounds harsh or nasal, lower High-Mid Boost. It also makes the sound louder, so compare at the same level.",
              1,  2, 0.0f,  2, 3.0f,  2, 4.0f,  0, 0 },
            { "Reduce Boxy Mids",             2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Turns down the cardboard-box sound that can build up around 300 to 500 Hz on guitars, drums and vocals. Listen for the sound opening up. "
              "If it sounds thin, lower Mid Dip.",
              1,  1, 0.0f,  1, 4.0f,  2, 0.0f,  0, 0 },
            { "Clear Low Rumble",             2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Removes the very low rumble that is not part of the sound, using a high-pass filter at 80 Hz. Listen for a tidier low end. "
              "If a bass or kick loses weight, choose a lower frequency or Off.",
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  3, 0 },
            { "Soften Top Hiss",    2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Rolls off the very top with a low-pass filter at 10 kHz, which tames hiss and a brittle top end. Listen for a smoother, darker sound. "
              "If it sounds dull, choose a higher frequency or Off.",
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  0, 3 },
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
        setParam (lowFrequencyId, (float) f.lowFrequency);
        setParam (lowBoostId, f.lowBoost);
        setParam (lowCutId, f.lowCut);
        setParam (highBoostFrequencyId, (float) f.highBoostFrequency);
        setParam (highBoostId, f.highBoost);
        setParam (highBandwidthId, f.bandwidth);
        setParam (highCutFrequencyId, (float) f.highCutFrequency);
        setParam (highCutId, f.highCut);
        setParam (modeId, (float) f.mode);
        setParam (midLowFrequencyId, (float) f.midLowFrequency);
        setParam (midLowBoostId, f.midLowBoost);
        setParam (midDipFrequencyId, (float) f.midDipFrequency);
        setParam (midDipId, f.midDip);
        setParam (midHighFrequencyId, (float) f.midHighFrequency);
        setParam (midHighBoostId, f.midHighBoost);
        setParam (highPassFrequencyId, (float) f.highPass);
        setParam (lowPassFrequencyId, (float) f.lowPass);
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
