#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PeqParams.h"
#include "PeqPresets.h"
#include "PassiveEngine.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithHeader.h"
#include "../../shared/Source/SongsmithOutputMeter.h"

class SongsmithPassiveEQProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = the three frequency switches, the four amounts, the bandwidth, Bypass, and the editor's
    // width and preset name. Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 1;

    SongsmithPassiveEQProcessor();

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
    int getNumPrograms() override { return (int) Peq::Presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    Peq::Presets presets { apvts };

    // Undo and Redo for what the musician changes in the window (never host automation). Message thread only.
    Songsmith::UI::Header::ParameterHistory history { *this };

    // The engine settings as the parameters now stand (read lock-free).
    SongsmithPassive::Params getEngineParams() const noexcept;

    // Editor state saved with the session (not automatable): window width.
    std::atomic<int> editorWidth { Peq::EditorLayout::defaultWidth };

    // The output level for the editor's stereo meter: each channel's peak after all the processing and Bypass, read once by the
    // editor (the same shared meter as Songsmith EQ and Songsmith Compressor). Written lock-free from the audio thread.
    Songsmith::UI::OutputMeterState outputMeter;

    // Optional timing of processBlock for the editor's Processing Load readout (off unless the editor switches it on).
    Songsmith::Diagnostics::LoadMeter loadMeter;

    juce::ValueTree createState();
    void restoreFromValueTree (const juce::ValueTree& saved);

    static constexpr double bypassFadeSeconds = 0.02;
    static inline const juce::Identifier versionId { "stateVersion" }, uiId { "UI" };

private:
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* lowFrequencyRaw = nullptr;
    std::atomic<float>* lowBoostRaw = nullptr;
    std::atomic<float>* lowCutRaw = nullptr;
    std::atomic<float>* highBoostFrequencyRaw = nullptr;
    std::atomic<float>* highBoostRaw = nullptr;
    std::atomic<float>* highBandwidthRaw = nullptr;
    std::atomic<float>* highCutFrequencyRaw = nullptr;
    std::atomic<float>* highCutRaw = nullptr;

    SongsmithPassive::Engine engine;
    int currentProgram = 0;

    // Bypass: a short linear cross-fade between the processed and the dry signal. No latency, so the dry signal needs no delay.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithPassiveEQProcessor)
};
