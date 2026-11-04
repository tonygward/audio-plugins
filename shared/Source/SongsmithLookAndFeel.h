#pragma once

#include "SongsmithType.h"
#include "SongsmithColours.h"

namespace Songsmith::UI
{
    // The base look-and-feel of every Songsmith editor (see docs/style-guide.md):
    //  - text: the embedded typeface and the type scale for labels, buttons, combo boxes, popup
    //    menus, tooltips, slider pop-ups and alert windows;
    //  - controls: the slightly bevelled look, lit from the top left. Buttons are raised when off
    //    and pushed in (filled with the accent) when on; knobs have a lit cap in a recessed ring;
    //    combo boxes, value fields and slider tracks are recessed wells;
    //  - colours: the shared neutral tokens (Songsmith::UI::Colours) plus one accent per plugin.
    // Each plugin's look-and-feel derives from it and adds only its special drawing (meters, the
    // EQ graph, the compressor's printed knob scale). Everything is drawn in the editor's design
    // units, so it scales with the interface size and stays crisp at 2x.
    class LookAndFeelBase : public juce::LookAndFeel_V4
    {
    public:
        // Component properties that change how a control is drawn.
        static constexpr const char* wellProperty = "songsmithWell";            // a TextButton drawn as a recessed field (the EQ's preset name)
        static constexpr const char* valueArcProperty = "songsmithValueArc";    // false: a knob without a value arc (the compressor's)

        static constexpr float cornerRadius = 4.0f;

        explicit LookAndFeelBase (juce::Colour accentColour = Colours::text) : accent (accentColour)
        {
            setDefaultSansSerifTypeface (typeface (Weight::Regular));

            setColour (juce::ResizableWindow::backgroundColourId, Colours::panel);
            setColour (juce::Label::textColourId, Colours::textDim);

            setColour (juce::TextButton::buttonColourId, Colours::panelLight);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, Colours::text);
            setColour (juce::TextButton::textColourOnId, Colours::recess);

            setColour (juce::ComboBox::backgroundColourId, Colours::recess);
            setColour (juce::ComboBox::textColourId, Colours::text);
            setColour (juce::ComboBox::outlineColourId, Colours::edge);
            setColour (juce::ComboBox::focusedOutlineColourId, Colours::edge);
            setColour (juce::ComboBox::arrowColourId, Colours::textDim);
            setColour (juce::ComboBox::buttonColourId, Colours::panelLight);

            setColour (juce::PopupMenu::backgroundColourId, Colours::panelLight);
            setColour (juce::PopupMenu::textColourId, Colours::text);
            setColour (juce::PopupMenu::headerTextColourId, Colours::textDim);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
            setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);

