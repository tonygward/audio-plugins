#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plugin by these codes (VST3 class ID, AU aufx Eq3b Tnyw). They must
// never change, or saved sessions stop finding the plugin. See eq/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x45713362 /* 'Eq3b' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

namespace
{
    const juce::Identifier slotsId  ("BANDSLOTS");
    const juce::Identifier uiId     ("UI");
    const juce::Identifier versionId ("version");
}

//==============================================================================
// Rebuilds the linear-phase FIRs in the background, at most every 40 ms, only while
// linear phase is selected and only when the EQ actually changed.
struct SongsmithEQAudioProcessor::LinearPhaseDesigner final : juce::Thread
{
    explicit LinearPhaseDesigner (SongsmithEQAudioProcessor& p) : juce::Thread (juce::String (JucePlugin_Name) + " linear phase"), processor (p) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            processor.updateLinearPhaseDesign (false);
            wait (40);
        }
    }

    SongsmithEQAudioProcessor& processor;
};

SongsmithEQAudioProcessor::~SongsmithEQAudioProcessor()
{
    stopTimer();
    // Wait for the designer to finish its current FIR rather than force-kill it after a timeout:
    // a killed thread can die holding a lock (or the heap lock) and deadlock the host. A design
    // takes milliseconds normally, longer only on a heavily loaded machine.
    if (lpDesigner != nullptr)
        lpDesigner->stopThread (-1);
}

EQ::LinearPhaseBands SongsmithEQAudioProcessor::gatherLinearPhaseBands() const
{
    using EQ::Param;
    EQ::LinearPhaseBands bands;
    const float scale = getGainScale();

    for (int b = 0; b < numBands; ++b)
    {
        auto& lb = bands[(size_t) b];
        lb.active = isBandUsed (b) && getBandValue (b, Param::Enabled) >= 0.5f;
        lb.type   = EQ::typeFromIndex ((int) std::lround (getBandValue (b, Param::Type)));
        lb.freq   = getBandValue (b, Param::Freq);
        lb.q      = getBandValue (b, Param::Q);
        lb.gain   = getBandValue (b, Param::Gain) * scale;
        lb.slope  = (int) std::lround (getBandValue (b, Param::Slope));
        lb.mode   = EQ::stereoModeFromIndex ((int) std::lround (getBandValue (b, Param::Stereo)));
    }
    return bands;
}

bool SongsmithEQAudioProcessor::updateLinearPhaseDesign (bool force)
{
    const std::lock_guard<std::mutex> lock (designMutex);
    linearPhase.collectGarbage();

    if (! force && ! isLinearPhaseSelected())
        return false;

    const auto bands = gatherLinearPhaseBands();
    const int length = getLinearPhaseLength();
    const bool mono = getMainBusNumInputChannels() < 2;

    // Cheap change detection over everything the design depends on.
    std::size_t hash = std::hash<double>() (getDesignSampleRate()) ^ ((std::size_t) length << 1) ^ (mono ? 0x9e37u : 0u);
    for (const auto& b : bands)
        for (float v : { b.active ? 1.0f : 0.0f, (float) b.type, b.freq, b.q, b.gain, (float) b.slope, (float) b.mode })
            hash = hash * 1099511628211ull ^ std::hash<float>() (v);

    if (! force && hash == lastDesignHash && designedLength.load() == length)
        return false;

    lastDesignHash = hash;
    linearPhase.submit (EQ::LinearPhaseEQ::design (bands, currentSampleRate, getDesignSampleRate(), length, mono));
    designedLength.store (length);
    return true;
}

void SongsmithEQAudioProcessor::updateLatencyNow()
{
    // Linear phase designs at the oversampled rate without running the oversampler, so the
    // two latencies never add up: it is either the FIR's or the oversampler's.
    const int latency = isLinearPhaseSelected() ? getLinearPhaseLength() / 2 : oversamplerLatency[(size_t) getOversamplingIndex()];
    if (getLatencySamples() != latency)
        setLatencySamples (latency);
}

