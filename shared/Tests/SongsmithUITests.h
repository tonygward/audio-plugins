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

    // Global Bypass (SUI-13): the shared red when engaged, in the top-right of the editor's content
    // (the same corner in every plugin), and it follows the real bypass parameter.
    inline juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (c); b != nullptr && b->getButtonText() == text)
                return b;
            if (auto* found = findButton (*c, text))
                return found;
        }
        return nullptr;
    }

    // Finds the editor's Bypass button itself; its parent is the editor's content, laid out at the design size.
    inline void checkGlobalBypass (juce::UnitTest& t, juce::Component& editor, juce::AudioProcessorParameter& parameter, const juce::String& context)
    {
        auto* found = findButton (editor, "Bypass");
        t.expect (found != nullptr, context + ": there is a Bypass button");
        if (found == nullptr)
            return;
        auto& bypass = *found;
        auto& content = *bypass.getParentComponent();

        t.expect (bypass.getButtonText() == "Bypass", context + ": the button is labelled Bypass");
        t.expect (bypass.findColour (juce::TextButton::buttonOnColourId) == Colours::bypass, context + ": engaged colour is the shared bypass token");
        t.expect (Colours::bypass == juce::Colour (0xffc9563c), context + ": the token is #C9563C");
        t.expect (bypass.findColour (juce::TextButton::buttonColourId) != Colours::bypass, context + ": not engaged is the neutral button");

        const auto bounds = bypass.getBounds();
        const auto inContent = bounds;
        t.expect (inContent.getRight() >= content.getWidth() - 24 && inContent.getRight() <= content.getWidth() - 12, context + ": at the right edge");
        t.expect (inContent.getY() < 50 && inContent.getBottom() <= 50, context + ": in the top bar");
        t.expect (bypass.getWidth() >= 70 && bypass.getHeight() >= 24, context + ": a comfortable click target");

        parameter.setValueNotifyingHost (1.0f);
        t.expect (bypass.getToggleState(), context + ": engaged follows the parameter");
        parameter.setValueNotifyingHost (0.0f);
        t.expect (! bypass.getToggleState(), context + ": released follows the parameter");
    }

    // Every button, slider and combo box in the editor is drawn by a look-and-feel derived from
    // the shared base, so no control slips back to a plain JUCE look.
    inline void checkControlsUseSharedLook (juce::UnitTest& t, juce::Component& editor, const juce::String& context)
    {
        int buttons = 0, sliders = 0, combos = 0;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            const bool isButton = dynamic_cast<juce::Button*> (&c) != nullptr, isSlider = dynamic_cast<juce::Slider*> (&c) != nullptr,
                       isCombo = dynamic_cast<juce::ComboBox*> (&c) != nullptr;
            if (isButton || isSlider || isCombo)
            {
                buttons += isButton ? 1 : 0; sliders += isSlider ? 1 : 0; combos += isCombo ? 1 : 0;
                const auto what = juce::String (isButton ? "button" : isSlider ? "slider" : "combo box") + " \"" + c.getName() + c.getTitle()
                                  + (isButton ? static_cast<juce::Button&> (c).getButtonText() : juce::String()) + "\"";
                t.expect (dynamic_cast<LookAndFeelBase*> (&c.getLookAndFeel()) != nullptr,
                          context + ": " + what + " uses the shared Songsmith look-and-feel");
            }
            for (auto* child : c.getChildren())
                walk (*child);
        };
        walk (editor);
        t.expectGreaterThan (buttons + sliders + combos, 0, context + ": found controls");
    }

    // Mean brightness of one pixel row across the middle half of an image.
    inline float rowBrightness (const juce::Image& image, int y)
    {
        float sum = 0.0f;
        int n = 0;
        for (int x = image.getWidth() / 4; x < image.getWidth() * 3 / 4; ++x, ++n)
            sum += image.getPixelAt (x, y).getBrightness();
        return n > 0 ? sum / (float) n : 0.0f;
    }

    // A button drawn by the look-and-feel, off or on, at 1x.
    inline juce::Image renderButton (juce::LookAndFeel& lf, bool on, juce::Colour background)
    {
        juce::TextButton b ("Button");
        b.setLookAndFeel (&lf);
        b.setBounds (0, 0, 120, 28);
        b.setToggleState (on, juce::dontSendNotification);
        juce::Image image (juce::Image::ARGB, 120, 28, true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.fillAll (background);
            lf.drawButtonBackground (g, b, b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId), false, false);
        }
        b.setLookAndFeel (nullptr);
        return image;
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

        beginTest ("Buttons are raised when off and pushed in when on");
        {
            LookAndFeelBase lf (juce::Colour (0xffe8a33d));
            const auto off = Testing::renderButton (lf, false, Colours::panel), on = Testing::renderButton (lf, true, Colours::panel);
            // Rows 2 and 25: just inside the top and bottom edges, below the outline.
            const float offTop = Testing::rowBrightness (off, 2), offBottom = Testing::rowBrightness (off, 25);
            const float onTop = Testing::rowBrightness (on, 2), onBottom = Testing::rowBrightness (on, 25);
            expectGreaterThan (offTop, offBottom + 0.01f, "off: lit from the top (top edge lighter than the bottom)");
            expectGreaterThan (onBottom, onTop + 0.01f, "on: pushed in (bottom edge lighter than the top)");
            expectGreaterThan (Testing::rowBrightness (on, 14), Testing::rowBrightness (off, 14), "on: filled with the accent");
        }

        beginTest ("Combo boxes are recessed wells, darker than the panel");
        {
            LookAndFeelBase lf;
            juce::ComboBox box;
            box.setLookAndFeel (&lf);
            box.setBounds (0, 0, 160, 26);
            juce::Image image (juce::Image::ARGB, 160, 26, true, juce::SoftwareImageType());
            {
                juce::Graphics g (image);
                g.fillAll (Colours::panel);
                lf.drawComboBox (g, 160, 26, false, 0, 0, 160, 26, box);
            }
            box.setLookAndFeel (nullptr);
            expectLessThan (Testing::rowBrightness (image, 13), Colours::panel.getBrightness(), "the well is darker than the panel");
            expectLessThan (Testing::rowBrightness (image, 2), Testing::rowBrightness (image, 24), "shadow at the top, light at the bottom");
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
