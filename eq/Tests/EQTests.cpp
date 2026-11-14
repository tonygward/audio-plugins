// EQ test runner + benchmark.
//   EQTests.exe           -> run all unit tests (exit code 1 on failure)
//   EQTests.exe --bench   -> CPU benchmark for 3 / 12 / 24 active bands
#include <juce_audio_processors/juce_audio_processors.h>
#include <set>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <random>
#include "PluginProcessor.h"
#include "EQEngine.h"
#include "SvfFilter.h"
#include "NoteNames.h"
#include "EQDisplay.h"
#include "BandPanel.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"

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

    double analyticDb (const std::vector<Spec>& specs, double fs, double f, bool matched = false)
    {
        double db = 0.0;
        for (auto& s : specs)
            if (s.enabled)
                db += getMagnitudeDb (makeFilter (s.type, fs, s.freq, s.q, typeHasGain (s.type) ? s.gain : 0.0f, s.slope, matched), f, fs);
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

    static void set (SongsmithEQAudioProcessor& p, int band, Param param, float value)
    {
        auto* rp = p.getBandParam (band, param);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    void runTest() override
    {
        beginTest ("Parameter identities");
        {
            SongsmithEQAudioProcessor p;
            constexpr int numGlobalParams = 8;   // output_gain, bypass, auto_gain, gain_scale, phase_mode, lp_quality, oversampling, hf_accuracy
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
            SongsmithEQAudioProcessor a;
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

            SongsmithEQAudioProcessor b;
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

            SongsmithEQAudioProcessor p;
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

            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor a, b;
            set (a, 0, Param::Gain, 6.0f);
            expectWithinAbsoluteError (b.getBandValue (0, Param::Gain), 0.0f, 1.0e-5f);   // interval snapping may leave ~1e-7 (FMA on Apple clang)
        }

        beginTest ("Undo / redo");
        {
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
    void setParam (SongsmithEQAudioProcessor& p, const juce::String& id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    void setBand (SongsmithEQAudioProcessor& p, int band, Param prm, float value)
    {
        auto* rp = p.getBandParam (band, prm);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    // Streams a signal through the processor in fixed blocks; 'onBlock' may change
    // parameters before each block. Returns the left output and fills 'inputOut'.
    std::vector<float> runProcessor (SongsmithEQAudioProcessor& p, double fs, int block, int total,
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor a;
            setParam (a, "output_gain", -7.5f);
            setParam (a, "bypass", 1.0f);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            SongsmithEQAudioProcessor b;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            p.edits.setType (0, FilterType::HighCut);
            p.edits.setSlope (0, 4);
            expectEquals ((int) p.getBandValue (0, Param::Slope), 4);
            p.edits.undo();
            expectEquals ((int) p.getBandValue (0, Param::Slope), defaultSlope, "undo slope");
            p.edits.redo();

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithEQAudioProcessor q;
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
        SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor a, b;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            p.soloBand = 1;
            expect (! p.edits.getUndoManager().canUndo(), "no undo history");

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            expect (! juce::String::fromUTF8 ((const char*) mb.getData(), (int) mb.getSize()).containsIgnoreCase ("solo"), "not in the state");

            SongsmithEQAudioProcessor q;
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
                             .getChildFile ("SongsmithEQ-tests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        TempFolder()  { dir.createDirectory(); }
        ~TempFolder() { dir.deleteRecursively(); }
    };

    static std::vector<float> allValues (SongsmithEQAudioProcessor& p)
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
            SongsmithEQAudioProcessor p;
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
            expect (tmp.dir.getChildFile ("Round trip.sspreset").existsAsFile(), "file written as .sspreset");

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
            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            p.editorWidth = 1500;
            p.getBypassParameter()->setValueNotifyingHost (1.0f);
            juce::String error;
            expect (p.presets.saveAs ("UI", error));
            const auto text = tmp.dir.getChildFile ("UI.sspreset").loadFileAsString();
            expect (! text.contains ("editorWidth") && ! text.contains ("analyserMode"), "no UI state");
            expect (! text.contains ("\"bypass\""), "no bypass");
            expect (text.contains ("version"), "versioned");
        }

        beginTest ("Loading a preset is one undo transaction");
        {
            SongsmithEQAudioProcessor p;
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
            int classic = 0;
            for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
                classic += PresetManager::isStartingPoint (i) ? 0 : 1;
            expectGreaterOrEqual (classic, 6);
            expectLessOrEqual (classic, 8);

            for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
            {
                SongsmithEQAudioProcessor p;
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

            SongsmithEQAudioProcessor p;
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

            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            juce::String error;
            expect (p.presets.load ({ PresetManager::getFactoryName (3), 3, {} }, error));
            juce::MemoryBlock mb;
            p.getStateInformation (mb);

            SongsmithEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.presets.hasCurrent(), "preset name restored");
            expectEquals (q.presets.getCurrent().name, PresetManager::getMenuLabel (3));
            expect (! q.presets.isModified(), "and not marked modified");
        }
    }

    static void set (SongsmithEQAudioProcessor& p, int band, Param prm, float v) { setBand (p, band, prm, v); }
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor q;
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
            SongsmithEQAudioProcessor p;
            expectEquals (p.getTotalNumInputChannels(), 2);
            expectEquals (p.getTotalNumOutputChannels(), 2);
            expect (p.getBus (true, 1) != nullptr && ! p.getBus (true, 1)->isEnabled(), "sidechain bus exists, disabled");
            expect (! p.isSidechainConnected());
        }

        beginTest ("Bus layouts accepted / rejected");
        {
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor withSc, without;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor q;
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
            SongsmithEQAudioProcessor p;
            p.analyserTilt = 3.0f;
            p.analyserTopDb = -12.0f;
            p.analyserBottomDb = -120.0f;
            p.setAnalyserFftOrder (12);
            p.setAnalyserSpeed (2);
            p.analyser.setPeakHold (true);

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.analyserTilt.load(), 3.0f, "tilt");
            expectEquals (q.analyserTopDb.load(), -12.0f, "top");
            expectEquals (q.analyserBottomDb.load(), -120.0f, "floor");
            expectEquals (q.analyser.getFftOrder(), 12, "resolution");
            expectEquals (q.sidechainAnalyser.getFftOrder(), 12, "resolution (sidechain)");
            expectEquals (q.analyser.getSpeed(), 2, "speed");
            expect (q.analyser.isPeakHold(), "peak hold");

            SongsmithEQAudioProcessor fresh;
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

    static void setup (SongsmithEQAudioProcessor& p, bool autoGainOn)
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
                SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor a, b;
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
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            setParam (p, "auto_gain", 1.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithEQAudioProcessor q;
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
        SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            setBand (p, 0, Param::Gain, 7.5f);
            setBand (p, 1, Param::Gain, -4.0f);
            setBand (p, 2, Param::Type, (float) FilterType::HighShelf);
            setBand (p, 2, Param::Gain, 3.0f);
            p.prepareToPlay (fs, block);

            Engine e;
            e.prepare (fs);
            e.setMatched (p.isHighFrequencyAccurate());   // EQ-44: same design as the processor
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
            expectWithinAbsoluteError (p.getBandValue (0, Param::Gain), 7.5f, 1.0e-5f, "stored gain untouched by the scale");   // ulp tolerance: FMA snapping on macOS
        }

        beginTest ("Rapid automation of the scale stays finite and smooth");
        {
            SongsmithEQAudioProcessor p;
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
            SongsmithEQAudioProcessor p;
            setParam (p, "gain_scale", 42.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (q.getGainScale(), 0.42f, 1.0e-4f, "restored");

            const auto data = q.edits.getPresetData();
            expectWithinAbsoluteError (data.gainScale, 42.0f, 0.01f, "in the preset data");
            SongsmithEQAudioProcessor r;
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
            SongsmithEQAudioProcessor p;
            const float snapped = (float) snapToSemitone (1000.0);
            setBand (p, 0, Param::Freq, snapped);
            expectWithinAbsoluteError (p.getBandValue (0, Param::Freq), snapped, snapped * 1.0e-6f, "stored as the semitone frequency");
        }

        beginTest ("Piano strip setting is saved with the UI state");
        {
            SongsmithEQAudioProcessor p;
            p.showPianoStrip = true;
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithEQAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.showPianoStrip.load(), "restored");
        }
    }
};

//==============================================================================
class DynamicEqTests final : public juce::UnitTest
{
public:
    DynamicEqTests() : juce::UnitTest ("Dynamic EQ", "EQ") {}

    static constexpr double fs = 48000.0;

    static BandSettings dynBell (float staticGain, float threshold, float range, float attack = 10.0f, float release = 150.0f)
    {
        BandSettings s;
        s.active = true;
        s.type = FilterType::Bell;
        s.freq = 1000.0f;
        s.gain = staticGain;
        s.q = 1.0f;
        s.dynOn = true;
        s.threshold = threshold;
        s.range = range;
        s.attackMs = attack;
        s.releaseMs = release;
        return s;
    }

    // Steady-state gain (dB) at f for a sine of 'amp' through band 0.
    static double steadyGainDb (const BandSettings& s, double f, double amp, double seconds = 1.5)
    {
        Engine e;
        e.prepare (fs);
        e.setBand (0, s);
        for (int b = 1; b < numBands; ++b) e.setBand (b, BandSettings {});
        e.snapToTargets();

        const int total = (int) (seconds * fs);
        std::vector<float> l ((size_t) total), r;
        for (int n = 0; n < total; ++n)
            l[(size_t) n] = (float) (amp * std::sin (2.0 * pi * f * n / fs));
        const auto in = l;
        r = l;
        for (int pos = 0; pos < total; pos += 64)
        {
            float* ch[] = { l.data() + pos, r.data() + pos };
            e.process (ch, 2, std::min (64, total - pos));
        }
        const size_t from = (size_t) (total * 2 / 3);
        return 20.0 * std::log10 (rms (l, from, l.size()) / rms (in, from, in.size()));
    }

    // Time (s) for the dynamic gain to cover 63.2 % of the way from 'from' to its final value
    // after the signal steps at t = 0.
    static double timeConstant (const std::vector<float>& dyn, double from, double to, int stepIndex, int hop)
    {
        const double target = from + (to - from) * (1.0 - std::exp (-1.0));
        for (size_t i = (size_t) stepIndex; i < dyn.size(); ++i)
            if ((to < from && dyn[i] <= target) || (to > from && dyn[i] >= target))
                return (double) ((int) i - stepIndex) * hop / fs;
        return -1.0;
    }

    void runTest() override
    {
        beginTest ("Below the threshold: the static response");
        {
            expectWithinAbsoluteError (steadyGainDb (dynBell (3.0f, -10.0f, -6.0f), 1000.0, 0.0316), 3.0, 0.05, "+3 dB static, -30 dBFS tone, threshold -10");
            expectWithinAbsoluteError (steadyGainDb (dynBell (0.0f, -20.0f, 12.0f), 1000.0, 0.01), 0.0, 0.05, "upward range, quiet tone");
        }

        beginTest ("Above the threshold: static gain + range");
        {
            expectWithinAbsoluteError (steadyGainDb (dynBell (3.0f, -30.0f, -6.0f), 1000.0, 0.5), -3.0, 0.3, "+3 static, -6 range");
            expectWithinAbsoluteError (steadyGainDb (dynBell (0.0f, -40.0f, -12.0f), 1000.0, 0.5), -12.0, 0.3, "de-esser style -12");
            expectWithinAbsoluteError (steadyGainDb (dynBell (2.0f, -30.0f, 6.0f), 1000.0, 0.5), 8.0, 0.3, "upward +6");

            // Shelves use a low-/high-pass detector over the region they act on.
            BandSettings shelf = dynBell (0.0f, -30.0f, -6.0f);
            shelf.type = FilterType::LowShelf;
            shelf.freq = 200.0f;
            expectWithinAbsoluteError (steadyGainDb (shelf, 50.0, 0.5), -6.0, 0.4, "low shelf on a loud 50 Hz tone");
            expectWithinAbsoluteError (steadyGainDb (shelf, 50.0, 0.003), 0.0, 0.05, "low shelf, quiet 50 Hz tone");
        }

        beginTest ("Attack and release timing within +-20 %");
        for (auto [attack, release] : { std::pair { 20.0f, 200.0f }, std::pair { 5.0f, 80.0f } })
        {
            Engine e;
            e.prepare (fs);
            e.setBand (0, dynBell (0.0f, -40.0f, -12.0f, attack, release));
            for (int b = 1; b < numBands; ++b) e.setBand (b, BandSettings {});
            e.snapToTargets();

            constexpr int hop = 16;
            const int onAt = (int) (0.2 * fs) / hop, offAt = (int) (1.4 * fs) / hop, total = (int) (2.8 * fs) / hop;
            std::vector<float> dyn ((size_t) total), l (hop), r (hop);
            for (int k = 0; k < total; ++k)
            {
                for (int i = 0; i < hop; ++i)
                {
                    const int n = k * hop + i;
                    l[(size_t) i] = r[(size_t) i] = (k >= onAt && k < offAt) ? (float) (0.5 * std::sin (2.0 * pi * 1000.0 * n / fs)) : 0.0f;
                }
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, hop);
                dyn[(size_t) k] = e.getDynamicGainDb (0);
            }

            const double att = timeConstant (dyn, 0.0, -12.0, onAt, hop);
            const double rel = timeConstant (dyn, dyn[(size_t) offAt - 1], 0.0, offAt, hop);
            expectWithinAbsoluteError (att * 1000.0, (double) attack, attack * 0.2, "attack " + juce::String (attack) + " ms");
            expectWithinAbsoluteError (rel * 1000.0, (double) release, release * 0.2, "release " + juce::String (release) + " ms");
        }

        beginTest ("Dynamics off is bit-identical (whatever the dynamic settings)");
        {
            BandSettings plain { true, FilterType::Bell, 800.0f, 5.0f, 1.5f };
            BandSettings off = plain;
            off.dynOn = false;
            off.threshold = -55.0f;
            off.range = 18.0f;
            off.attackMs = 0.1f;
            off.dynExternal = true;

            std::mt19937 rng (17);
            std::uniform_real_distribution<float> d (-0.7f, 0.7f);
            std::vector<float> in (16384);
            for (auto& v : in) v = d (rng);

            auto run = [&] (const BandSettings& s)
            {
                Engine e;
                e.prepare (fs);
                e.setBand (0, s);
                for (int b = 1; b < numBands; ++b) e.setBand (b, BandSettings {});
                auto l = in, r = in;
                for (size_t pos = 0; pos < l.size(); pos += 128)
                {
                    float* ch[] = { l.data() + pos, r.data() + pos };
                    e.process (ch, 2, 128);
                }
                return l;
            };
            expect (run (plain) == run (off), "identical");
        }

        beginTest ("External (sidechain) detection");
        {
            Engine e;
            e.prepare (fs);
            auto s = dynBell (0.0f, -30.0f, -9.0f);
            s.dynExternal = true;
            e.setBand (0, s);
            for (int b = 1; b < numBands; ++b) e.setBand (b, BandSettings {});
            e.snapToTargets();

            std::vector<float> l (64), r (64), sl (64);
            const float* sc[] = { sl.data(), sl.data() };
            for (int k = 0; k < (int) (1.0 * fs) / 64; ++k)
            {
                for (int i = 0; i < 64; ++i)
                {
                    const int n = k * 64 + i;
                    l[(size_t) i] = r[(size_t) i] = (float) (0.003 * std::sin (2.0 * pi * 1000.0 * n / fs));   // quiet main
                    sl[(size_t) i] = (float) (0.5 * std::sin (2.0 * pi * 1000.0 * n / fs));                       // loud key
                }
                e.setSidechain (sc, 2);
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, 64);
            }
            expectWithinAbsoluteError (e.getDynamicGainDb (0), -9.0f, 0.3f, "the sidechain drives the gain");

            e.setSidechain (nullptr, 0);   // no sidechain: falls back to the band's own (quiet) input
            for (int k = 0; k < (int) (2.0 * fs) / 64; ++k)
            {
                for (int i = 0; i < 64; ++i)
                    l[(size_t) i] = r[(size_t) i] = (float) (0.003 * std::sin (2.0 * pi * 1000.0 * (k * 64 + i) / fs));
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, 64);
            }
            expectWithinAbsoluteError (e.getDynamicGainDb (0), 0.0f, 0.1f, "fallback to internal: below threshold");
        }

        beginTest ("Extreme settings and rapid automation stay finite");
        for (double rate : { 44100.0, 192000.0 })
        {
            Engine e;
            e.prepare (rate);
            std::mt19937 rng (99);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);
            std::vector<float> l (32), r (32);
            bool finite = true;
            float peak = 0.0f;

            for (int k = 0; k < (int) (2.0 * rate) / 32; ++k)
            {
                for (int b = 0; b < 6; ++b)
                {
                    BandSettings s;
                    s.active = true;
                    s.type = typeFromIndex ((int) (u (rng) * 3.0f) % 3);   // bell / shelves
                    s.freq = minFreq * std::pow (maxFreq / minFreq, u (rng));
                    s.gain = -30.0f + 60.0f * u (rng);
                    s.q = minQ * std::pow (maxQ / minQ, u (rng));
                    s.dynOn = u (rng) > 0.2f;
                    s.dynExternal = u (rng) > 0.5f;
                    s.threshold = -60.0f * u (rng);
                    s.range = -24.0f + 48.0f * u (rng);
                    s.attackMs = u (rng) > 0.5f ? 0.1f : 100.0f * u (rng);
                    s.releaseMs = 10.0f + 990.0f * u (rng);
                    e.setBand (b, s);
                }
                for (int i = 0; i < 32; ++i)
                    l[(size_t) i] = r[(size_t) i] = 0.9f * (u (rng) * 2.0f - 1.0f);
                const float* sc[] = { l.data(), r.data() };
                e.setSidechain (sc, 2);
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, 32);
                for (float v : l) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
            }
            expect (finite, "finite at " + juce::String (rate));
            juce::ignoreUnused (peak);   // stacked +48 dB bands may legitimately be huge; stability is checked below

            // Stop automating, return to neutral, feed silence: everything must decay away.
            for (int b = 0; b < 6; ++b)
                e.setBand (b, BandSettings { true, FilterType::Bell, 1000.0f, 0.0f, 1.0f });
            e.setSidechain (nullptr, 0);
            float tail = 0.0f;
            for (int k = 0; k < (int) (3.0 * rate) / 32; ++k)
            {
                std::fill (l.begin(), l.end(), 0.0f);
                std::fill (r.begin(), r.end(), 0.0f);
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, 32);
                if (k > (int) (2.5 * rate) / 32)
                    for (float v : l) tail = std::max (tail, std::abs (v));
            }
            expectLessThan (tail, 1.0e-6f, "decays to silence at " + juce::String (rate));
        }

        beginTest ("Legacy session loads with dynamics off; dynamic settings are undoable and saved");
        {
            SongsmithEQAudioProcessor p;
            setBand (p, 0, Param::DynOn, 1.0f);
            setBand (p, 0, Param::Threshold, -12.0f);
            auto state = p.apvts.copyState();
            state.setProperty ("version", 2, nullptr);
            for (int b = 0; b < numBands; ++b)
                for (auto prm : { Param::DynOn, Param::Threshold, Param::Range, Param::Attack, Param::Release, Param::DynExternal })
                    state.removeChild (state.getChildWithProperty ("id", paramId (b, prm)), nullptr);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());

            bool allOff = true;
            for (int b = 0; b < numBands; ++b)
                allOff = allOff && p.getBandValue (b, Param::DynOn) == 0.0f
                                && std::abs (p.getBandValue (b, Param::Threshold) - defaultThreshold) < 0.01f;
            expect (allOff, "dynamics off, defaults");

            p.edits.setDynamic (1, true);
            p.edits.setValue (1, Param::Range, -9.0f);
            expect (p.getBandValue (1, Param::DynOn) == 1.0f);
            p.edits.undo();
            p.edits.undo();
            expect (p.getBandValue (1, Param::DynOn) == 0.0f, "undo dynamic on");
            p.edits.redo();
            p.edits.redo();

            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            SongsmithEQAudioProcessor q;
            q.setStateInformation (saved.getData(), (int) saved.getSize());
            expect (q.getBandValue (1, Param::DynOn) == 1.0f, "restored on");
            expectWithinAbsoluteError (q.getBandValue (1, Param::Range), -9.0f, 0.01f, "restored range");
        }
    }
};

//==============================================================================
class LinearPhaseTests final : public juce::UnitTest
{
public:
    LinearPhaseTests() : juce::UnitTest ("Linear phase", "EQ") {}

    static constexpr double fs = 48000.0;
    static constexpr int block = 512;

    // Switches 'p' to linear phase at 'quality', prepares it and waits until the
    // convolution engines run the designed IR.
    bool prepareLinear (SongsmithEQAudioProcessor& p, int quality)
    {
        setParam (p, "phase_mode", 1.0f);
        setParam (p, "lp_quality", (float) quality);
        p.setRateAndBufferSizeDetails (fs, block);
        p.prepareToPlay (fs, block);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        for (int i = 0; i < 1000 && ! p.isLinearPhaseReady(); ++i)
        {
            buf.clear();
            p.processBlock (buf, midi);
            juce::Thread::sleep (2);
        }
        // Flush the 50 ms IR cross-fade and any old tail.
        for (int i = 0; i < (int) (0.5 * fs) / block; ++i)
        {
            buf.clear();
            p.processBlock (buf, midi);
        }
        return p.isLinearPhaseReady();
    }

