// All Buttons, editor, meter and recall tests (step 4). Definitions in fet/docs/all-buttons.md
// and fet/docs/editor.md.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FETEngine.h"
#include "EngineMeasure.h"
#include "ColourMeasure.h"
#include "../../shared/Tests/SongsmithUITests.h"
#include "SongsmithWording.h"
#include "FETPresets.h"

using namespace FET;
using namespace FET::UI;

namespace
{
    // Settled output level (dB) for a constant-envelope input at levelDb.
    float settledOut (Engine& e, float levelDb)
    {
        return levelDb - Measure::settledGrDb (e, 48000.0, levelDb);
    }

    // A synthetic drum hit: a 1 ms click on top of a decaying 80 Hz body.
    std::vector<float> drumHit (double fs, float amp)
    {
        std::vector<float> x ((size_t) (fs * 0.3));
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / fs;
            const double body = std::sin (juce::MathConstants<double>::twoPi * 80.0 * t) * std::exp (-t / 0.08);
            const double click = (t < 0.001 ? 1.0 - t / 0.001 : 0.0) * std::sin (juce::MathConstants<double>::twoPi * 3000.0 * t);
            x[i] = amp * (float) (0.7 * body + 0.6 * click);
        }
        return x;
    }

    // How far the hit's initial transient (first 2 ms) sticks out above its compressed body
    // (20-60 ms), compared with the same measure on the input: positive = the compressor let
    // the transient through relative to the body.
    float transientPassDb (bool allButtons, float attackKnob)
    {
        Engine e;
        e.prepare (48000.0);
        e.setRatioIndex (3);
        e.setAllButtons (allButtons);
        e.setAttackKnob (attackKnob);
        e.setReleaseKnob (4.0f);
        auto hit = drumHit (48000.0, 0.9f);
        auto in = hit;
        float* ch[] = { hit.data() };
        e.process (ch, 1, (int) hit.size());
        const auto peak = [] (const std::vector<float>& x, size_t from, size_t to)
        {
            float p = 0.0f;
            for (size_t i = from; i < to; ++i) p = std::max (p, std::abs (x[i]));
            return p;
        };
        const float inStick  = peak (in, 0, 96) / peak (in, 960, 2880);
        const float outStick = peak (hit, 0, 96) / peak (hit, 960, 2880);
        return 20.0f * std::log10 (outStick / inStick);
    }
}

//==============================================================================
struct AllButtonsTests final : juce::UnitTest
{
    AllButtonsTests() : juce::UnitTest ("All Buttons", "FET") {}

