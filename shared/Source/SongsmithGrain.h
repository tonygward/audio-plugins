#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithColours.h"

// The fine brushed-metal grain used on the Songsmith Analog Compressor and Songsmith Maximiser faceplates
// (and, with Analog Warmth on, on the Songsmith Compressor's floating panel). Vector paths built once from a
// fixed seed, so the texture is identical every time, scales with the editor and is never regenerated when
// meters repaint. COMP-03 moved it here from the two plugins; the drawing is unchanged.
namespace Songsmith::UI
{
    struct BrushedGrain
    {
        juce::Path light, dark;

        // Streaks run left to right inside 'area'. The same seed and spacing as the faceplates: a row every
        // 2 px, each streak 24 to 144 px long with gaps of up to 70 px, 0.8 px thick.
        explicit BrushedGrain (juce::Rectangle<float> area)
        {
            juce::Random random (1176);
            const float right = area.getRight();
            for (float y = area.getY(); y < area.getBottom(); y += 2.0f)
                for (float x = area.getX(); x < right;)
                {
                    const float length = 24.0f + random.nextFloat() * 120.0f;
                    (random.nextBool() ? light : dark).addRectangle (x, y, juce::jmin (length, right - x), 0.8f);
                    x += length + random.nextFloat() * 70.0f;
                }
        }

        void draw (juce::Graphics& g) const
        {
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            g.fillPath (light);
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.fillPath (dark);
        }
    };
}
