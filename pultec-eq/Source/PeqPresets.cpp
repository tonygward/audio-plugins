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
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  0, 4 },

            // PEQ-05: nine use-case starting points across the three modes. Songsmith starting points from general practice,
            // tuned against the engine's own response (see docs/presets.md); not listened to level-matched yet.
            { "Punchy Kick",                  2, 9.5f, 3.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Gives a kick drum more punch and weight at 60 Hz, with a Low Cut that keeps the low middle from turning boomy. Listen for a firmer thump. "
              "If the kick sounds hollow, lower Low Cut. It also makes the sound louder, so compare at the same level." },
            { "Deep, Tight Bass",             1, 9.0f, 3.5f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Adds deep weight to a bass at 30 Hz and tightens the notes so they stop ringing. Listen for low notes that are solid and clean. "
              "If you want more punch and less depth, choose 60 Hz instead. It also makes the sound louder, so compare at the same level." },
            { "Mix Bus Air",                  1, 0.0f, 0.0f,   6, 2.0f, 1.5f,   1, 0.0f,
              "A small, broad lift of the very top of a whole mix at 16 kHz. Listen for a little more openness and shine. "
              "If nothing seems to change at the same level, nothing needs to change." },
            { "Bass Definition",              2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Turns down the muddy area around 300 Hz and lifts 1 kHz, so each bass note can be heard more clearly on small speakers. Listen for the notes sounding more distinct. "
              "If the bass turns thin or clicky, lower Low-Mid Boost. It also makes the sound louder, so compare at the same level.",
              1,  4, 3.0f,  1, 4.0f,  2, 0.0f,  0, 0 },
            { "Clear Lead Vocal",             2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Takes a little thickness out of a lead vocal at 300 Hz and adds a small lift at 3 kHz so the words come through. Listen for a clearer, closer voice. "
              "If it sounds harsh, lower High-Mid Boost. It also makes the sound louder, so compare at the same level.",
              1,  2, 0.0f,  1, 2.0f,  2, 2.5f,  0, 0 },
            { "More Attack on a Soft-Sounding Part", 2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Helps a dull or soft part cut through: less woolliness at 500 Hz, a small lift at 700 Hz and a lift at 3 kHz for the start of each note. Listen for crisper, more defined notes. "
              "If it sounds sharp or tiring, lower High-Mid Boost. It also makes the sound louder, so compare at the same level.",
              1,  3, 2.0f,  2, 4.5f,  2, 3.0f,  0, 0 },
            { "Rumble Cleanup",               2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Removes the very low rumble from stands, footsteps and handling noise with a high-pass filter at 50 Hz. Listen for a tidier low end with the notes untouched. "
              "If a bass or kick loses weight, choose a lower frequency or Off. For a stronger clean-up, choose 80 Hz.",
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  2, 0 },
            { "Tighter Rhythm Guitar",        2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "Keeps a rhythm guitar out of the way of the bass and the cymbals: a high-pass filter at 150 Hz and a low-pass filter at 5 kHz. Listen for a narrower, tighter guitar that sits in the middle. "
              "If it sounds thin or dull on its own, lower the filters. That is normal, because it is meant to sit inside the mix.",
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  5, 2 },
            { "Telephone Voice or Guitar",    2, 0.0f, 0.0f,   3, 0.0f, 5.0f,   1, 0.0f,
              "A deliberately thin, small sound, like an old telephone or a cheap radio: a high-pass filter at 500 Hz and a low-pass filter at 3 kHz. Listen for the body and the shine disappearing. "
              "If it is too extreme, choose lower and higher frequencies. Use it as an effect, not on a whole mix.",
              2,  2, 0.0f,  4, 0.0f,  2, 0.0f,  7, 1 },
        };
        return presets;
    }

    // PEQ-06: which group each starting point is in, and the order within a group. Written by name so adding or reordering a
    // starting point can never silently move another one (the test in PassiveEQTests checks every starting point is in
    // exactly one group). Each is in one group only, even when it could suit two: "Telephone Voice or Guitar" is under
    // Vocals, and "More Attack on a Soft-Sounding Part" under Utility because it is not tied to one instrument.
    const std::vector<FactoryCategory>& Presets::categories()
    {
        static const std::vector<FactoryCategory> groups = []
        {
            struct Named { const char* group; std::vector<const char*> names; };
            const std::vector<Named> layout {
                { "Drums",                 { "Punchy Kick", "Kick Drum Weight" } },
                { "Bass",                  { "Deep, Tight Bass", "Bass Weight", "Bass Definition" } },
                { "Guitar",                { "Acoustic Guitar Brightness", "Tighter Rhythm Guitar" } },
                { "Vocals",                { "Clear Lead Vocal", "Lead Vocal Presence", "Lead Vocal Air", "Telephone Voice or Guitar" } },
                { "Mix Bus and Mastering", { "Mix Bus Air", "Mix Bus Polish" } },
                { "Utility",               { "Rumble Cleanup", "Clear Low Rumble", "Reduce Boxy Mids", "Soften Harsh Highs",
                                             "Soften Top Hiss", "More Attack on a Soft-Sounding Part" } },
            };
            std::vector<FactoryCategory> result;
            const auto& all = factory();
            for (auto& named : layout)
            {
                FactoryCategory group { named.group, {} };
                for (auto* name : named.names)
                    for (int i = 0; i < (int) all.size(); ++i)
                        if (juce::String (all[(size_t) i].name) == juce::String (name))
                            group.presets.push_back (i);
                result.push_back (std::move (group));
            }
            return result;
        }();
        return groups;
    }

    juce::String Presets::categoryOf (int factoryIndex)
    {
        for (auto& group : categories())
            for (int index : group.presets)
                if (index == factoryIndex)
                    return group.name;
        return {};
    }

    std::vector<FactoryCategory> Presets::withoutEmptyCategories (const std::vector<FactoryCategory>& groups)
    {
        std::vector<FactoryCategory> kept;
        for (auto& group : groups)
            if (! group.presets.empty())
                kept.push_back (group);
        return kept;
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
        // Missing values load as defaults, like an old session; out-of-range values are clamped. A file written before
        // the filter steps grew (no "version" 2) stores the old filter positions, so those are moved to the same frequency.
        const bool oldFilterSteps = xml->getIntAttribute ("version", 1) < 2;
        for (int i = 0; i < numPresetParameters; ++i)
        {
            const char* id = presetParameterIds[i];
            auto* p = apvts.getParameter (id);
            const float def = p->convertFrom0to1 (p->getDefaultValue());
            double v = xml->getDoubleAttribute (id, def);
            if (std::isfinite (v) && oldFilterSteps && xml->hasAttribute (id))
            {
                if (juce::String (id) == highPassFrequencyId) v = migratedHighPassIndex ((int) std::lround (v));
                if (juce::String (id) == lowPassFrequencyId)  v = migratedLowPassIndex ((int) std::lround (v));
            }
            setParam (id, std::isfinite (v) ? (float) v : def);
        }
        currentName = file.getFileNameWithoutExtension();
        currentHint = {};
        return true;
    }

    bool Presets::isFactoryName (const juce::String& name)
    {
        // A starting point's name may hold a character a file name cannot (the comma in "Deep, Tight Bass"), so both sides are cleaned.
        const auto clean = juce::File::createLegalFileName (name.trim());
        for (const auto& f : factory())
            if (clean.equalsIgnoreCase (f.name) || clean.equalsIgnoreCase (juce::File::createLegalFileName (f.name)))
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
        xml.setAttribute ("version", 2);   // 2: the filter positions follow the PEQ-05 steps
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
