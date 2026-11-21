#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include "AnalyzerParams.h"
#include "AnalysisMath.h"
#include "Session.h"
#include "Slots.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithOutputMeter.h"

// Songsmith Plugin Analyzer (ANALYZER-01): measures what another plugin does to the frequency response, inside the host.
//
// Put one instance as the Before Plugin (it used to be called the Reference) before the plugin and one as the After Plugin (the Result)
// after it. They find each other on their own. Press Start on the After Plugin: the Before Plugin sends two logarithmic sweeps through the
// plugin, the After Plugin records what arrives, and the curve appears as a capture. A second capture is drawn over the first.
// While measuring, the Before Plugin replaces the sound with the sweep. When it is not measuring, both leave the sound untouched.
//
// Real-time rules: processBlock allocates nothing, takes no lock, writes no file and does no analysis. The test program and the
// capture buffer are made in prepareToPlay. The analysis runs on its own thread when a capture is complete.
class SongsmithPluginAnalyzerProcessor : public juce::AudioProcessor,
                                         private juce::AsyncUpdater,
                                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    // Saved-state format. 1 = the settings, four named measurement slots, which were drawn and compared, and the editor's width.
    // 2 (ANALYZER-02) = the settings, two captures with whether each is shown, and the editor's width. Version 1 opens as version 2:
    // its filled slots become the two captures, in order.
    static constexpr int stateVersion = 2;

    SongsmithPluginAnalyzerProcessor();
    ~SongsmithPluginAnalyzerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { audioReset.store (true); }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // A measurement tool has no starting points.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // --- What the musician does (message thread) ------------------------------------------------------
    PluginAnalyzer::Role getRole() const noexcept { return juce::roundToInt (roleRaw->load()) == 0 ? PluginAnalyzer::Role::reference : PluginAnalyzer::Role::result; }
    int getSessionIndex() const noexcept { return juce::jlimit (0, PluginAnalyzer::numSessions - 1, juce::roundToInt (sessionRaw->load())); }
    int getChannelIndex() const noexcept { return juce::jlimit (0, 2, juce::roundToInt (channelRaw->load())); }
    float getTestLevelDb() const noexcept { return levelRaw->load(); }
    bool isBypassed() const noexcept { return bypassParam->get(); }

    void requestStart();     // begin a measurement: the Before Plugin plays its sweeps and the After Plugin records
    void requestStop();      // stop it early; nothing is kept
    void clearCaptures();    // stop, and empty both captures (the editor asks first)

    // --- Finding the other half of the pair (ANALYZER-02) ------------------------------------------------
    // Puts this instance in the session where its other half is waiting, or in a free one. Runs when the plugin is made and when its
    // role is changed by hand, never when a saved session opens (that keeps the pairing the project was saved with).
    void autoPair();
    // Switched off by the tests, which place instances by hand; always on in a real host.
    static inline std::atomic<bool> automaticPairing { true };
    // True when the musician has to say which pair this belongs to: more than one Before Plugin or After Plugin in the session, or the
    // other half is somewhere else. The editor shows the Pair With box only then.
    bool offersPairing() const;
    // For each of the eight pairs: how many Before Plugins and After Plugins are in it (this instance included).
    std::array<PluginAnalyzer::Occupancy, (size_t) PluginAnalyzer::numSessions> getPairOccupancy() const;

    // --- Status, in words ------------------------------------------------------------------------------
    enum class Status { idle, sending, recording, working, finished, problem };
    struct StatusInfo { Status status = Status::idle; juce::String text; float progress = 0.0f; };
    StatusInfo getStatus() const;

    // --- The measurements -------------------------------------------------------------------------------
    PluginAnalyzer::Slot& getSlot (int index) noexcept { return slots[(size_t) juce::jlimit (0, PluginAnalyzer::numSlots - 1, index)]; }
    const PluginAnalyzer::Slot& getSlot (int index) const noexcept { return slots[(size_t) juce::jlimit (0, PluginAnalyzer::numSlots - 1, index)]; }
    int measureSlot = 0;      // the capture the next measurement goes into
    bool captureVisible[PluginAnalyzer::numSlots] { true, true };   // whether each capture is drawn (hiding one never deletes it)
    int numFilledCaptures() const noexcept;
    int firstEmptyCapture() const noexcept;   // -1 when both are in use
    // Chooses where the next measurement goes, and the name to give it ("" keeps the capture's own name). Nothing is overwritten
    // until a measurement has finished and has a curve.
    void setNextCapture (int slot, const juce::String& name);
    // Set when a new measurement could not be made and an existing capture was kept instead; empty otherwise.
    juce::String getKeptNotice() const { return keptNotice; }

    // Picks up a finished capture (starts the analysis) and a finished analysis (fills the slot). Call it from a timer on the
    // message thread. Returns true when something changed that the editor should redraw.
    bool poll();
    // True while the curve is being worked out.
    bool isAnalysing() const noexcept { return analysing.load(); }
    // Counts finished measurements (for tests and the editor).
    int getMeasurementCount() const noexcept { return measurementCount.load(); }

    // For tests: how much of the capture buffer has been filled, and the test program's length.
    int getCapturedSamples() const noexcept { return capturePosition.load(); }
    int getProgramLength() const noexcept { return (int) program.size(); }
    int getCaptureLength() const noexcept { return PluginAnalyzer::captureSamples (spec, currentSampleRate); }
    const PluginAnalyzer::SweepSpec& getSweepSpec() const noexcept { return spec; }

    // The text of the CSV file for every capture that has been made.
    juce::String makeCsvText() const;

    // Editor state saved with the session (not automatable): window width.
    std::atomic<int> editorWidth { PluginAnalyzer::EditorLayout::defaultWidth };

    // SUI-23: what leaves the plugin, per channel, for the far-right Output meter (lock-free, read by the editor).
    Songsmith::UI::OutputMeterState outputMeter;
    Songsmith::Diagnostics::LoadMeter loadMeter;

    juce::ValueTree createState();
    void restoreFromValueTree (const juce::ValueTree& saved);

    static inline const juce::Identifier versionId { "stateVersion" }, uiId { "UI" };

    // For tests: how many instances are in this session as a Before Plugin and as an After Plugin.
    static int referencesIn (int session) noexcept { return PluginAnalyzer::sessionNumber (session).references.load(); }
    static int resultsIn (int session) noexcept { return PluginAnalyzer::sessionNumber (session).results.load(); }

