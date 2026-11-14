#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include "SongsmithColours.h"
#include "SongsmithType.h"

namespace Songsmith::UI
{
    inline const juce::Colour zeroLineColour  { 0x50ffffff };
    inline const juce::Colour gridMajorColour { 0x26ffffff };

    // Moved to the shared library (COMP-02) so the Songsmith Compressor uses the same meter as the Songsmith EQ.
    // Tall vertical output peak meter, on the right of the graph (task 27, EQ-39). From the top:
    // the CLIP warning, the held peak as a signed number in dBFS directly beneath it (EQ-42), the
    // channel names, the bars (fill bottom to top, -60 to +6 dBFS on a linear
    // dB scale) with the dB scale on the right. The number is red only while the held peak is at or
    // above 0 dBFS (the same boundary as CLIP) and never depends on the CLIP latch. One bar for a mono output, two (Left, Right) for stereo, each showing its own peak.
    // The owner calls update() from its timer with the peaks read from the audio thread (linear,
    // already reset there) and the audio thread's latched clip state. The CLIP warning is text as
    // well as colour. It is sample-peak clipping (a sample at or above 0 dBFS), not a true-peak
    // measurement, and it clears only when the output has gone quiet, which the audio thread decides.
    class OutputMeterDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        static constexpr float minDb = -60.0f, maxDb = 6.0f;

        // Layout, in design units.
        static constexpr int   preferredWidth = 48;
        static constexpr float barsWidth = 20.0f;               // all the bars together, mono or stereo
        static constexpr float barGap = 2.0f;                   // between the Left and Right bars
        static constexpr float clipWidth = 34.0f, clipHeight = 14.0f, clipGap = 3.0f;
        static constexpr float peakHeight = 14.0f, peakGap = 2.0f;   // the held-peak number, directly under CLIP
        static constexpr float namesHeight = 13.0f;             // the L and R above the bars
        static constexpr float bottomRow = 20.0f;               // same as the graph's frequency-label row

        // dB marks drawn on the scale.
        static constexpr float scaleMarks[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };

        OutputMeterDisplay()
        {
            setTooltip ("Output peak for each channel, measured after the EQ and the output gain. The number under the clip "
                        "warning is the highest sample level held for 1.5 seconds: negative means below 0 dBFS, positive means over it "
                        "and is red. The warning lights when a sample reaches 0 dBFS and stays lit while sound continues, so it can "
                        "stay lit after the number has fallen. This is sample peak, not true peak. If the warning lights and the next stage may not have "
                        "headroom, lower Output or the boosts. Click to clear the held number.");
        }

        // peakLeft / peakRight: linear peaks since the last call. channelCount: 1 for mono (only the
        // left value is used) or 2. clipLatched: the audio thread's warning state.
        void update (float peakLeft, float peakRight, int channelCount, bool clipLatched, double nowSeconds, double elapsedSeconds)
        {
            const int n = juce::jlimit (1, 2, channelCount);
            if (n != numChannels)
            {
                numChannels = n;
                reset();   // a new layout starts clean
            }

            const float peaks[2] = { peakLeft, peakRight };
            for (int c = 0; c < n; ++c)
            {
                const float db = juce::Decibels::gainToDecibels (peaks[c], -100.0f);

                // Fast attack, ~24 dB/s fall-off.
                level[c] = juce::jmax (db, level[c] - (float) (24.0 * elapsedSeconds));

                if (db >= holdDb[c])
                {
                    holdDb[c] = db;
                    holdTime[c] = nowSeconds;
                }
                else if (nowSeconds - holdTime[c] > 1.5)
                {
                    holdDb[c] = juce::jmax (db, holdDb[c] - (float) (24.0 * elapsedSeconds));
                }
            }

            clipped = clipLatched;
            repaint();
        }

        void reset()
        {
            for (int c = 0; c < 2; ++c)
            {
                level[c] = holdDb[c] = -100.0f;
                holdTime[c] = 0.0;
            }
            clipped = false;
            repaint();
        }

        // A click clears the held peak numbers. The CLIP warning is not cleared by hand: it clears
        // when the sound stops.
        void mouseDown (const juce::MouseEvent&) override
        {
            for (int c = 0; c < 2; ++c)
                holdDb[c] = level[c];
            repaint();
        }

        // Geometry (local coordinates), public for tests.
        juce::Rectangle<float> getClipBox() const
        {
            return { 1.0f, 1.0f, clipWidth, clipHeight };
        }

        // The held-peak number: directly under CLIP, above the channel names and bars.
        juce::Rectangle<float> getPeakBox() const
        {
            const auto clip = getClipBox();
            return { clip.getX(), clip.getBottom() + clipGap - 1.0f, clip.getWidth(), peakHeight };
        }

        // The area of all the bars together.
        juce::Rectangle<float> getBarArea() const
        {
            const float top = getPeakBox().getBottom() + peakGap + namesHeight;
            return { 1.0f, top, barsWidth, juce::jmax (1.0f, (float) getHeight() - bottomRow - top) };
        }

