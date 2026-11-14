// Pultec-style EQ engine tests (PEQ-01, slice 1). Run: PassiveTests.exe
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <limits>
#include <vector>
#include "PassiveEngine.h"

using namespace SongsmithPassive;

namespace
{
    // The gain in dB the engine gives a steady sine, measured on the RMS of the second half.
    float measureSineDb (const Params& p, double fs, double freq)
    {
        Engine e; e.prepare (fs); e.setParams (p);
        const int n = (int) (fs * 1.5);
        std::vector<float> x ((size_t) n), ref;
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / fs);
        ref = x;
        float* ch[] = { x.data() };
        e.process (ch, 1, n);
        double a = 0.0, b = 0.0;
        for (int i = n / 2; i < n; ++i) { a += (double) x[(size_t) i] * x[(size_t) i]; b += (double) ref[(size_t) i] * ref[(size_t) i]; }
        return (float) (10.0 * std::log10 (a / b));
    }
}

class PassiveEngineTests final : public juce::UnitTest
{
public:
    PassiveEngineTests() : juce::UnitTest ("Pultec-style EQ engine", "PassiveEQ") {}

    // The settled response: ask a prepared engine whose smoothing has finished.
    static float settledResponse (const Params& p, double fs, double freq)
    {
        Engine e; e.prepare (fs); e.setParams (p);
        return e.responseDb (freq);
    }

