#pragma once

// ACOMP-09 editor tests: the Set Levels flow, Undo, refusing to write over changes, wording and layout,
// and the Attack and Release tooltips against the DSP. Included after CalibrationUnitTests.h.
#include "CalibrationUnitTests.h"

struct CalibrationEditorTests final : juce::UnitTest
{
    CalibrationEditorTests() : juce::UnitTest ("Set Levels editor", "FET") {}

    using Stage = SongsmithAnalogCompressorEditor::LevelsStage;
    static constexpr double fs = 48000.0;

    void runTest() override
    {
        using namespace CalibrationTestSupport;

        beginTest ("Set Levels sits under Output, beside Input, and does nothing until Apply");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            auto& button = e.getSetLevelsButton();
            expectEquals (button.getButtonText(), juce::String ("Set Levels"));
            expect (button.getTooltip().isNotEmpty() && button.getTitle().isNotEmpty(), "tooltip and accessible name");
            const auto out = e.getKnob (ID::output).getBounds(), in = e.getKnob (ID::input).getBounds();
            expect (button.getX() >= out.getX() && button.getRight() <= out.getRight(), "directly under the Output knob");
            expect (button.getY() >= out.getBottom() - 1, "below the knob, not on it");
            expect (button.getX() > in.getRight() - 1, "beside Input");
            expect (! button.getBounds().intersects (e.getIgnoreBelow().getBounds()), "clear of Ignore Below");
            expect (! button.getBounds().intersects (e.getGrOffBounds()), "clear of Gain Reduction Off");
            expect (button.getBottom() <= FET::contentHeight, "inside the controls area");
            expect (! e.getSetLevelsCancelButton().isVisible(), "Cancel only while a calibration is open");

            const float in0 = getParam (p, ID::input);
            button.onClick();   // start listening
            expect (e.getLevelsStage() == Stage::listening);
            expect (e.getSetLevelsCancelButton().isVisible());
            expectEquals (getParam (p, ID::input), in0, "Input does not move while listening");
        }

        beginTest ("listen, finish, apply: Input and Output are written once, undoable and still adjustable");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            e.getSetLevelsButton().onClick();
            play (p, sustained (fs, 6.0, 0.1f));
            e.getSetLevelsButton().onClick();   // Finish Listening
            expect (e.getLevelsStage() == Stage::proposal);
            expectEquals (e.getSetLevelsButton().getButtonText(), juce::String ("Apply"));
            const auto r = e.getLevelsResult();
            expect (r.status == FET::Calibration::Status::good);
            expect (e.getSetLevelsMessage().getText().contains ("Input") && e.getSetLevelsMessage().getText().contains ("Output"),
                    "the proposal and measured reduction are shown: " + e.getSetLevelsMessage().getText());
            expect (e.getSetLevelsMessage().getText().contains ("gain reduction"));
            expectEquals (getParam (p, ID::input), 0.0f, "nothing changes before Apply");

            e.getSetLevelsButton().onClick();   // Apply
            expect (e.getLevelsStage() == Stage::idle);
            expectWithinAbsoluteError (getParam (p, ID::input), r.inputDb, 0.011f);
            expectWithinAbsoluteError (getParam (p, ID::output), r.outputDb, 0.011f);

            expect (p.history.canUndo(), "one Undo step");
            p.history.undo();
            expectEquals (getParam (p, ID::input), 0.0f);
            expectEquals (getParam (p, ID::output), 0.0f);
            expect (! p.history.canUndo(), "applying added exactly one step");
            p.history.redo();
            expectWithinAbsoluteError (getParam (p, ID::input), r.inputDb, 0.011f);