void SongsmithEQAudioProcessor::timerCallback()
{
    updateLatencyNow();
}

double SongsmithEQAudioProcessor::getTailLengthSeconds() const
{
    // Linear phase: the latency plus the second half of the FIR.
    return isLinearPhaseSelected() ? getLinearPhaseLength() / currentSampleRate : 0.0;
}

//==============================================================================
SongsmithEQAudioProcessor::SongsmithEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Side Chain", juce::AudioChannelSet::stereo(), false)),   // analysed only
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
    autoGainParam   = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (EQ::autoGainId));
    gainScaleParam  = apvts.getParameter (EQ::gainScaleId);
    gainScaleRaw    = apvts.getRawParameterValue (EQ::gainScaleId);
    jassert (gainScaleParam != nullptr && gainScaleRaw != nullptr);
    phaseModeParam  = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (EQ::phaseModeId));
    lpQualityParam  = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (EQ::lpQualityId));
    jassert (phaseModeParam != nullptr && lpQualityParam != nullptr);
    oversamplingParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (EQ::oversamplingId));
    jassert (oversamplingParam != nullptr);

    // A new instance opens at the UI size last chosen on this machine (a session overrides it).
    uiScale.store (loadMachineUiScale());
    editorWidth.store  (juce::roundToInt ((float) EQ::EditorSize::defaultWidth  * uiScale.load()));
    editorHeight.store (juce::roundToInt ((float) EQ::EditorSize::defaultHeight * uiScale.load()));

    // Polyphase IIR half-band stages: low latency (minimum phase), integer latency so it can
    // be reported exactly. Built here; prepareToPlay only sizes them.
    using OS = juce::dsp::Oversampling<float>;
    for (size_t k = 1; k < oversamplers.size(); ++k)
        oversamplers[k] = std::make_unique<OS> ((size_t) EQ::Engine::maxChannels, k, OS::filterHalfBandPolyphaseIIR, true, true);

    lpDesigner = std::make_unique<LinearPhaseDesigner> (*this);
    lpDesigner->startThread (juce::Thread::Priority::low);
    startTimerHz (10);
    jassert (autoGainParam != nullptr);
    jassert (outputGainParam != nullptr && outputGainRaw != nullptr && bypassParam != nullptr);

    dryBuffer.setSize (EQ::Engine::maxChannels, 512, false, true);   // resized in prepareToPlay; zeroed
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Frequency range/skew is identical to v0.1, so existing automation maps the same.
    // Continuous (no 0.1 Hz grid) so note-snapped frequencies are exact; the normalised
    // mapping is unchanged.
    juce::NormalisableRange<float> freqRange (EQ::minFreq, EQ::maxFreq, 0.0f);
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
            juce::ParameterID { EQ::paramId (b, Param::Freq), 1 }, name + " Frequency", freqRange, EQ::defaultFreq (b),
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Gain), 1 }, name + " Gain", gainRange, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Q), 1 }, name + " Q", qRange, EQ::defaultQ));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { EQ::paramId (b, Param::Type), 2 }, name + " Type", EQ::getTypeNames(), 0));

        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { EQ::paramId (b, Param::Enabled), 2 }, name + " Enabled", true));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { EQ::paramId (b, Param::Slope), 3 }, name + " Slope", EQ::getSlopeNames(), EQ::defaultSlope));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { EQ::paramId (b, Param::Stereo), 4 }, name + " Stereo", EQ::getStereoModeNames(), 0));

        // Dynamic EQ (v1.0). Off by default, so older sessions sound identical.
        juce::NormalisableRange<float> attackRange (EQ::minAttackMs, EQ::maxAttackMs, 0.01f);
        attackRange.setSkewForCentre (5.0f);
        juce::NormalisableRange<float> releaseRange (EQ::minReleaseMs, EQ::maxReleaseMs, 0.1f);
        releaseRange.setSkewForCentre (100.0f);

        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { EQ::paramId (b, Param::DynOn), 7 }, name + " Dynamic", false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Threshold), 7 }, name + " Threshold",
            juce::NormalisableRange<float> (EQ::minThreshold, EQ::maxThreshold, 0.1f), EQ::defaultThreshold,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Range), 7 }, name + " Range",
            juce::NormalisableRange<float> (EQ::minRange, EQ::maxRange, 0.1f), EQ::defaultRange,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Attack), 7 }, name + " Attack", attackRange, EQ::defaultAttackMs,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { EQ::paramId (b, Param::Release), 7 }, name + " Release", releaseRange, EQ::defaultReleaseMs,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));
        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { EQ::paramId (b, Param::DynExternal), 7 }, name + " Dynamic Side Chain", false));

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

    // Scale the whole curve (v0.5): every gain band's dB gain x scale. 100 % = as set.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { EQ::gainScaleId, 6 }, "Gain Scale",
        juce::NormalisableRange<float> (EQ::minGainScale, EQ::maxGainScale, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // Linear-phase mode (v1.0). Zero latency (the existing engine) is the default.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { EQ::phaseModeId, 8 }, "Phase Mode", juce::StringArray { "Zero latency", "Linear phase" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { EQ::lpQualityId, 8 }, "Linear Phase Quality", juce::StringArray { "Low", "Medium", "High" }, 1));

    // Optional oversampling (v1.0). Off by default (identical to before).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { EQ::oversamplingId, 9 }, "Oversampling", juce::StringArray { "Off", "2x", "4x" }, 0));

    // Loudness-matched output (v0.5). Applied before the output gain.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { EQ::autoGainId, 5 }, "Auto Gain", false));

    return layout;
}

