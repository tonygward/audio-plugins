#pragma once

// Shared checks for the latency and Processing Load strip, compiled into every plugin's test executable:
//   static SongsmithDiagnosticsTests sharedDiagnosticsTests { "EQ" };   // once per executable
//   Songsmith::Diagnostics::Testing::checkLatencyDisplay (*this, processor, "context");   // per latency mode
//   Songsmith::Diagnostics::Testing::checkLoadMeasured (*this, processor, processor.loadMeter, "context");

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/SongsmithDiagnostics.h"
#include "../Source/SongsmithWording.h"

namespace Songsmith::Diagnostics::Testing
{
    // Points the machine setting at a temporary file for as long as it lives, so a test never reads or
    // writes the real one.
    struct TemporarySettings
    {
        TemporarySettings() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getChildFile ("songsmith-diagnostics-test-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt())))
        {
            folder.createDirectory();
            Settings::fileOverrideForTests() = folder.getChildFile ("Diagnostics.xml");
        }
        ~TemporarySettings() { Settings::fileOverrideForTests() = juce::File(); folder.deleteRecursively(); }
        juce::File folder;
    };

    // "2.7ms (128 samples)" -> { 128, 2.7 }. Read back from the text itself, so a test compares what a
    // person sees with what the processor reports, not the formatter with itself.
    struct ParsedLatency { int samples = -1; double milliseconds = -1.0; };

    inline ParsedLatency parseLatency (const juce::String& text)
    {
        ParsedLatency r;
        const auto open = text.indexOfChar ('(');
        const auto samplesEnd = text.indexOf (" sample");
        if (open >= 0 && samplesEnd > open)
            r.samples = text.substring (open + 1, samplesEnd).getIntValue();
        else if (open < 0 && samplesEnd > 0)
            r.samples = text.substring (0, samplesEnd).getIntValue();   // no sample rate yet: samples only
        const auto ms = text.indexOf ("ms (");
        if (ms > 0)
            r.milliseconds = text.substring (0, ms).getDoubleValue();
        return r;
    }

    // The strip shows the processor's reported latency, in samples and in milliseconds at the current rate.
    // Call it after changing a mode and calling prepareToPlay (or after the mode change alone).
    inline void checkLatencyDisplay (juce::UnitTest& t, juce::AudioProcessor& processor, const juce::String& context)
    {
        TemporarySettings settings;
        LoadMeter unused;
        Bar bar (processor, unused, "Test");
        bar.refresh();

        const auto shown = parseLatency (bar.getLatencyText());
        const int reported = processor.getLatencySamples();
        const double rate = processor.getSampleRate();

        t.expectEquals (shown.samples, reported, context + ": the strip shows the samples the plugin reports (\"" + bar.getLatencyText() + "\")");
        if (rate > 0.0)
            t.expectWithinAbsoluteError (shown.milliseconds, 1000.0 * (double) reported / rate, 0.051,
                                         context + ": the milliseconds match the samples at " + juce::String (rate) + " Hz");
    }

    // SUI-12 / EQ-35 footer layout: the switch is at the far left, the readout appears directly to its
    // right only while the switch is on, and a plugin's own control stays right-aligned either way.
    inline void checkFooterLayout (juce::UnitTest& t, juce::AudioProcessor& processor, const juce::String& context)
    {
        TemporarySettings settings;
        LoadMeter meter;
        juce::TextButton probe ("Probe");
        probe.setSize (90, 22);
        Bar bar (processor, meter, "Test");
        bar.setTrailingControl (&probe);

        for (int width : { 720, 760, 1280, 2000 })
        {
            bar.setSize (width, Bar::designHeight);
            for (bool on : { false, true })
            {
                bar.getLoadSwitch().setToggleState (on, juce::sendNotificationSync);
                bar.resized();
                bar.refresh();
                const auto where = context + " at " + juce::String (width) + (on ? " px, Processing Load on" : " px, Processing Load off");
                auto& sw = bar.getLoadSwitch();

                t.expect (sw.getX() <= 20 && sw.getX() < bar.getWidth() / 10, where + ": the switch is at the far left");
                t.expect (sw.getButtonText() == "Processing Load", where + ": the switch is named Processing Load");
                t.expectEquals (probe.getRight(), bar.getWidth() - 16, where + ": the plugin's own control stays at the far right");
                t.expect (! sw.getBounds().intersects (probe.getBounds()), where + ": no overlap");
                t.expect (bar.isReadoutShown() == on, where + ": the readout is shown only while the switch is on");

                if (on)
                {
                    auto& readout = bar.getReadout();
                    const auto text = bar.getReadoutText();
                    t.expect (text.startsWith ("CPU ") && text.contains (", Latency "), where + ": readout reads \"" + text + "\"");
                    t.expect (readout.getX() >= sw.getRight() && readout.getX() - sw.getRight() <= 16, where + ": the readout follows the switch");
                    t.expect (readout.getRight() <= probe.getX(), where + ": the readout stops before the plugin's own control");
                    t.expect (text.contains (bar.getLatencyText()), where + ": the latency in the readout is the reported latency");

                    juce::GlyphArrangement glyphs;
                    glyphs.addLineOfText (static_cast<juce::Label&> (readout).getFont(), text, 0.0f, 0.0f);
                    t.expect (glyphs.getBoundingBox (0, -1, false).getWidth() <= (float) readout.getWidth(), where + ": the readout is not clipped");
                }
                else
                    t.expect (bar.getReadoutText().isEmpty(), where + ": no stale numbers left behind");
            }
        }
    }

