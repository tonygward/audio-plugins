#include "SpectrumAnalyser.h"
#include <cmath>

namespace
{
    constexpr double twoPi = 6.283185307179586476925;
}

SpectrumAnalyser::SpectrumAnalyser()
{
    fifoBuffer.assign ((size_t) fifo.getTotalSize(), 0.0f);
    history.assign ((size_t) fftSize, 0.0f);
    fftData.resize ((size_t) fftSize);
    displayDb.assign ((size_t) numBins, floorDb);
    peakDb.assign ((size_t) numBins, floorDb);

    // Reserve the largest tables once; smaller FFT sizes reuse the same storage.
    window.reserve ((size_t) fftSize);
    twiddles.reserve ((size_t) fftSize / 2);
    bitReverse.reserve ((size_t) fftSize);
    buildTables();
}

void SpectrumAnalyser::buildTables()
{
    // Hann window; its coherent gain (sum / N = 0.5) is compensated so a
    // full-scale sine on a bin centre reads 0 dBFS.
    window.resize ((size_t) size);
    double sum = 0.0;
    for (int i = 0; i < size; ++i)
    {
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * i / size));
        sum += window[(size_t) i];
    }
    windowGain = (float) (2.0 / sum);

    // Radix-2 FFT tables.
    twiddles.resize ((size_t) size / 2);
    for (int i = 0; i < size / 2; ++i)
        twiddles[(size_t) i] = std::polar (1.0f, (float) (-twoPi * i / size));

    bitReverse.resize ((size_t) size);
    for (int i = 0; i < size; ++i)
    {
        int r = 0;
        for (int b = 0; b < order; ++b)
            r |= ((i >> b) & 1) << (order - 1 - b);
        bitReverse[(size_t) i] = r;
    }
}

void SpectrumAnalyser::setFftOrder (int newOrder)
{
    newOrder = juce::jlimit (minFftOrder, fftOrder, newOrder);
    if (newOrder == order)
        return;

    order = newOrder;
    size = 1 << order;
    buildTables();

    // Bin frequencies changed: start the display (and peaks) afresh. The history ring
    // keeps the largest size, so the next update already has a full frame.
    std::fill (displayDb.begin(), displayDb.end(), floorDb);
    std::fill (peakDb.begin(), peakDb.end(), floorDb);
    newSamples = 1;
}

void SpectrumAnalyser::setSpeed (int speedIndex)
{
    speed = juce::jlimit (0, 2, speedIndex);
    decayDbPerSecond = speedDecayDbPerSecond[speed];
}

void SpectrumAnalyser::setPeakHold (bool on)
{
    peakHold = on;
    resetPeaks();
}

void SpectrumAnalyser::resetPeaks()
{
    std::fill (peakDb.begin(), peakDb.end(), floorDb);
}

void SpectrumAnalyser::push (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0 || numSamples <= 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);   // drops data if the GUI isn't reading

    const float scale = 1.0f / (float) numChannels;
    auto write = [&] (int dest, int src, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                s += channels[ch][src + i];
            fifoBuffer[(size_t) (dest + i)] = s * scale;
        }
    };

    write (start1, 0, size1);
    write (start2, size1, size2);
    fifo.finishedWrite (size1 + size2);
}

