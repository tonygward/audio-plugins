// EQ test runner + benchmark.
//   EQTests.exe           -> run all unit tests (exit code 1 on failure)
//   EQTests.exe --bench   -> CPU benchmark for 3 / 12 / 24 active bands
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <random>
#include "PluginProcessor.h"
#include "EQEngine.h"
#include "SvfFilter.h"
#include "NoteNames.h"

using namespace EQ;

namespace
{
    constexpr double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

    struct Spec
    {
        FilterType type;
        float freq, gain, q;
        bool enabled = true;
        int slope = defaultSlope;
    };

    void configure (Engine& e, const std::vector<Spec>& specs)
    {
        for (int b = 0; b < numBands; ++b)
        {
            BandSettings s;
            if (b < (int) specs.size())
            {
                const auto& sp = specs[(size_t) b];
                s.active = sp.enabled;
                s.type = sp.type;
                s.freq = sp.freq;
                s.gain = typeHasGain (sp.type) ? sp.gain : 0.0f;
                s.q = sp.q;
                s.slope = sp.slope;
            }
            e.setBand (b, s);
        }
    }

    double analyticDb (const std::vector<Spec>& specs, double fs, double f)
    {
        double db = 0.0;
        for (auto& s : specs)
            if (s.enabled)
                db += getMagnitudeDb (makeFilter (s.type, fs, s.freq, s.q, typeHasGain (s.type) ? s.gain : 0.0f, s.slope), f, fs);
        return db;
    }

    // Steady-state gain (dB) of the real-time engine for a sine at 'f'.
    double measureDb (const std::vector<Spec>& specs, double fs, double f, double settleSeconds = 0.5)
    {
        Engine e;
        e.prepare (fs);
        configure (e, specs);
        e.snapToTargets();

        const int settle = (int) (settleSeconds * fs);
        const double period = fs / f;
        const int periods = std::max (8, (int) std::ceil (0.25 * fs / period));
        const int measure = (int) std::lround (periods * period);

        constexpr int block = 512;
        std::vector<float> l (block), r (block);
        float* ch[] = { l.data(), r.data() };

        double sumIn = 0.0, sumOut = 0.0;
        const double w = 2.0 * pi * f / fs;
        int n = 0;

        while (n < settle + measure)
        {
            const int num = std::min (block, settle + measure - n);
            for (int i = 0; i < num; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) (0.25 * std::sin (w * (n + i)));

            std::vector<float> in (l.begin(), l.begin() + num);
            e.process (ch, 2, num);

            for (int i = 0; i < num; ++i)
                if (n + i >= settle)
                {
                    sumIn  += (double) in[(size_t) i] * in[(size_t) i];
                    sumOut += (double) l[(size_t) i] * l[(size_t) i];
                }
            n += num;
        }

        return 10.0 * std::log10 (std::max (sumOut, 1.0e-30) / sumIn);
    }

    // Runs a sine through the engine, calling 'onBlock' before each block so the
    // test can change settings mid-stream. Returns the left output signal.
    std::vector<float> runSine (Engine& e, double fs, double f, double amp, double seconds, int block,
                                const std::function<void (int blockIndex, int sample)>& onBlock)
    {
        const int total = (int) (seconds * fs);
        std::vector<float> out ((size_t) total);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        float* ch[] = { l.data(), r.data() };
        const double w = 2.0 * pi * f / fs;

        for (int n = 0, bi = 0; n < total; n += block, ++bi)
        {
            const int num = std::min (block, total - n);
            onBlock (bi, n);
            for (int i = 0; i < num; ++i)
                l[(size_t) i] = r[(size_t) i] = (float) (amp * std::sin (w * (n + i)));
            e.process (ch, 2, num);
            std::copy (l.begin(), l.begin() + num, out.begin() + n);
        }
        return out;
    }

    double maxStep (const std::vector<float>& x, size_t from, size_t to)
    {
        double m = 0.0;
        for (size_t i = std::max<size_t> (from, 1); i < std::min (to, x.size()); ++i)
            m = std::max (m, (double) std::abs (x[i] - x[i - 1]));
        return m;
    }
}

//==============================================================================
class FilterResponseTests final : public juce::UnitTest
{
public:
    FilterResponseTests() : juce::UnitTest ("Filter responses", "EQ") {}

    void near (double actual, double expected, double tol, const juce::String& what)
    {
        expectWithinAbsoluteError (actual, expected, tol,
                                   what + " (got " + juce::String (actual, 3) + ", expected " + juce::String (expected, 3) + ")");
    }

    // Measured engine output must match the analytic response the GUI draws.
    void matchesAnalytic (const std::vector<Spec>& specs, double fs, double f, double tol, const juce::String& what,
                          double settle = 0.5)
    {
        near (measureDb (specs, fs, f, settle), analyticDb (specs, fs, f), tol, what + " @" + juce::String (f) + " Hz, fs " + juce::String (fs));
    }

    void runTest() override
    {
        beginTest ("1 kHz bell +6 dB / -6 dB");
        for (double fs : sampleRates)
        {
            const std::vector<Spec> boost { { FilterType::Bell, 1000.0f, 6.0f, 1.0f } };
            const std::vector<Spec> cut   { { FilterType::Bell, 1000.0f, -6.0f, 1.0f } };
            near (measureDb (boost, fs, 1000.0), 6.0, 0.05, "bell +6 at centre, fs " + juce::String (fs));
            near (measureDb (cut,   fs, 1000.0), -6.0, 0.05, "bell -6 at centre, fs " + juce::String (fs));
            near (measureDb (boost, fs, 30.0), 0.0, 0.1, "bell +6 far below centre");
            near (measureDb (boost, fs, 15000.0), 0.0, 0.15, "bell +6 far above centre");
            for (double f : { 300.0, 700.0, 1400.0, 3000.0 })
                matchesAnalytic (boost, fs, f, 0.05, "bell +6 skirt");
        }

        beginTest ("Low shelf / high shelf");
        for (double fs : sampleRates)
        {
            const std::vector<Spec> ls { { FilterType::LowShelf, 200.0f, 6.0f, defaultQ } };
            const std::vector<Spec> hs { { FilterType::HighShelf, 5000.0f, -6.0f, defaultQ } };
            near (measureDb (ls, fs, 200.0), 3.0, 0.05, "low shelf half gain at corner");
            near (measureDb (ls, fs, 20.0), 6.0, 0.2, "low shelf full gain at 20 Hz");
            near (measureDb (ls, fs, 10000.0), 0.0, 0.05, "low shelf unity at 10 kHz");
            near (measureDb (hs, fs, 5000.0), -3.0, 0.05, "high shelf half gain at corner");
            near (measureDb (hs, fs, 50.0), 0.0, 0.05, "high shelf unity at 50 Hz");
            for (double f : { 50.0, 400.0, 2000.0, 12000.0 })
            {
                matchesAnalytic (ls, fs, f, 0.05, "low shelf");
                matchesAnalytic (hs, fs, f, 0.05, "high shelf");
            }
        }

        beginTest ("High-pass (low cut) / low-pass (high cut)");
        for (double fs : sampleRates)
        {
            const std::vector<Spec> hp { { FilterType::LowCut, 100.0f, 0.0f, defaultQ } };
            const std::vector<Spec> lp { { FilterType::HighCut, 1000.0f, 0.0f, defaultQ } };
            near (measureDb (hp, fs, 100.0), -3.01, 0.05, "HP -3 dB at cutoff");
            near (measureDb (hp, fs, 25.0), -24.1, 0.3, "HP 12 dB/oct, 2 octaves below");
            near (measureDb (hp, fs, 2000.0), 0.0, 0.05, "HP passband");
            near (measureDb (lp, fs, 1000.0), -3.01, 0.05, "LP -3 dB at cutoff");
            near (measureDb (lp, fs, 100.0), 0.0, 0.05, "LP passband");
            expectLessThan (measureDb (lp, fs, 4000.0), -23.0, "LP at least 12 dB/oct");
            for (double f : { 30.0, 200.0, 3000.0, 8000.0 })
            {
                matchesAnalytic (hp, fs, f, 0.05, "HP");
                matchesAnalytic (lp, fs, f, 0.05, "LP");
            }
        }

        beginTest ("Narrow notch");
        for (double fs : sampleRates)
        {
            const std::vector<Spec> notch { { FilterType::Notch, 1000.0f, 0.0f, 10.0f } };
            expectLessThan (measureDb (notch, fs, 1000.0, 1.0), -40.0, "notch depth at centre");
            near (measureDb (notch, fs, 500.0), 0.0, 0.1, "notch passband (octave below)");
            near (measureDb (notch, fs, 2000.0), 0.0, 0.1, "notch passband (octave above)");
            matchesAnalytic (notch, fs, 960.0, 0.05, "notch skirt", 1.0);
        }

        beginTest ("Multiple overlapping bands");
        for (double fs : sampleRates)
        {
            const std::vector<Spec> mix {
                { FilterType::Bell, 1000.0f, 6.0f, 1.0f },
                { FilterType::Bell, 1200.0f, -4.0f, 4.0f },
                { FilterType::Bell, 1000.0f, 6.0f, 1.0f },    // identical band stacked
                { FilterType::LowShelf, 200.0f, 3.0f, defaultQ },
                { FilterType::HighCut, 15000.0f, 0.0f, defaultQ },
            };
            for (double f : { 50.0, 200.0, 1000.0, 1200.0, 5000.0, 14000.0 })
                matchesAnalytic (mix, fs, f, 0.05, "overlapping bands");

            const std::vector<Spec> cancel { { FilterType::Bell, 1000.0f, 6.0f, 2.0f }, { FilterType::Bell, 1000.0f, -6.0f, 2.0f } };
            near (measureDb (cancel, fs, 1000.0), 0.0, 0.01, "+6 / -6 identical bells cancel");
        }

        beginTest ("Extreme Q, gain and frequency");
        for (double fs : sampleRates)
        {
            const std::vector<std::pair<std::vector<Spec>, double>> cases {
                { { { FilterType::Bell, 1000.0f, 30.0f, 40.0f } }, 4.0 },     // high-Q +30 dB
                { { { FilterType::Bell, 20.0f, -30.0f, 40.0f } }, 2.0 },      // high-Q cut at 20 Hz
                { { { FilterType::Bell, 20000.0f, 30.0f, 0.1f } }, 0.5 },     // very wide, near Nyquist
                { { { FilterType::Bell, 20000.0f, -30.0f, 40.0f } }, 0.5 },
                { { { FilterType::LowCut, 20.0f, 0.0f, 40.0f } }, 6.0 },      // resonant cut
                { { { FilterType::HighCut, 20000.0f, 0.0f, 0.1f } }, 0.5 },
                { { { FilterType::Notch, 20.0f, 0.0f, 40.0f } }, 6.0 },
            };

            for (auto& [specs, settle] : cases)
            {
                const auto& s = specs.front();
                const juce::String what = getTypeNames()[(int) s.type] + " f=" + juce::String (s.freq) + " q=" + juce::String (s.q)
                                        + " g=" + juce::String (s.gain);

                const double fc = std::min ((double) s.freq, fs * 0.45);
                for (double f : { fc * 0.8, fc, std::min (fc * 1.25, fs * 0.45) })
                {
                    const double measured = measureDb (specs, fs, f, settle);
                    expect (std::isfinite (measured), what + " finite");
                    if (analyticDb (specs, fs, f) > -60.0)   // skip deep-null points (noise floor)
                        near (measured, analyticDb (specs, fs, f), 0.1, what + " @" + juce::String (f, 1) + " fs " + juce::String (fs));
                }
            }
        }

        beginTest ("Neutral and bypassed bands are transparent");
        {
            Engine e;
            e.prepare (48000.0);
            configure (e, { { FilterType::Bell, 1000.0f, 12.0f, 1.0f, false },      // bypassed
                            { FilterType::LowCut, 500.0f, 0.0f, 1.0f, false } });  // bypassed
            e.snapToTargets();

            std::mt19937 rng (1);
            std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
            std::vector<float> l (4096), r (4096);
            for (auto& v : l) v = dist (rng);
            r = l;
            const auto ref = l;
            float* ch[] = { l.data(), r.data() };
            e.process (ch, 2, 4096);
            expect (l == ref && r == ref, "no active bands -> bit-identical output");

            Engine z;
            z.prepare (48000.0);
            configure (z, { { FilterType::Bell, 1000.0f, 0.0f, 1.0f } });
            z.snapToTargets();
            l = ref; r = ref;
            z.process (ch, 2, 4096);
            double maxErr = 0.0;
            for (size_t i = 0; i < l.size(); ++i)
                maxErr = std::max (maxErr, (double) std::abs (l[i] - ref[i]));
            expectLessThan (maxErr, 1.0e-6, "0 dB bell is transparent");
        }

        beginTest ("Left / right channels are independent; reset clears state");
        {
            Engine e;
            e.prepare (48000.0);
            configure (e, { { FilterType::Bell, 1000.0f, 12.0f, 4.0f }, { FilterType::LowCut, 50.0f, 0.0f, 2.0f } });
            e.snapToTargets();

            std::vector<float> l (8192), r (8192, 0.0f);
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = (float) std::sin (2.0 * pi * 1000.0 * (double) i / 48000.0);
            float* ch[] = { l.data(), r.data() };
            e.process (ch, 2, 8192);
            expect (std::all_of (r.begin(), r.end(), [] (float v) { return v == 0.0f; }), "silent right channel stays silent");

            e.reset();
            std::fill (l.begin(), l.end(), 0.0f);
            e.process (ch, 2, 8192);
            expect (std::all_of (l.begin(), l.end(), [] (float v) { return v == 0.0f; }), "after reset, silence in -> silence out");
        }
    }
};