        // One channel's bar: the whole area for mono, the left or right half for stereo.
        juce::Rectangle<float> getChannelBar (int channel) const
        {
            const auto all = getBarArea();
            if (numChannels < 2)
                return all;
            const float w = (all.getWidth() - barGap) * 0.5f;
            return channel == 0 ? all.withWidth (w) : all.withTrimmedLeft (w + barGap);
        }

        int getNumChannels() const noexcept { return numChannels; }

        // 0 at -60 dB (bottom), 1 at +6 dB (top), clamped.
        static float dbToProportion (float db) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        }

        float levelToY (float db) const
        {
            const auto bar = getBarArea();
            return bar.getBottom() - bar.getHeight() * dbToProportion (db);
        }

        // True when a held peak is over full scale. 0 dBFS counts (the same boundary as CLIP); the sign
        // of the number is the sign of the measurement, never of the CLIP latch.
        static bool isOverFullScale (float holdDb) noexcept   { return holdDb >= 0.0f; }
        bool isPeakOver() const noexcept                      { return getHoldDb() > minDb && isOverFullScale (getHoldDb()); }
        juce::Colour getPeakColour() const
        {
            return isPeakOver() ? juce::Colour (0xffff5c5c) : Colours::textDim;
        }

        // The text under CLIP: the highest held peak, or a dash when there's no signal.
        juce::String getPeakText() const
        {
            const float hold = getHoldDb();
            if (hold <= minDb)
                return juce::String::charToString (0x2013);   // en dash
            return (hold > 0.05f ? "+" : "") + juce::String (hold, 1);
        }

        void paint (juce::Graphics& g) override
        {
            // Edges on whole physical pixels, so the meter stays sharp at 125 % / 150 % / 200 %.
            const float s = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
            auto snapV = [s] (float v) { return std::round (v * s) / s; };
            auto snap = [snapV] (juce::Rectangle<float> rect)
            {
                return juce::Rectangle<float>::leftTopRightBottom (snapV (rect.getX()), snapV (rect.getY()),
                                                                  snapV (rect.getRight()), snapV (rect.getBottom()));
            };

            const auto r = snap (getBarArea());
            const auto clipBox = snap (getClipBox());
            auto toY = [&] (float db) { return snapV (r.getBottom() - r.getHeight() * dbToProportion (db)); };
            const float zeroY = toY (0.0f);
            const auto font = juce::Font (numbers (Type::caption));

            for (int c = 0; c < numChannels; ++c)
            {
                const auto bar = snap (getChannelBar (c));
                g.setColour (Colours::panelLight);
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (Colours::edge);
                g.drawRoundedRectangle (bar, 2.0f, 1.0f);

                // Bar: the existing teal-to-yellow gradient up to 0 dBFS, red above.
                const float y = toY (level[c]);
                if (y < bar.getBottom())
                {
                    const auto fill = bar.withTop (y).reduced (1.0f);
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3ddbc4), 0.0f, bar.getBottom(),
                                                             juce::Colour (0xfff2c94c), 0.0f, zeroY, false));
                    g.fillRect (fill.withTop (juce::jmax (fill.getY(), zeroY)));
                    if (fill.getY() < zeroY)
                    {
                        g.setColour (juce::Colour (0xffff5c5c));
                        g.fillRect (fill.withBottom (zeroY));
                    }
                }

                if (holdDb[c] > minDb)
                {
                    g.setColour (holdDb[c] >= 0.0f ? juce::Colour (0xffff5c5c) : Colours::text);
                    g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, toY (holdDb[c]) - 1.0f, bar.getWidth() - 2.0f, 2.0f));
                }

                if (numChannels == 2)   // Left and Right, as on a mixer strip
                {
                    g.setColour (Colours::textDim);
                    g.setFont (font);
                    g.drawText (c == 0 ? "L" : "R", juce::Rectangle<float> (bar.getX(), bar.getY() - namesHeight, bar.getWidth(), namesHeight),
                                juce::Justification::centred, false);
                }
            }

            // Scale: ticks and labels to the right of the bars. Labels that would overlap are dropped.
            g.setFont (font);
            const float labelX = r.getRight() + 4.0f, labelW = (float) getWidth() - labelX;
            float lastBottom = -1000.0f;
            for (auto db : scaleMarks)
            {
                const float ty = toY (db);
                g.setColour (db == 0.0f ? zeroLineColour.withMultipliedAlpha (2.0f) : gridMajorColour);
                g.fillRect (juce::Rectangle<float> (r.getRight(), ty - 0.5f / s, 3.0f, juce::jmax (1.0f / s, 1.0f)));

                const auto box = juce::Rectangle<float> (labelX, ty - 7.0f, labelW, 14.0f);
                if (box.getY() >= lastBottom)
                {
                    g.setColour (db == 0.0f ? Colours::text : Colours::textDim);
                    g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db), box,
                                juce::Justification::centredLeft, false);
                    lastBottom = box.getBottom();
                }
            }

            g.setColour (zeroLineColour);
            g.fillRect (juce::Rectangle<float> (r.getX(), zeroY, r.getWidth(), 1.0f / s));

            // CLIP warning: the word, so it doesn't rely on colour alone.
            g.setColour (clipped ? juce::Colour (0xffd93030) : Colours::panelLight);
            g.fillRoundedRectangle (clipBox, 2.0f);
            g.setColour (clipped ? juce::Colour (0xffff7a7a) : Colours::edge);
            g.drawRoundedRectangle (clipBox, 2.0f, 1.0f);
            if (clipped)
            {
                g.setColour (juce::Colours::white);
                g.setFont (font);
                g.drawText ("CLIP", clipBox, juce::Justification::centred, false);
            }

            // Held peak, directly under CLIP. Its colour follows the measured peak, not the CLIP latch.
            g.setColour (getPeakColour());
            g.setFont (font);
            g.drawText (getPeakText(), snap (getPeakBox()),
                        juce::Justification::centred, false);
        }

        bool isClipped() const noexcept { return clipped; }
        float getHoldDb() const noexcept { return numChannels > 1 ? juce::jmax (holdDb[0], holdDb[1]) : holdDb[0]; }
        float getLevelDb (int channel = 0) const noexcept { return level[juce::jlimit (0, 1, channel)]; }
        float getChannelHoldDb (int channel) const noexcept { return holdDb[juce::jlimit (0, 1, channel)]; }

    private:
        float level[2] = { -100.0f, -100.0f }, holdDb[2] = { -100.0f, -100.0f };
        double holdTime[2] = { 0.0, 0.0 };
        int numChannels = 2;
        bool clipped = false;
    };

    // The audio thread's side: written after every block, read (and reset) by the editor. A peak per main-output
    // channel, the channel count, and a clip warning that latches (sample peak >= 0 dBFS) until the output has
    // been near-silent for a while. Same semantics for every plugin (EQ-39, EQ-42).
    struct OutputMeterState
    {
        static constexpr int   maxChannels = 2;
        static constexpr float silenceThreshold = 1.0e-4f;   // -80 dBFS
        static constexpr double silenceSeconds = 0.75;

        std::atomic<float> peak { 0.0f };       // max |sample| since the last read (all channels)
        std::atomic<bool>  clipped { false };   // latched when a sample reaches 0 dBFS; cleared by the reader
        std::array<std::atomic<float>, maxChannels> channelPeak {};   // per channel, since the last read
        std::atomic<int>   channels { 2 };      // main output channels now: 1 (mono) or 2 (stereo)
        std::atomic<bool>  clipLatched { false };   // the CLIP warning: stays on while there is audio

        void push (float blockPeak) noexcept
        {
            float prev = peak.load (std::memory_order_relaxed);
            while (blockPeak > prev && ! peak.compare_exchange_weak (prev, blockPeak, std::memory_order_relaxed)) {}
            if (blockPeak >= 1.0f)
                clipped.store (true, std::memory_order_relaxed);
        }

        void pushChannel (int channel, float channelBlockPeak) noexcept
        {
            auto& a = channelPeak[(size_t) channel];
            float prev = a.load (std::memory_order_relaxed);
            while (channelBlockPeak > prev && ! a.compare_exchange_weak (prev, channelBlockPeak, std::memory_order_relaxed)) {}
        }

        // Audio thread, once per block, with the peak over every output channel: latches the warning on a
        // clip and clears it after 'silenceSeconds' of near-silence. A quiet pause shorter than that
        // does not clear it. Only the audio thread touches silentSamples.
        void updateClipLatch (float blockPeak, int numSamples) noexcept
        {
            if (blockPeak >= 1.0f)
            {
                clipLatched.store (true, std::memory_order_relaxed);
                silentSamples = 0.0;
            }
            else if (blockPeak < silenceThreshold)
            {
                silentSamples += (double) numSamples;
                if (silentSamples >= silenceSamples)
                    clipLatched.store (false, std::memory_order_relaxed);
            }
            else
            {
                silentSamples = 0.0;
            }
        }

        // Prepare / layout change: no stale warning from the old layout.
        void resetLatch (double sampleRate) noexcept
        {
            silenceSamples = silenceSeconds * juce::jmax (1.0, sampleRate);
            silentSamples = 0.0;
            clipLatched.store (false, std::memory_order_relaxed);
        }

        float readAndReset() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }
        float readAndResetChannel (int channel) noexcept { return channelPeak[(size_t) channel].exchange (0.0f, std::memory_order_relaxed); }

    private:
        double silentSamples = 0.0, silenceSamples = silenceSeconds * 48000.0;
    };
}
