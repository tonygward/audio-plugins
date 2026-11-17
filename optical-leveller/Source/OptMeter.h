#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "OptLookAndFeel.h"
#include "OpticalEngine.h"

namespace Opt::UI
{
    // The meter reads in VU. The digital reference is 0 VU = -18 dBFS RMS (the same as the Analog Compressor's +4 position):
    //   Gain Reduction: the needle shows -(reduction), 0 VU = no reduction;
    //   Output: 0 VU = -18 dBFS RMS.
    inline constexpr float meterMinVu = -20.0f, meterMaxVu = 3.0f;

    inline float meterReadingVu (int mode, float reductionDb, float outputRmsDbfs) noexcept
    {
        const float vu = mode == 0 ? -reductionDb : outputRmsDbfs - SongsmithOptical::referenceLevelDb;
        return juce::jlimit (meterMinVu - 2.0f, meterMaxVu + 1.0f, vu);
    }

    // Needle position 0..1 on a VU scale: linear in voltage, +3 VU at full scale.
    inline float needleFraction (float vu) noexcept
    {
        return std::pow (10.0f, (vu - meterMaxVu) / 20.0f);
    }

    // VU ballistics, independent of the audio: a second-order needle (mass and damping) with about 1 % overshoot, which
    // reaches 99 % of a step in 300 ms (IEC 60268-17). Tested.
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

            g.setFont (numbers (Type::control, Weight::Medium));
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