            e.getKnob (ID::input).slider.setValue (5.0, juce::sendNotificationSync);   // manual adjustment still works
            expectWithinAbsoluteError (getParam (p, ID::input), 5.0f, 0.011f);
        }

        beginTest ("Cancel leaves everything alone; silence is explained");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            set (p, ID::input, 3.0f);
            e.getSetLevelsButton().onClick();
            play (p, Signal ((size_t) (fs * 4.0), 0.0f));
            e.getSetLevelsButton().onClick();
            expect (e.getLevelsResult().status == FET::Calibration::Status::silent);
            expect (! e.getSetLevelsButton().isEnabled(), "nothing to apply");
            expect (e.getSetLevelsMessage().getText().containsIgnoreCase ("no sound"), e.getSetLevelsMessage().getText());
            e.getSetLevelsCancelButton().onClick();
            expect (e.getLevelsStage() == Stage::idle);
            expectEquals (getParam (p, ID::input), 3.0f);
            expect (! p.history.canUndo());
        }

        beginTest ("settings that change while listening or before Apply are never overwritten");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            e.getSetLevelsButton().onClick();
            play (p, sustained (fs, 4.0, 0.1f));
            set (p, ID::output, -6.0f);   // automation or the musician moves Output mid-way
            e.getSetLevelsButton().onClick();
            expect (e.getLevelsStage() == Stage::idle, "no proposal from stale settings");
            expectEquals (getParam (p, ID::output), -6.0f);

            e.getSetLevelsButton().onClick();
            play (p, sustained (fs, 4.0, 0.1f));
            e.getSetLevelsButton().onClick();
            expect (e.getLevelsStage() == Stage::proposal);
            set (p, ID::input, 20.0f);
            e.getSetLevelsButton().onClick();   // Apply
            expectEquals (getParam (p, ID::input), 20.0f, "the newer change wins");
            expectEquals (getParam (p, ID::output), -6.0f);
        }

        beginTest ("Set Levels wording is plain, and every message fits its box");
        {
            using namespace Songsmith::UI;
            using S = FET::Calibration::Status;
            FET::Calibration::Result r;
            r.inputDb = 9.5f; r.outputDb = 2.1f; r.reductionDb = 3.0f; r.clipRisk = true;
            for (auto s : { S::silent, S::tooShort, S::good, S::inputAtLimit, S::outputAtLimit })
            {
                r.status = s;
                for (float reduction : { 0.5f, 3.0f, 9.0f })
                {
                    r.reductionDb = reduction;
                    const auto text = SongsmithAnalogCompressorEditor::levelsMessageFor (r);
                    expect (Wording::findBanned (text).isEmpty(), "plain words: " + text);
                    expect (text.length() <= 150, "fits three lines: " + text);
                }
            }
            SongsmithAnalogCompressorProcessor p;
            SongsmithAnalogCompressorEditor e (p);
            expect (Wording::findBanned (e.getSetLevelsButton().getTooltip()).isEmpty());
            expect (Wording::findBanned (e.getSetLevelsCancelButton().getTooltip()).isEmpty());
        }

        beginTest ("Attack and Release tooltips say what they do and that clockwise is faster, and the DSP agrees");
        {
            SongsmithAnalogCompressorProcessor p;
            SongsmithAnalogCompressorEditor e (p);
            const auto attack = e.getKnob (ID::attack).slider.getTooltip();
            const auto release = e.getKnob (ID::release).slider.getTooltip();
            expect (attack.containsIgnoreCase ("how quickly compression starts"), attack);
            expect (release.containsIgnoreCase ("how quickly compression lets go"), release);
            for (const auto& tip : { attack, release })
            {
                expect (tip.containsIgnoreCase ("clockwise") && tip.containsIgnoreCase ("faster"), tip);
                expect (Songsmith::UI::Wording::findBanned (tip).isEmpty(), tip);
            }

            // Clockwise on the dial is the higher value...
            for (auto* id : { ID::attack, ID::release })
            {
                auto& s = e.getKnob (id).slider;
                const auto params = s.getRotaryParameters();
                expect (params.endAngleRadians > params.startAngleRadians, "the dial runs clockwise from low to high");
                expectGreaterThan (s.valueToProportionOfLength (7.0), s.valueToProportionOfLength (1.0));
            }
            // ...and a higher value is measurably faster.
            for (int ratio = 0; ratio < 4; ++ratio)
            {
                expectLessThan (Measure::attackSeconds (fs, ratio, 7.0f, 20.0f), Measure::attackSeconds (fs, ratio, 1.0f, 20.0f), "attack");
                expectLessThan (Measure::releaseSeconds (fs, ratio, 7.0f, 20.0f), Measure::releaseSeconds (fs, ratio, 1.0f, 20.0f), "release");
            }
        }
    }
};
