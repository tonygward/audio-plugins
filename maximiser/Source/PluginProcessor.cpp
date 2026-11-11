#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Mxlm Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See maximiser/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x4d786c6d /* 'Mxlm' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

namespace
{
    const juce::Identifier versionId ("version");
    const juce::Identifier uiId ("UI");
}

//==============================================================================
SongsmithMaximiserAudioProcessor::SongsmithMaximiserAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    thresholdRaw = apvts.getRawParameterValue (Max::thresholdId);
    ceilingRaw   = apvts.getRawParameterValue (Max::ceilingId);
    releaseRaw   = apvts.getRawParameterValue (Max::releaseId);
    bypassParam  = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Max::bypassId));
    peakModeParam    = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Max::peakModeId));
    autoReleaseParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (Max::autoReleaseId));
    ditherParam      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Max::ditherId));
    jassert (peakModeParam != nullptr && autoReleaseParam != nullptr && ditherParam != nullptr);
    jassert (thresholdRaw != nullptr && ceilingRaw != nullptr && releaseRaw != nullptr && bypassParam != nullptr);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
    loudnessTimer.startTimer (100);            // keeps the loudness readings up to date with no editor open
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithMaximiserAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (Max::minThresholdDb, Max::maxThresholdDb, 0.01f), Max::defaultThresholdDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::ceilingId, 1 }, "Output Ceiling",
        juce::NormalisableRange<float> (Max::minCeilingDb, Max::maxCeilingDb, 0.01f), Max::defaultCeilingDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::releaseId, 1 }, "Release", Max::releaseRange(), Max::defaultReleaseMs,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Max::bypassId, 1 }, "Bypass", false));

    // Version 2 (step 03). Defaults keep version-1 sessions sounding the same.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Max::peakModeId, 2 }, "Peak Mode", Max::getPeakModeNames(), 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Max::autoReleaseId, 2 }, "Auto Release", false));

    // Version 3 (step 04): final-stage dither, off by default.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { Max::ditherId, 3 }, "Dither", Max::getDitherNames(), 0));

    return layout;
}

//==============================================================================
bool SongsmithMaximiserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void SongsmithMaximiserAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (2, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    limiter.setThresholdDb (getThresholdDb());
    limiter.setCeilingDb (getCeilingDb());
    limiter.setTruePeak (isTruePeak());
    limiter.setAutoRelease (isAutoRelease());
    limiter.setReleaseMs (getReleaseMs());
    dither.setDepth (getDitherDepth());
    limiter.setCeilingHeadroom (Max::Dither::headroomFor (getDitherDepth()));
    limiter.prepare (sampleRate);
    dither.reset();
    loudnessMeter.prepare (sampleRate);
    wasPlaying = false;

    dryDelay.setSize (2, limiter.getLatencySamples() + 1, false, true, false);
    dryDelay.clear();
    dryDelayPos = 0;

    setLatencySamples (limiter.getLatencySamples());
}

void SongsmithMaximiserAudioProcessor::reset()
{
    limiter.reset();
    dither.reset();
    dryDelay.clear();
    dryDelayPos = 0;
}

void SongsmithMaximiserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, Max::BrickwallLimiter::maxChannels);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);
    limiter.setThresholdDb (getThresholdDb());
    limiter.setCeilingDb (getCeilingDb());
    limiter.setTruePeak (isTruePeak());
    limiter.setAutoRelease (isAutoRelease());
    limiter.setReleaseMs (getReleaseMs());

    // Dither is the last stage before the bypass mix (the dry signal is never dithered). While
    // it is on, the ceiling is lowered by its headroom so the quantised output stays under it.
    const auto depth = getDitherDepth();
    dither.setDepth (depth);
    limiter.setCeilingHeadroom (Max::Dither::headroomFor (depth));

    float inPeak = 0.0f, outPeak = 0.0f;

    for (int pos = 0; pos < total; pos += chunk)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[Max::BrickwallLimiter::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
        {
            ch[c] = buffer.getWritePointer (c, pos);
            inPeak = juce::jmax (inPeak, buffer.getMagnitude (c, pos, num));
        }

        // The dry signal, delayed by the look-ahead so bypass lines up with the processed path.
        // The ring always runs, so it is valid whenever bypass engages.
        const int size = dryDelay.getNumSamples();
        const int latency = limiter.getLatencySamples();
        for (int c = 0; c < numCh; ++c)
        {
            auto* ring = dryDelay.getWritePointer (c);
            auto* dry = dryBuffer.getWritePointer (c);
            int w = dryDelayPos;
            for (int i = 0; i < num; ++i)
            {
                ring[w] = ch[c][i];
                dry[i] = ring[(w - latency + size) % size];
                w = w + 1 == size ? 0 : w + 1;
            }
        }
        dryDelayPos = (dryDelayPos + num) % size;

        // The limiter always runs, even when bypassed, so un-bypassing never glitches.
        limiter.process (ch, numCh, num);
        dither.process (ch, numCh, num);

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

    // Loudness of what leaves the plugin (after the limiter, dither and bypass mix).
    if (auto* head = getPlayHead())
        if (const auto position = head->getPosition())
            notePlayState (position->getIsPlaying());
    loudnessMeter.process (buffer.getArrayOfReadPointers(), numCh, total);

    pushPeak (inputPeak, inPeak);
    pushPeak (outputPeak, outPeak);

    // Deepest gain reduction since the UI last read it (stored as a positive dB amount).
    pushPeak (gainReductionDb, -limiter.readAndResetMaxGainReductionDb());
}


//==============================================================================
juce::AudioProcessorEditor* SongsmithMaximiserAudioProcessor::createEditor()
{
    return new SongsmithMaximiserAudioProcessorEditor (*this);
}

juce::ValueTree SongsmithMaximiserAudioProcessor::createState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    // UI state: not automatable, saved with the session.
    juce::ValueTree ui (uiId);
    ui.setProperty ("link", linkThresholdCeiling.load(), nullptr);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    state.appendChild (ui, nullptr);
    return state;
}

void SongsmithMaximiserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithMaximiserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithMaximiserAudioProcessor::restoreFromValueTree (const juce::ValueTree& saved)
{
    // Version 1 (step 01-02) had no peak_mode / auto_release. auto_release restores to its default
    // (manual release) and peak_mode is set to Sample peak explicitly (see the migration below,
    // because the default is now True peak), which is exactly how version 1 sounded. A state from a newer version
    // keeps the parameters this version knows (by ID) and ignores the rest.
    // Rebuild a complete parameter tree: saved values where present, defaults otherwise.
    // (APVTS would otherwise leave missing parameters at their *current* values.)
    juce::ValueTree fresh (apvts.state.getType());

    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;

        float value = rp->convertFrom0to1 (rp->getDefaultValue());
        const auto child = saved.getChildWithProperty ("id", rp->paramID);

        // Migration: sessions saved before peak_mode existed (version 1) sounded as sample peak,
        // while a new instance now defaults to true peak. Keep them sounding the same.
        if (rp->paramID == Max::peakModeId && ! (child.isValid() && child.hasProperty ("value")))
            value = 0.0f;

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

    // UI state (version 3 on). Older sessions: link off, default size.
    const auto ui = saved.getChildWithName (uiId);
    linkThresholdCeiling.store (ui.isValid() && (bool) ui.getProperty ("link", false));
    editorWidth.store (Max::EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", Max::EditorLayout::defaultWidth)
                                                                       : Max::EditorLayout::defaultWidth));
}

void SongsmithMaximiserAudioProcessor::applyStartingPoint (int index)
{
    const auto& points = Max::getStartingPoints();
    if (index < 0 || index >= (int) points.size())
        return;

    const auto& sp = points[(size_t) index];
    auto set = [this] (const char* id, float value)
    {
        auto* param = apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };

    set (Max::thresholdId, sp.thresholdDb);
    set (Max::ceilingId, sp.ceilingDb);
    set (Max::releaseId, sp.releaseMs);
    set (Max::peakModeId, (float) sp.peakMode);
    set (Max::autoReleaseId, sp.autoRelease ? 1.0f : 0.0f);
    set (Max::ditherId, (float) sp.dither);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithMaximiserAudioProcessor();
}
