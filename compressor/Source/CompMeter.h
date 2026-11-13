#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CompLookAndFeel.h"
#include "CompParams.h"

// Vertical meter, drawn as vectors. Level meters fill upwards (-48 to +3 dBFS, sample peak); the
// gain-reduction meter fills downwards from 0 (0 to 24 dB). The owner feeds it from its timer; the
// ballistics live here, never in the audio thread. A click clears the hold and the clip mark.
class CompMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { level, reduction };

    explicit CompMeter (Kind k) : kind (k), ballistics (k == Kind::level ? minLevelDb : 0.0f) {}

    static constexpr float minLevelDb = -48.0f, maxLevelDb = 3.0f, maxReductionDb = 24.0f;

    // Level: dBFS. Reduction: the gain reduction as a positive dB amount.
    void push (float db, double elapsedSeconds)
    {
        ballistics.update (kind == Kind::level ? db : juce::jmin (db, maxReductionDb), elapsedSeconds);
        // A sample at or above 0 dBFS marks the meter, with the same boundary as the Songsmith EQ.
        clipped = clipped || (kind == Kind::level && db >= 0.0f);
        repaint();
    }

    void reset() { ballistics.reset(); clipped = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { reset(); }

    const Comp::MeterBallistics& getBallistics() const noexcept { return ballistics; }
    bool isClipped() const noexcept { return clipped; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (CompColours::track);
        g.fillRoundedRectangle (r, 3.0f);
        const auto bar = r.reduced (3.0f);

        g.setColour (CompColours::label.withAlpha (0.35f));
        const float tickLength = juce::jmin (4.0f, bar.getWidth() * 0.25f);
        auto tick = [&] (float y)
        {
            g.drawHorizontalLine (juce::roundToInt (y), bar.getX(), bar.getX() + tickLength);
            g.drawHorizontalLine (juce::roundToInt (y), bar.getRight() - tickLength, bar.getRight());
        };

        if (kind == Kind::level)
        {
            for (float db = minLevelDb; db < maxLevelDb; db += 6.0f)
                tick (bar.getBottom() - bar.getHeight() * (db - minLevelDb) / (maxLevelDb - minLevelDb));
            auto toY = [&] (float db) { return bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minLevelDb) / (maxLevelDb - minLevelDb)); };
            const float y = toY (ballistics.getLevel()), y0 = toY (0.0f);
            if (y < bar.getBottom())
            {
                g.setGradientFill (juce::ColourGradient (CompColours::accent, 0.0f, bar.getBottom(), CompColours::meterHigh, 0.0f, y0, false));
                g.fillRect (bar.withTop (juce::jmax (y, y0)));
                if (y < y0)
                {
                    g.setColour (CompColours::clip);
                    g.fillRect (bar.withTop (y).withBottom (y0));
                }
            }
            g.setColour (CompColours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (ballistics.getHold()) - 1.0f).withHeight (2.0f));
            g.setColour (CompColours::text.withAlpha (0.45f));
            g.drawHorizontalLine (juce::roundToInt (y0), bar.getX(), bar.getRight());
            if (clipped)
            {
                g.setColour (CompColours::clip);
                g.fillRect (r.withHeight (5.0f));
            }
        }
        else
        {
            for (float db = 6.0f; db < maxReductionDb; db += 6.0f)
                tick (bar.getY() + bar.getHeight() * db / maxReductionDb);
            auto toY = [&] (float db) { return bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxReductionDb); };
            g.setColour (CompColours::reduction);
            g.fillRect (bar.withBottom (toY (ballistics.getLevel())));
            g.setColour (CompColours::text.withAlpha (0.45f));
            g.drawHorizontalLine (juce::roundToInt (bar.getY()), bar.getX(), bar.getRight());
            g.setColour (CompColours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (ballistics.getHold()) - 1.0f).withHeight (2.0f));
        }

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

private:
    Kind kind;
    Comp::MeterBallistics ballistics;
    bool clipped = false;
};
