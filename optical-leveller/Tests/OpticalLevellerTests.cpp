// Songsmith Optical Leveller test runner: the plugin (identity, parameters, state, bypass, presets, editor)
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
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithOpticalLevellerProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithOpticalLevellerProcessor& p, const char* id)
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

    void prepare (SongsmithOpticalLevellerProcessor& p, double fs, int block, int numChannels = 2)
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
    std::vector<std::vector<float>> run (SongsmithOpticalLevellerProcessor& p, std::vector<std::vector<float>> in, int block)
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

    juce::MemoryBlock stateOf (SongsmithOpticalLevellerProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }
}

//==============================================================================
class OpticalLevellerPluginTests final : public juce::UnitTest
{
public:
    OpticalLevellerPluginTests() : juce::UnitTest ("Songsmith Optical Leveller plugin", "OpticalLeveller") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Optical Leveller"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            expectEquals (juce::String (OPT_MANUFACTURER_CODE), juce::String ("Tnyw"), "shared with the whole family");
            expectEquals (juce::String (OPT_PLUGIN_CODE), juce::String ("Opt1"), "this plugin's own code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1" })
                expect (juce::String (OPT_PLUGIN_CODE) != juce::String (other), juce::String ("differs from ") + other);
        }

        beginTest ("Parameter IDs, ranges and defaults are stable");
        {
            SongsmithOpticalLevellerProcessor p;
            struct Row { const char* id; float lo, hi, def; const char* name; };
            const Row rows[] = {
                { "peak_reduction", 0.0f, 100.0f, 40.0f, "Peak Reduction" },
                { "gain", 0.0f, 40.0f, 1.0f, "Gain" },
                { "mode", 0.0f, 1.0f, 0.0f, "Mode" },
                { "bypass", 0.0f, 1.0f, 0.0f, "Bypass" },
            };
            expectEquals (p.getParameters().size(), (int) std::size (rows), "four parameters, no more");
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
            expectEquals (Opt::modeNames().joinIntoString ("/"), juce::String ("Compress/Limit"));
            expect (p.getBypassParameter() == p.apvts.getParameter (Opt::bypassId), "the host's own Bypass is the plugin's Bypass");
        }

        beginTest ("Buses: stereo and mono are supported, nothing else");
        {
            SongsmithOpticalLevellerProcessor p;
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
                SongsmithOpticalLevellerProcessor p;
                prepare (p, 48000.0, block);
                setParam (p, Opt::peakReductionId, 80.0f);
                setParam (p, Opt::gainId, 0.0f);
                const auto out = run (p, { signal, signal }, block);
                expect (rms (out[0], 24000) < 0.85f * rms (signal, 24000), "Peak Reduction turns the loud sound down");
                expect (out[0] == out[1], "identical channels stay identical");
                if (reference.empty())
                    reference = out[0];
                else
                    expect (out[0] == reference, "the same result at a block size of " + juce::String (block));
            }

            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Opt::peakReductionId, 0.0f);
            setParam (p, Opt::gainId, 0.0f);
            const auto quietSignal = noise (96000, 0.1f, 4);   // always below the level where Peak Reduction 0 starts to act
            const auto off = run (p, { quietSignal, quietSignal }, 480);
            expect (std::equal (quietSignal.begin() + 4800, quietSignal.end(), off[0].begin() + 4800), "with Peak Reduction 0 and Gain 0 the sound is untouched (once Gain has settled)");
            setParam (p, Opt::gainId, 6.0f);
            p.reset();
            const auto louder = run (p, { signal, signal }, 480);
            expectWithinAbsoluteError (rms (louder[0], 24000) / rms (signal, 24000), std::pow (10.0f, 6.0f / 20.0f), 0.05f, "Gain adds 6 dB");
        }

        beginTest ("Compress and Limit sound different: Limit reduces more");
        {
            const auto signal = noise (96000, 0.5f, 5);
            float level[2] {};
            for (int mode = 0; mode < 2; ++mode)
            {
                SongsmithOpticalLevellerProcessor p;
                prepare (p, 48000.0, 480);
                setParam (p, Opt::peakReductionId, 70.0f);
                setParam (p, Opt::gainId, 0.0f);
                setParam (p, Opt::modeId, (float) mode);
                level[mode] = rms (run (p, { signal, signal }, 480)[0], 24000);
            }
            expect (level[1] < level[0], "Limit is quieter than Compress for the same settings");
        }

        beginTest ("Mono works");
        {
            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480, 1);
            setParam (p, Opt::peakReductionId, 80.0f);
            const auto out = run (p, { noise (48000, 0.5f, 9) }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s); }));
        }

        beginTest ("Bypass: the dry sound passes bit-exact, fades in and out without a jump, and never glitches on return");
        {
            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Opt::peakReductionId, 90.0f);
            setParam (p, Opt::gainId, 10.0f);
            const auto signal = noise (96000, 0.5f, 13);
            setParam (p, Opt::bypassId, 1.0f);
            auto out = run (p, { signal, signal }, 480);
            expect (std::equal (signal.begin() + 4800, signal.end(), out[0].begin() + 4800), "bit-exact once the short fade is over");

            float biggestStep = 0.0f;
            for (size_t i = 1; i < 4800; ++i)
                biggestStep = std::max (biggestStep, std::abs (out[0][i] - out[0][i - 1] - (signal[i] - signal[i - 1])));
            expect (biggestStep < 2.0f, "no click at the start of the fade");

            setParam (p, Opt::bypassId, 0.0f);
            out = run (p, { signal, signal }, 480);
            expect (std::all_of (out[0].begin(), out[0].end(), [] (float s) { return std::isfinite (s) && std::abs (s) < 20.0f; }), "returning is clean");
            expect (out[0] != signal, "and the processing is back");
        }

        beginTest ("Saved state round-trips every parameter and the editor's own settings");
        {
            SongsmithOpticalLevellerProcessor a;
            setParam (a, Opt::peakReductionId, 73.3f);
            setParam (a, Opt::gainId, 12.5f);
            setParam (a, Opt::modeId, 1.0f);
            setParam (a, Opt::bypassId, 1.0f);
            a.editorWidth = 1111;
            a.meterMode = 1;
            a.presets.currentName = "My Voice";
            const auto block = stateOf (a);

            SongsmithOpticalLevellerProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (const char* id : { Opt::peakReductionId, Opt::gainId, Opt::modeId, Opt::bypassId })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-4f, juce::String (id));
            expectEquals (b.editorWidth.load(), 1111);
            expectEquals (b.meterMode.load(), 1);
            expectEquals (b.presets.currentName, juce::String ("My Voice"));

            auto xml = a.createState().createXml();
            expect (xml->getChildByName ("UI") != nullptr, "the editor settings are saved");
            expectEquals (a.createState().getProperty ("stateVersion", 0).operator int(), SongsmithOpticalLevellerProcessor::stateVersion);
        }

        beginTest ("Older, partial, damaged and out-of-range saved state loads safely");
        {
            SongsmithOpticalLevellerProcessor p;
            setParam (p, Opt::peakReductionId, 90.0f);
            setParam (p, Opt::modeId, 1.0f);

            // Only Gain saved: everything else returns to its default, as a session from before it existed would.
            juce::ValueTree partial (p.apvts.state.getType());
            partial.appendChild (juce::ValueTree ("PARAM", { { "id", Opt::gainId }, { "value", 9.0f } }), nullptr);
            p.restoreFromValueTree (partial);
            expectWithinAbsoluteError (getParam (p, Opt::gainId), 9.0f, 1.0e-4f);
            expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), Opt::defaultPeakReduction, 1.0e-4f, "missing: default");
            expectEquals (getParam (p, Opt::modeId), 0.0f, "missing: Compress");
            expectEquals (p.editorWidth.load(), Opt::EditorLayout::defaultWidth, "missing: default size");

            // Out-of-range, non-finite and unknown values.
            juce::ValueTree odd (p.apvts.state.getType());
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Opt::gainId }, { "value", 999.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", Opt::peakReductionId }, { "value", std::numeric_limits<float>::quiet_NaN() } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "from_the_future" }, { "value", 1.0f } }), nullptr);
            juce::ValueTree ui ("UI");
            ui.setProperty ("editorWidth", 99999, nullptr);
            ui.setProperty ("meterMode", 42, nullptr);
            odd.appendChild (ui, nullptr);
            p.restoreFromValueTree (odd);
            expectEquals (getParam (p, Opt::gainId), SongsmithOptical::maxGainDb, "clamped");
            expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), Opt::defaultPeakReduction, 1.0e-4f, "not a number: default");
            expectEquals (p.editorWidth.load(), Opt::EditorLayout::maxWidth);
            expectEquals (p.meterMode.load(), 1);

            const char junk[] = "this is not a saved session";
            p.setStateInformation (junk, (int) sizeof (junk));
            p.setStateInformation (nullptr, 0);
            expectEquals (p.editorWidth.load(), Opt::EditorLayout::maxWidth, "junk is ignored");
        }

        beginTest ("Undo and Redo work for the musician's changes, and a loaded session starts with a clean history");
        {
            SongsmithOpticalLevellerProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Opt::peakReductionId, Opt::gainId, "Optical Leveller");
        }

        beginTest ("The starting points set every sound parameter, leave Bypass alone, and are the host's programs");
        {
            const auto& list = Opt::Presets::factory();
            expectEquals ((int) list.size(), 4);
            SongsmithOpticalLevellerProcessor p;
            expectEquals (p.getNumPrograms(), 4);
            setParam (p, Opt::bypassId, 1.0f);
            for (int i = 0; i < p.getNumPrograms(); ++i)
            {
                p.setCurrentProgram (i);
                const auto& f = list[(size_t) i];
                expectEquals (p.getProgramName (i), juce::String (f.name));
                expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), f.peakReduction, 1.0e-3f, f.name);
                expectWithinAbsoluteError (getParam (p, Opt::gainId), f.gainDb, 1.0e-3f, f.name);
                expectEquals (getParam (p, Opt::modeId), f.limit ? 1.0f : 0.0f, f.name);
                expectEquals (getParam (p, Opt::bypassId), 1.0f, "Bypass is not a preset's business");
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
                for (const char* banned : { "LA-2A", "LA2A", "Teletronix", "Gilder", "Magic", "Vintage" })
                    expect (! presetName.containsIgnoreCase (banned), presetName + " keeps model and educator names out of the title");
                expect (juce::String (f.hint).isNotEmpty() && ! juce::String (f.hint).containsIgnoreCase ("Gilder"),
                        presetName + " has a listening cue that stands on its own");
            }
        }

        beginTest ("Your own presets: save, list, reload, replace, and refuse the starting points' names and damaged files");
        {
            const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("opt-preset-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
            Opt::Presets::setDefaultFolderRootForTesting (root);
            {
                SongsmithOpticalLevellerProcessor p;
                const auto folder = p.presets.getUserFolder();
                expect (folder.getFullPathName().contains ("Songsmith Optical Leveller") && folder.getFullPathName().endsWith ("Presets"),
                        "a folder of its own: " + folder.getFullPathName());
                expect (p.presets.getUserPresetFiles().isEmpty());

                setParam (p, Opt::peakReductionId, 66.0f);
                setParam (p, Opt::gainId, 7.0f);
                setParam (p, Opt::modeId, 1.0f);
                juce::String error;
                expect (p.presets.saveAs ("  My: Vocal?  ", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1);
                expect (p.presets.userPresetExists ("My Vocal"), "the name is cleaned the same way when checking");
                expectEquals (p.presets.currentName, juce::String ("My Vocal"));

                setParam (p, Opt::peakReductionId, 10.0f);
                setParam (p, Opt::gainId, 0.0f);
                setParam (p, Opt::modeId, 0.0f);
                setParam (p, Opt::bypassId, 1.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error), error);
                expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), 66.0f, 1.0e-3f);
                expectWithinAbsoluteError (getParam (p, Opt::gainId), 7.0f, 1.0e-3f);
                expectEquals (getParam (p, Opt::modeId), 1.0f);
                expectEquals (getParam (p, Opt::bypassId), 1.0f, "a preset never touches Bypass");

                expect (! p.presets.saveAs ("", error) && error.isNotEmpty(), "an empty name is refused");
                for (const auto& f : Opt::Presets::factory())
                    expect (! p.presets.saveAs (f.name, error) && error.contains ("starting point"), juce::String (f.name) + " cannot be replaced");
                expect (! p.presets.saveAs (juce::String (Opt::Presets::factory()[0].name).toUpperCase(), error), "in any capitals");

                // A replaced preset is the new settings.
                setParam (p, Opt::peakReductionId, 20.0f);
                expect (p.presets.saveAs ("My Vocal", error), error);
                expectEquals (p.presets.getUserPresetFiles().size(), 1, "replaced, not duplicated");
                setParam (p, Opt::peakReductionId, 80.0f);
                expect (p.presets.loadFile (p.presets.getUserPresetFiles().getFirst(), error));
                expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), 20.0f, 1.0e-3f);

                // Damaged files.
                const auto bad = folder.getChildFile ("bad.sspreset");
                bad.replaceWithText ("this is not xml");
                expect (! p.presets.loadFile (bad, error) && error.contains ("isn't a"), "not xml");
                bad.replaceWithText ("<SomethingElse peak_reduction=\"50\"/>");
                expect (! p.presets.loadFile (bad, error), "the wrong kind of file");
                expect (! p.presets.loadFile (folder.getChildFile ("missing.sspreset"), error), "a missing file");
                bad.replaceWithText ("<SongsmithOpticalLevellerPreset peak_reduction=\"9999\" gain=\"nan\"/>");
                expect (p.presets.loadFile (bad, error), error);
                expectEquals (getParam (p, Opt::peakReductionId), SongsmithOptical::maxPeakReduction, "out of range is clamped");
                expectWithinAbsoluteError (getParam (p, Opt::gainId), Opt::defaultGainDb, 1.0e-3f, "not a number is the default");
            }
            Opt::Presets::setDefaultFolderRootForTesting (juce::File());
            root.deleteRecursively();
        }

        beginTest ("The meter's numbers come from the audio thread and are read once");
        {
            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Opt::peakReductionId, 80.0f);
            run (p, { noise (48000, 0.6f, 21), noise (48000, 0.6f, 22) }, 480);
            const float reduction = p.takeGainReductionDb();
            expect (reduction > 2.0f && reduction < 30.0f, "reduction " + juce::String (reduction));
            expect (p.takeOutputRmsDb() > -40.0f, "output level");
            expectEquals (p.takeGainReductionDb(), 0.0f, "read and reset");
            expectEquals (p.takeOutputRmsDb(), -150.0f);

            setParam (p, Opt::bypassId, 1.0f);
            run (p, { noise (48000, 0.6f, 23), noise (48000, 0.6f, 24) }, 480);
            p.takeGainReductionDb();
            run (p, { noise (4800, 0.6f, 25), noise (4800, 0.6f, 26) }, 480);
            expectEquals (p.takeGainReductionDb(), 0.0f, "no reduction is shown while bypassed");
        }
    }
};

