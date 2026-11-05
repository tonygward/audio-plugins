#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "EQParams.h"

// Small vector pictures of each filter type's frequency response, drawn from low to high
// frequency (EQ-33). They are decorative: the type name always stays visible beside them.
namespace EQ::FilterShapes
{
    // Response height for a filter type at x (0 = lowest frequency, 1 = highest), 0 = low, 1 = high.
    inline float shape (FilterType t, float x)
    {
        const auto smooth = [] (float a, float b, float v)
        {
            const float k = juce::jlimit (0.0f, 1.0f, (v - a) / (b - a));
            return k * k * (3.0f - 2.0f * k);
        };
        const auto bump = [] (float centre, float width, float v)
        {
            const float d = (v - centre) / width;
            return std::exp (-d * d);
        };

        switch (t)
        {
            case FilterType::Bell:      return 0.35f + 0.5f * bump (0.5f, 0.17f, x);
            case FilterType::LowShelf:  return 0.3f + 0.45f * (1.0f - smooth (0.25f, 0.65f, x));
            case FilterType::HighShelf: return 0.3f + 0.45f * smooth (0.35f, 0.75f, x);
            case FilterType::LowCut:    return 0.1f + 0.65f * smooth (0.05f, 0.6f, x);
            case FilterType::HighCut:   return 0.1f + 0.65f * (1.0f - smooth (0.4f, 0.95f, x));
            case FilterType::Notch:     return 0.75f - 0.65f * bump (0.5f, 0.09f, x);
        }
        return 0.5f;
    }

    // The response curve scaled into area (y grows downwards on screen).
    inline juce::Path curve (FilterType t, juce::Rectangle<float> area)
    {
        juce::Path p;
        constexpr int steps = 48;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const juce::Point<float> pt (area.getX() + x * area.getWidth(),
                                         area.getBottom() - shape (t, x) * area.getHeight());
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        return p;
    }

    constexpr float strokeWidth = 1.6f;

    inline void draw (juce::Graphics& g, FilterType t, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        const auto area = bounds.reduced (strokeWidth);
        g.setColour (colour.withMultipliedAlpha (0.3f));   // the flat 0 dB reference
        g.drawHorizontalLine ((int) std::round (area.getBottom() - 0.35f * area.getHeight()), area.getX(), area.getRight());
        g.setColour (colour);
        g.strokePath (curve (t, area), juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The icon as a Drawable, for popup menu items. Always the same size so the curves line up.
    inline std::unique_ptr<juce::Drawable> makeDrawable (FilterType t, juce::Colour colour, float width, float height)
    {
        struct Icon final : juce::Drawable
        {
            Icon (FilterType ty, juce::Colour c, juce::Rectangle<float> r) : type (ty), colour (c), box (r) {}
            void paint (juce::Graphics& g) const override { EQ::FilterShapes::draw (g, type, box, colour); }
            std::unique_ptr<Drawable> createCopy() const override { return std::make_unique<Icon> (type, colour, box); }
            juce::Rectangle<float> getDrawableBoundsUntransformed() const override { return box; }
            bool hitTest (juce::Point<float>) const override { return false; }
            juce::Path getOutlineAsPath() const override { return {}; }
            FilterType type; juce::Colour colour; juce::Rectangle<float> box;
        };
        return std::make_unique<Icon> (t, colour, juce::Rectangle<float> (0.0f, 0.0f, width, height));
    }

    constexpr int iconWidth = 24, iconHeight = 14;
    constexpr int iconInset = 6, textIndent = iconInset + iconWidth + 4;   // inside the type box
}

// The icon shown inside the band panel's type box, beside the selected type's name.
class FilterShapeIcon final : public juce::Component
{
public:
    FilterShapeIcon()
    {
        setInterceptsMouseClicks (false, false);
        setAccessible (false);   // decorative: the box announces the type name
    }

    void setType (EQ::FilterType t, juce::Colour c)
    {
        if (type != t || colour != c)
        {
            type = t;
            colour = c;
            repaint();
        }
    }

    EQ::FilterType getType() const noexcept { return type; }

    void paint (juce::Graphics& g) override { EQ::FilterShapes::draw (g, type, getLocalBounds().toFloat(), colour); }

private:
    EQ::FilterType type = EQ::FilterType::Bell;
    juce::Colour colour = juce::Colours::white;
};
