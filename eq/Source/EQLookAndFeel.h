#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithLookAndFeel.h"
#include "FrequencyGuide.h"

namespace EQColours
{
    const juce::Colour background   { 0xff15171c };
    const juce::Colour plotTop      { 0xff1d2027 };
    const juce::Colour plotBottom   { 0xff121419 };
    // The shared Songsmith neutrals (shared/Source/SongsmithColours.h).
    inline const juce::Colour panel     = Songsmith::UI::Colours::panelLight;
    inline const juce::Colour panelEdge = Songsmith::UI::Colours::edge;
    const juce::Colour gridMinor    { 0x14ffffff };
    const juce::Colour gridMajor    { 0x26ffffff };
    const juce::Colour zeroLine     { 0x50ffffff };
    inline const juce::Colour label     = Songsmith::UI::Colours::textDim;
    inline const juce::Colour text      = Songsmith::UI::Colours::text;
    const juce::Colour totalCurve   { 0xfff2c94c };
    const juce::Colour accent       { 0xfff2c94c };   // Songsmith yellow: the title, lit buttons, knob pointers

    // Stable colour identity per band slot: the original three, then golden-ratio hues.
    inline juce::Colour band (int index)
    {
        static const juce::Colour first[] = {
            juce::Colour (0xffff6b6b),   // 1 coral
            juce::Colour (0xffb57bff),   // 2 violet
            juce::Colour (0xff3ddbc4),   // 3 teal
            juce::Colour (0xff5aa9ff),   // 4 blue
            juce::Colour (0xffff9f43),   // 5 orange
            juce::Colour (0xff7bed6f),   // 6 green
            juce::Colour (0xffff6bcb),   // 7 pink
            juce::Colour (0xffa3a8ff),   // 8 periwinkle
        };

        if (index >= 0 && index < 8)
            return first[index];

        const float hue = std::fmod (0.07f + (float) index * 0.618034f, 1.0f);
        return juce::Colour::fromHSV (hue, 0.55f, 0.98f, 1.0f);
    }
    // --- Text ------------------------------------------------------------------------------
    // The shared Songsmith typeface and type scale (shared/Source/SongsmithType.h).
    using Songsmith::UI::Weight;
    using Songsmith::UI::typeface;
    using Songsmith::UI::font;
    using Songsmith::UI::numbers;
    namespace Type = Songsmith::UI::Type;
}

// Fonts, buttons, knobs, combo boxes and the bar sliders come from the shared
// Songsmith::UI::LookAndFeelBase. Band controls take their band's colour as their accent
// (BandPanel); the graph, nodes and meter are drawn by EQDisplay and OutputMeterDisplay.
class EQLookAndFeel : public Songsmith::UI::LookAndFeelBase
{

