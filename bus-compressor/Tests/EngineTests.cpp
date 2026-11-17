// Songsmith Bus Compressor engine tests (BUS-01). Run through BusCompressorTests.exe (BusCompressorTests.cpp has main).
// Each test checks a behaviour written down in bus-compressor/docs/engine.md.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <random>
#include <vector>
#include "BusEngine.h"
#include "BusPresets.h"

using namespace SongsmithBus;

namespace
{
    float amp (float db) { return std::pow (10.0f, db / 20.0f); }

    // A constant-envelope test signal: samples alternate +a, -a, so the detector sees a true level step at any sample rate.
    void runConstant (Engine& e, float levelDb, double seconds, double fs, std::vector<float>* grOut = nullptr)
    {
        const float a = amp (levelDb);
        const int samples = (int) (seconds * fs);
        for (int i = 0; i < samples; ++i)
        {
            float x = (i & 1) ? -a : a;
            float* ch[] = { &x };
            e.process (ch, 1, 1);
            if (grOut != nullptr)
                grOut->push_back (e.getGainReductionDb());
        }
    }

    // Time in ms for the gain reduction to move `fraction` of the way from its first to its last value, interpolated.
    float timeToFraction (const std::vector<float>& gr, double fs, float fraction)
    {
        const float start = gr.front(), end = gr.back();
        const float target = start + (end - start) * fraction;
        const bool rising = end > start;
        for (size_t i = 1; i < gr.size(); ++i)
        {
            if (rising ? gr[i] >= target : gr[i] <= target)
            {
                const float f = (target - gr[i - 1]) / (gr[i] - gr[i - 1]);
                return (float) (((double) i - 1.0 + (double) f) * 1000.0 / fs);
            }
        }
        return -1.0f;
    }

    Params base (float dial = 0.0f, int ratio = 1, int attack = 3, int release = 1, float makeup = 0.0f)
    {
        Params p;
        p.thresholdDial = dial;
        p.ratioIndex = ratio;
        p.attackIndex = attack;
        p.releaseIndex = release;
        p.makeupDb = makeup;
        return p;
    }

    // Starts a prepared engine with settled parameters.
    void start (Engine& e, double fs, const Params& p)
    {
        e.prepare (fs);
        e.setParams (p);
        e.reset();
        e.setParams (p);
    }

    // The time, in ms, for the reduction after a loud burst (then silence) to fall by `fraction` of where it started.
    float releaseMs (double fs, const Params& p, float burstSeconds, float levelDb, float fraction)
    {
        Engine e;
        start (e, fs, p);
        runConstant (e, levelDb, burstSeconds, fs);
        std::vector<float> gr { e.getGainReductionDb() };
        runConstant (e, -120.0f, 12.0, fs, &gr);
        const float target = gr.front() * (1.0f - fraction);
        for (size_t i = 1; i < gr.size(); ++i)
            if (gr[i] <= target)
                return (float) ((double) i * 1000.0 / fs);
        return -1.0f;
    }
}

class BusEngineTests final : public juce::UnitTest
{
public:
    BusEngineTests() : juce::UnitTest ("Bus Compressor engine", "BusCompressor") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("The published steps: three ratios, six attack times, four release times and Auto");
        {
            expectEquals ((int) ratios.size(), 3);
            expectEquals (ratios[0], 2.0f);
            expectEquals (ratios[1], 4.0f);
            expectEquals (ratios[2], 10.0f);
            const float attacks[] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
            for (size_t i = 0; i < 6; ++i)
                expectEquals (attackMilliseconds[i], attacks[i]);
            const float releases[] = { 0.1f, 0.3f, 0.6f, 1.2f };
            for (size_t i = 0; i < 4; ++i)
                expectEquals (releaseSeconds[i], releases[i]);
            expectEquals (autoReleaseIndex, 4, "Auto is the fifth release step");
            expectEquals (minThresholdDial, -15.0f);
            expectEquals (maxThresholdDial, 10.0f);
            expectEquals (maxMakeupDb, 15.0f);
        }

