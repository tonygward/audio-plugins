// Songsmith Maximiser test runner.
//   MaximiserTests.exe   -> run all unit tests (exit code 1 on failure)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"
#include "MaxParams.h"

namespace
{
    constexpr double testRates[] = { 44100.0, 48000.0, 96000.0 };

    void setParam (SongsmithMaximiserAudioProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithMaximiserAudioProcessor& p, const char* id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    // Deterministic noise in [-amp, amp].
    std::vector<float> noise (int n, float amp, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-amp, amp);
        std::vector<float> v ((size_t) n);
        for (auto& s : v)
            s = dist (rng);
        return v;
    }

    // Runs 'in' (one vector per channel) through the processor in blocks of 'block' samples
    // (or a repeating pattern of sizes), calling 'onBlock' before each block.
    std::vector<std::vector<float>> run (SongsmithMaximiserAudioProcessor& p, const std::vector<std::vector<float>>& in,
                                         std::vector<int> blockSizes,
                                         const std::function<void (int blockIndex, int sample)>& onBlock = {})
    {
        const int numCh = (int) in.size();
        const int total = (int) in[0].size();
        std::vector<std::vector<float>> out (in.size(), std::vector<float> ((size_t) total));

        int maxBlock = 1;
        for (int b : blockSizes)
            maxBlock = std::max (maxBlock, b);

        juce::AudioBuffer<float> buffer (numCh, maxBlock);
        juce::MidiBuffer midi;

        for (int pos = 0, bi = 0; pos < total; ++bi)
        {
            const int num = std::min (blockSizes[(size_t) bi % blockSizes.size()], total - pos);
            if (onBlock)
                onBlock (bi, pos);

            buffer.setSize (numCh, num, false, false, true);
            for (int c = 0; c < numCh; ++c)
                buffer.copyFrom (c, 0, in[(size_t) c].data() + pos, num);

            p.processBlock (buffer, midi);

            for (int c = 0; c < numCh; ++c)
                std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + num, out[(size_t) c].begin() + pos);
            pos += num;
        }
        return out;
    }

    void prepare (SongsmithMaximiserAudioProcessor& p, double fs, int block, int numChannels = 2)
    {
        const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.setRateAndBufferSizeDetails (fs, block);
        p.prepareToPlay (fs, block);
    }

    float maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b, int latency = 0)
    {
        float m = 0.0f;
        for (size_t i = (size_t) latency; i < a.size(); ++i)
            m = std::max (m, std::abs (a[i] - b[i - (size_t) latency]));
        return m;
    }
}

//==============================================================================
class IdentityTests final : public juce::UnitTest
{
public:
    IdentityTests() : juce::UnitTest ("Identity and parameters", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Plug-in identity");
        {
            expectEquals (juce::String (MAX_MANUFACTURER_CODE), juce::String ("Tnyw"), "Songsmith manufacturer code");
            expectEquals (juce::String (MAX_PLUGIN_CODE), juce::String ("Mxlm"), "plug-in code is pinned");
            expect (juce::String (MAX_PLUGIN_CODE) != "Eq3b", "distinct from Songsmith EQ");
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Maximiser"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));

            int upper = 0;
            for (auto c : juce::String (MAX_PLUGIN_CODE))
                upper += juce::CharacterFunctions::isUpperCase (c) ? 1 : 0;
            expectEquals (upper, 1, "AU sub-type has exactly one capital letter");
        }

        beginTest ("Stable parameter IDs and versions");
        {
            SongsmithMaximiserAudioProcessor p;
            const std::pair<const char*, int> expected[] = { { "threshold", 1 }, { "ceiling", 1 }, { "release", 1 }, { "bypass", 1 },
                                                             { "peak_mode", 2 }, { "auto_release", 2 }, { "dither", 3 } };

            for (auto [id, version] : expected)
            {
                auto* rp = p.apvts.getParameter (id);
                expect (rp != nullptr, juce::String ("parameter present: ") + id);
                if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*> (rp))
                    expectEquals (withId->getParameterID(), juce::String (id));
                if (auto* ranged = dynamic_cast<juce::AudioProcessorParameterWithID*> (rp))
                    expectEquals (ranged->getVersionHint(), version, juce::String ("version hint for ") + id);
            }

            expect (p.getBypassParameter() == p.apvts.getParameter (Max::bypassId), "bypass is the host bypass");
            expectEquals (p.getParameters().size(), 7, "no unexpected parameters");
        }

        beginTest ("Ranges and defaults");
        {
            SongsmithMaximiserAudioProcessor p;
            auto range = [&] (const char* id) { return p.apvts.getParameter (id)->getNormalisableRange(); };

            expectEquals (range (Max::thresholdId).start, -30.0f);
            expectEquals (range (Max::thresholdId).end, 0.0f);
            expectEquals (range (Max::ceilingId).start, -30.0f);
            expectEquals (range (Max::ceilingId).end, 0.0f);
            expectWithinAbsoluteError (range (Max::releaseId).start, 0.01f, 1.0e-6f, "release from 0.01 ms");
            expectWithinAbsoluteError (range (Max::releaseId).end, 1000.0f, 1.0e-3f, "release to 1000 ms");

            expectEquals (p.getThresholdDb(), 0.0f, "threshold default exactly 0 dB");
            expectEquals (p.getCeilingDb(), 0.0f, "ceiling default exactly 0 dB");
            expectWithinAbsoluteError (getParam (p, Max::releaseId), 1.0f, 1.0e-6f, "release default 1 ms");
            expect (! p.isBypassed(), "bypass off");

            // Logarithmic release control: each decade gets the same travel.
            auto* rel = p.apvts.getParameter (Max::releaseId);
            expectWithinAbsoluteError (rel->convertTo0to1 (0.01f), 0.0f, 1.0e-4f);
            expectWithinAbsoluteError (rel->convertTo0to1 (1.0f), 0.4f, 1.0e-4f);
            expectWithinAbsoluteError (rel->convertTo0to1 (10.0f), 0.6f, 1.0e-4f);
            expectWithinAbsoluteError (rel->convertTo0to1 (1000.0f), 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (rel->convertFrom0to1 (0.5f), 3.16f, 0.01f, "mid travel is ~3.2 ms");
            expectWithinAbsoluteError (rel->convertFrom0to1 (rel->convertTo0to1 (5.0f)), 5.0f, 1.0e-3f, "5 ms round trip");
        }

        beginTest ("Finite dB floor");
        {
            expectEquals (Max::gainToDb (0.0f), Max::dbFloor, "silence reads as the floor, not -inf");
            expectEquals (Max::gainToDb (-1.0f), Max::dbFloor, "negative gain reads as the floor");
            expectEquals (Max::gainToDb (1.0e-9f), Max::dbFloor, "below the floor clamps");
            expectWithinAbsoluteError (Max::gainToDb (0.5f), -6.0206f, 1.0e-3f);
            expectEquals (Max::dbToGain (Max::dbFloor), 0.0f, "the floor converts to exactly zero gain");
            expectEquals (Max::dbToGain (-500.0f), 0.0f);
            expectWithinAbsoluteError (Max::dbToGain (-6.0206f), 0.5f, 1.0e-5f);
            expect (std::isfinite (Max::gainToDb (std::numeric_limits<float>::min())), "denormal-range gain is finite");
        }
    }
};

//==============================================================================
class ShellProcessingTests final : public juce::UnitTest
{
public:
    ShellProcessingTests() : juce::UnitTest ("Transparency, layouts and bypass", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Mono and stereo layouts");
        {
            SongsmithMaximiserAudioProcessor p;
            juce::AudioProcessor::BusesLayout l;
            for (auto set : { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() })
            {
                l.inputBuses = { set };
                l.outputBuses = { set };
                expect (p.checkBusesLayoutSupported (l), set.getDescription() + " supported");
            }

            l.inputBuses = { juce::AudioChannelSet::mono() };
            l.outputBuses = { juce::AudioChannelSet::stereo() };
            expect (! p.checkBusesLayoutSupported (l), "mono in / stereo out rejected");

            l.inputBuses = { juce::AudioChannelSet::create5point1() };
            l.outputBuses = { juce::AudioChannelSet::create5point1() };
            expect (! p.checkBusesLayoutSupported (l), "5.1 rejected");
        }

        beginTest ("Transparent at defaults (mono, stereo, every rate, odd block sizes)");
        {
            for (double fs : testRates)
                for (int channels : { 1, 2 })
                {
                    SongsmithMaximiserAudioProcessor p;
                    prepare (p, fs, 512, channels);

                    std::vector<std::vector<float>> in;
                    for (int c = 0; c < channels; ++c)
                        in.push_back (noise ((int) fs / 2, 0.5f, 7u + (unsigned) c)); // True peak is the default: keep intersample peaks under 0 dBFS

                    const auto out = run (p, in, { 512, 1, 37, 1024, 300 });
                    const int latency = p.getLatencySamples();

                    float diff = 0.0f;
                    for (int c = 0; c < channels; ++c)
                        diff = std::max (diff, maxAbsDiff (out[(size_t) c], in[(size_t) c], latency));

                    expectEquals (diff, 0.0f, "bit-exact (after latency) at " + juce::String (fs) + " Hz, "
                                                  + juce::String (channels) + " ch");
                }
        }

        beginTest ("Bypass: bit-exact when settled, smooth while fading");
        {
            constexpr double fs = 48000.0;
            SongsmithMaximiserAudioProcessor p;
            prepare (p, fs, 256);

            setParam (p, Max::bypassId, 1.0f);
            const int total = (int) fs;
            std::vector<std::vector<float>> in { noise (total, 0.5f, 1u), noise (total, 0.5f, 2u) };
            const auto out = run (p, in, { 256 });

            const int settled = (int) (fs * SongsmithMaximiserAudioProcessor::bypassFadeSeconds) + 256;
            float diff = 0.0f;
            for (size_t c = 0; c < 2; ++c)
                for (size_t i = (size_t) settled; i < (size_t) total; ++i)
                    diff = std::max (diff, std::abs (out[c][i] - in[c][i - (size_t) p.getLatencySamples()]));
            expectEquals (diff, 0.0f, "bypassed output is the input, bit for bit");

            // A sine through repeated bypass toggles: no step larger than the sine's own slope.
            SongsmithMaximiserAudioProcessor q;
            prepare (q, fs, 128);
            std::vector<float> sine ((size_t) total);
            for (size_t i = 0; i < sine.size(); ++i)
                sine[i] = (float) (0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / fs));
            const auto toggled = run (q, { sine, sine }, { 128 }, [&] (int bi, int)
            {
                if (bi % 20 == 0)
                    setParam (q, Max::bypassId, (bi / 20) % 2 == 0 ? 1.0f : 0.0f);
            });

            const float maxSlope = (float) (0.5 * 2.0 * juce::MathConstants<double>::pi * 1000.0 / fs);
            float worst = 0.0f;
            for (size_t i = 1; i < toggled[0].size(); ++i)
                worst = std::max (worst, std::abs (toggled[0][i] - toggled[0][i - 1]));
            expect (worst <= maxSlope * 1.01f, "no clicks: largest step " + juce::String (worst) + " vs slope " + juce::String (maxSlope));
        }
    }
};