//==============================================================================
class SmoothingTests final : public juce::UnitTest
{
public:
    SmoothingTests() : juce::UnitTest ("Smoothing and robustness", "EQ") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("Gain jump is smoothed (no discontinuity)");
        {
            Engine e;
            e.prepare (fs);
            float gain = 0.0f;
            auto out = runSine (e, fs, 100.0, 0.5, 0.6, 64, [&] (int, int n)
            {
                if (n >= (int) (0.2 * fs)) gain = 12.0f;   // instant +12 dB request
                BandSettings s { true, FilterType::Bell, 100.0f, gain, 1.0f };
                e.setBand (0, s);
            });
            const double steady = maxStep (out, (size_t) (0.45 * fs), out.size());
            const double during = maxStep (out, (size_t) (0.19 * fs), (size_t) (0.3 * fs));
            // A ramping amplitude adds A'/(A*w) ~ 6% to the slope; an unsmoothed coefficient
            // jump here would produce a step of ~1.5 (about 60x the steady-state step).
            expectLessThan (during, steady * 1.15, "max sample step during the change ~ final steady-state step");
        }

        beginTest ("Frequency and Q sweeps are smooth");
        {
            Engine e;
            e.prepare (fs);
            auto out = runSine (e, fs, 1000.0, 0.25, 1.0, 32, [&] (int bi, int)
            {
                // Fast automation: sweep the bell across the tone and modulate Q.
                const float t = (float) bi / 1500.0f;
                BandSettings s { true, FilterType::Bell, 200.0f * std::pow (25.0f, t), 12.0f, 0.5f + 7.5f * std::abs (std::sin (t * 6.0f)) };
                e.setBand (0, s);
            });
            const double bound = 0.25 * std::pow (10.0, 12.0 / 20.0) * 2.0 * pi * 1000.0 / fs;
            expectLessThan (maxStep (out, 1, out.size()), bound * 1.5, "step bounded by max-gain sine slope");
        }

        beginTest ("Filter type change cross-fades");
        {
            Engine e;
            e.prepare (fs);
            auto out = runSine (e, fs, 1000.0, 0.25, 0.6, 64, [&] (int, int n)
            {
                const bool after = n >= (int) (0.3 * fs);
                BandSettings s { true, after ? FilterType::HighCut : FilterType::Bell, after ? 200.0f : 1000.0f, 12.0f, 1.0f };
                e.setBand (0, s);
            });
            const double before = maxStep (out, (size_t) (0.1 * fs), (size_t) (0.29 * fs));
            const double during = maxStep (out, (size_t) (0.29 * fs), (size_t) (0.4 * fs));
            expectLessThan (during, before * 1.05, "no jump bigger than the loud steady state");
        }

        beginTest ("Enable / bypass fades");
        {
            Engine e;
            e.prepare (fs);
            auto out = runSine (e, fs, 1000.0, 0.25, 0.8, 64, [&] (int, int n)
            {
                const bool on = n >= (int) (0.2 * fs) && n < (int) (0.5 * fs);
                BandSettings s { on, FilterType::Bell, 1000.0f, 12.0f, 1.0f };
                e.setBand (0, s);
            });
            const double loud = maxStep (out, (size_t) (0.35 * fs), (size_t) (0.48 * fs));
            expectLessThan (maxStep (out, 1, out.size()), loud * 1.05, "toggling never steps more than the boosted signal");
        }

        beginTest ("Rapid random parameter changes stay finite and recover");
        for (double rate : sampleRates)
        {
            Engine e;
            e.prepare (rate);
            std::mt19937 rng (42);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);

            const int block = 32;
            std::vector<float> l (block), r (block);
            float* ch[] = { l.data(), r.data() };
            bool finite = true;
            float peak = 0.0f;

            for (int bi = 0; bi < (int) (2.0 * rate / block); ++bi)
            {
                for (int b = 0; b < 8; ++b)
                {
                    BandSettings s;
                    s.active = u (rng) > 0.2f;
                    s.type = typeFromIndex ((int) (u (rng) * numFilterTypes));
                    s.freq = minFreq * std::pow (maxFreq / minFreq, u (rng));
                    s.gain = minGain + (maxGain - minGain) * u (rng);
                    s.q = minQ * std::pow (maxQ / minQ, u (rng));
                    e.setBand (b, s);
                }
                for (int i = 0; i < block; ++i)
                    l[(size_t) i] = r[(size_t) i] = 0.25f * (u (rng) * 2.0f - 1.0f);
                e.process (ch, 2, block);
                for (int i = 0; i < block; ++i)
                {
                    finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
                    peak = std::max (peak, std::abs (l[(size_t) i]));
                }
            }
            expect (finite, "all output finite at fs " + juce::String (rate));
            expectLessThan (peak, 1.0e4f, "output bounded at fs " + juce::String (rate));

            // Return to neutral + silence: everything must decay away.
            for (int b = 0; b < 8; ++b)
                e.setBand (b, BandSettings { true, FilterType::Bell, 1000.0f, 0.0f, 1.0f });
            float tail = 0.0f;
            for (int bi = 0; bi < (int) (1.0 * rate / block); ++bi)
            {
                std::fill (l.begin(), l.end(), 0.0f);
                std::fill (r.begin(), r.end(), 0.0f);
                e.process (ch, 2, block);
                if (bi > (int) (0.8 * rate / block))
                    for (float v : l) tail = std::max (tail, std::abs (v));
            }
            expectLessThan (tail, 1.0e-6f, "decays to silence at fs " + juce::String (rate));
        }
    }
};

//==============================================================================
class ProcessorTests final : public juce::UnitTest
{
public:
    ProcessorTests() : juce::UnitTest ("Processor, state and undo", "EQ") {}

    static void set (ThreeBandEQAudioProcessor& p, int band, Param param, float value)
    {
        auto* rp = p.getBandParam (band, param);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    void runTest() override
    {
        beginTest ("Parameter identities");
        {
            ThreeBandEQAudioProcessor p;
            constexpr int numGlobalParams = 4;   // output_gain, bypass, auto_gain, gain_scale
            expectEquals (p.getParameters().size(), numBands * numBandParams + numGlobalParams, "24 bands x per-band parameters + globals");

            juce::StringArray ids;
            for (auto* param : p.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                    ids.add (rp->paramID);
            ids.removeDuplicates (false);
            expectEquals (ids.size(), numBands * numBandParams + numGlobalParams, "IDs are unique");

            expect (p.apvts.getParameter ("output_gain") != nullptr, "output_gain present");
            expect (p.apvts.getParameter ("bypass") != nullptr, "bypass present");
            expect (p.getBypassParameter() == p.apvts.getParameter ("bypass"), "bypass is the host bypass parameter");

            for (const char* legacy : { "band1_freq", "band1_gain", "band1_q", "band2_freq", "band2_gain",
                                        "band2_q", "band3_freq", "band3_gain", "band3_q" })
                expect (p.apvts.getParameter (legacy) != nullptr, juce::String ("legacy ID kept: ") + legacy);

            expect (p.apvts.getParameter ("band24_type") != nullptr && p.apvts.getParameter ("band24_on") != nullptr,
                    "band 24 type/on present");
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), 100.0f, 0.01f, "band 1 default 100 Hz");
            expect (p.isBandUsed (0) && p.isBandUsed (2) && ! p.isBandUsed (3), "bands 1-3 exist by default");
        }

        beginTest ("Save / restore round trip");
        {
            ThreeBandEQAudioProcessor a;
            set (a, 4, Param::Type, (float) FilterType::HighShelf);
            set (a, 4, Param::Freq, 3000.0f);
            set (a, 4, Param::Gain, -7.5f);
            set (a, 4, Param::Q, 2.5f);
            set (a, 4, Param::Enabled, 0.0f);
            a.setBandUsed (4, true);
            set (a, 23, Param::Type, (float) FilterType::LowCut);
            set (a, 23, Param::Freq, 40.0f);
            a.setBandUsed (23, true);
            a.setBandUsed (1, false);
            a.selectedBand = 4;
            a.displayRangeDb = 12.0f;
            a.analyser.mode = (int) SpectrumAnalyser::Mode::Pre;
            a.editorWidth = 1234;
            a.editorHeight = 777;

            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            ThreeBandEQAudioProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());

            bool same = true;
            for (int band = 0; band < numBands; ++band)
            {
                same = same && a.isBandUsed (band) == b.isBandUsed (band);
                for (int prm = 0; prm < numBandParams; ++prm)
                    same = same && std::abs (a.getBandValue (band, (Param) prm) - b.getBandValue (band, (Param) prm)) < 1.0e-4f;
            }
            expect (same, "all 24 bands (values, types, enabled, used) restored");
            expectEquals (b.selectedBand.load(), 4, "selected band restored");
            expectEquals (b.displayRangeDb.load(), 12.0f, "display range restored");
            expectEquals (b.analyser.mode.load(), (int) SpectrumAnalyser::Mode::Pre, "analyser mode restored");
            expectEquals (b.editorWidth.load(), 1234, "editor width restored");
            expectEquals (b.editorHeight.load(), 777, "editor height restored");

            // Automation stays on the right band: band 5's params keep their IDs and values.
            expectWithinAbsoluteError (b.getBandValue (4, Param::Freq), 3000.0f, 0.1f, "band5_freq still band 5");
            expect (b.apvts.getParameter ("band5_freq") == b.getBandParam (4, Param::Freq), "ID -> band mapping");
        }

        beginTest ("Adding / removing a band notifies the host of a state change");
        {
            struct Listener final : juce::AudioProcessorListener
            {
                int nonParamChanges = 0;
                void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
                void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& d) override
                {
                    nonParamChanges += d.nonParameterStateChanged ? 1 : 0;
                }
            } listener;

            ThreeBandEQAudioProcessor p;
            p.addListener (&listener);
            p.edits.removeBand (0);
            expectGreaterThan (listener.nonParamChanges, 0, "remove band flags state change");
            const int afterRemove = listener.nonParamChanges;
            p.edits.undo();
            expectGreaterThan (listener.nonParamChanges, afterRemove, "undo remove flags state change");
            p.removeListener (&listener);
        }

        beginTest ("Legacy v0.1 session loads with identical sound");
        {
            const char* legacyXml =
                R"(<PARAMETERS><PARAM id="band1_freq" value="250.0"/><PARAM id="band1_gain" value="-4.0"/>)"
                R"(<PARAM id="band1_q" value="2.0"/><PARAM id="band2_freq" value="1500.0"/><PARAM id="band2_gain" value="3.0"/>)"
                R"(<PARAM id="band2_q" value="0.707"/><PARAM id="band3_freq" value="9000.0"/><PARAM id="band3_gain" value="1.5"/>)"
                R"(<PARAM id="band3_q" value="0.5"/></PARAMETERS>)";

            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*juce::parseXML (legacyXml), mb);

            ThreeBandEQAudioProcessor p;
            set (p, 9, Param::Gain, 9.0f);   // must be reset to default by the restore
            p.setBandUsed (9, true);
            p.setStateInformation (mb.getData(), (int) mb.getSize());

            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), 250.0f, 0.01f, "band 1 freq");
            expectWithinAbsoluteError (p.getBandValue (0, Param::Gain), -4.0f, 0.01f, "band 1 gain");
            expectWithinAbsoluteError (p.getBandValue (2, Param::Q), 0.5f, 0.01f, "band 3 Q");
            expect (p.isBandUsed (0) && p.isBandUsed (1) && p.isBandUsed (2), "bands 1-3 in use");
            expect (! p.isBandUsed (9), "other bands unused");
            expectWithinAbsoluteError (p.getBandValue (9, Param::Gain), 0.0f, 0.001f, "missing params reset to default");
            expectEquals ((int) p.getBandValue (1, Param::Type), (int) FilterType::Bell, "legacy bands are bells");
            expectEquals (p.getBandValue (1, Param::Enabled), 1.0f, "legacy bands enabled");
            expectWithinAbsoluteError (p.apvts.getRawParameterValue ("output_gain")->load(), 0.0f, 1.0e-5f, "legacy: 0 dB output gain");
            expectEquals (p.apvts.getRawParameterValue ("bypass")->load(), 0.0f, "legacy: bypass off");
        }

        beginTest ("Multiple instances are independent");
        {
            ThreeBandEQAudioProcessor a, b;
            set (a, 0, Param::Gain, 6.0f);
            expectWithinAbsoluteError (b.getBandValue (0, Param::Gain), 0.0f, 1.0e-5f);   // interval snapping may leave ~1e-7 (FMA on Apple clang)
        }

        beginTest ("Undo / redo");
        {
            ThreeBandEQAudioProcessor p;
            auto& edits = p.edits;

            const int band = edits.addBand (500.0f, 6.0f, FilterType::Bell);
            expectEquals (band, 3, "first free slot is band 4");
            expect (p.isBandUsed (3));
            edits.undo();
            expect (! p.isBandUsed (3), "undo add");
            edits.redo();
            expect (p.isBandUsed (3), "redo add");
            expectWithinAbsoluteError (p.getBandValue (3, Param::Gain), 6.0f, 0.01f);

            // A drag with 200 intermediate values is one undo step.
            const float startFreq = p.getBandValue (0, Param::Freq);
            edits.beginEdit (0, "Move band");
            for (int i = 0; i < 200; ++i)
            {
                edits.setParam (Param::Freq, 100.0f + (float) i * 10.0f);
                edits.setParam (Param::Gain, (float) i * 0.05f);
            }
            edits.endEdit();
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), 2090.0f, 0.1f);
            edits.undo();
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), startFreq, 0.01f, "one undo reverts whole drag");
            expectWithinAbsoluteError (p.getBandValue (0, Param::Gain), 0.0f, 0.01f);
            expect (p.isBandUsed (3), "previous step (add band) not undone by the same undo");

            edits.setType (0, FilterType::Notch);
            edits.resetBand (3);
            edits.removeBand (3);
            expect (! p.isBandUsed (3));
            edits.undo();
            expect (p.isBandUsed (3), "undo remove");
            edits.undo();
            expectWithinAbsoluteError (p.getBandValue (3, Param::Gain), 6.0f, 0.01f, "undo reset");
            edits.undo();
            expectEquals ((int) p.getBandValue (0, Param::Type), (int) FilterType::Bell, "undo type change");

            // Host automation must not create undo steps.
            const int before = p.edits.getUndoManager().getNumActionsInCurrentTransaction();
            set (p, 0, Param::Gain, 3.0f);
            expectEquals (p.edits.getUndoManager().getNumActionsInCurrentTransaction(), before, "automation not in history");
        }

        beginTest ("Silence and very low-level input (processor)");
        {
            ThreeBandEQAudioProcessor p;
            p.setRateAndBufferSizeDetails (48000.0, 256);
            p.prepareToPlay (48000.0, 256);
            for (int b = 0; b < numBands; ++b)
            {
                p.setBandUsed (b, true);
                set (p, b, Param::Type, (float) (b % numFilterTypes));
                set (p, b, Param::Freq, 30.0f * std::pow (600.0f, (float) b / numBands));
                set (p, b, Param::Gain, (b % 2) ? 12.0f : -12.0f);
                set (p, b, Param::Q, 4.0f);
            }

            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            bool silent = true;
            for (int i = 0; i < 200; ++i)
            {
                buf.clear();
                p.processBlock (buf, midi);
                silent = silent && buf.getMagnitude (0, 256) == 0.0f;
            }
            expect (silent, "digital silence in -> digital silence out (24 bands active)");

            bool ok = true;
            for (int i = 0; i < 400; ++i)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < 256; ++n)
                        buf.setSample (ch, n, 1.0e-36f * (float) std::sin (0.01 * (i * 256 + n)));   // near-denormal
                p.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < 256; ++n)
                    {
                        const float v = buf.getSample (ch, n);
                        ok = ok && std::isfinite (v) && std::fpclassify (v) != FP_SUBNORMAL;
                    }
            }
            expect (ok, "tiny input: finite output, no subnormals");
        }
    }
};

