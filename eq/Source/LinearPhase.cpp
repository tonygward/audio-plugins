#include "LinearPhase.h"

namespace EQ
{
    LinearPhaseEQ::~LinearPhaseEQ()
    {
        delete pending.exchange (nullptr);
        delete consumed.exchange (nullptr);
    }

    int LinearPhaseEQ::firLength (int quality, double sampleRate)
    {
        constexpr int base[] = { 4096, 8192, 16384 };
        const int factor = sampleRate > 100000.0 ? 4 : (sampleRate > 50000.0 ? 2 : 1);
        return base[juce::jlimit (0, numQualities - 1, quality)] * factor;
    }

    std::unique_ptr<LinearPhaseDesign> LinearPhaseEQ::design (const LinearPhaseBands& bands, double sampleRate, double designRate,
                                                              int length, bool mono)
    {
        const int half = length / 2;
        const int order = juce::roundToInt (std::log2 ((double) length));
        jassert ((1 << order) == length);

        // Per-path magnitude products on the FIR's own frequency grid.
        std::vector<double> magL ((size_t) half + 1, 1.0), magR = magL, magM = magL, magS = magL;
        bool anyMidSide = false;

        for (const auto& b : bands)
        {
            if (! b.active)
                continue;

            const auto coeffs = makeFilter (b.type, designRate, b.freq, b.q, typeHasGain (b.type) ? b.gain : 0.0f, b.slope);
            const auto mode = mono ? StereoMode::Stereo : b.mode;
            anyMidSide = anyMidSide || isMidSide (mode);

            for (int k = 0; k <= half; ++k)
            {
                const double m = getMagnitude (coeffs, k * sampleRate / length, designRate);
                switch (mode)
                {
                    case StereoMode::Stereo: magL[(size_t) k] *= m; magR[(size_t) k] *= m; break;
                    case StereoMode::Left:   magL[(size_t) k] *= m; break;
                    case StereoMode::Right:  magR[(size_t) k] *= m; break;
                    case StereoMode::Mid:    magM[(size_t) k] *= m; break;
                    case StereoMode::Side:   magS[(size_t) k] *= m; break;
                }
            }
        }

        juce::dsp::FFT fft (order);
        std::vector<juce::dsp::Complex<float>> spectrum ((size_t) length), time ((size_t) length);

        // Inverse-transform scaling (JUCE's backends differ): measure it on a flat response.
        auto inverse = [&] (const std::vector<double>& mag, float* out, float scale)
        {
            for (int k = 0; k <= half; ++k)
            {
                const float v = (float) (mag[(size_t) k] * ((k & 1) ? -1.0 : 1.0));   // delay N/2
                spectrum[(size_t) k] = { v, 0.0f };
                if (k > 0 && k < half)
                    spectrum[(size_t) (length - k)] = { v, 0.0f };
            }
            fft.perform (spectrum.data(), time.data(), true);

            for (int n = 0; n < length; ++n)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * pi * n / length);   // Hann, 1 at N/2
                out[n] = time[(size_t) n].real() * scale * (float) w;
            }
        };

        std::vector<float> probe ((size_t) length);
        inverse (std::vector<double> ((size_t) half + 1, 1.0), probe.data(), 1.0f);
        const float scale = std::abs (probe[(size_t) half]) > 1.0e-12f ? 1.0f / probe[(size_t) half] : 1.0f;

        auto d = std::make_unique<LinearPhaseDesign>();
        d->sampleRate = sampleRate;
        d->length = length;
        d->direct.setSize (2, length);
        d->cross.setSize (2, length);

        std::vector<double> ll ((size_t) half + 1), lr = ll, rl = ll, rr = ll;
        for (size_t k = 0; k <= (size_t) half; ++k)
        {
            const double sum = 0.5 * (magM[k] + magS[k]), diff = 0.5 * (magM[k] - magS[k]);
            ll[k] = magL[k] * sum;  lr[k] = magL[k] * diff;
            rl[k] = magR[k] * diff; rr[k] = magR[k] * sum;
        }

        inverse (ll, d->direct.getWritePointer (0), scale);
        inverse (rr, d->direct.getWritePointer (1), scale);
        if (anyMidSide)
        {
            inverse (lr, d->cross.getWritePointer (0), scale);
            inverse (rl, d->cross.getWritePointer (1), scale);
        }
        else
        {
            d->cross.clear();
        }
        return d;
    }

    void LinearPhaseEQ::prepare (double sampleRate, int maxBlockSize)
    {
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        direct.prepare (spec);
        cross.prepare (spec);
        swapped.setSize (2, maxBlockSize, false, true, false);
    }

    void LinearPhaseEQ::reset()
    {
        direct.reset();
        cross.reset();
    }

    void LinearPhaseEQ::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        // Take a new design (only when the previous one has been collected, so the audio
        // thread never has to free anything).
        if (consumed.load (std::memory_order_acquire) == nullptr)
        {
            if (auto* d = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                using C = juce::dsp::Convolution;
                direct.loadImpulseResponse (std::move (d->direct), d->sampleRate, C::Stereo::yes, C::Trim::no, C::Normalise::no);
                cross.loadImpulseResponse (std::move (d->cross), d->sampleRate, C::Stereo::yes, C::Trim::no, C::Normalise::no);
                consumed.store (d, std::memory_order_release);
            }
        }

        numChannels = juce::jmin (numChannels, 2);
        numSamples = juce::jmin (numSamples, swapped.getNumSamples());

        if (numChannels == 2)
        {
            juce::FloatVectorOperations::copy (swapped.getWritePointer (0), channels[1], numSamples);
            juce::FloatVectorOperations::copy (swapped.getWritePointer (1), channels[0], numSamples);
        }

        juce::dsp::AudioBlock<float> block (channels, (size_t) numChannels, (size_t) numSamples);
        direct.process (juce::dsp::ProcessContextReplacing<float> (block));

        if (numChannels == 2)
        {
            juce::dsp::AudioBlock<float> sw (swapped.getArrayOfWritePointers(), 2, (size_t) numSamples);
            cross.process (juce::dsp::ProcessContextReplacing<float> (sw));
            juce::FloatVectorOperations::add (channels[0], swapped.getReadPointer (0), numSamples);
            juce::FloatVectorOperations::add (channels[1], swapped.getReadPointer (1), numSamples);
        }
    }

    void LinearPhaseEQ::submit (std::unique_ptr<LinearPhaseDesign> d)
    {
        delete pending.exchange (d.release(), std::memory_order_acq_rel);   // drop a design the audio thread never took
    }

    void LinearPhaseEQ::collectGarbage()
    {
        delete consumed.exchange (nullptr, std::memory_order_acq_rel);
    }

    bool LinearPhaseEQ::isReady (int length) const
    {
        return direct.getCurrentIRSize() == length && cross.getCurrentIRSize() == length;
    }
}