    // Impulse response of the processor (left output for a left+right impulse, or per channel).
    static std::vector<float> impulseResponse (SongsmithEQAudioProcessor& p, int length, int channel = 0, bool leftOnly = false)
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int pos = 0; pos < length; pos += block)
        {
            buf.clear();
            if (pos == 0)
            {
                buf.setSample (0, 0, 1.0f);
                if (! leftOnly) buf.setSample (1, 0, 1.0f);
            }
            p.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
                out.push_back (buf.getSample (channel, i));
        }
        out.resize ((size_t) length);
        return out;
    }

    static double magnitudeDbAt (const std::vector<float>& h, double f)
    {
        std::complex<double> sum = 0.0;
        const double w = -2.0 * pi * f / fs;
        for (size_t n = 0; n < h.size(); ++n)
            sum += (double) h[n] * std::polar (1.0, w * (double) n);
        return 20.0 * std::log10 (std::max (std::abs (sum), 1.0e-12));
    }

    void runTest() override
    {
        struct Curve { const char* name; std::vector<Spec> bands; };
        const std::vector<Curve> curves {
            { "bell + shelves", { { FilterType::Bell, 1000.0f, 6.0f, 1.0f }, { FilterType::LowShelf, 200.0f, -4.0f, defaultQ },
                                  { FilterType::HighShelf, 8000.0f, 3.0f, defaultQ } } },
            { "cut + narrow bell", { { FilterType::LowCut, 40.0f, 0.0f, defaultQ, true, 3 }, { FilterType::Bell, 3000.0f, -9.0f, 4.0f },
                                     { FilterType::HighCut, 16000.0f, 0.0f, defaultQ } } },
            { "many bells", { { FilterType::Bell, 80.0f, 5.0f, 1.5f }, { FilterType::Bell, 400.0f, -6.0f, 2.0f },
                              { FilterType::Bell, 2500.0f, 4.0f, 0.7f }, { FilterType::Bell, 12000.0f, -3.0f, 1.0f } } },
        };

        auto applyCurve = [] (SongsmithEQAudioProcessor& p, const std::vector<Spec>& specs)
        {
            for (int b = 0; b < numBands; ++b)
                p.setBandUsed (b, b < (int) specs.size());
            for (size_t b = 0; b < specs.size(); ++b)
            {
                const auto& s = specs[b];
                setBand (p, (int) b, Param::Type, (float) s.type);
                setBand (p, (int) b, Param::Freq, s.freq);
                setBand (p, (int) b, Param::Gain, s.gain);
                setBand (p, (int) b, Param::Q, s.q);
                setBand (p, (int) b, Param::Slope, (float) s.slope);
            }
        };

        beginTest ("Magnitude matches the minimum-phase response within 0.2 dB (30 Hz - 18 kHz)");
        for (const auto& curve : curves)
        {
            SongsmithEQAudioProcessor p;
            applyCurve (p, curve.bands);
            expect (prepareLinear (p, 2), "IR loaded");

            const int length = p.getLinearPhaseLength();
            const auto h = impulseResponse (p, length * 2);
            double worst = 0.0;
            for (int i = 0; i <= 40; ++i)
            {
                const double f = 30.0 * std::pow (18000.0 / 30.0, i / 40.0);
                const double expected = analyticDb (curve.bands, fs, f, p.isHighFrequencyAccurate());   // EQ-44: same design as the processor
                if (expected < -40.0)
                    continue;   // deep stop band: relative accuracy is meaningless there
                worst = std::max (worst, std::abs (magnitudeDbAt (h, f) - expected));
            }
            expectLessThan (worst, 0.2, juce::String ("worst deviation, ") + curve.name);
        }

        beginTest ("Impulse response is symmetric (linear phase); latency = measured delay");
        for (int quality = 0; quality < 3; ++quality)
        {
            SongsmithEQAudioProcessor p;
            applyCurve (p, curves[0].bands);
            expect (prepareLinear (p, quality), "IR loaded");
            p.updateLatencyNow();

            const int length = p.getLinearPhaseLength();
            const int latency = p.getLatencySamples();
            expectEquals (latency, length / 2, "reported latency = N/2");

            const auto h = impulseResponse (p, length + 1024);
            int peak = 0;
            for (int n = 0; n < (int) h.size(); ++n)
                if (std::abs (h[(size_t) n]) > std::abs (h[(size_t) peak])) peak = n;
            expectEquals (peak, latency, "measured delay (IR peak) = reported latency, quality " + juce::String (quality));

            double asym = 0.0, peakValue = std::abs (h[(size_t) latency]);
            for (int n = 1; n < latency; ++n)
                asym = std::max (asym, (double) std::abs (h[(size_t) (latency + n)] - h[(size_t) (latency - n)]));
            expectLessThan (asym / peakValue, 1.0e-4, "symmetric about the centre, quality " + juce::String (quality));
        }

        beginTest ("Flat EQ in linear phase is a pure delay");
        {
            SongsmithEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b) p.setBandUsed (b, false);
            expect (prepareLinear (p, 0));
            const auto h = impulseResponse (p, p.getLinearPhaseLength());
            const int latency = p.getLinearPhaseLength() / 2;
            expectWithinAbsoluteError (h[(size_t) latency], 1.0f, 1.0e-4f, "unity at the latency");
            double rest = 0.0;
            for (size_t n = 0; n < h.size(); ++n)
                if ((int) n != latency) rest = std::max (rest, (double) std::abs (h[n]));
            expectLessThan (rest, 1.0e-4, "nothing elsewhere");
        }

        beginTest ("Stereo modes in linear phase (Left-only band leaves the right channel alone)");
        {
            SongsmithEQAudioProcessor p;
            applyCurve (p, { { FilterType::Bell, 1000.0f, 9.0f, 1.0f } });
            setBand (p, 0, Param::Stereo, (float) StereoMode::Left);
            expect (prepareLinear (p, 1));
            const int length = p.getLinearPhaseLength();
            const auto hl = impulseResponse (p, length, 0);
            expectWithinAbsoluteError (magnitudeDbAt (hl, 1000.0), 9.0, 0.2, "left boosted");

            SongsmithEQAudioProcessor q;
            applyCurve (q, { { FilterType::Bell, 1000.0f, 9.0f, 1.0f } });
            setBand (q, 0, Param::Stereo, (float) StereoMode::Left);
            expect (prepareLinear (q, 1));
            const auto hr = impulseResponse (q, length, 1);
            expectWithinAbsoluteError (magnitudeDbAt (hr, 1000.0), 0.0, 0.05, "right untouched");

            SongsmithEQAudioProcessor m;
            applyCurve (m, { { FilterType::Bell, 1000.0f, 6.0f, 1.0f } });
            setBand (m, 0, Param::Stereo, (float) StereoMode::Mid);
            expect (prepareLinear (m, 1));
            const auto hm = impulseResponse (m, length, 0);   // L = R impulse: pure Mid
            expectWithinAbsoluteError (magnitudeDbAt (hm, 1000.0), 6.0, 0.2, "mid band on mono content");
        }

        beginTest ("Rapid parameter changes stay finite with bounded steps");
        {
            SongsmithEQAudioProcessor p;
            applyCurve (p, curves[2].bands);
            expect (prepareLinear (p, 0));

            std::mt19937 rng (18);
            std::uniform_real_distribution<float> u (0.0f, 1.0f);
            const int total = (int) (3.0 * fs);
            const auto out = runProcessor (p, fs, block, total, [] (int n) { return (float) (0.1 * std::sin (2.0 * pi * 300.0 * n / fs)); },
                                           [&] (int)
            {
                for (int b = 0; b < 4; ++b)
                {
                    setBand (p, b, Param::Gain, -12.0f + 24.0f * u (rng));
                    setBand (p, b, Param::Freq, 100.0f * std::pow (100.0f, u (rng)));
                }
            });
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
            // Random +-12 dB on 4 bands every 10 ms, with JUCE cross-fading (50 ms) between very
            // different IRs: steps can reach ~25x the input slope. A hard switch would step by
            // the full amplitude (~1.6 here), i.e. more than 250x.
            const double inputStep = 0.1 * 2.0 * pi * 300.0 / fs;
            expectLessThan (maxStep (out, (size_t) p.getLinearPhaseLength(), out.size()), inputStep * 60.0, "no clicks");
        }

        beginTest ("Zero-latency mode reports no latency; switching modes fades and stays finite");
        {
            SongsmithEQAudioProcessor p;
            p.prepareToPlay (fs, block);
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), 0, "zero latency");
            expectEquals (p.getTailLengthSeconds(), 0.0, "no tail");

            const int total = (int) (2.0 * fs);
            const auto out = runProcessor (p, fs, block, total, [] (int n) { return (float) (0.2 * std::sin (2.0 * pi * 440.0 * n / fs)); },
                                           [&] (int n)
            {
                if (n == (int) (0.5 * fs) / block * block) setParam (p, "phase_mode", 1.0f);
                if (n == (int) (1.3 * fs) / block * block) setParam (p, "phase_mode", 0.0f);
            });
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
            const double bound = 0.2 * 2.0 * pi * 440.0 / fs;
            expectLessThan (maxStep (out, 1, out.size()), bound * 2.0, "fades, no hard switch");
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), 0, "back to zero latency");
        }

        beginTest ("Legacy sessions load in zero-latency mode");
        {
            SongsmithEQAudioProcessor p;
            setParam (p, "phase_mode", 1.0f);
            auto state = p.apvts.copyState();
            state.removeChild (state.getChildWithProperty ("id", "phase_mode"), nullptr);
            state.removeChild (state.getChildWithProperty ("id", "lp_quality"), nullptr);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! p.isLinearPhaseSelected(), "zero latency");
            expectEquals (p.getLinearPhaseQuality(), 1, "medium quality by default");
        }
    }
};

//==============================================================================
class OversamplingTests final : public juce::UnitTest
{
public:
    OversamplingTests() : juce::UnitTest ("Oversampling", "EQ") {}

    // Analogue prototype of the bell: H(s) = (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1).
    static double analogBellDb (double f, double f0, double gainDb, double q)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const std::complex<double> s (0.0, f / f0);
        return 20.0 * std::log10 (std::abs ((s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0)));
    }

    static double measureDb (SongsmithEQAudioProcessor& p, double fs, double f)
    {
        const int total = (int) (0.4 * fs);
        auto sig = [=] (int n) { return (float) (0.1 * std::sin (2.0 * pi * f * n / fs)); };
        const auto out = runProcessor (p, fs, 256, total, sig, [] (int) {});
        std::vector<float> in ((size_t) total);
        for (int n = 0; n < total; ++n) in[(size_t) n] = sig (n);
        return 20.0 * std::log10 (rms (out, (size_t) total / 2, out.size()) / rms (in, (size_t) total / 2, in.size()));
    }

    static void bell16k (SongsmithEQAudioProcessor& p)
    {
        for (int b = 0; b < numBands; ++b) p.setBandUsed (b, b == 0);
        setBand (p, 0, Param::Freq, 16000.0f);
        setBand (p, 0, Param::Gain, 12.0f);
        setBand (p, 0, Param::Q, 2.0f);
    }

    void runTest() override
    {
        constexpr double fs = 44100.0;

        beginTest ("44.1 kHz, 16 kHz +12 dB bell, Q 2: 2x / 4x are closer to the analogue prototype");
        {
            const double freqs[] = { 8000.0, 11000.0, 13000.0, 14500.0, 16000.0, 17000.0 };
            double rmsErr[3] {};
            juce::String report ("RMS error vs analogue (dB): ");

            for (int os = 0; os < 3; ++os)
            {
                double sum = 0.0;
                for (double f : freqs)
                {
                    SongsmithEQAudioProcessor p;
                    bell16k (p);
                    setParam (p, "oversampling", (float) os);
                    p.setRateAndBufferSizeDetails (fs, 256);
                    p.prepareToPlay (fs, 256);
                    const double err = measureDb (p, fs, f) - analogBellDb (f, 16000.0, 12.0, 2.0);
                    sum += err * err;
                }
                rmsErr[os] = std::sqrt (sum / std::size (freqs));
                report << juce::StringArray { "off", "2x", "4x" }[os] << " " << juce::String (rmsErr[os], 2) << "  ";
            }
            logMessage (report);
            expectLessThan (rmsErr[1], rmsErr[0] * 0.6, "2x closer than off");
            expectLessThan (rmsErr[2], rmsErr[1], "4x closer than 2x");

            // The drawn curve (designed at the oversampled rate) matches what is measured.
            SongsmithEQAudioProcessor p;
            bell16k (p);
            setParam (p, "oversampling", 2.0f);
            p.setRateAndBufferSizeDetails (fs, 256);
            p.prepareToPlay (fs, 256);
            const auto drawn = makeFilter (FilterType::Bell, fs * 4.0, 16000.0, 2.0, 12.0, defaultSlope);
            expectWithinAbsoluteError (measureDb (p, fs, 13000.0), getMagnitudeDb (drawn, 13000.0, fs * 4.0), 0.3,
                                       "4x: measured = curve drawn at the oversampled rate");
        }

        beginTest ("Reported latency equals the measured delay");
        for (int os = 1; os < 3; ++os)
        {
            SongsmithEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b) p.setBandUsed (b, false);
            setParam (p, "oversampling", (float) os);
            p.setRateAndBufferSizeDetails (fs, 256);
            p.prepareToPlay (fs, 256);
            p.updateLatencyNow();
            const int latency = p.getLatencySamples();
            expectGreaterThan (latency, 0, "some latency at " + juce::String (1 << os) + "x");

            const int total = 8192;
            auto sig = [] (int n) { return (float) (0.3 * std::sin (2.0 * pi * 300.0 * n / fs) + 0.2 * std::sin (2.0 * pi * 1100.0 * n / fs)); };
            const auto out = runProcessor (p, fs, 256, total, sig, [] (int) {});

            int best = -1;
            double bestErr = 1.0e30;
            for (int d = 0; d < 400; ++d)
            {
                double err = 0.0;
                for (int n = 2048; n < total; ++n)
                    err += std::abs ((double) out[(size_t) n] - sig (n - d));
                if (err < bestErr) { bestErr = err; best = d; }
            }
            expectWithinAbsoluteError (best, latency, 1, "measured delay = reported latency at " + juce::String (1 << os) + "x");
        }

        beginTest ("Off reports no latency (and processing is the plain engine)");
        {
            SongsmithEQAudioProcessor p;
            p.prepareToPlay (fs, 256);
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), 0);
            expectEquals (p.getOversamplingFactor(), 1);
        }

        beginTest ("All sample rates stay finite with 2x / 4x and automation");
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            for (int os = 1; os < 3; ++os)
            {
                SongsmithEQAudioProcessor p;
                setParam (p, "oversampling", (float) os);
                p.setRateAndBufferSizeDetails (rate, 128);
                p.prepareToPlay (rate, 128);
                for (int b = 0; b < 8; ++b)
                {
                    p.setBandUsed (b, true);
                    setBand (p, b, Param::Type, (float) (b % numFilterTypes));
                    setBand (p, b, Param::Freq, 30.0f * std::pow (600.0f, (float) b / 8.0f));
                }
                std::mt19937 rng ((unsigned) rate + (unsigned) os);
                std::uniform_real_distribution<float> u (0.0f, 1.0f);
                const auto out = runProcessor (p, rate, 128, (int) (0.5 * rate), [&] (int) { return 0.5f * (u (rng) * 2.0f - 1.0f); },
                                               [&] (int)
                {
                    for (int b = 0; b < 8; ++b)
                    {
                        setBand (p, b, Param::Gain, -24.0f + 48.0f * u (rng));
                        setBand (p, b, Param::Freq, minFreq * std::pow (maxFreq / minFreq, u (rng)));
                    }
                });
                expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }),
                        "finite at " + juce::String (rate) + " Hz, " + juce::String (1 << os) + "x");
            }

        beginTest ("Switching the factor while running fades (no hard switch)");
        {
            SongsmithEQAudioProcessor p;
            setBand (p, 1, Param::Gain, 6.0f);
            p.setRateAndBufferSizeDetails (48000.0, 256);
            p.prepareToPlay (48000.0, 256);
            const auto out = runProcessor (p, 48000.0, 256, 48000, [] (int n) { return (float) (0.2 * std::sin (2.0 * pi * 440.0 * n / 48000.0)); },
                                           [&] (int n)
            {
                if (n == 256 * 40) setParam (p, "oversampling", 2.0f);
                if (n == 256 * 120) setParam (p, "oversampling", 1.0f);
            });
            expect (std::all_of (out.begin(), out.end(), [] (float v) { return std::isfinite (v); }), "finite");
            expectLessThan (maxStep (out, 1, out.size()), 0.2 * 2.0 * 2.0 * pi * 440.0 / 48000.0 * 1.3, "smooth");
        }

        beginTest ("Linear phase uses the oversampled design without extra latency");
        {
            SongsmithEQAudioProcessor p;
            bell16k (p);
            setParam (p, "oversampling", 2.0f);
            setParam (p, "phase_mode", 1.0f);
            setParam (p, "lp_quality", 1.0f);
            p.setRateAndBufferSizeDetails (fs, 512);
            p.prepareToPlay (fs, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            for (int i = 0; i < 1000 && ! p.isLinearPhaseReady(); ++i) { buf.clear(); p.processBlock (buf, midi); juce::Thread::sleep (2); }
            for (int i = 0; i < 60; ++i) { buf.clear(); p.processBlock (buf, midi); }
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), p.getLinearPhaseLength() / 2, "latency is just the FIR's");

            const double measured = measureDb (p, fs, 13000.0);
            const auto drawn = makeFilter (FilterType::Bell, fs * 4.0, 16000.0, 2.0, 12.0, defaultSlope);
            expectWithinAbsoluteError (measured, getMagnitudeDb (drawn, 13000.0, fs * 4.0), 0.3, "un-cramped response in linear phase");
        }

        beginTest ("Legacy sessions load with oversampling off");
        {
            SongsmithEQAudioProcessor p;
            setParam (p, "oversampling", 2.0f);
            auto state = p.apvts.copyState();
            state.removeChild (state.getChildWithProperty ("id", "oversampling"), nullptr);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (p.getOversamplingFactor(), 1);
        }
    }
};

//==============================================================================
// Starting-point templates (factory presets based on Joe Gilder's EQ advice).
class StartingPointTests final : public juce::UnitTest
{
public:
    StartingPointTests() : juce::UnitTest ("Starting-point templates", "EQ") {}

    static constexpr int block = 512;

    static std::vector<int> templates()
    {
        std::vector<int> list;
        for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
            if (PresetManager::isStartingPoint (i))
                list.push_back (i);
        return list;
    }

    // K-weighted loudness change (dB) of pink noise through 'data' at 48 kHz.
    static double loudnessChange (const PresetData& data)
    {
        const double fs = AutoGainTests::fs;
        SongsmithEQAudioProcessor p;
        p.edits.loadPreset (data, "test");
        p.prepareToPlay (fs, block);

        AutoGainTests::Pink pink;
        const int total = (int) (8.0 * fs);
        std::vector<float> in ((size_t) total);
        for (auto& v : in) v = pink.next();
        const auto out = runProcessor (p, fs, block, total, [&] (int n) { return in[(size_t) n]; }, [] (int) {});

        const size_t from = (size_t) (3.0 * fs);
        return AutoGainTests::loudness (out, from, out.size()) - AutoGainTests::loudness (in, from, in.size());
    }

