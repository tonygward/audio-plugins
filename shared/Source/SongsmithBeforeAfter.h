#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include "SongsmithColours.h"
#include "SongsmithKWeighting.h"
#include "SongsmithType.h"

// Before and After loudness (songsmithstudio#7): one shared meter and one shared readout for every Songsmith processing plugin.
//
// Two short-term loudness readings, in LUFS: Before is the sound entering this plugin, After is the sound leaving it (after every
// stage, including make-up gain, output gain and Bypass). Both use exactly the same measurement, so the comparison means
// something. They measure this plugin only, never the whole track.
//
// The measurement: the K-weighted signal (Songsmith::KWeighting, the filter the Equaliser's Auto Gain and the Maximiser's loudness
// meter already use), the mean power of the last 3 seconds, summed over the channels, in LUFS: -0.691 + 10 log10 (power).
// That is the short-term loudness of ITU-R BS.1770 / EBU R 128 without any gating. A mono plugin instance counts its one channel.
//
// What the readout shows:
//   a dash         there is no sound (below -70 LUFS)
//   "Measuring…"   fewer than 3 seconds have been heard since playback started, so the window is not full
//   a number       the short-term loudness, to 0.1 LUFS
// When the host stops, the last readings stay; when it starts again they begin afresh. A host that reports no transport (a
// standalone app) is treated as always playing. Bypass needs no special case: the output is the input, so both agree.
//
// Real-time rules (see CLAUDE.md): the audio thread uses fixed memory, no allocation, no lock, no logging and no file access,
// and publishes each reading in one lock-free atomic. The window is about 40 operations a sample per channel for the pair.
namespace Songsmith::Loudness
{
    inline constexpr double windowSeconds = 3.0, stepSeconds = 0.1;
    inline constexpr int stepsInWindow = 30;                 // 30 steps of 0.1 s
    inline constexpr float silenceLufs = -70.0f;             // below this there is no sound worth a number
    inline constexpr float notMeasured = std::numeric_limits<float>::quiet_NaN();   // the window is not full yet

    inline double powerToLufs (double power) noexcept { return -0.691 + 10.0 * std::log10 (power); }

    //==============================================================================
    class ShortTermMeter
    {
    public:
        static constexpr int maxChannels = 2;

        // Allocation-free. Call from prepareToPlay (never during processing).
        void prepare (double sampleRate) noexcept
        {
            fs = sampleRate > 0.0 ? sampleRate : 48000.0;
            stepSamples = juce::jmax (1, juce::roundToInt (fs * stepSeconds));
            for (auto& c : channels)
                Songsmith::KWeighting::coefficients (fs, c.shelf, c.highPass);
            reset();
        }

        void reset() noexcept
        {
            for (auto& c : channels)
            {
                c.shelf.clear();
                c.highPass.clear();
            }
            ring.fill (0.0);
            ringPos = filled = stepCount = 0;
            stepPower = 0.0;
            published.store (notMeasured, std::memory_order_relaxed);
        }

        void process (const float* const* in, int numChannels, int numSamples) noexcept
        {
            const int numCh = juce::jlimit (0, maxChannels, numChannels);
            if (numCh == 0)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                double power = 0.0;
                for (int c = 0; c < numCh; ++c)
                {
                    double x = (double) in[c][i];
                    if (! std::isfinite (x))
                        x = 0.0;   // a damaged sample is silence to the meter
                    x = channels[(size_t) c].highPass.process (channels[(size_t) c].shelf.process (x));
                    power += x * x;
                }
                stepPower += power;

                if (++stepCount >= stepSamples)
                {
                    ring[(size_t) ringPos] = stepPower / (double) stepSamples;   // this step's power, channels summed
                    ringPos = (ringPos + 1) % stepsInWindow;
                    filled = juce::jmin (filled + 1, stepsInWindow);
                    stepPower = 0.0;
                    stepCount = 0;
                    publish();
                }
            }
        }

        // The latest reading: NaN while the window is not full, otherwise LUFS (at or below silenceLufs when there is no sound).
        float get() const noexcept { return published.load (std::memory_order_relaxed); }

    private:
        void publish() noexcept
        {
            if (filled < stepsInWindow)
                return;   // still "measuring"
            double sum = 0.0;
            for (double p : ring)
                sum += p;
            const double power = sum / (double) stepsInWindow;
            const float lufs = power > 1.0e-12 ? (float) powerToLufs (power) : -100.0f;
            published.store (lufs, std::memory_order_relaxed);
        }