    // EQ-38: instrument-specific, scalable line drawings. No font glyphs, files or DSP dependencies.
    static void drawGuideIcon (juce::Graphics& g, int index, juce::Rectangle<float> bounds)
    {
        if (index < 0 || index >= EQ::Guide::numInstruments())
            return;
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (bounds.getWidth() / 20.0f, bounds.getHeight() / 20.0f)
                            .translated (bounds.getX() * 20.0f / bounds.getWidth(),
                                         bounds.getY() * 20.0f / bounds.getHeight()));
        auto stroke = [&] (float x1, float y1, float x2, float y2)
        {
            g.drawLine (x1, y1, x2, y2, 1.4f);
        };
        if (index <= 3)  // Steel, nylon, electric and bass guitars
        {
            juce::Path body;
            if (index <= 1)
            {
                body.addEllipse (3.0f, 9.0f, 8.0f, 9.0f);
                body.addEllipse (5.5f, 6.8f, 5.0f, 6.0f);
            }
            else
            {
                body.startNewSubPath (3.0f, 16.5f);
                body.lineTo (3.0f, 9.0f);
                body.lineTo (7.5f, 11.0f);
                body.lineTo (10.0f, 7.0f);
                body.lineTo (12.5f, 13.5f);
                body.lineTo (9.0f, 18.0f);
                body.closeSubPath();
            }
            g.strokePath (body, juce::PathStrokeType (1.2f));
            stroke (9.0f, 8.0f, 14.5f, 2.0f);
            stroke (14.5f, 2.0f, 17.0f, 3.0f);
            stroke (15.0f, 2.0f, 17.5f, 4.0f);
            if (index <= 1)
            {
                g.drawEllipse (6.7f, 10.5f, 2.7f, 2.7f, 1.0f);
                stroke (5.0f, 15.0f, 9.0f, 15.0f);
                if (index == 1) // Classical: wider bridge and tie block
                    stroke (4.0f, 16.6f, 10.0f, 16.6f);
            }
            else
            {
                stroke (5.0f, 14.5f, 9.5f, 14.5f);
                if (index == 3) // Bass: long string path and two pickups
                    stroke (6.0f, 12.0f, 10.0f, 12.0f);
                else
                    g.fillEllipse (5.0f, 9.5f, 1.8f, 1.8f);
            }
        }
        else if (index == 4) // Vocal: microphone and stand
        {
            g.drawRoundedRectangle (7.0f, 2.0f, 6.0f, 10.0f, 3.0f, 1.4f);
            juce::Path cradle;
            cradle.addCentredArc (10.0f, 10.0f, 5.2f, 5.0f, 0.0f,
                                  juce::MathConstants<float>::pi,
                                  juce::MathConstants<float>::twoPi, true);
            g.strokePath (cradle, juce::PathStrokeType (1.4f));
            stroke (10.0f, 14.0f, 10.0f, 18.0f);
            stroke (6.0f, 18.0f, 14.0f, 18.0f);
        }
        else if (index == 5) // Kick: large drum with pedal
        {
            g.drawEllipse (3.0f, 3.0f, 14.0f, 14.0f, 1.5f);
            g.drawEllipse (6.0f, 6.0f, 8.0f, 8.0f, 1.0f);
            stroke (13.0f, 17.0f, 18.0f, 19.0f);
        }
        else if (index == 6) // Snare: shallow drum and sticks
        {
            g.drawEllipse (3.0f, 8.0f, 14.0f, 4.0f, 1.4f);
            stroke (3.0f, 10.0f, 3.0f, 15.0f);
            stroke (17.0f, 10.0f, 17.0f, 15.0f);
            stroke (3.0f, 15.0f, 17.0f, 15.0f);
            stroke (3.0f, 3.0f, 10.0f, 8.0f);
            stroke (17.0f, 3.0f, 10.0f, 8.0f);
        }
        else if (index == 7) // Toms: two drum heads
        {
            g.drawEllipse (1.5f, 5.0f, 8.0f, 5.0f, 1.3f);
            g.drawEllipse (10.5f, 5.0f, 8.0f, 5.0f, 1.3f);
            stroke (1.5f, 7.5f, 1.5f, 14.0f);
            stroke (9.5f, 7.5f, 9.5f, 14.0f);
            stroke (10.5f, 7.5f, 10.5f, 14.0f);
            stroke (18.5f, 7.5f, 18.5f, 14.0f);
            stroke (1.5f, 14.0f, 9.5f, 14.0f);
            stroke (10.5f, 14.0f, 18.5f, 14.0f);
        }
        else if (index == 8) // Drum bus: three channels joined to a mix bus
        {
            for (int y : { 4, 10, 16 })
            {
                g.drawEllipse (2.0f, (float) y - 1.5f, 3.0f, 3.0f, 1.2f);
                stroke (5.0f, (float) y, 12.0f, (float) y);
            }
            stroke (12.0f, 4.0f, 12.0f, 16.0f);
            stroke (12.0f, 10.0f, 17.0f, 10.0f);
        }
        else // Any instrument: recognisable universal waveform
        {
            juce::Path wave;
            wave.startNewSubPath (1.5f, 10.0f);
            wave.lineTo (4.0f, 10.0f);
            wave.lineTo (6.0f, 5.0f);
            wave.lineTo (8.5f, 15.0f);
            wave.lineTo (11.5f, 3.0f);
            wave.lineTo (14.0f, 13.0f);
            wave.lineTo (16.0f, 10.0f);
            wave.lineTo (18.5f, 10.0f);
            g.strokePath (wave, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    static constexpr int guideIconColumn = 24;

    static int guideIndexForName (const juce::String& name)
    {
        for (int i = 0; i < EQ::Guide::numInstruments(); ++i)
            if (name == EQ::Guide::instrument (i).name)
                return i;
        return -1;
    }

public:
    void drawComboBox (juce::Graphics& g, int width, int height, bool down,
                       int bx, int by, int bw, int bh, juce::ComboBox& box) override
    {
        LookAndFeelBase::drawComboBox (g, width, height, down, bx, by, bw, bh, box);
        if (box.getName() == "frequencyGuideInstrument")
        {
            g.setColour (box.findColour (juce::ComboBox::textColourId)
                            .withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
            drawGuideIcon (g, box.getSelectedItemIndex(), { 7.0f, ((float) height - 17.0f) * 0.5f, 17.0f, 17.0f });
        }
    }

    void getIdealPopupMenuItemSize (const juce::String& text, bool separator, int standardHeight,
                                    int& idealWidth, int& idealHeight) override
    {
        LookAndFeelBase::getIdealPopupMenuItemSize (text, separator, standardHeight, idealWidth, idealHeight);
        if (! separator && guideIndexForName (text) >= 0)
            idealWidth += guideIconColumn;
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool separator, bool active, bool highlighted, bool ticked,
                            bool submenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        const int index = separator ? -1 : guideIndexForName (text);
        // Keep the normal JUCE menu semantics, checkmarks and keyboard handling; guide items
        // just get a column at the left for their icon.
        LookAndFeelBase::drawPopupMenuItem (g, index >= 0 ? area.withTrimmedLeft (guideIconColumn) : area,
                                            separator, active, highlighted, ticked, submenu, text,
                                            shortcut, icon, textColour);
        if (index >= 0)
        {
            g.setColour (findColour (juce::PopupMenu::textColourId)
                            .withMultipliedAlpha (active ? 0.9f : 0.4f));
            drawGuideIcon (g, index, { (float) area.getX() + 6.0f,
                                       (float) area.getCentreY() - 8.0f, 16.0f, 16.0f });
        }
    }

    EQLookAndFeel() : LookAndFeelBase (EQColours::accent)
    {
        setColour (juce::Label::textColourId, EQColours::label);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
};
