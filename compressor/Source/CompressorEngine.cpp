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
    }

    void Engine::prepare (double sampleRate)
    {
        fs = juce::jmax (8000.0, sampleRate);
        makeupGain.reset (fs, 0.02);
        mix.reset (fs, 0.02);
        updateCoefficients();
        reset();
    }

    void Engine::reset() noexcept
    {
        grDb = 0.0f;
        makeupGain.setCurrentAndTargetValue (makeupGain.getTargetValue());
        mix.setCurrentAndTargetValue (mix.getTargetValue());
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
            float peakOut = 0.0f;
            for (int c = 0; c < n; ++c)
            {
                const float dry = sane (channels[c][i]);
                const float out = dry + wet * (dry * gain - dry);   // dry at mix 0, compressed at mix 1
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
