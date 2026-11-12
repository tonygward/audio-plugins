// Songsmith Analog Compressor test runner.
//   AnalogCompressorTests.exe          -> run all unit tests (exit code 1 on failure)
#include <set>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "../../shared/Tests/SongsmithHeaderTests.h"
#include "../../shared/Tests/SongsmithDiagnosticsTests.h"
#include "EngineMeasure.h"
#include "ColourMeasure.h"

using namespace FET;

namespace
{
    using Set = juce::AudioChannelSet;

    juce::AudioProcessor::BusesLayout layout (const Set& in, const Set& out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    }

    void setParam (juce::AudioProcessor& p, const juce::String& id, float plainValue)
    {
        auto& proc = dynamic_cast<SongsmithAnalogCompressorProcessor&> (p);
        auto* rp = proc.apvts.getParameter (id);
        rp->setValueNotifyingHost (rp->convertTo0to1 (plainValue));
    }

    float getParam (SongsmithAnalogCompressorProcessor& p, const juce::String& id)
    {
        return p.apvts.getRawParameterValue (id)->load();
    }

    void fillSine (juce::AudioBuffer<float>& b, double fs, double freq, float amp, int offset = 0)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (offset + i) / fs + c * 0.3));
    }
}

//==============================================================================
struct IdentityTests final : juce::UnitTest
{
    IdentityTests() : juce::UnitTest ("Identity", "FET") {}

    void runTest() override
    {
        beginTest ("plugin and manufacturer codes are pinned");
        expectEquals (juce::String (FET_MANUFACTURER_CODE), juce::String ("Tnyw"));
        expectEquals (juce::String (FET_PLUGIN_CODE), juce::String ("Sfc1"));
        expect (juce::String (FET_PLUGIN_CODE) != "Eq3b", "must differ from Songsmith EQ");
        expectEquals (juce::String (JucePlugin_Name), juce::String ("Songsmith Analog Compressor"));
        expectEquals (juce::String (JucePlugin_Manufacturer), juce::String ("Songsmith Studio"));

        beginTest ("parameter IDs, ranges and defaults are pinned");
        SongsmithAnalogCompressorProcessor p;

        struct Expected { const char* id; float min, max, def; int version = 1; };
        const Expected expected[] = {
            { "input",      -24.0f, 48.0f, 0.0f },
            { "output",     -36.0f, 24.0f, 0.0f },
            { "attack",       1.0f,  7.0f, 4.0f },
            { "release",      1.0f,  7.0f, 4.0f },
            { "ratio",        0.0f,  3.0f, 0.0f },
            { "allButtons",   0.0f,  1.0f, 0.0f },
            { "grOff",        0.0f,  1.0f, 0.0f },
            { "bypass",       0.0f,  1.0f, 0.0f },
            { "oversampling", 0.0f,  2.0f, 0.0f, 2 },
            { "detectorLowCut", 0.0f, 5.0f, 0.0f, 3 },
            { "ratioTwoToOne", 0.0f, 1.0f, 0.0f, 4 },   // ACOMP-11: the Songsmith 2:1 switch
        };

        expectEquals (p.getParameters().size(), (int) std::size (expected), "no parameters added or removed silently");

        for (const auto& e : expected)
        {
            auto* rp = p.apvts.getParameter (e.id);
            expect (rp != nullptr, juce::String ("parameter exists: ") + e.id);
            if (rp == nullptr)
                continue;

            const auto& r = rp->getNormalisableRange();
            expectWithinAbsoluteError (r.start, e.min, 1.0e-6f, e.id);
            expectWithinAbsoluteError (r.end, e.max, 1.0e-6f, e.id);
            expectWithinAbsoluteError (rp->convertFrom0to1 (rp->getDefaultValue()), e.def, 1.0e-4f, e.id);

            if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*> (rp))
                expectEquals (withId->getParameterID(), juce::String (e.id));
            expectEquals (rp->getVersionHint(), e.version, e.id);
        }

        expectEquals (ratioNames().joinIntoString (","), juce::String ("4:1,8:1,12:1,20:1"));

        beginTest ("host bypass parameter is the bypass ID");
        expect (p.getBypassParameter() == p.apvts.getParameter ("bypass"));
    }
};

//==============================================================================
struct LayoutTests final : juce::UnitTest
{
    LayoutTests() : juce::UnitTest ("Bus layouts", "FET") {}

    void runTest() override
    {
        SongsmithAnalogCompressorProcessor p;

        beginTest ("mono and stereo matching layouts are supported");
        expect (p.checkBusesLayoutSupported (layout (Set::mono(), Set::mono())), "mono");
        expect (p.checkBusesLayoutSupported (layout (Set::stereo(), Set::stereo())), "stereo");

        beginTest ("mismatched and surround layouts are rejected");
        expect (! p.checkBusesLayoutSupported (layout (Set::mono(), Set::stereo())), "mono -> stereo");
        expect (! p.checkBusesLayoutSupported (layout (Set::stereo(), Set::mono())), "stereo -> mono");
        expect (! p.checkBusesLayoutSupported (layout (Set::create5point1(), Set::create5point1())), "5.1");

        beginTest ("mono and stereo both process");
        for (auto set : { Set::mono(), Set::stereo() })
        {
            SongsmithAnalogCompressorProcessor q;
            expect (q.setBusesLayout (layout (set, set)));
            q.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> b (set.size(), 256);
            fillSine (b, 48000.0, 1000.0, 0.5f);
            juce::MidiBuffer m;
            q.processBlock (b, m);
            for (int c = 0; c < b.getNumChannels(); ++c)
                expectGreaterThan (b.getMagnitude (c, 0, 256), 0.1f, "signal passes");
        }
    }
};

//==============================================================================
struct TransparencyTests final : juce::UnitTest
{
    TransparencyTests() : juce::UnitTest ("Default transparency", "FET") {}

