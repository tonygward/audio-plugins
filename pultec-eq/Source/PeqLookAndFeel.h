#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "../../shared/Source/SongsmithLookAndFeel.h"

// Look of the Songsmith Passive EQ editor: an original dark-panel design informed by the hardware's grouped
// low, high boost and high cut controls (not a copy of its faceplate). Everything is drawn as vectors in design
// units, so it scales cleanly to any interface size and display density.
namespace Peq::UI
{
    namespace Colours
    {
        // The shared Songsmith neutrals (shared/Source/SongsmithColours.h)...
        inline const juce::Colour panel      = Songsmith::UI::Colours::panel;
        inline const juce::Colour panelLight = Songsmith::UI::Colours::panelLight;
        inline const juce::Colour recess     = Songsmith::UI::Colours::recess;
        inline const juce::Colour text       = Songsmith::UI::Colours::text;
        inline const juce::Colour textDim    = Songsmith::UI::Colours::textDim;
        // ...this plugin's accent (a soft sky blue, lighter than the compressor's blue themes)...
        inline const juce::Colour accent     { 0xff9fc8ff };
        // ...and the blue of its textured faceplate (PEQ-02): deeper at the bottom, lit from the top.
        inline const juce::Colour plateTop    { 0xff2a548f };
        inline const juce::Colour plateBottom { 0xff183560 };
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
