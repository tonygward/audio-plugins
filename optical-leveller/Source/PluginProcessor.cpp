#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Opt1 Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See optical-leveller/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x4f707431 /* 'Opt1' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

//==============================================================================
SongsmithOpticalLevellerProcessor::SongsmithOpticalLevellerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    peakReductionRaw = apvts.getRawParameterValue (Opt::peakReductionId);
    gainRaw          = apvts.getRawParameterValue (Opt::gainId);
    modeRaw          = apvts.getRawParameterValue (Opt::modeId);
    bypassParam      = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Opt::bypassId));
    jassert (peakReductionRaw && gainRaw && modeRaw && bypassParam);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithOpticalLevellerProcessor::createParameterLayout()
{
    using namespace SongsmithOptical;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Opt::peakReductionId, 1 }, "Peak Reduction",
        juce::NormalisableRange<float> (minPeakReduction, maxPeakReduction, 0.1f), Opt::defaultPeakReduction));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Opt::gainId, 1 }, "Gain",
        juce::NormalisableRange<float> (minGainDb, maxGainDb, 0.1f), Opt::defaultGainDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Opt::modeId, 1 }, "Mode", Opt::modeNames(), Opt::defaultMode));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Opt::bypassId, 1 }, "Bypass", false));

    return layout;
}

SongsmithOptical::Params SongsmithOpticalLevellerProcessor::getEngineParams() const noexcept
{
    SongsmithOptical::Params p;
    p.peakReduction = peakReductionRaw->load (std::memory_order_relaxed);
    p.gainDb        = gainRaw->load (std::memory_order_relaxed);
    p.limit         = modeRaw->load (std::memory_order_relaxed) >= 0.5f;
    return p;
}

//==============================================================================
bool SongsmithOpticalLevellerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithOpticalLevellerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    beforeAfter.prepare (sampleRate);
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    engine.setParams (getEngineParams());
    engine.prepare (sampleRate);
    engine.setParams (getEngineParams());
    gainReductionDb.store (0.0f);
    outputMs.store (0.0f);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumOutputChannels()), std::memory_order_relaxed);
    outputMeter.resetLatch (sampleRate);   // a new session or layout starts without a warning
    setLatencySamples (0);
}

void SongsmithOpticalLevellerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;
    const Songsmith::Loudness::BeforeAfterMeter::Scope loudnessScope (beforeAfter, buffer, getTotalNumInputChannels(), getPlayHead());   // Before on entry, After on every way out

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, SongsmithOptical::Engine::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    engine.setParams (getEngineParams());

    float reduction = 0.0f, outputPower = 0.0f, blockPeak = 0.0f;
    const bool bypassedNow = bypassParam->get();

    for (int pos = 0; pos < total;)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[SongsmithOptical::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);   // the dry signal, for bypass
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        engine.process (ch, numCh, num);
        reduction = juce::jmax (reduction, engine.readMaxReductionDb());

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

        for (int c = 0; c < numCh; ++c)
        {
            const float rms = buffer.getRMSLevel (c, pos, num);
            outputPower = juce::jmax (outputPower, rms * rms);
            const float channelPeak = buffer.getMagnitude (c, pos, num);   // after the leveller and Bypass
            blockPeak = juce::jmax (blockPeak, channelPeak);
            outputMeter.pushChannel (c, channelPeak);
        }

        pos += num;
    }

    pushMax (gainReductionDb, bypassedNow ? 0.0f : reduction);
    pushMax (outputMs, outputPower);

    outputMeter.push (blockPeak);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, numOut), std::memory_order_relaxed);
    outputMeter.updateClipLatch (blockPeak, total);
}

//==============================================================================
void SongsmithOpticalLevellerProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    presets.loadFactory (index);
}

const juce::String SongsmithOpticalLevellerProcessor::getProgramName (int index)
{
    const auto& list = Opt::Presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* SongsmithOpticalLevellerProcessor::createEditor()
{
    return new SongsmithOpticalLevellerEditor (*this);
}

juce::ValueTree SongsmithOpticalLevellerProcessor::createState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("meterMode", meterMode.load(), nullptr);
    ui.setProperty ("preset", presets.currentName, nullptr);
    ui.setProperty ("presetHint", presets.currentHint, nullptr);
    state.appendChild (ui, nullptr);
    return state;
}

void SongsmithOpticalLevellerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithOpticalLevellerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithOpticalLevellerProcessor::restoreFromValueTree (const juce::ValueTree& saved)
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
    editorWidth.store (Opt::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", Opt::EditorLayout::defaultWidth)
                                                                       : Opt::EditorLayout::defaultWidth));
    meterMode.store (juce::jlimit (0, 1, ui.isValid() ? (int) ui.getProperty ("meterMode", 0) : 0));
    presets.currentName = ui.isValid() ? ui.getProperty ("preset", juce::String()).toString() : juce::String();
    presets.currentHint = ui.isValid() ? ui.getProperty ("presetHint", juce::String()).toString() : juce::String();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithOpticalLevellerProcessor();
}
