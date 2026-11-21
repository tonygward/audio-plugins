// Songsmith Plugin Analyzer test runner (ANALYZER-01): the measurement against known filters, what it flags, how a Reference and a
// Result find each other, the plugins' audio rules, saved sessions, the CSV file and the editor, plus the shared Songsmith checks.
//
// The reference systems are generated here: a flat gain, a high-pass, a bell boost and cut and a low shelf (RBJ biquads, which have
// an exact formula for their response), with a delay in front. The measurement must agree with the formula to within half a decibel
// from 30 Hz to 18 kHz, which is what the ticket asks for, and with every block size.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <complex>
#include <functional>
#include <random>
#include <thread>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithUpdateTests.h"
#include "SongsmithWording.h"

using namespace PluginAnalyzer;

namespace
{
    constexpr double fs = 48000.0;

    // --- Systems with a known response ------------------------------------------------------------------
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        double process (double x) noexcept
        {
            const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
        double responseDb (double f) const
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
            return 20.0 * std::log10 (std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)));
        }
        static Biquad make (double nb0, double nb1, double nb2, double a0, double na1, double na2)
        {
            Biquad b; b.b0 = nb0 / a0; b.b1 = nb1 / a0; b.b2 = nb2 / a0; b.a1 = na1 / a0; b.a2 = na2 / a0; return b;
        }
        static Biquad peak (double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            return make (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
        }
        static Biquad highPass (double f, double q)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            return make ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
        }
        static Biquad lowShelf (double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
            const double sA = 2.0 * std::sqrt (A) * al;
            return make (A * ((A + 1) - (A - 1) * cw + sA), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - sA),
                         (A + 1) + (A - 1) * cw + sA, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - sA);
        }
    };

    struct System
    {
        std::vector<Biquad> stages;
        double gain = 1.0;
        int delaySamples = 0;
        double process (double x)
        {
            for (auto& s : stages) x = s.process (x);
            return x * gain;
        }
        double responseDb (double f) const
        {
            double db = 20.0 * std::log10 (gain);
            for (auto& s : stages) db += s.responseDb (f);
            return db;
        }
    };

    // The expected curve at a grid point: the formula's power averaged over the same sixth of an octave the measurement is smoothed over.
    double expectedDb (const System& sys, double centreHz)
    {
        double sum = 0.0;
        const int n = 41;
        for (int i = 0; i < n; ++i)
        {
            const double f = centreHz * std::pow (2.0, (-1.0 + 2.0 * (double) i / (double) (n - 1)) / 12.0);
            sum += std::pow (10.0, sys.responseDb (f) / 10.0);
        }
        return 10.0 * std::log10 (sum / (double) n);
    }

    // What the Result would record: the test program through the system, after a delay.
    std::vector<float> simulate (System sys, const SweepSpec& spec, float levelDb, std::function<void (std::vector<double>&)> shape = {})
    {
        const auto program = makeProgram (spec, fs, std::pow (10.0f, levelDb / 20.0f));
        const int length = captureSamples (spec, fs);
        std::vector<double> y ((size_t) length, 0.0);
        for (size_t i = 0; i < program.size(); ++i)
        {
            const size_t k = i + (size_t) sys.delaySamples;
            const double v = sys.process ((double) program[i]);
            if (k < y.size()) y[k] = v;
        }
        for (size_t i = program.size(); i < (size_t) length; ++i)   // the filters ring on after the program ends
        {
            const size_t k = i + (size_t) sys.delaySamples;
            const double v = sys.process (0.0);
            if (k < y.size()) y[k] = v;
        }
        if (shape) shape (y);
        std::vector<float> out (y.size());
        for (size_t i = 0; i < y.size(); ++i) out[i] = (float) y[i];
        return out;
    }

    // The largest difference from the formula over 30 Hz to 18 kHz, in decibels.
    double worstError (const Measurement& m, const System& sys, double* atHz = nullptr)
    {
        double worst = 0.0;
        if (m.valid.size() < (size_t) gridPoints || m.magnitudeDb.size() < (size_t) gridPoints)
            return 1.0e9;   // nothing was measured
        for (int i = 0; i < gridPoints; ++i)
        {
            const double f = gridFrequency (i);
            if (f < 30.0 || f > 18000.0)
                continue;
            if (! m.valid[(size_t) i])
                return 1.0e9;
            const double e = std::abs ((double) m.magnitudeDb[(size_t) i] - expectedDb (sys, f));
            if (e > worst) { worst = e; if (atHz) *atHz = f; }
        }
        return worst;
    }

    // --- Processors and a host loop ------------------------------------------------------------------------
    void setParam (SongsmithPluginAnalyzerProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }
    float getParam (SongsmithPluginAnalyzerProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    void pump (int ms = 60) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    // Generous on purpose: it returns the moment the work is done, and a busy or slow machine (the hosted macOS runner that builds the
    // universal packaging build) can take far longer than a developer's computer to finish an analysis on a background thread.
    bool waitUntil (const std::function<bool()>& done, int timeoutMs = 120000)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
        while (! done() && juce::Time::getMillisecondCounter() < end)
            pump (20);
        return done();
    }

    // A Result in a session (its Role changes on the message thread, so the loop runs until the session counts follow).
    void makeResult (SongsmithPluginAnalyzerProcessor& p, int session)
    {
        // Wait for this instance to join, not merely for someone to be there (another instance may already be in the session).
        const int before = SongsmithPluginAnalyzerProcessor::resultsIn (session);
        setParam (p, sessionId, (float) session);
        setParam (p, roleId, 1.0f);
        waitUntil ([&] { return SongsmithPluginAnalyzerProcessor::resultsIn (session) >= before + 1; }, 15000);
    }
    void makeReference (SongsmithPluginAnalyzerProcessor& p, int session)
    {
        // A new instance starts as a Reference in the first session, so only a move to another session is a change to wait for.
        const int before = SongsmithPluginAnalyzerProcessor::referencesIn (session);
        setParam (p, sessionId, (float) session);
        setParam (p, roleId, 0.0f);
        if (session != 0)
            waitUntil ([&] { return SongsmithPluginAnalyzerProcessor::referencesIn (session) >= before + 1; }, 15000);
    }

    // One pass of the host: the Reference, then whatever is between, then the Result.
    struct Host
    {
        SongsmithPluginAnalyzerProcessor& reference;
        SongsmithPluginAnalyzerProcessor& result;
        int block = 512;
        std::function<void (juce::AudioBuffer<float>&)> between;
        std::mt19937 rng { 5 };

        // The host starts running before anyone presses Start.
        void prime()
        {
            reference.prepareToPlay (fs, block);
            result.prepareToPlay (fs, block);
            for (int i = 0; i < 3; ++i) runBlock();
        }

        void runBlock (juce::AudioBuffer<float>* keep = nullptr)
        {
            juce::AudioBuffer<float> buf (2, block);
            std::uniform_real_distribution<float> noise (-0.05f, 0.05f);   // the track's own sound, which the sweep replaces
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                    buf.setSample (c, i, noise (rng));
            juce::MidiBuffer midi;
            reference.processBlock (buf, midi);
            if (between) between (buf);
            result.processBlock (buf, midi);
            if (keep != nullptr) keep->makeCopyOf (buf);
        }

        // Presses Start and runs until the capture is complete (or the limit is reached). Returns true when it completed.
        bool measure (int maxBlocks = 0)
        {
            // The whole capture is about 12 seconds of audio, so allow 20 seconds at any block size.
            if (maxBlocks <= 0) maxBlocks = (int) (20.0 * fs / (double) block) + 1;
            reference.requestStart();
            for (int b = 0; b < maxBlocks; ++b)
            {
                runBlock();
                if (result.getCapturedSamples() >= result.getCaptureLength())
                    break;
            }
            const bool completed = waitUntil ([&] { result.poll(); return result.getMeasurementCount() > 0; });
            if (! completed)
                std::printf ("  (measurement did not finish: captured %d of %d samples, status text '%s')\n", result.getCapturedSamples(), result.getCaptureLength(),
                             result.getStatus().text.toRawUTF8());
            return completed;
        }
    };

    // A system between the Reference and the Result, as a function on a block (kept state across blocks).
    std::function<void (juce::AudioBuffer<float>&)> through (std::shared_ptr<System> sys)
    {
        auto delayLine = std::make_shared<std::vector<double>> ((size_t) sys->delaySamples, 0.0);
        auto pos = std::make_shared<size_t> (0);
        return [sys, delayLine, pos] (juce::AudioBuffer<float>& buf)
        {
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                double v = sys->process ((double) buf.getSample (0, i));
                if (! delayLine->empty())
                {
                    const double out = (*delayLine)[*pos];
                    (*delayLine)[*pos] = v;
                    *pos = (*pos + 1) % delayLine->size();
                    v = out;
                }
                for (int c = 0; c < buf.getNumChannels(); ++c)
                    buf.setSample (c, i, (float) v);
            }
        };
    }
}

//==============================================================================
class PluginAnalyzerMeasurementTests final : public juce::UnitTest
{
public:
    PluginAnalyzerMeasurementTests() : juce::UnitTest ("Songsmith Plugin Analyzer measurement (ANALYZER-01)", "PluginAnalyzer") {}

