#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PassiveEngine.h"
#include "PeqLookAndFeel.h"

namespace Peq::UI
{
    // The frequency response of the current settings, from 20 Hz to 20 kHz and 20 dB up and down. It is the engine's own
    // response (the same filters the audio uses), so what is drawn is what is heard. The 0 dB line is the unchanged level.
    class ResponseCurve : public juce::Component
    {
    public:
        static constexpr float lowestHz = 20.0f, highestHz = 20000.0f, rangeDb = 20.0f;
        static constexpr double drawSampleRate = 48000.0;

        ResponseCurve() { engine.prepare (drawSampleRate); setInterceptsMouseClicks (false, false); }

        // Settings to draw. Repaints only when they changed.
        void setParams (const SongsmithPassive::Params& p)
        {
            if (std::memcmp (&p, &shown, sizeof (p)) == 0)
                return;
            shown = p;
            engine.setParams (p);
            engine.prepare (drawSampleRate);   // settles the smoothing, so the response is the target one
            repaint();
        }

        float responseDb (float hz) const { return engine.responseDb (hz); }

        // Where a frequency or a level falls inside the drawing area.
        static float xFor (float hz, juce::Rectangle<float> plot)
        {
            return plot.getX() + plot.getWidth() * std::log (hz / lowestHz) / std::log (highestHz / lowestHz);
        }
        static float yFor (float db, juce::Rectangle<float> plot)
        {
            return plot.getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * plot.getHeight() * 0.5f;
        }

        juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().withTrimmedLeft (46.0f).withTrimmedBottom (20.0f).reduced (0.0f, 6.0f); }

        void paint (juce::Graphics& g) override
        {
            const auto all = getLocalBounds().toFloat();
            const auto plot = plotArea();

            g.setColour (Colours::recess);
            g.fillRoundedRectangle (all, 6.0f);

            // Level lines and labels.
            g.setFont (numbers (Type::caption));
            for (int db = -20; db <= 20; db += 10)
            {
                const float y = yFor ((float) db, plot);
                g.setColour (db == 0 ? Colours::textDim.withAlpha (0.55f) : Colours::panelLight.withAlpha (0.8f));
                g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
                g.setColour (Colours::textDim);
                g.drawText ((db > 0 ? "+" : "") + juce::String (db) + " dB",
                            juce::Rectangle<float> (all.getX() + 2.0f, y - 7.0f, 42.0f, 14.0f), juce::Justification::centredRight);
            }

            // Frequency lines and labels.
            for (const auto& [hz, label] : { std::pair<float, const char*> { 20.0f, "20 Hz" }, { 50.0f, "50 Hz" }, { 100.0f, "100 Hz" }, { 200.0f, "200 Hz" },
                                             { 500.0f, "500 Hz" }, { 1000.0f, "1 kHz" }, { 2000.0f, "2 kHz" }, { 5000.0f, "5 kHz" },
                                             { 10000.0f, "10 kHz" }, { 20000.0f, "20 kHz" } })
            {
                const float x = xFor (hz, plot);
                g.setColour (Colours::panelLight.withAlpha (0.8f));
                g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
                g.setColour (Colours::textDim);
                auto r = juce::Rectangle<float> (50.0f, 14.0f).withCentre ({ x, plot.getBottom() + 11.0f });
                if (r.getX() < plot.getX() - 6.0f) r.setX (plot.getX() - 6.0f);
                if (r.getRight() > all.getRight()) r.setX (all.getRight() - r.getWidth());
                g.drawText (label, r, juce::Justification::centred);
            }

            // The response.
            juce::Path curve;
            const int steps = juce::jmax (2, juce::roundToInt (plot.getWidth()));
            for (int i = 0; i <= steps; ++i)
            {
                const float hz = lowestHz * std::pow (highestHz / lowestHz, (float) i / (float) steps);
                const juce::Point<float> pt (plot.getX() + plot.getWidth() * (float) i / (float) steps, yFor (responseDb (hz), plot));
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }
            juce::Path fill (curve);
            fill.lineTo (plot.getRight(), yFor (0.0f, plot));
            fill.lineTo (plot.getX(), yFor (0.0f, plot));
            fill.closeSubPath();
            g.setColour (Colours::accent.withAlpha (0.16f));
            g.fillPath (fill);
            g.setColour (Colours::accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
        }

    private:
        SongsmithPassive::Engine engine;
        SongsmithPassive::Params shown {};
    };
}