    void runTest() override
    {
        beginTest ("stepped tones: effective ratio between 12:1 and 20:1 above the knee (UM p. 22)");
        {
            Engine e;
            e.prepare (48000.0);
            e.setAllButtons (true);
            const float t = Engine::abThresholdDb;
            // Tones stepped in 2 dB steps from +10 to +22 dB over the threshold (below the
            // over-compression point): the overall ratio is in the published range, and no step
            // is looser than 8:1 (the soft knee).
            const float first = settledOut (e, t + 10.0f), last = settledOut (e, t + 22.0f);
            const float ratio = 12.0f / (last - first);
            expect (ratio >= 12.0f && ratio <= 20.0f, "effective ratio " + juce::String (ratio, 1) + ":1");
            for (float in = t + 10.0f; in < t + 22.0f; in += 2.0f)
            {
                const float slope = (settledOut (e, in + 2.0f) - settledOut (e, in)) / 2.0f;
                expectLessThan (slope, 1.0f / 8.0f, "step at +" + juce::String (in - t, 0) + " dB: " + juce::String (1.0f / slope, 1) + ":1");
            }
        }

        beginTest ("bias shift: more gain reduction than 20:1 at the same input");
        {
            Engine ab, r20;
            ab.prepare (48000.0);
            r20.prepare (48000.0);
            ab.setAllButtons (true);
            r20.setRatioIndex (3);
            for (float level : { -18.0f, -12.0f, -6.0f })
                expectGreaterThan (ab.staticGainReductionDb (level), r20.staticGainReductionDb (level) + 1.0f,
                                   "at " + juce::String (level, 0) + " dBFS");
        }

        beginTest ("over-compression: very hot input comes out quieter, not louder");
        {
            Engine e;
            e.prepare (48000.0);
            e.setAllButtons (true);
            const float t = Engine::abThresholdDb;
            expectLessThan (settledOut (e, t + 40.0f), settledOut (e, t + 30.0f));
        }

        beginTest ("the dynamic engine settles on the All Buttons curve at every step");
        {
            Engine e;
            e.prepare (48000.0);
            e.setAllButtons (true);
            for (float level = -40.0f; level <= 6.0f; level += 6.0f)
                expectWithinAbsoluteError (Measure::settledGrDb (e, 48000.0, level), e.staticGainReductionDb (level), 0.1f,
                                           juce::String (level, 0) + " dBFS");
        }

        beginTest ("drum transients: attack lag lets the initial transient through");
        {
            const float ab = transientPassDb (true, 7.0f), r20 = transientPassDb (false, 7.0f);
            expectGreaterThan (ab, r20 + 2.0f, "All Buttons " + juce::String (ab, 1) + " dB vs 20:1 " + juce::String (r20, 1) + " dB");
        }

        beginTest ("release speeds up with depth");
        {
            // Release from deep GR is faster than from shallow GR (the 1/e time).
            const auto releaseFrom = [] (float overDb)
            {
                Engine e;
                e.prepare (48000.0);
                e.setAllButtons (true);
                e.setReleaseKnob (4.0f);
                const float t = Engine::abThresholdDb;
                Measure::runConstant (e, t + overDb, 24000);
                const float start = e.getGainReductionDb();
                std::vector<float> gr;
                Measure::runConstant (e, t - 40.0f, 96000, &gr);
                for (size_t i = 0; i < gr.size(); ++i)
                    if (gr[i] <= start * std::exp (-1.0f))
                        return (double) i / 48000.0;
                return -1.0;
            };
            expectLessThan (releaseFrom (24.0f), releaseFrom (6.0f));
        }

        beginTest ("distortion rises radically at a matched level");
        {
            ColourMeasure::Settings s;
            s.inputDb = 12.0f;
            s.ratio = 3;
            const auto r20 = ColourMeasure::measure (s);
            // All Buttons via the parameter: render with it on, Output trimmed to the same level.
            SongsmithAnalogCompressorProcessor probe;
            juce::ignoreUnused (probe);
            ColourMeasure::Settings a = s;
            const auto abRun = [&] (float outDb)
            {
                SongsmithAnalogCompressorProcessor p;
                ColourMeasure::set (p, ID::allButtons, 1.0f);
                ColourMeasure::set (p, ID::ratio, 3.0f);
                ColourMeasure::set (p, ID::input, a.inputDb);
                ColourMeasure::set (p, ID::output, outDb);
                ColourMeasure::set (p, ID::release, 1.0f);
                p.prepareToPlay (48000.0, 512);
                const double f = ColourMeasure::snap (1000.0, 48000.0);
                const float amp = std::pow (10.0f, -18.0f / 20.0f) * std::sqrt (2.0f);
                juce::AudioBuffer<float> b (2, 512);
                juce::MidiBuffer m;
                std::vector<float> out ((size_t) ColourMeasure::fftSize);
                const int settle = 72000, total = settle + ColourMeasure::fftSize;
                for (int pos = 0; pos < total; pos += 512)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 512; ++i)
                            b.setSample (c, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * f * (pos + i) / 48000.0));
                    p.processBlock (b, m);
                    for (int i = 0; i < 512; ++i)
                        if (pos + i >= settle && pos + i < total)
                            out[(size_t) (pos + i - settle)] = b.getSample (0, i);
                }
                return ColourMeasure::analyse (out, 48000.0, 1000.0);
            };
            const auto first = abRun (0.0f);
            const auto ab = abRun ((float) (r20.fundamentalDb - first.fundamentalDb));
            expectWithinAbsoluteError (ab.fundamentalDb, r20.fundamentalDb, 0.5, "matched");
            expectGreaterThan (ab.thdPercent, 2.0 * r20.thdPercent,
                               "All Buttons " + juce::String (ab.thdPercent, 2) + " % vs 20:1 " + juce::String (r20.thdPercent, 2) + " %");
        }

        beginTest ("switching All Buttons and ratios is click-free");
        {
            Engine e;
            e.prepare (48000.0);
            std::vector<float> s (60000, 0.5f);
            float maxStep = 0.0f, prev = 0.0f;
            for (int k = 0; k < 60; ++k)
            {
                e.setAllButtons (k % 2 == 1);
                e.setRatioIndex (k % 4);
                float* ch[] = { s.data() + k * 1000 };
                e.process (ch, 1, 1000);
                for (int i = 0; i < 1000; ++i)
                {
                    const float v = s[(size_t) (k * 1000 + i)];
                    if (k * 1000 + i > 2000)
                        maxStep = std::max (maxStep, std::abs (v - prev));
                    prev = v;
                }
            }
            expectLessThan (maxStep, 0.01f, "largest step " + juce::String (maxStep, 4));
        }
    }
};

