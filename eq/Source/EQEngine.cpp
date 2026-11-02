#include "EQEngine.h"

namespace EQ
{
    //==========================================================================
    void Engine::Ramp::setTarget (double t, int length) noexcept
    {
        if (t == target)
            return;

        target = t;

        if (length <= 0)
        {
            current = t;
            remaining = 0;
            return;
        }

        remaining = length;
        step = (target - current) / (double) length;
    }

    void Engine::Ramp::advance (int n) noexcept
    {
        if (remaining <= 0)
            return;

        if (n >= remaining)
        {
            current = target;
            remaining = 0;
        }
        else
        {
            current += step * n;
            remaining -= n;
        }
    }

    //==========================================================================
    void Engine::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        smoothingSamples = std::max (1, (int) std::lround (smoothingSeconds * sampleRate));
        fadeSamples      = std::max (1, (int) std::lround (fadeSeconds * sampleRate));

        for (auto& b : bands)
            b.initialised = false;   // next setBand() snaps to its target

        reset();
    }

    void Engine::reset()
    {
        for (auto& b : bands)
        {
            for (auto& s : b.state)     s.reset();
            for (auto& s : b.prevState) s.reset();
            b.crossfading = false;
            b.coeffsDirty = true;
        }
    }

    void Engine::snapBand (Band& b)
    {
        b.type = b.target.type;
        b.logFreq.reset (std::log (clampFreqForSampleRate (b.target.freq, sampleRate)));
        b.gainDb.reset (b.target.gain);
        b.logQ.reset (std::log ((double) std::max (b.target.q, 0.01f)));
        b.wet.reset (b.target.active ? 1.0 : 0.0);
        b.xfade.reset (1.0);
        b.crossfading = false;
        b.coeffsDirty = true;
        b.initialised = true;
    }

    void Engine::snapToTargets()
    {
        for (auto& b : bands)
            snapBand (b);
    }

    void Engine::setBand (int index, const BandSettings& s)
    {
        auto& b = bands[(size_t) index];
        b.target = s;

        if (! b.initialised)
        {
            snapBand (b);
            return;
        }

        const bool silent = b.wet.current <= 0.0 && b.wet.target <= 0.0;

        if (s.active && silent)
        {
            // Fading in from nothing: no point sweeping from stale values.
            snapBand (b);
            for (auto& st : b.state) st.reset();
            b.wet.reset (0.0);
        }

        b.wet.setTarget (s.active ? 1.0 : 0.0, fadeSamples);
        b.logFreq.setTarget (std::log (clampFreqForSampleRate (s.freq, sampleRate)), smoothingSamples);
        b.gainDb.setTarget (s.gain, smoothingSamples);
        b.logQ.setTarget (std::log ((double) std::max (s.q, 0.01f)), smoothingSamples);
    }

    // Called at the start of every processing chunk.
    void Engine::beginBlockForBand (Band& b)
    {
        // Start a pending type change once any previous cross-fade has finished,
        // so rapid type automation never drops a fade half-way through.
        if (b.type != b.target.type && ! b.crossfading)
        {
            const bool audible = b.wet.current > 0.0 || b.wet.target > 0.0;

            if (audible)
            {
                b.prevCoeffs = b.coeffs;
                b.prevState  = b.state;
                b.xfade.reset (0.0);
                b.xfade.setTarget (1.0, fadeSamples);
                b.crossfading = true;
            }

            b.type = b.target.type;
            b.coeffsDirty = true;
        }

        if (b.coeffsDirty || b.logFreq.isRamping() || b.gainDb.isRamping() || b.logQ.isRamping())
        {
            b.coeffs = makeCoeffs (b.type, sampleRate,
                                   std::exp (b.logFreq.current),
                                   std::exp (b.logQ.current),
                                   b.gainDb.current);
            b.coeffsDirty = false;
        }
    }

    void Engine::processBand (Band& b, float* const* channels, int numChannels, int start, int num) noexcept
    {
        const bool simple = ! b.crossfading && ! b.wet.isRamping() && b.wet.current >= 1.0;

        if (simple)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = channels[ch] + start;
                auto& st = b.state[(size_t) ch];

                for (int i = 0; i < num; ++i)
                    data[i] = (float) processSample (b.coeffs, st, (double) data[i]);
            }
            return;
        }

        // Per-sample wet / cross-fade gains for this chunk (identical for every channel).
        double wetGain[updateInterval], xfGain[updateInterval];
        {
            Ramp w = b.wet, x = b.xfade;
            for (int i = 0; i < num; ++i)
            {
                w.advance (1);  x.advance (1);
                wetGain[i] = w.current;
                xfGain[i]  = x.current;
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = channels[ch] + start;
            auto& st  = b.state[(size_t) ch];
            auto& pst = b.prevState[(size_t) ch];

            for (int i = 0; i < num; ++i)
            {
                const double x = data[i];
                double y = processSample (b.coeffs, st, x);

                if (b.crossfading)
                {
                    const double yOld = processSample (b.prevCoeffs, pst, x);
                    y = yOld + (y - yOld) * xfGain[i];
                }

                data[i] = (float) (x + (y - x) * wetGain[i]);
            }
        }
    }

    void Engine::process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        for (int pos = 0; pos < numSamples; pos += updateInterval)
        {
            const int num = std::min (updateInterval, numSamples - pos);

            for (auto& b : bands)
            {
                if (! b.initialised)
                    continue;

                // Fully faded-out inactive band: skip, keep state clean for next activation.
                if (b.wet.current <= 0.0 && b.wet.target <= 0.0)
                    continue;

                beginBlockForBand (b);
                processBand (b, channels, numChannels, pos, num);

                b.logFreq.advance (num);
                b.gainDb.advance (num);
                b.logQ.advance (num);
                b.wet.advance (num);

                if (b.crossfading)
                {
                    b.xfade.advance (num);
                    if (! b.xfade.isRamping())
                        b.crossfading = false;
                }

                // Never let a numerical problem latch into permanent NaN/inf output.
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    if (! b.state[(size_t) ch].isFinite())     b.state[(size_t) ch].reset();
                    if (! b.prevState[(size_t) ch].isFinite()) b.prevState[(size_t) ch].reset();
                }

                if (b.wet.current <= 0.0 && b.wet.target <= 0.0)
                {
                    for (auto& s : b.state)     s.reset();
                    for (auto& s : b.prevState) s.reset();
                    b.crossfading = false;
                }
            }
        }
    }
}
