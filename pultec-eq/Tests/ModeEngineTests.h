#pragma once

// PEQ-04: the Mid Shaper and the Frequency Filter modes, and switching between modes. Included at the end of
// EngineTests.cpp (it uses that file's measureSineDb helper). Run as part of PassiveEQTests.exe (category PassiveEQ).

class PassiveModeEngineTests final : public juce::UnitTest
{
public:
    PassiveModeEngineTests() : juce::UnitTest ("Pultec-style EQ modes", "PassiveEQ") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("Tone Shaper is the default mode, and a mode with nothing set leaves every sample unchanged");
        {
            expect (Params().mode == Mode::toneShaper);
            for (auto mode : { Mode::toneShaper, Mode::midShaper, Mode::frequencyFilter })
            {
                Engine e; e.prepare (fs);
                Params p; p.mode = mode;
                e.setParams (p);
                std::vector<float> x (4800), ref;
                for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * (float) std::sin (0.07 * (double) i) + 0.1f * (float) std::sin (1.3 * (double) i);
                ref = x;
                float* ch[] = { x.data() };
                e.process (ch, 1, (int) x.size());
                expect (x == ref, "bit-identical with nothing set, mode " + juce::String ((int) mode));
                for (double hz : { 30.0, 200.0, 1000.0, 8000.0 })
                    expectEquals (e.responseDb (hz), 0.0f, "flat response, mode " + juce::String ((int) mode));
            }
        }

        beginTest ("Mid Shaper: low-mid boost, mid dip and high-mid boost each work on their own, at their own frequency");
        {
            struct Case { const char* name; int which; double hz, otherHz; };
            const Case cases[] = { { "low-mid boost at 500 Hz", 0, 500.0, 8000.0 }, { "mid dip at 1 kHz", 1, 1000.0, 50.0 }, { "high-mid boost at 3 kHz", 2, 3000.0, 100.0 } };
            for (const auto& c : cases)
            {
                Params p; p.mode = Mode::midShaper;
                float* amount = c.which == 0 ? &p.midLowBoostDb : c.which == 1 ? &p.midDipDb : &p.midHighBoostDb;
                *amount = 6.0f;
                const float expected = c.which == 1 ? -6.0f : 6.0f;
                expectWithinAbsoluteError (measureSineDb (p, fs, c.hz), expected, 0.35f, juce::String (c.name) + " reaches its amount at its frequency");
                expectLessThan (std::abs (measureSineDb (p, fs, c.otherHz)), 1.0f, juce::String (c.name) + " leaves a distant frequency alone");
            }
        }

        beginTest ("Mid Shaper: the shapes are broad (about half the amount or more, an octave either side), and the frequency switches move them");
        {
            Params p; p.mode = Mode::midShaper; p.midLowBoostDb = 8.0f; p.midLowFrequencyIndex = 2;   // 500 Hz
            Engine e; e.prepare (fs); e.setParams (p);
            expectGreaterThan (e.responseDb (250.0), 3.5f, "an octave below: about half the amount or more");
            expectGreaterThan (e.responseDb (1000.0), 3.5f, "an octave above: about half the amount or more");
            for (size_t i = 0; i < midLowFrequencies.size(); ++i)
            {
                p.midLowFrequencyIndex = (int) i;
                e.setParams (p); e.prepare (fs);
                expectWithinAbsoluteError (e.responseDb (midLowFrequencies[i]), 8.0f, 0.1f, "the low-mid boost peaks at switch position " + juce::String ((int) i));
            }
            Params d; d.mode = Mode::midShaper; d.midDipDb = 8.0f;
            for (size_t i = 0; i < midDipFrequencies.size(); ++i)
            {
                d.midDipFrequencyIndex = (int) i;
                e.setParams (d); e.prepare (fs);
                expectWithinAbsoluteError (e.responseDb (midDipFrequencies[i]), -8.0f, 0.1f, "the mid dip bottoms out at switch position " + juce::String ((int) i));
            }
            Params h; h.mode = Mode::midShaper; h.midHighBoostDb = 8.0f;
            for (size_t i = 0; i < midHighFrequencies.size(); ++i)
            {
                h.midHighFrequencyIndex = (int) i;
                e.setParams (h); e.prepare (fs);
                expectWithinAbsoluteError (e.responseDb (midHighFrequencies[i]), 8.0f, 0.1f, "the high-mid boost peaks at switch position " + juce::String ((int) i));
            }
        }

