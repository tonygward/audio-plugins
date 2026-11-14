#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <mutex>
#include "EQParams.h"
#include "EQEngine.h"
#include "EQEditController.h"
#include "PresetManager.h"
#include "SpectrumAnalyser.h"
#include "AutoGain.h"
#include "EditorSize.h"
#include "LinearPhase.h"
#include "../../shared/Source/SongsmithDiagnostics.h"

class SongsmithEQAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int numBands = EQ::numBands;
    static constexpr int stateVersion = 2;   // 1 = v0.1 (3 fixed bell bands), 2 = 24-band bank

    SongsmithEQAudioProcessor();
    ~SongsmithEQAudioProcessor() override;

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
    double getTailLengthSeconds() const override;

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

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }
    juce::RangedAudioParameter* getOutputGainParam() const noexcept { return outputGainParam; }
    juce::RangedAudioParameter* getGainScaleParam() const noexcept  { return gainScaleParam; }

    // Linear-phase mode
    bool isLinearPhaseSelected() const noexcept { return phaseModeParam->getIndex() == 1; }
    int getLinearPhaseQuality() const noexcept  { return lpQualityParam->getIndex(); }
    int getLinearPhaseLength() const noexcept   { return EQ::LinearPhaseEQ::firLength (getLinearPhaseQuality(), currentSampleRate); }
    int getOversamplingIndex() const noexcept   { return oversamplingParam->getIndex(); }
    bool isHighFrequencyAccurate() const noexcept { return hfAccuracyParam->get(); }
    int getOversamplingFactor() const noexcept  { return 1 << getOversamplingIndex(); }
    double getDesignSampleRate() const noexcept { return currentSampleRate * getOversamplingFactor(); }   // rate filters are designed at
    EQ::LinearPhaseBands gatherLinearPhaseBands() const;
    bool updateLinearPhaseDesign (bool force);   // designer thread (or tests): returns true if a design was submitted
    void designLinearPhaseNow() { updateLinearPhaseDesign (true); }
    bool isLinearPhaseReady() const { return linearPhase.isReady (getLinearPhaseLength()); }
    void updateLatencyNow();                     // what the timer does: report the latency of the selected mode

    // Scale the whole curve: multiplier (1 = 100 %) on every gain band's dB gain.
    float getGainScale() const noexcept
    {
        const float pct = gainScaleRaw->load (std::memory_order_relaxed);
        return std::abs (pct - 100.0f) < 0.01f ? 1.0f : pct / 100.0f;   // exactly 1 at 100 % (range snapping may leave ~1e-6)
    }

    // Auto gain: the correction currently applied (dB), for the UI.
    std::atomic<float> autoGainAppliedDb { 0.0f };

    // Dynamic EQ: live dynamic gain (dB) per band, published after every block (UI).
    std::array<std::atomic<float>, numBands> dynamicGainDb {};
    bool isAutoGainOn() const noexcept { return autoGainParam->get(); }

    // Output peak meter, written by the audio thread and read (and reset) by the editor.
    // EQ-39: also a peak per main-output channel, the channel count, and a clip warning that
    // latches (sample peak >= 0 dBFS) until the output has been near-silent for a while.
    struct OutputMeter
    {
        static constexpr int   maxChannels = 2;
        static constexpr float silenceThreshold = 1.0e-4f;   // -80 dBFS
        static constexpr double silenceSeconds = 0.75;

        std::atomic<float> peak { 0.0f };       // max |sample| since the last read (all channels)
        std::atomic<bool>  clipped { false };   // latched when a sample reaches 0 dBFS; cleared by the reader
        std::array<std::atomic<float>, maxChannels> channelPeak {};   // per channel, since the last read
        std::atomic<int>   channels { 2 };      // main output channels now: 1 (mono) or 2 (stereo)
        std::atomic<bool>  clipLatched { false };   // the CLIP warning: stays on while there is audio

        void push (float blockPeak) noexcept
        {
            float prev = peak.load (std::memory_order_relaxed);
            while (blockPeak > prev && ! peak.compare_exchange_weak (prev, blockPeak, std::memory_order_relaxed)) {}
            if (blockPeak >= 1.0f)
                clipped.store (true, std::memory_order_relaxed);
        }

        void pushChannel (int channel, float channelBlockPeak) noexcept
        {
            auto& a = channelPeak[(size_t) channel];
            float prev = a.load (std::memory_order_relaxed);
            while (channelBlockPeak > prev && ! a.compare_exchange_weak (prev, channelBlockPeak, std::memory_order_relaxed)) {}
        }

        // Audio thread, once per block, with the peak over every output channel: latches the warning on a
        // clip and clears it after 'silenceSeconds' of near-silence. A quiet pause shorter than that
        // does not clear it. Only the audio thread touches silentSamples.
        void updateClipLatch (float blockPeak, int numSamples) noexcept
        {
            if (blockPeak >= 1.0f)
            {
                clipLatched.store (true, std::memory_order_relaxed);
                silentSamples = 0.0;
            }
            else if (blockPeak < silenceThreshold)
            {
                silentSamples += (double) numSamples;
                if (silentSamples >= silenceSamples)
                    clipLatched.store (false, std::memory_order_relaxed);
            }
            else
            {
                silentSamples = 0.0;
            }
        }

        // Prepare / layout change: no stale warning from the old layout.
        void resetLatch (double sampleRate) noexcept
        {
            silenceSamples = silenceSeconds * juce::jmax (1.0, sampleRate);
            silentSamples = 0.0;
            clipLatched.store (false, std::memory_order_relaxed);
        }

        float readAndReset() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }
        float readAndResetChannel (int channel) noexcept { return channelPeak[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    private:
        double silentSamples = 0.0, silenceSamples = silenceSeconds * 48000.0;
    } outputMeter;

    // Solo / audition (transient, never saved or automated): the band being auditioned,
    // or -1. The editor sets it; it is cleared when the editor closes.
    std::atomic<int> soloBand { -1 };

    // Persisted UI state
    std::atomic<int>   selectedBand { 0 };
    std::atomic<float> displayRangeDb { 24.0f };
    std::atomic<int>   curveView { 0 };   // 0 all bands, 1 L, 2 R, 3 M, 4 S (see EQDisplay::isInCurveView)
    std::atomic<bool>  showPianoStrip { false };
    std::atomic<bool>  showFrequencyGuide { false };   // EQ-32: the Instrument Frequency Guide (drawing only)
    std::atomic<int>   guideInstrument { 0 };          // index into EQ::Guide::instruments()
    std::atomic<int>   editorWidth { EQ::EditorSize::defaultWidth }, editorHeight { EQ::EditorSize::defaultHeight };

    // Optional timing of processBlock for the editor's Processing Load readout (off unless the editor switches it on).
    Songsmith::Diagnostics::LoadMeter loadMeter;

    // UI size (75 % to 200 %, see EditorSize.h). Saved with the session; a new instance starts
    // at the per-machine default, which is the size last chosen on this computer.
    std::atomic<float> uiScale { 1.0f };
    void setUiScale (float scale);                 // also remembers it as this machine's default
    static juce::File getMachineSettingsFile();    // Songsmith Studio/Songsmith EQ/Settings.xml
    static float loadMachineUiScale();
    static void saveMachineUiScale (float scale);

    SpectrumAnalyser analyser;   // mode and settings are persisted with the UI state

    // Analyser display settings (UI state, message thread).
    std::atomic<float> analyserTilt { 4.5f };                          // dB/oct around 1 kHz: 0, 3, 4.5, 6
    std::atomic<float> analyserTopDb { 0.0f }, analyserBottomDb { -90.0f };
    void setAnalyserFftOrder (int order)  { analyser.setFftOrder (order); sidechainAnalyser.setFftOrder (order); }
    void setAnalyserSpeed (int speed)     { analyser.setSpeed (speed);    sidechainAnalyser.setSpeed (speed); }

    // Sidechain analyser (input bus 1, disabled by default). Captured only while an
    // editor shows it (analyser.activeViewers > 0 and sidechainView > 0).
    SpectrumAnalyser sidechainAnalyser;
    OutputMeter sidechainMeter;                // peak level, so the UI knows a signal is arriving
    std::atomic<int> sidechainView { 0 };      // 0 off, 1 spectrum, 2 spectrum + overlap highlight (UI state)
    bool isSidechainConnected() const;

    juce::AudioProcessorValueTreeState apvts;
    EQ::EditController edits { *this };
    EQ::PresetManager presets { *this };

    // The versioned state tree without UI state (sessions add the UI; presets drop bypass).
    juce::ValueTree createPresetState();

    // For tests: build a state tree with every parameter set from 'saved' or its default.
    void restoreFromValueTree (const juce::ValueTree& saved);

private:
    std::array<std::atomic<bool>, numBands> bandUsed;
    std::array<std::array<juce::RangedAudioParameter*, EQ::numBandParams>, numBands> bandParams {};
    std::array<std::array<std::atomic<float>*, EQ::numBandParams>, numBands> rawValues {};

    EQ::Engine engine;

    // Global output stage: per-sample output gain, then a dry/processed cross-fade for bypass.
    juce::RangedAudioParameter* outputGainParam = nullptr;
    juce::AudioParameterBool*   bypassParam = nullptr;
    juce::AudioParameterBool*   autoGainParam = nullptr;
    EQ::AutoGain autoGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> autoGainSmoothed { 1.0f };
    bool autoGainWasOn = false;
    static constexpr double autoGainSmoothingSeconds = 0.25;

    // Linear-phase mode (see LinearPhase.h). The designer thread rebuilds the FIRs when the
    // EQ changes (at most every 40 ms); the message-thread timer keeps the reported latency
    // in step; the audio thread fades out, switches path, and fades back in on a mode change.
    struct LinearPhaseDesigner;
    std::unique_ptr<LinearPhaseDesigner> lpDesigner;
    EQ::LinearPhaseEQ linearPhase;
    juce::AudioParameterChoice* phaseModeParam = nullptr;
    juce::AudioParameterChoice* lpQualityParam = nullptr;

    // Oversampling (zero-latency path): polyphase IIR half-band stages for 2x and 4x, all
    // prepared up front. Linear-phase mode instead designs its FIR from coefficients at the
    // oversampled rate (same un-cramped response, no extra latency).
    juce::AudioParameterChoice* oversamplingParam = nullptr;
    juce::AudioParameterBool* hfAccuracyParam = nullptr;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;   // [1] 2x, [2] 4x
    std::array<int, 3> oversamplerLatency {};
    int osActive = 0;                          // audio thread: 0 off, 1 = 2x, 2 = 4x
    juce::AudioBuffer<float> sidechainUp;      // sidechain held at the oversampled rate
    bool lpActive = false;                     // audio thread: path in use
    int lpLength = 8192;                       // FIR length in use (audio thread)
    std::atomic<int> designedLength { 0 };     // length of the last submitted design
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modeFade { 1.0f };
    juce::AudioBuffer<float> dryDelay;         // delays the bypass dry signal to match the latency
    int dryDelayPos = 0;
    std::size_t lastDesignHash = 0;
    std::mutex designMutex;                    // designer thread vs. direct calls (never the audio thread)
    double currentSampleRate = 48000.0;

    void timerCallback() override;
    std::atomic<float>* outputGainRaw = nullptr;
    juce::RangedAudioParameter* gainScaleParam = nullptr;
    std::atomic<float>* gainScaleRaw = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassMix { 0.0f };   // 1 = fully bypassed
    juce::AudioBuffer<float> dryBuffer;
    static constexpr double outputGainSmoothingSeconds = 0.02, bypassFadeSeconds = 0.02;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithEQAudioProcessor)
};