    void runTest() override
    {
        const auto list = templates();

        beginTest ("The starting points are listed after the original factory presets");
        {
            expectEquals ((int) list.size(), 19);
            const int firstTemplate = list.empty() ? 0 : list.front();
            expectEquals (firstTemplate, 8, "the 8 original presets keep their indices");
            for (size_t k = 0; k < list.size(); ++k)
                expectEquals (list[k], firstTemplate + (int) k, "templates are contiguous");
        }

        // Exception (task 26): index 6 was renamed from "Acoustic guitar" to "Bright acoustic" and its
        // 2.5 kHz boost became a cut (see "Renamed presets" below). Its band fields are still classic.
        beginTest ("The original factory presets are unchanged by the new band fields");
        {
            for (int i = 0; i < 8; ++i)
            {
                const auto d = PresetManager::getFactoryPreset (i);
                expect (PresetManager::getFactoryHint (i).isEmpty());
                for (int b = 0; b < PresetManager::getFactoryUsedBands (i); ++b)
                {
                    const auto& s = d.bands[(size_t) b];
                    expect (s.used && s.enabled && ! s.dynOn && ! s.dynExternal, PresetManager::getFactoryName (i));
                    expectEquals (s.threshold, defaultThreshold);
                    expectEquals (s.range, defaultRange);
                    expectEquals (s.attack, defaultAttackMs);
                    expectEquals (s.release, defaultReleaseMs);
                }
            }
        }

        beginTest ("Every template loads and is finite at 44.1, 48 and 96 kHz");
        {
            for (int i : list)
            {
                for (double fs : { 44100.0, 48000.0, 96000.0 })
                {
                    SongsmithEQAudioProcessor p;
                    juce::String error;
                    expect (p.presets.load ({ PresetManager::getFactoryName (i), i, {} }, error), error);
                    expectEquals (p.getNumUsedBands(), PresetManager::getFactoryUsedBands (i), PresetManager::getFactoryName (i));
                    expect (! p.presets.isModified());

                    p.prepareToPlay (fs, block);
                    AutoGainTests::Pink pink;
                    bool finite = true;
                    const auto out = runProcessor (p, fs, block, (int) fs, [&] (int) { return pink.next() * 8.0f; }, [] (int) {});
                    for (float v : out)
                        finite = finite && std::isfinite (v) && std::abs (v) < 10.0f;
                    expect (finite, "finite output: " + PresetManager::getFactoryName (i) + " at " + juce::String (fs));
                }
            }
        }

        beginTest ("Bypassed bands load as used-but-off and don't change the sound");
        {
            int bypassedSeen = 0;
            for (int i : list)
            {
                const auto data = PresetManager::getFactoryPreset (i);
                auto without = data;
                for (auto& s : without.bands)
                    if (s.used && ! s.enabled)
                    {
                        s.used = false;
                        ++bypassedSeen;
                    }

                SongsmithEQAudioProcessor a, b;
                juce::String error;
                expect (a.presets.load ({ PresetManager::getFactoryName (i), i, {} }, error), error);
                for (int k = 0; k < PresetManager::getFactoryUsedBands (i); ++k)
                    expect (a.isBandUsed (k), "band is used");
                b.edits.loadPreset (without, "without");

                a.prepareToPlay (48000.0, block);
                b.prepareToPlay (48000.0, block);
                AutoGainTests::Pink pinkA, pinkB;
                const auto outA = runProcessor (a, 48000.0, block, 48000, [&] (int) { return pinkA.next(); }, [] (int) {});
                const auto outB = runProcessor (b, 48000.0, block, 48000, [&] (int) { return pinkB.next(); }, [] (int) {});

                float maxDiff = 0.0f;
                for (size_t n = 0; n < outA.size(); ++n)
                    maxDiff = std::max (maxDiff, std::abs (outA[n] - outB[n]));
                expectLessThan (maxDiff, 1.0e-6f, "null test: " + PresetManager::getFactoryName (i));
            }
            expectGreaterThan (bypassedSeen, 5, "templates use bypassed 'only if needed' bands");
        }

        beginTest ("Every template is loudness-matched on pink noise within 0.25 dB");
        {
            for (int i : list)
            {
                const double change = loudnessChange (PresetManager::getFactoryPreset (i));
                logMessage ("  " + PresetManager::getFactoryName (i) + ": " + juce::String (change, 2) + " dB");
                expectWithinAbsoluteError (change, 0.0, 0.25, PresetManager::getFactoryName (i));
            }
        }

        beginTest ("Cut first: active boosts never outweigh active cuts");
        {
            for (int i : list)
            {
                const auto presetName = PresetManager::getFactoryName (i);
                if (presetName == "Electric guitar bus")   // deliberate exception: the 360 Hz warmth boost is the move
                    continue;
                // Task 31 exceptions: Tony's own album tom EQ, copied as is rather than changed to fit the rule.
                // Rack tom: +4.6/+5.9/+3.3 against -11.9 (SSL E-Channel, tony-ward.md lines 1114, 2295, 2315).
                // Floor tom: +7.8/+5.9/+6.8 against -6.6 (Yesterday's Toy, tony-ward.md line 2147).
                if (presetName == "Rack tom" || presetName == "Mid tom" || presetName == "High tom" || presetName == "Floor tom")
                    continue;

                const auto d = PresetManager::getFactoryPreset (i);
                float boosts = 0.0f, cuts = 0.0f;
                for (const auto& s : d.bands)
                    if (s.used && s.enabled)
                    {
                        (s.gain > 0.0f ? boosts : cuts) += std::abs (s.gain);
                        if (s.dynOn)   // dynamic bands count by their range
                            (s.range > 0.0f ? boosts : cuts) += std::abs (s.range);
                    }
                expectLessOrEqual (boosts, cuts, presetName);
            }
        }

        beginTest ("Acoustic guitar: three cuts and a boost, with Tony's dynamic band");
        {
            const int index = 11;
            expectEquals (PresetManager::getFactoryName (index), juce::String ("Acoustic guitar"));
            expectEquals (PresetManager::getFactoryUsedBands (index), 5);
            const auto d = PresetManager::getFactoryPreset (index);

            struct Expected { FilterType type; float freq, gain, q; bool enabled, dynamic; float range; };
            const Expected expected[] = {
                { FilterType::LowCut,    50.0f,    0.0f, defaultQ, true, false,  0.0f },
                { FilterType::Bell,      150.0f,  -3.0f, 1.4f,     true, false,  0.0f },
                { FilterType::Bell,      500.0f,  -1.5f, 2.0f,     true, false,  0.0f },
                { FilterType::Bell,      1800.0f,  0.0f, 4.0f,     true, true,  -2.5f },
                { FilterType::HighShelf, 10000.0f, 1.5f, defaultQ, true, false,  0.0f },
            };
            for (int b = 0; b < 5; ++b)
            {
                const auto& s = d.bands[(size_t) b];
                const auto& e = expected[b];
                const auto what = "band " + juce::String (b + 1);
                expect (s.type == (int) e.type, what + " type");
                expectWithinAbsoluteError (s.freq, e.freq, 0.01f, what + " frequency");
                expectWithinAbsoluteError (s.gain, e.gain, 0.01f, what + " gain");
                expectWithinAbsoluteError (s.q, e.q, 0.01f, what + " Q");
                expect (s.enabled == e.enabled, what + " is on");
                expect (s.dynOn == e.dynamic, what + " dynamic");
                if (e.dynamic)
                    expectWithinAbsoluteError (s.range, e.range, 0.01f, what + " range");
            }
            expectEquals (d.bands[0].slope, 1, "low cut 12 dB/oct");

            expectEquals (PresetManager::getFactoryName (19), juce::String ("Acoustic guitar in a full mix"));
            expectEquals (PresetManager::getFactoryName (20), juce::String ("Acoustic guitar pickup"));
            expect (PresetManager::getFactoryPreset (19).bands[1].dynOn, "full mix: dynamic boom band");
            expect (PresetManager::getFactoryPreset (20).bands[1].dynOn, "pickup: dynamic quack band");
        }

        beginTest ("Acoustic guitar's 2 kHz band: still on normal playing, cuts on loud strums");
        {
            // Pink noise stands in for a strummed guitar: -20 dBFS RMS ~ normal playing on a
            // well-recorded track, -6 dBFS RMS ~ the loudest strums.
            auto deepestCut = [this] (float rmsDb)
            {
                AutoGainTests::Pink calibrate;
                double sum = 0.0;
                for (int n = 0; n < 48000; ++n) { const double v = calibrate.next(); sum += v * v; }
                const float scale = juce::Decibels::decibelsToGain (rmsDb) / (float) std::sqrt (sum / 48000.0);

                SongsmithEQAudioProcessor p;
                juce::String error;
                expect (p.presets.load ({ PresetManager::getFactoryName (11), 11, {} }, error), error);
                p.prepareToPlay (48000.0, block);
                AutoGainTests::Pink pink;
                float deepest = 0.0f;
                runProcessor (p, 48000.0, block, 3 * 48000, [&] (int) { return pink.next() * scale; },
                              [&] (int n) { if (n > 48000) deepest = juce::jmin (deepest, p.dynamicGainDb[3].load()); });
                return deepest;
            };

            const float normal = deepestCut (-20.0f), loud = deepestCut (-6.0f);
            logMessage ("  deepest dynamic cut: " + juce::String (normal, 2) + " dB at -20 dBFS RMS, "
                        + juce::String (loud, 2) + " dB at -6 dBFS RMS");
            expectGreaterThan (normal, -0.5f, "normal playing is left alone");
            expectLessThan (loud, -1.0f, "loud strums get cut");
            expectGreaterOrEqual (loud, -2.5f, "never more than the range");
        }

        beginTest ("Renamed presets: old sessions still show the right preset");
        {
            auto restore = [] (const juce::String& savedName, int index)
            {
                SongsmithEQAudioProcessor p;
                juce::ValueTree ui ("UI");
                ui.setProperty ("presetName", savedName, nullptr);
                ui.setProperty ("presetFactory", index, nullptr);
                p.presets.restoreFromUiState (ui);
                return p.presets.getCurrent();
            };

            const auto body = restore ("Acoustic guitar body", 11);
            expectEquals (body.factoryIndex, 11);
            expectEquals (body.name, juce::String ("Guitars - Acoustic Guitar"), "old template name at its old index");

            const auto classic = restore ("Acoustic guitar", 6);
            expectEquals (classic.factoryIndex, 6);
            expectEquals (classic.name, juce::String ("Guitars - Acoustic Guitar (Bright)"), "the classic preset at index 6");

            const auto current = restore ("Acoustic guitar", 11);
            expectEquals (current.name, juce::String ("Guitars - Acoustic Guitar"));

            // The renamed classic preset loads at index 6, with a cut (not a boost) at 2.5 kHz.
            expectEquals (PresetManager::getFactoryName (6), juce::String ("Bright acoustic"));
            expect (! PresetManager::isStartingPoint (6));
            SongsmithEQAudioProcessor p;
            juce::String error;
            expect (p.presets.load ({ PresetManager::getFactoryName (6), 6, {} }, error), error);
            expectEquals (p.presets.getCurrent().factoryIndex, 6);
            expectWithinAbsoluteError (PresetManager::getFactoryPreset (6).bands[2].gain, -1.5f, 0.01f);
        }

        beginTest ("Every template has a short usage hint");
        {
            for (int i : list)
            {
                const auto hint = PresetManager::getFactoryHint (i);
                expect (hint.isNotEmpty(), PresetManager::getFactoryName (i));
                expectLessOrEqual (hint.length(), 100, PresetManager::getFactoryName (i));
            }
        }

        beginTest ("Every factory preset keeps its index");
        {
            const char* names[] = { "Flat", nullptr, nullptr, nullptr, nullptr, nullptr, "Bright acoustic", nullptr,
                                    nullptr, nullptr, nullptr, "Acoustic guitar", nullptr, nullptr, nullptr, nullptr,
                                    nullptr, nullptr, nullptr, "Acoustic guitar in a full mix", "Acoustic guitar pickup",
                                    "Reverb send clean-up", "Rack tom", "Floor tom", "Toms bus", "Mid tom", "High tom" };
            expectGreaterOrEqual (PresetManager::getNumFactoryPresets(), 27);
            for (int i = 0; i < (int) std::size (names); ++i)
                if (names[i] != nullptr)
                    expectEquals (PresetManager::getFactoryName (i), juce::String (names[i]), "index " + juce::String (i));
        }

        beginTest ("EQ-40: the Starting Points menu is grouped by instrument and has no Factory section");
        {
            const auto groups = PresetManager::getMenuGroups();
            expectEquals ((int) groups.size(), 5);
            expect (groups[0] == "Drums" && groups[1] == "Guitars" && groups[2] == "Vocals"
                    && groups[3] == "Effects" && groups[4] == "General", "group order");

            // Every requested label is there, loads, and sits in its group.
            const struct { const char* label; const char* group; } wanted[] = {
                { "Drums - Kick", "Drums" }, { "Drums - Snare", "Drums" }, { "Drums - Mid Tom", "Drums" },
                { "Drums - High Tom", "Drums" }, { "Drums - Floor Tom", "Drums" }, { "Drums - Toms Bus", "Drums" },
                { "Drums - Drum Bus", "Drums" }, { "Guitars - Acoustic Guitar", "Guitars" },
                { "Guitars - Acoustic Guitar (Full Mix)", "Guitars" }, { "Guitars - Acoustic Guitar (Pickup)", "Guitars" },
                { "Guitars - Electric Guitar", "Guitars" }, { "Guitars - Electric Guitar Bus", "Guitars" },
                { "Guitars - Bass Guitar", "Guitars" }, { "Vocals - Lead Vocal", "Vocals" },
                { "Vocals - Backing Vocals", "Vocals" }, { "Vocals - Spoken Word", "Vocals" },
                { "Effects - Reverb Return", "Effects" }, { "Effects - Reverb Send Clean-up", "Effects" } };

            SongsmithEQAudioProcessor p;
            const auto all = p.presets.getAllPresets();
            for (const auto& w : wanted)
            {
                int found = -1;
                for (const auto& e : all)
                    if (e.name == w.label)
                        found = e.factoryIndex;
                expect (found >= 0, juce::String (w.label) + " is in the menu");
                if (found >= 0)
                {
                    expect (PresetManager::getMenuGroup (found) == w.group, juce::String (w.label) + " group");
                    juce::String error;
                    SongsmithEQAudioProcessor q;
                    expect (q.presets.load ({ w.label, found, {} }, error), error);
                    expectEquals (q.presets.getCurrent().name, juce::String (w.label));
                }
            }

            // Order: grouped, labels start with their group, and every listed name is unique.
            juce::StringArray seen;
            juce::String lastGroup;
            int groupChanges = 0;
            for (const auto& e : all)
            {
                if (! e.isFactory())
                    continue;
                expect (e.name.startsWith (e.group + " - "), e.name + " starts with its group");
                expect (! seen.contains (e.name), e.name + " is listed once");
                seen.add (e.name);
                if (e.group != lastGroup)
                {
                    ++groupChanges;
                    lastGroup = e.group;
                }
            }
            expectEquals (groupChanges, (int) groups.size(), "each group is one block");

            for (const auto& e : all)
                expect (! e.name.containsIgnoreCase ("factory") && ! e.name.containsIgnoreCase ("five-zone"), e.name);
        }

        beginTest ("EQ-40: hidden presets are not in the menu but still load at their old index");
        {
            const int hidden[] = { 0, 18, 22 };   // Flat, Five-zone start, Rack tom
            SongsmithEQAudioProcessor p;
            const auto all = p.presets.getAllPresets();
            for (int index : hidden)
            {
                expect (! PresetManager::isListed (index), PresetManager::getFactoryName (index) + " is hidden");
                for (const auto& e : all)
                    expect (e.factoryIndex != index, PresetManager::getFactoryName (index) + " is not offered");

                // A session saved with it loads the same bands as the factory preset at that index.
                juce::ValueTree ui ("UI");
                ui.setProperty ("presetName", PresetManager::getFactoryName (index), nullptr);
                ui.setProperty ("presetFactory", index, nullptr);
                SongsmithEQAudioProcessor q;
                q.presets.restoreFromUiState (ui);
                expectEquals (q.presets.getCurrent().factoryIndex, index);
                expectEquals (q.presets.getCurrent().name, PresetManager::getFactoryName (index), "shown under its stored name");
                juce::String error;
                expect (q.presets.load (q.presets.getCurrent(), error), error);
                expectEquals (q.getNumUsedBands(), PresetManager::getFactoryUsedBands (index));
                expect (! q.presets.isModified(), "it recalls exactly");
            }
            expect (PresetManager::getFactoryName (18) == "Five-zone start", "its stored name is unchanged");
        }

        beginTest ("EQ-40: every factory preset keeps its index, bands and name, and every one is listed or deliberately hidden");
        {
            const auto listed = PresetManager::getListedIndices();
            for (int i = 0; i < PresetManager::getNumFactoryPresets(); ++i)
            {
                const bool isListed = std::find (listed.begin(), listed.end(), i) != listed.end();
                const bool deliberatelyHidden = i == 0 || i == 18 || i == 22;
                expect (isListed != deliberatelyHidden, PresetManager::getFactoryName (i) + (isListed ? " listed" : " hidden"));
            }
            expectEquals ((int) std::set<int> (listed.begin(), listed.end()).size(), (int) listed.size(), "no preset listed twice");
            expectEquals ((int) listed.size(), PresetManager::getNumFactoryPresets() - 3);

            // Legacy names recall under the new labels by index (the old indices of the renamed classics).
            const struct { int index; const char* oldName; const char* label; } renamed[] = {
                { 5, "Kick punch", "Drums - Kick" }, { 6, "Bright acoustic", "Guitars - Acoustic Guitar (Bright)" },
                { 1, "Vocal clean-up", "Vocals - Quick Clean-up" }, { 2, "Low-end tidy", "General - Low-End Tidy" },
                { 7, "De-mud", "General - Reduce Mud" }, { 3, "Air", "General - Add Air" }, { 4, "Telephone", "Effects - Telephone" } };
            for (const auto& r : renamed)
            {
                expectEquals (PresetManager::getFactoryName (r.index), juce::String (r.oldName));
                expectEquals (PresetManager::getMenuLabel (r.index), juce::String (r.label));
                juce::ValueTree ui ("UI");
                ui.setProperty ("presetName", r.oldName, nullptr);
                ui.setProperty ("presetFactory", r.index, nullptr);
                SongsmithEQAudioProcessor q;
                q.presets.restoreFromUiState (ui);
                expectEquals (q.presets.getCurrent().name, juce::String (r.label), r.oldName);
            }
        }

        beginTest ("EQ-40: Mid Tom and High Tom are the Rack Tom curve under their own labels");
        {
            const auto rack = PresetManager::getFactoryPreset (22);
            for (int index : { 25, 26 })
            {
                const auto d = PresetManager::getFactoryPreset (index);
                expectEquals (PresetManager::getFactoryUsedBands (index), PresetManager::getFactoryUsedBands (22));
                expect (PresetManager::isStartingPoint (index) && PresetManager::getFactoryHint (index).isNotEmpty());
                for (int b = 0; b < 5; ++b)
                {
                    expect (d.bands[(size_t) b].type == rack.bands[(size_t) b].type);
                    expectEquals (d.bands[(size_t) b].freq, rack.bands[(size_t) b].freq);
                    expectEquals (d.bands[(size_t) b].gain, rack.bands[(size_t) b].gain);
                    expectEquals (d.bands[(size_t) b].q, rack.bands[(size_t) b].q);
                }
                expectEquals (d.outputGain, rack.outputGain);
            }
        }

        beginTest ("EQ-40: moving between presets visits only what the menu offers, and user presets stay separate");
        {
            SongsmithEQAudioProcessor p;
            PresetTests::TempFolder folder;
            p.presets.setUserFolder (folder.dir);
            juce::String error;
            expect (p.presets.saveAs ("Drums - Kick", error), error);   // a user preset may share a label

            const auto all = p.presets.getAllPresets();
            int user = 0, factory = 0;
            for (const auto& e : all)
                (e.isFactory() ? factory : user)++;
            expectEquals (factory, (int) PresetManager::getListedIndices().size());
            expectEquals (user, 1);

            // Next from the last Starting Point is the first user preset, and the one after wraps round.
            expect (p.presets.load (all[(size_t) factory - 1], error), error);
            expect (p.presets.loadNext (+1, error), error);
            expect (! p.presets.getCurrent().isFactory(), "then the user preset");
            expect (p.presets.loadNext (+1, error), error);
            expect (p.presets.getCurrent().isFactory(), "then the first Starting Point");
            expectEquals (p.presets.getCurrent().name, all[0].name);

            // Starting Points can't be deleted or overwritten; user presets still can.
            expect (p.presets.load (all[0], error), error);
            expect (! p.presets.canSaveInPlace(), "a Starting Point isn't saved in place");
            expect (p.presets.load (all[(size_t) factory], error), error);
            expect (p.presets.canSaveInPlace(), "a user preset is");
            expect (p.presets.deleteCurrent (error), error);
            expectEquals ((int) p.presets.getUserPresets().size(), 0);
        }

        beginTest ("SUI-09: a duplicate name is detected before it is replaced, and replacing adds no second file");
        {
            SongsmithEQAudioProcessor p;
            PresetTests::TempFolder folder;
            p.presets.setUserFolder (folder.dir);
            juce::String error;

            expect (! p.presets.userPresetExists ("Guitar Warmth"), "nothing saved yet");
            expect (! p.presets.userPresetExists ("   "), "an empty name never exists");
            expect (p.presets.saveAs ("Guitar Warmth", error), error);
            expect (p.presets.userPresetExists ("Guitar Warmth"));
            expect (p.presets.userPresetExists ("  Guitar Warmth "), "the check cleans the name like a save does");

            expect (p.presets.saveAs ("Guitar Warmth", error), error);   // deliberate replace, after the confirmation
            expectEquals ((int) p.presets.getUserPresets().size(), 1);
        }

        beginTest ("Reverb send clean-up: Tony's Pro-Q 4 preset, band for band");
        {
            const int index = 21;
            expectEquals (PresetManager::getFactoryName (index), juce::String ("Reverb send clean-up"));
            expect (PresetManager::isStartingPoint (index));
            expectEquals (PresetManager::getFactoryUsedBands (index), 5);
            const auto d = PresetManager::getFactoryPreset (index);

            struct Expected { FilterType type; float freq, gain, q; };
            const Expected expected[] = {
                { FilterType::LowCut,   377.28f,  0.0f,  defaultQ },
                { FilterType::Bell,     594.40f,  0.54f, 1.923f },
                { FilterType::HighCut,  17056.0f, 0.0f,  defaultQ },
                { FilterType::Bell,     10094.0f, 0.41f, 2.158f },
                { FilterType::Bell,     2000.0f, -3.0f,  3.016f },
            };
            for (int b = 0; b < 5; ++b)
            {
                const auto& s = d.bands[(size_t) b];
                const auto& e = expected[b];
                const auto what = "band " + juce::String (b + 1);
                expect (s.used && s.enabled && ! s.dynOn, what + " used and on");
                expect (s.type == (int) e.type, what + " type");
                expectWithinAbsoluteError (s.freq, e.freq, e.freq * 0.01f, what + " frequency within 1 %");
                expectWithinAbsoluteError (s.gain, e.gain, 0.01f, what + " gain");
                expectWithinAbsoluteError (s.q, e.q, 0.01f, what + " Q");
            }
            expectEquals (d.bands[0].slope, 1, "low cut 12 dB/oct");
            expectEquals (d.bands[2].slope, 1, "high cut 12 dB/oct");

            // Loaded into the processor, the bands read back the same (within parameter rounding).
            SongsmithEQAudioProcessor p;
            juce::String error;
            expect (p.presets.load ({ PresetManager::getFactoryName (index), index, {} }, error), error);
            for (int b = 0; b < 5; ++b)
            {
                expectEquals ((int) p.getBandValue (b, Param::Type), (int) expected[b].type);
                expectWithinAbsoluteError (p.getBandValue (b, Param::Freq), expected[b].freq, expected[b].freq * 0.01f);
            }
        }

        beginTest ("Tom presets: Tony's album settings, band for band");
        {
            struct Expected { FilterType type; float freq, gain, q; bool enabled; int slope; };
            auto checkPreset = [this] (int index, const char* presetLabel, std::initializer_list<Expected> bands)
            {
                expectEquals (PresetManager::getFactoryName (index), juce::String (presetLabel));
                expect (PresetManager::isStartingPoint (index));
                expectEquals (PresetManager::getFactoryUsedBands (index), (int) bands.size(), presetLabel);
                const auto d = PresetManager::getFactoryPreset (index);
                int b = 0;
                for (const auto& e : bands)
                {
                    const auto& s = d.bands[(size_t) b];
                    const auto what = juce::String (presetLabel) + " band " + juce::String (b + 1);
                    expect (s.used && ! s.dynOn, what + " used, not dynamic");
                    expect (s.enabled == e.enabled, what + (e.enabled ? " on" : " off"));
                    expect (s.type == (int) e.type, what + " type");
                    expectWithinAbsoluteError (s.freq, e.freq, e.freq * 0.01f, what + " frequency within 1 %");
                    expectWithinAbsoluteError (s.gain, e.gain, 0.1f, what + " gain within 0.1 dB");
                    expectWithinAbsoluteError (s.q, e.q, 0.01f, what + " Q");
                    if (typeHasSlope (e.type))
                        expectEquals (s.slope, e.slope, what + " slope");
                    ++b;
                }

                for (double fs : { 44100.0, 48000.0, 96000.0 })
                {
                    SongsmithEQAudioProcessor p;
                    juce::String error;
                    expect (p.presets.load ({ presetLabel, index, {} }, error), error);
                    p.prepareToPlay (fs, block);
                    AutoGainTests::Pink pink;
                    bool finite = true;
                    for (float v : runProcessor (p, fs, block, (int) fs, [&] (int) { return pink.next() * 4.0f; }, [] (int) {}))
                        finite = finite && std::isfinite (v);
                    expect (finite, juce::String (presetLabel) + " finite at " + juce::String (fs));
                }
            };

            using T = FilterType;
            checkPreset (22, "Rack tom", { { T::LowCut, 162.0f, 0.0f, defaultQ, true, 2 },   // SSL high-pass, 18 dB/oct
                                           { T::Bell, 163.0f, 4.6f, 0.7f, true, 1 },
                                           { T::Bell, 670.0f, -11.9f, 1.7f, true, 1 },
                                           { T::Bell, 4070.0f, 5.9f, 1.0f, true, 1 },
                                           { T::Bell, 10320.0f, 3.3f, 0.7f, true, 1 } });
            checkPreset (23, "Floor tom", { { T::LowShelf, 122.0f, 7.8f, defaultQ, true, 1 },
                                            { T::Bell, 930.0f, -6.6f, 1.7f, true, 1 },
                                            { T::Bell, 4070.0f, 5.9f, 1.0f, true, 1 },
                                            { T::HighShelf, 10320.0f, 6.8f, defaultQ, true, 1 } });
            checkPreset (24, "Toms bus", { { T::LowCut, 55.0f, 0.0f, defaultQ, true, 5 },
                                           { T::Bell, 119.0f, -4.8f, 2.0f, true, 1 },
                                           { T::Bell, 317.0f, -8.0f, 1.4f, false, 1 },
                                           { T::HighCut, 10700.0f, 0.0f, defaultQ, true, 3 } });
        }
    }
};

//==============================================================================
// Task 21: TW Pro EQ -> Songsmith EQ. Sessions, presets and plugin identity carry over.
class RenameTests final : public juce::UnitTest
{
public:
    RenameTests() : juce::UnitTest ("Rename to Songsmith EQ", "EQ") {}

    using TempFolder = PresetTests::TempFolder;

    static void writePreset (SongsmithEQAudioProcessor& p, const juce::File& file, float band1Gain)
    {
        setBand (p, 0, Param::Gain, band1Gain);
        auto state = p.createPresetState();
        if (auto xml = state.createXml())
            xml->writeTo (file);
    }

    void runTest() override
    {
        beginTest ("Names and plugin codes");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith EQ"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            // Never change these: they are the VST3 class ID / AU identity existing sessions look for.
            expectEquals (juce::String (EQ_MANUFACTURER_CODE), juce::String ("Tnyw"), "manufacturer code unchanged");
            expectEquals (juce::String (EQ_PLUGIN_CODE), juce::String ("Eq3b"), "plugin code unchanged");
        }

        beginTest ("A session saved by TW Pro EQ 0.2.0 opens with identical settings");
        {
            const auto fixture = juce::File (EQ_TEST_FIXTURES).getChildFile ("tw-pro-eq-0.2.0-session.b64");
            expect (fixture.existsAsFile(), "fixture present: " + fixture.getFullPathName());

            juce::MemoryBlock mb;
            expect (mb.fromBase64Encoding (fixture.loadFileAsString().trim()), "fixture decodes");
            const auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            expect (xml != nullptr, "fixture holds plugin state");
            if (xml == nullptr)
                return;

            SongsmithEQAudioProcessor p;
            p.setStateInformation (mb.getData(), (int) mb.getSize());

            int checked = 0, nonDefault = 0;
            juce::String diffs;
            for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
            {
                const auto id = child->getStringAttribute ("id");
                const auto saved = (float) child->getDoubleAttribute ("value");
                auto* rp = p.apvts.getParameter (id);
                if (rp == nullptr)
                {
                    diffs << id << " missing; ";
                    continue;
                }
                const float now = rp->convertFrom0to1 (rp->getValue());
                if (std::abs (now - saved) > 1.0e-4f * std::max (1.0f, std::abs (saved)))
                    diffs << id << ": " << saved << " -> " << now << "; ";
                nonDefault += std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-6f ? 1 : 0;
                ++checked;
            }
            expect (diffs.isEmpty(), "every parameter round-trips: " + diffs);
            // The fixture predates EQ-44, so it has every parameter except hf_accuracy, which loads off.
            expectEquals (checked, p.getParameters().size() - 1, "the session holds every parameter it knew about");
            expect (! p.isHighFrequencyAccurate(), "a session from before Accurate Highs loads with it off");
            expectGreaterThan (nonDefault, checked * 3 / 4, "the fixture really moves the parameters off their defaults");

            bool slots = true;
            for (int b = 0; b < numBands; ++b)
                slots = slots && p.isBandUsed (b) == ((b % 3) != 1);
            expect (slots, "used/unused band slots restored");
            expectEquals (p.selectedBand.load(), 5, "UI state restored");
            expectEquals (p.editorWidth.load(), 1111);
            expectEquals (p.editorHeight.load(), 666);

            // Saving again writes the same parameter values back.
            juce::MemoryBlock again;
            p.getStateInformation (again);
            SongsmithEQAudioProcessor q;
            q.setStateInformation (again.getData(), (int) again.getSize());
            bool same = true;
            for (int i = 0; i < p.getParameters().size(); ++i)
                same = same && std::abs (p.getParameters()[i]->getValue() - q.getParameters()[i]->getValue()) < 1.0e-6f;
            expect (same, "re-saved session is identical");
        }

        beginTest ("Default preset folders");
        {
            const auto now = PresetManager::getDefaultUserFolder();
            const auto old = PresetManager::getLegacyUserFolder();
            expect (now.getFullPathName().endsWith (juce::File::addTrailingSeparator ("Songsmith Studio") + "Songsmith EQ"
                                                    + juce::File::getSeparatorString() + "Presets"), now.getFullPathName());
            expect (old.getFullPathName().endsWith (juce::File::addTrailingSeparator ("TW Pro EQ") + "Presets"), old.getFullPathName());
            expect (now.getParentDirectory().getParentDirectory().getParentDirectory()
                        == old.getParentDirectory().getParentDirectory(), "both under the same application-data folder");
            const auto realAppData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
            expect (! now.isAChildOf (realAppData) && ! old.isAChildOf (realAppData), "tests never use the real folders");
        }

        beginTest ("Old presets folder and no new folder: presets are copied, the old folder is untouched");
        {
            TempFolder tmp;
            const auto legacy = tmp.dir.getChildFile ("TW Pro EQ").getChildFile ("Presets");
            const auto target = tmp.dir.getChildFile ("Songsmith Studio").getChildFile ("Songsmith EQ").getChildFile ("Presets");
            legacy.createDirectory();

            SongsmithEQAudioProcessor writer;
            writePreset (writer, legacy.getChildFile ("Vocal.twpreset"), -4.0f);
            writePreset (writer, legacy.getChildFile ("Bass.twpreset"), 2.5f);
            const auto vocalText = legacy.getChildFile ("Vocal.twpreset").loadFileAsString();

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (target, legacy);
            const auto list = p.presets.getUserPresets();

            expectEquals ((int) list.size(), 2, "both old presets listed");
            expect (target.getChildFile ("Vocal.twpreset").existsAsFile() && target.getChildFile ("Bass.twpreset").existsAsFile(),
                    "copied into the new folder");
            expectEquals (target.getChildFile ("Vocal.twpreset").loadFileAsString(), vocalText, "copied byte for byte");
            expect (legacy.getChildFile ("Vocal.twpreset").existsAsFile() && legacy.getChildFile ("Bass.twpreset").existsAsFile(),
                    "old folder left in place");
            expectEquals (legacy.getChildFile ("Vocal.twpreset").loadFileAsString(), vocalText, "old files unchanged");
            expectEquals (target.getParentDirectory().findChildFiles (juce::File::findDirectories, false).size(), 1,
                          "no temporary folder left behind");

            juce::String error;
            for (auto& e : list)
                expect (e.file.isAChildOf (target) && p.presets.load (e, error), "loads from the new folder: " + e.name + " " + error);

            // Deleting a preset later must not bring it back: migration only happens once.
            target.getChildFile ("Bass.twpreset").deleteFile();
            SongsmithEQAudioProcessor again;
            again.presets.setUserFolder (target, legacy);
            expectEquals ((int) again.presets.getUserPresets().size(), 1, "no second copy once the new folder exists");
        }

        beginTest ("Both folders exist: nothing is copied or overwritten");
        {
            TempFolder tmp;
            const auto legacy = tmp.dir.getChildFile ("old");
            const auto target = tmp.dir.getChildFile ("new");
            legacy.createDirectory();
            target.createDirectory();
            legacy.getChildFile ("Old only.twpreset").replaceWithText ("<PARAMETERS/>");
            legacy.getChildFile ("Same.twpreset").replaceWithText ("old");
            target.getChildFile ("Same.twpreset").replaceWithText ("new");

            expect (! PresetManager::migrateLegacyFolder (legacy, target), "nothing to do");

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (target, legacy);
            expectEquals ((int) p.presets.getUserPresets().size(), 1);
            expect (! target.getChildFile ("Old only.twpreset").exists(), "nothing copied");
            expectEquals (target.getChildFile ("Same.twpreset").loadFileAsString(), juce::String ("new"), "nothing overwritten");
        }

        beginTest ("Neither folder exists: the new folder works and starts empty");
        {
            TempFolder tmp;
            const auto legacy = tmp.dir.getChildFile ("old");
            const auto target = tmp.dir.getChildFile ("Songsmith Studio").getChildFile ("Songsmith EQ").getChildFile ("Presets");

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (target, legacy);
            expect (p.presets.getUserPresets().empty(), "empty");
            expect (! legacy.exists(), "old folder not created");

            juce::String error;
            expect (p.presets.saveAs ("First", error), error);
            expect (target.getChildFile ("First.sspreset").existsAsFile(), "saves into the new folder as .sspreset");
            expectEquals ((int) p.presets.getUserPresets().size(), 1);
        }