//==============================================================================
float SongsmithEQAudioProcessor::getBandValue (int band, EQ::Param p) const
{
    return rawValues[(size_t) band][(size_t) p]->load();
}

void SongsmithEQAudioProcessor::setBandUsed (int band, bool used)
{
    bandUsed[(size_t) band].store (used);
}

void SongsmithEQAudioProcessor::notifyNonParameterStateChanged()
{
    updateHostDisplay (ChangeDetails{}.withNonParameterStateChanged (true));
}

int SongsmithEQAudioProcessor::getNumUsedBands() const noexcept
{
    int n = 0;
    for (auto& u : bandUsed)
        n += u.load() ? 1 : 0;
    return n;
}

int SongsmithEQAudioProcessor::findFreeBand() const noexcept
{
    for (int b = 0; b < numBands; ++b)
        if (! bandUsed[(size_t) b].load())
            return b;
    return -1;
}

//==============================================================================
bool SongsmithEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    // Optional sidechain (analyser only): disabled, mono or stereo.
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

bool SongsmithEQAudioProcessor::isSidechainConnected() const
{
    auto* bus = getBus (true, 1);
    return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
}

void SongsmithEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);

    // Larger host blocks are processed in chunks of this size, so nothing is allocated later.
    dryBuffer.setSize (EQ::Engine::maxChannels, juce::jmax (512, samplesPerBlock), false, true, false);
    dryBuffer.clear();

    outputGain.reset (sampleRate, outputGainSmoothingSeconds);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainRaw->load()));
    bypassMix.reset (sampleRate, bypassFadeSeconds);
    bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.0f : 0.0f);

    autoGain.prepare (sampleRate);
    autoGainSmoothed.reset (sampleRate, autoGainSmoothingSeconds);
    autoGainSmoothed.setCurrentAndTargetValue ((float) juce::Decibels::decibelsToGain (autoGainParam->get() ? autoGain.getCorrectionDb() : 0.0));
    autoGainWasOn = autoGainParam->get();

    // Linear phase: everything sized for the longest FIR at this rate up front.
    currentSampleRate = sampleRate;
    linearPhase.prepare (sampleRate, dryBuffer.getNumSamples());
    lpActive = isLinearPhaseSelected();
    lpLength = getLinearPhaseLength();

    // Oversampling: size every stage for the chunk size now (no allocation later).
    for (size_t k = 1; k < oversamplers.size(); ++k)
    {
        oversamplers[k]->initProcessing ((size_t) dryBuffer.getNumSamples());
        oversamplers[k]->reset();
        oversamplerLatency[k] = juce::roundToInt (oversamplers[k]->getLatencyInSamples());
    }
    sidechainUp.setSize (EQ::Engine::maxChannels, dryBuffer.getNumSamples() * 4, false, true, false);
    osActive = lpActive ? 0 : getOversamplingIndex();
    engine.prepare (sampleRate * (1 << osActive));
    modeFade.reset (sampleRate, 0.01);
    modeFade.setCurrentAndTargetValue (1.0f);
    dryDelay.setSize (EQ::Engine::maxChannels,
                      juce::jmax (EQ::LinearPhaseEQ::firLength (EQ::LinearPhaseEQ::numQualities - 1, sampleRate) / 2,
                                  oversamplerLatency[1], oversamplerLatency[2]) + 1,
                      false, true, false);
    dryDelay.clear();
    dryDelayPos = 0;
    updateLatencyNow();
    if (lpActive)
        designLinearPhaseNow();
}

void SongsmithEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    // Main bus only: the sidechain (input bus 1) is analysed, never processed or output.
    const int numIn  = getMainBusNumInputChannels();
    const int numOut = getMainBusNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    using EQ::Param;
    const float gainScale = getGainScale();
    for (int b = 0; b < numBands; ++b)
    {
        const auto& raw = rawValues[(size_t) b];

        EQ::BandSettings s;
        s.active = bandUsed[(size_t) b].load (std::memory_order_relaxed) && raw[(size_t) Param::Enabled]->load() >= 0.5f;
        s.type   = EQ::typeFromIndex ((int) std::lround (raw[(size_t) Param::Type]->load()));
        s.freq   = raw[(size_t) Param::Freq]->load();
        // The scale multiplies the dB gain on its way into the engine (stored values stay
        // intact) and goes through the engine's gain ramp, so moving it is click-free.
        s.gain   = EQ::typeHasGain (s.type) ? raw[(size_t) Param::Gain]->load() * gainScale : 0.0f;
        s.q      = raw[(size_t) Param::Q]->load();
        s.slope  = (int) std::lround (raw[(size_t) Param::Slope]->load());
        s.stereo = EQ::stereoModeFromIndex ((int) std::lround (raw[(size_t) Param::Stereo]->load()));
        s.dynOn       = raw[(size_t) Param::DynOn]->load() >= 0.5f;
        s.dynExternal = raw[(size_t) Param::DynExternal]->load() >= 0.5f;
        s.threshold   = raw[(size_t) Param::Threshold]->load();
        s.range       = raw[(size_t) Param::Range]->load() * gainScale;   // the curve scale applies to dynamics too
        s.attackMs    = raw[(size_t) Param::Attack]->load();
        s.releaseMs   = raw[(size_t) Param::Release]->load();
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

    const bool agOn = autoGainParam->get();
    if (agOn && ! autoGainWasOn)
    {
        autoGain.reset();              // fresh measurement window
        autoGain.setCorrectionDb (0.0);   // start from unity and glide to the match
    }
    autoGainWasOn = agOn;

    // Processing path: linear phase, or the zero-latency engine at 1x / 2x / 4x.
    const bool wantLP = isLinearPhaseSelected();
    const int wantOS = wantLP ? 0 : getOversamplingIndex();
    auto pathChangePending = [&] { return wantLP != lpActive || wantOS != osActive; };
    if (pathChangePending())
        modeFade.setTargetValue (0.0f);   // fade out; the path switches once silent
    float blockPeak = 0.0f;

    for (int pos = 0; pos < total; pos += chunk)
    {
        const int num = juce::jmin (chunk, total - pos);

        float* ch[EQ::Engine::maxChannels] = {};
        for (int c = 0; c < numCh; ++c)
            ch[c] = buffer.getWritePointer (c, pos);

        if (capture && analyserMode == SpectrumAnalyser::Mode::Pre)
            analyser.push (ch, numCh, num);

        // Path switch (phase mode or oversampling factor): once the fade-out has reached
        // silence, change path and fade back in.
        if (pathChangePending() && modeFade.getCurrentValue() <= 0.0f && ! modeFade.isSmoothing())
        {
            lpActive = wantLP;
            lpLength = getLinearPhaseLength();
            if (lpActive)
            {
                linearPhase.reset();
            }
            else
            {
                if (wantOS != osActive)
                    engine.prepare (currentSampleRate * (1 << wantOS));   // no allocation; bands re-snap
                engine.reset();
            }
            osActive = wantOS;
            if (osActive > 0)
                oversamplers[(size_t) osActive]->reset();
            dryDelay.clear();
            dryDelayPos = 0;
            modeFade.setTargetValue (1.0f);
        }

        // The engine always runs, even when bypassed, so un-bypassing never glitches.
        const bool needDry = bypassMix.isSmoothing() || bypassMix.getTargetValue() > 0.0f;
        const int pathLatency = lpActive ? (lpLength = getLinearPhaseLength()) / 2 : oversamplerLatency[(size_t) osActive];
        if (pathLatency > 0)
        {
            // The output is delayed by the path latency, so the bypass dry signal is delayed
            // to match (the ring always runs, so it is valid whenever bypass engages).
            const int size = dryDelay.getNumSamples();
            const int delay = juce::jmin (pathLatency, size - 1);
            for (int c = 0; c < numCh; ++c)
            {
                auto* ring = dryDelay.getWritePointer (c);
                auto* dry = dryBuffer.getWritePointer (c);
                int w = dryDelayPos;
                for (int i = 0; i < num; ++i)
                {
                    const float delayed = ring[(w - delay + size) % size];
                    ring[w] = ch[c][i];
                    dry[i] = delayed;
                    w = (w + 1) % size;
                }
            }
            dryDelayPos = (dryDelayPos + num) % size;
        }
        else if (needDry)
        {
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::copy (dryBuffer.getWritePointer (c), ch[c], num);
        }

        if (agOn)
            autoGain.measureInput (ch, numCh, num);

        if (lpActive)
        {
            linearPhase.process (ch, numCh, num);   // dynamics and solo need the zero-latency engine
        }
        else
        {
            const int factor = 1 << osActive;

            // Sidechain for dynamic bands set to external, at the engine's rate (held for
            // 'factor' samples when oversampling; the detector's band-pass removes the images).
            const float* scPtrs[EQ::Engine::maxChannels] = {};
            int numScCh = 0;
            if (isSidechainConnected())
            {
                auto scBus = getBusBuffer (buffer, true, 1);
                numScCh = juce::jmin (scBus.getNumChannels(), EQ::Engine::maxChannels);
                for (int c = 0; c < numScCh; ++c)
                {
                    const float* src = scBus.getReadPointer (c, pos);
                    if (factor == 1)
                    {
                        scPtrs[c] = src;
                    }
                    else
                    {
                        auto* up = sidechainUp.getWritePointer (c);
                        for (int i = 0; i < num; ++i)
                            for (int r = 0; r < factor; ++r)
                                up[i * factor + r] = src[i];
                        scPtrs[c] = up;
                    }
                }
            }
            engine.setSidechain (numScCh > 0 ? scPtrs : nullptr, numScCh);

            if (factor == 1)
            {
                engine.process (ch, numCh, num);
            }
            else
            {
                auto& os = *oversamplers[(size_t) osActive];
                juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) num);
                auto up = os.processSamplesUp (block);
                float* upCh[EQ::Engine::maxChannels] = {};
                for (int c = 0; c < numCh; ++c)
                    upCh[c] = up.getChannelPointer ((size_t) c);
                engine.process (upCh, numCh, (int) up.getNumSamples());
                os.processSamplesDown (block);
            }
        }

        if (modeFade.isSmoothing() || modeFade.getTargetValue() < 1.0f)
            for (int i = 0; i < num; ++i)
            {
                const float g = modeFade.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] *= g;
            }

        // Auto gain: compare the EQ's output with its input, then apply the smoothed
        // correction before the output gain. Off and settled = untouched (bit-exact).
        if (agOn)
        {
            autoGain.measureOutput (ch, numCh, num);
            autoGain.advance (num);
            autoGainSmoothed.setTargetValue ((float) juce::Decibels::decibelsToGain (autoGain.getCorrectionDb()));
        }
        else
        {
            autoGainSmoothed.setTargetValue (1.0f);
        }

        if (autoGainSmoothed.isSmoothing())
        {
            for (int i = 0; i < num; ++i)
            {
                const float g = autoGainSmoothed.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    ch[c][i] *= g;
            }
        }
        else if (const float g = autoGainSmoothed.getTargetValue(); g != 1.0f)
        {
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::multiply (ch[c], g, num);
        }

        if (outputGain.isSmoothing() || bypassMix.isSmoothing())
        {
            // The dry copy is only valid when needDry: never mix in stale buffer contents.
            for (int i = 0; i < num; ++i)
            {
                const float g = outputGain.getNextValue();
                const float m = bypassMix.getNextValue();

                for (int c = 0; c < numCh; ++c)
                {
                    const float wet = ch[c][i] * g;
                    ch[c][i] = needDry ? wet + (dryBuffer.getSample (c, i) - wet) * m : wet;
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
    autoGainAppliedDb.store (juce::Decibels::gainToDecibels (autoGainSmoothed.getCurrentValue(), -100.0f), std::memory_order_relaxed);

    for (int b = 0; b < numBands; ++b)
        dynamicGainDb[(size_t) b].store (engine.getDynamicGainDb (b), std::memory_order_relaxed);

    // Sidechain: level for the UI, and the spectrum while a viewer shows it. It never
    // touches the main channels.
    if (isSidechainConnected())
    {
        const auto sc = getBusBuffer (buffer, true, 1);
        const int numSc = juce::jmin (sc.getNumChannels(), 2);
        float scPeak = 0.0f;
        for (int c = 0; c < numSc; ++c)
            scPeak = juce::jmax (scPeak, sc.getMagnitude (c, 0, sc.getNumSamples()));
        sidechainMeter.push (scPeak);

        if (capture && sidechainView.load (std::memory_order_relaxed) > 0)
            sidechainAnalyser.push (sc.getArrayOfReadPointers(), numSc, sc.getNumSamples());
    }
}

//==============================================================================
juce::AudioProcessorEditor* SongsmithEQAudioProcessor::createEditor()
{
    return new SongsmithEQAudioProcessorEditor (*this);
}

juce::ValueTree SongsmithEQAudioProcessor::createPresetState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree slots (slotsId);
    for (int b = 0; b < numBands; ++b)
        slots.setProperty ("b" + juce::String (b + 1), isBandUsed (b), nullptr);
    state.appendChild (slots, nullptr);
    return state;
}

void SongsmithEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = createPresetState();

    juce::ValueTree ui (uiId);
    presets.saveToUiState (ui);
    ui.setProperty ("selectedBand", selectedBand.load(), nullptr);
    ui.setProperty ("displayRange", displayRangeDb.load(), nullptr);
    ui.setProperty ("curveView", curveView.load(), nullptr);
    ui.setProperty ("pianoStrip", showPianoStrip.load(), nullptr);
    ui.setProperty ("sidechainView", sidechainView.load(), nullptr);
    ui.setProperty ("analyserTilt", analyserTilt.load(), nullptr);
    ui.setProperty ("analyserTop", analyserTopDb.load(), nullptr);
    ui.setProperty ("analyserBottom", analyserBottomDb.load(), nullptr);
    ui.setProperty ("analyserFftOrder", analyser.getFftOrder(), nullptr);
    ui.setProperty ("analyserSpeed", analyser.getSpeed(), nullptr);
    ui.setProperty ("analyserPeakHold", analyser.isPeakHold(), nullptr);
    ui.setProperty ("analyserMode", analyser.mode.load(), nullptr);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("editorHeight", editorHeight.load(), nullptr);
    ui.setProperty ("uiScale", uiScale.load(), nullptr);
    state.appendChild (ui, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithEQAudioProcessor::restoreFromValueTree (const juce::ValueTree& saved)
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
        curveView.store (juce::jlimit (0, EQ::numStereoModes - 1, (int) ui.getProperty ("curveView", 0)));
        showPianoStrip.store ((bool) ui.getProperty ("pianoStrip", false));
        sidechainView.store (juce::jlimit (0, 2, (int) ui.getProperty ("sidechainView", 0)));
        analyserTilt.store (juce::jlimit (0.0f, 6.0f, (float) ui.getProperty ("analyserTilt", 4.5f)));
        analyserTopDb.store (juce::jlimit (-30.0f, 12.0f, (float) ui.getProperty ("analyserTop", 0.0f)));
        analyserBottomDb.store (juce::jlimit (-150.0f, -40.0f, (float) ui.getProperty ("analyserBottom", -90.0f)));
        setAnalyserFftOrder ((int) ui.getProperty ("analyserFftOrder", SpectrumAnalyser::fftOrder));
        setAnalyserSpeed ((int) ui.getProperty ("analyserSpeed", 1));
        analyser.setPeakHold ((bool) ui.getProperty ("analyserPeakHold", false));
        analyser.mode.store (juce::jlimit (0, 2, (int) ui.getProperty ("analyserMode", (int) SpectrumAnalyser::Mode::Post)));
        // Sessions saved before the UI size setting open at 100 %. Sizes are clamped to the
        // current limits (older versions allowed 760 x 440 to 2000 x 1200).
        uiScale.store (EQ::EditorSize::snapUiScale ((float) ui.getProperty ("uiScale", 1.0f)));
        const auto size = EQ::EditorSize::constrain ((int) ui.getProperty ("editorWidth", EQ::EditorSize::defaultWidth),
                                                     (int) ui.getProperty ("editorHeight", EQ::EditorSize::defaultHeight),
                                                     uiScale.load());
        editorWidth.store (size.x);
        editorHeight.store (size.y);
    }

    presets.restoreFromUiState (ui);   // invalid tree -> no current preset

    if (juce::MessageManager::existsAndIsCurrentThread())
        edits.getUndoManager().clearUndoHistory();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithEQAudioProcessor();
}