//==============================================================================
class AutomationAndStateTests final : public juce::UnitTest
{
public:
    AutomationAndStateTests() : juce::UnitTest ("Automation and state recall", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Automation reaches the processor");
        {
            SongsmithMaximiserAudioProcessor p;
            prepare (p, 48000.0, 64);

            const std::vector<std::vector<float>> in { noise (48000, 0.25f, 3u), noise (48000, 0.25f, 4u) };
            std::vector<float> seenThreshold;
            const auto out = run (p, in, { 64 }, [&] (int bi, int)
            {
                // Host-style automation: normalised values every block.
                const float t = (float) (bi % 100) / 99.0f;
                p.apvts.getParameter (Max::thresholdId)->setValueNotifyingHost (t);
                p.apvts.getParameter (Max::ceilingId)->setValueNotifyingHost (1.0f - t);
                p.apvts.getParameter (Max::releaseId)->setValueNotifyingHost (t);
                seenThreshold.push_back (p.getThresholdDb());
            });

            bool finite = true;
            for (auto& ch : out)
                for (float s : ch)
                    finite = finite && std::isfinite (s);
            expect (finite, "automated output stays finite");

            expectWithinAbsoluteError (seenThreshold[0], -30.0f, 1.0e-4f, "normalised 0 -> -30 dB");
            expectWithinAbsoluteError (seenThreshold[99], 0.0f, 1.0e-4f, "normalised 1 -> 0 dB");
            expectWithinAbsoluteError (p.getReleaseMs(), Max::releaseRange().convertFrom0to1 ((float) ((750 - 1) % 100) / 99.0f), 1.0e-2f,
                                       "release follows host automation");
        }

        beginTest ("Save / restore round trip");
        {
            SongsmithMaximiserAudioProcessor a;
            setParam (a, Max::thresholdId, -6.5f);
            setParam (a, Max::ceilingId, -1.0f);
            setParam (a, Max::releaseId, 4.0f);
            setParam (a, Max::bypassId, 1.0f);

            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            SongsmithMaximiserAudioProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (b.getThresholdDb(), -6.5f, 1.0e-4f);
            expectWithinAbsoluteError (b.getCeilingDb(), -1.0f, 1.0e-4f);
            expectWithinAbsoluteError (b.getReleaseMs(), 4.0f, 1.0e-3f);
            expect (b.isBypassed(), "bypass restored");

            const auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            expect (xml != nullptr && xml->getIntAttribute ("version") == SongsmithMaximiserAudioProcessor::stateVersion,
                    "state carries its version");
        }

        beginTest ("Restore fills in missing parameters with defaults");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::thresholdId, -10.0f);
            setParam (p, Max::releaseId, 50.0f);

            // A state that only has the ceiling: everything else goes back to its default,
            // not to whatever the instance had before.
            juce::ValueTree saved ("PARAMETERS");
            saved.setProperty ("version", 1, nullptr);
            saved.appendChild (juce::ValueTree ("PARAM", { { "id", "ceiling" }, { "value", -2.0f } }), nullptr);
            p.restoreFromValueTree (saved);

            expectWithinAbsoluteError (p.getCeilingDb(), -2.0f, 1.0e-4f);
            expectEquals (p.getThresholdDb(), Max::defaultThresholdDb);
            expectWithinAbsoluteError (p.getReleaseMs(), Max::defaultReleaseMs, 1.0e-6f);
        }

        beginTest ("Restore is robust to bad, out-of-range and future data");
        {
            SongsmithMaximiserAudioProcessor p;
            juce::ValueTree saved ("PARAMETERS");
            saved.setProperty ("version", 99, nullptr);   // a future version
            saved.appendChild (juce::ValueTree ("PARAM", { { "id", "threshold" }, { "value", -100.0f } }), nullptr);
            saved.appendChild (juce::ValueTree ("PARAM", { { "id", "ceiling" }, { "value", "nan" } }), nullptr);
            saved.appendChild (juce::ValueTree ("PARAM", { { "id", "release" }, { "value", 5000.0f } }), nullptr);
            saved.appendChild (juce::ValueTree ("PARAM", { { "id", "some_future_param" }, { "value", 1.0f } }), nullptr);
            p.restoreFromValueTree (saved);

            expectEquals (p.getThresholdDb(), -30.0f, "clamped to the range");
            expect (std::isfinite (p.getCeilingDb()), "non-numeric value ignored");
            expectWithinAbsoluteError (p.getReleaseMs(), 1000.0f, 1.0e-3f, "clamped to 1000 ms");
            expect (p.apvts.getParameter ("some_future_param") == nullptr, "unknown IDs ignored");

            // Garbage bytes leave the instance untouched.
            setParam (p, Max::thresholdId, -3.0f);
            const char junk[] = "not a saved state";
            p.setStateInformation (junk, (int) sizeof (junk));
            expectWithinAbsoluteError (p.getThresholdDb(), -3.0f, 1.0e-4f);
        }

        beginTest ("Editor opens and closes");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            expect (ed != nullptr && ed->getWidth() > 0 && ed->getHeight() > 0);
        }

        beginTest ("Global Bypass is top-right and uses the suite's Bypass red (SUI-13)");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (Max::bypassId), "Maximiser");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass; Peak Mode and Dither moved to the output settings (SUI-18)");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Maximiser");
            if (auto* me = dynamic_cast<SongsmithMaximiserAudioProcessorEditor*> (ed.get()))
            {
                expect (me->getPeakModeBox().getY() > 50 && me->getDitherBox().getY() > 50, "Peak Mode and Dither are below the title bar");
                expect (me->getTitleBounds().getRight() < me->getStartingPointBox().getX(), "the title does not overlap the preset selector");

                // Saved values still recall: the moved boxes stay bound to their parameters.
                p.apvts.getParameter (Max::ditherId)->setValueNotifyingHost (1.0f);
                expectEquals (me->getDitherBox().getSelectedItemIndex(), (int) std::round (p.apvts.getRawParameterValue (Max::ditherId)->load()));
            }
        }

        beginTest ("Undo and Redo reverse user edits and preset loads, never host automation (SUI-18)");
        {
            SongsmithMaximiserAudioProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Max::thresholdId, Max::releaseId, "Maximiser");
        }
    }
};

//==============================================================================
namespace
{
    std::vector<float> sine (int n, double fs, double freq, double amp, double phase = 0.0)
    {
        std::vector<float> v ((size_t) n);
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / fs;
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = (float) (amp * std::sin (w * (double) i + phase));
        return v;
    }

    float peakOf (const std::vector<float>& v, size_t from = 0, size_t to = std::numeric_limits<size_t>::max())
    {
        float m = 0.0f;
        for (size_t i = from; i < std::min (to, v.size()); ++i)
            m = std::max (m, std::abs (v[i]));
        return m;
    }

    // Runs the bare limiter over 'in' in blocks of 'block' samples.
    std::vector<std::vector<float>> runLimiter (Max::BrickwallLimiter& lim, std::vector<std::vector<float>> in, int block)
    {
        const int total = (int) in[0].size();
        for (int pos = 0; pos < total; pos += block)
        {
            const int num = std::min (block, total - pos);
            float* ch[2] = {};
            for (size_t c = 0; c < in.size(); ++c)
                ch[c] = in[c].data() + pos;
            lim.process (ch, (int) in.size(), num);
        }
        return in;
    }

    // Tolerance for "at or below the ceiling": one float rounding step of the final multiply.
    constexpr float ceilingTolerance = 1.0e-6f;
}

class LimiterTests final : public juce::UnitTest
{
public:
    LimiterTests() : juce::UnitTest ("Brickwall limiter", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Look-ahead and reported latency at 44.1 / 48 / 96 kHz");
        {
            const std::pair<double, int> expected[] = { { 44100.0, 67 }, { 48000.0, 72 }, { 96000.0, 144 } };
            for (auto [fs, samples] : expected)
            {
                expectEquals (Max::BrickwallLimiter::lookAheadSamples (fs), samples, "1.5 ms at " + juce::String (fs));

                SongsmithMaximiserAudioProcessor p;
                prepare (p, fs, 512);
                const int latency = samples + Max::TruePeakDetector::delaySamples;
                expectEquals (p.getLatencySamples(), latency, "reported latency (look-ahead + detector delay) at " + juce::String (fs));

                // A quiet impulse comes out unchanged, exactly 'latency' samples later.
                std::vector<float> imp (2048, 0.0f);
                imp[100] = 0.25f;
                const auto out = run (p, { imp, imp }, { 512 });
                expectEquals (out[0][(size_t) (100 + latency)], 0.25f, "impulse delayed by the latency, unchanged");
                expectEquals (peakOf (out[0], 0, (size_t) (100 + latency)), 0.0f, "nothing before it");
            }
        }

        beginTest ("T = -6, C = -1: +5 dB below threshold, -1 dBFS above it");
        {
            constexpr double fs = 48000.0;
            const float c = Max::dbToGain (-1.0f);

            // Below threshold: a -20 dBFS tone comes out at -15 dBFS.
            {
                SongsmithMaximiserAudioProcessor p;
                setParam (p, Max::thresholdId, -6.0f);
                setParam (p, Max::ceilingId, -1.0f);
                prepare (p, fs, 256);
                const auto in = sine ((int) fs, fs, 997.0, Max::dbToGain (-20.0f));
                const auto out = run (p, { in, in }, { 256 });
                const float gainDb = Max::gainToDb (peakOf (out[0], (size_t) fs / 2) / peakOf (in, (size_t) fs / 2));
                expectWithinAbsoluteError (gainDb, 5.0f, 0.01f, "constant +5 dB below threshold");
                expectWithinAbsoluteError (p.readAndReset (p.gainReductionDb), 0.0f, 1.0e-6f, "no gain reduction");
            }

            // Above threshold: 0 dBFS noise and a -3 dBFS tone peak at -1 dBFS, never above.
            for (auto& in : { noise ((int) fs, 1.0f, 11u), sine ((int) fs, fs, 1000.0, Max::dbToGain (-3.0f)) })
            {
                SongsmithMaximiserAudioProcessor p;
                setParam (p, Max::thresholdId, -6.0f);
                setParam (p, Max::ceilingId, -1.0f);
                setParam (p, Max::peakModeId, 0.0f); // this test checks the sample ceiling; true peak sits lower
                prepare (p, fs, 256);
                const auto out = run (p, { in, in }, { 256 });
                const float pk = peakOf (out[0]);
                expect (pk <= c * (1.0f + ceilingTolerance), "never above -1 dBFS: " + juce::String (Max::gainToDb (pk), 6));
                expect (pk >= c * 0.999f, "reaches the ceiling: " + juce::String (Max::gainToDb (pk), 6));
                expect (p.readAndReset (p.gainReductionDb) > 1.0f, "gain reduction reported");
            }
        }

        beginTest ("Impulses at every block offset hit the ceiling exactly on time");
        {
            constexpr double fs = 44100.0;
            constexpr int block = 64;
            const float c = Max::dbToGain (-1.0f);
            bool allBelow = true, allReach = true, allOnTime = true;

            for (int offset = 0; offset < block; ++offset)
            {
                Max::BrickwallLimiter lim;
                lim.setThresholdDb (-6.0f);
                lim.setCeilingDb (-1.0f);
                lim.setReleaseMs (1.0f);
                lim.prepare (fs);
                const int L = lim.getLatencySamples();

                const int at = 3 * block + offset;
                std::vector<float> in (2048, 0.0f);
                in[(size_t) at] = 1.0f;                     // +5 dB baseline -> 1.78, 6 dB over the ceiling
                const auto out = runLimiter (lim, { in, in }, block);

                allBelow = allBelow && peakOf (out[0]) <= c * (1.0f + ceilingTolerance);
                allReach = allReach && std::abs (out[0][(size_t) (at + L)]) >= c * 0.9999f;
                size_t argMax = 0;
                for (size_t i = 0; i < out[0].size(); ++i)
                    if (std::abs (out[0][i]) > std::abs (out[0][argMax])) argMax = i;
                allOnTime = allOnTime && (int) argMax == at + L;
            }

            expect (allBelow, "no impulse overshoots the ceiling");
            expect (allReach, "every impulse lands on the ceiling (not over-limited)");
            expect (allOnTime, "the peak sample is the delayed impulse");
        }

        beginTest ("Threshold / ceiling interaction below threshold");
        {
            constexpr double fs = 48000.0;
            const std::pair<float, float> settings[] = { { -6.0f, -1.0f }, { -10.0f, -3.0f }, { -3.0f, -0.1f }, { -20.0f, -20.0f }, { 0.0f, -6.0f } };
            for (auto [t, cdb] : settings)
            {
                Max::BrickwallLimiter lim;
                lim.setThresholdDb (t);
                lim.setCeilingDb (cdb);
                lim.prepare (fs);

                // A tone well below the threshold: the gain is exactly C - T.
                const float amp = Max::dbToGain (t - 6.0f);
                const auto in = sine ((int) fs / 4, fs, 440.0, amp);
                const auto out = runLimiter (lim, { in }, 128);
                const float gainDb = Max::gainToDb (peakOf (out[0], 4096) / peakOf (in, 4096));
                expectWithinAbsoluteError (gainDb, cdb - t, 0.01f, "T " + juce::String (t) + ", C " + juce::String (cdb));
            }
        }

        beginTest ("Stereo image: one shared gain");
        {
            constexpr double fs = 48000.0;
            Max::BrickwallLimiter lim;
            lim.setThresholdDb (-12.0f);
            lim.setCeilingDb (-1.0f);
            lim.prepare (fs);

            const auto l = noise ((int) fs, 1.0f, 21u);
            auto r = l;
            for (auto& s : r) s *= 0.5f;
            const auto out = runLimiter (lim, { l, r }, 100);

            float worst = 0.0f;
            for (size_t i = 0; i < l.size(); ++i)
                worst = std::max (worst, std::abs (out[1][i] - 0.5f * out[0][i]));
            expectLessOrEqual (worst, 1.0e-6f, "right stays exactly half of left");

            // The loud side drives the gain: a hard-left signal still limits.
            std::vector<float> silent (l.size(), 0.0f);
            lim.reset();
            const auto outL = runLimiter (lim, { l, silent }, 100);
            expect (peakOf (outL[0]) <= Max::dbToGain (-1.0f) * (1.0f + ceilingTolerance), "left limited");
            expectEquals (peakOf (outL[1]), 0.0f, "silent right stays silent");
        }

        beginTest ("Variable block sizes give identical output");
        {
            constexpr double fs = 48000.0;
            const auto in = noise ((int) fs, 1.0f, 31u);
            std::vector<std::vector<float>> results;

            for (auto sizes : { std::vector<int> { 512 }, std::vector<int> { 1 }, std::vector<int> { 37, 1, 1000, 64, 3 }, std::vector<int> { 4096 } })
            {
                SongsmithMaximiserAudioProcessor p;
                setParam (p, Max::thresholdId, -9.0f);
                setParam (p, Max::ceilingId, -0.5f);
                setParam (p, Max::releaseId, 20.0f);
                prepare (p, fs, 512);
                results.push_back (run (p, { in, in }, sizes)[0]);
            }

            for (size_t k = 1; k < results.size(); ++k)
                expectEquals (maxAbsDiff (results[0], results[k]), 0.0f, "block pattern " + juce::String ((int) k) + " is bit-identical");
        }

        beginTest ("Automation never breaks the ceiling");
        {
            constexpr double fs = 48000.0;
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::thresholdId, 0.0f);   // the first block's values, so nothing glides from the defaults
            setParam (p, Max::ceilingId, -6.0f);
            prepare (p, fs, 64);
            const auto in = noise ((int) fs * 2, 1.0f, 41u);

            const auto out = run (p, { in, in }, { 64 }, [&] (int bi, int)
            {
                // Threshold sweeps -30..0, ceiling -6..-1, release 0.01..1000 ms.
                const float t = (float) (bi % 200) / 199.0f;
                setParam (p, Max::thresholdId, -30.0f * t);
                setParam (p, Max::ceilingId, -1.0f - 5.0f * (1.0f - t));
                p.apvts.getParameter (Max::releaseId)->setValueNotifyingHost (t);
            });

            bool finite = true;
            for (float s : out[0]) finite = finite && std::isfinite (s);
            expect (finite, "finite output");
            expect (peakOf (out[0]) <= Max::dbToGain (-1.0f) * (1.0f + ceilingTolerance),
                    "never above the highest ceiling used: " + juce::String (Max::gainToDb (peakOf (out[0])), 6));

            // Settle at a -6 dB ceiling: then nothing above -6 dBFS.
            setParam (p, Max::thresholdId, -20.0f);
            setParam (p, Max::ceilingId, -6.0f);
            const auto settled = run (p, { in, in }, { 64 });
            expect (peakOf (settled[0], 4800) <= Max::dbToGain (-6.0f) * (1.0f + ceilingTolerance), "settled at the new ceiling");
        }

