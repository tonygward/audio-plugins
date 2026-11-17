#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Peq1 Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See pultec-eq/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x50657131 /* 'Peq1' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

//==============================================================================
SongsmithPassiveEQProcessor::SongsmithPassiveEQProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    lowFrequencyRaw       = apvts.getRawParameterValue (Peq::lowFrequencyId);
    lowBoostRaw           = apvts.getRawParameterValue (Peq::lowBoostId);
    lowCutRaw             = apvts.getRawParameterValue (Peq::lowCutId);
    highBoostFrequencyRaw = apvts.getRawParameterValue (Peq::highBoostFrequencyId);
    highBoostRaw          = apvts.getRawParameterValue (Peq::highBoostId);
    highBandwidthRaw      = apvts.getRawParameterValue (Peq::highBandwidthId);
    highCutFrequencyRaw   = apvts.getRawParameterValue (Peq::highCutFrequencyId);
    highCutRaw            = apvts.getRawParameterValue (Peq::highCutId);
    bypassParam           = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Peq::bypassId));
    jassert (lowFrequencyRaw && lowBoostRaw && lowCutRaw && highBoostFrequencyRaw && highBoostRaw
             && highBandwidthRaw && highCutFrequencyRaw && highCutRaw && bypassParam);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithPassiveEQProcessor::createParameterLayout()
{
    using namespace SongsmithPassive;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto gain = [&layout] (const char* id, const char* name, float maxDb)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, maxDb, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    };

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Peq::lowFrequencyId, 1 }, "Low Frequency", Peq::frequencyNames (lowFrequencies), Peq::defaultLowFrequency));
    gain (Peq::lowBoostId, "Low Boost", maxLowBoostDb);
    gain (Peq::lowCutId, "Low Cut", maxLowCutDb);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Peq::highBoostFrequencyId, 1 }, "High Boost Frequency", Peq::frequencyNames (highBoostFrequencies),
        Peq::defaultHighBoostFrequency));
    gain (Peq::highBoostId, "High Boost", maxHighBoostDb);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Peq::highBandwidthId, 1 }, "High Boost Bandwidth",
        juce::NormalisableRange<float> (0.0f, Peq::maxBandwidth, 0.1f), Peq::defaultBandwidth));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Peq::highCutFrequencyId, 1 }, "High Cut Frequency", Peq::frequencyNames (highCutFrequencies),
        Peq::defaultHighCutFrequency));
    gain (Peq::highCutId, "High Cut", maxHighCutDb);

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Peq::bypassId, 1 }, "Bypass", false));

    return layout;
}

SongsmithPassive::Params SongsmithPassiveEQProcessor::getEngineParams() const noexcept
{
    SongsmithPassive::Params p;
    p.lowFrequencyIndex       = juce::roundToInt (lowFrequencyRaw->load (std::memory_order_relaxed));
    p.lowBoostDb              = lowBoostRaw->load (std::memory_order_relaxed);
    p.lowCutDb                = lowCutRaw->load (std::memory_order_relaxed);
    p.highBoostFrequencyIndex = juce::roundToInt (highBoostFrequencyRaw->load (std::memory_order_relaxed));
    p.highBoostDb             = highBoostRaw->load (std::memory_order_relaxed);
    p.bandwidth               = highBandwidthRaw->load (std::memory_order_relaxed) / Peq::maxBandwidth;
    p.highCutFrequencyIndex   = juce::roundToInt (highCutFrequencyRaw->load (std::memory_order_relaxed));
    p.highCutDb               = highCutRaw->load (std::memory_order_relaxed);
    return p;
}

//==============================================================================
bool SongsmithPassiveEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithPassiveEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    engine.setParams (getEngineParams());
    engine.prepare (sampleRate);
    engine.setParams (getEngineParams());
    setLatencySamples (0);
}

void SongsmithPassiveEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, SongsmithPassive::Engine::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    engine.setParams (getEngineParams());

    for (int pos = 0; pos < total;)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[SongsmithPassive::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);   // the dry signal, for bypass
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        engine.process (ch, numCh, num);

        if (bypassMix.isSmoothing())
        {
            for (int i = 0; i < num; ++i)
            {
                const float m = bypassMix.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] += (dryBuffer.getSample (c, i) - ch[c][i]) * m;
            }
        }
        else if (bypassMix.getTargetValue() > 0.0f)
        {
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::copy (ch[c], dryBuffer.getReadPointer (c), num);   // bit-exact dry
        }

        pos += num;
    }
}

//==============================================================================
void SongsmithPassiveEQProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    presets.loadFactory (index);
}

const juce::String SongsmithPassiveEQProcessor::getProgramName (int index)
{
    const auto& list = Peq::Presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* SongsmithPassiveEQProcessor::createEditor()
{
    return new SongsmithPassiveEQEditor (*this);
}

juce::ValueTree SongsmithPassiveEQProcessor::createState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("preset", presets.currentName, nullptr);
    ui.setProperty ("presetHint", presets.currentHint, nullptr);
    state.appendChild (ui, nullptr);
    return state;
}

void SongsmithPassiveEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithPassiveEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithPassiveEQProcessor::restoreFromValueTree (const juce::ValueTree& saved)
{
    // Rebuild a complete parameter tree: saved values where present, defaults otherwise. (APVTS would
    // otherwise leave missing parameters at their current values.) A state from a newer version keeps
    // the parameters this version knows, by ID, and ignores the rest.
    juce::ValueTree fresh (apvts.state.getType());

    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        const auto child = saved.getChildWithProperty ("id", rp->paramID);
        if (child.isValid() && child.hasProperty ("value"))
        {
            const float v = (float) child.getProperty ("value");
            if (std::isfinite (v))
                value = rp->getNormalisableRange().snapToLegalValue (v);
        }
        fresh.appendChild (juce::ValueTree ("PARAM", { { "id", rp->paramID }, { "value", value } }), nullptr);
    }

    apvts.replaceState (fresh);
    history.clear();   // a loaded session starts with a clean Undo history

    const auto ui = saved.getChildWithName (uiId);
    editorWidth.store (Peq::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", Peq::EditorLayout::defaultWidth)
                                                                       : Peq::EditorLayout::defaultWidth));
    presets.currentName = ui.isValid() ? ui.getProperty ("preset", juce::String()).toString() : juce::String();
    presets.currentHint = ui.isValid() ? ui.getProperty ("presetHint", juce::String()).toString() : juce::String();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithPassiveEQProcessor();
}
