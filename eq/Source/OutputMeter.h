#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EQLookAndFeel.h"

// Tall vertical output peak meter, on the right of the graph (task 27). From the top:
// clip indicator, the bar (fills bottom to top, -60 to +6 dBFS on a linear dB scale) with
// its dB scale on the right, and the held peak as a number in the frequency-axis row.
// The owner calls update() from its timer with the peak read from the audio thread
// (linear, already reset there). Click anywhere on it to clear the clip indicator.
// One mono bar: the processor publishes a single peak across all channels.
class OutputMeterDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float minDb = -60.0f, maxDb = 6.0f;

    // Layout, in design units.
    static constexpr int   preferredWidth = 36;
    static constexpr float barWidth = 10.0f, clipHeight = 8.0f, clipGap = 3.0f;
    static constexpr float bottomRow = 20.0f;   // same as the graph's frequency-label row

    // dB marks drawn on the scale.
    static constexpr float scaleMarks[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };

    OutputMeterDisplay()
    {
        setTooltip ("Output peak (number: held peak, 1.5 s). The red box at the top lights when the output reaches "
                    "0 dBFS; click the meter to reset it.");
    }

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

    // Geometry (local coordinates), public for tests.
    juce::Rectangle<float> getClipBox() const
    {
        return { 1.0f, 1.0f, barWidth, clipHeight };
    }

    juce::Rectangle<float> getBarArea() const
    {
        const float top = 1.0f + clipHeight + clipGap;
        return { 1.0f, top, barWidth, juce::jmax (1.0f, (float) getHeight() - bottomRow - top) };
    }

    // 0 at -60 dB (bottom), 1 at +6 dB (top), clamped.
    static float dbToProportion (float db) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    }

    float levelToY (float db) const
    {
        const auto bar = getBarArea();
        return bar.getBottom() - bar.getHeight() * dbToProportion (db);
    }

    // The text under the meter: the held peak, or a dash when there's no signal.
    juce::String getPeakText() const
    {
        if (holdDb <= minDb)
            return juce::String::charToString (0x2013);   // en dash
        return (holdDb > 0.05f ? "+" : "") + juce::String (holdDb, 1);
    }

    void paint (juce::Graphics& g) override
    {
        // Edges on whole physical pixels, so the meter stays sharp at 125 % / 150 % / 200 %.
        const float s = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        auto snapV = [s] (float v) { return std::round (v * s) / s; };
        auto snap = [snapV] (juce::Rectangle<float> rect)
        {
            return juce::Rectangle<float>::leftTopRightBottom (snapV (rect.getX()), snapV (rect.getY()),
                                                              snapV (rect.getRight()), snapV (rect.getBottom()));
        };

        const auto r = snap (getBarArea());
        const auto clipBox = snap (getClipBox());
        auto toY = [&] (float db) { return snapV (r.getBottom() - r.getHeight() * dbToProportion (db)); };
        const float zeroY = toY (0.0f);

        g.setColour (EQColours::panel);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        // Bar: the existing teal-to-yellow gradient up to 0 dBFS, red above.
        const float y = toY (level);
        if (y < r.getBottom())
        {
            const auto bar = r.withTop (y).reduced (1.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3ddbc4), 0.0f, r.getBottom(),
                                                     juce::Colour (0xfff2c94c), 0.0f, zeroY, false));
            g.fillRect (bar.withTop (juce::jmax (bar.getY(), zeroY)));
            if (bar.getY() < zeroY)
            {
                g.setColour (juce::Colour (0xffff5c5c));
                g.fillRect (bar.withBottom (zeroY));
            }
        }

        if (holdDb > minDb)
        {
            g.setColour (holdDb >= 0.0f ? juce::Colour (0xffff5c5c) : EQColours::text);
            g.fillRect (juce::Rectangle<float> (r.getX() + 1.0f, toY (holdDb) - 1.0f, r.getWidth() - 2.0f, 2.0f));
        }

        // Scale: ticks and labels to the right of the bar. Labels that would overlap are dropped.
        const auto font = juce::Font (EQColours::numbers (EQColours::TextSize::gridLabel));
        g.setFont (font);
        const float labelX = r.getRight() + 4.0f, labelW = (float) getWidth() - labelX;
        float lastBottom = -1000.0f;
        for (auto db : scaleMarks)
        {
            const float ty = toY (db);
            g.setColour (db == 0.0f ? EQColours::zeroLine.withMultipliedAlpha (2.0f) : EQColours::gridMajor);
            g.fillRect (juce::Rectangle<float> (r.getRight(), ty - 0.5f / s, 3.0f, juce::jmax (1.0f / s, 1.0f)));

            const auto box = juce::Rectangle<float> (labelX, ty - 7.0f, labelW, 14.0f);
            if (box.getY() >= lastBottom)
            {
                g.setColour (db == 0.0f ? EQColours::text : EQColours::label);
                g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db), box,
                            juce::Justification::centredLeft, false);
                lastBottom = box.getBottom();
            }
        }

        g.setColour (EQColours::zeroLine);
        g.fillRect (juce::Rectangle<float> (r.getX(), zeroY, r.getWidth(), 1.0f / s));

        // Clip indicator.
        g.setColour (clipped ? juce::Colour (0xffff3b3b) : EQColours::panel);
        g.fillRoundedRectangle (clipBox, 2.0f);
        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (clipBox, 2.0f, 1.0f);

        // Held peak, in the frequency-axis row.
        g.setColour (clipped || holdDb >= 0.0f ? juce::Colour (0xffff5c5c) : EQColours::label);
        g.drawText (getPeakText(), juce::Rectangle<float> (0.0f, (float) getHeight() - bottomRow + 2.0f, (float) getWidth(), bottomRow - 4.0f),
                    juce::Justification::centred, false);
    }

    bool isClipped() const noexcept { return clipped; }
    float getHoldDb() const noexcept { return holdDb; }
    float getLevelDb() const noexcept { return level; }

private:
    float level = -100.0f, holdDb = -100.0f;
    double holdTime = 0.0;
    bool clipped = false;
};