    void runTest() override
    {
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            beginTest ("clean engine without oversampling is bit-exact below the threshold at " + juce::String (fs, 0) + " Hz");
            SongsmithAnalogCompressorProcessor p;
            p.setColourEnabled (false);
            setParam (p, ID::oversampling, 0.0f);
            p.setBusesLayout (layout (Set::stereo(), Set::stereo()));
            p.prepareToPlay (fs, 512);

            juce::MidiBuffer m;
            bool exact = true;
            int offset = 0;
            for (int block : { 512, 1, 37, 512, 100, 3 })
            {
                juce::AudioBuffer<float> b (2, block), ref (2, block);
                fillSine (b, fs, 997.0, 0.01f, offset);   // -40 dBFS: below every knee
                ref.makeCopyOf (b);
                p.processBlock (b, m);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < block; ++i)
                        exact = exact && b.getSample (c, i) == ref.getSample (c, i);
                offset += block;
            }
            expect (exact, "output equals input sample for sample");
        }

        beginTest ("bypass settles to bit-exact pass-through");
        {
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::oversampling, 0.0f);
            p.prepareToPlay (48000.0, 512);
            setParam (p, ID::output, -12.0f);
            setParam (p, ID::bypass, 1.0f);

            juce::MidiBuffer m;
            juce::AudioBuffer<float> b (2, 512), ref (2, 512);
            for (int k = 0; k < 4; ++k)
            {
                fillSine (b, 48000.0, 440.0, 0.5f, k * 512);
                p.processBlock (b, m);
            }
            fillSine (b, 48000.0, 440.0, 0.5f, 4 * 512);
            ref.makeCopyOf (b);
            p.processBlock (b, m);

            bool exact = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    exact = exact && b.getSample (c, i) == ref.getSample (c, i);
            expect (exact);
        }

        beginTest ("silence stays silent");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (44100.0, 64);
            juce::AudioBuffer<float> b (2, 64);
            b.clear();
            juce::MidiBuffer m;
            p.processBlock (b, m);
            expectEquals (b.getMagnitude (0, 64), 0.0f);
        }
    }
};

//==============================================================================
struct StateTests final : juce::UnitTest
{
    StateTests() : juce::UnitTest ("State", "FET") {}

    void runTest() override
    {
        beginTest ("state round-trips every parameter");
        juce::MemoryBlock saved;
        {
            SongsmithAnalogCompressorProcessor a;
            setParam (a, ID::input, 18.5f);
            setParam (a, ID::output, -7.25f);
            setParam (a, ID::attack, 6.5f);
            setParam (a, ID::release, 2.0f);
            setParam (a, ID::ratio, 2.0f);
            setParam (a, ID::allButtons, 1.0f);
            setParam (a, ID::grOff, 1.0f);
            setParam (a, ID::bypass, 1.0f);
            setParam (a, ID::detectorLowCut, 3.0f);
            a.getStateInformation (saved);
        }

        SongsmithAnalogCompressorProcessor b;
        b.setStateInformation (saved.getData(), (int) saved.getSize());
        expectWithinAbsoluteError (getParam (b, ID::input), 18.5f, 0.01f);
        expectWithinAbsoluteError (getParam (b, ID::output), -7.25f, 0.01f);
        expectWithinAbsoluteError (getParam (b, ID::attack), 6.5f, 0.01f);
        expectWithinAbsoluteError (getParam (b, ID::release), 2.0f, 0.01f);
        expectEquals ((int) getParam (b, ID::ratio), 2);
        expectEquals ((int) getParam (b, ID::allButtons), 1);
        expectEquals ((int) getParam (b, ID::grOff), 1);
        expectEquals ((int) getParam (b, ID::bypass), 1);
        expectEquals ((int) getParam (b, ID::detectorLowCut), 3, "Low Cut is recalled");

        beginTest ("a fresh instance has Low Cut Off");
        {
            SongsmithAnalogCompressorProcessor fresh;
            expectEquals ((int) getParam (fresh, ID::detectorLowCut), 0);
            expectEquals (FET::lowCutHz[(int) getParam (fresh, ID::detectorLowCut)], 0.0f);
        }

        beginTest ("sessions saved before Low Cut existed (state versions 1 and 2) load it Off, not at its current value");
        for (int version : { 1, 2 })
        {
            SongsmithAnalogCompressorProcessor c;
            setParam (c, ID::detectorLowCut, 4.0f);
            setParam (c, ID::input, 30.0f);

            juce::XmlElement old ("SongsmithAnalogCompressor");
            old.setAttribute ("stateVersion", version);
            auto* child = old.createNewChildElement ("PARAM");
            child->setAttribute ("id", "input");
            child->setAttribute ("value", 12.0);
            if (version == 2)
            {
                auto* os = old.createNewChildElement ("PARAM");
                os->setAttribute ("id", "oversampling");
                os->setAttribute ("value", 2.0);
            }
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (old, mb);
            c.setStateInformation (mb.getData(), (int) mb.getSize());

            expectEquals ((int) getParam (c, ID::detectorLowCut), 0, "Low Cut is Off for a version " + juce::String (version) + " session");
            expectWithinAbsoluteError (getParam (c, ID::input), 12.0f, 0.01f, "the rest of the session still loads");
            if (version == 2)
                expectEquals ((int) getParam (c, ID::oversampling), 2, "version 2 oversampling is kept");
        }

        beginTest ("state carries a version");
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        expect (xml != nullptr);
        if (xml != nullptr)
            expectEquals (xml->getIntAttribute ("stateVersion"), SongsmithAnalogCompressorProcessor::stateVersion);

        beginTest ("missing parameters fall back to defaults, not current values");
        {
            SongsmithAnalogCompressorProcessor c;
            setParam (c, ID::release, 7.0f);
            setParam (c, ID::input, 30.0f);

            juce::XmlElement partial ("SongsmithAnalogCompressor");
            partial.setAttribute ("stateVersion", 1);
            auto* child = partial.createNewChildElement ("PARAM");
            child->setAttribute ("id", "input");
            child->setAttribute ("value", 6.0);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (partial, mb);
            c.setStateInformation (mb.getData(), (int) mb.getSize());

            expectWithinAbsoluteError (getParam (c, ID::input), 6.0f, 0.01f);
            expectWithinAbsoluteError (getParam (c, ID::release), releaseDefault, 0.01f);
        }

        beginTest ("out-of-range and foreign state is ignored or clamped");
        {
            SongsmithAnalogCompressorProcessor c;
            juce::XmlElement wild ("SongsmithAnalogCompressor");
            auto* child = wild.createNewChildElement ("PARAM");
            child->setAttribute ("id", "attack");
            child->setAttribute ("value", 99.0);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (wild, mb);
            c.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (getParam (c, ID::attack), knobMax, 0.01f);

            setParam (c, ID::attack, 2.0f);
            juce::XmlElement other ("SomethingElse");
            juce::AudioProcessor::copyXmlToBinary (other, mb);
            c.setStateInformation (mb.getData(), (int) mb.getSize());
            expectWithinAbsoluteError (getParam (c, ID::attack), 2.0f, 0.01f, "unchanged");

            c.setStateInformation ("garbage", 7);
            expectWithinAbsoluteError (getParam (c, ID::attack), 2.0f, 0.01f, "unchanged");
        }
    }
};