        struct Channel { Songsmith::KWeighting::Biquad shelf, highPass; };
        std::array<Channel, maxChannels> channels;
        std::array<double, stepsInWindow> ring {};
        double fs = 48000.0, stepPower = 0.0;
        int stepSamples = 4800, stepCount = 0, ringPos = 0, filled = 0;
        std::atomic<float> published { notMeasured };
    };

    //==============================================================================
    // The pair. A plugin owns one, calls prepare() from prepareToPlay, and puts one Scope at the top of processBlock.
    class BeforeAfterMeter
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            before.prepare (sampleRate);
            after.prepare (sampleRate);
            wasPlaying = false;
        }

        void reset() noexcept { before.reset(); after.reset(); }

        float getBefore() const noexcept { return before.get(); }
        float getAfter() const noexcept  { return after.get(); }

        //==========================================================================
        // Measures the buffer as it arrives (Before) and as it leaves (After): construct at the top of processBlock, and the
        // end of the block, including every early return, takes the second reading. 'numChannels' is the number of INPUT
        // channels (the buffer may hold more output channels than that).
        class Scope
        {
        public:
            Scope (BeforeAfterMeter& m, juce::AudioBuffer<float>& b, int numChannels, juce::AudioPlayHead* playHead) noexcept
                : meter (m), buffer (b), channels (juce::jmin (numChannels, b.getNumChannels(), ShortTermMeter::maxChannels))
            {
                bool playing = true;   // no transport (a standalone app) counts as playing
                if (playHead != nullptr)
                    if (const auto position = playHead->getPosition())
                        playing = position->getIsPlaying();

                if (playing && ! meter.wasPlaying)
                    meter.reset();   // a new take starts afresh
                meter.wasPlaying = playing;
                active = playing;

                if (active)
                    meter.before.process (buffer.getArrayOfReadPointers(), channels, buffer.getNumSamples());
            }

            ~Scope()
            {
                if (active)
                    meter.after.process (buffer.getArrayOfReadPointers(), channels, buffer.getNumSamples());
            }

        private:
            BeforeAfterMeter& meter;
            juce::AudioBuffer<float>& buffer;
            int channels;
            bool active = true;
        };

    private:
        ShortTermMeter before, after;
        bool wasPlaying = false;   // audio thread only
    };

    //==============================================================================
    // Text. The same for every plugin; the tests read these.
    inline juce::String readingText (float lufs)
    {
        if (std::isnan (lufs))
            return juce::String::fromUTF8 ("Measuring\xe2\x80\xa6");   // "Measuring…"
        if (lufs <= silenceLufs)
            return juce::String::fromUTF8 ("\xe2\x80\x94");            // an em dash: no sound
        return juce::String (lufs, 1) + " LUFS";
    }

    inline juce::String tooltipText()
    {
        return "Loudness before and after this plugin, as short-term LUFS: the average of the last 3 seconds, weighted the way the ear hears loudness. "
               "It measures this plugin only, not the whole track. It shows a dash when there is no sound, and Measuring... for the first 3 seconds after playback starts. "
               "Nothing to do: use it to compare, and trust your ears.";
    }

    //==============================================================================
    // The shared readout: "Before" and its reading, "After" and its reading, on one line. It is fixed in width, so the numbers never
    // make the strip jump about. A plugin never builds one itself: the footer strip (Songsmith::Diagnostics::Bar) does.
    class Display final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Display (const BeforeAfterMeter& m) : meter (m)
        {
            setTooltip (tooltipText());
            setTitle ("Loudness before and after this plugin");
            setInterceptsMouseClicks (true, false);
            update();
        }

        static juce::Font labelFont() { return juce::Font (UI::font (UI::Type::control, UI::Weight::Medium)); }
        static juce::Font valueFont() { return juce::Font (UI::numbers (UI::Type::control)); }

        static float widthOf (const juce::String& text, const juce::Font& f)
        {
            juce::GlyphArrangement g;
            g.addLineOfText (f, text, 0.0f, 0.0f);
            return g.getBoundingBox (0, -1, false).getWidth();
        }

        // The widest a reading can be ("Measuring…" or "-88.8 LUFS"), so the width never changes.
        static int valueWidth()
        {
            return (int) std::ceil (juce::jmax (widthOf (readingText (notMeasured), valueFont()), widthOf (readingText (-88.8f), valueFont()),
                                                widthOf (readingText (-100.0f), valueFont())));
        }
        static int labelWidth() { return (int) std::ceil (juce::jmax (widthOf ("Before", labelFont()), widthOf ("After", labelFont()))); }
        static constexpr int gap = 6, between = 16;
        static int preferredWidth() { return 2 * (labelWidth() + gap + valueWidth()) + between + 4; }

        // Re-reads the meter; repaints only if what is shown changed. Message thread, a few times a second.
        void update()
        {
            const auto b = readingText (meter.getBefore()), a = readingText (meter.getAfter());
            if (b != beforeText || a != afterText)
            {
                beforeText = b;
                afterText = a;
                repaint();
            }
        }

        const juce::String& getBeforeText() const noexcept { return beforeText; }
        const juce::String& getAfterText() const noexcept  { return afterText; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            const int lw = labelWidth(), vw = valueWidth();
            auto group = [&] (const juce::String& label, const juce::String& value)
            {
                auto row = r.removeFromLeft (lw + gap + vw);
                g.setFont (labelFont());
                g.setColour (UI::Colours::textDim);
                g.drawText (label, row.removeFromLeft (lw), juce::Justification::centredLeft, false);
                row.removeFromLeft (gap);
                g.setFont (valueFont());
                g.setColour (UI::Colours::text);
                g.drawText (value, row, juce::Justification::centredLeft, false);
            };
            r.removeFromLeft (2);
            group ("Before", beforeText);
            r.removeFromLeft (between);
            group ("After", afterText);
        }

    private:
        const BeforeAfterMeter& meter;
        juce::String beforeText, afterText;
    };
}
