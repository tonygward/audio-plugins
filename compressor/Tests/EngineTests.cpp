// Songsmith Compressor engine tests (COMP-01). Run through CompressorTests.exe (CompressorTests.cpp has main).
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include "CompressorEngine.h"

using namespace SongsmithComp;

namespace
{
    float amp (float db) { return std::pow (10.0f, db / 20.0f); }

    // A constant-envelope test signal: samples alternate +a, -a, so the peak detector sees a true
    // level step at any sample rate.
    void runConstant (Engine& e, float levelDb, int samples, std::vector<float>* grOut = nullptr)
    {
        const float a = amp (levelDb);
        for (int i = 0; i < samples; ++i)
        {
            float x = (i & 1) ? -a : a;
            float* ch[] = { &x };
            e.process (ch, 1, 1);
            if (grOut != nullptr)
                grOut->push_back (e.getGainReductionDb());
        }
    }

    float settledGrDb (Engine& e, double fs, float levelDb)
    {
        e.reset();
        runConstant (e, levelDb, (int) (fs * 5.0));
        return e.getGainReductionDb();
    }

    // Time in ms for the gain reduction to reach `fraction` of its final value, interpolated.
    float timeToFraction (const std::vector<float>& gr, double fs, float fraction, bool rising)
    {
        const float start = gr.front(), end = gr.back();
        const float target = start + (end - start) * fraction;
        for (size_t i = 1; i < gr.size(); ++i)
        {
            const bool crossed = rising ? gr[i] >= target : gr[i] <= target;
            if (crossed)
            {
                const float f = (target - gr[i - 1]) / (gr[i] - gr[i - 1]);
                return (float) (((double) i - 1.0 + (double) f) * 1000.0 / fs);
            }
        }
        return -1.0f;
    }
}

class CompressorEngineTests final : public juce::UnitTest
{
public:
    CompressorEngineTests() : juce::UnitTest ("Compressor engine", "Compressor") {}

    static Params base()
    {
        Params p;
        p.thresholdDb = -24.0f; p.ratio = 4.0f; p.attackMs = 10.0f; p.releaseMs = 100.0f;
        p.kneeDb = 0.0f; p.makeupDb = 0.0f; p.autoMakeup = false; p.mix = 1.0f;
        return p;
    }

    void runTest() override
    {
        beginTest ("Static curve: below the threshold nothing; above it, the ratio sets the slope");
        for (float ratio : { 1.2f, 1.3f, 1.5f, 2.0f, 4.0f, 10.0f, 20.0f })
        {
            expectWithinAbsoluteError (staticGainDb (-40.0f, -24.0f, ratio, 0.0f), 0.0f, 1.0e-6f, "below threshold");
            expectWithinAbsoluteError (staticGainDb (-24.0f, -24.0f, ratio, 0.0f), 0.0f, 1.0e-6f, "at threshold");
            const float out = -24.0f + 12.0f / ratio;   // 12 dB over the threshold
            expectWithinAbsoluteError (staticGainDb (-12.0f, -24.0f, ratio, 0.0f), out - (-12.0f), 1.0e-4f, "ratio " + juce::String (ratio));
        }
        expectWithinAbsoluteError (staticGainDb (0.0f, -60.0f, 1.0f, 6.0f), 0.0f, 1.0e-6f, "1:1 never reduces");

        beginTest ("Soft knee: continuous, starts half a knee below the threshold, joins the straight line half a knee above");
        for (float knee : { 2.0f, 6.0f, 12.0f, 24.0f })
        {
            const float t = -24.0f, r = 4.0f;
            expectWithinAbsoluteError (staticGainDb (t - knee * 0.5f, t, r, knee), 0.0f, 1.0e-5f, "knee start");
            expectWithinAbsoluteError (staticGainDb (t + knee * 0.5f, t, r, knee), (1.0f / r - 1.0f) * knee * 0.5f, 1.0e-4f, "knee end");
            float last = 0.0f;
            for (float x = t - knee; x <= t + knee; x += 0.05f)
            {
                const float g = staticGainDb (x, t, r, knee);
                expect (g <= last + 1.0e-5f, "never rises as the level rises");
                expect (std::abs (g - last) < 0.2f, "no jumps");
                last = g;
            }
        }

        beginTest ("1:1 produces no gain reduction and leaves the signal bit-identical");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.ratio = 1.0f; p.thresholdDb = -60.0f; p.kneeDb = 12.0f;
            e.setParams (p); e.reset();
            juce::Random rng (7);
            std::vector<float> in (4096), out;
            for (auto& s : in) s = rng.nextFloat() * 1.8f - 0.9f;
            out = in;
            float* ch[] = { out.data() };
            e.process (ch, 1, (int) out.size());
            expect (out == in, "identical samples");
            expectEquals (e.readMeters().gainReductionDb, 0.0f);
        }

