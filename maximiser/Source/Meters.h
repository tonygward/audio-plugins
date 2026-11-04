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
        g.setColour (MaxColours::panel);
        g.fillRoundedRectangle (r, 3.0f);

        const auto bar = r.reduced (3.0f);
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
            g.setColour (MaxColours::panelEdge);
            g.drawHorizontalLine (juce::roundToInt (y0), bar.getX(), bar.getRight());
            if (clipped)
            {
                g.setColour (MaxColours::clip);
                g.fillRect (r.withHeight (4.0f));
            }
        }
        else
        {
            auto toY = [&] (float db) { return bar.getY() + bar.getHeight() * juce::jlimit (0.0f, 1.0f, db / maxReductionDb); };
            g.setColour (MaxColours::reduction);
            g.fillRect (bar.withBottom (toY (ballistics.getLevel())));
            g.setColour (MaxColours::text.withAlpha (0.8f));
            g.fillRect (bar.withTop (toY (ballistics.getHold()) - 1.0f).withHeight (2.0f));
        }

        g.setColour (MaxColours::panelEdge);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

private:
    Kind kind;
    Max::MeterBallistics ballistics;
    bool clipped = false;
};
