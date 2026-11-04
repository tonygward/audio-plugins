// Feedback engine tests (step 2). The measurement definitions used here are documented in
// analog-compressor/docs/engine.md and printed by `AnalogCompressorTests --timing`.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <random>
#include <vector>
#include "FETEngine.h"
#include "FETParams.h"
#include "PluginProcessor.h"
#include "EngineMeasure.h"

using namespace FET;

namespace
{
    float dbOf (float x) { return 20.0f * std::log10 (std::max (1.0e-12f, x)); }
}

//==============================================================================
struct TransferTests final : juce::UnitTest
{
    TransferTests() : juce::UnitTest ("Engine transfer curves", "FET") {}

    void runTest() override
    {
        for (int r = 0; r < Engine::numRatios; ++r)
        {
            const float ratio = Engine::ratios[(size_t) r];
            beginTest ("static curve at " + juce::String (ratio, 0) + ":1");

            Engine e;
            e.prepare (48000.0);
            e.setRatioIndex (r);
            const float t = Engine::thresholdsDb[(size_t) r], w = Engine::kneeWidthsDb[(size_t) r];

            // Well below the knee: no gain reduction.
            expectWithinAbsoluteError (e.staticGainReductionDb (t - w), 0.0f, 1.0e-4f, "below knee");

            // Well above the knee: output slope = 1 / ratio.
            const float in1 = t + 0.5f * w * ratio + 5.0f, in2 = in1 + 10.0f;   // output above the knee
            const float out1 = in1 - e.staticGainReductionDb (in1), out2 = in2 - e.staticGainReductionDb (in2);
            expectWithinAbsoluteError ((out2 - out1) / 10.0f, 1.0f / ratio, 0.01f, "slope above the knee");

            // Monotonic, continuous (soft knee) and never above unity gain.
            float prevOut = -200.0f, prevSlope = 0.0f;
            bool monotonic = true, smooth = true, nonNegative = true;
            for (float in = t - 30.0f; in <= t + 30.0f; in += 0.01f)
            {
                const float g = e.staticGainReductionDb (in);
                const float out = in - g;
                const float slope = (out - prevOut) / 0.01f;
                monotonic = monotonic && out >= prevOut;
                if (in > t - 29.0f)
                    smooth = smooth && std::abs (slope - prevSlope) < 0.1f;
                nonNegative = nonNegative && g >= 0.0f;
                prevOut = out;
                prevSlope = slope;
            }
            expect (monotonic, "monotonic");
            expect (smooth, "soft knee: no slope jumps");
            expect (nonNegative, "never boosts");

            // The running engine settles on the static curve (constant-envelope input).
            for (float over : { 5.0f, 15.0f, 30.0f })
            {
                const float level = t + over;
                const float measured = Measure::settledGrDb (e, 48000.0, level);
                expectWithinAbsoluteError (measured, e.staticGainReductionDb (level), 0.05f,
                                           "dynamic settles on the static curve at +" + juce::String (over, 0) + " dB");
            }
        }

        beginTest ("threshold rises with ratio (UM p. 21)");
        for (int r = 1; r < Engine::numRatios; ++r)
            expectGreaterThan (Engine::thresholdsDb[(size_t) r], Engine::thresholdsDb[(size_t) r - 1]);

        beginTest ("calibration: 20:1, 1 kHz sine at -18 dBFS RMS gives 1 dB GR");
        {
            Engine e;
            e.prepare (48000.0);
            e.setRatioIndex (3);
            e.setAttackKnob (4.0f);
            e.setReleaseKnob (4.0f);
            const float gr = Measure::sineGrDb (e, 48000.0, 1000.0, -18.0f);
            expectWithinAbsoluteError (gr, 1.0f, 0.25f, "measured " + juce::String (gr, 2) + " dB");
        }
    }
};

//==============================================================================
struct TimingTests final : juce::UnitTest
{
    TimingTests() : juce::UnitTest ("Engine timing", "FET") {}

