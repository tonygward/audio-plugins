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

    // Hann window; its coherent gain (sum / N = 0.5) is compensated so a
    // full-scale sine on a bin centre reads 0 dBFS.
    window.resize ((size_t) fftSize);
    double sum = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPi * i / fftSize));
        sum += window[(size_t) i];
    }
    windowGain = (float) (2.0 / sum);

    // Radix-2 FFT tables.
    twiddles.resize ((size_t) fftSize / 2);
    for (int i = 0; i < fftSize / 2; ++i)
        twiddles[(size_t) i] = std::polar (1.0f, (float) (-twoPi * i / fftSize));

    bitReverse.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
    {
        int r = 0;
        for (int b = 0; b < fftOrder; ++b)
            r |= ((i >> b) & 1) << (fftOrder - 1 - b);
        bitReverse[(size_t) i] = r;
    }
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
    newSamples = 0;
}

void SpectrumAnalyser::performFFT()
{
    // Window the most recent fftSize samples (oldest first) in bit-reversed order.
    for (int i = 0; i < fftSize; ++i)
    {
        const float s = history[(size_t) ((historyPos + i) % fftSize)] * window[(size_t) i];
        fftData[(size_t) bitReverse[(size_t) i]] = { s, 0.0f };
    }

    // Iterative radix-2 Cooley-Tukey.
    for (int len = 2; len <= fftSize; len <<= 1)
    {
        const int half = len / 2;
        const int step = fftSize / len;

        for (int i = 0; i < fftSize; i += len)
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

    // Drain the FIFO into the history ring.
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

    const float decay = decayDbPerSecond * (float) dt;
    bool changed = false;

    if (newSamples > 0)
    {
        newSamples = 0;
        performFFT();

        for (int b = 0; b < numBins; ++b)
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
        for (auto& d : displayDb)
            if (d > floorDb)
            {
                d = juce::jmax (floorDb, d - decay);
                changed = true;
            }
    }

    return changed;
}

float SpectrumAnalyser::getLevelDb (double freqLow, double freqHigh, double sampleRate) const noexcept
{
    const double binWidth = sampleRate / fftSize;
    const double b0 = freqLow / binWidth, b1 = freqHigh / binWidth;

    if (b1 - b0 < 1.0)
    {
        // Less than one bin per pixel: interpolate at the centre frequency.
        const double centre = juce::jlimit (0.0, (double) numBins - 1.001, 0.5 * (b0 + b1));
        const int i = (int) centre;
        const float frac = (float) (centre - i);
        return displayDb[(size_t) i] + (displayDb[(size_t) i + 1] - displayDb[(size_t) i]) * frac;
    }

    float best = floorDb;
    const int last = juce::jmin (numBins - 1, (int) std::ceil (b1));
    for (int i = juce::jmax (0, (int) std::floor (b0)); i <= last; ++i)
        best = juce::jmax (best, displayDb[(size_t) i]);
    return best;
}
