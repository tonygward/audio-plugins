#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Renders one row per control in each state, drawn by the given look-and-feel exactly as the
// editor draws it: buttons off, hover, pressed, on and disabled; a combo box (normal and
// disabled); knobs; a bar slider; a vertical fader. Used for the style PRs' before/after pictures
// (EQTests --screenshots, AnalogCompressorTests --screenshots, MaximiserTests --screenshot).
namespace Songsmith::UI::Testing
{
    inline juce::Image renderControlSheet (juce::LookAndFeel& lf, juce::Colour background, float scale)
    {
        constexpr int w = 640, h = 300;
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), true,
                           juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale (scale));
            g.fillAll (background);

            auto paintAt = [&] (juce::Component& c, juce::Rectangle<int> bounds)
            {
                c.setLookAndFeel (&lf);
                c.setBounds (bounds);
            };

            // Buttons: off, hover, pressed, on, disabled.
            const char* names[] = { "Off", "Hover", "Pressed", "On", "Disabled" };
            for (int i = 0; i < 5; ++i)
            {
                juce::TextButton b (names[i]);
                b.setLookAndFeel (&lf);
                b.setBounds (16 + i * 122, 16, 110, 28);
                b.setToggleState (i == 3, juce::dontSendNotification);
                b.setEnabled (i != 4);
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (b.getPosition());
                lf.drawButtonBackground (g, b, b.findColour (b.getToggleState() ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId),
                                         i == 1, i == 2);
                lf.drawButtonText (g, b, i == 1, i == 2);
                b.setLookAndFeel (nullptr);
            }

            // Combo boxes: normal and disabled.
            for (int i = 0; i < 2; ++i)
            {
                juce::ComboBox box;
                box.addItem (i == 0 ? "Oversampling: off" : "Disabled", 1);
                box.setSelectedId (1, juce::dontSendNotification);
                box.setEnabled (i == 0);
                paintAt (box, { 16 + i * 200, 60, 180, 26 });
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (box.getPosition());
                box.paintEntireComponent (g, false);
                box.setLookAndFeel (nullptr);
            }

            // Knobs: unipolar, bipolar, disabled.
            for (int i = 0; i < 3; ++i)
            {
                juce::Slider s (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
                s.setRange (i == 1 ? -24.0 : 0.0, 24.0);
                s.setValue (i == 1 ? 9.0 : 15.0, juce::dontSendNotification);
                s.setEnabled (i != 2);
                paintAt (s, { 16 + i * 110, 100, 96, 96 });
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (s.getPosition());
                s.paintEntireComponent (g, false);
                s.setLookAndFeel (nullptr);
            }

            // A bar slider (the EQ's output gain) and a horizontal slider.
            {
                juce::Slider bar (juce::Slider::LinearBar, juce::Slider::NoTextBox);
                bar.setRange (-24.0, 24.0);
                bar.setValue (6.0, juce::dontSendNotification);
                paintAt (bar, { 360, 110, 160, 28 });
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (bar.getPosition());
                bar.paintEntireComponent (g, false);
                bar.setLookAndFeel (nullptr);
            }
            {
                juce::Slider hs (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
                hs.setRange (0.0, 1.0);
                hs.setValue (0.4, juce::dontSendNotification);
                paintAt (hs, { 360, 160, 160, 28 });
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (hs.getPosition());
                hs.paintEntireComponent (g, false);
                hs.setLookAndFeel (nullptr);
            }

            // A vertical fader (the maximiser's Threshold and Out Ceiling).
            {
                juce::Slider v (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
                v.setRange (-30.0, 0.0);
                v.setValue (-8.0, juce::dontSendNotification);
                paintAt (v, { 560, 60, 60, 220 });
                juce::Graphics::ScopedSaveState state (g);
                g.setOrigin (v.getPosition());
                v.paintEntireComponent (g, false);
                v.setLookAndFeel (nullptr);
            }
        }
        return image;
    }

    inline void writeControlSheet (juce::LookAndFeel& lf, juce::Colour background, float scale, const juce::File& file)
    {
        const auto image = renderControlSheet (lf, background, scale);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("%s\n", file.getFullPathName().toRawUTF8());
    }
}
