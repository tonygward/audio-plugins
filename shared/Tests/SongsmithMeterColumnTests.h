#pragma once

// SUI-23: shared checks for a plugin's far-right meter column, compiled into each test executable that has one:
//   static SongsmithMeterColumnTests<MyProcessor, MyEditor> sharedMeterColumnTests { "MyPlugin", designWidth, contentHeight };
// The processor must expose `apvts` (with a "bypass" parameter), `outputMeter` and `takeGainReductionDb()`;
// the editor must expose `getMeterColumn()`.

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "../Source/SongsmithMeterColumn.h"

template <class Processor, class Editor>
class SongsmithMeterColumnTests final : public juce::UnitTest
{
public:
    SongsmithMeterColumnTests (const juce::String& plugin, int designWidthIn, int contentHeightIn)
        : juce::UnitTest ("Songsmith meter column (SUI-23)", plugin), designWidth (designWidthIn), contentHeight (contentHeightIn) {}

    void runTest() override
    {
        using namespace Songsmith::UI;

        beginTest ("The Output meter is at the far right, with Gain Reduction beside it, clear of every control");
        {
            Processor p;
            std::unique_ptr<juce::AudioProcessorEditor> owner (p.createEditor());
            auto* ed = dynamic_cast<Editor*> (owner.get());
            expect (ed != nullptr);
            if (ed == nullptr)
                return;

            auto& col = ed->getMeterColumn();
            expect (col.hasReduction(), "a plugin that turns the level down shows Gain Reduction");
            const auto b = col.getBounds();
            expectEquals (b.getRight(), designWidth - MeterColumn::margin, "the column ends at the suite's right margin");
            expect (b.getBottom() <= contentHeight, "inside the controls area, above the footer strip");
            expectEquals (b.getWidth(), MeterColumn::widthFor (true));

            const auto out = col.getOutput().getBounds();
            expectEquals (out.getRight(), col.getWidth(), "the Output meter is the rightmost thing");
            expectEquals (out.getWidth(), (int) OutputMeterDisplay::preferredWidth);
            const auto gr = col.getReduction().getBounds(), grLabel = col.getReductionLabel().getBounds(), grValue = col.getReductionValue().getBounds();
            expect (gr.getRight() < out.getX(), "Gain Reduction sits beside the Output meter, on its left");
            expect (grLabel.getBottom() <= gr.getY() && gr.getBottom() <= grValue.getY(), "label above the bar, number below it");
            expect (col.getReductionLabel().getText() == "Gain Reduction", "labelled in plain words");
            expect (col.getOutput().getTitle().isNotEmpty() && col.getReduction().getTitle().isNotEmpty(), "both have an accessible name");
            expect (col.getOutput().getTooltip().isNotEmpty() && col.getReduction().getTooltip().isNotEmpty(), "both have a tooltip");

            if (auto* parent = col.getParentComponent())
                for (auto* sibling : parent->getChildren())
                    if (sibling != &col && sibling->isVisible() && sibling->getWidth() > 0)
                        expect (! sibling->getBounds().intersects (b), "no control overlaps the meter column");
        }

        beginTest ("The column shows what it is fed, and clears");
        {
            MeterColumn col (true);
            col.setSize (MeterColumn::widthFor (true), 300);
            col.update (0.5f, 0.25f, 2, false, 6.0f, 1.0, 0.03);
            expectWithinAbsoluteError (col.getReduction().getLevel(), 6.0f, 1.0e-4f);
            expect (col.getReductionValue().getText() == "6.0 dB", "the number matches the bar");
            col.update (0.0f, 0.0f, 2, false, 0.0f, 2.0, 5.0);
            expectWithinAbsoluteError (col.getReduction().getLevel(), 0.0f, 1.0e-4f);
            expect (col.getReductionValue().getText() == "0.0 dB", "no reduction reads 0.0 dB, nothing made up");
            col.update (0.0f, 0.0f, 2, false, 99.0f, 3.0, 0.03);
            expectWithinAbsoluteError (col.getReduction().getLevel(), 24.0f, 1.0e-4f, "the bar stops at 24 dB");
        }

        beginTest ("The audio thread publishes each channel's real output peak, and no reduction when bypassed");
        {
            Processor p;
            p.prepareToPlay (48000.0, 512);
            auto* bypass = p.apvts.getParameter ("bypass");
            expect (bypass != nullptr);
            if (bypass != nullptr)
                bypass->setValueNotifyingHost (1.0f);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 40; ++block)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = std::sin (0.05f * (float) (block * 512 + i));
                    buffer.setSample (0, i, 0.5f * s);
                    buffer.setSample (1, i, 0.25f * s);
                }
                p.processBlock (buffer, midi);
                if (block == 19)   // let the Bypass fade finish, then measure only the settled sound
                {
                    p.outputMeter.readAndResetChannel (0);
                    p.outputMeter.readAndResetChannel (1);
                }
            }
            const float left = p.outputMeter.readAndResetChannel (0), right = p.outputMeter.readAndResetChannel (1);
            expectWithinAbsoluteError (left, 0.5f, 0.02f, "left peak");
            expectWithinAbsoluteError (right, 0.25f, 0.02f, "right peak is its own, not the left's");
            // The engine may keep following the sound while bypassed, but the column must not show reduction that is not applied.
            std::unique_ptr<juce::AudioProcessorEditor> owner (p.createEditor());
            if (auto* ed = dynamic_cast<Editor*> (owner.get()))
            {
                ed->advanceMeter (0.03f);
                expectEquals (ed->getMeterColumn().getReduction().getLevel(), 0.0f, "bypassed means the column shows no reduction");
            }
            else
                expect (false, "the editor opens");
        }
    }

private:
    int designWidth, contentHeight;
};