        beginTest ("The static curve: the dial sets the threshold, the ratio sets the slope, and the knee is smooth");
        {
            expectWithinAbsoluteError (thresholdDbfs (0.0f), -18.0f, 1.0e-5f, "0 on the dial is the suite's reference level");
            expectWithinAbsoluteError (thresholdDbfs (10.0f), -8.0f, 1.0e-5f);
            expectWithinAbsoluteError (thresholdDbfs (-15.0f), -33.0f, 1.0e-5f);
            expectWithinAbsoluteError (thresholdDbfs (99.0f), -8.0f, 1.0e-5f, "above the dial is clamped");
            expectWithinAbsoluteError (thresholdDbfs (-99.0f), -33.0f, 1.0e-5f, "below the dial is clamped");

            expectEquals (staticReductionDb (-60.0f, 0.0f, 1), 0.0f, "well below the threshold nothing is reduced");
            for (int r = 0; r < 3; ++r)
                expectWithinAbsoluteError (staticReductionDb (-8.0f, 0.0f, r), 10.0f * (1.0f - 1.0f / ratios[(size_t) r]), 1.0e-4f,
                                           "above the knee the slope is " + juce::String (ratios[(size_t) r]) + " to 1");
            expect (staticReductionDb (-8.0f, 0.0f, 2) > staticReductionDb (-8.0f, 0.0f, 1)
                    && staticReductionDb (-8.0f, 0.0f, 1) > staticReductionDb (-8.0f, 0.0f, 0), "a higher ratio reduces more");
            expectWithinAbsoluteError (staticReductionDb (-18.0f, 0.0f, 1), 0.75f * kneeDb / 8.0f, 1.0e-4f, "at the threshold the knee gives a little");

            // Continuous at both ends of the knee, and never falls as the level rises.
            for (int r = 0; r < 3; ++r)
            {
                const float lo = -18.0f - 0.5f * kneeDb, hi = -18.0f + 0.5f * kneeDb;
                expectWithinAbsoluteError (staticReductionDb (lo - 1.0e-3f, 0.0f, r), staticReductionDb (lo + 1.0e-3f, 0.0f, r), 1.0e-3f, "no step at the foot of the knee");
                expectWithinAbsoluteError (staticReductionDb (hi - 1.0e-3f, 0.0f, r), staticReductionDb (hi + 1.0e-3f, 0.0f, r), 1.0e-2f, "no step at the top of the knee");
                float previous = -1.0f;
                for (float level = -60.0f; level <= 0.0f; level += 0.25f)
                {
                    const float reduction = staticReductionDb (level, 0.0f, r);
                    expect (reduction >= previous - 1.0e-6f, "never falls as the level rises");
                    previous = reduction;
                }
            }
            expectEquals (staticReductionDb (-8.0f, 0.0f, 99), staticReductionDb (-8.0f, 0.0f, 2), "a step number past the end is clamped");
        }

        beginTest ("A steady signal settles on the static curve, at every ratio, and Threshold moves it");
        for (int ratio = 0; ratio < 3; ++ratio)
            for (float dial : { -10.0f, 0.0f, 6.0f })
            {
                Engine e;
                start (e, fs, base (dial, ratio, 3, 1));
                runConstant (e, -8.0f, 3.0, fs);
                expectWithinAbsoluteError (e.getGainReductionDb(), staticReductionDb (-8.0f, dial, ratio), 0.02f,
                                           "ratio " + juce::String (ratio) + ", dial " + juce::String (dial));
            }

