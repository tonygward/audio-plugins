// Songsmith Compressor test runner: the plugin (identity, parameters, state, bypass, presets, editor)
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
#include "../../shared/Tests/SongsmithUpdateTests.h"
#include "../../shared/Tests/SongsmithBeforeAfterTests.h"
#include "SongsmithWording.h"

namespace
{
    void setParam (SongsmithCompressorAudioProcessor& p, const char* id, float value)
    {
        auto* rp = p.apvts.getParameter (id);
        jassert (rp != nullptr);
        rp->setValueNotifyingHost (rp->convertTo0to1 (value));
    }

    float getParam (const SongsmithCompressorAudioProcessor& p, const char* id)
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

    void prepare (SongsmithCompressorAudioProcessor& p, double fs, int block, int numChannels = 2)
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
    std::vector<std::vector<float>> run (SongsmithCompressorAudioProcessor& p, std::vector<std::vector<float>> in, int block)
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
}

//==============================================================================
class CompressorPluginTests final : public juce::UnitTest
{
public:
    CompressorPluginTests() : juce::UnitTest ("Songsmith Compressor plugin", "Compressor") {}

    void runTest() override
    {
        beginTest ("Plug-in identity: its own name and codes, never the Analog Compressor's");
        {
            expectEquals (juce::String (COMP_MANUFACTURER_CODE), juce::String ("Tnyw"), "Songsmith manufacturer code");
            expectEquals (juce::String (COMP_PLUGIN_CODE), juce::String ("Cmp1"), "pinned plugin code");
            for (auto* other : { "Eq3b", "Sfc1", "Mxlm" })
                expect (juce::String (COMP_PLUGIN_CODE) != other, "distinct from " + juce::String (other));
            expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Compressor"));
            expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));
        }

        beginTest ("Parameters: stable IDs, ranges and gentle defaults");
        {
            SongsmithCompressorAudioProcessor p;
            struct Row { const char* id; float lo, hi, def; };
            const Row rows[] = { { Comp::thresholdId, -60.0f, 0.0f, -18.0f }, { Comp::ratioId, 1.0f, 20.0f, 2.0f },
                                 { Comp::attackId, 0.1f, 100.0f, 10.0f }, { Comp::releaseId, 10.0f, 2000.0f, 100.0f },
                                 { Comp::kneeId, 0.0f, 24.0f, 6.0f }, { Comp::makeupId, 0.0f, 24.0f, 0.0f },
                                 { Comp::mixId, 0.0f, 100.0f, 100.0f } };
            for (auto& r : rows)
            {
                auto* prm = p.apvts.getParameter (r.id);
                expect (prm != nullptr, juce::String (r.id) + " exists");
                if (prm == nullptr) continue;
                const auto range = p.apvts.getParameterRange (r.id);
                expectWithinAbsoluteError (range.start, r.lo, 1.0e-4f, juce::String (r.id) + " minimum");
                expectWithinAbsoluteError (range.end, r.hi, 1.0e-4f, juce::String (r.id) + " maximum");
                expectWithinAbsoluteError (getParam (p, r.id), r.def, 1.0e-3f, juce::String (r.id) + " default");
            }
            expect (p.apvts.getParameter (Comp::autoMakeupId) != nullptr && p.apvts.getParameter (Comp::bypassId) != nullptr);
            expectEquals (p.getParameters().size(), 11, "eleven parameters (Analog Warmth Model was added in version 3)");
            expect (p.apvts.getParameter (Comp::warmthId) != nullptr, "Analog Warmth exists");
            expectWithinAbsoluteError (getParam (p, Comp::warmthId), 0.0f, 1.0e-6f, "and is off by default");
            expectEquals (p.apvts.getParameter (Comp::warmthId)->getName (100), juce::String ("Analog Warmth"));
            expect (p.getBypassParameter() != nullptr, "native host bypass");

            // The ratio control is finest at the gentle end: 1.3 sits well inside the first half of the travel.
            auto* ratio = p.apvts.getParameter (Comp::ratioId);
            expectLessThan (ratio->convertTo0to1 (1.5f), 0.25f, "1.5 to 1 is in the first quarter of the travel");
            expectWithinAbsoluteError (ratio->convertFrom0to1 (1.0f), 20.0f, 1.0e-3f);
        }

        beginTest ("No latency, mono and stereo layouts, and nothing else");
        {
            SongsmithCompressorAudioProcessor p;
            prepare (p, 48000.0, 512);
            expectEquals (p.getLatencySamples(), 0);
            expect (p.getTailLengthSeconds() == 0.0);
            prepare (p, 48000.0, 512, 1);
            expectEquals (p.getTotalNumInputChannels(), 1);

            juce::AudioProcessor::BusesLayout surround;
            surround.inputBuses.add (juce::AudioChannelSet::create5point1());
            surround.outputBuses.add (juce::AudioChannelSet::create5point1());
            expect (! p.isBusesLayoutSupported (surround), "5.1 is not supported");
            juce::AudioProcessor::BusesLayout mismatch;
            mismatch.inputBuses.add (juce::AudioChannelSet::mono());
            mismatch.outputBuses.add (juce::AudioChannelSet::stereo());
            expect (! p.isBusesLayoutSupported (mismatch), "different input and output layouts are refused");
        }

        beginTest ("Processing: the plugin gives what the engine gives, at any block size, and reduces loud sound");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            const auto left = noise (24000, 0.8f, 1), right = noise (24000, 0.6f, 2);
            auto render = [&] (int block)
            {
                SongsmithCompressorAudioProcessor p;
                setParam (p, Comp::thresholdId, -24.0f);
                setParam (p, Comp::ratioId, 4.0f);
                prepare (p, fs, block);
                return run (p, { left, right }, block);
            };
            const auto big = render (512), small = render (37);
            expect (big == small, "block size does not change the result at " + juce::String (fs));

            double inRms = 0.0, outRms = 0.0;
            for (size_t i = 12000; i < left.size(); ++i) { inRms += left[i] * left[i]; outRms += big[0][i] * big[0][i]; }
            expectLessThan (outRms, inRms * 0.7, "the loud noise is turned down");
        }

        beginTest ("Bypass: after the short fade the sound is bit-identical, and nothing clicks on the way");
        {
            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::thresholdId, -40.0f);
            setParam (p, Comp::ratioId, 20.0f);
            prepare (p, 48000.0, 512);
            const auto in = noise (48000, 0.7f, 3);
            auto first = run (p, { in, in }, 512);
            expect (first[0] != in, "compressing");

            setParam (p, Comp::bypassId, 1.0f);
            juce::MidiBuffer midi;
            const int settled = (int) (48000.0 * SongsmithCompressorAudioProcessor::bypassFadeSeconds) + 512;
            for (int b = 0; b < settled / 512 + 2; ++b)
            {
                juce::AudioBuffer<float> buffer (2, 512);
                for (int c = 0; c < 2; ++c)
                    buffer.copyFrom (c, 0, in.data(), 512);
                p.processBlock (buffer, midi);
                if (b >= settled / 512 + 1)
                    for (int i = 0; i < 512; ++i)
                        expectEquals (buffer.getSample (0, i), in[(size_t) i], "bit-exact dry");
            }
        }

        beginTest ("Saved state: round trip, defaults for anything missing, tolerant of newer versions");
        {
            SongsmithCompressorAudioProcessor a;
            setParam (a, Comp::thresholdId, -31.5f);
            setParam (a, Comp::ratioId, 1.3f);
            setParam (a, Comp::attackId, 30.0f);
            setParam (a, Comp::releaseId, 50.0f);
            setParam (a, Comp::kneeId, 12.0f);
            setParam (a, Comp::makeupId, 4.0f);
            setParam (a, Comp::autoMakeupId, 0.0f);
            setParam (a, Comp::mixId, 40.0f);
            setParam (a, Comp::warmthId, 1.0f);
            a.editorWidth = 1000;

            juce::MemoryBlock block;
            a.getStateInformation (block);
            SongsmithCompressorAudioProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            for (auto* id : { Comp::thresholdId, Comp::ratioId, Comp::attackId, Comp::releaseId, Comp::kneeId, Comp::makeupId,
                              Comp::autoMakeupId, Comp::mixId, Comp::warmthId, Comp::bypassId })
                expectWithinAbsoluteError (getParam (b, id), getParam (a, id), 1.0e-3f, juce::String (id) + " restored");
            expectEquals (b.editorWidth.load(), 1000);

            auto xml = a.createState().createXml();
            expect (xml != nullptr && xml->getIntAttribute ("version") == SongsmithCompressorAudioProcessor::stateVersion, "version saved");

            // A session that knows only some parameters (as an older version would): the rest take defaults.
            auto tree = a.createState();
            tree.removeChild (tree.getChildWithProperty ("id", Comp::mixId), nullptr);
            tree.removeChild (tree.getChildWithProperty ("id", Comp::kneeId), nullptr);
            tree.removeChild (tree.getChildWithName ("UI"), nullptr);
            SongsmithCompressorAudioProcessor c;
            c.restoreFromValueTree (tree);
            expectWithinAbsoluteError (getParam (c, Comp::mixId), 100.0f, 1.0e-3f, "missing Mix is the default");
            expectWithinAbsoluteError (getParam (c, Comp::kneeId), 6.0f, 1.0e-3f, "missing Knee is the default");
            expectWithinAbsoluteError (getParam (c, Comp::thresholdId), -31.5f, 0.1f, "known values kept");
            expectEquals (c.editorWidth.load(), Comp::EditorLayout::defaultWidth, "default size");

            // A version-1 session (saved before Analog Warmth existed) opens with it off and everything else as saved.
            auto v1 = a.createState();
            v1.setProperty ("version", 1, nullptr);
            v1.removeChild (v1.getChildWithProperty ("id", Comp::warmthId), nullptr);
            SongsmithCompressorAudioProcessor old;
            old.restoreFromValueTree (v1);
            expectWithinAbsoluteError (getParam (old, Comp::warmthId), 0.0f, 1.0e-6f, "an older session opens with Analog Warmth off");
            expectWithinAbsoluteError (getParam (old, Comp::ratioId), 1.3f, 0.011f, "and keeps its other settings");
            expect (! old.getEngineParams().warmth, "and sounds as it did");

            // A newer version with an unknown extra parameter and an absurd value: kept in range, extras ignored.
            auto newer = a.createState();
            newer.setProperty ("version", 99, nullptr);
            newer.appendChild (juce::ValueTree ("PARAM", { { "id", "from_the_future" }, { "value", 1.0f } }), nullptr);
            newer.getChildWithProperty ("id", Comp::ratioId).setProperty ("value", 1.0e9f, nullptr);
            newer.getChildWithProperty ("id", Comp::thresholdId).setProperty ("value", std::numeric_limits<float>::quiet_NaN(), nullptr);
            SongsmithCompressorAudioProcessor d;
            d.restoreFromValueTree (newer);
            expectWithinAbsoluteError (getParam (d, Comp::ratioId), 20.0f, 1.0e-3f, "clamped into range");
            expectWithinAbsoluteError (getParam (d, Comp::thresholdId), -18.0f, 1.0e-3f, "NaN falls back to the default");

            SongsmithCompressorAudioProcessor e;
            const char junk[] = "not a session";
            e.setStateInformation (junk, (int) sizeof (junk));   // must not crash or change anything
            expectWithinAbsoluteError (getParam (e, Comp::ratioId), 2.0f, 1.0e-3f);
        }

        beginTest ("Undo and Redo of what the musician changes, not host automation");
        {
            SongsmithCompressorAudioProcessor p;
            Songsmith::UI::Testing::checkParameterHistory (*this, p, Comp::thresholdId, Comp::releaseId, "Compressor");
        }

        beginTest ("Factory presets: plain names, in range, unique, Acoustic Guitar as published, loaded in one undo step");
        {
            const auto& presets = Comp::getPresets();
            juce::StringArray names;
            for (auto& pr : presets)
            {
                expect (! names.contains (pr.name), "unique name " + juce::String (pr.name));
                names.add (pr.name);
                expect (pr.thresholdDb >= -60.0f && pr.thresholdDb <= 0.0f && pr.ratio >= 1.0f && pr.ratio <= 20.0f
                        && pr.attackMs >= 0.1f && pr.attackMs <= 100.0f && pr.releaseMs >= 10.0f && pr.releaseMs <= 2000.0f
                        && pr.kneeDb >= 0.0f && pr.kneeDb <= 24.0f && pr.makeupDb >= 0.0f && pr.makeupDb <= 24.0f
                        && pr.mixPercent >= 0.0f && pr.mixPercent <= 100.0f, juce::String (pr.name) + " is in range");
                expect (! juce::String (pr.name).containsIgnoreCase ("butter") && ! juce::String (pr.hint).containsIgnoreCase ("butter"),
                        "no tutorial wording on screen");
                expect (juce::String (pr.hint).length() > 40 && juce::String (pr.hint).endsWithChar ('.'), "a full explanation");
            }
            expect (names.contains ("Acoustic Guitar"), "an Acoustic Guitar preset");

            SongsmithCompressorAudioProcessor p;
            p.history.clear();
            const int guitar = names.indexOf ("Acoustic Guitar");
            p.history.runStep ("preset load", [&] { p.applyPreset (guitar); });
            expectWithinAbsoluteError (getParam (p, Comp::ratioId), 1.3f, 0.011f, "ratio 1.3 to 1");
            expectWithinAbsoluteError (getParam (p, Comp::attackId), 30.0f, 0.05f, "attack 30 ms");
            expectWithinAbsoluteError (getParam (p, Comp::releaseId), 50.0f, 0.5f, "release 50 ms");
            expectLessThan (getParam (p, Comp::thresholdId), -24.0f, "a low threshold");
            expect (p.getEngineParams().autoMakeup, "level matched");
            p.history.undo();
            expectWithinAbsoluteError (getParam (p, Comp::ratioId), 2.0f, 0.011f, "one undo restores everything");
            expect (! p.history.canUndo(), "and it was one step");

            setParam (p, Comp::bypassId, 1.0f);
            p.applyPreset (0);
            expect (p.isBypassed(), "loading a preset leaves Bypass alone");
            p.applyPreset (99);   // out of range: ignored

            setParam (p, Comp::warmthId, 1.0f);
            p.applyPreset (1);
            expect (! p.getEngineParams().warmth, "every preset is the clean compressor (Analog Warmth off)");
        }

        beginTest ("Analog Warmth in the plugin: off is bit-identical to a plugin without it, on changes the sound a little");
        {
            const auto in = noise (24000, 0.4f, 8);
            auto render = [&] (bool warmth)
            {
                SongsmithCompressorAudioProcessor p;
                setParam (p, Comp::thresholdId, -20.0f);
                setParam (p, Comp::warmthId, warmth ? 1.0f : 0.0f);
                prepare (p, 48000.0, 512);
                return run (p, { in, in }, 512);
            };
            const auto off = render (false), on = render (true), off2 = render (false);
            expect (off == off2, "deterministic");
            expect (off != on, "on is different");
            float diff = 0.0f;
            for (size_t i = 0; i < in.size(); ++i) diff = std::max (diff, std::abs (on[0][i] - off[0][i]));
            expectLessThan (diff, 0.3f, "by a small amount");
            SongsmithCompressorAudioProcessor p;
            prepare (p, 48000.0, 512);
            expectEquals (p.getLatencySamples(), 0);
            setParam (p, Comp::warmthId, 1.0f);
            expectEquals (p.getLatencySamples(), 0, "Analog Warmth adds no latency");
        }

        beginTest ("Auto make-up keeps the level of a -18 dBFS signal; manual make-up adds its decibels");
        {
            SongsmithCompressorAudioProcessor p;
            p.applyPreset (0);   // Acoustic Guitar
            prepare (p, 48000.0, 480);
            // A steady-level signal (samples alternate +a and -a), so the peak detector sees a constant level.
            // A sine reads up to about 0.8 dB louder, because the detector follows each wave's peaks (see docs/engine.md).
            std::vector<float> steady (48000);
            for (size_t i = 0; i < steady.size(); ++i)
                steady[i] = (i & 1) ? -0.1259f : 0.1259f;   // -18 dBFS
            const auto out = run (p, { steady, steady }, 480);
            expectWithinAbsoluteError (20.0f * std::log10 (std::abs (out[0][47998]) / 0.1259f), 0.0f, 0.1f, "level matched within 0.1 dB");
        }
    }
};

