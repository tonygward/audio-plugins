#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "../../shared/Source/SongsmithLookAndFeel.h"

// Look of the Songsmith Bus Compressor editor: an original dark-panel design informed by the hardware's
// row-of-knobs control layout (not a copy of its faceplate). Everything is drawn as vectors in design units, so it
// scales cleanly to any interface size and display density.
namespace BusComp::UI
{
    namespace Colours
    {
        // The shared Songsmith neutrals (shared/Source/SongsmithColours.h)...
        inline const juce::Colour panel      = Songsmith::UI::Colours::panel;
        inline const juce::Colour panelLight = Songsmith::UI::Colours::panelLight;
        inline const juce::Colour recess     = Songsmith::UI::Colours::recess;
        inline const juce::Colour text       = Songsmith::UI::Colours::text;
        inline const juce::Colour textDim    = Songsmith::UI::Colours::textDim;
        // ...the bus compressor's accent (a warm amber), and colours only the meter uses.
        inline const juce::Colour accent     { 0xffe6a15c };
        inline const juce::Colour meterFace  { 0xfff1e6c8 };
        inline const juce::Colour meterInk   { 0xff2a2620 };
        inline const juce::Colour meterRed   { 0xffb8402e };
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
