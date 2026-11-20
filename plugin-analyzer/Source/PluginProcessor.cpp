#include "PluginProcessor.h"
#include "PluginEditor.h"

// Hosts identify the plug-in by these codes (VST3 class ID). They must never change, or saved sessions stop finding the plug-in.
// See plugin-analyzer/CMakeLists.txt.
#if defined (JucePlugin_ManufacturerCode) && defined (JucePlugin_PluginCode)
static_assert (JucePlugin_ManufacturerCode == 0x546e7977 /* 'Tnyw' */ && JucePlugin_PluginCode == 0x416e6c31 /* 'Anl1' */,
               "PLUGIN_MANUFACTURER_CODE / PLUGIN_CODE changed: existing sessions would no longer load");
#endif

using namespace PluginAnalyzer;

//==============================================================================
// The analysis runs here, never on the audio thread or the message thread.
class SongsmithPluginAnalyzerProcessor::AnalysisThread : public juce::Thread
{
public:
    AnalysisThread (SongsmithPluginAnalyzerProcessor& o, std::vector<float> samples, int length, SweepSpec s, double rate, float level, juce::String channelName)
        : juce::Thread ("Songsmith Plugin Analyzer: working out the curve"), owner (o), data (std::move (samples)), numSamples (length),
          sweepSpec (s), sampleRate (rate), levelDb (level), channel (std::move (channelName))
    {}

    void run() override
    {
        auto m = analyse (data, numSamples, sweepSpec, sampleRate, levelDb);
        {
            const juce::ScopedLock lock (owner.pendingLock);
            owner.pending = std::move (m);
            owner.pendingChannel = channel;
            owner.pendingReady = true;
        }
        owner.analysisFinished.store (true);
    }

private:
    SongsmithPluginAnalyzerProcessor& owner;
    std::vector<float> data;
    int numSamples;
    SweepSpec sweepSpec;
    double sampleRate;
    float levelDb;
    juce::String channel;
};

//==============================================================================
SongsmithPluginAnalyzerProcessor::SongsmithPluginAnalyzerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    roleRaw    = apvts.getRawParameterValue (roleId);
    sessionRaw = apvts.getRawParameterValue (sessionId);
    channelRaw = apvts.getRawParameterValue (channelId);
    levelRaw   = apvts.getRawParameterValue (levelId);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (bypassId));
    jassert (roleRaw && sessionRaw && channelRaw && levelRaw && bypassParam);

    for (int i = 0; i < numSlots; ++i)
        slots[(size_t) i].clear (i);

    apvts.addParameterListener (roleId, this);
    apvts.addParameterListener (sessionId, this);
    bindSession();
}

SongsmithPluginAnalyzerProcessor::~SongsmithPluginAnalyzerProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (roleId, this);
    apvts.removeParameterListener (sessionId, this);
    if (analysisThread != nullptr)
        analysisThread->stopThread (10000);
    leaveSession();
}

juce::AudioProcessorValueTreeState::ParameterLayout SongsmithPluginAnalyzerProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { roleId, 1 }, "Role", roleNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { sessionId, 1 }, "Session", sessionNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { channelId, 1 }, "Channel", channelNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelId, 1 }, "Test Level",
        juce::NormalisableRange<float> (minTestLevelDb, maxTestLevelDb, 0.5f), defaultTestLevelDb,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypassId, 1 }, "Bypass", false));
    return layout;
}

//==============================================================================
// Which session and role this instance is in. Counting them is how a Result knows whether a Reference exists.
void SongsmithPluginAnalyzerProcessor::leaveSession()
{
    if (boundSession < 0)
        return;
    auto& s = sessionNumber (boundSession);
    (boundRole == (int) Role::reference ? s.references : s.results).fetch_sub (1);
    if (s.references.load() <= 0 && s.results.load() <= 0)
        s.clear();
    session.store (nullptr, std::memory_order_release);
    boundSession = -1;
    boundRole = -1;
}

void SongsmithPluginAnalyzerProcessor::bindSession()
{
    const int newSession = getSessionIndex(), newRole = (int) getRole();
    if (newSession == boundSession && newRole == boundRole)
        return;
    leaveSession();
    auto& s = sessionNumber (newSession);
    (newRole == (int) Role::reference ? s.references : s.results).fetch_add (1);
    boundSession = newSession;
    boundRole = newRole;
    session.store (&s, std::memory_order_release);
    audioReset.store (true);   // the audio thread starts again from the new session's state
    captureState.store (captureIdle);
}

