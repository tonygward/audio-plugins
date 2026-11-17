#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithColours.h"

// The suite's tubular meter look (SUI-22), taken from the Maximiser's meters, which are the visual reference: a dark
// recessed housing with a shadow along the top and a lit lower lip, a soft sheen down the left of the lit area and crisp
// borders. Every level and gain reduction meter in every plugin draws its housing and finish with these two functions, so
// the look lives in one place. They only paint; the meters keep their own scales, ballistics and clip behaviour.
namespace Songsmith::UI::Tube
{
    // The colour of a gain reduction meter's fill, in every plugin. Red, because it shows level being taken away.
    inline const juce::Colour reductionRed { 0xffff6b6b };

    // The corner radius of a tube, and the one-pixel inset between its housing and its lit area.
    inline constexpr float cornerRadius = 3.0f, wallThickness = 3.0f;

    // Paints the housing into 'r' (the whole meter). Returns the area the scale and the fill use.
    inline juce::Rectangle<float> paintHousing (juce::Graphics& g, juce::Rectangle<float> r, float corner = cornerRadius, float wall = wallThickness)
    {
        g.setColour (Colours::recess.darker (0.35f));
        g.fillRoundedRectangle (r, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, r.getY() + 10.0f, false));
        g.fillRoundedRectangle (r, corner);
        return r.reduced (wall);
    }

    // Paints the sheen over the lit area 'bar' and the borders around the housing 'r'. Call it after the fill.
    inline void paintFinish (juce::Graphics& g, juce::Rectangle<float> r, juce::Rectangle<float> bar, float corner = cornerRadius)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), bar.getX(), 0.0f,
                                                 juce::Colours::transparentWhite, bar.getX() + bar.getWidth() * 0.5f, 0.0f, false));
        g.fillRect (bar);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, corner, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (1.0f), corner - 1.0f, 1.0f);
    }
}
