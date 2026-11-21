#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AnalysisMath.h"
#include "../../shared/Source/SongsmithLookAndFeel.h"

// ANALYZER-01: the frequency-response graph. Frequency runs left to right on a logarithmic scale from 20 Hz to 20 kHz, and gain
// runs up and down in decibels, with 0 dB drawn firmly. One measurement is drawn in the accent colour and a second one over it
// in a warm colour. Everything is drawn in design units, so it scales with the editor.
namespace PluginAnalyzer::UI
{
    namespace Colours
    {
        inline const juce::Colour accent  { 0xffb5d46a };   // the plugin's own accent (buttons, the progress bar)
        // ANALYZER-02: the two captures. Blue and orange are told apart by almost everyone, including people who cannot tell green from
        // orange, and the second is also drawn dashed, so the graph never depends on colour alone.
        inline const juce::Colour first   { 0xff6cb4ee };   // the first capture: a solid line
        inline const juce::Colour second  { 0xffe6a15c };   // the second capture: a dashed line
        inline const juce::Colour compare = second;         // also the colour of a problem message
    }

    class ResponseGraph : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        struct Curve
        {
            const Measurement* measurement = nullptr;   // null: nothing to draw
            juce::String name;
            juce::Colour colour;
            bool dashed = false;
        };

        ResponseGraph() { setTitle ("Frequency response graph"); setInterceptsMouseClicks (true, false); }

        void setCurves (Curve first, Curve second) { curves[0] = std::move (first); curves[1] = std::move (second); repaint(); }
        void setRangeDb (float range) { rangeDb = range; repaint(); }
        float getRangeDb() const noexcept { return rangeDb; }
        void setMessage (const juce::String& text) { message = text; repaint(); }
        const juce::String& getMessage() const noexcept { return message; }
        // For tests: how many curves are drawn (a capture with no valid curve is not drawn).
        int getNumCurvesDrawn() const noexcept
        {
            int n = 0;
            for (auto& c : curves)
                n += (c.measurement != nullptr && c.measurement->isValidCurve()) ? 1 : 0;
            return n;
        }
        const Curve& getCurve (int index) const noexcept { return curves[(size_t) index]; }