//==============================================================================
struct EditorTests final : juce::UnitTest
{
    EditorTests() : juce::UnitTest ("Editor", "FET") {}

    void runTest() override
    {
        beginTest ("editor opens and closes");
        SongsmithAnalogCompressorProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        expect (e != nullptr);
        expectGreaterThan (e->getWidth(), 0);
        e.reset();

        beginTest ("global Bypass is top-right and uses the suite's Bypass red (SUI-13)");
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        Songsmith::UI::Testing::checkGlobalBypass (*this, *ed, *p.apvts.getParameter (ID::bypass), "Analog Compressor");

        beginTest ("title bar reads name, preset, Undo, Redo, Bypass; Interface size and Oversampling are not in it (SUI-18)");
        Songsmith::UI::Testing::checkSuiteHeader (*this, *ed, "Analog Compressor");
        if (auto* ac = dynamic_cast<SongsmithAnalogCompressorEditor*> (ed.get()))
        {
            expect (ac->getOversamplingBox().getBottom() > 50, "Oversampling sits with the processing controls, below the title bar");
            expect (ac->getOversamplingBox().getY() > ac->getIgnoreBelowKnob().getY(), "Oversampling is under Ignore Below");
            expect (ac->getTitleBounds().getRight() < ac->getPresetBox().getX(), "the title does not overlap the preset selector");
        }

        beginTest ("Undo and Redo reverse user edits and preset loads, never host automation (SUI-18)");
        {
            SongsmithAnalogCompressorProcessor q;
            Songsmith::UI::Testing::checkParameterHistory (*this, q, ID::input, ID::output, "Analog Compressor");
        }
    }
};

//==============================================================================
// The latency and Processing Load strip (suite task S11).
class DiagnosticsTests final : public juce::UnitTest
{
public:
    DiagnosticsTests() : juce::UnitTest ("Latency and Processing Load", "FET") {}

    void runTest() override
    {
        using namespace Songsmith::Diagnostics;

        beginTest ("The strip shows the reported latency in every oversampling setting and sample rate");
        {
            int distinct = 0, last = -1;
            for (double fs : { 44100.0, 48000.0, 96000.0 })
                for (int oversampling : { 0, 1, 2 })
                {
                    SongsmithAnalogCompressorProcessor p;
                    setParam (p, ID::oversampling, (float) oversampling);
                    p.setRateAndBufferSizeDetails (fs, 512);
                    p.prepareToPlay (fs, 512);
                    p.updateLatencyNow();

                    Testing::checkLatencyDisplay (*this, p, "oversampling " + juce::String (oversampling) + ", " + juce::String (fs) + " Hz");
                    if (p.getLatencySamples() != last) { ++distinct; last = p.getLatencySamples(); }
                }
            expectGreaterThan (distinct, 2, "the settings really do report different latencies");
        }

        beginTest ("The strip follows a setting change while it is open");
        {
            Testing::TemporarySettings settings;
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::oversampling, 0.0f);
            p.setRateAndBufferSizeDetails (48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            p.updateLatencyNow();
            Bar bar (p, p.loadMeter, JucePlugin_Name);
            bar.refresh();
            expectEquals (Testing::parseLatency (bar.getLatencyText()).samples, 0, "no oversampling: no delay");

            setParam (p, ID::oversampling, 2.0f);
            p.prepareToPlay (48000.0, 512);
            p.updateLatencyNow();
            bar.refresh();
            expectGreaterThan (Testing::parseLatency (bar.getLatencyText()).samples, 0, "4x oversampling adds delay and the strip shows it");
            expectEquals (Testing::parseLatency (bar.getLatencyText()).samples, p.getLatencySamples());
        }

        beginTest ("Processing Load is off until switched on, and measures real blocks at every size");
        {
            Testing::TemporarySettings settings;
            SongsmithAnalogCompressorProcessor p;
            expect (! p.loadMeter.isEnabled(), "off by default");
            Testing::checkLoadMeasured (*this, p, p.loadMeter, "Analog Compressor");
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
            SongsmithAnalogCompressorProcessor p;
            juce::MemoryBlock before, after;
            p.getStateInformation (before);
            p.loadMeter.setEnabled (true);
            p.getStateInformation (after);
            expect (before == after, "timing on or off changes nothing in the session state");
        }
    }
};
//==============================================================================
// ACOMP-08: the 1176-inspired editor refinements (UI only).
struct EditorRefinementTests final : juce::UnitTest
{
    EditorRefinementTests() : juce::UnitTest ("Editor refinements", "FET") {}

    // The editor's content (laid out at the design size) and the controls placed directly in it.
    static juce::Component* findContent (juce::Component& editor)
    {
        for (auto* c : editor.getChildren())
            if (c->getWidth() == FET::designWidth && c->getHeight() == FET::designHeight)
                return c;
        return nullptr;
    }

