// Songsmith Analog Desk test runner: the plugin (identity, parameters, state, bypass, presets, editor)
// and the shared Songsmith checks. The engine is tested in DeskEngineTests.cpp.
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
#include "../../shared/Tests/SongsmithMeterColumnTests.h"
#include "../../shared/Tests/SongsmithGuidanceTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithAnalogDeskProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithAnalogDeskProcessor& p, const char* id)
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

    void prepare (SongsmithAnalogDeskProcessor& p, double fs, int block, int numChannels = 2)
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
    std::vector<std::vector<float>> run (SongsmithAnalogDeskProcessor& p, std::vector<std::vector<float>> in, int block)
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

    juce::MemoryBlock stateOf (SongsmithAnalogDeskProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    // Every parameter ID, with the default the strip must open with. Pinned: hosts store sessions under these.
    struct Row { const char* id; float def; };
    const Row allParameters[] = {
        { "input_trim", 0.0f }, { "output_trim", 0.0f }, { "drive", 0.0f }, { "high_pass", 15.0f }, { "low_pass", 22500.0f },
        { "eq_on", 1.0f }, { "lf_frequency", 100.0f }, { "lf_gain", 0.0f }, { "lf_bell", 0.0f },
        { "lmf_frequency", 600.0f }, { "lmf_gain", 0.0f }, { "lmf_q", 1.0f },
        { "hmf_frequency", 2500.0f }, { "hmf_gain", 0.0f }, { "hmf_q", 1.0f },
        { "hf_frequency", 8000.0f }, { "hf_gain", 0.0f }, { "hf_bell", 0.0f },
        { "dynamics_on", 1.0f }, { "dynamics_after_eq", 0.0f }, { "comp_threshold", 0.0f }, { "comp_ratio", 1.0f }, { "comp_attack", 3.0f },
        { "comp_release", 0.3f }, { "comp_soft_knee", 1.0f }, { "comp_makeup", 0.0f },
        { "gate_on", 0.0f }, { "gate_expander", 0.0f }, { "gate_threshold", -50.0f }, { "gate_range", 20.0f }, { "gate_attack", 1.0f },
        { "gate_hold", 0.1f }, { "gate_release", 0.3f },
        { "bypass", 0.0f } };
}

//==============================================================================
class AnalogDeskPluginTests final : public juce::UnitTest
{
public:
    AnalogDeskPluginTests() : juce::UnitTest ("Songsmith Analog Desk plugin", "AnalogDeskPlugin") {}

    void runTest() override
    {
        beginTest ("Identity: the product name and the codes hosts store sessions under never change");
        {
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Analog Desk"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
            expectEquals (juce::String (DESK_MANUFACTURER_CODE), juce::String ("Tnyw"), "shared with the whole family");
            expectEquals (juce::String (DESK_PLUGIN_CODE), juce::String ("Desk"), "this plugin's own code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm", "Cmp1", "Opt1", "Peq1", "Bus1", "Anl1" })
                expect (juce::String (DESK_PLUGIN_CODE) != juce::String (other), juce::String ("differs from ") + other);
        }

        beginTest ("Parameter IDs and defaults are stable, and every parameter exists exactly once");
        {
            SongsmithAnalogDeskProcessor p;
            expectEquals ((int) p.getParameters().size(), (int) std::size (allParameters), "no parameter added or removed without updating this table");
            for (const auto& row : allParameters)
            {
                auto* rp = p.apvts.getParameter (row.id);
                expect (rp != nullptr, juce::String (row.id) + " exists");
                if (rp == nullptr)
                    continue;
                expectWithinAbsoluteError (rp->convertFrom0to1 (rp->getDefaultValue()), row.def, 1.0e-3f, juce::String (row.id) + " default");
            }
        }