        // For tests: where a frequency and a gain are on the graph.
        juce::Rectangle<float> getPlotArea() const noexcept { return plot(); }
        float xForFrequency (double hz) const noexcept
        {
            const auto p = plot();
            return p.getX() + (float) (std::log (hz / gridLowHz) / std::log (gridHighHz / gridLowHz)) * p.getWidth();
        }
        float yForDb (float db) const noexcept
        {
            const auto p = plot();
            return p.getY() + (rangeDb - juce::jlimit (-rangeDb, rangeDb, db)) / (2.0f * rangeDb) * p.getHeight();
        }
        double frequencyForX (float x) const noexcept
        {
            const auto p = plot();
            return gridLowHz * std::pow (gridHighHz / gridLowHz, (double) juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
        }

        void mouseMove (const juce::MouseEvent& e) override { hoverX = e.position.x; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hoverX = -1.0f; repaint(); }

        void paint (juce::Graphics& g) override
        {
            namespace U = Songsmith::UI;
            U::LookAndFeelBase::drawWell (g, getLocalBounds().toFloat().reduced (0.5f), U::Colours::recess);
            const auto p = plot();

            // Grid: decibels every 6 dB (every 3 dB on the narrowest range), frequencies at 1, 2 and 5 of each decade.
            const float step = rangeDb <= 6.0f ? 3.0f : 6.0f;
            g.setFont (U::font (U::Type::caption));
            for (float db = -rangeDb; db <= rangeDb + 0.01f; db += step)
            {
                const float y = yForDb (db);
                const bool zero = std::abs (db) < 0.01f;
                g.setColour (zero ? U::Colours::textDim.withAlpha (0.8f) : U::Colours::edge);
                g.drawHorizontalLine ((int) y, p.getX(), p.getRight());
                g.setColour (U::Colours::textDim);
                g.drawText ((db > 0.01f ? "+" : "") + juce::String ((int) std::lround (db)) + " dB", juce::Rectangle<float> (2.0f, y - 7.0f, p.getX() - 6.0f, 14.0f),
                            juce::Justification::centredRight);
            }
            static const double marks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
            static const char* markNames[] = { "20 Hz", "50 Hz", "100 Hz", "200 Hz", "500 Hz", "1 kHz", "2 kHz", "5 kHz", "10 kHz", "20 kHz" };
            for (int i = 0; i < 10; ++i)
            {
                const float x = xForFrequency (marks[i]);
                g.setColour (U::Colours::edge);
                g.drawVerticalLine ((int) x, p.getY(), p.getBottom());
                g.setColour (U::Colours::textDim);
                g.drawText (markNames[i], juce::Rectangle<float> (x - 28.0f, p.getBottom() + 3.0f, 56.0f, 14.0f), juce::Justification::centred);
            }

            // The curves. A gap in the measured points breaks the line.
            g.saveState();
            g.reduceClipRegion (p.toNearestInt());
            for (int c = 1; c >= 0; --c)   // the first curve is drawn last, on top
            {
                const auto& curve = curves[(size_t) c];
                if (curve.measurement == nullptr || ! curve.measurement->isValidCurve())
                    continue;
                const auto& m = *curve.measurement;
                juce::Path path;
                bool open = false;
                for (size_t i = 0; i < m.frequencyHz.size(); ++i)
                {
                    if (! m.valid[i]) { open = false; continue; }
                    const float x = xForFrequency (m.frequencyHz[i]), y = yForDb (m.magnitudeDb[i]);
                    if (! open) { path.startNewSubPath (x, y); open = true; }
                    else path.lineTo (x, y);
                }
                g.setColour (curve.colour);
                const juce::PathStrokeType stroke (c == 0 ? 2.4f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                if (curve.dashed)
                {
                    const float dashes[] = { 8.0f, 5.0f };
                    juce::Path dashed;
                    stroke.createDashedStroke (dashed, path, dashes, 2);
                    g.fillPath (dashed);
                }
                else
                    g.strokePath (path, stroke);
            }
            g.restoreState();
            // The legend (which curve is which, and showing or hiding each) is the row of controls under the graph.

            // The readout under the pointer.
            if (hoverX >= p.getX() && hoverX <= p.getRight())
            {
                const double hz = frequencyForX (hoverX);
                g.setColour (U::Colours::textDim);
                g.drawVerticalLine ((int) hoverX, p.getY(), p.getBottom());
                juce::String text = hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (hz, 0) + " Hz";
                for (auto& curve : curves)
                    if (curve.measurement != nullptr && curve.measurement->isValidCurve())
                    {
                        const int i = juce::jlimit (0, gridPoints - 1, (int) std::lround (std::log (hz / gridLowHz) / std::log (gridHighHz / gridLowHz) * (gridPoints - 1)));
                        if (curve.measurement->valid[(size_t) i])
                            text << "   " << curve.name << ": " << (curve.measurement->magnitudeDb[(size_t) i] > 0.05f ? "+" : "")
                                 << juce::String (curve.measurement->magnitudeDb[(size_t) i], 1) << " dB";
                    }
                g.setFont (U::font (U::Type::hint));
                g.setColour (U::Colours::text);
                g.drawText (text, juce::Rectangle<float> (p.getX() + 8.0f, p.getBottom() - 22.0f, p.getWidth() - 16.0f, 16.0f), juce::Justification::centredRight);
            }

            if (message.isNotEmpty())
            {
                g.setFont (U::font (U::Type::control));
                g.setColour (U::Colours::textDim);
                g.drawFittedText (message, p.toNearestInt().reduced (40, 60), juce::Justification::centred, 6);
            }
        }

    private:
        juce::Rectangle<float> plot() const noexcept
        {
            return getLocalBounds().toFloat().withTrimmedLeft (52.0f).withTrimmedRight (14.0f).withTrimmedTop (10.0f).withTrimmedBottom (26.0f);
        }

        std::array<Curve, 2> curves;
        float rangeDb = 12.0f;
        float hoverX = -1.0f;
        juce::String message;
    };
}