    void runTest() override
    {
        using namespace FET;

        SongsmithAnalogCompressorProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        auto* ed = dynamic_cast<SongsmithAnalogCompressorEditor*> (editor.get());
        expect (ed != nullptr);
        if (ed == nullptr) return;
        auto* content = findContent (*ed);
        expect (content != nullptr, "found the editor's content");
        if (content == nullptr) return;

        const juce::Rectangle<int> design (0, 0, designWidth, designHeight);

        beginTest ("The analogue meter is clearly larger than before, and keeps a comfortable shape");
        {
            const auto meter = ed->getMeter().getBounds();
            expectGreaterThan (meter.getWidth() * meter.getHeight(), (int) (296 * 132 * 1.4), "at least 40 % more face than the original 296 x 132");
            expectGreaterThan (meter.getWidth(), 296);
            expectGreaterThan (meter.getHeight(), 132);
            expect (design.contains (meter), "inside the editor");
        }

        beginTest ("No two controls overlap and all are inside the editor, with a click target of at least 26 px");
        {
            juce::Array<juce::Component*> controls;
            for (auto* c : content->getChildren())
                if (c->isVisible() && (dynamic_cast<juce::Button*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr
                                       || dynamic_cast<FET::UI::NeedleMeter*> (c) != nullptr || dynamic_cast<FETKnob*> (c) != nullptr))
                    controls.add (c);
            expectGreaterThan (controls.size(), 14, "found the controls");
            for (int i = 0; i < controls.size(); ++i)
            {
                expect (design.contains (controls[i]->getBounds()), "a control is inside the editor");
                if (dynamic_cast<juce::Button*> (controls[i]) != nullptr || dynamic_cast<juce::ComboBox*> (controls[i]) != nullptr)
                    expectGreaterOrEqual (controls[i]->getHeight(), 26, "click target height");
                for (int j = i + 1; j < controls.size(); ++j)
                    expect (! controls[i]->getBounds().intersects (controls[j]->getBounds()),
                            "two controls overlap: " + controls[i]->getBounds().toString() + " and " + controls[j]->getBounds().toString());
            }
        }

        beginTest ("Ignore Below is a rotary knob beside Gain Reduction Off and under the Gain Reduction meter, with a plain label and tooltip");
        {
            auto& knob = ed->getIgnoreBelowKnob();
            const juce::Rectangle<int> slot (lowCutX, lowCutY, lowCutWidth, lowCutHeight);
            expect (knob.isVisible());
            expect (slot.contains (knob.getBounds()), "inside its place");
            expect (juce::Rectangle<int> (0, 0, designWidth, contentHeight).contains (knob.getBounds()), "inside the controls area");
            expectLessThan (knob.getBottom(), 356, "above the hint line");
            expect (ed->getGrOffBounds().getRight() < knob.getX(), "to the right of Gain Reduction Off");
            expect (ed->getGrOffBounds().getBottom() > knob.getY() && ed->getGrOffBounds().getY() < knob.getBottom(), "on the same row");
            expect (knob.slider.getSliderStyle() == juce::Slider::RotaryHorizontalVerticalDrag, "a rotary, not a dropdown");
            expectGreaterOrEqual (knob.slider.getWidth(), 40, "a comfortable knob");
            expectEquals (knob.slider.getTitle(), juce::String ("Ignore Below"));
            expect (knob.isCompact());

            for (int i = 0; i < numLowCutSteps; ++i)
            {
                const auto text = SongsmithAnalogCompressorEditor::lowCutText (i);
                expectEquals (text, i == 0 ? juce::String ("Off") : juce::String ((int) lowCutHz[i]) + " Hz");
                expect (Songsmith::UI::Wording::findBanned (text).isEmpty(), "plain words: " + text);
            }

            expectEquals (knob.getValueText(), juce::String ("Off"), "starts Off");
            expectEquals (knob.slider.getDoubleClickReturnValue(), 0.0, "double-click returns to Off");

            const auto tip = knob.slider.getTooltip();
            expectEquals (tip, juce::String ("Ignore bass below this frequency when deciding how much to compress. The bass you hear stays unchanged."));
            expect (Songsmith::UI::Wording::findBanned (tip).isEmpty(), "plain words: " + Songsmith::UI::Wording::findBanned (tip));

            for (int i = 0; i < numLowCutSteps; ++i)
            {
                knob.slider.setValue ((double) i, juce::sendNotificationSync);
                expectEquals ((int) getParam (p, ID::detectorLowCut), i, "turning the knob sets the saved choice");
                expectEquals (knob.getValueText(), SongsmithAnalogCompressorEditor::lowCutText (i));
            }
            setParam (p, ID::detectorLowCut, 2.0f);
            expectEquals (juce::roundToInt (knob.slider.getValue()), 2, "and the parameter moves the knob (automation)");
            expectEquals (knob.getValueText(), juce::String ("80 Hz"));
            setParam (p, ID::detectorLowCut, 0.0f);
        }

        beginTest ("The ratio buttons say what they are, and All is called All");
        {
            const char* const expected[] = { "4:1", "8:1", "12:1", "20:1" };
            const char* const tips[] = {
                "Balanced compression. Controls louder notes while keeping the sound lively and punchy. A good everyday starting point.",
                "Strong compression. Keeps volume more consistent and makes louder notes stand out less. Useful for bass, drums and powerful vocals.",
                "Very strong compression. Firmly controls loud peaks and creates a more compressed, upfront sound.",
                "Limiter-like compression. Heavily controls loud peaks for a dense, aggressive sound. Not a guaranteed brick-wall limiter." };
            for (int i = 0; i < 4; ++i)
            {
                auto& b = dynamic_cast<juce::TextButton&> (ed->getRatioButton (i));
                expectEquals (b.getButtonText(), juce::String (expected[i]));
                expectEquals (b.getTooltip(), juce::String (tips[i]));
                expect (b.getTooltip().length() < 260, "short enough to read");
            }
            auto& all = dynamic_cast<juce::TextButton&> (ed->getAllButton());
            expectEquals (all.getButtonText(), juce::String ("All"));
            expectEquals (all.getTooltip(), juce::String ("Aggressive 1176-style compression. Produces a distinctive, energetic sound with extra character. Great for parallel drums and special effects."));
            for (const auto& text : { all.getTooltip(), dynamic_cast<juce::TextButton&> (ed->getRatioButton (3)).getTooltip() })
                expect (Songsmith::UI::Wording::findBanned (text).isEmpty(), "plain words: " + Songsmith::UI::Wording::findBanned (text));
        }

        beginTest ("Every ratio button has its own tooltip, and no description is drawn permanently");
        {
            juce::StringArray seen;
            for (int i = 0; i < 5; ++i)
                seen.addIfNotAlreadyThere (ed->getRatioButton (i).getTooltip());
            seen.addIfNotAlreadyThere (ed->getAllButton().getTooltip());
            expectEquals (seen.size(), 6);
            expect (! seen.contains (juce::String()), "no blank tooltip");
        }

        beginTest ("The Songsmith 2:1 button is labelled and explained");
        {
            auto& b = dynamic_cast<juce::TextButton&> (ed->getRatioButton (4));
            expectEquals (b.getButtonText(), juce::String ("2:1"));
            expectEquals (b.getTooltip(), juce::String ("Gentle compression. Smooths out volume differences while keeping the performance natural. Great for acoustic guitar and vocals."));
            expect (Songsmith::UI::Wording::findBanned (b.getTooltip()).isEmpty(), "plain words");
            expect (b.getTooltip().length() < 360, "short enough to read");
            expect (ed->getRatioButton (4).getBounds().getHeight() >= 30, "large enough to press at the smallest size");
            for (int i = 0; i < 4; ++i)
                expect (! ed->getRatioButton (4).getBounds().intersects (ed->getRatioButton (i).getBounds()), "buttons do not overlap");
            expect (! ed->getRatioButton (4).getBounds().intersects (ed->getAllButton().getBounds()), "nor the All button");
        }

        beginTest ("All still presses every ratio button and a single ratio still leaves All");
        {
            ed->getAllButton().onClick();
            expect (p.isAllButtonsOn(), "All turns all four on");
            for (int i = 0; i < 4; ++i)
                expect (ed->getRatioButton (i).getToggleState());
            ed->getRatioButton (1).onClick();
            expect (! p.isAllButtonsOn(), "a single ratio leaves All");
        }

        beginTest ("The faceplate is textured, crisp at 1x and 2x, and the footer and Bypass are where the suite puts them");
        for (float scale : { 1.0f, 2.0f })
        {
            const auto image = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            expectEquals (image.getWidth(), juce::roundToInt ((float) ed->getWidth() * scale));
            // An empty strip of the faceplate (between the knobs and the hint line) is not one flat colour.
            std::set<juce::uint32> colours;
            const int y = juce::roundToInt (340.0f * ed->getHeight() / (float) designHeight * scale);
            for (int x = juce::roundToInt (60.0f * scale); x < juce::roundToInt (360.0f * scale); ++x)
                for (int dy = 0; dy < juce::roundToInt (8.0f * scale); ++dy)
                    colours.insert (image.getPixelAt (x, y + dy).getARGB());
            expectGreaterThan ((int) colours.size(), 3, "the grain gives the plate texture at " + juce::String (scale) + "x");
            expect (ed->getDiagnosticsBounds().getY() >= contentHeight, "the Processing Load strip is below the controls");
            expect (ed->getDiagnosticsBounds().getX() == 0 && ed->getDiagnosticsBounds().getWidth() == designWidth, "and spans the whole editor");
        }
    }
};

