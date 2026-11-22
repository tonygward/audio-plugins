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
    oversamplingRaw  = apvts.getRawParameterValue (Opt::oversamplingId);
    bypassParam      = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Opt::bypassId));
    jassert (peakReductionRaw && gainRaw && modeRaw && oversamplingRaw && bypassParam);

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

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Opt::oversamplingId, 2 }, "Oversampling", Opt::oversamplingNames(), Opt::defaultOversampling));

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

    levelCapture.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    baseRate = sampleRate;
    using OS = juce::dsp::Oversampling<float>;
    for (size_t k = 1; k < oversamplers.size(); ++k)
    {
        if (oversamplers[k] == nullptr)
            oversamplers[k] = std::make_unique<OS> ((size_t) SongsmithOptical::Engine::maxChannels, k, OS::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[k]->initProcessing ((size_t) dryBuffer.getNumSamples());
        oversamplers[k]->reset();
        osLatency[k] = juce::roundToInt (oversamplers[k]->getLatencyInSamples());
    }
    dryRing.setSize (2, juce::jmax (osLatency[1], osLatency[2]) + 1, false, true, true);
    pathFade.reset (sampleRate, pathFadeSeconds);
    pathFade.setCurrentAndTargetValue (1.0f);
    switchPath (getOversamplingIndex());
    engine.setParams (getEngineParams());
    gainReductionDb.store (0.0f);
    outputMs.store (0.0f);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumOutputChannels()), std::memory_order_relaxed);
    outputMeter.resetLatch (sampleRate);   // a new session or layout starts without a warning
    updateLatencyNow();
    startTimerHz (10);   // reports a changed latency to the host from the message thread, never the audio thread
}

SongsmithOpticalLevellerProcessor::~SongsmithOpticalLevellerProcessor()
{
    stopTimer();
}

void SongsmithOpticalLevellerProcessor::updateLatencyNow()
{
    const int latency = osLatency[(size_t) getOversamplingIndex()];
    if (getLatencySamples() != latency)
        setLatencySamples (latency);   // told to the host whenever it changes
}

// Switches the engine to the rate of an Oversampling setting. Allocation-free: only coefficients are re-derived.
void SongsmithOpticalLevellerProcessor::switchPath (int index) noexcept
{
    engine.prepare (baseRate * (double) (1 << index));
    engine.setParams (getEngineParams());
    if (index > 0)
        oversamplers[(size_t) index]->reset();
    dryRing.clear();
    dryRingPos = 0;
    osActive = index;
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

    levelCapture.push (buffer.getArrayOfReadPointers(), numCh, total);   // Set Levels listens to the dry input; a plain copy

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    engine.setParams (getEngineParams());

    float reduction = 0.0f, outputPower = 0.0f, blockPeak = 0.0f;
    const bool bypassedNow = bypassParam->get();

    for (int pos = 0; pos < total;)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[SongsmithOptical::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
            ch[c] = buffer.getWritePointer (c, pos);

        // Oversampling change: fade out, switch once silent, fade back in (no clicks, nothing allocated).
        const int wantOS = getOversamplingIndex();
        if (wantOS != osActive)
        {
            if (! pathFade.isSmoothing() && pathFade.getCurrentValue() <= 0.0f)
            {
                switchPath (wantOS);
                pathFade.setTargetValue (1.0f);
            }
            else
            {
                pathFade.setTargetValue (0.0f);
            }
        }

        // The dry signal for Bypass, delayed by the path's latency so it lines up with the processed one.
        const int latency = osLatency[(size_t) osActive];
        for (int c = 0; c < numCh; ++c)
        {
            auto* d = dryBuffer.getWritePointer (c);
            if (latency == 0)
            {
                juce::FloatVectorOperations::copy (d, ch[c], num);
                continue;
            }
            auto* ring = dryRing.getWritePointer (c);
            const int size = dryRing.getNumSamples();
            int w = dryRingPos;
            for (int i = 0; i < num; ++i)
            {
                d[i] = ring[(w - latency + size) % size];
                ring[w] = ch[c][i];
                w = (w + 1) % size;
            }
        }
        if (latency > 0)
            dryRingPos = (dryRingPos + num) % dryRing.getNumSamples();

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        if (osActive == 0)
        {
            engine.process (ch, numCh, num);
        }
        else
        {
            auto& os = *oversamplers[(size_t) osActive];
            juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) num);
            auto up = os.processSamplesUp (block);
            float* upCh[SongsmithOptical::Engine::maxChannels] = {};
            for (int c = 0; c < numCh; ++c)
                upCh[c] = up.getChannelPointer ((size_t) c);
            engine.process (upCh, numCh, (int) up.getNumSamples());
            os.processSamplesDown (block);
        }
        reduction = juce::jmax (reduction, engine.readMaxReductionDb());

        if (pathFade.isSmoothing() || pathFade.getTargetValue() < 1.0f)
            for (int i = 0; i < num; ++i)
            {
                const float g = pathFade.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] *= g;
            }

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
