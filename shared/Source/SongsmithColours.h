#pragma once

#include <juce_graphics/juce_graphics.h>

// The neutral colours every Songsmith editor shares (see docs/style-guide.md, "Colours"). Each
// plugin adds one accent colour; nothing else should be a new grey.
namespace Songsmith::UI::Colours
{
    inline const juce::Colour panel      { 0xff1e2024 };   // panel, bottom of the panel gradient
    inline const juce::Colour panelLight { 0xff2b2d32 };   // top of the panel gradient, raised controls
    inline const juce::Colour recess     { 0xff121417 };   // wells: combo boxes, value fields, knob rings, tracks
    inline const juce::Colour edge       { 0xff30343b };   // rules and panel outlines
    inline const juce::Colour text       { 0xffe7e6e3 };   // values and labels that matter
    inline const juce::Colour textDim    { 0xff9a9b97 };   // captions, hints, scales (4.5:1 or more on the panel)

    // Light comes from the top left. Raised surfaces get this on their top edge...
    inline const juce::Colour highlight  { 0x1fffffff };
    // ...and this on their bottom edge; recessed ones the other way round.
    inline const juce::Colour shadow     { 0x73000000 };
}
