#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Desk Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See analog-desk/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x4465736b /* 'Desk' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

namespace
{
    using Range = juce::NormalisableRange<float>;

    // A range with a comfortable middle: 'mid' sits halfway along the control.
    Range skewed (float lo, float hi, float step, float mid)
    {
        Range r (lo, hi, step);
        r.setSkewForCentre (mid);
        return r;
    }

    juce::String hzText (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
    }

    constexpr float smoothSeconds = 0.02f;
    constexpr int controlChunk = 32;   // samples between filter and equaliser updates
}

//==============================================================================
SongsmithAnalogDeskProcessor::SongsmithAnalogDeskProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    using namespace Desk;
    auto get = [this] (const char* id) { auto* v = apvts.getRawParameterValue (id); jassert (v != nullptr); return v; };
    raw = { get (inputTrimId), get (outputTrimId), get (driveId), get (highPassId), get (lowPassId),
            get (lfFreqId), get (lfGainId), get (lmfFreqId), get (lmfGainId), get (lmfQId), get (hmfFreqId), get (hmfGainId), get (hmfQId),
            get (hfFreqId), get (hfGainId),
            get (compThresholdId), get (compRatioId), get (compAttackId), get (compReleaseId), get (compMakeupId),
            get (gateThresholdId), get (gateRangeId), get (gateAttackId), get (gateHoldId), get (gateReleaseId),
            get (autoGainId),
            get (eqOnId), get (lfBellId), get (hfBellId), get (dynamicsOnId), get (afterEqId), get (compKneeId), get (gateOnId), get (gateExpanderId) };
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (bypassId));
    jassert (bypassParam != nullptr);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithAnalogDeskProcessor::createParameterLayout()
{
    using namespace Desk;
    using namespace SongsmithDesk;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const char* name, Range range, float def, juce::AudioParameterFloatAttributes attr)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attr));
    };
    auto addBool = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };
    auto db = [] { return juce::AudioParameterFloatAttributes().withLabel ("dB"); };
    auto ms = [] { return juce::AudioParameterFloatAttributes().withLabel ("ms"); };
    auto seconds = [] { return juce::AudioParameterFloatAttributes().withLabel ("s"); };
    auto hz = [] { return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return hzText (v); }); };

    addFloat (inputTrimId, "Input Trim", Range (minTrimDb, maxTrimDb, 0.1f), 0.0f, db());
    addFloat (driveId, "Drive", Range (0.0f, 1.0f, 0.01f), 0.0f, juce::AudioParameterFloatAttributes()
                  .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }));
    addFloat (highPassId, "High-Pass Filter", skewed (highPassDialMinHz, maxHighPassHz, 1.0f, 80.0f), highPassDialMinHz,
              juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < highPassOffBelowHz ? juce::String ("Off") : hzText (v); }));
    addFloat (lowPassId, "Low-Pass Filter", skewed (minLowPassHz, lowPassDialMaxHz, 1.0f, 10000.0f), lowPassDialMaxHz,
              juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v > lowPassOffAboveHz ? juce::String ("Off") : hzText (v); }));

    addBool (eqOnId, "Equaliser On", true);
    addFloat (lfFreqId, "Low Frequency", skewed (minLfHz, maxLfHz, 1.0f, 120.0f), defaultLfHz, hz());
    addFloat (lfGainId, "Low Gain", Range (-eqRangeDb, eqRangeDb, 0.1f), 0.0f, db());
    addBool (lfBellId, "Low Bell", false);
    addFloat (lmfFreqId, "Low-Mid Frequency", skewed (minLmfHz, maxLmfHz, 1.0f, 800.0f), defaultLmfHz, hz());
    addFloat (lmfGainId, "Low-Mid Gain", Range (-eqRangeDb, eqRangeDb, 0.1f), 0.0f, db());
    addFloat (lmfQId, "Low-Mid Q", skewed (minQ, maxQ, 0.01f, 1.2f), defaultQ, juce::AudioParameterFloatAttributes());
    addFloat (hmfFreqId, "High-Mid Frequency", skewed (minHmfHz, maxHmfHz, 1.0f, 2500.0f), defaultHmfHz, hz());
    addFloat (hmfGainId, "High-Mid Gain", Range (-eqRangeDb, eqRangeDb, 0.1f), 0.0f, db());
    addFloat (hmfQId, "High-Mid Q", skewed (minQ, maxQ, 0.01f, 1.2f), defaultQ, juce::AudioParameterFloatAttributes());
    addFloat (hfFreqId, "High Frequency", skewed (minHfHz, maxHfHz, 1.0f, 6000.0f), defaultHfHz, hz());
    addFloat (hfGainId, "High Gain", Range (-eqRangeDb, eqRangeDb, 0.1f), 0.0f, db());
    addBool (hfBellId, "High Bell", false);

    addBool (dynamicsOnId, "Dynamics On", true);
    addBool (afterEqId, "Dynamics After Equaliser", false);
    addFloat (compThresholdId, "Compressor Threshold", Range (minThresholdDb, maxThresholdDb, 0.1f), maxThresholdDb, db());
    addFloat (compRatioId, "Compressor Ratio", skewed (minRatio, maxRatio, 0.1f, 4.0f), minRatio, juce::AudioParameterFloatAttributes());
    addFloat (compAttackId, "Compressor Attack", skewed (minAttackMs, maxAttackMs, 0.1f, 3.0f), defaultCompAttackMs, ms());
    addFloat (compReleaseId, "Compressor Release", skewed (minReleaseSeconds, maxReleaseSeconds, 0.01f, 0.4f), defaultCompReleaseSeconds, seconds());
    addBool (compKneeId, "Soft Knee", true);
    addFloat (compMakeupId, "Make-up Gain", Range (0.0f, maxMakeupDb, 0.1f), 0.0f, db());

    addBool (gateOnId, "Gate On", false);
    addBool (gateExpanderId, "Expander", false);
    addFloat (gateThresholdId, "Gate Threshold", Range (minGateThresholdDb, maxGateThresholdDb, 0.1f), defaultGateThresholdDb, db());
    addFloat (gateRangeId, "Gate Range", Range (minGateRangeDb, maxGateRangeDb, 0.1f), defaultGateRangeDb, db());
    addFloat (gateAttackId, "Gate Attack", skewed (minAttackMs, maxAttackMs, 0.1f, 3.0f), defaultGateAttackMs, ms());
    addFloat (gateHoldId, "Gate Hold", Range (minGateHoldSeconds, maxGateHoldSeconds, 0.01f), defaultGateHoldSeconds, seconds());
    addFloat (gateReleaseId, "Gate Release", skewed (minReleaseSeconds, maxReleaseSeconds, 0.01f, 0.4f), defaultGateReleaseSeconds, seconds());

    addFloat (outputTrimId, "Output Trim", Range (minTrimDb, maxTrimDb, 0.1f), 0.0f, db());
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoGainId, 2 }, "Auto Gain", false));
    addBool (bypassId, "Bypass", false);

    return layout;
}