//==============================================================================
class CompressorEditorTests final : public juce::UnitTest
{
public:
    CompressorEditorTests() : juce::UnitTest ("Songsmith Compressor editor", "Compressor") {}

    static std::unique_ptr<SongsmithCompressorAudioProcessorEditor> make (SongsmithCompressorAudioProcessor& p, int width)
    {
        p.editorWidth = width;
        return std::unique_ptr<SongsmithCompressorAudioProcessorEditor> (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (p.createEditor()));
    }

    void runTest() override
    {
        beginTest ("The editor opens at the default, minimum and maximum sizes, keeps its shape and remembers its size");
        for (int width : { Comp::EditorLayout::defaultWidth, Comp::EditorLayout::minWidth, Comp::EditorLayout::maxWidth })
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, width);
            expect (ed != nullptr);
            expectEquals (ed->getWidth(), width);
            expectEquals (ed->getHeight(), Comp::EditorLayout::heightFor (width));
            expectWithinAbsoluteError (ed->getContentScale(), (float) width / (float) Comp::EditorLayout::designWidth, 1.0e-5f);
        }
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, 1000);
            ed->setSize (1200, Comp::EditorLayout::heightFor (1200));
            expectEquals (p.editorWidth.load(), 1200, "the size is saved with the session");
            p.editorWidth = 1;
            expectEquals (Comp::EditorLayout::constrainWidth (1), Comp::EditorLayout::minWidth);
        }

        beginTest ("Title bar reads name, preset, Undo, Redo, Bypass, and Bypass is top-right in the suite red");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Compressor");
            Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (Comp::bypassId), "Compressor");
            expect (ed->getTitleBounds().getRight() < ed->getPresetBox().getX(), "the title does not overlap the preset selector");
            expect (ed->getTitleHint().getBounds() == ed->getTitleBounds(), "the hint sits exactly over the title");
        }

        beginTest ("Layout: the graph fills the window, the controls float in a panel inside it, and the meters sit on the right");
        for (int width : { Comp::EditorLayout::minWidth, Comp::EditorLayout::defaultWidth, Comp::EditorLayout::maxWidth })
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, width);
            const auto context = "at " + juce::String (width) + " px";
            auto& panel = ed->getPanel();
            const auto graph = ed->getGraph().getBounds(), panelBounds = panel.getBounds();
            expect (graph.getY() >= 52, context + ": the graph is below the title bar");
            expect (graph.contains (panelBounds), context + ": the floating panel stays whole inside the graph");

            // Every control is inside the panel, none overlap, and Auto Makeup is by the knob it switches off.
            std::vector<juce::Component*> parts { &ed->getThresholdSlider(), &ed->getRatioSlider(), &ed->getAttackSlider(),
                                                  &ed->getReleaseSlider(), &ed->getKneeSlider(), &ed->getMakeupSlider(),
                                                  &ed->getMixSlider(), &ed->getAutoMakeupButton(), &ed->getModelBox() };
            for (size_t i = 0; i < parts.size(); ++i)
            {
                expect (parts[i]->getParentComponent() == &panel, context + ": in the panel");
                expect (panel.getLocalBounds().contains (parts[i]->getBounds()), context + ": inside the panel");
                for (size_t j = i + 1; j < parts.size(); ++j)
                    expect (! parts[i]->getBounds().intersects (parts[j]->getBounds()), context + ": no overlap");
            }
            const auto makeup = ed->getMakeupSlider().getBounds(), button = ed->getAutoMakeupButton().getBounds();
            expect (button.getY() >= makeup.getBottom() - 2 && std::abs (button.getCentreX() - makeup.getCentreX()) < 120, context + ": Auto Makeup is under Makeup Gain");

            // The right edge: Gain Reduction bar, then the same output meter as the Songsmith EQ, clear of the graph.
            auto& meter = ed->getOutputMeter();
            auto& reduction = ed->getReductionMeter();
            expect (meter.getX() >= graph.getRight() && reduction.getX() >= graph.getRight(), context + ": the meters are right of the graph");
            expect (reduction.getRight() <= meter.getX(), context + ": Gain Reduction is beside the output meter");
            expectEquals (meter.getWidth(), Songsmith::UI::OutputMeterDisplay::preferredWidth, context + ": the EQ's meter width");
            expectEquals (meter.getRight(), ed->getContent().getWidth() - 16, context + ": at the right edge");
            expectEquals (meter.getBottom(), graph.getBottom(), context + ": as tall as the graph");
            expect (ed->getContent().getLocalBounds().contains (meter.getBounds()), context + ": inside the editor");
            expect (graph.contains (ed->getPauseButton().getBounds()), context + ": the Pause button is on the graph");
            expect (! ed->getPauseButton().getBounds().intersects (panelBounds), context + ": and clear of the panel at its default place");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Makeup Gain is greyed out while Auto Makeup is on, and the knobs show and take values");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            ed->syncFromParameters();
            expect (ed->getAutoMakeupButton().getToggleState() && ! ed->getMakeupSlider().isEnabled(), "Auto on: Makeup Gain greyed");
            setParam (p, Comp::autoMakeupId, 0.0f);
            ed->syncFromParameters();
            expect (ed->getMakeupSlider().isEnabled(), "Auto off: Makeup Gain available");

            expectEquals (ed->getThresholdSlider().getTextFromValue (-18.0), juce::String ("-18.0 dB"));
            expectEquals (ed->getRatioSlider().getTextFromValue (2.0), juce::String ("2.00:1"));
            expectEquals (ed->getAttackSlider().getTextFromValue (0.1), juce::String ("0.10 ms"));
            expectEquals (ed->getReleaseSlider().getTextFromValue (100.0), juce::String ("100 ms"));
            expectEquals (ed->getMixSlider().getTextFromValue (100.0), juce::String ("100%"));
            expectWithinAbsoluteError (ed->getRatioSlider().getValueFromText ("1.3"), 1.3, 1.0e-6, "typing a value works");
            expectWithinAbsoluteError (ed->getThresholdSlider().getValueFromText ("-30 dB"), -30.0, 1.0e-6, "typing with the unit works");

            ed->getThresholdSlider().setValue (-40.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, Comp::thresholdId), -40.0f, 0.1f, "the knob moves the parameter");
            ed->getThresholdSlider().mouseDoubleClick (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {}, juce::ModifierKeys(),
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &ed->getThresholdSlider(), &ed->getThresholdSlider(), juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 2, false));
            expectWithinAbsoluteError (getParam (p, Comp::thresholdId), -18.0f, 0.1f, "double-click returns to the default");
        }

        beginTest ("Choosing a preset sets the knobs, the tooltip explains it, and it is one undo step");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            ed->getPresetBox().setSelectedItemIndex (0, juce::sendNotificationSync);
            expectWithinAbsoluteError (ed->getRatioSlider().getValue(), 1.3, 0.011, "ratio on the knob");
            expect (ed->getPresetBox().getTooltip().contains ("acoustic guitar"), "tooltip explains the preset");
            expect (ed->getUndoButton().isEnabled(), "Undo is available");
            ed->getUndoButton().onClick();
            expectWithinAbsoluteError (ed->getRatioSlider().getValue(), 2.0, 0.011, "Undo returns the knob");
        }

        beginTest ("Meters: Gain Reduction is clamped and positive; the output meter is the Songsmith EQ's and follows its rules");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            ed->getReductionMeter().push (40.0f, 0.03);
            expectLessThan (ed->getReductionMeter().getBallistics().getLevel(), 24.01f, "clamped to the scale");
            expectGreaterThan (ed->getReductionMeter().getBallistics().getLevel(), 0.0f, "a positive amount, never a minus sign");
            expect (! ed->getReductionValue().getText().contains ("-"), "and shown without a sign");

            auto& m = ed->getOutputMeter();
            m.update (juce::Decibels::decibelsToGain (-0.6f), juce::Decibels::decibelsToGain (-3.0f), 2, false, 0.0, 0.0);
            expectEquals (m.getPeakText(), juce::String ("-0.6"), "a negative held peak keeps its sign");
            expect (! m.isPeakOver() && ! m.isClipped(), "and is not red");
            m.update (juce::Decibels::decibelsToGain (0.2f), 0.1f, 2, true, 1.0, 0.1);
            expectEquals (m.getPeakText(), juce::String ("+0.2"));
            expect (m.isPeakOver() && m.isClipped(), "over 0 dBFS is red and lights CLIP");
            m.update (juce::Decibels::decibelsToGain (-6.0f), 0.1f, 2, true, 9.0, 8.0);
            expect (m.isClipped() && ! m.isPeakOver(), "a latched CLIP does not make a negative number red (EQ-42)");
            expect (m.getTooltip().contains ("sample peak, not true peak") && m.getTooltip().contains ("Makeup Gain"), "the tooltip is the compressor's own");
        }

        beginTest ("Plain words: every text, parameter name and preset, and every label fits at two sizes");
        for (int width : { Comp::EditorLayout::defaultWidth, Comp::EditorLayout::minWidth })
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, width);
            const auto context = "Compressor at " + juce::String (width) + " px";
            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*ed, texts);
            for (auto* prm : p.getParameters())
                texts.add (prm->getName (100));
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& pr : Comp::getPresets())
            {
                texts.add (pr.name);
                texts.add (pr.hint);
            }
            expectGreaterThan (texts.size(), 25, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);
            for (auto* t : { "Threshold", "Ratio", "Attack", "Release", "Knee", "Makeup Gain", "Mix", "Auto Makeup", "Pause", "Gain Reduction" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), context);
        }

        beginTest ("Every control uses the shared bevelled look, the embedded typeface and the type scale");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *ed, "Compressor");
            Songsmith::UI::Testing::checkEditorFonts (*this, *ed, "Compressor editor");
        }

        beginTest ("COMP-03: any analogue model gives the floating panel the brushed grain at once, from the dropdown, from automation and from a restored session (songsmithstudio#23)");
        {
            SongsmithCompressorAudioProcessor p;
            auto ed = make (p, Comp::EditorLayout::defaultWidth);
            auto& panel = ed->getPanel();
            expect (! panel.isTextured(), "Clean by default: the plain panel");
            expectEquals (ed->getModelBox().getSelectedItemIndex(), 0, "Clean is shown");

            const auto snapshot = [&] { return panel.createComponentSnapshot (panel.getLocalBounds(), false, 1.0f); };
            const auto differs = [] (const juce::Image& a, const juce::Image& b)
            {
                if (a.getBounds() != b.getBounds())
                    return true;
                for (int y = 0; y < a.getHeight(); ++y)
                    for (int x = 0; x < a.getWidth(); ++x)
                        if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
                            return true;
                return false;
            };
            const auto plain = snapshot();

            ed->getModelBox().setSelectedItemIndex (1, juce::sendNotificationSync);   // Black
            expect (panel.isTextured(), "choosing a model switches the texture on immediately");
            expect (getParam (p, Comp::warmthId) > 0.5f, "and it is the Analog Warmth parameter that changed");
            expect (differs (plain, snapshot()), "the panel really looks different with the grain");

            ed->getModelBox().setSelectedItemIndex (0, juce::sendNotificationSync);   // Clean
            expect (! panel.isTextured(), "Clean switches it off immediately");
            expect (! differs (plain, snapshot()), "and the plain panel is exactly as it was");

            setParam (p, Comp::warmthId, 1.0f);
            expect (panel.isTextured(), "host automation switches it on");
            setParam (p, Comp::warmthId, 0.0f);
            expect (! panel.isTextured(), "and off");

            // Only the panel is textured: the rest of the window is not.
            expect (ed->getPauseButton().isVisible() && ed->getPanel().getParentComponent() != nullptr, "the panel is the floating one");

            // A saved session with a model on opens with the grain.
            setParam (p, Comp::warmthId, 1.0f);
            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            SongsmithCompressorAudioProcessor q;
            q.setStateInformation (saved.getData(), (int) saved.getSize());
            auto ed2 = make (q, Comp::EditorLayout::defaultWidth);
            expect (ed2->getPanel().isTextured(), "a restored session with a model on opens textured");
            expectEquals (ed2->getModelBox().getSelectedItemIndex(), 1, "with Black shown");

            // The grain is the faceplates' own: same streaks every time.
            const Songsmith::UI::BrushedGrain g1 (juce::Rectangle<float> (0.0f, 0.0f, 400.0f, 100.0f)), g2 (juce::Rectangle<float> (0.0f, 0.0f, 400.0f, 100.0f));
            expect (g1.light.getBounds() == g2.light.getBounds() && g1.dark.getBounds() == g2.dark.getBounds() && ! g1.light.isEmpty() && ! g1.dark.isEmpty(),
                    "built from a fixed seed, so identical every time");
        }
    }
};