//==============================================================================
class AnalyserTests final : public juce::UnitTest
{
public:
    AnalyserTests() : juce::UnitTest ("Spectrum analyser", "EQ") {}

    static void feedSine (SpectrumAnalyser& a, double fs, double f, float amp, int samples)
    {
        std::vector<float> l (512);
        const float* ch[] = { l.data(), l.data() };
        for (int n = 0; n < samples; n += 512)
        {
            for (int i = 0; i < 512; ++i)
                l[(size_t) i] = amp * (float) std::sin (2.0 * pi * f * (n + i) / fs);
            a.push (ch, 2, 512);
        }
    }

    static int peakBin (const SpectrumAnalyser& a)
    {
        int best = 1;
        for (int b = 1; b < SpectrumAnalyser::numBins; ++b)
            if (a.getBinDb (b) > a.getBinDb (best))
                best = b;
        return best;
    }

    void runTest() override
    {
        constexpr double fs = 48000.0;
        const double binHz = fs / SpectrumAnalyser::fftSize;

        beginTest ("Level calibration (0 dBFS and -20 dBFS sine on a bin centre)");
        for (float ampDb : { 0.0f, -20.0f, -60.0f })
        {
            SpectrumAnalyser a;
            const double f = 171 * binHz;   // ~1002 Hz, exactly on bin 171
            feedSine (a, fs, f, juce::Decibels::decibelsToGain (ampDb), SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            expectEquals (peakBin (a), 171, "peak in the right bin");
            expectWithinAbsoluteError (a.getBinDb (171), ampDb, 0.05f, "level in dBFS");
        }

        beginTest ("Frequency accuracy and off-bin level (Hann scalloping <= 1.5 dB)");
        for (double f : { 50.0, 440.0, 3333.0, 15000.0 })
        {
            SpectrumAnalyser a;
            feedSine (a, fs, f, 1.0f, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            expectLessOrEqual (std::abs (peakBin (a) * binHz - f), binHz, "peak within one bin of " + juce::String (f));
            expectGreaterThan (a.getLevelDb (f * 0.98, f * 1.02, fs), -1.5f, "level at " + juce::String (f));
        }

        beginTest ("Decay after the signal stops");
        {
            SpectrumAnalyser a;
            a.decayDbPerSecond = 30.0f;
            feedSine (a, fs, 1000.0, 1.0f, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            const float start = a.getLevelDb (990.0, 1010.0, fs);
            a.update (1.5);   // 0.5 s with no new audio
            expectWithinAbsoluteError (a.getLevelDb (990.0, 1010.0, fs), start - 15.0f, 0.5f, "30 dB/s decay");
        }

        beginTest ("Processor captures pre/post only while a viewer is open");
        {
            ThreeBandEQAudioProcessor p;
            p.setRateAndBufferSizeDetails (fs, 512);
            p.prepareToPlay (fs, 512);
            // Band 1: deep notch at 1 kHz, so pre and post differ at 1 kHz.
            auto set = [&] (Param prm, float v) { auto* rp = p.getBandParam (0, prm); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
            set (Param::Type, (float) FilterType::Notch);
            set (Param::Freq, 1000.0f);
            set (Param::Q, 2.0f);

            auto run = [&] (SpectrumAnalyser::Mode mode, int viewers)
            {
                p.analyser.mode = (int) mode;
                p.analyser.activeViewers = viewers;
                p.analyser.clear();
                juce::AudioBuffer<float> buf (2, 512);
                juce::MidiBuffer midi;
                for (int n = 0; n < 48000; n += 512)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 512; ++i)
                            buf.setSample (ch, i, 0.5f * (float) std::sin (2.0 * pi * 1000.0 * (n + i) / fs));
                    p.processBlock (buf, midi);
                    p.analyser.update (1.0 + n / fs);
                }
                return p.analyser.getLevelDb (990.0, 1010.0, fs);
            };

            const float pre  = run (SpectrumAnalyser::Mode::Pre, 1);
            const float post = run (SpectrumAnalyser::Mode::Post, 1);
            const float none = run (SpectrumAnalyser::Mode::Post, 0);
            expectWithinAbsoluteError (pre, -6.0f, 1.5f, "pre-EQ sees the -6 dBFS input");
            expectLessThan (post, pre - 30.0f, "post-EQ sees the notch");
            expectLessThan (none, -100.0f, "no capture without a viewer");
        }
    }
};

//==============================================================================
// Shared helpers for processor-level tests.
namespace
{
    void setParam (ThreeBandEQAudioProcessor& p, const juce::String& id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    void setBand (ThreeBandEQAudioProcessor& p, int band, Param prm, float value)
    {
        auto* rp = p.getBandParam (band, prm);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    // Streams a signal through the processor in fixed blocks; 'onBlock' may change
    // parameters before each block. Returns the left output and fills 'inputOut'.
    std::vector<float> runProcessor (ThreeBandEQAudioProcessor& p, double fs, int block, int total,
                                     const std::function<float (int n)>& signal,
                                     const std::function<void (int sample)>& onBlock,
                                     std::vector<float>* rightOut = nullptr)
    {
        p.setRateAndBufferSizeDetails (fs, block);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        std::vector<float> out ((size_t) total);
        if (rightOut != nullptr)
            rightOut->assign ((size_t) total, 0.0f);

        for (int n = 0; n < total; n += block)
        {
            const int num = std::min (block, total - n);
            onBlock (n);
            buf.setSize (2, num, false, false, true);
            for (int i = 0; i < num; ++i)
            {
                const float v = signal (n + i);
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < num; ++i)
            {
                out[(size_t) (n + i)] = buf.getSample (0, i);
                if (rightOut != nullptr)
                    (*rightOut)[(size_t) (n + i)] = buf.getSample (1, i);
            }
        }
        return out;
    }

    double rms (const std::vector<float>& x, size_t from, size_t to)
    {
        double s = 0.0;
        for (size_t i = from; i < to; ++i)
            s += (double) x[i] * x[i];
        return std::sqrt (s / (double) std::max<size_t> (1, to - from));
    }
}

class OutputStageTests final : public juce::UnitTest
{
public:
    OutputStageTests() : juce::UnitTest ("Output gain and bypass", "EQ") {}

    void runTest() override
    {
        constexpr double fs = 48000.0;
        constexpr int block = 256;
        auto sine = [] (double f, double amp) { return [=] (int n) { return (float) (amp * std::sin (2.0 * pi * f * n / fs)); }; };

        beginTest ("Output gain accuracy (-12, 0, +12 dB)");
        for (float db : { -12.0f, 0.0f, 12.0f })
        {
            ThreeBandEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b)
                p.setBandUsed (b, false);
            setParam (p, "output_gain", db);
            p.prepareToPlay (fs, block);

            const int total = (int) (0.5 * fs);
            const auto out = runProcessor (p, fs, block, total, sine (997.0, 0.25), [] (int) {});
            std::vector<float> in ((size_t) total);
            for (int n = 0; n < total; ++n)
                in[(size_t) n] = sine (997.0, 0.25) (n);

            const double measured = 20.0 * std::log10 (rms (out, (size_t) total / 2, (size_t) total) / rms (in, (size_t) total / 2, (size_t) total));
            expectWithinAbsoluteError (measured, (double) db, 0.01, "output gain " + juce::String (db) + " dB");
        }

        beginTest ("Output gain jump is smoothed");
        {
            ThreeBandEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b)
                p.setBandUsed (b, false);
            p.prepareToPlay (fs, block);

            const int total = (int) (0.6 * fs);
            const auto out = runProcessor (p, fs, block, total, sine (100.0, 0.2), [&] (int n)
            {
                if (n >= (int) (0.2 * fs) && n < (int) (0.2 * fs) + block)
                    setParam (p, "output_gain", 12.0f);
            });

            const double steady = maxStep (out, (size_t) (0.45 * fs), out.size());
            const double during = maxStep (out, (size_t) (0.19 * fs), (size_t) (0.3 * fs));
            // An unsmoothed 4x jump would step by up to 0.6 in one sample (~100x the steady step).
            // Multiplicative smoothing over 20 ms adds ln(4)/(0.02 * 2 pi 100) ~ 11 % to the slope.
            expectLessThan (during, steady * 1.2, "max step during the gain change ~ final steady-state step");
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
        }

        beginTest ("Bypass cross-fades to bit-exact dry output");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 0, Param::Gain, 12.0f);   // band 1: +12 dB bell at 100 Hz -> clearly processed
            setBand (p, 1, Param::Gain, -9.0f);
            setParam (p, "output_gain", 6.0f);
            p.prepareToPlay (fs, block);

            std::mt19937 rng (3);
            std::uniform_real_distribution<float> dist (-0.3f, 0.3f);
            const int total = (int) (0.6 * fs);
            std::vector<float> in ((size_t) total);
            for (auto& v : in) v = dist (rng);

            const int bypassAt = (int) (0.2 * fs) / block * block;
            std::vector<float> right;
            const auto out = runProcessor (p, fs, block, total, [&] (int n) { return in[(size_t) n]; }, [&] (int n)
            {
                if (n == bypassAt)
                    setParam (p, "bypass", 1.0f);
            }, &right);

            double maxErr = 0.0;
            const auto settled = (size_t) (bypassAt + (int) (0.03 * fs));
            for (size_t i = settled; i < out.size(); ++i)
                maxErr = std::max ({ maxErr, (double) std::abs (out[i] - in[i]), (double) std::abs (right[i] - in[i]) });
            expectLessThan (maxErr, 1.0e-6, "bypassed output equals the input once the fade has finished");

            double before = 0.0;
            for (size_t i = (size_t) (0.1 * fs); i < (size_t) bypassAt; ++i)
                before = std::max (before, (double) std::abs (out[i] - in[i]));
            expectGreaterThan (before, 0.05, "processing was audible before bypass");
        }

        beginTest ("Bypass on/off transition is smooth");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 1, Param::Gain, 12.0f);   // +12 dB at 1 kHz
            setBand (p, 1, Param::Q, 1.0f);
            p.prepareToPlay (fs, block);

            const int total = (int) (0.8 * fs);
            const auto out = runProcessor (p, fs, block, total, sine (1000.0, 0.1), [&] (int n)
            {
                if (n == (int) (0.2 * fs) / block * block) setParam (p, "bypass", 1.0f);
                if (n == (int) (0.5 * fs) / block * block) setParam (p, "bypass", 0.0f);
            });

            const double loud = maxStep (out, (size_t) (0.1 * fs), (size_t) (0.19 * fs));
            expectLessThan (maxStep (out, 1, out.size()), loud * 1.05, "no step bigger than the processed signal's");

            const double quiet = rms (out, (size_t) (0.35 * fs), (size_t) (0.45 * fs));
            expectWithinAbsoluteError (20.0 * std::log10 (quiet / (0.1 / std::sqrt (2.0))), 0.0, 0.01, "bypassed level = input level");
            const double back = rms (out, (size_t) (0.7 * fs), (size_t) (0.8 * fs));
            expectWithinAbsoluteError (20.0 * std::log10 (back / (0.1 / std::sqrt (2.0))), 12.0, 0.1, "un-bypassed: processing returns");
        }

        beginTest ("Output meter sees peaks and clipping");
        {
            ThreeBandEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b)
                p.setBandUsed (b, false);
            setParam (p, "output_gain", 6.0f);
            p.prepareToPlay (fs, block);

            runProcessor (p, fs, block, (int) (0.1 * fs), sine (500.0, 0.25), [] (int) {});
            expectWithinAbsoluteError (p.outputMeter.readAndReset(), 0.25f * juce::Decibels::decibelsToGain (6.0f), 0.01f, "peak");
            expect (! p.outputMeter.clipped.load(), "no clip at -6 dBFS");
            expectEquals (p.outputMeter.readAndReset(), 0.0f, "read resets the peak");

            runProcessor (p, fs, block, (int) (0.1 * fs), sine (500.0, 0.9), [] (int) {});
            expect (p.outputMeter.clipped.load(), "clip latched above 0 dBFS");
        }

        beginTest ("Output gain and bypass are saved with the session");
        {
            ThreeBandEQAudioProcessor a;
            setParam (a, "output_gain", -7.5f);
            setParam (a, "bypass", 1.0f);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            ThreeBandEQAudioProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (b.apvts.getRawParameterValue ("output_gain")->load(), -7.5f, 0.001f, "output gain restored");
            expectEquals (b.apvts.getRawParameterValue ("bypass")->load(), 1.0f, "bypass restored");

            // A v2 session (saved before these parameters existed) loads with neutral values,
            // even into an instance where they were changed.
            auto state = b.apvts.copyState();
            state.setProperty ("version", 2, nullptr);
            state.removeChild (state.getChildWithProperty ("id", "output_gain"), nullptr);
            state.removeChild (state.getChildWithProperty ("id", "bypass"), nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), old);
            b.setStateInformation (old.getData(), (int) old.getSize());
            expectWithinAbsoluteError (b.apvts.getRawParameterValue ("output_gain")->load(), 0.0f, 1.0e-5f, "old session: 0 dB");
            expectEquals (b.apvts.getRawParameterValue ("bypass")->load(), 0.0f, "old session: bypass off");
        }
    }
};

