#pragma once

// Shared checks for the plugin title tooltip (SUI-17, replacing the ADV-01 Guidance button),
// compiled into every plugin's test executable:
//   static SongsmithGuidanceTests sharedGuidanceTests { "EQ" };   // once per executable

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/SongsmithGuidance.h"
#include "../Source/SongsmithWording.h"

class SongsmithGuidanceTests final : public juce::UnitTest
{
public:
    explicit SongsmithGuidanceTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith Title Tooltip", category) {}

    void runTest() override
    {
        using namespace Songsmith::Guidance;

        const juce::StringArray names { "Songsmith EQ", "Songsmith Analog Compressor", "Songsmith Maximiser" };

        beginTest ("Every plugin title has a short, plain, truthful tooltip");
        {
            const juce::StringArray forbidden { "mud", "streaming", "spotify", "apple music", "youtube", "must", "required",
                                                "should be", "target", "lufs", "bit depth", "measured", "detected", "diagnos" };
            for (const auto& name : names)
            {
                const auto text = titleTooltipFor (name);
                expect (text.isNotEmpty(), name + " has a title tooltip");
                expect (text.length() <= 200, "short enough to read at a glance: " + name);
                expect (text.endsWithChar ('.'), "a full sentence: " + name);
                expect (Songsmith::UI::Wording::findBanned (text).isEmpty(),
                        "no abbreviation in \"" + text + "\" (found \"" + Songsmith::UI::Wording::findBanned (text) + "\")");
                for (const auto& word : forbidden)
                    expect (! text.containsIgnoreCase (word), "forbidden claim \"" + word + "\" in \"" + text + "\"");
                expect (! text.contains ("color") && ! text.contains ("analyze"), "British spelling");
            }
            expect (titleTooltipFor ("Something Else").isEmpty(), "an unknown name gets no invented tooltip");
        }

        beginTest ("Title hint carries the tooltip, the accessible text and keyboard focus");
        {
            for (const auto& name : names)
            {
                TitleHint hint (name);
                expectEquals (hint.getTooltip(), titleTooltipFor (name));
                expectEquals (hint.getTitle(), name);
                expectEquals (hint.getDescription(), titleTooltipFor (name));
                expect (hint.getWantsKeyboardFocus(), "reachable from the keyboard");
                expect (hint.isAccessible());
            }
        }

        beginTest ("Title hint covers only the title, not the space beside it");
        {
            juce::Component parent;
            parent.setSize (600, 40);
            parent.setVisible (true);   // components start hidden, and getComponentAt skips hidden ones
            TitleHint hint (names[0]);
            parent.addAndMakeVisible (hint);
            hint.setBounds (16, 7, 120, 26);
            expect (parent.getComponentAt (20, 20) == &hint, "over the title");
            expect (parent.getComponentAt (400, 20) != &hint, "blank space shows nothing");
        }
    }
};
