// Songsmith Passive EQ test runner: the plugin (identity, parameters, state, bypass, presets, editor)
// and the shared Songsmith checks. The engine is tested in EngineTests.cpp.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <random>
#include <set>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithPassiveEQProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithPassiveEQProcessor& p, const char* id)
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

    std::vector<float> sine (int n, float amp, double hz, double fs)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) i / fs);
        return v;
    }

    void prepare (SongsmithPassiveEQProcessor& p, double fs, int block, int numChannels = 2)
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
    std::vector<std::vector<float>> run (SongsmithPassiveEQProcessor& p, std::vector<std::vector<float>> in, int block)
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

    juce::MemoryBlock stateOf (SongsmithPassiveEQProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    const char* const soundIds[] = { Peq::lowFrequencyId, Peq::lowBoostId, Peq::lowCutId, Peq::highBoostFrequencyId,
                                     Peq::highBoostId, Peq::highBandwidthId, Peq::highCutFrequencyId, Peq::highCutId };
}

//==============================================================================
class PassiveEQPluginTests final : public juce::UnitTest
{
public:
    PassiveEQPluginTests() : juce::UnitTest ("Songsmith Passive EQ plugin", "PassiveEQ") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Passive EQ"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            expectEquals (juce::String (PEQ_MANUFACTURER_CODE), juce::String ("Tnyw"), "shared with the whole family");
            expectEquals (juce::String (PEQ_PLUGIN_CODE), juce::String ("Peq1"), "this plugin's own code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1", "Opt1" })
                expect (juce::String (PEQ_PLUGIN_CODE) != juce::String (other), juce::String ("differs from ") + other);
        }

        beginTest ("Parameter IDs, ranges and defaults are stable");
        {
            SongsmithPassiveEQProcessor p;
            struct Row { const char* id; float lo, hi, def; const char* name; };
            const Row rows[] = {
                { "low_frequency", 0.0f, 3.0f, 2.0f, "Low Frequency" },
                { "low_boost", 0.0f, 13.5f, 0.0f, "Low Boost" },
                { "low_cut", 0.0f, 17.5f, 0.0f, "Low Cut" },
                { "high_boost_frequency", 0.0f, 6.0f, 3.0f, "High Boost Frequency" },
                { "high_boost", 0.0f, 18.0f, 0.0f, "High Boost" },
                { "high_bandwidth", 0.0f, 10.0f, 5.0f, "High Boost Bandwidth" },
                { "high_cut_frequency", 0.0f, 2.0f, 1.0f, "High Cut Frequency" },
                { "high_cut", 0.0f, 16.0f, 0.0f, "High Cut" },
                { "bypass", 0.0f, 1.0f, 0.0f, "Bypass" },
            };
            expectEquals (p.getParameters().size(), (int) std::size (rows), "nine parameters, no more");
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
            expectEquals (Peq::frequencyNames (SongsmithPassive::lowFrequencies).joinIntoString ("/"), juce::String ("20 Hz/30 Hz/60 Hz/100 Hz"));
            expectEquals (Peq::frequencyNames (SongsmithPassive::highBoostFrequencies).joinIntoString ("/"),
                          juce::String ("3 kHz/4 kHz/5 kHz/8 kHz/10 kHz/12 kHz/16 kHz"));
            expectEquals (Peq::frequencyNames (SongsmithPassive::highCutFrequencies).joinIntoString ("/"), juce::String ("5 kHz/10 kHz/20 kHz"));
            expect (p.getBypassParameter() == p.apvts.getParameter (Peq::bypassId), "the host's own Bypass is the plugin's Bypass");
        }

        beginTest ("Buses: stereo and mono are supported, nothing else");
        {
            SongsmithPassiveEQProcessor p;
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

        beginTest ("A new instance leaves the sound exactly as it was, at any block size");
        for (int block : { 1, 64, 480, 4096 })
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, block);
            const auto signal = noise (9600, 0.5f, 3);
            const auto out = run (p, { signal, signal }, block);
            expect (out[0] == signal && out[1] == signal, "bit-identical at a block size of " + juce::String (block));
        }

        beginTest ("The sound follows the parameters, including host automation, at any block size");
        {
            const auto signal = noise (96000, 0.3f, 4);
            std::vector<float> reference;
            for (int block : { 64, 480, 4096 })
            {
                SongsmithPassiveEQProcessor p;
                prepare (p, 48000.0, block);
                setParam (p, Peq::lowBoostId, 10.0f);
                setParam (p, Peq::highBoostId, 8.0f);
                setParam (p, Peq::highCutId, 6.0f);
                const auto out = run (p, { signal, signal }, block);
                expect (out[0] == out[1], "identical channels stay identical");
                expect (out[0] != signal, "the sound is changed");
                if (reference.empty())
                    reference = out[0];
                else
                    expect (out[0] == reference, "the same result at a block size of " + juce::String (block));
            }

            // Each control does what its name says, as the engine's own response predicts.
            struct Case { const char* id; float value; double hz; const char* what; };
            const Case cases[] = { { Peq::lowBoostId, 12.0f, 60.0, "Low Boost raises 60 Hz" },
                                   { Peq::lowCutId, 12.0f, 200.0, "Low Cut lowers the low middle" },
                                   { Peq::highBoostId, 12.0f, 8000.0, "High Boost raises 8 kHz" },
                                   { Peq::highCutId, 12.0f, 15000.0, "High Cut lowers 15 kHz" } };
            for (const auto& c : cases)
            {
                SongsmithPassiveEQProcessor p;
                prepare (p, 48000.0, 480);
                setParam (p, c.id, c.value);
                const auto tone = sine (48000, 0.1f, c.hz, 48000.0);
                const auto out = run (p, { tone, tone }, 480);
                const float measured = 20.0f * std::log10 (rms (out[0], 24000) / rms (tone, 24000));

                SongsmithPassive::Engine e;
                e.prepare (48000.0);
                e.setParams (p.getEngineParams());
                expectWithinAbsoluteError (measured, e.responseDb (c.hz), 0.3f, c.what);
                expect ((c.id == Peq::lowBoostId || c.id == Peq::highBoostId) ? measured > 3.0f : measured < -3.0f, c.what);
            }
        }

        beginTest ("The frequency switches move the boost, with the engine's own response");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Peq::highBoostId, 12.0f);
            float atFrequency[2] {};
            for (int i = 0; i < 2; ++i)
            {
                setParam (p, Peq::highBoostFrequencyId, i == 0 ? 3.0f : 6.0f);   // 8 kHz, then 16 kHz
                p.reset();
                const auto tone = sine (48000, 0.1f, 16000.0, 48000.0);
                const auto out = run (p, { tone, tone }, 480);
                atFrequency[i] = 20.0f * std::log10 (rms (out[0], 24000) / rms (tone, 24000));
            }
            expect (atFrequency[1] > atFrequency[0] + 3.0f, "the 16 kHz setting lifts 16 kHz more than the 8 kHz setting does");
        }

        beginTest ("Mono works");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480, 1);
            setParam (p, Peq::lowBoostId, 10.0f);
            setParam (p, Peq::highBoostId, 10.0f);
            const auto out = run (p, { noise (48000, 0.5f, 9) }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s); }));
        }

        beginTest ("Bypass: the dry sound passes bit-exact, fades in and out without a jump, and never glitches on return");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Peq::lowBoostId, 13.5f);
            setParam (p, Peq::highBoostId, 18.0f);
            const auto signal = noise (96000, 0.5f, 13);
            setParam (p, Peq::bypassId, 1.0f);
            auto out = run (p, { signal, signal }, 480);
            expect (std::equal (signal.begin() + 4800, signal.end(), out[0].begin() + 4800), "bit-exact once the short fade is over");

            float biggestStep = 0.0f;
            for (size_t i = 1; i < 4800; ++i)
                biggestStep = std::max (biggestStep, std::abs (out[0][i] - out[0][i - 1] - (signal[i] - signal[i - 1])));
            expect (biggestStep < 2.0f, "no click at the start of the fade");

            setParam (p, Peq::bypassId, 0.0f);
            out = run (p, { signal, signal }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 20.0f; }), "returning is clean");
            expect (out[0] != signal, "and the processing is back");
        }

        beginTest ("Saved state round-trips every parameter and the editor's own settings");
        {
            SongsmithPassiveEQProcessor a;
            setParam (a, Peq::lowFrequencyId, 3.0f);
            setParam (a, Peq::lowBoostId, 7.3f);
            setParam (a, Peq::lowCutId, 12.5f);
            setParam (a, Peq::highBoostFrequencyId, 6.0f);
            setParam (a, Peq::highBoostId, 9.9f);
            setParam (a, Peq::highBandwidthId, 8.2f);
            setParam (a, Peq::highCutFrequencyId, 0.0f);
            setParam (a, Peq::highCutId, 4.4f);
            setParam (a, Peq::bypassId, 1.0f);
            a.editorWidth = 1111;
            a.presets.currentName = "My Voice";
            const auto block = stateOf (a);

            SongsmithPassiveEQProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (auto* id : soundIds)
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-4f, juce::String (id));
            expectEquals (getParam (b, Peq::bypassId), 1.0f);
            expectEquals (b.editorWidth.load(), 1111);
            expectEquals (b.presets.currentName, juce::String ("My Voice"));

            auto xml = a.createState().createXml();
            expect (xml->getChildByName ("UI") != nullptr, "the editor settings are saved");
            expectEquals (a.createState().getProperty ("stateVersion", 0).operator int(), SongsmithPassiveEQProcessor::stateVersion);
        }

        beginTest ("Older, partial, damaged and out-of-range saved state loads safely");
        {
            SongsmithPassiveEQProcessor p;
            setParam (p, Peq::lowBoostId, 9.0f);
            setParam (p, Peq::highCutFrequencyId, 2.0f);

            // Only High Cut saved: everything else returns to its default, as a session from before it existed would.
            juce::ValueTree partial (p.apvts.state.getType());
            partial.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::highCutId }, { "value", 9.0f } }), nullptr);
            p.restoreFromValueTree (partial);
            expectWithinAbsoluteError (getParam (p, Peq::highCutId), 9.0f, 1.0e-4f);
            expectEquals (getParam (p, Peq::lowBoostId), 0.0f, "missing: default");
            expectEquals (getParam (p, Peq::highCutFrequencyId), (float) Peq::defaultHighCutFrequency, "missing: default switch");
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::defaultWidth, "missing: default size");

            // Out-of-range, non-finite and unknown values.
            juce::ValueTree odd (p.apvts.state.getType());
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::highBoostId }, { "value", 999.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::lowCutId }, { "value", std::numeric_limits<float>::quiet_NaN() } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Peq::lowFrequencyId }, { "value", 77.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "from_the_future" }, { "value", 1.0f } }), nullptr);
            juce::ValueTree ui ("UI");
            ui.setProperty ("editorWidth", 99999, nullptr);
            odd.appendChild (ui, nullptr);
            p.restoreFromValueTree (odd);
            expectEquals (getParam (p, Peq::highBoostId), SongsmithPassive::maxHighBoostDb, "clamped");
            expectEquals (getParam (p, Peq::lowCutId), 0.0f, "not a number: default");
            expectEquals (getParam (p, Peq::lowFrequencyId), 3.0f, "an unknown switch position is the nearest real one");
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::maxWidth);

            const char junk[] = "this is not a saved session";
            p.setStateInformation (junk, (int) sizeof (junk));
            p.setStateInformation (nullptr, 0);
            expectEquals (p.editorWidth.load(), Peq::EditorLayout::maxWidth, "junk is ignored");
        }

        beginTest ("Undo and Redo work for the musician's changes, and a loaded session starts with a clean history");
        {
            SongsmithPassiveEQProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Peq::lowBoostId, Peq::highBoostId, "Passive EQ");
        }

        beginTest ("The starting points set every sound parameter, leave Bypass alone, and are the host's programs");
        {
            const auto& list = Peq::Presets::factory();
            expectEquals ((int) list.size(), 6);
            SongsmithPassiveEQProcessor p;
            expectEquals (p.getNumPrograms(), 6);
            setParam (p, Peq::bypassId, 1.0f);
            for (int i = 0; i < p.getNumPrograms(); ++i)
            {
                p.setCurrentProgram (i);
                const auto& f = list[(size_t) i];
                expectEquals (p.getProgramName (i), juce::String (f.name));
                expectEquals (getParam (p, Peq::lowFrequencyId), (float) f.lowFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), f.lowBoost, 1.0e-3f, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::lowCutId), f.lowCut, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::highBoostFrequencyId), (float) f.highBoostFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highBoostId), f.highBoost, 1.0e-3f, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highBandwidthId), f.bandwidth, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::highCutFrequencyId), (float) f.highCutFrequency, f.name);
                expectWithinAbsoluteError (getParam (p, Peq::highCutId), f.highCut, 1.0e-3f, f.name);
                expectEquals (getParam (p, Peq::bypassId), 1.0f, "Bypass is not a preset's business");
                expectEquals (p.getCurrentProgram(), i);
                expectEquals (p.presets.currentName, juce::String (f.name));

                // Gentle: no starting point lifts or cuts more than 6 dB anywhere in the audible range.
                SongsmithPassive::Engine e;
                e.prepare (48000.0);
                e.setParams (p.getEngineParams());
                for (double hz = 20.0; hz < 20000.0; hz *= 1.1)
                    expect (std::abs (e.responseDb (hz)) <= 6.0f, juce::String (f.name) + " stays gentle at " + juce::String (hz) + " Hz");
            }
            expectEquals (p.getProgramName (99), juce::String());

            // Names say what they are for: Title Case, no model names, no tutorial titles, no playful names.
            for (const auto& f : list)
            {
                const juce::String presetName (f.name);
                expect (presetName.length() >= 8 && presetName.length() <= 30, presetName + " is short and clear");
                for (const auto& word : juce::StringArray::fromTokens (presetName, " ", ""))
                    expect (juce::CharacterFunctions::isUpperCase (word[0]), presetName + " is Title Case");
                for (const char* banned : { "Pultec", "EQP", "Gilder", "Magic", "Vintage" })
                    expect (! presetName.containsIgnoreCase (banned), presetName + " keeps model and educator names out of the title");
                expect (juce::String (f.hint).isNotEmpty() && ! juce::String (f.hint).containsIgnoreCase ("Gilder")
                            && ! juce::String (f.hint).containsIgnoreCase ("Pultec"),
                        presetName + " has a listening cue that stands on its own");
            }
        }

        beginTest ("Your own presets: save, list, reload, replace, and refuse the starting points' names and damaged files");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("peq-preset-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            Peq::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithPassiveEQProcessor p;
                const auto folder = p.presets.getUserFolder();
                expect (folder.getFullPathName().contains ("Songsmith Passive EQ") && folder.getFullPathName().endsWith ("Presets"),
                        "a folder of their own: " + folder.getFullPathName());
                expect (p.presets.getUserPresetFiles().isEmpty());

                setParam (p, Peq::lowFrequencyId, 1.0f);
                setParam (p, Peq::lowBoostId, 6.0f);
                setParam (p, Peq::highBoostFrequencyId, 5.0f);
                setParam (p, Peq::highBoostId, 7.0f);
                setParam (p, Peq::highBandwidthId, 2.5f);
                setParam (p, Peq::highCutFrequencyId, 2.0f);
                setParam (p, Peq::highCutId, 3.0f);
                juce::String error;
                expect (p.presets.saveAs ("  My: Vocal?  ", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1);
                expect (p.presets.userPresetExists ("My Vocal"), "the name is cleaned the same way when checking");
                expectEquals (p.presets.currentName, juce::String ("My Vocal"));

                float saved[8];
                for (int i = 0; i < 8; ++i)
                    saved[i] = getParam (p, soundIds[i]);
                for (auto* id : soundIds)
                    setParam (p, id, 0.0f);
                setParam (p, Peq::bypassId, 1.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error), error);
                for (int i = 0; i < 8; ++i)
                    expectWithinAbsoluteError (getParam (p, soundIds[i]), saved[i], 1.0e-3f, soundIds[i]);
                expectEquals (getParam (p, Peq::bypassId), 1.0f, "a preset never touches Bypass");

                expect (! p.presets.saveAs ("", error) && error.isNotEmpty(), "an empty name is refused");
                for (const auto& f : Peq::Presets::factory())
                    expect (! p.presets.saveAs (f.name, error) && error.contains ("starting point"), juce::String (f.name) + " cannot be replaced");
                expect (! p.presets.saveAs (juce::String (Peq::Presets::factory()[0].name).toUpperCase(), error), "in any capitals");

                // A replaced preset is the new settings.
                setParam (p, Peq::lowBoostId, 2.0f);
                expect (p.presets.saveAs ("My Vocal", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1, "replaced, not duplicated");
                setParam (p, Peq::lowBoostId, 11.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error));
                expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), 2.0f, 1.0e-3f);

                // Damaged files.
                const auto bad = folder.getChildFile ("bad.sspreset");
                bad.replaceWithText ("this is not xml");
                expect (! p.presets.loadFile (bad, error) && error.contains ("isn't a"), "not xml");
                bad.replaceWithText ("<SomethingElse low_boost=\"5\"/>");
                expect (! p.presets.loadFile (bad, error), "the wrong kind of file");
                expect (! p.presets.loadFile (folder.getChildFile ("missing.sspreset"), error), "a missing file");
                bad.replaceWithText ("<SongsmithPassiveEQPreset low_boost=\"9999\" high_cut=\"nan\"/>");
                expect (p.presets.loadFile (bad, error), error);
                expectEquals (getParam (p, Peq::lowBoostId), SongsmithPassive::maxLowBoostDb, "out of range is clamped");
                expectEquals (getParam (p, Peq::highCutId), 0.0f, "not a number is the default");
            }
            Peq::Presets::setDefaultFolderRootForTesting (juce::File());
            root.deleteRecursively();
        }
    }
};

