#include "GuidanceView.h"
#include "../../shared/Source/SongsmithType.h"
#include "../../shared/Source/SongsmithColours.h"

namespace Songsmith::Analyzer
{
    namespace
    {
        constexpr int gapAfterHeading = 4, gapAfterParagraph = 6, gapBeforeHeading = 12;

        int wrappedHeight (const juce::Label& label, int width)
        {
            juce::AttributedString text;
            text.setJustification (juce::Justification::topLeft);
            text.append (label.getText(), label.getFont(), label.findColour (juce::Label::textColourId));
            juce::TextLayout layout;
            layout.createLayout (text, (float) width);
            return (int) std::ceil (layout.getHeight()) + 2;
        }
    }

    GuidanceView::GuidanceView() { setInterceptsMouseClicks (false, true); }

    void GuidanceView::clear()
    {
        blocks.clear();
        removeAllChildren();
        setSize (getWidth(), 0);
    }

    void GuidanceView::addBlock (const juce::String& text, bool heading, bool summary)
    {
        Block b;
        b.heading = heading;
        b.label = std::make_unique<juce::Label> ("guidance", text);
        auto& l = *b.label;
        l.setFont (UI::font (heading ? UI::Type::heading : UI::Type::control, heading || summary ? UI::Weight::SemiBold : UI::Weight::Regular));
        l.setColour (juce::Label::textColourId, heading ? UI::Colours::text : (summary ? UI::Colours::text : UI::Colours::textDim));
        l.setJustificationType (juce::Justification::topLeft);
        l.setBorderSize ({ 0, 0, 0, 0 });
        l.setMinimumHorizontalScale (1.0f);   // wrap, never squeeze
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
        blocks.push_back (std::move (b));
    }

    void GuidanceView::setAdvice (const FileAdvice::Advice& a, const juce::StringArray& extra)
    {
        blocks.clear();
        removeAllChildren();

        if (a.summary.isNotEmpty())
            addBlock (a.summary, false, true);

        if (! extra.isEmpty())
        {
            addBlock ("Also measured", true);
            for (auto& line : extra)
                addBlock (line, false);
        }

        for (auto kind : { FileAdvice::Kind::noChangeNecessary, FileAdvice::Kind::tryIfHearingAProblem, FileAdvice::Kind::forNextRecording })
        {
            bool any = false;
            for (auto& step : a.steps)
                if (step.kind == kind)
                {
                    if (! any)
                        addBlock (FileAdvice::heading (kind), true);
                    addBlock (step.text, false);
                    any = true;
                }
        }

        if (! a.cannotKnow.isEmpty())
        {
            addBlock ("What this tool cannot know", true);
            for (auto& line : a.cannotKnow)
                addBlock (line, false);
        }
        resized();
    }

    int GuidanceView::layoutBlocks (int width, bool apply)
    {
        int y = 0;
        bool first = true;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            auto& b = blocks[i];
            if (b.heading && ! first)
                y += gapBeforeHeading;
            const int h = wrappedHeight (*b.label, width);
            if (apply)
                b.label->setBounds (0, y, width, h);
            y += h + (b.heading ? gapAfterHeading : gapAfterParagraph);
            first = false;
        }
        return juce::jmax (0, y - gapAfterParagraph);
    }

    int GuidanceView::getHeightForWidth (int width) { return layoutBlocks (width, false); }

    void GuidanceView::resized() { layoutBlocks (getWidth(), true); }

    juce::StringArray GuidanceView::getAllText() const
    {
        juce::StringArray out;
        for (auto& b : blocks)
            out.add (b.label->getText());
        return out;
    }

    //==============================================================================
    GuidanceViewport::GuidanceViewport()
    {
        setViewedComponent (&view, false);
        setScrollBarsShown (true, false);
        setScrollBarThickness (8);
        setTitle ("Guidance");
        setDescription ("Plain-English advice about this file. Scroll to read more.");
    }

    void GuidanceViewport::setAdvice (const FileAdvice::Advice& advice, const juce::StringArray& extra)
    {
        view.setAdvice (advice, extra);
        resized();
        setViewPosition (0, 0);
    }

    void GuidanceViewport::resized()
    {
        // The column is as wide as the area, less the scroll bar when the text is taller than the area.
        int width = getWidth();
        int height = view.getHeightForWidth (width);
        if (height > getHeight())
        {
            width = getWidth() - getScrollBarThickness() - 6;
            height = view.getHeightForWidth (width);
        }
        view.setBounds (0, 0, width, juce::jmax (height, 1));
    }
}
