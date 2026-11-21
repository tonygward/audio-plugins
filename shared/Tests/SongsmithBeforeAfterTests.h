#pragma once

// songsmithstudio#7: shared checks for the Before and After loudness, compiled into every plugin's test executable:
//   static SongsmithBeforeAfterTests<MyProcessor, MyEditor> sharedBeforeAfterTests { "MyPlugin" };
// The processor must expose `beforeAfter` (and a host Bypass parameter); the editor's footer must hold the readout.
// The measurement is the same code in every plugin, so its mathematics is tested here against published reference values.

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <cmath>
#include <limits>
#include "../Source/SongsmithBeforeAfter.h"
#include "../Source/SongsmithDiagnostics.h"
#include "SongsmithWording.h"

template <class Processor, class Editor>
class SongsmithBeforeAfterTests final : public juce::UnitTest
{
public:
    explicit SongsmithBeforeAfterTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith Before and After loudness (songsmithstudio#7)", category) {}

    // A pretend host transport.
    struct Transport final : juce::AudioPlayHead
    {
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            return info;
        }
    };

    static std::vector<float> sine (int n, double fs, double peakDb, double freq = 1000.0)
    {
        const double a = std::pow (10.0, peakDb / 20.0);
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = (float) (a * std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / fs));
        return v;
    }

    // Feeds one reading's worth through a meter, in blocks of 'block' samples; 'channels' copies of the same signal.
    static void feed (Songsmith::Loudness::ShortTermMeter& m, const std::vector<float>& x, int channels, int block)
    {
        for (size_t pos = 0; pos < x.size(); pos += (size_t) block)
        {
            const int n = (int) std::min<size_t> ((size_t) block, x.size() - pos);
            const float* ch[2] = { x.data() + pos, x.data() + pos };
            m.process (ch, channels, n);
        }
    }

    void runTest() override
    {
        using namespace Songsmith::Loudness;

        beginTest ("Reference signals: a 1 kHz sine at -23 dBFS reads -23.0 LUFS in stereo and -26.0 LUFS in mono, as the EBU test signals say");
        for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        {
            ShortTermMeter stereo, mono;
            stereo.prepare (fs);
            mono.prepare (fs);
            const auto x = sine ((int) (fs * 5.0), fs, -23.0);
            feed (stereo, x, 2, 512);
            feed (mono, x, 1, 512);
            expectWithinAbsoluteError (stereo.get(), -23.0f, 0.1f, "stereo at " + juce::String (fs) + " Hz");
            expectWithinAbsoluteError (mono.get(), -26.0f, 0.1f, "mono at " + juce::String (fs) + " Hz");
        }

        beginTest ("A gain change moves the reading by exactly that gain, and low and high tones are weighted as K-weighting says");
        {
            const double fs = 48000.0;
            float base = 0.0f;
            for (double gain : { -12.0, -6.0, 0.0, 6.0, 12.0 })
            {
                ShortTermMeter m;
                m.prepare (fs);
                feed (m, sine ((int) (fs * 4.0), fs, -23.0 + gain), 2, 480);
                if (gain == 0.0)
                    base = m.get();
                expectWithinAbsoluteError (m.get(), -23.0f + (float) gain, 0.1f, "gain " + juce::String (gain) + " dB");
            }
            // 100 Hz is weighted down a little, 10 kHz up a little, by the K-weighting response (the same filter the EQ and Maximiser use).
            for (double freq : { 100.0, 500.0, 4000.0, 10000.0 })
            {
                ShortTermMeter m;
                m.prepare (fs);
                feed (m, sine ((int) (fs * 4.0), fs, -23.0, freq), 2, 480);
                const float expected = -23.0f + (float) (Songsmith::KWeighting::magnitudeDb (fs, freq) - Songsmith::KWeighting::magnitudeDb (fs, 1000.0));
                expectWithinAbsoluteError (m.get() - base, expected + 23.0f, 0.1f, "weighting at " + juce::String (freq) + " Hz");
            }
        }

        beginTest ("Placeholders: Measuring until the 3-second window is full, a dash for silence, and a number after");
        {
            ShortTermMeter m;
            m.prepare (48000.0);
            const auto x = sine (48000, 48000.0, -20.0);
            expect (std::isnan (m.get()), "nothing yet: measuring");
            feed (m, x, 2, 480);
            feed (m, x, 2, 480);
            expect (std::isnan (m.get()), "after 2 seconds the window is not full: measuring");
            feed (m, x, 2, 480);
            expect (! std::isnan (m.get()) && m.get() > -25.0f, "after 3 seconds there is a number");
            expectEquals (readingText (notMeasured), juce::String::fromUTF8 ("Measuring\xe2\x80\xa6"));
            expectEquals (readingText (m.get()), juce::String (m.get(), 1) + " LUFS");

            ShortTermMeter silent;
            silent.prepare (48000.0);
            feed (silent, std::vector<float> (48000 * 4, 0.0f), 2, 480);
            expect (silent.get() <= silenceLufs, "silence reads below -70");
            expectEquals (readingText (silent.get()), juce::String::fromUTF8 ("\xe2\x80\x94"));
            expectEquals (readingText (-70.0f), juce::String::fromUTF8 ("\xe2\x80\x94"));
            expectEquals (readingText (-23.04f), juce::String ("-23.0 LUFS"));
        }

        beginTest ("The window forgets: after 3 seconds of quieter sound the reading is the quieter level, and Reset starts over");
        {
            ShortTermMeter m;
            m.prepare (48000.0);
            feed (m, sine (48000 * 4, 48000.0, -10.0), 2, 480);
            expectWithinAbsoluteError (m.get(), -10.0f, 0.1f);
            feed (m, sine (48000 * 3, 48000.0, -30.0), 2, 480);
            expectWithinAbsoluteError (m.get(), -30.0f, 0.15f, "only the last 3 seconds count");
            m.reset();
            expect (std::isnan (m.get()), "reset: measuring again");
        }

        beginTest ("The result does not depend on the host's block size");
        {
            const auto x = sine (48000 * 4, 48000.0, -17.0, 440.0);
            float first = 0.0f;
            for (int block : { 1, 7, 64, 480, 1000, 4096, 48000 })
            {
                ShortTermMeter m;
                m.prepare (48000.0);
                feed (m, x, 2, block);
                if (block == 1)
                    first = m.get();
                expectEquals (m.get(), first, "blocks of " + juce::String (block) + " read the same as blocks of 1");
            }
        }

        beginTest ("Stereo is 3 dB louder than the same signal in mono, and a damaged sample is treated as silence");
        {
            ShortTermMeter s, m;
            s.prepare (48000.0);
            m.prepare (48000.0);
            const auto x = sine (48000 * 4, 48000.0, -20.0);
            feed (s, x, 2, 480);
            feed (m, x, 1, 480);
            expectWithinAbsoluteError (s.get() - m.get(), 3.01f, 0.02f);

            ShortTermMeter d;
            d.prepare (48000.0);
            auto y = x;
            for (size_t i = 0; i < y.size(); i += 97)
                y[i] = (i % 2 == 0) ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
            feed (d, y, 2, 480);
            expect (std::isfinite (d.get()), "damaged input still gives a finite reading");
        }

        beginTest ("Before is what arrives and After is what leaves: a 6 dB cut inside the block shows as exactly 6 dB, and bypass is equal");
        {
            BeforeAfterMeter meter;
            meter.prepare (48000.0);
            const auto x = sine (480, 48000.0, -14.0);
            juce::AudioBuffer<float> buffer (2, 480);
            for (int blockIndex = 0; blockIndex < 450; ++blockIndex)
            {
                for (int c = 0; c < 2; ++c)
                    buffer.copyFrom (c, 0, x.data(), 480);
                {
                    BeforeAfterMeter::Scope scope (meter, buffer, 2, nullptr);
                    buffer.applyGain (0.5f);   // what the plugin does to the sound
                }
            }
            expectWithinAbsoluteError (meter.getBefore(), -14.0f, 0.1f, "before");
            expectWithinAbsoluteError (meter.getAfter(), -20.02f, 0.1f, "after, 6 dB lower");

            BeforeAfterMeter bypassed;
            bypassed.prepare (48000.0);
            for (int blockIndex = 0; blockIndex < 450; ++blockIndex)
            {
                for (int c = 0; c < 2; ++c)
                    buffer.copyFrom (c, 0, x.data(), 480);
                BeforeAfterMeter::Scope scope (bypassed, buffer, 2, nullptr);   // the plugin leaves the sound alone
            }
            expectEquals (bypassed.getBefore(), bypassed.getAfter(), "bypass: Before and After are the same number");

            // An early return in the plugin still takes the After reading (the scope's end does it).
            BeforeAfterMeter early;
            early.prepare (48000.0);
            auto run = [&] { BeforeAfterMeter::Scope scope (early, buffer, 2, nullptr); if (buffer.getNumSamples() > 0) return; };
            for (int blockIndex = 0; blockIndex < 450; ++blockIndex)
            {
                for (int c = 0; c < 2; ++c)
                    buffer.copyFrom (c, 0, x.data(), 480);
                run();
            }
            expect (! std::isnan (early.getAfter()), "after is measured on every way out");
        }

        beginTest ("Transport: the readings hold while the host is stopped and begin afresh when it plays again");
        {
            BeforeAfterMeter meter;
            meter.prepare (48000.0);
            Transport transport;
            const auto x = sine (480, 48000.0, -18.0);
            juce::AudioBuffer<float> buffer (2, 480);
            auto play = [&] (int blocks, float scale)
            {
                for (int i = 0; i < blocks; ++i)
                {
                    for (int c = 0; c < 2; ++c)
                        buffer.copyFrom (c, 0, x.data(), 480);
                    buffer.applyGain (scale);
                    BeforeAfterMeter::Scope scope (meter, buffer, 2, &transport);
                }
            };
            play (450, 1.0f);
            const float held = meter.getBefore();
            expectWithinAbsoluteError (held, -18.0f, 0.1f);
            transport.playing = false;
            play (450, 0.0f);   // silence arrives while stopped
            expectEquals (meter.getBefore(), held, "stopped: the reading holds (silence does not wipe it)");
            transport.playing = true;
            play (10, 1.0f);
            expect (std::isnan (meter.getBefore()) && std::isnan (meter.getAfter()), "playing again: measuring afresh");
            play (450, 1.0f);
            expect (! std::isnan (meter.getBefore()), "and a number again after 3 seconds");
        }

        beginTest ("Real-time safety and cost: fixed memory, nothing that can block, and a small share of the audio's own time");
        {
            static_assert (noexcept (std::declval<ShortTermMeter&>().process (nullptr, 0, 0)), "process never throws");
            static_assert (std::is_trivially_destructible_v<std::array<double, stepsInWindow>>, "the window is a fixed array");
            expectLessThan ((int) sizeof (BeforeAfterMeter), 2048, "a fixed, small object: no buffers that grow");

            BeforeAfterMeter meter;
            meter.prepare (48000.0);
            juce::AudioBuffer<float> buffer (2, 512);
            const auto x = sine (512, 48000.0, -20.0);
            for (int c = 0; c < 2; ++c)
                buffer.copyFrom (c, 0, x.data(), 512);
            const int blocks = (int) (48000.0 * 20.0 / 512.0);
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < blocks; ++i)
                BeforeAfterMeter::Scope scope (meter, buffer, 2, nullptr);
            const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
            logMessage ("    Before and After meters: " + juce::String (seconds * 1000.0, 1) + " ms for 20 s of stereo audio ("
                        + juce::String (100.0 * seconds / 20.0, 3) + " % of one core)");
            expectLessThan (seconds, 1.0, "under 5 % of real time even on a slow machine");
        }

        beginTest ("The readout: fixed width whatever it shows, plain words, and the same texts as the meter gives");
        {
            BeforeAfterMeter meter;
            meter.prepare (48000.0);
            Display display (meter);
            expectEquals (display.getBeforeText(), juce::String::fromUTF8 ("Measuring\xe2\x80\xa6"));
            expectEquals (display.getAfterText(), juce::String::fromUTF8 ("Measuring\xe2\x80\xa6"));
            expect (display.getTooltip().contains ("short-term") && display.getTooltip().contains ("this plugin only"), "the tooltip says what it measures");
            expect (Songsmith::UI::Wording::findBanned (display.getTooltip()).isEmpty(), "no abbreviation in the tooltip: " + Songsmith::UI::Wording::findBanned (display.getTooltip()));
            expect (Display::valueWidth() >= (int) Display::widthOf (juce::String::fromUTF8 ("Measuring\xe2\x80\xa6"), Display::valueFont()), "room for the longest text");
            expect (Display::valueWidth() >= (int) Display::widthOf ("-88.8 LUFS", Display::valueFont()), "and for the largest number");
            expectEquals (display.getTitle(), juce::String ("Loudness before and after this plugin"));
        }

        //======================================================================
        // The plugin itself.
        beginTest ("In this plugin: Before reads what arrives, bypass makes After equal Before, and the footer shows both");
        {
            Processor p;
            const double fs = 48000.0;
            p.setRateAndBufferSizeDetails (fs, 480);
            p.prepareToPlay (fs, 480);
            const auto x = sine (480, fs, -23.0);
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> buffer (2, 480);
            auto run = [&] (int blocks)
            {
                for (int i = 0; i < blocks; ++i)
                {
                    for (int c = 0; c < 2; ++c)
                        buffer.copyFrom (c, 0, x.data(), 480);
                    p.processBlock (buffer, midi);
                }
            };
            run (500);   // 5 seconds
            expectWithinAbsoluteError (p.beforeAfter.getBefore(), -23.0f, 0.1f, "Before is the level that came in");
            expect (std::isfinite (p.beforeAfter.getAfter()), "After is a number");

            if (auto* bypass = p.getBypassParameter())
            {
                bypass->setValueNotifyingHost (1.0f);
                run (500);
                expectWithinAbsoluteError (p.beforeAfter.getAfter(), p.beforeAfter.getBefore(), 0.05f, "bypassed, the sound leaves as it came in");
                bypass->setValueNotifyingHost (0.0f);
            }

            std::unique_ptr<juce::AudioProcessorEditor> owner (p.createEditor());
            auto* ed = dynamic_cast<Editor*> (owner.get());
            expect (ed != nullptr);
            if (ed == nullptr)
                return;

            Songsmith::Loudness::Display* display = nullptr;
            Songsmith::Diagnostics::Bar* bar = nullptr;
            std::function<void (juce::Component&)> find = [&] (juce::Component& c)
            {
                if (auto* b = dynamic_cast<Songsmith::Diagnostics::Bar*> (&c))
                    bar = b;
                if (auto* d = dynamic_cast<Songsmith::Loudness::Display*> (&c))
                    display = d;
                for (auto* child : c.getChildren())
                    find (*child);
            };
            find (*ed);
            expect (display != nullptr && bar != nullptr, "the footer holds the shared Before and After readout");
            if (display == nullptr || bar == nullptr)
                return;

            display->update();
            expect (display->getBeforeText().endsWith ("LUFS"), "the readout shows Before: " + display->getBeforeText());
            expect (display->getAfterText().endsWith ("LUFS"), "and After: " + display->getAfterText());
            expect (display->getParentComponent() == bar && display->isVisible(), "it lives in the footer strip");
            expect (display->getRight() <= bar->getWidth() - 16 && display->getX() > bar->getLoadSwitch().getRight(), "inside the strip, after the Processing Load switch");
            expectEquals (display->getWidth(), Songsmith::Loudness::Display::preferredWidth(), "the same width in every plugin");
            bar->getLoadSwitch().setToggleState (true, juce::sendNotificationSync);
            bar->resized();
            bar->refresh();
            expect (bar->getReadout().getRight() <= display->getX(), "the readout stops before it");
            expect (bar->getReadout().getWidth() > 150, "and still has room for its numbers (" + juce::String (bar->getReadout().getWidth()) + " px)");
            bar->getLoadSwitch().setToggleState (false, juce::sendNotificationSync);
        }
    }
};