    void runTest() override
    {
        const SweepSpec spec;

        beginTest ("The test program: two logarithmic sweeps from 20 Hz to 20 kHz, faded at both ends, in a known layout");
        {
            const auto sweep = makeSweep (spec, fs);
            expectEquals ((int) sweep.size(), sweepSamples (spec, fs));
            float peak = 0.0f;
            for (auto v : sweep) peak = std::max (peak, std::abs (v));
            expectWithinAbsoluteError (peak, 1.0f, 0.01f, "unit amplitude");
            expectWithinAbsoluteError (sweep.front(), 0.0f, 1.0e-6f, "starts at zero");
            expectLessThan (std::abs (sweep.back()), 0.01f, "ends at (nearly) zero");

            // Zero crossings per second at the start and end read the instantaneous frequency: 20 Hz up to 20 kHz.
            auto crossingsIn = [&] (int from, int length)
            {
                int n = 0;
                for (int i = from + 1; i < from + length; ++i)
                    if ((sweep[(size_t) i - 1] < 0.0f) != (sweep[(size_t) i] < 0.0f)) ++n;
                return 0.5 * (double) n / ((double) length / fs);
            };
            expectWithinAbsoluteError (crossingsIn (0, (int) (0.2 * fs)), 25.0, 5.0, "about 20 to 30 Hz in the first fifth of a second");
            expectGreaterThan (crossingsIn ((int) sweep.size() - (int) (0.2 * fs), (int) (0.1 * fs)), 15000.0, "about 20 kHz at the end");

            const auto program = makeProgram (spec, fs, 0.5f);
            expectEquals ((int) program.size(), programSamples (spec, fs));
            expectEquals (programSamples (spec, fs), samplesFor (spec.leadInSeconds, fs) + 2 * (sweepSamples (spec, fs) + samplesFor (spec.gapSeconds, fs)));
            expectEquals (program[100], 0.0f, "silence in the lead-in");
            float programPeak = 0.0f;
            for (auto v : program) programPeak = std::max (programPeak, std::abs (v));
            expectWithinAbsoluteError (programPeak, 0.5f, 0.01f, "scaled to the level asked for");
            // The same program twice (to see whether the processing changes), and a gap after each sweep.
            const int rep = repetitionSamples (spec, fs), lead = samplesFor (spec.leadInSeconds, fs);
            for (int i = 0; i < (int) sweep.size(); i += 997)
                expectEquals (program[(size_t) (lead + i)], program[(size_t) (lead + rep + i)], "the second sweep is the first again");
            expectEquals (program[(size_t) (lead + (int) sweep.size() + 50)], 0.0f, "the gap is silent");

            // At a low sample rate the sweep stops below the Nyquist limit instead of aliasing.
            expectLessThan (topFrequency (spec, 32000.0), 14500.0);
        }

        beginTest ("Identity: with nothing between the instances the curve is flat at 0 dB (the bypass reference), at any delay");
        {
            for (int delay : { 0, 5, 777, 4800, 24000 })
            {
                System sys;
                sys.delaySamples = delay;
                const auto m = analyse (simulate (sys, spec, -18.0f), captureSamples (spec, fs), spec, fs, -18.0f);
                expect (m.isValidCurve(), "valid at a delay of " + juce::String (delay));
                expectLessThan (worstError (m, sys), 0.1, "flat at a delay of " + juce::String (delay));
                expectWithinAbsoluteError (m.delaySamples, (double) delay, 1.5, "the delay is found, " + juce::String (delay) + " samples");
                expectEquals ((int) m.flags, 0, "nothing flagged");
            }
        }

        beginTest ("Known filters within half a decibel from 30 Hz to 18 kHz: gain, high-pass, bell boost and cut, low shelf");
        {
            struct Case { const char* name; System sys; };
            std::vector<Case> cases;
            { Case c { "a flat gain of +6 dB", {} }; c.sys.gain = 2.0; cases.push_back (c); }
            { Case c { "a flat cut of -12 dB", {} }; c.sys.gain = 0.25; cases.push_back (c); }
            { Case c { "a high-pass at 100 Hz", {} }; c.sys.stages = { Biquad::highPass (100.0, 0.7071) }; cases.push_back (c); }
            { Case c { "a high-pass at 30 Hz", {} }; c.sys.stages = { Biquad::highPass (30.0, 0.7071) }; cases.push_back (c); }
            { Case c { "a bell boost of +6 dB at 1 kHz", {} }; c.sys.stages = { Biquad::peak (1000.0, 1.0, 6.0) }; cases.push_back (c); }
            { Case c { "a bell cut of -9 dB at 3 kHz", {} }; c.sys.stages = { Biquad::peak (3000.0, 1.4, -9.0) }; cases.push_back (c); }
            { Case c { "a bell boost of +4 dB at 10 kHz", {} }; c.sys.stages = { Biquad::peak (10000.0, 1.0, 4.0) }; cases.push_back (c); }
            { Case c { "a low shelf of +5 dB at 100 Hz", {} }; c.sys.stages = { Biquad::lowShelf (100.0, 0.707, 5.0) }; cases.push_back (c); }
            { Case c { "three filters together, with a delay", {} }; c.sys.stages = { Biquad::highPass (60.0, 0.7071), Biquad::peak (400.0, 0.8, -5.0), Biquad::peak (5000.0, 1.0, 3.5) };
                                                                    c.sys.delaySamples = 2400; cases.push_back (c); }
            for (auto& c : cases)
            {
                const auto m = analyse (simulate (c.sys, spec, -18.0f), captureSamples (spec, fs), spec, fs, -18.0f);
                double at = 0.0;
                const double err = worstError (m, c.sys, &at);
                expect (m.isValidCurve() && m.flags == 0, juce::String (c.name) + ": measured cleanly (flags " + juce::String ((int) m.flags) + ")");
                expectLessThan (err, 0.5, juce::String (c.name) + ": worst difference " + juce::String (err, 3) + " dB at " + juce::String (at, 0) + " Hz");
            }
        }

        beginTest ("A noisy path still reads true: white noise at -70 dBFS does not move the curve by half a decibel");
        {
            System sys;
            sys.stages = { Biquad::peak (2000.0, 1.0, 6.0) };
            auto noisy = [] (std::vector<double>& y)
            {
                std::mt19937 gen (11);
                std::normal_distribution<double> n (0.0, 0.00032);   // about -70 dBFS RMS
                for (auto& v : y) v += n (gen);
            };
            const auto m = analyse (simulate (sys, spec, -18.0f, noisy), captureSamples (spec, fs), spec, fs, -18.0f);
            expect (m.isValidCurve());
            expectLessThan (worstError (m, sys), 0.5);
        }

        beginTest ("It never claims what it cannot know: silence, a quiet path, clipping, a path that changes, and unrelated sound");
        {
            System sys;
            const auto silence = std::vector<float> ((size_t) captureSamples (spec, fs), 0.0f);
            auto m = analyse (silence, (int) silence.size(), spec, fs, -18.0f);
            expect ((m.flags & flagNoSignal) != 0 && ! m.isValidCurve(), "silence is flagged and shows no curve");
            expect (describeFlags (m).size() == 1 && describeFlags (m)[0].contains ("Nothing reached the After Plugin"));

            System quiet; quiet.gain = 0.002;   // -54 dB: the sweep arrives below the low-signal line
            m = analyse (simulate (quiet, spec, -36.0f), captureSamples (spec, fs), spec, fs, -36.0f);
            expect ((m.flags & flagLowSignal) != 0, "a very quiet arrival is flagged");

            System loud; loud.gain = 30.0;      // +30 dB, hard-limited at full scale
            auto clip = [] (std::vector<double>& y) { for (auto& v : y) v = juce::jlimit (-1.0, 1.0, v); };
            m = analyse (simulate (loud, spec, -18.0f, clip), captureSamples (spec, fs), spec, fs, -18.0f);
            expect ((m.flags & flagClipped) != 0, "reaching full scale is flagged");
            expect (describeFlags (m).joinIntoString (" ").contains ("Lower Test Level"));

            // A path that changes with time: a tremolo of 4 dB at 3 Hz. The two sweeps meet it at different moments.
            auto tremolo = [] (std::vector<double>& y)
            {
                for (size_t i = 0; i < y.size(); ++i)
                    y[i] *= std::pow (10.0, 3.0 * std::sin (2.0 * juce::MathConstants<double>::pi * 0.37 * (double) i / fs) / 20.0);
            };
            m = analyse (simulate (sys, spec, -18.0f, tremolo), captureSamples (spec, fs), spec, fs, -18.0f);
            expect ((m.flags & flagTimeVarying) != 0, "a path that changes over time is flagged (the sweeps differ by " + juce::String (m.repeatDifferenceDb, 2) + " dB)");
            expectGreaterThan (m.repeatDifferenceDb, 0.5f);
            expect (describeFlags (m).joinIntoString (" ").contains ("not a fixed equaliser curve"));

            // The same path without the change reads clean: the flag is not always on.
            m = analyse (simulate (sys, spec, -18.0f), captureSamples (spec, fs), spec, fs, -18.0f);
            expectLessThan (m.repeatDifferenceDb, 0.1f);

            // Sound that has nothing to do with the test program cannot be lined up, and says so.
            std::mt19937 gen (3);
            std::normal_distribution<float> n (0.0f, 0.1f);
            std::vector<float> unrelated ((size_t) captureSamples (spec, fs));
            for (auto& v : unrelated) v = n (gen);
            m = analyse (unrelated, (int) unrelated.size(), spec, fs, -18.0f);
            expect ((m.flags & flagNotAligned) != 0 && ! m.isValidCurve(), "unrelated sound is not turned into a curve (quality " + juce::String (m.alignmentQuality, 1) + ")");
        }

        beginTest ("A level-dependent path is measured at the test level only, and a measured curve has no invented parts");
        {
            // A gentle saturator changes with level but not with time, so both sweeps agree: it reads as a curve at this level. The
            // ticket says static linear EQ comes first and says nonlinear devices are outside the guarantee; the CSV and the README say so too.
            System sys;
            auto saturate = [] (std::vector<double>& y) { for (auto& v : y) v = std::tanh (3.0 * v) / 3.0; };
            const auto m = analyse (simulate (sys, spec, -18.0f, saturate), captureSamples (spec, fs), spec, fs, -18.0f);
            expect (m.isValidCurve());
            int measured = 0;
            for (int i = 0; i < gridPoints; ++i)
            {
                measured += m.valid[(size_t) i];
                expect (std::isfinite (m.magnitudeDb[(size_t) i]), "no NaN or infinity");
            }
            expectGreaterThan (measured, gridPoints * 9 / 10, "nearly the whole range is measured");
            for (int i = 1; i < gridPoints; ++i)
                expect (m.frequencyHz[(size_t) i] > m.frequencyHz[(size_t) i - 1], "frequencies increase");
            expectWithinAbsoluteError (m.frequencyHz.front(), 20.0f, 0.01f);
            expectWithinAbsoluteError (m.frequencyHz.back(), 20000.0f, 1.0f);
        }

        beginTest ("Other sample rates: 44.1, 96 and 32 kHz measure the same way");
        {
            for (double rate : { 44100.0, 96000.0 })
            {
                // (The reference biquads above are for 48 kHz, so use a flat gain and a pure delay here.)
                const auto program = makeProgram (spec, rate, 0.125f);
                const int length = captureSamples (spec, rate);
                std::vector<float> y ((size_t) length, 0.0f);
                for (size_t i = 0; i < program.size(); ++i)
                    if (i + 300 < y.size()) y[i + 300] = 2.0f * program[i];
                const auto m = analyse (y, length, spec, rate, -18.0f);
                expect (m.isValidCurve(), juce::String (rate) + " Hz is measured");
                for (int i = 0; i < gridPoints; ++i)
                {
                    const double f = gridFrequency (i);
                    if (f >= 30.0 && f <= 18000.0 && m.valid[(size_t) i])
                        expectWithinAbsoluteError ((double) m.magnitudeDb[(size_t) i], 6.02, 0.3, juce::String (rate) + " Hz at " + juce::String (f, 0) + " Hz");
                }
            }
        }
    }
};

