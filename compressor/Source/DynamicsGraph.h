#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "CompLookAndFeel.h"

// The data the audio thread hands to the graph, one column for every 10 ms of audio. All three numbers are
// measured from the real signal (never drawn from the settings):
//   inDb   the loudest sample at the input over the column, either channel (the same link as the detector)
//   outDb  the loudest sample at the output over the column, after compression, Makeup Gain, Analog Warmth, Mix and
//          Bypass, either channel
//   grDb   the deepest gain reduction in the column, as a positive number of decibels (0 while bypassed)
// Gain reduction and Makeup Gain are kept apart on purpose: grDb is the compressor turning the level down, while
// outDb includes whatever Makeup Gain adds back, so the red line can sit above the grey one with Makeup Gain.
namespace Comp
{
    struct GraphColumn
    {
        float inDb = -120.0f, outDb = -120.0f, grDb = 0.0f;
        bool bypassed = false;
    };

    constexpr double graphColumnSeconds = 0.01;
    constexpr int    graphColumns = 800;   // the picture holds the last 8 seconds

    // One writer (the audio thread) and one reader (the editor), fixed size, no locks and no allocation. If the
    // editor is closed or slow, the newest columns are dropped rather than the audio waiting.
    class ColumnFifo
    {
    public:
        static constexpr int capacity = 2048;

        bool push (const GraphColumn& c) noexcept
        {
            const auto scope = fifo.write (1);
            if (scope.blockSize1 + scope.blockSize2 < 1)
                return false;
            items[(size_t) (scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2)] = c;
            return true;
        }

        // Editor thread: everything waiting, oldest first. Returns how many were read.
        int popAll (GraphColumn* out, int maxCount) noexcept
        {
            const int n = juce::jmin (fifo.getNumReady(), maxCount);
            if (n <= 0)
                return 0;
            const auto scope = fifo.read (n);
            int k = 0;
            for (int i = 0; i < scope.blockSize1; ++i) out[k++] = items[(size_t) (scope.startIndex1 + i)];
            for (int i = 0; i < scope.blockSize2; ++i) out[k++] = items[(size_t) (scope.startIndex2 + i)];
            return k;
        }

        void clear() noexcept { fifo.reset(); }

    private:
        juce::AbstractFifo fifo { capacity };
        std::array<GraphColumn, (size_t) capacity> items {};
    };

    // Scrolling level-against-time graph: the sound going in (grey) and the sound coming out (red), newest at the right.
    class DynamicsGraph : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr float minDb = -60.0f, maxDb = 6.0f;

        DynamicsGraph()
        {
            history.resize ((size_t) graphColumns);
            setTooltip ("Level over the last 8 seconds. Grey is the sound going in; red is the sound coming out, after "
                        "compression and Makeup Gain. Where red falls below grey, the compressor is turning the level down. "
                        "These are sample peaks, not true peaks.");
        }

        // Called from the editor's timer with each column that arrived. Ignored while paused.
        void push (const GraphColumn& c)
        {
            if (paused)
                return;
            history[(size_t) head] = c;
            head = (head + 1) % graphColumns;
            count = juce::jmin (count + 1, graphColumns);
            dirty = true;
        }

        void setPaused (bool p) { paused = p; repaint(); }
        bool isPaused() const noexcept { return paused; }
        void clearHistory() { count = 0; head = 0; dirty = true; repaint(); }

        void setThresholdDb (float db) { if (db != thresholdDb) { thresholdDb = db; dirty = true; } }
        void refreshIfChanged() { if (dirty) { dirty = false; repaint(); } }

        int getNumColumns() const noexcept { return count; }
        // 0 is the oldest column still held, getNumColumns() - 1 the newest.
        GraphColumn getColumn (int index) const noexcept
        {
            const int start = (head - count + graphColumns) % graphColumns;
            return history[(size_t) ((start + juce::jlimit (0, juce::jmax (0, count - 1), index)) % graphColumns)];
        }