            setColour (juce::Slider::rotarySliderFillColourId, accent);
            setColour (juce::Slider::rotarySliderOutlineColourId, Colours::recess);
            setColour (juce::Slider::trackColourId, accent);
            setColour (juce::Slider::thumbColourId, accent);
            setColour (juce::Slider::backgroundColourId, Colours::recess);
            setColour (juce::Slider::textBoxTextColourId, Colours::text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));

            setColour (juce::TextEditor::backgroundColourId, Colours::recess);
            setColour (juce::TextEditor::textColourId, Colours::text);
            setColour (juce::TextEditor::outlineColourId, Colours::edge);
            setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
            setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
            setColour (juce::CaretComponent::caretColourId, Colours::text);

            setColour (juce::AlertWindow::backgroundColourId, Colours::panelLight);
            setColour (juce::AlertWindow::textColourId, Colours::text);
            setColour (juce::AlertWindow::outlineColourId, Colours::edge);

            setColour (juce::TooltipWindow::backgroundColourId, Colours::recess.withAlpha (0.94f));
            setColour (juce::TooltipWindow::outlineColourId, Colours::edge);
            setColour (juce::TooltipWindow::textColourId, Colours::text);

            setColour (juce::ScrollBar::thumbColourId, Colours::textDim);
        }

        juce::Colour getAccent() const noexcept { return accent; }

        //==============================================================================
        // Surfaces. 'r' is the outline, normally the component bounds reduced by 0.5 so 1 px
        // edges land on whole pixels.

        // Raised (a button that is off): a little lighter at the top, a 1 px highlight on the top
        // inner edge, a 1 px darker edge at the bottom.
        static void drawRaised (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float radius = cornerRadius)
        {
            g.setGradientFill (juce::ColourGradient (base.brighter (0.12f), 0.0f, r.getY(), base.darker (0.14f), 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, radius);
            edgeLine (g, r, radius, true, Colours::highlight);
            edgeLine (g, r, radius, false, Colours::shadow.withMultipliedAlpha (0.55f));
            g.setColour (Colours::shadow);
            g.drawRoundedRectangle (r, radius, 1.0f);
        }

        // Pushed in (a button that is on or held down): the gradient reversed and darker at the
        // top, a shadow on the top inner edge, the highlight on the bottom edge.
        static void drawPressed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float radius = cornerRadius)
        {
            g.setGradientFill (juce::ColourGradient (base.darker (0.22f), 0.0f, r.getY(), base.brighter (0.04f), 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, radius);
            edgeLine (g, r, radius, true, Colours::shadow.withMultipliedAlpha (0.7f));
            edgeLine (g, r, radius, false, Colours::highlight);
            g.setColour (Colours::shadow);
            g.drawRoundedRectangle (r, radius, 1.0f);
        }

        // Recessed well (combo boxes, value fields, tracks): darker than the panel, a shadow on the
        // top edge and a highlight on the bottom edge.
        static void drawWell (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill = Colours::recess, float radius = cornerRadius)
        {
            g.setGradientFill (juce::ColourGradient (fill.darker (0.35f), 0.0f, r.getY(), fill, 0.0f, r.getY() + juce::jmin (r.getHeight(), 10.0f), false));
            g.fillRoundedRectangle (r, radius);
            g.setColour (Colours::shadow.withMultipliedAlpha (0.8f));
            g.drawRoundedRectangle (r, radius, 1.0f);
            edgeLine (g, r.expanded (0.0f, 1.0f), radius, false, Colours::highlight.withMultipliedAlpha (0.8f));
        }

        // The panel behind the controls: lighter at the top, darker at the bottom.
        static void drawPanel (juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.setGradientFill (juce::ColourGradient (Colours::panelLight, 0.0f, area.getY(), Colours::panel, 0.0f, area.getBottom(), false));
            g.fillRect (area);
        }

        // The darker title bar across the top of the panel.
        static void drawTitleBar (juce::Graphics& g, juce::Rectangle<float> bar)
        {
            g.setColour (Colours::recess.withAlpha (0.6f));
            g.fillRect (bar);
        }

        //==============================================================================
        // Buttons.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& background,
                                   bool over, bool down) override
        {
            const auto r = b.getLocalBounds().toFloat().reduced (0.5f);

            if (b.getProperties().getWithDefault (wellProperty, false))
            {
                drawWell (g, r, over && b.isEnabled() ? Colours::recess.brighter (0.08f) : Colours::recess);
                return;
            }

            if (! b.isEnabled())   // dimmed and flat
            {
                g.setColour (background.withMultipliedAlpha (0.45f));
                g.fillRoundedRectangle (r, cornerRadius);
                g.setColour (Colours::shadow.withMultipliedAlpha (0.6f));
                g.drawRoundedRectangle (r, cornerRadius, 1.0f);
                return;
            }

            const auto base = over ? background.brighter (0.08f) : background;
            if (b.getToggleState() || down)
                drawPressed (g, r, b.getToggleState() ? base : base.darker (0.1f));
            else
                drawRaised (g, r, base);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            const auto f = getTextButtonFont (b, b.getHeight());
            g.setFont (f);
            const auto colour = b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);
            g.setColour (colour.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));

            const bool well = b.getProperties().getWithDefault (wellProperty, false);
            // A tall button wraps a long name onto a second line rather than squashing it.
            const auto area = b.getLocalBounds().reduced (well ? 8 : 4, 1);
            g.drawFittedText (b.getButtonText(), area, well ? juce::Justification::centredLeft : juce::Justification::centred,
                              juce::jmax (1, (int) ((float) area.getHeight() / f.getHeight())), 1.0f);
        }

        //==============================================================================
        // Combo boxes: a recessed well with a chevron.
        void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
        {
            const auto r = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
            drawWell (g, r, box.findColour (juce::ComboBox::backgroundColourId));

            const auto arrow = juce::Rectangle<float> ((float) width - 26.0f, 0.0f, 18.0f, (float) height).withSizeKeepingCentre (9.0f, 5.0f);
            juce::Path chevron;
            chevron.startNewSubPath (arrow.getX(), arrow.getY());
            chevron.lineTo (arrow.getCentreX(), arrow.getBottom());
            chevron.lineTo (arrow.getRight(), arrow.getY());
            g.setColour (box.findColour (juce::ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
            g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (4, 1, box.getWidth() - 30, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        //==============================================================================
        // Rotary knobs: a recessed ring, a dark cap lit from the top left with a thin dark
        // outline, and a pointer in the knob's accent colour (Slider::rotarySliderFillColourId).
        // Unless valueArcProperty is false, the value shows as an arc in the ring (from the
        // centre for ranges that cross zero).
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const bool arc = s.getProperties().getWithDefault (valueArcProperty, true);
            const auto knobAccent = s.findColour (juce::Slider::rotarySliderFillColourId);

            // The recessed ring: in shadow at the top left, catching light at the bottom right.
            const auto ring = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
            g.setGradientFill (juce::ColourGradient (Colours::recess.darker (0.3f), c.x - radius, c.y - radius,
                                                     Colours::recess.brighter (0.25f), c.x + radius, c.y + radius, false));
            g.fillEllipse (ring);

            const float capR = radius * (arc ? 0.74f : 0.86f);
            if (arc)
            {
                const float arcR = (radius + capR) * 0.5f, arcW = juce::jmax (2.0f, (radius - capR) * 0.55f);
                const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
                const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
                if (std::abs (angle - from) > 0.001f)
                {
                    juce::Path value;
                    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
                    g.setColour (s.isEnabled() ? knobAccent : knobAccent.withSaturation (0.1f).withMultipliedAlpha (0.6f));
                    g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }

            // The cap.
            const auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
            g.setGradientFill (juce::ColourGradient (Colours::panelLight.brighter (0.25f), c.x - capR * 0.6f, c.y - capR * 0.7f,
                                                     Colours::panel.darker (0.4f), c.x + capR * 0.6f, c.y + capR * 0.8f, true));
            g.fillEllipse (cap);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (cap, 1.0f);

            // The pointer.
            g.setColour (s.isEnabled() ? knobAccent : Colours::textDim);
            g.drawLine ({ c.getPointOnCircumference (capR * 0.35f, angle), c.getPointOnCircumference (capR * 0.9f, angle) },
                        juce::jmax (2.0f, radius * 0.07f));
        }

        //==============================================================================
        // Linear sliders. Bar sliders (the EQ's output gain and scale) are recessed fields that
        // fill with the value (from zero for ranges that cross it). Faders get a recessed track,
        // filled with Slider::trackColourId (vertical faders fill from the top, like the L1's), and
        // a raised handle with the same bevel as a button.
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle style, juce::Slider& slider) override
        {
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
            const auto fill = slider.findColour (juce::Slider::trackColourId);
            const float alpha = slider.isEnabled() ? 1.0f : 0.35f;

            if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
            {
                const auto r = bounds.reduced (0.5f);
                drawWell (g, r);
                const bool vertical = style == juce::Slider::LinearBarVertical;
                const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
                const float zero = bipolar ? (float) slider.getPositionOfValue (0.0) : (vertical ? r.getBottom() : r.getX());
                const float from = juce::jmin (zero, sliderPos), to = juce::jmax (zero, sliderPos);
                if (to - from > 0.5f)
                {
                    g.setColour (fill.withAlpha (0.55f * alpha));
                    g.fillRect (vertical ? juce::Rectangle<float> (r.getX() + 1.0f, from, r.getWidth() - 2.0f, to - from)
                                         : juce::Rectangle<float> (from, r.getY() + 1.0f, to - from, r.getHeight() - 2.0f));
                }
                return;
            }

            if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearHorizontal)
            {
                LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
                return;
            }

            const bool vertical = style == juce::Slider::LinearVertical;
            constexpr float trackW = 6.0f;
            const auto track = vertical ? juce::Rectangle<float> (bounds.getCentreX() - trackW * 0.5f, bounds.getY(), trackW, bounds.getHeight())
                                        : juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - trackW * 0.5f, bounds.getWidth(), trackW);
            drawWell (g, track, Colours::recess, trackW * 0.5f);

            g.setColour (fill.withMultipliedAlpha (0.9f * alpha));
            g.fillRoundedRectangle (vertical ? track.withBottom (sliderPos).reduced (1.0f, 0.0f)
                                             : track.withRight (sliderPos).reduced (0.0f, 1.0f), (trackW - 2.0f) * 0.5f);

            const auto handle = vertical ? juce::Rectangle<float> (juce::jmin (bounds.getWidth(), 34.0f), 12.0f).withCentre ({ bounds.getCentreX(), sliderPos })
                                         : juce::Rectangle<float> (12.0f, juce::jmin (bounds.getHeight(), 24.0f)).withCentre ({ sliderPos, bounds.getCentreY() });
            drawRaised (g, handle.reduced (0.5f), slider.isEnabled() ? Colours::panelLight.brighter (0.15f) : Colours::panelLight, 3.0f);
            g.setColour (fill.withMultipliedAlpha (alpha));
            g.fillRect (vertical ? handle.withSizeKeepingCentre (handle.getWidth() - 10.0f, 2.0f)
                                 : handle.withSizeKeepingCentre (2.0f, handle.getHeight() - 10.0f));
        }

        // A slider's value box with an outline colour (the maximiser's fader readouts) is a value
        // field, so it's drawn as a recessed well. Other labels draw as JUCE draws them.
        void drawLabel (juce::Graphics& g, juce::Label& label) override
        {
            // A bar slider's text sits on the bar, which is already the well: text only.
            auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent());
            const bool bar = slider != nullptr && (slider->getSliderStyle() == juce::Slider::LinearBar
                                                   || slider->getSliderStyle() == juce::Slider::LinearBarVertical);
            const bool field = slider != nullptr && ! bar && slider->findColour (juce::Slider::textBoxOutlineColourId).getAlpha() > 0;
            if (label.isBeingEdited() || ! (bar || field))
            {
                LookAndFeel_V4::drawLabel (g, label);
                return;
            }

            if (field)
                drawWell (g, label.getLocalBounds().toFloat().reduced (0.5f), Colours::recess, 3.0f);
            const auto f = getLabelFont (label);
            g.setFont (f);
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
            const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), area, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) area.getHeight() / f.getHeight())), label.getMinimumHorizontalScale());
        }

        //==============================================================================
        // Text. Every font getter returns the embedded typeface explicitly. (A Font with the default
        // typeface name is resolved through the *default* LookAndFeel, which a plugin shouldn't
        // replace, so overriding getTypefaceForFont alone isn't enough.)
        juce::Font getLabelFont (juce::Label& label) override
        {
            // A slider's value box: tabular figures so the number doesn't jitter while dragging.
            if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
                return juce::Font (numbers (slider->getSliderStyle() == juce::Slider::LinearBar ? Type::heading : Type::control));

            const auto f = label.getFont();
            if (isEmbedded (f.getTypefacePtr()))
                return f;   // set with Songsmith::UI::font()

            return juce::Font (font (juce::jmax (Type::hint, f.getHeight()), f.isBold() ? Weight::SemiBold : Weight::Regular));
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (font (Type::forControlHeight (buttonHeight), Weight::Medium));
        }

        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return juce::Font (font (Type::forControlHeight (box.getHeight())));
        }

        juce::Font getPopupMenuFont() override                 { return juce::Font (font (Type::heading)); }
        juce::Font getSliderPopupFont (juce::Slider&) override { return juce::Font (numbers (Type::control)); }
        juce::Font getAlertWindowTitleFont() override          { return juce::Font (font (Type::title, Weight::SemiBold)); }
        juce::Font getAlertWindowMessageFont() override        { return juce::Font (font (Type::heading)); }
        juce::Font getAlertWindowFont() override               { return juce::Font (font (Type::control)); }

        juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
        {
            if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
                return typeface (f.isBold() ? Weight::SemiBold : Weight::Regular);
            return LookAndFeel_V4::getTypefaceForFont (f);
        }

        // Tooltips: JUCE's default uses a bold font at 15 px; use the type scale. Colours come from
        // TooltipWindow::backgroundColourId, outlineColourId and textColourId.
        static juce::TextLayout layoutTooltip (const juce::String& text, juce::Colour colour)
        {
            juce::AttributedString s;
            s.setWordWrap (juce::AttributedString::WordWrap::byWord);
            s.setJustification (juce::Justification::centredLeft);
            s.append (text, juce::Font (font (Type::control)), colour);

            juce::TextLayout tl;
            tl.createLayoutWithBalancedLineLengths (s, 420.0f);
            return tl;
        }

        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override
        {
            const auto tl = layoutTooltip (tipText, juce::Colours::black);
            const int w = (int) std::ceil (tl.getWidth() + 18.0f), h = (int) std::ceil (tl.getHeight() + 10.0f);

            return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                         screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6)  : screenPos.y + 6,
                                         w, h).constrainedWithin (parentArea);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
        {
            const auto bounds = juce::Rectangle<int> (width, height).toFloat();
            g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (findColour (juce::TooltipWindow::outlineColourId));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

            layoutTooltip (text, findColour (juce::TooltipWindow::textColourId)).draw (g, bounds.reduced (9.0f, 5.0f));
        }

    private:
        // A 1 px line just inside the top or bottom edge, between the corners.
        static void edgeLine (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool top, juce::Colour colour)
        {
            const float inset = juce::jmin (radius, r.getWidth() * 0.5f);
            g.setColour (colour);
            g.fillRect (juce::Rectangle<float> (r.getX() + inset, top ? r.getY() + 0.5f : r.getBottom() - 1.5f,
                                                juce::jmax (0.0f, r.getWidth() - inset * 2.0f), 1.0f));
        }

        juce::Colour accent;
    };
}