//==============================================================================
class PluginAnalyzerPluginTests final : public juce::UnitTest
{
public:
    PluginAnalyzerPluginTests() : juce::UnitTest ("Songsmith Plugin Analyzer plugin (ANALYZER-01)", "PluginAnalyzer") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Plugin Analyzer"));
            expectEquals (juce::String (ANA_MANUFACTURER_CODE), juce::String ("Tnyw"));
            expectEquals (juce::String (ANA_PLUGIN_CODE), juce::String ("Anl1"));
            for (const char* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1", "Opt1", "Peq1", "Bus1" })
                expect (juce::String (ANA_PLUGIN_CODE) != other, juce::String ("different from ") + other);
            SongsmithPluginAnalyzerProcessor p;
            expectEquals (p.getName(), juce::String ("Songsmith Plugin Analyzer"));
            expectEquals (p.getLatencySamples(), 0, "it adds no delay");
            expectEquals (p.getTailLengthSeconds(), 0.0);
        }

        beginTest ("Parameter IDs, ranges and defaults are stable");
        {
            SongsmithPluginAnalyzerProcessor p;
            struct Row { const char* id; float lo, hi, def; const char* name; };
            const Row rows[] = { { "role", 0, 1, 0, "Role" }, { "session", 0, 7, 0, "Session" }, { "channel", 0, 2, 0, "Channel" },
                                 { "test_level", -36.0f, -6.0f, -18.0f, "Test Level" }, { "bypass", 0, 1, 0, "Bypass" } };
            for (auto& r : rows)
            {
                auto* prm = p.apvts.getParameter (r.id);
                expect (prm != nullptr, juce::String (r.id) + " exists");
                if (prm == nullptr) continue;
                expectEquals (prm->getName (100), juce::String (r.name));
                expectEquals (prm->convertFrom0to1 (0.0f), r.lo, r.id);
                expectEquals (prm->convertFrom0to1 (1.0f), r.hi, r.id);
                expectEquals (prm->convertFrom0to1 (prm->getDefaultValue()), r.def, r.id);
            }
            expectEquals (p.getParameters().size(), 5, "no other parameters");
            expectEquals ((int) p.getBypassParameter()->getParameterIndex() >= 0 ? 1 : 0, 1, "the host's own bypass is the Bypass parameter");
        }