//==============================================================================
// UI size: per session (the UI child of the state) and per machine (a small settings file next
// to the presets folder, so the next new instance opens at the size the user last chose).
juce::File SongsmithEQAudioProcessor::getMachineSettingsFile()
{
    return EQ::PresetManager::getDefaultUserFolder().getParentDirectory().getChildFile ("Settings.xml");
}

float SongsmithEQAudioProcessor::loadMachineUiScale()
{
    juce::PropertiesFile settings (getMachineSettingsFile(), {});
    return EQ::EditorSize::snapUiScale ((float) settings.getDoubleValue ("uiScale", 1.0));
}

void SongsmithEQAudioProcessor::saveMachineUiScale (float scale)
{
    juce::PropertiesFile::Options options;
    options.millisecondsBeforeSaving = 0;   // write now: another instance may open next
    juce::PropertiesFile settings (getMachineSettingsFile(), options);
    settings.setValue ("uiScale", (double) EQ::EditorSize::snapUiScale (scale));
    settings.saveIfNeeded();
}

void SongsmithEQAudioProcessor::setUiScale (float scale)
{
    scale = EQ::EditorSize::snapUiScale (scale);
    if (std::abs (scale - uiScale.load()) < 1.0e-4f)
        return;

    uiScale.store (scale);
    saveMachineUiScale (scale);
    notifyNonParameterStateChanged();
}
