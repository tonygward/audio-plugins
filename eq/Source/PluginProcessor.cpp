#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier slotsId  ("BANDSLOTS");
    const juce::Identifier uiId     ("UI");
    const juce::Identifier versionId ("version");
}

//==============================================================================
ThreeBandEQAudioProcessor::ThreeBandEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int b = 0; b < numBands; ++b)
    {
        bandUsed[(size_t) b].store (EQ::defaultUsed (b));

        for (int p = 0; p < EQ::numBandParams; ++p)
        {
            const auto id = EQ::paramId (b, (EQ::Param) p);
            bandParams[(size_t) b][(size_t) p] = apvts.getParameter (id);
            rawValues[(size_t) b][(size_t) p]  = apvts.getRawParameterValue (id);
            jassert (bandParams[(size_t) b][(size_t) p] != nullptr);
        }
    }

    outputGainParam = apvts.getParameter (EQ::outputGainId);
    outputGainRaw   = apvts.getRawParameterValue (EQ::outputGainId);
    bypassParam     = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (EQ::bypassId));
    jassert (outputGainParam != nullptr && outputGainRaw != nullptr && bypassParam != nullptr);

    dryBuffer.setSize (EQ::Engine::maxChannels, 512);   // resized in prepareToPlay
}

juce::AudioProcessorValueTreeState::ParameterLayout ThreeBandEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Frequency range/skew is identical to v0.1, so existing automation maps the same.
    juce::NormalisableRange<float> freqRange (EQ::minFreq, EQ::maxFreq, 0.1f);
    freqRange.setSkewForCentre (1000.0f);

    juce::NormalisableRange<float> gainRange (EQ::minGain, EQ::maxGain, 0.1f);

    juce::NormalisableRange<float> qRange (EQ::minQ, EQ::maxQ, 0.01f);
    qRange.setSkewForCentre (1.0f);

    for (int b = 0; b < numBands; ++b)
    {
        const juce::String name = "Band " + juce::String (b + 1);
        using EQ::Param;

        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            "band" + juce::String (b + 1), name, " | ");

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Freq), 1 }, name + " Freq", freqRange, EQ::defaultFreq (b),
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Gain), 1 }, name + " Gain", gainRange, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Q), 1 }, name + " Q", qRange, EQ::defaultQ));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { EQ::paramId (b, Param::Type), 2 }, name + " Type", EQ::getTypeNames(), 0));

        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { EQ::paramId (b, Param::Enabled), 2 }, name + " On", true));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { EQ::paramId (b, Param::Slope), 3 }, name + " Slope", EQ::getSlopeNames(), EQ::defaultSlope));

        layout.add (std::move (group));
    }

    // Global output stage (added in v0.3). Defaults are neutral, so sessions saved
    // before these existed load with 0 dB output gain and bypass off.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { EQ::outputGainId, 3 }, "Output Gain",
        juce::NormalisableRange<float> (EQ::minOutputGain, EQ::maxOutputGain, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { EQ::bypassId, 3 }, "Bypass", false));

    return layout;
}

//==============================================================================
float ThreeBandEQAudioProcessor::getBandValue (int band, EQ::Param p) const
{
    return rawValues[(size_t) band][(size_t) p]->load();
}

void ThreeBandEQAudioProcessor::setBandUsed (int band, bool used)
{
    bandUsed[(size_t) band].store (used);
}

void ThreeBandEQAudioProcessor::notifyNonParameterStateChanged()
{
    updateHostDisplay (ChangeDetails{}.withNonParameterStateChanged (true));
}

int ThreeBandEQAudioProcessor::getNumUsedBands() const noexcept
{
    int n = 0;
    for (auto& u : bandUsed)
        n += u.load() ? 1 : 0;
    return n;
}

int ThreeBandEQAudioProcessor::findFreeBand() const noexcept
{
    for (int b = 0; b < numBands; ++b)
        if (! bandUsed[(size_t) b].load())
            return b;
    return -1;
}

//==============================================================================
bool ThreeBandEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void ThreeBandEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);

    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (EQ::Engine::maxChannels, juce::jmax (512, samplesPerBlock), false, true, false);

    outputGain.reset (sampleRate, outputGainSmoothingSeconds);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainRaw->load()));
    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);
}

void ThreeBandEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    using EQ::Param;
    for (int b = 0; b < numBands; ++b)
    {
        const auto& raw = rawValues[(size_t) b];

        EQ::BandSettings s;
        s.active = bandUsed[(size_t) b].load (std::memory_order_relaxed) && raw[(size_t) Param::Enabled]->load() >= 0.5f;
        s.type   = EQ::typeFromIndex ((int) std::lround (raw[(size_t) Param::Type]->load()));
        s.freq   = raw[(size_t) Param::Freq]->load();
        s.gain   = EQ::typeHasGain (s.type) ? raw[(size_t) Param::Gain]->load() : 0.0f;
        s.q      = raw[(size_t) Param::Q]->load();
        s.slope  = (int) std::lround (raw[(size_t) Param::Slope]->load());
        engine.setBand (b, s);
    }

    const int solo = soloBand.load (std::memory_order_relaxed);
    engine.setSolo (solo >= 0 && solo < numBands && bandUsed[(size_t) solo].load (std::memory_order_relaxed) ? solo : -1);

    const int numCh = juce::jmin (numIn, EQ::Engine::maxChannels);
    const auto analyserMode = (SpectrumAnalyser::Mode) analyser.mode.load (std::memory_order_relaxed);
    const bool capture = analyser.activeViewers.load (std::memory_order_relaxed) > 0;

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainRaw->load (std::memory_order_relaxed)));
    bypassMix.setTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    const int total = buffer.getNumSamples();
    const int chunk = juce::jmax (1, dryBuffer.getNumSamples());
    float blockPeak = 0.0f;

    for (int pos = 0; pos < total; pos += chunk)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[EQ::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
            ch[c] = buffer.getWritePointer (c, pos);

        if (capture && analyserMode == SpectrumAnalyser::Mode::Pre)
            analyser.push (ch, numCh, num);

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        const bool needDry = bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f;
        if (needDry)
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);

        engine.process (ch, numCh, num);

        if (outputGain.isSmoothing() || bypassMix.isSmoothing())
        {
            for (int i = 0; i < num; ++i)
            {
                const float g = outputGain.getNextValue();
                const float m = bypassMix.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    const float wet = ch[c][i] * g;
                    ch[c][i] = wet + (dryBuffer.getSample (c, i) - wet) * m;
                }
            }
        }
        else if (bypassMix.getTargetValue() >= 1.0f)
        {
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::copy (ch[c], dryBuffer.getReadPointer (c), num);   // bit-exact dry
        }
        else if (const float g = outputGain.getTargetValue(); g != 1.0f)
        {
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::multiply (ch[c], g, num);
        }

        if (capture && analyserMode == SpectrumAnalyser::Mode::Post)
            analyser.push (ch, numCh, num);

        for (int c = 0; c < numCh; ++c)
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (ch[c], num);
            blockPeak = juce::jmax (blockPeak, -range.getStart(), range.getEnd());
        }
    }

    outputMeter.push (blockPeak);
}

//==============================================================================
juce::AudioProcessorEditor* ThreeBandEQAudioProcessor::createEditor()
{
    return new ThreeBandEQAudioProcessorEditor (*this);
}

void ThreeBandEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree slots (slotsId);
    for (int b = 0; b < numBands; ++b)
        slots.setProperty ("b" + juce::String (b + 1), isBandUsed (b), nullptr);
    state.appendChild (slots, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("selectedBand", selectedBand.load(), nullptr);
    ui.setProperty ("displayRange", displayRangeDb.load(), nullptr);
    ui.setProperty ("analyserMode", analyser.mode.load(), nullptr);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("editorHeight", editorHeight.load(), nullptr);
    state.appendChild (ui, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ThreeBandEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void ThreeBandEQAudioProcessor::restoreFromValueTree (const juce::ValueTree& saved)
{
    const int version = saved.getProperty (versionId, 1);

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
            value = rp->getNormalisableRange().snapToLegalValue ((float) child.getProperty ("value"));

        fresh.appendChild (juce::ValueTree ("PARAM", { { "id", rp->paramID }, { "value", value } }), nullptr);
    }

    apvts.replaceState (fresh);

    const auto slots = saved.getChildWithName (slotsId);
    for (int b = 0; b < numBands; ++b)
    {
        const bool used = (version >= 2 && slots.isValid())
                              ? (bool) slots.getProperty ("b" + juce::String (b + 1), false)
                              : EQ::defaultUsed (b);
        setBandUsed (b, used);
    }

    const auto ui = saved.getChildWithName (uiId);
    if (ui.isValid())
    {
        selectedBand.store (juce::jlimit (-1, numBands - 1, (int) ui.getProperty ("selectedBand", 0)));
        displayRangeDb.store (juce::jlimit (6.0f, 30.0f, (float) ui.getProperty ("displayRange", 24.0f)));
        analyser.mode.store (juce::jlimit (0, 2, (int) ui.getProperty ("analyserMode", (int) SpectrumAnalyser::Mode::Post)));
        editorWidth.store  (juce::jlimit (760, 2000, (int) ui.getProperty ("editorWidth", 980)));
        editorHeight.store (juce::jlimit (440, 1200, (int) ui.getProperty ("editorHeight", 580)));
    }

    if (juce::MessageManager::existsAndIsCurrentThread())
        edits.getUndoManager().clearUndoHistory();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThreeBandEQAudioProcessor();
}