//==============================================================================
// COMP-02: the graph's data path, the graph, the floating panel and the shared output meter's feed.
// songsmithstudio#14: Analog Warmth has the Analog Compressor's four models (Black, Blue, Anniversary, Lindell), run by the same shared code.
class WarmthModelTests final : public juce::UnitTest
{
public:
    WarmthModelTests() : juce::UnitTest ("Analog Warmth models (songsmithstudio#14)", "Compressor") {}

    using Models = Songsmith::AnalogModels::Model;

    // A steady-level signal (samples alternate +a and -a), so the detector sees a constant level and the gain reduction settles.
    static std::vector<float> steady (int n, float a)
    {
        std::vector<float> v ((size_t) n);
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = (i & 1) ? -a : a;
        return v;
    }

    static std::vector<float> sine (int n, double fs, double freq, double peakDb)
    {
        std::vector<float> v ((size_t) n);
        const double a = std::pow (10.0, peakDb / 20.0);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = (float) (a * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / fs));
        return v;
    }

    static float binDb (const std::vector<float>& x, double fs, double freq, size_t start)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = start; i < x.size(); ++i)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * freq * (double) i / fs;
            re += (double) x[i] * std::cos (w);
            im += (double) x[i] * std::sin (w);
        }
        return (float) (20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / (double) (x.size() - start) + 1.0e-12));
    }

    static SongsmithComp::Params params (bool warmth, int model)
    {
        SongsmithComp::Params p;
        p.thresholdDb = -24.0f; p.ratio = 4.0f; p.attackMs = 10.0f; p.releaseMs = 100.0f; p.kneeDb = 6.0f;
        p.makeupDb = 0.0f; p.autoMakeup = false; p.mix = 1.0f;
        p.warmth = warmth; p.warmthModel = model;
        return p;
    }

    // Runs 'in' through an engine; returns the output and the gain reduction at the end.
    static std::vector<float> through (const SongsmithComp::Params& p, const std::vector<float>& in, double fs, float* endReduction = nullptr)
    {
        SongsmithComp::Engine e;
        e.prepare (fs);
        e.setParams (p);
        e.reset();
        auto out = in;
        float* ch[] = { out.data() };
        e.process (ch, 1, (int) out.size());
        if (endReduction != nullptr)
            *endReduction = e.getGainReductionDb();
        return out;
    }

    void runTest() override
    {
        namespace AM = Songsmith::AnalogModels;

        beginTest ("The parameter: ID, version, the four names in order, Black by default; the dropdown's words come from the one shared table");
        {
            SongsmithCompressorAudioProcessor p;
            auto* rp = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (Comp::warmthModelId));
            expect (rp != nullptr);
            if (rp != nullptr)
            {
                expectEquals (rp->choices.joinIntoString ("/"), juce::String ("Blackface/Blue Stripe/Anniversary Edition/Lindell"), "the host's names are the Analog Compressor's");
                expectEquals (rp->getIndex(), 0, "Black by default");
                expectEquals (rp->getVersionHint(), 3);
            }
            expectEquals (juce::String (Comp::warmthModelId), juce::String ("warmth_model"));
            expectEquals (SongsmithCompressorAudioProcessor::stateVersion, 3);
            expect (! p.isWarmthClassic(), "a new instance uses the models");
            expectEquals (AM::numModels, 4);
            expectEquals (juce::String (AM::modelLabelTable[0]) + "/" + AM::modelLabelTable[1] + "/" + AM::modelLabelTable[2] + "/" + AM::modelLabelTable[3],
                          juce::String ("Black/Blue/Anniversary/Lindell"));
        }

        beginTest ("The same code as the Analog Compressor: with a model on, the output is exactly the shared FET cell and output stage applied to the gain-reduced sound");
        for (int model = 0; model < AM::numModels; ++model)
        {
            const double fs = 48000.0;
            const auto in = steady (int (fs * 5.0), 0.4f);
            auto p = params (true, model);
            float reduction = 0.0f;
            const auto out = through (p, in, fs, &reduction);
            expectGreaterThan (reduction, 1.0f, "the compressor is working (model " + juce::String (model) + ")");

            // The reference: the Analog Compressor's own classes, fed the same sound at the same gain reduction.
            const float r = juce::Decibels::decibelsToGain (-reduction);
            const float colour = AM::voicingFor (model).colourScale;
            AM::OutputStage stage;
            stage.prepare (fs);
            float worst = 0.0f;
            for (size_t i = 0; i < in.size(); ++i)
            {
                const float y = AM::FetCell::process (in[i] * r, r, colour);
                const float expected = stage.processSample (0, y);
                if (i > (size_t) (fs * 4.0))
                    worst = std::max (worst, std::abs (out[i] - expected));
            }
            expectLessThan (worst, 5.0e-4f, "model " + juce::String (model) + " matches the shared stages sample for sample (worst " + juce::String (worst, 6) + ")");
        }

        beginTest ("Black leaves the compression exactly as set; the other models move it as the Analog Compressor's do, and the colour follows the table");
        {
            const double fs = 48000.0;
            const auto loud = sine (int (fs * 3.0), fs, 1000.0, -6.0);
            float clean = 0.0f, black = 0.0f;
            through (params (false, 0), loud, fs, &clean);
            through (params (true, 0), loud, fs, &black);
            expectEquals (black, clean, "Black: the gain reduction is bit-identical to the clean compressor's");

            // Attack: Blue 0.7x, Black 1x, Lindell 1.6x, so a short burst is caught in that order.
            auto burst = std::vector<float> (int (fs * 0.0025), 0.0f);
            std::vector<float> quietThenLoud = steady (int (fs * 0.5), 0.01f);
            for (int i = 0; i < (int) burst.size(); ++i)
                quietThenLoud.push_back ((i & 1) ? -0.9f : 0.9f);
            float rBlue = 0, rBlack = 0, rLindell = 0;
            through (params (true, 1), quietThenLoud, fs, &rBlue);
            through (params (true, 0), quietThenLoud, fs, &rBlack);
            through (params (true, 3), quietThenLoud, fs, &rLindell);
            expect (rBlue > rBlack && rBlack > rLindell, "after 2.5 ms of a loud burst the reduction is Blue " + juce::String (rBlue, 2) + " > Black "
                    + juce::String (rBlack, 2) + " > Lindell " + juce::String (rLindell, 2) + " dB (attack 0.7x, 1x, 1.6x)");

            // Colour: the second harmonic follows the table (Blue 2.0, Black 1.0, Anniversary 0.6) at the same input.
            auto secondHarmonic = [&] (int model)
            {
                const auto out = through (params (true, model), sine (int (fs * 3.0), fs, 1000.0, -6.0), fs);
                return binDb (out, fs, 2000.0, (size_t) (fs * 2.0)) - binDb (out, fs, 1000.0, (size_t) (fs * 2.0));
            };
            const float h2Blue = secondHarmonic (1), h2Black = secondHarmonic (0), h2Anniversary = secondHarmonic (2);
            expect (h2Blue > h2Black && h2Black > h2Anniversary, "second harmonic: Blue " + juce::String (h2Blue, 1) + " > Black " + juce::String (h2Black, 1)
                    + " > Anniversary " + juce::String (h2Anniversary, 1) + " dB (colour 2.0, 1.0, 0.6)");
            expectLessThan (h2Blue, -30.0f, "and it stays a gentle warmth, not distortion");
        }

        beginTest ("Level matched: at a normal level every model keeps the loudness within half a decibel of the clean compressor");
        for (int model = 0; model < AM::numModels; ++model)
        {
            const double fs = 48000.0;
            const auto in = sine (int (fs * 3.0), fs, 1000.0, -18.0);
            auto cleanP = params (false, 0);
            cleanP.autoMakeup = true;
            auto warmP = params (true, model);
            warmP.autoMakeup = true;
            const float clean = binDb (through (cleanP, in, fs), fs, 1000.0, (size_t) (fs * 2.0));
            const float warm = binDb (through (warmP, in, fs), fs, 1000.0, (size_t) (fs * 2.0));
            expectWithinAbsoluteError (warm, clean, 0.5f, "model " + juce::String (model) + ": " + juce::String (warm, 2) + " dB against " + juce::String (clean, 2));
        }

        beginTest ("Click free: switching Analog Warmth on and off, and changing the model, while playing never steps the sound");
        {
            const double fs = 48000.0;
            const auto in = sine (int (fs * 2.0), fs, 220.0, -6.0);
            SongsmithComp::Engine e;
            e.prepare (fs);
            e.setParams (params (false, 0));
            e.reset();
            auto out = in;
            float biggestStep = 0.0f, previous = 0.0f;
            const int block = 480;
            for (int pos = 0; pos < (int) out.size(); pos += block)
            {
                const int step = pos / block;
                // every 12 blocks: warmth on/off, or a different model
                auto p = params ((step / 12) % 3 != 0, (step / 12) % 4);
                e.setParams (p);
                float* ch[] = { out.data() + pos };
                e.process (ch, 1, juce::jmin (block, (int) out.size() - pos));
                for (int i = pos; i < pos + block && i < (int) out.size(); ++i)
                {
                    if (i > 0)
                        biggestStep = std::max (biggestStep, std::abs (out[(size_t) i] - previous));
                    previous = out[(size_t) i];
                }
            }
            const float natural = (float) (2.0 * juce::MathConstants<double>::pi * 220.0 / fs * std::pow (10.0, -6.0 / 20.0));
            expectLessThan (biggestStep, natural * 1.3f + 0.003f, "the biggest sample-to-sample step is a 220 Hz sine's own (" + juce::String (natural, 4) + "), found " + juce::String (biggestStep, 4));
        }

        beginTest ("Off is exactly the clean compressor at every model, and the model choice is remembered while it is off");
        {
            const double fs = 48000.0;
            const auto in = noise (48000, 0.4f, 33);
            const auto clean = through (params (false, 0), in, fs);
            for (int model = 0; model < AM::numModels; ++model)
                expect (through (params (false, model), in, fs) == clean, "model " + juce::String (model) + " with Analog Warmth off is bit-identical");

            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::warmthModelId, 2.0f);
            setParam (p, Comp::warmthId, 1.0f);
            setParam (p, Comp::warmthId, 0.0f);
            setParam (p, Comp::warmthId, 1.0f);
            expectEquals (p.getWarmthModelIndex(), 2, "toggling Analog Warmth off and on keeps the model");
        }

        beginTest ("Older sessions keep their sound: with no model saved, Analog Warmth stays the original curve until a model is chosen");
        {
            const double fs = 48000.0;
            const auto in = noise (24000, 0.5f, 12);
            // A session saved by version 2: Analog Warmth on, no model in it.
            SongsmithCompressorAudioProcessor source;
            setParam (source, Comp::thresholdId, -20.0f);
            setParam (source, Comp::warmthId, 1.0f);
            auto old = source.createState();
            for (int i = old.getNumChildren() - 1; i >= 0; --i)
                if (old.getChild (i).getProperty ("id").toString() == Comp::warmthModelId)
                    old.removeChild (i, nullptr);
            old.setProperty ("stateVersion", 2, nullptr);
            for (int i = 0; i < old.getNumChildren(); ++i)
                if (old.getChild (i).getType() == juce::Identifier ("UI"))
                    old.getChild (i).removeProperty ("warmthClassic", nullptr);

            SongsmithCompressorAudioProcessor p;
            p.restoreFromValueTree (old);
            expect (p.isWarmthClassic(), "it opens in the classic Analog Warmth");
            prepare (p, fs, 512);
            const auto classic = run (p, { in, in }, 512)[0];

            // What the engine did before the models: the original curve.
            auto ep = params (true, -1);
            ep.thresholdDb = -20.0f; ep.ratio = 2.0f; ep.kneeDb = 6.0f; ep.autoMakeup = true;
            expect (through (ep, in, fs) == classic, "the output is exactly the original Analog Warmth, bit for bit");

            // Choosing a model replaces it, even Black, and the choice sticks across a save and reload.
            p.chooseWarmthModel (0);
            expect (! p.isWarmthClassic(), "choosing a model (even Black) ends the classic sound");
            SongsmithCompressorAudioProcessor q;
            q.restoreFromValueTree (p.createState());
            expect (! q.isWarmthClassic(), "and a session saved after that opens with the models");
            expectEquals (q.getWarmthModelIndex(), 0);

            // A classic session stays classic through a save and reload.
            SongsmithCompressorAudioProcessor r;
            r.restoreFromValueTree (old);
            SongsmithCompressorAudioProcessor s;
            s.restoreFromValueTree (r.createState());
            expect (s.isWarmthClassic(), "classic survives a save and reload until a model is chosen");

            // Host automation of the model also ends it; loading a preset does not touch it.
            SongsmithCompressorAudioProcessor a;
            a.restoreFromValueTree (old);
            a.applyPreset (0);
            expect (a.isWarmthClassic(), "a preset leaves the Analog Warmth choice alone");
            setParam (a, Comp::warmthModelId, 3.0f);
            expect (! a.isWarmthClassic(), "the host choosing a model ends it");

            // An empty or damaged session is a new one, with the models.
            SongsmithCompressorAudioProcessor fresh;
            expect (! fresh.isWarmthClassic());
            const char junk[] = "not a session";
            fresh.setStateInformation (junk, (int) sizeof (junk));
            expect (! fresh.isWarmthClassic(), "junk changes nothing");

            // Tests the original Analog Warmth with the models in the way: the model sound is different.
            SongsmithCompressorAudioProcessor m;
            setParam (m, Comp::thresholdId, -20.0f);
            setParam (m, Comp::warmthId, 1.0f);
            prepare (m, fs, 512);
            expect (run (m, { in, in }, 512)[0] != classic, "the models do not sound like the original curve");
        }

        beginTest ("Sessions and automation: the model is saved and restored, and a block size does not matter");
        {
            SongsmithCompressorAudioProcessor a;
            setParam (a, Comp::warmthId, 1.0f);
            setParam (a, Comp::warmthModelId, 3.0f);
            juce::MemoryBlock block;
            a.getStateInformation (block);
            SongsmithCompressorAudioProcessor b;
            b.setStateInformation (block.getData(), (int) block.getSize());
            expectEquals (b.getWarmthModelIndex(), 3);
            expect (b.apvts.getRawParameterValue (Comp::warmthId)->load() > 0.5f);
            expect (! b.isWarmthClassic());

            const auto in = noise (48000, 0.4f, 4);
            auto render = [&] (int blockSize)
            {
                SongsmithCompressorAudioProcessor p;
                setParam (p, Comp::thresholdId, -22.0f);
                setParam (p, Comp::warmthId, 1.0f);
                setParam (p, Comp::warmthModelId, 1.0f);
                prepare (p, 48000.0, blockSize);
                return run (p, { in, in }, blockSize)[0];
            };
            const auto x = render (64), y = render (480), z = render (4096);
            float worst = 0.0f;
            for (size_t i = 0; i < x.size(); ++i)
                worst = std::max ({ worst, std::abs (x[i] - y[i]), std::abs (x[i] - z[i]) });
            expectLessThan (worst, 1.0e-4f, "blocks of 64, 480 and 4,096 sound the same");
        }

        beginTest ("The one Model dropdown: Clean and the four models, no Analog Warmth button, compact, and it follows the session (songsmithstudio#23)");
        for (int width : { Comp::EditorLayout::minWidth, Comp::EditorLayout::defaultWidth, Comp::EditorLayout::maxWidth })
        {
            SongsmithCompressorAudioProcessor p;
            p.editorWidth = width;
            std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (p.createEditor()));
            auto& box = ed->getModelBox();
            const auto where = "at " + juce::String (width) + " px: ";
            expectEquals (box.getNumItems(), 5, where + "exactly five");
            expectEquals (box.getItemText (0), juce::String ("Clean"), where + "Clean first");
            for (int i = 0; i < 4; ++i)
                expectEquals (box.getItemText (i + 1), juce::String (AM::modelLabelTable[i]), where + "item " + juce::String (i + 1));
            expect (box.isVisible() && box.getTitle() == "Model", where + "visible and named");
            expect (box.isEnabled(), where + "always usable: there is no switch to turn on first");
            expectEquals (box.getSelectedItemIndex(), 0, where + "Clean is shown for a session with no analogue sound");

            // No separate Analog Warmth button remains anywhere in the toolbox.
            std::function<void (juce::Component&)> noWarmthButton = [&] (juce::Component& c)
            {
                if (auto* b = dynamic_cast<juce::Button*> (&c))
                    expect (! b->getButtonText().containsIgnoreCase ("warmth") && ! b->getTitle().containsIgnoreCase ("warmth"), where + "no Analog Warmth button (" + b->getButtonText() + ")");
                for (auto* child : c.getChildren())
                    noWarmthButton (*child);
            };
            noWarmthButton (*ed);

            // Compact: it sits at the left of the row Auto Makeup is on, level with it, with no gap where the toggle was.
            auto& autoMakeup = ed->getAutoMakeupButton();
            expect (box.getY() == autoMakeup.getY() && box.getHeight() == autoMakeup.getHeight(), where + "level with Auto Makeup");
            expect (box.getHeight() >= 26 && box.getRight() <= box.getParentComponent()->getWidth(), where + "a comfortable target, inside the panel");
            expect (! box.getBounds().intersects (autoMakeup.getBounds()), where + "clear of Auto Makeup");
            expect (box.getX() <= 16, where + "starts at the panel's left padding");
            const int fit = Songsmith::UI::Dropdown::widthFor (box, box.getLookAndFeel().getComboBoxFont (box));
            expectEquals (box.getWidth(), fit, where + "as wide as its longest item and the chevron need");
            Songsmith::UI::Wording::expectTextFits (*this, *ed, ed->getLookAndFeel(), "Compressor model " + where);

            // Choosing changes only the Analog Warmth and model parameters.
            std::vector<std::pair<juce::String, float>> others;
            for (auto* prm : p.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    if (rp->paramID != Comp::warmthId && rp->paramID != Comp::warmthModelId)
                        others.push_back ({ rp->paramID, rp->getValue() });
            box.setSelectedItemIndex (4, juce::sendNotificationSync);   // Lindell
            expectEquals (p.getWarmthModelIndex(), 3, where + "choosing Lindell sets the model");
            expect (getParam (p, Comp::warmthId) > 0.5f, where + "and switches the analogue sound on");
            expectEquals (box.getTooltip(), juce::String (AM::modelTooltipTable[3]), where + "the tooltip describes the model shown");
            bool untouched = true;
            for (auto& [id, v] : others)
                untouched = untouched && p.apvts.getParameter (id)->getValue() == v;
            expect (untouched, where + "Threshold, Ratio, Attack, Release, Knee, Makeup Gain, Mix and the rest do not move");
            setParam (p, Comp::warmthModelId, 1.0f);
            expectEquals (box.getSelectedItemIndex(), 2, where + "automation moves the dropdown (Blue)");
            expectEquals (box.getTooltip(), juce::String (AM::modelTooltipTable[1]), where + "and its tooltip");
            box.setSelectedItemIndex (0, juce::sendNotificationSync);
            expect (getParam (p, Comp::warmthId) < 0.5f && p.getWarmthModelIndex() == 1, where + "Clean switches the sound off and remembers Blue");
            expect (box.getTooltip().startsWith ("Clean"), where + "and the tooltip says what Clean is");
            box.setSelectedItemIndex (2, juce::sendNotificationSync);
            expect (getParam (p, Comp::warmthId) > 0.5f && p.getWarmthModelIndex() == 1, where + "choosing Blue again brings it back");

            // One Undo step undoes the choice.
            p.history.undo();
            expect (box.getSelectedItemIndex() == 0 && getParam (p, Comp::warmthId) < 0.5f, where + "Undo goes back to Clean in one step");
            p.history.redo();
            expectEquals (box.getSelectedItemIndex(), 2, where + "and Redo to Blue");
        }

        beginTest ("The toolbox takes the colours of the chosen model, readable, and Clean keeps the teal (songsmithstudio#23)");
        {
            namespace MT = Songsmith::UI::ModelTheme;
            // The Analog Compressor's own faceplate values (the shared table is what its editor draws from).
            expect (MT::forLook (2).tint == juce::Colour (0xff2f6bc4) && MT::forLook (3).tint == juce::Colour (0xff1c3f94) && MT::forLook (4).tint == juce::Colour (0xff9aa1ab),
                    "Blue, Anniversary and Lindell tints are the Analog Compressor's");
            expect (MT::forLook (1).accent == juce::Colour (0xffe8a33d), "Black's accent is the Analog Compressor's amber");
            expect (MT::forLook (0).accent == CompColours::accent, "Clean keeps the Compressor's teal");
            for (int a = 0; a < MT::numLooks; ++a)
                for (int b = a + 1; b < MT::numLooks; ++b)
                    expect (MT::forLook (a).accent != MT::forLook (b).accent, "no two looks share an accent");

            SongsmithCompressorAudioProcessor p;
            std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (p.createEditor()));
            auto& panel = ed->getPanel();
            auto contrast = [] (juce::Colour a, juce::Colour b)
            {
                auto lum = [] (juce::Colour c)
                {
                    auto f = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
                    return 0.2126f * f (c.getFloatRed()) + 0.7152f * f (c.getFloatGreen()) + 0.0722f * f (c.getFloatBlue());
                };
                const float la = lum (a), lb = lum (b);
                return (juce::jmax (la, lb) + 0.05f) / (juce::jmin (la, lb) + 0.05f);
            };
            const auto baseLabelColour = ed->getLookAndFeel().findColour (juce::Label::textColourId);
            for (int look = 0; look < MT::numLooks; ++look)
            {
                ed->getModelBox().setSelectedItemIndex (look, juce::sendNotificationSync);
                const auto where = "look " + juce::String (look) + ": ";
                const auto theme = MT::forLook (look);
                expect (panel.getTheme().accent == theme.accent && panel.getAccent() == theme.accent, where + "the panel outline and surface use the model's colours");
                for (auto* knob : { &ed->getThresholdSlider(), &ed->getRatioSlider(), &ed->getAttackSlider(), &ed->getReleaseSlider(),
                                    &ed->getKneeSlider(), &ed->getMakeupSlider(), &ed->getMixSlider() })
                    expect (knob->findColour (juce::Slider::rotarySliderFillColourId) == theme.accent, where + "every knob ring uses the accent");
                expect (ed->getLookAndFeel().findColour (juce::TextButton::buttonOnColourId) == theme.accent.withAlpha (0.85f), where + "lit buttons use the accent");
                // Readable: the accent against the darkest and the lightest the panel can be (its tint over the dark panel), 3:1 for graphics.
                const auto surface = Songsmith::UI::Colours::panel.interpolatedWith (theme.tintStrength > 0.0f ? theme.tint : Songsmith::UI::Colours::panel, theme.tintStrength);
                expectGreaterThan (contrast (theme.accent, surface), 3.0f, where + "the accent reads on the panel (3:1)");
                expectGreaterThan (contrast (Songsmith::UI::Colours::text, surface), 4.5f, where + "labels and values read on the panel (4.5:1)");
                expect (ed->getLookAndFeel().findColour (juce::Label::textColourId) == baseLabelColour, where + "label colours never change");
            }
            expect (ed->getGraph().getBounds().contains (panel.getBounds()), "the graph and the meters are not part of the theme");
            ed->getModelBox().setSelectedItemIndex (0, juce::sendNotificationSync);
            expect (panel.getAccent() == CompColours::accent, "back to Clean: the teal");
        }

        beginTest ("Saved sessions map to the dropdown: Analog Warmth off is Clean, on with a model is that model, on without one is Classic (songsmithstudio#23)");
        {
            for (int model = 0; model < 4; ++model)
            {
                SongsmithCompressorAudioProcessor a;
                setParam (a, Comp::warmthId, 1.0f);
                setParam (a, Comp::warmthModelId, (float) model);
                juce::MemoryBlock saved;
                a.getStateInformation (saved);
                SongsmithCompressorAudioProcessor b;
                b.setStateInformation (saved.getData(), (int) saved.getSize());
                std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (b.createEditor()));
                expectEquals (ed->getModelBox().getSelectedItemIndex(), model + 1, "warmth on with model " + juce::String (model) + " opens as that model");
            }
            SongsmithCompressorAudioProcessor off;
            setParam (off, Comp::warmthModelId, 2.0f);   // a model remembered while off
            juce::MemoryBlock saved;
            off.getStateInformation (saved);
            SongsmithCompressorAudioProcessor off2;
            off2.setStateInformation (saved.getData(), (int) saved.getSize());
            std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (off2.createEditor()));
            expectEquals (ed->getModelBox().getSelectedItemIndex(), 0, "warmth off opens as Clean, whatever model was remembered");
            expectEquals (off2.getWarmthModelIndex(), 2, "and the remembered model is kept");
            expect (! off2.getEngineParams().warmth, "and the sound is the clean compressor");
        }

        beginTest ("Each model has its own plain-words description, the same table as the Analog Compressor's, and a classic session reads Classic");
        {
            SongsmithCompressorAudioProcessor p;
            std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (p.createEditor()));
            for (int i = 0; i < 4; ++i)
            {
                const juce::String tip (AM::modelTooltipTable[i]);
                expect (Songsmith::UI::Wording::findBanned (tip).isEmpty(), "plain words: " + tip);
                expect (tip.length() <= 160 && tip.indexOfChar ('\n') < 0, "short, one paragraph, not a list of models");
                for (int j = 0; j < i; ++j)
                    expect (tip != juce::String (AM::modelTooltipTable[j]), "no two models share a description");
            }

            // A session from before the models shows Classic until one is chosen.
            SongsmithCompressorAudioProcessor old;
            setParam (old, Comp::warmthId, 1.0f);   // Analog Warmth was on, with no model saved
            auto state = old.createState();
            for (int i = state.getNumChildren() - 1; i >= 0; --i)
                if (state.getChild (i).getProperty ("id").toString() == Comp::warmthModelId)
                    state.removeChild (i, nullptr);
            SongsmithCompressorAudioProcessor q;
            q.restoreFromValueTree (state);
            std::unique_ptr<SongsmithCompressorAudioProcessorEditor> ed2 (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (q.createEditor()));
            auto& box2 = ed2->getModelBox();
            expectEquals (box2.getSelectedItemIndex(), -1, "nothing in the list is selected");
            expectEquals (box2.getTextWhenNothingSelected(), juce::String ("Classic"));
            expect (box2.getTooltip().startsWith ("Classic"), "and the tooltip says what Classic is");
            expectEquals (box2.getNumItems(), 5, "Clean and the four models");
            box2.setSelectedItemIndex (1, juce::sendNotificationSync);
            expect (! q.isWarmthClassic(), "choosing Black replaces Classic");
            expectEquals (box2.getSelectedItemIndex(), 1);
        }
    }
};
static WarmthModelTests warmthModelTests;