//==============================================================================
class SlopeTests final : public juce::UnitTest
{
public:
    SlopeTests() : juce::UnitTest ("Cut slopes", "EQ") {}

    void runTest() override
    {
        beginTest ("Butterworth section Qs");
        {
            double qs[maxSvfSections] {};
            expectEquals (butterworthQs (2, qs), 1);
            expectWithinAbsoluteError (qs[0], 0.70711, 1.0e-4);
            expectEquals (butterworthQs (4, qs), 2);
            expectWithinAbsoluteError (qs[0], 0.54120, 1.0e-4);
            expectWithinAbsoluteError (qs[1], 1.30656, 1.0e-4);
            expectEquals (butterworthQs (3, qs), 1);
            expectWithinAbsoluteError (qs[0], 1.0, 1.0e-6);
            expectEquals (butterworthQs (8, qs), 4);
            expectWithinAbsoluteError (qs[3], 2.56292, 1.0e-4);
        }

        beginTest ("Slope steepness, -3 dB at cutoff (44.1 / 96 / 192 kHz)");
        for (double fs : { 44100.0, 96000.0, 192000.0 })
            for (int slope = 0; slope < numSlopes; ++slope)
            {
                const int n = slopeOrder (slope);
                const juce::String what = getSlopeNames()[slope] + " fs " + juce::String (fs);

                // Low cut at 400 Hz: 1 and 2 octaves below.
                const std::vector<Spec> hp { { FilterType::LowCut, 400.0f, 0.0f, defaultQ, true, slope } };
                const double hp0 = measureDb (hp, fs, 400.0), hp1 = measureDb (hp, fs, 200.0), hp2 = measureDb (hp, fs, 100.0);
                expectWithinAbsoluteError (hp0, -3.01, 0.05, "low cut -3 dB at cutoff, " + what);
                expectWithinAbsoluteError (hp1 - hp2, 6.0 * n, 1.0, "low cut dB/oct between 1 and 2 octaves, " + what);
                expectWithinAbsoluteError (-hp2 / 2.0, 6.0 * n, 1.0, "low cut average dB/oct over 2 octaves, " + what);
                expectWithinAbsoluteError (hp1, analyticDb (hp, fs, 200.0), 0.05, "low cut DSP = drawn curve, " + what);
                expectWithinAbsoluteError (measureDb (hp, fs, 3000.0), analyticDb (hp, fs, 3000.0), 0.05, "low cut passband, " + what);
                expectGreaterThan (analyticDb (hp, fs, 3000.0), -0.1, "low cut passband near 0 dB, " + what);

                // High cut at 500 Hz: 1 and 2 octaves above.
                const std::vector<Spec> lp { { FilterType::HighCut, 500.0f, 0.0f, defaultQ, true, slope } };
                const double lp0 = measureDb (lp, fs, 500.0), lp1 = measureDb (lp, fs, 1000.0), lp2 = measureDb (lp, fs, 2000.0);
                expectWithinAbsoluteError (lp0, -3.01, 0.05, "high cut -3 dB at cutoff, " + what);
                expectWithinAbsoluteError (lp1 - lp2, 6.0 * n, 1.0, "high cut dB/oct between 1 and 2 octaves, " + what);
                expectWithinAbsoluteError (-lp2 / 2.0, 6.0 * n, 1.0, "high cut average dB/oct over 2 octaves, " + what);
                expectWithinAbsoluteError (lp1, analyticDb (lp, fs, 1000.0), 0.05, "high cut DSP = drawn curve, " + what);
                expectWithinAbsoluteError (measureDb (lp, fs, 60.0), analyticDb (lp, fs, 60.0), 0.05, "high cut passband, " + what);
                expectGreaterThan (analyticDb (lp, fs, 60.0), -0.1, "high cut passband near 0 dB, " + what);
            }

        beginTest ("Q adds resonance on steep slopes; slope ignored for other types");
        {
            constexpr double fs = 48000.0;
            const std::vector<Spec> flat     { { FilterType::LowCut, 400.0f, 0.0f, defaultQ, true, 3 } };
            const std::vector<Spec> resonant { { FilterType::LowCut, 400.0f, 0.0f, 3.0f, true, 3 } };
            expectGreaterThan (measureDb (resonant, fs, 420.0), measureDb (flat, fs, 420.0) + 6.0, "Q 3 peaks near the cutoff");
            expectLessThan (measureDb (resonant, fs, 100.0), -40.0, "still 24 dB/oct below");

            const std::vector<Spec> bell12 { { FilterType::Bell, 1000.0f, 6.0f, 1.0f, true, 1 } };
            const std::vector<Spec> bell48 { { FilterType::Bell, 1000.0f, 6.0f, 1.0f, true, 5 } };
            expectEquals (measureDb (bell48, fs, 700.0), measureDb (bell12, fs, 700.0), "bell ignores the slope parameter");
        }

        beginTest ("12 dB/oct is bit-identical to the single-SVF path");
        for (auto type : { FilterType::LowCut, FilterType::HighCut })
        {
            constexpr double fs = 48000.0;
            Engine e;
            e.prepare (fs);
            configure (e, { { type, 300.0f, 0.0f, 1.3f, true, defaultSlope } });
            e.snapToTargets();

            // The engine smooths in the log domain, so it sees exp(log(x)), not x.
            const auto c = makeCoeffs (type, fs, std::exp (std::log (300.0)), std::exp (std::log ((double) 1.3f)), 0.0);
            SvfState st;
            std::mt19937 rng (5);
            std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
            std::vector<float> l (4096), r (4096);
            for (auto& v : l) v = dist (rng);
            r = l;
            std::vector<float> ref (l.size());
            for (size_t i = 0; i < l.size(); ++i)
                ref[i] = (float) processSample (c, st, (double) l[i]);

            float* ch[] = { l.data(), r.data() };
            e.process (ch, 2, (int) l.size());
            expect (l == ref, "identical samples for " + getTypeNames()[(int) type]);
        }

        beginTest ("Slope change while running: finite, no discontinuity");
        for (double fs : { 44100.0, 192000.0 })
        {
            Engine e;
            e.prepare (fs);
            int slope = 1;
            auto out = runSine (e, fs, 300.0, 0.25, 0.8, 64, [&] (int, int n)
            {
                if (n >= (int) (0.3 * fs)) slope = 5;
                if (n >= (int) (0.5 * fs)) slope = 0;
                e.setBand (0, BandSettings { true, FilterType::LowCut, 250.0f, 0.0f, defaultQ, slope });
            });
            // Each slope has a different steady level at 300 Hz; transitions must not step
            // further than the loudest of them.
            const double steady = std::max ({ maxStep (out, (size_t) (0.1 * fs), (size_t) (0.29 * fs)),
                                              maxStep (out, (size_t) (0.4 * fs), (size_t) (0.49 * fs)),
                                              maxStep (out, (size_t) (0.65 * fs), out.size()) });
            expectLessThan (maxStep (out, (size_t) (0.29 * fs), out.size()), steady * 1.05,
                            "no step bigger than the steady state, fs " + juce::String (fs));

            // Rapid slope automation, every 2 ms, across every slope and both cut types.
            std::mt19937 rng (9);
            bool finite = true;
            float peak = 0.0f;
            const int block = (int) (0.002 * fs);
            auto noisy = runSine (e, fs, 180.0, 0.25, 1.0, block, [&] (int bi, int)
            {
                e.setBand (0, BandSettings { true, (bi % 7) < 4 ? FilterType::LowCut : FilterType::HighCut,
                                             100.0f + (float) (rng() % 4000), 0.0f, 0.3f + (float) (rng() % 100) * 0.1f,
                                             (int) (rng() % numSlopes) });
            });
            for (float v : noisy)
            {
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
            }
            expect (finite, "finite under slope automation");
            expectLessThan (peak, 20.0f, "bounded under slope automation");
        }

        beginTest ("Legacy session loads at 12 dB/oct");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 0, Param::Slope, 5.0f);   // must be reset by the restore
            auto state = p.apvts.copyState();
            state.setProperty ("version", 2, nullptr);
            for (int b = 0; b < numBands; ++b)
                state.removeChild (state.getChildWithProperty ("id", paramId (b, Param::Slope)), nullptr);
            state.getChildWithProperty ("id", "band1_type").setProperty ("value", (int) FilterType::LowCut, nullptr);

            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());

            expectEquals ((int) p.getBandValue (0, Param::Type), (int) FilterType::LowCut);
            bool all12 = true;
            for (int b = 0; b < numBands; ++b)
                all12 = all12 && (int) p.getBandValue (b, Param::Slope) == defaultSlope;
            expect (all12, "every band at 12 dB/oct");
        }

        beginTest ("Slope edits are undoable and stored in the session");
        {
            ThreeBandEQAudioProcessor p;
            p.edits.setType (0, FilterType::HighCut);
            p.edits.setSlope (0, 4);
            expectEquals ((int) p.getBandValue (0, Param::Slope), 4);
            p.edits.undo();
            expectEquals ((int) p.getBandValue (0, Param::Slope), defaultSlope, "undo slope");
            p.edits.redo();

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals ((int) q.getBandValue (0, Param::Slope), 4, "slope restored");
        }
    }
};

//==============================================================================
class SoloTests final : public juce::UnitTest
{
public:
    SoloTests() : juce::UnitTest ("Band solo", "EQ") {}

    static constexpr double fs = 48000.0;
    static constexpr int block = 256;

    // Steady-state gain (dB) of the processor for a sine at f, with band 'solo' soloed.
    static double soloGainDb (FilterType type, float freq, float q, float gain, double f, int solo = 1)
    {
        ThreeBandEQAudioProcessor p;
        setBand (p, 1, Param::Type, (float) type);
        setBand (p, 1, Param::Freq, freq);
        setBand (p, 1, Param::Q, q);
        setBand (p, 1, Param::Gain, gain);
        p.soloBand = solo;
        p.prepareToPlay (fs, block);

        const int total = (int) (0.6 * fs);
        auto sig = [f] (int n) { return (float) (0.25 * std::sin (2.0 * pi * f * n / fs)); };
        const auto out = runProcessor (p, fs, block, total, sig, [] (int) {});
        std::vector<float> in ((size_t) total);
        for (int n = 0; n < total; ++n) in[(size_t) n] = sig (n);
        return 20.0 * std::log10 (rms (out, (size_t) (0.3 * fs), out.size()) / rms (in, (size_t) (0.3 * fs), in.size()));
    }

    void runTest() override
    {
        beginTest ("Soloed bell: unity at the band frequency, >= 18 dB down two octaves away");
        for (float q : { 0.1f, 0.707f, 2.0f, 8.0f })
            for (float gain : { 0.0f, 9.0f, -15.0f })
            {
                const juce::String what = "Q " + juce::String (q) + ", gain " + juce::String (gain);
                expectWithinAbsoluteError (soloGainDb (FilterType::Bell, 1000.0f, q, gain, 1000.0), 0.0, 0.5, "centre, " + what);
                expectLessThan (soloGainDb (FilterType::Bell, 1000.0f, q, gain, 250.0), -18.0, "two octaves below, " + what);
                expectLessThan (soloGainDb (FilterType::Bell, 1000.0f, q, gain, 4000.0), -18.0, "two octaves above, " + what);
            }

        beginTest ("Soloed shelves and cuts play the region they affect");
        {
            expectGreaterThan (soloGainDb (FilterType::LowShelf, 500.0f, 0.707f, 6.0f, 100.0), -1.0, "low shelf: lows pass");
            expectLessThan (soloGainDb (FilterType::LowShelf, 500.0f, 0.707f, 6.0f, 2000.0), -18.0, "low shelf: highs removed");
            expectGreaterThan (soloGainDb (FilterType::LowCut, 500.0f, 0.707f, 0.0f, 100.0), -1.0, "low cut: hear what is cut");
            expectGreaterThan (soloGainDb (FilterType::HighShelf, 4000.0f, 0.707f, 6.0f, 12000.0), -1.0, "high shelf: highs pass");
            expectLessThan (soloGainDb (FilterType::HighCut, 4000.0f, 0.707f, 0.0f, 1000.0), -18.0, "high cut: lows removed");
        }

        beginTest ("Releasing solo returns bit-identical normal processing");
        {
            ThreeBandEQAudioProcessor a, b;
            for (auto* p : { &a, &b })
            {
                setBand (*p, 0, Param::Gain, 6.0f);
                setBand (*p, 2, Param::Type, (float) FilterType::HighCut);
                p->prepareToPlay (fs, block);
            }

            std::mt19937 rng (11);
            std::uniform_real_distribution<float> dist (-0.4f, 0.4f);
            const int total = (int) (1.0 * fs);
            std::vector<float> in ((size_t) total);
            for (auto& v : in) v = dist (rng);

            const int on = (int) (0.2 * fs) / block * block, off = (int) (0.5 * fs) / block * block;
            const auto outA = runProcessor (a, fs, block, total, [&] (int n) { return in[(size_t) n]; }, [&] (int n)
            {
                if (n == on)  a.soloBand = 0;
                if (n == off) a.soloBand = -1;
            });
            const auto outB = runProcessor (b, fs, block, total, [&] (int n) { return in[(size_t) n]; }, [] (int) {});

            bool differed = false;
            for (int n = on + 2000; n < off; ++n)
                differed = differed || outA[(size_t) n] != outB[(size_t) n];
            expect (differed, "solo changed the output");

            bool identical = true;
            for (size_t n = (size_t) (off + (int) (0.03 * fs)); n < outA.size(); ++n)
                identical = identical && outA[n] == outB[n];
            expect (identical, "bit-identical once the fade-out has finished");

            expectLessThan (maxStep (outA, (size_t) on - 10, (size_t) on + 2000), 0.85, "fade in bounded");
        }

        beginTest ("Solo fades smoothly, also when switching bands");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 0, Param::Freq, 300.0f);
            setBand (p, 1, Param::Freq, 600.0f);
            p.prepareToPlay (fs, block);

