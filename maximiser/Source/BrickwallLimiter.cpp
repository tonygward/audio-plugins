#include "BrickwallLimiter.h"
#include "MaxParams.h"

namespace Max
{
    void BrickwallLimiter::prepare (double sampleRate)
    {
        fs = sampleRate;
        lookAhead = lookAheadSamples (sampleRate);
        window = lookAhead + 1;

        for (auto& d : delay)
            d.assign ((size_t) window, 0.0f);
        needRing.assign ((size_t) window, 1.0f);
        dqVal.assign ((size_t) window + 1, 1.0f);
        dqIdx.assign ((size_t) window + 1, 0);
        const int l1 = lookAhead / 2 + 1;
        box1.prepare (l1);
        box2.prepare (lookAhead - l1 + 2);
        for (auto& d : preDelay)
            d.assign ((size_t) TruePeakDetector::delaySamples + 1, 0.0f);

        baseline.reset (sampleRate, parameterSmoothingSeconds);
        ceiling.reset (sampleRate, parameterSmoothingSeconds);
        setReleaseMs (releaseMs);
        reset();
    }

    void BrickwallLimiter::reset() noexcept
    {
        for (auto& d : delay)
            std::fill (d.begin(), d.end(), 0.0f);
        std::fill (needRing.begin(), needRing.end(), 1.0f);
        box1.reset();
        box2.reset();
        writePos = 0;
        dqHead = dqSize = 0;
        sampleIndex = 0;
        env = sustained = 1.0f;
        minGain = lastGain = 1.0f;
        detector.reset();
        for (auto& d : preDelay)
            std::fill (d.begin(), d.end(), 0.0f);
        prePos = 0;

        baseline.setCurrentAndTargetValue (dbToGain (ceilingDb - thresholdDb));
        ceiling.setCurrentAndTargetValue (dbToGain (ceilingDb));
    }

    void BrickwallLimiter::setThresholdDb (float db) noexcept
    {
        thresholdDb = db;
        updateBaselineTarget();
    }

    void BrickwallLimiter::setCeilingDb (float db) noexcept
    {
        ceilingDb = db;
        ceiling.setTargetValue (dbToGain (db));
        updateBaselineTarget();
    }

    void BrickwallLimiter::updateBaselineTarget() noexcept
    {
        baseline.setTargetValue (dbToGain (ceilingDb - thresholdDb));
    }

    void BrickwallLimiter::setReleaseMs (float ms) noexcept
    {
        const float clamped = juce::jlimit (minReleaseMs, maxReleaseMs, ms);
        if (clamped == releaseMs && coefRate == fs)
            return;   // called every block: only the four exp() calls below are worth skipping

        releaseMs = clamped;
        coefRate = fs;
        auto coef = [this] (double timeMs) { return (float) std::exp (-1.0 / (timeMs * 0.001 * fs)); };
        releaseCoef = coef ((double) releaseMs);

        // Auto release (Songsmith recovery law, see the class comment): the slow stage is never
        // faster than the manual Release.
        slowCoef = coef (juce::jmax ((double) releaseMs, autoSlowReleaseMs));
        sustainDownCoef = coef (autoSustainAttackMs);
        sustainUpCoef = coef (autoSustainReleaseMs);
    }

    float BrickwallLimiter::slidingMin (float value) noexcept
    {
        const int cap = (int) dqVal.size();

        // Drop entries from the back that can never be the minimum again.
        while (dqSize > 0)
        {
            const int back = (dqHead + dqSize - 1) % cap;
            if (dqVal[(size_t) back] < value)
                break;
            --dqSize;
        }

        const int slot = (dqHead + dqSize) % cap;
        dqVal[(size_t) slot] = value;
        dqIdx[(size_t) slot] = sampleIndex;
        ++dqSize;

        // Drop the front once it is older than the window [n-L, n].
        if (dqIdx[(size_t) dqHead] <= sampleIndex - window)
        {
            dqHead = (dqHead + 1) % cap;
            --dqSize;
        }

        return dqVal[(size_t) dqHead];
    }

    void BrickwallLimiter::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = juce::jlimit (0, maxChannels, numChannels);
        const float relGain = 1.0f - releaseCoef;

        for (int i = 0; i < numSamples; ++i)
        {
            const float b = baseline.getNextValue();
            const float c = juce::jmax (1.0e-6f, ceiling.getNextValue() - headroom);

            // Baseline gain, then the true-peak detector. Both peak modes use the audio delayed by
            // the detector delay, so the latency is the same in both.
            float x[maxChannels] = {};
            for (int ch = 0; ch < numChannels; ++ch)
                x[ch] = channels[ch][i] * b;

            float samplePeak = 0.0f;
            const float tp = detector.push (x, numChannels, samplePeak, truePeak);   // both for sample n - D
            const float peak = truePeak ? tp : samplePeak;

            const int preRead = prePos + 1 == (int) preDelay[0].size() ? 0 : prePos + 1;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                preDelay[ch][(size_t) prePos] = x[ch];
                delay[ch][(size_t) writePos] = preDelay[ch][(size_t) preRead];
            }
            prePos = preRead;

            const float need = peak > c ? (float) ((double) c / (double) peak) : 1.0f;

            if (! autoRelease)
            {
                // Manual: instant attack, exponential release, never above what this sample needs.
                env = juce::jmin (need, env + (1.0f - env) * relGain);
            }
            else
            {
                // Auto release: recover quickly (manual Release) while the gain is below the
                // sustained estimate, i.e. after a transient; slowly once it is back at the level
                // the material keeps asking for, so sustained limiting doesn't pump or distort.
                const float k = env < sustained ? relGain : 1.0f - slowCoef;
                env = juce::jmin (need, env + (1.0f - env) * k);
                sustained += (env - sustained) * (1.0f - (env < sustained ? sustainDownCoef : sustainUpCoef));
            }

            // Sliding minimum, then two cascaded box-car means (an S-shaped attack over L samples).
            const float smoothed = box2.push (box1.push (slidingMin (env)));

            // The oldest slot (one past the write position) holds sample n-L.
            const int readPos = writePos + 1 == window ? 0 : writePos + 1;
            needRing[(size_t) writePos] = need;
            const float g = juce::jmin (smoothed, needRing[(size_t) readPos], 1.0f);

            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] = delay[ch][(size_t) readPos] * g;

            minGain = juce::jmin (minGain, g);
            lastGain = g;
            writePos = readPos;
            ++sampleIndex;
        }
    }

    float BrickwallLimiter::readAndResetMaxGainReductionDb() noexcept
    {
        const float db = gainToDb (minGain);
        minGain = lastGain;
        return juce::jmin (0.0f, db);
    }
}
