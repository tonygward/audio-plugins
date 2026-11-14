#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CompParams.h"
#include "CompPresets.h"
#include "CompressorEngine.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithHeader.h"

class SongsmithCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = the eight sound parameters, bypass and the editor width. 2 = adds Analog Warmth
    // (sessions saved before it open with it off, which is exactly how they sounded).
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 2;

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
    // Peaks are linear; gain reduction is the deepest reduction since the last read, as positive dB.
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionDb { 0.0f };
    static float readAndReset (std::atomic<float>& meter) noexcept { return meter.exchange (0.0f, std::memory_order_relaxed); }

    // Optional timing of processBlock for the editor's Processing Load readout.
    Songsmith::Diagnostics::LoadMeter loadMeter;

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
    juce::AudioParameterBool* bypassParam = nullptr;

    SongsmithComp::Engine engine;

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