        beginTest ("Release law: exponential recovery with time constant = Release");
        {
            constexpr double fs = 48000.0;
            for (float releaseMs : { 5.0f, 50.0f, 300.0f })
            {
                Max::BrickwallLimiter lim;
                lim.setThresholdDb (-12.0f);
                lim.setCeilingDb (0.0f);
                lim.setReleaseMs (releaseMs);
                lim.prepare (fs);
                const int L = lim.getLatencySamples();

                // A 50 ms burst 6 dB over the ceiling, then a quiet probe tone (-30 dBFS + 12 dB).
                const int n = (int) fs;
                auto in = std::vector<float> ((size_t) n, Max::dbToGain (-30.0f));   // DC probe: easy to read the gain
                const int burstEnd = 4800;
                for (int i = 2400; i < burstEnd; ++i)
                    in[(size_t) i] = (i % 2 == 0 ? 1.0f : -1.0f) * Max::dbToGain (-6.0f);   // +12 dB -> 6 dB over
                const auto out = runLimiter (lim, { in }, 256);

                // Limiter gain one Release time after the burst leaves the look-ahead window:
                // expect 1 - (1 - g0) / e, with g0 = -6 dB (box-car smoothing adds ~L/2 samples).
                const int at = burstEnd + L + L / 2 + (int) std::lround (releaseMs * 0.001 * fs);
                const float measured = out[0][(size_t) at] / (Max::dbToGain (-30.0f) * Max::dbToGain (12.0f));
                const float g0 = Max::dbToGain (-6.0f);
                const float expected = 1.0f - (1.0f - g0) * std::exp (-1.0f);
                expectWithinAbsoluteError (measured, expected, 0.03f, "recovery after " + juce::String (releaseMs) + " ms");
            }
        }

        beginTest ("Reset clears the look-ahead and the envelope");
        {
            constexpr double fs = 48000.0;
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::thresholdId, -20.0f);
            setParam (p, Max::releaseId, 1000.0f);
            prepare (p, fs, 256);
            run (p, { noise (4800, 1.0f, 51u), noise (4800, 1.0f, 52u) }, { 256 });

            p.reset();
            expectEquals (p.getLimiter().getCurrentGain(), 1.0f, "gain back to unity");
            const auto out = run (p, { std::vector<float> (1024, 0.0f), std::vector<float> (1024, 0.0f) }, { 256 });
            expectEquals (peakOf (out[0]), 0.0f, "no stale audio comes out of the delay");
            expectEquals (p.getLatencySamples(), 72 + Max::TruePeakDetector::delaySamples, "latency unchanged");
        }

        beginTest ("Bypass is time-aligned with the processed path");
        {
            constexpr double fs = 48000.0;
            SongsmithMaximiserAudioProcessor p;
            prepare (p, fs, 128);
            setParam (p, Max::bypassId, 1.0f);
            const auto in = noise (9600, 0.5f, 61u);
            const auto out = run (p, { in, in }, { 128 });
            expectEquals (maxAbsDiff (out[0], in, p.getLatencySamples()), 0.0f, "bypassed = input delayed by the latency, bit-exact");
        }
    }
};

static IdentityTests identityTests;
static ShellProcessingTests shellProcessingTests;
static AutomationAndStateTests automationAndStateTests;
static LimiterTests limiterTests;

//==============================================================================
namespace
{
    // True peak of a signal measured by the independent meter (8x, 32 taps/phase), in dBTP.
    // The meter sees the whole signal (so its filter is settled), but only peaks from 'from' on
    // count: starting it mid-signal would add the filter's own Gibbs ringing (~13 % on a step).
    float truePeakDb (const std::vector<std::vector<float>>& sig, size_t from = 0)
    {
        Max::TruePeakMeter meter;
        const float* head[2] = {};
        const float* tail[2] = {};
        for (size_t c = 0; c < sig.size(); ++c)
        {
            head[c] = sig[c].data();
            tail[c] = sig[c].data() + from;
        }
        meter.process (head, (int) sig.size(), (int) from);
        meter.readAndReset();
        meter.process (tail, (int) sig.size(), (int) (sig[0].size() - from));
        return Max::gainToDb (meter.getPeak());
    }

    // Total harmonic distortion of a steady tone of 'freq' (an integer number of periods in
    // [from, to)): sqrt (sum of harmonics 2..20 squared) / fundamental.
    double thd (const std::vector<float>& v, double fs, double freq, size_t from, size_t to)
    {
        auto amplitude = [&] (double f)
        {
            double re = 0.0, im = 0.0;
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            for (size_t i = from; i < to; ++i)
            {
                re += (double) v[i] * std::cos (w * (double) i);
                im += (double) v[i] * std::sin (w * (double) i);
            }
            return std::sqrt (re * re + im * im);
        };

        const double fundamental = amplitude (freq);
        double harmonics = 0.0;
        for (int k = 2; k <= 20 && freq * k < fs * 0.5; ++k)
            harmonics += std::pow (amplitude (freq * k), 2.0);
        return std::sqrt (harmonics) / fundamental;
    }

    // Runs the limiter one sample at a time and returns its gain (dB) for every sample.
    std::vector<float> gainTrace (Max::BrickwallLimiter& lim, std::vector<float> in)
    {
        std::vector<float> g (in.size());
        for (size_t i = 0; i < in.size(); ++i)
        {
            float* ch[1] = { in.data() + i };
            lim.process (ch, 1, 1);
            g[i] = Max::gainToDb (lim.getCurrentGain());
        }
        return g;
    }
}

class TruePeakAndAutoReleaseTests final : public juce::UnitTest
{
public:
    TruePeakAndAutoReleaseTests() : juce::UnitTest ("True peak and auto release", "Maximiser") {}

    // Measured overshoot allowance for true-peak mode, from the independent 8x meter.
    static constexpr float truePeakToleranceDb = 0.1f, whiteNoiseToleranceDb = 0.35f;