    void runTest() override
    {
        beginTest ("knob mapping: clockwise is faster, ends match the published ranges");
        expectWithinAbsoluteError (Engine::attackSeconds (1.0f), 800.0e-6, 1.0e-9);
        expectWithinAbsoluteError (Engine::attackSeconds (7.0f), 20.0e-6, 1.0e-9);
        expectWithinAbsoluteError (Engine::releaseSeconds (1.0f), 1.1, 1.0e-9);
        expectWithinAbsoluteError (Engine::releaseSeconds (7.0f), 0.05, 1.0e-9);
        for (float k = 1.0f; k < 7.0f; k += 0.5f)
        {
            expectGreaterThan (Engine::attackSeconds (k), Engine::attackSeconds (k + 0.5f));
            expectGreaterThan (Engine::releaseSeconds (k), Engine::releaseSeconds (k + 0.5f));
        }

        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("step-response timing at " + juce::String (fs, 0) + " Hz");
            for (int r : { 0, 3 })
                for (float knob : { 1.0f, 4.0f, 7.0f })
                    for (float over : { 20.0f, 30.0f })
                    {
                        const auto label = juce::String (Engine::ratios[(size_t) r], 0) + ":1 knob "
                                         + juce::String (knob, 0) + " +" + juce::String (over, 0) + " dB";

                        const double nominalA = Engine::attackSeconds (knob);
                        const double a = Measure::attackSeconds (fs, r, knob, over);
                        // ±25 % or ±1 sample, whichever is larger (sub-sample limit at 20 µs).
                        const double tolA = std::max (0.25 * nominalA, 1.0 / fs);
                        expectWithinAbsoluteError (a, nominalA, tolA, "attack " + label + ": " + juce::String (a * 1.0e6, 1) + " us");

                        const double nominalR = Engine::releaseSeconds (knob);
                        const double rel = Measure::releaseSeconds (fs, r, knob, over);
                        expectWithinAbsoluteError (rel, nominalR, 0.05 * nominalR, "release " + label + ": " + juce::String (rel * 1.0e3, 1) + " ms");
                    }
        }
    }
};

//==============================================================================
struct EngineBehaviourTests final : juce::UnitTest
{
    EngineBehaviourTests() : juce::UnitTest ("Engine behaviour", "FET") {}

    void runTest() override
    {
        beginTest ("stereo is linked: both channels get the same gain");
        {
            Engine e;
            e.prepare (48000.0);
            std::vector<float> l (4800), r (4800);
            for (size_t i = 0; i < l.size(); ++i)
            {
                l[i] = 0.9f * (float) std::sin (0.05 * (double) i);
                r[i] = 0.01f * (float) std::sin (0.05 * (double) i + 1.0);
            }
            auto l0 = l, r0 = r;
            float* ch[] = { l.data(), r.data() };
            e.process (ch, 2, (int) l.size());
            bool same = true;
            for (size_t i = 0; i < l.size(); ++i)
                if (std::abs (l0[i]) > 1.0e-3f && std::abs (r0[i]) > 1.0e-5f)
                    same = same && std::abs (l[i] / l0[i] - r[i] / r0[i]) < 1.0e-4f;
            expect (same);
            expectGreaterThan (e.getGainReductionDb(), 3.0f, "the loud channel compresses");
        }

        beginTest ("silence: no gain reduction, silent output, no NaN");
        {
            Engine e;
            e.prepare (44100.0);
            std::vector<float> s (44100, 0.0f);
            float* ch[] = { s.data() };
            e.process (ch, 1, (int) s.size());
            expectEquals (e.getGainReductionDb(), 0.0f);
            bool silent = true;
            for (auto v : s) silent = silent && v == 0.0f;
            expect (silent);
        }

        beginTest ("hot input: finite and held down");
        {
            Engine e;
            e.prepare (44100.0);
            e.setRatioIndex (3);
            e.setAttackKnob (7.0f);
            std::vector<float> s (44100);
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = 100.0f * (float) std::sin (0.1 * (double) i);   // +40 dBFS
            float* ch[] = { s.data() };
            e.process (ch, 1, (int) s.size());
            bool finite = true;
            float peak = 0.0f;
            for (size_t i = 4410; i < s.size(); ++i)
            {
                finite = finite && std::isfinite (s[i]);
                peak = std::max (peak, std::abs (s[i]));
            }
            expect (finite);
            expectLessThan (dbOf (peak), Engine::thresholdsDb[3] + 6.0f, "limited to near the threshold");
        }

        beginTest ("variable block sizes give identical output");
        {
            std::vector<float> a (20000), b;
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> d (-1.0f, 1.0f);
            for (size_t i = 0; i < a.size(); ++i)
                a[i] = d (rng) * (i % 5000 < 2500 ? 0.9f : 0.05f);
            b = a;

            Engine e1, e2;
            e1.prepare (48000.0);
            e2.prepare (48000.0);
            float* c1[] = { a.data() };
            e1.process (c1, 1, (int) a.size());

            std::uniform_int_distribution<int> sizes (1, 700);
            int pos = 0;
            while (pos < (int) b.size())
            {
                const int n = std::min (sizes (rng), (int) b.size() - pos);
                float* c2[] = { b.data() + pos };
                e2.process (c2, 1, n);
                pos += n;
            }
            expect (a == b);
        }

        beginTest ("ratio and knob changes are click-free");
        {
            Engine e;
            e.prepare (48000.0);
            e.setReleaseKnob (7.0f);
            std::vector<float> s (48000);
            for (size_t i = 0; i < s.size(); ++i)
                s[i] = 0.5f;   // DC: any gain step shows up as a sample step
            float maxStep = 0.0f, prev = 0.0f;
            int pos = 0;
            for (int step = 0; step < 48; ++step)
            {
                e.setRatioIndex (step % 4);
                e.setAttackKnob (step % 2 == 0 ? 1.0f : 7.0f);
                float* ch[] = { s.data() + pos };
                e.process (ch, 1, 1000);
                for (int i = 0; i < 1000; ++i)
                {
                    if (pos + i > 2000)
                        maxStep = std::max (maxStep, std::abs (s[(size_t) (pos + i)] - prev));
                    prev = s[(size_t) (pos + i)];
                }
                pos += 1000;
            }
            expectLessThan (maxStep, 0.01f, "largest sample step " + juce::String (maxStep, 4));
        }

        beginTest ("gain reduction off fades GR out and then passes audio unchanged");
        {
            Engine e;
            e.prepare (48000.0);
            std::vector<float> s (48000, 0.5f);
            float* ch[] = { s.data() };
            e.process (ch, 1, 4800);
            expectGreaterThan (e.getGainReductionDb(), 1.0f);
            e.setGainReductionEnabled (false);
            float* ch2[] = { s.data() + 4800 };
            e.process (ch2, 1, 9600);
            expectEquals (e.getGainReductionDb(), 0.0f);
            bool unchanged = true;
            for (size_t i = 14000; i < 14400; ++i)
                unchanged = unchanged && s[i] == 0.5f;
            expect (unchanged);
        }
    }
};