void SongsmithPluginAnalyzerProcessor::parameterChanged (const juce::String&, float)
{
    // May be called from any thread (host automation), so only ask for the work to be done on the message thread.
    triggerAsyncUpdate();
}

void SongsmithPluginAnalyzerProcessor::handleAsyncUpdate() { bindSession(); }

//==============================================================================
bool SongsmithPluginAnalyzerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SongsmithPluginAnalyzerProcessor::prepareToPlay (double sampleRate, int)
{
    // Both buffers are made here, so processBlock never allocates. (About 4 MB each at 48 kHz.)
    currentSampleRate = sampleRate;
    program = makeProgram (spec, sampleRate, 1.0f);
    capture.assign ((size_t) captureSamples (spec, sampleRate) + 16, 0.0f);
    capturePosition.store (0);
    captureState.store (captureIdle);
    captureRateMismatch.store (false);
    audioReset.store (true);

    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, getMainBusNumOutputChannels()), std::memory_order_relaxed);
    outputMeter.resetLatch (sampleRate);
    setLatencySamples (0);
}

void SongsmithPluginAnalyzerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const auto loadScope = loadMeter.measure (buffer.getNumSamples(), getSampleRate());
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels(), num = buffer.getNumSamples();
    auto* s = sessionPtr();

    if (audioReset.exchange (false))
    {
        if (playing && s != nullptr)   // a Reference that was sending: end the measurement for everyone
        {
            s->measuring.store (false);
            s->abortedGeneration.store (s->generation.load());
        }
        playing = false;
        capturing = false;
        seenGeneration = s != nullptr ? s->generation.load() : 0;
    }

    const bool bypassed = bypassParam->get();
    if (s != nullptr && ! program.empty() && ! capture.empty())
    {
        const bool isReference = juce::roundToInt (roleRaw->load (std::memory_order_relaxed)) == 0;

        if (isReference)
        {
            // Start: only when exactly one Reference is in the session and Bypass is off.
            if (! playing && s->startRequested.exchange (false) && ! bypassed && s->references.load() == 1)
            {
                playing = true;
                playPosition = 0;
                const float level = levelRaw->load (std::memory_order_relaxed);
                playGain = std::pow (10.0f, level / 20.0f);
                s->testLevelDb.store (level);
                s->sampleRate.store (currentSampleRate);
                s->programLength.store ((std::int64_t) program.size());
                s->sentSamples.store (0);
                s->stopRequested.store (false);
                s->measuring.store (true);
                s->generation.fetch_add (1);
            }

            if (playing)
            {
                if (bypassed || s->stopRequested.exchange (false))
                {
                    playing = false;
                    s->measuring.store (false);
                    s->abortedGeneration.store (s->generation.load());
                }
                else
                {
                    // The sweep replaces the sound on every channel; after the last sample, the sound goes back to normal.
                    const int programSize = (int) program.size();
                    for (int i = 0; i < num; ++i)
                    {
                        const float v = playPosition < programSize ? program[(size_t) playPosition] * playGain : 0.0f;
                        for (int c = 0; c < numCh; ++c)
                            buffer.getWritePointer (c)[i] = v;
                        ++playPosition;
                    }
                    s->sentSamples.store ((std::int64_t) juce::jmin (playPosition, programSize));
                    if (playPosition >= programSize)
                    {
                        playing = false;
                        s->measuring.store (false);
                    }
                }
            }
        }
        else if (! bypassed)
        {
            // Result: a new generation means a measurement has started, so start recording at once.
            const auto generation = s->generation.load();
            if (generation != seenGeneration)
            {
                seenGeneration = generation;
                capturing = true;
                captureGeneration = generation;
                capturePosition.store (0);
                captureLevelDb.store (s->testLevelDb.load());
                captureChannel.store (juce::jlimit (0, 2, juce::roundToInt (channelRaw->load (std::memory_order_relaxed))));
                captureRateMismatch.store (std::abs (s->sampleRate.load() - currentSampleRate) > 0.5);
                captureState.store (captureRecording);
            }

            if (capturing)
            {
                if (s->abortedGeneration.load() == captureGeneration)
                {
                    capturing = false;
                    captureState.store (captureIdle);
                }
                else
                {
                    const int length = captureSamples (spec, currentSampleRate);
                    int pos = capturePosition.load (std::memory_order_relaxed);
                    const int channelChoice = captureChannel.load (std::memory_order_relaxed);
                    const int ch0 = juce::jmin (channelChoice == 1 ? 1 : 0, numCh - 1), ch1 = juce::jmin (1, numCh - 1);
                    const auto* left = buffer.getReadPointer (ch0);
                    const auto* right = buffer.getReadPointer (ch1);
                    for (int i = 0; i < num && pos < length; ++i, ++pos)
                        capture[(size_t) pos] = channelChoice == 2 ? 0.5f * (buffer.getReadPointer (0)[i] + right[i]) : left[i];
                    capturePosition.store (pos, std::memory_order_relaxed);
                    if (pos >= length)
                    {
                        capturing = false;
                        captureState.store (captureReady);
                    }
                }
            }
        }
    }

    // What leaves the plug-in, for the Output meter.
    float blockPeak = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        const float channelPeak = buffer.getMagnitude (c, 0, num);
        blockPeak = juce::jmax (blockPeak, channelPeak);
        outputMeter.pushChannel (c, channelPeak);
    }
    outputMeter.push (blockPeak);
    outputMeter.channels.store (juce::jlimit (1, (int) Songsmith::UI::OutputMeterState::maxChannels, numCh), std::memory_order_relaxed);
    outputMeter.updateClipLatch (blockPeak, num);
}