    void runTest() override
    {
        constexpr double fs = 44100.0;
        const double pi = juce::MathConstants<double>::pi;

        beginTest ("Reconstruction meter and detector read intersample peaks");
        {
            // fs/4 at 45 degrees: every sample is +-0.707, the waveform peaks at 1.0 (+3 dB).
            const auto tone = sine (8192, fs, fs / 4.0, 1.0, pi / 4.0);
            expectWithinAbsoluteError (Max::gainToDb (peakOf (tone)), -3.01f, 0.01f, "sample peak -3 dB");
            expectWithinAbsoluteError (truePeakDb ({ tone }, 1024), 0.0f, 0.02f, "meter reads the 0 dBTP peak");

            Max::TruePeakDetector det;
            float tpMax = 0.0f, sp = 0.0f;
            for (size_t i = 0; i < tone.size(); ++i)
            {
                const float x[1] = { tone[i] };
                const float tp = det.push (x, 1, sp);
                if (i > 64) tpMax = std::max (tpMax, tp);
            }
            expectWithinAbsoluteError (Max::gainToDb (tpMax), 0.0f, 0.15f, "detector estimate");

            const auto lf = sine (8192, fs, 1000.0, 0.5);
            expectWithinAbsoluteError (truePeakDb ({ lf }, 1024), Max::gainToDb (0.5f), 0.01f, "1 kHz reads its amplitude");
        }

        beginTest ("Same latency in both modes; quiet audio passes unchanged in true-peak mode");
        {
            SongsmithMaximiserAudioProcessor p;
            expect (p.isTruePeak(), "a fresh instance starts in true peak mode");
            setParam (p, Max::peakModeId, 0.0f);
            expect (! p.isTruePeak());
            prepare (p, fs, 256);
            const int sampleLatency = p.getLatencySamples();
            setParam (p, Max::peakModeId, 1.0f);
            expect (p.isTruePeak());
            prepare (p, fs, 256);
            expectEquals (p.getLatencySamples(), sampleLatency, "no latency change when switching mode");

            const auto in = noise (8192, 0.3f, 71u);
            const auto out = run (p, { in, in }, { 256 });
            expectEquals (maxAbsDiff (out[0], in, p.getLatencySamples()), 0.0f, "transparent below the ceiling");
        }

        beginTest ("Phase-shifted high-frequency tones: true-peak mode holds the ceiling");
        {
            float worstTp = -100.0f, worstSp = -100.0f;

            for (double freq : { 5000.0, 11025.0, 15000.0, 19000.0 })
                for (double phase : { 0.0, pi / 8.0, pi / 4.0, pi / 3.0 })
                {
                    const auto in = sine ((int) fs / 2, fs, freq, Max::dbToGain (-2.0f), phase);   // T -6 -> 4 dB over

                    for (bool tpMode : { true, false })
                    {
                        Max::BrickwallLimiter lim;
                        lim.setThresholdDb (-6.0f);
                        lim.setCeilingDb (-1.0f);
                        lim.setTruePeak (tpMode);
                        lim.prepare (fs);
                        const auto out = runLimiter (lim, { in, in }, 128);
                        const float overDb = truePeakDb (out, 2048) + 1.0f;   // overshoot above -1 dBTP
                        (tpMode ? worstTp : worstSp) = std::max (tpMode ? worstTp : worstSp, overDb);
                    }
                }

            logMessage ("    worst true-peak overshoot above the ceiling: true-peak mode " + juce::String (worstTp, 3)
                        + " dB, sample-peak mode " + juce::String (worstSp, 3) + " dB");
            expectLessOrEqual (worstTp, truePeakToleranceDb, "true-peak mode overshoot within tolerance");
            expectGreaterThan (worstSp, 1.0f, "sample-peak mode lets intersample peaks through (reference)");
        }

        beginTest ("Intersample peaks in noise and transient bursts");
        {
            juce::String report;
            float worst = -100.0f;

            // High-frequency bursts and +-1 doublet clicks (large reconstruction overshoot).
            std::vector<std::vector<float>> signals;

            auto bursts = std::vector<float> ((size_t) fs, 0.0f);
            for (size_t i = 0; i < bursts.size(); ++i)
                if ((i / 2205) % 4 == 0)   // 50 ms bursts every 200 ms
                    bursts[i] = (float) std::sin (2.0 * pi * 17000.0 * (double) i / fs + 0.7) * (i % 3 == 0 ? 1.0f : 0.8f);
            signals.push_back (bursts);

            auto clicks = std::vector<float> ((size_t) fs, 0.0f);
            for (size_t i = 1000; i + 1 < clicks.size(); i += 3001)
            {
                clicks[i] = 1.0f;
                clicks[i + 1] = -1.0f;
            }
            signals.push_back (clicks);

            // Full-band white noise at full scale: a worst case (real material has far less energy
            // near Nyquist), so it gets its own, looser tolerance.
            signals.push_back (noise ((int) fs, 1.0f, 81u));

            for (size_t s = 0; s < signals.size(); ++s)
            {
                Max::BrickwallLimiter lim;
                lim.setThresholdDb (-6.0f);
                lim.setCeilingDb (-1.0f);
                lim.setTruePeak (true);
                lim.prepare (fs);
                const auto out = runLimiter (lim, { signals[s], signals[s] }, 64);
                const float over = truePeakDb (out, 0) + 1.0f;
                report << juce::String (over, 3) << " dB  ";
                if (s + 1 < signals.size())
                    worst = std::max (worst, over);
                else
                    expectLessOrEqual (over, whiteNoiseToleranceDb, "full-band white noise within its tolerance");
            }

            logMessage ("    true-peak overshoot (HF bursts, clicks, white noise): " + report);
            expectLessOrEqual (worst, truePeakToleranceDb, "bursts and clicks within tolerance");
        }

        beginTest ("Sample-peak mode is unchanged by the detector");
        {
            Max::BrickwallLimiter lim;
            lim.setThresholdDb (-6.0f);
            lim.setCeilingDb (-1.0f);
            lim.prepare (fs);
            const auto out = runLimiter (lim, { noise ((int) fs, 1.0f, 91u) }, 64);
            expect (peakOf (out[0]) <= Max::dbToGain (-1.0f) * (1.0f + ceilingTolerance), "sample peak still exactly at or under the ceiling");
        }

        beginTest ("Auto release vs manual: distortion, pumping, recovery, CPU");
        {
            constexpr double rate = 48000.0;
            auto makeLimiter = [&] (bool autoRel, float releaseMs)
            {
                auto lim = std::make_unique<Max::BrickwallLimiter>();
                lim->setThresholdDb (-8.0f);
                lim->setCeilingDb (-1.0f);
                lim->setReleaseMs (releaseMs);
                lim->setAutoRelease (autoRel);
                lim->prepare (rate);
                return lim;
            };

            // Distortion: THD of a 50 Hz tone limited by ~7 dB.
            const auto bass = sine ((int) rate, rate, 50.0, 1.0);
            auto distortion = [&] (bool autoRel, float releaseMs)
            {
                const size_t from = (size_t) rate / 2, to = from + 960 * 20;   // 20 whole periods
                return thd (runLimiter (*makeLimiter (autoRel, releaseMs), { bass }, 256)[0], rate, 50.0, from, to);
            };

            // Pumping: a sustained 60 Hz bass that needs no limiting (-12 dBFS + 7 dB) with a full-scale
            // 12 ms kick every 500 ms. Pumping = mean gain reduction on the bass between kicks (dB).
            auto mix = sine ((int) rate * 2, rate, 60.0, Max::dbToGain (-12.0f));
            const size_t kickEvery = (size_t) rate / 2, kickLength = 576;
            for (size_t k = 0; k < mix.size(); k += kickEvery)
                for (size_t i = k; i < std::min (mix.size(), k + kickLength); ++i)
                    mix[i] += (float) std::sin (2.0 * pi * 100.0 * (double) (i - k) / rate) * 0.9f;
            auto pumping = [&] (bool autoRel, float releaseMs)
            {
                auto lim = makeLimiter (autoRel, releaseMs);
                const auto g = gainTrace (*lim, mix);
                double sum = 0.0;
                int n = 0;
                for (size_t k = kickEvery; k < mix.size(); k += kickEvery)   // skip the first kick
                    for (size_t i = k + kickLength + 2400; i < std::min (mix.size(), k + kickEvery); ++i)   // 50 ms after the kick
                    {
                        sum -= (double) g[i];
                        ++n;
                    }
                return sum / juce::jmax (1, n);
            };

            // Recovery: an isolated 5 ms spike 10 dB over on a quiet tone; ms until the gain is
            // back within 1 dB of unity.
            auto spike = sine ((int) rate, rate, 440.0, Max::dbToGain (-30.0f));
            for (size_t i = 4800; i < 5040; ++i)
                spike[i] = (i % 2 == 0 ? 1.0f : -1.0f) * Max::dbToGain (1.0f);
            auto recovery = [&] (bool autoRel, float releaseMs)
            {
                auto lim = makeLimiter (autoRel, releaseMs);
                const auto g = gainTrace (*lim, spike);
                size_t last = 5040;
                for (size_t i = 0; i < g.size(); ++i)
                    if (g[i] < -1.0f) last = i;
                return (double) (last - 5040) * 1000.0 / rate;
            };

            // CPU: ns per stereo sample.
            auto bench = [&] (bool autoRel, bool tp)
            {
                auto lim = makeLimiter (autoRel, 1.0f);
                lim->setTruePeak (tp);
                std::vector<std::vector<float>> buf { noise ((int) rate * 4, 1.0f, 5u), noise ((int) rate * 4, 1.0f, 6u) };
                const auto t0 = juce::Time::getHighResolutionTicks();
                runLimiter (*lim, buf, 512);
                const auto t1 = juce::Time::getHighResolutionTicks();
                return juce::Time::highResolutionTicksToSeconds (t1 - t0) * 1.0e9 / (rate * 4);
            };

            struct Row { const char* name; bool autoRel; float releaseMs; double thd = 0, pump = 0, rec = 0, ns = 0; };
            Row rows[] = { { "manual 1 ms  ", false, 1.0f }, { "manual 150 ms", false, 150.0f }, { "auto (1 ms)  ", true, 1.0f } };
            logMessage ("    mode            THD 50 Hz   pumping    recovery   ns/sample (sample peak)");
            for (auto& r : rows)
            {
                r.thd = distortion (r.autoRel, r.releaseMs);
                r.pump = pumping (r.autoRel, r.releaseMs);
                r.rec = recovery (r.autoRel, r.releaseMs);
                r.ns = bench (r.autoRel, false);
                logMessage (juce::String ("    ") + r.name + "   " + juce::String (r.thd * 100.0, 2) + " %      "
                            + juce::String (r.pump, 2) + " dB    " + juce::String (r.rec, 1) + " ms    " + juce::String (r.ns, 1));
            }
            logMessage ("    true peak (manual): " + juce::String (bench (false, true), 1) + " ns/sample, true peak + auto: "
                        + juce::String (bench (true, true), 1) + " ns/sample");

            const auto& fast = rows[0];
            const auto& slow = rows[1];
            const auto& autoRow = rows[2];
            expectLessThan (autoRow.thd, fast.thd * 0.5, "auto distorts sustained bass far less than a fast manual release");
            expectLessThan (autoRow.pump, slow.pump * 0.5, "auto pumps far less than a slow manual release");
            expectLessThan (autoRow.rec, slow.rec * 0.5, "auto recovers from an isolated peak far faster than a slow release");
        }


        beginTest ("Version-1 sessions load as sample peak, manual release");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::peakModeId, 1.0f);
            setParam (p, Max::autoReleaseId, 1.0f);

            juce::ValueTree v1 ("PARAMETERS");
            v1.setProperty ("version", 1, nullptr);
            v1.appendChild (juce::ValueTree ("PARAM", { { "id", "threshold" }, { "value", -4.0f } }), nullptr);
            p.restoreFromValueTree (v1);
            expect (! p.isTruePeak() && ! p.isAutoRelease(), "version 1 has no peak mode: the migration sets sample peak, and release stays manual");
            expectWithinAbsoluteError (p.getThresholdDb(), -4.0f, 1.0e-4f);

            setParam (p, Max::peakModeId, 1.0f);
            setParam (p, Max::autoReleaseId, 1.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithMaximiserAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.isTruePeak() && q.isAutoRelease(), "round trip");
        }

        beginTest ("Fresh instance is true peak and holds the ceiling on intersample peaks; version 3 sample peak is kept");
        {
            SongsmithMaximiserAudioProcessor p;
            expect (p.isTruePeak(), "default is true peak");
            prepare (p, fs, 256);
            setParam (p, Max::thresholdId, -6.0f);
            setParam (p, Max::ceilingId, -1.0f);
            prepare (p, fs, 256);
            // Full-scale tone at fs/4 with a 45 degree phase: samples reach only -3 dBFS, true peak is 0 dBFS.
            const auto in = sine ((int) fs / 2, fs, fs / 4.0, 1.0, pi / 4.0);
            const auto out = run (p, { in, in }, { 256 });
            expectLessOrEqual (truePeakDb (out, 2048), -1.0f + truePeakToleranceDb, "true-peak meter stays at the ceiling");

            setParam (p, Max::peakModeId, 0.0f);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithMaximiserAudioProcessor q;
            expect (q.isTruePeak());
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (! q.isTruePeak(), "a saved sample-peak state loads as sample peak");
        }
    }
};

