#pragma once

// ACOMP-10 tests: switchable models. Included once by AnalogCompressorTests.cpp after CalibrationTests.h.
#include "CalibrationTests.h"

struct ModelTests final : juce::UnitTest
{
    ModelTests() : juce::UnitTest ("Models", "FET") {}

    static constexpr double fs = 48000.0;

    // Steady gain reduction and the time (ms) to reach half of it, for a constant -6 dBFS level then silence.
    struct Timing { float steadyDb; double attackHalfMs, releaseHalfMs; };

    static Timing measureTiming (int model, bool slow)
    {
        FET::Engine e;
        e.prepare (fs);
        e.setRatioIndex (0);
        e.setAttackKnob (4.0f);
        e.setReleaseKnob (4.0f);
        e.setVoicing (model, slow);
        auto step = [&e] (float level)
        {
            float x = level;
            float* ch[1] = { &x };
            e.process (ch, 1, 1);
            return e.getGainReductionDb();
        };
        const int total = (int) (fs * 2.0);
        std::vector<float> up;
        for (int i = 0; i < total; ++i)
            up.push_back (step (0.5f));
        Timing t {};
        t.steadyDb = up.back();
        for (size_t i = 0; i < up.size(); ++i)
            if (up[i] >= 0.5f * t.steadyDb) { t.attackHalfMs = 1000.0 * (double) i / fs; break; }
        const float start = up.back();
        for (int i = 0; i < (int) (fs * 4.0); ++i)
            if (step (0.0005f) <= 0.5f * start) { t.releaseHalfMs = 1000.0 * (double) i / fs; break; }
        return t;
    }

    void runTest() override
    {
        using namespace CalibrationTestSupport;

        beginTest ("model IDs, order and names are pinned; the original Rev D Blackface is the default");
        {
            expectEquals (FET::numModels, 4);
            expectEquals (FET::modelDefault, 0);
            expectEquals (juce::String (FET::modelIds[0]), juce::String ("revD-blackface"));
            expectEquals (juce::String (FET::modelIds[1]), juce::String ("revA-bluestripe"));
            expectEquals (juce::String (FET::modelIds[2]), juce::String ("ae1176-anniversary"));
            expectEquals (juce::String (FET::modelIds[3]), juce::String ("lindell-inspired"));
            for (int i = 0; i < FET::numModels; ++i)
                expectEquals (FET::modelIndexFromId (FET::modelIds[i]), i);
            expectEquals (FET::modelIndexFromId ("nonsense"), -1);
            expectEquals (FET::modelIndexFromId (nullptr), -1);
            expectEquals (FET::modelNames().size(), FET::numModels);
            expectEquals (FET::modelDescriptions().size(), FET::numModels);
            expect (FET::modelNames()[0].contains ("Rev D") && FET::modelNames()[0].contains ("Blackface"));
            expect (FET::modelNames()[3].startsWith ("Lindell"));
            SongsmithAnalogCompressorProcessor p;
            expectEquals (p.getModelIndex(), 0);
            expect (! p.isSlowAttackOn());
        }

        beginTest ("the Rev D Blackface voicing leaves the engine bit-exact");
        {
            juce::Random rnd (7);
            std::vector<float> in (20000);
            for (auto& s : in)
                s = (rnd.nextFloat() * 2.0f - 1.0f) * 0.6f;
            auto run = [&] (bool callVoicing)
            {
                FET::Engine e;
                e.prepare (fs);
                if (callVoicing)
                    e.setVoicing (0, false);
                auto copy = in;
                float* ch[1] = { copy.data() };
                e.process (ch, 1, (int) copy.size());
                return copy;
            };
            expect (run (true) == run (false), "identical samples");
        }

        beginTest ("gain reduction differs by model in the intended direction (4:1, -12 dBFS)");
        {
            auto gr = [] (int model)
            {
                FET::Engine e;
                e.prepare (fs);
                e.setRatioIndex (0);
                e.setVoicing (model, false);
                return e.staticGainReductionDb (-12.0f);
            };
            expectGreaterThan (gr (1), gr (0) + 0.3f);   // Rev A takes off more
            expectGreaterThan (gr (0), gr (2) + 0.3f);   // the Anniversary Edition is gentler
            expectGreaterThan (std::abs (gr (3) - gr (0)), 0.2f);
        }

        beginTest ("timing differs by model: attack and release (measured on the engine)");
        {
            const auto a = measureTiming (1, false), d = measureTiming (0, false), l = measureTiming (3, false);
            expectGreaterThan (d.steadyDb, 1.0f);
            expectLessThan (a.attackHalfMs, d.attackHalfMs, "Rev A attacks faster than Rev D");
            expectGreaterThan (l.attackHalfMs, d.attackHalfMs, "Lindell-inspired lets more of the first hit through");
            expectLessThan (a.releaseHalfMs, l.releaseHalfMs, "Rev A lets go sooner than Lindell-inspired");

            const auto ae = measureTiming (2, false), slow = measureTiming (2, true);
            expectGreaterThan (slow.attackHalfMs, ae.attackHalfMs * 2.0, "Slow Attack at least doubles the attack time");
            const auto notAe = measureTiming (0, true);   // Slow Attack means nothing outside the 1176AE
            expectWithinAbsoluteError (notAe.attackHalfMs, d.attackHalfMs, 1.0e-9, "ignored on Rev D");
        }

        beginTest ("harmonic colour differs by model: second harmonic scales with the model");
        {
            auto second = [] (int model)
            {
                const int n = 4800;   // 100 cycles of 1 kHz at 48 kHz
                double c2 = 0.0, s2 = 0.0;
                const float factor = FET::voicingFor (model).colourScale;
                for (int i = 0; i < n; ++i)
                {
                    const double ph = juce::MathConstants<double>::twoPi * 1000.0 * i / fs;
                    const float y = FET::FetCell::process (0.5f * (float) std::sin (ph), 0.5f, factor);
                    c2 += y * std::cos (2.0 * ph);
                    s2 += y * std::sin (2.0 * ph);
                }
                return std::sqrt (c2 * c2 + s2 * s2) * 2.0 / n;
            };
            expectGreaterThan (second (1), second (0) * 1.5, "Rev A is grittier");
            expectLessThan (second (2), second (0) * 0.8, "the Anniversary Edition is cleaner");
        }

        beginTest ("every model's session round-trips, and Slow Attack with it");
        for (int m = 0; m < FET::numModels; ++m)
        {
            juce::MemoryBlock mb;
            {
                SongsmithAnalogCompressorProcessor a;
                set (a, ID::model, (float) m);
                set (a, ID::slowAttack, m == 2 ? 1.0f : 0.0f);
                a.getStateInformation (mb);
            }
            SongsmithAnalogCompressorProcessor b;
            set (b, ID::model, (float) ((m + 1) % FET::numModels));
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.getModelIndex(), m);
            expectEquals (b.isSlowAttackOn(), m == 2);
        }

