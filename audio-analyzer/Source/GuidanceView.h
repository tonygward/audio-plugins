#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../shared/Source/SongsmithFileAdvice.h"

// REC-02: the plain-English advice for one file, as a column of headings and paragraphs inside a scrolling area.
// Every piece of text is a label, so the wording and text-fit checks see exactly what the musician reads.
namespace Songsmith::Analyzer
{
    class GuidanceView : public juce::Component
    {
    public:
        GuidanceView();

        // Rebuilds the column for this advice. 'extra' is the measured facts the reading boxes do not already show.
        void setAdvice (const FileAdvice::Advice& advice, const juce::StringArray& extra);
        void clear();

        // The height the column needs at this width (design units).
        int getHeightForWidth (int width);

        void resized() override;

        // For tests: every line of text shown, in order.
        juce::StringArray getAllText() const;

    private:
        struct Block { std::unique_ptr<juce::Label> label; bool heading = false; };
        void addBlock (const juce::String& text, bool heading, bool summary = false);
        int layoutBlocks (int width, bool apply);

        std::vector<Block> blocks;
    };

    // A scrolling area for the guidance, with a plain scroll bar.
    class GuidanceViewport : public juce::Viewport
    {
    public:
        GuidanceViewport();
        GuidanceView& getView() noexcept { return view; }
        void resized() override;
        void setAdvice (const FileAdvice::Advice& advice, const juce::StringArray& extra);

    private:
        GuidanceView view;
    };
}