//==============================================================================
class OpticalLevellerEditorTests final : public juce::UnitTest
{
public:
    OpticalLevellerEditorTests() : juce::UnitTest ("Songsmith Optical Leveller editor", "OpticalLeveller") {}

    static std::unique_ptr<SongsmithOpticalLevellerEditor> make (SongsmithOpticalLevellerProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithOpticalLevellerEditor> (dynamic_cast<SongsmithOpticalLevellerEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        beginTest ("The editor opens at the default, minimum and maximum sizes, keeps its shape and remembers its size");
        for (int width : { Opt::EditorLayout::defaultWidth, Opt::EditorLayout::minWidth, Opt::EditorLayout::maxWidth })
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, width);
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), Opt::EditorLayout::heightFor (width));
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) Opt::EditorLayout::designWidth, 1.0e-5f);
        }
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, 1000);
            ed->setSize (1200, Opt::EditorLayout::heightFor (1200));
            expectEquals (p.editorWidth.load(), 1200, "the size is saved with the session");
            expectEquals (Opt::EditorLayout::constrainWidth (1), Opt::EditorLayout::minWidth);
            expectEquals (Opt::EditorLayout::constrainWidth (99999), Opt::EditorLayout::maxWidth);
            expectEquals (Opt::EditorLayout::minWidth * 4, Opt::EditorLayout::designWidth * 3, "75 % at the smallest");
            expectEquals (Opt::EditorLayout::maxWidth, Opt::EditorLayout::designWidth * 3, "300 % at the largest");
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass, and Bypass is top-right in the suite red");
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Optical Leveller");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (Opt::bypassId), "Optical Leveller");
            expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), "the title does not overlap the preset selector");
            expect (ed->getTitleHint().getBounds() == ed->getTitleBounds(), "the hint sits exactly over the title");
        }

        beginTest ("Layout: Gain left of the meter, Peak Reduction right of it, the mode switch under the meter, nothing overlapping");
        for (int width : { Opt::EditorLayout::minWidth, Opt::EditorLayout::defaultWidth, Opt::EditorLayout::maxWidth })
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            std::vector<juce::Component*> parts { &ed->getGainKnob(), &ed->getPeakReductionKnob(), &ed->getMeter(),
                                                  &ed->getModeButton (0), &ed->getModeButton (1),
                                                  &ed->getMeterButton (0), &ed->getMeterButton (1) };
            auto* content = ed->getGainKnob().getParentComponent();
            for (size_t i = 0; i < parts.size(); ++i)
            {
                expect (content->getLocalBounds().contains (parts[i]->getBounds()), context + ": inside the editor");
                expect (parts[i]->getBottom() <= Opt::EditorLayout::contentHeight, context + ": above the footer strip");
                for (size_t j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), context + ": no overlap");
            }
            const auto gain = ed->getGainKnob().getBounds(), peak = ed->getPeakReductionKnob().getBounds(), meter = ed->getMeter().getBounds();
            expect (gain.getRight() <= meter.getX() && meter.getRight() <= peak.getX(), context + ": Gain, meter, Peak Reduction from left to right");
            for (int i = 0; i < 2; ++i)
            {
                expect (ed->getModeButton (i).getY() >= meter.getBottom(), context + ": Compress and Limit are under the meter they act on");
                expect (ed->getMeterButton (i).getY() >= meter.getBottom() && ed->getMeterButton (i).getY() < ed->getModeButton (i).getY(),
                        context + ": the meter's own switches sit directly under it");
            }
            expect (ed->getModeButton (0).getX() < ed->getModeButton (1).getX(), context + ": Compress, then Limit");
            expect (ed->getBypassButton().getRight() <= content->getWidth(), context + ": Bypass is inside");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Compress and Limit light up for the mode, from a click, automation and a restored session");
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            expect (ed->getModeButton (0).getToggleState() && ! ed->getModeButton (1).getToggleState(), "Compress at first");
            ed->getModeButton (1).onClick();
            expectEquals (getParam (p, Opt::modeId), 1.0f, "a click sets the parameter");
            expect (! ed->getModeButton (0).getToggleState() && ed->getModeButton (1).getToggleState());
            setParam (p, Opt::modeId, 0.0f);
            expect (ed->getModeButton (0).getToggleState(), "automation moves the lit button");
            ed->getModeButton (0).onClick();
            expectEquals (getParam (p, Opt::modeId), 0.0f);

            SongsmithOpticalLevellerProcessor q;
            setParam (q, Opt::modeId, 1.0f);
            auto e2 = make (q, Opt::EditorLayout::defaultWidth);
            expect (e2->getModeButton (1).getToggleState(), "a restored Limit session opens lit");
        }

        beginTest ("The knobs show and take values, and double-click returns them to the default");
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            expectEquals (ed->getGainKnob().getValueText(), juce::String ("+1.0 dB"));
            expectEquals (ed->getPeakReductionKnob().getValueText(), juce::String ("40"));
            ed->getPeakReductionKnob().slider.setValue (75.0, juce::sendNotificationSync);
            ed->getGainKnob().slider.setValue (12.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), 75.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, Opt::gainId), 12.0f, 1.0e-3f);
            expectEquals (ed->getPeakReductionKnob().getValueText(), juce::String ("75"));
            expectEquals (ed->getGainKnob().getValueText(), juce::String ("+12.0 dB"));
            setParam (p, Opt::gainId, 0.0f);
            expectEquals (ed->getGainKnob().getValueText(), juce::String ("0.0 dB"));
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { Opt::EditorLayout::defaultWidth, Opt::EditorLayout::minWidth })
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, width);
            const auto context = "Optical Leveller at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : Opt::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 15, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Gain", "Peak Reduction", "Compress", "Limit", "Gain Reduction", "Output", "Bypass", "Undo", "Redo" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Optical Leveller");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Optical Leveller editor");
        }

        beginTest ("The footer shows the version and latency and the Processing Load switch, in the shared strip");
        {
            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            expectEquals (ed->getDiagnosticsBounds().getY(), Opt::EditorLayout::contentHeight);
            expectEquals (ed->getDiagnosticsBounds().getHeight(), Opt::EditorLayout::footerHeight);
            Songsmith::Diagnostics::Testing::checkVersionBinding (*this, p, "Optical Leveller");
            Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, p, "Optical Leveller");
            Songsmith::Diagnostics::Testing::checkFooterLayout (*this, p, "Optical Leveller");
        }

        beginTest ("The meter: the needle's scale, the VU ballistics and the readout wording");
        {
            using namespace Opt::UI;
            expectWithinAbsoluteError (meterReadingVu (0, 6.0f, -30.0f), -6.0f, 1.0e-5f, "Gain Reduction shows minus the reduction");
            expectWithinAbsoluteError (meterReadingVu (0, 0.0f, -30.0f), 0.0f, 1.0e-5f, "none is 0 VU");
            expectWithinAbsoluteError (meterReadingVu (1, 0.0f, -18.0f), 0.0f, 1.0e-5f, "Output: -18 dBFS is 0 VU");
            expectWithinAbsoluteError (meterReadingVu (1, 0.0f, -12.0f), 6.0f > meterMaxVu + 1.0f ? meterMaxVu + 1.0f : 6.0f, 1.0e-5f, "and is clamped at the top");
            expectWithinAbsoluteError (meterReadingVu (0, 90.0f, 0.0f), meterMinVu - 2.0f, 1.0e-5f, "and at the bottom");
            expectWithinAbsoluteError (needleFraction (meterMaxVu), 1.0f, 1.0e-5f, "+3 VU is full scale");
            expect (needleFraction (0.0f) > 0.65f && needleFraction (0.0f) < 0.75f, "0 VU is about 71 % across");

            VuBallistics b;
            b.reset (0.0f);
            float t99 = -1.0f, peak = 0.0f;
            for (int ms = 1; ms <= 600; ++ms)
            {
                const float x = b.advance (1.0f, 0.001f);
                peak = std::max (peak, x);
                if (t99 < 0.0f && x >= 0.99f)
                    t99 = (float) ms;
            }
            expect (t99 > 250.0f && t99 < 350.0f, "reaches 99 % in " + juce::String (t99) + " ms (about 300)");
            expect (peak < 1.03f, "with about 1 % overshoot");

            expectEquals (SongsmithOpticalLevellerEditor::gainReductionText (3.14f), juce::String ("Gain Reduction 3.1 dB"));
            expectEquals (SongsmithOpticalLevellerEditor::gainReductionText (-1.0f), juce::String ("Gain Reduction 0.0 dB"));
        }

        beginTest ("The meter follows the audio, and the meter mode is remembered");
        {
            SongsmithOpticalLevellerProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, Opt::peakReductionId, 85.0f);
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            run (p, { noise (48000, 0.7f, 31), noise (48000, 0.7f, 32) }, 480);
            ed->advanceMeter (0.5f);
            expect (ed->getMeter().ballistics.position() < Opt::UI::needleFraction (-2.0f), "the needle has moved down");

            ed->getMeterButton (1).onClick();
            expectEquals (p.meterMode.load(), 1);
            expect (ed->getMeterButton (1).getToggleState() && ! ed->getMeterButton (0).getToggleState());
            const auto state = stateOf (p);
            SongsmithOpticalLevellerProcessor q;
            q.setStateInformation (state.getData(), (int) state.getSize());
            auto e2 = make (q, Opt::EditorLayout::defaultWidth);
            expect (e2->getMeterButton (1).getToggleState(), "a restored session opens on the same meter");
        }

        beginTest ("The preset menu lists the starting points, your presets, Save and the folder, and loading a starting point is one Undo step");
        {
            SongsmithOpticalLevellerProcessor p;
            auto ed = make (p, Opt::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            int starting = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemId (i) >= 1 && box.getItemId (i) <= 4)
                    ++starting;
            expectEquals (starting, 4, "four starting points");
            box.setSelectedId (1, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), Opt::Presets::factory()[0].peakReduction, 1.0e-3f);
            expect (ed->getUndoButton().isEnabled(), "Undo is available after loading");
            expectEquals (box.getText(), juce::String (Opt::Presets::factory()[0].name));
            expect (box.getTooltip().contains (Opt::Presets::factory()[0].hint), "the listening cue is on hover");
            p.history.undo();
            expectWithinAbsoluteError (getParam (p, Opt::peakReductionId), Opt::defaultPeakReduction, 1.0e-3f, "one Undo restores everything the preset changed");
            expectWithinAbsoluteError (getParam (p, Opt::gainId), Opt::defaultGainDb, 1.0e-3f);
        }
    }
};

