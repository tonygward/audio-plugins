#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "FETParams.h"
#include "FETEngine.h"
#include "FETColour.h"
#include "FETPresets.h"
#include "../../shared/Source/SongsmithDiagnostics.h"

class SongsmithAnalogCompressorProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    // 1 = first release (input, output, attack, release, ratio, allButtons, grOff, bypass).
    // 2 = adds oversampling (sessions from 1 load with the default, which is Off, as version 1 sounded;
    // version 2 sessions keep the oversampling they saved, including 4x).
    // 3 = adds the detector Low Cut (sessions from 1 and 2 load it Off, as they sounded before).
    static constexpr int stateVersion = 3;

    SongsmithAnalogCompressorProcessor();
    ~SongsmithAnalogCompressorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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

    // Host program list = the factory starting points (Studio One, Logic and others list them).
    int getNumPrograms() override { return (int) FET::Presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    FET::Presets presets { apvts };

    // Gain reduction for the meter: the largest GR (dB, positive) since the last call.
    float takeGainReductionDb() noexcept { return engine.takePeakGainReductionDb(); }
    const FET::Engine& getEngine() const noexcept { return engine; }

    // Latency of an oversampling setting (0 = off, 1 = 2x, 2 = 4x), valid after prepareToPlay.
    int getOversamplingLatency (int index) const noexcept { return osLatency[(size_t) juce::jlimit (0, 2, index)]; }
    int getActiveOversamplingIndex() const noexcept { return osActive; }

    // Output level for the meter: the largest block RMS (dBFS, loudest channel) since the last call.
    float takeOutputRmsDb() noexcept
    {
        const float ms = outputMs.exchange (0.0f, std::memory_order_relaxed);
        return ms > 1.0e-15f ? 10.0f * std::log10 (ms) : -150.0f;
    }
    bool isAllButtonsOn() const noexcept { return allButtonsParam->load() >= 0.5f; }

    // Editor state saved with the session (not automatable): interface size and meter mode.
    std::atomic<float> uiScale { 1.0f };   // the nearest interface size option (menu); old sessions only had this
    std::atomic<int> editorWidth { 0 };    // the dragged window width; 0 = not set, use uiScale (SUI-10)

    // Optional timing of processBlock for the editor's Processing Load readout (off unless the editor switches it on).
    Songsmith::Diagnostics::LoadMeter loadMeter;
    std::atomic<int> meterMode { 0 };   // 0 = GR, 1 = +4, 2 = +8 (UG p. 3)
    static inline const juce::Identifier uiId { "UI" };

    // Test hook: false = the clean engine only (no FET or output-stage colour).
    void setColourEnabled (bool on) noexcept { colourEnabled = on; }

    void updateLatencyNow();   // what the timer does: report the selected setting's latency

    static inline const juce::Identifier versionId { "stateVersion" };

private:
    void timerCallback() override { updateLatencyNow(); }
    void restoreFromValueTree (const juce::ValueTree& saved);
    void processChunk (float* const* ch, int numCh, int num) noexcept;
    void runCore (float* const* ch, int numCh, int num) noexcept;
    void switchPath (int index) noexcept;
    int getOversamplingIndex() const noexcept { return juce::jlimit (0, 2, (int) oversamplingParam->load()); }

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* inputDb  = nullptr;
    std::atomic<float>* outputDb = nullptr;
    std::atomic<float>* attackKnob = nullptr;
    std::atomic<float>* releaseKnob = nullptr;
    std::atomic<float>* ratioIndex = nullptr;
    std::atomic<float>* grOffParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* allButtonsParam = nullptr;
    std::atomic<float>* lowCutParam = nullptr;
    std::atomic<float> outputMs { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> fetFactor { 1.0f };

    FET::Engine engine;
    FET::OutputStage outputStage;
    bool colourEnabled = true;

    // Oversampling: polyphase IIR half-band stages with integer latency, [1] = 2x, [2] = 4x.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    std::array<int, 3> osLatency {};
    int osActive = 0;
    int currentProgram = 0;
    double baseRate = 44100.0;

    static constexpr int chunkSize = 512;
    std::vector<float> gains;              // per-sample engine gain at the running rate
    juce::AudioBuffer<float> dryRing, dry; // bypass dry signal, delayed by the path latency
    int dryRingPos = 0;

    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;
    Smoothed inputGain { 1.0f }, outputGain { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pathFade { 1.0f };    // oversampling switch

    static constexpr double gainSmoothingSeconds = 0.02, bypassFadeSeconds = 0.02, pathFadeSeconds = 0.01;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogCompressorProcessor)
};
