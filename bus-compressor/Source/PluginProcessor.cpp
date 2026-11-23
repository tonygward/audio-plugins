#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Bus1 Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See bus-compressor/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x42757331 /* 'Bus1' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

//==============================================================================
SongsmithBusCompressorProcessor::SongsmithBusCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdRaw = apvts.getRawParameterValue (BusComp::thresholdId);
    ratioRaw     = apvts.getRawParameterValue (BusComp::ratioId);
    attackRaw    = apvts.getRawParameterValue (BusComp::attackId);
    releaseRaw   = apvts.getRawParameterValue (BusComp::releaseId);
    makeupRaw    = apvts.getRawParameterValue (BusComp::makeupId);
    bypassParam  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (BusComp::bypassId));
    outputTrimRaw = apvts.getRawParameterValue (BusComp::outputTrimId);
    autoGainRaw   = apvts.getRawParameterValue (BusComp::autoGainId);
    jassert (outputTrimRaw && autoGainRaw);
    jassert (thresholdRaw && ratioRaw && attackRaw && releaseRaw && makeupRaw && bypassParam);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithBusCompressorProcessor::createParameterLayout()
{
    using namespace SongsmithBus;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { BusComp::thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (minThresholdDial, maxThresholdDial, 0.5f), BusComp::defaultThreshold,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { BusComp::ratioId, 1 }, "Ratio", BusComp::ratioNames(), BusComp::defaultRatio));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { BusComp::attackId, 1 }, "Attack", BusComp::attackNames(), BusComp::defaultAttack));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { BusComp::releaseId, 1 }, "Release", BusComp::releaseNames(), BusComp::defaultRelease));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { BusComp::makeupId, 1 }, "Make-up Gain",
        juce::NormalisableRange<float> (minMakeupDb, maxMakeupDb, 0.1f), BusComp::defaultMakeupDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { BusComp::bypassId, 1 }, "Bypass", false));

    // Version 2 (songsmithstudio#16): the suite's Output Trim and Auto Gain. 0 dB and off, so a session saved before sounds as it did.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { BusComp::outputTrimId, 2 }, "Output Trim", juce::NormalisableRange<float> (BusComp::minOutputTrimDb, BusComp::maxOutputTrimDb, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { BusComp::autoGainId, 2 }, "Auto Gain", false));

    return layout;
}

SongsmithBus::Params SongsmithBusCompressorProcessor::getEngineParams() const noexcept
{
    SongsmithBus::Params p;
    p.thresholdDial = thresholdRaw->load (std::memory_order_relaxed);
    p.ratioIndex    = juce::roundToInt (ratioRaw->load (std::memory_order_relaxed));
    p.attackIndex   = juce::roundToInt (attackRaw->load (std::memory_order_relaxed));
    p.releaseIndex  = juce::roundToInt (releaseRaw->load (std::memory_order_relaxed));
    p.makeupDb      = makeupRaw->load (std::memory_order_relaxed);
    return p;
}

//==============================================================================
bool SongsmithBusCompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithBusCompressorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    beforeAfter.prepare (sampleRate);
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    outputStage.prepare (sampleRate);
    outputStage.setTrimImmediately (outputTrimRaw->load());
    engine.setParams (getEngineParams());
    engine.prepare (sampleRate);
    engine.setParams (getEngineParams());
    gainReductionDb.store (0.0f);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumOutputChannels()), std::memory_order_relaxed);
    outputMeter.resetLatch (sampleRate);   // a new session or layout starts without a warning
    inputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumInputChannels()), std::memory_order_relaxed);
    inputMeter.resetLatch (sampleRate);
    setLatencySamples (0);
}

void SongsmithBusCompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;
    const Songsmith::Loudness::BeforeAfterMeter::Scope loudnessScope (beforeAfter, buffer, getTotalNumInputChannels(), getPlayHead());   // Before on entry, After on every way out

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, SongsmithBus::Engine::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    engine.setParams (getEngineParams());

    float reduction = 0.0f, blockPeak = 0.0f, inputPeak = 0.0f;
    const bool bypassedNow = bypassParam->get();
    outputStage.beginBlock (isAutoGainOn());

    for (int pos = 0; pos < total;)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[SongsmithBus::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);   // the dry signal, for bypass
            const float arriving = buffer.getMagnitude (c, pos, num);
            inputPeak = juce::jmax (inputPeak, arriving);
            inputMeter.pushChannel (c, arriving);   // what arrives, before the compressor
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        outputStage.measureInput (ch, numCh, num);   // Auto Gain compares what comes out with what went in
        engine.process (ch, numCh, num);
        outputStage.process (ch, numCh, num, outputTrimRaw->load (std::memory_order_relaxed));   // Auto Gain, then Output Trim
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
            const float channelPeak = buffer.getMagnitude (c, pos, num);   // after the compressor, Make-up Gain and Bypass
            blockPeak = juce::jmax (blockPeak, channelPeak);
            outputMeter.pushChannel (c, channelPeak);
        }

        pos += num;
    }

    pushMax (gainReductionDb, bypassedNow ? 0.0f : reduction);

    inputMeter.push (inputPeak);
    inputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, numIn), std::memory_order_relaxed);
    inputMeter.updateClipLatch (inputPeak, total);
    outputMeter.push (blockPeak);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, numOut), std::memory_order_relaxed);
    outputMeter.updateClipLatch (blockPeak, total);
}

//==============================================================================
void SongsmithBusCompressorProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    presets.loadFactory (index);
}

const juce::String SongsmithBusCompressorProcessor::getProgramName (int index)
{
    const auto& list = BusComp::Presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* SongsmithBusCompressorProcessor::createEditor()
{
    return new SongsmithBusCompressorEditor (*this);
}

juce::ValueTree SongsmithBusCompressorProcessor::createState()
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

void SongsmithBusCompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithBusCompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithBusCompressorProcessor::restoreFromValueTree (const juce::ValueTree& saved)
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
    editorWidth.store (BusComp::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", BusComp::EditorLayout::defaultWidth)
                                                                       : BusComp::EditorLayout::defaultWidth));
    presets.currentName = ui.isValid() ? ui.getProperty ("preset", juce::String()).toString() : juce::String();
    presets.currentHint = ui.isValid() ? ui.getProperty ("presetHint", juce::String()).toString() : juce::String();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithBusCompressorProcessor();
}
