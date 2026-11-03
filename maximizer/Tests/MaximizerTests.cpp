// Songsmith Maximiser test runner.
//   MaximizerTests.exe   -> run all unit tests (exit code 1 on failure)
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <random>
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
            const std::pair<const char*, int> expected[] = { { "threshold", 1 }, { "ceiling", 1 }, { "release", 1 }, { "bypass", 1 } };

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
            expectEquals (p.getParameters().size(), 4, "no unexpected parameters");
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
                        in.push_back (noise ((int) fs / 2, 0.9f, 7u + (unsigned) c));

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
                    diff = std::max (diff, std::abs (out[c][i] - in[c][i]));
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
    }
};

static IdentityTests identityTests;
static ShellProcessingTests shellProcessingTests;
static AutomationAndStateTests automationAndStateTests;

//==============================================================================
struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

int main (int, char*[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

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
