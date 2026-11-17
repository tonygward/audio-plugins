// Songsmith Optical Leveller engine tests (OPT-01). Run through OpticalLevellerTests.exe (OpticalLevellerTests.cpp has main).
// Each test checks a behaviour written down in optical-leveller/docs/engine.md.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <random>
#include <vector>
#include "OpticalEngine.h"
#include "OptPresets.h"

using namespace SongsmithOptical;

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

    Params base (float peakReduction = 50.0f, bool limit = false, float gainDb = 0.0f)
    {
        Params p;
        p.peakReduction = peakReduction;
        p.limit = limit;
        p.gainDb = gainDb;
        return p;
    }

    // Reduction from a burst of `burstSeconds` at `levelDb`, then silence: how long, in ms, until `fraction` of it has gone.
    float releaseMs (double fs, float burstSeconds, float levelDb, float fraction, float peakReduction = 50.0f)
    {
        Engine e;
        e.prepare (fs);
        e.setParams (base (peakReduction));
        runConstant (e, levelDb, burstSeconds, fs);
        const float start = e.getGainReductionDb();
        std::vector<float> gr { start };
        runConstant (e, -120.0f, 30.0, fs, &gr);
        // Time until the reduction has fallen to (1 - fraction) of where it started.
        const float target = start * (1.0f - fraction);
        for (size_t i = 1; i < gr.size(); ++i)
            if (gr[i] <= target)
                return (float) ((double) i * 1000.0 / fs);
        return -1.0f;
    }
}

class OpticalEngineTests final : public juce::UnitTest
{
public:
    OpticalEngineTests() : juce::UnitTest ("Optical Leveller engine", "OpticalLeveller") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("The static curve: Peak Reduction sets the threshold, Compress is gentle and Limit is firm");
        {
            expectWithinAbsoluteError (thresholdDb (0.0f), -4.0f, 1.0e-5f);
            expectWithinAbsoluteError (thresholdDb (50.0f), -22.0f, 1.0e-5f);
            expectWithinAbsoluteError (thresholdDb (100.0f), -40.0f, 1.0e-5f);
            expectWithinAbsoluteError (thresholdDb (-5.0f), -4.0f, 1.0e-5f, "below the range is clamped");
            expectWithinAbsoluteError (thresholdDb (500.0f), -40.0f, 1.0e-5f, "above the range is clamped");

            expectEquals (staticReductionDb (-40.0f, 50.0f, false), 0.0f, "well below the threshold nothing is reduced");
            expectWithinAbsoluteError (staticReductionDb (-12.0f, 50.0f, false), 10.0f * (1.0f - 1.0f / compressRatio), 1.0e-4f, "above the knee: 3 to 1");
            expectWithinAbsoluteError (staticReductionDb (-12.0f, 50.0f, true), 10.0f * (1.0f - 1.0f / limitRatio), 1.0e-4f, "above the knee: 12 to 1");
            expect (staticReductionDb (-12.0f, 50.0f, true) > staticReductionDb (-12.0f, 50.0f, false), "Limit reduces more than Compress");
            expectWithinAbsoluteError (staticReductionDb (-22.0f, 50.0f, false), 0.5f * 0.5f * compressKneeDb * (1.0f - 1.0f / compressRatio) * 0.5f, 1.0e-3f, "at the threshold the knee gives a little");

            float previous = -1.0f;
            for (float level = -60.0f; level <= 0.0f; level += 0.5f)
            {
                const float r = staticReductionDb (level, 60.0f, false);
                expect (r >= previous - 1.0e-6f, "never falls as the level rises");
                previous = r;
            }
            for (float pr : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
            {
                float prev = -1.0f;
                for (float level = -60.0f; level <= 0.0f; level += 1.0f)
                {
                    expect (staticReductionDb (level, pr, true) >= prev - 1.0e-6f, "Limit never falls as the level rises");
                    prev = staticReductionDb (level, pr, true);
                }
            }
            expect (staticReductionDb (-20.0f, 80.0f, false) > staticReductionDb (-20.0f, 20.0f, false), "turning Peak Reduction up gives more reduction");
        }

        beginTest ("A steady level settles at the static reduction");
        for (bool limit : { false, true })
            for (float level : { -30.0f, -22.0f, -12.0f, -6.0f })
            {
                Engine e;
                e.prepare (fs);
                e.setParams (base (50.0f, limit));
                runConstant (e, level, 25.0, fs);
                expectWithinAbsoluteError (e.getGainReductionDb(), staticReductionDb (level, 50.0f, limit), 0.05f,
                                           juce::String (limit ? "Limit" : "Compress") + " at " + juce::String (level) + " dB");
            }

        beginTest ("With nothing to reduce the sound passes through unchanged, and Gain is applied exactly");
        {
            Engine e;
            e.prepare (fs);
            e.setParams (base (0.0f, false, 0.0f));
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> dist (-0.12f, 0.12f);   // always below -18 dBFS
            std::vector<float> in (48000), out;
            for (auto& s : in)
                s = dist (rng);
            out = in;
            float* ch[] = { out.data() };
            e.process (ch, 1, (int) out.size());
            expect (std::equal (in.begin(), in.end(), out.begin()), "bit-identical at Peak Reduction 0 and Gain 0");

            e.reset();
            e.setParams (base (0.0f, false, 6.0f));
            e.reset();
            out = in;
            float* ch2[] = { out.data() };
            e.process (ch2, 1, (int) out.size());
            for (size_t i = 0; i < in.size(); i += 997)
                expectWithinAbsoluteError (out[i], in[i] * amp (6.0f), 1.0e-5f);
        }

        beginTest ("Attack: about 10 ms, as published");
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            Engine e;
            e.prepare (rate);
            e.setParams (base (50.0f));
            std::vector<float> gr { 0.0f };
            runConstant (e, -6.0f, 0.5, rate, &gr);
            const float t63 = timeToFraction (gr, rate, 0.632f);
            logMessage ("MEASURED attack to 63 %: " + juce::String (t63, 1) + " ms at " + juce::String (rate) + " Hz");
            expect (t63 > 6.0f && t63 < 16.0f, "reaches 63 % in " + juce::String (t63) + " ms at " + juce::String (rate) + " Hz");
        }

