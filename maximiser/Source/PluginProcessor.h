#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MaxParams.h"
#include "BrickwallLimiter.h"
#include "Dither.h"
#include "EditorLogic.h"
#include "StartingPoints.h"

class SongsmithMaximiserAudioProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = threshold, ceiling, release, bypass. 2 = adds peak_mode, auto_release.
    // 3 = adds dither and the UI child (link, editor width).
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 3;

    SongsmithMaximiserAudioProcessor();
    ~SongsmithMaximiserAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;   // clears the look-ahead delay and the gain envelope
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return (double) getLatencySamples() / juce::jmax (1.0, getSampleRate()); }

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
    // Threshold and ceiling sit on a 0.01 dB grid. The normalised round trip can leave a tiny
    // error (-6.7e-7 instead of 0 on Apple clang), so snap here: 0 dB is exactly 0 (unity gain).
    static float snapDb (float db) noexcept { return (float) (std::round ((double) db * 100.0) / 100.0); }
    float getThresholdDb() const noexcept { return snapDb (thresholdRaw->load (std::memory_order_relaxed)); }
    float getCeilingDb() const noexcept   { return snapDb (ceilingRaw->load (std::memory_order_relaxed)); }
    float getReleaseMs() const noexcept   { return releaseRaw->load (std::memory_order_relaxed); }
    bool isBypassed() const noexcept      { return bypassParam->get(); }
    bool isTruePeak() const noexcept      { return peakModeParam->getIndex() == 1; }
    bool isAutoRelease() const noexcept   { return autoReleaseParam->get(); }
    Max::Dither::Depth getDitherDepth() const noexcept { return (Max::Dither::Depth) juce::jlimit (0, 2, ditherParam->getIndex()); }

    juce::AudioProcessorValueTreeState apvts;

    // UI state, saved with the session (not automatable).
    std::atomic<bool> linkThresholdCeiling { false };
    std::atomic<int> editorWidth { Max::EditorLayout::defaultWidth };

    // The versioned state tree (parameters + version). Used by sessions and the tests.
    juce::ValueTree createState();

    // Restores a saved tree: saved values where present, defaults for anything missing.
    void restoreFromValueTree (const juce::ValueTree& saved);

    // Loads factory starting point 'index' (see StartingPoints.h): one host gesture per parameter.
    // Message thread. Bypass and Link are left as they are.
    void applyStartingPoint (int index);

    static constexpr double bypassFadeSeconds = 0.02;

    // Meters, written by the audio thread after every block and read (and reset) by the UI.
    // Peaks are linear; gain reduction is the deepest limiter gain since the last read, in dB.
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionDb { 0.0f };   // GR: positive dB
    static float readAndReset (std::atomic<float>& meter, float resetTo = 0.0f) noexcept { return meter.exchange (resetTo, std::memory_order_relaxed); }

    const Max::BrickwallLimiter& getLimiter() const noexcept { return limiter; }

private:
    std::atomic<float>* thresholdRaw = nullptr;
    std::atomic<float>* ceilingRaw = nullptr;
    std::atomic<float>* releaseRaw = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;
    juce::AudioParameterChoice* peakModeParam = nullptr;
    juce::AudioParameterBool* autoReleaseParam = nullptr;
    juce::AudioParameterChoice* ditherParam = nullptr;

    Max::Dither dither;

    // Bypass: a short linear cross-fade between the processed and the dry signal.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    Max::BrickwallLimiter limiter;

    // The dry signal for bypass, delayed by the limiter latency so bypass stays time-aligned.
    juce::AudioBuffer<float> dryDelay;
    int dryDelayPos = 0;

    static void pushPeak (std::atomic<float>& meter, float peak) noexcept
    {
        float prev = meter.load (std::memory_order_relaxed);
        while (peak > prev && ! meter.compare_exchange_weak (prev, peak, std::memory_order_relaxed)) {}
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithMaximiserAudioProcessor)
};