    // Runs a few blocks of silence at different sizes with timing on and checks a load is published; with
    // timing off, nothing is.
    inline void checkLoadMeasured (juce::UnitTest& t, juce::AudioProcessor& processor, LoadMeter& meter, const juce::String& context)
    {
        const double rate = 48000.0;
        for (int block : { 64, 512, 2048 })
        {
            processor.setPlayConfigDetails (processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels(), rate, block);
            processor.prepareToPlay (rate, block);
            juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), block);
            juce::MidiBuffer midi;

            meter.setEnabled (false);
            buffer.clear();
            processor.processBlock (buffer, midi);
            t.expect (! meter.read().has_value(), context + ": no measurement while timing is off (" + juce::String (block) + " samples)");

            meter.setEnabled (true);
            for (int i = 0; i < 20; ++i)
            {
                buffer.clear();
                processor.processBlock (buffer, midi);
            }
            const auto load = meter.read();
            t.expect (load.has_value(), context + ": a load is published once blocks are timed (" + juce::String (block) + " samples)");
            if (load.has_value())
            {
                t.expectGreaterOrEqual (*load, 0.0f, context + ": the load is not negative");
                t.expectLessThan (*load, 50.0f, context + ": the load is a plausible fraction of the block time (" + juce::String (block) + " samples)");
            }
            meter.setEnabled (false);
        }
    }
}

class SongsmithDiagnosticsTests final : public juce::UnitTest
{
public:
    explicit SongsmithDiagnosticsTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith Diagnostics", category) {}