//==============================================================================
struct MeterTests final : juce::UnitTest
{
    MeterTests() : juce::UnitTest ("Meter", "FET") {}

    void runTest() override
    {
        beginTest ("calibration: 0 VU = -18 dBFS in +4 mode, -14 dBFS in +8 mode, GR mode reads -GR");
        expectWithinAbsoluteError (meterReadingVu (MeterMode::Plus4, 0.0f, -18.0f, false), 0.0f, 1.0e-4f);
        expectWithinAbsoluteError (meterReadingVu (MeterMode::Plus8, 0.0f, -14.0f, false), 0.0f, 1.0e-4f);
        expectWithinAbsoluteError (meterReadingVu (MeterMode::Plus8, 0.0f, -18.0f, false), -4.0f, 1.0e-4f);
        expectWithinAbsoluteError (meterReadingVu (MeterMode::GR, 7.0f, -10.0f, false), -7.0f, 1.0e-4f);
        expectWithinAbsoluteError (meterReadingVu (MeterMode::GR, 0.0f, -10.0f, false), 0.0f, 1.0e-4f);

        beginTest ("All Buttons exaggerates the GR reading (the meter 'goes wild', UM p. 9)");
        expectLessThan (meterReadingVu (MeterMode::GR, 6.0f, -10.0f, true), -6.0f - 3.0f + 0.01f);
        expectEquals (meterReadingVu (MeterMode::GR, 0.0f, -10.0f, true), 0.0f, "rests at 0 without GR");

        beginTest ("needle scale: +3 VU is full scale, 0 VU sits at about 71 %");
        expectWithinAbsoluteError (needleFraction (3.0f), 1.0f, 1.0e-5f);
        expectWithinAbsoluteError (needleFraction (0.0f), 0.708f, 0.002f);

        beginTest ("VU ballistics: 99 % in 300 ms, about 1 % overshoot (IEC 60268-17)");
        {
            VuBallistics b;
            b.reset (0.0f);
            float t = 0.0f, reached = -1.0f, peak = 0.0f;
            while (t < 2.0f)
            {
                const float x = b.advance (1.0f, 0.001f);
                t += 0.001f;
                if (reached < 0.0f && x >= 0.99f)
                    reached = t;
                peak = std::max (peak, x);
            }
            expectWithinAbsoluteError (reached, 0.3f, 0.03f, "99 % at " + juce::String (reached * 1000.0f, 0) + " ms");
            expect (peak >= 1.003f && peak <= 1.015f, "overshoot " + juce::String ((peak - 1.0f) * 100.0f, 2) + " %");
        }

        beginTest ("display ballistics are independent of the audio block size");
        {
            VuBallistics a, b;
            a.reset (0.0f);
            b.reset (0.0f);
            for (int i = 0; i < 30; ++i) a.advance (0.8f, 1.0f / 30.0f);
            for (int i = 0; i < 60; ++i) b.advance (0.8f, 1.0f / 60.0f);
            expectWithinAbsoluteError (a.position(), b.position(), 1.0e-3f);
        }
    }
};

