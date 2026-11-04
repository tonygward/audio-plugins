#pragma once

// Shared checks for the Songsmith UI library, compiled into every plugin's test executable:
//   static SongsmithUITests sharedUITests { "EQ" };     // once per executable, in its test category
//   Songsmith::UI::Testing::checkEditorFonts (*this, *editor, "context");   // in an editor test

#include "SongsmithLookAndFeel.h"

namespace Songsmith::UI::Testing
{
    // Walks the editor's component tree: every label, button and combo box gets its font from the
    // embedded typeface, and none is smaller than the type scale allows (12 px for labels, 13 px
    // for buttons and combo boxes; only grid and scale labels, drawn in paint(), use 11 px).
    inline void checkEditorFonts (juce::UnitTest& t, juce::Component& editor, const juce::String& context)
    {
        auto& lf = editor.getLookAndFeel();
        int checked = 0;

        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            auto check = [&] (const juce::Font& f, float minimum, const juce::String& what)
            {
                ++checked;
                t.expectGreaterOrEqual (f.getHeight(), minimum, context + ": " + what + " font height");
                t.expectGreaterOrEqual (f.getHeight(), Type::smallest, context + ": " + what + " is above the smallest role");
                t.expect (isEmbedded (f.getTypefacePtr()), context + ": " + what + " uses the embedded Inter typeface");
            };

            if (auto* l = dynamic_cast<juce::Label*> (&c); l != nullptr && l->getText().isNotEmpty())
                check (lf.getLabelFont (*l), Type::hint, "label \"" + l->getText() + "\"");
            if (auto* box = dynamic_cast<juce::ComboBox*> (&c))
                check (lf.getComboBoxFont (*box), Type::control, "combo box \"" + box->getText() + "\"");
            if (auto* b = dynamic_cast<juce::TextButton*> (&c))
                check (lf.getTextButtonFont (*b, b->getHeight()), Type::control, "button \"" + b->getButtonText() + "\"");

            for (auto* child : c.getChildren())
                walk (*child);
        };
        walk (editor);
        t.expectGreaterThan (checked, 0, context + ": found labels, buttons or combo boxes");

        for (auto f : { lf.getPopupMenuFont(), lf.getAlertWindowMessageFont(), lf.getAlertWindowTitleFont() })
        {
            t.expectGreaterOrEqual (f.getHeight(), Type::control, context + ": menus and dialogs");
            t.expect (isEmbedded (f.getTypefacePtr()), context + ": menus and dialogs use the embedded typeface");
        }
    }
}

class SongsmithUITests final : public juce::UnitTest
{
public:
    explicit SongsmithUITests (const juce::String& category) : juce::UnitTest ("Shared Songsmith UI", category) {}

    void runTest() override
    {
        using namespace Songsmith::UI;

        beginTest ("The embedded Inter typeface loads once, in three weights");
        {
            const char* styles[] = { "Regular", "Medium", "SemiBold" };
            int i = 0;
            for (auto w : { Weight::Regular, Weight::Medium, Weight::SemiBold })
            {
                const auto tf = typeface (w);
                expect (tf != nullptr, "typeface loaded");
                if (tf == nullptr)
                    continue;
                expect (tf->getName().startsWith ("Inter"), "embedded Inter, got " + tf->getName());
                expectEquals (tf->getStyle(), juce::String (styles[i++]), "weight");
                expect (typeface (w) == tf, "loaded once and shared");
            }
            expect (typeface (Weight::Regular) != typeface (Weight::SemiBold), "weights are different faces");
        }

        beginTest ("font() and numbers() use the embedded typeface, not a system fallback");
        {
            for (auto w : { Weight::Regular, Weight::Medium, Weight::SemiBold })
                for (float h : { Type::caption, Type::hint, Type::control, Type::heading, Type::title })
                {
                    const juce::Font f (font (h, w)), n (numbers (h, w));
                    expect (f.getTypefacePtr() == typeface (w), "font() returns the embedded face");
                    expect (n.getTypefacePtr() == typeface (w), "numbers() returns the embedded face");
                    expectWithinAbsoluteError (f.getHeight(), h, 0.001f);
                }

            // Tabular figures: every digit has the same advance.
            const juce::Font n (numbers (Type::control));
            expectWithinAbsoluteError (juce::GlyphArrangement::getStringWidth (n, "1111"),
                                       juce::GlyphArrangement::getStringWidth (n, "8888"), 0.01f, "tabular digits don't jitter");
        }

        beginTest ("The type scale goes up in order and keeps the minimum sizes");
        {
            expectEquals (Type::smallest, 11.0f, "grid and scale labels");
            expectEquals (Type::hint, 12.0f, "the smallest size for anything else");
            expect (Type::caption < Type::hint && Type::hint < Type::control && Type::control < Type::heading
                    && Type::heading < Type::title);
            expectEquals (Type::forControlHeight (22), Type::control);
            expectEquals (Type::forControlHeight (28), Type::heading);
        }

        beginTest ("The base look-and-feel serves the embedded typeface");
        {
            LookAndFeelBase lf;
            juce::Font plain (juce::FontOptions (13.0f)), bold (juce::FontOptions (13.0f, juce::Font::bold));
            expect (lf.getTypefaceForFont (plain) == typeface (Weight::Regular), "the default sans-serif face is Inter Regular");
            expect (lf.getTypefaceForFont (bold) == typeface (Weight::SemiBold), "bold is Inter SemiBold");

            juce::Label label ({}, "Text");
            label.setFont (juce::FontOptions (9.0f));   // a system font, too small
            const auto f = lf.getLabelFont (label);
            expect (isEmbedded (f.getTypefacePtr()));
            expectGreaterOrEqual (f.getHeight(), Type::hint, "never below the hint size");

            lf.setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
            const auto tip = lf.getTooltipBounds ("Sets how fast the gain comes back.", { 100, 100 }, { 0, 0, 1000, 1000 });
            expect (tip.getHeight() >= (int) Type::control, "tooltips are laid out in the embedded font");
            expect (lf.getPopupMenuFont().getTypefacePtr() == typeface (Weight::Regular));
        }

        beginTest ("The title splits brand and product and measures both");
        {
            const auto parts = splitTitle ("Songsmith Analog Compressor");
            expectEquals (parts.brand, juce::String ("Songsmith"));
            expectEquals (parts.product, juce::String ("Analog Compressor"));
            expect (splitTitle ("Other").brand.isEmpty());

            const float w = titleWidth ("Songsmith EQ");
            const float brand = juce::GlyphArrangement::getStringWidth (juce::Font (font (Type::title, Weight::SemiBold)), "Songsmith");
            expectGreaterThan (w, brand + 10.0f, "brand, space and product");

            juce::Image image (juce::Image::ARGB, 300, 52, true, juce::SoftwareImageType());
            {
                juce::Graphics g (image);   // flushed when it goes out of scope
                drawTitle (g, { titleInset, 0.0f, 270.0f, titleBarHeight }, "Songsmith EQ", juce::Colours::white, juce::Colours::yellow);
            }
            bool inked = false;
            for (int x = 0; x < image.getWidth() && ! inked; ++x)
                for (int y = 0; y < image.getHeight() && ! inked; ++y)
                    inked = image.getPixelAt (x, y).getAlpha() > 0;
            expect (inked, "the title draws");
        }
    }
};
