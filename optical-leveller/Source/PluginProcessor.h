#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "OptParams.h"
#include "OptPresets.h"
#include "OpticalEngine.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithBeforeAfter.h"
#include "../../shared/Source/SongsmithHeader.h"
#include "../../shared/Source/SongsmithOutputMeter.h"

class SongsmithOpticalLevellerProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = Peak Reduction, Gain, Compress or Limit, Bypass, and the editor's width, meter and preset name.
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 1;

    SongsmithOpticalLevellerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
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

    // The host's program list is the factory starting points.
    int getNumPrograms() override { return (int) Opt::Presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    Opt::Presets presets { apvts };

    // Undo and Redo for what the musician changes in the window (never host automation). Message thread only.
    Songsmith::UI::Header::ParameterHistory history { *this };

    // The engine settings as the parameters now stand (read lock-free).
    SongsmithOptical::Params getEngineParams() const noexcept;

    // For the meter: the deepest gain reduction (dB, positive) and the loudest block RMS (dBFS, loudest channel) since the last call.
    float takeGainReductionDb() noexcept { return gainReductionDb.exchange (0.0f, std::memory_order_relaxed); }
    float takeOutputRmsDb() noexcept
    {
        const float ms = outputMs.exchange (0.0f, std::memory_order_relaxed);
        return ms > 1.0e-15f ? 10.0f * std::log10 (ms) : -150.0f;
    }

    // Editor state saved with the session (not automatable): window width and meter mode.
    std::atomic<int> editorWidth { Opt::EditorLayout::defaultWidth };
    std::atomic<int> meterMode { 0 };   // 0 = Gain Reduction, 1 = Output

    // SUI-23: what leaves the plugin, per channel, for the far-right Output meter (lock-free, read by the editor).
    Songsmith::UI::OutputMeterState outputMeter;

    // Optional timing of processBlock for the editor's Processing Load readout (off unless the editor switches it on).
    Songsmith::Diagnostics::LoadMeter loadMeter;

    // songsmithstudio#7: the Before and After loudness shown in the footer. Fed by one scope at the top of processBlock.
    Songsmith::Loudness::BeforeAfterMeter beforeAfter;

    juce::ValueTree createState();
    void restoreFromValueTree (const juce::ValueTree& saved);

    static constexpr double bypassFadeSeconds = 0.02;
    static inline const juce::Identifier versionId { "stateVersion" }, uiId { "UI" };

private:
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* peakReductionRaw = nullptr;
    std::atomic<float>* gainRaw = nullptr;
    std::atomic<float>* modeRaw = nullptr;

    SongsmithOptical::Engine engine;
    int currentProgram = 0;

    std::atomic<float> gainReductionDb { 0.0f }, outputMs { 0.0f };

    // Bypass: a short linear cross-fade between the processed and the dry signal. No latency, so the dry signal needs no delay.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    static void pushMax (std::atomic<float>& meter, float value) noexcept
    {
        float prev = meter.load (std::memory_order_relaxed);
        while (value > prev && ! meter.compare_exchange_weak (prev, value, std::memory_order_relaxed)) {}
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithOpticalLevellerProcessor)
};