static TruePeakAndAutoReleaseTests truePeakAndAutoReleaseTests;

//==============================================================================
class DitherTests final : public juce::UnitTest
{
public:
    DitherTests() : juce::UnitTest ("Dither and quantiser", "Maximiser") {}

    static double lsb (int bits) { return std::ldexp (1.0, 1 - bits); }

    void runTest() override
    {
        constexpr double fs = 48000.0;

        beginTest ("Off by default; LSB and headroom");
        {
            SongsmithMaximiserAudioProcessor p;
            expect (p.getDitherDepth() == Max::Dither::Depth::off, "dither off by default");
            expectEquals (Max::Dither::lsbFor (Max::Dither::Depth::bits16), 1.0 / 32768.0);
            expectEquals (Max::Dither::lsbFor (Max::Dither::Depth::bits24), 1.0 / 8388608.0);
            expectEquals (Max::Dither::headroomFor (Max::Dither::Depth::off), 0.0f);
            expectWithinAbsoluteError ((double) Max::Dither::headroomFor (Max::Dither::Depth::bits16), 1.5 / 32768.0, 1.0e-9);
        }

        beginTest ("Ceiling holds after quantisation, output on the 16/24-bit grid");
        {
            for (int depth : { 1, 2 })
                for (int mode : { 0, 1 })
                {
                    const int bits = depth == 1 ? 16 : 24;
                    SongsmithMaximiserAudioProcessor p;
                    setParam (p, Max::thresholdId, -12.0f);
                    setParam (p, Max::ceilingId, -0.1f);
                    setParam (p, Max::ditherId, (float) depth);
                    setParam (p, Max::peakModeId, (float) mode);
                    prepare (p, fs, 256);

                    const auto out = run (p, { noise ((int) fs, 1.0f, 111u), noise ((int) fs, 1.0f, 112u) }, { 256 });
                    const float c = Max::dbToGain (-0.1f);
                    bool onGrid = true;
                    float pk = 0.0f;
                    for (auto& ch : out)
                        for (float s : ch)
                        {
                            const double codes = (double) s / lsb (bits);
                            onGrid = onGrid && std::abs (codes - std::round (codes)) < 1.0e-6;
                            pk = std::max (pk, std::abs (s));
                        }

                    const auto what = juce::String (bits) + "-bit, " + Max::getPeakModeNames()[mode];
                    expect (onGrid, what + ": every sample is a whole " + juce::String (bits) + "-bit code");
                    expect (pk <= c, what + ": quantised peak " + juce::String (Max::gainToDb (pk), 5) + " dB <= ceiling");
                    if (mode == 0)   // in true-peak mode the samples sit below the ceiling by design
                        expect (pk >= c - 4.0f * (float) lsb (bits) - 1.0e-4f, what + ": still reaches the ceiling (headroom is tiny)");
                }
        }

        beginTest ("TPDF noise: level, flat spectrum, uncorrelated channels");
        {
            Max::Dither d;
            d.setDepth (Max::Dither::Depth::bits16);
            d.reset();
            constexpr int n = 1 << 17;
            std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
            float* ch[2] = { l.data(), r.data() };
            d.process (ch, 2, n);

            const double q = lsb (16);
            double mean = 0.0, var = 0.0, cross = 0.0, lag1 = 0.0;
            for (size_t i = 0; i < (size_t) n; ++i) mean += (double) l[i];
            mean /= n;
            for (size_t i = 0; i < (size_t) n; ++i)
            {
                var += ((double) l[i] - mean) * ((double) l[i] - mean);
                cross += (double) l[i] * (double) r[i];
                if (i > 0) lag1 += (double) l[i] * (double) l[i - 1];
            }
            var /= n;
            expectWithinAbsoluteError (mean / q, 0.0, 0.01, "zero mean");
            expectWithinAbsoluteError (var / (q * q), 0.25, 0.01, "noise power q^2/4 (TPDF q^2/6 + rounding q^2/12)");
            expectLessThan (std::abs (cross / n) / var, 0.02, "left and right noise uncorrelated");
            expectLessThan (std::abs (lag1 / n) / var, 0.02, "white: no sample-to-sample correlation");

            // Spectrum: average 1024-point power spectra; 8 equal bands within 0.5 dB of each other.
            constexpr int order = 10, size = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<double> power ((size_t) size / 2, 0.0);
            std::vector<float> frame ((size_t) size * 2);
            for (int f = 0; f + size <= n; f += size)
            {
                std::fill (frame.begin(), frame.end(), 0.0f);
                std::copy (l.begin() + f, l.begin() + f + size, frame.begin());
                fft.performFrequencyOnlyForwardTransform (frame.data());
                for (size_t k = 0; k < power.size(); ++k)
                    power[k] += (double) frame[k] * (double) frame[k];
            }
            double minBand = 1.0e30, maxBand = 0.0;
            for (int b = 0; b < 8; ++b)
            {
                double sum = 0.0;
                for (size_t k = (size_t) (b * 64 + 1); k < (size_t) (b * 64 + 64); ++k)
                    sum += power[k];
                minBand = std::min (minBand, sum);
                maxBand = std::max (maxBand, sum);
            }
            const double spreadDb = 10.0 * std::log10 (maxBand / minBand);
            logMessage ("    dither spectrum: 8-band spread " + juce::String (spreadDb, 2) + " dB");
            expectLessThan (spreadDb, 0.5, "flat (white) noise spectrum");
        }

        beginTest ("Bypass skips dither; state recall; older sessions load with dither off");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::ditherId, 1.0f);
            setParam (p, Max::bypassId, 1.0f);
            prepare (p, fs, 128);
            const auto in = noise (9600, 0.5f, 121u);
            const auto out = run (p, { in, in }, { 128 });
            expectEquals (maxAbsDiff (out[0], in, p.getLatencySamples()), 0.0f, "bypassed: the undithered input");

            juce::MemoryBlock mb;
            setParam (p, Max::ditherId, 2.0f);
            p.getStateInformation (mb);
            SongsmithMaximiserAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.getDitherDepth() == Max::Dither::Depth::bits24, "24-bit restored");

            juce::ValueTree v2 ("PARAMETERS");
            v2.setProperty ("version", 2, nullptr);
            v2.appendChild (juce::ValueTree ("PARAM", { { "id", "peak_mode" }, { "value", 1.0f } }), nullptr);
            q.restoreFromValueTree (v2);
            expect (q.getDitherDepth() == Max::Dither::Depth::off && q.isTruePeak(), "version-2 session: dither off");
        }
    }
};

//==============================================================================
class EditorTests final : public juce::UnitTest
{
public:
    EditorTests() : juce::UnitTest ("Editor, link and meters", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Plain words: every text, parameter name and starting point, and every label fits");
        for (int width : { Max::EditorLayout::defaultWidth, Max::EditorLayout::minWidth })
        {
            SongsmithMaximiserAudioProcessor p;
            p.editorWidth = width;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            const auto context = "Maximiser at " + juce::String (width) + " px";

            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*editor, texts);
            for (auto* prm : p.getParameters())
            {
                texts.add (prm->getName (100));
                texts.add (prm->getCurrentValueAsText());
            }
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& sp : Max::getStartingPoints())
            {
                texts.add (sp.name);
                texts.add (sp.hint);
            }
            expectGreaterThan (texts.size(), 30, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);

