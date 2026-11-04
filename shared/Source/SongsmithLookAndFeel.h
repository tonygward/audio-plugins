#pragma once

#include "SongsmithType.h"

namespace Songsmith::UI
{
    // The base look-and-feel of every Songsmith editor. It serves the embedded typeface and gives
    // labels, buttons, combo boxes, popup menus, tooltips, slider pop-ups and alert windows their
    // sizes from the type scale. Each plugin's look-and-feel derives from it and adds its colours
    // and special drawing (meters, faders, the EQ graph). Shared control drawing (task S2) goes here.
    class LookAndFeelBase : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeelBase()
        {
            setDefaultSansSerifTypeface (typeface (Weight::Regular));
        }

        // Every font getter returns the embedded typeface explicitly. (A Font with the default
        // typeface name is resolved through the *default* LookAndFeel, which a plugin shouldn't
        // replace, so overriding getTypefaceForFont alone isn't enough.)
        juce::Font getLabelFont (juce::Label& label) override
        {
            // A slider's value box: tabular figures so the number doesn't jitter while dragging.
            if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
                return juce::Font (numbers (slider->getSliderStyle() == juce::Slider::LinearBar ? Type::heading : Type::control));

            const auto f = label.getFont();
            if (isEmbedded (f.getTypefacePtr()))
                return f;   // set with Songsmith::UI::font()

            return juce::Font (font (juce::jmax (Type::hint, f.getHeight()), f.isBold() ? Weight::SemiBold : Weight::Regular));
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (font (Type::forControlHeight (buttonHeight), Weight::Medium));
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return juce::Font (font (Type::forControlHeight (box.getHeight())));
        }

        juce::Font getPopupMenuFont() override                 { return juce::Font (font (Type::heading)); }
        juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (numbers (Type::control)); }
        juce::Font getAlertWindowTitleFont() override          { return juce::Font (font (Type::title, Weight::SemiBold)); }
        juce::Font getAlertWindowMessageFont() override        { return juce::Font (font (Type::heading)); }
        juce::Font getAlertWindowFont() override               { return juce::Font (font (Type::control)); }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
                return typeface (f.isBold() ? Weight::SemiBold : Weight::Regular);
            return LookAndFeel_V4::getTypefaceForFont (f);
        }

        // Tooltips: JUCE's default uses a bold font at 15 px; use the type scale. Colours come from
        // TooltipWindow::backgroundColourId, outlineColourId and textColourId.
        static juce::TextLayout layoutTooltip (const juce::String& text, juce::Colour colour)
        {
            juce::AttributedString s;
            s.setWordWrap (juce::AttributedString::WordWrap::byWord);
            s.setJustification (juce::Justification::centredLeft);
            s.append (text, juce::Font (font (Type::control)), colour);

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
            g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (findColour (juce::TooltipWindow::outlineColourId));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

            layoutTooltip (text, findColour (juce::TooltipWindow::textColourId)).draw (g, bounds.reduced (9.0f, 5.0f));
        }
    };
}
