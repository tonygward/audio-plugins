#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithColours.h"
#include "SongsmithOutputMeter.h"
#include "SongsmithTube.h"
#include "SongsmithType.h"

// SUI-23: the one right-hand meter column for every Songsmith plugin, laid out like the Songsmith
// Compressor's (the suite's reference): the shared Output meter at the far right edge and, for a
// plugin that turns the level down, a labelled Gain Reduction bar beside it with its number below.
// Equalisers use the Output meter alone (never a made-up Gain Reduction).
//
// A new plugin adds one MeterColumn to its editor, places it with boundsFor() and keeps its own
// controls left of contentRightEdge(). tests/ (see the SUI-23 suite test) check that every plugin in
// the repository does.
namespace Songsmith::UI
{
    // The Gain Reduction bar: fills downwards from the top, 0 to 24 dB, in the suite's tubular look
    // and the suite's reduction red. Instant rise, a steady fall and a held peak, as the Compressor's.
    class GainReductionBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr float maxReductionDb = 24.0f, fallDbPerSecond = 20.0f, holdSeconds = 1.5f;

        // reductionDb: the gain reduction as a positive dB amount, 0 when nothing is being turned down.
        void push (float reductionDb, double elapsedSeconds)
        {
            const float db = juce::jlimit (0.0f, maxReductionDb, reductionDb);
            level = juce::jmax (db, level - fallDbPerSecond * (float) elapsedSeconds);
            holdAge += elapsedSeconds;
            if (db >= hold || holdAge > (double) holdSeconds)
            {
                hold = juce::jmax (db, level);
                holdAge = 0.0;
            }
            repaint();
        }

        void reset() { level = hold = 0.0f; holdAge = 0.0; repaint(); }
        void mouseDown (const juce::MouseEvent&) override { reset(); }

        float getLevel() const noexcept { return level; }
        float getHold() const noexcept  { return hold; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const auto bar = Tube::paintHousing (g, r);

            g.setColour (Colours::textDim.withAlpha (0.35f));
            const float tickLength = juce::jmin (4.0f, bar.getWidth() * 0.25f);
            auto toY = [&] (float db) { return bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxReductionDb); };
            for (float db = 6.0f; db < maxReductionDb; db += 6.0f)
            {
                const int y = juce::roundToInt (toY (db));
                g.drawHorizontalLine (y, bar.getX(), bar.getX() + tickLength);
                g.drawHorizontalLine (y, bar.getRight() - tickLength, bar.getRight());
            }

            g.setColour (Tube::reductionRed);
            g.fillRect (bar.withBottom (toY (level)));
            g.setColour (Colours::text.withAlpha (0.45f));
            g.drawHorizontalLine (juce::roundToInt (bar.getY()), bar.getX(), bar.getRight());
            g.setColour (Colours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (hold) - 1.0f).withHeight (2.0f));

            Tube::paintFinish (g, r, bar);
        }

    private:
        float level = 0.0f, hold = 0.0f;
        double holdAge = 0.0;
    };

    class MeterColumn : public juce::Component
    {
    public:
        // Layout, in design units, taken from the Songsmith Compressor.
        static constexpr int margin = 16;           // from the editor's right edge
        static constexpr int grSlotWidth = 88;      // the Gain Reduction label, bar and number share this width
        static constexpr int grBarWidth = 26;
        static constexpr int grGap = 14;            // between the Gain Reduction slot and the Output meter
        static constexpr int contentGap = 12;       // between a plugin's own controls and the column

        static constexpr int widthFor (bool withGainReduction) noexcept
        {
            return OutputMeterDisplay::preferredWidth + (withGainReduction ? grGap + grSlotWidth : 0);
        }

        // Where to put the column: the right edge, from top to bottom.
        static juce::Rectangle<int> boundsFor (int designWidth, int top, int bottom, bool withGainReduction) noexcept
        {
            const int w = widthFor (withGainReduction);
            return { designWidth - margin - w, top, w, bottom - top };
        }

        // The plugin's own controls end here.
        static int contentRightEdge (int designWidth, bool withGainReduction) noexcept
        {
            return designWidth - margin - widthFor (withGainReduction) - contentGap;
        }

        explicit MeterColumn (bool withGainReduction) : hasGainReduction (withGainReduction)
        {
            output.setTitle ("Output level");
            addAndMakeVisible (output);

            if (hasGainReduction)
            {
                reduction.setTitle ("Gain Reduction");
                reduction.setTooltip ("Gain Reduction: how much the plugin is turning the level down right now, 0 to 24 dB. "
                                      "Nothing to do: it shows the effect working. Click to clear the held mark.");
                addAndMakeVisible (reduction);

                label.setText ("Gain Reduction", juce::dontSendNotification);
                label.setFont (juce::Font (font (Type::control, Weight::Medium)));
                label.setColour (juce::Label::textColourId, Colours::textDim);
                value.setText ("0.0 dB", juce::dontSendNotification);
                value.setFont (juce::Font (numbers (Type::control)));
                value.setColour (juce::Label::textColourId, Colours::text);
                for (auto* l : { &label, &value })
                {
                    l->setJustificationType (juce::Justification::centred);
                    l->setInterceptsMouseClicks (false, false);
                    addAndMakeVisible (*l);
                }
            }
        }

        bool hasReduction() const noexcept { return hasGainReduction; }
        OutputMeterDisplay& getOutput() noexcept { return output; }
        GainReductionBar& getReduction() noexcept { return reduction; }
        juce::Label& getReductionLabel() noexcept { return label; }
        juce::Label& getReductionValue() noexcept { return value; }

        // Called from the editor's timer. Peaks are linear, already read and cleared from the audio thread.
        // reductionDb is ignored when the column has no Gain Reduction bar.
        void update (float peakLeft, float peakRight, int channelCount, bool clipLatched, float reductionDb,
                     double nowSeconds, double elapsedSeconds)
        {
            output.update (peakLeft, peakRight, channelCount, clipLatched, nowSeconds, elapsedSeconds);
            if (hasGainReduction)
            {
                reduction.push (reductionDb, elapsedSeconds);
                value.setText (juce::String (reduction.getLevel(), 1) + " dB", juce::dontSendNotification);
            }
        }

        void resized() override
        {
            const int h = getHeight();
            output.setBounds (getWidth() - OutputMeterDisplay::preferredWidth, 0, OutputMeterDisplay::preferredWidth, h);
            if (! hasGainReduction)
                return;

            // The bar starts level with the Output meter's bars and ends level with their bottom.
            const int barsTop = 46, barsBottom = h - (int) OutputMeterDisplay::bottomRow;
            label.setBounds (0, 4, grSlotWidth, 36);
            reduction.setBounds ((grSlotWidth - grBarWidth) / 2, barsTop, grBarWidth, barsBottom - barsTop);
            value.setBounds (0, barsBottom + 2, grSlotWidth, (int) OutputMeterDisplay::bottomRow - 4);
        }

    private:
        bool hasGainReduction;
        OutputMeterDisplay output;
        GainReductionBar reduction;
        juce::Label label, value;
    };
}