//==============================================================================
struct ProcessorEngineTests final : juce::UnitTest
{
    ProcessorEngineTests() : juce::UnitTest ("Processor with engine", "FET") {}

    static float runGr (float outputDb)
    {
        SongsmithAnalogCompressorProcessor p;
        p.prepareToPlay (48000.0, 480);
        auto* o = p.apvts.getParameter (ID::output);
        o->setValueNotifyingHost (o->convertTo0to1 (outputDb));
        auto* in = p.apvts.getParameter (ID::input);
        in->setValueNotifyingHost (in->convertTo0to1 (12.0f));

        juce::AudioBuffer<float> b (2, 480);
        juce::MidiBuffer m;
        float gr = 0.0f;
        for (int k = 0; k < 200; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 480; ++i)
                    b.setSample (c, i, 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (k * 480 + i) / 48000.0));
            p.processBlock (b, m);
            gr = p.takeGainReductionDb();
        }
        return gr;
    }

    void runTest() override
    {
        beginTest ("Output trim is after the detector: it never changes gain reduction");
        const float a = runGr (-12.0f), b = runGr (12.0f);
        expectGreaterThan (a, 1.0f, "compressing");
        expectWithinAbsoluteError (a, b, 1.0e-4f);

        beginTest ("Input drives compression");
        SongsmithAnalogCompressorProcessor p;
        p.prepareToPlay (48000.0, 480);
        juce::AudioBuffer<float> buf (2, 480);
        juce::MidiBuffer m;
        float grLow = 0.0f, grHigh = 0.0f;
        for (float drive : { 0.0f, 20.0f })
        {
            auto* in = p.apvts.getParameter (ID::input);
            in->setValueNotifyingHost (in->convertTo0to1 (drive));
            for (int k = 0; k < 200; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 480; ++i)
                        buf.setSample (c, i, 0.1f * (float) std::sin (0.05 * (k * 480 + i)));
                p.processBlock (buf, m);
            }
            (drive == 0.0f ? grLow : grHigh) = p.takeGainReductionDb();
        }
        expectGreaterThan (grHigh, grLow + 5.0f);
    }
};

static TransferTests transferTests;
static TimingTests timingTests;
static EngineBehaviourTests engineBehaviourTests;
static ProcessorEngineTests processorEngineTests;
