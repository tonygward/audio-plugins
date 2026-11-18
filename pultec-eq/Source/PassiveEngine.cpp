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

    Biquad makeHighPass (double fs, double f, double q) noexcept
    {
        const double w = 2.0 * pi * limitFrequency (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    Biquad makeLowPass (double fs, double f, double q) noexcept
    {
        const double w = 2.0 * pi * limitFrequency (f, fs) / fs;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    std::array<float, 4> Engine::targetGains (const Params& p) noexcept
    {
        switch (p.mode)
        {
            case Mode::midShaper:       return { p.midLowBoostDb, -p.midDipDb, p.midHighBoostDb, 0.0f };
            case Mode::frequencyFilter: return { 0.0f, 0.0f, 0.0f, 0.0f };   // the filters have no gain
            case Mode::toneShaper:      break;
        }
        return { p.lowBoostDb, -p.lowCutDb, p.highBoostDb, -p.highCutDb };
    }

    std::array<Biquad, Engine::numSections> Engine::buildSections (const Params& p, const std::array<float, 4>& g, double fs) noexcept
    {
        std::array<Biquad, numSections> s {};   // every slot starts as a neutral filter (passes everything)
        switch (p.mode)
        {
            case Mode::midShaper:
                s[0] = makePeak (fs, midLowFrequencies[(size_t) p.midLowFrequencyIndex], midBoostQ, g[0]);
                s[1] = makePeak (fs, midDipFrequencies[(size_t) p.midDipFrequencyIndex], midDipQ, g[1]);
                s[2] = makePeak (fs, midHighFrequencies[(size_t) p.midHighFrequencyIndex], midBoostQ, g[2]);
                break;

            case Mode::frequencyFilter:
                if (p.highPassIndex > 0)
                    s[0] = makeHighPass (fs, highPassFrequencies[(size_t) p.highPassIndex], filterQ);
                if (p.lowPassIndex > 0)
                    s[1] = makeLowPass (fs, lowPassFrequencies[(size_t) p.lowPassIndex], filterQ);
                break;

            case Mode::toneShaper:
            {
                const double lowF = lowFrequencies[(size_t) p.lowFrequencyIndex];
                const double q = minBandwidthQ + (maxBandwidthQ - minBandwidthQ) * p.bandwidth;
                s[0] = makeLowShelf  (fs, lowF, shelfQ, g[0]);
                s[1] = makeLowShelf  (fs, lowF * lowCutCornerRatio, shelfQ, g[1]);
                s[2] = makePeak      (fs, highBoostFrequencies[(size_t) p.highBoostFrequencyIndex], q, g[2]);
                s[3] = makeHighShelf (fs, highCutFrequencies[(size_t) p.highCutFrequencyIndex], shelfQ, g[3]);
                break;
            }
        }
        return s;
    }

    bool Engine::isNeutral (const Params& p) noexcept
    {
        if (p.mode == Mode::frequencyFilter)
            return p.highPassIndex == 0 && p.lowPassIndex == 0;
        for (float g : targetGains (p))
            if (g != 0.0f)
                return false;
        return true;
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
        updateSections (targetGains (params));
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
        params.mode = (int) p.mode >= 0 && (int) p.mode < numModes ? p.mode : Mode::toneShaper;
        params.midLowFrequencyIndex  = juce::jlimit (0, (int) midLowFrequencies.size() - 1, p.midLowFrequencyIndex);
        params.midDipFrequencyIndex  = juce::jlimit (0, (int) midDipFrequencies.size() - 1, p.midDipFrequencyIndex);
        params.midHighFrequencyIndex = juce::jlimit (0, (int) midHighFrequencies.size() - 1, p.midHighFrequencyIndex);
        params.highPassIndex = juce::jlimit (0, (int) highPassFrequencies.size() - 1, p.highPassIndex);
        params.lowPassIndex  = juce::jlimit (0, (int) lowPassFrequencies.size() - 1, p.lowPassIndex);
        params.midLowBoostDb  = juce::jlimit (0.0f, maxMidBoostDb, sane (p.midLowBoostDb));
        params.midDipDb       = juce::jlimit (0.0f, maxMidDipDb,   sane (p.midDipDb));
        params.midHighBoostDb = juce::jlimit (0.0f, maxMidBoostDb, sane (p.midHighBoostDb));
        params.lowBoostDb  = juce::jlimit (0.0f, maxLowBoostDb,  sane (p.lowBoostDb));
        params.lowCutDb    = juce::jlimit (0.0f, maxLowCutDb,    sane (p.lowCutDb));
        params.highBoostDb = juce::jlimit (0.0f, maxHighBoostDb, sane (p.highBoostDb));
        params.highCutDb   = juce::jlimit (0.0f, maxHighCutDb,   sane (p.highCutDb));
        params.bandwidth   = juce::jlimit (0.0f, 1.0f, sane (p.bandwidth));

        // A switch moved while sound is passing: keep the filters as they were and fade to the new ones.
        // Only the controls of the mode in use count: a switch of another mode changes nothing you can hear.
        const bool modeChanged = before.mode != params.mode;
        bool switchMoved = modeChanged;
        switch (params.mode)
        {
            case Mode::midShaper:
                switchMoved = switchMoved || before.midLowFrequencyIndex != params.midLowFrequencyIndex
                                          || before.midDipFrequencyIndex != params.midDipFrequencyIndex
                                          || before.midHighFrequencyIndex != params.midHighFrequencyIndex;
                break;
            case Mode::frequencyFilter:
                switchMoved = switchMoved || before.highPassIndex != params.highPassIndex || before.lowPassIndex != params.lowPassIndex;
                break;
            case Mode::toneShaper:
                switchMoved = switchMoved || before.lowFrequencyIndex != params.lowFrequencyIndex
                                          || before.highBoostFrequencyIndex != params.highBoostFrequencyIndex
                                          || before.highCutFrequencyIndex != params.highCutFrequencyIndex;
                break;
        }
        if (switchMoved && ! flat)
        {
            fadeSections = sections;
            fadeState = state;
            fadeRemaining = fadeLength;
        }

        // A different mode means different filters in each slot, so its gains start where they are going (the crossfade
        // above covers the change) instead of gliding from the other mode's amounts.
        const auto target = targetGains (params);
        for (size_t i = 0; i < 4; ++i)
        {
            if (modeChanged)
                gain[i].setCurrentAndTargetValue (target[i]);
            else
                gain[i].setTargetValue (target[i]);
        }
    }

    void Engine::updateSections (const std::array<float, 4>& g) noexcept
    {
        sections = buildSections (params, g, fs);
    }

    float Engine::responseDb (double frequency) const noexcept
    {
        float total = 0.0f;
        for (const auto& b : buildSections (params, targetGains (params), fs))
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

                flat = ! moving && fadeRemaining == 0 && isNeutral (params);
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
