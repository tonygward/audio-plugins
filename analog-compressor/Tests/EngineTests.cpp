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

//==============================================================================
// ACOMP-07: detector Low Cut. Definitions in analog-compressor/docs/detector-low-cut.md.
struct DetectorLowCutTests final : juce::UnitTest
{
    DetectorLowCutTests() : juce::UnitTest ("Detector Low Cut", "FET") {}

    static float lowCutIndexFor (float hz)
    {
        for (int i = 0; i < numLowCutSteps; ++i)
            if (lowCutHz[i] == hz)
                return (float) i;
        return 0.0f;
    }

    // Settled average GR (dB) of the engine for a sine at the given RMS level, optionally with a second channel.
    static float engineGr (double fs, float cutHz, double freq, float rmsDb, double freq2 = 0.0, float rmsDb2 = -100.0f)
    {
        FET::Engine e;
        Measure::configure (e, fs, 0, 4.0f);
        e.setDetectorLowCutHz (cutHz);
        e.reset();
        const float a = Measure::amp (rmsDb) * std::sqrt (2.0f), a2 = Measure::amp (rmsDb2) * std::sqrt (2.0f);
        const int settle = (int) (fs * 2.0), measure = (int) (fs * 0.5);
        double sum = 0.0;
        for (int i = 0; i < settle + measure; ++i)
        {
            float l = a * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / fs);
            float r = freq2 > 0.0 ? a2 * (float) std::sin (juce::MathConstants<double>::twoPi * freq2 * i / fs) : 0.0f;
            float* ch[] = { &l, &r };
            e.process (ch, 2, 1);
            if (i >= settle)
                sum += e.getGainReductionDb();
        }
        return (float) (sum / measure);
    }

    // The processor with the clean engine, a settled sine, and the largest GR of the last block.
    static float processorGr (int oversampling, float cutHz, double freq, float amp)
    {
        SongsmithAnalogCompressorProcessor p;
        p.setColourEnabled (false);
        auto set = [&] (const char* id, float v) { auto* rp = p.apvts.getParameter (id); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
        set (ID::oversampling, (float) oversampling);
        set (ID::detectorLowCut, lowCutIndexFor (cutHz));
        set (ID::input, 12.0f);
        p.prepareToPlay (48000.0, 480);
        juce::AudioBuffer<float> b (2, 480);
        juce::MidiBuffer m;
        float gr = 0.0f;
        for (int k = 0; k < 250; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 480; ++i)
                    b.setSample (c, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (k * 480 + i) / 48000.0));
            p.processBlock (b, m);
            gr = p.takeGainReductionDb();
        }
        return gr;
    }

    void runTest() override
    {
        beginTest ("Off is the default and the setting list is Off plus practical frequencies");
        {
            expectEquals (lowCutNames().joinIntoString (","), juce::String ("Off,60 Hz,80 Hz,100 Hz,150 Hz,200 Hz"));
            expectEquals (lowCutDefault, 0);
            expectEquals (lowCutHz[0], 0.0f);
            for (int i = 1; i < numLowCutSteps; ++i)
                expect (lowCutNames()[i] == juce::String ((int) lowCutHz[i]) + " Hz", "the name matches the frequency");
        }

        beginTest ("A bass-heavy signal gives less gain reduction with Low Cut on (engine, 44.1 and 48 kHz)");
        for (double fs : { 44100.0, 48000.0 })
        {
            const float off = engineGr (fs, 0.0f, 50.0, -12.0f);
            expectGreaterThan (off, 3.0f, "compressing without Low Cut");
            float previous = off;
            for (float hz : { 60.0f, 100.0f, 200.0f })
            {
                const float on = engineGr (fs, hz, 50.0, -12.0f);
                expectLessThan (on, previous + 0.05f, "a higher cutoff never compresses bass more");
                previous = on;
            }
            expectLessThan (engineGr (fs, 100.0f, 50.0, -12.0f), off - 3.0f, "100 Hz takes at least 3 dB off the bass's gain reduction");
            expectLessThan (engineGr (fs, 200.0f, 50.0, -12.0f), 0.5f, "200 Hz leaves almost none");
        }

        beginTest ("A vocal-range signal is still compressed the same with Low Cut on");
        {
            const float off = engineGr (48000.0, 0.0f, 1000.0, -12.0f);
            expectGreaterThan (off, 3.0f);
            for (float hz : { 60.0f, 80.0f, 100.0f, 150.0f, 200.0f })
                expectWithinAbsoluteError (engineGr (48000.0, hz, 1000.0, -12.0f), off, 0.15f, "1 kHz at " + juce::String (hz) + " Hz");
            // Roughly the 12 dB/oct slope at an octave above the cutoff: bass at twice the cutoff is down by about 1.5 dB.
            const float oneKhz = engineGr (48000.0, 0.0f, 400.0, -12.0f);
            expectWithinAbsoluteError (engineGr (48000.0, 200.0f, 400.0, -12.0f), oneKhz, 1.0f, "an octave above the cutoff");
        }

        beginTest ("Stereo link: the loudest filtered channel drives both channels, and the cut applies to both");
        {
            // Left: loud bass. Right: a moderate 1 kHz tone. With Low Cut the bass no longer leads, so the
            // linked reduction equals what the 1 kHz tone alone causes.
            const float both = engineGr (48000.0, 150.0f, 50.0, -6.0f, 1000.0, -14.0f);
            const float alone = engineGr (48000.0, 150.0f, 1000.0, -14.0f);
            expectWithinAbsoluteError (both, alone, 0.1f);
            const float withoutCut = engineGr (48000.0, 0.0f, 50.0, -6.0f, 1000.0, -14.0f);
            expectGreaterThan (withoutCut, alone + 3.0f, "without Low Cut the bass leads");
        }

        beginTest ("The filter is confined to the detector: with no gain reduction the audio is bit-exact at every setting");
        for (int os : { 0, 1, 2 })
            for (double freq : { 30.0, 50.0, 120.0, 1000.0, 8000.0 })
            {
                juce::AudioBuffer<float> reference;
                for (int pass = 0; pass < 2; ++pass)
                {
                    SongsmithAnalogCompressorProcessor p;
                    p.setColourEnabled (false);
                    auto set = [&] (const char* id, float v) { auto* rp = p.apvts.getParameter (id); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
                    set (ID::oversampling, (float) os);
                    set (ID::detectorLowCut, pass == 0 ? 0.0f : (float) (numLowCutSteps - 1));
                    p.prepareToPlay (48000.0, 480);
                    juce::AudioBuffer<float> out (2, 480 * 8);
                    juce::MidiBuffer m;
                    for (int k = 0; k < 8; ++k)
                    {
                        juce::AudioBuffer<float> b (2, 480);
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < 480; ++i)
                                b.setSample (c, i, 0.005f * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (k * 480 + i) / 48000.0));   // -46 dBFS: below every knee
                        p.processBlock (b, m);
                        for (int c = 0; c < 2; ++c)
                            out.copyFrom (c, k * 480, b, c, 0, 480);
                    }
                    expectEquals (p.takeGainReductionDb(), 0.0f, "no gain reduction at this level");
                    if (pass == 0)
                        reference.makeCopyOf (out);
                    else
                    {
                        bool same = true;
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < out.getNumSamples(); ++i)
                                same = same && out.getSample (c, i) == reference.getSample (c, i);
                        expect (same, "audio with 200 Hz Low Cut equals audio with it Off at " + juce::String (freq) + " Hz, oversampling " + juce::String (os));
                    }
                }
            }

        beginTest ("The processor compresses a bass-heavy signal less with Low Cut on, in every oversampling mode");
        for (int os : { 0, 1, 2 })
        {
            const float off = processorGr (os, 0.0f, 50.0, 0.25f);
            const float on = processorGr (os, 150.0f, 50.0, 0.25f);
            expectGreaterThan (off, 1.0f, "compressing with Low Cut off, oversampling " + juce::String (os));
            expectLessThan (on, off - 3.0f, "less gain reduction with Low Cut on, oversampling " + juce::String (os));
            const float mid = processorGr (os, 0.0f, 1000.0, 0.25f);
            expectWithinAbsoluteError (processorGr (os, 150.0f, 1000.0, 0.25f), mid, 0.3f, "1 kHz is unaffected, oversampling " + juce::String (os));
        }

        beginTest ("Switching Low Cut or its frequency while playing glides: no jump in gain reduction");
        {
            FET::Engine e;
            Measure::configure (e, 48000.0, 0, 1.0f);
            const float a = Measure::amp (-12.0f);   // the slowest attack still moves gain reduction by under 0.2 dB a sample
            float previous = 0.0f, worst = 0.0f;
            bool finite = true;
            const float settings[] = { 0.0f, 60.0f, 200.0f, 0.0f, 100.0f };
            for (int step = 0; step < 5; ++step)
            {
                e.setDetectorLowCutHz (settings[step]);
                for (int i = 0; i < 48000; ++i)
                {
                    float x = a * (float) std::sin (juce::MathConstants<double>::twoPi * 50.0 * (step * 48000 + i) / 48000.0);
                    float* ch[] = { &x };
                    e.process (ch, 1, 1);
                    if (step > 0)   // step 0 is the first second of settling from silence
                        worst = std::max (worst, std::abs (e.getGainReductionDb() - previous));
                    previous = e.getGainReductionDb();
                    finite = finite && std::isfinite (previous);
                }
            }
            expect (finite);
            expectLessThan (worst, 0.3f, "largest gain reduction change in one sample (dB); an unsmoothed jump would be several dB");
        }

        beginTest ("Automating Low Cut every block stays finite and never makes the audio louder");
        for (int os : { 0, 1, 2 })
        {
            SongsmithAnalogCompressorProcessor p;
            p.setColourEnabled (false);
            p.prepareToPlay (44100.0, 256);
            auto* os_ = p.apvts.getParameter (ID::oversampling);
            os_->setValueNotifyingHost (os_->convertTo0to1 ((float) os));
            auto* lc = p.apvts.getParameter (ID::detectorLowCut);
            juce::MidiBuffer m;
            bool ok = true;
            for (int k = 0; k < 120; ++k)
            {
                lc->setValueNotifyingHost (lc->convertTo0to1 ((float) (k % numLowCutSteps)));
                juce::AudioBuffer<float> b (2, 256);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                        b.setSample (c, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 70.0 * (k * 256 + i) / 44100.0));
                p.processBlock (b, m);
                ok = ok && std::isfinite (b.getMagnitude (0, 256)) && b.getMagnitude (0, 256) <= 0.75f;
            }
            expect (ok, "finite and bounded, oversampling " + juce::String (os));
        }

        beginTest ("Latency is the same with Low Cut on or off, in every oversampling mode");
        for (int os : { 0, 1, 2 })
        {
            int latency[2] = {};
            for (int pass = 0; pass < 2; ++pass)
            {
                SongsmithAnalogCompressorProcessor p;
                auto set = [&] (const char* id, float v) { auto* rp = p.apvts.getParameter (id); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
                set (ID::oversampling, (float) os);
                set (ID::detectorLowCut, pass == 0 ? 0.0f : 3.0f);
                p.prepareToPlay (48000.0, 512);
                p.updateLatencyNow();
                latency[pass] = p.getLatencySamples();
            }
            expectEquals (latency[1], latency[0], "oversampling " + juce::String (os));
        }

        beginTest ("Bypass stays bit-exact with Low Cut on");
        {
            SongsmithAnalogCompressorProcessor p;
            auto set = [&] (const char* id, float v) { auto* rp = p.apvts.getParameter (id); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
            set (ID::oversampling, 0.0f);
            set (ID::detectorLowCut, 3.0f);
            p.prepareToPlay (48000.0, 512);
            set (ID::bypass, 1.0f);
            juce::MidiBuffer m;
            juce::AudioBuffer<float> b (2, 512), ref (2, 512);
            for (int k = 0; k < 5; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        b.setSample (c, i, 0.5f * (float) std::sin (0.03 * (k * 512 + i)));
                ref.makeCopyOf (b);
                p.processBlock (b, m);
            }
            bool exact = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    exact = exact && b.getSample (c, i) == ref.getSample (c, i);
            expect (exact);
        }
    }
};

static TransferTests transferTests;
static DetectorLowCutTests detectorLowCutTests;
static TimingTests timingTests;
static EngineBehaviourTests engineBehaviourTests;
static ProcessorEngineTests processorEngineTests;