// Message thread. Only consumes from the FIFO (never resets it), so it is safe
// while the audio thread keeps writing.
void SpectrumAnalyser::clear()
{
    const int ready = fifo.getNumReady();
    if (ready > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (ready, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
    }

    std::fill (history.begin(), history.end(), 0.0f);
    std::fill (displayDb.begin(), displayDb.end(), floorDb);
    std::fill (peakDb.begin(), peakDb.end(), floorDb);
    newSamples = 0;
}

void SpectrumAnalyser::performFFT()
{
    // Window the most recent 'size' samples (oldest first) in bit-reversed order.
    // The history ring is always fftSize (the maximum) long.
    const int start = historyPos - size + fftSize;
    for (int i = 0; i < size; ++i)
    {
        const float s = history[(size_t) ((start + i) % fftSize)] * window[(size_t) i];
        fftData[(size_t) bitReverse[(size_t) i]] = { s, 0.0f };
    }

    // Iterative radix-2 Cooley-Tukey.
    for (int len = 2; len <= size; len <<= 1)
    {
        const int half = len / 2;
        const int step = size / len;

        for (int i = 0; i < size; i += len)
            for (int j = 0; j < half; ++j)
            {
                const auto t = twiddles[(size_t) (j * step)] * fftData[(size_t) (i + j + half)];
                const auto u = fftData[(size_t) (i + j)];
                fftData[(size_t) (i + j)]        = u + t;
                fftData[(size_t) (i + j + half)] = u - t;
            }
    }
}

bool SpectrumAnalyser::update (double now)
{
    const double dt = lastUpdateTime > 0.0 ? juce::jlimit (0.0, 0.5, now - lastUpdateTime) : 0.0;
    lastUpdateTime = now;

    // Drain the FIFO into the history ring (also while frozen, so unfreezing shows
    // the current signal straight away).
    const int ready = fifo.getNumReady();
    if (ready > 0)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (ready, start1, size1, start2, size2);

        auto read = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                history[(size_t) historyPos] = fifoBuffer[(size_t) (start + i)];
                historyPos = (historyPos + 1) % fftSize;
            }
        };
        read (start1, size1);
        read (start2, size2);
        fifo.finishedRead (size1 + size2);
        newSamples += ready;
    }

    if (frozen)
        return false;

    const float decay = decayDbPerSecond * (float) dt;
    const int bins = getNumBins();
    bool changed = false;

    if (newSamples > 0)
    {
        newSamples = 0;
        performFFT();

        for (int b = 0; b < bins; ++b)
        {
            const float mag = std::abs (fftData[(size_t) b]) * windowGain;
            const float db = mag > 1.0e-7f ? 20.0f * std::log10 (mag) : floorDb;

            // Instant attack, linear-in-dB release.
            auto& d = displayDb[(size_t) b];
            d = juce::jmax (db, d - decay, floorDb);
        }
        changed = true;
    }
    else if (decay > 0.0f)
    {
        for (int b = 0; b < bins; ++b)
        {
            auto& d = displayDb[(size_t) b];
            if (d > floorDb)
            {
                d = juce::jmax (floorDb, d - decay);
                changed = true;
            }
        }
    }

    if (peakHold)
        for (int b = 0; b < bins; ++b)
            peakDb[(size_t) b] = juce::jmax (peakDb[(size_t) b], displayDb[(size_t) b]);

    return changed;
}

float SpectrumAnalyser::levelFrom (const std::vector<float>& data, double freqLow, double freqHigh, double sampleRate) const noexcept
{
    const int bins = getNumBins();
    const double binWidth = sampleRate / size;
    const double b0 = freqLow / binWidth, b1 = freqHigh / binWidth;

    if (b1 - b0 < 1.0)
    {
        // Less than one bin per pixel: interpolate at the centre frequency.
        const double centre = juce::jlimit (0.0, (double) bins - 1.001, 0.5 * (b0 + b1));
        const int i = (int) centre;
        const float frac = (float) (centre - i);
        return data[(size_t) i] + (data[(size_t) i + 1] - data[(size_t) i]) * frac;
    }

    float best = floorDb;
    const int last = juce::jmin (bins - 1, (int) std::ceil (b1));
    for (int i = juce::jmax (0, (int) std::floor (b0)); i <= last; ++i)
        best = juce::jmax (best, data[(size_t) i]);
    return best;
}

float SpectrumAnalyser::getLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept
{
    return levelFrom (displayDb, freqLow, freqHigh, sampleRate);
}

float SpectrumAnalyser::getPeakLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept
{
    return levelFrom (peakDb, freqLow, freqHigh, sampleRate);
}

float SpectrumAnalyser::tiltDb (double freqLow, double freqHigh, float dbPerOct) noexcept
{
    return dbPerOct * (float) std::log2 (std::sqrt (freqLow * freqHigh) / 1000.0);
}