//==============================================================================
// ACOMP-11: the Songsmith 2:1 ratio. See analog-compressor/docs/two-to-one-ratio.md.
struct TwoToOneTests final : juce::UnitTest
{
    TwoToOneTests() : juce::UnitTest ("Two-to-one ratio", "FET") {}

    static float settledGr (SongsmithAnalogCompressorProcessor& p, float amp)
    {
        juce::MidiBuffer m;
        float gr = 0.0f;
        int offset = 0;
        for (int block = 0; block < 60; ++block)
        {
            juce::AudioBuffer<float> b (2, 512);
            fillSine (b, 48000.0, 997.0, amp, offset);
            offset += 512;
            p.processBlock (b, m);
            gr = p.takeGainReductionDb();
        }
        return gr;
    }

    void runTest() override
    {
        beginTest ("the four legacy ratio choices keep their stored index and host-normalised value");
        {
            SongsmithAnalogCompressorProcessor p;
            auto* ratio = p.apvts.getParameter (ID::ratio);
            expectEquals (ratio->getNumSteps(), 4, "still a four-item choice, so index / 3 is unchanged");
            expectEquals (ratioNames().size(), 4);
            const float normalised[] = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
            for (int i = 0; i < 4; ++i)
            {
                setParam (p, ID::ratio, (float) i);
                expectWithinAbsoluteError (ratio->getValue(), normalised[i], 1.0e-6f, "index " + juce::String (i) + " normalises as before");
                expectEquals ((int) getParam (p, ID::ratio), i);

                // What a host sends back when it replays automation recorded by an older beta.
                setParam (p, ID::ratio, 0.0f);
                ratio->setValueNotifyingHost (normalised[i]);
                expectEquals ((int) getParam (p, ID::ratio), i, "old automation value " + juce::String (normalised[i], 4) + " selects the same ratio");
                expectEquals ((int) getParam (p, ID::ratioTwoToOne), 0, "and never turns 2:1 on");
            }
            expectEquals (ratioDefault, 0, "default stays 4:1");
        }

        beginTest ("the engine's four hardware ratios are untouched and 2:1 is appended");
        {
            using E = Engine;
            expectEquals (E::numHardwareRatios, 4);
            expectEquals (E::gentleRatioIndex, 4);
            const float ratios[] = { 4.0f, 8.0f, 12.0f, 20.0f }, thresholds[] = { -19.5f, -17.0f, -15.5f, -13.8f }, knees[] = { 10.0f, 8.0f, 7.0f, 6.0f };
            for (int i = 0; i < 4; ++i)
            {
                expectEquals (E::ratios[(size_t) i], ratios[i]);
                expectEquals (E::thresholdsDb[(size_t) i], thresholds[i]);
                expectEquals (E::kneeWidthsDb[(size_t) i], knees[i]);
            }
            expectEquals (E::ratios[4], FET::gentleRatioValue);
        }

        beginTest ("2:1 measures about 2:1 above the knee, and is gentler than 4:1 at loud levels");
        {
            Engine e;
            e.prepare (48000.0);
            e.setRatioIndex (Engine::gentleRatioIndex);
            const float t = Engine::thresholdsDb[4], w = Engine::kneeWidthsDb[4];
            for (float over : { w, 20.0f, 30.0f })   // levels above the knee
            {
                const float in1 = t + over, in2 = in1 + 6.0f;
                // Steady-state through the running engine, not just the static curve.
                const float out1 = in1 - Measure::settledGrDb (e, 48000.0, in1), out2 = in2 - Measure::settledGrDb (e, 48000.0, in2);
                expectWithinAbsoluteError ((out2 - out1) / 6.0f, 0.5f, 0.03f, "output rises 1 dB per 2 dB in at +" + juce::String (over, 0) + " dB");
            }
            expectWithinAbsoluteError (e.staticGainReductionDb (t - w), 0.0f, 1.0e-4f, "no gain reduction well below the knee");

            Engine four;
            four.prepare (48000.0);
            four.setRatioIndex (0);
            expectLessThan (e.staticGainReductionDb (-6.0f), four.staticGainReductionDb (-6.0f), "less gain reduction than 4:1 at -6 dBFS");
            expectLessThan (e.staticGainReductionDb (-10.0f), four.staticGainReductionDb (-10.0f), "and at -10 dBFS");
        }

        beginTest ("the 2:1 switch takes over from the ratio choice, and leaves the choice's own value alone");
        {
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::oversampling, 0.0f);
            setParam (p, ID::input, 12.0f);
            p.setBusesLayout (layout (Set::stereo(), Set::stereo()));
            p.prepareToPlay (48000.0, 512);
            const float four = settledGr (p, 0.25f);

            setParam (p, ID::ratioTwoToOne, 1.0f);
            const float two = settledGr (p, 0.25f);
            expectEquals ((int) getParam (p, ID::ratio), 0, "the stored ratio choice is still 4:1");
            expect (two > 0.5f && std::abs (two - four) > 0.5f, "2:1 changes the gain reduction (" + juce::String (two, 2) + " dB vs " + juce::String (four, 2) + " dB)");

            setParam (p, ID::ratioTwoToOne, 0.0f);
            expectWithinAbsoluteError (settledGr (p, 0.25f), four, 0.05f, "switching 2:1 off returns to the saved ratio");
        }