//==============================================================================
void SongsmithPluginAnalyzerProcessor::requestStart()
{
    justFinished = false;
    if (auto* s = sessionPtr())
        if (! isBypassed())
            s->startRequested.store (true);
}

void SongsmithPluginAnalyzerProcessor::requestStop()
{
    justFinished = false;
    if (auto* s = sessionPtr())
        s->stopRequested.store (true);
    audioReset.store (true);
    captureState.store (captureIdle);
}

void SongsmithPluginAnalyzerProcessor::resetMeasurement()
{
    requestStop();
    getSlot (measureSlot).clear (measureSlot);
    justFinished = false;
}

SongsmithPluginAnalyzerProcessor::StatusInfo SongsmithPluginAnalyzerProcessor::getStatus() const
{
    StatusInfo info;
    auto* s = sessionPtr();
    const auto sessionName = juce::String (getSessionIndex() + 1);
    auto problem = [&] (const juce::String& text) { info.status = Status::problem; info.text = text; return info; };

    if (s == nullptr)
        return problem ("This instance is not in a session.");
    if (isBypassed())
        return problem ("Bypass is on, so nothing is measured. Turn Bypass off to measure.");
    if (s->references.load() > 1)
        return problem ("More than one Reference is using Session " + sessionName + ". Give each Reference and Result pair its own session, or change the extra Reference to a Result.");
    if (getRole() == Role::result && s->references.load() == 0)
        return problem ("No Reference found in Session " + sessionName + ". Put a Reference before the plugin you want to measure, and choose the same session. "
                        "Some hosts load each plugin in its own process, and then two instances cannot find each other.");
    if (getRole() == Role::reference && s->results.load() == 0)
        return problem ("No Result found in Session " + sessionName + ". Put a Result after the plugin you want to measure, and choose the same session.");
    if (captureRateMismatch.load())
        return problem ("The Reference and the Result are running at different sample rates, so they cannot be compared.");

    if (s->measuring.load())
    {
        info.status = Status::sending;
        const auto total = (double) juce::jmax ((std::int64_t) 1, s->programLength.load());
        info.progress = (float) juce::jlimit (0.0, 1.0, (double) s->sentSamples.load() / total);
        info.text = getRole() == Role::reference ? "Sending the test sweeps. The host must be playing."
                                                 : "The Reference is sending the test sweeps. The host must be playing.";
        return info;
    }
    if (captureState.load() == captureRecording)
    {
        info.status = Status::recording;
        info.progress = (float) capturePosition.load() / (float) juce::jmax (1, getCaptureLength());
        info.text = "Recording what arrives, including the end of the sweeps.";
        return info;
    }
    if (captureState.load() == captureReady || analysing.load())
    {
        info.status = Status::working;
        info.text = "Working out the curve.";
        return info;
    }
    if (justFinished)
    {
        info.status = Status::finished;
        info.text = "Done. The curve is in " + getSlot (shownSlot).name + ".";
        return info;
    }
    info.text = "Ready. Start the host playing, then press Start.";
    return info;
}

