#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CompParams.h"
#include "CompPresets.h"
#include "CompressorEngine.h"
#include "DynamicsGraph.h"
#include "../../shared/Source/SongsmithOutputMeter.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithBeforeAfter.h"
#include "../../shared/Source/SongsmithHeader.h"

class SongsmithCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = the eight sound parameters, bypass and the editor width. 2 = adds Analog Warmth
    // (sessions saved before it open with it off, which is exactly how they sounded). 3 = adds the Analog Warmth model (a session saved
    // before it keeps the original Analog Warmth sound until a model is chosen).
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 3;

    SongsmithCompressorAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    bool isBypassed() const noexcept { return bypassParam->get(); }

    // The engine settings as the parameters now stand (read lock-free).
    SongsmithComp::Params getEngineParams() const noexcept;

    juce::AudioProcessorValueTreeState apvts;

    // Undo and Redo for what the musician changes in the window (never host automation).
    Songsmith::UI::Header::ParameterHistory history { *this };

    // UI state, saved with the session (not automatable).
    std::atomic<int> editorWidth { Comp::EditorLayout::defaultWidth };

    juce::ValueTree createState();
    void restoreFromValueTree (const juce::ValueTree& saved);

    // Loads factory preset 'index' as one host gesture per parameter. Message thread. Bypass is left alone.
    void applyPreset (int index);

    static constexpr double bypassFadeSeconds = 0.02;

    // Meters, written by the audio thread after every block and read (and reset) by the UI.
    // Gain reduction is the deepest reduction since the last read, as positive dB (0 while bypassed).
    std::atomic<float> gainReductionDb { 0.0f };

    // The output meter, shared with the Songsmith EQ: a peak per channel, the channel count and the latched CLIP
    // warning (EQ-39, EQ-42). Sample peak, after compression, Makeup Gain, Analog Warmth, Mix and Bypass.
    Songsmith::UI::OutputMeterState outputMeter;

    // The graph's data: one column for every 10 ms of audio (see DynamicsGraph.h), handed over without locks.
    Comp::ColumnFifo graphFifo;

    // UI state saved with the session (not automatable): where the floating panel sits, as fractions of its free travel.
    std::atomic<float> panelX { 0.03f }, panelY { 1.0f };
    static float readAndReset (std::atomic<float>& meter) noexcept { return meter.exchange (0.0f, std::memory_order_relaxed); }

    // Optional timing of processBlock for the editor's Processing Load readout.
    Songsmith::Diagnostics::LoadMeter loadMeter;

    // songsmithstudio#14: which Analog Warmth runs. The model (Black, Blue, Anniversary, Lindell) is a host parameter; a session saved before the
    // models existed is "classic": its Analog Warmth keeps the original sound until the musician chooses a model.
    bool isWarmthClassic() const noexcept { return classicWarmth.load (std::memory_order_relaxed); }
    int getWarmthModelIndex() const noexcept { return warmthModelParam->getIndex(); }
    bool isWarmthOn() const noexcept { return warmthParam->get(); }
    // songsmithstudio#23: the one Model dropdown. Look 0 is Clean (Analog Warmth off, the model kept), 1 to 4 are Black, Blue, Anniversary, Lindell
    // (Analog Warmth on with that model). Only the existing Analog Warmth and model parameters change, in one Undo step; nothing else moves.
    void chooseLook (int look);
    void chooseWarmthModel (int index);   // the musician chose one: it replaces the classic sound for good. Message thread.

    // songsmithstudio#7: the Before and After loudness shown in the footer. Fed by one scope at the top of processBlock.
    Songsmith::Loudness::BeforeAfterMeter beforeAfter;

private:
    std::atomic<float>* thresholdRaw = nullptr;
    std::atomic<float>* ratioRaw = nullptr;
    std::atomic<float>* attackRaw = nullptr;
    std::atomic<float>* releaseRaw = nullptr;
    std::atomic<float>* kneeRaw = nullptr;
    std::atomic<float>* makeupRaw = nullptr;
    std::atomic<float>* mixRaw = nullptr;
    juce::AudioParameterBool* autoMakeupParam = nullptr;
    juce::AudioParameterBool* warmthParam = nullptr;
    juce::AudioParameterChoice* warmthModelParam = nullptr;
    std::atomic<bool> classicWarmth { false };
    std::atomic<bool> restoring { false };
    struct ModelListener final : juce::AudioProcessorValueTreeState::Listener
    {
        explicit ModelListener (SongsmithCompressorAudioProcessor& o) : owner (o) {}
        void parameterChanged (const juce::String&, float) override { if (! owner.restoring.load()) owner.classicWarmth.store (false); }   // host automation or the window chose a model
        SongsmithCompressorAudioProcessor& owner;
    } modelListener { *this };
    juce::AudioParameterBool* bypassParam = nullptr;

    SongsmithComp::Engine engine;

    // The column being filled for the graph (audio thread only).
    int columnLength = 480, columnRemaining = 480;
    float columnIn = 0.0f, columnOut = 0.0f, columnReduction = 0.0f;

    // Bypass: a short linear cross-fade between the processed and the dry signal. No latency, so
    // the dry signal needs no delay.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    static void pushPeak (std::atomic<float>& meter, float peak) noexcept
    {
        float prev = meter.load (std::memory_order_relaxed);
        while (peak > prev && ! meter.compare_exchange_weak (prev, peak, std::memory_order_relaxed)) {}
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithCompressorAudioProcessor)
};
