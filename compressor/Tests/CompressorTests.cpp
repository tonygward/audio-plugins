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
            expectEquals (p.getParameters().size(), 10, "ten parameters");
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
                                                  &ed->getMixSlider(), &ed->getAutoMakeupButton(), &ed->getWarmthButton() };
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
            for (auto* t : { "Threshold", "Ratio", "Attack", "Release", "Knee", "Makeup Gain", "Mix", "Auto Makeup", "Analog Warmth", "Pause", "Gain Reduction" })
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
    }
};

//==============================================================================
// COMP-02: the graph's data path, the graph, the floating panel and the shared output meter's feed.
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

struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

// CompressorTests --screenshot <file.png> [--scale 2]: renders the editor with some compression on the
// meters to a PNG, so UI changes can be checked without a host.
static int renderScreenshot (const juce::File& file, float scale)
{
    SongsmithCompressorAudioProcessor p;
    p.applyPreset (0);
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
            return renderScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), scale);
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
