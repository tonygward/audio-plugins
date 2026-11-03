#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EQLookAndFeel.h"

// Compact horizontal peak meter with peak hold and a latched clip indicator.
// The owner calls update() from its timer with the peak read from the audio thread
// (linear, already reset there). Click the clip box to clear it.
class OutputMeterDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float minDb = -60.0f, maxDb = 6.0f;

    OutputMeterDisplay() { setTooltip ("Output peak (hold 1.5 s). Click to reset the clip indicator."); }

    void update (float peakLinear, bool clippedNow, double nowSeconds, double elapsedSeconds)
    {
        const float db = juce::Decibels::gainToDecibels (peakLinear, -100.0f);

        // Fast attack, ~24 dB/s fall-off.
        level = juce::jmax (db, level - (float) (24.0 * elapsedSeconds));

        if (db >= holdDb)
        {
            holdDb = db;
            holdTime = nowSeconds;
        }
        else if (nowSeconds - holdTime > 1.5)
        {
            holdDb = juce::jmax (db, holdDb - (float) (24.0 * elapsedSeconds));
        }

        clipped = clipped || clippedNow;
        repaint();
    }

    void reset()
    {
        level = holdDb = -100.0f;
        clipped = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        clipped = false;
        holdDb = level;
        if (onReset) onReset();
        repaint();
    }

    std::function<void()> onReset;


    void paint (juce::Graphics& g) override
    {
        // Edges on whole physical pixels, so the meter stays sharp at 125 % / 150 % scaling.
        const float s = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        auto snap = [s] (juce::Rectangle<float> rect)
        {
            return juce::Rectangle<float>::leftTopRightBottom (std::round (rect.getX() * s) / s, std::round (rect.getY() * s) / s,
                                                              std::round (rect.getRight() * s) / s, std::round (rect.getBottom() * s) / s);
        };

        auto r = snap (getLocalBounds().toFloat().reduced (0.5f, 6.0f));
        auto clipBox = snap (r.removeFromRight (8.0f));
        r = snap (r.withTrimmedRight (2.0f));

        g.setColour (EQColours::panel);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        auto toX = [&] (float db) { return r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); };

        const float x = toX (level);
        if (x > r.getX())
        {
            const auto bar = r.withRight (x).reduced (1.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3ddbc4), r.getX(), 0.0f,
                                                     juce::Colour (0xfff2c94c), toX (0.0f), 0.0f, false));
            g.fillRect (bar.withRight (juce::jmin (bar.getRight(), toX (0.0f))));
            if (bar.getRight() > toX (0.0f))
            {
                g.setColour (juce::Colour (0xffff5c5c));
                g.fillRect (bar.withLeft (toX (0.0f)));
            }
        }

        if (holdDb > minDb)
        {
            g.setColour (holdDb >= 0.0f ? juce::Colour (0xffff5c5c) : EQColours::text);
            g.fillRect (juce::Rectangle<float> (toX (holdDb) - 1.0f, r.getY() + 1.0f, 2.0f, r.getHeight() - 2.0f));
        }

        g.setColour (EQColours::zeroLine);
        g.drawVerticalLine (juce::roundToInt (toX (0.0f)), r.getY(), r.getBottom());

        g.setColour (clipped ? juce::Colour (0xffff3b3b) : EQColours::panel);
        g.fillRoundedRectangle (clipBox, 2.0f);
        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (clipBox, 2.0f, 1.0f);
    }

    bool isClipped() const noexcept { return clipped; }
    float getHoldDb() const noexcept { return holdDb; }

private:
    float level = -100.0f, holdDb = -100.0f;
    double holdTime = 0.0;
    bool clipped = false;
};
