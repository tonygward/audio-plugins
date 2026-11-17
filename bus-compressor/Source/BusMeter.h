#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "BusLookAndFeel.h"
#include "BusParams.h"

namespace BusComp::UI
{
    // The gain reduction meter: a row of lamps, one for each decibel from 0 to 20. A lamp is lit when the compressor
    // is turning the level down by at least that much.
    inline constexpr int numLamps = 20;

    // How a lamp row reads: instant rise, then a smooth fall (about a fifth of a second to lose most of the reading), so a
    // short reduction is still visible. Independent of the audio and tested.
    class Ballistics
    {
    public:
        static constexpr float fallSeconds = 0.15f;

        void reset (float db = 0.0f) noexcept { value = db; }

        float advance (float targetDb, float seconds) noexcept
        {
            targetDb = juce::jlimit (0.0f, meterMaxDb, targetDb);
            if (targetDb >= value)
                value = targetDb;
            else
                value = targetDb + (value - targetDb) * std::exp (-seconds / fallSeconds);
            return value;
        }

        float position() const noexcept { return value; }

    private:
        float value = 0.0f;
    };

    // How many lamps are lit for a reading in dB.
    inline int lampsLit (float db) noexcept
    {
        return juce::jlimit (0, numLamps, (int) std::floor (juce::jmax (0.0f, db) + 1.0e-4f));
    }

    class LampMeter : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        void setReading (float db, float seconds)
        {
            ballistics.advance (db, seconds);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat();
            g.setColour (Colours::recess);
            g.fillRoundedRectangle (r, 6.0f);

            const auto lamps = r.reduced (10.0f, 8.0f).withTrimmedBottom (16.0f);
            const float gap = 3.0f, width = (lamps.getWidth() - gap * (float) (numLamps - 1)) / (float) numLamps;
            const int lit = lampsLit (ballistics.position());

            for (int i = 0; i < numLamps; ++i)
            {
                const auto lamp = juce::Rectangle<float> (lamps.getX() + (float) i * (width + gap), lamps.getY(), width, lamps.getHeight());
                const bool on = i < lit;
                const auto colour = i >= 14 ? Colours::meterRed : Colours::accent;   // beyond 14 dB is a lot of reduction
                g.setColour (on ? colour : Colours::panelLight.darker (0.4f));
                g.fillRoundedRectangle (lamp, 2.0f);
                if (on)
                {
                    g.setColour (colour.brighter (0.4f).withAlpha (0.35f));
                    g.fillRoundedRectangle (lamp.withHeight (lamp.getHeight() * 0.4f).reduced (1.0f, 0.0f), 2.0f);
                }
            }

            // The printed scale under the lamps: the number sits under the lamp it names.
            g.setFont (numbers (Type::caption, Weight::Medium));
            g.setColour (Colours::textDim);
            for (int db : { 0, 2, 4, 6, 10, 14, 20 })
            {
                const float x = db == 0 ? lamps.getX() : lamps.getX() + (float) (db - 1) * (width + gap) + width * 0.5f;
                g.drawText (juce::String (db), juce::Rectangle<float> (28.0f, 14.0f).withCentre ({ x + (db == 0 ? 6.0f : 0.0f), lamps.getBottom() + 9.0f }),
                            juce::Justification::centred);
            }
        }

        Ballistics ballistics;
    };
}