//==============================================================================
struct EditorTests4 final : juce::UnitTest
{
    EditorTests4() : juce::UnitTest ("Editor (vector)", "FET") {}

    void runTest() override
    {
        beginTest ("Plain words: every text, parameter name and preset, and every label fits");
        for (float s : { 1.0f, 2.0f })   // the default (and smallest) interface size, and the largest
        {
            SongsmithAnalogCompressorProcessor p;
            p.uiScale.store (s);
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            const auto context = "Analog Compressor at " + juce::String (juce::roundToInt (s * 100.0f)) + " %";

            juce::StringArray texts;
            Songsmith::UI::Wording::collect (*editor, texts);
            for (int m = 0; m < 3; ++m)   // painted: the meter mode on the needle face
                texts.add (SongsmithAnalogCompressorEditor::meterModeName (m));
            texts.add (SongsmithAnalogCompressorEditor::gainReductionText (3.2f));
            for (auto* prm : p.getParameters())
            {
                texts.add (prm->getName (100));
                texts.add (prm->getCurrentValueAsText());
            }
            for (bool in : { true, false })
                texts.add (p.getBus (in, 0)->getName());
            for (auto& preset : FET::Presets::factory())
            {
                texts.add (preset.name);
                texts.add (preset.hint);
            }
            expectGreaterThan (texts.size(), 40, context + ": found the text");
            Songsmith::UI::Wording::expectPlainWords (*this, texts, context);

            for (auto* t : { "Gain Reduction Off", "All Buttons", "Gain Reduction", "Output +4", "Gain Reduction 3.2 dB", "Oversampling: off", "Interface size: 100 %" })
                expect (texts.contains (t), context + ": shows \"" + juce::String (t) + "\"");

            auto* ed = dynamic_cast<SongsmithAnalogCompressorEditor*> (editor.get());
            if (ed != nullptr)
                Songsmith::UI::Wording::expectTextFits (*this, *editor, editor->getLookAndFeel(), context);
        }

        beginTest ("Every button, slider and combo box uses the shared bevelled look");
        {
            SongsmithAnalogCompressorProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            Songsmith::UI::Testing::checkControlsUseSharedLook (*this, *editor, "editor");
        }

        beginTest ("Editor text uses the shared typeface and type scale");
        {
            SongsmithAnalogCompressorProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
            Songsmith::UI::Testing::checkEditorFonts (*this, *editor, "Analog Compressor editor");
        }

        beginTest ("100 %, 150 % and 200 % layouts keep the design proportions");
        for (float s : { 1.0f, 1.5f, 2.0f })
        {
            SongsmithAnalogCompressorProcessor p;
            p.uiScale.store (s);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            expectEquals (e->getWidth(), juce::roundToInt (designWidth * s));
            expectEquals (e->getHeight(), juce::roundToInt (designHeight * s));
            auto* ed = dynamic_cast<SongsmithAnalogCompressorEditor*> (e.get());
            expect (ed != nullptr);
            if (ed == nullptr) continue;

            // Every control is inside the window, at the scaled position.
            const auto knob = ed->getKnob (ID::input).slider.getScreenBounds();
            juce::ignoreUnused (knob);
            const auto area = e->getLocalArea (&ed->getKnob (ID::release), ed->getKnob (ID::release).getLocalBounds());
            expect (e->getLocalBounds().contains (area), "release knob inside the window at " + juce::String (s));
            expectWithinAbsoluteError ((float) area.getWidth(), 112.0f * s, 1.5f);
        }

        beginTest ("readable values and double-click defaults");
        {
            SongsmithAnalogCompressorProcessor p;
            SongsmithAnalogCompressorEditor e (p);
            auto& attack = e.getKnob (ID::attack);
            expect (attack.getValueText().contains ("126"), attack.getValueText());   // knob 4 = 126 us
            auto& input = e.getKnob (ID::input);
            input.slider.setValue (12.0, juce::sendNotificationSync);
            expectEquals (input.getValueText(), juce::String ("+12.0 dB"));
            expect (input.slider.isDoubleClickReturnEnabled());
            expectWithinAbsoluteError (input.slider.getDoubleClickReturnValue(), 0.0, 1.0e-6);
            expectWithinAbsoluteError (e.getKnob (ID::release).slider.getDoubleClickReturnValue(), (double) releaseDefault, 1.0e-6);
        }

        beginTest ("ratio buttons and All Buttons drive the parameters");
        {
            SongsmithAnalogCompressorProcessor p;
            SongsmithAnalogCompressorEditor e (p);
            e.getRatioButton (2).onClick();
            expectEquals ((int) p.apvts.getRawParameterValue (ID::ratio)->load(), 2);
            e.getAllButton().onClick();
            expect (p.isAllButtonsOn());
            for (int i = 0; i < 4; ++i)
                expect (e.getRatioButton (i).getToggleState(), "all four lit");
            e.getRatioButton (0).onClick();
            expect (! p.isAllButtonsOn(), "a single ratio leaves All Buttons");
            expectEquals ((int) p.apvts.getRawParameterValue (ID::ratio)->load(), 0);
        }

        beginTest ("automation gestures wrap every button press");
        {
            SongsmithAnalogCompressorProcessor p;
            struct Counter : juce::AudioProcessorParameter::Listener
            {
                int begins = 0, ends = 0;
                void parameterValueChanged (int, float) override {}
                void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
            } counter;
            auto* ratio = p.apvts.getParameter (ID::ratio);
            ratio->addListener (&counter);
            {
                SongsmithAnalogCompressorEditor e (p);
                e.getRatioButton (3).onClick();
            }
            ratio->removeListener (&counter);
            expectEquals (counter.begins, 1);
            expectEquals (counter.ends, 1);
        }

        beginTest ("session recall: interface size and meter mode");
        {
            juce::MemoryBlock mb;
            {
                SongsmithAnalogCompressorProcessor a;
                a.uiScale.store (1.5f);
                a.meterMode.store (2);
                a.getStateInformation (mb);
            }
            SongsmithAnalogCompressorProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (b.uiScale.load(), 1.5f);
            expectEquals (b.meterMode.load(), 2);
            std::unique_ptr<juce::AudioProcessorEditor> e (b.createEditor());
            expectEquals (e->getWidth(), juce::roundToInt (designWidth * 1.5f));

            SongsmithAnalogCompressorProcessor c;
            juce::XmlElement wild ("SongsmithAnalogCompressor");
            auto* ui = wild.createNewChildElement ("UI");
            ui->setAttribute ("uiScale", 7.0);
            ui->setAttribute ("meterMode", 9);
            juce::AudioProcessor::copyXmlToBinary (wild, mb);
            c.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals (c.uiScale.load(), 2.0f, "snapped to the largest size");
            expectEquals (c.meterMode.load(), 2, "clamped");
        }

        beginTest ("the meter follows the audio through the editor");
        {
            SongsmithAnalogCompressorProcessor p;
            ColourMeasure::set (p, ID::input, 24.0f);
            p.prepareToPlay (48000.0, 480);
            SongsmithAnalogCompressorEditor e (p);
            juce::AudioBuffer<float> b (2, 480);
            juce::MidiBuffer m;
            for (int k = 0; k < 100; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 480; ++i)
                        b.setSample (c, i, 0.2f * (float) std::sin (0.05 * (k * 480 + i)));
                p.processBlock (b, m);
                e.advanceMeter (0.01f);
            }
            expectLessThan (e.getMeter().ballistics.position(), needleFraction (-5.0f), "needle shows gain reduction");
        }
    }
};

static AllButtonsTests allButtonsTests;
static MeterTests meterTests;
static EditorTests4 editorTests4;
static SongsmithUITests sharedUITests { "FET" };
