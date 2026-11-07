#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EQLookAndFeel.h"

// Tall vertical output peak meter, on the right of the graph (task 27, EQ-39). From the top:
// the CLIP warning, the channel names, the bars (fill bottom to top, -60 to +6 dBFS on a linear
// dB scale) with the dB scale on the right, and the held peak as a number in the frequency-axis
// row. One bar for a mono output, two (Left, Right) for stereo, each showing its own peak.
// The owner calls update() from its timer with the peaks read from the audio thread (linear,
// already reset there) and the audio thread's latched clip state. The CLIP warning is text as
// well as colour. It is sample-peak clipping (a sample at or above 0 dBFS), not a true-peak
// measurement, and it clears only when the output has gone quiet, which the audio thread decides.
class OutputMeterDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float minDb = -60.0f, maxDb = 6.0f;

    // Layout, in design units.
    static constexpr int   preferredWidth = 48;
    static constexpr float barsWidth = 20.0f;               // all the bars together, mono or stereo
    static constexpr float barGap = 2.0f;                   // between the Left and Right bars
    static constexpr float clipWidth = 34.0f, clipHeight = 14.0f, clipGap = 3.0f;
    static constexpr float namesHeight = 13.0f;             // the L and R above the bars
    static constexpr float bottomRow = 20.0f;               // same as the graph's frequency-label row

    // dB marks drawn on the scale.
    static constexpr float scaleMarks[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };

    OutputMeterDisplay()
    {
        setTooltip ("Output peak for each channel (number: held peak, 1.5 s). The red warning lights when a sample reaches "
                    "0 dBFS and stays lit until the sound stops. Nothing needs doing unless you can hear distortion.");
    }

    // peakLeft / peakRight: linear peaks since the last call. channelCount: 1 for mono (only the
    // left value is used) or 2. clipLatched: the audio thread's warning state.
    void update (float peakLeft, float peakRight, int channelCount, bool clipLatched, double nowSeconds, double elapsedSeconds)
    {
        const int n = juce::jlimit (1, 2, channelCount);
        if (n != numChannels)
        {
            numChannels = n;
            reset();   // a new layout starts clean
        }

        const float peaks[2] = { peakLeft, peakRight };
        for (int c = 0; c < n; ++c)
        {
            const float db = juce::Decibels::gainToDecibels (peaks[c], -100.0f);

            // Fast attack, ~24 dB/s fall-off.
            level[c] = juce::jmax (db, level[c] - (float) (24.0 * elapsedSeconds));

            if (db >= holdDb[c])
            {
                holdDb[c] = db;
                holdTime[c] = nowSeconds;
            }
            else if (nowSeconds - holdTime[c] > 1.5)
            {
                holdDb[c] = juce::jmax (db, holdDb[c] - (float) (24.0 * elapsedSeconds));
            }
        }

        clipped = clipLatched;
        repaint();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            level[c] = holdDb[c] = -100.0f;
            holdTime[c] = 0.0;
        }
        clipped = false;
        repaint();
    }

    // A click clears the held peak numbers. The CLIP warning is not cleared by hand: it clears
    // when the sound stops.
    void mouseDown (const juce::MouseEvent&) override
    {
        for (int c = 0; c < 2; ++c)
            holdDb[c] = level[c];
        repaint();
    }

    // Geometry (local coordinates), public for tests.
    juce::Rectangle<float> getClipBox() const
    {
        return { 1.0f, 1.0f, clipWidth, clipHeight };
    }

    // The area of all the bars together.
    juce::Rectangle<float> getBarArea() const
    {
        const float top = 1.0f + clipHeight + clipGap + namesHeight;
        return { 1.0f, top, barsWidth, juce::jmax (1.0f, (float) getHeight() - bottomRow - top) };
    }

    // One channel's bar: the whole area for mono, the left or right half for stereo.
    juce::Rectangle<float> getChannelBar (int channel) const
    {
        const auto all = getBarArea();
        if (numChannels < 2)
            return all;
        const float w = (all.getWidth() - barGap) * 0.5f;
        return channel == 0 ? all.withWidth (w) : all.withTrimmedLeft (w + barGap);
    }

    int getNumChannels() const noexcept { return numChannels; }

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

    // The text under the meter: the highest held peak, or a dash when there's no signal.
    juce::String getPeakText() const
    {
        const float hold = getHoldDb();
        if (hold <= minDb)
            return juce::String::charToString (0x2013);   // en dash
        return (hold > 0.05f ? "+" : "") + juce::String (hold, 1);
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
        const auto font = juce::Font (EQColours::numbers (EQColours::Type::caption));

        for (int c = 0; c < numChannels; ++c)
        {
            const auto bar = snap (getChannelBar (c));
            g.setColour (EQColours::panel);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (EQColours::panelEdge);
            g.drawRoundedRectangle (bar, 2.0f, 1.0f);

            // Bar: the existing teal-to-yellow gradient up to 0 dBFS, red above.
            const float y = toY (level[c]);
            if (y < bar.getBottom())
            {
                const auto fill = bar.withTop (y).reduced (1.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3ddbc4), 0.0f, bar.getBottom(),
                                                         juce::Colour (0xfff2c94c), 0.0f, zeroY, false));
                g.fillRect (fill.withTop (juce::jmax (fill.getY(), zeroY)));
                if (fill.getY() < zeroY)
                {
                    g.setColour (juce::Colour (0xffff5c5c));
                    g.fillRect (fill.withBottom (zeroY));
                }
            }

            if (holdDb[c] > minDb)
            {
                g.setColour (holdDb[c] >= 0.0f ? juce::Colour (0xffff5c5c) : EQColours::text);
                g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, toY (holdDb[c]) - 1.0f, bar.getWidth() - 2.0f, 2.0f));
            }

            if (numChannels == 2)   // Left and Right, as on a mixer strip
            {
                g.setColour (EQColours::label);
                g.setFont (font);
                g.drawText (c == 0 ? "L" : "R", juce::Rectangle<float> (bar.getX(), bar.getY() - namesHeight, bar.getWidth(), namesHeight),
                            juce::Justification::centred, false);
            }
        }

        // Scale: ticks and labels to the right of the bars. Labels that would overlap are dropped.
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

        // CLIP warning: the word, so it doesn't rely on colour alone.
        g.setColour (clipped ? juce::Colour (0xffd93030) : EQColours::panel);
        g.fillRoundedRectangle (clipBox, 2.0f);
        g.setColour (clipped ? juce::Colour (0xffff7a7a) : EQColours::panelEdge);
        g.drawRoundedRectangle (clipBox, 2.0f, 1.0f);
        if (clipped)
        {
            g.setColour (juce::Colours::white);
            g.setFont (font);
            g.drawText ("CLIP", clipBox, juce::Justification::centred, false);
        }

        // Held peak, in the frequency-axis row.
        g.setColour (clipped || getHoldDb() >= 0.0f ? juce::Colour (0xffff5c5c) : EQColours::label);
        g.drawText (getPeakText(), juce::Rectangle<float> (0.0f, (float) getHeight() - bottomRow + 2.0f, (float) getWidth(), bottomRow - 4.0f),
                    juce::Justification::centred, false);
    }

    bool isClipped() const noexcept { return clipped; }
    float getHoldDb() const noexcept { return numChannels > 1 ? juce::jmax (holdDb[0], holdDb[1]) : holdDb[0]; }
    float getLevelDb (int channel = 0) const noexcept { return level[juce::jlimit (0, 1, channel)]; }
    float getChannelHoldDb (int channel) const noexcept { return holdDb[juce::jlimit (0, 1, channel)]; }

private:
    float level[2] = { -100.0f, -100.0f }, holdDb[2] = { -100.0f, -100.0f };
    double holdTime[2] = { 0.0, 0.0 };
    int numChannels = 2;
    bool clipped = false;
};
