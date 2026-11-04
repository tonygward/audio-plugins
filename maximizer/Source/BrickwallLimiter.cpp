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
        boxRing.assign ((size_t) window, 1.0f);

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
        std::fill (boxRing.begin(), boxRing.end(), 1.0f);
        writePos = boxPos = 0;
        boxSum = (double) window;
        resumCountdown = window;
        dqHead = dqSize = 0;
        sampleIndex = 0;
        env = 1.0f;
        minGain = lastGain = 1.0f;

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
        releaseMs = juce::jlimit (minReleaseMs, maxReleaseMs, ms);
        releaseCoef = (float) std::exp (-1.0 / ((double) releaseMs * 0.001 * fs));
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
        const double invWindow = 1.0 / (double) window;

        for (int i = 0; i < numSamples; ++i)
        {
            const float b = baseline.getNextValue();
            const float c = ceiling.getNextValue();

            // Detect on the (baseline-gained) input and store it in the delay line.
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = channels[ch][i] * b;
                peak = juce::jmax (peak, std::abs (x));
                delay[ch][(size_t) writePos] = x;
            }

            const float need = peak > c ? (float) ((double) c / (double) peak) : 1.0f;

            // Instant attack, exponential release, never above what this sample needs.
            env = juce::jmin (need, env + (1.0f - env) * relGain);

            // Sliding minimum, then the box-car mean over the same window.
            const float m = slidingMin (env);
            boxSum += (double) m - (double) boxRing[(size_t) boxPos];
            boxRing[(size_t) boxPos] = m;
            boxPos = boxPos + 1 == window ? 0 : boxPos + 1;

            // Re-add the window exactly now and then, so the running sum never drifts.
            if (--resumCountdown <= 0)
            {
                double s = 0.0;
                for (float v : boxRing)
                    s += (double) v;
                boxSum = s;
                resumCountdown = window;
            }

            // The oldest slot (one past the write position) holds sample n-L.
            const int readPos = writePos + 1 == window ? 0 : writePos + 1;
            needRing[(size_t) writePos] = need;
            const float g = juce::jmin ((float) (boxSum * invWindow), needRing[(size_t) readPos], 1.0f);

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
