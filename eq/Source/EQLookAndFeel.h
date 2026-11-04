#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"

namespace EQColours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour plotTop      { 0xff1d2027 };
    const juce::Colour plotBottom   { 0xff121419 };
    // The shared Songsmith neutrals (shared/Source/SongsmithColours.h).
    inline const juce::Colour panel     = Songsmith::UI::Colours::panelLight;
    inline const juce::Colour panelEdge = Songsmith::UI::Colours::edge;
    const juce::Colour gridMinor    { 0x14ffffff };
    const juce::Colour gridMajor    { 0x26ffffff };
    const juce::Colour zeroLine     { 0x50ffffff };
    inline const juce::Colour label     = Songsmith::UI::Colours::textDim;
    inline const juce::Colour text      = Songsmith::UI::Colours::text;
    const juce::Colour totalCurve   { 0xfff2c94c };
    const juce::Colour accent       { 0xfff2c94c };   // Songsmith yellow: the title, lit buttons, knob pointers

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
    // The shared Songsmith typeface and type scale (shared/Source/SongsmithType.h).
    using Songsmith::UI::Weight;
    using Songsmith::UI::typeface;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;
}

// Fonts, buttons, knobs, combo boxes and the bar sliders come from the shared
// Songsmith::UI::LookAndFeelBase. Band controls take their band's colour as their accent
// (BandPanel); the graph, nodes and meter are drawn by EQDisplay and OutputMeterDisplay.
class EQLookAndFeel : public Songsmith::UI::LookAndFeelBase
{
public:
    EQLookAndFeel() : LookAndFeelBase (EQColours::accent)
    {
        setColour (juce::Label::textColourId, EQColours::label);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
};