//==============================================================================
void SongsmithPluginAnalyzerProcessor::startAnalysis()
{
    if (analysing.load() || captureState.load() != captureReady)
        return;
    const int length = getCaptureLength();
    std::vector<float> copy (capture.begin(), capture.begin() + (std::ptrdiff_t) juce::jmin ((int) capture.size(), length));
    const auto channelName = channelNames()[juce::jlimit (0, 2, captureChannel.load())];
    captureState.store (captureIdle);   // the audio thread may now start another capture

    if (analysisThread != nullptr)
        analysisThread->stopThread (10000);
    analysing.store (true);
    analysisFinished.store (false);
    analysisThread = std::make_unique<AnalysisThread> (*this, std::move (copy), length, spec, currentSampleRate, captureLevelDb.load(), channelName);
    analysisThread->startThread (juce::Thread::Priority::background);
}

bool SongsmithPluginAnalyzerProcessor::poll()
{
    bool changed = false;
    if (captureState.load() == captureReady && ! analysing.load())
    {
        startAnalysis();
        changed = true;
    }
    if (analysisFinished.exchange (false))
    {
        Measurement m;
        juce::String channelName;
        bool have = false;
        {
            const juce::ScopedLock lock (pendingLock);
            if (pendingReady)
            {
                m = std::move (pending);
                channelName = pendingChannel;
                pendingReady = false;
                have = true;
            }
        }
        if (have)
        {
            auto& slot = getSlot (measureSlot);
            slot.measurement = std::move (m);
            slot.filled = true;
            slot.channel = channelName;
            slot.measuredAt = juce::Time::getCurrentTime().toISO8601 (true);
            shownSlot = measureSlot;
            justFinished = true;
            measurementCount.fetch_add (1);
        }
        analysing.store (false);
        changed = true;
    }
    return changed;
}

juce::String SongsmithPluginAnalyzerProcessor::makeCsvText() const
{
    std::vector<const Slot*> chosen;
    if (getSlot (shownSlot).filled)
        chosen.push_back (&getSlot (shownSlot));
    if (comparedSlot >= 0 && comparedSlot != shownSlot && getSlot (comparedSlot).filled)
        chosen.push_back (&getSlot (comparedSlot));
    if (chosen.empty())
        return {};
    return makeCsv (chosen, JucePlugin_VersionString);
}

//==============================================================================
juce::AudioProcessorEditor* SongsmithPluginAnalyzerProcessor::createEditor()
{
    return new SongsmithPluginAnalyzerEditor (*this);
}

juce::ValueTree SongsmithPluginAnalyzerProcessor::createState()
{
    auto state = apvts.copyState();
    state.setProperty (versionId, stateVersion, nullptr);

    juce::ValueTree ui (uiId);
    ui.setProperty ("editorWidth", editorWidth.load(), nullptr);
    ui.setProperty ("measureSlot", measureSlot, nullptr);
    ui.setProperty ("shownSlot", shownSlot, nullptr);
    ui.setProperty ("comparedSlot", comparedSlot, nullptr);
    for (int i = 0; i < numSlots; ++i)
        ui.appendChild (slotToTree (slots[(size_t) i], i), nullptr);
    state.appendChild (ui, nullptr);
    return state;
}

void SongsmithPluginAnalyzerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SongsmithPluginAnalyzerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restoreFromValueTree (juce::ValueTree::fromXml (*xml));
}

void SongsmithPluginAnalyzerProcessor::restoreFromValueTree (const juce::ValueTree& saved)
{
    // Rebuild a complete parameter tree: saved values where present, defaults otherwise. A state from a newer version keeps the
    // parameters this version knows, by ID, and ignores the rest.
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

    const auto ui = saved.getChildWithName (uiId);
    editorWidth.store (EditorLayout::constrainWidth (ui.isValid() ? (int) ui.getProperty ("editorWidth", EditorLayout::defaultWidth) : EditorLayout::defaultWidth));
    measureSlot = ui.isValid() ? juce::jlimit (0, numSlots - 1, (int) ui.getProperty ("measureSlot", 0)) : 0;
    shownSlot = ui.isValid() ? juce::jlimit (0, numSlots - 1, (int) ui.getProperty ("shownSlot", 0)) : 0;
    comparedSlot = ui.isValid() ? juce::jlimit (-1, numSlots - 1, (int) ui.getProperty ("comparedSlot", -1)) : -1;
    for (int i = 0; i < numSlots; ++i)
        slots[(size_t) i].clear (i);
    if (ui.isValid())
        for (auto slotTree : ui)
            if (slotTree.hasType ("SLOT"))
            {
                const int index = (int) slotTree.getProperty ("index", -1);
                if (index >= 0 && index < numSlots)
                    slotFromTree (slots[(size_t) index], slotTree, index);
            }
    justFinished = false;
    bindSession();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SongsmithPluginAnalyzerProcessor();
}