        beginTest ("A new strip leaves the sound exactly as it is, in mono and stereo");
        for (int channels : { 1, 2 })
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480, channels);
            std::vector<std::vector<float>> in;
            for (int c = 0; c < channels; ++c)
                in.push_back (noise (24000, 0.5f, 10 + (unsigned) c));
            const auto out = run (p, in, 480);
            float worst = 0.0f;
            for (int c = 0; c < channels; ++c)
                for (size_t i = 0; i < in[(size_t) c].size(); ++i)
                    worst = std::max (worst, std::abs (out[(size_t) c][i] - in[(size_t) c][i]));
            expectLessThan (worst, 1.0e-6f, juce::String (channels) + " channel(s): output equals input");
        }

        beginTest ("The settings reach the sound: the equaliser, the filters, the compressor, the gate and Drive each change it");
        {
            auto changes = [this] (const std::vector<std::pair<const char*, float>>& settings, const char* what)
            {
                SongsmithAnalogDeskProcessor p;
                prepare (p, 48000.0, 480);
                for (const auto& s : settings)
                    setParam (p, s.first, s.second);
                const auto in = noise (48000, 0.5f, 77);
                const auto out = run (p, { in, in }, 480);
                std::vector<float> diff (in.size());
                for (size_t i = 0; i < in.size(); ++i)
                    diff[i] = out[0][i] - in[i];
                expectGreaterThan (rms (diff, 12000), 1.0e-3f, juce::String (what) + " changes the sound");
            };
            changes ({ { "hmf_gain", 9.0f } }, "the high-mid band");
            changes ({ { "lf_gain", -9.0f }, { "lf_bell", 1.0f } }, "the low band as a bell");
            changes ({ { "high_pass", 250.0f } }, "the High-Pass Filter");
            changes ({ { "low_pass", 4000.0f } }, "the Low-Pass Filter");
            changes ({ { "drive", 0.8f } }, "Drive");
            changes ({ { "input_trim", 6.0f } }, "Input Trim");
            changes ({ { "comp_threshold", -30.0f }, { "comp_ratio", 8.0f } }, "the compressor");
            changes ({ { "input_trim", -20.0f }, { "gate_on", 1.0f }, { "gate_threshold", -10.0f }, { "gate_range", 40.0f } }, "the gate (here closing on quiet sound)");
            changes ({ { "output_trim", -6.0f } }, "Output Trim");
        }

        beginTest ("Equaliser On and Dynamics On each switch their own stage off");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, "hmf_gain", 12.0f);
            setParam (p, "eq_on", 0.0f);
            const auto in = noise (24000, 0.5f, 5);
            auto out = run (p, { in, in }, 480);
            float worst = 0.0f;
            for (size_t i = 0; i < in.size(); ++i)
                worst = std::max (worst, std::abs (out[0][i] - in[i]));
            expectLessThan (worst, 1.0e-6f, "with the equaliser off, its gain does nothing");

            SongsmithAnalogDeskProcessor q;
            prepare (q, 48000.0, 480);
            setParam (q, "comp_threshold", -40.0f);
            setParam (q, "comp_ratio", 20.0f);
            setParam (q, "dynamics_on", 0.0f);
            out = run (q, { in, in }, 480);
            worst = 0.0f;
            for (size_t i = 0; i < in.size(); ++i)
                worst = std::max (worst, std::abs (out[0][i] - in[i]));
            expectLessThan (worst, 1.0e-6f, "with the dynamics off, the compressor does nothing");
        }

        beginTest ("The compressor reports its gain reduction, and Bypass reports none");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, "comp_threshold", -30.0f);
            setParam (p, "comp_ratio", 10.0f);
            run (p, { noise (48000, 0.7f, 3), noise (48000, 0.7f, 4) }, 480);
            expectGreaterThan (p.takeGainReductionDb(), 3.0f, "the compressor is working");
            setParam (p, "bypass", 1.0f);
            run (p, { noise (48000, 0.7f, 3), noise (48000, 0.7f, 4) }, 480);
            expectEquals (p.takeGainReductionDb(), 0.0f, "nothing is reduced while bypassed");
        }

        beginTest ("Bypass passes the sound through bit-for-bit once its short fade has finished, and leaves no latency");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, "hmf_gain", 12.0f);
            setParam (p, "drive", 0.9f);
            setParam (p, "bypass", 1.0f);
            const auto in = noise (24000, 0.5f, 6);
            const auto out = run (p, { in, in }, 480);
            bool exact = true;
            for (size_t i = 4800; i < in.size(); ++i)
                exact = exact && out[0][i] == in[i] && out[1][i] == in[i];
            expect (exact, "bit-exact dry");
            expectEquals (p.getLatencySamples(), 0);
        }

        beginTest ("The result does not depend on the host's block size");
        {
            const auto in = noise (48000, 0.5f, 21);
            auto go = [&] (int block)
            {
                SongsmithAnalogDeskProcessor p;
                prepare (p, 48000.0, block);
                setParam (p, "hmf_gain", 6.0f);
                setParam (p, "lf_gain", 4.0f);
                setParam (p, "high_pass", 120.0f);
                setParam (p, "comp_threshold", -20.0f);
                setParam (p, "comp_ratio", 4.0f);
                setParam (p, "gate_on", 1.0f);
                setParam (p, "gate_threshold", -40.0f);
                return run (p, { in, in }, block)[0];
            };
            const auto a = go (64), b = go (480), c = go (4096);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i)
                worst = std::max ({ worst, std::abs (a[i] - b[i]), std::abs (a[i] - c[i]) });
            expectLessThan (worst, 1.0e-4f, "blocks of 64, 480 and 4,096 sound the same");
        }

        beginTest ("Moving a filter or equaliser control while playing does not click");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            const int n = 48000;
            std::vector<float> in ((size_t) n);
            for (int i = 0; i < n; ++i)
                in[(size_t) i] = 0.4f * std::sin (2.0f * 3.14159265f * 220.0f * (float) i / 48000.0f);
            std::vector<float> out;
            juce::MidiBuffer midi;
            for (int pos = 0; pos < n; pos += 480)
            {
                if (pos == 24000)
                {
                    setParam (p, "hmf_gain", 15.0f);
                    setParam (p, "hmf_frequency", 300.0f);
                    setParam (p, "lf_gain", 15.0f);
                    setParam (p, "high_pass", 300.0f);
                }
                juce::AudioBuffer<float> buffer (2, 480);
                for (int c = 0; c < 2; ++c)
                    buffer.copyFrom (c, 0, in.data() + pos, 480);
                p.processBlock (buffer, midi);
                for (int i = 0; i < 480; ++i)
                    out.push_back (buffer.getSample (0, i));
            }
            float biggestStep = 0.0f;
            for (size_t i = 24001; i < out.size(); ++i)
                biggestStep = std::max (biggestStep, std::abs (out[i] - out[i - 1]));
            expectLessThan (biggestStep, 1.5f, "no sample-to-sample jump beyond the gain the new settings cause");
        }

        beginTest ("State: a session restores every setting, and one from an older or damaged save opens safely");
        {
            SongsmithAnalogDeskProcessor a;
            setParam (a, "hmf_gain", 7.5f);
            setParam (a, "comp_ratio", 6.0f);
            setParam (a, "gate_on", 1.0f);
            setParam (a, "high_pass", 140.0f);
            setParam (a, "bypass", 1.0f);
            a.editorWidth.store (1500);
            a.presets.currentName = "My Sound";
            const auto saved = stateOf (a);

            SongsmithAnalogDeskProcessor b;
            b.setStateInformation (saved.getData(), (int) saved.getSize());
            expectWithinAbsoluteError (getParam (b, "hmf_gain"), 7.5f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (b, "comp_ratio"), 6.0f, 1.0e-3f);
            expectEquals (getParam (b, "gate_on"), 1.0f);
            expectWithinAbsoluteError (getParam (b, "high_pass"), 140.0f, 1.0e-3f);
            expectEquals (getParam (b, "bypass"), 1.0f);
            expectEquals (b.editorWidth.load(), 1500);
            expectEquals (b.presets.currentName, juce::String ("My Sound"));

            // A save with nothing in it gives the plain strip.
            juce::ValueTree empty (b.apvts.state.getType());
            b.restoreFromValueTree (empty);
            for (const auto& row : allParameters)
                expectWithinAbsoluteError (getParam (b, row.id), row.def, 1.0e-3f, juce::String (row.id) + " returns to its default");
            expectEquals (b.editorWidth.load(), Desk::EditorLayout::defaultWidth);

            // Values that are not numbers, out of range, or for parameters this version does not know.
            juce::ValueTree odd (b.apvts.state.getType());
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "hmf_gain" }, { "value", std::numeric_limits<float>::quiet_NaN() } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "lf_gain" }, { "value", 999.0f } }), nullptr);
            odd.appendChild (juce::ValueTree ("PARAM", { { "id", "a_future_setting" }, { "value", 1.0f } }), nullptr);
            odd.setProperty ("stateVersion", 99, nullptr);
            b.restoreFromValueTree (odd);
            expectWithinAbsoluteError (getParam (b, "hmf_gain"), 0.0f, 1.0e-4f, "not a number gives the default");
            expectEquals (getParam (b, "lf_gain"), 15.0f, "out of range is held at the limit");

            SongsmithAnalogDeskProcessor c;
            const char junk[] = "not a session";
            c.setStateInformation (junk, (int) sizeof (junk));
            expectWithinAbsoluteError (getParam (c, "hmf_gain"), 0.0f, 1.0e-4f, "junk changes nothing");
        }

        beginTest ("Presets: every starting point loads, names are unique, and each one changes the sound it should");
        {
            const auto& list = Desk::Presets::factory();
            expectGreaterThan ((int) list.size(), 5);
            juce::StringArray names;
            for (const auto& f : list)
            {
                expect (! names.contains (f.name), juce::String (f.name) + " is unique");
                names.add (f.name);
                expect (juce::String (f.hint).isNotEmpty(), juce::String (f.name) + " has a listening cue");
                for (const auto& s : f.settings)
                {
                    SongsmithAnalogDeskProcessor probe;
                    auto* rp = probe.apvts.getParameter (s.first);
                    expect (rp != nullptr, juce::String (f.name) + ": " + s.first + " is a real setting");
                    if (rp != nullptr)
                    {
                        const auto r = rp->getNormalisableRange();
                        expect (s.second >= r.start - 1.0e-4f && s.second <= r.end + 1.0e-4f, juce::String (f.name) + ": " + s.first + " is in range");
                    }
                }
            }

            for (int i = 0; i < (int) list.size(); ++i)
            {
                SongsmithAnalogDeskProcessor p;
                prepare (p, 48000.0, 480);
                p.setCurrentProgram (i);
                expectEquals (p.getProgramName (i), juce::String (list[(size_t) i].name));
                const auto in = noise (48000, 0.3f, 90 + (unsigned) i);
                const auto out = run (p, { in, in }, 480);
                std::vector<float> diff (in.size());
                for (size_t k = 0; k < in.size(); ++k)
                    diff[k] = out[0][k] - in[k];
                expectGreaterThan (rms (diff, 12000), 1.0e-3f, juce::String (list[(size_t) i].name) + " changes the sound");
                for (float v : out[0])
                    if (! std::isfinite (v)) { expect (false, "finite output"); break; }
            }

            // Loading one starting point after another leaves nothing from the first behind.
            SongsmithAnalogDeskProcessor p;
            p.setCurrentProgram (2);   // has a gate
            expectEquals (getParam (p, "gate_on"), 1.0f);
            p.setCurrentProgram (0);
            expectEquals (getParam (p, "gate_on"), 0.0f, "the next starting point starts from the plain strip");
        }

        beginTest ("Your presets: saved to their own folder, loaded back, and a starting point's name is refused");
        {
            auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("desk-presets-test-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
            SongsmithAnalogDeskProcessor p;
            p.presets.setUserFolder (root);
            setParam (p, "hmf_gain", 5.0f);
            setParam (p, "comp_ratio", 3.0f);
            setParam (p, "bypass", 1.0f);
            juce::String error;
            expect (p.presets.saveAs ("Test Sound", error), error);
            expect (p.presets.userPresetExists ("Test Sound"));
            expect (! p.presets.saveAs (Desk::Presets::factory()[0].name, error), "a starting point cannot be overwritten");
            expect (! p.presets.saveAs ("   ", error), "an empty name is refused");

            SongsmithAnalogDeskProcessor q;
            q.presets.setUserFolder (root);
            expectEquals (q.presets.getUserPresetFiles().size(), 1);
            expect (q.presets.loadFile (q.presets.getUserPresetFiles()[0], error), error);
            expectWithinAbsoluteError (getParam (q, "hmf_gain"), 5.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (q, "comp_ratio"), 3.0f, 1.0e-3f);
            expectEquals (getParam (q, "bypass"), 0.0f, "a preset never carries Bypass");
            expect (! q.presets.loadFile (root.getChildFile ("missing.sspreset"), error), "a missing file is refused");
            root.deleteRecursively();
        }

        beginTest ("Undo and Redo: a change the musician makes, and a preset load, each step back");
        {
            SongsmithAnalogDeskProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, "hmf_gain", "lf_gain", "Analog Desk");
        }
    }
};

