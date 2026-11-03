#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MaxParams.h"

class SongsmithMaximiserAudioProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = first release (threshold, ceiling, release, bypass).
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 1;

    SongsmithMaximiserAudioProcessor();
    ~SongsmithMaximiserAudioProcessor() override = default;

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // Current (denormalised) parameter values, read lock-free.
    float getThresholdDb() const noexcept { return thresholdRaw->load (std::memory_order_relaxed); }
    float getCeilingDb() const noexcept   { return ceilingRaw->load (std::memory_order_relaxed); }
    float getReleaseMs() const noexcept   { return releaseRaw->load (std::memory_order_relaxed); }
    bool isBypassed() const noexcept      { return bypassParam->get(); }

    juce::AudioProcessorValueTreeState apvts;

    // The versioned state tree (parameters + version). Used by sessions and the tests.
    juce::ValueTree createState();

    // Restores a saved tree: saved values where present, defaults for anything missing.
    void restoreFromValueTree (const juce::ValueTree& saved);

    static constexpr double bypassFadeSeconds = 0.02;

private:
    std::atomic<float>* thresholdRaw = nullptr;
    std::atomic<float>* ceilingRaw = nullptr;
    std::atomic<float>* releaseRaw = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    // Bypass: a short linear cross-fade between the processed and the dry signal.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithMaximiserAudioProcessor)
};