        beginTest ("Settled gain reduction matches the static curve at 44.1, 48 and 96 kHz");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float ratio : { 1.3f, 2.0f, 10.0f, 20.0f })
                for (float knee : { 0.0f, 12.0f })
                {
                    Engine e; e.prepare (fs);
                    auto p = base(); p.ratio = ratio; p.kneeDb = knee;
                    e.setParams (p);
                    const float expected = -staticGainDb (-10.0f, p.thresholdDb, ratio, knee);
                    expectWithinAbsoluteError (settledGrDb (e, fs, -10.0f), expected, 0.02f,
                                               juce::String (fs) + " Hz, ratio " + juce::String (ratio) + ", knee " + juce::String (knee));
                }

        beginTest ("Attack: the reduction reaches 63 % of its settled value after the attack time (0.1, 10, 30, 100 ms)");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float attack : { 0.1f, 10.0f, 30.0f, 100.0f })
            {
                Engine e; e.prepare (fs);
                auto p = base(); p.attackMs = attack; p.releaseMs = 2000.0f; p.ratio = 20.0f;
                e.setParams (p); e.reset();
                std::vector<float> gr;
                runConstant (e, -60.0f, 100, &gr);   // quiet: no reduction
                gr.clear();
                gr.push_back (0.0f);
                runConstant (e, 0.0f, (int) (fs * attack * 0.001 * 8.0) + 64, &gr);
                const float t = timeToFraction (gr, fs, 1.0f - std::exp (-1.0f) , true);
                // Compare with the settled value of the same run's final sample, which is nearly the target.
                const float tolerance = juce::jmax (0.05f * attack, 1.5f * 1000.0f / (float) fs);
                expectWithinAbsoluteError (t, attack, tolerance + 0.01f * attack,
                                           juce::String (fs) + " Hz, attack " + juce::String (attack) + " ms measured " + juce::String (t));
            }

        beginTest ("Release: the reduction falls to 37 % after the release time (10, 30, 50, 2000 ms)");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (float release : { 10.0f, 30.0f, 50.0f, 2000.0f })
            {
                Engine e; e.prepare (fs);
                auto p = base(); p.attackMs = 0.1f; p.releaseMs = release; p.ratio = 20.0f;
                e.setParams (p); e.reset();
                runConstant (e, 0.0f, (int) (fs * 0.1));   // settle deep
                const float start = e.getGainReductionDb();
                std::vector<float> gr { start };
                runConstant (e, -80.0f, (int) (fs * release * 0.001 * 6.0), &gr);
                // Falling to the target (zero): find the time the value drops to 1/e of the start.
                float t = -1.0f;
                for (size_t i = 1; i < gr.size(); ++i)
                    if (gr[i] <= start * std::exp (-1.0f))
                    {
                        const float f = (start * std::exp (-1.0f) - gr[i - 1]) / (gr[i] - gr[i - 1]);
                        t = (float) (((double) i - 1.0 + (double) f) * 1000.0 / fs);
                        break;
                    }
                expectWithinAbsoluteError (t, release, 0.06f * release + 1000.0f / (float) fs,
                                           juce::String (fs) + " Hz, release " + juce::String (release) + " ms measured " + juce::String (t));
            }

        beginTest ("Very low threshold and a gentle ratio give steady, continuous gentle reduction");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.thresholdDb = -60.0f; p.ratio = 1.3f; p.attackMs = 30.0f; p.releaseMs = 50.0f; p.kneeDb = 12.0f;
            e.setParams (p); e.reset();
            juce::Random rng (3);
            std::vector<float> gr;
            for (int block = 0; block < 200; ++block)
            {
                std::vector<float> in (480);
                for (auto& s : in) s = (rng.nextFloat() * 2.0f - 1.0f) * 0.01f;   // peaks at -40 dBFS, 20 dB over the threshold
                float* ch[] = { in.data() };
                e.process (ch, 1, (int) in.size());
                if (block > 20) gr.push_back (e.getGainReductionDb());
            }
            float lo = 1000.0f, hi = -1000.0f;
            for (auto g : gr) { lo = juce::jmin (lo, g); hi = juce::jmax (hi, g); }
            expectGreaterThan (lo, 0.5f, "always reducing a little");
            expectLessThan (hi, 6.0f, "never more than gentle");
        }

        beginTest ("Block size does not change the result");
        {
            auto render = [] (int block)
            {
                Engine e; e.prepare (48000.0);
                auto p = base(); p.kneeDb = 6.0f; p.makeupDb = 3.0f; p.mix = 0.8f;
                e.setParams (p); e.reset();
                juce::Random rng (11);
                std::vector<float> l (6000), r (6000);
                for (size_t i = 0; i < l.size(); ++i) { l[i] = rng.nextFloat() * 1.6f - 0.8f; r[i] = rng.nextFloat() * 1.6f - 0.8f; }
                for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
                {
                    const int n = (int) juce::jmin ((size_t) block, l.size() - pos);
                    float* ch[] = { l.data() + pos, r.data() + pos };
                    e.process (ch, 2, n);
                }
                return std::make_pair (l, r);
            };
            const auto a = render (6000), b = render (1), c = render (7), d = render (512);
            expect (a == b && a == c && a == d, "identical for block sizes 1, 7, 512 and 6000");
        }

        beginTest ("Stereo link: both channels get the same gain, set by the louder one");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.ratio = 10.0f;
            e.setParams (p); e.reset();
            float lastL = 0.0f, lastR = 0.0f;
            const float quiet = 0.02f;   // -34 dBFS, under the threshold on its own
            for (int i = 0; i < 48000; ++i)
            {
                const float s = (i & 1) ? -1.0f : 1.0f;
                float l = 0.8f * s, r = quiet * s;
                float* ch[] = { &l, &r };
                e.process (ch, 2, 1);
                lastL = std::abs (l); lastR = std::abs (r);
            }
            expectGreaterThan (e.getGainReductionDb(), 5.0f);
            expectWithinAbsoluteError (lastL / 0.8f, lastR / quiet, 1.0e-5f, "same gain on both channels");
            expectLessThan (lastR / quiet, 0.9f, "the quiet channel is turned down with the loud one");
        }

        beginTest ("Mix: 0 % is the untouched input, 100 % fully compressed, 50 % halfway between");
        {
            auto render = [] (float mix)
            {
                Engine e; e.prepare (48000.0);
                auto p = base(); p.ratio = 20.0f; p.mix = mix;
                e.setParams (p); e.reset();
                runConstant (e, 0.0f, 48000);
                float x = 0.5f;
                for (int i = 0; i < 2; ++i) { x = 0.5f; float* ch[] = { &x }; e.process (ch, 1, 1); }
                return x;
            };
            const float dry = render (0.0f), wet = render (1.0f), half = render (0.5f);
            expectWithinAbsoluteError (dry, 0.5f, 1.0e-6f, "mix 0");
            expectLessThan (wet, 0.45f, "mix 100 reduces");
            expectWithinAbsoluteError (half, 0.5f * (dry + wet), 1.0e-3f, "mix 50");
        }

        beginTest ("Make-up gain adds its dB; auto make-up matches the level at the reference and is never negative");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.thresholdDb = -30.0f; p.ratio = 1.3f; p.kneeDb = 0.0f; p.attackMs = 0.1f;
            p.autoMakeup = true;
            e.setParams (p); e.reset();
            runConstant (e, autoMakeupReferenceDb, 48000 * 2);
            e.readMeters();
            runConstant (e, autoMakeupReferenceDb, 4800);
            const auto m = e.readMeters();
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (m.outputPeak) - juce::Decibels::gainToDecibels (m.inputPeak),
                                       0.0f, 0.05f, "level matched at the reference level");
            expectGreaterThan (autoMakeupDb (p), 0.0f);
            auto none = p; none.thresholdDb = 0.0f;
            expectEquals (autoMakeupDb (none), 0.0f, "nothing to restore when nothing is reduced");

            Engine f; f.prepare (48000.0);
            auto q = base(); q.thresholdDb = 0.0f; q.makeupDb = 6.0f;
            f.setParams (q); f.reset();
            runConstant (f, -20.0f, 4800);
            const auto fm = f.readMeters();
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (fm.outputPeak) - juce::Decibels::gainToDecibels (fm.inputPeak), 6.0f, 0.02f);
        }

        beginTest ("Extreme settings stay in range: threshold -60 and 0, ratio 20, out-of-range and non-finite values");
        {
            Engine e; e.prepare (48000.0);
            for (float th : { -60.0f, 0.0f })
                for (float ratio : { 1.0f, 20.0f })
                {
                    auto p = base(); p.thresholdDb = th; p.ratio = ratio;
                    e.setParams (p); e.reset();
                    runConstant (e, 0.0f, 9600);
                    expect (std::isfinite (e.getGainReductionDb()) && e.getGainReductionDb() <= 60.1f, "finite and bounded");
                }
            Params wild;
            wild.thresholdDb = -1000.0f; wild.ratio = 1.0e9f; wild.attackMs = -5.0f; wild.releaseMs = 1.0e9f;
            wild.kneeDb = std::numeric_limits<float>::quiet_NaN(); wild.makeupDb = 999.0f; wild.mix = 7.0f;
            e.setParams (wild);
            runConstant (e, -3.0f, 4800);
            expect (std::isfinite (e.getGainReductionDb()), "finite with wild parameters");
        }

        beginTest ("Silence, non-finite input and very quiet input: no NaNs, no stuck reduction, exact silence out");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.releaseMs = 10.0f;
            e.setParams (p); e.reset();
            std::vector<float> in (4800, 0.0f);
            float* ch[] = { in.data() };
            e.process (ch, 1, (int) in.size());
            for (auto s : in) expectEquals (s, 0.0f);

            std::vector<float> bad (64, std::numeric_limits<float>::infinity());
            bad[3] = std::numeric_limits<float>::quiet_NaN();
            float* bc[] = { bad.data() };
            e.process (bc, 1, (int) bad.size());
            for (auto s : bad) expect (std::isfinite (s), "finite output for non-finite input");

            runConstant (e, 0.0f, 4800);
            expectGreaterThan (e.getGainReductionDb(), 1.0f);
            runConstant (e, -140.0f, 48000 * 3);
            expectEquals (e.getGainReductionDb(), 0.0f, "reduction fully released, no denormal tail");
        }

        beginTest ("Parameter changes while running are smooth: no step in the output when make-up or mix jumps");
        {
            Engine e; e.prepare (48000.0);
            auto p = base(); p.thresholdDb = 0.0f;
            e.setParams (p); e.reset();
            std::vector<float> out;
            const float a = 0.25f;
            for (int i = 0; i < 4800; ++i)
            {
                if (i == 2400) { p.makeupDb = 12.0f; e.setParams (p); }
                float x = a;   // constant (DC) so any step is the gain, not the signal
                float* ch[] = { &x };
                e.process (ch, 1, 1);
                out.push_back (x);
            }
            float maxStep = 0.0f;
            for (size_t i = 1; i < out.size(); ++i) maxStep = juce::jmax (maxStep, std::abs (out[i] - out[i - 1]));
            expectLessThan (maxStep, 0.01f, "a 12 dB make-up jump is ramped");
            expectWithinAbsoluteError (out.back(), a * amp (12.0f), 0.01f, "and arrives");
        }

        beginTest ("Meters report the deepest reduction and the input and output peaks, then reset");
        {
            Engine e; e.prepare (48000.0);
            e.setParams (base()); e.reset();
            runConstant (e, -6.0f, 4800);
            const auto m = e.readMeters();
            expectWithinAbsoluteError (m.inputPeak, amp (-6.0f), 1.0e-5f);
            expectGreaterThan (m.gainReductionDb, 1.0f);
            expectLessThan (m.outputPeak, m.inputPeak, "compressed");
            const auto again = e.readMeters();
            expectEquals (again.inputPeak, 0.0f);
            expectEquals (again.gainReductionDb, 0.0f);
        }
    }
};

static CompressorEngineTests compressorEngineTests;