        beginTest ("A Reference and a Result in the same session measure a known filter end to end, at every block size and with delay");
        for (int block : { 64, 333, 512, 4096 })
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            auto sys = std::make_shared<System>();
            sys->stages = { Biquad::highPass (80.0, 0.7071), Biquad::peak (1500.0, 1.0, 5.0) };
            sys->delaySamples = 480;   // a plugin with 10 ms of latency
            Host host { ref, res, block, through (sys) };
            host.prime();
            expect (host.measure(), "the measurement completes at block size " + juce::String (block));
            const auto& slot = res.getSlot (0);
            expect (slot.filled, "the slot is filled");
            double at = 0.0;
            const double err = worstError (slot.measurement, *sys, &at);
            expectLessThan (err, 0.5, "block " + juce::String (block) + ": worst difference " + juce::String (err, 3) + " dB at " + juce::String (at, 0) + " Hz");
            expectWithinAbsoluteError (slot.measurement.delaySamples, 480.0, 2.0, "the latency is found");
            expectEquals ((int) slot.measurement.flags, 0, "nothing flagged");
            expectEquals (slot.channel, juce::String ("Left"));
            expect (slot.measuredAt.isNotEmpty());
            expect (res.getStatus().status == SongsmithPluginAnalyzerProcessor::Status::finished, res.getStatus().text);
        }

        beginTest ("Sessions pair only with the same number: a different session never sees the sweep");
        {
            SongsmithPluginAnalyzerProcessor ref, other;
            makeReference (ref, 1);
            makeResult (other, 2);
            expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (1), 1);
            expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (2), 1);
            expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (1), 0);
            Host host { ref, other, 512, {} };
            host.prime();
            ref.requestStart();
            for (int b = 0; b < 400; ++b) host.runBlock();
            expectEquals (other.getCapturedSamples(), 0, "nothing was recorded in the other session");
            expect (! other.getSlot (0).filled);
            const auto status = other.getStatus();
            expect (status.status == SongsmithPluginAnalyzerProcessor::Status::problem && status.text.contains ("No Before Plugin found"), status.text);
            expect (status.text.contains ("Put a Before Plugin before"), "it says how to fix it");

            // Two pairs in two sessions at the same time do not mix.
            SongsmithPluginAnalyzerProcessor ref2, res1, res2;
            makeReference (ref2, 2);
            makeResult (res1, 1);
            makeResult (res2, 2);
            auto sysA = std::make_shared<System>(), sysB = std::make_shared<System>();
            sysA->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            sysB->stages = { Biquad::peak (1000.0, 1.0, -6.0) };
            Host hostA { ref, res1, 512, through (sysA) }, hostB { ref2, res2, 512, through (sysB) };
            hostA.prime(); hostB.prime();
            ref.requestStart(); ref2.requestStart();
            for (int b = 0; b < 2400 && (res1.getCapturedSamples() < res1.getCaptureLength() || res2.getCapturedSamples() < res2.getCaptureLength()); ++b)
            {
                hostA.runBlock();
                hostB.runBlock();
            }
            waitUntil ([&] { res1.poll(); res2.poll(); return res1.getMeasurementCount() > 0 && res2.getMeasurementCount() > 0; });
            expectLessThan (worstError (res1.getSlot (0).measurement, *sysA), 0.5, "pair one measured its own filter");
            expectLessThan (worstError (res2.getSlot (0).measurement, *sysB), 0.5, "pair two measured its own filter");
        }

        beginTest ("Pairing mistakes are named in plain words: two References, no Result, a Result with no Reference, Bypass");
        {
            SongsmithPluginAnalyzerProcessor a, b;
            makeReference (a, 3);
            makeReference (b, 3);
            expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (3), 2);
            expect (a.getStatus().text.contains ("More than one Before Plugin is paired here"), a.getStatus().text);
            // The two References must not both send a sweep.
            SongsmithPluginAnalyzerProcessor r;
            makeResult (r, 3);
            Host host { a, r, 512, {} };
            host.prime();
            a.requestStart();
            for (int i = 0; i < 100; ++i) host.runBlock();
            expectEquals (r.getCapturedSamples(), 0, "with two References nothing is sent");

            setParam (b, roleId, 1.0f);
            waitUntil ([&] { return SongsmithPluginAnalyzerProcessor::referencesIn (3) == 1; }, 15000);
            expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (3), 1, "changing the role moves the instance between the counts");
            expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (3), 2);

            SongsmithPluginAnalyzerProcessor lonely;
            makeReference (lonely, 5);
            expect (lonely.getStatus().text.contains ("No After Plugin found"), lonely.getStatus().text);

            setParam (lonely, bypassId, 1.0f);
            expect (lonely.getStatus().text.contains ("Bypass is on"), lonely.getStatus().text);
        }
        expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (3) + SongsmithPluginAnalyzerProcessor::resultsIn (3)
                      + SongsmithPluginAnalyzerProcessor::referencesIn (5), 0, "instances that are gone have left their sessions");

        beginTest ("Audio rules: untouched when not measuring, the sweep replaces the sound only while measuring, and the Result changes nothing");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            ref.prepareToPlay (fs, 256);
            res.prepareToPlay (fs, 256);
            juce::MidiBuffer midi;

            auto filled = [] { juce::AudioBuffer<float> b (2, 256); for (int i = 0; i < 256; ++i) { b.setSample (0, i, 0.01f * (float) i / 256.0f); b.setSample (1, i, -0.02f); } return b; };
            // Idle: both leave every sample exactly as it was.
            for (auto* p : { &ref, &res })
            {
                auto b = filled(); const auto original = filled();
                p->processBlock (b, midi);
                bool same = true;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                        same = same && b.getSample (c, i) == original.getSample (c, i);
                expect (same, "idle: the sound passes through untouched");
            }

            ref.requestStart();
            auto b = filled();
            ref.processBlock (b, midi);    // the first block of the lead-in is silent
            expectEquals (b.getSample (0, 100), 0.0f, "the lead-in is silence");
            // Run to where the first sweep is clearly sounding and check the Reference is sending it, at the level asked for.
            float peak = 0.0f;
            const int lead = samplesFor (SweepSpec().leadInSeconds, fs);
            for (int block = 1; block * 256 < lead + 24000; ++block)
            {
                auto x = filled();
                ref.processBlock (x, midi);
                if (block * 256 > lead + 2000)
                    peak = std::max (peak, x.getMagnitude (0, 256));
                res.processBlock (x, midi);
            }
            expectWithinAbsoluteError (peak, std::pow (10.0f, -18.0f / 20.0f), 0.01f, "the sweep is sent at the Test Level, -18 dB");

            // Stop: the sound comes back at once.
            ref.requestStop();
            auto after = filled();
            ref.processBlock (after, midi);
            expectEquals (after.getSample (1, 10), -0.02f, "after Stop the sound is the host's again");
            expect (ref.getStatus().status != SongsmithPluginAnalyzerProcessor::Status::sending, "no longer sending");
            expect (! res.getSlot (0).filled, "a stopped measurement keeps nothing");
        }

        beginTest ("Bypass: a bypassed Reference does not start, and a bypassed Result does not record");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            Host host { ref, res, 512, {} };
            host.prime();
            setParam (ref, bypassId, 1.0f);
            ref.requestStart();
            for (int i = 0; i < 100; ++i) host.runBlock();
            expectEquals (res.getCapturedSamples(), 0, "a bypassed Reference sends nothing");
            setParam (ref, bypassId, 0.0f);
            setParam (res, bypassId, 1.0f);
            ref.requestStart();
            for (int i = 0; i < 100; ++i) host.runBlock();
            expectEquals (res.getCapturedSamples(), 0, "a bypassed Result records nothing");
        }

        beginTest ("Stop part-way keeps nothing, and a second measurement afterwards still works");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            auto sys = std::make_shared<System>();
            sys->stages = { Biquad::peak (800.0, 1.0, 4.0) };
            Host host { ref, res, 512, through (sys) };
            host.prime();
            ref.requestStart();
            for (int i = 0; i < 200; ++i) host.runBlock();
            res.requestStop();
            for (int i = 0; i < 50; ++i) host.runBlock();
            expect (res.getCapturedSamples() < res.getCaptureLength(), "the capture did not finish");
            expect (! res.getSlot (0).filled && res.getMeasurementCount() == 0, "nothing was kept");
            expect (host.measure(), "a new measurement completes");
            expectLessThan (worstError (res.getSlot (0).measurement, *sys), 0.5);
        }

        beginTest ("Channels: Left, Right and Both Combined each measure what their channel carries");
        {
            for (int choice : { 0, 1, 2 })
            {
                SongsmithPluginAnalyzerProcessor ref, res;
                makeResult (res, 0);
                setParam (res, channelId, (float) choice);
                auto sys = std::make_shared<System>();          // the left channel is +6 dB
                sys->gain = 2.0;
                Host host { ref, res, 512, {} };
                host.between = [] (juce::AudioBuffer<float>& b)  // only the left channel is boosted; the right is cut by 6 dB
                {
                    for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample (0, i, 2.0f * b.getSample (0, i)); b.setSample (1, i, 0.5f * b.getSample (1, i)); }
                };
                host.prime();
                expect (host.measure());
                const auto& m = res.getSlot (0).measurement;
                const double expected = choice == 0 ? 6.02 : choice == 1 ? -6.02 : 20.0 * std::log10 (0.5 * (2.0 + 0.5));
                int checked = 0;
                for (int i = 0; i < gridPoints; ++i)
                    if (gridFrequency (i) > 100.0 && gridFrequency (i) < 10000.0 && m.valid[(size_t) i])
                    {
                        expectWithinAbsoluteError ((double) m.magnitudeDb[(size_t) i], expected, 0.3, "channel " + juce::String (choice));
                        ++checked;
                    }
                expectGreaterThan (checked, 200);
                expectEquals (res.getSlot (0).channel, channelNames()[choice]);
            }
        }

        beginTest ("A mono host works: one channel in, one channel out");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            ref.setPlayConfigDetails (1, 1, fs, 512);
            res.setPlayConfigDetails (1, 1, fs, 512);
            ref.prepareToPlay (fs, 512);
            res.prepareToPlay (fs, 512);
            juce::MidiBuffer midi;
            for (int i = 0; i < 3; ++i) { juce::AudioBuffer<float> b (1, 512); b.clear(); ref.processBlock (b, midi); res.processBlock (b, midi); }
            ref.requestStart();
            for (int b = 0; b < 4000 && res.getCapturedSamples() < res.getCaptureLength(); ++b)
            {
                juce::AudioBuffer<float> buf (1, 512);
                buf.clear();
                ref.processBlock (buf, midi);
                res.processBlock (buf, midi);
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > 0; });
            expect (res.getSlot (0).filled && res.getSlot (0).measurement.isValidCurve());
            System flat;
            expectLessThan (worstError (res.getSlot (0).measurement, flat), 0.2, "an empty chain reads 0 dB");
        }

        beginTest ("Captures: two named captures, the next one goes where it is told, both are kept and drawn, and Clear All empties them");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            Host host { ref, res, 512, {} };
            host.prime();
            auto sys1 = std::make_shared<System>(), sys2 = std::make_shared<System>();
            sys1->gain = 2.0;
            sys2->stages = { Biquad::peak (1000.0, 1.0, -6.0) };
            res.getSlot (0).name = "Hardware";
            res.getSlot (1).name = "Songsmith";
            res.measureSlot = 0;
            host.between = through (sys1);
            expect (host.measure());
            const int count = res.getMeasurementCount();
            res.measureSlot = 1;
            host.between = through (sys2);
            ref.requestStart();
            for (int b = 0; b < 4000 && res.getCapturedSamples() < res.getCaptureLength() + 1; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength()) break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count; });
            expect (res.getSlot (0).filled && res.getSlot (1).filled, "both captures are filled");
            expectEquals (res.getSlot (0).name, juce::String ("Hardware"));
            expectLessThan (worstError (res.getSlot (0).measurement, *sys1), 0.5, "slot one still holds the first measurement");
            expectLessThan (worstError (res.getSlot (1).measurement, *sys2), 0.5, "slot two holds the second");
            expect (res.captureVisible[0] && res.captureVisible[1], "both captures are drawn, with nothing to choose");

            // Saved with the DAW session and restored, whatever came before.
            juce::MemoryBlock block;
            res.getStateInformation (block);
            SongsmithPluginAnalyzerProcessor restored;
            restored.setStateInformation (block.getData(), (int) block.getSize());
            for (int s = 0; s < numSlots; ++s)
            {
                expect (restored.getSlot (s).filled == res.getSlot (s).filled, "slot " + juce::String (s) + " is filled or empty as it was");
                expectEquals (restored.getSlot (s).name, res.getSlot (s).name);
            }
            expectEquals (restored.getSlot (0).channel, res.getSlot (0).channel);
            expectEquals (restored.getSlot (0).measuredAt, res.getSlot (0).measuredAt);
            double worst = 0.0;
            for (int i = 0; i < gridPoints; ++i)
                worst = std::max (worst, (double) std::abs (restored.getSlot (1).measurement.magnitudeDb[(size_t) i] - res.getSlot (1).measurement.magnitudeDb[(size_t) i]));
            expectLessThan (worst, 1.0e-6, "the curve comes back exactly");
            expectEquals (restored.getSlot (1).measurement.sampleRate, res.getSlot (1).measurement.sampleRate);
            expectEquals (getParam (restored, roleId), 1.0f, "the role is restored too");
            expect (restored.captureVisible[0] && restored.captureVisible[1], "both captures are still shown");
            expectEquals ((int) restored.getSlot (0).measurement.flags, (int) res.getSlot (0).measurement.flags);

            res.captureVisible[1] = false;
            juce::MemoryBlock hiddenBlock;
            res.getStateInformation (hiddenBlock);
            SongsmithPluginAnalyzerProcessor hidden;
            hidden.setStateInformation (hiddenBlock.getData(), (int) hiddenBlock.getSize());
            expect (hidden.captureVisible[0] && ! hidden.captureVisible[1], "a hidden capture stays hidden when the session is reopened");
            expect (hidden.getSlot (1).filled, "and it is not deleted");
            res.clearCaptures();
            expect (! res.getSlot (0).filled && ! res.getSlot (1).filled && res.captureVisible[1], "Clear All empties both captures");
        }

        beginTest ("Damaged or old saved data never breaks a session: a bad slot is empty, a missing one is empty, a newer version loads");
        {
            SongsmithPluginAnalyzerProcessor p;
            // No saved data at all, and a block that is not ours.
            p.setStateInformation (nullptr, 0);
            const char junk[] = "not a state";
            p.setStateInformation (junk, (int) sizeof (junk));
            auto tree = p.createState();
            for (auto ui : tree)
                if (ui.hasType (SongsmithPluginAnalyzerProcessor::uiId))
                {
                    ui.removeAllChildren (nullptr);
                    juce::ValueTree bad ("SLOT");
                    bad.setProperty ("index", 1, nullptr);
                    bad.setProperty ("name", "Broken", nullptr);
                    bad.setProperty ("curve", "AAAA!!!not base64", nullptr);
                    ui.appendChild (bad, nullptr);
                    juce::ValueTree wrongSize ("SLOT");
                    wrongSize.setProperty ("index", 0, nullptr);
                    wrongSize.setProperty ("curve", juce::Base64::toBase64 ("short", 5), nullptr);
                    ui.appendChild (wrongSize, nullptr);
                    juce::ValueTree outOfRange ("SLOT");
                    outOfRange.setProperty ("index", 99, nullptr);
                    ui.appendChild (outOfRange, nullptr);
                    ui.setProperty ("measureSlot", 77, nullptr);
                    ui.setProperty ("visible0", "garbage", nullptr);
                    ui.setProperty ("editorWidth", 1, nullptr);
                }
            tree.setProperty ("stateVersion", 99, nullptr);   // a newer version
            p.restoreFromValueTree (tree);
            expect (! p.getSlot (1).filled && p.getSlot (1).name == "Broken");
            expect (! p.getSlot (0).filled);
            expect (p.measureSlot >= 0 && p.measureSlot < numSlots, "the capture choice is clamped");
            expectGreaterOrEqual (p.editorWidth.load(), EditorLayout::minWidth, "the window width is clamped");
            expectEquals (p.getNumPrograms(), 1);
        }

        beginTest ("The CSV file: a header that says what was measured, a column of decibels for each measurement, and empty cells where nothing was measured");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            expect (res.makeCsvText().isEmpty(), "nothing measured, nothing to export");
            Host host { ref, res, 512, {} };
            host.prime();
            auto sys = std::make_shared<System>();
            sys->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            host.between = through (sys);
            res.getSlot (0).name = "Hardware, \"Punch\"";
            expect (host.measure());
            const auto csv = res.makeCsvText();
            const auto lines = juce::StringArray::fromLines (csv);
            expect (lines[0].startsWith ("# Songsmith Plugin Analyzer ") && lines[0].contains (JucePlugin_VersionString), "the product and version: " + lines[0]);
            expect (csv.contains ("sample rate hz: 48000") && csv.contains ("channel: Left") && csv.contains ("test level db: -18.0"), "the metadata is there");
            expect (csv.contains ("# Column 1 name: \"Hardware, \"\"Punch\"\"\""), "a name with a comma and a quote is quoted");
            expect (csv.contains ("Only the size of the response is measured, not its phase"), "the limits are stated");
            int header = -1;
            for (int i = 0; i < lines.size(); ++i)
                if (lines[i].startsWith ("frequency_hz")) { header = i; break; }
            expectGreaterThan (header, 3);
            expectEquals (lines[header], juce::String ("frequency_hz,\"Hardware, \"\"Punch\"\" (db)\""));
            int rows = 0;
            double previousHz = 0.0;
            for (int i = header + 1; i < lines.size(); ++i)
            {
                if (lines[i].isEmpty()) continue;
                const auto cells = juce::StringArray::fromTokens (lines[i], ",", "");
                expectGreaterOrEqual (cells.size(), 2);
                const double hz = cells[0].getDoubleValue();
                expectGreaterThan (hz, previousHz, "frequencies increase down the table");
                previousHz = hz;
                if (hz > 100.0 && hz < 5000.0)
                    expect (cells[1].isNotEmpty() && std::abs (cells[1].getDoubleValue() - expectedDb (*sys, hz)) < 0.6, "a number in the middle: " + lines[i]);
                ++rows;
            }
            expectEquals (rows, gridPoints, "one row for each frequency");
            expectGreaterOrEqual (csv.indexOf ("\r\n"), 0, "lines end the way spreadsheets expect");

            // Two measurements make two columns, and a point that was not measured is left empty.
            auto& other = res.getSlot (1);
            other = res.getSlot (0);
            other.name = "Songsmith";
            other.measurement.valid[300] = 0;
            const auto two = res.makeCsvText();
            expect (two.contains ("# Column 2 name: Songsmith"));
            bool foundEmpty = false;
            for (auto& l : juce::StringArray::fromLines (two))
                if (l.startsWith (juce::String (gridFrequency (300), 2) + ",") && l.endsWith (","))
                    foundEmpty = true;
            expect (foundEmpty, "the unmeasured point is an empty cell");
        }

        beginTest ("A session saved with four slots (before ANALYZER-02) opens with its filled slots as the two captures, in order, both drawn");
        {
            auto fakeCapture = [] (double offsetDb)
            {
                Measurement m;
                m.sampleRate = 48000.0;
                m.sweeps = 2;
                m.frequencyHz.resize ((size_t) gridPoints);
                m.magnitudeDb.resize ((size_t) gridPoints);
                m.valid.assign ((size_t) gridPoints, (unsigned char) 1);
                for (int i = 0; i < gridPoints; ++i)
                {
                    m.frequencyHz[(size_t) i] = (float) gridFrequency (i);
                    m.magnitudeDb[(size_t) i] = (float) (offsetDb + 0.01 * i);
                }
                return m;
            };
            SongsmithPluginAnalyzerProcessor source;
            auto tree = source.createState();
            tree.setProperty ("stateVersion", 1, nullptr);
            for (auto ui : tree)
                if (ui.hasType (SongsmithPluginAnalyzerProcessor::uiId))
                {
                    ui.removeAllChildren (nullptr);
                    ui.removeProperty ("measureSlot", nullptr);
                    ui.setProperty ("shownSlot", 3, nullptr);
                    ui.setProperty ("comparedSlot", 1, nullptr);
                    for (int i = 0; i < legacySlotCount; ++i)
                    {
                        Slot s;
                        s.clear (i);
                        s.name = i == 1 ? juce::String ("Hardware") : "Measurement " + juce::String (i + 1);
                        s.filled = i == 1 || i == 3;
                        if (s.filled)
                        {
                            s.measurement = fakeCapture (i == 1 ? 3.0 : -2.0);
                            s.channel = "Left";
                        }
                        ui.appendChild (slotToTree (s, i), nullptr);
                    }
                }
            SongsmithPluginAnalyzerProcessor opened;
            opened.restoreFromValueTree (tree);
            expect (opened.getSlot (0).filled && opened.getSlot (1).filled, "the two filled slots are the two captures");
            expectEquals (opened.getSlot (0).name, juce::String ("Hardware"), "a name you chose is kept");
            expectEquals (opened.getSlot (1).name, juce::String ("Capture 2"), "an old default name becomes the new one for its place");
            expectWithinAbsoluteError (opened.getSlot (0).measurement.magnitudeDb[10], 3.1f, 1.0e-4f, "the first capture is the first filled slot's curve");
            expectWithinAbsoluteError (opened.getSlot (1).measurement.magnitudeDb[10], -1.9f, 1.0e-4f, "and the second is the next one");
            expect (opened.captureVisible[0] && opened.captureVisible[1], "both are drawn");
            expect (opened.measureSlot >= 0 && opened.measureSlot < numSlots);
        }

        beginTest ("A capture you have is never replaced by a measurement that has no curve, and the reason is given");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            Host host { ref, res, 512, {} };
            host.prime();
            auto sys = std::make_shared<System>();
            sys->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            host.between = through (sys);
            res.setNextCapture (0, "Hardware");
            expect (host.measure());
            expect (res.getSlot (0).filled && res.getSlot (0).measurement.isValidCurve());
            const auto when = res.getSlot (0).measuredAt;
            expectEquals (res.getSlot (0).name, juce::String ("Hardware"), "the name given with the choice is used");
            expect (res.getKeptNotice().isEmpty());

            // Now nothing reaches the After Plugin: the capture stays, and the musician is told.
            const int count = res.getMeasurementCount();
            res.setNextCapture (0, "Should not be applied");
            host.between = [] (juce::AudioBuffer<float>& buffer) { buffer.clear(); };
            ref.requestStart();
            for (int b = 0; b < 4000; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength())
                    break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count; });
            expect (res.getSlot (0).filled && res.getSlot (0).measurement.isValidCurve(), "the capture is still there, with its curve");
            expectEquals (res.getSlot (0).measuredAt, when, "and it is the one that was made before");
            expectEquals (res.getSlot (0).name, juce::String ("Hardware"), "with its own name");
            expect (res.getKeptNotice().contains ("was kept") && res.getKeptNotice().contains ("Hardware"), res.getKeptNotice());
            expect (res.getStatus().status != SongsmithPluginAnalyzerProcessor::Status::finished, "it is not reported as done");

            // An empty capture takes the result even when it has no curve, so the reason can be read next to it.
            res.setNextCapture (1, "Second");
            ref.requestStart();
            const int count2 = res.getMeasurementCount();
            for (int b = 0; b < 4000; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength())
                    break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count2; });
            expect (res.getSlot (1).filled && ! res.getSlot (1).measurement.isValidCurve(), "an empty capture records what happened");
        }

        beginTest ("Choosing the next pair: the rule picks the other half waiting alone, then a free pair, and keeps a pair together");
        {
            using O = PluginAnalyzer::Occupancy;
            std::array<O, (size_t) PluginAnalyzer::numSessions> none {};
            expectEquals (choosePairSession (Role::reference, none, 0), 0, "nobody about: stay");
            expectEquals (choosePairSession (Role::reference, none, 5), 0, "an empty room: the lowest free pair");

            auto lone = none;
            lone[3].afters = 1;
            expectEquals (choosePairSession (Role::reference, lone, 0), 3, "a Before Plugin joins the After Plugin waiting alone");
            auto lonely = none;
            lonely[2].befores = 1;
            lonely[5].befores = 1;
            expectEquals (choosePairSession (Role::result, lonely, 0), 2, "an After Plugin joins the lowest Before Plugin waiting alone");

            auto complete = none;
            complete[0] = { 1, 1 };
            expectEquals (choosePairSession (Role::reference, complete, 0), 1, "a complete pair is left alone: the next one gets the next free pair");
            auto half = none;
            half[4].befores = 1;
            half[0].befores = 1;
            expectEquals (choosePairSession (Role::result, half, 4), 4, "one half of a pair stays where it is");

            std::array<O, (size_t) PluginAnalyzer::numSessions> full;
            for (auto& o : full)
                o = { 1, 1 };
            expectEquals (choosePairSession (Role::reference, full, 6), 6, "with every pair taken it stays, and the Pair With box appears");
            auto crowded = none;
            crowded[2].befores = 2;
            expectEquals (choosePairSession (Role::result, crowded, 0), 0, "two Before Plugins in one pair is not a lone half to join");
        }

        beginTest ("Pairing happens by itself: a Before Plugin and an After Plugin find each other, the next pair gets its own, and an opened session keeps its pairing");
        {
            SongsmithPluginAnalyzerProcessor::automaticPairing = true;
            {
                SongsmithPluginAnalyzerProcessor a;                        // the first Before Plugin
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (0), 1);
                SongsmithPluginAnalyzerProcessor b;                        // a second instance next to it: a free pair, as a Before Plugin
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (1), 1);
                setParam (b, roleId, 1.0f);                                // made the After Plugin: it joins the Before Plugin that is waiting alone
                waitUntil ([&] { return SongsmithPluginAnalyzerProcessor::resultsIn (0) == 1; }, 15000);
                expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (0), 1, "the After Plugin joined the Before Plugin");
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (0), 1);
                expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (1), 0);
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (1), 0, "and left the free pair it had");
                expectEquals (getParam (b, sessionId), 0.0f);
                expect (! a.offersPairing() && ! b.offersPairing(), "a pair that found each other has nothing to choose");
                expect (b.getStatus().status != SongsmithPluginAnalyzerProcessor::Status::problem, b.getStatus().text);

                // A second pair, made the same way, gets its own.
                SongsmithPluginAnalyzerProcessor c, d;
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (1), 1, "the next Before Plugin takes the next free pair");
                setParam (d, roleId, 1.0f);
                waitUntil ([&] { return SongsmithPluginAnalyzerProcessor::resultsIn (1) == 1; }, 15000);
                expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (1), 1, "its After Plugin finds it");
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (1), 1);
                expectEquals (SongsmithPluginAnalyzerProcessor::resultsIn (0), 1, "and the first pair is untouched");
                expectEquals (SongsmithPluginAnalyzerProcessor::referencesIn (0), 1);
                expect (! c.offersPairing() && ! d.offersPairing());

                // A session that is opened keeps the pairing it was saved with, even when that is not what the rule would pick.
                juce::MemoryBlock saved;
                d.getStateInformation (saved);
                SongsmithPluginAnalyzerProcessor e;
                e.setStateInformation (saved.getData(), (int) saved.getSize());
                pump (100);
                expectEquals (e.getSessionIndex(), 1, "an opened session is not moved");
                expect (e.getRole() == Role::result);
                expect (e.offersPairing(), "two After Plugins in one pair: the Pair With box is offered");
                expect (e.getStatus().text.contains ("More than one After Plugin"), e.getStatus().text);
            }
            SongsmithPluginAnalyzerProcessor::automaticPairing = false;
        }

        beginTest ("The Pair With box is offered only when the plugins cannot tell which pair they are in");
        {
            {
                SongsmithPluginAnalyzerProcessor a, b;
                makeReference (a, 3);
                makeReference (b, 3);
                expect (a.offersPairing() && b.offersPairing(), "two Before Plugins in one pair");
            }
            {
                SongsmithPluginAnalyzerProcessor a, b;
                makeReference (a, 5);
                makeResult (b, 6);
                expect (a.offersPairing() && b.offersPairing(), "each half has its other half somewhere else");
            }
            {
                SongsmithPluginAnalyzerProcessor a, b;
                makeResult (b, 2);
                makeReference (a, 2);
                expect (! a.offersPairing() && ! b.offersPairing(), "a pair that agrees has nothing to choose");
            }
            {
                SongsmithPluginAnalyzerProcessor alone;
                makeReference (alone, 4);
                expect (! alone.offersPairing(), "a plugin with nothing else about has nothing to choose");
            }
        }

        beginTest ("The audio thread does not grow its buffers: the program and capture are made in prepareToPlay");
        {
            SongsmithPluginAnalyzerProcessor p;
            p.prepareToPlay (96000.0, 512);
            expectEquals (p.getProgramLength(), programSamples (SweepSpec(), 96000.0));
            expectEquals (p.getCaptureLength(), captureSamples (SweepSpec(), 96000.0));
            p.prepareToPlay (44100.0, 128);
            expectEquals (p.getProgramLength(), programSamples (SweepSpec(), 44100.0), "preparing again makes them for the new rate");
        }
    }
};

