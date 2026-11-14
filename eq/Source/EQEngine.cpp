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
            b.dyn.reset();
            b.coeffsDirty = true;
        }

        solo.resetState();
        solo.mix.reset (0.0);
        solo.band = -1;
    }

    static int effectiveSlope (const BandSettings& s) noexcept
    {
        return typeHasSlope (s.type) ? juce::jlimit (0, numSlopes - 1, s.slope) : defaultSlope;
    }

    void Engine::snapBand (Band& b)
    {
        b.type = b.target.type;
        b.slope = effectiveSlope (b.target);
        b.matched = matched;
        b.stereo = b.target.stereo;
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

        // Stereo-mode change: the band works on other channels (or in M/S), so it can't
        // cross-fade in place. Fade it out, switch while silent, then fade back in.
        if (b.stereo != s.stereo && b.wet.current <= 0.0 && b.wet.target <= 0.0)
        {
            b.stereo = s.stereo;
            for (auto& st : b.state) st.reset();
        }
        const bool modeSettled = b.stereo == s.stereo;

        b.wet.setTarget (s.active && modeSettled ? 1.0 : 0.0, fadeSamples);
        b.logFreq.setTarget (std::log (clampFreqForSampleRate (s.freq, sampleRate)), smoothingSamples);
        b.gainDb.setTarget (s.gain, smoothingSamples);
        b.logQ.setTarget (std::log ((double) std::max (s.q, 0.01f)), smoothingSamples);
    }

    // Called at the start of every processing chunk.
    void Engine::beginBlockForBand (Band& b)
    {
        // Start a pending type change once any previous cross-fade has finished,
        // so rapid type automation never drops a fade half-way through.
        const int targetSlope = effectiveSlope (b.target);

        if ((b.type != b.target.type || b.slope != targetSlope || b.matched != matched) && ! b.crossfading)
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
            b.slope = targetSlope;
            b.matched = matched;
            b.coeffsDirty = true;
        }

        if (b.coeffsDirty || b.logFreq.isRamping() || b.gainDb.isRamping() || b.logQ.isRamping()
            || b.dyn.gainDb != b.dyn.appliedDb)
        {
            b.coeffs = makeFilter (b.type, sampleRate,
                                   std::exp (b.logFreq.current),
                                   std::exp (b.logQ.current),
                                   b.gainDb.current + b.dyn.gainDb,   // static (ramped) + dynamic
                                   b.slope,
                                   b.matched);
            b.coeffsDirty = false;
            b.dyn.appliedDb = b.dyn.gainDb;
        }
    }

    //==========================================================================
    // Dynamic EQ, run once per 16-sample chunk before the band's coefficients are updated
    // (so a 1 ms attack is followed within 0.33 ms at 48 kHz).
    //
    // Detector: the band's input (or the sidechain, if "external" and connected) through a
    // band-pass at the band's frequency and Q - or, for shelves, a 2nd-order low-pass /
    // high-pass at the corner (the region the shelf acts on) - then a one-pole mean-square
    // with a time constant of two periods of the band frequency (1..20 ms). Levels are
    // sine-peak calibrated (10 log10 (2 x mean square)), so a 0 dBFS sine reads 0 dB.
    // Channels are linked: the louder one drives the gain.
    //
    // Gain computer: soft knee 6 dB wide around the threshold; the full 'range' is reached
    // 12 dB above the threshold (an implicit ratio that scales with the range), linear in dB
    // in between. Attack/release smooth the dynamic gain in dB: attack while it moves away
    // from 0 dB, release while it returns.
    void Engine::runDetector (Band& b, float* const* channels, int first, int last, int start, int num) noexcept
    {
        const auto& t = b.target;
        auto& d = b.dyn;
        const bool on = t.dynOn && typeHasGain (b.type);

        if (! on)
        {
            d.running = false;
            if (d.gainDb != 0.0)   // glide back to the static gain
            {
                d.gainDb *= std::exp (-num / (std::max ((double) t.releaseMs, 10.0) * 0.001 * sampleRate));
                if (std::abs (d.gainDb) < 1.0e-4)
                    d.gainDb = 0.0;
            }
            return;
        }

        if (! d.running)
        {
            for (auto& s : d.detState) s.reset();
            for (auto& s : d.scState)  s.reset();
            d.power.fill (0.0);
            d.scPower.fill (0.0);
            d.running = true;
        }

        const double freq = std::exp (b.logFreq.current);
        if (b.type == FilterType::LowShelf)       d.detector = makeCoeffs (FilterType::HighCut, sampleRate, freq, 0.7071, 0.0);
        else if (b.type == FilterType::HighShelf) d.detector = makeCoeffs (FilterType::LowCut, sampleRate, freq, 0.7071, 0.0);
        else                                      d.detector = makeBandPass (sampleRate, freq, std::exp (b.logQ.current));

        const double tau = std::clamp (2.0 / freq, 0.001, 0.02);
        const double a = 1.0 - std::exp (-1.0 / (tau * sampleRate));

        auto detect = [&] (const float* x, SvfState& st, double& p)
        {
            for (int i = 0; i < num; ++i)
            {
                const double y = processSample (d.detector, st, (double) x[i]);
                p += a * (y * y - p);
            }
            if (! std::isfinite (p) || ! st.isFinite())
            {
                p = 0.0;
                st.reset();
            }
            return p;
        };

        double power = 0.0;
        if (t.dynExternal && sidechain != nullptr && numSidechain > 0)
        {
            for (int c = 0; c < std::min (numSidechain, maxChannels); ++c)
                power = std::max (power, detect (sidechain[c] + start, d.scState[(size_t) c], d.scPower[(size_t) c]));
        }
        else
        {
            for (int c = first; c <= last; ++c)
                power = std::max (power, detect (channels[c] + start, d.detState[(size_t) c], d.power[(size_t) c]));
        }

        const double levelDb = 10.0 * std::log10 (std::max (2.0 * power, 1.0e-20));

        constexpr double kneeDb = 6.0, fullRangeAboveDb = 12.0;
        const double over = levelDb - t.threshold;
        const double effective = over <= -kneeDb * 0.5 ? 0.0
                               : over >=  kneeDb * 0.5 ? over
                               : (over + kneeDb * 0.5) * (over + kneeDb * 0.5) / (2.0 * kneeDb);
        const double target = t.range * std::clamp (effective / fullRangeAboveDb, 0.0, 1.0);

        const bool attacking = std::abs (target) > std::abs (d.gainDb);
        const double timeMs = std::max ((double) (attacking ? t.attackMs : t.releaseMs), 0.05);
        d.gainDb += (target - d.gainDb) * (1.0 - std::exp (-num / (timeMs * 0.001 * sampleRate)));

        if (! std::isfinite (d.gainDb))
            d.gainDb = 0.0;
    }

    void Engine::processBand (Band& b, float* const* channels, int firstChannel, int lastChannel, int start, int num) noexcept
    {
        const bool simple = ! b.crossfading && ! b.wet.isRamping() && b.wet.current >= 1.0;

        if (simple)
        {
            for (int ch = firstChannel; ch <= lastChannel; ++ch)
            {
                auto* data = channels[ch] + start;
                auto& st = b.state[(size_t) ch];

                if (b.coeffs.isSingleSvf())   // every non-cut band and 12 dB/oct cuts
                {
                    for (int i = 0; i < num; ++i)
                        data[i] = (float) processSample (b.coeffs.svf[0], st.svf[0], (double) data[i]);
                }
                else
                {
                    for (int i = 0; i < num; ++i)
                        data[i] = (float) processFilter (b.coeffs, st, (double) data[i]);
                }
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

        for (int ch = firstChannel; ch <= lastChannel; ++ch)
        {
            auto* data = channels[ch] + start;
            auto& st  = b.state[(size_t) ch];
            auto& pst = b.prevState[(size_t) ch];

            for (int i = 0; i < num; ++i)
            {
                const double x = data[i];
                double y = processFilter (b.coeffs, st, x);

                if (b.crossfading)
                {
                    const double yOld = processFilter (b.prevCoeffs, pst, x);
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

            // Solo: keep this chunk's input, which the audition filter plays.
            const bool soloing = soloTarget >= 0 || solo.band >= 0;
            float soloInput[maxChannels][updateInterval];

            if (soloing)
            {
                updateSolo();
                for (int ch = 0; ch < numChannels; ++ch)
                    std::copy (channels[ch] + pos, channels[ch] + pos + num, soloInput[ch]);
            }

            bool midSide = false;   // is this chunk currently M/S-encoded?

            for (auto& b : bands)
            {
                if (! b.initialised)
                    continue;

                // Fully faded-out inactive band: skip, keep state clean for next activation.
                if (b.wet.current <= 0.0 && b.wet.target <= 0.0)
                    continue;

                // Stereo modes. Bands run in index order; the chunk is M/S-encoded only while
                // the next band needs it and decoded again before an L/R band, so a session
                // with every band in Stereo never converts and stays bit-exact. A mono bus
                // treats every mode as Stereo.
                const auto mode = numChannels < 2 ? StereoMode::Stereo : b.stereo;
                const bool wantMidSide = isMidSide (mode);
                if (wantMidSide != midSide)
                {
                    if (wantMidSide) encodeMidSide (channels, pos, num);
                    else             decodeMidSide (channels, pos, num);
                    midSide = wantMidSide;
                }

                const int first = (mode == StereoMode::Right || mode == StereoMode::Side) ? 1 : 0;
                const int last  = (mode == StereoMode::Stereo) ? numChannels - 1 : first;

                runDetector (b, channels, first, last, pos, num);
                beginBlockForBand (b);
                processBand (b, channels, first, last, pos, num);

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
                    if (! b.state[(size_t) ch].isFinite (b.coeffs)) b.state[(size_t) ch].reset();
                    if (b.crossfading && ! b.prevState[(size_t) ch].isFinite (b.prevCoeffs)) b.prevState[(size_t) ch].reset();
                }

                if (b.wet.current <= 0.0 && b.wet.target <= 0.0)
                {
                    for (auto& s : b.state)     s.reset();
                    for (auto& s : b.prevState) s.reset();
                    b.crossfading = false;
                    b.dyn.reset();
                }
            }

            if (midSide)
                decodeMidSide (channels, pos, num);

            if (soloing)
                processSolo (channels, soloInput, numChannels, pos, num);
        }
    }

    //==========================================================================
    void Engine::encodeMidSide (float* const* channels, int start, int num) noexcept
    {
        auto* l = channels[0] + start;
        auto* r = channels[1] + start;
        for (int i = 0; i < num; ++i)
        {
            const float m = (l[i] + r[i]) * 0.5f, s = (l[i] - r[i]) * 0.5f;
            l[i] = m;
            r[i] = s;
        }
    }

    void Engine::decodeMidSide (float* const* channels, int start, int num) noexcept
    {
        auto* m = channels[0] + start;
        auto* s = channels[1] + start;
        for (int i = 0; i < num; ++i)
        {
            const float l = m[i] + s[i], r = m[i] - s[i];
            m[i] = l;
            s[i] = r;
        }
    }

    //==========================================================================
    // Solo state machine, run once per chunk. A different band (or a type change on the
    // soloed band) first fades the current audition out, then switches filters while
    // silent and fades back in, so the filter is never swapped while audible.
    void Engine::updateSolo() noexcept
    {
        const int target = (soloTarget >= 0 && soloTarget < numBands) ? soloTarget : -1;
        const bool mismatch = target != solo.band
                           || (target >= 0 && bands[(size_t) target].target.type != solo.type);

        if (mismatch)
        {
            if (solo.band >= 0 && solo.mix.current > 0.0)
            {
                solo.mix.setTarget (0.0, fadeSamples);   // fade out the old audition first
                return;
            }

            solo.band = target;
            solo.resetState();
            solo.mix.reset (0.0);

            if (target < 0)
                return;

            const auto& t = bands[(size_t) target].target;
            solo.type = t.type;
            solo.logFreq.reset (std::log (clampFreqForSampleRate (t.freq, sampleRate)));
            solo.logQ.reset (std::log ((double) std::max (t.q, 0.01f)));
        }

        if (solo.band >= 0)
        {
            const auto& t = bands[(size_t) solo.band].target;
            solo.logFreq.setTarget (std::log (clampFreqForSampleRate (t.freq, sampleRate)), smoothingSamples);
            solo.logQ.setTarget (std::log ((double) std::max (t.q, 0.01f)), smoothingSamples);
            solo.mix.setTarget (1.0, fadeSamples);
        }
    }

    void Engine::processSolo (float* const* channels, const float (*input)[updateInterval],
                              int numChannels, int start, int num) noexcept
    {
        if (solo.band < 0 || (solo.mix.current <= 0.0 && ! solo.mix.isRamping()))
            return;

        makeSoloFilter (solo.type, sampleRate, std::exp (solo.logFreq.current), std::exp (solo.logQ.current), solo.coeffs.data());

        double mixGain[updateInterval];
        {
            Ramp m = solo.mix;
            for (int i = 0; i < num; ++i)
            {
                m.advance (1);
                mixGain[i] = m.current;
            }
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = channels[ch] + start;
            auto& st = solo.state[(size_t) ch];

            for (int i = 0; i < num; ++i)
            {
                double y = input[ch][i];
                for (int s = 0; s < numSoloSections; ++s)
                    y = processSample (solo.coeffs[(size_t) s], st[(size_t) s], y);

                data[i] = (float) (data[i] + (y - data[i]) * mixGain[i]);
            }

            for (auto& s : st)
                if (! s.isFinite())
                    s.reset();
        }

        solo.mix.advance (num);
        solo.logFreq.advance (num);
        solo.logQ.advance (num);

        if (solo.mix.current <= 0.0 && ! solo.mix.isRamping() && soloTarget < 0)
        {
            solo.band = -1;   // fully faded out: back to bit-identical normal processing
            solo.resetState();
        }
    }
}