//==============================================================================
class PassiveEQEditorTests final : public juce::UnitTest
{
public:
    PassiveEQEditorTests() : juce::UnitTest ("Songsmith Passive EQ editor", "PassiveEQ") {}

    static std::unique_ptr<SongsmithPassiveEQEditor> make (SongsmithPassiveEQProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithPassiveEQEditor> (dynamic_cast<SongsmithPassiveEQEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        beginTest ("The editor opens at the default, minimum and maximum sizes, keeps its shape and remembers its size");
        for (int width : { Peq::EditorLayout::defaultWidth, Peq::EditorLayout::minWidth, Peq::EditorLayout::maxWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), Peq::EditorLayout::heightFor (width));
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) Peq::EditorLayout::designWidth, 1.0e-5f);
        }
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, 1000);
            ed->setSize (1200, Peq::EditorLayout::heightFor (1200));
            expectEquals (p.editorWidth.load(), 1200, "the size is saved with the session");
            expectEquals (Peq::EditorLayout::constrainWidth (1), Peq::EditorLayout::minWidth);
            expectEquals (Peq::EditorLayout::constrainWidth (99999), Peq::EditorLayout::maxWidth);
            expectEquals (Peq::EditorLayout::minWidth * 4, Peq::EditorLayout::designWidth * 3, "75 % at the smallest");
            expectEquals (Peq::EditorLayout::maxWidth, Peq::EditorLayout::designWidth * 3, "300 % at the largest");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass, and Bypass is top-right in the suite red");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Passive EQ");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (Peq::bypassId), "Passive EQ");
            expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), "the title does not overlap the preset selector");
            expect (ed->getTitleHint().getBounds() == ed->getTitleBounds(), "the hint sits exactly over the title");
        }

        beginTest ("Layout: the curve on top, Low, High Boost and High Cut groups left to right, each frequency switch above the knobs it belongs to");
        for (int width : { Peq::EditorLayout::minWidth, Peq::EditorLayout::defaultWidth, Peq::EditorLayout::maxWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            struct Part { juce::Component* c; juce::Rectangle<int> group; };
            const auto low = ed->getLowGroup(), hb = ed->getHighBoostGroup(), hc = ed->getHighCutGroup();
            const Part parts[] = { { &ed->getLowSwitch(), low }, { &ed->getLowBoostKnob(), low }, { &ed->getLowCutKnob(), low },
                                   { &ed->getHighBoostSwitch(), hb }, { &ed->getHighBoostKnob(), hb }, { &ed->getBandwidthKnob(), hb },
                                   { &ed->getHighCutSwitch(), hc }, { &ed->getHighCutKnob(), hc } };
            auto* content = ed->getCurve().getParentComponent();
            expect (content->getLocalBounds().contains (ed->getCurve().getBounds()), context + ": the curve is inside");
            expect (ed->getCurve().getBottom() <= low.getY(), context + ": the curve is above the groups");
            expect (low.getRight() <= hb.getX() && hb.getRight() <= hc.getX(), context + ": Low, High Boost, High Cut from left to right");
            expect (hc.getBottom() <= Peq::EditorLayout::contentHeight, context + ": the groups are above the footer strip");

            for (size_t i = 0; i < std::size (parts); ++i)
            {
                expect (parts[i].group.contains (parts[i].c->getBounds()), context + ": control " + juce::String ((int) i) + " sits inside its own group");
                for (size_t j = i + 1; j < std::size (parts); ++j)
                    expect (! parts[i].c->getBounds().intersects (parts[j].c->getBounds()), context + ": no overlap");
            }
            for (auto* knob : { &ed->getLowBoostKnob(), &ed->getLowCutKnob() })
                expect (ed->getLowSwitch().getBottom() <= knob->getY(), context + ": the low frequency switch is above its knobs");
            for (auto* knob : { &ed->getHighBoostKnob(), &ed->getBandwidthKnob() })
                expect (ed->getHighBoostSwitch().getBottom() <= knob->getY(), context + ": the high boost switch is above its knobs");
            expect (ed->getHighCutSwitch().getBottom() <= ed->getHighCutKnob().getY(), context + ": the high cut switch is above its knob");
            expect (ed->getLowBoostKnob().getX() < ed->getLowCutKnob().getX(), context + ": Boost then Cut");
            expect (ed->getHighBoostKnob().getX() < ed->getBandwidthKnob().getX(), context + ": High Boost then Bandwidth");
            expect (ed->getBypassButton().getRight() <= content->getWidth(), context + ": Bypass is inside");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("The frequency switches light up for the choice, from a click, automation and a restored session");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getLowSwitch().getNumButtons(), 4);
            expectEquals (ed->getHighBoostSwitch().getNumButtons(), 7);
            expectEquals (ed->getHighCutSwitch().getNumButtons(), 3);
            expect (ed->getLowSwitch().getButton (2).getToggleState() && ! ed->getLowSwitch().getButton (0).getToggleState(), "60 Hz at first");
            expect (ed->getHighBoostSwitch().getButton (3).getToggleState(), "8 kHz at first");
            expect (ed->getHighCutSwitch().getButton (1).getToggleState(), "10 kHz at first");

            ed->getLowSwitch().getButton (0).onClick();
            expectEquals (getParam (p, Peq::lowFrequencyId), 0.0f, "a click sets the parameter");
            expect (ed->getLowSwitch().getButton (0).getToggleState() && ! ed->getLowSwitch().getButton (2).getToggleState());
            setParam (p, Peq::lowFrequencyId, 3.0f);
            expect (ed->getLowSwitch().getButton (3).getToggleState(), "automation moves the lit button");
            ed->getHighBoostSwitch().getButton (6).onClick();
            expectEquals (getParam (p, Peq::highBoostFrequencyId), 6.0f);
            ed->getHighCutSwitch().getButton (2).onClick();
            expectEquals (getParam (p, Peq::highCutFrequencyId), 2.0f);

            SongsmithPassiveEQProcessor q;
            setParam (q, Peq::highBoostFrequencyId, 0.0f);
            auto e2 = make (q, Peq::EditorLayout::defaultWidth);
            expect (e2->getHighBoostSwitch().getButton (0).getToggleState(), "a restored session opens lit");
        }

        beginTest ("The knobs show and take values, and double-click returns them to the default");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("0.0 dB"));
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("5.0"));
            ed->getLowBoostKnob().slider.setValue (6.5, juce::sendNotificationSync);
            ed->getLowCutKnob().slider.setValue (3.0, juce::sendNotificationSync);
            ed->getHighBoostKnob().slider.setValue (12.0, juce::sendNotificationSync);
            ed->getHighCutKnob().slider.setValue (4.0, juce::sendNotificationSync);
            ed->getBandwidthKnob().slider.setValue (10.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), 6.5f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::lowCutId), 3.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::highBoostId), 12.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Peq::highCutId), 4.0f, 1.0e-3f);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("+6.5 dB"));
            expectEquals (ed->getLowCutKnob().getValueText(), juce::String ("-3.0 dB"), "a cut reads as minus");
            expectEquals (ed->getHighBoostKnob().getValueText(), juce::String ("+12.0 dB"));
            expectEquals (ed->getHighCutKnob().getValueText(), juce::String ("-4.0 dB"));
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("Narrow"));
            setParam (p, Peq::highBandwidthId, 0.0f);
            expectEquals (ed->getBandwidthKnob().getValueText(), juce::String ("Broad"));
            setParam (p, Peq::lowBoostId, 0.0f);
            expectEquals (ed->getLowBoostKnob().getValueText(), juce::String ("0.0 dB"));
        }

        beginTest ("The response curve is the engine's own response for the settings, and follows them");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            ed->refreshCurve();
            for (double hz : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
                expectWithinAbsoluteError (ed->getCurve().responseDb ((float) hz), 0.0f, 1.0e-3f, "flat when every amount is zero");

            setParam (p, Peq::lowBoostId, 10.0f);
            setParam (p, Peq::highBoostId, 10.0f);
            ed->refreshCurve();
            SongsmithPassive::Engine e;
            e.prepare (48000.0);
            e.setParams (p.getEngineParams());
            for (double hz : { 40.0, 60.0, 500.0, 8000.0 })
                expectWithinAbsoluteError (ed->getCurve().responseDb ((float) hz), e.responseDb (hz), 1.0e-3f, juce::String (hz) + " Hz");
            expect (ed->getCurve().responseDb (60.0f) > 5.0f && ed->getCurve().responseDb (8000.0f) > 5.0f, "the boosts show");

            using Curve = Peq::UI::ResponseCurve;
            const auto plot = ed->getCurve().plotArea();
            expect (plot.getWidth() > 400.0f && plot.getHeight() > 60.0f, "there is room to draw");
            expect (Curve::xFor (20.0f, plot) < Curve::xFor (1000.0f, plot) && Curve::xFor (1000.0f, plot) < Curve::xFor (20000.0f, plot), "frequency runs left to right");
            expect (Curve::yFor (10.0f, plot) < Curve::yFor (0.0f, plot) && Curve::yFor (-10.0f, plot) > Curve::yFor (0.0f, plot), "boosts are drawn above the 0 dB line");
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { Peq::EditorLayout::defaultWidth, Peq::EditorLayout::minWidth })
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, width);
            const auto context = "Passive EQ at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : Peq::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 30, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Low Boost", "Low Cut", "High Boost", "High Cut", "High Boost Bandwidth", "Low Frequency 60 Hz", "High Boost Frequency 16 kHz",
                             "High Cut Frequency 20 kHz", "20 Hz", "100 Hz", "3 kHz", "16 kHz", "5 kHz", "Bypass", "Undo", "Redo" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Passive EQ");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Passive EQ editor");
        }

        beginTest ("The footer shows the version and latency and the Processing Load switch, in the shared strip");
        {
            SongsmithPassiveEQProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            expectEquals (ed->getDiagnosticsBounds().getY(), Peq::EditorLayout::contentHeight);
            expectEquals (ed->getDiagnosticsBounds().getHeight(), Peq::EditorLayout::footerHeight);
            Songsmith::Diagnostics::Testing::checkVersionBinding (*this, p, "Passive EQ");
            Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, p, "Passive EQ");
            Songsmith::Diagnostics::Testing::checkFooterLayout (*this, p, "Passive EQ");
        }

        beginTest ("The preset menu lists the starting points, your presets, Save and the folder, and loading a starting point is one Undo step");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            int starting = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemId (i) >= 1 && box.getItemId (i) <= 6)
                    ++starting;
            expectEquals (starting, 6, "six starting points");
            box.setSelectedId (1, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Peq::lowBoostId), Peq::Presets::factory()[0].lowBoost, 1.0e-3f);
            expect (ed->getUndoButton().isEnabled(), "Undo is available after loading");
            expectEquals (box.getText(), juce::String (Peq::Presets::factory()[0].name));
            expect (box.getTooltip().contains (Peq::Presets::factory()[0].hint), "the listening cue is on hover");
            expect (ed->getLowSwitch().getButton (Peq::Presets::factory()[0].lowFrequency).getToggleState(), "the switch follows the preset");
            p.history.undo();
            expectEquals (getParam (p, Peq::lowBoostId), 0.0f, "one Undo restores everything the preset changed");
            expectEquals (getParam (p, Peq::lowCutId), 0.0f);
            expectEquals (getParam (p, Peq::lowFrequencyId), (float) Peq::defaultLowFrequency);
        }

        beginTest ("The faceplate is blue and textured, not flat, and the title bar keeps the suite's neutral look");
        {
            SongsmithPassiveEQProcessor p;
            auto ed = make (p, Peq::EditorLayout::defaultWidth);
            const auto image = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            // The strip of faceplate left of the first group, between the screws, over a tall stretch.
            int bluer = 0, samples = 0;
            std::set<juce::uint32> distinct;
            for (int y = 200; y < 380; ++y)
                for (int x = 9; x < 13; ++x)
                {
                    const auto c = image.getPixelAt (x, y);
                    ++samples;
                    bluer += c.getBlue() > c.getRed() + 30 && c.getBlue() > c.getGreen() + 10 ? 1 : 0;
                    distinct.insert (c.getARGB());
                }
            expect (bluer > samples * 9 / 10, "the faceplate is clearly blue: " + juce::String (bluer) + " of " + juce::String (samples));
            expect (distinct.size() > 8, "with a grain and a shaded gradient, not a flat fill: " + juce::String ((int) distinct.size()) + " colours");
            const auto titleBar = image.getPixelAt (700, 10);
            expect (std::abs (titleBar.getBlue() - titleBar.getRed()) < 20, "the suite's title bar stays neutral");
        }
    }
};

static PassiveEQPluginTests passiveEQPluginTests;
static PassiveEQEditorTests passiveEQEditorTests;

//==============================================================================
static SongsmithUITests sharedUITests { "PassiveEQ" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "PassiveEQ" };
static SongsmithGuidanceTests sharedGuidanceTests { "PassiveEQ" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// PassiveEQTests --screenshot <file.png> [--scale 2] [--preset N]: renders the editor with a starting point
// loaded to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale, int preset)
{
    SongsmithPassiveEQProcessor p;
    p.setCurrentProgram (preset);
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithPassiveEQEditor> editor (dynamic_cast<SongsmithPassiveEQEditor*> (p.createEditor()));
    editor->refreshCurve();

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
                    preset = juce::jlimit (0, (int) Peq::Presets::factory().size() - 1, juce::String (argv[j + 1]).getIntValue());
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, preset);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("PassiveEQ");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