//==============================================================================
class PluginAnalyzerEditorTests final : public juce::UnitTest
{
public:
    PluginAnalyzerEditorTests() : juce::UnitTest ("Songsmith Plugin Analyzer editor (ANALYZER-01)", "PluginAnalyzer") {}

    static std::unique_ptr<SongsmithPluginAnalyzerEditor> make (SongsmithPluginAnalyzerProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithPluginAnalyzerEditor> (dynamic_cast<SongsmithPluginAnalyzerEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        namespace L = PluginAnalyzer::EditorLayout;

        beginTest ("Layout: the controls sit by what they affect, nothing overlaps, Bypass is top-right and the Output meter is at the far right");
        {
            SongsmithPluginAnalyzerProcessor p;
            auto ed = make (p, L::defaultWidth);
            const juce::Rectangle<int> design (0, 0, L::designWidth, L::designHeight);
            std::vector<juce::Component*> parts { &ed->getRoleBox(), &ed->getPairBox(), &ed->getChannelBox(), &ed->getLevelSlider(), &ed->getStartButton(),
                                                  &ed->getStopButton(), &ed->getClearButton(), &ed->getExportButton(), &ed->getBypassButton(), &ed->getRangeBox(),
                                                  &ed->getCaptureRow (0), &ed->getCaptureRow (1), &ed->getGraph(), &ed->getMeterColumn() };
            for (auto* c : parts)
                expect (design.contains (c->getBounds()), c->getTitle() + " " + c->getBounds().toString() + " is inside the editor");
            for (size_t i = 0; i < parts.size(); ++i)
                for (size_t j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), "controls " + juce::String ((int) i) + " and " + juce::String ((int) j) + " do not overlap");

            // Bypass: top-right of the title bar, in the suite's red when on.
            expect (ed->getBypassButton().getRight() <= L::designWidth - 16 && ed->getBypassButton().getY() < (int) Songsmith::UI::titleBarHeight, "Bypass is in the title bar, at the right");
            expect (ed->getBypassButton().getX() > L::designWidth / 2);
            // The Output meter is the rightmost thing, with no Gain Reduction (a measuring tool turns nothing down).
            auto& col = ed->getMeterColumn();
            expectEquals (col.getBounds().getRight(), L::designWidth - Songsmith::UI::MeterColumn::margin);
            expect (! col.hasReduction(), "no made-up Gain Reduction");
            expect (col.getOutput().getTooltip().isNotEmpty() && col.getOutput().getTitle().isNotEmpty());
            expect (ed->getGraph().getRight() < col.getX(), "the graph is left of the meter");
            // The Start button sits with the settings it uses; the slot choices sit under the graph they change; Export sits by them.
            expect (ed->getStartButton().getX() < ed->getGraph().getX() && ed->getStartButton().getY() > ed->getChannelBox().getY(), "Start is under the settings");
            expect (ed->getCaptureRow (0).getY() >= ed->getGraph().getBottom() && ed->getCaptureRow (1).getY() >= ed->getGraph().getBottom(), "the captures are listed under the graph they are drawn on");
            expect (ed->getCaptureRow (0).getY() < ed->getCaptureRow (1).getY(), "the first capture is above the second");
            expect (ed->getChoicePanel().getBounds() == ed->getGraph().getBounds(), "the question about Start covers the graph, where the captures are drawn");
            expect (ed->getExportButton().getY() > ed->getGraph().getBottom(), "Export CSV is under the graph");
            expect (ed->getDiagnosticsBounds().getY() >= L::contentHeight, "the Processing Load strip is below everything");
        }

        beginTest ("Window sizes: 75 %, 100 % and 300 % keep the proportions and the saved width");
        for (int width : { L::minWidth, L::defaultWidth, L::maxWidth })
        {
            SongsmithPluginAnalyzerProcessor p;
            auto ed = make (p, width);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), L::heightFor (width));
            expectEquals (p.editorWidth.load(), width, "the width is saved");
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) L::designWidth, 0.001f);
        }

        beginTest ("The Before Plugin shows what it is, and the After Plugin shows what to do; a capture draws a curve and can be named");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            auto edRef = make (ref, L::defaultWidth);
            edRef->refresh();
            expect (edRef->getGraph().getMessage().contains ("This is the Before Plugin"), edRef->getGraph().getMessage());
            expect (edRef->getGraph().getMessage().contains ("press Start there"), "it says where to press Start");
            expect (! edRef->getChannelBox().isEnabled() && edRef->getLevelSlider().isEnabled(), "the Before Plugin uses Test Level, not Channel");
            expect (! edRef->getStartButton().isVisible(), "Start is on the After Plugin");
            expect (! edRef->getCaptureRow (0).isVisible() && ! edRef->getExportButton().isVisible(), "the Before Plugin has no captures to list");
            expectEquals (edRef->getRoleBox().getItemText (0), juce::String ("Before Plugin"));
            expectEquals (edRef->getRoleBox().getItemText (1), juce::String ("After Plugin"));

            auto edRes = make (res, L::defaultWidth);
            edRes->refresh();
            expect (edRes->getGraph().getMessage().contains ("No capture yet"), edRes->getGraph().getMessage());
            expect (edRes->getStartButton().isVisible() && edRes->getChannelBox().isEnabled() && ! edRes->getLevelSlider().isEnabled(), "the After Plugin uses Channel, not Test Level");
            expect (! edRes->getExportButton().isEnabled(), "nothing to export yet");
            expect (! edRes->getClearButton().isEnabled(), "nothing to clear yet");
            expect (! edRes->getCaptureRow (0).isFilled() && edRes->getCaptureRow (0).getInfoLabel().getText() == "Not made yet");
            expect (edRes->getStatusLabel().getText().isNotEmpty());

            Host host { ref, res, 512, {} };
            host.prime();
            auto sys = std::make_shared<System>();
            sys->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            host.between = through (sys);
            expect (host.measure());
            edRes->refresh();
            expect (edRes->getGraph().getMessage().isEmpty(), "a curve replaces the message");
            expect (edRes->getExportButton().isEnabled(), "there is something to export");
            expect (edRes->getClearButton().isEnabled());
            expectEquals (edRes->getCaptureRow (0).getNameEditor().getText(), juce::String ("Capture 1"), "a first capture has a sensible name");
            expect (edRes->getCaptureRow (0).isFilled() && edRes->getCaptureRow (0).getVisibleToggle().getToggleState());
            expect (edRes->getStatusLabel().getText().startsWith ("Done"), edRes->getStatusLabel().getText());
            expectEquals (edRes->getGraph().getNumCurvesDrawn(), 1, "just the one curve");
            const auto g = edRes->getGraph().getPlotArea();
            expect (edRes->getGraph().yForDb (0.0f) > g.getY() && edRes->getGraph().yForDb (0.0f) < g.getBottom(), "0 dB is inside the graph");
            expect (edRes->getGraph().xForFrequency (1000.0) > edRes->getGraph().xForFrequency (100.0), "frequency runs left to right");
            expectWithinAbsoluteError (edRes->getGraph().frequencyForX (edRes->getGraph().xForFrequency (3000.0)), 3000.0, 1.0);

            // A capture that was flagged says so.
            auto& second = res.getSlot (1);
            second = res.getSlot (0);
            second.name = "Second";
            second.measurement.flags |= flagTimeVarying;
            second.measurement.repeatDifferenceDb = 2.0f;
            edRes->refresh();
            expect (edRes->getNotesLabel().getText().contains ("not a fixed equaliser curve"), edRes->getNotesLabel().getText());
            expect (edRes->getNotesLabel().getText().contains ("Second") && edRes->getNotesLabel().getText().contains ("Capture 1"), "the notes name each capture");
            // With only a capture that has no signal showing, no curve is drawn and the reason is.
            second.measurement.flags = flagNoSignal;
            res.captureVisible[0] = false;
            edRes->refresh();
            expectEquals (edRes->getGraph().getNumCurvesDrawn(), 0);
            expect (edRes->getGraph().getMessage().contains ("Nothing reached the After Plugin"), "no curve is drawn for a capture with no signal");
            res.captureVisible[0] = true;
            second.measurement.flags = 0;

            // Names can be changed at any time, and emptying one goes back to the sensible default.
            edRes->getCaptureRow (1).getNameEditor().setText ("Songsmith Passive EQ", true);
            pump (50);   // the editor reports a text change on the message loop, not at once
            expectEquals (res.getSlot (1).name, juce::String ("Songsmith Passive EQ"));
            edRes->refresh();
            expectEquals (edRes->getCaptureRow (1).getNameEditor().getText(), juce::String ("Songsmith Passive EQ"));
            edRes->getCaptureRow (1).getNameEditor().setText ("", true);
            pump (50);
            expectEquals (res.getSlot (1).name, juce::String ("Capture 2"));
            edRes->getCaptureRow (1).getNameEditor().setText ("Songsmith Passive EQ", true);
            pump (50);

            edRes->getRangeBox().setSelectedId (1, juce::sendNotificationSync);
            expectEquals (edRes->getGraph().getRangeDb(), 6.0f);
            edRes->getRangeBox().setSelectedId (3, juce::sendNotificationSync);
            expectEquals (edRes->getGraph().getRangeDb(), 24.0f);
        }

        beginTest ("Both curves are drawn together by default, in two colours and two line styles, and each can be hidden without deleting it");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            Host host { ref, res, 512, {} };
            host.prime();
            auto sysA = std::make_shared<System>(), sysB = std::make_shared<System>();
            sysA->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            sysB->stages = { Biquad::peak (3000.0, 1.0, -5.0) };
            host.between = through (sysA);
            expect (host.measure());
            auto ed = make (res, L::defaultWidth);
            ed->refresh();
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 1);

            // The second capture is made, and both curves appear with no choice to make.
            const int count = res.getMeasurementCount();
            res.setNextCapture (1, "");
            host.between = through (sysB);
            ref.requestStart();
            for (int b = 0; b < 4000; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength())
                    break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count; });
            ed->refresh();
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 2, "both frequency responses are on the graph");
            expect (ed->getCaptureRow (0).getVisibleToggle().getToggleState() && ed->getCaptureRow (1).getVisibleToggle().getToggleState(), "both are shown");
            const auto& c0 = ed->getGraph().getCurve (0);
            const auto& c1 = ed->getGraph().getCurve (1);
            expect (c0.colour != c1.colour, "two different colours");
            expect (! c0.dashed && c1.dashed, "and two different line styles, so colour is never the only difference");
            expect (c0.colour == PluginAnalyzer::UI::Colours::first, "the first capture is drawn in the first colour");
            expect (c1.colour == PluginAnalyzer::UI::Colours::second, "the second capture is drawn in the second colour");
            expect (std::abs (c0.colour.getHue() - c1.colour.getHue()) > 0.15f, "the hues are well apart");
            expect (c0.measurement != c1.measurement && c0.name != c1.name, "each curve is its own capture, with its own name");
            expectEquals (ed->getCaptureRow (0).getNameEditor().getText(), juce::String ("Capture 1"));
            expectEquals (ed->getCaptureRow (1).getNameEditor().getText(), juce::String ("Capture 2"), "the second capture has a sensible default name");

            // Each capture shows or hides on its own, and hiding does not delete anything.
            auto toggle = [&] (int row, bool on)
            {
                ed->getCaptureRow (row).getVisibleToggle().setToggleState (on, juce::dontSendNotification);
                ed->getCaptureRow (row).getVisibleToggle().onClick();
            };
            toggle (0, false);
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 1, "hiding the first leaves the second");
            expect (ed->getGraph().getCurve (0).measurement == nullptr && ed->getGraph().getCurve (1).measurement != nullptr);
            expect (res.getSlot (0).filled && res.getSlot (0).measurement.isValidCurve(), "the hidden capture is still there");
            toggle (1, false);
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 0);
            expect (ed->getGraph().getMessage().contains ("Both captures are hidden"), ed->getGraph().getMessage());
            toggle (0, true);
            toggle (1, true);
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 2, "and both come back");
            expect (ed->getCaptureRow (0).getVisibleToggle().getTooltip().contains ("does not delete"), "the switch says it does not delete");
        }

        beginTest ("Start is the entry point: the first capture needs no question, later ones ask whether to replace or add, and nothing is replaced silently");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            auto ed = make (res, L::defaultWidth);
            auto& session = PluginAnalyzer::sessionNumber (0);
            auto& panel = ed->getChoicePanel();
            expect (! ed->startNeedsChoice(), "with no capture, Start just runs");
            ed->getStartButton().onClick();
            expect (session.startRequested.load() && ! panel.isOpen(), "the first Start runs the sweep with no question");
            expectEquals (res.measureSlot, 0, "into the first capture");
            session.clear();

            Host host { ref, res, 512, {} };
            host.prime();
            auto sysA = std::make_shared<System>(), sysB = std::make_shared<System>();
            sysA->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
            sysB->stages = { Biquad::peak (3000.0, 1.0, -5.0) };
            host.between = through (sysA);
            expect (host.measure());
            ed->refresh();
            session.clear();

            // One capture: replace it or add a second, naming exactly what would be replaced.
            expect (ed->startNeedsChoice(), "with a capture, Start has to ask");
            ed->getStartButton().onClick();
            expect (panel.isOpen(), "the question is open");
            expect (! session.startRequested.load(), "nothing has started yet");
            expectEquals ((int) panel.getOptions().size(), 3);
            expectEquals (panel.getOptions()[0].label, juce::String ("Replace \"Capture 1\""), "it says which capture would be replaced");
            expectEquals (panel.getOptions()[1].label, juce::String ("Add to Compare"));
            expectEquals (panel.getOptions()[2].label, juce::String ("Cancel"));
            expect (panel.getNameEditor().isVisible() && panel.getNameEditor().getText() == "Capture 2", "a name is offered for the new capture, with a sensible default");
            expect (panel.getTitleLabel().getText().isNotEmpty() && panel.getMessageLabel().getText().contains ("Nothing changes until"), panel.getMessageLabel().getText());
            expect (! ed->getStartButton().isEnabled() || (ed->refresh(), ! ed->getStartButton().isEnabled()), "Start is off while the question is open");
            const auto before = res.getSlot (0).measuredAt;

            panel.getButton (2).onClick();   // Cancel
            expect (! panel.isOpen() && ! session.startRequested.load(), "Cancel changes nothing and starts nothing");
            expect (res.getSlot (0).filled && res.getSlot (0).measuredAt == before);

            ed->getStartButton().onClick();
            panel.getButton (1).onClick();   // Add to Compare
            expect (session.startRequested.load(), "adding starts the sweep");
            expectEquals (res.measureSlot, 1, "into the second capture");
            expect (res.getSlot (0).filled && res.getSlot (0).measuredAt == before, "the first capture is untouched");
            session.clear();
            const int count = res.getMeasurementCount();
            host.between = through (sysB);
            ref.requestStart();
            for (int b = 0; b < 4000; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength())
                    break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count; });
            ed->refresh();
            expect (res.getSlot (0).filled && res.getSlot (1).filled, "both captures exist");
            expectEquals (res.getSlot (1).name, juce::String ("Capture 2"));
            expectEquals (ed->getGraph().getNumCurvesDrawn(), 2, "and both are on the graph");

            // Two captures: which one to replace. The other is kept, and a replaced capture keeps its name.
            ed->getStartButton().onClick();
            expect (panel.isOpen());
            expectEquals ((int) panel.getOptions().size(), 3);
            expectEquals (panel.getOptions()[0].label, juce::String ("Replace \"Capture 1\""));
            expectEquals (panel.getOptions()[1].label, juce::String ("Replace \"Capture 2\""));
            expect (! panel.getNameEditor().isVisible(), "no new name is asked for when replacing");
            const auto firstWhen = res.getSlot (0).measuredAt, secondWhen = res.getSlot (1).measuredAt;
            panel.getButton (1).onClick();   // replace Capture 2
            expectEquals (res.measureSlot, 1);
            expect (res.getSlot (1).filled && res.getSlot (1).measuredAt == secondWhen, "the capture is kept until the new measurement is finished");
            session.clear();
            const int count2 = res.getMeasurementCount();
            host.between = through (sysA);
            ref.requestStart();
            for (int b = 0; b < 4000; ++b)
            {
                host.runBlock();
                if (res.getCapturedSamples() >= res.getCaptureLength())
                    break;
            }
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count2; });
            expect (res.getSlot (1).measuredAt != secondWhen, "the second capture is now the new measurement");
            expect (res.getSlot (0).measuredAt == firstWhen, "and the first was never touched");
            expectEquals (res.getSlot (1).name, juce::String ("Capture 2"), "a replaced capture keeps its name");

            // Escape closes the question.
            ed->refresh();
            ed->getStartButton().onClick();
            expect (panel.isOpen());
            expect (panel.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)) && ! panel.isOpen(), "Escape is the same as Cancel");
        }

        beginTest ("Clear All asks first, keeps everything on Keep, and empties both captures on Clear");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            for (int i = 0; i < numSlots; ++i)
            {
                res.getSlot (i).filled = true;
                res.getSlot (i).measurement.frequencyHz.assign ((size_t) gridPoints, 1000.0f);
                res.getSlot (i).measurement.magnitudeDb.assign ((size_t) gridPoints, 0.0f);
                res.getSlot (i).measurement.valid.assign ((size_t) gridPoints, (unsigned char) 1);
            }
            auto ed = make (res, L::defaultWidth);
            ed->refresh();
            auto& panel = ed->getChoicePanel();
            ed->getClearButton().onClick();
            expect (panel.isOpen());
            expectEquals (panel.getOptions()[0].label, juce::String ("Clear All Captures"));
            expectEquals (panel.getOptions()[1].label, juce::String ("Keep Captures"));
            panel.getButton (1).onClick();
            expect (! panel.isOpen() && res.getSlot (0).filled && res.getSlot (1).filled, "Keep leaves both captures");
            ed->getClearButton().onClick();
            panel.getButton (0).onClick();
            expect (! res.getSlot (0).filled && ! res.getSlot (1).filled, "Clear empties both");
            expectEquals (res.getSlot (0).name, juce::String ("Capture 1"));
        }

        beginTest ("Roles read Before Plugin and After Plugin everywhere, and the old slot and session controls are gone from the normal window");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            for (auto* proc : { &ref, &res })
            {
                auto ed = make (*proc, L::defaultWidth);
                ed->refresh();
                juce::StringArray texts;
                Songsmith::UI::Wording::collect (*ed, texts);
                texts.add (ed->getGraph().getMessage());
                texts.add (ed->getStatusLabel().getText());
                texts.add (ed->getRoleBox().getTooltip());
                texts.add (ed->getStartButton().getTooltip());
                texts.add (ed->getChannelBox().getTooltip());
                texts.add (ed->getLevelSlider().getTooltip());
                texts.add (ed->getGraph().getTooltip());
                texts.add (ed->getExportButton().getTooltip());
                texts.add (proc->makeCsvText());
                for (auto& line : texts)
                {
                    expect (! line.containsWholeWord ("Reference") && ! line.containsWholeWord ("Result"), "no old role name: " + line);
                    expect (! line.contains ("Measure Into") && ! line.contains ("Compare With") && ! line.contains ("Name for the Next Measurement")
                                && ! line.containsWholeWord ("Session"), "no slot or session control: " + line);
                }
                expect (! ed->getPairBox().isVisible() && ! ed->getPairLabel().isVisible(), "no Pair With box when the pair is clear");
            }
            expect (Songsmith::Guidance::titleTooltipFor (JucePlugin_Name).contains ("Before Plugin"));
            expect (! Songsmith::Guidance::titleTooltipFor (JucePlugin_Name).contains ("Reference"));
        }

        beginTest ("The Pair With box shows only when the pairing is ambiguous, and choosing a pair in it moves the plugin to that pair");
        {
            SongsmithPluginAnalyzerProcessor a, b;
            makeReference (a, 3);
            makeReference (b, 3);
            auto ed = make (a, L::defaultWidth);
            ed->refresh();
            expect (ed->getPairBox().isVisible() && ed->getPairLabel().isVisible(), "two Before Plugins in one pair: it is offered");
            expect (ed->getStatusLabel().getText().contains ("Pair With"), ed->getStatusLabel().getText());
            expect (ed->getPairBox().getTooltip().isNotEmpty() && ed->getPairBox().getTitle() == "Pair With");
            expectEquals (ed->getPairBox().getText(), juce::String ("Pair 4"), "the pairs are called Pair 1 to Pair 8");
            ed->getPairBox().setSelectedId (5, juce::sendNotificationSync);
            pump (50);
            expectEquals (getParam (a, sessionId), 4.0f, "it follows the choice");
            makeResult (b, 4);   // the other plugin goes to the same pair
            ed->refresh();
            expect (! ed->getPairBox().isVisible(), "and is hidden again when the pair is clear");
        }

        beginTest ("Buttons: Stop ends it, and Start and Clear All are on the After Plugin only");
        {
            SongsmithPluginAnalyzerProcessor ref, res;
            makeResult (res, 0);
            auto ed = make (ref, L::defaultWidth);
            ed->getStopButton().onClick();
            expect (PluginAnalyzer::sessionNumber (0).stopRequested.load(), "Stop asks it to end");
            expect (! ed->getStartButton().isVisible() && ! ed->getClearButton().isVisible(), "not on the Before Plugin");
            auto edRes = make (res, L::defaultWidth);
            edRes->refresh();
            expect (edRes->getStartButton().isVisible() && edRes->getClearButton().isVisible());
            PluginAnalyzer::sessionNumber (0).clear();
        }

        beginTest ("Plain words and text that fits, in every state, at three window sizes");
        {
            for (int width : { L::minWidth, L::defaultWidth, L::maxWidth })
            {
                SongsmithPluginAnalyzerProcessor ref, res, lone;
                makeResult (res, 0);
                makeReference (lone, 6);
                Host host { ref, res, 512, {} };
                host.prime();
                auto sys = std::make_shared<System>();
                sys->stages = { Biquad::peak (1000.0, 1.0, 6.0) };
                host.between = through (sys);
                host.measure();

                struct Case { SongsmithPluginAnalyzerProcessor* proc; juce::String name; };
                for (auto& c : { Case { &ref, "Before Plugin" }, Case { &res, "After Plugin with a curve" }, Case { &lone, "Before Plugin with no After Plugin" } })
                {
                    auto ed = make (*c.proc, width);
                    ed->refresh();
                    const auto context = "Plugin Analyzer " + c.name + " at " + juce::String (width) + " px";
                    juce::StringArray texts;
                    Songsmith::UI::Wording::collect (*ed, texts);
                    for (auto* prm : c.proc->getParameters())
                        texts.add (prm->getName (100));
                    texts.add (ed->getGraph().getMessage());
                    for (auto& line : describeFlags ([] { Measurement m; m.flags = 0xffu; m.repeatDifferenceDb = 1.2f; return m; }()))
                        texts.add (line);
                    for (auto* s : { &ref, &res, &lone })
                        texts.add (s->getStatus().text);
                    expectGreaterThan (texts.size(), 30, context + ": found the text");
                    Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
                    Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
                    Songsmith::UI::Testing::checkEditorFonts (*this, *ed, context);
                    // The After Plugin shows the whole workflow; the Before Plugin shows what is its own.
                    const bool isAfter = c.proc == &res;
                    for (auto* t : isAfter ? std::initializer_list<const char*> { "Role", "Channel", "Test Level", "Start", "Stop", "Clear All", "Export CSV...", "Bypass", "Range", "Show" }
                                           : std::initializer_list<const char*> { "Role", "Channel", "Test Level", "Stop", "Bypass" })
                        expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
                }
            }
        }

        beginTest ("Every control has a tooltip that says what it does, and an accessible name");
        {
            SongsmithPluginAnalyzerProcessor p;
            auto ed = make (p, L::defaultWidth);
            expectEquals (ed->getRoleBox().getText(), juce::String ("Before Plugin"), "the Role box shows the saved choice, not a blank");
            expect (ed->getPairBox().getText().isNotEmpty(), "the Pair With box shows the saved choice");
            expect (ed->getChannelBox().getText().isNotEmpty(), "the Channel box shows the saved choice");
            for (auto* c : std::initializer_list<juce::Component*> { &ed->getRoleBox(), &ed->getPairBox(), &ed->getChannelBox(), &ed->getLevelSlider(), &ed->getStartButton(),
                                                                     &ed->getStopButton(), &ed->getClearButton(), &ed->getExportButton(), &ed->getBypassButton(),
                                                                     &ed->getRangeBox(), &ed->getCaptureRow (0).getVisibleToggle(), &ed->getCaptureRow (0).getNameEditor(),
                                                                     &ed->getCaptureRow (1).getVisibleToggle(), &ed->getCaptureRow (1).getNameEditor(), &ed->getGraph() })
            {
                auto* tip = dynamic_cast<juce::SettableTooltipClient*> (c);
                expect (tip != nullptr && tip->getTooltip().isNotEmpty(), c->getTitle() + " has a tooltip");
                expect (c->getTitle().isNotEmpty() || dynamic_cast<juce::Button*> (c) != nullptr, "a control has a name for screen readers");
                // (JUCE reports a disabled control as not wanting focus, and several are disabled in this state, so only enabled ones are checked.)
                expect (! c->isEnabled() || c->getWantsKeyboardFocus() || dynamic_cast<juce::Slider*> (c) != nullptr || c == &ed->getGraph(), c->getTitle() + " can be reached by keyboard");
            }
            expect (Songsmith::Guidance::titleTooltipFor (JucePlugin_Name).contains ("Before Plugin"), "the title explains the plugin");
        }

        beginTest ("The Bypass button is the suite's red when on, and follows the parameter");
        {
            SongsmithPluginAnalyzerProcessor p;
            auto ed = make (p, L::defaultWidth);
            expect (! ed->getBypassButton().getToggleState());
            setParam (p, bypassId, 1.0f);
            pump (50);
            expect (ed->getBypassButton().getToggleState(), "it follows the parameter");
            expect (ed->getBypassButton().findColour (juce::TextButton::buttonOnColourId).getARGB() == Songsmith::UI::Colours::bypass.getARGB(), "the suite's bypass red");
        }
    }
};

