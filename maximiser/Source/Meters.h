#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MaxLookAndFeel.h"
#include "EditorLogic.h"

// Vertical meter, drawn as vectors. Level meters fill upwards (-48 to +3 dBFS); the gain-reduction
// meter fills downwards from 0 (0 to 24 dB), like the L1's Attenuation meter. The owner feeds it
// readings from its timer; the ballistics live here, in the UI, never in the audio thread.
class VerticalMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Kind { level, reduction };

    explicit VerticalMeter (Kind k) : kind (k), ballistics (k == Kind::level ? minLevelDb : 0.0f) {}

    static constexpr float minLevelDb = -48.0f, maxLevelDb = 3.0f, maxReductionDb = 24.0f;

    // Level: dBFS. Reduction: the gain reduction as a positive dB amount.
    void push (float db, double elapsedSeconds)
    {
        // Reduction runs the same ballistics on its own scale: instant rise, steady fall.
        ballistics.update (kind == Kind::level ? db : juce::jmin (db, maxReductionDb), elapsedSeconds);
        clipped = clipped || (kind == Kind::level && db > 0.0f);
        repaint();
    }

    void reset() { ballistics.reset(); clipped = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { reset(); }

    const Max::MeterBallistics& getBallistics() const noexcept { return ballistics; }
    bool isClipped() const noexcept { return clipped; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        // Dark recessed housing: a deep well with a shadow along the top and a lit lower lip.
        g.setColour (MaxColours::track.darker (0.35f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
        g.fillRoundedRectangle (r, 3.0f);

        const auto bar = r.reduced (3.0f);

        // Scale ticks every 6 dB along both edges, drawn under the fill so the bar stays clean.
        {
            g.setColour (MaxColours::label.withAlpha (0.35f));
            const float tickLength = juce::jmin (4.0f, bar.getWidth() * 0.25f);
            auto tick = [&] (float y)
            {
                g.drawHorizontalLine (juce::roundToInt (y), bar.getX(), bar.getX() + tickLength);
                g.drawHorizontalLine (juce::roundToInt (y), bar.getRight() - tickLength, bar.getRight());
            };
            if (kind == Kind::level)
                for (float db = minLevelDb; db < maxLevelDb; db += 6.0f)
                    tick (bar.getBottom() - bar.getHeight() * (db - minLevelDb) / (maxLevelDb - minLevelDb));
            else
                for (float db = 6.0f; db < maxReductionDb; db += 6.0f)
                    tick (bar.getY() + bar.getHeight() * db / maxReductionDb);
        }

        if (kind == Kind::level)
        {
            auto toY = [&] (float db) { return bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, (db - minLevelDb) / (maxLevelDb - minLevelDb)); };
            const float y = toY (ballistics.getLevel()), y0 = toY (0.0f);
            if (y < bar.getBottom())
            {
                g.setGradientFill (juce::ColourGradient (MaxColours::meterLow, 0.0f, bar.getBottom(), MaxColours::accent, 0.0f, y0, false));
                g.fillRect (bar.withTop (juce::jmax (y, y0)));
                if (y < y0)
                {
                    g.setColour (MaxColours::clip);
                    g.fillRect (bar.withTop (y).withBottom (y0));
                }
            }
            g.setColour (MaxColours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (ballistics.getHold()) - 1.0f).withHeight (2.0f));
            // The 0 dBFS line, a little brighter than the ticks.
            g.setColour (MaxColours::text.withAlpha (0.45f));
            g.drawHorizontalLine (juce::roundToInt (y0), bar.getX(), bar.getRight());
            if (clipped)
            {
                g.setColour (MaxColours::clip);
                g.fillRect (r.withHeight (5.0f));
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillRect (r.withHeight (1.5f).reduced (2.0f, 0.0f));
            }
        }
        else
        {
            auto toY = [&] (float db) { return bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxReductionDb); };
            g.setColour (MaxColours::reduction);
            g.fillRect (bar.withBottom (toY (ballistics.getLevel())));
            // The zero line at the top, where reduction starts, matches the level meters' 0 dBFS line.
            g.setColour (MaxColours::text.withAlpha (0.45f));
            g.drawHorizontalLine (juce::roundToInt (bar.getY()), bar.getX(), bar.getRight());
            g.setColour (MaxColours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (ballistics.getHold()) - 1.0f).withHeight (2.0f));
        }

        // A soft sheen down the left of the active fill area, then crisp borders.
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), bar.getX(), 0.0f,
                                                 juce::Colours::transparentWhite, bar.getX() + bar.getWidth() * 0.5f, 0.0f, false));
        g.fillRect (bar);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (1.0f), 2.0f, 1.0f);
    }

private:
    Kind kind;
    Max::MeterBallistics ballistics;
    bool clipped = false;
};