    void runTest() override
    {
        beginTest ("The switch positions and gain ranges are the published panel values");
        {
            expect (lowFrequencies == std::array<float, 4> { 20.0f, 30.0f, 60.0f, 100.0f });
            expect (highBoostFrequencies == std::array<float, 7> { 3000.0f, 4000.0f, 5000.0f, 8000.0f, 10000.0f, 12000.0f, 16000.0f });
            expect (highCutFrequencies == std::array<float, 3> { 5000.0f, 10000.0f, 20000.0f });
            expectEquals (maxLowBoostDb, 13.5f);
            expectEquals (maxLowCutDb, 17.5f);
            expectEquals (maxHighBoostDb, 18.0f);
            expectEquals (maxHighCutDb, 16.0f);
        }

        beginTest ("Everything at zero: the signal passes through bit-identical, at every sample rate and block size");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 1, 7, 512 })
            {
                Engine e; e.prepare (fs); e.setParams (Params());
                juce::Random rng (5);
                std::vector<float> in (3000), out;
                for (auto& s : in) s = rng.nextFloat() * 1.8f - 0.9f;
                out = in;
                for (size_t pos = 0; pos < out.size(); pos += (size_t) block)
                {
                    float* ch[] = { out.data() + pos };
                    e.process (ch, 1, (int) juce::jmin ((size_t) block, out.size() - pos));
                }
                expect (out == in, "identical at " + juce::String (fs) + " Hz, block " + juce::String (block));
            }

        beginTest ("Low boost: the full boost far below the corner, half of it at the corner, nothing far above");
        for (int i = 0; i < 4; ++i)
        {
            Params p; p.lowFrequencyIndex = i; p.lowBoostDb = 13.5f;
            const double f = lowFrequencies[(size_t) i];
            expectWithinAbsoluteError (settledResponse (p, 48000.0, 1.0), 13.5f, 0.1f, "far below, " + juce::String (f));
            expectWithinAbsoluteError (settledResponse (p, 48000.0, f), 6.75f, 0.3f, "at the corner, " + juce::String (f));
            expectWithinAbsoluteError (settledResponse (p, 48000.0, f * 64.0), 0.0f, 0.3f, "far above, " + juce::String (f));
        }

        beginTest ("Low cut: the full cut far below the corner, and nothing far above");
        for (int i = 0; i < 4; ++i)
        {
            Params p; p.lowFrequencyIndex = i; p.lowCutDb = 17.5f;
            expectWithinAbsoluteError (settledResponse (p, 48000.0, 1.0), -17.5f, 0.1f);
            expectWithinAbsoluteError (settledResponse (p, 48000.0, lowFrequencies[(size_t) i] * 128.0), 0.0f, 0.3f);
        }

        beginTest ("Boost and cut together: a net bump at the bottom with a dip above it (the Songsmith model of the classic trick)");
        {
            Params p; p.lowFrequencyIndex = 2; p.lowBoostDb = 10.0f; p.lowCutDb = 6.0f;
            expectWithinAbsoluteError (settledResponse (p, 48000.0, 1.0), 4.0f, 0.15f, "net boost at the very bottom");
            float lowest = 100.0f;
            for (double f = 60.0; f <= 500.0; f *= 1.05)
                lowest = juce::jmin (lowest, settledResponse (p, 48000.0, f));
            expectLessThan (lowest, -1.0f, "a dip between the corners");
            expectWithinAbsoluteError (settledResponse (p, 48000.0, 3000.0), 0.0f, 0.3f, "flat well above");
            Params equal; equal.lowBoostDb = 8.0f; equal.lowCutDb = 8.0f;
            expectWithinAbsoluteError (settledResponse (equal, 48000.0, 1.0), 0.0f, 0.1f, "equal settings cancel at the bottom");
        }

        beginTest ("High boost: the set boost at the centre for every switch position, narrower at the narrow setting");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int i = 0; i < 7; ++i)
            {
                Params p; p.highBoostFrequencyIndex = i; p.highBoostDb = 12.0f; p.bandwidth = 0.5f;
                const double f = highBoostFrequencies[(size_t) i];
                if (f > 0.45 * fs) continue;
                expectWithinAbsoluteError (settledResponse (p, fs, f), 12.0f, 0.1f, juce::String (f) + " Hz at " + juce::String (fs));

                auto widthAtHalf = [&] (float bandwidth)
                {
                    Params q = p; q.bandwidth = bandwidth;
                    double lo = f, hi = f;
                    while (lo > 20.0 && settledResponse (q, fs, lo) > 6.0f) lo /= 1.01;
                    while (hi < 0.45 * fs && settledResponse (q, fs, hi) > 6.0f) hi *= 1.01;
                    return std::log2 (hi / lo);
                };
                expectLessThan (widthAtHalf (1.0f), widthAtHalf (0.0f), "narrow is narrower than broad at " + juce::String (f));
            }

        beginTest ("High cut: a shelf that reaches the full cut well above its corner and leaves the lows alone");
        for (int i = 0; i < 3; ++i)
        {
            Params p; p.highCutFrequencyIndex = i; p.highCutDb = 16.0f;
            const double f = highCutFrequencies[(size_t) i];
            expectWithinAbsoluteError (settledResponse (p, 96000.0, 100.0), 0.0f, 0.1f, "lows untouched");
            if (f <= 10000.0)
                expectWithinAbsoluteError (settledResponse (p, 96000.0, f * 4.0), -16.0f, 1.0f, "cut well above " + juce::String (f));
            expectWithinAbsoluteError (settledResponse (p, 96000.0, f), -8.0f, 0.4f, "half the cut at the corner " + juce::String (f));
        }

        beginTest ("The same settings sound the same at 44.1, 48 and 96 kHz below 10 kHz");
        {
            Params p; p.lowFrequencyIndex = 2; p.lowBoostDb = 9.0f; p.lowCutDb = 4.0f;
            p.highBoostFrequencyIndex = 2; p.highBoostDb = 8.0f; p.bandwidth = 0.3f; p.highCutFrequencyIndex = 0; p.highCutDb = 5.0f;
            for (double f : { 30.0, 100.0, 400.0, 1000.0, 3000.0, 5000.0, 8000.0 })
            {
                const float a = settledResponse (p, 48000.0, f);
                expectWithinAbsoluteError (settledResponse (p, 44100.0, f), a, 0.6f, juce::String (f) + " Hz, 44.1 kHz");
                expectWithinAbsoluteError (settledResponse (p, 96000.0, f), a, 0.6f, juce::String (f) + " Hz, 96 kHz");
            }
        }

        beginTest ("A sine through the engine is changed by the amount the response says");
        {
            Params p; p.lowFrequencyIndex = 3; p.lowBoostDb = 9.0f; p.lowCutDb = 3.0f;
            p.highBoostFrequencyIndex = 3; p.highBoostDb = 10.0f; p.highCutFrequencyIndex = 1; p.highCutDb = 6.0f;
            for (double f : { 40.0, 120.0, 800.0, 8000.0, 12000.0 })
                expectWithinAbsoluteError (measureSineDb (p, 48000.0, f), settledResponse (p, 48000.0, f), 0.15f, juce::String (f) + " Hz");
        }

        beginTest ("Block size does not change the result, and the two channels are independent");
        {
            Params p; p.lowBoostDb = 8.0f; p.highBoostDb = 6.0f; p.highCutDb = 4.0f;
            auto render = [&] (int block)
            {
                Engine e; e.prepare (48000.0); e.setParams (p);
                juce::Random rng (9);
                std::vector<float> l (5000), r (5000, 0.0f);
                for (auto& s : l) s = rng.nextFloat() * 1.4f - 0.7f;
                for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
                {
                    float* ch[] = { l.data() + pos, r.data() + pos };
                    e.process (ch, 2, (int) juce::jmin ((size_t) block, l.size() - pos));
                }
                return std::make_pair (l, r);
            };
            const auto a = render (5000), b = render (3), c = render (64);
            expect (a == b && a == c, "identical for block sizes 3, 64 and 5000");
            for (auto s : a.second) expectEquals (s, 0.0f, "silent right channel stays silent");
        }

        beginTest ("Moving a control while running glides: no jump, no NaN, and it arrives");
        {
            Engine e; e.prepare (48000.0);
            Params p;
            e.setParams (p);
            std::vector<float> out;
            for (int i = 0; i < 24000; ++i)
            {
                if (i == 4800) { p.lowBoostDb = 13.5f; p.highBoostDb = 18.0f; e.setParams (p); }
                float x = 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (double) i / 48000.0);
                float* ch[] = { &x };
                e.process (ch, 1, 1);
                expect (std::isfinite (x));
                out.push_back (x);
            }
            float maxStep = 0.0f;
            for (size_t i = 4800; i < out.size(); ++i) maxStep = juce::jmax (maxStep, std::abs (out[i] - out[i - 1]));
            expectLessThan (maxStep, 0.12f, "no click while gains glide");
            float peak = 0.0f;
            for (size_t i = 20000; i < out.size(); ++i) peak = juce::jmax (peak, std::abs (out[i]));
            expectWithinAbsoluteError (peak, 0.2f * std::pow (10.0f, e.responseDb (100.0) / 20.0f), 0.2f * 0.08f, "arrives at the level the response gives at 100 Hz");
        }

        beginTest ("Extreme and non-finite values, and silence");
        {
            Engine e; e.prepare (48000.0);
            Params wild;
            wild.lowFrequencyIndex = 99; wild.highBoostFrequencyIndex = -4; wild.highCutFrequencyIndex = 77;
            wild.lowBoostDb = 1000.0f; wild.lowCutDb = std::numeric_limits<float>::quiet_NaN(); wild.highBoostDb = -50.0f;
            wild.bandwidth = 9.0f; wild.highCutDb = 1.0e9f;
            e.setParams (wild);
            std::vector<float> x (9600, 0.3f);
            x[100] = std::numeric_limits<float>::infinity(); x[101] = std::numeric_limits<float>::quiet_NaN();
            float* ch[] = { x.data() };
            e.process (ch, 1, (int) x.size());
            for (auto s : x) expect (std::isfinite (s), "finite out");

            Engine q; q.prepare (48000.0);
            Params p; p.lowBoostDb = 10.0f; p.highBoostDb = 10.0f;
            q.setParams (p);
            std::vector<float> z (48000, 0.0f);
            float* zc[] = { z.data() };
            q.process (zc, 1, (int) z.size());
            for (auto s : z) expectEquals (s, 0.0f, "silence in, silence out");
        }
    }
};

static PassiveEngineTests passiveEngineTests;

int main (int, char**)
{
    struct Runner final : juce::UnitTestRunner
    {
        void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); }
    } runner;
    runner.runAllTests();

    int failures = 0, checks = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        checks += runner.getResult (i)->passes + runner.getResult (i)->failures;
    }
    std::printf ("\n==== %d checks passed, %d failed ====\n", checks - failures, failures);
    return failures == 0 ? 0 : 1;
}
