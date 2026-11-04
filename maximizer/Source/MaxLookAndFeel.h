#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"

// Colours of the Songsmith Maximiser. Fonts, the type scale, the neutral colours and the bevelled
// controls (buttons, the Release knob, the faders, combo boxes) come from the shared Songsmith UI
// library (shared/), like every Songsmith plugin. Only the meters are drawn here (Meters.h).
namespace MaxColours
{
    // The shared Songsmith neutrals (shared/Source/SongsmithColours.h)...
    inline const juce::Colour background = Songsmith::UI::Colours::panel;
    inline const juce::Colour panel      = Songsmith::UI::Colours::panelLight;
    inline const juce::Colour panelEdge  = Songsmith::UI::Colours::edge;
    inline const juce::Colour track      = Songsmith::UI::Colours::recess;
    inline const juce::Colour label      = Songsmith::UI::Colours::textDim;
    inline const juce::Colour text       = Songsmith::UI::Colours::text;
    // ...the maximiser's accent, and colours only it uses.
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
    MaxLookAndFeel() : LookAndFeelBase (MaxColours::accent)
    {
        setColour (juce::TextButton::buttonOnColourId, MaxColours::accent.withAlpha (0.85f));
    }
};