            for (auto* t : { "Input", "Output", "Gain Reduction", "Output Ceiling" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");

            Songsmith::UI::Wording::expectTextFits (*this, *editor, editor->getLookAndFeel(), context);
        }

        beginTest ("Every button, slider and combo box uses the shared bevelled look");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *editor, "editor");
        }

        beginTest ("Editor text uses the shared typeface and type scale");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            Songsmith::UI::Testing::checkEditorFonts (*this, *editor, "Maximiser editor");
        }

        beginTest ("Linked move keeps C - T within the common range");
        {
            auto m = Max::moveLinked (-6.0f, -1.0f, -12.0f, true);
            expectWithinAbsoluteError (m.threshold, -12.0f, 1.0e-5f);
            expectWithinAbsoluteError (m.ceiling, -7.0f, 1.0e-5f, "ceiling follows");

            m = Max::moveLinked (-6.0f, -1.0f, 3.0f, true);   // the ceiling hits 0 first
            expectWithinAbsoluteError (m.ceiling, 0.0f, 1.0e-5f);
            expectWithinAbsoluteError (m.threshold, -5.0f, 1.0e-5f, "threshold stops where the ceiling ran out of range");

            m = Max::moveLinked (-5.0f, -10.0f, -30.0f, true);   // C below T: the ceiling hits -30 first
            expectWithinAbsoluteError (m.ceiling, -30.0f, 1.0e-5f);
            expectWithinAbsoluteError (m.threshold, -25.0f, 1.0e-5f);

            m = Max::moveLinked (-20.0f, -2.0f, -4.0f, false);   // moving the ceiling
            expectWithinAbsoluteError (m.threshold, -22.0f, 1.0e-5f);
            expectWithinAbsoluteError (m.ceiling, -4.0f, 1.0e-5f);
        }

        beginTest ("Meter ballistics (display only)");
        {
            Max::MeterBallistics a, b;
            a.update (0.0f, 0.0);
            b.update (0.0f, 0.0);
            expectEquals (a.getLevel(), 0.0f, "instant rise");
            for (int i = 0; i < 10; ++i)
                a.update (-100.0f, 0.1);
            b.update (-100.0f, 1.0);
            expectWithinAbsoluteError (a.getLevel(), -20.0f, 1.0e-3f, "falls 20 dB/s");
            expectWithinAbsoluteError (a.getLevel(), b.getLevel(), 1.0e-3f, "independent of the update rate");
            expectEquals (a.getHold(), 0.0f, "hold still up after 1 s");
            a.update (-100.0f, 0.6);
            expect (a.getHold() < -20.0f, "hold released after 1.5 s");
        }

        beginTest ("Editor opens, scales and recalls its size");
        {
            SongsmithMaximiserAudioProcessor p;
            std::unique_ptr<SongsmithMaximiserAudioProcessorEditor> ed (dynamic_cast<SongsmithMaximiserAudioProcessorEditor*> (p.createEditor()));
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), Max::EditorLayout::designWidth);
            expectEquals (ed->getHeight(), Max::EditorLayout::designHeight);

            ed->setSize (1080, Max::EditorLayout::heightFor (1080));
            expectWithinAbsoluteError (ed->getContentScale(), 1.5f, 1.0e-4f, "content scales with the window");
            expectEquals (p.editorWidth.load(), 1080);

            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithMaximiserAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (q.editorWidth.load(), 1080, "size saved with the session");
            std::unique_ptr<juce::AudioProcessorEditor> ed2 (q.createEditor());
            expectEquals (ed2->getWidth(), 1080, "reopens at that size");

            // Every control fits inside the design area.
            bool inside = true;
            for (auto* child : ed->getContent().getChildren())
                inside = inside && ed->getContent().getLocalBounds().contains (child->getBounds());
            expect (inside, "all controls inside the content area");
        }

        beginTest ("MAX-09: Integrated LUFS sits right of the Release dial, alone, at every size");
        for (int width : { Max::EditorLayout::minWidth, Max::EditorLayout::defaultWidth, Max::EditorLayout::maxWidth })
        {
            SongsmithMaximiserAudioProcessor p;
            p.editorWidth = width;
            std::unique_ptr<SongsmithMaximiserAudioProcessorEditor> ed (dynamic_cast<SongsmithMaximiserAudioProcessorEditor*> (p.createEditor()));
            const auto context = "at " + juce::String (width) + " px";
            auto& content = ed->getContent();

            const auto dial = ed->getReleaseDialBounds();
            const auto value = ed->getIntegratedLabel().getBounds();
            const auto unit = ed->getIntegratedUnit().getBounds();
            const auto reset = ed->getLoudnessResetButton().getBounds();
            expect (value.getX() >= dial.getRight(), context + ": the reading is right of the dial");
            expect (value.getY() < dial.getBottom() && value.getBottom() > dial.getY(), context + ": level with the dial, not below it");
            for (auto r : { value, unit, reset, ed->getIntegratedCaption().getBounds() })
                expect (content.getLocalBounds().contains (r) && r.getX() >= Max::EditorLayout::controlsWidth,
                        context + ": inside the loudness column");
            expect (! value.intersects (unit) && ! value.intersects (reset), context + ": no overlap");
            expect (ed->getBypassButton().getBottom() < ed->getIntegratedCaption().getY(), context + ": clear of the title bar");

            // Only Integrated is shown, in LUFS.
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto& t : texts)
                expect (! t.containsIgnoreCase ("Short Term") && ! t.containsIgnoreCase ("Momentary"), context + ": no extra readings: " + t);
            expectEquals (ed->getIntegratedUnit().getText(), juce::String ("LUFS"));

            // The tooltip covers the meaning, the whole song, reset and a qualified reference.
            const auto tip = ed->getIntegratedLabel().getTooltip();
            for (auto* word : { "average perceived loudness", "whole song", "reset", "14 LUFS", "not a rule", "judge by ear" })
                expect (tip.containsIgnoreCase (word), context + ": tooltip mentions " + juce::String (word));
        }

        beginTest ("Link follows the user, not host automation");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::thresholdId, -6.0f);
            setParam (p, Max::ceilingId, -1.0f);
            SongsmithMaximiserAudioProcessorEditor ed (p);

            ed.getLinkButton().setToggleState (true, juce::sendNotificationSync);
            expect (p.linkThresholdCeiling.load(), "link stored in the processor");

            ed.getThresholdSlider().setValue (-10.0, juce::sendNotificationSync);   // the user moves Threshold
            expectWithinAbsoluteError (p.getThresholdDb(), -10.0f, 1.0e-3f);
            expectWithinAbsoluteError (p.getCeilingDb(), -5.0f, 1.0e-3f, "ceiling followed: C - T still +5 dB");

            ed.getCeilingSlider().setValue (0.0, juce::sendNotificationSync);   // ceiling up 5 dB
            expectWithinAbsoluteError (p.getThresholdDb(), -5.0f, 1.0e-3f, "threshold followed the ceiling");

            ed.getThresholdSlider().setValue (2.0, juce::sendNotificationSync);   // beyond the range
            expectWithinAbsoluteError (p.getCeilingDb() - p.getThresholdDb(), 5.0f, 1.0e-3f, "C - T kept at the range end");

            // Host automation of one parameter never drags the other along.
            setParam (p, Max::thresholdId, -20.0f);
            ed.syncFromParameters();
            expectWithinAbsoluteError ((float) ed.getThresholdSlider().getValue(), -20.0f, 1.0e-3f, "fader follows the host");
            expectWithinAbsoluteError (p.getCeilingDb(), 0.0f, 1.0e-3f, "ceiling untouched by automation");

            // Unlinked: independent.
            ed.getLinkButton().setToggleState (false, juce::sendNotificationSync);
            ed.getThresholdSlider().setValue (-8.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (p.getCeilingDb(), 0.0f, 1.0e-3f, "unlinked: ceiling stays");

            // Link is part of the session.
            ed.getLinkButton().setToggleState (true, juce::sendNotificationSync);
            juce::MemoryBlock mb;
            p.getStateInformation (mb);
            SongsmithMaximiserAudioProcessor q;
            q.setStateInformation (mb.getData(), (int) mb.getSize());
            expect (q.linkThresholdCeiling.load(), "link restored");
        }

        beginTest ("Meters: the editor reads the processor; the audio doesn't depend on the editor");
        {
            constexpr double fs = 48000.0;
            const auto in = noise (24000, 1.0f, 131u);
            std::vector<std::vector<float>> outs;
            for (bool withEditor : { false, true })
            {
                SongsmithMaximiserAudioProcessor p;
                setParam (p, Max::thresholdId, -10.0f);
                prepare (p, fs, 256);
                std::unique_ptr<SongsmithMaximiserAudioProcessorEditor> ed;
                if (withEditor)
                    ed.reset (dynamic_cast<SongsmithMaximiserAudioProcessorEditor*> (p.createEditor()));
                outs.push_back (run (p, { in, in }, { 256 }, [&] (int, int) { if (ed != nullptr) ed->updateMeters (256.0 / fs); })[0]);
                if (ed != nullptr)
                {
                    ed->updateMeters (0.01);
                    expect (ed->getReductionMeter().getBallistics().getLevel() > 3.0f, "attenuation meter shows the gain reduction");
                }
            }
            expectEquals (maxAbsDiff (outs[0], outs[1]), 0.0f, "identical audio with and without the editor");
        }

        beginTest ("Combo boxes follow the parameters");
        {
            SongsmithMaximiserAudioProcessor p;
            SongsmithMaximiserAudioProcessorEditor ed (p);
            setParam (p, Max::peakModeId, 1.0f);
            setParam (p, Max::ditherId, 1.0f);
            expectEquals (ed.getPeakModeBox().getText(), juce::String ("True peak"));
            expectEquals (ed.getDitherBox().getText(), juce::String ("16-bit"));
        }
    }
};

static DitherTests ditherTests;
static EditorTests editorTests;

//==============================================================================
class StartingPointTests final : public juce::UnitTest
{
public:
    StartingPointTests() : juce::UnitTest ("Starting points", "Maximiser") {}

    void runTest() override
    {
        const auto& points = Max::getStartingPoints();

        beginTest ("Three conservative starting points, dither off");
        {
            expectEquals ((int) points.size(), 3);
            expectEquals (juce::String (points[0].name), juce::String ("Mix bus"));
            expectEquals (juce::String (points[1].name), juce::String ("Demo loudness"));
            expectEquals (juce::String (points[2].name), juce::String ("Final export"));

            for (const auto& sp : points)
            {
                const juce::String n (sp.name);
                expectEquals (sp.dither, 0, n + ": dither off");
                expect (sp.thresholdDb >= Max::minThresholdDb && sp.thresholdDb <= Max::maxThresholdDb, n + ": threshold in range");
                expect (sp.ceilingDb <= -1.0f && sp.ceilingDb >= Max::minCeilingDb, n + ": ceiling at or below -1 dB");
                const float gain = sp.ceilingDb - sp.thresholdDb;
                expect (gain >= 0.0f && gain <= 6.0f, n + ": gain below threshold 0..6 dB (conservative)");
                expect (sp.releaseMs >= Max::minReleaseMs && sp.releaseMs <= Max::maxReleaseMs, n + ": release in range");
                expect (juce::String (sp.hint).isNotEmpty(), n + ": has a hint");
            }
            expectEquals (points[1].peakMode, 1, "demo loudness limits true peak");
            expectEquals (points[2].peakMode, 1, "final export limits true peak");
            expect (points[2].releaseMs >= 3.0f && points[2].releaseMs <= 7.0f, "final export release in the manual's 3-7 ms");
        }

        beginTest ("Loading sets the parameters and leaves Bypass and Link alone");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::bypassId, 1.0f);
            setParam (p, Max::ditherId, 1.0f);
            p.linkThresholdCeiling = true;

            for (int i = 0; i < (int) points.size(); ++i)
            {
                p.applyStartingPoint (i);
                const auto& sp = points[(size_t) i];
                expectWithinAbsoluteError (p.getThresholdDb(), sp.thresholdDb, 1.0e-3f);
                expectWithinAbsoluteError (p.getCeilingDb(), sp.ceilingDb, 1.0e-3f);
                expectWithinAbsoluteError (p.getReleaseMs(), sp.releaseMs, 1.0e-2f);
                expectEquals (p.isTruePeak() ? 1 : 0, sp.peakMode);
                expect (p.isAutoRelease() == sp.autoRelease, "auto release as stored");
                expect (p.getDitherDepth() == Max::Dither::Depth::off, "dither switched off");
            }
            expect (p.isBypassed() && p.linkThresholdCeiling.load(), "bypass and link untouched");

            p.applyStartingPoint (-1);
            p.applyStartingPoint (99);   // out of range: ignored
            expectWithinAbsoluteError (p.getThresholdDb(), points.back().thresholdDb, 1.0e-3f);

            // From the editor's menu.
            SongsmithMaximiserAudioProcessor q;
            SongsmithMaximiserAudioProcessorEditor ed (q);
            ed.getStartingPointBox().setSelectedItemIndex (1, juce::sendNotificationSync);
            expectWithinAbsoluteError (q.getThresholdDb(), points[1].thresholdDb, 1.0e-3f, "menu loads it");
            expectWithinAbsoluteError ((float) ed.getThresholdSlider().getValue(), points[1].thresholdDb, 1.0e-3f, "fader shows it");
        }

        beginTest ("Each starting point holds its ceiling on loud material");
        {
            constexpr double fs = 48000.0;
            const double pi = juce::MathConstants<double>::pi;
            // A loud mix of a bass line, a mid tone and bright hats, peaking near 0 dBFS.
            std::vector<float> mix ((size_t) fs);
            for (size_t i = 0; i < mix.size(); ++i)
            {
                const double t = (double) i / fs;
                mix[i] = (float) (0.5 * std::sin (2.0 * pi * 55.0 * t) + 0.3 * std::sin (2.0 * pi * 880.0 * t + 0.3)
                                  + ((i % 12000) < 600 ? 0.25 * std::sin (2.0 * pi * 12000.0 * t) : 0.0));
            }

            for (int i = 0; i < (int) points.size(); ++i)
            {
                SongsmithMaximiserAudioProcessor p;
                p.applyStartingPoint (i);
                prepare (p, fs, 512);
                const auto out = run (p, { mix, mix }, { 512 });
                const float c = Max::dbToGain (points[(size_t) i].ceilingDb);
                const juce::String n (points[(size_t) i].name);
                expect (peakOf (out[0]) <= c * (1.0f + ceilingTolerance), n + ": sample peak at or under the ceiling");
                if (points[(size_t) i].peakMode == 1)
                    expectLessOrEqual (truePeakDb (out, 4800) - points[(size_t) i].ceilingDb, 0.1f, n + ": true peak within 0.1 dB");
            }
        }
    }
};

//==============================================================================
// Randomised stress: rates, block sizes, settings, automation and nasty signals.
class StressTests final : public juce::UnitTest
{
public:
    StressTests() : juce::UnitTest ("Limiter stress", "Maximiser") {}

