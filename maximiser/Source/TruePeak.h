#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>
#include <vector>

namespace Max
{
    // Polyphase interpolator coefficients: a Kaiser-windowed sinc for 'factor'x upsampling with
    // 'tapsPerPhase' base-rate taps per phase. Phase q of base sample n estimates the signal at
    // time n - tapsPerPhase/2 + q/factor; phase 0 is the input itself (delayed), exactly.
    inline std::vector<float> designInterpolator (int factor, int tapsPerPhase, double beta)
    {
        const int length = factor * tapsPerPhase;
        const double centre = (double) length * 0.5;
        auto besselI0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 50; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };

        std::vector<float> h ((size_t) length);
        for (int m = 0; m < length; ++m)
        {
            const double t = ((double) m - centre) / (double) factor;   // in base samples
            const double sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
            const double r = ((double) m - centre) / centre;
            const double w = besselI0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / besselI0 (beta);
            h[(size_t) m] = (float) (sinc * w);
        }

        // Normalise every phase to unity DC gain.
        for (int q = 0; q < factor; ++q)
        {
            double sum = 0.0;
            for (int k = 0; k < tapsPerPhase; ++k)
                sum += (double) h[(size_t) (k * factor + q)];
            for (int k = 0; k < tapsPerPhase; ++k)
                h[(size_t) (k * factor + q)] = (float) ((double) h[(size_t) (k * factor + q)] / sum);
        }
        return h;
    }

    // Real-time true-peak detector used by the limiter (Songsmith design choice): 8x oversampling,
    // 24 taps per phase, Kaiser beta 8 (4x under-reads content near Nyquist by up to ~0.7 dB). For each base sample it reports the largest magnitude
    // over the sample itself and the reconstructed values on both sides of it (7 intermediate
    // points per interval, all channels). Its output for sample k is ready 'delaySamples' samples
    // after k arrives, so the limiter delays its audio by the same amount.
    class TruePeakDetector
    {
    public:
        static constexpr int factor = 8, tapsPerPhase = 24, maxChannels = 2;
        static constexpr int delaySamples = tapsPerPhase / 2;   // 12 base samples at any rate

        TruePeakDetector() : coeffs (designInterpolator (factor, tapsPerPhase, 8.0)) { reset(); }

        void reset() noexcept
        {
            for (auto& h : history)
                h.fill (0.0f);
            pos = 0;
            previousInterval = 0.0f;
        }

        // Pushes one sample per channel; returns the true-peak estimate for the sample that
        // arrived 'delaySamples' ago (written into samplePeak: its plain sample peak).
        float push (const float* x, int numChannels, float& samplePeak, bool interpolate = true) noexcept
        {
            pos = (pos + 1) & (ringSize - 1);
            for (int c = 0; c < numChannels; ++c)
                history[(size_t) c][(size_t) pos] = x[c];

            float interval = 0.0f;   // phase q estimates time n - delaySamples + q / factor: between samples n-12 and n-11
            samplePeak = 0.0f;
            for (int c = 0; c < numChannels; ++c)
            {
                const auto& h = history[(size_t) c];
                samplePeak = juce::jmax (samplePeak, std::abs (h[(size_t) ((pos - delaySamples) & (ringSize - 1))]));

                if (! interpolate)
                    continue;   // sample-peak mode: keep the history, skip the filter

                for (int q = 1; q < factor; ++q)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < tapsPerPhase; ++k)
                        acc += coeffs[(size_t) (k * factor + q)] * h[(size_t) ((pos - k) & (ringSize - 1))];
                    interval = juce::jmax (interval, std::abs (acc));
                }
            }

            const float tp = juce::jmax (samplePeak, interval, previousInterval);
            previousInterval = interval;
            return tp;
        }

    private:
        static constexpr int ringSize = 32;   // power of two > tapsPerPhase
        std::vector<float> coeffs;
        std::array<std::array<float, ringSize>, maxChannels> history {};
        int pos = 0;
        float previousInterval = 0.0f;
    };

    // Independent reconstruction meter, for verification and the output meter: a different, longer
    // interpolator (8x, 32 taps per phase, Kaiser beta 10) than the detector, so the tests do not
    // grade the detector against itself. Reports the largest reconstructed magnitude seen.
    class TruePeakMeter
    {
    public:
        static constexpr int factor = 8, tapsPerPhase = 32, maxChannels = 2;

        TruePeakMeter() : coeffs (designInterpolator (factor, tapsPerPhase, 10.0)) { reset(); }

        void reset() noexcept
        {
            for (auto& h : history)
                h.fill (0.0f);
            pos = 0;
            peak = 0.0f;
        }

        void process (const float* const* channels, int numChannels, int numSamples) noexcept
        {
            numChannels = juce::jmin (numChannels, maxChannels);
            for (int i = 0; i < numSamples; ++i)
            {
                pos = (pos + 1) & (ringSize - 1);
                for (int c = 0; c < numChannels; ++c)
                {
                    auto& h = history[(size_t) c];
                    h[(size_t) pos] = channels[c][i];
                    for (int q = 0; q < factor; ++q)
                    {
                        float acc = 0.0f;
                        for (int k = 0; k < tapsPerPhase; ++k)
                            acc += coeffs[(size_t) (k * factor + q)] * h[(size_t) ((pos - k) & (ringSize - 1))];
                        peak = juce::jmax (peak, std::abs (acc));
                    }
                }
            }
        }

        float getPeak() const noexcept { return peak; }
        float readAndReset() noexcept { const float p = peak; peak = 0.0f; return p; }

    private:
        static constexpr int ringSize = 32;
        std::vector<float> coeffs;
        std::array<std::array<float, ringSize>, maxChannels> history {};
        int pos = 0;
        float peak = 0.0f;
    };
}
