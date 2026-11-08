// Presets (step 5). See analog-compressor/docs/presets.md.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "PluginProcessor.h"
#include "FETPresets.h"
#include "ColourMeasure.h"

using namespace FET;

namespace
{
    // Level in and out (dBFS RMS) and GR for a -18 dBFS RMS 1 kHz sine, with oversampling off.
    struct Match { float inDb, outDb, grDb; };

    Match measurePreset (int index)
    {
        SongsmithAnalogCompressorProcessor p;
        p.setCurrentProgram (index);
        ColourMeasure::set (p, ID::oversampling, 0.0f);
        p.prepareToPlay (48000.0, 480);
        juce::AudioBuffer<float> b (2, 480);
        juce::MidiBuffer m;
        const float amp = std::pow (10.0f, -18.0f / 20.0f) * std::sqrt (2.0f);
        double sumOut = 0.0;
        int count = 0;
        float gr = 0.0f;
        for (int k = 0; k < 300; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 480; ++i)
                    b.setSample (c, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (k * 480 + i) / 48000.0));
            p.processBlock (b, m);
            gr = p.takeGainReductionDb();
            if (k >= 200)
                for (int i = 0; i < 480; ++i, ++count)
                    sumOut += (double) b.getSample (0, i) * b.getSample (0, i);
        }
        return { -18.0f, (float) (10.0 * std::log10 (sumOut / count)), gr };
    }
}

struct PresetTests final : juce::UnitTest
{
    PresetTests() : juce::UnitTest ("Presets", "FET") {}

    void runTest() override
    {
        beginTest ("four conservative starting points");
        const auto& f = Presets::factory();
        expectEquals ((int) f.size(), 4);
        juce::StringArray names;
        for (const auto& p : f)
        {
            names.add (p.name);
            expect (juce::String (p.hint).isNotEmpty(), juce::String (p.name) + " has a listening cue");
            expect (! p.allButtons, "starting points stay out of All Buttons");
        }
        expectEquals (names.joinIntoString (", "), juce::String ("Lead Vocal Control, Acoustic Guitar Strum Control, Bass Guitar Evening, Drum Room Punch"));

        beginTest ("each starting point is level-matched and gently compressing at 0 VU");
        for (int i = 0; i < (int) f.size(); ++i)
        {
            const auto r = measurePreset (i);
            const auto label = juce::String (f[(size_t) i].name) + ": out " + juce::String (r.outDb, 1) + " dBFS, GR "
                             + juce::String (r.grDb, 1) + " dB";
            expectWithinAbsoluteError (r.outDb, r.inDb, 1.0f, label);
            expect (r.grDb >= 1.5f && r.grDb <= 10.0f, label);
        }

        beginTest ("host program list is the starting points");
        {
            SongsmithAnalogCompressorProcessor p;
            expectEquals (p.getNumPrograms(), 4);
            expectEquals (p.getProgramName (3), juce::String ("Drum Room Punch"));
            p.setCurrentProgram (2);
            expectEquals (p.getCurrentProgram(), 2);
            expectEquals ((int) p.apvts.getRawParameterValue (ID::ratio)->load(), f[2].ratio);
            expectWithinAbsoluteError (p.apvts.getRawParameterValue (ID::input)->load(), f[2].input, 0.01f);
            expectEquals (p.presets.currentName, juce::String ("Bass Guitar Evening"));
        }

        beginTest ("loading a preset is one gesture per changed parameter");
        {
            SongsmithAnalogCompressorProcessor p;
            struct Counter : juce::AudioProcessorParameter::Listener
            {
                int begins = 0, ends = 0;
                void parameterValueChanged (int, float) override {}
                void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
            } counter;
            auto* input = p.apvts.getParameter (ID::input);
            input->addListener (&counter);
            p.presets.loadFactory (0);
            p.presets.loadFactory (0);   // no change the second time: no gesture
            input->removeListener (&counter);
            expectEquals (counter.begins, 1);
            expectEquals (counter.ends, 1);
        }

        beginTest ("user presets: save, list and load; never bypass or oversampling");
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile ("fet-preset-test", {}, false);
            SongsmithAnalogCompressorProcessor a;
            a.presets.setUserFolder (folder);
            ColourMeasure::set (a, ID::input, 17.0f);
            ColourMeasure::set (a, ID::ratio, 3.0f);
            ColourMeasure::set (a, ID::allButtons, 1.0f);
            ColourMeasure::set (a, ID::bypass, 1.0f);
            ColourMeasure::set (a, ID::oversampling, 0.0f);
            juce::String error;
            expect (a.presets.saveAs ("My: vocal?", error), error);
            const auto files = a.presets.getUserPresetFiles();
            expectEquals (files.size(), 1);
            expect (files[0].getFileName().endsWith (".sspreset"));

            SongsmithAnalogCompressorProcessor b;
            b.presets.setUserFolder (folder);
            expect (b.presets.loadFile (files[0], error), error);
            expectWithinAbsoluteError (b.apvts.getRawParameterValue (ID::input)->load(), 17.0f, 0.01f);
            expectEquals ((int) b.apvts.getRawParameterValue (ID::ratio)->load(), 3);
            expect (b.isAllButtonsOn());
            expectEquals ((int) b.apvts.getRawParameterValue (ID::bypass)->load(), 0, "bypass isn't part of a preset");
            expectEquals ((int) b.apvts.getRawParameterValue (ID::oversampling)->load(), oversamplingDefault, "nor is oversampling");

            const auto bad = folder.getChildFile ("Broken.sspreset");
            bad.replaceWithText ("not xml");
            expect (! b.presets.loadFile (bad, error), "garbage is refused");
            expect (error.isNotEmpty());
            expect (! b.presets.saveAs ("   ", error), "empty name refused");
            folder.deleteRecursively();
        }

