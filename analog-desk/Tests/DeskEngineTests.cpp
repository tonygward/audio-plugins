// Songsmith Analog Desk engine tests (DESK-01). Each test checks a behaviour written down in analog-desk/docs/engine.md.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <random>
#include <vector>
#include "DeskEngine.h"

using namespace SongsmithDesk;

namespace
{
    constexpr double fs = 48000.0;
    const double pi = juce::MathConstants<double>::pi;

    float amp (double db) { return (float) std::pow (10.0, db / 20.0); }

    std::unique_ptr<Engine> makeEngine (const Params& p)
    {
        auto e = std::make_unique<Engine> ();
        e->prepare (fs);
        e->setParams (p);
        e->reset();
        return e;
    }

    // Runs a mono signal through the engine in blocks and returns what comes out.
    std::vector<float> run (Engine& e, std::vector<float> in, int block = 480)
    {
        for (size_t start = 0; start < in.size(); start += (size_t) block)
        {
            const int n = (int) std::min<size_t> ((size_t) block, in.size() - start);
            float* ch[] = { in.data() + start };
            e.process (ch, 1, n);
        }
        return in;
    }

    std::vector<float> sine (double hz, double levelDb, double seconds)
    {
        std::vector<float> v ((size_t) (seconds * fs));
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = amp (levelDb) * (float) std::sin (2.0 * pi * hz * (double) i / fs);
        return v;
    }

    double rms (const std::vector<float>& v, size_t from, size_t to)
    {
        double sum = 0.0;
        for (size_t i = from; i < to; ++i) sum += (double) v[i] * (double) v[i];
        return std::sqrt (sum / (double) (to - from));
    }

    // The gain in dB the strip gives a steady sine, measured on its second half.
    double measuredGainDb (const Params& p, double hz, double levelDb = -30.0, double seconds = 1.0)
    {
        auto e = makeEngine (p);
        const auto in = sine (hz, levelDb, seconds);
        const auto out = run (*e, in);
        return 20.0 * std::log10 (rms (out, out.size() / 2, out.size()) / rms (in, in.size() / 2, in.size()));
    }

    // The analytic answers for the filters, with the bilinear transform's own frequency warping.
    double highPass3Db (double hz, double fc) { return -10.0 * std::log10 (1.0 + std::pow (std::tan (pi * fc / fs) / std::tan (pi * hz / fs), 6.0)); }
    double lowPass2Db (double hz, double fc)  { return -10.0 * std::log10 (1.0 + std::pow (std::tan (pi * hz / fs) / std::tan (pi * fc / fs), 4.0)); }

    // Compressor tests use a constant-envelope signal (samples alternate +a and -a), so the detector sees a true level step.
    void runConstant (Engine& e, double levelDb, double seconds, std::vector<float>* reduction = nullptr, int channels = 1)
    {
        const float a = amp (levelDb);
        const int samples = (int) (seconds * fs);
        for (int i = 0; i < samples; ++i)
        {
            float l = (i & 1) ? -a : a, r = l;
            float* ch[] = { &l, &r };
            e.process (ch, channels, 1);
            if (reduction != nullptr)
                reduction->push_back (e.getGainReductionDb());
        }
    }

    double timeToFractionMs (const std::vector<float>& values, float fraction)
    {
        const float start = values.front(), end = values.back(), target = start + (end - start) * fraction;
        const bool rising = end > start;
        for (size_t i = 1; i < values.size(); ++i)
            if (rising ? values[i] >= target : values[i] <= target)
                return (double) i / fs * 1000.0;
        return -1.0;
    }

    // The size of one frequency in a signal (a Goertzel filter), in dB relative to 1.
    double binDb (const std::vector<float>& v, double hz)
    {
        const double w = 2.0 * pi * hz / fs, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (float x : v)
        {
            const double s = (double) x + c * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
        return 10.0 * std::log10 (std::max (1.0e-30, power)) - 20.0 * std::log10 ((double) v.size() * 0.5);
    }
}

class DeskEngineTests final : public juce::UnitTest
{
public:
    DeskEngineTests() : juce::UnitTest ("Songsmith Analog Desk engine (DESK-01)", "AnalogDesk") {}