        beginTest ("Mid Shaper: a boost and a dip at the same frequency work together");
        {
            Params p; p.mode = Mode::midShaper; p.midLowBoostDb = 6.0f; p.midLowFrequencyIndex = 4; p.midDipDb = 6.0f; p.midDipFrequencyIndex = 4;   // both 1 kHz
            Engine e; e.prepare (fs); e.setParams (p);
            expectWithinAbsoluteError (e.responseDb (1000.0), 0.0f, 0.05f, "equal boost and dip cancel at their shared frequency");
        }

        beginTest ("Frequency Filter: the high-pass removes the low end and the low-pass removes the top, and neither touches the middle");
        {
            Params hp; hp.mode = Mode::frequencyFilter; hp.highPassIndex = 3;   // 80 Hz
            expectWithinAbsoluteError (measureSineDb (hp, fs, 80.0), -3.0f, 0.3f, "a high-pass is 3 dB down at its frequency");
            expectLessThan (measureSineDb (hp, fs, 20.0), -20.0f, "and far down two octaves below");
            expectLessThan (std::abs (measureSineDb (hp, fs, 1000.0)), 0.1f, "and flat in the middle");
            expectLessThan (std::abs (measureSineDb (hp, fs, 8000.0)), 0.1f, "and flat at the top");

            Params lp; lp.mode = Mode::frequencyFilter; lp.lowPassIndex = 3;   // 10 kHz
            expectWithinAbsoluteError (measureSineDb (lp, fs, 10000.0), -3.0f, 0.5f, "a low-pass is 3 dB down at its frequency");
            expectLessThan (measureSineDb (lp, fs, 20000.0), -9.0f, "and well down an octave above");
            expectLessThan (std::abs (measureSineDb (lp, fs, 1000.0)), 0.1f, "and flat in the middle");
            expectLessThan (std::abs (measureSineDb (lp, fs, 100.0)), 0.1f, "and flat at the bottom");

            for (size_t i = 1; i < highPassFrequencies.size(); ++i)
            {
                Params q; q.mode = Mode::frequencyFilter; q.highPassIndex = (int) i;
                Engine e; e.prepare (fs); e.setParams (q);
                expectWithinAbsoluteError (e.responseDb (highPassFrequencies[i]), -3.0f, 0.1f, "high-pass position " + juce::String ((int) i));
            }
            for (size_t i = 1; i < lowPassFrequencies.size(); ++i)
            {
                Params q; q.mode = Mode::frequencyFilter; q.lowPassIndex = (int) i;
                Engine e; e.prepare (fs); e.setParams (q);
                expectWithinAbsoluteError (e.responseDb (lowPassFrequencies[i]), -3.0f, 0.5f, "low-pass position " + juce::String ((int) i));
            }

            Params both; both.mode = Mode::frequencyFilter; both.highPassIndex = 3; both.lowPassIndex = 2;
            Engine e; e.prepare (fs); e.setParams (both);
            expectLessThan (e.responseDb (30.0), -6.0f, "both together: the low end is cut");
            expectLessThan (e.responseDb (16000.0), -6.0f, "and the top is cut");
            expectLessThan (std::abs (e.responseDb (1000.0)), 0.2f, "and the middle stays");
        }

        beginTest ("Each mode's settings do nothing while another mode is selected");
        {
            Params p; p.lowBoostDb = 8.0f; p.midLowBoostDb = 8.0f; p.midDipDb = 8.0f; p.highPassIndex = 3; p.lowPassIndex = 2;
            p.mode = Mode::toneShaper;
            Engine tone; tone.prepare (fs); tone.setParams (p);
            Params onlyTone; onlyTone.lowBoostDb = 8.0f;
            Engine toneOnly; toneOnly.prepare (fs); toneOnly.setParams (onlyTone);
            for (double hz : { 40.0, 80.0, 500.0, 1000.0, 9000.0 })
                expectEquals (tone.responseDb (hz), toneOnly.responseDb (hz), "Tone Shaper ignores the other modes at " + juce::String (hz));

            p.mode = Mode::midShaper;
            Engine mid; mid.prepare (fs); mid.setParams (p);
            expectLessThan (std::abs (mid.responseDb (40.0)), 0.5f, "Mid Shaper has no low boost");
            p.mode = Mode::frequencyFilter;
            Engine filter; filter.prepare (fs); filter.setParams (p);
            expectLessThan (std::abs (filter.responseDb (500.0)), 0.2f, "Frequency Filter has no mid shapes");
        }