        static float dbToProportion (float db) noexcept { return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb)); }

        juce::Rectangle<float> getPlotArea() const { return getLocalBounds().toFloat().reduced (1.0f).withTrimmedLeft (30.0f).withTrimmedBottom (16.0f).withTrimmedTop (6.0f).withTrimmedRight (6.0f); }

        void paint (juce::Graphics& g) override
        {
            const auto outer = getLocalBounds().toFloat().reduced (0.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d2027), 0.0f, outer.getY(), juce::Colour (0xff121419), 0.0f, outer.getBottom(), false));
            g.fillRoundedRectangle (outer, 6.0f);

            const auto plot = getPlotArea();
            auto toY = [&] (float db) { return plot.getBottom() - plot.getHeight() * dbToProportion (db); };
            const auto font = juce::Font (CompColours::numbers (CompColours::Type::caption));

            // Grid every 12 dB, the 0 dBFS line a little brighter, labels in plain numbers (the unit is in the corner).
            g.setFont (font);
            for (int db = -60; db <= 0; db += 12)
            {
                const float y = toY ((float) db);
                g.setColour (db == 0 ? juce::Colour (0x50ffffff) : juce::Colour (0x26ffffff));
                g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
                g.setColour (CompColours::label);
                g.drawText (juce::String (db), juce::Rectangle<float> (outer.getX() + 2.0f, y - 7.0f, 26.0f, 14.0f), juce::Justification::centredRight, false);
            }
            g.setColour (CompColours::label);
            g.drawText ("dBFS", juce::Rectangle<float> (plot.getX() + 4.0f, plot.getY() + 1.0f, 50.0f, 13.0f), juce::Justification::centredLeft, false);
            g.drawText ("Last 8 seconds", juce::Rectangle<float> (plot.getRight() - 120.0f, plot.getBottom() + 2.0f, 120.0f, 13.0f), juce::Justification::centredRight, false);

            // The legend: what each line is.
            {
                g.setFont (juce::Font (CompColours::font (CompColours::Type::hint, CompColours::Weight::Medium)));
                float lx = plot.getX() + 56.0f;
                const float ly = plot.getY() + 1.0f;
                for (auto [colour, text] : { std::pair<juce::Colour, const char*> { juce::Colour (0xffa4a7ad), "Input" }, { juce::Colour (0xffff5c5c), "Output" } })
                {
                    g.setColour (colour);
                    g.fillRoundedRectangle (lx, ly + 5.5f, 14.0f, 3.0f, 1.5f);
                    g.setColour (CompColours::label);
                    g.drawText (text, juce::Rectangle<float> (lx + 18.0f, ly, 56.0f, 14.0f), juce::Justification::centredLeft, false);
                    lx += 78.0f;
                }
                g.setFont (font);
            }

            // The threshold, as set: a dashed line, so the picture explains where compression starts.
            {
                const float y = toY (thresholdDb);
                g.setColour (CompColours::accent.withAlpha (0.55f));
                const float dashes[] = { 5.0f, 4.0f };
                g.drawDashedLine (juce::Line<float> (plot.getX(), y, plot.getRight(), y), dashes, 2, 1.0f);
            }

            if (count > 1)
            {
                const float step = plot.getWidth() / (float) (graphColumns - 1);
                auto xOf = [&] (int i) { return plot.getRight() - (float) (count - 1 - i) * step; };

                juce::Path input, output, gap;
                for (int i = 0; i < count; ++i)
                {
                    const auto c = getColumn (i);
                    const float x = xOf (i), yi = toY (c.inDb), yo = toY (c.outDb);
                    if (i == 0) { input.startNewSubPath (x, yi); output.startNewSubPath (x, yo); }
                    else        { input.lineTo (x, yi); output.lineTo (x, yo); }
                }
                // The subdued gap between the two lines: the input line forward, the output line back.
                gap = input;
                for (int i = count - 1; i >= 0; --i)
                    gap.lineTo (xOf (i), toY (getColumn (i).outDb));
                gap.closeSubPath();

                juce::Graphics::ScopedSaveState state (g);
                g.reduceClipRegion (plot.toNearestInt());

                g.setColour (CompColours::clip.withAlpha (0.14f));
                g.fillPath (gap);

                juce::Path inputFill (input);
                inputFill.lineTo (xOf (count - 1), plot.getBottom());
                inputFill.lineTo (xOf (0), plot.getBottom());
                inputFill.closeSubPath();
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillPath (inputFill);

                g.setColour (juce::Colour (0xffa4a7ad).withAlpha (0.85f));
                g.strokePath (input, juce::PathStrokeType (1.4f));
                g.setColour (juce::Colour (0xffff5c5c));
                g.strokePath (output, juce::PathStrokeType (1.7f));
            }

            // Honest about bypass: the lines then lie on top of each other, and the picture says why.
            if (count > 0 && getColumn (count - 1).bypassed)
            {
                g.setColour (juce::Colour (0xffc9563c));
                g.setFont (juce::Font (CompColours::font (CompColours::Type::hint, CompColours::Weight::Medium)));
                g.drawText ("Bypassed: the compressor is not changing the sound", plot.withTrimmedTop (plot.getHeight() * 0.4f).toNearestInt().toFloat(),
                            juce::Justification::centredTop, false);
            }
            else if (paused)
            {
                g.setColour (CompColours::label);
                g.setFont (juce::Font (CompColours::font (CompColours::Type::hint, CompColours::Weight::Medium)));
                g.drawText ("Paused", plot.reduced (8.0f).toNearestInt().toFloat(), juce::Justification::topRight, false);
            }

            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (outer, 6.0f, 1.0f);
        }

    private:
        std::vector<GraphColumn> history;
        int head = 0, count = 0;
        bool paused = false, dirty = false;
        float thresholdDb = -18.0f;
    };
}
