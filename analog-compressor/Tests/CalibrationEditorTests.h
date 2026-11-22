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

        beginTest ("Set Levels sits under Input, beside Ignore Below, and does nothing until Apply");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            auto& button = e.getSetLevelsButton();
            expectEquals (button.getButtonText(), juce::String ("Set Levels"));
            expect (button.getTooltip().isNotEmpty() && button.getTitle().isNotEmpty(), "tooltip and accessible name");
            const auto in = e.getKnob (ID::input).getBounds();
            expect (button.getX() >= in.getX() && button.getRight() <= in.getRight(), "directly under the Input knob (Output moved into the Output panel)");
            expect (button.getY() >= in.getBottom() - 1, "below the knob, not on it");
            expect (button.getX() > e.getIgnoreBelow().getRight() - 1, "beside Ignore Below, on its right");
            expect (! button.getBounds().intersects (e.getIgnoreBelow().getBounds()), "clear of Ignore Below");
            expect (! button.getBounds().intersects (e.getOutputPanel().getBounds()), "and of the Output panel");
            expect (! button.getBounds().intersects (e.getGrOffBounds()), "clear of Gain Reduction Off");
            expect (button.getBottom() <= FET::contentHeight, "inside the controls area");
            expect (! e.getSetLevelsPanel().isVisible(), "the steps panel is only open while Set Levels is in use");

            // ACOMP-21: the faceplate stays exactly where it is while the panel opens and its stages change.
            auto snapshot = [&]
            {
                juce::Array<juce::Rectangle<int>> r;
                for (auto id : { ID::input, ID::attack, ID::release })
                    r.add (e.getKnob (id).getBounds());
                r.add (e.getIgnoreBelow().getBounds());
                r.add (e.getMeter().getBounds());
                r.add (button.getBounds());
                r.add (e.getGrOffBounds());
                r.add (e.getOversamplingBox().getBounds());
                for (int i = 0; i < 4; ++i)
                    r.add (e.getRatioButton (i).getBounds());
                return r;
            };
            const auto before = snapshot();

            const float in0 = getParam (p, ID::input);
            button.onClick();   // start listening
            expect (e.getLevelsStage() == Stage::listening);
            expect (e.getSetLevelsPanel().isVisible() && e.getSetLevelsCancelButton().isVisible(), "the panel opens, with Cancel in it");
            expect (e.getSetLevelsActionButton().isVisible());
            expectWithinAbsoluteError (getParam (p, ID::input), in0, 0.011f, "Input does not move while listening");

            const auto panel = e.getSetLevelsPanel().getBounds();
            expect (juce::Rectangle<int> (0, 0, FET::designWidth, FET::contentHeight).contains (panel), "the panel stays inside the editor");
            expect (panel.getBottom() <= button.getY(), "the panel is anchored above the entry button");
            expect (panel.getRight() >= button.getRight() - 1 && panel.getX() <= button.getX(), "and over it, not drifting away");
            for (auto* c : { &e.getSetLevelsActionButton(), &e.getSetLevelsCancelButton() })
            {
                expect (c->getHeight() >= 26, "click target height");
                expect (e.getSetLevelsPanel().getLocalBounds().contains (c->getBounds()), "buttons are inside the panel");
            }
            expect (! e.getSetLevelsActionButton().getBounds().intersects (e.getSetLevelsCancelButton().getBounds()));
            expect (! e.getSetLevelsMessage().getBounds().intersects (e.getSetLevelsActionButton().getBounds()), "message clear of the buttons");
            expect (button.getButtonText() == "Set Levels", "the entry button never changes its label");

            expect (snapshot() == before, "listening does not move or resize any faceplate control");
            e.getSetLevelsPanel().keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (e.getLevelsStage() == Stage::idle && ! e.getSetLevelsPanel().isVisible(), "Escape cancels and closes the panel");
            expect (snapshot() == before, "and nothing moved");
        }

        beginTest ("listen, finish, apply: Input and Output are written once, undoable and still adjustable");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            e.getSetLevelsButton().onClick();
            play (p, sustained (fs, 6.0, 0.1f));
            e.getSetLevelsActionButton().onClick();   // Finish Listening
            expect (e.getLevelsStage() == Stage::proposal);
            expectEquals (e.getSetLevelsActionButton().getButtonText(), juce::String ("Apply"));
            expect (e.getSetLevelsPanel().isVisible(), "the proposal is shown in the panel");
            const auto r = e.getLevelsResult();
            expect (r.status == FET::Calibration::Status::good);
            expect (e.getSetLevelsMessage().getText().contains ("Input") && e.getSetLevelsMessage().getText().contains ("Output"),
                    "the proposal and measured reduction are shown: " + e.getSetLevelsMessage().getText());
            expect (e.getSetLevelsMessage().getText().contains ("gain reduction"));
            expectWithinAbsoluteError (getParam (p, ID::input), 0.0f, 0.011f, "nothing changes before Apply");

            e.getSetLevelsActionButton().onClick();   // Apply
            expect (e.getLevelsStage() == Stage::idle);
            // songsmithstudio#13: Apply is the confirmation. No second message, no Close button to press.
            expect (! e.getSetLevelsPanel().isVisible(), "Apply closes the panel at once");
            expect (e.getSetLevelsMessage().getText().isEmpty(), "with no \"Applied\" message left behind");
            expect (! e.getSetLevelsScrim().isVisible(), "and nothing is left over the controls");
            expect (e.getSetLevelsScrim().getBounds().getWidth() > 0, "(the scrim exists, hidden)");
            expectWithinAbsoluteError (getParam (p, ID::input), r.inputDb, 0.011f);
            expectWithinAbsoluteError (getParam (p, ID::output), r.outputDb, 0.011f);

            expect (p.history.canUndo(), "one Undo step");
            p.history.undo();
            expectWithinAbsoluteError (getParam (p, ID::input), 0.0f, 0.011f);
            expectWithinAbsoluteError (getParam (p, ID::output), 0.0f, 0.011f);
            expect (! p.history.canUndo(), "applying added exactly one step");
            p.history.redo();
            expectWithinAbsoluteError (getParam (p, ID::input), r.inputDb, 0.011f);

            e.getKnob (ID::input).slider.setValue (5.0, juce::sendNotificationSync);   // manual adjustment still works
            expectWithinAbsoluteError (getParam (p, ID::input), 5.0f, 0.011f);
        }

        beginTest ("songsmithstudio#13: while the panel is open nothing behind it can be reached, Tab stays inside it, and after Apply the controls work at once");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            e.setSize (FET::designWidth, FET::designHeight);
            auto& inputKnob = e.getKnob (ID::input);
            auto& content = *e.getSetLevelsPanel().getParentComponent();   // the window's whole content, in design units
            const auto knobCentre = content.getLocalPoint (&inputKnob, inputKnob.getLocalBounds().getCentre());
            expect (content.getComponentAt (knobCentre) != &e.getSetLevelsScrim(), "before Set Levels the knob is reachable");

            e.getSetLevelsButton().onClick();
            expect (e.getSetLevelsPanel().isVisible() && e.getSetLevelsScrim().isVisible(), "the panel and its scrim are shown");
            auto behindTheDialog = [&] (juce::Component* hit) { return hit == &e.getSetLevelsScrim() || hit == &e.getSetLevelsPanel() || e.getSetLevelsPanel().isParentOf (hit); };   // the panel or anything on it (its buttons, the ear)
            expect (behindTheDialog (content.getComponentAt (knobCentre)), "a click on the Input knob lands on the scrim or the panel, never on the knob");
            expect (behindTheDialog (content.getComponentAt (content.getLocalPoint (&e.getBypassButton(), e.getBypassButton().getLocalBounds().getCentre()))), "and Bypass is behind it too");
            expect (behindTheDialog (content.getComponentAt (content.getLocalPoint (&e.getOutputPanel().getTrim(), e.getOutputPanel().getTrim().getLocalBounds().getCentre()))), "and the Output Trim knob");
            expect (e.getSetLevelsPanel().isKeyboardFocusContainer(),
                    "Tab cycles inside the panel only");
            expect (content.getIndexOfChildComponent (&e.getSetLevelsScrim()) < content.getIndexOfChildComponent (&e.getSetLevelsPanel()), "the scrim is under the panel");
            expect (content.getIndexOfChildComponent (&e.getSetLevelsPanel()) == content.getNumChildComponents() - 1, "and the panel is on top");

            play (p, sustained (fs, 6.0, 0.1f));
            e.getSetLevelsActionButton().onClick();   // Finish Listening
            e.getSetLevelsActionButton().onClick();   // Apply
            expect (! e.getSetLevelsPanel().isVisible() && ! e.getSetLevelsScrim().isVisible(), "Apply closes both");
            expect (content.getComponentAt (knobCentre) != &e.getSetLevelsScrim(), "and the knob is reachable again at once");
            e.getKnob (ID::input).slider.setValue (4.0, juce::sendNotificationSync);
            expectWithinAbsoluteError (getParam (p, ID::input), 4.0f, 0.011f, "so the plugin is usable straight after Apply");

            // Escape and Cancel apply nothing and release the controls too.
            const float inputBefore = getParam (p, ID::input);
            e.getSetLevelsButton().onClick();
            e.getSetLevelsPanel().keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
            expect (! e.getSetLevelsScrim().isVisible(), "Escape releases the controls");
            expectEquals (getParam (p, ID::input), inputBefore, "and changes nothing");
            e.getSetLevelsButton().onClick();
            e.getSetLevelsCancelButton().onClick();
            expect (! e.getSetLevelsScrim().isVisible() && ! e.getSetLevelsPanel().isVisible(), "Cancel closes everything");
            expectEquals (getParam (p, ID::input), inputBefore, "Cancel changes nothing");
        }

        beginTest ("songsmithstudio#13: the panel is subtly see-through, and its text stays clear over the brightest faceplate");
        {
            const float panelAlpha = SongsmithAnalogCompressorEditor::getSetLevelsPanelFillAlpha();
            expect (panelAlpha < 1.0f && panelAlpha >= 0.8f, "subtle: between 80 % and fully solid (" + juce::String (panelAlpha) + ")");

            auto luminance = [] (juce::Colour c)
            {
                auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
                return 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
            };
            const auto fill = FET::UI::Colours::panel.brighter (0.12f);
            const auto text = FET::UI::Colours::text;
            // The worst case is the lightest thing that can be behind the panel: the Lindell faceplate's light grey, and white as an extreme.
            for (auto behind : { juce::Colour (0xffd0d0d0), juce::Colours::white })
            {
                // what really shows: the scrim dims what is behind, then the panel is laid over that
                const auto shown = behind.overlaidWith (juce::Colours::black.withAlpha (SongsmithAnalogCompressorEditor::getSetLevelsScrimDimAlpha())).overlaidWith (fill.withAlpha (panelAlpha));
                const float lt = luminance (text) + 0.05f, lb = luminance (shown) + 0.05f;
                const float contrast = lt > lb ? lt / lb : lb / lt;
                expectGreaterThan (contrast, 7.0f, "text contrast over " + behind.toDisplayString (false) + " is " + juce::String (contrast, 1) + ":1 (7:1 is the strong level)");
            }
        }

        beginTest ("Cancel leaves everything alone; silence is explained");
        {
            SongsmithAnalogCompressorProcessor p;
            p.prepareToPlay (fs, 512);
            SongsmithAnalogCompressorEditor e (p);
            set (p, ID::input, 3.0f);
            e.getSetLevelsButton().onClick();
            play (p, Signal ((size_t) (fs * 4.0), 0.0f));
            e.getSetLevelsActionButton().onClick();
            expect (e.getLevelsResult().status == FET::Calibration::Status::silent);
            expect (! e.getSetLevelsActionButton().isEnabled(), "nothing to apply");
            expect (e.getSetLevelsMessage().getText().containsIgnoreCase ("no sound"), e.getSetLevelsMessage().getText());
            e.getSetLevelsCancelButton().onClick();
            expect (e.getLevelsStage() == Stage::idle && ! e.getSetLevelsPanel().isVisible(), "Cancel closes the panel");
            expectWithinAbsoluteError (getParam (p, ID::input), 3.0f, 0.011f);
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
            e.getSetLevelsActionButton().onClick();   // Finish Listening
            expect (e.getLevelsStage() == Stage::idle, "no proposal from stale settings");
            expect (e.getSetLevelsPanel().isVisible() && e.getSetLevelsMessage().getText().contains ("changed"), "and the panel says why");
            expectWithinAbsoluteError (getParam (p, ID::output), -6.0f, 0.011f);

            e.getSetLevelsButton().onClick();   // try again
            play (p, sustained (fs, 4.0, 0.1f));
            e.getSetLevelsActionButton().onClick();
            expect (e.getLevelsStage() == Stage::proposal);
            set (p, ID::input, 20.0f);
            e.getSetLevelsActionButton().onClick();   // Apply
            expectWithinAbsoluteError (getParam (p, ID::input), 20.0f, 0.011f, "the newer change wins");
            expectWithinAbsoluteError (getParam (p, ID::output), -6.0f, 0.011f);
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
            expect (Wording::findBanned (e.getSetLevelsPanel().getTitle()).isEmpty());
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
