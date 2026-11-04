#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithFonts.h"

// Text for every Songsmith Studio plugin: one embedded typeface, one type scale and one title.
// See docs/style-guide.md. Plugins link this library (shared/) instead of copying it.
namespace Songsmith::UI
{
    // Inter (SIL OFL 1.1, shared/Resources/Fonts), so Windows and macOS look the same. Hierarchy
    // comes from weight and size, not capitals: SemiBold for titles and headings, Medium for
    // labels and buttons, Regular for values and hints.
    enum class Weight { Regular, Medium, SemiBold };

    // Loaded once and shared by every editor in the process.
    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static const juce::Typeface::Ptr faces[] = {
            juce::Typeface::createSystemTypefaceFor (SongsmithFonts::InterRegular_ttf,  (size_t) SongsmithFonts::InterRegular_ttfSize),
            juce::Typeface::createSystemTypefaceFor (SongsmithFonts::InterMedium_ttf,   (size_t) SongsmithFonts::InterMedium_ttfSize),
            juce::Typeface::createSystemTypefaceFor (SongsmithFonts::InterSemiBold_ttf, (size_t) SongsmithFonts::InterSemiBold_ttfSize),
        };
        return faces[(int) w];
    }

    inline bool isEmbedded (const juce::Typeface::Ptr& t)
    {
        return t != nullptr && (t == typeface (Weight::Regular) || t == typeface (Weight::Medium) || t == typeface (Weight::SemiBold));
    }

    // The type scale, in px at 100 % interface size (the editors scale on top). Use a role, never
    // a number. 'height' is JUCE's font height (ascent + descent).
    namespace Type
    {
        constexpr float caption = 11.0f;   // graph grid and knob scale labels; lighter colour only
        constexpr float hint    = 12.0f;   // hints, status words, meter captions, small pills
        constexpr float control = 13.0f;   // control labels and values, buttons and menus in panels
        constexpr float heading = 14.0f;   // band and knob titles, section captions, top-bar controls, menus
        constexpr float title   = 18.0f;   // "Songsmith" + product name

        constexpr float smallest = caption;   // nothing on screen is smaller
        constexpr float smallestControl = hint;   // nothing but grid and scale labels is smaller

        // Buttons and combo boxes: top-bar sized controls (26 px or taller) use heading, smaller ones control.
        constexpr float forControlHeight (int height) { return height >= 26 ? heading : control; }
    }

    // The single entry point for fonts.
    inline juce::FontOptions font (float height, Weight weight = Weight::Regular)
    {
        return juce::FontOptions (typeface (weight)).withHeight (height);
    }

    // Tabular (fixed-width) figures, for numbers that change while you drag, so they don't jitter.
    inline juce::FontOptions numbers (float height, Weight weight = Weight::Regular)
    {
        return font (height, weight).withFeatureEnabled ("tnum");
    }

    //==============================================================================
    // The title: "Songsmith" in SemiBold, then the product name in Regular in the plugin's accent
    // colour, both at Type::title. Every plugin draws it with drawTitle, left aligned and
    // vertically centred in its title area, starting titleInset from the editor's left edge.
    constexpr float titleInset = 24.0f;
    constexpr float titleBarHeight = 52.0f;   // the compressor's title bar; plugins without a bar centre it in their top row

    struct TitleParts { juce::String brand, product; };

    // "Songsmith EQ" -> { "Songsmith", "EQ" }. A name without the brand is all product.
    inline TitleParts splitTitle (const juce::String& pluginName)
    {
        const juce::String brand ("Songsmith");
        if (pluginName.startsWith (brand + " "))
            return { brand, pluginName.substring (brand.length() + 1) };
        return { {}, pluginName };
    }

    inline float titleWidth (const juce::String& pluginName)
    {
        const auto parts = splitTitle (pluginName);
        const juce::Font brandFont (font (Type::title, Weight::SemiBold)), productFont (font (Type::title, Weight::Regular));
        float w = juce::GlyphArrangement::getStringWidth (productFont, parts.product);
        if (parts.brand.isNotEmpty())
            w += juce::GlyphArrangement::getStringWidth (brandFont, parts.brand)
                 + juce::GlyphArrangement::getStringWidth (productFont, " ");
        return w;
    }

    inline void drawTitle (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& pluginName,
                           juce::Colour textColour, juce::Colour accent)
    {
        const auto parts = splitTitle (pluginName);
        const juce::Font brandFont (font (Type::title, Weight::SemiBold)), productFont (font (Type::title, Weight::Regular));

        if (parts.brand.isNotEmpty())
        {
            const float w = juce::GlyphArrangement::getStringWidth (brandFont, parts.brand);
            g.setColour (textColour);
            g.setFont (brandFont);
            g.drawText (parts.brand, area.removeFromLeft (w + 1.0f), juce::Justification::centredLeft, false);
            area.removeFromLeft (juce::GlyphArrangement::getStringWidth (productFont, " ") - 1.0f);
        }

        g.setColour (accent);
        g.setFont (productFont);
        g.drawText (parts.product, area, juce::Justification::centredLeft, false);
    }
}
