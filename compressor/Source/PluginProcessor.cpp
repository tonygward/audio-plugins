#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Cmp1 Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See compressor/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x436d7031 /* 'Cmp1' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

namespace
{
    const juce::Identifier versionId ("version");
    const juce::Identifier uiId ("UI");
}

//==============================================================================
SongsmithCompressorAudioProcessor::SongsmithCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdRaw = apvts.getRawParameterValue (Comp::thresholdId);
    ratioRaw     = apvts.getRawParameterValue (Comp::ratioId);
    attackRaw    = apvts.getRawParameterValue (Comp::attackId);
    releaseRaw   = apvts.getRawParameterValue (Comp::releaseId);
    kneeRaw      = apvts.getRawParameterValue (Comp::kneeId);
    makeupRaw    = apvts.getRawParameterValue (Comp::makeupId);
    mixRaw       = apvts.getRawParameterValue (Comp::mixId);
    autoMakeupParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Comp::autoMakeupId));
    bypassParam     = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Comp::bypassId));
    warmthParam     = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Comp::warmthId));
    jassert (thresholdRaw && ratioRaw && attackRaw && releaseRaw && kneeRaw && makeupRaw && mixRaw && autoMakeupParam && warmthParam && bypassParam);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithCompressorAudioProcessor::createParameterLayout()
{
    using namespace SongsmithComp;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (minThresholdDb, maxThresholdDb, 0.1f), Comp::defaultThresholdDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::ratioId, 1 }, "Ratio", Comp::ratioRange(), Comp::defaultRatio,
        juce::AudioParameterFloatAttributes().withLabel (":1")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::attackId, 1 }, "Attack", Comp::logRange (minAttackMs, maxAttackMs, 0.01f), Comp::defaultAttackMs,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::releaseId, 1 }, "Release", Comp::logRange (minReleaseMs, maxReleaseMs, 0.1f), Comp::defaultReleaseMs,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::kneeId, 1 }, "Knee",
        juce::NormalisableRange<float> (minKneeDb, maxKneeDb, 0.1f), Comp::defaultKneeDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::makeupId, 1 }, "Makeup Gain",
        juce::NormalisableRange<float> (minMakeupDb, maxMakeupDb, 0.1f), Comp::defaultMakeupDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Comp::autoMakeupId, 1 }, "Auto Makeup", Comp::defaultAutoMakeup));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Comp::mixId, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), Comp::defaultMixPercent,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Comp::bypassId, 1 }, "Bypass", false));

    // Version 2: Analog Warmth, off by default so version-1 sessions sound the same.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Comp::warmthId, 2 }, "Analog Warmth", false));

    return layout;
}

SongsmithComp::Params SongsmithCompressorAudioProcessor::getEngineParams() const noexcept
{
    SongsmithComp::Params p;
    p.thresholdDb = thresholdRaw->load (std::memory_order_relaxed);
    p.ratio       = ratioRaw->load (std::memory_order_relaxed);
    p.attackMs    = attackRaw->load (std::memory_order_relaxed);
    p.releaseMs   = releaseRaw->load (std::memory_order_relaxed);
    p.kneeDb      = kneeRaw->load (std::memory_order_relaxed);
    p.makeupDb    = makeupRaw->load (std::memory_order_relaxed);
    p.autoMakeup  = autoMakeupParam->get();
    p.warmth      = warmthParam->get();
    p.mix         = mixRaw->load (std::memory_order_relaxed) * 0.01f;
    return p;
}

//==============================================================================
bool SongsmithCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    engine.prepare (sampleRate);
    engine.setParams (getEngineParams());
    engine.reset();

    setLatencySamples (0);
}

void SongsmithCompressorAudioProcessor::reset()
{
    engine.reset();
}

void SongsmithCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, SongsmithComp::Engine::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    engine.setParams (getEngineParams());

    float inPeak = 0.0f, outPeak = 0.0f, reduction = 0.0f;

    for (int pos = 0; pos < total; pos += chunk)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[SongsmithComp::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, pos, num));
            juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);   // the dry signal, for bypass
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        engine.process (ch, numCh, num);
        reduction = juce::jmax (reduction, engine.readMeters().gainReductionDb);

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
            outPeak = juce::jmax (outPeak, buffer.getMagnitude (c, pos, num));
    }

    pushPeak (inputPeak, inPeak);
    pushPeak (outputPeak, outPeak);
    pushPeak (gainReductionDb, bypassParam->get() ? 0.0f : reduction);
}

//==============================================================================
juce::AudioProcessorEditor* SongsmithCompressorAudioProcessor::createEditor()
{
    return new SongsmithCompressorAudioProcessorEditor (*this);
}

juce::ValueTree SongsmithCompressorAudioProcessor::createState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    state.appendChild (ui, nullptr);
    return state;
}

void SongsmithCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithCompressorAudioProcessor::restoreFromValueTree (const juce::ValueTree& saved)
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
    editorWidth.store (Comp::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", Comp::EditorLayout::defaultWidth)
                                                                        : Comp::EditorLayout::defaultWidth));
}

void SongsmithCompressorAudioProcessor::applyPreset (int index)
{
    const auto& presets = Comp::getPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    const auto& preset = presets[(size_t) index];
    auto set = [this] (const char* id, float value)
    {
        auto* param = apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };

    set (Comp::thresholdId, preset.thresholdDb);
    set (Comp::ratioId, preset.ratio);
    set (Comp::attackId, preset.attackMs);
    set (Comp::releaseId, preset.releaseMs);
    set (Comp::kneeId, preset.kneeDb);
    set (Comp::makeupId, preset.makeupDb);
    set (Comp::autoMakeupId, preset.autoMakeup ? 1.0f : 0.0f);
    set (Comp::mixId, preset.mixPercent);
    set (Comp::warmthId, 0.0f);   // every preset is the clean compressor; Analog Warmth is the musician's choice
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithCompressorAudioProcessor();
}