        beginTest ("switching between 2:1 and the other ratios during playback is smooth and stable");
        {
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::input, 18.0f);
            p.setBusesLayout (layout (Set::stereo(), Set::stereo()));
            p.prepareToPlay (48000.0, 512);
            juce::MidiBuffer m;
            float peak = 0.0f;
            bool finite = true;
            int offset = 0;
            for (int block = 0; block < 80; ++block)
            {
                setParam (p, ID::ratioTwoToOne, (block / 5) % 2 == 0 ? 1.0f : 0.0f);
                setParam (p, ID::ratio, (float) ((block / 10) % 4));
                setParam (p, ID::allButtons, block % 40 >= 30 ? 1.0f : 0.0f);
                juce::AudioBuffer<float> b (2, 256);
                fillSine (b, 48000.0, 440.0, 0.2f, offset);
                offset += 256;
                p.processBlock (b, m);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                    {
                        finite = finite && std::isfinite (b.getSample (c, i));
                        peak = std::max (peak, std::abs (b.getSample (c, i)));
                    }
            }
            expect (finite, "no NaN or infinity");
            expectLessThan (peak, 8.0f, "no runaway level");
        }

        beginTest ("fresh sessions and sessions saved by earlier betas keep their ratio and load 2:1 Off");
        {
            SongsmithAnalogCompressorProcessor fresh;
            expectEquals ((int) getParam (fresh, ID::ratioTwoToOne), 0);
            expectEquals ((int) getParam (fresh, ID::ratio), 0);

            for (int version : { 1, 2, 3 })
                for (int saved = 0; saved < 4; ++saved)
                    for (int all : { 0, 1 })
                    {
                        SongsmithAnalogCompressorProcessor p;
                        setParam (p, ID::ratioTwoToOne, 1.0f);   // would stick if the loader kept current values

                        juce::XmlElement old ("SongsmithAnalogCompressor");
                        old.setAttribute ("stateVersion", version);
                        auto* r = old.createNewChildElement ("PARAM");
                        r->setAttribute ("id", "ratio");
                        r->setAttribute ("value", (double) saved);
                        auto* a = old.createNewChildElement ("PARAM");
                        a->setAttribute ("id", "allButtons");
                        a->setAttribute ("value", (double) all);
                        juce::MemoryBlock mb;
                        juce::AudioProcessor::copyXmlToBinary (old, mb);
                        p.setStateInformation (mb.getData(), (int) mb.getSize());

                        expectEquals ((int) getParam (p, ID::ratio), saved, "version " + juce::String (version) + " ratio index kept");
                        expectEquals ((int) getParam (p, ID::allButtons), all, "All Buttons kept");
                        expectEquals ((int) getParam (p, ID::ratioTwoToOne), 0, "2:1 is Off");
                    }
        }

        beginTest ("a session saved with 2:1 on recalls it, and also keeps the ratio choice underneath");
        {
            juce::MemoryBlock saved;
            {
                SongsmithAnalogCompressorProcessor a;
                setParam (a, ID::ratio, 2.0f);
                setParam (a, ID::ratioTwoToOne, 1.0f);
                a.getStateInformation (saved);
            }
            SongsmithAnalogCompressorProcessor b;
            b.setStateInformation (saved.getData(), (int) saved.getSize());
            expectEquals ((int) getParam (b, ID::ratioTwoToOne), 1);
            expectEquals ((int) getParam (b, ID::ratio), 2);
        }

        beginTest ("presets: starting points turn 2:1 off, older preset files load it Off, and a saved preset recalls it");
        {
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::ratioTwoToOne, 1.0f);
            p.presets.loadFactory (0);
            expectEquals ((int) getParam (p, ID::ratioTwoToOne), 0, "a starting point leaves 2:1");
            expectEquals ((int) getParam (p, ID::ratio), 0);

            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("acomp11", "", false);
            juce::String error;
            p.presets.setUserFolder (folder);
            setParam (p, ID::ratioTwoToOne, 1.0f);
            expect (p.presets.saveAs ("Gentle test", error), error);

            setParam (p, ID::ratioTwoToOne, 0.0f);
            const auto files = p.presets.getUserPresetFiles();
            expectEquals (files.size(), 1);
            if (files.size() == 1)
            {
                expect (p.presets.loadFile (files[0], error), error);
                expectEquals ((int) getParam (p, ID::ratioTwoToOne), 1, "saved preset recalls 2:1");

                // An older preset file has no ratioTwoToOne attribute.
                auto xml = juce::XmlDocument::parse (files[0]);
                expect (xml != nullptr);
                if (xml != nullptr)
                {
                    xml->removeAttribute (ID::ratioTwoToOne);
                    xml->setAttribute (ID::ratio, 3.0);
                    expect (xml->writeTo (files[0]));
                    expect (p.presets.loadFile (files[0], error), error);
                    expectEquals ((int) getParam (p, ID::ratioTwoToOne), 0, "an older preset loads 2:1 Off");
                    expectEquals ((int) getParam (p, ID::ratio), 3, "and keeps its ratio");
                }
            }
            folder.deleteRecursively();
        }

        beginTest ("the 2:1 button selects 2:1 without touching the ratio choice, and the others leave it");
        {
            SongsmithAnalogCompressorProcessor p;
            setParam (p, ID::ratio, 2.0f);
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            auto* ed = dynamic_cast<SongsmithAnalogCompressorEditor*> (editor.get());
            expect (ed != nullptr);
            if (ed == nullptr) return;

            ed->getRatioButton (4).onClick();
            expect (p.isGentleRatioOn(), "pressing 2:1 turns it on");
            expect (ed->getRatioButton (4).getToggleState(), "and lights it");
            for (int i = 0; i < 4; ++i)
                expect (! ed->getRatioButton (i).getToggleState(), "the others go out");
            expectEquals ((int) getParam (p, ID::ratio), 2, "the stored ratio is unchanged");

            ed->getRatioButton (1).onClick();
            expect (! p.isGentleRatioOn(), "pressing another ratio turns 2:1 off");
            expectEquals ((int) getParam (p, ID::ratio), 1);
            expect (ed->getRatioButton (1).getToggleState() && ! ed->getRatioButton (4).getToggleState());

            ed->getAllButton().onClick();
            ed->getRatioButton (4).onClick();
            expect (! p.isAllButtonsOn() && p.isGentleRatioOn(), "pressing 2:1 leaves All Buttons");

            ed->getAllButton().onClick();
            expect (p.isAllButtonsOn() && ! ed->getRatioButton (4).getToggleState(), "All Buttons lights only the four hardware buttons");

            setParam (p, ID::allButtons, 0.0f);
            setParam (p, ID::ratioTwoToOne, 0.0f);
            expect (ed->getRatioButton (1).getToggleState() && ! ed->getRatioButton (4).getToggleState(), "automation updates the buttons");
        }
    }
};

