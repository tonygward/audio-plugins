#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <complex>
#include <vector>

// Real-time spectrum analyser.
//
//  Audio thread:   push()      - lock-free, allocation-free write into a FIFO
//  Message thread: update()    - drains the FIFO, runs a Hann-windowed FFT and
//                                applies peak-hold/decay ballistics
//
// Levels are calibrated so a full-scale sine reads 0 dBFS.
class SpectrumAnalyser
{
public:
    enum class Mode { Off = 0, Pre, Post };

    static constexpr int fftOrder = 13;
    static constexpr int fftSize  = 1 << fftOrder;     // 8192
    static constexpr int numBins  = fftSize / 2 + 1;
    static constexpr float floorDb = -120.0f;

    SpectrumAnalyser();

    // Audio thread. Mixes the channels to mono and queues the samples.
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;

    // Message thread. Returns true if the spectrum changed (needs repaint).
    bool update (double nowSeconds);

    // Message thread: smoothed level of an FFT bin, in dBFS.
    float getBinDb (int bin) const noexcept { return displayDb[(size_t) bin]; }

    // Message thread: level at an arbitrary frequency (interpolated between bins
    // at the low end, max over bins where one pixel spans several).
    float getLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept;

    void clear();

    std::atomic<int> mode { (int) Mode::Post };
    std::atomic<int> activeViewers { 0 };   // only capture audio while an editor is showing it

    float decayDbPerSecond = 30.0f;

private:
    void performFFT();

    juce::AbstractFifo fifo { 1 << 15 };
    std::vector<float> fifoBuffer;

    std::vector<float> history;           // last fftSize samples (ring)
    int historyPos = 0;
    int newSamples = 0;

    std::vector<float> window;
    std::vector<std::complex<float>> fftData, twiddles;
    std::vector<int> bitReverse;
    float windowGain = 1.0f;

    std::vector<float> displayDb;
    double lastUpdateTime = 0.0;
};