    void runTest() override
    {
        beginTest ("Randomised settings, signals, rates and block sizes");
        {
            juce::Random rng (20261103);
            const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
            int cases = 0, finiteFailures = 0, ceilingFailures = 0;
            float worstOverDb = -100.0f;

            for (int iteration = 0; iteration < 120; ++iteration)
            {
                const double fs = rates[rng.nextInt (5)];
                const int channels = rng.nextBool() ? 2 : 1;
                const int n = (int) (fs * 0.15);

                // Signal: noise up to +24 dBFS, square, DC, silence with impulses, a sweep or denormals.
                std::vector<std::vector<float>> in ((size_t) channels, std::vector<float> ((size_t) n));
                const int kind = rng.nextInt (6);
                const float gain = Max::dbToGain (-20.0f + 44.0f * rng.nextFloat());
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        float v = 0.0f;
                        switch (kind)
                        {
                            case 0:  v = rng.nextFloat() * 2.0f - 1.0f; break;
                            case 1:  v = ((i / 37) % 2 == 0) ? 1.0f : -1.0f; break;
                            case 2:  v = c == 0 ? 1.0f : -1.0f; break;
                            case 3:  v = (i % 997 == 0) ? (i % 2 == 0 ? 1.0f : -1.0f) : 0.0f; break;
                            case 4:  v = (float) std::sin (2.0 * juce::MathConstants<double>::pi * (20.0 + 20000.0 * i / n) * i / fs); break;
                            default: v = 1.0e-39f * (float) ((i % 3) - 1); break;
                        }
                        in[(size_t) c][(size_t) i] = v * gain;
                    }

                SongsmithMaximiserAudioProcessor p;
                setParam (p, Max::peakModeId, rng.nextBool() ? 1.0f : 0.0f);
                setParam (p, Max::autoReleaseId, rng.nextBool() ? 1.0f : 0.0f);
                setParam (p, Max::ditherId, (float) rng.nextInt (3));
                setParam (p, Max::thresholdId, -30.0f * rng.nextFloat());
                setParam (p, Max::ceilingId, -0.1f - 12.0f * rng.nextFloat());
                p.apvts.getParameter (Max::releaseId)->setValueNotifyingHost (rng.nextFloat());
                std::vector<int> blocks;
                for (int b = 0; b < 4; ++b)
                    blocks.push_back (1 + rng.nextInt (rng.nextBool() ? 64 : 4096));
                prepare (p, fs, 4096, channels);

                // The highest ceiling in use bounds the output; automation moves it below that.
                const float maxCeilingDb = p.getCeilingDb();
                const auto out = run (p, in, blocks, [&] (int bi, int)
                {
                    if (bi % 7 == 3)
                    {
                        setParam (p, Max::thresholdId, -30.0f * rng.nextFloat());
                        setParam (p, Max::ceilingId, maxCeilingDb - 6.0f * rng.nextFloat());
                        p.apvts.getParameter (Max::releaseId)->setValueNotifyingHost (rng.nextFloat());
                    }
                });

                bool finite = true;
                float pk = 0.0f;
                for (auto& ch : out)
                    for (float s : ch)
                    {
                        finite = finite && std::isfinite (s);
                        pk = std::max (pk, std::abs (s));
                    }

                ++cases;
                finiteFailures += finite ? 0 : 1;
                // Sample peak never above the ceiling (both modes; dither headroom keeps it under).
                const float limit = Max::dbToGain (maxCeilingDb) * (1.0f + ceilingTolerance);
                ceilingFailures += pk <= limit ? 0 : 1;
                worstOverDb = std::max (worstOverDb, Max::gainToDb (pk) - maxCeilingDb);
            }

            logMessage ("    " + juce::String (cases) + " random cases; worst sample peak relative to the ceiling: "
                        + juce::String (worstOverDb, 4) + " dB");
            expectEquals (finiteFailures, 0, "always finite");
            expectEquals (ceilingFailures, 0, "never above the ceiling");
        }

        beginTest ("Extreme input: +40 dBFS, NaN-free, and silence after");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::thresholdId, -30.0f);
            setParam (p, Max::ceilingId, -1.0f);
            setParam (p, Max::peakModeId, 1.0f);
            prepare (p, 48000.0, 256);
            auto loud = noise (24000, 100.0f, 7u);   // +40 dBFS
            loud.resize (48000, 0.0f);
            const auto out = run (p, { loud, loud }, { 256 });
            expect (peakOf (out[0]) <= Max::dbToGain (-1.0f) * (1.0f + ceilingTolerance), "ceiling holds at +70 dB over");
            expectEquals (peakOf (out[0], 30000), 0.0f, "silence afterwards is silent");
        }
    }
};

//==============================================================================
// The latency and Processing Load strip (suite task S11).
class DiagnosticsTests final : public juce::UnitTest
{
public:
    DiagnosticsTests() : juce::UnitTest ("Latency and Processing Load", "Maximiser") {}

    void runTest() override
    {
        using namespace Songsmith::Diagnostics;

        beginTest ("The strip shows the reported latency at every sample rate and peak mode");
        {
            int distinct = 0;
            int last = -1;
            for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
                for (float truePeak : { 0.0f, 1.0f })
                {
                    SongsmithMaximiserAudioProcessor p;
                    setParam (p, Max::peakModeId, truePeak);
                    p.setRateAndBufferSizeDetails (fs, 512);
                    p.prepareToPlay (fs, 512);

                    Testing::checkLatencyDisplay (*this, p, juce::String (truePeak > 0.5f ? "true peak, " : "sample peak, ") + juce::String (fs) + " Hz");
                    expectGreaterThan (p.getLatencySamples(), 0, "the look-ahead always adds a delay");
                    if (p.getLatencySamples() != last) { ++distinct; last = p.getLatencySamples(); }
                }
            expectGreaterThan (distinct, 2, "the delay in samples really does change with the sample rate");
        }

        beginTest ("The strip follows a sample rate change while it is open");
        {
            Testing::TemporarySettings settings;
            SongsmithMaximiserAudioProcessor p;
            p.setRateAndBufferSizeDetails (44100.0, 512);
            p.prepareToPlay (44100.0, 512);
            Bar bar (p, p.loadMeter, JucePlugin_Name);
            bar.refresh();
            const auto before = Testing::parseLatency (bar.getLatencyText());
            expectEquals (before.samples, p.getLatencySamples());

            p.setRateAndBufferSizeDetails (96000.0, 512);
            p.prepareToPlay (96000.0, 512);
            bar.refresh();
            const auto after = Testing::parseLatency (bar.getLatencyText());
            expectEquals (after.samples, p.getLatencySamples());
            expectGreaterThan (after.samples, before.samples, "more samples at the higher rate");
            expectWithinAbsoluteError (after.milliseconds, 1000.0 * p.getLatencySamples() / 96000.0, 0.051);
        }

        beginTest ("Processing Load is off until switched on, and measures real blocks at every size");
        {
            Testing::TemporarySettings settings;
            SongsmithMaximiserAudioProcessor p;
            expect (! p.loadMeter.isEnabled(), "off by default");
            Testing::checkLoadMeasured (*this, p, p.loadMeter, "Maximiser");
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
            SongsmithMaximiserAudioProcessor p;
            juce::MemoryBlock before, after;
            p.getStateInformation (before);
            p.loadMeter.setEnabled (true);
            p.getStateInformation (after);
            expect (before == after, "timing on or off changes nothing in the session state");
        }
    }
};
static StartingPointTests startingPointTests;
static DiagnosticsTests diagnosticsTests;
static StressTests stressTests;

//==============================================================================
// Loudness meter (MAX-07): BS.1770-4 K-weighting and gating, EBU R 128 windows, EBU Tech 3341 signals.
// The signals are generated here; no audio files are stored.
static std::atomic<bool> countAllocations { false };
static std::atomic<int> allocationCount { 0 };

