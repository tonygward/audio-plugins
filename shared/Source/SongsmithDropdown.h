#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// Dropdowns that every Songsmith plugin sizes and words the same way (songsmithstudio#15).
//
// Width: the longest item, plus room for the chevron, the padding and the focus outline, and nothing more. It follows the
// text, so a longer label (a translation, a new choice) still fits and a short one never leaves wasted space.
// Oversampling: one wording, "Oversampling: off", "Oversampling: 2×", "Oversampling: 4×", in every plugin that has it.
namespace Songsmith::UI::Dropdown
{
    // Left padding, the chevron, the right padding and the focus outline, in design units.
    inline constexpr int chevronAndPadding = 44;

    inline int textWidth (const juce::String& text, const juce::Font& font)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, text, 0.0f, 0.0f);
        return (int) std::ceil (glyphs.getBoundingBox (0, -1, false).getWidth());
    }

    // The width a dropdown needs for the items it holds now, set in 'font'.
    inline int widthFor (const juce::ComboBox& box, const juce::Font& font)
    {
        int widest = 0;
        for (int i = 0; i < box.getNumItems(); ++i)
            widest = juce::jmax (widest, textWidth (box.getItemText (i), font));
        // A box that reserves room for an icon (the "textIndent" property, default 4) needs that room too.
        const int indent = (int) box.getProperties().getWithDefault ("textIndent", 4);
        return widest + chevronAndPadding + juce::jmax (0, indent - 4);
    }

    // Puts a dropdown at (x, y) with the given height, as wide as its text needs, and returns its right edge. The font follows
    // the height (see Songsmith::UI::LookAndFeelBase::getComboBoxFont), so the height is set before the width is measured.
    inline int place (juce::ComboBox& box, int x, int y, int height)
    {
        box.setBounds (x, y, 100, height);
        const int width = widthFor (box, box.getLookAndFeel().getComboBoxFont (box));
        box.setBounds (x, y, width, height);
        return x + width;
    }

    // 0 = off, 1 = 2×, 2 = 4× (the multiplication sign, not the letter x).
    inline juce::String oversamplingText (int index)
    {
        return index <= 0 ? juce::String ("Oversampling: off")
                          : "Oversampling: " + juce::String (1 << juce::jlimit (1, 2, index)) + juce::String::charToString (0x00d7);
    }

    inline juce::StringArray oversamplingItems() { return { oversamplingText (0), oversamplingText (1), oversamplingText (2) }; }
}
