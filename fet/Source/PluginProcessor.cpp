#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace FET;

SongsmithAnalogCompressorProcessor::SongsmithAnalogCompressorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SongsmithAnalogCompressor", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ID::bypass));
    inputDb  = apvts.getRawParameterValue (ID::input);
    outputDb = apvts.getRawParameterValue (ID::output);
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

void SongsmithAnalogCompressorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    inputGain.reset (sampleRate, gainSmoothingSeconds);
    outputGain.reset (sampleRate, gainSmoothingSeconds);
    bypassMix.reset (sampleRate, bypassFadeSeconds);

    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputDb->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputDb->load()));
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    dry.setSize (juce::jmax (1, getTotalNumOutputChannels()), juce::jmax (1, samplesPerBlock), false, false, true);
}

void SongsmithAnalogCompressorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int n = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (n == 0 || numCh == 0)
        return;

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (inputDb->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputDb->load()));
    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    // Fully bypassed and settled: leave the buffer untouched (bit-exact pass-through).
    if (! bypassMix.isSmoothing() && bypassMix.getTargetValue() >= 1.0f)
    {
        inputGain.skip (n);
        outputGain.skip (n);
        return;
    }

    // Hosts may pass a bigger block than announced; the dry copy is only kept for the fade.
    const bool fading = bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f;
    const bool dryFits = dry.getNumChannels() >= numCh && dry.getNumSamples() >= n;
    if (fading && dryFits)
        for (int c = 0; c < numCh; ++c)
            dry.copyFrom (c, 0, buffer, c, 0, n);

    // Shell: input drive and output trim only. The compressor engine arrives in step 2.
    for (int i = 0; i < n; ++i)
    {
        const float g = inputGain.getNextValue() * outputGain.getNextValue();
        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[i] *= g;
    }

    if (fading)
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = bypassMix.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                auto* w = buffer.getWritePointer (c);
                const float d = dryFits ? dry.getSample (c, i) : w[i];
                w[i] = w[i] * (1.0f - m) + d * m;
            }
        }
    }
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
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithAnalogCompressorProcessor();
}