void* operator new (std::size_t size)
{
    if (countAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size > 0 ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

namespace
{
    using Max::Loudness::LoudnessReadings;
    using Max::Loudness::StepMeter;

    // 'seconds' of a sine at 'peakDb' dBFS (1 kHz by default), or silence when peakDb is -200.
    std::vector<float> tone (double fs, double seconds, double peakDb, double freq = 1000.0)
    {
        std::vector<float> v ((size_t) std::llround (seconds * fs), 0.0f);
        if (peakDb <= -200.0)
            return v;
        const double amp = std::pow (10.0, peakDb / 20.0);
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = (float) (amp * std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / fs));
        return v;
    }

    struct Meter
    {
        StepMeter steps;
        LoudnessReadings readings;
        double fs;
        int channels;

        Meter (double sampleRate, int numChannels) : fs (sampleRate), channels (numChannels) { steps.prepare (fs); }

        // Feeds the same signal to every channel, in blocks of 'block' samples.
        void feed (const std::vector<float>& signal, int block = 4800)
        {
            for (size_t pos = 0; pos < signal.size();)
            {
                const int n = (int) std::min ((size_t) block, signal.size() - pos);
                const float* ptr[2] = { signal.data() + pos, signal.data() + pos };
                steps.process (ptr, channels, n);
                readings.update (steps);
                pos += (size_t) n;
            }
        }
    };

    double lufsOf (const Max::Loudness::Reading& r) { return r.valid ? (double) r.lufs : -1000.0; }
}

class LoudnessTests final : public juce::UnitTest
{
public:
    LoudnessTests() : juce::UnitTest ("Loudness meter", "Maximiser") {}

    void runTest() override
    {
        beginTest ("K-weighting: BS.1770-4 coefficients at 48 kHz");
        {
            Max::Loudness::Biquad shelf, hp;
            Max::Loudness::kWeightingCoefficients (48000.0, shelf, hp);
            // The standard's published 48 kHz values.
            expectWithinAbsoluteError (shelf.b0,  1.53512485958697, 1.0e-5, "shelf b0");
            expectWithinAbsoluteError (shelf.b1, -2.69169618940638, 1.0e-5, "shelf b1");
            expectWithinAbsoluteError (shelf.b2,  1.19839281085285, 1.0e-5, "shelf b2");
            expectWithinAbsoluteError (shelf.a1, -1.69065929318241, 1.0e-5, "shelf a1");
            expectWithinAbsoluteError (shelf.a2,  0.73248077421585, 1.0e-5, "shelf a2");
            expectWithinAbsoluteError (hp.a1, -1.99004745483398, 1.0e-5, "high pass a1");
            expectWithinAbsoluteError (hp.a2,  0.99007225036621, 1.0e-5, "high pass a2");

            // Response shape: +0.7 dB at 1 kHz, about +4 dB at the top, -6 dB at the 38 Hz corner (Q 0.5).
            expectWithinAbsoluteError (Max::Loudness::kWeightingMagnitudeDb (48000.0, 1000.0), 0.69, 0.05, "1 kHz");
            expectWithinAbsoluteError (Max::Loudness::kWeightingMagnitudeDb (48000.0, 10000.0), 4.0, 0.3, "10 kHz");
            expectWithinAbsoluteError (Max::Loudness::kWeightingMagnitudeDb (48000.0, 38.13547), -6.0, 0.3, "38 Hz");

            // Derived for other rates, the response agrees where the sample rate does not warp it.
            for (double fs : { 44100.0, 96000.0, 192000.0 })
                for (double f : { 100.0, 1000.0 })
                    expectWithinAbsoluteError (Max::Loudness::kWeightingMagnitudeDb (fs, f),
                                               Max::Loudness::kWeightingMagnitudeDb (48000.0, f), 0.05,
                                               juce::String (fs) + " Hz rate at " + juce::String (f) + " Hz");
        }

        beginTest ("Tech 3341: 1 kHz tone at -23 and -33 dBFS on both channels");
        for (double level : { -23.0, -33.0 })
        {
            Meter m (48000.0, 2);
            m.feed (tone (48000.0, 20.0, level));
            const auto name = juce::String (level) + " dBFS";
            expectWithinAbsoluteError (lufsOf (m.readings.getMomentary()), level, 0.1, name + " Momentary");
            expectWithinAbsoluteError (lufsOf (m.readings.getShortTerm()), level, 0.1, name + " Short Term");
            expectWithinAbsoluteError (lufsOf (m.readings.getIntegrated()), level, 0.1, name + " Integrated");
        }

        beginTest ("Readings agree at 44.1, 48, 96 and 192 kHz");
        for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Meter m (fs, 2);
            m.feed (tone (fs, 12.0, -23.0), (int) std::llround (fs * 0.1));
            expectWithinAbsoluteError (lufsOf (m.readings.getIntegrated()), -23.0, 0.1, juce::String (fs) + " Hz Integrated");
            expectWithinAbsoluteError (lufsOf (m.readings.getShortTerm()), -23.0, 0.1, juce::String (fs) + " Hz Short Term");
        }

        beginTest ("Gating: the relative gate, the absolute gate, and a level change both above the gate");
        {
            // Modelled on the gated sequences of Tech 3341 (cases 3 to 5); built here from their descriptions.
            auto sequence = [] (std::initializer_list<std::pair<double, double>> parts)   // { seconds, level }
            {
                std::vector<float> all;
                for (auto& [seconds, level] : parts)
                {
                    const auto t = tone (48000.0, seconds, level);
                    all.insert (all.end(), t.begin(), t.end());
                }
                return all;
            };

            Meter relative (48000.0, 2);   // 10 s at -36, 60 s at -23, 10 s at -36: the quiet parts fall below the relative gate
            relative.feed (sequence ({ { 10.0, -36.0 }, { 60.0, -23.0 }, { 10.0, -36.0 } }));
            expectWithinAbsoluteError (lufsOf (relative.readings.getIntegrated()), -23.0, 0.1, "relative gate");

            Meter absolute (48000.0, 2);   // 20 s at -23, then 20 s far below the -70 LUFS absolute gate
            absolute.feed (sequence ({ { 20.0, -23.0 }, { 20.0, -80.0 } }));
            expectWithinAbsoluteError (lufsOf (absolute.readings.getIntegrated()), -23.0, 0.1, "absolute gate");

            Meter both (48000.0, 2);   // 20 s at -23 and 20 s at -30: both pass the gates, so the energies average
            both.feed (sequence ({ { 20.0, -23.0 }, { 20.0, -30.0 } }));
            const double expected = 10.0 * std::log10 (0.5 * std::pow (10.0, -2.3) + 0.5 * std::pow (10.0, -3.0));
            expectWithinAbsoluteError (lufsOf (both.readings.getIntegrated()), expected, 0.1, "no gating needed");
        }

        beginTest ("Momentary (400 ms) and Short Term (3 s) rise and fall at the right pace");
        {
            Meter m (48000.0, 2);
            m.feed (tone (48000.0, 1.0, -200.0));   // silence first: no reading
            expect (! m.readings.getMomentary().valid, "no Momentary reading in silence");

            const auto step = tone (48000.0, 0.1, -23.0);
            for (int n = 1; n <= 4; ++n)
            {
                m.feed (step);
                // n of the 4 steps in the window hold the tone, the rest silence.
                expectWithinAbsoluteError (lufsOf (m.readings.getMomentary()), -23.0 + 10.0 * std::log10 (n / 4.0), 0.1,
                                           "Momentary rising, " + juce::String (n * 100) + " ms in");
            }
            for (int n = 1; n < 30; ++n)
                m.feed (step);   // 3 s of tone in all
            expectWithinAbsoluteError (lufsOf (m.readings.getShortTerm()), -23.0, 0.1, "Short Term after 3 s");

            m.feed (tone (48000.0, 0.1, -200.0));   // one step of silence
            expectWithinAbsoluteError (lufsOf (m.readings.getMomentary()), -23.0 + 10.0 * std::log10 (3.0 / 4.0), 0.1, "Momentary falling");
            expectWithinAbsoluteError (lufsOf (m.readings.getShortTerm()), -23.0 + 10.0 * std::log10 (29.0 / 30.0), 0.1, "Short Term falling slowly");
            m.feed (tone (48000.0, 0.5, -200.0));
            expect (lufsOf (m.readings.getMomentary()) < -70.0 || ! m.readings.getMomentary().valid, "Momentary has fallen away after 400 ms");
        }

        beginTest ("Silence: no reading, nothing infinite or NaN");
        {
            Meter m (48000.0, 2);
            m.feed (tone (48000.0, 10.0, -200.0));
            expect (! m.readings.getMomentary().valid && ! m.readings.getShortTerm().valid && ! m.readings.getIntegrated().valid,
                    "no readings");
            expectEquals (Max::Loudness::format (m.readings.getIntegrated()), juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")));
            for (const auto& r : { m.readings.getMomentary(), m.readings.getShortTerm(), m.readings.getIntegrated() })
                expect (std::isfinite (r.lufs) && r.lufs >= Max::dbFloor, "finite, at or above the floor");

            // Bad input never poisons the meter.
            m.readings.addStep (std::numeric_limits<double>::quiet_NaN());
            m.readings.addStep (std::numeric_limits<double>::infinity() * 0.0);
            std::vector<float> loud ((size_t) 4800, 1.0f);
            m.feed (loud);
            expect (std::isfinite (m.readings.getIntegrated().lufs), "finite after bad steps");
        }

        beginTest ("Mono reads 3 dB lower than the same signal in both stereo channels");
        {
            Meter stereo (48000.0, 2), mono (48000.0, 1);
            const auto t = tone (48000.0, 10.0, -23.0);
            stereo.feed (t);
            mono.feed (t);
            expectWithinAbsoluteError (lufsOf (stereo.readings.getIntegrated()) - lufsOf (mono.readings.getIntegrated()), 3.01, 0.1);
            expectWithinAbsoluteError (lufsOf (mono.readings.getIntegrated()), -26.0, 0.1, "mono -23 dBFS");
        }

        beginTest ("Integrated counts only audio after a reset");
        {
            Meter m (48000.0, 2);
            m.feed (tone (48000.0, 8.0, -23.0));
            m.steps.requestReset();   // as when playback starts from a stop
            m.feed (tone (48000.0, 8.0, -33.0));
            expectWithinAbsoluteError (lufsOf (m.readings.getIntegrated()), -33.0, 0.1, "after the reset marker");

            m.feed (tone (48000.0, 8.0, -23.0));   // no reset: carries on, the energies average
            const double mixed = 10.0 * std::log10 ((std::pow (10.0, -3.3) + std::pow (10.0, -2.3)) / 2.0);
            expectWithinAbsoluteError (lufsOf (m.readings.getIntegrated()), mixed, 0.15, "carrying on does not reset");
        }

        beginTest ("Block sizes 1, 64, 441, 4096 and 1000 give the same readings");
        {
            const auto signal = noise (48000 * 6, 0.3f, 77u);
            double ref[3] = {};
            bool first = true;
            for (int block : { 4800, 1, 64, 441, 4096, 1000, 7 })
            {
                Meter m (48000.0, 2);
                m.feed (signal, block);
                const double got[3] = { lufsOf (m.readings.getMomentary()), lufsOf (m.readings.getShortTerm()), lufsOf (m.readings.getIntegrated()) };
                if (first)
                    for (int i = 0; i < 3; ++i) ref[i] = got[i];
                first = false;
                for (int i = 0; i < 3; ++i)
                    expectWithinAbsoluteError (got[i], ref[i], 1.0e-6, "block " + juce::String (block) + " reading " + juce::String (i));
            }
        }

        beginTest ("The processor measures its output, resets on play from a stop only, and bypass passes through");
        {
            constexpr double fs = 48000.0;
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::bypassId, 1.0f);   // output = input, so the expected values are exact
            prepare (p, fs, 512);
            const auto a = tone (fs, 6.0, -23.0), b = tone (fs, 6.0, -33.0);

            p.notePlayState (false);
            p.notePlayState (true);   // playing from a stop: a new measurement
            run (p, { a, a }, { 512 });
            p.updateLoudness();
            expectWithinAbsoluteError (lufsOf (p.getLoudnessReadings().getIntegrated()), -23.0, 0.1, "bypassed: what passes through");

            p.notePlayState (true);   // still playing: no reset
            run (p, { b, b }, { 512 });
            p.updateLoudness();
            expectWithinAbsoluteError (lufsOf (p.getLoudnessReadings().getIntegrated()),
                                       10.0 * std::log10 ((std::pow (10.0, -2.3) + std::pow (10.0, -3.3)) / 2.0), 0.2, "carried on");

            p.notePlayState (false);
            p.notePlayState (true);   // stopped, then started: reset
            run (p, { b, b }, { 512 });
            p.updateLoudness();
            expectWithinAbsoluteError (lufsOf (p.getLoudnessReadings().getIntegrated()), -33.0, 0.1, "only the audio after the restart");

            p.resetLoudness();   // the Reset button
            expect (! p.getLoudnessReadings().getIntegrated().valid, "nothing to show right after Reset");
            run (p, { a, a }, { 512 });
            p.updateLoudness();
            expectWithinAbsoluteError (lufsOf (p.getLoudnessReadings().getIntegrated()), -23.0, 0.1, "after Reset");
        }

        beginTest ("The meter is not saved and does not change the audio");
        {
            SongsmithMaximiserAudioProcessor p;
            juce::MemoryBlock before, after;
            p.getStateInformation (before);
            prepare (p, 48000.0, 256);
            const auto t = tone (48000.0, 2.0, -10.0);
            run (p, { t, t }, { 256 });
            p.updateLoudness();
            p.getStateInformation (after);
            expect (before == after, "readings are not in the session");
            expectEquals (SongsmithMaximiserAudioProcessor::stateVersion, 3, "state version unchanged");
        }

        beginTest ("The editor shows the readings, Reset and a click on the number start again");
        {
            SongsmithMaximiserAudioProcessor p;
            setParam (p, Max::bypassId, 1.0f);
            prepare (p, 48000.0, 512);
            SongsmithMaximiserAudioProcessorEditor ed (p);
            const auto dash = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93"));
            expectEquals (ed.getIntegratedLabel().getText(), dash, "a dash until there is audio");

            const auto t = tone (48000.0, 5.0, -23.0);
            run (p, { t, t }, { 512 });
            ed.updateMeters (0.03);
            expectEquals (ed.getIntegratedLabel().getText(), juce::String ("-23.0"));

            ed.getLoudnessResetButton().onClick();   // what pressing Reset runs
            expectEquals (ed.getIntegratedLabel().getText(), dash, "Reset clears the readout");
            run (p, { t, t }, { 512 });
            ed.updateMeters (0.03);
            expectEquals (ed.getIntegratedLabel().getText(), juce::String ("-23.0"), "measuring again");
            ed.clickIntegratedReadout();   // what a click on the Integrated number runs
            expectEquals (ed.getIntegratedLabel().getText(), dash, "clicking the number resets");
        }

        beginTest ("No allocation in processBlock with the meter running");
        {
            SongsmithMaximiserAudioProcessor p;
            prepare (p, 48000.0, 512);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            const auto n = noise (512, 0.5f, 5u);
            for (int c = 0; c < 2; ++c)
                buffer.copyFrom (c, 0, n.data(), 512);

            p.processBlock (buffer, midi);   // warm up
            allocationCount = 0;
            countAllocations = true;
            for (int i = 0; i < 200; ++i)   // 2.1 s: many finished steps and a reset
            {
                if (i == 100) { p.notePlayState (false); p.notePlayState (true); }
                p.processBlock (buffer, midi);
            }
            countAllocations = false;
            expectEquals (allocationCount.load(), 0, "allocations in processBlock");
        }
    }
};
static LoudnessTests loudnessTests;

//==============================================================================
static SongsmithUITests sharedUITests { "Maximiser" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "Maximiser" };
static SongsmithGuidanceTests sharedGuidanceTests { "Maximiser" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// MaximiserTests --screenshot <file.png> [--scale 2]: renders the editor (with some gain reduction on
// the meters) to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale)
{
    SongsmithMaximiserAudioProcessor p;
    setParam (p, Max::thresholdId, -8.0f);
    setParam (p, Max::ceilingId, -1.0f);
    setParam (p, Max::releaseId, 3.0f);
    setParam (p, Max::peakModeId, 1.0f);
    p.linkThresholdCeiling = true;
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithMaximiserAudioProcessorEditor> editor (dynamic_cast<SongsmithMaximiserAudioProcessorEditor*> (p.createEditor()));
    const auto in = noise (96000, 0.7f, 1u);   // two seconds, so the loudness readings have something to show
    run (p, { in, in }, { 512 });
    editor->updateMeters (0.02);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
        return 1;
    std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());

    // Every control state, drawn by the editor's look-and-feel, next to it.
    MaxLookAndFeel lf;
    Songsmith::UI::Testing::writeControlSheet (lf, MaxColours::background, scale,
        file.getSiblingFile (file.getFileNameWithoutExtension() + "-controls.png"));
    return 0;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshot")
        {
            float scale = 1.0f;
            for (int j = 1; j + 1 < argc; ++j)
                if (juce::String (argv[j]) == "--scale")
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Maximiser");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
