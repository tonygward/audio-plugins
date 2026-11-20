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
// Put one instance as the Reference before the plugin and one as the Result after it, in the same Session. Press Start (on either):
// the Reference sends two logarithmic sweeps through the plugin, the Result records what arrives, and the curve appears.
// While measuring, the Reference replaces the sound with the sweep. When it is not measuring, both leave the sound untouched.
//
// Real-time rules: processBlock allocates nothing, takes no lock, writes no file and does no analysis. The test program and the
// capture buffer are made in prepareToPlay. The analysis runs on its own thread when a capture is complete.
class SongsmithPluginAnalyzerProcessor : public juce::AudioProcessor,
                                         private juce::AsyncUpdater,
                                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    // Saved-state format. 1 = the settings, the four measurement slots and the editor's width.
    static constexpr int stateVersion = 1;

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

    void requestStart();     // begin a measurement: the Reference plays its sweeps and the Result records
    void requestStop();      // stop it early; nothing is kept
    void resetMeasurement(); // stop, and empty the slot that the next measurement would fill

    // --- Status, in words ------------------------------------------------------------------------------
    enum class Status { idle, sending, recording, working, finished, problem };
    struct StatusInfo { Status status = Status::idle; juce::String text; float progress = 0.0f; };
    StatusInfo getStatus() const;

    // --- The measurements -------------------------------------------------------------------------------
    PluginAnalyzer::Slot& getSlot (int index) noexcept { return slots[(size_t) juce::jlimit (0, PluginAnalyzer::numSlots - 1, index)]; }
    const PluginAnalyzer::Slot& getSlot (int index) const noexcept { return slots[(size_t) juce::jlimit (0, PluginAnalyzer::numSlots - 1, index)]; }
    int measureSlot = 0;      // the slot the next measurement goes into
    int shownSlot = 0;        // the slot drawn on the graph
    int comparedSlot = -1;    // a second slot drawn over it, or -1 for none

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

    // The text of the CSV file for the shown slot, and the compared one if there is one.
    juce::String makeCsvText() const;

    // Editor state saved with the session (not automatable): window width.
    std::atomic<int> editorWidth { PluginAnalyzer::EditorLayout::defaultWidth };

    // SUI-23: what leaves the plugin, per channel, for the far-right Output meter (lock-free, read by the editor).
    Songsmith::UI::OutputMeterState outputMeter;
    Songsmith::Diagnostics::LoadMeter loadMeter;

    juce::ValueTree createState();
    void restoreFromValueTree (const juce::ValueTree& saved);

    static inline const juce::Identifier versionId { "stateVersion" }, uiId { "UI" };

    // For tests: how many instances are in this session as a Reference and as a Result.
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

    // A finished analysis waits here until the editor's timer (poll) moves it into its slot.
    juce::CriticalSection pendingLock;
    PluginAnalyzer::Measurement pending;
    juce::String pendingChannel;
    bool pendingReady = false;
    std::atomic<bool> analysisFinished { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPluginAnalyzerProcessor)
};
