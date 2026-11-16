#pragma once

// ACOMP-09: Set Levels (guided calibration) and the Attack and Release tooltips.
// Included by AnalogCompressorTests.cpp, which provides the includes, `FET` and the helpers used here.
#include "EngineMeasure.h"
#include "../../shared/Source/SongsmithWording.h"

namespace CalibrationTestSupport
{
    using Signal = std::vector<float>;   // mono; the plugin is fed the same sound on both channels

    inline Signal sustained (double fs, double seconds, float amp)
    {
        Signal s ((size_t) (fs * seconds));
        for (size_t i = 0; i < s.size(); ++i)
        {
            const double t = (double) i / fs;
            // A chord-like sustained tone, with a slow swell so it is not perfectly steady.
            const double swell = 0.8 + 0.2 * std::sin (juce::MathConstants<double>::twoPi * 0.5 * t);
            s[i] = (float) (amp * swell * (0.6 * std::sin (juce::MathConstants<double>::twoPi * 196.0 * t)
                                           + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 587.0 * t)));
        }
        return s;
    }

    // Drum-like hits: a short noisy burst with a fast decay twice a second, on a quiet bed.
    inline Signal transients (double fs, double seconds, float amp)
    {
        Signal s ((size_t) (fs * seconds), 0.0f);
        juce::Random rng (7);
        const int period = (int) (fs * 0.5), length = (int) (fs * 0.12);
        for (size_t i = 0; i < s.size(); ++i)
        {
            const int pos = (int) i % period;
            const float env = pos < length ? std::exp (-(float) pos / (float) (fs * 0.02)) : 0.0f;
            const float tone = (float) std::sin (juce::MathConstants<double>::twoPi * 90.0 * (double) pos / fs);
            s[i] = amp * env * (0.7f * tone + 0.3f * (rng.nextFloat() * 2.0f - 1.0f)) + 0.002f * (rng.nextFloat() - 0.5f);
        }
        return s;
    }

    inline double rmsDb (const Signal& s, size_t from = 0)
    {
        double sum = 0.0;
        for (size_t i = from; i < s.size(); ++i)
            sum += (double) s[i] * s[i];
        return 10.0 * std::log10 (juce::jmax (1.0e-15, sum / (double) juce::jmax<size_t> (1, s.size() - from)));
    }

    // Plays the signal through the processor in 512-sample blocks and returns the left channel.
    // `grPerWindow` (optional) receives the peak gain reduction of each 100 ms.
    inline Signal play (SongsmithAnalogCompressorProcessor& p, const Signal& in, std::vector<float>* grPerWindow = nullptr)
    {
        constexpr int block = 512;
        Signal out;
        out.reserve (in.size());
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer midi;
        const int window = (int) (p.getSampleRate() * 0.1);
        int inWindow = 0;
        float windowMax = 0.0f;
        for (size_t pos = 0; pos < in.size(); pos += block)
        {
            const int n = (int) juce::jmin<size_t> ((size_t) block, in.size() - pos);
            b.setSize (2, n, false, false, true);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    b.setSample (c, i, in[pos + (size_t) i]);
            p.processBlock (b, midi);
            for (int i = 0; i < n; ++i)
                out.push_back (b.getSample (0, i));
            windowMax = juce::jmax (windowMax, p.takeGainReductionDb());
            inWindow += n;
            if (inWindow >= window)
            {
                if (grPerWindow != nullptr)
                    grPerWindow->push_back (windowMax);
                windowMax = 0.0f;
                inWindow = 0;
            }
        }
        return out;
    }

    inline void set (SongsmithAnalogCompressorProcessor& p, const char* id, float v)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    }

    // Listens to `in` with the processor's current settings and returns the proposal.
    inline FET::Calibration::Result calibrate (SongsmithAnalogCompressorProcessor& p, const Signal& in)
    {
        p.calibrationCapture.start();
        play (p, in);
        p.calibrationCapture.stop();
        const float* ch[2] = { p.calibrationCapture.channel (0), p.calibrationCapture.channel (1) };
        return FET::Calibration::analyse (ch, 2, p.calibrationCapture.getSamples(), p.calibrationCapture.getSampleRate(),
                                          p.getCalibrationSettings());
    }

    inline float percentile90 (std::vector<float> v)
    {
        std::sort (v.begin(), v.end());
        return v.empty() ? 0.0f : v[juce::jmin (v.size() - 1, (size_t) ((double) v.size() * 0.9))];
    }
}