        beginTest ("A legacy path that isn't a folder is ignored");
        {
            TempFolder tmp;
            const auto notAFolder = tmp.dir.getChildFile ("TW Pro EQ");
            notAFolder.replaceWithText ("not a folder");
            const auto target = tmp.dir.getChildFile ("new");
            expect (! PresetManager::migrateLegacyFolder (notAFolder, target));
            expect (! target.exists(), "no folder created");
        }

        beginTest (".twpreset and .sspreset files are both listed and both load");
        {
            TempFolder tmp;
            SongsmithEQAudioProcessor writer;
            writePreset (writer, tmp.dir.getChildFile ("From TW Pro EQ.twpreset"), -6.0f);
            writePreset (writer, tmp.dir.getChildFile ("From Songsmith.sspreset"), 3.0f);
            tmp.dir.getChildFile ("Not a preset.txt").replaceWithText ("ignored");

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            const auto list = p.presets.getUserPresets();
            expectEquals ((int) list.size(), 2, "both extensions listed, other files ignored");

            for (auto& e : list)
            {
                juce::String error;
                expect (p.presets.load (e, error), e.name + ": " + error);
                const float expected = e.file.hasFileExtension (PresetManager::legacyFileExtension) ? -6.0f : 3.0f;
                expectWithinAbsoluteError (p.getBandValue (0, Param::Gain), expected, 0.01f, e.name);
            }
        }

        beginTest ("Saving over a .twpreset updates it in place; new presets are .sspreset");
        {
            TempFolder tmp;
            SongsmithEQAudioProcessor writer;
            writePreset (writer, tmp.dir.getChildFile ("Vocal.twpreset"), -6.0f);

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (tmp.dir);
            juce::String error;
            expect (p.presets.load (p.presets.getUserPresets().front(), error), error);
            setBand (p, 0, Param::Gain, 1.5f);
            expect (p.presets.saveAs (p.presets.getCurrent().name, error), error);   // what "Save" does

            expect (! tmp.dir.getChildFile ("Vocal.sspreset").exists(), "no second 'Vocal'");
            expectEquals ((int) p.presets.getUserPresets().size(), 1);
            expect (p.presets.canSaveInPlace() && p.presets.getCurrent().file == tmp.dir.getChildFile ("Vocal.twpreset"),
                    "current preset is still the .twpreset file");

            SongsmithEQAudioProcessor q;
            q.presets.setUserFolder (tmp.dir);
            expect (q.presets.load (q.presets.getUserPresets().front(), error), error);
            expectWithinAbsoluteError (q.getBandValue (0, Param::Gain), 1.5f, 0.01f, "the .twpreset holds the new settings");

            expect (p.presets.saveAs ("Another", error), error);
            expect (tmp.dir.getChildFile ("Another.sspreset").existsAsFile(), "new names save as .sspreset");
        }

        beginTest ("A session pointing at a preset in the old folder uses the migrated copy");
        {
            TempFolder tmp;
            const auto legacy = tmp.dir.getChildFile ("old");
            const auto target = tmp.dir.getChildFile ("new");
            legacy.createDirectory();
            SongsmithEQAudioProcessor writer;
            writePreset (writer, legacy.getChildFile ("Mix.twpreset"), 2.0f);

            SongsmithEQAudioProcessor p;
            p.presets.setUserFolder (target, legacy);
            juce::ValueTree ui ("UI");
            ui.setProperty ("presetName", "Mix", nullptr);
            ui.setProperty ("presetFactory", -1, nullptr);
            ui.setProperty ("presetFile", legacy.getChildFile ("Mix.twpreset").getFullPathName(), nullptr);

            p.presets.restoreFromUiState (ui);
            expect (p.presets.getCurrent().file == legacy.getChildFile ("Mix.twpreset"), "before migration: the old file");
            expect (! target.exists(), "restoring a session never writes");

            p.presets.getUserPresets();   // first use migrates
            p.presets.restoreFromUiState (ui);
            expect (p.presets.getCurrent().file == target.getChildFile ("Mix.twpreset"), "after migration: the copy");
        }

        beginTest ("Messages use the new name");
        {
            PresetData d;
            juce::String error;
            expect (! PresetManager::decode (juce::ValueTree ("SOMETHING"), d, error));
            expect (error.contains ("Songsmith EQ") && ! error.contains ("TW Pro"), error);
        }
    }
};

//==============================================================================
// Task 24: every word on screen is spelled out, and the embedded font is used.
class WordingAndFontTests final : public juce::UnitTest
{
public:
    WordingAndFontTests() : juce::UnitTest ("Plain wording and fonts", "EQ") {}

    // The suite's shared wording check (shared/Source/SongsmithWording.h), which started here.
    static juce::String findBanned (const juce::String& text)                 { return Songsmith::UI::Wording::findBanned (text); }
    static void collect (juce::Component& c, juce::StringArray& out)          { Songsmith::UI::Wording::collect (c, out); }
    static void collect (const juce::PopupMenu& menu, juce::StringArray& out) { Songsmith::UI::Wording::collect (menu, out); }

    void runTest() override
    {
        beginTest ("The abbreviation check itself");
        {
            expectEquals (findBanned ("SC"), juce::String ("SC"));
            expectEquals (findBanned ("AG +1.2"), juce::String ("AG"));
            expectEquals (findBanned ("OS 2x"), juce::String ("OS"));
            expectEquals (findBanned ("Out +0.0 dB"), juce::String ("Out"));
            expectEquals (findBanned ("Sidechain: spectrum"), juce::String ("Sidechain"));
            expectEquals (findBanned ("D"), juce::String ("D"));
            expectEquals (findBanned ("THRESH"), juce::String ("THRESH"));
            expect (findBanned ("Q").isEmpty());
            expect (findBanned ("Undo (Ctrl+Z)").isEmpty());
            expect (findBanned ("440 Hz " + juce::String::charToString (0x00b7) + " A4 +0").isEmpty());
            expect (findBanned ("C#5, Eb3, 48 dB/oct, -60 dBFS, 10 ms, Oversampling: 2x").isEmpty());
            expect (findBanned ("Output +1.5 dB").isEmpty());
            expectEquals (findBanned ("GR -3.2 dB"), juce::String ("GR"));
            expectEquals (findBanned ("In"), juce::String ("In"));
            expectEquals (findBanned ("Atten"), juce::String ("Atten"));
            expectEquals (findBanned ("TPDF dither"), juce::String ("TPDF"));
            expectEquals (findBanned ("Attack \"OFF\""), juce::String ("OFF"));
            expectEquals (findBanned ("For the BV bus"), juce::String ("BV"));
            expectEquals (findBanned ("Band A"), juce::String ("A"));
            expect (findBanned ("A safety net. I like it: A bit more.").isEmpty(), "the words A and I start a sentence");
            expect (findBanned ("-14 LUFS, -1 dBTP, +4 dBu, VST3 and AU").isEmpty());
        }

        beginTest ("No abbreviations anywhere in the editor");
        {
            SongsmithEQAudioProcessor p;
            p.prepareToPlay (48000.0, 512);
            setParam (p, "auto_gain", 1.0f);
            const int dyn = p.edits.addBand (2500.0f, -2.0f, FilterType::Bell);
            p.edits.setDynamic (dyn, true);
            p.edits.setStereoMode (dyn, 3);

            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                ed->getDisplay().setSelectedBand (dyn);

            juce::StringArray texts;
            collect (*editor, texts);
            if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                collect (ed->createAnalyserMenu(), texts);

            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (bool input : { true, false })
                for (int i = 0; i < p.getBusCount (input); ++i)
                    texts.add (p.getBus (input, i)->getName());

            expectGreaterThan (texts.size(), 100, "found the editor's text");
            for (auto& t : texts)
                expect (findBanned (t).isEmpty(), "abbreviation \"" + findBanned (t) + "\" in \"" + t + "\"");

            expect (texts.contains ("Side Chain"), "the side chain button says Side Chain");
            expect (texts.contains ("Dynamic"), "the dynamic button says Dynamic");
            expect (texts.contains ("Threshold") && texts.contains ("Frequency"), "knob labels are spelled out");
            expect (texts.contains ("Side Chain: spectrum and overlap"));
            expect (texts.contains ("Auto Gain 0.0 dB"), "auto gain shows its correction with the unit");
            expect (texts.contains ("Output +0.0 dB") || texts.contains ("Output 0.0 dB"));
            expect (texts.contains ("Band 1 Frequency") && texts.contains ("Band 1 Enabled")
                    && texts.contains ("Band 1 Dynamic Side Chain"), "parameter display names");
        }

        beginTest ("Typed output gain accepts the label or a bare number");
        {
            expectWithinAbsoluteError (SongsmithEQAudioProcessorEditor::parseOutputGainText ("Output +1.5 dB"), 1.5, 1.0e-9);
            expectWithinAbsoluteError (SongsmithEQAudioProcessorEditor::parseOutputGainText ("-3 dB"), -3.0, 1.0e-9);
            expectWithinAbsoluteError (SongsmithEQAudioProcessorEditor::parseOutputGainText ("2"), 2.0, 1.0e-9);
            expectWithinAbsoluteError (SongsmithEQAudioProcessorEditor::parseOutputGainText ("Output -12.0 dB"), -12.0, 1.0e-9);
        }

        beginTest ("Plain words in factory presets, and every label fits at the default and minimum sizes");
        {
            juce::StringArray texts;
            for (int i = 0; i < EQ::PresetManager::getNumFactoryPresets(); ++i)
            {
                texts.add (EQ::PresetManager::getFactoryName (i));
                texts.add (EQ::PresetManager::getFactoryHint (i));
            }
            expectGreaterThan (texts.size(), 4, "found the factory presets");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, "EQ factory presets");

            for (auto size : { std::pair<int, int> { 1280, 760 }, std::pair<int, int> { EQ::EditorSize::minWidth, 440 } })
            {
                SongsmithEQAudioProcessor p;
                std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
                editor->setSize (size.first, size.second);
                if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                    ed->getDisplay().setSelectedBand (0);
                Songsmith::UI::Wording::expectTextFits (*this, *editor, editor->getLookAndFeel(),
                                                        "EQ at " + juce::String (size.first) + " x " + juce::String (size.second));
            }
        }

        beginTest ("Every button, slider and combo box uses the shared bevelled look");
        {
            SongsmithEQAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                ed->getDisplay().setSelectedBand (0);   // the band panel's controls too
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *editor, "editor");
        }

        beginTest ("Editor text uses the shared typeface and type scale");
        {
            // The typeface itself is checked by the shared SongsmithUITests.
            for (auto size : { std::pair<int, int> { 1280, 760 }, std::pair<int, int> { EQ::EditorSize::minWidth, 440 } })
            {
                SongsmithEQAudioProcessor p;
                std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
                editor->setSize (size.first, size.second);
                if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                    ed->getDisplay().setSelectedBand (0);   // the band panel's labels and buttons too
                Songsmith::UI::Testing::checkEditorFonts (*this, *editor, juce::String (size.first) + " x " + juce::String (size.second));
            }
        }

        beginTest ("Top-bar text fits at the minimum editor size");
        {
            SongsmithEQAudioProcessor p;
            setParam (p, "auto_gain", 1.0f);
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            editor->setSize (EQ::EditorSize::minWidth, 440);
            if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
                checkLayout (*this, *ed, juce::String (EQ::EditorSize::minWidth) + " x 440");
        }
    }

    // Every visible top-level control sits inside the (scaled) content, no two overlap, and no
    // text is wider than its control in the embedded font. Used by the UI size tests too.
    static void checkLayout (juce::UnitTest& t, SongsmithEQAudioProcessorEditor& ed, const juce::String& context)
    {
        auto& content = ed.getContent();
        auto& lf = ed.getLookAndFeel();
        const auto window = ed.getLocalBounds();
        const auto contentOnWindow = content.getBoundsInParent();   // after the transform
        t.expect (window.expanded (1).contains (contentOnWindow), context + ": content fills the window ("
                  + contentOnWindow.toString() + " in " + window.toString() + ")");

        std::vector<juce::Rectangle<int>> placed;
        for (auto* c : content.getChildren())
        {
            if (! c->isVisible() || dynamic_cast<juce::TooltipWindow*> (c) != nullptr)
                continue;
            const auto what = context + ": " + c->getName() + " " + c->getBounds().toString();
            t.expect (content.getLocalBounds().contains (c->getBounds()), what + " inside the editor");
            if (dynamic_cast<EQDisplay*> (c) != nullptr)
                continue;

            for (auto& r : placed)
                t.expect (! r.intersects (c->getBounds()), what + " overlaps another top-bar control");
            placed.push_back (c->getBounds());

            if (auto* box = dynamic_cast<juce::ComboBox*> (c))
                for (int i = 0; i < box->getNumItems(); ++i)
                    t.expect (juce::GlyphArrangement::getStringWidth (lf.getComboBoxFont (*box), box->getItemText (i))
                                  <= (float) box->getWidth() - 30.0f, context + ": fits: " + box->getItemText (i));
            if (auto* b = dynamic_cast<juce::TextButton*> (c))
                t.expect (juce::GlyphArrangement::getStringWidth (lf.getTextButtonFont (*b, b->getHeight()), b->getButtonText())
                              <= (float) b->getWidth() - 8.0f, context + ": fits: " + b->getButtonText());
            if (auto* l = dynamic_cast<juce::Label*> (c); l != nullptr && l->getText().isNotEmpty())
                t.expect (juce::GlyphArrangement::getStringWidth (lf.getLabelFont (*l), l->getText())
                              <= (float) l->getWidth() - (float) l->getBorderSize().getLeftAndRight() + 0.5f,
                          context + ": label not truncated: " + l->getText());
        }
    }
};

//==============================================================================
// Task 25: bigger default, 4K maximum, UI size setting, proportional layout.
class UiSizeTests final : public juce::UnitTest
{
public:
    UiSizeTests() : juce::UnitTest ("Interface size and resolution", "EQ") {}

    static std::unique_ptr<SongsmithEQAudioProcessorEditor> makeEditor (SongsmithEQAudioProcessor& p, float uiScale, int width, int height)
    {
        p.uiScale = uiScale;
        p.editorWidth = width;
        p.editorHeight = height;
        std::unique_ptr<SongsmithEQAudioProcessorEditor> ed (dynamic_cast<SongsmithEQAudioProcessorEditor*> (p.createEditor()));
        return ed;
    }

    static juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pos)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(),
                                 juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &c, &c, now, pos, now, 1, false);
    }

    void runTest() override
    {
        using namespace EQ::EditorSize;

        beginTest ("Limits: default about 1280 x 788 (760 plus the diagnostics strip), up to 4K, aspect range");
        {
            expectEquals (defaultWidth, 1280);
            expectEquals (defaultHeight, 760 + diagnosticsHeight);
            expectEquals (diagnosticsHeight, Songsmith::Diagnostics::Bar::designHeight, "the editor strip is as tall as the shared bar");
            expect (maxWidth >= 3840 && maxHeight >= 2160, "fills a 4K screen");
            expectEquals (constrain (99999, 99999, 1.0f).x, maxWidth);
            expectEquals (constrain (10, 10, 1.0f).x, minWidth);
            expectEquals (constrain (10, 10, 1.0f).y, minHeight);
            expectEquals (constrain (10, 10, 2.0f).x, minWidth * 2, "the minimum scales with the UI size");
            const auto wide = constrain (3800, 800, 1.0f);
            expectLessOrEqual ((double) wide.x / wide.y, maxAspect + 0.01);
            const auto tall = constrain (900, 2000, 1.0f);
            expectGreaterOrEqual ((double) tall.x / tall.y, minAspect - 0.01);
            expectEquals (snapUiScale (1.3f), 1.25f);
            expectEquals (snapUiScale (9.0f), 2.0f);
            expectEquals (snapUiScale (std::numeric_limits<float>::quiet_NaN()), 1.0f);

            SongsmithEQAudioProcessor p;
            auto ed = makeEditor (p, 1.0f, defaultWidth, defaultHeight);
            expectEquals (ed->getWidth(), 1280);
            expectEquals (ed->getHeight(), 760 + diagnosticsHeight);
            expectEquals (ed->getContentScale(), 1.0f, "no extra scaling at the default size");
        }

        beginTest ("Layout at every UI size, default and minimum window");
        {
            for (int i = 0; i < numUiScales; ++i)
            {
                const float s = uiScales[i];
                for (auto [w, h] : { std::pair<int, int> { defaultWidth, defaultHeight }, std::pair<int, int> { minWidth, minHeight } })
                {
                    SongsmithEQAudioProcessor p;
                    p.prepareToPlay (48000.0, 512);
                    setParam (p, "auto_gain", 1.0f);
                    const int dyn = p.edits.addBand (2500.0f, -2.0f, FilterType::Bell);
                    p.edits.setDynamic (dyn, true);

                    const int ww = juce::roundToInt ((float) w * s), wh = juce::roundToInt ((float) h * s);
                    auto ed = makeEditor (p, s, ww, wh);
                    ed->getDisplay().setSelectedBand (dyn);
                    const auto context = juce::String (juce::roundToInt (s * 100.0f)) + "% at " + juce::String (ww) + " x " + juce::String (wh);

                    expectEquals (ed->getWidth(), ww, context + ": window width");
                    expectEquals (ed->getHeight(), wh, context + ": window height");
                    expectWithinAbsoluteError (ed->getContentScale(), s * ed->getLayoutStep(), 1.0e-6f);
                    // Same design size at every UI size: the layout is identical, just scaled.
                    expectWithinAbsoluteError ((float) ed->getContent().getWidth(), (float) w, 1.0f, context + ": design width");
                    WordingAndFontTests::checkLayout (*this, *ed, context);

                    // The band panel (with its dynamics row) stays inside the graph.
                    for (auto* c : ed->getDisplay().getChildren())
                        if (c->isVisible())
                            expect (ed->getDisplay().getLocalBounds().contains (c->getBounds()),
                                    context + ": band panel inside the graph " + c->getBounds().toString());
                }
            }
        }

        beginTest ("Large windows step up controls and text, but the graph grows more");
        {
            SongsmithEQAudioProcessor p;
            auto ed = makeEditor (p, 1.0f, 3840, 2160);
            expectGreaterThan (ed->getLayoutStep(), 1.4f, "controls grow on a 4K window");
            expectLessOrEqual (ed->getLayoutStep(), 1.75f);
            expectGreaterThan ((float) ed->getContent().getWidth(), (float) defaultWidth * 1.5f, "the graph gets most of the room");

            ed->setSize (1300, 780);
            expectEquals (ed->getLayoutStep(), 1.0f, "near the default size nothing steps up");
        }

        beginTest ("Changing the UI size resizes the window and keeps the layout");
        {
            SongsmithEQAudioProcessor p;
            auto ed = makeEditor (p, 1.0f, 1400, 800);
            const auto designBefore = ed->getContent().getLocalBounds();
            ed->setUiScale (1.5f);
            expectEquals (p.uiScale.load(), 1.5f);
            expectEquals (ed->getWidth(), 2100);
            expectEquals (ed->getHeight(), 1200);
            expect (ed->getContent().getLocalBounds() == designBefore, "same design size: " + ed->getContent().getLocalBounds().toString());
            expectEquals (SongsmithEQAudioProcessor::loadMachineUiScale(), 1.5f, "remembered as this machine's default");

            SongsmithEQAudioProcessor fresh;
            expectEquals (fresh.uiScale.load(), 1.5f, "a new instance opens at the last chosen size");
            expectEquals (fresh.editorWidth.load(), juce::roundToInt (1280 * 1.5f));

            ed->setUiScale (1.0f);
            expectEquals (SongsmithEQAudioProcessor::loadMachineUiScale(), 1.0f);
        }

        beginTest ("UI size and window size round-trip through the state; old states open at 100%");
        {
            SongsmithEQAudioProcessor a;
            a.uiScale = 1.75f;
            a.editorWidth = 2400;
            a.editorHeight = 1400;
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            SongsmithEQAudioProcessor::saveMachineUiScale (0.75f);   // the session must win over this
            SongsmithEQAudioProcessor b;
            expectEquals (b.uiScale.load(), 0.75f);
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.uiScale.load(), 1.75f, "session wins over the machine default");
            expectEquals (b.editorWidth.load(), 2400);
            expectEquals (b.editorHeight.load(), 1400);

            // An old session: no uiScale, a size from the old 980 x 580 default.
            auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            if (xml != nullptr)
            {
                if (auto* ui = xml->getChildByName ("UI"))
                {
                    ui->removeAttribute ("uiScale");
                    ui->setAttribute ("editorWidth", 980);
                    ui->setAttribute ("editorHeight", 580);
                }
                juce::MemoryBlock old;
                juce::AudioProcessor::copyXmlToBinary (*xml, old);
                SongsmithEQAudioProcessor c;
                c.setStateInformation (old.getData(), (int) old.getSize());
                expectEquals (c.uiScale.load(), 1.0f, "old state opens at 100%");
                expectEquals (c.editorWidth.load(), 980, "old state keeps its saved size");
                expectEquals (c.editorHeight.load(), 580);

                // Out of range: clamped.
                if (auto* ui = xml->getChildByName ("UI"))
                {
                    ui->setAttribute ("uiScale", 7.0);
                    ui->setAttribute ("editorWidth", 50000);
                    ui->setAttribute ("editorHeight", 3);
                }
                juce::MemoryBlock bad;
                juce::AudioProcessor::copyXmlToBinary (*xml, bad);
                SongsmithEQAudioProcessor d;
                d.setStateInformation (bad.getData(), (int) bad.getSize());
                expectEquals (d.uiScale.load(), 2.0f, "UI size snapped to an option");
                expectLessOrEqual (d.editorWidth.load(), maxWidth);
                expectGreaterOrEqual (d.editorHeight.load(), minHeight * 2);
                const double aspect = (double) d.editorWidth.load() / d.editorHeight.load();
                expect (aspect >= minAspect - 0.01 && aspect <= maxAspect + 0.01, "aspect clamped: " + juce::String (aspect));
            }
            else
            {
                expect (false, "state XML");
            }
            SongsmithEQAudioProcessor::saveMachineUiScale (1.0f);
        }

        beginTest ("Clicking a node's centre selects it at 75% and at 200%");
        {
            for (float s : { 0.75f, 2.0f })
            {
                SongsmithEQAudioProcessor p;
                p.prepareToPlay (48000.0, 512);
                const int band = p.edits.addBand (1000.0f, 4.0f, FilterType::Bell);
                auto ed = makeEditor (p, s, juce::roundToInt (1280 * s), juce::roundToInt (760 * s));
                auto& display = ed->getDisplay();
                display.setSelectedBand (-1);

                // Node centre in window pixels and back: the transform is applied both ways.
                const auto centre = display.getNodeCentre (band);
                const auto inWindow = ed->getLocalPoint (&display, centre);
                expectWithinAbsoluteError (inWindow.x / s, (float) display.getBoundsInParent().getX() + centre.x, 1.0f,
                                           "window position scales with the UI size");
                const auto back = display.getLocalPoint (ed.get(), inWindow);
                expectEquals (display.findNodeAt (back), band, "hit at the centre");

                // A click 12 design units away still hits (a 24 x 24 target at every size).
                expectEquals (display.findNodeAt (back + juce::Point<float> (11.0f, 0.0f)), band, "24 x 24 target");

                display.mouseDown (mouseAt (display, back));
                display.mouseUp (mouseAt (display, back));
                expectEquals (display.getSelectedBand(), band, juce::String (s) + ": click selects the node");
            }
        }
    }
};

//==============================================================================
// Task 29: double-clicking near the left edge adds a Low Cut, near the right a High Cut.
class EdgeClickTests final : public juce::UnitTest
{
public:
    EdgeClickTests() : juce::UnitTest ("Edge clicks add cuts", "EQ") {}