    void runTest() override
    {
        using namespace Songsmith::Diagnostics;
        using namespace Songsmith::Diagnostics::Testing;

        beginTest ("The load is the share of the block's time budget, at every block size");
        {
            expectWithinAbsoluteError (loadFraction (0.001, 480, 48000.0), 0.1f, 1.0e-5f, "1 ms of a 10 ms block");
            expectWithinAbsoluteError (loadFraction (0.001, 64, 48000.0), 0.75f, 1.0e-5f, "1 ms of a 1.33 ms block");
            expectWithinAbsoluteError (loadFraction (0.001, 2048, 48000.0), 0.0234375f, 1.0e-5f, "1 ms of a 42.7 ms block");
            expectWithinAbsoluteError (loadFraction (0.001, 441, 44100.0), 0.1f, 1.0e-5f, "the sample rate sets the budget");
            expectWithinAbsoluteError (loadFraction (0.001, 960, 96000.0), 0.1f, 1.0e-5f, "96 kHz");
            expectEquals (loadFraction (0.001, 0, 48000.0), 0.0f, "an empty block has no load");
            expectEquals (loadFraction (0.001, 512, 0.0), 0.0f, "an unknown sample rate has no load");
            expectEquals (loadFraction (-1.0, 512, 48000.0), 0.0f, "a negative duration is ignored");
            expectEquals (blockSeconds (512, 48000.0), 512.0 / 48000.0);
        }

        beginTest ("Smoothing settles in the same time whatever the buffer size");
        {
            // One second of a steady 100 % load, from zero, in blocks of different sizes.
            std::vector<float> settled;
            for (int block : { 32, 64, 512, 2048 })
            {
                float value = 0.0f;
                const double seconds = (double) block / 48000.0;
                for (double t = 0.0; t < 1.0; t += seconds)
                    value = smoothStep (value, 1.0f, seconds, LoadMeter::timeConstantSeconds);
                settled.push_back (value);
            }
            for (float v : settled)
            {
                expectWithinAbsoluteError (v, 1.0f - std::exp (-1.0f / (float) LoadMeter::timeConstantSeconds), 0.06f, "about 86 % of the way after two time constants");
                expectWithinAbsoluteError (v, settled.front(), 0.06f, "block sizes agree");
            }
            expectEquals (smoothStep (0.3f, 0.9f, 0.0), 0.3f, "no time passed, no change");
        }

        beginTest ("The meter publishes a smoothed load and shows unavailable without a recent measurement");
        {
            LoadMeter meter;
            expect (! meter.read().has_value(), "off by default");

            meter.setEnabled (true);
            expect (! meter.read().has_value(), "on, but nothing measured yet: unavailable");

            meter.record (0.001, 480, 48000.0);
            expect (meter.read().has_value(), "a measurement is published");
            expectWithinAbsoluteError (*meter.read(), 0.1f, 1.0e-4f, "the first block sets the value straight away");

            for (int i = 0; i < 200; ++i)
                meter.record (0.0005, 512, 48000.0);   // a steady load of about 4.7 %
            expectWithinAbsoluteError (*meter.read(), 0.0005f / (512.0f / 48000.0f), 0.002f, "it settles on the steady load");

            // Processing stops: no new blocks, so the last value goes stale.
            const auto now = juce::Time::getHighResolutionTicks();
            expect (meter.read (now).has_value(), "just measured: available");
            expect (! meter.read (now + juce::Time::secondsToHighResolutionTicks (LoadMeter::staleAfterSeconds + 0.5)).has_value(),
                    "no block for over a second: unavailable");

            meter.setEnabled (false);
            expect (! meter.read().has_value(), "switched off: unavailable");
            meter.setEnabled (true);
            expect (! meter.read().has_value(), "switched on again: starts afresh");

            // Different block sizes at the same share of their budget read the same.
            for (int block : { 32, 512, 4096 })
            {
                LoadMeter m;
                m.setEnabled (true);
                const double budget = (double) block / 48000.0;
                for (int i = 0; i < 10; ++i)
                    m.record (0.25 * budget, block, 48000.0);
                expectWithinAbsoluteError (*m.read(), 0.25f, 1.0e-4f, "25 % of a " + juce::String (block) + "-sample block");
            }
        }

        beginTest ("Timing a block measures real time, and costs nothing when off");
        {
            LoadMeter meter;
            {
                auto scope = meter.measure (512, 48000.0);   // off: a no-op
            }
            expect (! meter.read().has_value(), "off: nothing recorded");

            meter.setEnabled (true);
            {
                auto scope = meter.measure (512, 48000.0);
                const auto start = juce::Time::getHighResolutionTicks();
                while (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) < 0.002) {}   // about 2 ms
            }
            const auto load = meter.read();
            expect (load.has_value(), "a timed block is published");
            if (load.has_value())
            {
                expectGreaterThan (*load, 0.05f, "2 ms of a 10.7 ms block is at least 5 %");
                expectLessThan (*load, 50.0f, "and a plausible number even on a busy machine");
            }
        }

