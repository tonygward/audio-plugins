#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "SvfFilter.h"

namespace EQ
{
    // One band as the linear-phase designer sees it (dynamics are not available there).
    struct LinearPhaseBand
    {
        bool active = false;
        FilterType type = FilterType::Bell;
        float freq = 1000.0f, q = defaultQ, gain = 0.0f;
        int slope = defaultSlope;
        StereoMode mode = StereoMode::Stereo;
        bool matched = false;   // EQ-44: High Frequency Accuracy
    };

    using LinearPhaseBands = std::array<LinearPhaseBand, numBands>;

    // A finished design: two stereo impulse responses (see LinearPhaseEQ).
    struct LinearPhaseDesign
    {
        juce::AudioBuffer<float> direct, cross;
        double sampleRate = 48000.0;
        int length = 0;
    };

    // Linear-phase EQ: a symmetric FIR whose magnitude is the product of the bands' exact
    // magnitudes (the same makeFilter / getMagnitude maths the display draws), applied with
    // juce::dsp::Convolution (uniform partitioned, zero extra latency).
    //
    //  * Design: frequency sampling on the FIR's own N-point grid, zero phase -> delay of
    //    N/2 samples ((-1)^k), inverse FFT, Hann window. Latency = N/2 samples.
    //  * Stereo modes: in linear phase all filters are pure delays times zero-phase
    //    responses, so the whole EQ is a 2x2 matrix of FIRs. With per-path products
    //    Lr, Rr (Stereo/Left/Right bands) and Mr, Sr (Mid/Side bands):
    //        L_out = Lr (M+S)/2 * L_in + Lr (M-S)/2 * R_in
    //        R_out = Rr (M-S)/2 * L_in + Rr (M+S)/2 * R_in
    //    i.e. the M/S bands act first, then the L/R bands (documented ordering).
    //    'direct' holds (LL, RR); 'cross' holds (LR, RL) and runs on the channel-swapped input.
    //  * Handover: the designer thread submits a design through a lock-free slot; the
    //    audio thread takes it and calls Convolution::loadImpulseResponse (wait-free; JUCE
    //    builds the engine on its own background thread and cross-fades old -> new IR over
    //    50 ms). Designs are only ever deleted by the designer thread.
    class LinearPhaseEQ
    {
    public:
        static constexpr int numQualities = 3;   // Low, Medium, High

        ~LinearPhaseEQ();

        // FIR length for a quality at a sample rate: 4096 / 8192 / 16384 at 44.1-48 kHz,
        // doubled at 88.2-96 kHz and quadrupled at 176.4-192 kHz (same time span).
        static int firLength (int quality, double sampleRate);

        // designRate: the rate the band filters are designed at (sampleRate x oversampling
        // factor), so the FIR reproduces the un-cramped oversampled response.
        static std::unique_ptr<LinearPhaseDesign> design (const LinearPhaseBands&, double sampleRate, double designRate, int length, bool mono);

        void prepare (double sampleRate, int maxBlockSize);
        void reset();

        // Audio thread. Picks up a pending design first.
        void process (float* const* channels, int numChannels, int numSamples) noexcept;

        // Designer thread.
        void submit (std::unique_ptr<LinearPhaseDesign>);
        void collectGarbage();

        // True once both convolutions run an IR of 'length' samples.
        bool isReady (int length) const;

    private:
        juce::dsp::Convolution direct, cross;
        juce::AudioBuffer<float> swapped;
        std::atomic<LinearPhaseDesign*> pending { nullptr }, consumed { nullptr };
    };
}