            const int total = (int) (1.2 * fs);
            const auto out = runProcessor (p, fs, block, total, [] (int n) { return (float) (0.25 * std::sin (2.0 * pi * 440.0 * n / fs)); },
                                           [&] (int n)
            {
                if (n == 12288) p.soloBand = 0;
                if (n == 24576) p.soloBand = 1;
                if (n == 36864) p.soloBand = 0;
                if (n == 37120) p.soloBand = 2;   // rapid switching while fading
                if (n == 49152) p.soloBand = -1;
            });

            const double bound = 0.25 * 2.0 * pi * 440.0 / fs;   // the input sine's maximum step
            expectLessThan (maxStep (out, 1, out.size()), bound * 1.3, "no clicks when soloing / switching / releasing");
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
        }

        beginTest ("Solo is transient: no undo entry, not saved, cleared when the editor closes");
        {
            ThreeBandEQAudioProcessor p;
            p.soloBand = 1;
            expect (! p.edits.getUndoManager().canUndo(), "no undo history");

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            expect (! juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize()).containsIgnoreCase ("solo"), "not in the state");

            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.soloBand.load(), -1, "a restored session is never soloed");

            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            p.soloBand = 2;
            editor.reset();
            expectEquals (p.soloBand.load(), -1, "closing the editor clears solo");
        }
    }
};

//==============================================================================
class PresetTests final : public juce::UnitTest
{
public:
    PresetTests() : juce::UnitTest ("Presets", "EQ") {}

    struct TempFolder
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("TWProEQ-tests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        TempFolder()  { dir.createDirectory(); }
        ~TempFolder() { dir.deleteRecursively(); }
    };

    static std::vector<float> allValues (ThreeBandEQAudioProcessor& p)
    {
        std::vector<float> v;
        for (int b = 0; b < numBands; ++b)
        {
            v.push_back (p.isBandUsed (b) ? 1.0f : 0.0f);
            for (int prm = 0; prm < numBandParams; ++prm)
                v.push_back (p.getBandValue (b, (Param) prm));
        }
        v.push_back (p.apvts.getRawParameterValue ("output_gain")->load());
        return v;
    }

    void runTest() override
    {
        beginTest ("Save then load round-trips every parameter exactly");
        {
            TempFolder tmp;
            ThreeBandEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);

            std::mt19937 rng (21);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);
            for (int b = 0; b < numBands; ++b)
            {
                p.setBandUsed (b, u (rng) > 0.4f);
                for (int prm = 0; prm < numBandParams; ++prm)
                    p.getBandParam (b, (Param) prm)->setValueNotifyingHost (u (rng));
            }
            p.getOutputGainParam()->setValueNotifyingHost (0.3f);
            const auto saved = allValues (p);

            juce::String error;
            expect (p.presets.saveAs ("Round trip", error), error);
            expect (tmp.dir.getChildFile ("Round trip.twpreset").existsAsFile(), "file written");

            // Scramble, then load.
            for (int b = 0; b < numBands; ++b)
            {
                p.setBandUsed (b, b % 3 == 0);
                for (int prm = 0; prm < numBandParams; ++prm)
                    p.getBandParam (b, (Param) prm)->setValueNotifyingHost (u (rng));
            }
            p.getOutputGainParam()->setValueNotifyingHost (0.9f);

            const auto user = p.presets.getUserPresets();
            expectEquals ((int) user.size(), 1);
            expect (p.presets.load (user.front(), error), error);
            expect (allValues (p) == saved, "every band parameter, used flag and the output gain restored exactly");
            expect (! p.presets.isModified(), "not modified straight after loading");

            set (p, 0, Param::Gain, 3.3f);
            expect (p.presets.isModified() || ! p.isBandUsed (0), "modified marker after an edit");
        }

        beginTest ("A preset holds no UI state and no bypass");
        {
            TempFolder tmp;
            ThreeBandEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            p.editorWidth = 1500;
            p.getBypassParameter()->setValueNotifyingHost (1.0f);
            juce::String error;
            expect (p.presets.saveAs ("UI", error));
            const auto text = tmp.dir.getChildFile ("UI.twpreset").loadFileAsString();
            expect (! text.contains ("editorWidth") && ! text.contains ("analyserMode"), "no UI state");
            expect (! text.contains ("\"bypass\""), "no bypass");
            expect (text.contains ("version"), "versioned");
        }

        beginTest ("Loading a preset is one undo transaction");
        {
            ThreeBandEQAudioProcessor p;
            const auto before = allValues (p);
            juce::String error;
            expect (p.presets.load ({ "Kick punch", 5, {} }, error));
            expect (allValues (p) != before, "preset changed the EQ");
            expect (p.edits.undo(), "undo");
            {
                const auto after = allValues (p);
                juce::String diffs;
                for (size_t k = 0; k < after.size(); ++k)
                    if (std::abs (after[k] - before[k]) > std::abs (before[k]) * 1.0e-5f)   // ulp-level drift through the normalised range
                        diffs << (int) k << ":" << before[k] << "->" << after[k] << " ";
                expect (diffs.isEmpty(), "one undo restores everything " + diffs);
            }
            expect (! p.edits.getUndoManager().canUndo(), "it was a single transaction");
            expect (p.edits.redo());
            expect (! p.presets.isModified(), "redo brings the preset back");
        }

        beginTest ("Every factory preset loads, is finite and has the expected bands");
        {
            expectGreaterOrEqual (PresetManager::getNumFactoryPresets(), 6);
            expectLessOrEqual (PresetManager::getNumFactoryPresets(), 8);

            for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
            {
                ThreeBandEQAudioProcessor p;
                juce::String error;
                expect (p.presets.load ({ PresetManager::getFactoryName (i), i, {} }, error), error);
                expectEquals (p.getNumUsedBands(), PresetManager::getFactoryUsedBands (i), PresetManager::getFactoryName (i));
                expect (! p.presets.isModified(), "factory preset not modified after load");

                p.prepareToPlay (48000.0, 256);
                std::mt19937 rng ((unsigned) i);
                std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
                bool finite = true;
                const auto out = runProcessor (p, 48000.0, 256, 48000, [&] (int) { return dist (rng); }, [] (int) {});
                for (float v : out)
                    finite = finite && std::isfinite (v) && std::abs (v) < 10.0f;
                expect (finite, "finite output: " + PresetManager::getFactoryName (i));
            }
        }

        beginTest ("Presets written by older versions migrate");
        {
            TempFolder tmp;
            const char* v1 =
                R"(<PARAMETERS><PARAM id="band1_freq" value="250.0"/><PARAM id="band1_gain" value="-4.0"/>)"
                R"(<PARAM id="band1_q" value="2.0"/><PARAM id="band2_freq" value="1500.0"/><PARAM id="band2_gain" value="3.0"/>)"
                R"(<PARAM id="band3_freq" value="9000.0"/><PARAM id="band3_gain" value="1.5"/></PARAMETERS>)";
            tmp.dir.getChildFile ("Old.twpreset").replaceWithText (v1);

            ThreeBandEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            p.setBandUsed (7, true);
            set (p, 7, Param::Gain, 5.0f);
            set (p, 0, Param::Slope, 4.0f);

            juce::String error;
            expect (p.presets.load (p.presets.getUserPresets().front(), error), error);
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), 250.0f, 0.01f, "band 1 freq");
            expectWithinAbsoluteError (p.getBandValue (1, Param::Gain), 3.0f, 0.01f, "band 2 gain");
            expect (p.isBandUsed (0) && p.isBandUsed (1) && p.isBandUsed (2) && ! p.isBandUsed (7), "v1: bands 1-3 used");
            expectEquals ((int) p.getBandValue (0, Param::Slope), defaultSlope, "missing slope -> default");
            expectWithinAbsoluteError (p.getBandValue (7, Param::Gain), 0.0f, 1.0e-5f, "missing params -> defaults");
        }

        beginTest ("Corrupt or foreign files are rejected and change nothing");
        {
            TempFolder tmp;
            tmp.dir.getChildFile ("Garbage.twpreset").replaceWithText ("this is <not xml");
            tmp.dir.getChildFile ("Foreign.twpreset").replaceWithText ("<SOMETHING><PARAM id=\"band1_gain\" value=\"12\"/></SOMETHING>");
            juce::FileOutputStream (tmp.dir.getChildFile ("Empty.twpreset")).flush();

            ThreeBandEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            set (p, 1, Param::Gain, 4.0f);
            const auto before = allValues (p);

            for (auto& e : p.presets.getUserPresets())
            {
                juce::String error;
                expect (! p.presets.load (e, error), "rejected: " + e.name);
                expect (error.isNotEmpty(), "with a message: " + e.name);
                expect (allValues (p) == before, "state unchanged: " + e.name);
            }

            juce::String error;
            expect (! p.presets.load ({ "Gone", -1, tmp.dir.getChildFile ("Missing.twpreset") }, error), "missing file rejected");
            expect (! p.edits.getUndoManager().canUndo(), "no undo entries from failed loads");
        }

        beginTest ("Current preset survives saving and restoring the session");
        {
            ThreeBandEQAudioProcessor p;
            juce::String error;
            expect (p.presets.load ({ PresetManager::getFactoryName (3), 3, {} }, error));
            juce::MemoryBlock mb;
            p.getStateInformation (mb);

            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.presets.hasCurrent(), "preset name restored");
            expectEquals (q.presets.getCurrent().name, PresetManager::getFactoryName (3));
            expect (! q.presets.isModified(), "and not marked modified");
        }
    }

    static void set (ThreeBandEQAudioProcessor& p, int band, Param prm, float v) { setBand (p, band, prm, v); }
};

//==============================================================================
class StereoModeTests final : public juce::UnitTest
{
public:
    StereoModeTests() : juce::UnitTest ("Stereo modes", "EQ") {}

    static constexpr double fs = 48000.0;

    // Runs separate L/R signals through an engine configured with 'bands'.
    static void run (Engine& e, std::vector<float>& l, std::vector<float>& r, int block = 64)
    {
        for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
        {
            const int num = (int) std::min ((size_t) block, l.size() - pos);
            float* ch[] = { l.data() + pos, r.data() + pos };
            e.process (ch, 2, num);
        }
    }

    static Engine makeEngine (const std::vector<BandSettings>& bands)
    {
        Engine e;
        e.prepare (fs);
        for (int b = 0; b < numBands; ++b)
            e.setBand (b, b < (int) bands.size() ? bands[(size_t) b] : BandSettings {});
        e.snapToTargets();
        return e;
    }

    static std::vector<float> sine (double f, double amp, size_t n, double phase = 0.0)
    {
        std::vector<float> x (n);
        for (size_t i = 0; i < n; ++i)
            x[i] = (float) (amp * std::sin (2.0 * pi * f * (double) i / fs + phase));
        return x;
    }

    static double gainDb (const std::vector<float>& out, const std::vector<float>& in)
    {
        const size_t from = out.size() / 2;
        return 20.0 * std::log10 (rms (out, from, out.size()) / rms (in, from, in.size()));
    }

    void runTest() override
    {
        const size_t n = (size_t) fs;   // 1 s

        beginTest ("Mid-only +6 dB bell boosts mono-compatible (L=R) content on both channels");
        {
            auto e = makeEngine ({ { true, FilterType::Bell, 1000.0f, 6.0f, 1.0f, defaultSlope, StereoMode::Mid } });
            auto in = sine (1000.0, 0.25, n);
            auto l = in, r = in;
            run (e, l, r);
            expectWithinAbsoluteError (gainDb (l, in), 6.0, 0.05, "left +6 dB");
            expectWithinAbsoluteError (gainDb (r, in), 6.0, 0.05, "right +6 dB");
        }

        beginTest ("Mid-only band leaves Side content alone");
        {
            auto e = makeEngine ({ { true, FilterType::Bell, 1000.0f, 6.0f, 1.0f, defaultSlope, StereoMode::Mid } });
            auto in = sine (1000.0, 0.25, n);
            auto l = in, r = in;
            for (auto& v : r) v = -v;   // pure Side (L = -R)
            const auto inR = r;
            run (e, l, r);
            expectWithinAbsoluteError (gainDb (l, in), 0.0, 0.01, "side-only signal unchanged (L)");
            expectWithinAbsoluteError (gainDb (r, inR), 0.0, 0.01, "side-only signal unchanged (R)");
        }

        beginTest ("Side-only band has no effect on L=R input");
        {
            auto e = makeEngine ({ { true, FilterType::Bell, 1000.0f, 12.0f, 1.0f, defaultSlope, StereoMode::Side } });
            std::mt19937 rng (4);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            std::vector<float> in (n);
            for (auto& v : in) v = d (rng);
            auto l = in, r = in;
            run (e, l, r);
            double err = 0.0;
            for (size_t i = 0; i < n; ++i)
                err = std::max ({ err, (double) std::abs (l[i] - in[i]), (double) std::abs (r[i] - in[i]) });
            expectLessThan (err, 1.0e-6, "unchanged");

            // And it does act on side content.
            auto l2 = sine (1000.0, 0.25, n), r2 = l2;
            for (auto& v : r2) v = -v;
            const auto in2 = l2;
            run (e, l2, r2);
            expectWithinAbsoluteError (gainDb (l2, in2), 12.0, 0.1, "side content boosted");
        }

        beginTest ("Left-only band leaves the right channel bit-exact");
        {
            auto e = makeEngine ({ { true, FilterType::Bell, 1000.0f, 9.0f, 2.0f, defaultSlope, StereoMode::Left },
                                   { true, FilterType::LowCut, 200.0f, 0.0f, defaultQ, 3, StereoMode::Left } });
            std::mt19937 rng (8);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            std::vector<float> l (n), r (n);
            for (auto& v : l) v = d (rng);
            for (auto& v : r) v = d (rng);
            const auto inL = l, inR = r;
            run (e, l, r);
            expect (r == inR, "right untouched");
            expect (l != inL, "left processed");

            auto e2 = makeEngine ({ { true, FilterType::Bell, 1000.0f, 9.0f, 2.0f, defaultSlope, StereoMode::Right } });
            l = inL; r = inR;
            run (e2, l, r);
            expect (l == inL, "Right-only band leaves left untouched");
        }

        beginTest ("All bands in Stereo mode: identical to the plain cascade");
        {
            const std::vector<BandSettings> bands { { true, FilterType::Bell, 300.0f, 4.0f, 1.2f },
                                                    { true, FilterType::HighShelf, 6000.0f, -3.0f, defaultQ },
                                                    { true, FilterType::LowCut, 40.0f, 0.0f, defaultQ } };
            auto e = makeEngine (bands);

            std::vector<SvfCoeffs> c;
            for (auto& s : bands)
                c.push_back (makeCoeffs (s.type, fs, std::exp (std::log ((double) s.freq)), std::exp (std::log ((double) s.q)), s.gain));
            std::vector<SvfState> sl (c.size()), sr (c.size());

            std::mt19937 rng (9);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            std::vector<float> l (8192), r (8192);
            for (auto& v : l) v = d (rng);
            for (auto& v : r) v = d (rng);
            auto refL = l, refR = r;
            for (size_t i = 0; i < l.size(); ++i)
                for (size_t k = 0; k < c.size(); ++k)
                {
                    refL[i] = (float) processSample (c[k], sl[k], (double) refL[i]);
                    refR[i] = (float) processSample (c[k], sr[k], (double) refR[i]);
                }

            run (e, l, r);
            expect (l == refL && r == refR, "bit-identical");
        }

        beginTest ("Mode changes while running: finite, no spikes");
        for (double rate : { 44100.0, 96000.0 })
        {
            Engine e;
            e.prepare (rate);
            const int block = 64;
            const size_t total = (size_t) (1.5 * rate);
            std::vector<float> l (total), r (total);
            for (size_t i = 0; i < total; ++i)
            {
                l[i] = (float) (0.25 * std::sin (2.0 * pi * 1000.0 * (double) i / rate));
                r[i] = (float) (0.25 * std::sin (2.0 * pi * 1000.0 * (double) i / rate + 1.0));
            }

            int bi = 0;
            for (size_t pos = 0; pos < total; pos += (size_t) block, ++bi)
            {
                // Change band 1's mode every 50 ms, band 2's every 7 blocks (faster than the fades).
                e.setBand (0, { true, FilterType::Bell, 1000.0f, 12.0f, 1.0f, defaultSlope, stereoModeFromIndex ((int) (pos / (size_t) (0.05 * rate)) % numStereoModes) });
                e.setBand (1, { true, FilterType::Bell, 1500.0f, -6.0f, 2.0f, defaultSlope, stereoModeFromIndex ((bi / 7) % numStereoModes) });
                const int num = (int) std::min ((size_t) block, total - pos);
                float* ch[] = { l.data() + pos, r.data() + pos };
                e.process (ch, 2, num);
            }

            bool finite = true;
            for (size_t i = 0; i < total; ++i)
                finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]);
            expect (finite, "finite");

