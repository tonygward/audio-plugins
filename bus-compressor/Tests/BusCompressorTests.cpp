// Songsmith Bus Compressor test runner: the plugin (identity, parameters, state, bypass, presets, editor)
// and the shared Songsmith checks. The engine is tested in EngineTests.cpp.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <random>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithMeterColumnTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "../../shared/Tests/SongsmithUpdateTests.h"
#include "../../shared/Tests/SongsmithBeforeAfterTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithBusCompressorProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithBusCompressorProcessor& p, const char* id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    std::vector<float> noise (int n, float amp, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-amp, amp);
        std::vector<float> v ((size_t) n);
        for (auto& s : v)
            s = dist (rng);
        return v;
    }

    void prepare (SongsmithBusCompressorProcessor& p, double fs, int block, int numChannels = 2)
    {
        const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        p.setBusesLayout (layout);
        p.setRateAndBufferSizeDetails (fs, block);
        p.prepareToPlay (fs, block);
    }

    // Runs 'in' (one vector per channel) through the processor in blocks of 'block' samples.
    std::vector<std::vector<float>> run (SongsmithBusCompressorProcessor& p, std::vector<std::vector<float>> in, int block)
    {
        const int channels = (int) in.size(), total = (int) in[0].size();
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> buffer (channels, n);
            for (int c = 0; c < channels; ++c)
                buffer.copyFrom (c, 0, in[(size_t) c].data() + pos, n);
            p.processBlock (buffer, midi);
            for (int c = 0; c < channels; ++c)
                std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + n, in[(size_t) c].begin() + pos);
        }
        return in;
    }

    float rms (const std::vector<float>& v, size_t from = 0)
    {
        double sum = 0.0;
        for (size_t i = from; i < v.size(); ++i)
            sum += (double) v[i] * v[i];
        return (float) std::sqrt (sum / (double) (v.size() - from));
    }

    juce::MemoryBlock stateOf (SongsmithBusCompressorProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }
}