static IdentityTests identityTests;
static TwoToOneTests twoToOneTests;
static DiagnosticsTests diagnosticsTests;
static LayoutTests layoutTests;
static TransparencyTests transparencyTests;
static StateTests stateTests;
static EditorTests editorTests;
static EditorRefinementTests editorRefinementTests;

// AnalogCompressorTests --timing: measured attack and release (definitions in docs/engine.md).
static int runTimingReport()
{
    std::printf ("%-8s %-6s %-6s %-6s %12s %12s %12s %12s\n", "fs", "ratio", "knob", "over", "attack nom", "attack meas", "release nom", "release meas");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int r = 0; r < FET::Engine::numRatios; ++r)
            for (float knob : { 1.0f, 4.0f, 7.0f })
                for (float over : { 10.0f, 20.0f, 30.0f })
                    std::printf ("%-8.0f %-6.0f %-6.0f %-6.0f %10.1fus %10.1fus %10.1fms %10.1fms\n", fs,
                                 FET::Engine::ratios[(size_t) r], knob, over,
                                 FET::Engine::attackSeconds (knob) * 1.0e6, Measure::attackSeconds (fs, r, knob, over) * 1.0e6,
                                 FET::Engine::releaseSeconds (knob) * 1.0e3, Measure::releaseSeconds (fs, r, knob, over) * 1.0e3);
    return 0;
}

// AnalogCompressorTests --screenshots <folder> [--scale 2]: the editor at 100, 150 and 200 %, plus All Buttons
// and the +4 meter, with audio running so the meter and GR readout show real values.
static int renderScreenshots (const juce::File& folder, float backingScale)
{
    folder.createDirectory();
    const auto shoot = [&] (float uiScale, bool allButtons, int meterMode, const juce::String& name)
    {
        SongsmithAnalogCompressorProcessor p;
        const auto set = [&] (const char* id, float v) { auto* rp = p.apvts.getParameter (id); rp->setValueNotifyingHost (rp->convertTo0to1 (v)); };
        set (ID::input, 18.0f);
        set (ID::output, 4.0f);
        set (ID::attack, 3.0f);
        set (ID::release, 6.0f);
        set (ID::ratio, 0.0f);
        set (ID::allButtons, allButtons ? 1.0f : 0.0f);
        p.uiScale.store (uiScale);
        p.meterMode.store (meterMode);
        p.prepareToPlay (48000.0, 480);
        std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
        auto* ed = dynamic_cast<SongsmithAnalogCompressorEditor*> (editor.get());
        juce::AudioBuffer<float> b (2, 480);
        juce::MidiBuffer m;
        for (int k = 0; k < 100; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 480; ++i)
                    b.setSample (c, i, 0.15f * (float) std::sin (0.04 * (k * 480 + i)));
            p.processBlock (b, m);
            if (ed != nullptr) ed->advanceMeter (0.01f);
        }
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, backingScale);
        const auto file = folder.getChildFile (name + (backingScale != 1.0f ? "@" + juce::String (backingScale, 0) + "x" : juce::String()) + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("%s\n", file.getFullPathName().toRawUTF8());
    };
    for (float s : { 1.0f, 1.5f, 2.0f })
        shoot (s, false, 0, "analog-compressor-" + juce::String (juce::roundToInt (s * 100.0f)));
    shoot (1.0f, true, 0, "analog-compressor-all-buttons");
    shoot (1.0f, false, 1, "analog-compressor-meter-plus4");

    // Every control state, drawn by the editor's look-and-feel.
    FET::UI::LookAndFeel lf;
    Songsmith::UI::Testing::writeControlSheet (lf, FET::UI::Colours::panel, backingScale,
        folder.getChildFile ("controls" + (backingScale != 1.0f ? "@" + juce::String (backingScale, 0) + "x" : juce::String()) + ".png"));
    return 0;
}

