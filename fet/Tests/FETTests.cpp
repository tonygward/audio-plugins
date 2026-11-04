// Songsmith Analog Compressor test runner.
//   FETTests.exe          -> run all unit tests (exit code 1 on failure)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"
#include "PluginEditor.h"

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

        struct Expected { const char* id; float min, max, def; };
        const Expected expected[] = {
            { "input",      -24.0f, 48.0f, 0.0f },
            { "output",     -36.0f, 24.0f, 0.0f },
            { "attack",       1.0f,  7.0f, 4.0f },
            { "release",      1.0f,  7.0f, 4.0f },
            { "ratio",        0.0f,  3.0f, 0.0f },
            { "allButtons",   0.0f,  1.0f, 0.0f },
            { "grOff",        0.0f,  1.0f, 0.0f },
            { "bypass",       0.0f,  1.0f, 0.0f },
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
            expectEquals (rp->getVersionHint(), 1, e.id);
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
                expectGreaterThan (b.getMagnitude (c, 0, 256), 0.4f, "signal passes");
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
            beginTest ("defaults are bit-exact at " + juce::String (fs, 0) + " Hz");
            SongsmithAnalogCompressorProcessor p;
            p.setBusesLayout (layout (Set::stereo(), Set::stereo()));
            p.prepareToPlay (fs, 512);

            juce::MidiBuffer m;
            bool exact = true;
            int offset = 0;
            for (int block : { 512, 1, 37, 512, 100, 3 })
            {
                juce::AudioBuffer<float> b (2, block), ref (2, block);
                fillSine (b, fs, 997.0, 0.7f, offset);
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
        beginTest ("debug editor opens and closes");
        SongsmithAnalogCompressorProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
        expect (e != nullptr);
        expectGreaterThan (e->getWidth(), 0);
        e.reset();
    }
};

static IdentityTests identityTests;
static LayoutTests layoutTests;
static TransparencyTests transparencyTests;
static StateTests stateTests;
static EditorTests editorTests;

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