//==============================================================================
class BusCompressorPluginTests final : public juce::UnitTest
{
public:
    BusCompressorPluginTests() : juce::UnitTest ("Songsmith Bus Compressor plugin", "BusCompressor") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Bus Compressor"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            expectEquals (juce::String (BUS_MANUFACTURER_CODE), juce::String ("Tnyw"), "shared with the whole family");
            expectEquals (juce::String (BUS_PLUGIN_CODE), juce::String ("Bus1"), "this plugin's own code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1", "Opt1", "Peq1" })
                expect (juce::String (BUS_PLUGIN_CODE) != juce::String (other), juce::String ("differs from ") + other);
        }

        beginTest ("Parameter IDs, ranges and defaults are stable");
        {
            SongsmithBusCompressorProcessor p;
            struct Row { const char* id; float lo, hi, def; const char* name; };
            const Row rows[] = {
                { "threshold", -15.0f, 10.0f, 4.0f, "Threshold" },
                { "ratio", 0.0f, 2.0f, 0.0f, "Ratio" },
                { "attack", 0.0f, 5.0f, 4.0f, "Attack" },
                { "release", 0.0f, 4.0f, 4.0f, "Release" },
                { "makeup_gain", 0.0f, 15.0f, 0.0f, "Make-up Gain" },
                { "bypass", 0.0f, 1.0f, 0.0f, "Bypass" },
            };
            expectEquals (p.getParameters().size(), (int) std::size (rows), "six parameters, no more");
            for (const auto& r : rows)
            {
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (r.id));
                expect (rp != nullptr, juce::String (r.id) + " exists");
                if (rp == nullptr)
                    continue;
                expectEquals (rp->getName (100), juce::String (r.name));
                expectWithinAbsoluteError (rp->getNormalisableRange().start, r.lo, 1.0e-6f);
                expectWithinAbsoluteError (rp->getNormalisableRange().end, r.hi, 1.0e-6f);
                expectWithinAbsoluteError (rp->convertFrom0to1 (rp->getDefaultValue()), r.def, 1.0e-4f);
            }
            expectEquals (BusComp::ratioNames().joinIntoString ("/"), juce::String ("2:1/4:1/10:1"));
            expectEquals (BusComp::attackNames().joinIntoString ("/"), juce::String ("0.1 ms/0.3 ms/1 ms/3 ms/10 ms/30 ms"));
            expectEquals (BusComp::releaseNames().joinIntoString ("/"), juce::String ("0.1 s/0.3 s/0.6 s/1.2 s/Auto"));
            expect (p.getBypassParameter() == p.apvts.getParameter (BusComp::bypassId), "the host's own Bypass is the plugin's Bypass");
        }

        beginTest ("Buses: stereo and mono are supported, nothing else");
        {
            SongsmithBusCompressorProcessor p;
            auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
            {
                juce::AudioProcessor::BusesLayout l;
                l.inputBuses.add (in);
                l.outputBuses.add (out);
                return l;
            };
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
            expect (p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())));
            expect (! p.isBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())));
            expectEquals (p.getLatencySamples(), 0, "no latency");
            expectEquals (p.getTailLengthSeconds(), 0.0);
        }

        beginTest ("The sound follows the parameters, including host automation, at any block size");
        {
            const auto signal = noise (96000, 0.5f, 3);
            std::vector<float> reference;
            for (int block : { 64, 480, 4096 })
            {
                SongsmithBusCompressorProcessor p;
                prepare (p, 48000.0, block);
                setParam (p, BusComp::thresholdId, -10.0f);
                setParam (p, BusComp::ratioId, 1.0f);
                setParam (p, BusComp::makeupId, 0.0f);
                const auto out = run (p, { signal, signal }, block);
                expect (rms (out[0], 24000) < 0.85f * rms (signal, 24000), "a low Threshold turns the loud sound down");
                expect (out[0] == out[1], "identical channels stay identical");
                if (reference.empty())
                    reference = out[0];
                else
                    expect (out[0] == reference, "the same result at a block size of " + juce::String (block));
            }

            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, BusComp::thresholdId, 10.0f);
            setParam (p, BusComp::makeupId, 0.0f);
            const auto quietSignal = noise (96000, 0.1f, 4);   // always below the highest Threshold
            const auto off = run (p, { quietSignal, quietSignal }, 480);
            expect (quietSignal == off[0], "below the Threshold, with no Make-up Gain, the sound is untouched");
            setParam (p, BusComp::makeupId, 6.0f);
            p.reset();
            const auto louder = run (p, { quietSignal, quietSignal }, 480);
            expectWithinAbsoluteError (rms (louder[0], 24000) / rms (quietSignal, 24000), std::pow (10.0f, 6.0f / 20.0f), 0.05f, "Make-up Gain adds 6 dB");
        }

        beginTest ("A higher ratio reduces more, and a slower attack lets more of a sudden hit through");
        {
            const auto signal = noise (96000, 0.5f, 5);
            float level[3] {};
            for (int ratio = 0; ratio < 3; ++ratio)
            {
                SongsmithBusCompressorProcessor p;
                prepare (p, 48000.0, 480);
                setParam (p, BusComp::thresholdId, -5.0f);
                setParam (p, BusComp::makeupId, 0.0f);
                setParam (p, BusComp::ratioId, (float) ratio);
                level[ratio] = rms (run (p, { signal, signal }, 480)[0], 24000);
            }
            expect (level[1] < level[0] && level[2] < level[1], "2:1, then 4:1, then 10:1 each turn the sound down more");

            std::vector<float> hit ((size_t) 4800, 0.01f);
            for (size_t i = 2400; i < 2880; ++i)
                hit[i] = 0.9f;
            float peak[2] {};
            int index = 0;
            for (int attack : { 1, 5 })
            {
                SongsmithBusCompressorProcessor p;
                prepare (p, 48000.0, 4800);
                setParam (p, BusComp::thresholdId, -10.0f);
                setParam (p, BusComp::ratioId, 2.0f);
                setParam (p, BusComp::attackId, (float) attack);
                setParam (p, BusComp::makeupId, 0.0f);
                const auto out = run (p, { hit, hit }, 4800);
                peak[index++] = *std::max_element (out[0].begin() + 2400, out[0].begin() + 2880);
            }
            expect (peak[1] > peak[0], "the 30 ms Attack lets more of the first hit through than 1 ms");
        }

        beginTest ("Mono works");
        {
            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480, 1);
            setParam (p, BusComp::thresholdId, -12.0f);
            const auto out = run (p, { noise (48000, 0.5f, 9) }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s); }));
        }

        beginTest ("Bypass: the dry sound passes bit-exact, fades in and out without a jump, and never glitches on return");
        {
            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, BusComp::thresholdId, -12.0f);
            setParam (p, BusComp::makeupId, 10.0f);
            const auto signal = noise (96000, 0.5f, 13);
            setParam (p, BusComp::bypassId, 1.0f);
            auto out = run (p, { signal, signal }, 480);
            expect (std::equal (signal.begin() + 4800, signal.end(), out[0].begin() + 4800), "bit-exact once the short fade is over");

            float biggestStep = 0.0f;
            for (size_t i = 1; i < 4800; ++i)
                biggestStep = std::max (biggestStep, std::abs (out[0][i] - out[0][i - 1] - (signal[i] - signal[i - 1])));
            expect (biggestStep < 2.0f, "no click at the start of the fade");

            setParam (p, BusComp::bypassId, 0.0f);
            out = run (p, { signal, signal }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 20.0f; }), "returning is clean");
            expect (out[0] != signal, "and the processing is back");
        }

        beginTest ("Saved state round-trips every parameter and the editor's own settings");
        {
            SongsmithBusCompressorProcessor a;
            setParam (a, BusComp::thresholdId, -7.5f);
            setParam (a, BusComp::ratioId, 2.0f);
            setParam (a, BusComp::attackId, 1.0f);
            setParam (a, BusComp::releaseId, 3.0f);
            setParam (a, BusComp::makeupId, 12.5f);
            setParam (a, BusComp::bypassId, 1.0f);
            a.editorWidth = 1111;
            a.presets.currentName = "My Drums";
            const auto block = stateOf (a);

            SongsmithBusCompressorProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (const char* id : { BusComp::thresholdId, BusComp::ratioId, BusComp::attackId, BusComp::releaseId, BusComp::makeupId, BusComp::bypassId })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-4f, juce::String (id));
            expectEquals (b.editorWidth.load(), 1111);
            expectEquals (b.presets.currentName, juce::String ("My Drums"));

            auto xml = a.createState().createXml();
            expect (xml->getChildByName ("UI") != nullptr, "the editor settings are saved");
            expectEquals (a.createState().getProperty ("stateVersion", 0).operator int(), SongsmithBusCompressorProcessor::stateVersion);
        }

        beginTest ("Older, partial, damaged and out-of-range saved state loads safely");
        {
            SongsmithBusCompressorProcessor p;
            setParam (p, BusComp::thresholdId, -12.0f);
            setParam (p, BusComp::ratioId, 2.0f);

            // Only Make-up Gain saved: everything else returns to its default, as a session from before it existed would.
            juce::ValueTree partial (p.apvts.state.getType());
            partial.appendChild (juce::ValueTree ("PARAM", { { "id", BusComp::makeupId }, { "value", 9.0f } }), nullptr);
            p.restoreFromValueTree (partial);
            expectWithinAbsoluteError (getParam (p, BusComp::makeupId), 9.0f, 1.0e-4f);
            expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), BusComp::defaultThreshold, 1.0e-4f, "missing: default");
            expectEquals (getParam (p, BusComp::ratioId), 0.0f, "missing: 2:1");
            expectEquals (p.editorWidth.load(), BusComp::EditorLayout::defaultWidth, "missing: default size");

            // Out-of-range, non-finite and unknown values.
            juce::ValueTree odd (p.apvts.state.getType());
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", BusComp::makeupId }, { "value", 999.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", BusComp::thresholdId }, { "value", std::numeric_limits<float>::quiet_NaN() } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", BusComp::ratioId }, { "value", 77.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "from_the_future" }, { "value", 1.0f } }), nullptr);
            juce::ValueTree ui ("UI");
            ui.setProperty ("editorWidth", 99999, nullptr);
            odd.appendChild (ui, nullptr);
            p.restoreFromValueTree (odd);
            expectEquals (getParam (p, BusComp::makeupId), SongsmithBus::maxMakeupDb, "clamped");
            expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), BusComp::defaultThreshold, 1.0e-4f, "not a number: default");
            expectEquals (getParam (p, BusComp::ratioId), 2.0f, "a step past the end is the last step");
            expectEquals (p.editorWidth.load(), BusComp::EditorLayout::maxWidth);

            const char junk[] = "this is not a saved session";
            p.setStateInformation (junk, (int) sizeof (junk));
            p.setStateInformation (nullptr, 0);
            expectEquals (p.editorWidth.load(), BusComp::EditorLayout::maxWidth, "junk is ignored");
        }

        beginTest ("Undo and Redo work for the musician's changes, and a loaded session starts with a clean history");
        {
            SongsmithBusCompressorProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, BusComp::thresholdId, BusComp::makeupId, "Bus Compressor");
        }

        beginTest ("The starting points set every sound parameter, leave Bypass alone, and are the host's programs");
        {
            const auto& list = BusComp::Presets::factory();
            expectEquals ((int) list.size(), 5);
            SongsmithBusCompressorProcessor p;
            expectEquals (p.getNumPrograms(), 5);
            setParam (p, BusComp::bypassId, 1.0f);
            for (int i = 0; i < p.getNumPrograms(); ++i)
            {
                p.setCurrentProgram (i);
                const auto& f = list[(size_t) i];
                expectEquals (p.getProgramName (i), juce::String (f.name));
                expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), f.threshold, 1.0e-3f, f.name);
                expectEquals (getParam (p, BusComp::ratioId), (float) f.ratio, f.name);
                expectEquals (getParam (p, BusComp::attackId), (float) f.attack, f.name);
                expectEquals (getParam (p, BusComp::releaseId), (float) f.release, f.name);
                expectWithinAbsoluteError (getParam (p, BusComp::makeupId), f.makeupDb, 1.0e-3f, f.name);
                expectEquals (getParam (p, BusComp::bypassId), 1.0f, "Bypass is not a preset's business");
                expectEquals (p.getCurrentProgram(), i);
                expectEquals (p.presets.currentName, juce::String (f.name));
            }
            expectEquals (p.getProgramName (99), juce::String());

            // Names say what they are for: Title Case, no model names, no tutorial titles, no playful names.
            for (const auto& f : list)
            {
                const juce::String presetName (f.name);
                expect (presetName.length() >= 8 && presetName.length() <= 30, presetName + " is short and clear");
                for (const auto& word : juce::StringArray::fromTokens (presetName, " ", ""))
                    expect (juce::CharacterFunctions::isUpperCase (word[0]), presetName + " is Title Case");
                for (const char* banned : { "SSL", "G Series", "Solid State", "Gilder", "Magic", "Vintage", "Legendary" })
                    expect (! presetName.containsIgnoreCase (banned), presetName + " keeps model and educator names out of the title");
                expect (juce::String (f.hint).isNotEmpty() && ! juce::String (f.hint).containsIgnoreCase ("Gilder"),
                        presetName + " has a listening cue that stands on its own");
            }
        }

        beginTest ("Your own presets: save, list, reload, replace, and refuse the starting points' names and damaged files");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bus-preset-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            BusComp::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithBusCompressorProcessor p;
                const auto folder = p.presets.getUserFolder();
                expect (folder.getFullPathName().contains ("Songsmith Bus Compressor") && folder.getFullPathName().endsWith ("Presets"),
                        "a folder of its own: " + folder.getFullPathName());
                expect (p.presets.getUserPresetFiles().isEmpty());

                setParam (p, BusComp::thresholdId, -6.0f);
                setParam (p, BusComp::ratioId, 2.0f);
                setParam (p, BusComp::attackId, 1.0f);
                setParam (p, BusComp::releaseId, 2.0f);
                setParam (p, BusComp::makeupId, 7.0f);
                juce::String error;
                expect (p.presets.saveAs ("  My: Drums?  ", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1);
                expect (p.presets.userPresetExists ("My Drums"), "the name is cleaned the same way when checking");
                expectEquals (p.presets.currentName, juce::String ("My Drums"));

                setParam (p, BusComp::thresholdId, 8.0f);
                setParam (p, BusComp::ratioId, 0.0f);
                setParam (p, BusComp::attackId, 5.0f);
                setParam (p, BusComp::releaseId, 4.0f);
                setParam (p, BusComp::makeupId, 0.0f);
                setParam (p, BusComp::bypassId, 1.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error), error);
                expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), -6.0f, 1.0e-3f);
                expectEquals (getParam (p, BusComp::ratioId), 2.0f);
                expectEquals (getParam (p, BusComp::attackId), 1.0f);
                expectEquals (getParam (p, BusComp::releaseId), 2.0f);
                expectWithinAbsoluteError (getParam (p, BusComp::makeupId), 7.0f, 1.0e-3f);
                expectEquals (getParam (p, BusComp::bypassId), 1.0f, "a preset never touches Bypass");

                expect (! p.presets.saveAs ("", error) && error.isNotEmpty(), "an empty name is refused");
                for (const auto& f : BusComp::Presets::factory())
                    expect (! p.presets.saveAs (f.name, error) && error.contains ("starting point"), juce::String (f.name) + " cannot be replaced");
                expect (! p.presets.saveAs (juce::String (BusComp::Presets::factory()[0].name).toUpperCase(), error), "in any capitals");

                // A replaced preset is the new settings.
                setParam (p, BusComp::thresholdId, 3.0f);
                expect (p.presets.saveAs ("My Drums", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1, "replaced, not duplicated");
                setParam (p, BusComp::thresholdId, -12.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error));
                expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), 3.0f, 1.0e-3f);

                // Damaged files.
                const auto bad = folder.getChildFile ("bad.sspreset");
                bad.replaceWithText ("this is not xml");
                expect (! p.presets.loadFile (bad, error) && error.contains ("isn't a"), "not xml");
                bad.replaceWithText ("<SomethingElse threshold=\"5\"/>");
                expect (! p.presets.loadFile (bad, error), "the wrong kind of file");
                expect (! p.presets.loadFile (folder.getChildFile ("missing.sspreset"), error), "a missing file");
                bad.replaceWithText ("<SongsmithBusCompressorPreset threshold=\"9999\" makeup_gain=\"nan\"/>");
                expect (p.presets.loadFile (bad, error), error);
                expectEquals (getParam (p, BusComp::thresholdId), SongsmithBus::maxThresholdDial, "out of range is clamped");
                expectWithinAbsoluteError (getParam (p, BusComp::makeupId), BusComp::defaultMakeupDb, 1.0e-3f, "not a number is the default");
            }
            BusComp::Presets::setDefaultFolderRootForTesting (juce::File());
            root.deleteRecursively();
        }

        beginTest ("The meter's number comes from the audio thread and is read once");
        {
            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, BusComp::thresholdId, -12.0f);
            run (p, { noise (48000, 0.6f, 21), noise (48000, 0.6f, 22) }, 480);
            const float reduction = p.takeGainReductionDb();
            expect (reduction > 2.0f && reduction < 20.0f, "reduction " + juce::String (reduction));
            expectEquals (p.takeGainReductionDb(), 0.0f, "read and reset");

            setParam (p, BusComp::bypassId, 1.0f);
            run (p, { noise (48000, 0.6f, 23), noise (48000, 0.6f, 24) }, 480);
            p.takeGainReductionDb();
            run (p, { noise (4800, 0.6f, 25), noise (4800, 0.6f, 26) }, 480);
            expectEquals (p.takeGainReductionDb(), 0.0f, "no reduction is shown while bypassed");
        }
    }
};