SongsmithDesk::Params SongsmithAnalogDeskProcessor::getEngineParams() const noexcept
{
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto b = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) > 0.5f; };

    SongsmithDesk::Params p;
    p.inputTrimDb = f (raw.inputTrim);
    p.outputTrimDb = 0.0f;   // Output Trim is applied after Auto Gain, by the output stage (songsmithstudio#16)
    p.drive = f (raw.drive);
    p.highPassHz = Desk::highPassFromDial (f (raw.highPass));
    p.lowPassHz = Desk::lowPassFromDial (f (raw.lowPass));
    p.eqOn = b (raw.eqOn);
    p.lfHz = f (raw.lfFreq);   p.lfDb = f (raw.lfGain);   p.lfBell = b (raw.lfBell);
    p.lmfHz = f (raw.lmfFreq); p.lmfDb = f (raw.lmfGain); p.lmfQ = f (raw.lmfQ);
    p.hmfHz = f (raw.hmfFreq); p.hmfDb = f (raw.hmfGain); p.hmfQ = f (raw.hmfQ);
    p.hfHz = f (raw.hfFreq);   p.hfDb = f (raw.hfGain);   p.hfBell = b (raw.hfBell);
    p.dynamicsOn = b (raw.dynamicsOn);
    p.dynamicsAfterEq = b (raw.afterEq);
    p.compThresholdDb = f (raw.compThreshold);
    p.compRatio = f (raw.compRatio);
    p.compAttackMs = f (raw.compAttack);
    p.compReleaseSeconds = f (raw.compRelease);
    p.compSoftKnee = b (raw.compKnee);
    p.compMakeupDb = f (raw.compMakeup);
    p.gateOn = b (raw.gateOn);
    p.gateThresholdDb = f (raw.gateThreshold);
    p.gateRangeDb = f (raw.gateRange);
    p.gateExpander = b (raw.gateExpander);
    p.gateAttackMs = f (raw.gateAttack);
    p.gateHoldSeconds = f (raw.gateHold);
    p.gateReleaseSeconds = f (raw.gateRelease);
    return p;
}

//==============================================================================
bool SongsmithAnalogDeskProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithAnalogDeskProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    beforeAfter.prepare (sampleRate);
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    outputStage.prepare (sampleRate);
    outputStage.setTrimImmediately (raw.outputTrim->load());
    engine.setParams (getEngineParams());
    engine.prepare (sampleRate);
    engine.setParams (getEngineParams());
    smoothersReady = false;
    gainReductionDb.store (0.0f);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumOutputChannels()), std::memory_order_relaxed);
    outputMeter.resetLatch (sampleRate);   // a new session or layout starts without a warning
    setLatencySamples (0);
}

void SongsmithAnalogDeskProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;
    const Songsmith::Loudness::BeforeAfterMeter::Scope loudnessScope (beforeAfter, buffer, getTotalNumInputChannels(), getPlayHead());   // Before on entry, After on every way out

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, SongsmithDesk::Engine::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, juce::jmin (controlChunk, dryBuffer.getNumSamples()));

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    const bool bypassedNow = bypassParam->get();

    // The filters and the equaliser change their coefficients in steps, so their controls are smoothed here and applied
    // every few samples: moving a knob, or automating it, does not click.
    const auto target = getEngineParams();
    if (! smoothersReady)
    {
        for (auto* s : { &sHighPass, &sLowPass, &sLfFreq, &sLfGain, &sLmfFreq, &sLmfGain, &sLmfQ, &sHmfFreq, &sHmfGain, &sHmfQ, &sHfFreq, &sHfGain })
            s->reset (getSampleRate(), smoothSeconds);
        sHighPass.setCurrentAndTargetValue (raw.highPass->load());
        sLowPass.setCurrentAndTargetValue (raw.lowPass->load());
        sLfFreq.setCurrentAndTargetValue (target.lfHz);   sLfGain.setCurrentAndTargetValue (target.lfDb);
        sLmfFreq.setCurrentAndTargetValue (target.lmfHz); sLmfGain.setCurrentAndTargetValue (target.lmfDb); sLmfQ.setCurrentAndTargetValue (target.lmfQ);
        sHmfFreq.setCurrentAndTargetValue (target.hmfHz); sHmfGain.setCurrentAndTargetValue (target.hmfDb); sHmfQ.setCurrentAndTargetValue (target.hmfQ);
        sHfFreq.setCurrentAndTargetValue (target.hfHz);   sHfGain.setCurrentAndTargetValue (target.hfDb);
        smoothersReady = true;
    }
    sHighPass.setTargetValue (raw.highPass->load (std::memory_order_relaxed));
    sLowPass.setTargetValue (raw.lowPass->load (std::memory_order_relaxed));
    sLfFreq.setTargetValue (target.lfHz);   sLfGain.setTargetValue (target.lfDb);
    sLmfFreq.setTargetValue (target.lmfHz); sLmfGain.setTargetValue (target.lmfDb); sLmfQ.setTargetValue (target.lmfQ);
    sHmfFreq.setTargetValue (target.hmfHz); sHmfGain.setTargetValue (target.hmfDb); sHmfQ.setTargetValue (target.hmfQ);
    sHfFreq.setTargetValue (target.hfHz);   sHfGain.setTargetValue (target.hfDb);

    float reduction = 0.0f, blockPeak = 0.0f;
    outputStage.beginBlock (raw.autoGain->load (std::memory_order_relaxed) > 0.5f);

    for (int pos = 0; pos < total;)
    {
        const int num = juce::jmin (chunk, total - pos);

        auto p = target;
        p.highPassHz = Desk::highPassFromDial (sHighPass.skip (num));
        p.lowPassHz = Desk::lowPassFromDial (sLowPass.skip (num));
        p.lfHz = sLfFreq.skip (num);   p.lfDb = sLfGain.skip (num);
        p.lmfHz = sLmfFreq.skip (num); p.lmfDb = sLmfGain.skip (num); p.lmfQ = sLmfQ.skip (num);
        p.hmfHz = sHmfFreq.skip (num); p.hmfDb = sHmfGain.skip (num); p.hmfQ = sHmfQ.skip (num);
        p.hfHz = sHfFreq.skip (num);   p.hfDb = sHfGain.skip (num);
        engine.setParams (p);

        float* ch[SongsmithDesk::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);   // the dry signal, for bypass
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        outputStage.measureInput (ch, numCh, num);   // Auto Gain compares what comes out with what went in
        engine.process (ch, numCh, num);
        reduction = juce::jmax (reduction, engine.readMaxGainReductionDb());
        outputStage.process (ch, numCh, num, raw.outputTrim->load (std::memory_order_relaxed));   // Auto Gain, then Output Trim

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
            const float channelPeak = buffer.getMagnitude (c, pos, num);   // after everything, including Bypass
            blockPeak = juce::jmax (blockPeak, channelPeak);
            outputMeter.pushChannel (c, channelPeak);
        }

        pos += num;
    }

    pushMax (gainReductionDb, bypassedNow ? 0.0f : reduction);

    outputMeter.push (blockPeak);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, numOut), std::memory_order_relaxed);
    outputMeter.updateClipLatch (blockPeak, total);
}

//==============================================================================
void SongsmithAnalogDeskProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    presets.loadFactory (index);
}

const juce::String SongsmithAnalogDeskProcessor::getProgramName (int index)
{
    const auto& list = Desk::Presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* SongsmithAnalogDeskProcessor::createEditor()
{
    return new SongsmithAnalogDeskEditor (*this);
}

juce::ValueTree SongsmithAnalogDeskProcessor::createState()
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

void SongsmithAnalogDeskProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithAnalogDeskProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithAnalogDeskProcessor::restoreFromValueTree (const juce::ValueTree& saved)
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
    smoothersReady = false;   // a loaded session jumps to its settings instead of gliding to them

    const auto ui = saved.getChildWithName (uiId);
    editorWidth.store (Desk::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", Desk::EditorLayout::defaultWidth)
                                                                         : Desk::EditorLayout::defaultWidth));
    presets.currentName = ui.isValid() ? ui.getProperty ("preset", juce::String()).toString() : juce::String();
    presets.currentHint = ui.isValid() ? ui.getProperty ("presetHint", juce::String()).toString() : juce::String();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithAnalogDeskProcessor();
}
