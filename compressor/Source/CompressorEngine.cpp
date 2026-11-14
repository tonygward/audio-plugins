#include "CompressorEngine.h"

namespace SongsmithComp
{
    namespace
    {
        constexpr float silenceFloorDb = -120.0f;
        constexpr float tinyGr = 1.0e-9f;   // below this the smoothed reduction snaps to zero (no denormals)

        // The one-pole coefficient whose step response reaches 1 - 1/e after 'ms' milliseconds.
        float coefficientFor (float ms, double fs) noexcept
        {
            return std::exp (-1.0f / (0.001f * juce::jmax (0.01f, ms) * (float) fs));
        }

        float sane (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

        // Analog Warmth design constants (documented in docs/engine.md).
        constexpr double warmDrive = 0.8;    // how hard the curve bends: small signals are untouched, peaks are rounded
        constexpr double warmBias  = 0.08;   // a small asymmetry, which adds even harmonics as well as odd ones

        double lnCosh (double u) noexcept   // ln(cosh(u)) without overflow
        {
            const double a = std::abs (u);
            return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453;
        }

        // Slope at zero, so the curve is scaled to a gain of exactly 1 for small signals.
        double warmNorm() noexcept
        {
            const double t = std::tanh (warmDrive * warmBias);
            return warmDrive * (1.0 - t * t);
        }
    }

    double Engine::warmthCurve (double x) noexcept
    {
        return (std::tanh (warmDrive * (x + warmBias)) - std::tanh (warmDrive * warmBias)) / warmNorm();
    }

    // First-order antiderivative anti-aliasing (ADAA): the average of a curve between this sample and the last,
    // which removes much of the aliasing a plain waveshaper makes, with no oversampling and no latency.
    // It is applied only to the curve's non-linear part (the curve minus the straight line it follows for small
    // signals). Averaging the straight part as well would act as a low-pass filter and dull the highs, so that
    // part passes through exactly: small signals keep their level and tone at every frequency.
    // Then a very gentle high-pass (about 5 Hz) takes out the DC the asymmetry would make.
    double Engine::warmShape (WarmState& s, double x) noexcept
    {
        if (! s.primed)
        {
            s.xPrev = x;
            s.dcIn = s.dcOut = 0.0;
            s.primed = true;
        }
        const double d = x - s.xPrev;
        const double t0 = std::tanh (warmDrive * warmBias), norm = warmNorm();
        double residual;   // the curve minus the straight line y = x, averaged over the last step
        if (std::abs (d) < 1.0e-6)
        {
            const double mid = 0.5 * (x + s.xPrev);
            residual = warmthCurve (mid) - mid;
        }
        else
        {
            auto antiderivative = [&] (double v) { return (lnCosh (warmDrive * (v + warmBias)) / warmDrive - t0 * v) / norm - 0.5 * v * v; };
            residual = (antiderivative (x) - antiderivative (s.xPrev)) / d;
        }
        const double y = x + residual;
        s.xPrev = x;

        const double hp = y - s.dcIn + dcCoeff * s.dcOut;
        s.dcIn = y;
        s.dcOut = std::abs (hp) < 1.0e-20 ? 0.0 : hp;
        return s.dcOut;
    }

    void Engine::prepare (double sampleRate)
    {
        fs = juce::jmax (8000.0, sampleRate);
        makeupGain.reset (fs, 0.02);
        mix.reset (fs, 0.02);
        warmthAmount.reset (fs, 0.02);
        dcCoeff = 1.0 - 2.0 * juce::MathConstants<double>::pi * 5.0 / fs;
        updateCoefficients();
        reset();
    }

    void Engine::reset() noexcept
    {
        grDb = 0.0f;
        makeupGain.setCurrentAndTargetValue (makeupGain.getTargetValue());
        mix.setCurrentAndTargetValue (mix.getTargetValue());
        warmthAmount.setCurrentAndTargetValue (warmthAmount.getTargetValue());
        warm = {};
        meterGr = meterIn = meterOut = 0.0f;
    }

    void Engine::updateCoefficients() noexcept
    {
        attackCoeff  = coefficientFor (params.attackMs, fs);
        releaseCoeff = coefficientFor (params.releaseMs, fs);
    }

    void Engine::setParams (const Params& p) noexcept
    {
        params.thresholdDb = juce::jlimit (minThresholdDb, maxThresholdDb, sane (p.thresholdDb));
        params.ratio       = juce::jlimit (minRatio, maxRatio, sane (p.ratio));
        params.attackMs    = juce::jlimit (minAttackMs, maxAttackMs, sane (p.attackMs));
        params.releaseMs   = juce::jlimit (minReleaseMs, maxReleaseMs, sane (p.releaseMs));
        params.kneeDb      = juce::jlimit (minKneeDb, maxKneeDb, sane (p.kneeDb));
        params.makeupDb    = juce::jlimit (minMakeupDb, maxMakeupDb, sane (p.makeupDb));
        params.autoMakeup  = p.autoMakeup;
        params.mix         = juce::jlimit (0.0f, 1.0f, sane (p.mix));
        updateCoefficients();

        const float makeup = params.autoMakeup ? autoMakeupDb (params) : params.makeupDb;
        makeupGain.setTargetValue (juce::Decibels::decibelsToGain (makeup));
        mix.setTargetValue (params.mix);
        params.warmth = p.warmth;
        warmthAmount.setTargetValue (params.warmth ? 1.0f : 0.0f);
    }

    void Engine::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;
        const int n = juce::jlimit (1, maxChannels, numChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            // Detector: the loudest channel, so both channels get the same gain (stereo link).
            float level = 0.0f;
            for (int c = 0; c < n; ++c)
                level = juce::jmax (level, std::abs (sane (channels[c][i])));
            meterIn = juce::jmax (meterIn, level);

            const float levelDb = level > 1.0e-6f ? juce::Decibels::gainToDecibels (level, silenceFloorDb) : silenceFloorDb;
            const float targetGr = staticGainDb (levelDb, params.thresholdDb, params.ratio, params.kneeDb);

            // Smooth the reduction itself: attack while it deepens, release while it recovers.
            const float coeff = targetGr < grDb ? attackCoeff : releaseCoeff;
            grDb = coeff * grDb + (1.0f - coeff) * targetGr;
            if (grDb > -tinyGr)
                grDb = 0.0f;
            meterGr = juce::jmax (meterGr, -grDb);

            const float gain = juce::Decibels::decibelsToGain (grDb) * makeupGain.getNextValue();
            const float wet = mix.getNextValue();
            const float warmAmount = warmthAmount.getNextValue();
            const bool warmActive = warmAmount > 0.0f || warmthAmount.isSmoothing();
            if (! warmActive)
                for (auto& s : warm) s.primed = false;   // starts clean when it is next switched on
            float peakOut = 0.0f;
            for (int c = 0; c < n; ++c)
            {
                const float dry = sane (channels[c][i]);
                float compressed = dry * gain;
                if (warmActive)
                    compressed += warmAmount * ((float) warmShape (warm[(size_t) c], (double) compressed) - compressed);
                const float out = dry + wet * (compressed - dry);   // dry at mix 0, compressed (and warmed) at mix 1
                channels[c][i] = out;
                peakOut = juce::jmax (peakOut, std::abs (out));
            }
            meterOut = juce::jmax (meterOut, peakOut);
        }
    }

    Engine::Meters Engine::readMeters() noexcept
    {
        const Meters m { meterGr, meterIn, meterOut };
        meterGr = meterIn = meterOut = 0.0f;
        return m;
    }
}