static PluginAnalyzerMeasurementTests measurementTests;
static PluginAnalyzerPluginTests pluginTests;
static PluginAnalyzerEditorTests editorTests;
static SongsmithUITests sharedUITests { "PluginAnalyzer" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "PluginAnalyzer" };
static SongsmithUpdateTests sharedUpdateTests { "PluginAnalyzer" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// PluginAnalyzerTests --screenshot <file.png> [--scale 2] [--state curve|single|choice|reference|problem|pair]: renders the editor to a PNG, so UI
// changes can be checked without a host. "curve" (the default) is an After Plugin with two captures drawn together; "single" has one;
// "choice" shows the question that Start asks when a capture exists; "pair" shows the Pair With box.
static int renderScreenshot (const juce::File& file, float scale, const juce::String& state)
{
    SongsmithPluginAnalyzerProcessor ref, res;
    std::unique_ptr<SongsmithPluginAnalyzerProcessor> lone;   // only made when needed: a spare Before Plugin would share the first pair
    makeResult (res, 0);
    SongsmithPluginAnalyzerProcessor* shown = &res;
    if (state == "reference")
        shown = &ref;
    else if (state == "problem")
    {
        lone = std::make_unique<SongsmithPluginAnalyzerProcessor>();
        makeReference (*lone, 6);
        shown = lone.get();
    }
    else if (state == "pair")
    {
        lone = std::make_unique<SongsmithPluginAnalyzerProcessor>();
        makeReference (*lone, 0);   // a second Before Plugin in the pair: the plugin cannot tell which is its own
        shown = &ref;
    }
    else
    {
        // Two captures: a bell boost and, over it, a high-pass with a bell cut.
        Host host { ref, res, 512, {} };
        host.prime();
        auto sysA = std::make_shared<System>(), sysB = std::make_shared<System>();
        sysA->stages = { Biquad::peak (3000.0, 1.0, 5.0), Biquad::lowShelf (100.0, 0.707, 4.0) };
        sysB->stages = { Biquad::highPass (80.0, 0.7071), Biquad::peak (400.0, 1.2, -4.0), Biquad::peak (8000.0, 0.8, 3.0) };
        res.setNextCapture (0, "UAD Pultec EQP-1A");
        host.between = through (sysA);
        host.measure();
        if (state != "single" && state != "choice")
        {
            const int count = res.getMeasurementCount();
            res.setNextCapture (1, "Songsmith Passive EQ");
            host.between = through (sysB);
            host.measure();   // (it runs a block before looking, so the first measurement's full capture is not mistaken for this one)
            waitUntil ([&] { res.poll(); return res.getMeasurementCount() > count; });
        }
    }

    auto editor = PluginAnalyzerEditorTests::make (*shown, EditorLayout::defaultWidth);
    editor->refresh();
    if (state == "choice")
    {
        editor->pressStart();   // with a capture, Start asks first
        pump (50);
    }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
        return 1;
    std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());
    return 0;
}

int main (int argc, char* argv[])
{
    SongsmithPluginAnalyzerProcessor::automaticPairing = false;   // the tests place instances by hand; a host pairs them on its own
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshot")
        {
            float scale = 1.0f;
            juce::String state = "curve";
            for (int j = 1; j + 1 < argc; ++j)
            {
                if (juce::String (argv[j]) == "--scale")
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
                if (juce::String (argv[j]) == "--state")
                    state = argv[j + 1];
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, state);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("PluginAnalyzer");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