//==============================================================================
class BusCompressorEditorTests final : public juce::UnitTest
{
public:
    BusCompressorEditorTests() : juce::UnitTest ("Songsmith Bus Compressor editor", "BusCompressor") {}

    static std::unique_ptr<SongsmithBusCompressorEditor> make (SongsmithBusCompressorProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithBusCompressorEditor> (dynamic_cast<SongsmithBusCompressorEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        beginTest ("The editor opens at the default, minimum and maximum sizes, keeps its shape and remembers its size");
        for (int width : { BusComp::EditorLayout::defaultWidth, BusComp::EditorLayout::minWidth, BusComp::EditorLayout::maxWidth })
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, width);
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), BusComp::EditorLayout::heightFor (width));
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) BusComp::EditorLayout::designWidth, 1.0e-5f);
        }
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, 1000);
            ed->setSize (1200, BusComp::EditorLayout::heightFor (1200));
            expectEquals (p.editorWidth.load(), 1200, "the size is saved with the session");
            expectEquals (BusComp::EditorLayout::constrainWidth (1), BusComp::EditorLayout::minWidth);
            expectEquals (BusComp::EditorLayout::constrainWidth (99999), BusComp::EditorLayout::maxWidth);
            expectEquals (BusComp::EditorLayout::minWidth * 4, BusComp::EditorLayout::designWidth * 3, "75 % at the smallest");
            expectEquals (BusComp::EditorLayout::maxWidth, BusComp::EditorLayout::designWidth * 3, "300 % at the largest");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass, and Bypass is top-right in the suite red");
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Bus Compressor");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (BusComp::bypassId), "Bus Compressor");
            expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), "the title does not overlap the preset selector");
            expect (ed->getTitleHint().getBounds() == ed->getTitleBounds(), "the hint sits exactly over the title");
        }

        beginTest ("Layout: the meter above the knobs, the knobs in signal order, nothing overlapping");
        for (int width : { BusComp::EditorLayout::minWidth, BusComp::EditorLayout::defaultWidth, BusComp::EditorLayout::maxWidth })
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            std::vector<juce::Component*> parts { &ed->getThresholdKnob(), &ed->getRatioKnob(), &ed->getAttackKnob(), &ed->getReleaseKnob(),
                                                  &ed->getMakeupKnob(), &ed->getMeter() };
            auto* content = ed->getMeter().getParentComponent();
            for (size_t i = 0; i < parts.size(); ++i)
            {
                expect (content->getLocalBounds().contains (parts[i]->getBounds()), context + ": inside the editor");
                expect (parts[i]->getBottom() <= BusComp::EditorLayout::contentHeight, context + ": above the footer strip");
                for (size_t j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), context + ": no overlap");
            }
            const auto meter = ed->getMeter().getBounds();
            for (size_t i = 0; i < 5; ++i)
                expect (parts[i]->getY() >= meter.getBottom(), context + ": the knobs are under the meter that shows their effect");
            for (size_t i = 1; i < 5; ++i)
                expect (parts[i - 1]->getRight() <= parts[i]->getX(), context + ": Threshold, Ratio, Attack, Release, Make-up Gain from left to right");
            expect (ed->getBypassButton().getRight() <= content->getWidth(), context + ": Bypass is inside");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("The knobs show and take values, name their steps, and double-click returns them to the default");
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            expectEquals (ed->getThresholdKnob().getValueText(), juce::String ("+4.0 dB"));
            expectEquals (ed->getRatioKnob().getValueText(), juce::String ("2:1"));
            expectEquals (ed->getAttackKnob().getValueText(), juce::String ("10 ms"));
            expectEquals (ed->getReleaseKnob().getValueText(), juce::String ("Auto"));
            expectEquals (ed->getMakeupKnob().getValueText(), juce::String ("0.0 dB"));

            ed->getThresholdKnob().slider.setValue (-7.5, juce::sendNotificationSync);
            ed->getRatioKnob().slider.setValue (2.0, juce::sendNotificationSync);
            ed->getAttackKnob().slider.setValue (0.0, juce::sendNotificationSync);
            ed->getReleaseKnob().slider.setValue (2.0, juce::sendNotificationSync);
            ed->getMakeupKnob().slider.setValue (12.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), -7.5f, 1.0e-3f);
            expectEquals (getParam (p, BusComp::ratioId), 2.0f);
            expectEquals (getParam (p, BusComp::attackId), 0.0f);
            expectEquals (getParam (p, BusComp::releaseId), 2.0f);
            expectWithinAbsoluteError (getParam (p, BusComp::makeupId), 12.0f, 1.0e-3f);
            expectEquals (ed->getThresholdKnob().getValueText(), juce::String ("-7.5 dB"));
            expectEquals (ed->getRatioKnob().getValueText(), juce::String ("10:1"));
            expectEquals (ed->getAttackKnob().getValueText(), juce::String ("0.1 ms"));
            expectEquals (ed->getReleaseKnob().getValueText(), juce::String ("0.6 s"));
            expectEquals (ed->getMakeupKnob().getValueText(), juce::String ("+12.0 dB"));

            setParam (p, BusComp::releaseId, 3.0f);
            expectEquals (ed->getReleaseKnob().getValueText(), juce::String ("1.2 s"), "automation moves the knob");
            setParam (p, BusComp::thresholdId, 0.0f);
            expectEquals (ed->getThresholdKnob().getValueText(), juce::String ("0.0 dB"));

            // Every step of every stepped knob can be reached, and a knob has exactly as many steps as the control has.
            struct Stepped { BusKnob& knob; int steps; };
            for (auto& s : { Stepped { ed->getRatioKnob(), 3 }, Stepped { ed->getAttackKnob(), 6 }, Stepped { ed->getReleaseKnob(), 5 } })
            {
                expectEquals (s.knob.slider.getMinimum(), 0.0);
                expectEquals (s.knob.slider.getMaximum(), (double) (s.steps - 1));
                expectEquals (s.knob.slider.getInterval(), 1.0);
            }
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { BusComp::EditorLayout::defaultWidth, BusComp::EditorLayout::minWidth })
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, width);
            const auto context = "Bus Compressor at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (const auto& names : { BusComp::ratioNames(), BusComp::attackNames(), BusComp::releaseNames() })
                texts.addArray (names);
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : BusComp::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 15, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Threshold", "Ratio", "Attack", "Release", "Make-up Gain", "Gain Reduction", "Bypass", "Undo", "Redo" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Bus Compressor");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Bus Compressor editor");
        }

        beginTest ("The footer shows the version and latency and the Processing Load switch, in the shared strip");
        {
            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            expectEquals (ed->getDiagnosticsBounds().getY(), BusComp::EditorLayout::contentHeight);
            expectEquals (ed->getDiagnosticsBounds().getHeight(), BusComp::EditorLayout::footerHeight);
            Songsmith::Diagnostics::Testing::checkVersionBinding (*this, p, "Bus Compressor");
            Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, p, "Bus Compressor");
            Songsmith::Diagnostics::Testing::checkFooterLayout (*this, p, "Bus Compressor");
        }

        beginTest ("The meter: one lamp for each decibel, a quick rise, a smooth fall, and the readout wording");
        {
            using namespace BusComp::UI;
            expectEquals (lampsLit (0.0f), 0);
            expectEquals (lampsLit (0.99f), 0, "a lamp lights when its whole decibel is reached");
            expectEquals (lampsLit (1.0f), 1);
            expectEquals (lampsLit (3.4f), 3);
            expectEquals (lampsLit (25.0f), numLamps, "and the row stops at 20");
            expectEquals (lampsLit (-4.0f), 0);

            Ballistics b;
            b.reset();
            expectEquals (b.advance (8.0f, 0.001f), 8.0f, "it rises at once");
            const float fallen = b.advance (0.0f, Ballistics::fallSeconds);
            expectWithinAbsoluteError (fallen, 8.0f * std::exp (-1.0f), 1.0e-4f, "and falls smoothly");
            expectEquals (b.advance (99.0f, 0.01f), BusComp::meterMaxDb, "and is held at the end of the scale");

            expectEquals (SongsmithBusCompressorEditor::gainReductionText (3.14f), juce::String ("Gain Reduction 3.1 dB"));
            expectEquals (SongsmithBusCompressorEditor::gainReductionText (-1.0f), juce::String ("Gain Reduction 0.0 dB"));
        }

        beginTest ("The meter follows the audio");
        {
            SongsmithBusCompressorProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, BusComp::thresholdId, -12.0f);
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            run (p, { noise (48000, 0.7f, 31), noise (48000, 0.7f, 32) }, 480);
            ed->advanceMeter (0.5f);
            expect (ed->getMeter().ballistics.position() > 2.0f, "the lamps have lit");
            ed->advanceMeter (2.0f);
            expect (ed->getMeter().ballistics.position() < 0.1f, "and go out again when nothing is happening");
        }

        beginTest ("The preset menu lists the starting points, your presets, Save and the folder, and loading a starting point is one Undo step");
        {
            SongsmithBusCompressorProcessor p;
            auto ed = make (p, BusComp::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            int starting = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemId (i) >= 1 && box.getItemId (i) <= 5)
                    ++starting;
            expectEquals (starting, 5, "five starting points");
            box.setSelectedId (3, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), BusComp::Presets::factory()[2].threshold, 1.0e-3f);
            expectEquals (getParam (p, BusComp::attackId), (float) BusComp::Presets::factory()[2].attack);
            expect (ed->getUndoButton().isEnabled(), "Undo is available after loading");
            expectEquals (box.getText(), juce::String (BusComp::Presets::factory()[2].name));
            expect (box.getTooltip().contains (BusComp::Presets::factory()[2].hint), "the listening cue is on hover");
            p.history.undo();
            expectWithinAbsoluteError (getParam (p, BusComp::thresholdId), BusComp::defaultThreshold, 1.0e-3f, "one Undo restores everything the preset changed");
            expectEquals (getParam (p, BusComp::attackId), (float) BusComp::defaultAttack);
            expectEquals (getParam (p, BusComp::releaseId), (float) BusComp::defaultRelease);
            expectWithinAbsoluteError (getParam (p, BusComp::makeupId), BusComp::defaultMakeupDb, 1.0e-3f);
        }
    }
};