    void runTest() override
    {
        beginTest ("Identity: a new strip leaves the sound exactly as it is, and each stage that is switched off does nothing");
        {
            std::mt19937 rng (3);
            std::uniform_real_distribution<float> noise (-0.8f, 0.8f);
            std::vector<float> in (20000);
            for (auto& x : in) x = noise (rng);
            auto e = makeEngine ({});
            const auto out = run (*e, in);
            float worst = 0.0f;
            for (size_t i = 0; i < in.size(); ++i) worst = std::max (worst, std::abs (out[i] - in[i]));
            expectLessThan (worst, 1.0e-6f, "default settings are transparent");

            for (double hz : { 40.0, 500.0, 5000.0, 15000.0 })
                expectWithinAbsoluteError (e->responseDb (hz), 0.0, 1.0e-9, "the linear response is flat at " + juce::String (hz) + " Hz");

            // Trims that cancel, with no drive, leave the sound as it is.
            Params trims;
            trims.inputTrimDb = 6.0f;
            trims.outputTrimDb = -6.0f;
            auto t = makeEngine (trims);
            const auto trimmed = run (*t, in);
            worst = 0.0f;
            for (size_t i = 0; i < in.size(); ++i) worst = std::max (worst, std::abs (trimmed[i] - in[i]));
            expectLessThan (worst, 2.0e-4f, "+6 dB in and -6 dB out cancel");

            // The input and output trims are exactly what they say.
            Params trim;
            trim.inputTrimDb = 6.0f;
            expectWithinAbsoluteError (measuredGainDb (trim, 1000.0), 6.0, 0.05, "Input Trim");
            trim.inputTrimDb = 0.0f;
            trim.outputTrimDb = -9.0f;
            expectWithinAbsoluteError (measuredGainDb (trim, 1000.0), -9.0, 0.05, "Output Trim");
        }

        beginTest ("Filters: an 18 dB per octave high-pass and a 12 dB per octave low-pass, -3 dB at their frequency, off when set to off");
        {
            Params p;
            p.highPassHz = 100.0f;
            auto e = makeEngine (p);
            expectWithinAbsoluteError (e->responseDb (100.0), -3.01, 0.05, "the high-pass is 3 dB down at its frequency");
            for (double hz : { 25.0, 50.0, 70.0, 100.0, 200.0, 1000.0 })
                expectWithinAbsoluteError (e->responseDb (hz), highPass3Db (hz, 100.0), 0.05, "high-pass at " + juce::String (hz) + " Hz");
            expectWithinAbsoluteError (e->responseDb (50.0) - e->responseDb (25.0), 18.0, 0.6, "18 dB per octave in the stop band");
            expectWithinAbsoluteError (e->responseDb (5000.0), 0.0, 0.01, "the pass band is flat");

            Params l;
            l.lowPassHz = 4000.0f;
            auto le = makeEngine (l);
            expectWithinAbsoluteError (le->responseDb (4000.0), -3.01, 0.05, "the low-pass is 3 dB down at its frequency");
            for (double hz : { 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 })
                expectWithinAbsoluteError (le->responseDb (hz), lowPass2Db (hz, 4000.0), 0.05, "low-pass at " + juce::String (hz) + " Hz");
            expectWithinAbsoluteError (le->responseDb (8000.0) - le->responseDb (4000.0), lowPass2Db (8000.0, 4000.0) - lowPass2Db (4000.0, 4000.0), 0.05, "about 12 dB per octave (a little more near the top of the range, where the digital filter bends)");
            expectLessThan (le->responseDb (8000.0) - le->responseDb (4000.0), -9.0, "at least about 9 dB lower an octave up");

            // What comes out agrees with the response.
            for (double hz : { 50.0, 100.0, 200.0 })
                expectWithinAbsoluteError (measuredGainDb (p, hz), e->responseDb (hz), 0.1, "high-pass through the audio path at " + juce::String (hz) + " Hz");
            for (double hz : { 2000.0, 4000.0, 9000.0 })
                expectWithinAbsoluteError (measuredGainDb (l, hz), le->responseDb (hz), 0.1, "low-pass through the audio path at " + juce::String (hz) + " Hz");

            // Off is exactly off, and the ends of the ranges hold.
            auto off = makeEngine ({});
            expectEquals (off->responseDb (20.0), 0.0);
            Params lo;
            lo.highPassHz = 1.0f;
            expectEquals (makeEngine (lo)->getParams().highPassHz, minHighPassHz, "below the range, the high-pass sits at the bottom of its range");
            lo.highPassHz = 5000.0f;
            expectEquals (makeEngine (lo)->getParams().highPassHz, maxHighPassHz, "and above it at the top");
            Params hi;
            hi.lowPassHz = 100000.0f;
            expectEquals (makeEngine (hi)->getParams().lowPassHz, maxLowPassHz);
        }

        beginTest ("Equaliser: four bands, each cuts or boosts by its own amount at its own frequency, with sweepable mids and a shelf or bell on the ends");
        {
            struct Band { const char* name; float Params::*hz; float Params::*db; double lowHz, highHz; };
            const Band bands[] = { { "LF", &Params::lfHz, &Params::lfDb, minLfHz, maxLfHz }, { "LMF", &Params::lmfHz, &Params::lmfDb, minLmfHz, maxLmfHz },
                                   { "HMF", &Params::hmfHz, &Params::hmfDb, minHmfHz, maxHmfHz }, { "HF", &Params::hfHz, &Params::hfDb, minHfHz, maxHfHz } };
            for (auto& b : bands)
                for (double sweepHz : { b.lowHz, std::sqrt (b.lowHz * b.highHz), b.highHz })
                    for (float gain : { -15.0f, -6.0f, 6.0f, 15.0f })
                    {
                        Params p;
                        p.*b.hz = (float) sweepHz;
                        p.*b.db = gain;
                        // A bell reads its full gain at its centre. (The ends are shelves by default, so they are set to bells here.)
                        p.lfBell = p.hfBell = true;
                        auto e = makeEngine (p);
                        expectWithinAbsoluteError (e->responseDb (sweepHz), (double) gain, 0.1,
                                                   juce::String (b.name) + " at " + juce::String (sweepHz, 0) + " Hz, " + juce::String (gain) + " dB");
                    }

            // Beyond the ends of their ranges, bands and amounts hold at the end.
            Params wide;
            wide.lfDb = 40.0f; wide.lmfHz = 5.0f; wide.hfHz = 90000.0f; wide.hmfQ = 100.0f;
            const auto& clamped = makeEngine (wide)->getParams();
            expectEquals (clamped.lfDb, eqRangeDb, "an amount beyond the range holds at the range");
            expectEquals (clamped.lmfHz, minLmfHz);
            expectEquals (clamped.hfHz, maxHfHz);
            expectEquals (clamped.hmfQ, maxQ);

            // Shelves reach their amount well beyond their frequency and leave the far side alone.
            Params lf;
            lf.lfHz = 100.0f; lf.lfDb = 10.0f;
            auto lfe = makeEngine (lf);
            expectWithinAbsoluteError (lfe->responseDb (20.0), 10.0, 0.6, "a low shelf reaches its amount below its frequency");
            expectWithinAbsoluteError (lfe->responseDb (100.0), 5.0, 1.0, "and is about half-way at its frequency");
            expectWithinAbsoluteError (lfe->responseDb (2000.0), 0.0, 0.1, "and leaves the high end alone");
            Params hf;
            hf.hfHz = 8000.0f; hf.hfDb = -10.0f;
            auto hfe = makeEngine (hf);
            expectWithinAbsoluteError (hfe->responseDb (22000.0), -10.0, 0.9, "a high shelf reaches its amount above its frequency");
            expectWithinAbsoluteError (hfe->responseDb (500.0), 0.0, 0.1, "and leaves the low end alone");
            lf.lfBell = true;
            expect (makeEngine (lf)->responseDb (20.0) < 3.0, "the same low band as a bell does not lift the bottom octaves");

            // A narrower Q is narrower.
            Params wideQ, narrowQ;
            wideQ.lmfHz = narrowQ.lmfHz = 1000.0f; wideQ.lmfDb = narrowQ.lmfDb = 10.0f; wideQ.lmfQ = 0.5f; narrowQ.lmfQ = 2.5f;
            expect (makeEngine (wideQ)->responseDb (1500.0) > makeEngine (narrowQ)->responseDb (1500.0) + 3.0, "Q 2.5 is much narrower than Q 0.5");
            expectWithinAbsoluteError (makeEngine (narrowQ)->responseDb (1000.0), 10.0, 0.1, "at the same amount");

            // The bands add, and what comes out agrees with the response.
            Params all;
            all.lfHz = 80.0f; all.lfDb = 4.0f; all.lmfHz = 400.0f; all.lmfDb = -5.0f; all.hmfHz = 3000.0f; all.hmfDb = 6.0f; all.hfHz = 10000.0f; all.hfDb = 3.0f;
            auto ae = makeEngine (all);
            for (double hz : { 60.0, 200.0, 400.0, 1200.0, 3000.0, 9000.0 })
                expectWithinAbsoluteError (measuredGainDb (all, hz), ae->responseDb (hz), 0.1, "all four bands through the audio path at " + juce::String (hz) + " Hz");
        }

        beginTest ("Independent bypass: the EQ off leaves the dynamics working, and the dynamics off leaves the EQ working");
        {
            Params p;
            p.lmfHz = 1000.0f; p.lmfDb = 12.0f;
            p.compThresholdDb = -30.0f; p.compRatio = 10.0f;
            p.eqOn = false;
            auto eqOff = makeEngine (p);
            for (double hz : { 100.0, 1000.0, 8000.0 })
                expectEquals (eqOff->responseDb (hz), 0.0, "with the EQ off the response is flat");
            runConstant (*eqOff, -10.0, 1.0);
            expectGreaterThan (eqOff->getGainReductionDb(), 10.0f, "and the compressor still works");

            p.eqOn = true;
            p.dynamicsOn = false;
            auto dynOff = makeEngine (p);
            expectWithinAbsoluteError (dynOff->responseDb (1000.0), 12.0, 0.1, "with the dynamics off the EQ still works");
            runConstant (*dynOff, -10.0, 1.0);
            expectEquals (dynOff->getGainReductionDb(), 0.0f, "and nothing is compressed");
            expectWithinAbsoluteError (measuredGainDb (p, 1000.0, -10.0), 12.0, 0.1, "the sound is the EQ's alone");
        }

        beginTest ("Compressor: the published ratios and the soft knee follow the static curve, and make-up gain adds exactly its amount");
        {
            for (float ratio : { 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 10.0f, 20.0f })
            {
                Params p;
                p.compThresholdDb = -20.0f; p.compRatio = ratio; p.compSoftKnee = false; p.compAttackMs = 0.1f; p.compReleaseSeconds = 0.1f;
                auto e = makeEngine (p);
                runConstant (*e, -8.0, 1.5);   // 12 dB over
                expectWithinAbsoluteError (e->getGainReductionDb(), 12.0f * (1.0f - 1.0f / ratio), 0.05f, "ratio " + juce::String (ratio) + ":1 above the threshold");
            }
            Params below;
            below.compThresholdDb = -20.0f; below.compRatio = 10.0f; below.compSoftKnee = false;
            auto be = makeEngine (below);
            runConstant (*be, -30.0, 1.0);
            expectEquals (be->getGainReductionDb(), 0.0f, "nothing is reduced below the threshold");

            Params off;
            off.compThresholdDb = -40.0f; off.compRatio = 1.0f;
            auto oe = makeEngine (off);
            runConstant (*oe, -5.0, 1.0);
            expectEquals (oe->getGainReductionDb(), 0.0f, "a ratio of 1 is no compression at any level");

            // The knee: a hard knee has none at the threshold, a soft one has the textbook amount, and the curves meet well above it.
            expectEquals (compressorReductionDb (-20.0f, -20.0f, 4.0f, 0.0f), 0.0f);
            expectWithinAbsoluteError (compressorReductionDb (-20.0f, -20.0f, 4.0f, softKneeDb), 0.75f * softKneeDb / 8.0f, 1.0e-5f, "a soft knee at the threshold");
            expectEquals (compressorReductionDb (-10.0f, -20.0f, 4.0f, softKneeDb), compressorReductionDb (-10.0f, -20.0f, 4.0f, 0.0f), "above the knee the two agree");
            for (float level = -40.0f; level < 0.0f; level += 0.5f)
                expect (compressorReductionDb (level + 0.5f, -20.0f, 4.0f, softKneeDb) >= compressorReductionDb (level, -20.0f, 4.0f, softKneeDb) - 1.0e-6f, "reduction never falls as the level rises");

            Params mk;
            mk.compMakeupDb = 6.0f;
            expectWithinAbsoluteError (measuredGainDb (mk, 1000.0), 6.0, 0.05, "make-up gain adds its amount when nothing is reduced");
            mk.compMakeupDb = 99.0f;
            expectEquals (makeEngine (mk)->getParams().compMakeupDb, maxMakeupDb, "and holds at its range");
        }

        beginTest ("Compressor attack and release: the time is how long the reduction takes to cover 63 % of the way");
        {
            for (float attack : { 3.0f, 10.0f, 30.0f })
            {
                Params p;
                p.compThresholdDb = -30.0f; p.compRatio = 10.0f; p.compSoftKnee = false; p.compAttackMs = attack;
                auto e = makeEngine (p);
                std::vector<float> reduction;
                runConstant (*e, -10.0, 0.5, &reduction);
                expectWithinAbsoluteError (timeToFractionMs (reduction, 0.632f), (double) attack, attack * 0.1 + 0.1, "attack " + juce::String (attack) + " ms");
            }
            for (float release : { 0.3f, 1.0f })
            {
                Params p;
                p.compThresholdDb = -30.0f; p.compRatio = 10.0f; p.compSoftKnee = false; p.compAttackMs = 0.1f; p.compReleaseSeconds = release;
                auto e = makeEngine (p);
                runConstant (*e, -10.0, 0.5);
                std::vector<float> reduction;
                runConstant (*e, -60.0, (double) release * 6.0, &reduction);
                // The reduction falls from its held value towards nothing; it has covered 63 % of the way after about the release time
                // (a few milliseconds more, for the detector to let go).
                expectWithinAbsoluteError (timeToFractionMs (reduction, 0.632f) / 1000.0, (double) release, (double) release * 0.1 + 0.02, "release " + juce::String (release) + " s");
            }
        }

        beginTest ("Compressor stereo link: one gain for both channels, so a loud left and a quiet right keep their balance");
        {
            Params p;
            p.compThresholdDb = -30.0f; p.compRatio = 6.0f; p.compSoftKnee = false; p.compAttackMs = 0.5f;
            auto e = makeEngine (p);
            const float left = amp (-12.0), right = amp (-32.0);
            std::vector<float> outL, outR;
            for (int i = 0; i < (int) (fs * 0.5); ++i)
            {
                float l = (i & 1) ? -left : left, r = (i & 1) ? -right : right;
                float* ch[] = { &l, &r };
                e->process (ch, 2, 1);
                if (i > (int) (fs * 0.4)) { outL.push_back (l); outR.push_back (r); }
            }
            const double balanceIn = 20.0 * std::log10 (left / right);
            const double balanceOut = 20.0 * std::log10 (rms (outL, 0, outL.size()) / rms (outR, 0, outR.size()));
            expectWithinAbsoluteError (balanceOut, balanceIn, 0.01, "the left and right keep the same difference in level");
            expectGreaterThan (e->getGainReductionDb(), 5.0f, "and the loud one made it work");
        }

        beginTest ("Gate and expander: a gate closes steeply down to its range, an expander gently, the hold keeps it open, and above the threshold nothing changes");
        {
            Params g;
            g.gateOn = true; g.gateThresholdDb = -40.0f; g.gateRangeDb = 30.0f; g.gateExpander = false; g.gateAttackMs = 1.0f;
            g.gateReleaseSeconds = 0.1f; g.gateHoldSeconds = 0.0f;
            auto closed = makeEngine (g);
            runConstant (*closed, -60.0, 1.5);
            expectWithinAbsoluteError (closed->getGateReductionDb(), 30.0f, 0.05f, "a gate goes down by its range");
            auto above = makeEngine (g);
            runConstant (*above, -30.0, 1.0);
            expectEquals (above->getGateReductionDb(), 0.0f, "above its threshold nothing is turned down");
            expectWithinAbsoluteError (measuredGainDb (g, 1000.0, -20.0), 0.0, 0.01, "and a loud sound passes at its own level");

            Params x = g;
            x.gateExpander = true;
            auto expander = makeEngine (x);
            runConstant (*expander, -50.0, 1.5);   // 10 dB below: the expander turns it down by about 10 dB (ratio 2:1)
            expectWithinAbsoluteError (expander->getGateReductionDb(), 10.0f, 0.2f, "an expander goes down by the amount it is below, at 2:1");
            expect (expander->getGateReductionDb() < closed->getGateReductionDb(), "gentler than the gate");

            Params none = g;
            none.gateRangeDb = 0.0f;
            auto noRange = makeEngine (none);
            runConstant (*noRange, -60.0, 1.0);
            expectEquals (noRange->getGateReductionDb(), 0.0f, "a range of nothing is no gate");

            Params off = g;
            off.gateOn = false;
            auto gateOff = makeEngine (off);
            runConstant (*gateOff, -60.0, 1.0);
            expectEquals (gateOff->getGateReductionDb(), 0.0f, "with the gate off nothing happens");

            // Hold: after a loud sound stops, the gate stays open for the hold time before it closes.
            Params h = g;
            h.gateHoldSeconds = 0.5f; h.gateReleaseSeconds = 0.1f;
            auto held = makeEngine (h);
            runConstant (*held, -20.0, 0.3);
            std::vector<float> after;
            for (int i = 0; i < (int) (fs * 1.2); ++i)
            {
                float v = 0.0f;
                float* ch[] = { &v };
                held->process (ch, 1, 1);
                after.push_back (held->getGateReductionDb());
            }
            expectEquals (after[(size_t) (fs * 0.3)], 0.0f, "still open 0.3 s after the sound stopped, with a hold of 0.5 s");
            expectGreaterThan (after.back(), 25.0f, "and closed once the hold and the release have passed");

            // It opens again quickly.
            std::vector<float> opening;
            runConstant (*closed, -20.0, 0.05, &opening);
            expectLessThan (closed->getGateReductionDb(), 1.0f, "a loud sound opens it within a few milliseconds");
        }

        beginTest ("Dynamics order: before the equaliser they see the sound as it comes in, after it they see what the equaliser made of it");
        {
            Params p;
            p.lfBell = true; p.lfHz = 100.0f; p.lfDb = 12.0f;
            p.compThresholdDb = -30.0f; p.compRatio = 8.0f; p.compSoftKnee = false; p.compAttackMs = 0.5f;
            const auto in = sine (100.0, -36.0, 1.0);
            auto before = makeEngine (p);
            run (*before, in);
            const float reductionBefore = before->getGainReductionDb();
            p.dynamicsAfterEq = true;
            auto after = makeEngine (p);
            run (*after, in);
            // Before the equaliser the sound is 6 dB under the threshold, after it 6 dB over (it was lifted 12 dB).
            expectLessThan (reductionBefore, 0.5f, "before the equaliser the compressor does not touch the quiet low note");
            expectGreaterThan (after->getGainReductionDb(), 3.0f, "after the equaliser it sees the boosted low note and reduces it");
        }

        beginTest ("Drive: 0 is exactly transparent, quiet sounds keep their level, loud ones are rounded off, mostly odd harmonics, and it folds back less than a plain clip");
        {
            Params none;
            expectWithinAbsoluteError (measuredGainDb (none, 1000.0, -6.0), 0.0, 0.01, "drive 0 changes nothing");

            Params d;
            d.drive = 1.0f;
            expectWithinAbsoluteError (measuredGainDb (d, 1000.0, -50.0), 0.0, 0.05, "a quiet sound keeps its level at full drive");
            double previous = 100.0;
            for (float amount : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                Params p;
                p.drive = amount;
                const double gain = measuredGainDb (p, 1000.0, -6.0);
                expectLessOrEqual (gain, previous + 1.0e-6, "more drive never makes a loud sound louder");
                previous = gain;
            }
            expectLessThan (measuredGainDb (d, 1000.0, -6.0), -3.0, "a loud sound is rounded off at full drive");

            // A symmetric curve makes odd harmonics: the third is far above the second.
            auto e = makeEngine (d);
            const auto out = run (*e, sine (1000.0, -6.0, 1.0));
            const std::vector<float> tail (out.begin() + 24000, out.end());
            expectGreaterThan (binDb (tail, 3000.0), binDb (tail, 2000.0) + 20.0, "the third harmonic is well above the second");

            // A tone near the top of the range makes harmonics that would fold back into the audible range. The drive folds back less
            // than the same curve applied to each sample by itself.
            const double toneHz = 17000.0;
            auto tone = sine (toneHz, -6.0, 1.0);
            auto driven = makeEngine (d);
            const auto drivenOut = run (*driven, tone);
            const float g = driveGain (1.0f);
            std::vector<float> plain (tone.size());
            for (size_t i = 0; i < tone.size(); ++i)
                plain[i] = std::tanh (g * tone[i]) / g;
            const std::vector<float> drivenTail (drivenOut.begin() + 24000, drivenOut.end()), plainTail (plain.begin() + 24000, plain.end());
            // The third harmonic (51 kHz) folds to 3 kHz at this sample rate: it is only there as an alias.
            const double aliasDriven = binDb (drivenTail, 3000.0), aliasPlain = binDb (plainTail, 3000.0);
            expectLessThan (aliasDriven, aliasPlain - 3.0, "the folded-back third harmonic is at least 3 dB lower than from a plain clip");
        }

        beginTest ("Block size makes no difference to the sound, with everything switched on");
        {
            Params p;
            p.inputTrimDb = 3.0f; p.drive = 0.4f; p.highPassHz = 80.0f; p.lowPassHz = 12000.0f;
            p.lfDb = 5.0f; p.lmfDb = -4.0f; p.hmfDb = 3.0f; p.hfDb = 4.0f;
            p.compThresholdDb = -24.0f; p.compRatio = 4.0f; p.compMakeupDb = 3.0f;
            p.gateOn = true; p.gateThresholdDb = -55.0f; p.gateRangeDb = 15.0f; p.outputTrimDb = -2.0f;
            std::mt19937 rng (9);
            std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
            std::vector<float> in (30000);
            for (auto& x : in) x = noise (rng);
            auto reference = makeEngine (p);
            const auto expected = run (*reference, in, 480);
            for (int block : { 1, 7, 64, 1000, 30000 })
            {
                auto e = makeEngine (p);
                const auto got = run (*e, in, block);
                float worst = 0.0f;
                for (size_t i = 0; i < in.size(); ++i) worst = std::max (worst, std::abs (got[i] - expected[i]));
                expectEquals (worst, 0.0f, "block size " + juce::String (block) + " gives the same sound");
            }
        }

        beginTest ("Mono and stereo both work, and a damaged input never makes the strip misbehave");
        {
            Params p;
            p.drive = 0.6f; p.highPassHz = 60.0f; p.lmfDb = 8.0f; p.compThresholdDb = -20.0f; p.compRatio = 6.0f; p.gateOn = true;
            auto e = makeEngine (p);
            std::vector<float> left (4000), right (4000, 0.123f);
            for (size_t i = 0; i < left.size(); ++i) left[i] = 0.4f * (float) std::sin (2.0 * pi * 440.0 * (double) i / fs);
            auto rightBefore = right;
            float* mono[] = { left.data(), right.data() };
            e->process (mono, 1, (int) left.size());
            expect (right == rightBefore, "with one channel the second is not touched");

            auto stereo = makeEngine (p);
            std::vector<float> l2 (4000), r2 (4000);
            for (size_t i = 0; i < l2.size(); ++i) l2[i] = r2[i] = 0.4f * (float) std::sin (2.0 * pi * 440.0 * (double) i / fs);
            float* both[] = { l2.data(), r2.data() };
            stereo->process (both, 2, (int) l2.size());
            bool same = true;
            for (size_t i = 0; i < l2.size(); ++i) same = same && l2[i] == r2[i];
            expect (same, "two identical channels stay identical");

            // Damaged input (not a number, infinite) and silence, with a spread of settings.
            std::mt19937 rng (21);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);
            for (int trial = 0; trial < 40; ++trial)
            {
                Params q;
                q.inputTrimDb = -20.0f + 40.0f * u (rng); q.outputTrimDb = -20.0f + 40.0f * u (rng); q.drive = u (rng);
                q.highPassHz = u (rng) > 0.5f ? 16.0f + 334.0f * u (rng) : 0.0f; q.lowPassHz = u (rng) > 0.5f ? 3000.0f + 19000.0f * u (rng) : 0.0f;
                q.lfDb = -15.0f + 30.0f * u (rng); q.lfHz = 30.0f + 420.0f * u (rng); q.lfBell = u (rng) > 0.5f;
                q.lmfDb = -15.0f + 30.0f * u (rng); q.lmfHz = 200.0f + 2300.0f * u (rng); q.lmfQ = 0.5f + 2.0f * u (rng);
                q.hmfDb = -15.0f + 30.0f * u (rng); q.hmfHz = 600.0f + 6400.0f * u (rng); q.hmfQ = 0.5f + 2.0f * u (rng);
                q.hfDb = -15.0f + 30.0f * u (rng); q.hfHz = 1500.0f + 14500.0f * u (rng); q.hfBell = u (rng) > 0.5f;
                q.compThresholdDb = -40.0f * u (rng); q.compRatio = 1.0f + 19.0f * u (rng); q.compAttackMs = 0.1f + 29.9f * u (rng);
                q.compReleaseSeconds = 0.1f + 3.9f * u (rng); q.compMakeupDb = 20.0f * u (rng);
                q.gateOn = u (rng) > 0.5f; q.gateThresholdDb = -70.0f + 60.0f * u (rng); q.gateRangeDb = 40.0f * u (rng); q.gateExpander = u (rng) > 0.5f;
                q.dynamicsAfterEq = u (rng) > 0.5f; q.eqOn = u (rng) > 0.3f; q.dynamicsOn = u (rng) > 0.3f;
                auto qe = makeEngine (q);
                std::vector<float> block (3000);
                for (size_t i = 0; i < block.size(); ++i)
                    block[i] = i % 500 == 0 ? std::numeric_limits<float>::quiet_NaN() : (i % 777 == 0 ? std::numeric_limits<float>::infinity() : 2.0f * u (rng) - 1.0f);
                float* ch[] = { block.data() };
                qe->process (ch, 1, (int) block.size());
                bool finite = true;
                for (float v : block) finite = finite && std::isfinite (v);
                expect (finite, "a damaged input leaves the output finite (trial " + juce::String (trial) + ")");
                // Silence in, long enough for the filters and the dynamics to settle after what came before, gives silence out.
                std::vector<float> silence (240000, 0.0f);
                float* sc[] = { silence.data() };
                qe->process (sc, 1, (int) silence.size());
                float peak = 0.0f;
                for (size_t i = silence.size() - 4000; i < silence.size(); ++i) peak = std::max (peak, std::abs (silence[i]));
                expectLessThan (peak, 1.0e-3f, "and silence stays silent (trial " + juce::String (trial) + ")");
            }
        }
    }
};

static DeskEngineTests deskEngineTests;

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

int main()
{
    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("AnalogDesk");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }
    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
