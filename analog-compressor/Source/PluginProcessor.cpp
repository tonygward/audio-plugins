#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace FET;

namespace
{
    // Exactly 1.0f at 0 dB, so default settings stay bit-exact. The parameter value can come
    // back from the normalised range as a few 1e-7 dB rather than 0 (it differs by platform), so
    // anything within half a step (0.01 dB) of 0 is unity.
    float dbToGain (float db) noexcept { return std::abs (db) < 0.005f ? 1.0f : juce::Decibels::decibelsToGain (db); }
}

SongsmithAnalogCompressorProcessor::SongsmithAnalogCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SongsmithAnalogCompressor", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ID::bypass));
    inputDb  = apvts.getRawParameterValue (ID::input);
    outputDb = apvts.getRawParameterValue (ID::output);
    attackKnob = apvts.getRawParameterValue (ID::attack);
    releaseKnob = apvts.getRawParameterValue (ID::release);
    ratioIndex = apvts.getRawParameterValue (ID::ratio);
    grOffParam = apvts.getRawParameterValue (ID::grOff);
    oversamplingParam = apvts.getRawParameterValue (ID::oversampling);
    allButtonsParam = apvts.getRawParameterValue (ID::allButtons);
    jassert (bypassParam != nullptr && inputDb != nullptr && outputDb != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithAnalogCompressorProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const auto v = [] (const char* id) { return ParameterID { id, parameterVersion }; };

    const auto dbText = [] (float value, int) { return String (value, 1) + " dB"; };
    const auto knobText = [] (float value, int) { return String (value, 1); };

    layout.add (std::make_unique<AudioParameterFloat> (
        v (ID::input), "Input", NormalisableRange<float> (inputMinDb, inputMaxDb, 0.01f), inputDefaultDb,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        v (ID::output), "Output", NormalisableRange<float> (outputMinDb, outputMaxDb, 0.01f), outputDefaultDb,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        v (ID::attack), "Attack", NormalisableRange<float> (knobMin, knobMax, 0.01f), attackDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction (knobText)));

    layout.add (std::make_unique<AudioParameterFloat> (
        v (ID::release), "Release", NormalisableRange<float> (knobMin, knobMax, 0.01f), releaseDefault,
        AudioParameterFloatAttributes().withStringFromValueFunction (knobText)));

    layout.add (std::make_unique<AudioParameterChoice> (v (ID::ratio), "Ratio", ratioNames(), ratioDefault));
    layout.add (std::make_unique<AudioParameterBool> (v (ID::allButtons), "All Buttons", false));
    layout.add (std::make_unique<AudioParameterBool> (v (ID::grOff), "Gain Reduction Off", false));
    layout.add (std::make_unique<AudioParameterBool> (v (ID::bypass), "Bypass", false));

    // Added in state version 2.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::oversampling, 2 }, "Oversampling",
                                                        oversamplingNames(), oversamplingDefault));

    return layout;
}

//==============================================================================
bool SongsmithAnalogCompressorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return out == layouts.getMainInputChannelSet();
}

SongsmithAnalogCompressorProcessor::~SongsmithAnalogCompressorProcessor()
{
    stopTimer();
}

void SongsmithAnalogCompressorProcessor::updateLatencyNow()
{
    const int latency = osLatency[(size_t) getOversamplingIndex()];
    if (getLatencySamples() != latency)
        setLatencySamples (latency);
}

void SongsmithAnalogCompressorProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    baseRate = sampleRate;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    // All buffers are sized for one chunk now; processBlock splits host blocks into chunks,
    // so nothing allocates later whatever block size the host sends.
    using OS = juce::dsp::Oversampling<float>;
    for (size_t k = 1; k < oversamplers.size(); ++k)
    {
        if (oversamplers[k] == nullptr)
            oversamplers[k] = std::make_unique<OS> ((size_t) FET::Engine::maxChannels, k, OS::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[k]->initProcessing ((size_t) chunkSize);
        oversamplers[k]->reset();
        osLatency[k] = juce::roundToInt (oversamplers[k]->getLatencyInSamples());
    }

    gains.assign ((size_t) chunkSize * 4, 1.0f);
    dry.setSize (numCh, chunkSize, false, true, true);
    dryRing.setSize (numCh, juce::jmax (osLatency[1], osLatency[2]) + 1, false, true, true);
    dryRing.clear();
    dryRingPos = 0;

    inputGain.reset (sampleRate, gainSmoothingSeconds);
    bypassMix.reset (sampleRate, bypassFadeSeconds);
    pathFade.reset (sampleRate, pathFadeSeconds);
    inputGain.setCurrentAndTargetValue (dbToGain (inputDb->load()));
    outputGain.setCurrentAndTargetValue (dbToGain (outputDb->load()));
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    pathFade.setCurrentAndTargetValue (1.0f);

    switchPath (getOversamplingIndex());
    engine.reset();

    updateLatencyNow();
    startTimerHz (10);
}

void SongsmithAnalogCompressorProcessor::switchPath (int index) noexcept
{
    const double rate = baseRate * (1 << index);
    engine.prepare (rate);          // no allocation: times are in seconds, coefficients re-derive
    outputStage.prepare (rate);
    const float current = outputGain.getCurrentValue();
    outputGain.reset (rate, gainSmoothingSeconds);
    outputGain.setCurrentAndTargetValue (current);
    const float factor = fetFactor.getTargetValue();
    fetFactor.reset (rate, gainSmoothingSeconds);
    fetFactor.setCurrentAndTargetValue (factor);
    if (index > 0)
        oversamplers[(size_t) index]->reset();
    dryRing.clear();
    dryRingPos = 0;
    osActive = index;
}

void SongsmithAnalogCompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels(), FET::Engine::maxChannels);
    const int n = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (n == 0 || numCh == 0)
        return;

    inputGain.setTargetValue (dbToGain (inputDb->load()));
    outputGain.setTargetValue (dbToGain (outputDb->load()));
    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    engine.setRatioIndex ((int) ratioIndex->load());
    engine.setAttackKnob (attackKnob->load());
    engine.setReleaseKnob (releaseKnob->load());
    engine.setGainReductionEnabled (grOffParam->load() < 0.5f);
    engine.setAllButtons (isAllButtonsOn());
    fetFactor.setTargetValue (isAllButtonsOn() ? FET::FetCell::allButtonsFactor : 1.0f);

    // Fully bypassed and settled with no latency to keep: leave the buffer untouched.
    if (! bypassMix.isSmoothing() && bypassMix.getTargetValue() >= 1.0f
        && osActive == 0 && getOversamplingIndex() == 0)
    {
        inputGain.skip (n);
        return;
    }

    for (int pos = 0; pos < n; pos += chunkSize)
    {
        const int num = juce::jmin (chunkSize, n - pos);
        float* ch[FET::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
            ch[c] = buffer.getWritePointer (c, pos);
        processChunk (ch, numCh, num);
    }

    // Output level for the meter (after bypass, so the +4/+8 modes show what leaves the plugin).
    float ms = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        const float rms = buffer.getRMSLevel (c, 0, n);
        ms = juce::jmax (ms, rms * rms);
    }
    float prevMs = outputMs.load (std::memory_order_relaxed);
    while (ms > prevMs && ! outputMs.compare_exchange_weak (prevMs, ms, std::memory_order_relaxed)) {}
}

void SongsmithAnalogCompressorProcessor::processChunk (float* const* ch, int numCh, int num) noexcept
{
    // Oversampling change: fade out, switch once silent, fade back in (no clicks, no alloc).
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

    // Dry signal for the bypass fade, delayed by the path latency so both line up.
    const int latency = osLatency[(size_t) osActive];
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = dry.getWritePointer (c);
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

    // Input drive at the base rate (linear, so it commutes with the oversampler).
    if (inputGain.isSmoothing() || inputGain.getTargetValue() != 1.0f)
        for (int i = 0; i < num; ++i)
        {
            const float g = inputGain.getNextValue();
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= g;
        }

    if (osActive == 0)
    {
        runCore (ch, numCh, num);
    }
    else
    {
        auto& os = *oversamplers[(size_t) osActive];
        juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) num);
        auto up = os.processSamplesUp (block);
        float* upCh[FET::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
            upCh[c] = up.getChannelPointer ((size_t) c);
        runCore (upCh, numCh, (int) up.getNumSamples());
        os.processSamplesDown (block);
    }

    if (pathFade.isSmoothing() || pathFade.getTargetValue() < 1.0f)
        for (int i = 0; i < num; ++i)
        {
            const float g = pathFade.getNextValue();
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= g;
        }

    if (bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f)
        for (int i = 0; i < num; ++i)
        {
            const float m = bypassMix.getNextValue();
            for (int c = 0; c < numCh; ++c)
                ch[c][i] = ch[c][i] * (1.0f - m) + dry.getSample (c, i) * m;
        }
}

