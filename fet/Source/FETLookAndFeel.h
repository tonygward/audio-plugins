#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"

// Look of the Songsmith Analog Compressor editor: an original dark-panel design informed by the
// hardware's control layout (not a copy of its faceplate). Everything is drawn as vectors in
// design units, so it scales cleanly to any interface size and display density.
namespace FET::UI
{
    namespace Colours
    {
        inline const juce::Colour panel      { 0xff1d1e21 };
        inline const juce::Colour panelLight { 0xff2a2c30 };
        inline const juce::Colour recess     { 0xff121315 };
        inline const juce::Colour text       { 0xffe9e4d8 };
        inline const juce::Colour textDim    { 0xff9b978d };
        inline const juce::Colour accent     { 0xffe8a33d };   // amber: lit buttons, pointers
        inline const juce::Colour bypass     { 0xffc9563c };
        inline const juce::Colour meterFace  { 0xfff1e6c8 };
        inline const juce::Colour meterInk   { 0xff2a2620 };
        inline const juce::Colour meterRed   { 0xffb8402e };
    }

    // The shared Songsmith typeface and type scale (shared/Source/SongsmithType.h).
    using Songsmith::UI::Weight;
    using Songsmith::UI::typeface;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;

    // Fonts come from the shared Songsmith::UI::LookAndFeelBase.
    class LookAndFeel : public Songsmith::UI::LookAndFeelBase
    {
    public:
        LookAndFeel()
        {
            setColour (juce::ComboBox::backgroundColourId, Colours::recess);
            setColour (juce::ComboBox::textColourId, Colours::text);
            setColour (juce::ComboBox::outlineColourId, Colours::panelLight);
            setColour (juce::ComboBox::arrowColourId, Colours::textDim);
            setColour (juce::PopupMenu::backgroundColourId, Colours::panelLight);
            setColour (juce::PopupMenu::textColourId, Colours::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.35f));
            setColour (juce::Label::textColourId, Colours::text);
        }

        // Rotary knob: recessed ring, a dark cap with a soft highlight, and an amber pointer.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);

            g.setColour (Colours::recess);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));

            const float capR = radius * 0.86f;
            juce::ColourGradient cap (Colours::panelLight.brighter (0.25f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                      Colours::panel.darker (0.4f), c.x + capR * 0.6f, c.y + capR * 0.8f, true);
            g.setGradientFill (cap);
            g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);

            const auto tip = c.getPointOnCircumference (capR * 0.9f, angle);
            const auto base = c.getPointOnCircumference (capR * 0.35f, angle);
            g.setColour (s.isEnabled() ? Colours::accent : Colours::textDim);
            g.drawLine ({ base, tip }, juce::jmax (2.0f, radius * 0.07f));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool over, bool down) override
        {
            const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            const auto lit = b.getProperties().getWithDefault ("bypassColour", false) ? Colours::bypass : Colours::accent;

            g.setColour (on ? lit : Colours::recess);
            g.fillRoundedRectangle (r, 4.0f);
            if (over || down)
            {
                g.setColour (juce::Colours::white.withAlpha (down ? 0.18f : 0.08f));
                g.fillRoundedRectangle (r, 4.0f);
            }
            g.setColour (on ? lit.brighter (0.3f) : Colours::panelLight.brighter (0.2f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setFont (getTextButtonFont (b, b.getHeight()));
            g.setColour (b.getToggleState() ? Colours::recess : Colours::text);
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
        }
    };
}
