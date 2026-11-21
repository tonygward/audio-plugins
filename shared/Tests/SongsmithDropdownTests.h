#pragma once

// songsmithstudio#15: the Oversampling dropdown is worded the same way and sized by its text in every plugin that has it:
//   static SongsmithDropdownTests<MyProcessor, MyEditor> sharedDropdownTests { "MyPlugin" };
// The editor must expose getOversamplingBox().

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/SongsmithDropdown.h"

template <class Processor, class Editor>
class SongsmithDropdownTests final : public juce::UnitTest
{
public:
    explicit SongsmithDropdownTests (const juce::String& plugin) : juce::UnitTest ("Shared Songsmith Dropdowns (songsmithstudio#15)", plugin) {}

    void runTest() override
    {
        using namespace Songsmith::UI::Dropdown;

        beginTest ("Oversampling reads the same in every plugin: Off, 2x, 4x, with the multiplication sign");
        {
            expectEquals (oversamplingText (0), juce::String ("Oversampling: off"));
            expectEquals (oversamplingText (1), "Oversampling: 2" + juce::String::charToString (0x00d7));
            expectEquals (oversamplingText (2), "Oversampling: 4" + juce::String::charToString (0x00d7));
            Processor p;
            std::unique_ptr<juce::AudioProcessorEditor> owner (p.createEditor());
            auto* ed = dynamic_cast<Editor*> (owner.get());
            expect (ed != nullptr);
            if (ed == nullptr)
                return;
            auto& box = ed->getOversamplingBox();
            expectEquals (box.getNumItems(), 3);
            for (int i = 0; i < 3; ++i)
                expectEquals (box.getItemText (i), oversamplingText (i), "item " + juce::String (i));
        }

        beginTest ("The Oversampling dropdown is as wide as its longest item and its chevron need, not wider, at every size");
        for (double scale : { 0.75, 1.0, 2.0 })
        {
            Processor p;
            std::unique_ptr<juce::AudioProcessorEditor> owner (p.createEditor());
            auto* ed = dynamic_cast<Editor*> (owner.get());
            if (ed == nullptr)
                return;
            const int width = juce::roundToInt ((double) ed->getWidth() * scale);
            ed->setSize (width, juce::roundToInt ((double) width * (double) ed->getHeight() / (double) juce::jmax (1, ed->getWidth())));
            auto& box = ed->getOversamplingBox();
            const auto font = box.getLookAndFeel().getComboBoxFont (box);
            int widest = 0;
            for (int i = 0; i < box.getNumItems(); ++i)
                widest = juce::jmax (widest, textWidth (box.getItemText (i), font));
            const auto where = "at " + juce::String (width) + " px: ";
            expectGreaterOrEqual (box.getWidth(), widest + 24, where + "room for the longest item and the chevron");
            expectLessOrEqual (box.getWidth(), widest + chevronAndPadding + 16, where + "no wasted space (width " + juce::String (box.getWidth()) + ", text " + juce::String (widest) + ")");
        }
    }

};
