#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "FETLookAndFeel.h"

namespace FET::UI
{
    // Meter readings in VU (UG p. 3, UM p. 22). The digital reference is 0 VU = +4 dBm =
    // -18 dBFS RMS (as in docs/engine.md), so:
    //   GR mode: the needle shows -GR (0 VU = no gain reduction);
    //   +4 mode: 0 VU = +4 dBm output = -18 dBFS RMS;
    //   +8 mode: 0 VU = +8 dBm output = -14 dBFS RMS.
    // All Buttons makes "the meter go wild, often resting at maximum" (UM p. 9): in GR mode the
    // reading is exaggerated (x1.5, plus 3 dB). Tuned, see docs/all-buttons.md.
    enum class MeterMode { GR = 0, Plus4 = 1, Plus8 = 2 };

    inline constexpr float meterMinVu = -20.0f, meterMaxVu = 3.0f;

    inline float meterReadingVu (MeterMode mode, float grDb, float outputRmsDbfs, bool allButtons) noexcept
    {
        float vu = 0.0f;
        switch (mode)
        {
            case MeterMode::GR:    vu = allButtons && grDb > 0.05f ? -(grDb * 1.5f + 3.0f) : -grDb; break;
            case MeterMode::Plus4: vu = outputRmsDbfs + 18.0f; break;
            case MeterMode::Plus8: vu = outputRmsDbfs + 14.0f; break;
        }
        return juce::jlimit (meterMinVu - 2.0f, meterMaxVu + 1.0f, vu);
    }

    // Needle position 0..1 on a VU scale: linear in voltage, +3 VU at full scale.
    inline float needleFraction (float vu) noexcept
    {
        return std::pow (10.0f, (vu - meterMaxVu) / 20.0f);
    }

    // VU ballistics, independent of the audio: a second-order needle (mass + damping) driven by
    // the reading in needle-position units. Damping 0.82 gives the ~1 % overshoot of a VU meter,
    // and omega is set so a step reaches 99 % in 300 ms (IEC 60268-17). Tested.
    class VuBallistics
    {
    public:
        static constexpr float damping = 0.82f;
        static constexpr float omega = 13.8f;   // rad/s

        void reset (float position) noexcept { x = position; v = 0.0f; }

        float advance (float target, float seconds) noexcept
        {
            constexpr float step = 0.001f;
            for (float t = 0.0f; t < seconds; t += step)
            {
                const float dt = std::min (step, seconds - t);
                const float a = omega * omega * (target - x) - 2.0f * damping * omega * v;
                v += a * dt;
                x += v * dt;
            }
            return x;
        }

        float position() const noexcept { return x; }

    private:
        float x = 0.0f, v = 0.0f;
    };

    class NeedleMeter : public juce::Component
    {
    public:
        void setReading (float vu, float seconds)
        {
            ballistics.advance (needleFraction (vu), seconds);
            repaint();
        }

        void setModeLabel (const juce::String& s) { modeLabel = s; repaint(); }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            g.setColour (Colours::recess);
            g.fillRoundedRectangle (r, 6.0f);

            const auto face = r.reduced (6.0f);
            juce::ColourGradient glow (Colours::meterFace, face.getCentreX(), face.getBottom(),
                                       Colours::meterFace.darker (0.12f), face.getCentreX(), face.getY(), true);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (face, 4.0f);

            const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f);
            const float radius = face.getHeight() * 0.92f;
            const float a0 = -0.80f, a1 = 0.80f;   // radians either side of vertical
            const auto angleFor = [&] (float vu) { return a0 + (a1 - a0) * needleFraction (vu); };

            // Scale: arc, ticks and numbers; red above 0 VU.
            juce::Path arc;
            arc.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, a0, angleFor (0.0f), true);
            g.setColour (Colours::meterInk);
            g.strokePath (arc, juce::PathStrokeType (1.2f));
            juce::Path red;
            red.addCentredArc (pivot.x, pivot.y, radius * 0.78f, radius * 0.78f, 0.0f, angleFor (0.0f), a1, true);
            g.setColour (Colours::meterRed);
            g.strokePath (red, juce::PathStrokeType (3.0f));

            g.setFont (numbers (Type::control, Weight::Medium));   // the scale numbers are easy to read at the default size
            for (float vu : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
            {
                const float a = angleFor (vu);
                const auto p0 = pivot.getPointOnCircumference (radius * 0.78f, a);
                const auto p1 = pivot.getPointOnCircumference (radius * 0.86f, a);
                g.setColour (vu > 0.0f ? Colours::meterRed : Colours::meterInk);
                g.drawLine ({ p0, p1 }, 1.2f);
                if (vu == -20.0f || vu == -10.0f || vu == -5.0f || vu == -3.0f || vu == 0.0f || vu == 3.0f)
                {
                    const auto t = pivot.getPointOnCircumference (radius * 0.96f, a);
                    const auto txt = vu > 0.0f ? "+" + juce::String ((int) vu) : juce::String ((int) vu);
                    g.drawText (txt, juce::Rectangle<float> (34.0f, 16.0f).withCentre (t), juce::Justification::centred);
                }
            }

            g.setColour (Colours::meterInk.withAlpha (0.8f));
            g.setFont (font (Type::hint, Weight::SemiBold));
            g.drawText (modeLabel, face.withTrimmedTop (face.getHeight() * 0.66f).withHeight (18.0f), juce::Justification::centred);

            // Needle, clipped to the face.
            g.saveState();
            g.reduceClipRegion (face.toNearestInt());
            const float a = a0 + (a1 - a0) * juce::jlimit (-0.05f, 1.06f, ballistics.position());
            g.setColour (Colours::meterInk);
            g.drawLine ({ pivot, pivot.getPointOnCircumference (radius * 0.93f, a) }, 1.6f);
            g.restoreState();
        }

        VuBallistics ballistics;

    private:
        juce::String modeLabel { "Gain Reduction" };
    };
}