        beginTest ("Attack: the reduction covers 63 % of a sudden step in about the attack time, at any sample rate");
        for (double rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int attack : { 3, 4, 5 })
            {
                Engine e;
                start (e, rate, base (0.0f, 1, attack, 3));
                runConstant (e, -60.0f, 0.2, rate);
                std::vector<float> gr { e.getGainReductionDb() };
                runConstant (e, -8.0f, 0.4, rate, &gr);
                const float expected = attackMilliseconds[(size_t) attack];
                const float t = timeToFraction (gr, rate, 0.632f);
                expect (t > expected * 0.85f && t < expected * 1.15f,
                        juce::String (expected) + " ms attack at " + juce::String (rate) + " Hz reached 63 % in " + juce::String (t) + " ms");
            }

        beginTest ("The fastest attacks are fast: 0.1 ms, 0.3 ms and 1 ms");
        for (int attack : { 0, 1, 2 })
        {
            Engine e;
            start (e, 96000.0, base (0.0f, 1, attack, 3));
            std::vector<float> gr { 0.0f };
            runConstant (e, -8.0f, 0.1, 96000.0, &gr);
            const float expected = attackMilliseconds[(size_t) attack];
            const float t = timeToFraction (gr, 96000.0, 0.632f);
            expect (t > expected * 0.8f && t < expected * 1.3f, juce::String (expected) + " ms attack reached 63 % in " + juce::String (t) + " ms");
        }

        beginTest ("Release: the reduction lets go over about the release time, for each fixed release");
        for (double rate : { 44100.0, 48000.0, 96000.0 })
            for (int release = 0; release < 4; ++release)
            {
                const float expected = releaseSeconds[(size_t) release] * 1000.0f;
                const float t = releaseMs (rate, base (0.0f, 1, 3, release), 3.0f, -8.0f, 0.632f);
                expect (t > expected * 0.9f && t < expected * 1.3f,
                        juce::String (expected) + " ms release at " + juce::String (rate) + " Hz fell by 63 % in " + juce::String (t) + " ms");
            }

        beginTest ("Auto release: quick after a short hit, slow after a long hard push, and always between the two limits");
        {
            const auto autoSettings = base (0.0f, 1, 3, autoReleaseIndex);
            const float shortHit = releaseMs (fs, autoSettings, 0.05f, -8.0f, 0.632f);
            const float longPush = releaseMs (fs, autoSettings, 6.0f, -8.0f, 0.632f);
            expect (shortHit > 0.0f && shortHit < 250.0f, "a short hit lets go in " + juce::String (shortHit) + " ms");
            expect (longPush > 600.0f && longPush < 1500.0f, "a long push lets go in " + juce::String (longPush) + " ms");
            expect (longPush > 3.0f * shortHit, "the long push is much slower");

            Engine e;
            start (e, fs, autoSettings);
            expectWithinAbsoluteError (e.getReleaseSeconds(), autoFastSeconds, 1.0e-6f, "at rest Auto is as quick as it gets");
            runConstant (e, -8.0f, 12.0, fs);
            expectWithinAbsoluteError (e.getReleaseSeconds(), autoSlowSeconds, 0.02f, "after a long hard push Auto is as slow as it gets");
            Engine f;
            start (f, fs, base (0.0f, 1, 3, 2));
            expectEquals (f.getReleaseSeconds(), 0.6f, "a fixed release reports its own time");
        }

        beginTest ("Stereo linking: one gain for both channels, driven by the louder one, so the image does not move");
        {
            Engine e;
            start (e, fs, base (0.0f, 1, 3, 1));
            const float left = amp (-8.0f), right = amp (-40.0f);
            float l = 0.0f, r = 0.0f;
            for (int i = 0; i < (int) (fs * 2.0); ++i)
            {
                l = (i & 1) ? -left : left;
                r = (i & 1) ? -right : right;
                float* ch[] = { &l, &r };
                e.process (ch, 2, 1);
            }
            expectWithinAbsoluteError (std::abs (r) / std::abs (l), right / left, 1.0e-6f, "the left and right levels keep their balance");
            expectWithinAbsoluteError (20.0f * std::log10 (std::abs (l) / left), -e.getGainReductionDb(), 1.0e-3f, "both channels are turned down by the same amount");
            expect (e.getGainReductionDb() > 4.0f, "and it is the loud channel that sets it");

            // A quiet right channel on its own is not reduced at all.
            Engine q;
            start (q, fs, base (0.0f, 1, 3, 1));
            runConstant (q, -40.0f, 1.0, fs);
            expectEquals (q.getGainReductionDb(), 0.0f);
        }