    static juce::MouseEvent doubleClickAt (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods,
                                 juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &c, &c, now, pos, now, 2, false);
    }

    void runTest() override
    {
        auto check = [this] (float freq, float clickedGain, bool forceBell, FilterType expectedType, const juce::String& what)
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            const int band = ed->getDisplay().addBandAt (freq, clickedGain, forceBell);
            expect (band >= 0, what + ": added");
            if (band < 0)
                return -1;
            expectEquals ((int) p.getBandValue (band, Param::Type), (int) expectedType, what + ": type");
            expectWithinAbsoluteError (p.getBandValue (band, Param::Freq), freq, freq * 0.001f, what + ": frequency");
            const float expectedGain = expectedType == FilterType::Bell ? clickedGain : 0.0f;
            expectWithinAbsoluteError (p.getBandValue (band, Param::Gain), expectedGain, 0.01f, what + ": gain");
            if (EQ::typeHasSlope (expectedType))
            {
                expectEquals ((int) p.getBandValue (band, Param::Slope), EQ::defaultSlope, what + ": default slope");
                expectWithinAbsoluteError (p.getBandValue (band, Param::Q), EQ::defaultQ, 0.01f, what + ": default Q");
            }
            expectEquals (ed->getDisplay().getSelectedBand(), band, what + ": selected");
            return band;
        };

        beginTest ("25 Hz: Low Cut at 0 dB; 16 kHz: High Cut at 0 dB; 1 kHz: Bell at the clicked gain");
        check (25.0f, 6.0f, false, FilterType::LowCut, "25 Hz");
        check (16000.0f, -5.0f, false, FilterType::HighCut, "16 kHz");
        check (1000.0f, 4.5f, false, FilterType::Bell, "1 kHz");

        beginTest ("Zone boundaries: just inside and just outside");
        check (EQ::lowCutZoneHz * 0.98f, 3.0f, false, FilterType::LowCut, "just below the low zone edge");
        check (EQ::lowCutZoneHz * 1.02f, 3.0f, false, FilterType::Bell, "just above the low zone edge");
        check (EQ::highCutZoneHz * 1.02f, 3.0f, false, FilterType::HighCut, "just above the high zone edge");
        check (EQ::highCutZoneHz * 0.98f, 3.0f, false, FilterType::Bell, "just below the high zone edge");
        expectEquals (EQ::lowCutZoneHz, 40.0f);
        expectEquals (EQ::highCutZoneHz, 12000.0f);

        beginTest ("Ctrl/Cmd forces a Bell at the edges");
        check (25.0f, 6.0f, true, FilterType::Bell, "25 Hz forced");
        check (16000.0f, -2.0f, true, FilterType::Bell, "16 kHz forced");

        beginTest ("Stacking: a second Low Cut is added and existing bands are untouched");
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            const int first = ed->getDisplay().addBandAt (30.0f, 0.0f, false);
            p.edits.setValue (first, Param::Freq, 35.0f);
            const int second = ed->getDisplay().addBandAt (25.0f, 0.0f, false);
            expect (second >= 0 && second != first);
            expectEquals ((int) p.getBandValue (second, Param::Type), (int) FilterType::LowCut);
            expectEquals ((int) p.getBandValue (first, Param::Type), (int) FilterType::LowCut);
            expectWithinAbsoluteError (p.getBandValue (first, Param::Freq), 35.0f, 0.01f, "first cut unchanged");
        }

        beginTest ("Undo after an edge add removes the band in one step");
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            const int usedBefore = p.getNumUsedBands();
            const int band = ed->getDisplay().addBandAt (25.0f, 6.0f, false);
            expectEquals (p.getNumUsedBands(), usedBefore + 1);
            p.edits.undo();
            expectEquals (p.getNumUsedBands(), usedBefore, "one undo");
            expect (! p.isBandUsed (band));
            p.edits.redo();
            expectEquals ((int) p.getBandValue (band, Param::Type), (int) FilterType::LowCut, "redo restores the cut");
        }

        beginTest ("Real double-clicks at the edges and the middle of the graph");
        {
            SongsmithEQAudioProcessor p;
            for (int b = 0; b < numBands; ++b)
                if (p.isBandUsed (b))
                    p.edits.removeBand (b);
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            auto& display = ed->getDisplay();
            const float plotRight = (float) display.getWidth() - 36.0f, midY = ((float) display.getHeight() - 20.0f) * 0.5f;

            auto typeAfterClick = [&] (float x, float y, juce::ModifierKeys mods)
            {
                display.mouseDoubleClick (doubleClickAt (display, { x, y }, mods));
                return (int) p.getBandValue (display.getSelectedBand(), Param::Type);
            };
            expectEquals (typeAfterClick (4.0f, midY, {}), (int) FilterType::LowCut, "far left");
            expectEquals (typeAfterClick (plotRight - 4.0f, midY, {}), (int) FilterType::HighCut, "far right");
            expectEquals (typeAfterClick (plotRight * 0.5f, midY, {}), (int) FilterType::Bell, "middle");
            expectEquals (typeAfterClick (8.0f, midY + 80.0f, juce::ModifierKeys (juce::ModifierKeys::commandModifier)), (int) FilterType::Bell,
                          "Ctrl/Cmd at the far left");
            expectEquals (p.getNumUsedBands(), 4);
        }
    }
};

//==============================================================================
// Task 27: tall vertical output meter on the right of the graph.
class OutputMeterTests final : public juce::UnitTest
{
public:
    OutputMeterTests() : juce::UnitTest ("Vertical output meter", "EQ") {}

    void runTest() override
    {
        beginTest ("Level to height: -60 dB at the bottom, +6 dB at the top, 0 dB on the scale mark");
        {
            OutputMeterDisplay m;
            m.setSize (OutputMeterDisplay::preferredWidth, 400);
            const auto bar = m.getBarArea();
            expectWithinAbsoluteError (m.levelToY (-60.0f), bar.getBottom(), 1.0e-4f);
            expectWithinAbsoluteError (m.levelToY (6.0f), bar.getY(), 1.0e-4f);
            expectWithinAbsoluteError (m.levelToY (0.0f), bar.getBottom() - bar.getHeight() * 60.0f / 66.0f, 1.0e-3f);
            expectWithinAbsoluteError (m.levelToY (-100.0f), bar.getBottom(), 1.0e-4f, "below the range: empty");
            expectWithinAbsoluteError (m.levelToY (20.0f), bar.getY(), 1.0e-4f, "above the range: full");
            expectLessThan (m.levelToY (-6.0f), m.levelToY (-12.0f), "fills bottom to top");
            expect (bar.getBottom() <= (float) m.getHeight() - OutputMeterDisplay::bottomRow + 0.01f,
                    "the peak number has the frequency-axis row");
            expect (m.getClipBox().getBottom() < bar.getY(), "clip indicator above the bar");
            expectEquals ((int) std::size (OutputMeterDisplay::scaleMarks), 6);
        }

        beginTest ("The display shows the audio thread's latched CLIP warning and a click does not clear it");
        {
            OutputMeterDisplay m;
            m.setSize (OutputMeterDisplay::preferredWidth, 300);
            m.update (0.1f, 0.1f, 2, false, 0.0, 0.0);
            expect (! m.isClipped());
            m.update (1.2f, 0.1f, 2, true, 0.1, 0.1);
            expect (m.isClipped());
            expectGreaterThan (m.getHoldDb(), 1.0f);
            m.update (0.01f, 0.01f, 2, true, 0.2, 0.1);
            expect (m.isClipped(), "stays lit after the signal drops");
            expectGreaterThan (m.getHoldDb(), 1.0f, "peak held");
            m.update (0.01f, 0.01f, 2, true, 5.0, 1.0);
            expect (m.isClipped(), "still lit seconds later");
            expectLessThan (m.getHoldDb(), 1.0f, "hold falls after 1.5 s");

            m.mouseDown (UiSizeTests::mouseAt (m, m.getClipBox().getCentre()));
            expect (m.isClipped(), "a click doesn't clear the warning");
            m.update (0.0f, 0.0f, 2, false, 6.0, 1.0);
            expect (! m.isClipped(), "it clears when the audio thread says the sound has stopped");
        }

        beginTest ("One bar for mono, two side-by-side bars for stereo, each with its own level");
        {
            OutputMeterDisplay m;
            m.setSize (OutputMeterDisplay::preferredWidth, 400);
            m.update (0.5f, 0.0f, 1, false, 0.0, 0.0);
            expectEquals (m.getNumChannels(), 1);
            expect (m.getChannelBar (0) == m.getBarArea(), "mono uses the whole bar area");

            m.update (0.5f, 0.05f, 2, false, 0.1, 0.1);
            expectEquals (m.getNumChannels(), 2);
            const auto left = m.getChannelBar (0), right = m.getChannelBar (1);
            expect (left.getRight() < right.getX(), "left bar beside right bar");
            expectWithinAbsoluteError (left.getWidth(), right.getWidth(), 1.0e-4f);
            expect (m.getBarArea().contains (left) && m.getBarArea().contains (right), "both inside the bar area");
            expectWithinAbsoluteError (m.getLevelDb (0), juce::Decibels::gainToDecibels (0.5f), 0.01f, "left level");
            expectWithinAbsoluteError (m.getLevelDb (1), juce::Decibels::gainToDecibels (0.05f), 0.01f, "right level");

            m.update (0.0f, 0.9f, 2, false, 0.2, 0.0);   // right only
            expectWithinAbsoluteError (m.getChannelHoldDb (1), juce::Decibels::gainToDecibels (0.9f), 0.01f);
            expect (m.getChannelHoldDb (0) < juce::Decibels::gainToDecibels (0.5f) + 0.01f, "left unaffected by right");
            expect (m.getClipBox().getBottom() < m.getBarArea().getY(), "CLIP sits above the bars");
        }

        beginTest ("Ballistics: fast attack, about 24 dB/s fall-off");
        {
            OutputMeterDisplay m;
            m.update (0.5f, 0.5f, 2, false, 0.0, 0.0);
            expectWithinAbsoluteError (m.getLevelDb(), juce::Decibels::gainToDecibels (0.5f), 0.01f);
            m.update (0.0f, 0.0f, 2, false, 0.5, 0.5);
            expectWithinAbsoluteError (m.getLevelDb(), juce::Decibels::gainToDecibels (0.5f) - 12.0f, 0.01f);
        }

        beginTest ("Meter sits right of the graph, full height, at the minimum, default and largest sizes");
        {
            using namespace EQ::EditorSize;
            struct Case { float scale; int w, h; };
            for (auto c : { Case { 1.0f, minWidth, minHeight }, Case { 1.0f, defaultWidth, defaultHeight },
                            Case { 1.0f, maxWidth, 2160 }, Case { 2.0f, minWidth * 2, minHeight * 2 },
                            Case { 0.75f, juce::roundToInt (defaultWidth * 0.75f), juce::roundToInt (defaultHeight * 0.75f) } })
            {
                SongsmithEQAudioProcessor p;
                auto ed = UiSizeTests::makeEditor (p, c.scale, c.w, c.h);
                const auto context = juce::String (c.w) + " x " + juce::String (c.h) + " at " + juce::String (c.scale);
                const auto meter = ed->getOutputMeter().getBounds();
                const auto graph = ed->getDisplay().getBounds();
                expect (meter.getX() >= graph.getRight(), context + ": meter right of the graph");
                expect (! meter.intersects (graph), context + ": no overlap with the graph");
                expectEquals (meter.getY(), graph.getY(), context + ": same top as the graph");
                expectEquals (meter.getBottom(), graph.getBottom(), context + ": same bottom as the graph");
                expectEquals (meter.getWidth(), OutputMeterDisplay::preferredWidth);
                expect (ed->getContent().getLocalBounds().contains (meter), context + ": inside the editor");
                WordingAndFontTests::checkLayout (*this, *ed, context);
            }
        }
    }
};

//==============================================================================
// EQ-39: per-channel output peaks, the channel count, and the latched CLIP warning.
class OutputMeterClipTests final : public juce::UnitTest
{
public:
    OutputMeterClipTests() : juce::UnitTest ("Output meter channels and latched clip warning", "EQ") {}

    using Set = juce::AudioChannelSet;

    static juce::AudioProcessor::BusesLayout layout (Set in, Set out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.inputBuses.add (Set::disabled());   // the sidechain, off
        l.outputBuses.add (out);
        return l;
    }

    // Feeds 'blocks' blocks of constant-amplitude (alternating-sign) samples per channel.
    static void feed (SongsmithEQAudioProcessor& p, int numCh, float left, float right, int blocks, int block = 512)
    {
        juce::AudioBuffer<float> buffer (numCh, block);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float sign = (i & 1) ? -1.0f : 1.0f;
                buffer.setSample (0, i, left * sign);
                if (numCh > 1)
                    buffer.setSample (1, i, right * sign);
            }
            p.processBlock (buffer, midi);
        }
    }

    static SongsmithEQAudioProcessor& flat (SongsmithEQAudioProcessor& p)
    {
        for (int b = 0; b < numBands; ++b)
            p.setBandUsed (b, false);
        return p;
    }

    void runTest() override
    {
        constexpr double fs = 48000.0;
        const int secondsOfBlocks = (int) (fs / 512.0) + 2;   // a little over a second

        beginTest ("Stereo reports two channels and each channel's own peak; mono reports one");
        {
            SongsmithEQAudioProcessor p;
            flat (p).prepareToPlay (fs, 512);
            expectEquals (p.outputMeter.channels.load(), 2);
            feed (p, 2, 0.5f, 0.0f, 4);
            expectWithinAbsoluteError (p.outputMeter.readAndResetChannel (0), 0.5f, 0.01f, "left only");
            expectLessThan (p.outputMeter.readAndResetChannel (1), 1.0e-4f, "right silent");
            feed (p, 2, 0.0f, 0.25f, 4);
            expectLessThan (p.outputMeter.readAndResetChannel (0), 1.0e-4f, "left silent");
            expectWithinAbsoluteError (p.outputMeter.readAndResetChannel (1), 0.25f, 0.01f, "right only");

            SongsmithEQAudioProcessor m;
            expect (m.setBusesLayout (layout (Set::mono(), Set::mono())), "mono layout accepted");
            flat (m).prepareToPlay (fs, 512);
            expectEquals (m.outputMeter.channels.load(), 1);
            feed (m, 1, 0.5f, 0.0f, 4);
            expectWithinAbsoluteError (m.outputMeter.readAndResetChannel (0), 0.5f, 0.01f);
            expectEquals (m.outputMeter.channels.load(), 1);

            m.releaseResources();
            expect (m.setBusesLayout (layout (Set::stereo(), Set::stereo())), "back to stereo");
            m.prepareToPlay (fs, 512);
            expectEquals (m.outputMeter.channels.load(), 2, "layout change is noticed");
        }

        beginTest ("A level below 0 dBFS does not show CLIP; a full-scale sample on either channel does");
        {
            SongsmithEQAudioProcessor p;
            flat (p).prepareToPlay (fs, 512);
            feed (p, 2, 0.9f, 0.9f, 8);
            expect (! p.outputMeter.clipLatched.load(), "no CLIP at -0.9 dBFS");

            SongsmithEQAudioProcessor l;
            flat (l).prepareToPlay (fs, 512);
            feed (l, 2, 1.0f, 0.1f, 1);
            expect (l.outputMeter.clipLatched.load(), "left channel clip");

            SongsmithEQAudioProcessor r;
            flat (r).prepareToPlay (fs, 512);
            feed (r, 2, 0.1f, 1.0f, 1);
            expect (r.outputMeter.clipLatched.load(), "right channel clip");

            SongsmithEQAudioProcessor t;   // a single-sample transient
            flat (t).prepareToPlay (fs, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            buffer.clear();
            buffer.setSample (1, 200, -1.0f);
            t.processBlock (buffer, midi);
            expect (t.outputMeter.clipLatched.load(), "one clipped sample is enough");
        }

        beginTest ("CLIP stays on through quiet-but-audible sound and short pauses, and clears after sustained silence");
        {
            SongsmithEQAudioProcessor p;
            flat (p).prepareToPlay (fs, 512);
            feed (p, 2, 1.0f, 1.0f, 2);
            expect (p.outputMeter.clipLatched.load(), "latched");

            feed (p, 2, 0.01f, 0.01f, secondsOfBlocks * 3);   // -40 dBFS for three seconds
            expect (p.outputMeter.clipLatched.load(), "quiet but audible keeps it lit");

            feed (p, 2, 0.0f, 0.0f, 40);   // about 0.43 s of silence
            expect (p.outputMeter.clipLatched.load(), "a short pause doesn't clear it");
            feed (p, 2, 0.01f, 0.01f, 2);
            feed (p, 2, 0.0f, 0.0f, 40);
            expect (p.outputMeter.clipLatched.load(), "the pause timer restarts when sound returns");

            feed (p, 2, 0.0f, 0.0f, secondsOfBlocks);   // more than 0.75 s of silence
            expect (! p.outputMeter.clipLatched.load(), "sustained silence clears it");

            feed (p, 2, 0.5f, 0.5f, 4);
            expect (! p.outputMeter.clipLatched.load(), "new playback starts clean");
            feed (p, 2, 1.0f, 1.0f, 1);
            expect (p.outputMeter.clipLatched.load(), "and a new clip latches again");
        }

        beginTest ("One active stereo channel stops the warning clearing");
        {
            SongsmithEQAudioProcessor p;
            flat (p).prepareToPlay (fs, 512);
            feed (p, 2, 1.0f, 1.0f, 1);
            feed (p, 2, 0.0f, 0.05f, secondsOfBlocks * 2);   // left silent, right audible
            expect (p.outputMeter.clipLatched.load(), "right is still sounding");
            feed (p, 2, 0.0f, 0.0f, secondsOfBlocks);
            expect (! p.outputMeter.clipLatched.load(), "both silent: cleared");
        }

        beginTest ("Preparing again, as after reopening a session or changing layout, clears a stale warning");
        {
            SongsmithEQAudioProcessor p;
            flat (p).prepareToPlay (fs, 512);
            feed (p, 2, 1.0f, 1.0f, 1);
            expect (p.outputMeter.clipLatched.load());
            p.prepareToPlay (fs, 512);
            expect (! p.outputMeter.clipLatched.load(), "no stale warning");
        }

        beginTest ("The meter doesn't change the sound");
        {
            SongsmithEQAudioProcessor p;
            p.prepareToPlay (fs, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            for (int i = 0; i < 512; ++i)
            {
                buffer.setSample (0, i, 0.3f * std::sin (0.05f * (float) i));
                buffer.setSample (1, i, 0.2f * std::sin (0.07f * (float) i));
            }
            auto copy = buffer;
            p.processBlock (buffer, midi);
            SongsmithEQAudioProcessor q;
            q.prepareToPlay (fs, 512);
            q.processBlock (copy, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    expectEquals (buffer.getSample (c, i), copy.getSample (c, i));
        }
    }
};

//==============================================================================
// Task 28 / EQ-35: a "Piano" toggle at the far right of the footer for the piano strip.
class PianoToggleTests final : public juce::UnitTest
{
public:
    PianoToggleTests() : juce::UnitTest ("Piano strip toggle", "EQ") {}

    static juce::TextButton* findButton (juce::Component& content, const juce::String& text)
    {
        for (auto* c : content.getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
                return b;
            if (dynamic_cast<Songsmith::Diagnostics::Bar*> (c) != nullptr)   // the footer strip (EQ-35)
                if (auto* b = findButton (*c, text))
                    return b;
        }
        return nullptr;
    }

    static juce::ComboBox* findCombo (juce::Component& content, const juce::String& firstItem)
    {
        for (auto* c : content.getChildren())
            if (auto* b = dynamic_cast<juce::ComboBox*> (c); b != nullptr && b->getNumItems() > 0 && b->getItemText (0) == firstItem)
                return b;
        return nullptr;
    }

    void runTest() override
    {
        beginTest ("The button turns the strip on and off, and stays in step with the menu setting");
        {
            SongsmithEQAudioProcessor p;
            expect (! p.showPianoStrip.load(), "off by default");
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            auto* piano = findButton (ed->getContent(), "Piano");
            expect (piano != nullptr, "Piano button exists");
            if (piano == nullptr)
                return;
            expect (piano->isVisible());
            expect (! piano->getToggleState());

            piano->setToggleState (! piano->getToggleState(), juce::sendNotificationSync);   // as a click does
            expect (piano->getToggleState());
            expect (p.showPianoStrip.load(), "button turns it on");

            juce::MemoryBlock state;
            p.getStateInformation (state);
            SongsmithEQAudioProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expect (restored.showPianoStrip.load(), "saved with the session");
            auto ed2 = UiSizeTests::makeEditor (restored, 1.0f, 1280, 760);
            auto* piano2 = findButton (ed2->getContent(), "Piano");
            expect (piano2 != nullptr && piano2->getToggleState(), "a reopened session shows the button on");

            // As the right-click menu does: the button follows on the next timer tick.
            p.showPianoStrip.store (false);
            ed->refreshFromProcessor();
            expect (! piano->getToggleState(), "menu change updates the button");

            piano->setToggleState (! piano->getToggleState(), juce::sendNotificationSync);   // as a click does
            piano->setToggleState (! piano->getToggleState(), juce::sendNotificationSync);   // as a click does
            expect (! p.showPianoStrip.load(), "a second click turns it off");
            p.getStateInformation (state);
            SongsmithEQAudioProcessor restoredOff;
            restoredOff.showPianoStrip.store (true);
            restoredOff.setStateInformation (state.getData(), (int) state.getSize());
            expect (! restoredOff.showPianoStrip.load(), "off round-trips too");
        }

        beginTest ("The Piano button sits at the far right of the footer, beside the strip it controls (EQ-35)");
        {
            using namespace EQ::EditorSize;
            for (auto s : uiScales)
            {
                SongsmithEQAudioProcessor p;
                const int w = juce::roundToInt ((float) minWidth * s), h = juce::roundToInt ((float) minHeight * s);
                auto ed = UiSizeTests::makeEditor (p, s, w, h);
                const auto context = juce::String (juce::roundToInt (s * 100.0f)) + "% at " + juce::String (w) + " x " + juce::String (h);
                auto& content = ed->getContent();
                auto* piano = findButton (content, "Piano");
                expect (piano != nullptr);
                if (piano == nullptr)
                    continue;
                auto* footer = piano->getParentComponent();
                expect (dynamic_cast<Songsmith::Diagnostics::Bar*> (footer) != nullptr, context + ": the button lives in the footer strip");
                if (footer == nullptr)
                    continue;
                const auto inContent = piano->getBounds() + footer->getPosition();
                expect (piano->isVisible(), context + ": the button is shown");
                expectEquals (inContent.getRight(), content.getWidth() - 16, context + ": right-aligned at the far right");
                expect (inContent.getY() >= ed->getDisplay().getBottom(), context + ": below the graph and its piano-note strip");
                expect (inContent.getX() > content.getWidth() / 2, context + ": in the right half");
                expect (findButton (content, "Piano") == piano, context + ": only one Piano button");
                WordingAndFontTests::checkLayout (*this, *ed, context);
            }
        }

        beginTest ("Turning the strip on doesn't move the graph or the nodes");
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 6.0f, FilterType::Bell);
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            const auto before = ed->getDisplay().getBounds();
            const auto nodeBefore = ed->getDisplay().getNodeCentre (band);
            p.showPianoStrip.store (true);
            ed->refreshFromProcessor();
            expect (ed->getDisplay().getBounds() == before);
            expect (ed->getDisplay().getNodeCentre (band) == nodeBefore);
        }
    }
};

//==============================================================================
// The latency and Processing Load strip (suite task S11).
class DiagnosticsTests final : public juce::UnitTest
{
public:
    DiagnosticsTests() : juce::UnitTest ("Latency and Processing Load", "EQ") {}

    void runTest() override
    {
        using namespace Songsmith::Diagnostics;

        beginTest ("The strip shows the reported latency in every latency-changing mode and sample rate");
        {
            int distinct = 0, last = -1;
            for (double fs : { 44100.0, 48000.0, 96000.0 })
                for (int phase : { 0, 1 })
                    for (int oversampling : { 0, 1, 2 })
                    {
                        SongsmithEQAudioProcessor p;
                        setParam (p, "phase_mode", (float) phase);
                        setParam (p, "oversampling", (float) oversampling);
                        p.setRateAndBufferSizeDetails (fs, 512);
                        p.prepareToPlay (fs, 512);
                        p.updateLatencyNow();

                        const juce::String context = juce::String (phase ? "linear phase" : "zero latency") + ", oversampling "
                                                     + juce::String (oversampling) + ", " + juce::String (fs) + " Hz";
                        Testing::checkLatencyDisplay (*this, p, context);
                        if (p.getLatencySamples() != last) { ++distinct; last = p.getLatencySamples(); }
                    }
            expectGreaterThan (distinct, 3, "the modes really do report different latencies");

            SongsmithEQAudioProcessor zero;
            zero.setRateAndBufferSizeDetails (48000.0, 512);
            zero.prepareToPlay (48000.0, 512);
            expectEquals (zero.getLatencySamples(), 0, "the default mode adds no delay");
        }

        beginTest ("The strip follows a mode change while it is open");
        {
            Testing::TemporarySettings settings;
            SongsmithEQAudioProcessor p;
            p.setRateAndBufferSizeDetails (48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            LoadMeter meter;
            Bar bar (p, meter, JucePlugin_Name);

            bar.refresh();
            expectEquals (Testing::parseLatency (bar.getLatencyText()).samples, 0, "zero latency to begin with");

            setParam (p, "oversampling", 2.0f);
            p.prepareToPlay (48000.0, 512);
            p.updateLatencyNow();
            bar.refresh();
            expectGreaterThan (Testing::parseLatency (bar.getLatencyText()).samples, 0, "oversampling adds delay and the strip shows it");
            expectEquals (Testing::parseLatency (bar.getLatencyText()).samples, p.getLatencySamples());

            p.setRateAndBufferSizeDetails (96000.0, 512);
            p.prepareToPlay (96000.0, 512);
            p.updateLatencyNow();
            bar.refresh();
            const auto shown = Testing::parseLatency (bar.getLatencyText());
            expectWithinAbsoluteError (shown.milliseconds, 1000.0 * p.getLatencySamples() / 96000.0, 0.051, "the milliseconds follow the new sample rate");
        }

        beginTest ("Processing Load is off until switched on, and measures real blocks at every size");
        {
            Testing::TemporarySettings settings;
            SongsmithEQAudioProcessor p;
            expect (! p.loadMeter.isEnabled(), "off by default");
            Testing::checkLoadMeasured (*this, p, p.loadMeter, "EQ");
            Testing::checkFooterLayout (*this, p, JucePlugin_Name);

            Bar bar (p, p.loadMeter, JucePlugin_Name);
            expectEquals (bar.getLoadText(), juce::String ("Off"));
            bar.getLoadSwitch().setToggleState (true, juce::sendNotificationSync);   // runs the click handler now
            expect (p.loadMeter.isEnabled(), "the switch turns timing on");
            expectEquals (bar.getLoadText(), juce::String ("Unavailable"), "no block yet");
            expect (Settings::loadEnabled (JucePlugin_Name), "the choice is saved to the machine setting, not the session");
        }

        beginTest ("The load meter leaves the saved session alone");
        {
            SongsmithEQAudioProcessor p;
            juce::MemoryBlock before, after;
            p.getStateInformation (before);
            p.loadMeter.setEnabled (true);
            p.getStateInformation (after);
            expect (before == after, "timing on or off changes nothing in the session state");
        }
    }
};
//==============================================================================
// EQ-36: the Dynamic button explains why it is greyed out, and the explanation can be hovered.
class DynamicTooltipTests final : public juce::UnitTest
{
public:
    DynamicTooltipTests() : juce::UnitTest ("Dynamic button tooltip", "EQ") {}

