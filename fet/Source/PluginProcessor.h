#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FETParams.h"
#include "FETEngine.h"

class SongsmithAnalogCompressorProcessor : public juce::AudioProcessor
{
public:
    // 1 = first release (input, output, attack, release, ratio, allButtons, grOff, bypass).
    static constexpr int stateVersion = 1;

    SongsmithAnalogCompressorProcessor();
    ~SongsmithAnalogCompressorProcessor() override = default;

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

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Gain reduction for the meter: the largest GR (dB, positive) since the last call.
    float takeGainReductionDb() noexcept { return engine.takePeakGainReductionDb(); }
    const FET::Engine& getEngine() const noexcept { return engine; }

    static inline const juce::Identifier versionId { "stateVersion" };

private:
    void restoreFromValueTree (const juce::ValueTree& saved);

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* inputDb  = nullptr;
    std::atomic<float>* outputDb = nullptr;
    std::atomic<float>* attackKnob = nullptr;
    std::atomic<float>* releaseKnob = nullptr;
    std::atomic<float>* ratioIndex = nullptr;
    std::atomic<float>* grOffParam = nullptr;

    FET::Engine engine;

    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>;
    static void applyGain (juce::AudioBuffer<float>&, int numCh, int n, Smoothed&) noexcept;
    Smoothed inputGain { 1.0f }, outputGain { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dry;

    static constexpr double gainSmoothingSeconds = 0.02, bypassFadeSeconds = 0.02;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogCompressorProcessor)
};