// Engine (gain cell + feedback detector), FET colour, Output trim, Class A output stage:
// the hardware order (UG p. 7 Fig. 1), at the running (possibly oversampled) rate.
void SongsmithAnalogCompressorProcessor::runCore (float* const* ch, int numCh, int num) noexcept
{
    engine.process (ch, numCh, num, gains.data());

    const bool colour = colourEnabled;
    const bool trim = outputGain.isSmoothing() || outputGain.getTargetValue() != 1.0f;

    if (! colour && ! trim)
        return;   // clean engine at unity: bit-exact below the threshold

    for (int i = 0; i < num; ++i)
    {
        const float g = trim ? outputGain.getNextValue() : 1.0f;
        const float factor = fetFactor.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            float y = ch[c][i];
            if (colour)
                y = FET::FetCell::process (y, gains[(size_t) i], factor);
            y *= g;   // the Output control drives the output amplifier (UG p. 7)
            if (colour)
                y = outputStage.processSample (c, y);
            ch[c][i] = y;
        }
    }
}

void SongsmithAnalogCompressorProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    presets.loadFactory (index);
}

const juce::String SongsmithAnalogCompressorProcessor::getProgramName (int index)
{
    const auto& list = FET::Presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

//==============================================================================
juce::AudioProcessorEditor* SongsmithAnalogCompressorProcessor::createEditor()
{
    return new SongsmithAnalogCompressorEditor (*this);
}

void SongsmithAnalogCompressorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("uiScale", uiScale.load(), nullptr);
    if (editorWidth.load() > 0)
        ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("meterMode", meterMode.load(), nullptr);
    ui.setProperty ("preset", presets.currentName, nullptr);
    ui.setProperty ("presetHint", presets.currentHint, nullptr);
    state.appendChild (ui, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithAnalogCompressorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithAnalogCompressorProcessor::restoreFromValueTree (const juce::ValueTree& saved)
{
    // Rebuild a complete parameter tree: saved values where present, defaults otherwise
    // (APVTS would otherwise leave missing parameters at their *current* values). Newer
    // versions can migrate here, keyed on saved.getProperty (versionId).
    juce::ValueTree fresh (apvts.state.getType());

    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        const auto child = saved.getChildWithProperty ("id", rp->paramID);

        if (child.isValid() && child.hasProperty ("value"))
            value = rp->getNormalisableRange().snapToLegalValue ((float) child.getProperty ("value"));

        fresh.appendChild (juce::ValueTree ("PARAM", { { "id", rp->paramID }, { "value", value } }), nullptr);
    }

    apvts.replaceState (fresh);

    const auto ui = saved.getChildWithName (uiId);
    uiScale.store (FET::snapUiScale ((float) ui.getProperty ("uiScale", 1.0f)));
    // Sessions saved before drag-resizing have no width: they reopen at their saved interface size.
    editorWidth.store (ui.hasProperty ("editorWidth") ? FET::constrainEditorWidth ((int) ui.getProperty ("editorWidth"))
                                                      : 0);
    meterMode.store (juce::jlimit (0, 2, (int) ui.getProperty ("meterMode", 0)));
    presets.currentName = ui.getProperty ("preset", juce::String()).toString();
    presets.currentHint = ui.getProperty ("presetHint", juce::String()).toString();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithAnalogCompressorProcessor();
}
