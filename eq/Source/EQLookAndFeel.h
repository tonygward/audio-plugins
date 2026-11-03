#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EQBinaryData.h"

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
    // --- Text ------------------------------------------------------------------------------
    // One embedded typeface, Inter (SIL OFL 1.1, eq/Resources/Fonts), so Windows and macOS look
    // the same. Hierarchy comes from weight, not capitals: SemiBold for titles, Medium for
    // labels and buttons, Regular for values and hints.
    enum class Weight { Regular, Medium, SemiBold };

    // Loaded once and shared by every editor in the process.
    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static const juce::Typeface::Ptr faces[] = {
            juce::Typeface::createSystemTypefaceFor (EQBinaryData::InterRegular_ttf,  (size_t) EQBinaryData::InterRegular_ttfSize),
            juce::Typeface::createSystemTypefaceFor (EQBinaryData::InterMedium_ttf,   (size_t) EQBinaryData::InterMedium_ttfSize),
            juce::Typeface::createSystemTypefaceFor (EQBinaryData::InterSemiBold_ttf, (size_t) EQBinaryData::InterSemiBold_ttfSize),
        };
        return faces[(int) w];
    }

    // Minimum text sizes at the default editor size (see task 24): 13 for control labels and
    // values, 14-15 for the band title and top-bar controls, 12 for anything else, and 11 only
    // for the graph's grid labels.
    namespace TextSize
    {
        constexpr float gridLabel = 11.0f;
        constexpr float small     = 12.0f;
        constexpr float control   = 13.0f;
        constexpr float topBar    = 14.0f;
        constexpr float title     = 15.0f;
    }

    // The single entry point for fonts. 'height' is JUCE's font height (ascent + descent).
    inline juce::FontOptions font (float height, Weight weight)
    {
        return juce::FontOptions (typeface (weight)).withHeight (height);
    }

    inline juce::FontOptions font (float height, bool bold = false)
    {
        return font (height, bold ? Weight::SemiBold : Weight::Regular);
    }

    // Tabular (fixed-width) figures, for numbers that change while you drag, so they don't jitter.
    inline juce::FontOptions numbers (float height, Weight weight = Weight::Regular)
    {
        return font (height, weight).withFeatureEnabled ("tnum");
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

    // Every font getter returns the embedded typeface explicitly. (A Font with the default
    // typeface name is resolved through the *default* LookAndFeel, which a plug-in shouldn't
    // replace, so overriding getTypefaceForFont alone isn't enough.)
    juce::Font getLabelFont (juce::Label& label) override
    {
        // A slider's value box: tabular figures so the number doesn't jitter while dragging.
        if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
            return juce::Font (EQColours::numbers (slider->getSliderStyle() == juce::Slider::LinearBar
                                                       ? EQColours::TextSize::topBar : EQColours::TextSize::control));

        const auto f = label.getFont();
        if (f.getTypefacePtr() == EQColours::typeface (EQColours::Weight::Regular)
            || f.getTypefacePtr() == EQColours::typeface (EQColours::Weight::Medium)
            || f.getTypefacePtr() == EQColours::typeface (EQColours::Weight::SemiBold))
            return f;   // set with EQColours::font()

        return juce::Font (EQColours::font (juce::jmax (EQColours::TextSize::small, f.getHeight()), f.isBold()));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (EQColours::font (juce::jlimit (EQColours::TextSize::control, EQColours::TextSize::topBar,
                                                          (float) buttonHeight * 0.6f),
                                            EQColours::Weight::Medium));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (EQColours::font (box.getHeight() >= 26 ? EQColours::TextSize::topBar : EQColours::TextSize::control));
    }

    juce::Font getPopupMenuFont() override               { return juce::Font (EQColours::font (15.0f)); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (EQColours::numbers (EQColours::TextSize::control)); }
    juce::Font getAlertWindowTitleFont() override        { return juce::Font (EQColours::font (17.0f, EQColours::Weight::SemiBold)); }
    juce::Font getAlertWindowMessageFont() override      { return juce::Font (EQColours::font (15.0f)); }
    juce::Font getAlertWindowFont() override             { return juce::Font (EQColours::font (14.0f)); }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
            return EQColours::typeface (f.isBold() ? EQColours::Weight::SemiBold : EQColours::Weight::Regular);
        return LookAndFeel_V4::getTypefaceForFont (f);
    }

    // Tooltips: JUCE's default uses a bold system font at 13 px; use the embedded one.
    static juce::TextLayout layoutTooltip (const juce::String& text, juce::Colour colour)
    {
        juce::AttributedString s;
        s.setWordWrap (juce::AttributedString::WordWrap::byWord);
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, juce::Font (EQColours::font (EQColours::TextSize::control)), colour);

        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths (s, 420.0f);
        return tl;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const auto tl = layoutTooltip (tipText, juce::Colours::black);
        const int w = (int) std::ceil (tl.getWidth() + 18.0f), h = (int) std::ceil (tl.getHeight() + 10.0f);

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                     w, h).constrainedWithin (parentArea);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        const auto bounds = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (juce::Colour (0xf0101216));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        layoutTooltip (text, EQColours::text).draw (g, bounds.reduced (9.0f, 5.0f));
    }

    // Compact bar sliders (top-bar output gain): bipolar ranges fill from zero.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto r = juce::Rectangle<int> (x, y, width, height).toFloat();
        g.setColour (EQColours::panel);
        g.fillRoundedRectangle (r, 4.0f);

        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
        const float zeroX = bipolar ? (float) slider.getPositionOfValue (0.0) : r.getX();
        const float from = juce::jmin (zeroX, sliderPos), to = juce::jmax (zeroX, sliderPos);

        if (to - from > 0.5f)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId).withAlpha (slider.isEnabled() ? 0.55f : 0.2f));
            g.fillRect (juce::Rectangle<float> (from, r.getY() + 1.0f, to - from, r.getHeight() - 2.0f));
        }

        g.setColour (EQColours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
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
