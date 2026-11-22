#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// songsmithstudio#23: the colours of the Analog Compressor's four models (Black, Blue, Anniversary, Lindell), in one place, so the
// Songsmith Compressor's toolbox and the Analog Compressor's faceplate can never drift apart. These are the Analog Compressor's own
// values (they were inline in its editor); they are original designs, not copies of any hardware faceplate.
//
// "Look" numbers: 0 is Clean (the Compressor's own teal identity, no tint), 1 to 4 are Black, Blue, Anniversary, Lindell
// (the model index plus one). Each look has an accent (knob pointers, rings, lit buttons, outlines), and a tint with stripes for the
// panel's surface. The tint is drawn over the panel's own dark gradient and under the brushed grain.
namespace Songsmith::UI::ModelTheme
{
    enum class Stripes { none, oneBroad, pinstripes, fineLines };

    struct Theme
    {
        juce::Colour accent;
        juce::Colour tint;
        float tintStrength = 0.0f;   // 0 = no tint
        juce::Colour stripe;
        Stripes stripes = Stripes::none;
    };

    inline constexpr int numLooks = 5;

    inline const juce::Colour cleanAccent { 0xff3ddbc4 };   // the Songsmith Compressor's teal
    inline const juce::Colour blackAccent { 0xffe8a33d };   // the Analog Compressor's amber

    inline Theme forLook (int look)
    {
        switch (look)
        {
            case 1:  return { blackAccent, juce::Colour(), 0.0f, juce::Colour(), Stripes::none };   // Black is left exactly as the faceplate always looked
            case 2:  return { juce::Colour (0xff6fa8ee), juce::Colour (0xff2f6bc4), 0.34f, juce::Colour (0xff6fa8ee).withAlpha (0.85f), Stripes::oneBroad };
            case 3:  return { juce::Colour (0xfff1f4fa), juce::Colour (0xff1c3f94), 0.40f, juce::Colours::white.withAlpha (0.78f), Stripes::pinstripes };
            case 4:  return { juce::Colour (0xffc7ccd4), juce::Colour (0xff9aa1ab), 0.30f, juce::Colours::white.withAlpha (0.05f), Stripes::fineLines };
            default: return { cleanAccent, juce::Colour(), 0.0f, juce::Colour(), Stripes::none };
        }
    }

    // The look for a model index (0 to 3) with its Analog Warmth switched on, or Clean (-1).
    inline Theme forModel (int modelIndex) { return forLook (modelIndex < 0 ? 0 : juce::jlimit (0, 3, modelIndex) + 1); }

    // Draws the tint and the stripes over 'plate' (a rounded rectangle of the given corner radius).
    inline void paintSurface (juce::Graphics& g, juce::Rectangle<float> plate, const Theme& t, float cornerRadius)
    {
        if (t.tintStrength <= 0.0f)
            return;

        g.setGradientFill (juce::ColourGradient (t.tint.withAlpha (t.tintStrength * 1.15f), 0.0f, plate.getY(),
                                                 t.tint.withAlpha (t.tintStrength * 0.75f), 0.0f, plate.getBottom(), false));
        g.fillRoundedRectangle (plate, cornerRadius);

        const float inset = 24.0f;
        if (plate.getWidth() < 2.0f * inset + 8.0f)
            return;
        g.setColour (t.stripe);
        switch (t.stripes)
        {
            case Stripes::oneBroad:
                g.fillRect (plate.getX() + inset, plate.getY() + 5.0f, plate.getWidth() - 2.0f * inset, 4.0f);
                break;
            case Stripes::pinstripes:
                g.fillRect (plate.getX() + inset, plate.getY() + 5.0f, plate.getWidth() - 2.0f * inset, 1.5f);
                g.fillRect (plate.getX() + inset, plate.getY() + 9.0f, plate.getWidth() - 2.0f * inset, 1.5f);
                g.fillRect (plate.getX() + inset, plate.getBottom() - 11.0f, plate.getWidth() - 2.0f * inset, 1.5f);
                g.fillRect (plate.getX() + inset, plate.getBottom() - 7.0f, plate.getWidth() - 2.0f * inset, 1.5f);
                break;
            case Stripes::fineLines:
                for (float x = plate.getX() + 18.0f; x < plate.getRight() - 14.0f; x += 7.0f)
                    g.fillRect (x, plate.getY() + 14.0f, 1.0f, plate.getHeight() - 28.0f);
                break;
            case Stripes::none:
                break;
        }
    }
}
