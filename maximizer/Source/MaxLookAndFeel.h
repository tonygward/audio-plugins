#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"

// Colours and control drawing shared with Songsmith EQ (eq/Source/EQLookAndFeel.h): the same
// palette and the same flat arc knobs. Fonts and the type scale come from the shared Songsmith UI
// library (shared/Source/SongsmithType.h), like every Songsmith plugin.
namespace MaxColours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour panel        { 0xff1f232a };
    const juce::Colour panelEdge    { 0xff2e333c };
    const juce::Colour track        { 0xff2b3039 };
    const juce::Colour label        { 0xff8a919e };
    const juce::Colour text         { 0xffe4e7ec };
    const juce::Colour accent       { 0xfff2c94c };   // Songsmith yellow (the EQ's total curve)
    const juce::Colour threshold    { 0xff5aa9ff };
    const juce::Colour ceiling      { 0xffff9f43 };
    const juce::Colour reduction    { 0xffff6b6b };
    const juce::Colour meterLow     { 0xff3ddbc4 };
    const juce::Colour clip         { 0xffff3b3b };

    using Songsmith::UI::Weight;
    using Songsmith::UI::typeface;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;
}

class MaxLookAndFeel : public Songsmith::UI::LookAndFeelBase
{
public:
    MaxLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, MaxColours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, MaxColours::label);
        setColour (juce::ComboBox::backgroundColourId, MaxColours::panel);
        setColour (juce::ComboBox::outlineColourId, MaxColours::panelEdge);
        setColour (juce::ComboBox::textColourId, MaxColours::text);
        setColour (juce::ComboBox::arrowColourId, MaxColours::label);
        setColour (juce::PopupMenu::backgroundColourId, MaxColours::panel);
        setColour (juce::PopupMenu::textColourId, MaxColours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, MaxColours::panelEdge);
        setColour (juce::TextButton::buttonColourId, MaxColours::panel);
        setColour (juce::TextButton::buttonOnColourId, MaxColours::accent.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOffId, MaxColours::text);
        setColour (juce::TextButton::textColourOnId, MaxColours::background);
        setColour (juce::ComboBox::focusedOutlineColourId, MaxColours::panelEdge);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto colour = background;
        if (down)             colour = colour.brighter (0.15f);
        else if (highlighted) colour = colour.brighter (0.07f);
        g.setColour (colour);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (MaxColours::panelEdge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    // Vertical faders (Threshold, Out Ceiling): a slim track filled from the top down to the value,
    // like the L1's sliders, with a flat handle.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float trackW = 6.0f;
        const auto track = juce::Rectangle<float> (bounds.getCentreX() - trackW * 0.5f, bounds.getY(), trackW, bounds.getHeight());
        g.setColour (MaxColours::track);
        g.fillRoundedRectangle (track, trackW * 0.5f);

        const auto accent = slider.findColour (juce::Slider::trackColourId);
        g.setColour (accent.withAlpha (slider.isEnabled() ? 0.9f : 0.3f));
        g.fillRoundedRectangle (track.withBottom (sliderPos), trackW * 0.5f);

        const auto handle = juce::Rectangle<float> (juce::jmin (bounds.getWidth(), 34.0f), 12.0f).withCentre ({ bounds.getCentreX(), sliderPos });
        g.setColour (juce::Colour (0xff343a45));
        g.fillRoundedRectangle (handle, 3.0f);
        g.setColour (accent);
        g.fillRect (handle.withSizeKeepingCentre (handle.getWidth() - 8.0f, 2.0f));
    }

    // Flat arc knob, as in the EQ.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float trackWidth = juce::jmax (2.5f, radius * 0.12f);
        const float arcRadius = radius - trackWidth * 0.5f;
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        const float bodyRadius = arcRadius - trackWidth * 1.4f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343a45), centre.x, centre.y - bodyRadius,
                                                 juce::Colour (0xff1e2229), centre.x, centre.y + bodyRadius, false));
        g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (MaxColours::track);
        g.strokePath (trackPath, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
        if (valueAngle - startAngle > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, valueAngle, true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const auto tip = centre.getPointOnCircumference (bodyRadius * 0.85f, valueAngle);
        const auto base = centre.getPointOnCircumference (bodyRadius * 0.35f, valueAngle);
        g.setColour (MaxColours::text);
        g.drawLine ({ base, tip }, 2.0f);
    }
};