//==============================================================================
class AnalogDeskEditorTests final : public juce::UnitTest
{
public:
    AnalogDeskEditorTests() : juce::UnitTest ("Songsmith Analog Desk editor", "AnalogDeskPlugin") {}

    static std::unique_ptr<SongsmithAnalogDeskEditor> make (SongsmithAnalogDeskProcessor& p, int width)
    {
        std::unique_ptr<SongsmithAnalogDeskEditor> ed (dynamic_cast<SongsmithAnalogDeskEditor*> (p.createEditor()));
        ed->setSize (width, Desk::EditorLayout::heightFor (width));
        return ed;
    }

    void runTest() override
    {
        beginTest ("Every setting has a control, and the controls sit in the order of the strip");
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            std::set<juce::String> shown;
            for (auto* k : ed->getKnobs())
                shown.insert (k->slider.getTitle());
            for (auto* b : ed->getToggles())
                shown.insert (b->getTitle());
            shown.insert (ed->getBypassButton().getTitle());
            for (auto* prm : p.getParameters())
                expect (shown.count (prm->getName (100)) == 1, "a control named \"" + prm->getName (100) + "\"");
            expectEquals ((int) shown.size(), (int) std::size (allParameters), "and no control without a setting");

            // Top to bottom: filters, equaliser, dynamics. Within a row, left to right.
            auto y = [&ed] (const char* id) { return ed->findKnob (id)->getY(); };
            expect (y ("high_pass") < y ("lf_frequency") && y ("lf_frequency") < y ("comp_threshold"), "filters, then equaliser, then dynamics");
            expect (ed->findKnob ("lf_frequency")->getX() < ed->findKnob ("lmf_frequency")->getX()
                    && ed->findKnob ("lmf_frequency")->getX() < ed->findKnob ("hmf_frequency")->getX()
                    && ed->findKnob ("hmf_frequency")->getX() < ed->findKnob ("hf_frequency")->getX(), "the bands run low to high");
        }