        beginTest ("Make-up Gain adds exactly its amount, below the threshold nothing else changes, and parameters are clamped");
        {
            Engine e;
            start (e, fs, base (10.0f, 1, 3, 1, 0.0f));
            const float a = amp (-50.0f);
            bool identical = true;
            for (int i = 0; i < 4800; ++i)
            {
                float x = (i & 1) ? -a : a, y = x;
                float* ch[] = { &y };
                e.process (ch, 1, 1);
                identical = identical && y == x;
            }
            expect (identical, "below the threshold with no Make-up Gain the sound is untouched, bit for bit");

            Engine g;
            start (g, fs, base (10.0f, 1, 3, 1, 6.0f));
            float last = 0.0f;
            for (int i = 0; i < 4800; ++i)
            {
                float x = (i & 1) ? -a : a;
                float* ch[] = { &x };
                g.process (ch, 1, 1);
                last = std::abs (x);
            }
            expectWithinAbsoluteError (20.0f * std::log10 (last / a), 6.0f, 0.01f, "6 dB of Make-up Gain");

            Engine c;
            c.prepare (fs);
            c.setParams (base (99.0f, 99, 99, 99, 99.0f));
            expectEquals (c.getParams().thresholdDial, maxThresholdDial);
            expectEquals (c.getParams().ratioIndex, 2);
            expectEquals (c.getParams().attackIndex, 5);
            expectEquals (c.getParams().releaseIndex, 4);
            expectEquals (c.getParams().makeupDb, maxMakeupDb);
            c.setParams (base (-99.0f, -3, -3, -3, -99.0f));
            expectEquals (c.getParams().thresholdDial, minThresholdDial);
            expectEquals (c.getParams().ratioIndex, 0);
            expectEquals (c.getParams().attackIndex, 0);
            expectEquals (c.getParams().releaseIndex, 0);
            expectEquals (c.getParams().makeupDb, minMakeupDb);
        }

        beginTest ("Changing Make-up Gain while sound plays moves smoothly, without a click");
        {
            Engine e;
            start (e, fs, base (10.0f, 1, 3, 1, 0.0f));
            const float a = amp (-30.0f);
            float previous = a, worst = 0.0f;
            for (int i = 0; i < 9600; ++i)
            {
                if (i == 2000)
                    e.setParams (base (10.0f, 1, 3, 1, 15.0f));
                float x = a;
                float* ch[] = { &x };
                e.process (ch, 1, 1);
                worst = std::max (worst, std::abs (x / previous - 1.0f));
                previous = x;
            }
            expect (worst < 0.01f, "the biggest step in gain is " + juce::String (worst * 100.0f) + " %");
            expectWithinAbsoluteError (20.0f * std::log10 (previous / a), 15.0f, 0.01f, "and it arrives");
        }

        beginTest ("The result does not depend on the host's block size");
        {
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> dist (-0.7f, 0.7f);
            std::vector<float> signal ((size_t) fs);
            for (auto& s : signal)
                s = dist (rng);

            std::vector<float> reference;
            for (int block : { 1, 7, 64, 480, 4096 })
            {
                Engine e;
                start (e, fs, base (-5.0f, 1, 3, autoReleaseIndex, 2.0f));
                std::vector<float> out = signal;
                for (size_t pos = 0; pos < out.size(); pos += (size_t) block)
                {
                    float* ch[] = { out.data() + pos };
                    e.process (ch, 1, (int) std::min ((size_t) block, out.size() - pos));
                }
                if (reference.empty())
                    reference = out;
                else
                    expect (out == reference, "identical at a block size of " + juce::String (block));
            }
        }