class CompressorDisplayTests final : public juce::UnitTest
{
public:
    CompressorDisplayTests() : juce::UnitTest ("Songsmith Compressor dynamics display", "Compressor") {}

    // Runs a steady-level signal (samples alternate +a and -a, so the peak is exactly a) and returns every graph column.
    static std::vector<Comp::GraphColumn> columnsFor (SongsmithCompressorAudioProcessor& p, double fs, int block, float leftDb, float rightDb, double seconds, int channels = 2, bool doPrepare = true)
    {
        if (doPrepare)
            prepare (p, fs, block, channels);
        const int total = (int) (fs * seconds);
        const float a = std::pow (10.0f, leftDb / 20.0f), b = std::pow (10.0f, rightDb / 20.0f);
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i) { l[(size_t) i] = (i & 1) ? -a : a; r[(size_t) i] = (i & 1) ? -b : b; }
        std::vector<std::vector<float>> in { l };
        if (channels == 2) in.push_back (r);
        std::vector<Comp::GraphColumn> out;
        Comp::GraphColumn tmp[256];
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> buffer (channels, n);
            for (int c = 0; c < channels; ++c)
                buffer.copyFrom (c, 0, in[(size_t) c].data() + pos, n);
            p.processBlock (buffer, midi);
            for (int k; (k = p.graphFifo.popAll (tmp, 256)) > 0;)
                out.insert (out.end(), tmp, tmp + k);
        }
        return out;
    }

    void runTest() override
    {
        beginTest ("One column per 10 ms at 44.1, 48 and 96 kHz and any block size, and the columns do not depend on the block size");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            std::vector<Comp::GraphColumn> reference;
            for (int block : { 1024, 480, 37, 1 })
            {
                if (block == 1 && fs > 50000.0) continue;   // one sample at a time at 96 kHz is slow and adds nothing
                SongsmithCompressorAudioProcessor p;
                setParam (p, Comp::thresholdId, -30.0f);
                setParam (p, Comp::ratioId, 4.0f);
                const auto cols = columnsFor (p, fs, block, -10.0f, -10.0f, 0.5, 2);
                const int expected = (int) (fs * 0.5 / std::round (fs * 0.01));
                expectEquals ((int) cols.size(), expected, "column count at " + juce::String (fs) + " Hz, block " + juce::String (block));
                if (reference.empty()) reference = cols;
                else for (size_t i = 0; i < cols.size() && i < reference.size(); ++i)
                    expect (cols[i].inDb == reference[i].inDb && cols[i].outDb == reference[i].outDb && cols[i].grDb == reference[i].grDb, "identical columns for block " + juce::String (block));
            }
        }

        beginTest ("The columns are the measured signal: output is input minus gain reduction, plus Makeup Gain only when it is set");
        {
            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::thresholdId, -30.0f); setParam (p, Comp::ratioId, 4.0f); setParam (p, Comp::kneeId, 0.0f);
            setParam (p, Comp::attackId, 1.0f); setParam (p, Comp::autoMakeupId, 0.0f); setParam (p, Comp::makeupId, 0.0f);
            const auto cols = columnsFor (p, 48000.0, 480, -10.0f, -10.0f, 1.5);
            const auto& last = cols.back();
            expectWithinAbsoluteError (last.inDb, -10.0f, 0.01f, "input level");
            expectWithinAbsoluteError (last.grDb, 15.0f, 0.2f, "20 dB over at 4:1 is 15 dB of reduction");
            expectWithinAbsoluteError (last.outDb, last.inDb - last.grDb, 0.1f, "output is input minus the reduction");
            expectGreaterThan (cols.front().inDb - cols.front().outDb, -0.01f, "the red line is never above the grey one without Makeup Gain");

            SongsmithCompressorAudioProcessor q;
            setParam (q, Comp::thresholdId, -30.0f); setParam (q, Comp::ratioId, 4.0f); setParam (q, Comp::kneeId, 0.0f);
            setParam (q, Comp::attackId, 1.0f); setParam (q, Comp::autoMakeupId, 0.0f); setParam (q, Comp::makeupId, 6.0f);
            const auto withMakeup = columnsFor (q, 48000.0, 480, -10.0f, -10.0f, 1.5).back();
            expectWithinAbsoluteError (withMakeup.grDb, last.grDb, 0.05f, "Makeup Gain does not change the reduction");
            expectWithinAbsoluteError (withMakeup.outDb, withMakeup.inDb - withMakeup.grDb + 6.0f, 0.1f, "it only lifts the output line");
        }

        beginTest ("Attack and release show in the curves: slower attack, later reduction; reduction recovers after the sound stops");
        {
            auto timeToReduce = [] (float attackMs)
            {
                SongsmithCompressorAudioProcessor p;
                setParam (p, Comp::thresholdId, -30.0f); setParam (p, Comp::ratioId, 10.0f); setParam (p, Comp::kneeId, 0.0f);
                setParam (p, Comp::attackId, attackMs);
                const auto cols = columnsFor (p, 48000.0, 480, -6.0f, -6.0f, 1.0);
                for (size_t i = 0; i < cols.size(); ++i)
                    if (cols[i].grDb > 0.5f * cols.back().grDb) return (int) i;
                return -1;
            };
            const int fast = timeToReduce (1.0f), slow = timeToReduce (60.0f);
            expectGreaterThan (slow, fast + 2, "a slower attack reaches half its reduction later");

            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::thresholdId, -30.0f); setParam (p, Comp::ratioId, 10.0f); setParam (p, Comp::releaseId, 200.0f);
            auto loud = columnsFor (p, 48000.0, 480, -6.0f, -6.0f, 0.5);
            const auto quiet = columnsFor (p, 48000.0, 480, -80.0f, -80.0f, 1.5, 2, false);   // carries straight on, no new prepare
            expectGreaterThan (loud.back().grDb, 10.0f);
            expectGreaterThan (quiet.front().grDb, 0.0f, "the reduction does not vanish the instant the sound stops");
            expectLessThan (quiet.back().grDb, 0.1f, "and has recovered after the release time");
            expectLessThan (quiet.back().inDb, -70.0f, "silence reads as very quiet, not as a made-up level");
        }

        beginTest ("Stereo is the linked maximum, mono works, and bypass shows no reduction");
        {
            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::thresholdId, -30.0f); setParam (p, Comp::ratioId, 4.0f);
            const auto stereo = columnsFor (p, 48000.0, 480, -8.0f, -40.0f, 0.5).back();
            expectWithinAbsoluteError (stereo.inDb, -8.0f, 0.01f, "the louder channel sets the input level");
            expectGreaterThan (stereo.grDb, 5.0f, "and drives the reduction, as in the detector");

            SongsmithCompressorAudioProcessor m;
            setParam (m, Comp::thresholdId, -30.0f); setParam (m, Comp::ratioId, 4.0f);
            const auto mono = columnsFor (m, 48000.0, 480, -8.0f, -8.0f, 0.5, 1).back();
            expectWithinAbsoluteError (mono.inDb, -8.0f, 0.01f);
            expectWithinAbsoluteError (mono.grDb, stereo.grDb, 0.3f, "mono and stereo agree");

            SongsmithCompressorAudioProcessor b;
            setParam (b, Comp::thresholdId, -40.0f); setParam (b, Comp::ratioId, 20.0f); setParam (b, Comp::bypassId, 1.0f);
            const auto cols = columnsFor (b, 48000.0, 480, -6.0f, -6.0f, 1.0);
            expect (cols.back().bypassed, "marked as bypassed");
            expectEquals (cols.back().grDb, 0.0f, "never a made-up reduction while bypassed");
            expectWithinAbsoluteError (cols.back().outDb, cols.back().inDb, 0.001f, "and the output line lies on the input line");
        }

        beginTest ("The hand-over is lock-free and bounded: oldest first, newest dropped when full, nothing blocks");
        {
            Comp::ColumnFifo fifo;
            for (int i = 0; i < Comp::ColumnFifo::capacity + 50; ++i)
            {
                Comp::GraphColumn c; c.inDb = (float) i;
                const bool ok = fifo.push (c);
                expect (ok == (i < Comp::ColumnFifo::capacity - 1), "accepted until full (one slot is the ring buffer's gap), then dropped");
            }
            std::vector<Comp::GraphColumn> out ((size_t) Comp::ColumnFifo::capacity);
            expectEquals (fifo.popAll (out.data(), (int) out.size()), Comp::ColumnFifo::capacity - 1);
            expectEquals (out.front().inDb, 0.0f, "oldest first");
            expectEquals (out[(size_t) (Comp::ColumnFifo::capacity - 2)].inDb, (float) (Comp::ColumnFifo::capacity - 2), "in order, and nothing newer than the last accepted");
            expectEquals (fifo.popAll (out.data(), 8), 0, "empty after reading");
            static_assert (std::is_trivially_copyable_v<Comp::GraphColumn>, "plain data, so nothing allocates on the audio thread");
        }

        beginTest ("The graph keeps the last 8 seconds, scrolls newest to the right, pauses honestly and draws at every size");
        {
            Comp::DynamicsGraph graph;
            for (int i = 0; i < Comp::graphColumns + 100; ++i)
            {
                Comp::GraphColumn c; c.inDb = (float) i; c.outDb = (float) i - 3.0f; c.grDb = 3.0f;
                graph.push (c);
            }
            expectEquals (graph.getNumColumns(), Comp::graphColumns, "the window is full, not growing");
            expectEquals (graph.getColumn (graph.getNumColumns() - 1).inDb, (float) (Comp::graphColumns + 99), "newest last");
            expectEquals (graph.getColumn (0).inDb, 100.0f, "the oldest 100 have scrolled off");

            graph.setPaused (true);
            Comp::GraphColumn extra; extra.inDb = -1.0f;
            graph.push (extra);
            expectEquals (graph.getColumn (graph.getNumColumns() - 1).inDb, (float) (Comp::graphColumns + 99), "paused: the picture holds");
            graph.setPaused (false);
            graph.push (extra);
            expectEquals (graph.getColumn (graph.getNumColumns() - 1).inDb, -1.0f, "resumed");
            graph.clearHistory();
            expectEquals (graph.getNumColumns(), 0);

            for (auto size : { juce::Point<int> (300, 150), juce::Point<int> (772, 408), juce::Point<int> (2200, 1200) })
            {
                graph.setSize (size.x, size.y);
                graph.setThresholdDb (-24.0f);
                for (int i = 0; i < 500; ++i)
                {
                    Comp::GraphColumn c; c.inDb = -30.0f + 20.0f * std::sin ((float) i * 0.05f); c.outDb = c.inDb - 6.0f; c.grDb = 6.0f;
                    graph.push (c);
                }
                const auto image = graph.createComponentSnapshot (graph.getLocalBounds(), false, 1.0f);
                expect (image.isValid() && image.getWidth() == size.x, "draws at " + juce::String (size.x) + " px");
            }
            expect (graph.getTooltip().contains ("not true peaks") && graph.getTooltip().contains ("Makeup Gain"), "the legend in the tooltip");
        }

        beginTest ("The floating panel stays inside, drags, remembers its place and survives resizing and old sessions");
        {
            Songsmith::UI::FloatingPanel panel;
            juce::Component parent;
            parent.setSize (800, 500);
            parent.addAndMakeVisible (panel);
            panel.setSize (300, 100);
            panel.setLimits ({ 20, 40, 600, 400 });
            panel.setRelativePosition ({ 0.0f, 0.0f });
            expect (panel.getPosition() == juce::Point<int> (20, 40), "top left of the limits");
            panel.setRelativePosition ({ 1.0f, 1.0f });
            expect (panel.getRight() == 620 && panel.getBottom() == 440, "bottom right of the limits");
            panel.setRelativePosition ({ 2.0f, std::numeric_limits<float>::quiet_NaN() });
            expect (panel.getRight() == 620 && panel.getY() == 40, "out-of-range and non-finite fractions are made safe");

            panel.setRelativePosition ({ 0.5f, 0.5f });
            const auto centre = panel.getBounds();
            int moved = 0;
            panel.onUserMoved = [&] { ++moved; };
            const auto now = juce::Time::getCurrentTime();
            const auto down = juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 50.0f, 50.0f }, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &panel, &panel, now, { 50.0f, 50.0f }, now, 1, false);
            panel.mouseDown (down);
            const auto drag = juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 5000.0f, -5000.0f }, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &panel, &panel, now, { 50.0f, 50.0f }, now, 1, true);
            panel.mouseDrag (drag);
            expect (moved > 0, "reports a user move");
            expect (juce::Rectangle<int> (20, 40, 600, 400).contains (panel.getBounds()), "dragging far away keeps it inside");
            expect (panel.getBounds() != centre, "and it moved");

            panel.setLimits ({ 0, 0, 200, 60 });   // smaller than the panel: the top-left corner stays reachable
            expect (panel.getPosition() == juce::Point<int> (0, 0));

            // In the plugin: saved with the session, safe for an old one.
            SongsmithCompressorAudioProcessor p;
            p.panelX = 0.25f; p.panelY = 0.75f;
            juce::MemoryBlock block;
            p.getStateInformation (block);
            SongsmithCompressorAudioProcessor q;
            q.setStateInformation (block.getData(), (int) block.getSize());
            expectWithinAbsoluteError (q.panelX.load(), 0.25f, 1.0e-6f);
            expectWithinAbsoluteError (q.panelY.load(), 0.75f, 1.0e-6f);

            auto tree = p.createState();
            tree.getChildWithName ("UI").removeProperty ("panelX", nullptr);
            tree.getChildWithName ("UI").setProperty ("panelY", 9.0f, nullptr);
            SongsmithCompressorAudioProcessor old;
            old.restoreFromValueTree (tree);
            expectWithinAbsoluteError (old.panelX.load(), 0.03f, 1.0e-6f, "a session without it opens with the default place");
            expectWithinAbsoluteError (old.panelY.load(), 1.0f, 1.0e-6f, "and a wild value is clamped");

            // The editor puts the panel where the session says, at any size, and a drag is saved.
            for (int width : { Comp::EditorLayout::minWidth, Comp::EditorLayout::maxWidth })
            {
                SongsmithCompressorAudioProcessor e;
                e.panelX = 0.0f; e.panelY = 0.0f;
                auto ed = CompressorEditorTests::make (e, width);
                const auto graphBounds = ed->getGraph().getBounds().reduced (8);
                expect (ed->getPanel().getPosition() == graphBounds.getPosition(), "top left of the graph when saved there, at " + juce::String (width));
                ed->getPanel().setRelativePosition ({ 1.0f, 0.0f });
                if (ed->getPanel().onUserMoved) ed->getPanel().onUserMoved();
                expectWithinAbsoluteError (e.panelX.load(), 1.0f, 1.0e-6f, "a move is remembered");
            }
        }

        beginTest ("The output meter is fed as the Songsmith EQ feeds it: per channel, mono and stereo, and a latched CLIP that clears on silence");
        {
            SongsmithCompressorAudioProcessor p;
            setParam (p, Comp::thresholdId, 0.0f); setParam (p, Comp::kneeId, 0.0f); setParam (p, Comp::ratioId, 1.0f); setParam (p, Comp::autoMakeupId, 0.0f);
            prepare (p, 48000.0, 480, 2);
            expectEquals (p.outputMeter.channels.load(), 2);
            const auto cols = columnsFor (p, 48000.0, 480, -1.0f, -6.0f, 0.2);
            juce::ignoreUnused (cols);
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (p.outputMeter.readAndResetChannel (0)), -1.0f, 0.3f, "left peak, not just the larger one");
            expectWithinAbsoluteError (juce::Decibels::gainToDecibels (p.outputMeter.readAndResetChannel (1)), -6.0f, 0.3f, "right peak");
            expect (! p.outputMeter.clipLatched.load(), "-1 dBFS does not light CLIP");

            SongsmithCompressorAudioProcessor c;
            setParam (c, Comp::thresholdId, 0.0f); setParam (c, Comp::kneeId, 0.0f); setParam (c, Comp::ratioId, 1.0f); setParam (c, Comp::autoMakeupId, 0.0f);
            columnsFor (c, 48000.0, 480, 0.0f, -20.0f, 0.1);   // exactly 0 dBFS
            expect (c.outputMeter.clipLatched.load(), "0 dBFS lights CLIP: the same boundary as the EQ");
            columnsFor (c, 48000.0, 480, -20.0f, -20.0f, 0.3, 2, false);
            expect (c.outputMeter.clipLatched.load(), "and stays lit while sound continues");
            columnsFor (c, 48000.0, 480, -140.0f, -140.0f, 1.0, 2, false);
            expect (! c.outputMeter.clipLatched.load(), "and clears after sustained silence");

            SongsmithCompressorAudioProcessor m;
            prepare (m, 48000.0, 480, 1);
            expectEquals (m.outputMeter.channels.load(), 1, "mono shows one bar");
        }
    }
};

