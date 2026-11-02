#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace EQColours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour plotTop      { 0xff1d2027 };
    const juce::Colour plotBottom   { 0xff121419 };
    const juce::Colour panel        { 0xff1f232a };
    const juce::Colour panelEdge    { 0xff2e333c };
    const juce::Colour gridMinor    { 0x14ffffff };
    const juce::Colour gridMajor    { 0x26ffffff };
    const juce::Colour zeroLine     { 0x50ffffff };
    const juce::Colour label        { 0xff8a919e };
    const juce::Colour text         { 0xffe4e7ec };
    const juce::Colour totalCurve   { 0xfff2c94c };

    // Stable colour identity per band slot: the original three, then golden-ratio hues.
    inline juce::Colour band (int index)
    {
        static const juce::Colour first[] = {
            juce::Colour (0xffff6b6b),   // 1 coral
            juce::Colour (0xffb57bff),   // 2 violet
            juce::Colour (0xff3ddbc4),   // 3 teal
            juce::Colour (0xff5aa9ff),   // 4 blue
            juce::Colour (0xffff9f43),   // 5 orange
            juce::Colour (0xff7bed6f),   // 6 green
            juce::Colour (0xffff6bcb),   // 7 pink
            juce::Colour (0xffa3a8ff),   // 8 periwinkle
        };

        if (index >= 0 && index < 8)
            return first[index];

        const float hue = std::fmod (0.07f + (float) index * 0.618034f, 1.0f);
        return juce::Colour::fromHSV (hue, 0.55f, 0.98f, 1.0f);
    }
    inline juce::FontOptions font (float height, bool bold = false)
    {
        return juce::FontOptions ("Segoe UI", height, bold ? juce::Font::bold : juce::Font::plain);
    }
}

// Flat, arc-style rotary knobs. Bipolar ranges (e.g. gain) fill from the centre.
class EQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EQLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, EQColours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, EQColours::label);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        return juce::Font (EQColours::font (label.getFont().getHeight(), label.getFont().isBold()));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (EQColours::font (juce::jmin (15.0f, (float) buttonHeight * 0.6f), true));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float trackWidth = juce::jmax (2.5f, radius * 0.12f);
        const float arcRadius = radius - trackWidth * 0.5f;

        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // Knob body
        const float bodyRadius = arcRadius - trackWidth * 1.4f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343a45), centre.x, centre.y - bodyRadius,
                                                 juce::Colour (0xff1e2229), centre.x, centre.y + bodyRadius, false));
        g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

        // Background track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2b3039));
        g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
        const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        if (std::abs (valueAngle - fromAngle) > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                    juce::jmin (fromAngle, valueAngle), juce::jmax (fromAngle, valueAngle), true);
            g.setColour (accent);
            g.strokePath (valueArc, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pointer
        const auto tip = centre.getPointOnCircumference (bodyRadius * 0.85f, valueAngle);
        const auto base = centre.getPointOnCircumference (bodyRadius * 0.35f, valueAngle);
        g.setColour (EQColours::text);
        g.drawLine ({ base, tip }, 2.0f);
    }
};