        beginTest ("Silence, damaged input and every setting stay finite, and nothing lingers in silence");
        {
            std::mt19937 rng (11);
            std::uniform_real_distribution<float> dist (-1.5f, 1.5f);
            bool finite = true;
            for (int ratio = 0; ratio < 3; ++ratio)
                for (int attack = 0; attack < 6; ++attack)
                    for (int release = 0; release < 5; ++release)
                    {
                        Engine e;
                        start (e, fs, base (-15.0f, ratio, attack, release, 15.0f));
                        for (int i = 0; i < 6000; ++i)
                        {
                            float x = dist (rng);
                            if (i % 997 == 0) x = std::numeric_limits<float>::quiet_NaN();
                            if (i % 1999 == 0) x = std::numeric_limits<float>::infinity();
                            float y = x;
                            float* ch[] = { &y };
                            e.process (ch, 1, 1);
                            finite = finite && (std::isfinite (y) || ! std::isfinite (x)) && std::isfinite (e.getGainReductionDb());
                        }
                        float quiet = 0.0f;
                        for (int i = 0; i < 6000; ++i)
                        {
                            float x = 0.0f;
                            float* ch[] = { &x };
                            e.process (ch, 1, 1);
                            quiet = std::max (quiet, std::abs (x));
                        }
                        finite = finite && quiet == 0.0f;
                    }
            expect (finite, "every combination stays finite and goes silent");

            Engine e;
            start (e, fs, base (0.0f, 1, 3, 0));
            runConstant (e, -8.0f, 0.5, fs);
            runConstant (e, -150.0f, 4.0, fs);
            expectEquals (e.getGainReductionDb(), 0.0f, "the reduction lets go completely");
            expect (e.getAutoDepthDb() < 0.3f, "and Auto's memory of the push has faded: " + juce::String (e.getAutoDepthDb()) + " dB");
        }

        beginTest ("The meter reading is the deepest reduction since the last read");
        {
            Engine e;
            start (e, fs, base (0.0f, 1, 3, 1));
            runConstant (e, -8.0f, 0.5, fs);
            const float deepest = e.readMaxReductionDb();
            expect (deepest > 4.0f, "reads the deepest reduction: " + juce::String (deepest));
            runConstant (e, -8.0f, 0.01, fs);
            expectWithinAbsoluteError (e.readMaxReductionDb(), e.getGainReductionDb(), 0.05f, "then reads what is happening now");
        }

        beginTest ("Every factory starting point is level-matched at a peak level of -12 dBFS");
        for (const auto& preset : BusComp::Presets::factory())
        {
            const auto p = base (preset.threshold, preset.ratio, preset.attack, preset.release, preset.makeupDb);
            Engine e;
            start (e, fs, p);
            const float a = amp (referenceLevelDb);
            float last = 0.0f;
            for (int i = 0; i < (int) (fs * 8.0); ++i)
            {
                float x = (i & 1) ? -a : a;
                float* ch[] = { &x };
                e.process (ch, 1, 1);
                last = std::abs (x);
            }
            const float changeDb = 20.0f * std::log10 (last / a);
            expect (std::abs (changeDb) < 1.0f, juce::String (preset.name) + " changes the level by " + juce::String (changeDb) + " dB");
            expect (std::abs (preset.makeupDb - staticReductionDb (referenceLevelDb, preset.threshold, preset.ratio)) < 1.0f,
                    juce::String (preset.name) + ": Make-up Gain matches the reduction it adds at the reference level");
            expect (staticReductionDb (referenceLevelDb, preset.threshold, preset.ratio) > 0.5f, juce::String (preset.name) + " does something at the reference level");
            expect (staticReductionDb (referenceLevelDb, preset.threshold, preset.ratio) < 8.0f, juce::String (preset.name) + " is not extreme");
        }
    }
};

static BusEngineTests busEngineTests;