static CompressorPluginTests compressorPluginTests;
static CompressorDisplayTests compressorDisplayTests;
static CompressorEditorTests compressorEditorTests;

//==============================================================================
static SongsmithUITests sharedUITests { "Compressor" };
static SongsmithDiagnosticsTests sharedDiagnosticsTests { "Compressor" };
static SongsmithGuidanceTests sharedGuidanceTests { "Compressor" };
static SongsmithUpdateTests sharedUpdateTests { "Compressor" };
static SongsmithBeforeAfterTests<SongsmithCompressorAudioProcessor, SongsmithCompressorAudioProcessorEditor> sharedBeforeAfterTests { "Compressor" };

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// CompressorTests --screenshot <file.png> [--scale 2]: renders the editor with some compression on the
// meters to a PNG, so UI changes can be checked without a host.
// look: 0 Clean, 1 to 4 Black, Blue, Anniversary, Lindell (songsmithstudio#23)
static int renderScreenshot (const juce::File& file, float scale, int look = 0)
{
    SongsmithCompressorAudioProcessor p;
    p.applyPreset (0);
    if (look > 0)
    {
        setParam (p, Comp::warmthId, 1.0f);
        setParam (p, Comp::warmthModelId, (float) (look - 1));
    }
    prepare (p, 48000.0, 512);
    std::unique_ptr<SongsmithCompressorAudioProcessorEditor> editor (dynamic_cast<SongsmithCompressorAudioProcessorEditor*> (p.createEditor()));
    // Six seconds of plucked notes, each one louder than the last and dying away, so the graph has something to show.
    std::vector<float> in (288000);
    for (size_t i = 0; i < in.size(); ++i)
    {
        const double sec = (double) i / 48000.0, local = std::fmod (sec, 0.75);
        const double pluck = std::exp (-local * 3.2) * (0.25 + 0.65 * std::fmod (std::floor (sec / 0.75) * 0.37, 1.0));
        in[i] = (float) (pluck * std::sin (2.0 * 3.14159265358979 * (110.0 + 55.0 * std::floor (sec / 0.75)) * sec));
    }
    run (p, { in, in }, 512);
    editor->updateMeters (0.02);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, out))
        return 1;
    std::printf ("Wrote %s\n", file.getFullPathName().toRawUTF8());

    CompLookAndFeel lf;
    Songsmith::UI::Testing::writeControlSheet (lf, CompColours::background, scale,
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
            int look = 0;
            for (int j = 1; j + 1 < argc; ++j)
            {
                if (juce::String (argv[j]) == "--look")
                    look = juce::jlimit (0, 4, juce::String (argv[j + 1]).getIntValue());
                if (juce::String (argv[j]) == "--warmth")
                    look = 1;
            }
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale, look);
        }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("Compressor");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
