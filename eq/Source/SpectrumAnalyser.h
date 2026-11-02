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
//
// Settings (message thread): FFT size 2048 / 4096 / 8192 (all buffers are allocated
// for the largest size up front, and the audio thread never sees the FFT size, so
// changing it never allocates on or blocks the audio thread), speed (release rate),
// freeze and peak hold.
class SpectrumAnalyser
{
public:
    enum class Mode { Off = 0, Pre, Post };

    static constexpr int fftOrder = 13;                // maximum (and default) order
    static constexpr int fftSize  = 1 << fftOrder;     // 8192: maximum size
    static constexpr int numBins  = fftSize / 2 + 1;   // maximum number of bins
    static constexpr int minFftOrder = 11;             // 2048
    static constexpr float floorDb = -120.0f;

    SpectrumAnalyser();

    // Audio thread. Mixes the channels to mono and queues the samples.
    void push (const float* const* channels, int numChannels, int numSamples) noexcept;

    // Message thread. Returns true if the spectrum changed (needs repaint).
    bool update (double nowSeconds);

    // Message thread: smoothed level of an FFT bin, in dBFS.
    float getBinDb (int bin) const noexcept { return displayDb[(size_t) bin]; }
    float getPeakBinDb (int bin) const noexcept { return peakDb[(size_t) bin]; }

    // Message thread: level at an arbitrary frequency (interpolated between bins
    // at the low end, max over bins where one pixel spans several).
    float getLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept;
    float getPeakLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept;

    // Display tilt: dB added at the band's centre, 'dbPerOct' around 1 kHz.
    static float tiltDb (double freqLow, double freqHigh, float dbPerOct) noexcept;

    void clear();

    //==========================================================================
    // Settings (message thread)
    void setFftOrder (int order);                      // 11..13
    int getFftOrder() const noexcept { return order; }
    int getFftSize() const noexcept  { return size; }
    int getNumBins() const noexcept  { return size / 2 + 1; }

    void setSpeed (int speedIndex);                    // 0 slow, 1 medium, 2 fast
    int getSpeed() const noexcept { return speed; }
    static constexpr float speedDecayDbPerSecond[] = { 12.0f, 30.0f, 80.0f };

    void setFrozen (bool f) noexcept { frozen = f; }
    bool isFrozen() const noexcept { return frozen; }

    void setPeakHold (bool on);
    bool isPeakHold() const noexcept { return peakHold; }
    void resetPeaks();

    std::atomic<int> mode { (int) Mode::Post };
    std::atomic<int> activeViewers { 0 };   // only capture audio while an editor is showing it

    float decayDbPerSecond = speedDecayDbPerSecond[1];

private:
    void buildTables();
    void performFFT();
    float levelFrom (const std::vector<float>& data, double freqLow, double freqHigh, double sampleRate) const noexcept;

    juce::AbstractFifo fifo { 1 << 15 };
    std::vector<float> fifoBuffer;

    std::vector<float> history;           // last fftSize (max) samples (ring)
    int historyPos = 0;
    int newSamples = 0;

    int order = fftOrder, size = fftSize, speed = 1;
    bool frozen = false, peakHold = false;

    std::vector<float> window;
    std::vector<std::complex<float>> fftData, twiddles;
    std::vector<int> bitReverse;
    float windowGain = 1.0f;

    std::vector<float> displayDb, peakDb;
    double lastUpdateTime = 0.0;
};