            // Worst case: both bands at full boost on a channel, 1 kHz sine.
            const double bound = 0.25 * 4.0 * 2.0 * pi * 1000.0 / rate;
            expectLessThan (std::max (maxStep (l, 1, total), maxStep (r, 1, total)), bound * 1.1, "no step beyond the loudest steady state");
        }

        beginTest ("Mono bus treats every mode as Stereo");
        {
            auto e = makeEngine ({ { true, FilterType::Bell, 1000.0f, 6.0f, 1.0f, defaultSlope, StereoMode::Side } });
            auto in = sine (1000.0, 0.25, n);
            auto x = in;
            for (size_t pos = 0; pos < n; pos += 64)
            {
                float* ch[] = { x.data() + pos };
                e.process (ch, 1, (int) std::min ((size_t) 64, n - pos));
            }
            expectWithinAbsoluteError (gainDb (x, in), 6.0, 0.05, "mono: Side band acts like Stereo");
        }

        beginTest ("Legacy session loads with every band in Stereo; mode is undoable and saved");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 0, Param::Stereo, 3.0f);
            auto state = p.apvts.copyState();
            state.setProperty ("version", 2, nullptr);
            for (int b = 0; b < numBands; ++b)
                state.removeChild (state.getChildWithProperty ("id", paramId (b, Param::Stereo)), nullptr);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());

            bool allStereo = true;
            for (int b = 0; b < numBands; ++b)
                allStereo = allStereo && (int) p.getBandValue (b, Param::Stereo) == 0;
            expect (allStereo, "every band Stereo");

            p.edits.setStereoMode (1, (int) StereoMode::Side);
            expectEquals ((int) p.getBandValue (1, Param::Stereo), (int) StereoMode::Side);
            p.edits.undo();
            expectEquals ((int) p.getBandValue (1, Param::Stereo), 0, "undo");
            p.edits.redo();

            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (saved.getData(), (int) saved.getSize());
            expectEquals ((int) q.getBandValue (1, Param::Stereo), (int) StereoMode::Side, "restored");
        }
    }
};

//==============================================================================
class SidechainTests final : public juce::UnitTest
{
public:
    SidechainTests() : juce::UnitTest ("Sidechain analyser", "EQ") {}

    using Set = juce::AudioChannelSet;

    static juce::AudioProcessor::BusesLayout layout (Set in, Set out, std::optional<Set> sidechain)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        if (sidechain.has_value())
            l.inputBuses.add (*sidechain);
        l.outputBuses.add (out);
        return l;
    }

    void runTest() override
    {
        constexpr double fs = 48000.0;
        constexpr int block = 512;

        beginTest ("Default layout is a plain stereo effect (sidechain disabled)");
        {
            ThreeBandEQAudioProcessor p;
            expectEquals (p.getTotalNumInputChannels(), 2);
            expectEquals (p.getTotalNumOutputChannels(), 2);
            expect (p.getBus (true, 1) != nullptr && ! p.getBus (true, 1)->isEnabled(), "sidechain bus exists, disabled");
            expect (! p.isSidechainConnected());
        }

        beginTest ("Bus layouts accepted / rejected");
        {
            ThreeBandEQAudioProcessor p;
            const auto st = Set::stereo(), mo = Set::mono(), off = Set::disabled();
            expect (p.checkBusesLayoutSupported (layout (st, st, off)), "stereo, sidechain off");
            expect (p.checkBusesLayoutSupported (layout (st, st, mo)), "stereo, mono sidechain");
            expect (p.checkBusesLayoutSupported (layout (st, st, st)), "stereo, stereo sidechain");
            expect (p.checkBusesLayoutSupported (layout (mo, mo, st)), "mono, stereo sidechain");
            expect (p.checkBusesLayoutSupported (layout (mo, mo, off)), "mono, sidechain off");
            expect (p.isBusesLayoutSupported (layout (st, st, std::nullopt)), "host without the sidechain bus");
            expect (! p.checkBusesLayoutSupported (layout (st, st, Set::create5point1())), "5.1 sidechain rejected");
            expect (! p.checkBusesLayoutSupported (layout (st, mo, off)), "stereo in / mono out rejected");
            expect (! p.checkBusesLayoutSupported (layout (Set::create5point1(), Set::create5point1(), off)), "5.1 main rejected");
        }

        beginTest ("The sidechain never reaches the output (bit-exact versus no sidechain)");
        {
            ThreeBandEQAudioProcessor withSc, without;
            expect (withSc.setBusesLayout (layout (Set::stereo(), Set::stereo(), Set::stereo())), "sidechain layout set");
            expect (withSc.isSidechainConnected());

            for (auto* p : { &withSc, &without })
            {
                setBand (*p, 0, Param::Gain, 6.0f);
                setBand (*p, 2, Param::Type, (float) FilterType::LowCut);
                p->setRateAndBufferSizeDetails (fs, block);
                p->prepareToPlay (fs, block);
                p->analyser.activeViewers = 1;
                p->sidechainView = 2;
            }

            std::mt19937 rng (12);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            juce::AudioBuffer<float> a (4, block), b (2, block);
            juce::MidiBuffer midi;
            bool identical = true;

            for (int i = 0; i < 100; ++i)
            {
                for (int n = 0; n < block; ++n)
                {
                    const float l = d (rng), r = d (rng);
                    a.setSample (0, n, l);  a.setSample (1, n, r);
                    b.setSample (0, n, l);  b.setSample (1, n, r);
                    a.setSample (2, n, 0.9f * std::sin ((float) n * 0.3f));   // loud sidechain
                    a.setSample (3, n, d (rng));
                }
                withSc.processBlock (a, midi);
                without.processBlock (b, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < block; ++n)
                        identical = identical && a.getSample (ch, n) == b.getSample (ch, n);
            }
            expect (identical, "main output identical");
            expectGreaterThan (withSc.sidechainMeter.readAndReset(), 0.5f, "sidechain level seen");
        }

        beginTest ("Sidechain analyser level and frequency calibration");
        for (float ampDb : { 0.0f, -20.0f })
        {
            ThreeBandEQAudioProcessor p;
            p.setBusesLayout (layout (Set::stereo(), Set::stereo(), Set::stereo()));
            p.setRateAndBufferSizeDetails (fs, block);
            p.prepareToPlay (fs, block);
            p.analyser.activeViewers = 1;
            p.analyser.mode = (int) SpectrumAnalyser::Mode::Post;
            p.sidechainView = 1;

            const double binHz = fs / SpectrumAnalyser::fftSize;
            const double f = 171 * binHz;
            const float amp = juce::Decibels::decibelsToGain (ampDb);
            juce::AudioBuffer<float> buf (4, block);
            juce::MidiBuffer midi;

            for (int pos = 0; pos < SpectrumAnalyser::fftSize * 2; pos += block)
            {
                buf.clear();   // silent main input
                for (int n = 0; n < block; ++n)
                {
                    const float v = amp * (float) std::sin (2.0 * pi * f * (pos + n) / fs);
                    buf.setSample (2, n, v);
                    buf.setSample (3, n, v);
                }
                p.processBlock (buf, midi);
            }

            p.sidechainAnalyser.update (1.0);
            p.analyser.update (1.0);

            int best = 1;
            for (int bin = 1; bin < SpectrumAnalyser::numBins; ++bin)
                if (p.sidechainAnalyser.getBinDb (bin) > p.sidechainAnalyser.getBinDb (best))
                    best = bin;
            expectEquals (best, 171, "peak in the right bin");
            expectWithinAbsoluteError (p.sidechainAnalyser.getBinDb (171), ampDb, 0.05f, "level in dBFS");
            expectLessThan (p.analyser.getLevelDb (f * 0.98, f * 1.02, fs), -100.0f, "main analyser doesn't see the sidechain");
        }

        beginTest ("No sidechain capture without a viewer; the setting is saved");
        {
            ThreeBandEQAudioProcessor p;
            p.setBusesLayout (layout (Set::stereo(), Set::stereo(), Set::stereo()));
            p.prepareToPlay (fs, block);
            p.sidechainView = 1;
            p.analyser.activeViewers = 0;
            juce::AudioBuffer<float> buf (4, block);
            juce::MidiBuffer midi;
            for (int i = 0; i < 40; ++i)
            {
                for (int n = 0; n < block; ++n)
                    buf.setSample (2, n, 0.5f * std::sin ((float) n * 0.1f));
                p.processBlock (buf, midi);
            }
            p.sidechainAnalyser.update (1.0);
            expectLessThan (p.sidechainAnalyser.getBinDb (100), -100.0f, "nothing captured");

            p.sidechainView = 2;
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.sidechainView.load(), 2, "sidechain view restored");
        }
    }
};

//==============================================================================
class AnalyserControlTests final : public juce::UnitTest
{
public:
    AnalyserControlTests() : juce::UnitTest ("Analyser controls", "EQ") {}

    static constexpr double fs = 48000.0;

    static void feed (SpectrumAnalyser& a, const std::function<float (int)>& signal, int samples, int offset = 0)
    {
        std::vector<float> l (512);
        const float* ch[] = { l.data() };
        for (int n = 0; n < samples; n += 512)
        {
            for (int i = 0; i < 512; ++i)
                l[(size_t) i] = signal (offset + n + i);
            a.push (ch, 1, 512);
        }
    }

    // Mean displayed level (dB, with tilt) over the bins of one octave starting at f.
    static double octaveMeanDb (const SpectrumAnalyser& a, double f, float tilt)
    {
        const double binHz = fs / a.getFftSize();
        double sum = 0.0;
        int count = 0;
        for (int b = (int) std::ceil (f / binHz); b * binHz < 2.0 * f; ++b, ++count)
            sum += a.getBinDb (b) + SpectrumAnalyser::tiltDb (b * binHz, b * binHz, tilt);
        return sum / count;
    }