        beginTest ("Changing the mode while sound plays fades, without a click, and settles to the new mode");
        for (auto [from, to] : { std::pair<Mode, Mode> { Mode::toneShaper, Mode::midShaper }, { Mode::midShaper, Mode::frequencyFilter }, { Mode::frequencyFilter, Mode::toneShaper } })
        {
            auto biggestStep = [&] (bool change)
            {
                Engine e; e.prepare (fs);
                Params p; p.lowBoostDb = 12.0f; p.lowFrequencyIndex = 2; p.midLowBoostDb = 9.0f; p.midDipDb = 9.0f; p.highPassIndex = 3; p.lowPassIndex = 2;
                p.mode = from;
                e.setParams (p);
                const int n = (int) (fs * 0.6), at = (int) (fs * 0.3);
                float last = 0.0f, biggest = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    if (change && i == at) { p.mode = to; e.setParams (p); }
                    float x = 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * (double) i / fs);
                    float* ch[] = { &x };
                    e.process (ch, 1, 1);
                    if (i > (int) (fs * 0.2) && std::isfinite (x)) biggest = juce::jmax (biggest, std::abs (x - last));
                    last = x;
                }
                return biggest;
            };
            const float steady = biggestStep (false), changed = biggestStep (true);
            expectLessThan (changed, steady * 2.5f + 0.05f, "no click changing mode " + juce::String ((int) from) + " to " + juce::String ((int) to));
        }
        {
            Engine e; e.prepare (fs);
            Params p; p.lowBoostDb = 10.0f; p.midDipDb = 10.0f; p.midDipFrequencyIndex = 4; p.mode = Mode::toneShaper;
            e.setParams (p);
            std::vector<float> x (48000);
            for (size_t i = 0; i < x.size(); ++i) x[i] = 0.1f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / fs);
            float* ch[] = { x.data() };
            e.process (ch, 1, 24000);
            p.mode = Mode::midShaper;
            e.setParams (p);
            float* ch2[] = { x.data() + 24000 };
            e.process (ch2, 1, 24000);
            double power = 0.0;
            for (size_t i = 36000; i < x.size(); ++i) power += (double) x[i] * x[i];
            const float measured = (float) (10.0 * std::log10 (power / 12000.0 / (0.1 * 0.1 * 0.5)));
            expectWithinAbsoluteError (measured, e.responseDb (1000.0), 0.3f, "settles to the Mid Shaper dip");
            expectLessThan (measured, -8.0f);
        }

        beginTest ("Every mode gives the same result whatever the block size, at every supported sample rate, and tolerates bad values");
        for (auto mode : { Mode::midShaper, Mode::frequencyFilter })
        {
            Params p; p.mode = mode; p.midLowBoostDb = 6.0f; p.midDipDb = 5.0f; p.midHighBoostDb = 7.0f; p.highPassIndex = 4; p.lowPassIndex = 3;
            auto render = [&] (int block, double rate)
            {
                Engine e; e.prepare (rate); e.setParams (p);
                std::vector<float> x ((size_t) rate / 2);
                for (size_t i = 0; i < x.size(); ++i) x[i] = 0.25f * (float) std::sin (0.03 * (double) i) + 0.1f * (float) std::sin (0.9 * (double) i);
                for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
                {
                    float* ch[] = { x.data() + pos };
                    e.process (ch, 1, (int) juce::jmin ((size_t) block, x.size() - pos));
                }
                return x;
            };
            for (double rate : { 44100.0, 48000.0, 96000.0 })
                expect (render (64, rate) == render (4096, rate), "block size does not change the result");

            Params wild; wild.mode = (Mode) 99; wild.midDipFrequencyIndex = -5; wild.highPassIndex = 99; wild.lowPassIndex = -1;
            wild.midLowBoostDb = std::numeric_limits<float>::quiet_NaN(); wild.midHighBoostDb = 1.0e9f;
            Engine e; e.prepare (fs); e.setParams (wild);
            std::vector<float> x (4800, 0.2f);
            float* ch[] = { x.data() };
            e.process (ch, 1, (int) x.size());
            for (auto s : x) expect (std::isfinite (s), "finite out");
        }
    }
};

static PassiveModeEngineTests passiveModeEngineTests;
