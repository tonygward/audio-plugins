#pragma once

// Harmonic and aliasing measurements through the whole processor (definitions in
// fet/docs/colour.md, "Measurements").
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"

namespace ColourMeasure
{
    constexpr int fftOrder = 15, fftSize = 1 << fftOrder;

    struct Settings
    {
        double fs = 48000.0;
        int oversampling = 0;      // 0 off, 1 2x, 2 4x
        float inputDb = 0.0f, outputDb = 0.0f;
        bool grOff = false, colour = true;
        int ratio = 0;
        float attack = 4.0f, release = 1.0f;   // slow release: little GR ripple
        double freq = 1000.0;      // snapped to an FFT bin centre
        float levelDbfs = -18.0f;  // sine RMS at the plugin input
    };

    struct Result
    {
        double fundamentalDb = 0.0;   // output fundamental, dBFS RMS
        double thdPercent = 0.0;      // harmonics below Nyquist, relative to the fundamental
        double h2Db = -200.0, h3Db = -200.0;   // dB relative to the fundamental
        double aliasDb = -200.0;      // everything else 20 Hz-20 kHz, relative to the fundamental
        float grDb = 0.0f;
    };

    inline void set (SongsmithAnalogCompressorProcessor& p, const char* id, float v)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    }

    inline double snap (double f, double fs) { return std::round (f * fftSize / fs) * fs / fftSize; }

    // Runs a settled sine through the plugin and returns the last fftSize output samples.
    inline std::vector<float> render (const Settings& s, float* grOut = nullptr)
    {
        SongsmithAnalogCompressorProcessor p;
        p.setColourEnabled (s.colour);
        set (p, FET::ID::oversampling, (float) s.oversampling);
        set (p, FET::ID::input, s.inputDb);
        set (p, FET::ID::output, s.outputDb);
        set (p, FET::ID::grOff, s.grOff ? 1.0f : 0.0f);
        set (p, FET::ID::ratio, (float) s.ratio);
        set (p, FET::ID::attack, s.attack);
        set (p, FET::ID::release, s.release);
        p.setBusesLayout ([] { juce::AudioProcessor::BusesLayout l; l.inputBuses.add (juce::AudioChannelSet::mono());
                                l.outputBuses.add (juce::AudioChannelSet::mono()); return l; }());
        p.prepareToPlay (s.fs, 512);

        const double f = snap (s.freq, s.fs);
        const float a = std::pow (10.0f, s.levelDbfs / 20.0f) * std::sqrt (2.0f);
        const int settle = (int) (s.fs * 1.5);
        const int total = settle + fftSize;
        std::vector<float> out ((size_t) fftSize);
        juce::AudioBuffer<float> b (1, 512);
        juce::MidiBuffer m;
        float gr = 0.0f;
        for (int pos = 0; pos < total; pos += 512)
        {
            for (int i = 0; i < 512; ++i)
                b.setSample (0, i, a * (float) std::sin (juce::MathConstants<double>::twoPi * f * (pos + i) / s.fs));
            p.processBlock (b, m);
            gr = p.takeGainReductionDb();
            for (int i = 0; i < 512; ++i)
                if (pos + i >= settle && pos + i < total)
                    out[(size_t) (pos + i - settle)] = b.getSample (0, i);
        }
        if (grOut != nullptr)
            *grOut = gr;
        return out;
    }

    inline Result analyse (const std::vector<float>& x, double fs, double freq)
    {
        // 4-term Blackman-Harris window: sidelobes below -92 dB.
        std::vector<float> buf ((size_t) fftSize * 2, 0.0f);
        double wsum = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            const double t = juce::MathConstants<double>::twoPi * i / (fftSize - 1);
            const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
            wsum += w;
            buf[(size_t) i] = (float) (x[(size_t) i] * w);
        }
        juce::dsp::FFT fft (fftOrder);
        fft.performFrequencyOnlyForwardTransform (buf.data());

        std::vector<double> power ((size_t) fftSize / 2);
        for (size_t k = 0; k < power.size(); ++k)
            power[k] = (double) buf[k] * buf[k];

        const auto binOf = [&] (double f) { return (int) std::round (f * fftSize / fs); };
        const auto band = [&] (int centre) { double sum = 0.0; for (int k = centre - 4; k <= centre + 4; ++k)
                                                 if (k > 0 && k < (int) power.size()) sum += power[(size_t) k]; return sum; };

        const double f0 = snap (freq, fs);
        const int b0 = binOf (f0);
        const double p0 = band (b0);

        Result r;
        // Amplitude of a sine with window sum wsum: peak bin = A * wsum / 2.
        const double peak = power[(size_t) b0];   // bin-centred: the peak bin carries A * wsum / 2
        r.fundamentalDb = 10.0 * std::log10 (peak / ((wsum / 2.0) * (wsum / 2.0)) / 2.0);

        std::vector<bool> used (power.size(), false);
        const auto mark = [&] (int c) { for (int k = c - 4; k <= c + 4; ++k) if (k >= 0 && k < (int) used.size()) used[(size_t) k] = true; };
        mark (b0);
        for (int k = 0; k < 6; ++k) used[(size_t) k] = true;   // DC

        double harm = 0.0;
        for (int h = 2; h * f0 < fs / 2 - 50.0; ++h)
        {
            const int bh = binOf (h * f0);
            const double ph = band (bh);
            harm += ph;
            if (h == 2) r.h2Db = 10.0 * std::log10 (ph / p0 + 1e-30);
            if (h == 3) r.h3Db = 10.0 * std::log10 (ph / p0 + 1e-30);
            mark (bh);
        }
        r.thdPercent = 100.0 * std::sqrt (harm / p0);

        double alias = 0.0;
        const int lo = binOf (20.0), hi = std::min ((int) power.size() - 1, binOf (20000.0));
        for (int k = lo; k <= hi; ++k)
            if (! used[(size_t) k])
                alias += power[(size_t) k];
        r.aliasDb = 10.0 * std::log10 (alias / p0 + 1e-30);
        return r;
    }

    inline Result measure (const Settings& s)
    {
        float gr = 0.0f;
        auto out = render (s, &gr);
        auto r = analyse (out, s.fs, s.freq);
        r.grDb = gr;
        return r;
    }
}