    void runTest() override
    {
        beginTest ("Calibration and frequency accuracy at every FFT size");
        for (int order = SpectrumAnalyser::minFftOrder; order <= SpectrumAnalyser::fftOrder; ++order)
        {
            const int size = 1 << order;
            const double binHz = fs / size;
            const juce::String what = juce::String (size) + "-point";

            for (float ampDb : { 0.0f, -20.0f })
            {
                SpectrumAnalyser a;
                a.setFftOrder (order);
                expectEquals (a.getFftSize(), size);
                const int bin = (int) std::round (1000.0 / binHz);
                const double f = bin * binHz;
                const float amp = juce::Decibels::decibelsToGain (ampDb);
                feed (a, [&] (int n) { return amp * (float) std::sin (2.0 * pi * f * n / fs); }, SpectrumAnalyser::fftSize * 2);
                a.update (1.0);

                int best = 1;
                for (int b = 1; b < a.getNumBins(); ++b)
                    if (a.getBinDb (b) > a.getBinDb (best))
                        best = b;
                expectEquals (best, bin, "peak bin, " + what);
                expectWithinAbsoluteError (a.getBinDb (bin), ampDb, 0.05f, "level, " + what);
            }

            for (double f : { 100.0, 3333.0, 15000.0 })
            {
                SpectrumAnalyser a;
                a.setFftOrder (order);
                feed (a, [&] (int n) { return (float) std::sin (2.0 * pi * f * n / fs); }, SpectrumAnalyser::fftSize * 2);
                a.update (1.0);
                expectGreaterThan (a.getLevelDb (f - binHz, f + binHz, fs), -1.5f, "off-bin level at " + juce::String (f) + ", " + what);
            }
        }

        beginTest ("Changing the FFT size keeps the analyser consistent");
        {
            SpectrumAnalyser a;
            feed (a, [] (int n) { return 0.5f * (float) std::sin (2.0 * pi * 1000.0 * n / fs); }, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            a.setFftOrder (11);
            a.update (1.1);   // no new audio: recomputed from the kept history
            expectWithinAbsoluteError (a.getLevelDb (990.0, 1010.0, fs), -6.0f, 1.6f, "level right after switching");
        }

        beginTest ("Tilt: white noise shows the selected slope (+-0.5 dB/oct)");
        for (float tilt : { 0.0f, 3.0f, 4.5f, 6.0f })
        {
            SpectrumAnalyser a;
            a.decayDbPerSecond = 0.0f;
            std::mt19937 rng (77);
            std::normal_distribution<float> d (0.0f, 0.1f);
            feed (a, [&] (int) { return d (rng); }, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);

            const double slope = (octaveMeanDb (a, 4000.0, tilt) - octaveMeanDb (a, 250.0, tilt)) / 4.0;
            expectWithinAbsoluteError (slope, (double) tilt, 0.5, "tilt " + juce::String (tilt));
        }

        beginTest ("Peak hold keeps the maximum after the signal drops; reset clears it");
        {
            SpectrumAnalyser a;
            a.setPeakHold (true);
            feed (a, [] (int n) { return 0.5f * (float) std::sin (2.0 * pi * 1000.0 * n / fs); }, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            const float peak = a.getPeakLevelDb (990.0, 1010.0, fs);
            expectWithinAbsoluteError (peak, -6.0f, 1.6f, "peak captured");

            feed (a, [] (int) { return 0.0f; }, SpectrumAnalyser::fftSize * 2);
            a.update (1.4);
            a.update (2.0);
            expectLessThan (a.getLevelDb (990.0, 1010.0, fs), peak - 20.0f, "live spectrum fell");
            expectEquals (a.getPeakLevelDb (990.0, 1010.0, fs), peak, "peak held");

            a.resetPeaks();
            expectLessThan (a.getPeakLevelDb (990.0, 1010.0, fs), -100.0f, "reset");
        }

        beginTest ("Freeze keeps the data unchanged while new audio arrives");
        {
            SpectrumAnalyser a;
            feed (a, [] (int n) { return 0.5f * (float) std::sin (2.0 * pi * 1000.0 * n / fs); }, SpectrumAnalyser::fftSize * 2);
            a.update (1.0);
            std::vector<float> before;
            for (int b = 0; b < a.getNumBins(); ++b) before.push_back (a.getBinDb (b));

            a.setFrozen (true);
            feed (a, [] (int n) { return 0.9f * (float) std::sin (2.0 * pi * 5000.0 * n / fs); }, SpectrumAnalyser::fftSize * 2);
            expect (! a.update (1.5), "no change reported");
            a.update (3.0);
            bool same = true;
            for (int b = 0; b < a.getNumBins(); ++b) same = same && a.getBinDb (b) == before[(size_t) b];
            expect (same, "frozen spectrum unchanged (no new data, no decay)");

            a.setFrozen (false);
            a.update (3.1);
            expectGreaterThan (a.getLevelDb (4990.0, 5010.0, fs), -3.0f, "unfreezing shows the current signal");
        }

        beginTest ("Speed sets the release rate");
        {
            for (int s = 0; s < 3; ++s)
            {
                SpectrumAnalyser a;
                a.setSpeed (s);
                expectEquals (a.decayDbPerSecond, SpectrumAnalyser::speedDecayDbPerSecond[s]);
            }
            expectLessThan (SpectrumAnalyser::speedDecayDbPerSecond[0], SpectrumAnalyser::speedDecayDbPerSecond[2], "slow < fast");
        }

        beginTest ("Settings survive saving and restoring");
        {
            ThreeBandEQAudioProcessor p;
            p.analyserTilt = 3.0f;
            p.analyserTopDb = -12.0f;
            p.analyserBottomDb = -120.0f;
            p.setAnalyserFftOrder (12);
            p.setAnalyserSpeed (2);
            p.analyser.setPeakHold (true);

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.analyserTilt.load(), 3.0f, "tilt");
            expectEquals (q.analyserTopDb.load(), -12.0f, "top");
            expectEquals (q.analyserBottomDb.load(), -120.0f, "floor");
            expectEquals (q.analyser.getFftOrder(), 12, "resolution");
            expectEquals (q.sidechainAnalyser.getFftOrder(), 12, "resolution (sidechain)");
            expectEquals (q.analyser.getSpeed(), 2, "speed");
            expect (q.analyser.isPeakHold(), "peak hold");

            ThreeBandEQAudioProcessor fresh;
            expectEquals (fresh.analyserTilt.load(), 4.5f, "default tilt 4.5 dB/oct");
        }
    }
};

//==============================================================================
class AutoGainTests final : public juce::UnitTest
{
public:
    AutoGainTests() : juce::UnitTest ("Auto gain", "EQ") {}

    static constexpr double fs = 48000.0;
    static constexpr int block = 512;

    // Paul Kellet's pink noise filter over white noise.
    struct Pink
    {
        std::mt19937 rng { 1234 };
        std::uniform_real_distribution<float> d { -1.0f, 1.0f };
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next()
        {
            const float w = d (rng);
            b0 = 0.99886f * b0 + w * 0.0555179f;  b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;  b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;  b5 = -0.7616f * b5 - w * 0.0168980f;
            const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.05f;
        }
    };

    // K-weighted loudness (dB) of a mono signal over [from, to).
    static double loudness (const std::vector<float>& x, size_t from, size_t to)
    {
        KWeighting k;
        k.prepare (fs);
        double e = 0.0;
        for (size_t i = 0; i < to; ++i)
        {
            const double y = k.process (x[i]);
            if (i >= from) e += y * y;
        }
        return -0.691 + 10.0 * std::log10 (e / (double) (to - from));
    }

    static void setup (ThreeBandEQAudioProcessor& p, bool autoGainOn)
    {
        setBand (p, 0, Param::Gain, 0.0f);
        setBand (p, 1, Param::Gain, 6.0f);    // broad +6 dB bell at 1 kHz
        setBand (p, 1, Param::Q, 0.5f);
        setBand (p, 2, Param::Gain, 0.0f);
        setParam (p, "auto_gain", autoGainOn ? 1.0f : 0.0f);
        p.prepareToPlay (fs, block);
    }

    void runTest() override
    {
        beginTest ("A broad +6 dB bell on pink noise is loudness-matched within 0.5 dB");
        {
            for (bool on : { false, true })
            {
                ThreeBandEQAudioProcessor p;
                setup (p, on);
                Pink pink;
                const int total = (int) (8.0 * fs);
                std::vector<float> in ((size_t) total);
                for (auto& v : in) v = pink.next();
                const auto out = runProcessor (p, fs, block, total, [&] (int n) { return in[(size_t) n]; }, [] (int) {});

                const size_t from = (size_t) (5.0 * fs);
                const double diff = loudness (out, from, out.size()) - loudness (in, from, in.size());
                if (on)
                {
                    expectWithinAbsoluteError (diff, 0.0, 0.5, "matched loudness");
                    expectLessThan (p.autoGainAppliedDb.load(), -1.0f, "a cut was applied");
                }
                else
                {
                    expectGreaterThan (diff, 2.0, "without auto gain the boost is louder");
                }
            }
        }

        beginTest ("Silence produces no gain drift");
        {
            ThreeBandEQAudioProcessor p;
            setup (p, true);
            const auto silent = runProcessor (p, fs, block, (int) (4.0 * fs), [] (int) { return 0.0f; }, [] (int) {});
            expectEquals (p.autoGainAppliedDb.load(), 0.0f, "no correction from silence");
            expect (std::all_of (silent.begin(), silent.end(), [] (float v) { return v == 0.0f; }), "silence stays silent");

            Pink pink;
            runProcessor (p, fs, block, (int) (5.0 * fs), [&] (int) { return pink.next(); }, [] (int) {});
            const float settled = p.autoGainAppliedDb.load();
            expectLessThan (settled, -1.0f, "correction after programme material");

            runProcessor (p, fs, block, (int) (6.0 * fs), [] (int) { return 0.0f; }, [] (int) {});
            expectWithinAbsoluteError (p.autoGainAppliedDb.load(), settled, 0.25f, "frozen during silence");

            // Very quiet input (below -60 dB) also doesn't move it.
            runProcessor (p, fs, block, (int) (4.0 * fs), [&] (int) { return pink.next() * 1.0e-4f; }, [] (int) {});
            expectWithinAbsoluteError (p.autoGainAppliedDb.load(), settled, 0.25f, "frozen below -60 dB");
        }

        beginTest ("Auto gain off is bit-identical (including after it was used)");
        {
            ThreeBandEQAudioProcessor a, b;
            setup (a, false);
            setup (b, true);
            Pink pinkA, pinkB;
            const int total = (int) (6.0 * fs);
            const int offAt = (int) (3.0 * fs) / block * block;
            const auto outA = runProcessor (a, fs, block, total, [&] (int) { return pinkA.next(); }, [] (int) {});
            const auto outB = runProcessor (b, fs, block, total, [&] (int) { return pinkB.next(); }, [&] (int n)
            {
                if (n == offAt) setParam (b, "auto_gain", 0.0f);
            });

            bool identical = true;
            for (size_t i = (size_t) offAt + (size_t) fs; i < outA.size(); ++i)
                identical = identical && outA[i] == outB[i];
            expect (identical, "identical once auto gain is off and its fade has finished");
            expectEquals (b.autoGainAppliedDb.load(), 0.0f, "unity after switching off");
        }

        beginTest ("On/off transitions are smooth");
        {
            ThreeBandEQAudioProcessor p;
            setup (p, false);
            const int total = (int) (10.0 * fs);
            const auto out = runProcessor (p, fs, block, total, [] (int n) { return (float) (0.2 * std::sin (2.0 * pi * 1000.0 * n / fs)); },
                                           [&] (int n)
            {
                if (n == (int) (1.0 * fs) / block * block) setParam (p, "auto_gain", 1.0f);
                if (n == (int) (6.0 * fs) / block * block) setParam (p, "auto_gain", 0.0f);
            });
            const double loud = maxStep (out, (size_t) (0.5 * fs), (size_t) (0.95 * fs));
            expectLessThan (maxStep (out, (size_t) (0.95 * fs), out.size()), loud * 1.02, "never steps beyond the uncorrected signal");
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");

            const double corrected = rms (out, (size_t) (4.5 * fs), (size_t) (5.5 * fs));
            expectWithinAbsoluteError (20.0 * std::log10 (corrected / (0.2 / std::sqrt (2.0))), 0.0, 0.5, "1 kHz tone level matched");
        }

        beginTest ("Auto gain is saved with the session; legacy sessions load with it off");
        {
            ThreeBandEQAudioProcessor p;
            setParam (p, "auto_gain", 1.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.isAutoGainOn(), "restored");

            auto state = q.apvts.copyState();
            state.removeChild (state.getChildWithProperty ("id", "auto_gain"), nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), old);
            q.setStateInformation (old.getData(), (int) old.getSize());
            expect (! q.isAutoGainOn(), "older session: off");
        }
    }
};

//==============================================================================
class GainScaleTests final : public juce::UnitTest
{
public:
    GainScaleTests() : juce::UnitTest ("Scale the whole curve", "EQ") {}

    static constexpr double fs = 48000.0;
    static constexpr int block = 256;

    // Steady-state processor gain (dB) at f with a single band and the given scale.
    static double measure (FilterType type, float freq, float gain, float scalePct, double f)
    {
        ThreeBandEQAudioProcessor p;
        for (int b = 1; b < numBands; ++b)
            p.setBandUsed (b, false);
        setBand (p, 0, Param::Type, (float) type);
        setBand (p, 0, Param::Freq, freq);
        setBand (p, 0, Param::Gain, gain);
        setBand (p, 0, Param::Q, 1.0f);
        setParam (p, "gain_scale", scalePct);
        p.prepareToPlay (fs, block);

        const int total = (int) (0.5 * fs);
        auto sig = [f] (int n) { return (float) (0.25 * std::sin (2.0 * pi * f * n / fs)); };
        const auto out = runProcessor (p, fs, block, total, sig, [] (int) {});
        std::vector<float> in ((size_t) total);
        for (int n = 0; n < total; ++n) in[(size_t) n] = sig (n);
        return 20.0 * std::log10 (rms (out, (size_t) total / 2, out.size()) / rms (in, (size_t) total / 2, in.size()));
    }

