#include "DeskPresets.h"
#include "DeskParams.h"

namespace Desk
{
    namespace
    {
        juce::File& folderRootOverride()
        {
            static juce::File root;
            return root;
        }
    }

    const std::vector<const char*>& Presets::presetParameterIds()
    {
        static const std::vector<const char*> ids {
            inputTrimId, outputTrimId, driveId, highPassId, lowPassId,
            eqOnId, lfFreqId, lfGainId, lfBellId, lmfFreqId, lmfGainId, lmfQId, hmfFreqId, hmfGainId, hmfQId, hfFreqId, hfGainId, hfBellId,
            dynamicsOnId, afterEqId, compThresholdId, compRatioId, compAttackId, compReleaseId, compKneeId, compMakeupId,
            gateOnId, gateExpanderId, gateThresholdId, gateRangeId, gateAttackId, gateHoldId, gateReleaseId };
        return ids;
    }

    // Starting points named for what they are used for. They are Songsmith starting points, not copies of anyone's settings, and
    // they have not been compared with a real console: they are sensible places to begin and then adjust by ear. Whatever a preset
    // does not list is left at its plain setting, so each one starts from a strip that changes nothing.
    const std::vector<FactoryPreset>& Presets::factory()
    {
        static const std::vector<FactoryPreset> presets {
            { "Lead Vocal Presence and Control",
              "Removes low rumble, takes a little boxiness out of the middle and adds some air. The Gain Reduction meter should move a few decibels on the loudest words.",
              { { highPassId, 90.0f }, { lmfFreqId, 300.0f }, { lmfGainId, -2.0f }, { hmfFreqId, 4000.0f }, { hmfGainId, 2.5f },
                { hfFreqId, 12000.0f }, { hfGainId, 2.0f },
                { compThresholdId, -18.0f }, { compRatioId, 3.0f }, { compAttackId, 10.0f }, { compReleaseId, 0.2f }, { compMakeupId, 3.0f } } },
            { "Kick Drum Punch",
              "Adds weight low down, clears some boxiness and lets the first hit through before the compressor acts. If it sounds thin, turn Low Gain up or raise the Compressor Threshold.",
              { { highPassId, 30.0f }, { lfFreqId, 60.0f }, { lfGainId, 4.0f }, { lfBellId, 1.0f },
                { lmfFreqId, 400.0f }, { lmfGainId, -4.0f }, { lmfQId, 1.5f }, { hmfFreqId, 3500.0f }, { hmfGainId, 3.0f },
                { compThresholdId, -15.0f }, { compRatioId, 4.0f }, { compAttackId, 30.0f }, { compReleaseId, 0.1f }, { compMakeupId, 2.0f } } },
            { "Snare Crack and Control",
              "Brings out the crack and keeps the ring between hits down. Raise the Gate Threshold if the snare is cut off, and lower it if other drums leak through.",
              { { highPassId, 80.0f }, { lfFreqId, 200.0f }, { lfGainId, 2.0f }, { hmfFreqId, 5000.0f }, { hmfGainId, 3.0f },
                { hfFreqId, 10000.0f }, { hfGainId, 2.0f },
                { compThresholdId, -14.0f }, { compRatioId, 4.0f }, { compAttackId, 10.0f }, { compReleaseId, 0.15f }, { compMakeupId, 2.0f },
                { gateOnId, 1.0f }, { gateThresholdId, -40.0f }, { gateRangeId, 20.0f }, { gateAttackId, 1.0f }, { gateHoldId, 0.1f }, { gateReleaseId, 0.2f } } },
            { "Bass Guitar Solid Low End",
              "Firms up the low notes, adds a little warmth and keeps the level even. Listen for the notes sounding evenly loud from the lowest string to the highest.",
              { { highPassId, 35.0f }, { driveId, 0.2f }, { lfFreqId, 80.0f }, { lfGainId, 3.0f }, { lmfFreqId, 800.0f }, { lmfGainId, 2.0f }, { lmfQId, 1.2f },
                { compThresholdId, -20.0f }, { compRatioId, 4.0f }, { compAttackId, 30.0f }, { compReleaseId, 0.3f }, { compMakeupId, 4.0f },
                { lowPassId, 8000.0f } } },
            { "Acoustic Guitar Clarity",
              "Takes some boom out of the body of the guitar and brightens the strings a little. Use the High Gain control to taste.",
              { { highPassId, 80.0f }, { lmfFreqId, 250.0f }, { lmfGainId, -2.5f }, { hfFreqId, 10000.0f }, { hfGainId, 2.5f },
                { compThresholdId, -20.0f }, { compRatioId, 2.0f }, { compAttackId, 20.0f }, { compReleaseId, 0.4f }, { compMakeupId, 2.0f } } },
            { "Drum Room Cleanup",
              "Turns the room microphone down gently between hits, and trims the rumble and hiss at either end. Listen to the tails of the cymbals, and lower the Range if they are cut short.",
              { { highPassId, 60.0f }, { lowPassId, 12000.0f },
                { gateOnId, 1.0f }, { gateExpanderId, 1.0f }, { gateThresholdId, -45.0f }, { gateRangeId, 12.0f },
                { gateAttackId, 2.0f }, { gateHoldId, 0.15f }, { gateReleaseId, 0.4f } } },
            { "Mix Bus Gentle Glue",
              "A very light squeeze that helps a whole mix sit together. The Gain Reduction meter should barely move. If it never moves, nothing is being changed.",
              { { afterEqId, 1.0f }, { hfFreqId, 12000.0f }, { hfGainId, 1.0f },
                { compThresholdId, -14.0f }, { compRatioId, 2.0f }, { compAttackId, 30.0f }, { compReleaseId, 0.3f }, { compMakeupId, 2.0f } } },
            { "Warm Saturation",
              "Adds a soft, warm edge by gently rounding off the peaks. Compare with Bypass at the same listening level, and turn Drive down if it sounds fuzzy.",
              { { driveId, 0.45f }, { lowPassId, 16000.0f }, { lfFreqId, 100.0f }, { lfGainId, 1.5f } } },
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

        // Start from the plain strip, then apply what the starting point lists.
        for (auto* id : presetParameterIds())
        {
            auto* p = apvts.getParameter (id);
            const float def = p->convertFrom0to1 (p->getDefaultValue());
            float value = def;
            for (const auto& s : f.settings)
                if (juce::String (s.first) == id)
                    value = s.second;
            setParam (id, value);
        }
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
        for (auto* id : presetParameterIds())
        {
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
        for (auto* id : presetParameterIds())
            xml.setAttribute (id, (double) apvts.getRawParameterValue (id)->load());

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
