#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DeskParams.h"
#include "DeskPresets.h"
#include "DeskEngine.h"
#include "../../shared/Source/SongsmithDiagnostics.h"
#include "../../shared/Source/SongsmithBeforeAfter.h"
#include "../../shared/Source/SongsmithHeader.h"
#include "../../shared/Source/SongsmithOutputMeter.h"
#include "../../shared/Source/SongsmithOutputStage.h"

class SongsmithAnalogDeskProcessor : public juce::AudioProcessor
{
public:
    // Saved-state format. 1 = every setting listed in DeskParams.h, Bypass, and the editor's width and preset name.
    // Bump it when the meaning of saved data changes, and migrate in restoreFromValueTree().
    static constexpr int stateVersion = 2;   // 2 = adds Auto Gain (older sessions load with it off)

    SongsmithAnalogDeskProcessor();

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
    int getNumPrograms() override { return (int) Desk::Presets::factory().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    Desk::Presets presets { apvts };

    // Undo and Redo for what the musician changes in the window (never host automation). Message thread only.
    Songsmith::UI::Header::ParameterHistory history { *this };

    // The engine settings as the parameters now stand (read lock-free).
    SongsmithDesk::Params getEngineParams() const noexcept;

    // For the meter: the deepest compressor gain reduction (dB, positive) since the last call.
    float takeGainReductionDb() noexcept { return gainReductionDb.exchange (0.0f, std::memory_order_relaxed); }

    // For the editor: the correction Auto Gain is applying right now, in dB (0 when off).
    float getAutoGainAppliedDb() const noexcept { return outputStage.getAutoGainAppliedDb(); }
    bool isAutoGainOn() const noexcept { return raw.autoGain->load (std::memory_order_relaxed) > 0.5f; }

    // Editor state saved with the session (not automatable): window width.
    std::atomic<int> editorWidth { Desk::EditorLayout::defaultWidth };

    // What leaves the plugin, per channel, for the far-right Output meter (lock-free, read by the editor).
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
    // Raw parameter values, looked up once.
    struct Raw
    {
        std::atomic<float> *inputTrim, *outputTrim, *drive, *highPass, *lowPass,
                           *lfFreq, *lfGain, *lmfFreq, *lmfGain, *lmfQ, *hmfFreq, *hmfGain, *hmfQ, *hfFreq, *hfGain,
                           *compThreshold, *compRatio, *compAttack, *compRelease, *compMakeup,
                           *gateThreshold, *gateRange, *gateAttack, *gateHold, *gateRelease,
                           *autoGain,
                           *eqOn, *lfBell, *hfBell, *dynamicsOn, *afterEq, *compKnee, *gateOn, *gateExpander;
    } raw {};

    juce::AudioParameterBool* bypassParam = nullptr;

    SongsmithDesk::Engine engine;
    Songsmith::OutputStage outputStage;   // songsmithstudio#16: Auto Gain, then Output Trim, after the strip
    int currentProgram = 0;

    std::atomic<float> gainReductionDb { 0.0f };

    // Bypass: a short linear cross-fade between the processed and the dry signal. No latency, so the dry signal needs no delay.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;

    // Smoothed copies of the controls that change filter coefficients (see processBlock).
    using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smoothed sHighPass, sLowPass, sLfFreq, sLfGain, sLmfFreq, sLmfGain, sLmfQ, sHmfFreq, sHmfGain, sHmfQ, sHfFreq, sHfGain;
    std::atomic<bool> smoothersReady { false };   // false until the first block after prepareToPlay or a session load

    static void pushMax (std::atomic<float>& meter, float value) noexcept
    {
        float prev = meter.load (std::memory_order_relaxed);
        while (value > prev && ! meter.compare_exchange_weak (prev, value, std::memory_order_relaxed)) {}
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithAnalogDeskProcessor)
};