static BusCompressorPluginTests busCompressorPluginTests;
static BusCompressorEditorTests busCompressorEditorTests;

//==============================================================================
static SongsmithUITests sharedUITests { "BusCompressor" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "BusCompressor" };
static SongsmithMeterColumnTests<SongsmithBusCompressorProcessor, SongsmithBusCompressorEditor> sharedMeterColumnTests { "BusCompressor", BusComp::EditorLayout::designWidth, BusComp::EditorLayout::contentHeight };
static SongsmithGuidanceTests sharedGuidanceTests { "BusCompressor" };
static SongsmithUpdateTests sharedUpdateTests { "BusCompressor" };
static SongsmithBeforeAfterTests<SongsmithBusCompressorProcessor, SongsmithBusCompressorEditor> sharedBeforeAfterTests { "BusCompressor" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// BusCompressorTests --screenshot <file.png> [--scale 2] [--preset N]: renders the editor with some compression on
// the meter to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale, int preset)
{
    SongsmithBusCompressorProcessor p;
    p.setCurrentProgram (juce::jlimit (0, p.getNumPrograms() - 1, preset));
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithBusCompressorEditor> editor (dynamic_cast<SongsmithBusCompressorEditor*> (p.createEditor()));
    // Four seconds of a steady, uneven tone peaking around -8 dBFS, so the lamps have something to show.
    std::vector<float> in (192000);
    for (size_t i = 0; i < in.size(); ++i)
    {
        const double sec = (double) i / 48000.0;
        const double swell = 0.22 + 0.17 * (0.5 + 0.5 * std::sin (sec * 2.1));
        in[i] = (float) (swell * std::sin (2.0 * 3.14159265358979 * 220.0 * sec));
    }
    run (p, { in, in }, 512);
    editor->advanceMeter (0.5f);

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
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--screenshot")
        {
            float scale = 1.0f;
            int preset = 0;
            for (int j = 1; j < argc; ++j)
            {
                if (juce::String (argv[j]) == "--scale" && j + 1 < argc)
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
                if (juce::String (argv[j]) == "--preset" && j + 1 < argc)
                    preset = juce::String (argv[j + 1]).getIntValue();
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, preset);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("BusCompressor");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