        beginTest ("Layout: everything inside, above the footer, clear of the meter column and of each other");
        for (int width : { Desk::EditorLayout::minWidth, Desk::EditorLayout::defaultWidth, Desk::EditorLayout::maxWidth })
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            std::vector<juce::Component*> parts;
            for (auto* k : ed->getKnobs())
                parts.push_back (k);
            for (auto* b : ed->getToggles())
                parts.push_back (b);
            auto* content = ed->getMeterColumn().getParentComponent();
            for (size_t i = 0; i < parts.size(); ++i)
            {
                expect (content->getLocalBounds().contains (parts[i]->getBounds()), context + ": inside the editor");
                expect (parts[i]->getBottom() <= Desk::EditorLayout::contentHeight, context + ": above the footer strip");
                expect (parts[i]->getRight() <= ed->getMeterColumn().getX(), context + ": clear of the meter column");
                for (size_t j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), context + ": no overlap");
            }
            expect (ed->getBypassButton().getRight() <= content->getWidth(), context + ": Bypass is inside");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("The suite title bar and the global Bypass are where every plugin has them");
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Analog Desk");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter ("bypass"), "Analog Desk");
        }

        beginTest ("The knobs show and take values, and double-click returns them to the default");
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            expectEquals (ed->findKnob ("high_pass")->getValueText(), juce::String ("Off"));
            expectEquals (ed->findKnob ("low_pass")->getValueText(), juce::String ("Off"));
            expectEquals (ed->findKnob ("comp_ratio")->getValueText(), juce::String ("Off"));
            expectEquals (ed->findKnob ("hmf_gain")->getValueText(), juce::String ("0.0 dB"));

            ed->findKnob ("high_pass")->slider.setValue (120.0, juce::sendNotificationSync);
            ed->findKnob ("hmf_gain")->slider.setValue (4.5, juce::sendNotificationSync);
            ed->findKnob ("lf_frequency")->slider.setValue (60.0, juce::sendNotificationSync);
            ed->findKnob ("hf_frequency")->slider.setValue (12000.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "high_pass"), 120.0f, 1.0e-3f);
            expectWithinAbsoluteError (getParam (p, "hmf_gain"), 4.5f, 1.0e-3f);
            expectEquals (ed->findKnob ("high_pass")->getValueText(), juce::String ("120 Hz"));
            expectEquals (ed->findKnob ("hmf_gain")->getValueText(), juce::String ("+4.5 dB"));
            expectEquals (ed->findKnob ("lf_frequency")->getValueText(), juce::String ("60 Hz"));
            expectEquals (ed->findKnob ("hf_frequency")->getValueText(), juce::String ("12.00 kHz"));

            setParam (p, "comp_ratio", 4.0f);
            expectEquals (ed->findKnob ("comp_ratio")->getValueText(), juce::String ("4.0:1"), "automation moves the knob");
            setParam (p, "low_pass", 9000.0f);
            expectEquals (ed->findKnob ("low_pass")->getValueText(), juce::String ("9.00 kHz"));

            // The toggles follow their settings, and clicking one changes it.
            auto toggle = [&ed] (const juce::String& title) -> juce::Button*
            {
                for (auto* b : ed->getToggles())
                    if (b->getTitle() == title)
                        return b;
                return nullptr;
            };
            auto* gate = toggle ("Gate On");
            expect (gate != nullptr && ! gate->getToggleState());
            if (gate != nullptr)
            {
                gate->setToggleState (true, juce::sendNotificationSync);
                expectEquals (getParam (p, "gate_on"), 1.0f);
            }
            expect (toggle ("Equaliser On")->getToggleState(), "the equaliser starts on");
            expect (toggle ("Soft Knee")->getToggleState(), "and the compressor starts with a soft knee");
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { Desk::EditorLayout::defaultWidth, Desk::EditorLayout::minWidth })
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, width);
            const auto context = "Analog Desk at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            texts.addArray (SongsmithAnalogDeskEditor::paintedTexts());
            for (auto* k : ed->getKnobs())
                texts.add (k->getShownName());
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : Desk::Presets::factory())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 60, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Equaliser On", "Dynamics On", "Make-up Gain", "Gain Reduction", "Bypass", "Undo", "Redo", "Input Trim", "Output Trim" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithAnalogDeskProcessor p;
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Analog Desk");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Analog Desk editor");
        }

        beginTest ("The footer shows the version and latency and the Processing Load switch, in the shared strip");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            expectEquals (ed->getDiagnosticsBounds().getY(), Desk::EditorLayout::contentHeight);
            expectEquals (ed->getDiagnosticsBounds().getHeight(), Desk::EditorLayout::footerHeight);
            Songsmith::Diagnostics::Testing::checkVersionBinding (*this, p, "Analog Desk");
            Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, p, "Analog Desk");
            Songsmith::Diagnostics::Testing::checkFooterLayout (*this, p, "Analog Desk");
        }

        beginTest ("The meter follows the compressor");
        {
            SongsmithAnalogDeskProcessor p;
            prepare (p, 48000.0, 480);
            setParam (p, "comp_threshold", -30.0f);
            setParam (p, "comp_ratio", 10.0f);
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            run (p, { noise (48000, 0.7f, 31), noise (48000, 0.7f, 32) }, 480);
            ed->advanceMeter (0.5f);
            expectGreaterThan (ed->getMeterColumn().getReduction().getLevel(), 2.0f, "the Gain Reduction bar has moved");
        }

        beginTest ("The preset menu lists the starting points, your presets, Save and the folder, and loading a starting point is one Undo step");
        {
            SongsmithAnalogDeskProcessor p;
            const auto count = (int) Desk::Presets::factory().size();
            auto ed = make (p, Desk::EditorLayout::defaultWidth);
            auto& box = ed->getPresetBox();
            int starting = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                if (box.getItemId (i) >= 1 && box.getItemId (i) <= count)
                    ++starting;
            expectEquals (starting, count);
            box.setSelectedId (2, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, "hmf_gain"), 3.0f, 1.0e-3f);
            expect (ed->getUndoButton().isEnabled(), "Undo is available after loading");
            expectEquals (box.getText(), juce::String (Desk::Presets::factory()[1].name));
            expect (box.getTooltip().contains (Desk::Presets::factory()[1].hint), "the listening cue is on hover");
            p.history.undo();
            expectWithinAbsoluteError (getParam (p, "hmf_gain"), 0.0f, 1.0e-4f, "one Undo restores everything the preset changed");
            expectWithinAbsoluteError (getParam (p, "comp_ratio"), 1.0f, 1.0e-4f);
            expectWithinAbsoluteError (getParam (p, "high_pass"), 15.0f, 1.0e-3f);
        }
    }
};

static AnalogDeskPluginTests analogDeskPluginTests;
static AnalogDeskEditorTests analogDeskEditorTests;

//==============================================================================
static SongsmithUITests sharedUITests { "AnalogDeskPlugin" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "AnalogDeskPlugin" };
static SongsmithMeterColumnTests<SongsmithAnalogDeskProcessor, SongsmithAnalogDeskEditor> sharedMeterColumnTests { "AnalogDeskPlugin", Desk::EditorLayout::designWidth, Desk::EditorLayout::contentHeight };
static SongsmithGuidanceTests sharedGuidanceTests { "AnalogDeskPlugin" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// AnalogDeskTests --screenshot <file.png> [--scale 2] [--preset N]: renders the editor with a starting point loaded and some
// compression on the meter to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale, int preset)
{
    SongsmithAnalogDeskProcessor p;
    p.setCurrentProgram (juce::jlimit (0, p.getNumPrograms() - 1, preset));
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithAnalogDeskEditor> editor (dynamic_cast<SongsmithAnalogDeskEditor*> (p.createEditor()));
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
    runner.runTestsInCategory ("AnalogDesk");
    runner.runTestsInCategory ("AnalogDeskPlugin");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