    static juce::TextButton* findDynamic (juce::Component& c)
    {
        if (auto* b = dynamic_cast<juce::TextButton*> (&c))
            if (b->getButtonText() == "Dynamic")
                return b;
        for (auto* child : c.getChildren())
            if (auto* b = findDynamic (*child))
                return b;
        return nullptr;
    }

    void runTest() override
    {
        using namespace EQ;

        beginTest ("Each unsupported filter type is named, and Bell, Low Shelf and High Shelf are offered");
        for (auto t : { FilterType::LowCut, FilterType::HighCut, FilterType::Notch })
        {
            const auto tip = dynamicTooltip (t, false);
            expect (tip.contains (getTypeNames()[(int) t] + " filters"), tip);
            expect (tip.contains ("Bell, Low Shelf or High Shelf"), tip);
            expect (! tip.contains ("Linear phase"), "no false mention of the mode: " + tip);
        }
        expectEquals (dynamicTooltip (FilterType::LowCut, false),
                      juce::String ("Dynamic EQ isn't available on Low Cut filters. Use Bell, Low Shelf or High Shelf to enable dynamic processing."));

        beginTest ("Linear phase gives the mode explanation, and both when the filter is also unsupported");
        for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf })
        {
            const auto tip = dynamicTooltip (t, true);
            expect (tip.contains ("Linear phase mode") && tip.contains ("Zero latency"), tip);
            expect (! tip.contains ("filters or"), tip);
        }
        const auto both = dynamicTooltip (FilterType::Notch, true);
        expect (both.contains ("Notch filters or in Linear phase mode") && both.contains ("Zero latency"), both);

        beginTest ("Supported filters in the normal mode keep the ordinary description");
        for (auto t : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf })
            expect (dynamicTooltip (t, false).startsWith ("Dynamic EQ changes this band's gain"));

        beginTest ("The greyed-out button is hoverable and shows the right tip");
        for (int ti = 0; ti < numFilterTypes; ++ti)
            for (int lp = 0; lp < 2; ++lp)
            {
                SongsmithEQAudioProcessor p;
                const auto type = typeFromIndex (ti);
                const int band = p.edits.addBand (1000.0f, 0.0f, type);
                auto* rp = p.apvts.getParameter (phaseModeId);
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) lp));

                BandPanel panel (p);
                panel.setBand (band);
                panel.setSize (BandPanel::panelWidth, panel.getPreferredHeight());
                panel.setVisible (true);   // hit-testing ignores components that are not visible
                auto* dyn = findDynamic (panel);
                expect (dyn != nullptr);
                if (dyn == nullptr) continue;

                const bool shouldBeEnabled = typeHasGain (type) && lp == 0;
                const auto context = getTypeNames()[ti] + (lp ? ", linear phase" : ", zero latency");
                expect (dyn->isEnabled() == shouldBeEnabled, context);
                expectEquals (dyn->getTooltip(), dynamicTooltip (type, lp == 1), context);

                // What JUCE's TooltipWindow asks for: the component under the mouse.
                auto* hit = panel.getComponentAt (dyn->getBounds().getCentre());
                expect (hit == dyn, context + ": the button is under the pointer even when disabled");
                if (auto* ttc = dynamic_cast<juce::TooltipClient*> (hit))
                    expectEquals (ttc->getTooltip(), dynamicTooltip (type, lp == 1), context);

            }
    }
};
static DynamicTooltipTests dynamicTooltipTests;

//==============================================================================
// SUI-13: global Bypass is top-right and uses the suite's Bypass red.
class GlobalBypassTests final : public juce::UnitTest
{
public:
    GlobalBypassTests() : juce::UnitTest ("Global Bypass", "EQ") {}

    void runTest() override
    {
        beginTest ("Bypass is top-right, uses the shared red, and follows the parameter");
        for (float s : { 1.0f, 2.0f })
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, s, juce::roundToInt (1280 * s), juce::roundToInt (788 * s));
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (EQ::bypassId), "EQ at " + juce::String (s) + "x");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass; Gain Scale stays reachable in the toolbox (SUI-18)");
        for (float s : { 1.0f, 2.0f })
        {
            SongsmithEQAudioProcessor p;
            const auto context = "EQ at " + juce::String (s) + "x";
            p.apvts.getParameter (EQ::gainScaleId)->setValueNotifyingHost (p.apvts.getParameter (EQ::gainScaleId)->convertTo0to1 (150.0f));
            auto ed = UiSizeTests::makeEditor (p, s, juce::roundToInt (1280 * s), juce::roundToInt (788 * s));
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, context);

            if (auto* eqEditor = dynamic_cast<SongsmithEQAudioProcessorEditor*> (ed.get()))
            {
                auto& scale = eqEditor->getScaleSlider();
                expect (scale.isVisible() && scale.getY() > 42, context + ": the Scale control is out of the title bar and still visible");
                expectWithinAbsoluteError (scale.getValue(), 150.0, 0.5, context + ": a saved non-default Gain Scale is kept, not reset to 100 %");
                expectWithinAbsoluteError ((double) p.getGainScale(), 1.5, 0.01, context + ": and the sound still uses it");

                // EQ-41: the label says Gain Scale, typing works with or without the words, the help is plain.
                expect (scale.getTextFromValue (100.0) == "Gain Scale 100%", context + ": reads Gain Scale 100%");
                expect (scale.getTextFromValue (-100.0) == "Gain Scale -100%", context + ": negative end");
                for (auto text : { "50", "50%", "Gain Scale 50%" })
                    expectWithinAbsoluteError (scale.getValueFromText (text), 50.0, 0.001, context + ": typing \"" + juce::String (text) + "\" sets 50");
                expect (scale.getTooltip().startsWith ("Turns every band's boosts and cuts up or down together."), context + ": plain tooltip");
                expect (scale.getTooltip().contains ("Double-click to return to 100%."), context + ": tooltip mentions reset");
                const auto* editorBounds = ed.get();
                expect (scale.getRight() <= editorBounds->getWidth() && scale.getWidth() > 60, context + ": fits inside the editor");            }
        }
    }
};
static GlobalBypassTests globalBypassTests;

// EQ-33: each filter type shows a small picture of its response.
class FilterShapeIconTests final : public juce::UnitTest
{
public:
    FilterShapeIconTests() : juce::UnitTest ("Filter shape icons", "EQ") {}

    void runTest() override
    {
        using namespace EQ;
        using namespace EQ::FilterShapes;

        beginTest ("Each curve has the right shape from low to high frequency");
        {
            const auto lo = [] (FilterType t) { return shape (t, 0.0f); };
            const auto mid = [] (FilterType t) { return shape (t, 0.5f); };
            const auto hi = [] (FilterType t) { return shape (t, 1.0f); };
            expect (mid (FilterType::Bell) > lo (FilterType::Bell) + 0.3f && mid (FilterType::Bell) > hi (FilterType::Bell) + 0.3f, "Bell peaks in the middle");
            expect (lo (FilterType::LowShelf) > hi (FilterType::LowShelf) + 0.3f, "Low Shelf is raised on the left");
            expect (hi (FilterType::HighShelf) > lo (FilterType::HighShelf) + 0.3f, "High Shelf is raised on the right");
            expect (hi (FilterType::LowCut) > lo (FilterType::LowCut) + 0.5f, "Low Cut rises from the left");
            expect (lo (FilterType::HighCut) > hi (FilterType::HighCut) + 0.5f, "High Cut falls to the right");
            expect (mid (FilterType::Notch) < lo (FilterType::Notch) - 0.5f && mid (FilterType::Notch) < hi (FilterType::Notch) - 0.5f, "Notch dips in the middle");
        }

        beginTest ("The six pictures are all different and stay inside their box");
        for (int a = 0; a < numFilterTypes; ++a)
        {
            const auto box = juce::Rectangle<float> (0.0f, 0.0f, (float) iconWidth, (float) iconHeight);
            expect (box.expanded (0.01f).contains (curve (typeFromIndex (a), box.reduced (strokeWidth)).getBounds()), "inside the box");
            for (int b = a + 1; b < numFilterTypes; ++b)
            {
                float difference = 0.0f;
                for (int i = 0; i <= 20; ++i)
                    difference += std::abs (shape (typeFromIndex (a), i / 20.0f) - shape (typeFromIndex (b), i / 20.0f));
                expect (difference > 1.0f, getTypeNames()[a] + " and " + getTypeNames()[b] + " must look different");
            }
        }

        beginTest ("Icons actually draw, at normal and double size");
        for (float scale : { 1.0f, 2.0f })
            for (int t = 0; t < numFilterTypes; ++t)
            {
                juce::Image img (juce::Image::ARGB, (int) (iconWidth * scale), (int) (iconHeight * scale), true);
                {
                    juce::Graphics g (img);
                    g.addTransform (juce::AffineTransform::scale (scale));
                    makeDrawable (typeFromIndex (t), juce::Colours::white, (float) iconWidth, (float) iconHeight)->draw (g, 1.0f);
                }
                int lit = 0;
                for (int y = 0; y < img.getHeight(); ++y)
                    for (int x = 0; x < img.getWidth(); ++x)
                        lit += img.getPixelAt (x, y).getAlpha() > 128 ? 1 : 0;
                expect (lit > 15 * scale, getTypeNames()[t] + " draws something visible, lit=" + juce::String (lit));
            }

        beginTest ("The band panel shows the picture beside the selected name, keeping selection behaviour");
        for (int t = 0; t < numFilterTypes; ++t)
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 0.0f, typeFromIndex (t));
            BandPanel panel (p);
            panel.setBand (band);
            panel.setSize (BandPanel::panelWidth, panel.getPreferredHeight());
            panel.setVisible (true);

            FilterShapeIcon* icon = nullptr;
            juce::ComboBox* box = nullptr;
            for (auto* c : panel.getChildren())
            {
                if (auto* i = dynamic_cast<FilterShapeIcon*> (c)) icon = i;
                if (auto* b = dynamic_cast<juce::ComboBox*> (c); b != nullptr && b->getNumItems() == numFilterTypes
                                                                  && b->getItemText (0) == getTypeNames()[0]) box = b;
            }
            expect (icon != nullptr && box != nullptr);
            if (icon == nullptr || box == nullptr) continue;

            expect (icon->getType() == typeFromIndex (t), "the icon follows the band's type");
            expect (box->getBounds().contains (icon->getBounds()), "the icon sits inside the type box");
            expect (icon->getRight() < box->getX() + (int) box->getProperties()["textIndent"], "the name starts after the icon");
            { bool onSelf = true, onChildren = true; icon->getInterceptsMouseClicks (onSelf, onChildren); expect (! onSelf, "the icon never steals clicks"); }
            expectEquals (box->getSelectedItemIndex(), t, "selection unchanged");
            expectEquals (box->getText(), getTypeNames()[t], "the name stays visible");

            int count = 0;
            for (juce::PopupMenu::MenuItemIterator it (*box->getRootMenu()); it.next(); ++count)
            {
                expect (it.getItem().image != nullptr, "every list entry has a picture");
                expectEquals (it.getItem().text, getTypeNames()[count], "list order unchanged");
            }
            expectEquals (count, numFilterTypes);
        }
    }
};
static FilterShapeIconTests filterShapeIconTests;

// EQ-34: the processing-mode selector explains Linear phase in plain words.
class LinearPhaseTooltipTests final : public juce::UnitTest
{
public:
    LinearPhaseTooltipTests() : juce::UnitTest ("Linear phase tooltip", "EQ") {}

    void runTest() override
    {
        beginTest ("The tip says what it does, when to try it, why it adds delay and what to use for recording");
        {
            const juce::String tip (EQ::linearPhaseTooltip());
            expect (tip.contains ("blending a filtered track with a copy of the same sound"), "when to try it");
            expect (tip.contains ("timing of some frequencies"), "what it does");
            expect (tip.contains ("adds delay"), "why latency");
            expect (tip.contains ("recording") && tip.contains ("Zero latency"), "what to use instead");
            expect (tip.length() < 400, "short enough to read at a glance");
        }

        beginTest ("The editor shows that tip on the mode selector");
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 760);
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            bool found = false;
            for (auto& t : texts)
                found = found || t == juce::String (EQ::linearPhaseTooltip());
            expect (found, "the selector carries the tooltip");
        }
    }
};
static LinearPhaseTooltipTests linearPhaseTooltipTests;

//==============================================================================
// EQ-37: the floating band panel can be dragged and always stays inside the graph.
class BandPanelDragTests final : public juce::UnitTest
{
public:
    BandPanelDragTests() : juce::UnitTest ("Movable band panel", "EQ") {}

    static bool inside (juce::Rectangle<int> panel, juce::Rectangle<int> limit)
    {
        return limit.expanded (1).contains (panel);
    }

    // Every control the panel shows is inside the panel and visible, so nothing is cut off.
    static void checkControlsReachable (juce::UnitTest& t, BandPanel& panel, const juce::String& context)
    {
        int shown = 0;
        for (auto* c : panel.getChildren())
            if (c->isVisible())
            {
                ++shown;
                t.expect (panel.getLocalBounds().contains (c->getBounds()), context + ": a control is inside the panel");
            }
        t.expectGreaterThan (shown, 8, context + ": the panel shows its controls");
    }

    void runTest() override
    {
        using namespace EQ;

        beginTest ("The panel opens inside the graph for bands at every edge and corner, at 1x and 2x");
        for (float scale : { 1.0f, 2.0f })
        {
            struct Spot { float freq, gain; const char* name; };
            for (auto spot : { Spot { 25.0f, 20.0f, "top left" }, Spot { 19000.0f, 20.0f, "top right" },
                               Spot { 25.0f, -20.0f, "bottom left" }, Spot { 19000.0f, -20.0f, "bottom right" },
                               Spot { 1000.0f, 0.0f, "middle" }, Spot { 1000.0f, -20.0f, "bottom middle" } })
            {
                SongsmithEQAudioProcessor p;
                const int band = p.edits.addBand (spot.freq, spot.gain, FilterType::Bell);
                auto ed = UiSizeTests::makeEditor (p, scale, juce::roundToInt (1280 * scale), juce::roundToInt (788 * scale));
                auto& display = ed->getDisplay();
                display.setSelectedBand (band);
                const auto context = juce::String (spot.name) + " at " + juce::String (scale) + "x";
                expect (display.getPanel().isVisible(), context + ": the panel is shown");
                expect (inside (display.getPanel().getBounds(), display.getPanelLimits().toNearestInt()), context + ": inside the graph");
                checkControlsReachable (*this, display.getPanel(), context);
            }
        }

        beginTest ("The panel stays inside the graph at the smallest editor size");
        for (float s : EditorSize::uiScales)
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, -20.0f, FilterType::Bell);
            p.edits.setDynamic (band, true);
            const int w = juce::roundToInt ((float) EditorSize::minWidth * s), h = juce::roundToInt ((float) EditorSize::minHeight * s);
            auto ed = UiSizeTests::makeEditor (p, s, w, h);
            auto& display = ed->getDisplay();
            display.setSelectedBand (band);
            display.getPanel().refresh();
            const auto context = "minimum size at " + juce::String (juce::roundToInt (s * 100.0f)) + "%";
            const auto limit = display.getPanelLimits().toNearestInt();
            const auto bounds = display.getPanel().getBounds();
            if (limit.getWidth() >= bounds.getWidth() && limit.getHeight() >= bounds.getHeight())
                expect (inside (bounds, limit), context + ": all of the panel, with the Dynamic row, is inside the graph");
            else
                expect (bounds.getX() == limit.getX() || bounds.getY() == limit.getY(), context + ": too small for the panel, so its top-left corner stays reachable");
        }

        beginTest ("Dragging moves the panel to where it is dropped, keeps it there, and keeps it inside the graph");
        {
            SongsmithEQAudioProcessor p;
            const int a = p.edits.addBand (500.0f, 3.0f, FilterType::Bell);
            const int b = p.edits.addBand (4000.0f, -3.0f, FilterType::Bell);
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            auto& display = ed->getDisplay();
            auto& panel = display.getPanel();
            const auto limit = display.getPanelLimits().toNearestInt();
            display.setSelectedBand (a);
            expect (! display.isPanelPlacedByUser(), "automatic placement to begin with");

            // Through the same path a real drag takes.
            panel.onUserMove ({ limit.getX() + 40, limit.getY() + 30 });
            expect (display.isPanelPlacedByUser(), "a drag marks it as placed by the user");
            expectEquals (panel.getX(), limit.getX() + 40);
            expectEquals (panel.getY(), limit.getY() + 30);

            for (juce::Point<int> far : { juce::Point<int> (-5000, -5000), juce::Point<int> (9000, 9000), juce::Point<int> (-5000, 9000), juce::Point<int> (9000, -5000) })
            {
                panel.onUserMove (far);
                expect (inside (panel.getBounds(), limit), "dragged far outside, it is stopped at the edge of the graph");
            }

            panel.onUserMove ({ limit.getX() + 100, limit.getY() + 60 });
            const auto dropped = panel.getBounds();
            ed->refreshFromProcessor();
            display.setSelectedBand (a);
            expect (panel.getBounds() == dropped, "it does not jump when the display refreshes");
            display.setSelectedBand (b);
            expect (panel.getBounds().getPosition() == dropped.getPosition(), "it stays put when another band is chosen");
            expectEquals (display.getSelectedBand(), b);

            display.setSelectedBand (-1);
            expect (! panel.isVisible(), "no band, no panel");
            display.setSelectedBand (a);
            expect (! display.isPanelPlacedByUser(), "closing and reopening puts it beside its band again");
            expect (inside (panel.getBounds(), limit));
        }

        beginTest ("Critical: Dynamic opens a taller panel near the bottom, and every control is kept in view");
        for (float scale : { 1.0f, 2.0f })
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 0.0f, FilterType::Bell);
            auto ed = UiSizeTests::makeEditor (p, scale, juce::roundToInt (1280 * scale), juce::roundToInt (788 * scale));
            auto& display = ed->getDisplay();
            auto& panel = display.getPanel();
            display.setSelectedBand (band);
            const auto limit = display.getPanelLimits().toNearestInt();

            panel.onUserMove ({ limit.getX() + 200, limit.getBottom() - panel.getHeight() });   // dragged right down to the bottom
            expectEquals (panel.getBottom(), limit.getBottom(), "starts flush with the bottom");
            const int compactHeight = panel.getHeight();
            const int x = panel.getX();

            int lastY = -1;
            for (int round = 0; round < 4; ++round)
            {
                const auto context = "round " + juce::String (round) + " at " + juce::String (scale) + "x";
                p.edits.setDynamic (band, true);
                panel.refresh();
                expectGreaterThan (panel.getHeight(), compactHeight, context + ": the Dynamic row makes it taller");
                expect (inside (panel.getBounds(), limit), context + ": the expanded panel is inside the graph");
                expectEquals (panel.getX(), x, context + ": it moves up, not sideways");
                checkControlsReachable (*this, panel, context + " expanded");
                for (auto* c : { (juce::Component*) panel.getChildren().getLast() }) juce::ignoreUnused (c);
                if (lastY >= 0)
                    expectEquals (panel.getY(), lastY, context + ": opening again does not jump");
                lastY = panel.getY();

                p.edits.setDynamic (band, false);
                panel.refresh();
                expectEquals (panel.getHeight(), compactHeight, context + ": back to compact");
                expect (inside (panel.getBounds(), limit), context + ": the compact panel is inside the graph");
                expectEquals (panel.getY(), lastY, context + ": closing Dynamic does not move it");
            }
        }

        beginTest ("Dynamic also fits the panel when it was placed automatically");
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 10.0f, FilterType::Bell);   // node in the upper part: panel opens low
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            auto& display = ed->getDisplay();
            display.setSelectedBand (band);
            p.edits.setDynamic (band, true);
            display.getPanel().refresh();
            expect (inside (display.getPanel().getBounds(), display.getPanelLimits().toNearestInt()), "inside the graph");
            checkControlsReachable (*this, display.getPanel(), "automatic placement");
        }

        beginTest ("Moving the panel changes no sound parameter and does not touch a node");
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 6.0f, FilterType::Bell);
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            auto& display = ed->getDisplay();
            display.setSelectedBand (band);
            juce::MemoryBlock before, after;
            p.getStateInformation (before);
            const auto node = display.getNodeCentre (band);
            const auto limit = display.getPanelLimits().toNearestInt();
            for (int i = 0; i < 10; ++i)
                display.getPanel().onUserMove ({ limit.getX() + i * 37, limit.getY() + i * 21 });
            p.getStateInformation (after);
            expect (before == after, "the saved session is byte-for-byte the same");
            expect (display.getNodeCentre (band) == node, "the node has not moved");
            expectEquals (display.getSelectedBand(), band);
        }

        beginTest ("The background is the handle: move cursor and tooltip there, ordinary pointer on every control");
        {
            SongsmithEQAudioProcessor p;
            const int band = p.edits.addBand (1000.0f, 0.0f, FilterType::Bell);
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            ed->getDisplay().setSelectedBand (band);
            auto& panel = ed->getDisplay().getPanel();
            expectEquals (panel.getTooltip(), juce::String ("Drag to move these band controls."));
            expect (panel.getMouseCursor() == juce::MouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor), "move cursor on the panel");
            int controls = 0;
            for (auto* c : panel.getChildren())
            {
                bool onSelf = true, onChildren = true;
                c->getInterceptsMouseClicks (onSelf, onChildren);
                if (! onSelf)
                    continue;
                ++controls;
                expect (c->getMouseCursor() == juce::MouseCursor (juce::MouseCursor::NormalCursor)
                            || dynamic_cast<juce::Slider*> (c) != nullptr,
                        "a control keeps its own pointer");
            }
            expectGreaterThan (controls, 5, "the controls still take the mouse");
            WordingAndFontTests::checkLayout (*this, *ed, "band panel");
        }
    }
};
static BandPanelDragTests bandPanelDragTests;

//==============================================================================
// EQ-32: the Instrument Frequency Guide. Visual only.
class FrequencyGuideTests final : public juce::UnitTest
{
public:
    FrequencyGuideTests() : juce::UnitTest ("Instrument Frequency Guide", "EQ") {}

    static juce::StringArray labelsOf (const EQ::Guide::Instrument& instrument)
    {
        juce::StringArray labels;
        for (const auto& r : instrument.regions)
            labels.add (r.label);
        return labels;
    }

    static const EQ::Guide::Instrument& named (const juce::String& name)
    {
        for (const auto& i : EQ::Guide::instruments())
            if (name == i.name)
                return i;
        jassertfalse;
        return EQ::Guide::instrument (0);
    }