// AnalogCompressorTests --colour: harmonics and aliasing versus drive, level-matched (docs/colour.md).
static int runColourReport()
{
    using namespace ColourMeasure;
    std::printf ("A. Output stage alone (Gain Reduction Off), 1 kHz, by output level\n");
    std::printf ("%-7s %-4s %8s %9s %9s %9s %9s\n", "fs", "OS", "out dBFS", "THD %", "H2 dB", "H3 dB", "alias dB");
    for (double fs : { 44100.0, 48000.0 })
        for (int os : { 0, 1, 2 })
            for (float outDb : { -18.0f, -6.0f, 0.0f, 3.0f })
            {
                Settings s; s.fs = fs; s.oversampling = os; s.grOff = true; s.levelDbfs = outDb;
                const auto r = measure (s);
                std::printf ("%-7.0f %-4d %8.1f %9.3f %9.1f %9.1f %9.1f\n", fs, 1 << os, r.fundamentalDb, r.thdPercent, r.h2Db, r.h3Db, r.aliasDb);
            }

    std::printf ("\nB. Drive with and without compression, level-matched at the output (input -18 dBFS RMS, 4:1)\n");
    std::printf ("%-7s %-4s %-6s %-6s %6s %8s %9s %9s %9s %9s\n", "fs", "OS", "freq", "drive", "GR", "out dBFS", "THD %", "H2 dB", "H3 dB", "alias dB");
    for (double fs : { 44100.0, 48000.0 })
        for (int os : { 0, 1, 2 })
            for (double freq : { 1000.0, 7000.0 })
                for (float drive : { 6.0f, 18.0f, 30.0f })
                {
                    Settings c; c.fs = fs; c.oversampling = os; c.freq = freq; c.inputDb = drive;
                    const auto rc = measure (c);
                    Settings o = c; o.grOff = true;
                    o.outputDb = (float) (rc.fundamentalDb - (-18.0 + drive));   // match the compressed level
                    const auto ro = measure (o);
                    std::printf ("%-7.0f %-4d %-6.0f %-6.0f %6.1f %8.1f %9.3f %9.1f %9.1f %9.1f   comp\n", fs, 1 << os, freq, drive, rc.grDb, rc.fundamentalDb, rc.thdPercent, rc.h2Db, rc.h3Db, rc.aliasDb);
                    std::printf ("%-7s %-4s %-6s %-6s %6s %8.1f %9.3f %9.1f %9.1f %9.1f   GR off\n", "", "", "", "", "", ro.fundamentalDb, ro.thdPercent, ro.h2Db, ro.h3Db, ro.aliasDb);
                }

    std::printf ("\nC. Frequency response re 1 kHz (GR off, -18 dBFS) and reported latency\n");
    std::printf ("%-7s %-4s %8s %8s %8s %8s %8s %9s\n", "fs", "OS", "20 Hz", "50 Hz", "10 kHz", "16 kHz", "20 kHz", "latency");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int os : { 0, 1, 2 })
        {
            Settings s; s.fs = fs; s.oversampling = os; s.grOff = true;
            const double ref = measure (s).fundamentalDb;
            std::printf ("%-7.0f %-4d", fs, 1 << os);
            for (double f : { 20.0, 50.0, 10000.0, 16000.0, 20000.0 })
            {
                s.freq = f;
                std::printf (" %8.2f", measure (s).fundamentalDb - ref);
            }
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            std::printf (" %9d\n", p.getOversamplingLatency (os));
        }
    return 0;
}

// AnalogCompressorTests --bench: plugin CPU per channel-second (stereo, 48 kHz, 512-sample blocks).
static double benchOnce (int oversampling, bool colour)
{
    SongsmithAnalogCompressorProcessor p;
    p.setColourEnabled (colour);
    auto* os = p.apvts.getParameter (ID::oversampling);
    os->setValueNotifyingHost (os->convertTo0to1 ((float) oversampling));
    p.prepareToPlay (48000.0, 512);
    auto* in = p.apvts.getParameter (ID::input);
    in->setValueNotifyingHost (in->convertTo0to1 (18.0f));
    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer m;
    const int blocks = 48000 * 20 / 512;   // 20 s of audio
    juce::Random rng (1);
    const auto t0 = juce::Time::getHighResolutionTicks();
    for (int k = 0; k < blocks; ++k)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                b.setSample (c, i, 0.3f * (float) std::sin (0.031 * (k * 512 + i)) + 0.01f * (rng.nextFloat() - 0.5f));
        p.processBlock (b, m);
    }
    const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
    const double audio = blocks * 512.0 / 48000.0;
    return 100.0 * secs / audio;
}

static int runBenchmark()
{
    std::printf ("CPU, stereo 48 kHz, 512-sample blocks, 18 dB drive (%% of one core)\n");
    std::printf ("%-14s %10s %10s\n", "oversampling", "clean", "colour");
    for (int os : { 0, 1, 2 })
        std::printf ("%-14s %9.2f%% %9.2f%%\n", oversamplingNames()[os].toRawUTF8(), benchOnce (os, false), benchOnce (os, true));
    return 0;
}

//==============================================================================
struct ConsoleRunner final : juce::UnitTestRunner
{
    void logMessage (const juce::String& m) override { std::printf ("%s\n", m.toRawUTF8()); std::fflush (stdout); }
};

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // No test may touch the real user presets folder: point it at a throwaway folder.
    const auto appData = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getNonexistentChildFile ("fet-tests-appdata", {}, false);
    FET::Presets::setDefaultFolderRootForTesting (appData);
    struct Cleanup { juce::File dir; ~Cleanup() { dir.deleteRecursively(); } } cleanup { appData };

    for (int i = 1; i < argc; ++i)
    {
        if (juce::String (argv[i]) == "--timing") return runTimingReport();
        if (juce::String (argv[i]) == "--bench")  return runBenchmark();
        if (juce::String (argv[i]) == "--colour") return runColourReport();
        if (juce::String (argv[i]) == "--screenshots" && i + 1 < argc)
        {
            float scale = 1.0f;
            for (int j = 1; j + 1 < argc; ++j)
                if (juce::String (argv[j]) == "--scale")
                    scale = juce::String (argv[j + 1]).getFloatValue();
            return renderScreenshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]), juce::jlimit (1.0f, 4.0f, scale));
        }
    }

    ConsoleRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("FET");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\n==== %d checks passed, %d failed ====\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
