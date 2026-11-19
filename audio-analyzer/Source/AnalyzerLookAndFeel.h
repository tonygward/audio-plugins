#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../shared/Source/SongsmithLookAndFeel.h"

// Look of the Songsmith Audio Analyzer: the suite's dark panel, bevelled buttons and Inter typeface from the shared
// library, with one accent colour (a calm teal). Everything is drawn in design units, so it scales with the window.
namespace Songsmith::Analyzer
{
    namespace Colours
    {
        inline const juce::Colour accent { 0xff6cc4b8 };
    }

    class LookAndFeel : public Songsmith::UI::LookAndFeelBase
    {
    public:
        LookAndFeel() : LookAndFeelBase (Colours::accent)
        {
            setColour (juce::Label::textColourId, UI::Colours::text);
            setColour (juce::ProgressBar::backgroundColourId, UI::Colours::recess);
            setColour (juce::ProgressBar::foregroundColourId, Colours::accent);
            setColour (juce::ScrollBar::thumbColourId, UI::Colours::textDim.withAlpha (0.6f));
            setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
        }
    };
}
