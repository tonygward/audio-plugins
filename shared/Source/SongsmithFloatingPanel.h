#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SongsmithColours.h"

// A floating control panel the musician can drag, the same look and behaviour as the Songsmith EQ's band panel
// (EQ-37): a rounded, softly lit panel with a grip of dots at the top edge, moved by dragging its background, and
// always kept whole inside the area it floats over. The parent sets that area with setLimits(). The position can be
// read and set as two fractions of the free travel (0 to 1), so it can be saved with a session and still land in the
// same relative place at any editor size.
namespace Songsmith::UI
{
    class FloatingPanel : public juce::Component
    {
    public:
        // Called after the user has moved the panel (not when the parent repositions it).
        std::function<void()> onUserMoved;

        void setAccent (juce::Colour c) { accent = c; repaint(); }

        // Where the whole panel must stay, in the parent's coordinates. Keeps the panel inside at once.
        void setLimits (juce::Rectangle<int> area)
        {
            limits = area;
            applyBounds (fitInside (getBounds()));
        }

        // The nearest place to 'wanted' where the whole panel is inside the limits. If the area is too small for the
        // panel, the top-left corner stays visible so the first controls remain reachable.
        juce::Rectangle<int> fitInside (juce::Rectangle<int> wanted) const
        {
            const int x = juce::jlimit (limits.getX(), juce::jmax (limits.getX(), limits.getRight() - wanted.getWidth()), wanted.getX());
            const int y = juce::jlimit (limits.getY(), juce::jmax (limits.getY(), limits.getBottom() - wanted.getHeight()), wanted.getY());
            return wanted.withPosition (x, y);
        }

        // The panel's top-left as a fraction of its free travel inside the limits: (0, 0) top left, (1, 1) bottom right.
        juce::Point<float> getRelativePosition() const
        {
            const float travelX = (float) juce::jmax (1, limits.getWidth() - getWidth());
            const float travelY = (float) juce::jmax (1, limits.getHeight() - getHeight());
            return { juce::jlimit (0.0f, 1.0f, (float) (getX() - limits.getX()) / travelX),
                     juce::jlimit (0.0f, 1.0f, (float) (getY() - limits.getY()) / travelY) };
        }

        void setRelativePosition (juce::Point<float> p)
        {
            relative = { juce::jlimit (0.0f, 1.0f, std::isfinite (p.x) ? p.x : 0.0f), juce::jlimit (0.0f, 1.0f, std::isfinite (p.y) ? p.y : 0.0f) };
            const int travelX = juce::jmax (0, limits.getWidth() - getWidth()), travelY = juce::jmax (0, limits.getHeight() - getHeight());
            applyBounds (getBounds().withPosition (limits.getX() + juce::roundToInt (relative.x * (float) travelX),
                                                   limits.getY() + juce::roundToInt (relative.y * (float) travelY)));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (auto* parent = getParentComponent())
            {
                dragStartPosition = getPosition();
                dragStartMouse = e.getEventRelativeTo (parent).position;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (auto* parent = getParentComponent())
            {
                const auto delta = e.getEventRelativeTo (parent).position - dragStartMouse;
                applyBounds (fitInside (getBounds().withPosition (dragStartPosition + delta.roundToInt())));
                relative = getRelativePosition();
                if (onUserMoved)
                    onUserMoved();
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setGradientFill (juce::ColourGradient (Colours::panelLight.withAlpha (0.97f), 0.0f, r.getY(),
                                                     Colours::panel.withAlpha (0.97f), 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (accent.withAlpha (0.7f));
            g.drawRoundedRectangle (r, 9.0f, 1.2f);

            // A small grip of dots at the top edge says "you can move this".
            g.setColour (Colours::textDim.withAlpha (0.55f));
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 5; ++col)
                    g.fillEllipse ((float) getWidth() * 0.5f + ((float) col - 2.0f) * 6.0f - 1.0f, 4.0f + (float) row * 4.0f, 2.0f, 2.0f);
        }

    private:
        void applyBounds (juce::Rectangle<int> b)
        {
            if (b != getBounds())
                setBounds (b);
        }

        juce::Rectangle<int> limits;
        juce::Colour accent = Colours::edge;
        juce::Point<float> relative { 0.0f, 0.0f };
        juce::Point<int> dragStartPosition;
        juce::Point<float> dragStartMouse;
    };
}