        beginTest ("Release after a short signal: half is gone in about 60 ms, the rest follows more slowly");
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            const float half = releaseMs (rate, 0.3f, -12.0f, 0.5f);
            logMessage ("MEASURED short signal release: half " + juce::String (half, 1) + " ms, 80 % " + juce::String (releaseMs (rate, 0.3f, -12.0f, 0.8f), 0) + " ms at " + juce::String (rate) + " Hz");
            expect (half > 40.0f && half < 90.0f, "half gone after " + juce::String (half) + " ms at " + juce::String (rate) + " Hz");
            const float most = releaseMs (rate, 0.3f, -12.0f, 0.8f);
            expect (most > half * 1.5f, "the last part lets go more slowly (" + juce::String (most) + " ms to 80 %)");
            expect (most < 800.0f, "but a short signal is clear within a second");
        }

        beginTest ("Release depends on the programme: a long, hard signal lets go far more slowly than a short one");
        {
            const float shortBurst = releaseMs (fs, 0.3f, -6.0f, 0.8f), longBurst = releaseMs (fs, 20.0f, -6.0f, 0.8f);
            logMessage ("MEASURED 80 % release: 0.3 s signal " + juce::String (shortBurst, 0) + " ms, 20 s signal " + juce::String (longBurst, 0) + " ms; 50 %: " + juce::String (releaseMs (fs, 0.3f, -6.0f, 0.5f), 0) + " ms and " + juce::String (releaseMs (fs, 20.0f, -6.0f, 0.5f), 0) + " ms");
            expect (shortBurst > 0.0f && shortBurst < 600.0f, "a 0.3 second signal is 80 % clear in " + juce::String (shortBurst) + " ms");
            expect (longBurst > 2000.0f, "a 20 second signal takes " + juce::String (longBurst) + " ms to be 80 % clear");
            expect (longBurst > 3.0f * shortBurst, "the long signal takes much more than three times as long");
            expect (longBurst < 12000.0f, "and still lets go within the published few seconds");

            // A harder signal holds on for longer than a gentler one of the same length.
            const float gentle = releaseMs (fs, 10.0f, -22.0f, 0.8f), hard = releaseMs (fs, 10.0f, -6.0f, 0.8f);
            expect (hard > gentle, "the harder signal lets go more slowly than the gentler one");
        }

        beginTest ("The memory builds while the reduction is held and fades afterwards");
        {
            Engine e;
            e.prepare (fs);
            e.setParams (base (50.0f));
            expectEquals (e.getMemory(), 0.0f);
            runConstant (e, -6.0f, 0.2, fs);
            const float brief = e.getMemory();
            runConstant (e, -6.0f, 15.0, fs);
            const float held = e.getMemory();
            expect (held > 0.8f && held > 5.0f * brief, "held: " + juce::String (held) + ", brief: " + juce::String (brief));
            runConstant (e, -120.0f, 40.0, fs);
            expect (e.getMemory() < 0.1f * held, "the memory fades in silence");
        }

        beginTest ("Compress and Limit: the same signal is reduced more in Limit, and the reduction follows Peak Reduction");
        {
            Engine compress, limit;
            compress.prepare (fs);
            limit.prepare (fs);
            compress.setParams (base (50.0f, false));
            limit.setParams (base (50.0f, true));
            runConstant (compress, -10.0f, 10.0, fs);
            runConstant (limit, -10.0f, 10.0, fs);
            expect (limit.getGainReductionDb() > compress.getGainReductionDb() + 1.0f);

            float previous = -1.0f;
            for (float pr : { 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f })
            {
                Engine e;
                e.prepare (fs);
                e.setParams (base (pr));
                runConstant (e, -14.0f, 10.0, fs);
                expect (e.getGainReductionDb() >= previous - 1.0e-3f, "more Peak Reduction never gives less reduction");
                previous = e.getGainReductionDb();
            }
        }

        beginTest ("Stereo: one gain for both channels, so the image holds");
        {
            Engine e;
            e.prepare (fs);
            e.setParams (base (60.0f));
            const float loud = amp (-8.0f), quiet = amp (-40.0f);
            std::vector<float> l (96000), r (96000);
            for (size_t i = 0; i < l.size(); ++i)
            {
                l[i] = (i & 1) ? -loud : loud;
                r[i] = (i & 1) ? -quiet : quiet;
            }
            float* ch[] = { l.data(), r.data() };
            e.process (ch, 2, (int) l.size());
            const float gl = std::abs (l.back()) / loud, gr = std::abs (r.back()) / quiet;
            expectWithinAbsoluteError (gl, gr, 1.0e-4f, "left and right are turned down by the same amount");
            expect (gl < 0.7f, "and the loud side drove it");
        }

        beginTest ("The result does not depend on the block size");
        {
            std::mt19937 rng (11);
            std::uniform_real_distribution<float> dist (-0.6f, 0.6f);
            std::vector<float> in (48000 * 2);
            for (auto& s : in)
                s = dist (rng);

            Engine whole;
            whole.prepare (fs);
            whole.setParams (base (65.0f, false, 3.0f));
            auto reference = in;
            float* wholeCh[] = { reference.data() };
            whole.process (wholeCh, 1, (int) reference.size());

            for (int block : { 1, 7, 64, 480, 1000 })
            {
                Engine e;
                e.prepare (fs);
                e.setParams (base (65.0f, false, 3.0f));
                auto out = in;
                for (size_t pos = 0; pos < out.size(); pos += (size_t) block)
                {
                    float* ch[] = { out.data() + pos };
                    e.process (ch, 1, (int) std::min<size_t> ((size_t) block, out.size() - pos));
                }
                expect (out == reference, "bit-identical at a block size of " + juce::String (block));
            }
        }

        beginTest ("Silence, extreme and non-finite input leave the engine finite and quiet");
        {
            Engine e;
            e.prepare (fs);
            e.setParams (base (90.0f, true, 20.0f));
            std::vector<float> x (4800, 0.0f);
            float* ch[] = { x.data() };
            e.process (ch, 1, (int) x.size());
            expect (std::all_of (x.begin(), x.end(), [] (float s) { return s == 0.0f; }), "silence stays silent");

            std::fill (x.begin(), x.end(), 1.0e6f);
            e.process (ch, 1, (int) x.size());
            expect (std::isfinite (e.getGainReductionDb()), "a huge input keeps the reduction finite");

            x.assign (4800, 0.3f);
            x[100] = std::numeric_limits<float>::infinity();
            x[200] = std::numeric_limits<float>::quiet_NaN();
            e.process (ch, 1, (int) x.size());
            expect (std::isfinite (e.getGainReductionDb()), "infinity and NaN do not poison the detector");

            x.assign (4800, 0.3f);
            e.process (ch, 1, (int) x.size());
            expect (std::all_of (x.begin(), x.end(), [] (float s) { return std::isfinite (s); }), "and later sound is clean");

            runConstant (e, -120.0f, 80.0, fs);
            expect (e.getGainReductionDb() < 0.01f, "reduction falls to nothing in silence");
        }

        beginTest ("Out-of-range settings are clamped");
        {
            Engine e;
            e.prepare (fs);
            Params p;
            p.peakReduction = 1000.0f;
            p.gainDb = 500.0f;
            e.setParams (p);
            expectEquals (e.getParams().peakReduction, maxPeakReduction);
            expectEquals (e.getParams().gainDb, maxGainDb);
            p.peakReduction = -1000.0f;
            p.gainDb = -500.0f;
            e.setParams (p);
            expectEquals (e.getParams().peakReduction, minPeakReduction);
            expectEquals (e.getParams().gainDb, minGainDb);
        }

        beginTest ("Every factory starting point is level-matched at -18 dBFS");
        for (const auto& preset : Opt::Presets::factory())
        {
            Engine e;
            e.prepare (fs);
            e.setParams (base (preset.peakReduction, preset.limit, preset.gainDb));
            e.reset();
            e.setParams (base (preset.peakReduction, preset.limit, preset.gainDb));
            const float a = amp (referenceLevelDb);
            float last = 0.0f;
            for (int i = 0; i < (int) (fs * 25.0); ++i)
            {
                float x = (i & 1) ? -a : a;
                float* ch[] = { &x };
                e.process (ch, 1, 1);
                last = std::abs (x);
            }
            const float changeDb = 20.0f * std::log10 (last / a);
            expect (std::abs (changeDb) < 1.0f, juce::String (preset.name) + " changes the level by " + juce::String (changeDb) + " dB");
            expect (std::abs (preset.gainDb - staticReductionDb (referenceLevelDb, preset.peakReduction, preset.limit)) < 1.0f,
                    juce::String (preset.name) + ": Gain matches the reduction it adds at the reference level");
        }
    }
};

static OpticalEngineTests opticalEngineTests;