static OpticalLevellerPluginTests opticalLevellerPluginTests;
static OpticalLevellerEditorTests opticalLevellerEditorTests;

//==============================================================================
static SongsmithUITests sharedUITests { "OpticalLeveller" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "OpticalLeveller" };
static SongsmithGuidanceTests sharedGuidanceTests { "OpticalLeveller" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// OpticalLevellerTests --screenshot <file.png> [--scale 2]: renders the editor with some levelling on the meter
// to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale, bool limit)
{
    SongsmithOpticalLevellerProcessor p;
    p.setCurrentProgram (limit ? 3 : 0);
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithOpticalLevellerEditor> editor (dynamic_cast<SongsmithOpticalLevellerEditor*> (p.createEditor()));
    // Four seconds of a steady, uneven voice-like tone around -18 dBFS, so the needle has something to show.
    std::vector<float> in (192000);
    for (size_t i = 0; i < in.size(); ++i)
    {
        const double sec = (double) i / 48000.0;
        const double swell = 0.10 + 0.17 * (0.5 + 0.5 * std::sin (sec * 2.1));
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
            bool limit = false;
            for (int j = 1; j < argc; ++j)
            {
                if (juce::String (argv[j]) == "--scale" && j + 1 < argc)
                    scale = juce::jlimit (1.0f, 4.0f, juce::String (argv[j + 1]).getFloatValue());
                if (juce::String (argv[j]) == "--limit")
                    limit = true;
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, limit);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("OpticalLeveller");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
