// Songsmith Analog Compressor test runner.
//   FETTests.exe          -> run all unit tests (exit code 1 on failure)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/Tests/SongsmithControlSheet.h"
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
            { "oversampling", 0.0f,  2.0f, 2.0f, 2 },
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
        beginTest ("editor opens and closes");
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

// FETTests --timing: measured attack and release (definitions in docs/engine.md).
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

// FETTests --screenshots <folder> [--scale 2]: the editor at 100, 150 and 200 %, plus All Buttons
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

// FETTests --colour: harmonics and aliasing versus drive, level-matched (docs/colour.md).
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

// FETTests --bench: plugin CPU per channel-second (stereo, 48 kHz, 512-sample blocks).
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
