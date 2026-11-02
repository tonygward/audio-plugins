#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "EQParams.h"
#include "EQEngine.h"
#include "EQEditController.h"
#include "SpectrumAnalyser.h"

class ThreeBandEQAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = EQ::numBands;
    static constexpr int stateVersion = 2;   // 1 = v0.1 (3 fixed bell bands), 2 = 24-band bank

    ThreeBandEQAudioProcessor();
    ~ThreeBandEQAudioProcessor() override = default;

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

    //==============================================================================
    // Band slots: which of the 24 bands exist in the UI. Not a host parameter
    // (parameters must not appear/disappear); stored in the plugin state instead.
    bool isBandUsed (int band) const noexcept   { return bandUsed[(size_t) band].load(); }
    void setBandUsed (int band, bool used);
    int getNumUsedBands() const noexcept;
    int findFreeBand() const noexcept;

    juce::RangedAudioParameter* getBandParam (int band, EQ::Param p) const { return bandParams[(size_t) band][(size_t) p]; }
    float getBandValue (int band, EQ::Param p) const;   // denormalised (Hz, dB, Q, type index, 0/1)

    // Tell the host that state outside the parameters changed (band added/removed,
    // UI settings), so it marks the project as modified and saves it.
    void notifyNonParameterStateChanged();

    // Persisted UI state
    std::atomic<int>   selectedBand { 0 };
    std::atomic<float> displayRangeDb { 24.0f };
    std::atomic<int>   editorWidth { 980 }, editorHeight { 580 };

    SpectrumAnalyser analyser;   // mode is persisted with the UI state

    juce::AudioProcessorValueTreeState apvts;
    EQ::EditController edits { *this };

    // For tests: build a state tree with every parameter set from 'saved' or its default.
    void restoreFromValueTree (const juce::ValueTree& saved);

private:
    std::array<std::atomic<bool>, numBands> bandUsed;
    std::array<std::array<juce::RangedAudioParameter*, 5>, numBands> bandParams {};
    std::array<std::array<std::atomic<float>*, 5>, numBands> rawValues {};

    EQ::Engine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThreeBandEQAudioProcessor)
};
