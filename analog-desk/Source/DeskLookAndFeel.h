#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "../../shared/Source/SongsmithLookAndFeel.h"

// Look of the Songsmith Analog Desk editor: an original dark-panel design informed by the way a channel strip is arranged
// (input and filters, equaliser, dynamics), not a copy of any faceplate. Everything is drawn as vectors in design units, so
// it scales cleanly to any interface size and display density.
namespace Desk::UI
{
    namespace Colours
    {
        // The shared Songsmith neutrals (shared/Source/SongsmithColours.h)...
        inline const juce::Colour panel      = Songsmith::UI::Colours::panel;
        inline const juce::Colour panelLight = Songsmith::UI::Colours::panelLight;
        inline const juce::Colour recess     = Songsmith::UI::Colours::recess;
        inline const juce::Colour text       = Songsmith::UI::Colours::text;
        inline const juce::Colour textDim    = Songsmith::UI::Colours::textDim;
        // ...and the Analog Desk's accent (a cool teal).
        inline const juce::Colour accent     { 0xff6fb6c8 };
    }

    // The shared Songsmith typeface and type scale (shared/Source/SongsmithType.h).
    using Songsmith::UI::Weight;
    using Songsmith::UI::typeface;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;

    // Fonts, buttons, knobs and combo boxes come from the shared Songsmith::UI::LookAndFeelBase.
    class LookAndFeel : public Songsmith::UI::LookAndFeelBase
    {
    public:
        LookAndFeel() : LookAndFeelBase (Colours::accent)
        {
            setColour (juce::Label::textColourId, Colours::text);
        }
    };
}