    static std::vector<float> render (SongsmithEQAudioProcessor& p, int blocks = 6)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float v = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 330.0f * (float) (b * 256 + i) / 48000.0f)
                                + 0.1f * std::sin (juce::MathConstants<float>::twoPi * 3100.0f * (float) (b * 256 + i) / 48000.0f);
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, 0.5f * v);
            }
            p.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    out.push_back (buffer.getSample (c, i));
        }
        return out;
    }

    void runTest() override
    {
        using namespace EQ;
        const auto& all = Guide::instruments();

        beginTest ("The ten instruments are in the agreed order, each with regions running from low to high");
        {
            const juce::StringArray expected { "Acoustic Guitar", "Nylon Guitar", "Electric Guitar", "Bass Guitar", "Vocal",
                                               "Kick Drum", "Snare Drum", "Toms", "Drum Bus", "Any Instrument" };
            expectEquals (Guide::numInstruments(), expected.size());
            for (int i = 0; i < Guide::numInstruments(); ++i)
            {
                const auto& inst = Guide::instrument (i);
                expectEquals (juce::String (inst.name), expected[i]);
                expectGreaterThan ((int) inst.regions.size(), 2, expected[i] + " has regions");
                float previousLow = 0.0f;
                for (const auto& r : inst.regions)
                {
                    expect (r.lowHz >= minFreq && r.highHz <= maxFreq && r.lowHz < r.highHz, expected[i] + "/" + r.label + ": a sensible range");
                    expect (r.lowHz >= previousLow, expected[i] + "/" + r.label + ": runs from low to high");
                    previousLow = r.lowHz;
                }
            }
        }

        beginTest ("Acoustic Guitar shows Rumble, Thump, Plastic, Cheap and Air; Electric Guitar has Warmth and Crunch");
        {
            expectEquals (labelsOf (named ("Acoustic Guitar")).joinIntoString (","), juce::String ("Rumble,Thump,Plastic,Cheap,Air"));
            expect (labelsOf (named ("Electric Guitar")).contains ("Warmth") && labelsOf (named ("Electric Guitar")).contains ("Crunch"));
            expect (labelsOf (named ("Any Instrument")).joinIntoString (",") == "Lows,Low Mids,Mids,High Mids,Highs");
        }

        beginTest ("Nylon Guitar's 1 to 3 kHz is Melody, and it never suggests cutting it; Melody and String Noise overlap on purpose");
        {
            const auto& nylon = named ("Nylon Guitar");
            const Guide::Region* melody = nullptr;
            const Guide::Region* noise = nullptr;
            for (const auto& r : nylon.regions)
            {
                if (juce::String (r.label) == "Melody") melody = &r;
                if (juce::String (r.label) == "String Noise") noise = &r;
            }
            expect (melody != nullptr && noise != nullptr);
            if (melody == nullptr || noise == nullptr) return;
            expect (melody->lowHz == 1000.0f && melody->highHz == 3000.0f);
            expect (! juce::String (melody->tip).containsIgnoreCase ("cut"), "no advice to cut the melody: " + juce::String (melody->tip));
            expect (noise->lowHz < melody->highHz && noise->highHz > melody->lowHz, "the two share a range");
            expect (Guide::regionsAt (nylon, 2500.0f).size() == 2, "at 2.5 kHz both are under the pointer");
        }

        beginTest ("Every region has a tooltip, a source, and a range that says around, under or above");
        for (const auto& inst : all)
        {
            expect (juce::String (inst.note).isNotEmpty(), juce::String (inst.name) + ": a note");
            for (const auto& r : inst.regions)
            {
                const auto where = juce::String (inst.name) + "/" + r.label;
                const juce::String tip (r.tip), source (r.source);
                expect (tip.length() > 20 && tip.length() < 340, where + ": a short explanation");
                expect (source.contains ("Joe Gilder") || source.contains ("Tony"), where + ": a source");
                if (source.contains ("Joe Gilder"))
                    expect (source.containsAnyOf ("0123456789") && (source.contains ("-0") || source.contains ("-1") || source.contains ("-2")), where + ": Joe's video is named with its date");
                const auto text = Guide::tooltipFor (inst, r);
                expect (text.contains (juce::String (r.label)) && text.contains (inst.name), where + ": names the word and the instrument");
                expect (text.contains ("around") || text.contains ("under") || text.contains ("above"), where + ": the range is only approximate");
                expect (text.contains ("Hz"), where + ": says where it is");
            }
        }

        beginTest ("On-screen guide text names no educator or person; provenance stays in the source field");
        for (const auto& inst : all)
        {
            juce::StringArray texts { inst.name, inst.note };
            for (const auto& r : inst.regions)
            {
                texts.add (r.label);
                texts.add (Guide::tooltipFor (inst, r));
                expect (juce::String (r.source).isNotEmpty(), juce::String (inst.name) + "/" + r.label + ": provenance kept");
            }
            for (const auto& t : texts)
            {
                expect (! t.containsIgnoreCase ("gilder"), "\"" + t.substring (0, 40) + "\" names an educator");
                expect (! t.containsWholeWord ("Joe") && ! t.containsWholeWord ("Joe's"), "\"" + t.substring (0, 40) + "\" names Joe");
                expect (! t.containsWholeWord ("Tony") && ! t.contains ("Tony's"), "\"" + t.substring (0, 40) + "\" names a person");
                expect (! t.containsIgnoreCase ("video") && ! t.containsIgnoreCase ("tutorial"), "\"" + t.substring (0, 40) + "\" cites a tutorial");
            }
        }

        beginTest ("Plastic and Cheap are kept, explained in hedged listening terms, with a note that no change may be needed");
        {
            for (const auto* label : { "Plastic", "Cheap" })
            {
                const Guide::Region* found = nullptr;
                for (const auto& r : named ("Acoustic Guitar").regions)
                    if (juce::String (r.label) == label)
                        found = &r;
                expect (found != nullptr, juce::String (label) + " is still there");
                if (found == nullptr) continue;
                const juce::String tip (found->tip);
                expect (tip.containsIgnoreCase ("if this band is strong"), juce::String (label) + ": conditional, not a diagnosis");
                expect (tip.containsIgnoreCase ("may need no adjustment"), juce::String (label) + ": says nothing may be needed");
                expect (tip.containsIgnoreCase ("listen"), juce::String (label) + ": says what to do: listen");
                expect (! tip.containsIgnoreCase ("always") && ! tip.containsIgnoreCase ("electronic"), juce::String (label) + ": no absolute or genre claim");
            }
        }

        beginTest ("Sources that are Tony's notes exist in his music-recording folder (when it is on this machine)");
        {
            const juce::File notes ("C:/dev/music-recording");
            if (! notes.isDirectory())
                logMessage ("music-recording is not on this machine: skipped");
            else
                for (const auto& inst : all)
                    for (const auto& r : inst.regions)
                    {
                        const juce::String source (r.source);
                        if (! source.contains ("Tony:"))
                            continue;
                        juce::StringArray tokens;
                        tokens.addTokens (source.fromFirstOccurrenceOf ("Tony:", false, false), " ,;", "");
                        for (auto token : tokens)
                            if (token.endsWith (".md") && token.contains ("/"))
                            {
                                token = token.trimCharactersAtEnd (".");
                                const bool inNotes = notes.getChildFile (token).existsAsFile();
                                const bool inRepo = juce::File ("C:/dev/audio-plugins").getChildFile (token).existsAsFile();
                                expect (inNotes || inRepo, juce::String (inst.name) + "/" + r.label + " cites " + token);
                            }
                    }
        }

        beginTest ("All the guide's words are plain: no abbreviations, and the unit words are spelled as the suite does");
        for (const auto& inst : all)
        {
            juce::StringArray texts { inst.name, inst.note };
            for (const auto& r : inst.regions)
            {
                texts.add (r.label);
                texts.add (r.tip);
                texts.add (Guide::tooltipFor (inst, r));
            }
            for (const auto& t : texts)
                expect (Songsmith::UI::Wording::findBanned (t).isEmpty(), "\"" + t.substring (0, 40) + "\" has \"" + Songsmith::UI::Wording::findBanned (t) + "\"");
        }

        beginTest ("The range text uses units and the word around");
        {
            expectEquals (Guide::rangeText ({ "x", 100.0f, 200.0f, "", "" }), juce::String ("around 100 to 200 Hz"));
            expectEquals (Guide::rangeText ({ "x", 1000.0f, 3000.0f, "", "" }), juce::String ("around 1 to 3 kHz"));
            expectEquals (Guide::rangeText ({ "x", 20.0f, 50.0f, "", "" }), juce::String ("under 50 Hz"));
            expectEquals (Guide::rangeText ({ "x", 8000.0f, 20000.0f, "", "" }), juce::String ("above 8 kHz"));
            expectEquals (Guide::rangeText ({ "x", 800.0f, 1200.0f, "", "" }), juce::String ("around 800 Hz to 1.2 kHz"));
        }

        beginTest ("Every guide instrument has room for its icon beside its full name in the selector menu");
        {
            EQLookAndFeel lf;
            for (int i = 0; i < Guide::numInstruments(); ++i)
            {
                int plainW = 0, plainH = 0, guideW = 0, guideH = 0;
                const juce::String label (Guide::instrument (i).name);
                lf.LookAndFeel_V4::getIdealPopupMenuItemSize (label, false, 24, plainW, plainH);
                lf.getIdealPopupMenuItemSize (label, false, 24, guideW, guideH);
                expect (guideW >= plainW + 20, label + " reserves an icon column");
                expectEquals (guideH, plainH);
            }
            int otherW = 0, otherH = 0, baseW = 0, baseH = 0;
            lf.LookAndFeel_V4::getIdealPopupMenuItemSize ("Not an instrument", false, 24, baseW, baseH);
            lf.getIdealPopupMenuItemSize ("Not an instrument", false, 24, otherW, otherH);
            expectEquals (otherW, baseW);
        }

        beginTest ("Switching the guide on, off or to another instrument leaves the sound and the bands exactly as they were");
        {
            SongsmithEQAudioProcessor reference;
            reference.edits.addBand (250.0f, -4.0f, FilterType::Bell);
            reference.edits.addBand (3000.0f, 3.0f, FilterType::HighShelf);
            reference.prepareToPlay (48000.0, 256);
            const auto expected = render (reference);
            const auto expectedState = reference.createPresetState().toXmlString();

            for (int on = 0; on < 2; ++on)
                for (int i = 0; i < Guide::numInstruments(); ++i)
                {
                    SongsmithEQAudioProcessor p;
                    p.edits.addBand (250.0f, -4.0f, FilterType::Bell);
                    p.edits.addBand (3000.0f, 3.0f, FilterType::HighShelf);
                    p.showFrequencyGuide.store (on == 1);
                    p.guideInstrument.store (i);
                    p.prepareToPlay (48000.0, 256);
                    const auto out = render (p);
                    expect (out == expected, "identical audio with the guide " + juce::String (on ? "on" : "off") + " for " + juce::String (Guide::instrument (i).name));
                    expect (p.createPresetState().toXmlString() == expectedState, "no band or parameter changed");
                }
        }

        beginTest ("The guide is saved with the session; an older session loads with it off");
        {
            SongsmithEQAudioProcessor p;
            p.showFrequencyGuide.store (true);
            p.guideInstrument.store (6);
            juce::MemoryBlock saved;
            p.getStateInformation (saved);

            SongsmithEQAudioProcessor q;
            q.setStateInformation (saved.getData(), (int) saved.getSize());
            expect (q.showFrequencyGuide.load(), "restored on");
            expectEquals (q.guideInstrument.load(), 6);

            // The same session as an older version wrote it: no guide properties at all.
            if (auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize()))
            {
                auto tree = juce::ValueTree::fromXml (*xml);
                auto ui = tree.getChildWithName ("UI");
                if (! ui.isValid())
                    for (auto child : tree)
                        if (child.hasProperty ("frequencyGuide")) ui = child;
                expect (ui.isValid() && ui.hasProperty ("frequencyGuide"), "found the saved editor state");
                ui.removeProperty ("frequencyGuide", nullptr);
                ui.removeProperty ("guideInstrument", nullptr);
                juce::MemoryBlock older;
                juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), older);

                SongsmithEQAudioProcessor r;
                r.showFrequencyGuide.store (true);
                r.guideInstrument.store (4);
                r.setStateInformation (older.getData(), (int) older.getSize());
                expect (! r.showFrequencyGuide.load(), "an older session loads with the guide off");
                expectEquals (r.guideInstrument.load(), 0);
            }

            const auto fixture = juce::File (EQ_TEST_FIXTURES).getChildFile ("tw-pro-eq-0.2.0-session.b64");
            if (fixture.existsAsFile())
            {
                juce::MemoryBlock legacy;
                legacy.fromBase64Encoding (fixture.loadFileAsString().trim());
                SongsmithEQAudioProcessor s;
                s.showFrequencyGuide.store (true);
                s.setStateInformation (legacy.getData(), (int) legacy.getSize());
                expect (! s.showFrequencyGuide.load(), "the 0.2.0 session loads with the guide off");
            }
        }

        beginTest ("Loading a starting point does not turn the guide on or change its instrument");
        {
            SongsmithEQAudioProcessor p;
            p.showFrequencyGuide.store (true);
            p.guideInstrument.store (3);
            juce::String error;
            expect (p.presets.load ({ PresetManager::getFactoryName (0), 0, {} }, error), error);
            expect (p.showFrequencyGuide.load());
            expectEquals (p.guideInstrument.load(), 3);

            SongsmithEQAudioProcessor off;
            expect (off.presets.load ({ PresetManager::getFactoryName (0), 0, {} }, error), error);
            expect (! off.showFrequencyGuide.load(), "and it does not switch it on either");
        }

        beginTest ("The controls: Frequency Guide toggle and an instrument box in the agreed order, off by default");
        {
            SongsmithEQAudioProcessor p;
            expect (! p.showFrequencyGuide.load(), "off by default");
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            auto* toggle = PianoToggleTests::findButton (ed->getContent(), "Frequency Guide");
            auto* instrumentBox = PianoToggleTests::findCombo (ed->getContent(), "Acoustic Guitar");
            expect (toggle != nullptr && instrumentBox != nullptr);
            if (toggle == nullptr || instrumentBox == nullptr) return;
            expectEquals (instrumentBox->getNumItems(), Guide::numInstruments());
            for (int i = 0; i < Guide::numInstruments(); ++i)
                expectEquals (instrumentBox->getItemText (i), juce::String (Guide::instrument (i).name));
            expect (! toggle->getToggleState());
            expect (toggle->getTooltip().contains ("never changes the sound"), "says it only draws");
            expect (instrumentBox->getTooltip().contains (Guide::instrument (0).note), "the box describes the instrument");

            const auto children = ed->getDisplay().getNumChildComponents();
            toggle->setToggleState (true, juce::dontSendNotification);
            toggle->onClick();
            expect (p.showFrequencyGuide.load(), "the toggle turns the guide on");
            instrumentBox->setSelectedItemIndex (5, juce::sendNotificationSync);
            expectEquals (p.guideInstrument.load(), 5);
            expectEquals (ed->getDisplay().getNumChildComponents(), children, "drawing only: no component is added over the graph");

            // The guide is found without knowing any technical word.
            for (auto* c : { (juce::Component*) toggle, (juce::Component*) instrumentBox })
                expect (c->isVisible() && ed->getContent().getLocalBounds().contains (c->getBounds()));
        }

        beginTest ("The words never overlap or leave the graph, at every interface size, the minimum and 4K");
        {
            struct Case { float scale; int w, h; };
            using namespace EQ::EditorSize;
            for (auto c : { Case { 1.0f, minWidth, minHeight }, Case { 1.0f, defaultWidth, defaultHeight }, Case { 1.0f, maxWidth, 2160 },
                            Case { 0.75f, juce::roundToInt (minWidth * 0.75f), juce::roundToInt (minHeight * 0.75f) },
                            Case { 2.0f, minWidth * 2, minHeight * 2 } })
                for (int i = 0; i < Guide::numInstruments(); ++i)
                {
                    SongsmithEQAudioProcessor p;
                    p.showFrequencyGuide.store (true);
                    p.guideInstrument.store (i);
                    p.edits.addBand (1000.0f, 3.0f, FilterType::Bell);
                    auto ed = UiSizeTests::makeEditor (p, c.scale, c.w, c.h);
                    auto& display = ed->getDisplay();
                    const auto area = display.getPlotAreaForGuide();
                    const auto labels = display.getGuideLabels (area);
                    const auto where = juce::String (Guide::instrument (i).name) + " at " + juce::String (c.w) + " x " + juce::String (c.h);
                    expectEquals ((int) labels.size(), (int) Guide::instrument (i).regions.size(), where + ": every word is shown");
                    for (size_t a = 0; a < labels.size(); ++a)
                    {
                        expect (area.expanded (0.5f).contains (labels[a].bounds), where + ": inside the graph");
                        expect (labels[a].bounds.getBottom() < area.getBottom() - 24.0f, where + ": leaves room above the axis");
                        const auto font = juce::Font (EQColours::font (EQColours::Type::hint, EQColours::Weight::Medium));
                        expect (labels[a].bounds.getWidth() >= juce::GlyphArrangement::getStringWidth (font, Guide::instrument (i).regions[(size_t) labels[a].region].label),
                                where + ": the word is shown whole");
                        for (size_t b = a + 1; b < labels.size(); ++b)
                            expect (! labels[a].bounds.intersects (labels[b].bounds), where + ": two words overlap");
                    }
                }
        }

        beginTest ("Hovering the graph explains the region, and every region under the pointer when they overlap");
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, 1.0f, 1280, 788);
            auto& display = ed->getDisplay();
            const auto area = display.getPlotAreaForGuide();
            const auto xAt = [&] (float hz)
            {
                const float t = std::log (hz / minFreq) / std::log (maxFreq / minFreq);
                return area.getX() + t * area.getWidth();
            };

            expect (display.getGuideTooltipAtX (xAt (150.0f)).isEmpty(), "guide off: no tooltip");
            p.showFrequencyGuide.store (true);
            p.guideInstrument.store (0);
            const auto thump = display.getGuideTooltipAtX (xAt (150.0f));
            expect (thump.startsWith ("Thump (around 100 to 200 Hz, Acoustic Guitar)"), thump);
            expect (thump.contains ("Turn it down, never out"), "says what to do");
            expect (display.getGuideTooltipAtX (xAt (300.0f)).isEmpty(), "between regions: nothing");

            p.guideInstrument.store (1);
            const auto both = display.getGuideTooltipAtX (xAt (2500.0f));
            expect (both.contains ("Melody") && both.contains ("String Noise"), "overlapping regions explain themselves together");
            expect (both.indexOf ("Melody") < both.indexOf ("String Noise"), "low to high");
        }
    }
};
static FrequencyGuideTests frequencyGuideTests;

//==============================================================================
// EQ-42: the held-peak number sits under CLIP, carries the measured sign, is red only at or above
// 0 dBFS, and is independent of the CLIP latch. Fixtures check detection, latch and readout apart.
class OutputMeterReadoutTests final : public juce::UnitTest
{
public:
    OutputMeterReadoutTests() : juce::UnitTest ("Output meter peak readout and clip policy", "EQ") {}

    static float gainOf (float db) { return juce::Decibels::decibelsToGain (db); }

    void runTest() override
    {
        constexpr double fs = 48000.0;
        using Meter = OutputMeterDisplay;
        const auto red = juce::Colour (0xffff5c5c);

        beginTest ("Layout: CLIP, then the peak number, then the channel names and bars; no number under the bars");
        for (int height : { 300, 400, 900 })
        {
            Meter m;
            m.setSize (Meter::preferredWidth, height);
            const auto clip = m.getClipBox(), peak = m.getPeakBox(), bars = m.getBarArea();
            expect (peak.getY() >= clip.getBottom(), "number below CLIP");
            expect (peak.getBottom() + Meter::namesHeight <= bars.getY() + 0.01f, "number above the L and R names");
            expectWithinAbsoluteError (peak.getCentreX(), clip.getCentreX(), 0.01f, "centred under CLIP");
            expect (peak.getRight() <= (float) m.getWidth(), "inside the meter");
            expect (bars.getBottom() <= (float) height - Meter::bottomRow + 0.01f, "the bottom row stays free for the axis");
            expect (peak.getHeight() >= 12.0f, "tall enough for 12 px text");
        }

        beginTest ("Sign and colour follow the measured held peak: -6, -0.6, just below 0, 0, +0.2, +0.6 dBFS and silence");
        {
            struct Case { float db; const char* text; bool red; };
            const Case cases[] = { { -6.0f, "-6.0", false }, { -0.6f, "-0.6", false }, { -0.01f, "-0.0", false },
                                   { 0.0f, "0.0", true }, { 0.2f, "+0.2", true }, { 0.6f, "+0.6", true } };
            for (auto c : cases)
                for (int channels : { 1, 2 })
                {
                    Meter m;
                    m.setSize (Meter::preferredWidth, 400);
                    m.update (gainOf (c.db), channels == 2 ? gainOf (c.db) : 0.0f, channels, false, 0.0, 0.0);
                    const auto context = juce::String (c.db) + " dBFS, " + juce::String (channels) + " channel(s)";
                    expectEquals (m.getPeakText(), juce::String (c.text), context + ": text");
                    expect (m.isPeakOver() == c.red, context + ": red or not");
                    expect (m.getPeakColour() == (c.red ? red : EQColours::label), context + ": colour");
                    expect (! m.isClipped(), context + ": the number alone never lights CLIP");
                }

            Meter silent;
            silent.update (0.0f, 0.0f, 2, false, 0.0, 0.0);
            expect (! silent.isPeakOver() && silent.getPeakText().length() == 1, "silence: a dash, not red");
            expect (Meter::isOverFullScale (0.0f) && ! Meter::isOverFullScale (-0.0001f), "the boundary is 0 dBFS");
        }

        beginTest ("A latched CLIP does not turn a negative held peak red, and the larger channel is the number");
        {
            Meter m;
            m.setSize (Meter::preferredWidth, 400);
            m.update (gainOf (0.6f), 0.1f, 2, true, 0.0, 0.0);
            expect (m.isClipped() && m.isPeakOver() && m.getPeakText() == "+0.6", "over: both red");
            m.update (gainOf (-0.6f), 0.1f, 2, true, 5.0, 5.0);   // the hold has fallen away to the new peak
            expect (m.isClipped(), "CLIP stays lit");
            expectEquals (m.getPeakText(), juce::String ("-0.6"));
            expect (! m.isPeakOver() && m.getPeakColour() == EQColours::label, "the number is not red");
            m.update (0.1f, gainOf (-3.0f), 2, false, 6.0, 0.0);
            expect (! m.isClipped(), "and CLIP clears only when the audio thread says so");
        }

        beginTest ("Fixtures: limited master at -1 dBFS shows no CLIP; exactly 0 dBFS does (documented policy)");
        for (bool mono : { false, true })
        {
            using Set = juce::AudioChannelSet;
            const int ch = mono ? 1 : 2;
            auto make = [&] (SongsmithEQAudioProcessor& p)
            {
                p.setBusesLayout (OutputMeterClipTests::layout (mono ? Set::mono() : Set::stereo(), mono ? Set::mono() : Set::stereo()));
                OutputMeterClipTests::flat (p).prepareToPlay (fs, 512);
            };

            SongsmithEQAudioProcessor limited;
            make (limited);
            OutputMeterClipTests::feed (limited, ch, gainOf (-1.0f), gainOf (-1.0f), 8);
            expect (! limited.outputMeter.clipLatched.load() && ! limited.outputMeter.clipped.load(), "-1 dBFS: no clip");
            Meter shown;
            shown.update (limited.outputMeter.readAndResetChannel (0), limited.outputMeter.readAndResetChannel (1), ch,
                          limited.outputMeter.clipLatched.load(), 0.0, 0.0);
            expectEquals (shown.getPeakText(), juce::String ("-1.0"));
            expect (! shown.isPeakOver(), "-1.0 is not red");

            SongsmithEQAudioProcessor atZero;
            make (atZero);
            OutputMeterClipTests::feed (atZero, ch, 1.0f, 1.0f, 2);
            expect (atZero.outputMeter.clipLatched.load(), "exactly 0 dBFS latches CLIP");
            Meter zero;
            zero.update (atZero.outputMeter.readAndResetChannel (0), atZero.outputMeter.readAndResetChannel (1), ch, true, 0.0, 0.0);
            expectEquals (zero.getPeakText(), juce::String ("0.0"));
            expect (zero.isPeakOver(), "0.0 is red, matching CLIP");
        }

        beginTest ("Fixture: a -0.9 dBFS master pushed over by +1.1 dB of Output gain reads +0.2 and latches CLIP");
        for (bool mono : { false, true })
        {
            using Set = juce::AudioChannelSet;
            const int ch = mono ? 1 : 2;
            SongsmithEQAudioProcessor p;
            p.setBusesLayout (OutputMeterClipTests::layout (mono ? Set::mono() : Set::stereo(), mono ? Set::mono() : Set::stereo()));
            OutputMeterClipTests::flat (p).prepareToPlay (fs, 512);
            auto* gain = p.apvts.getParameter (EQ::outputGainId);
            gain->setValueNotifyingHost (gain->convertTo0to1 (1.1f));
            OutputMeterClipTests::feed (p, ch, 0.9f, 0.9f, 200);   // let the gain smoothing settle
            (void) p.outputMeter.readAndResetChannel (0);
            (void) p.outputMeter.readAndResetChannel (1);
            OutputMeterClipTests::feed (p, ch, 0.9f, 0.9f, 4);
            expect (p.outputMeter.clipLatched.load(), "over after the boost: CLIP latched");
            Meter shown;
            shown.update (p.outputMeter.readAndResetChannel (0), p.outputMeter.readAndResetChannel (1), ch, true, 0.0, 0.0);
            expectEquals (shown.getPeakText(), juce::String ("+0.2"));
            expect (shown.isPeakOver(), "red");
        }

        beginTest ("Fixture: a brief over then sustained quiet audio: CLIP stays, the number is negative and not red, then CLIP clears");
        {
            SongsmithEQAudioProcessor p;
            OutputMeterClipTests::flat (p).prepareToPlay (fs, 512);
            OutputMeterClipTests::feed (p, 2, gainOf (0.4f), gainOf (0.4f), 1);
            expect (p.outputMeter.clipLatched.load());
            (void) p.outputMeter.readAndResetChannel (0);
            (void) p.outputMeter.readAndResetChannel (1);
            OutputMeterClipTests::feed (p, 2, 0.5f, 0.5f, 200);
            expect (p.outputMeter.clipLatched.load(), "latch survives audible sound");
            Meter shown;
            shown.update (p.outputMeter.readAndResetChannel (0), p.outputMeter.readAndResetChannel (1), 2, true, 10.0, 10.0);
            expectEquals (shown.getPeakText(), juce::String ("-6.0"));
            expect (shown.isClipped() && ! shown.isPeakOver(), "CLIP lit, number normal");
            OutputMeterClipTests::feed (p, 2, 0.0f, 0.0f, 100);
            expect (! p.outputMeter.clipLatched.load(), "sustained silence clears the latch");
        }
    }
};
static OutputMeterReadoutTests outputMeterReadoutTests;

static PianoToggleTests pianoToggleTests;
static EdgeClickTests edgeClickTests;
static OutputMeterTests outputMeterTests;
static OutputMeterClipTests outputMeterClipTests;

//==============================================================================
// EQ-44: Accurate Highs (matched bells and shelves).
class AccurateHighsTests final : public juce::UnitTest
{
public:
    AccurateHighsTests() : juce::UnitTest ("Accurate Highs", "EQ") {}

