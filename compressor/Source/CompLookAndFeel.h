#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"

// Colours of the Songsmith Compressor. Fonts, the type scale, the neutral colours and the bevelled
// controls come from the shared Songsmith UI library (shared/), like every Songsmith plugin. Only
// the meters are drawn here (CompMeter.h).
namespace CompColours
{
    inline const juce::Colour background = Songsmith::UI::Colours::panel;
    inline const juce::Colour panelEdge  = Songsmith::UI::Colours::edge;
    inline const juce::Colour track      = Songsmith::UI::Colours::recess;
    inline const juce::Colour label      = Songsmith::UI::Colours::textDim;
    inline const juce::Colour text       = Songsmith::UI::Colours::text;
    // The compressor's accent: Songsmith teal. Colours only the meters use follow.
    const juce::Colour accent    { 0xff3ddbc4 };
    const juce::Colour reduction { 0xffff9f43 };
    const juce::Colour meterHigh { 0xfff2c94c };
    const juce::Colour clip      { 0xffff3b3b };

    using Songsmith::UI::Weight;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;
}

class CompLookAndFeel : public Songsmith::UI::LookAndFeelBase
{
public:
    CompLookAndFeel() : LookAndFeelBase (CompColours::accent)
    {
        setColour (juce::TextButton::buttonOnColourId, CompColours::accent.withAlpha (0.85f));
    }
};
