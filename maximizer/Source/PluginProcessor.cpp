#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID, AU aufx Mxlm Tnyw). They must
// never change, or saved sessions stop finding the plug-in. See maximizer/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x4d786c6d /* 'Mxlm' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

namespace
{
    const juce::Identifier versionId ("version");
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
    jassert (thresholdRaw != nullptr && ceilingRaw != nullptr && releaseRaw != nullptr && bypassParam != nullptr);

    dryBuffer.setSize (2, 512, false, true);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithMaximiserAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::thresholdId, 1 }, "Threshold",
        juce::NormalisableRange<float> (Max::minThresholdDb, Max::maxThresholdDb, 0.01f), Max::defaultThresholdDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::ceilingId, 1 }, "Out Ceiling",
        juce::NormalisableRange<float> (Max::minCeilingDb, Max::maxCeilingDb, 0.01f), Max::defaultCeilingDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { Max::releaseId, 1 }, "Release", Max::releaseRange(), Max::defaultReleaseMs,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { Max::bypassId, 1 }, "Bypass", false));

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
}

void SongsmithMaximiserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numCh = juce::jmin (numIn, 2);
    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());

    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    for (int pos = 0; pos < total; pos += chunk)
    {
        const int num = juce::jmin (chunk, total - pos);

        // Step 01 shell: the processing path is transparent (the limiter arrives in step 02),
        // so the dry and processed signals are identical and the bypass fade is inaudible.
        // The structure is already the final one: keep a dry copy while bypass is fading or on.
        const bool needDry = bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f;
        if (needDry)
            for (int c = 0; c < numCh; ++c)
                dryBuffer.copyFrom (c, 0, buffer, c, pos, num);

        if (bypassMix.isSmoothing())
        {
            for (int i = 0; i < num; ++i)
            {
                const float m = bypassMix.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c, pos);
                    d[i] += (dryBuffer.getSample (c, i) - d[i]) * m;
                }
            }
        }
        else if (needDry)
        {
            for (int c = 0; c < numCh; ++c)
                buffer.copyFrom (c, pos, dryBuffer, c, 0, num);   // bit-exact dry
        }
    }
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
    // Version 1 is the first format, so there is nothing to migrate yet. A state from a newer
    // version keeps the parameters this version knows (by ID) and ignores the rest.
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

        if (child.isValid() && child.hasProperty ("value"))
        {
            const float v = (float) child.getProperty ("value");
            if (std::isfinite (v))
                value = rp->getNormalisableRange().snapToLegalValue (v);
        }

        fresh.appendChild (juce::ValueTree ("PARAM", { { "id", rp->paramID }, { "value", value } }), nullptr);
    }

    apvts.replaceState (fresh);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithMaximiserAudioProcessor();
}