        beginTest ("The overhead of timing a block is small (benchmark)");
        {
            const int iterations = 200000;
            LoadMeter meter;
            auto timeIt = [&] (bool on)
            {
                meter.setEnabled (on);
                const auto start = juce::Time::getHighResolutionTicks();
                for (int i = 0; i < iterations; ++i)
                {
                    auto scope = meter.measure (512, 48000.0);
                    juce::ignoreUnused (scope);
                }
                return 1.0e9 * juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) / iterations;
            };
            timeIt (true);   // warm up
            const double onNs = timeIt (true), offNs = timeIt (false);
            logMessage ("Timing overhead per block: " + juce::String (onNs, 0) + " ns with Processing Load on, "
                        + juce::String (offNs, 1) + " ns with it off");
            expectLessThan (onNs, 5000.0, "on: well under 5 microseconds a block (the budget is 1.3 ms at 64 samples)");
            expectLessThan (offNs, 200.0, "off: a single atomic read");
        }

        beginTest ("Latency text: samples, and milliseconds at the current rate");
        {
            expectEquals (latencyText (128, 48000.0), juce::String ("2.7ms (128 samples)"));
            expectEquals (latencyText (84, 48000.0), juce::String ("1.8ms (84 samples)"));
            expectEquals (latencyText (0, 48000.0), juce::String ("0.0ms (0 samples)"));
            expectEquals (latencyText (1, 44100.0), juce::String ("0.0ms (1 sample)"));
            expectEquals (latencyText (4410, 44100.0), juce::String ("100.0ms (4410 samples)"));
            expectEquals (latencyText (4096, 96000.0), juce::String ("42.7ms (4096 samples)"));
            expectEquals (latencyText (128, 0.0), juce::String ("128 samples"), "no sample rate yet: just the samples");

            const auto p = parseLatency (latencyText (2048, 44100.0));
            expectEquals (p.samples, 2048);
            expectWithinAbsoluteError (p.milliseconds, 46.4, 0.051);
        }

        beginTest ("Load text has enough digits for light loads");
        {
            expectEquals (loadText (std::nullopt), juce::String ("Unavailable"));
            expectEquals (loadText (0.0f), juce::String ("0%"), "a real zero reads 0%");
            expectEquals (loadText (0.0035f), juce::String ("0.35%"), "light loads are not rounded to zero");
            expectEquals (loadText (0.0007f), juce::String ("0.07%"));
            expectEquals (loadText (0.032f), juce::String ("3.2%"));
            expectEquals (loadText (0.14f), juce::String ("14%"));
            expectEquals (loadText (1.5f), juce::String ("150%"), "over budget is shown, not capped");
            expectEquals (loadText (-0.5f), juce::String ("0%"), "never negative");
        }

        beginTest ("Everything on the strip is plain words, and the tooltips say what the numbers mean");
        {
            const juce::StringArray everything { "Processing Load", "Off", "Unavailable", loadText (0.04f), latencyText (128, 48000.0),
                                                 readoutText (0.0035f, 0, 48000.0), readoutText (std::nullopt, 1, 44100.0),
                                                 latencyTooltip(), loadTooltip() };
            expectEquals (readoutText (0.0035f, 0, 48000.0), juce::String ("CPU 0.35%, Latency 0.0ms (0 samples)"), "the readout in the agreed order and wording");
            expectEquals (readoutText (std::nullopt, 128, 48000.0), juce::String ("CPU Unavailable, Latency 2.7ms (128 samples)"),
                          "no measurement: unavailable, never a made-up zero, and the latency is still shown");
            expect (Songsmith::UI::Wording::findBanned ("CPU is busy").isNotEmpty(), "CPU stays banned everywhere else");
            for (const auto& text : everything)
                expect (Songsmith::UI::Wording::findBanned (text).isEmpty(),
                        "no abbreviation in \"" + text + "\" (found \"" + Songsmith::UI::Wording::findBanned (text) + "\")");

            expectEquals (juce::String (latencyTooltip()), juce::String ("The delay this plugin adds. Your recording software may compensate for it."));
            expect (juce::String (loadTooltip()).containsIgnoreCase ("this plugin"), "the load tooltip says it describes this plugin");
            expect (juce::String (loadTooltip()).containsIgnoreCase ("computer") && juce::String (loadTooltip()).containsIgnoreCase ("buffer size"),
                    "the load tooltip says it varies with the computer, the settings and the buffer size");
            expect (juce::String (loadTooltip()).containsIgnoreCase ("settings"));
        }

        beginTest ("Processing Load is off by default and kept in its own file, away from presets and sessions");
        {
            TemporarySettings settings;
            expect (! Settings::loadEnabled ("Test"), "off when nothing is saved");

            Settings::saveEnabled ("Test", true);
            expect (Settings::loadEnabled ("Test"), "on once chosen");
            expect (Settings::file ("Test").getFileName() == "Diagnostics.xml", "its own file");
            expect (! Settings::file ("Test").getParentDirectory().getFileName().containsIgnoreCase ("Presets"), "not in a presets folder");

            Settings::saveEnabled ("Test", false);
            expect (! Settings::loadEnabled ("Test"), "and off again");

            Settings::fileOverrideForTests() = juce::File();
            const auto real = Settings::file ("Songsmith Test Plugin");
            expect (real.getFullPathName().containsIgnoreCase ("Songsmith Studio"), "next to the plugin's presets: " + real.getFullPathName());
            expect (real.getFileName() == "Diagnostics.xml");
            Settings::fileOverrideForTests() = settings.folder.getChildFile ("Diagnostics.xml");   // restored by the destructor
        }
    }
};