    // Worst deviation (dB) of a digital design from the analog prototype, 20 Hz to 'top'.
    static double worstError (FilterType type, double fs, double f0, double q, double gain, bool matched, double top)
    {
        const auto c = makeCoeffs (type, fs, f0, q, gain, matched);
        double worst = 0.0;
        for (int i = 0; i <= 200; ++i)
        {
            const double f = 20.0 * std::pow (top / 20.0, i / 200.0);
            const double analog = 20.0 * std::log10 (analogMagnitude (type, fs, f0, q, gain, f));
            worst = std::max (worst, std::abs (getMagnitudeDb (c, f, fs) - analog));
        }
        return worst;
    }

    void runTest() override
    {
        beginTest ("Bells and shelves near the top keep their analog shape (and the bilinear design does not)");
        for (double fs : { 44100.0, 48000.0 })
            for (auto type : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf })
                for (double f0 : { 8000.0, 12000.0, 16000.0 })
                    for (double gain : { -12.0, 6.0, 12.0 })
                    {
                        const double q = 1.0, top = 0.45 * fs;
                        const double matched = worstError (type, fs, f0, q, gain, true, top);
                        const double bilinear = worstError (type, fs, f0, q, gain, false, top);
                        const auto what = juce::String (type == FilterType::Bell ? "bell" : type == FilterType::LowShelf ? "low shelf" : "high shelf")
                                        + " " + juce::String (f0 / 1000.0, 0) + " kHz " + juce::String (gain, 0) + " dB at " + juce::String (fs / 1000.0, 1) + " kHz";
                        logMessage ("  " + what + ": matched " + juce::String (matched, 2) + " dB, bilinear " + juce::String (bilinear, 2) + " dB");
                        expectLessThan (matched, bilinear + 0.01, what + ": never worse than the bilinear design");
                        if (bilinear > 1.0 && ! (type != FilterType::Bell && f0 >= 16000.0))
                            expectLessThan (matched, 0.5 * bilinear, what + ": at least halves the error");
                        if (type != FilterType::Bell && f0 <= 12000.0)
                            expectLessThan (matched, 0.5, what + ": shelf within 0.5 dB of the analog shape");
                    }

        beginTest ("Lower bands are unchanged where it matters");
        for (auto type : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf })
            for (double f0 : { 100.0, 1000.0 })
            {
                const auto m = makeCoeffs (type, 48000.0, f0, 0.7, 9.0, true);
                const auto b = makeCoeffs (type, 48000.0, f0, 0.7, 9.0, false);
                double worst = 0.0;
                for (double f = 20.0; f <= 5000.0; f *= 1.1)
                    worst = std::max (worst, std::abs (getMagnitudeDb (m, f, 48000.0) - getMagnitudeDb (b, f, 48000.0)));
                expectLessThan (worst, 0.1, "matched and bilinear agree below 5 kHz at " + juce::String (f0) + " Hz");
            }

        beginTest ("Every setting gives a stable, finite filter, never worse than the bilinear one, and cuts and notches are untouched");
        {
            bool ok = true;
            int badCount = 0;
            juce::String firstBad, allBad;
            for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
                for (auto type : { FilterType::Bell, FilterType::LowShelf, FilterType::HighShelf, FilterType::Notch, FilterType::LowCut, FilterType::HighCut })
                    for (double f0 = 20.0; f0 <= 0.48 * fs; f0 *= 1.5)
                        for (double q : { 0.1, 0.7, 2.0, 10.0, 40.0 })
                            for (double gain : { -24.0, -3.0, 0.0, 3.0, 24.0 })
                            {
                                const auto c = makeCoeffs (type, fs, f0, q, gain, true);
                                const bool finite = std::isfinite (c.g) && std::isfinite (c.k) && std::isfinite (c.m0) && std::isfinite (c.m1) && std::isfinite (c.m2);
                                const bool stable = c.g > 0.0 && c.k > 0.0;   // SVF with positive g and k is stable
                                // Never worse than the bilinear design, over the whole grid (the fallback rule in
                                // makeMatchedCoeffs is what guarantees it at run time, without comparing both).
                                const double top = 0.45 * fs;
                                const bool notWorse = worstError (type, fs, f0, q, gain, true, top) <= worstError (type, fs, f0, q, gain, false, top) + 0.05;
                                bool same = true;
                                if (type == FilterType::Notch || type == FilterType::LowCut || type == FilterType::HighCut || gain == 0.0)
                                {
                                    const auto b = makeCoeffs (type, fs, f0, q, gain, false);
                                    same = c.g == b.g && c.k == b.k && c.m0 == b.m0 && c.m1 == b.m1 && c.m2 == b.m2;
                                }
                                if (! (finite && stable && same && notWorse))
                                {
                                    ++badCount;
                                    if (badCount <= 6)
                                        allBad << juce::String ((int) type) << " " << f0 << " Hz q " << q << " gain " << gain << " at " << fs << (notWorse ? "; " : " (worse); ");
                                }
                                if (! (finite && stable && same && notWorse) && ok)
                                {
                                    ok = false;
                                    firstBad = juce::String ((int) type) + " " + juce::String (f0) + " Hz q " + juce::String (q) + " gain " + juce::String (gain) + " at " + juce::String (fs) + (notWorse ? "" : " (matched worse)");
                                }
                            }
            expect (ok, juce::String (badCount) + " bad cases, first: " + allBad);
        }

        beginTest ("The engine plays what the curve shows (a 15 kHz bell at 44.1 kHz)");
        {
            constexpr double fs = 44100.0;
            Engine e;
            e.prepare (fs);
            e.setMatched (true);
            BandSettings s;
            s.active = true; s.type = FilterType::Bell; s.freq = 15000.0f; s.gain = 9.0f; s.q = 1.0f;
            e.setBand (0, s);
            for (int b = 1; b < numBands; ++b) e.setBand (b, BandSettings {});
            e.snapToTargets();

            const auto drawn = makeFilter (FilterType::Bell, fs, 15000.0, 1.0, 9.0, defaultSlope, true);
            for (double f : { 5000.0, 12000.0, 15000.0, 19000.0 })
            {
                const int n = (int) fs;
                std::vector<float> l ((size_t) n), r ((size_t) n);
                for (int i = 0; i < n; ++i)
                    l[(size_t) i] = r[(size_t) i] = (float) (0.1 * std::sin (2.0 * pi * f * i / fs));
                float* ch[] = { l.data(), r.data() };
                e.process (ch, 2, n);
                double sum = 0.0;
                for (int i = n / 2; i < n; ++i) sum += (double) l[(size_t) i] * l[(size_t) i];
                const double measured = 20.0 * std::log10 (std::sqrt (sum / (n / 2)) / (0.1 / std::sqrt (2.0)));
                expectWithinAbsoluteError (measured, getMagnitudeDb (drawn, f, fs), 0.1, "at " + juce::String (f) + " Hz");
            }
        }

        beginTest ("On for a new instance; sessions saved before it load with it off");
        {
            SongsmithEQAudioProcessor fresh;
            expect (fresh.isHighFrequencyAccurate(), "new instance: on");

            juce::MemoryBlock saved;
            fresh.getStateInformation (saved);
            auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
            expect (xml != nullptr);
            auto tree = juce::ValueTree::fromXml (*xml);
            tree.removeChild (tree.getChildWithProperty ("id", EQ::hfAccuracyId), nullptr);   // as saved by an older version
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), old);

            SongsmithEQAudioProcessor reopened;
            reopened.setStateInformation (old.getData(), (int) old.getSize());
            expect (! reopened.isHighFrequencyAccurate(), "an older session keeps the bilinear design");

            SongsmithEQAudioProcessor roundTrip;
            roundTrip.setStateInformation (saved.getData(), (int) saved.getSize());
            expect (roundTrip.isHighFrequencyAccurate(), "a new session keeps it on");
        }

        beginTest ("Accurate Highs sits right beside Oversampling, fully visible, at every size");
        for (float s : { 0.75f, 1.0f, 2.0f })
        {
            SongsmithEQAudioProcessor p;
            auto ed = UiSizeTests::makeEditor (p, s, juce::roundToInt (980 * s), juce::roundToInt (468 * s));
            auto* eqEditor = dynamic_cast<SongsmithEQAudioProcessorEditor*> (ed.get());
            expect (eqEditor != nullptr);
            if (eqEditor == nullptr)
                continue;
            auto& button = eqEditor->getAccurateHighsButton();
            auto& os = eqEditor->getOversamplingBox();
            const auto context = "at " + juce::String (s) + "x";
            expect (button.isVisible() && button.getWidth() > 0, context + ": visible");
            expectEquals (button.getY(), os.getY(), context + ": same row as Oversampling");
            expect (button.getX() > os.getRight() && button.getX() - os.getRight() < 16, context + ": immediately right of Oversampling");
            expect (button.getToggleState(), context + ": on in a new instance");
            expect (button.getTooltip().startsWith ("Accurate highs:"), context + ": tooltip");
        }
    }
};
static AccurateHighsTests accurateHighsTests;
static UiSizeTests uiSizeTests;
static WordingAndFontTests wordingAndFontTests;
static SongsmithUITests sharedUITests { "EQ" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "EQ" };
static SongsmithGuidanceTests sharedGuidanceTests { "EQ" };
static DiagnosticsTests diagnosticsTests;
static RenameTests renameTests;
static StartingPointTests startingPointTests;
static OversamplingTests oversamplingTests;
static LinearPhaseTests linearPhaseTests;
static DynamicEqTests dynamicEqTests;
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
                       const std::function<void (SongsmithEQAudioProcessor&, int band)>& setup,
                       const std::function<void (SongsmithEQAudioProcessor&, int band, int blockIndex)>& automate,
                       const std::vector<int>& blockSizes = { 32, 128, 512 })
{
    constexpr double fs = 48000.0;
    constexpr double seconds = 20.0;

    std::printf ("\n%s\n", title);
    std::printf ("%-7s %-6s %-10s %12s %12s %10s\n", "bands", "block", "mode", "us/block", "ns/sample", "% of RT");

    for (int block : blockSizes)
        for (bool modulated : { false, true })
        {
            SongsmithEQAudioProcessor p;
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
                SongsmithEQAudioProcessor p;
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
               [] (SongsmithEQAudioProcessor& p, int b)
               {
                   setBand (p, b, Param::Type, (float) (b % 2 == 0 ? FilterType::LowCut : FilterType::HighCut));
                   setBand (p, b, Param::Freq, b % 2 == 0 ? 20.0f + (float) b : 20000.0f - (float) b * 100.0f);
                   setBand (p, b, Param::Slope, 5.0f);
               },
               [] (SongsmithEQAudioProcessor& p, int b, int bi)   // frequency automation
               {
                   const float base = b % 2 == 0 ? 30.0f : 15000.0f;
                   setBand (p, b, Param::Freq, base * (1.0f + 0.2f * std::sin ((float) bi * 0.05f + (float) b)));
               });

    benchCase ("24 bells, mixed stereo modes (S/L/R/M/S...)",
               [] (SongsmithEQAudioProcessor& p, int b)
               {
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 6.0f);
                   setBand (p, b, Param::Q, 1.5f);
                   setBand (p, b, Param::Stereo, (float) (b % numStereoModes));
               },
               [] (SongsmithEQAudioProcessor& p, int b, int bi)   // gain automation
               {
                   setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
               });

    benchCase ("24 dynamic bands (bells/shelves, all compressing), 64-sample blocks",
               [] (SongsmithEQAudioProcessor& p, int b)
               {
                   setBand (p, b, Param::Type, (float) (b % 3));   // bell, low shelf, high shelf
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 3.0f);
                   setBand (p, b, Param::Q, 1.5f);
                   setBand (p, b, Param::DynOn, 1.0f);
                   setBand (p, b, Param::Threshold, -40.0f);   // noise at -6 dBFS: always active
                   setBand (p, b, Param::Range, -6.0f);
                   setBand (p, b, Param::Attack, 1.0f);
               },
               [] (SongsmithEQAudioProcessor& p, int b, int bi)
               {
                   setBand (p, b, Param::Threshold, -40.0f + 10.0f * std::sin ((float) bi * 0.05f + (float) b));
               },
               { 64 });

    for (int os = 0; os < 3; ++os)
    {
        const juce::String title = "24 bands, oversampling " + juce::StringArray { "off", "2x", "4x" }[os];
        benchCase (title.toRawUTF8(),
                   [os] (SongsmithEQAudioProcessor& p, int b)
                   {
                       setBand (p, b, Param::Type, (float) (b % numFilterTypes));
                       setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                       setBand (p, b, Param::Gain, 6.0f);
                       setBand (p, b, Param::Q, 1.5f);
                       if (b == numBands - 1)
                       {
                           setParam (p, "oversampling", (float) os);
                           juce::AudioBuffer<float> buf (2, 32);
                           juce::MidiBuffer midi;
                           for (int i = 0; i < 100; ++i) { buf.clear(); p.processBlock (buf, midi); }   // finish the switch fade
                       }
                   },
                   [] (SongsmithEQAudioProcessor& p, int b, int bi)
                   {
                       setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
                   });
    }

    for (int quality = 0; quality < 3; ++quality)
    {
        const juce::String title = "Linear phase, quality " + juce::String (juce::StringArray { "low", "medium", "high" }[quality])
                                 + " (24 bands; FIR " + juce::String (EQ::LinearPhaseEQ::firLength (quality, 48000.0)) + ")";
        benchCase (title.toRawUTF8(),
                   [quality] (SongsmithEQAudioProcessor& p, int b)
                   {
                       setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                       setBand (p, b, Param::Gain, 3.0f);
                       if (b == numBands - 1)
                       {
                           setParam (p, "lp_quality", (float) quality);
                           setParam (p, "phase_mode", 1.0f);
                           juce::AudioBuffer<float> buf (2, 512);
                           juce::MidiBuffer midi;
                           p.designLinearPhaseNow();
                           for (int i = 0; i < 2000 && ! p.isLinearPhaseReady(); ++i)
                           {
                               buf.clear();
                               p.processBlock (buf, midi);   // switches path, loads the IR
                               juce::Thread::sleep (2);
                           }
                       }
                   },
                   [] (SongsmithEQAudioProcessor& p, int b, int bi)   // the designer thread rebuilds as gains move
                   {
                       setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
                   });
    }

    benchCase ("24 bells, auto gain on",
               [] (SongsmithEQAudioProcessor& p, int b)
               {
                   if (b == 0)
                       setParam (p, "auto_gain", 1.0f);
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 6.0f);
                   setBand (p, b, Param::Q, 1.5f);
               },
               [] (SongsmithEQAudioProcessor& p, int b, int bi)
               {
                   setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
               });

    // EQ-44: the same with Accurate Highs off, to show what the matched design costs while gains move.
    benchCase ("24 bells, auto gain on, Accurate Highs off",
               [] (SongsmithEQAudioProcessor& p, int b)
               {
                   if (b == 0)
                   {
                       setParam (p, "auto_gain", 1.0f);
                       setParam (p, EQ::hfAccuracyId, 0.0f);
                   }
                   setBand (p, b, Param::Freq, 40.0f * std::pow (400.0f, (float) b / numBands));
                   setBand (p, b, Param::Gain, 6.0f);
                   setBand (p, b, Param::Q, 1.5f);
               },
               [] (SongsmithEQAudioProcessor& p, int b, int bi)
               {
                   setBand (p, b, Param::Gain, 6.0f * std::sin ((float) bi * 0.05f + (float) b));
               });
}
//==============================================================================
// EQTests --screenshots <folder> [--scale 2]
// Renders the editor (the same component the standalone app and the plug-ins show) with a
// fixed scene: a low cut, a Mid-only bell, a dynamic bell with its band panel open and a
// high shelf. Saves PNGs at the default and the minimum editor size. Used for the UI PRs'
// before/after pictures; works headless, so CI can make Retina (--scale 2) shots on macOS.
static int renderScreenshots (const juce::File& folder, float scale)
{
    folder.createDirectory();

    auto shoot = [&] (int width, int height, const juce::String& name, float uiScale = 1.0f, bool piano = false, bool panelAtBottom = false, int guide = -1, bool mono = false, bool clip = false)
    {
        SongsmithEQAudioProcessor p;
        if (mono)   // EQ-39: a mono track shows one meter bar
        {
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::mono());
            l.inputBuses.add (juce::AudioChannelSet::disabled());
            l.outputBuses.add (juce::AudioChannelSet::mono());
            p.setBusesLayout (l);
        }
        p.prepareToPlay (48000.0, 512);
        for (int b = 0; b < numBands; ++b)   // a fresh instance starts with three bands
            if (p.isBandUsed (b))
                p.edits.removeBand (b);

        auto add = [&] (FilterType type, float freq, float gain, float q)
        {
            const int b = p.edits.addBand (freq, gain, type);
            p.edits.setValue (b, Param::Q, q);
            return b;
        };
        const int cut = add (FilterType::LowCut, 70.0f, 0.0f, 0.71f);
        p.edits.setSlope (cut, 3);
        const int mid = add (FilterType::Bell, 280.0f, -3.5f, 1.2f);
        p.edits.setStereoMode (mid, 3);   // Mid
        const int dyn = add (FilterType::Bell, 2600.0f, -2.0f, 2.0f);
        p.edits.setDynamic (dyn, true);
        p.edits.setValue (dyn, Param::Range, -4.0f);
        add (FilterType::HighShelf, 10000.0f, 1.5f, 0.71f);

        // A 1 kHz tone at about -8 dBFS, so the output meter shows a level.
        {
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 8; ++block)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float v = 0.4f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) (block * 512 + i) / 48000.0f);
                    buffer.setSample (0, i, v);
                    buffer.setSample (1, i, v);
                }
                p.processBlock (buffer, midi);
            }

            if (clip)   // EQ-39: one full-scale block on the left channel, then back to the tone
            {
                buffer.setSample (0, 0, 3.0f);
                p.processBlock (buffer, midi);
                for (int i = 0; i < 512; ++i)
                {
                    const float v = 0.4f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) i / 48000.0f);
                    buffer.setSample (0, i, v);
                    buffer.setSample (1, i, v);
                }
                p.processBlock (buffer, midi);
            }
        }

        p.uiScale = uiScale;
        p.showPianoStrip.store (piano);
        p.showFrequencyGuide.store (guide >= 0);   // EQ-32: -1 is off, otherwise the instrument
        p.guideInstrument.store (juce::jmax (0, guide));
        p.editorWidth = width;
        p.editorHeight = height;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        editor->setSize (width, height);

        if (auto* ed = dynamic_cast<SongsmithEQAudioProcessorEditor*> (editor.get()))
        {
            ed->getDisplay().setSelectedBand (dyn);
            if (panelAtBottom)   // EQ-37: dragged right down to the bottom edge; the Dynamic row still fits
            {
                auto& panel = ed->getDisplay().getPanel();
                const auto limit = ed->getDisplay().getPanelLimits().toNearestInt();
                panel.onUserMove ({ limit.getX() + 180, limit.getBottom() });
            }
        }

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        const auto file = folder.getChildFile (name + (scale != 1.0f ? "@" + juce::String (scale, 0) + "x" : juce::String()) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("%s\n", file.getFullPathName().toRawUTF8());
        p.releaseResources();
    };

    SongsmithEQAudioProcessor defaults;
    shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "default-size");
    shoot (EQ::EditorSize::minWidth, EQ::EditorSize::minHeight, "minimum-size");
    shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "meter-mono", 1.0f, false, false, -1, true, false);   // EQ-39
    shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "meter-stereo-clip", 1.0f, false, false, -1, false, true);
    shoot (EQ::EditorSize::minWidth, EQ::EditorSize::minHeight, "meter-stereo-clip-minimum-size", 1.0f, false, false, -1, false, true);
    for (int i = 0; i < EQ::Guide::numInstruments(); ++i)   // the Frequency Guide, one picture per instrument
    {
        const auto slug = juce::String (EQ::Guide::instrument (i).name).toLowerCase().replaceCharacter (' ', '-');
        shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "guide-" + slug, 1.0f, false, false, i);
        shoot (EQ::EditorSize::minWidth, EQ::EditorSize::minHeight, "guide-" + slug + "-minimum-size", 1.0f, false, false, i);
    }
    shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "panel-dragged-to-bottom", 1.0f, false, true);
    shoot (defaults.editorWidth.load(), defaults.editorHeight.load(), "default-size-piano", 1.0f, true);
    shoot (EQ::EditorSize::minWidth, EQ::EditorSize::minHeight, "minimum-size-piano", 1.0f, true);
    shoot (EQ::EditorSize::maxWidth, 2160, "maximum-size-4k");

    // Every interface size at its default window (design size 1280 x 760, scaled).
    for (auto s : EQ::EditorSize::uiScales)
        shoot (juce::roundToInt ((float) EQ::EditorSize::defaultWidth * s), juce::roundToInt ((float) EQ::EditorSize::defaultHeight * s),
               "interface-size-" + juce::String (juce::roundToInt (s * 100.0f)), s);

    // Every control state, drawn by the editor's look-and-feel.
    EQLookAndFeel lf;
    Songsmith::UI::Testing::writeControlSheet (lf, EQColours::background, scale,
        folder.getChildFile ("controls" + (scale != 1.0f ? "@" + juce::String (scale, 0) + "x" : juce::String()) + ".png"));
    return 0;
}

//==============================================================================
// EQTests --paint-bench
// Times EQDisplay::paint (the graph, with the analyser showing pink noise and five bands) at
// the editor's default and largest window sizes, at 1x and 2x (Retina / 200 %) backing scale.
static int runPaintBenchmark()
{
    auto timeAt = [] (int width, int height, float backingScale)
    {
        SongsmithEQAudioProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        for (int b = 0; b < numBands; ++b)
            if (p.isBandUsed (b))
                p.edits.removeBand (b);
        p.edits.addBand (70.0f, 0.0f, FilterType::LowCut);
        p.edits.addBand (280.0f, -3.5f, FilterType::Bell);
        const int dyn = p.edits.addBand (2600.0f, -2.0f, FilterType::Bell);
        p.edits.setDynamic (dyn, true);
        p.edits.addBand (10000.0f, 1.5f, FilterType::HighShelf);
        p.edits.addBand (500.0f, -1.5f, FilterType::Bell);

        // A second of pink-ish noise so the analyser has a spectrum to draw.
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        juce::Random rng (1);
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        for (int block = 0; block < 100; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float w = rng.nextFloat() * 2.0f - 1.0f;
                b0 = 0.99765f * b0 + w * 0.0990460f;
                b1 = 0.96300f * b1 + w * 0.2965164f;
                b2 = 0.57000f * b2 + w * 1.0526913f;
                const float pink = 0.1f * (b0 + b1 + b2 + w * 0.1848f);
                buffer.setSample (0, i, pink);
                buffer.setSample (1, i, pink);
            }
            p.processBlock (buffer, midi);
        }

        p.editorWidth = width;
        p.editorHeight = height;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        editor->setSize (width, height);

        EQDisplay* display = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            if (auto* d = dynamic_cast<EQDisplay*> (&c)) display = d;
            for (auto* child : c.getChildren()) find (*child);
        };
        find (*editor);
        if (display == nullptr)
            return std::pair<double, juce::String> { -1.0, {} };

        const auto bounds = display->getLocalBounds();
        const float total = backingScale * (float) juce::Component::getApproximateScaleFactorForComponent (display);
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) bounds.getWidth() * total),
                           juce::roundToInt ((float) bounds.getHeight() * total), true);

        auto paintOnce = [&]
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (total));
            display->paintEntireComponent (g, true);
        };
        paintOnce();   // warm-up (fills the curve cache, as on screen)

        constexpr int runs = 20;
        const auto start = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < runs; ++i)
            paintOnce();
        const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0 / runs;
        return std::pair<double, juce::String> { ms, juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()) + " -> " + juce::String (image.getWidth()) + " x " + juce::String (image.getHeight()) };
    };

    SongsmithEQAudioProcessor defaults;
    const int dw = defaults.editorWidth.load(), dh = defaults.editorHeight.load();


    std::printf ("Graph paint time (EQDisplay, analyser on, 5 bands), mean of 20 paints\n");
    std::printf ("%-24s %-8s %-16s %10s\n", "window", "backing", "graph pixels", "ms/paint");
    for (auto [w, h] : { std::pair<int, int> { dw, dh }, std::pair<int, int> { 2000, 1200 }, std::pair<int, int> { 3840, 2160 } })
        for (float s : { 1.0f, 2.0f })
        {
            const auto [ms, pixels] = timeAt (w, h, s);
            std::printf ("%-24s %-8s %-16s %10.2f\n", (juce::String (w) + " x " + juce::String (h)).toRawUTF8(),
                         (juce::String (s, 0) + "x").toRawUTF8(), pixels.toRawUTF8(), ms);
        }
    return 0;
}

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // No test may touch the real user preset folders (or migrate the real old ones):
    // point the default locations at a throwaway folder for the whole run.
    const auto appData = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getNonexistentChildFile ("SongsmithEQ-tests-appdata", {}, false);
    PresetManager::setDefaultFoldersRootForTesting (appData);
    struct Cleanup { juce::File dir; ~Cleanup() { dir.deleteRecursively(); } } cleanup { appData };

    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--bench")
        {
            runBenchmark();
            return 0;
        }

        if (juce::String (argv[i]) == "--paint-bench")
            return runPaintBenchmark();

        if (juce::String (argv[i]) == "--screenshots" && i + 1 < argc)
        {
            float scale = 1.0f;
            for (int j = 1; j + 1 < argc; ++j)
                if (juce::String (argv[j]) == "--scale")
                    scale = juce::String (argv[j + 1]).getFloatValue();
            return renderScreenshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]),
                                      juce::jlimit (1.0f, 4.0f, scale));
        }
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