private:
    class AnalysisThread;

    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void bindSession();    // enters the session and role the parameters now say (message thread)
    void leaveSession();
    void startAnalysis();
    PluginAnalyzer::Session* sessionPtr() const noexcept { return session.load (std::memory_order_acquire); }

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* roleRaw = nullptr;
    std::atomic<float>* sessionRaw = nullptr;
    std::atomic<float>* channelRaw = nullptr;
    std::atomic<float>* levelRaw = nullptr;

    // Which session and role this instance has registered. Changed on the message thread only.
    int boundSession = -1, boundRole = -1;
    std::atomic<PluginAnalyzer::Session*> session { nullptr };

    PluginAnalyzer::SweepSpec spec;
    double currentSampleRate = 48000.0;
    std::vector<float> program;    // the test program at unit level (the Reference)
    std::vector<float> capture;    // what the Result records

    // Audio thread only:
    bool playing = false;
    int playPosition = 0;
    float playGain = 1.0f;
    std::uint32_t seenGeneration = 0;
    std::uint32_t captureGeneration = 0;
    bool capturing = false;

    std::atomic<bool> audioReset { false };

    // Shared between the audio thread and the editor:
    static constexpr int captureIdle = 0, captureRecording = 1, captureReady = 2;
    std::atomic<int> captureState { captureIdle };
    std::atomic<int> capturePosition { 0 };
    std::atomic<float> captureLevelDb { -18.0f };
    std::atomic<int> captureChannel { 0 };
    std::atomic<bool> captureRateMismatch { false };

    std::array<PluginAnalyzer::Slot, (size_t) PluginAnalyzer::numSlots> slots;
    std::unique_ptr<AnalysisThread> analysisThread;
    std::atomic<bool> analysing { false };
    std::atomic<int> measurementCount { 0 };
    bool justFinished = false;
    juce::String pendingName, keptNotice;

    // A finished analysis waits here until the editor's timer (poll) moves it into its slot.
    juce::CriticalSection pendingLock;
    PluginAnalyzer::Measurement pending;
    juce::String pendingChannel;
    bool pendingReady = false;
    std::atomic<bool> analysisFinished { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPluginAnalyzerProcessor)
};
