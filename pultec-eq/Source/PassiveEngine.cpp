#include "PassiveEngine.h"

namespace SongsmithPassive
{
    namespace
    {
        constexpr double pi = juce::MathConstants<double>::pi;

        double limitFrequency (double f, double fs) noexcept { return juce::jlimit (5.0, 0.45 * fs, f); }

        Biquad normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        {
            Biquad q;
            q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0;
            q.a1 = a1 / a0; q.a2 = a2 / a0;
            return q;
        }

        float sane (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

        template <typename Sections, typename State>
        double runChain (const Sections& chain, State& memory, double v) noexcept
        {
            for (size_t s = 0; s < chain.size(); ++s)
            {
                const auto& b = chain[s];
                auto& z = memory[s];
                const double out = b.b0 * v + z[0];
                z[0] = b.b1 * v - b.a1 * out + z[1];
                z[1] = b.b2 * v - b.a2 * out;
                v = out;
            }
            return v;
        }
    }

    Biquad makeLowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * pi * limitFrequency (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q), tsa = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1.0) - (A - 1.0) * cw + tsa),
                          2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                          A * ((A + 1.0) - (A - 1.0) * cw - tsa),
                          (A + 1.0) + (A - 1.0) * cw + tsa,
                          -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                          (A + 1.0) + (A - 1.0) * cw - tsa);
    }

    Biquad makeHighShelf (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * pi * limitFrequency (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q), tsa = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1.0) + (A - 1.0) * cw + tsa),
                          -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                          A * ((A + 1.0) + (A - 1.0) * cw - tsa),
                          (A + 1.0) - (A - 1.0) * cw + tsa,
                          2.0 * ((A - 1.0) - (A + 1.0) * cw),
                          (A + 1.0) - (A - 1.0) * cw - tsa);
    }

    Biquad makePeak (double fs, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * pi * limitFrequency (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise (1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A,
                          1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
    }

    void Engine::prepare (double sampleRate)
    {
        fs = juce::jmax (8000.0, sampleRate);
        fadeLength = juce::jmax (1, juce::roundToInt (fs * fadeSeconds));
        for (auto& g : gain)
            g.reset (fs, 0.02);
        reset();
        setParams (params);
        for (auto& g : gain)
            g.setCurrentAndTargetValue (g.getTargetValue());
        updateSections ({ gain[0].getTargetValue(), gain[1].getTargetValue(), gain[2].getTargetValue(), gain[3].getTargetValue() });
    }

    void Engine::reset() noexcept
    {
        chunkRemaining = 0;
        fadeRemaining = 0;
        for (auto& ch : state)
            for (auto& s : ch)
                s = { 0.0, 0.0 };
        for (auto& ch : fadeState)
            for (auto& s : ch)
                s = { 0.0, 0.0 };
    }

    void Engine::setParams (const Params& p) noexcept
    {
        const auto before = params;
        params.lowFrequencyIndex       = juce::jlimit (0, (int) lowFrequencies.size() - 1, p.lowFrequencyIndex);
        params.highBoostFrequencyIndex = juce::jlimit (0, (int) highBoostFrequencies.size() - 1, p.highBoostFrequencyIndex);
        params.highCutFrequencyIndex   = juce::jlimit (0, (int) highCutFrequencies.size() - 1, p.highCutFrequencyIndex);
        params.lowBoostDb  = juce::jlimit (0.0f, maxLowBoostDb,  sane (p.lowBoostDb));
        params.lowCutDb    = juce::jlimit (0.0f, maxLowCutDb,    sane (p.lowCutDb));
        params.highBoostDb = juce::jlimit (0.0f, maxHighBoostDb, sane (p.highBoostDb));
        params.highCutDb   = juce::jlimit (0.0f, maxHighCutDb,   sane (p.highCutDb));
        params.bandwidth   = juce::jlimit (0.0f, 1.0f, sane (p.bandwidth));

        // A switch moved while sound is passing: keep the filters as they were and fade to the new ones.
        const bool switchMoved = before.lowFrequencyIndex != params.lowFrequencyIndex
                                 || before.highBoostFrequencyIndex != params.highBoostFrequencyIndex
                                 || before.highCutFrequencyIndex != params.highCutFrequencyIndex;
        if (switchMoved && ! flat)
        {
            fadeSections = sections;
            fadeState = state;
            fadeRemaining = fadeLength;
        }

        gain[0].setTargetValue (params.lowBoostDb);
        gain[1].setTargetValue (-params.lowCutDb);
        gain[2].setTargetValue (params.highBoostDb);
        gain[3].setTargetValue (-params.highCutDb);
    }

    void Engine::updateSections (const std::array<float, 4>& g) noexcept
    {
        const double lowF = lowFrequencies[(size_t) params.lowFrequencyIndex];
        const double q = minBandwidthQ + (maxBandwidthQ - minBandwidthQ) * params.bandwidth;
        sections[0] = makeLowShelf  (fs, lowF, shelfQ, g[0]);
        sections[1] = makeLowShelf  (fs, lowF * lowCutCornerRatio, shelfQ, g[1]);
        sections[2] = makePeak      (fs, highBoostFrequencies[(size_t) params.highBoostFrequencyIndex], q, g[2]);
        sections[3] = makeHighShelf (fs, highCutFrequencies[(size_t) params.highCutFrequencyIndex], shelfQ, g[3]);
    }

    float Engine::responseDb (double frequency) const noexcept
    {
        const double lowF = lowFrequencies[(size_t) params.lowFrequencyIndex];
        const double q = minBandwidthQ + (maxBandwidthQ - minBandwidthQ) * params.bandwidth;
        const Biquad s[] = { makeLowShelf  (fs, lowF, shelfQ, gain[0].getTargetValue()),
                             makeLowShelf  (fs, lowF * lowCutCornerRatio, shelfQ, gain[1].getTargetValue()),
                             makePeak      (fs, highBoostFrequencies[(size_t) params.highBoostFrequencyIndex], q, gain[2].getTargetValue()),
                             makeHighShelf (fs, highCutFrequencies[(size_t) params.highCutFrequencyIndex], shelfQ, gain[3].getTargetValue()) };
        float total = 0.0f;
        for (const auto& b : s)
            total += b.magnitudeDb (frequency, fs);
        return total;
    }

    void Engine::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        juce::ScopedNoDenormals noDenormals;
        const int n = juce::jlimit (1, maxChannels, numChannels);

        int done = 0;
        while (done < numSamples)
        {
            // Coefficients are refreshed on a fixed grid of 'chunk' samples that carries across blocks,
            // so the result never depends on how the host splits the audio.
            if (chunkRemaining == 0)
            {
                bool moving = false;
                for (const auto& g : gain)
                    moving = moving || g.isSmoothing();

                std::array<float, 4> g;
                for (size_t i = 0; i < 4; ++i)
                    g[i] = moving ? gain[i].skip (chunk) : gain[i].getTargetValue();

                flat = ! moving && fadeRemaining == 0 && g[0] == 0.0f && g[1] == 0.0f && g[2] == 0.0f && g[3] == 0.0f;
                if (flat)
                    reset();   // all gains zero: the input passes untouched and the filter memory is cleared
                else
                    updateSections (g);
                chunkRemaining = chunk;
            }

            const int count = juce::jmin (chunkRemaining, numSamples - done);
            if (! flat)
            {
                for (int c = 0; c < n; ++c)
                {
                    float* x = channels[c] + done;
                    for (int i = 0; i < count; ++i)
                    {
                        const double in = (double) sane (x[i]);
                        double v = runChain (sections, state[(size_t) c], in);
                        if (i < fadeRemaining)
                        {
                            // The old filters' share falls from 1 to 0 across the fade.
                            const double old = runChain (fadeSections, fadeState[(size_t) c], in);
                            v += (old - v) * ((double) (fadeRemaining - i) / (double) fadeLength);
                        }
                        x[i] = (float) v;
                    }
                }
                fadeRemaining = juce::jmax (0, fadeRemaining - count);
            }
            chunkRemaining -= count;
            done += count;
        }
    }
}