        beginTest ("sessions saved before models existed open as the Rev D Blackface");
        {
            juce::MemoryBlock mb;
            {
                SongsmithAnalogCompressorProcessor a;
                set (a, ID::input, 11.0f);
                a.getStateInformation (mb);
            }
            auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
            expect (xml != nullptr);
            if (xml == nullptr)
                return;
            xml->setAttribute ("stateVersion", 5);
            for (const char* id : { "model", "slowAttack" })
                if (auto* child = xml->getChildByAttribute ("id", id))
                    xml->removeChildElement (child, true);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);

            SongsmithAnalogCompressorProcessor b;
            set (b, ID::model, 3.0f);
            set (b, ID::slowAttack, 1.0f);
            b.setStateInformation (old.getData(), (int) old.getSize());
            expectEquals (b.getModelIndex(), 0, "Rev D Blackface");
            expect (! b.isSlowAttackOn());
            expectWithinAbsoluteError (b.apvts.getRawParameterValue (ID::input)->load(), 11.0f, 0.01f, "the rest is untouched");
        }

        beginTest ("presets: the model is saved by its stable ID, recalled, and old files load Rev D");
        {
            const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile ("fet-model-preset-test", {}, false);
            juce::String error;
            for (int m = 0; m < FET::numModels; ++m)
            {
                SongsmithAnalogCompressorProcessor a;
                a.presets.setUserFolder (folder);
                set (a, ID::model, (float) m);
                set (a, ID::ratioTwoToOne, m == 2 ? 1.0f : 0.0f);
                set (a, ID::slowAttack, m == 2 ? 1.0f : 0.0f);
                const auto name = "Model " + juce::String (m);
                expect (a.presets.saveAs (name, error), error);
                const auto file = folder.getChildFile (name + ".sspreset");
                auto xml = juce::XmlDocument::parse (file);
                expect (xml != nullptr && xml->getStringAttribute ("model") == FET::modelIds[m], "stable ID in the file");

                SongsmithAnalogCompressorProcessor b;
                set (b, ID::model, (float) ((m + 2) % FET::numModels));
                expect (b.presets.loadFile (file, error), error);
                expectEquals (b.getModelIndex(), m, "custom preset round-trip");
                expectEquals (b.isGentleRatioOn(), m == 2);
                expectEquals (b.isSlowAttackOn(), m == 2);
            }

            // A file from before models, and one naming a model this version does not know.
            for (const char* modelAttr : { "", " model=\"from-the-future\"" })
            {
                const auto file = folder.getChildFile ("Legacy.sspreset");
                file.replaceWithText ("<SongsmithAnalogCompressorPreset version=\"1\" input=\"7\"" + juce::String (modelAttr) + "/>");
                SongsmithAnalogCompressorProcessor c;
                set (c, ID::model, 3.0f);
                expect (c.presets.loadFile (file, error), error);
                expectEquals (c.getModelIndex(), 0, "legacy or unknown model loads Rev D Blackface");
                expectWithinAbsoluteError (c.apvts.getRawParameterValue (ID::input)->load(), 7.0f, 0.01f);
            }
            folder.deleteRecursively();
        }

        beginTest ("factory presets state a valid model, and loading one switches to it");
        {
            for (const auto& f : FET::Presets::factory())
                expect (f.model >= 0 && f.model < FET::numModels, juce::String (f.name) + " has a model");
            for (size_t i = 0; i < FET::Presets::factory().size(); ++i)
            {
                SongsmithAnalogCompressorProcessor p;
                set (p, ID::model, 3.0f);
                set (p, ID::slowAttack, 1.0f);
                p.setCurrentProgram ((int) i);
                expectEquals (p.getModelIndex(), FET::Presets::factory()[i].model);
                expect (! p.isSlowAttackOn());
            }
        }

        beginTest ("switching model during playback is safe, and latency and bypass do not depend on the model");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            set (p, ID::input, 12.0f);
            juce::AudioBuffer<float> b (2, 512);
            juce::MidiBuffer midi;
            float lastSample = 0.0f, worstStep = 0.0f;
            bool finite = true;
            int pos = 0;
            for (int block = 0; block < 80; ++block)
            {
                if (block % 5 == 0)
                    set (p, ID::model, (float) ((block / 5) % FET::numModels));
                for (int i = 0; i < 512; ++i, ++pos)
                {
                    const float s = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * pos / fs);
                    b.setSample (0, i, s);
                    b.setSample (1, i, s);
                }
                p.processBlock (b, midi);
                for (int i = 0; i < 512; ++i)
                {
                    const float s = b.getSample (0, i);
                    finite = finite && std::isfinite (s);
                    if (block > 0)
                        worstStep = juce::jmax (worstStep, std::abs (s - lastSample));
                    lastSample = s;
                }
            }
            expect (finite, "no NaN or infinity");
            expectLessThan (worstStep, 0.5f, "no click when the model changes");

            for (int m = 0; m < FET::numModels; ++m)
            {
                SongsmithAnalogCompressorProcessor q;
                q.prepareToPlay (fs, 512);
                set (q, ID::model, (float) m);
                q.updateLatencyNow();
                expectEquals (q.getLatencySamples(), 0, "no added delay with Oversampling off");
                set (q, ID::bypass, 1.0f);
                std::vector<float> in ((size_t) (fs * 0.5));
                for (size_t i = 0; i < in.size(); ++i)
                    in[i] = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * (double) i / fs);
                const auto out = play (q, in);
                bool exact = true;
                for (size_t i = (size_t) (fs * 0.2); i < in.size(); ++i)
                    exact = exact && out[i] == in[i];
                expect (exact, "bypass is bit-exact on model " + juce::String (m));
            }
        }

        beginTest ("Set Levels works for every model and measures what that model plays");
        {
            const auto in = sustained (fs, 6.0, 0.1f);
            for (int m = 0; m < FET::numModels; ++m)
            {
                SongsmithAnalogCompressorProcessor p;
                p.prepareToPlay (fs, 512);
                set (p, ID::model, (float) m);
                const auto r = calibrate (p, in);
                expect (r.status == FET::Calibration::Status::good, "good on model " + juce::String (m));
                expectGreaterThan (r.reductionDb, 1.0f);
                expectLessThan (r.reductionDb, 5.0f);
            }
        }
    }
};