        beginTest ("user presets have their own folder, apart from Songsmith EQ's");
        {
            const auto folder = Presets::getDefaultUserFolder().getFullPathName();
            expect (folder.contains ("Songsmith Studio"));
            expect (folder.contains ("Songsmith Analog Compressor"));
            expect (! folder.contains ("Songsmith EQ"));
            expect (folder.contains ("fet-tests-appdata"), "tests never touch the real folder");
        }

        beginTest ("the preset name and cue are recalled with the session");
        {
            juce::MemoryBlock mb;
            {
                SongsmithAnalogCompressorProcessor a;
                a.setCurrentProgram (1);
                a.getStateInformation (mb);
            }
            SongsmithAnalogCompressorProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.presets.currentName, juce::String ("Acoustic Guitar Strum Control"));
            expect (b.presets.currentHint.isNotEmpty());
        }

        beginTest ("SUI-09: duplicate names are detected, a replace keeps one file, factory names are never files");
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile ("fet-preset-dup-test", {}, false);
            SongsmithAnalogCompressorProcessor a;
            a.presets.setUserFolder (folder);
            juce::String error;

            expect (! a.presets.userPresetExists ("Mine"), "nothing saved yet");
            ColourMeasure::set (a, ID::input, 10.0f);
            expect (a.presets.saveAs ("Mine", error), error);
            expect (a.presets.userPresetExists ("Mine"));
            expect (a.presets.userPresetExists ("  Mine  "), "names are trimmed like saveAs does");
            expect (! a.presets.userPresetExists ("   "), "an empty name never exists");

            ColourMeasure::set (a, ID::input, 20.0f);
            expect (a.presets.saveAs ("Mine", error), error);   // the editor asks first, then calls this
            expectEquals (a.presets.getUserPresetFiles().size(), 1, "replacing doesn't add a second file");

            SongsmithAnalogCompressorProcessor b;
            b.presets.setUserFolder (folder);
            expect (b.presets.loadFile (a.presets.getUserPresetFiles()[0], error), error);
            expectWithinAbsoluteError (b.apvts.getRawParameterValue (ID::input)->load(), 20.0f, 0.01f, "the newer settings won");

            // A factory preset is not a file, so saving under its name makes a separate user preset.
            const auto factoryName = juce::String (Presets::factory()[0].name);
            expect (! a.presets.userPresetExists (factoryName));
            expect (a.presets.saveAs (factoryName, error), error);
            expectEquals (a.presets.getUserPresetFiles().size(), 2);
            expectEquals ((int) Presets::factory().size(), 4, "the factory list is unchanged");

            // Invalid files: wrong root tag and empty files are refused without changing the sound.
            const auto before = b.apvts.getRawParameterValue (ID::input)->load();
            const auto wrong = folder.getChildFile ("Wrong.sspreset");
            wrong.replaceWithText ("<SomethingElse input=\"3\"/>");
            expect (! b.presets.loadFile (wrong, error), "a different root tag is refused");
            const auto empty = folder.getChildFile ("Empty.sspreset");
            empty.replaceWithText ("");
            expect (! b.presets.loadFile (empty, error), "an empty file is refused");
            expectEquals (b.apvts.getRawParameterValue (ID::input)->load(), before, "a refused file changes nothing");
            folder.deleteRecursively();
        }
    }
};

static PresetTests presetTests;