    void runTest() override
    {
        beginTest ("0 % flattens bells and shelves; cuts and notches stay active");
        {
            expectWithinAbsoluteError (measure (FilterType::Bell, 1000.0f, 12.0f, 0.0f, 1000.0), 0.0, 0.01, "bell flat");
            expectWithinAbsoluteError (measure (FilterType::LowShelf, 300.0f, 9.0f, 0.0f, 100.0), 0.0, 0.01, "low shelf flat");
            expectWithinAbsoluteError (measure (FilterType::HighShelf, 3000.0f, -9.0f, 0.0f, 10000.0), 0.0, 0.01, "high shelf flat");
            expectLessThan (measure (FilterType::LowCut, 400.0f, 0.0f, 0.0f, 100.0), -20.0, "low cut still cuts");
            expectLessThan (measure (FilterType::Notch, 1000.0f, 0.0f, 0.0f, 1000.0), -20.0, "notch still notches");
        }

        beginTest ("50 % halves, -100 % inverts, 200 % doubles a +12 dB bell");
        {
            expectWithinAbsoluteError (measure (FilterType::Bell, 1000.0f, 12.0f, 50.0f, 1000.0), 6.0, 0.05, "50 %");
            expectWithinAbsoluteError (measure (FilterType::Bell, 1000.0f, 12.0f, -100.0f, 1000.0), -12.0, 0.05, "-100 %");
            expectWithinAbsoluteError (measure (FilterType::Bell, 1000.0f, 6.0f, 200.0f, 1000.0), 12.0, 0.05, "200 %");
        }

        beginTest ("100 % is bit-identical to the plain engine; stored gains are unchanged");
        {
            ThreeBandEQAudioProcessor p;
            setBand (p, 0, Param::Gain, 7.5f);
            setBand (p, 1, Param::Gain, -4.0f);
            setBand (p, 2, Param::Type, (float) FilterType::HighShelf);
            setBand (p, 2, Param::Gain, 3.0f);
            p.prepareToPlay (fs, block);

            Engine e;
            e.prepare (fs);
            for (int b = 0; b < numBands; ++b)
            {
                BandSettings s;
                s.active = p.isBandUsed (b);
                s.type = typeFromIndex ((int) p.getBandValue (b, Param::Type));
                s.freq = p.getBandValue (b, Param::Freq);
                s.gain = typeHasGain (s.type) ? p.getBandValue (b, Param::Gain) : 0.0f;
                s.q = p.getBandValue (b, Param::Q);
                e.setBand (b, s);
            }

            std::mt19937 rng (15);
            std::uniform_real_distribution<float> d (-0.5f, 0.5f);
            std::vector<float> in ((size_t) (0.5 * fs));
            for (auto& v : in) v = d (rng);
            const auto out = runProcessor (p, fs, block, (int) in.size(), [&] (int n) { return in[(size_t) n]; }, [] (int) {});

            auto l = in, r = in;
            for (size_t pos = 0; pos < l.size(); pos += block)
            {
                float* ch[] = { l.data() + pos, r.data() + pos };
                e.process (ch, 2, (int) std::min ((size_t) block, l.size() - pos));
            }
            expect (out == l, "identical");

            setParam (p, "gain_scale", 30.0f);
            expectEquals (p.getBandValue (0, Param::Gain), 7.5f, "stored gain untouched by the scale");
        }

        beginTest ("Rapid automation of the scale stays finite and smooth");
        {
            ThreeBandEQAudioProcessor p;
            for (int b = 0; b < 3; ++b)
                setBand (p, b, Param::Gain, 6.0f);   // bells at 100 Hz, 1 kHz, 8 kHz
            p.prepareToPlay (fs, block);

            std::mt19937 rng (16);
            const int total = (int) (2.0 * fs);
            const auto out = runProcessor (p, fs, 64, total, [] (int n) { return (float) (0.1 * std::sin (2.0 * pi * 1000.0 * n / fs)); },
                                           [&] (int) { setParam (p, "gain_scale", -100.0f + (float) (rng() % 3001) * 0.1f); });

            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
            // Worst case +12 dB (200 % of +6) on a 1 kHz sine of 0.1.
            const double bound = 0.1 * std::pow (10.0, 12.0 / 20.0) * 2.0 * pi * 1000.0 / fs;
            expectLessThan (maxStep (out, 1, out.size()), bound * 1.5, "bounded slope (no clicks)");
        }

        beginTest ("Saved, stored in presets, legacy sessions load at 100 %");
        {
            ThreeBandEQAudioProcessor p;
            setParam (p, "gain_scale", 42.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (q.getGainScale(), 0.42f, 1.0e-4f, "restored");

            const auto data = q.edits.getPresetData();
            expectWithinAbsoluteError (data.gainScale, 42.0f, 0.01f, "in the preset data");
            ThreeBandEQAudioProcessor r;
            r.edits.loadPreset (data, "test");
            expectWithinAbsoluteError (r.getGainScale(), 0.42f, 1.0e-4f, "preset load applies the scale");

            auto state = q.apvts.copyState();
            state.removeChild (state.getChildWithProperty ("id", "gain_scale"), nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), old);
            q.setStateInformation (old.getData(), (int) old.getSize());
            expectEquals (q.getGainScale(), 1.0f, "older session: exactly 100 %");
        }
    }
};

//==============================================================================
class NoteTests final : public juce::UnitTest
{
public:
    NoteTests() : juce::UnitTest ("Note labels", "EQ") {}

    void runTest() override
    {
        using namespace EQ::Notes;

        beginTest ("Frequency <-> note + cents round-trips over 20 Hz - 20 kHz");
        {
            double worst = 0.0;
            bool centsInRange = true;
            for (int i = 0; i <= 2000; ++i)
            {
                const double f = 20.0 * std::pow (1000.0, i / 2000.0);
                const auto n = freqToNote (f);
                centsInRange = centsInRange && n.cents >= -50.0 - 1e-9 && n.cents <= 50.0 + 1e-9;
                const double back = midiToFreq (n.midi) * std::pow (2.0, n.cents / 1200.0);
                worst = std::max (worst, std::abs (back - f) / f);
            }
            expectLessThan (worst, 1.0e-12, "relative round-trip error");
            expect (centsInRange, "cents within +-50");

            expectEquals (formatNote (440.0), juce::String ("A4 +0") + juce::String::charToString (0x00a2));
            expectEquals (noteName (60), juce::String ("C4"));
            expectEquals (noteName (0), juce::String ("C-1"));
            expectEquals (noteName (127), juce::String ("G9"));
            expectEquals (freqToNote (1000.0).midi, 83, "1 kHz is nearest B5");
            expectWithinAbsoluteError (freqToNote (1000.0).cents, 21.31, 0.01, "1 kHz is B5 +21 cents");
        }

        beginTest ("Note-name parsing: sharps, flats, octaves -1..9, garbage rejected");
        {
            auto near = [this] (const char* text, double hz)
            {
                const auto v = parseNote (text);
                expect (v.has_value(), juce::String ("parses ") + text);
                if (v) expectWithinAbsoluteError (*v, hz, hz * 1.0e-9, text);
            };

            near ("A4", 440.0);
            near ("a4", 440.0);
            near (" A2 ", 110.0);
            near ("C#5", midiToFreq (73));
            near ("Db5", midiToFreq (73));
            near ("Eb3", midiToFreq (51));
            near ("D#3", midiToFreq (51));
            near ("B#3", midiToFreq (60));   // = C4
            near ("Cb4", midiToFreq (59));   // = B3
            near ("C-1", midiToFreq (0));
            near ("Bb-1", midiToFreq (10));
            near ("G9", midiToFreq (127));
            near ("B9", midiToFreq (131));

            for (const char* bad : { "", "A", "4", "H4", "A#", "Ab", "Abc", "A10", "C-2", "C--1", "A4x", "#4", "A 4", "1kHz", "-" })
                expect (! parseNote (bad).has_value(), juce::String ("rejects \"") + bad + "\"");
        }

        beginTest ("Snapping lands exactly on the semitone frequency");
        {
            expectEquals (snapToSemitone (450.0), 440.0, "450 Hz -> A4");
            expectEquals (snapToSemitone (1000.0), midiToFreq (83), "1 kHz -> B5");
            expectEquals (snapToSemitone (61.0), midiToFreq (35), "61 Hz -> B1");
            expectEquals (snapToSemitone (midiToFreq (64)), midiToFreq (64), "already on a semitone");

            for (int i = 0; i < 500; ++i)
            {
                const double f = 20.0 * std::pow (1000.0, i / 500.0);
                const double s = snapToSemitone (f);
                expectLessOrEqual (std::abs (1200.0 * std::log2 (s / f)), 50.0 + 1e-9, "within half a semitone");
            }

            // The frequency parameter is continuous, so a snapped value is stored exactly
            // (to float precision) instead of being rounded to a 0.1 Hz grid.
            ThreeBandEQAudioProcessor p;
            const float snapped = (float) snapToSemitone (1000.0);
            setBand (p, 0, Param::Freq, snapped);
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), snapped, snapped * 1.0e-6f, "stored as the semitone frequency");
        }

        beginTest ("Piano strip setting is saved with the UI state");
        {
            ThreeBandEQAudioProcessor p;
            p.showPianoStrip = true;
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            ThreeBandEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.showPianoStrip.load(), "restored");
        }
    }
};

static NoteTests noteTests;
static GainScaleTests gainScaleTests;
static AutoGainTests autoGainTests;
static AnalyserControlTests analyserControlTests;
static SidechainTests sidechainTests;
static StereoModeTests stereoModeTests;
static PresetTests presetTests;
static SoloTests soloTests;
static SlopeTests slopeTests;
static OutputStageTests outputStageTests;
static AnalyserTests analyserTests;
static FilterResponseTests filterResponseTests;
static SmoothingTests smoothingTests;
static ProcessorTests processorTests;

//==============================================================================
// One benchmark table: all 24 bands used and set up by 'setup', static and with
// 'automate' called for every band before every block.
static void benchCase (const char* title,
                       const std::function<void (ThreeBandEQAudioProcessor&, int band)>& setup,
                       const std::function<void (ThreeBandEQAudioProcessor&, int band, int blockIndex)>& automate)
{
    constexpr double fs = 48000.0;
    constexpr double seconds = 20.0;

    std::printf ("\n%s\n", title);
    std::printf ("%-7s %-6s %-10s %12s %12s %10s\n", "bands", "block", "mode", "us/block", "ns/sample", "% of RT");

    for (int block : { 32, 128, 512 })
        for (bool modulated : { false, true })
        {
            ThreeBandEQAudioProcessor p;
            p.setRateAndBufferSizeDetails (fs, block);
            p.prepareToPlay (fs, block);

            for (int b = 0; b < numBands; ++b)
            {
                p.setBandUsed (b, true);
                setup (p, b);
            }

            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            std::mt19937 rng (7);
            std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
            const int numBlocks = (int) (seconds * fs / block);

            double best = 1.0e30;
            for (int run = 0; run < 3; ++run)
            {
                double elapsed = 0.0;
                for (int bi = 0; bi < numBlocks; ++bi)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int n = 0; n < block; ++n)
                            buf.setSample (ch, n, noise (rng));

                    if (modulated)
                        for (int b = 0; b < numBands; ++b)
                            automate (p, b, bi);

                    const auto t0 = juce::Time::getHighResolutionTicks();
                    p.processBlock (buf, midi);
                    elapsed += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                }
                best = std::min (best, elapsed);
            }

            std::printf ("%-7d %-6d %-10s %12.2f %12.2f %9.3f%%\n", numBands, block, modulated ? "automated" : "static",
                         best / numBlocks * 1.0e6, best / (numBlocks * (double) block) * 1.0e9, best / seconds * 100.0);
        }
}

static void runBenchmark()
{
    constexpr double fs = 48000.0;
    constexpr double seconds = 20.0;

    std::printf ("CPU benchmark: stereo, %.0f Hz, %.0f s of audio per run (best of 3)\n", fs, seconds);
    std::printf ("%-7s %-6s %-10s %12s %12s %10s\n", "bands", "block", "mode", "us/block", "ns/sample", "% of RT");

    for (int numActive : { 3, 12, 24 })
        for (int block : { 32, 128, 512 })
            for (bool modulated : { false, true })
            {
                ThreeBandEQAudioProcessor p;
                p.setRateAndBufferSizeDetails (fs, block);
                p.prepareToPlay (fs, block);

                for (int b = 0; b < numBands; ++b)
                {
                    p.setBandUsed (b, b < numActive);
                    auto set = [&] (Param prm, float v) { auto* rp = p.getBandParam (b, prm); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
                    set (Param::Type, (float) (b % numFilterTypes));
                    set (Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                    set (Param::Gain, 6.0f);
                    set (Param::Q, 1.5f);
                }

                juce::AudioBuffer<float> buf (2, block);
                juce::MidiBuffer midi;
                std::mt19937 rng (7);
                std::uniform_real_distribution<float> noise (-0.5f, 0.5f);
                const int numBlocks = (int) (seconds * fs / block);

                double best = 1.0e30;
                for (int run = 0; run < 3; ++run)
                {
                    double elapsed = 0.0;
                    for (int bi = 0; bi < numBlocks; ++bi)
                    {
                        for (int ch = 0; ch < 2; ++ch)
                            for (int n = 0; n < block; ++n)
                                buf.setSample (ch, n, noise (rng));

                        if (modulated)   // continuous automation: every band's gain moves every block
                            for (int b = 0; b < numActive; ++b)
                            {
                                auto* rp = p.getBandParam (b, Param::Gain);
                                rp->setValueNotifyingHost (rp->convertTo0to1 (6.0f * std::sin ((float) bi * 0.05f + (float) b)));
                            }

                        const auto t0 = juce::Time::getHighResolutionTicks();
                        p.processBlock (buf, midi);
                        elapsed += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
                    }
                    best = std::min (best, elapsed);
                }

                std::printf ("%-7d %-6d %-10s %12.2f %12.2f %9.3f%%\n", numActive, block, modulated ? "automated" : "static",
                             best / numBlocks * 1.0e6, best / (numBlocks * (double) block) * 1.0e9, best / seconds * 100.0);
            }

    // Extra worst cases for features added later.
    benchCase ("24 cut bands at 48 dB/oct",
               [] (ThreeBandEQAudioProcessor& p, int b)
               {
                   setBand (p, b, Param::Type, (float) (b % 2 == 0 ? FilterType::LowCut : FilterType::HighCut));
                   setBand (p, b, Param::Freq, b % 2 == 0 ? 20.0f + (float) b : 20000.0f - (float) b * 100.0f);
                   setBand (p, b, Param::Slope, 5.0f);
               },
               [] (ThreeBandEQAudioProcessor& p, int b, int bi)   // frequency automation
               {
                   const float base = b % 2 == 0 ? 30.0f : 15000.0f;
                   setBand (p, b, Param::Freq, base * (1.0f + 0.2f * std::sin ((float) bi * 0.05f + (float) b)));
               });

    benchCase ("24 bells, mixed stereo modes (S/L/R/M/S...)",
               [] (ThreeBandEQAudioProcessor& p, int b)
               {
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 6.0f);
                   setBand (p, b, Param::Q, 1.5f);
                   setBand (p, b, Param::Stereo, (float) (b % numStereoModes));
               },
               [] (ThreeBandEQAudioProcessor& p, int b, int bi)   // gain automation
               {
                   setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
               });

    benchCase ("24 bells, auto gain on",
               [] (ThreeBandEQAudioProcessor& p, int b)
               {
                   if (b == 0)
                       setParam (p, "auto_gain", 1.0f);
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 6.0f);
                   setBand (p, b, Param::Q, 1.5f);
               },
               [] (ThreeBandEQAudioProcessor& p, int b, int bi)
               {
                   setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
               });
}
struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--bench")
        {
            runBenchmark();
            return 0;
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("EQ");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
