#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#include "MaxParams.h"

// Loudness metering to ITU-R BS.1770-4 (K-weighting, channel weights, gating) with the EBU R 128
// windows (Momentary 400 ms, Short Term 3 s, Integrated). A Songsmith addition: the L1 had no such meter.
//
//   StepMeter       audio thread. K-weights the output and adds up the mean square per 100 ms step.
//                   Each finished step goes to the UI through a lock-free single-producer,
//                   single-consumer FIFO. No allocation, locks or logging after construction.
//   LoudnessReadings UI thread. Turns the steps into Momentary, Short Term and Integrated.
namespace Max::Loudness
{
    constexpr double stepSeconds = 0.1;
    constexpr double resetMarker = -1.0;   // a FIFO entry that is not a step: "start measuring again"

    // One second-order section, transposed direct form II, in double precision.
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.0; }
    };

    // BS.1770-4 gives coefficients for 48 kHz only. These are the analogue prototype (stage 1: a high
    // shelf of +4 dB around 1.68 kHz; stage 2: a 38 Hz high pass), mapped to any rate by the bilinear
    // transform. At 48 kHz they reproduce the standard's published coefficients (tested).
    inline void kWeightingCoefficients (double fs, Biquad& shelf, Biquad& highPass) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        {
            const double f0 = 1681.974450955533, gain = 3.999843853973347, q = 0.7071752369554196;
            const double k = std::tan (pi * f0 / fs);
            const double vh = std::pow (10.0, gain / 20.0), vb = std::pow (vh, 0.4996667741545416);
            const double a0 = 1.0 + k / q + k * k;
            shelf.b0 = (vh + vb * k / q + k * k) / a0;
            shelf.b1 = 2.0 * (k * k - vh) / a0;
            shelf.b2 = (vh - vb * k / q + k * k) / a0;
            shelf.a1 = 2.0 * (k * k - 1.0) / a0;
            shelf.a2 = (1.0 - k / q + k * k) / a0;
        }
        {
            const double f0 = 38.13547087602444, q = 0.5003270373238773;
            const double k = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + k / q + k * k;
            highPass.b0 = 1.0; highPass.b1 = -2.0; highPass.b2 = 1.0;
            highPass.a1 = 2.0 * (k * k - 1.0) / a0;
            highPass.a2 = (1.0 - k / q + k * k) / a0;
        }
    }

    // Magnitude of the two stages together at 'freq', for the response test.
    inline double kWeightingMagnitudeDb (double fs, double freq) noexcept
    {
        Biquad s, h;
        kWeightingCoefficients (fs, s, h);
        const double w = 2.0 * 3.14159265358979323846 * freq / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
        auto mag = [&] (const Biquad& b) { return (b.b0 + b.b1 * z1 + b.b2 * z2) / (1.0 + b.a1 * z1 + b.a2 * z2); };
        return 20.0 * std::log10 (std::abs (mag (s) * mag (h)));
    }

    inline double meanSquareToLufs (double meanSquare) noexcept
    {
        return -0.691 + 10.0 * std::log10 (meanSquare);
    }

    //==========================================================================
    class StepMeter
    {
    public:
        static constexpr int maxChannels = 2;
        static constexpr int fifoSteps = 4096;   // about 6.8 minutes of steps; the editor drains it 30 times a second

        // The FIFO is allocated once here, not in prepare(), so preparing at a new rate can never race the
        // editor reading it. Nothing allocates after this.
        StepMeter() : fifo (fifoSteps), storage ((size_t) fifoSteps, 0.0) {}

        // Message/prepare thread, never while processing.
        void prepare (double sampleRate) noexcept
        {
            stepLength = juce::jmax (1, juce::roundToInt (sampleRate * stepSeconds));
            for (auto& c : channels) { kWeightingCoefficients (sampleRate, c.shelf, c.highPass); c.shelf.clear(); c.highPass.clear(); }
            restart();
        }

        // Audio thread. Channel weight is 1.0 for left, right and mono (BS.1770-4).
        void process (const float* const* data, int numChannels, int numSamples) noexcept
        {
            numChannels = juce::jmin (numChannels, maxChannels);
            for (int i = 0; i < numSamples; ++i)
            {
                for (int c = 0; c < numChannels; ++c)
                {
                    auto& ch = channels[(size_t) c];
                    const double y = ch.highPass.process (ch.shelf.process ((double) data[c][i]));
                    sum += y * y;
                }
                if (++count >= stepLength)
                {
                    push (sum / (double) stepLength);
                    restart();
                }
            }
        }

        // Audio thread: the next step is the first of a new measurement. The part-finished step is dropped.
        void requestReset() noexcept
        {
            push (resetMarker);
            restart();
        }

        // UI thread: the next entry (a mean square, or resetMarker), if any.
        bool pop (double& value) noexcept
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (1, s1, n1, s2, n2);
            if (n1 + n2 == 0)
                return false;
            value = storage[(size_t) (n1 > 0 ? s1 : s2)];
            fifo.finishedRead (1);
            return true;
        }

    private:
        struct Channel { Biquad shelf, highPass; };

        void restart() noexcept { sum = 0.0; count = 0; }

        void push (double value) noexcept
        {
            int s1, n1, s2, n2;
            fifo.prepareToWrite (1, s1, n1, s2, n2);
            if (n1 + n2 == 0)
                return;   // the editor has not been reading: drop the step rather than block
            storage[(size_t) (n1 > 0 ? s1 : s2)] = value;
            fifo.finishedWrite (1);
        }

        std::array<Channel, maxChannels> channels;
        juce::AbstractFifo fifo;
        std::vector<double> storage;
        int stepLength = 4800, count = 0;
        double sum = 0.0;
    };

    //==========================================================================
    struct Reading
    {
        bool valid = false;
        float lufs = Max::dbFloor;
    };

    class LoudnessReadings
    {
    public:
        static constexpr int momentarySteps = 4, shortTermSteps = 30;
        static constexpr double absoluteGateLufs = -70.0, relativeGateLu = -10.0;
        static constexpr double binWidth = 0.01, histMin = -70.0, histMax = 20.0;
        static constexpr int numBins = (int) ((histMax - histMin) / binWidth + 0.5);

        LoudnessReadings() : counts ((size_t) numBins, 0), energy ((size_t) numBins, 0.0) {}   // UI thread only

        void reset()
        {
            steps.fill (0.0);
            numSteps = 0;
            head = 0;
            std::fill (counts.begin(), counts.end(), 0);
            std::fill (energy.begin(), energy.end(), 0.0);
        }

        // Drains the processor's FIFO. Returns true if anything changed.
        bool update (StepMeter& meter)
        {
            bool changed = false;
            for (double v; meter.pop (v);)
            {
                if (v < 0.0) reset(); else addStep (v);
                changed = true;
            }
            return changed;
        }

        void addStep (double meanSquare)
        {
            if (! std::isfinite (meanSquare) || meanSquare < 0.0)
                meanSquare = 0.0;

            steps[(size_t) head] = meanSquare;
            head = (head + 1) % shortTermSteps;
            ++numSteps;

            // One 400 ms gating block per 100 ms step (75 % overlap, as BS.1770-4 specifies).
            if (numSteps >= momentarySteps)
            {
                const double z = window (momentarySteps);
                if (z > 0.0)
                {
                    const double lufs = meanSquareToLufs (z);
                    if (lufs >= absoluteGateLufs)
                    {
                        const int bin = juce::jlimit (0, numBins - 1, (int) ((lufs - histMin) / binWidth));
                        ++counts[(size_t) bin];
                        energy[(size_t) bin] += z;
                    }
                }
            }
        }

        Reading getMomentary() const   { return fromWindow (momentarySteps); }
        Reading getShortTerm() const   { return fromWindow (shortTermSteps); }
        int getNumSteps() const noexcept { return numSteps; }

        // Gated loudness of everything since the reset. The cost is the histogram size, not the length played.
        Reading getIntegrated() const
        {
            double totalEnergy = 0.0;
            long long totalCount = 0;
            for (int i = 0; i < numBins; ++i)
            {
                totalEnergy += energy[(size_t) i];
                totalCount += counts[(size_t) i];
            }
            if (totalCount == 0)
                return {};

            const double relativeGate = meanSquareToLufs (totalEnergy / (double) totalCount) + relativeGateLu;
            const int firstBin = juce::jlimit (0, numBins - 1, (int) std::ceil ((relativeGate - histMin) / binWidth));

            double e = 0.0;
            long long n = 0;
            for (int i = firstBin; i < numBins; ++i)
            {
                e += energy[(size_t) i];
                n += counts[(size_t) i];
            }
            return n == 0 ? Reading {} : toReading (e / (double) n);
        }

    private:
        // Mean square over the newest 'n' steps (only once that many have arrived).
        double window (int n) const
        {
            double sum = 0.0;
            for (int i = 1; i <= n; ++i)
                sum += steps[(size_t) ((head - i + 2 * shortTermSteps) % shortTermSteps)];
            return sum / (double) n;
        }

        Reading fromWindow (int n) const
        {
            return numSteps >= n ? toReading (window (n)) : Reading {};
        }

        static Reading toReading (double meanSquare)
        {
            if (! (meanSquare > 0.0))
                return {};
            const double lufs = meanSquareToLufs (meanSquare);
            return { std::isfinite (lufs), juce::jmax (Max::dbFloor, (float) lufs) };
        }

        std::array<double, shortTermSteps> steps {};
        int numSteps = 0, head = 0;
        std::vector<int> counts;
        std::vector<double> energy;
    };

    // How a reading is written on screen: "-23.0", or a dash while there is nothing to show.
    inline juce::String format (const Reading& r)
    {
        return r.valid ? juce::String (r.lufs, 1) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93"));
    }
}
