#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <optional>
#include "SongsmithColours.h"
#include "SongsmithLookAndFeel.h"
#include "SongsmithType.h"

// The diagnostics every Songsmith plugin shows in a strip along the bottom of its editor:
//   Latency          the delay this instance reports to the host, in samples and milliseconds.
//   Processing Load  optional, off by default: how much of the audio block's time this instance uses.
//
// Real-time rules (see CLAUDE.md): the audio thread only reads the clock and writes two atomics, with no
// allocation, lock, logging or file access. When Processing Load is off, a block costs one relaxed
// atomic read. The editor reads the published values on the message thread.
//
// The on/off choice is a machine setting in its own file (Diagnostics.xml, next to the plugin's
// presets folder, never inside it). It is not a parameter and not saved with sessions or presets,
// so it never changes how a session loads and needs no stateVersion change.
namespace Songsmith::Diagnostics
{
    namespace UI = Songsmith::UI;
    namespace Colours = Songsmith::UI::Colours;

    //==============================================================================
    // Pure maths and text, so the tests can check them without a window or a clock.

    inline double blockSeconds (int numSamples, double sampleRate) noexcept
    {
        return (numSamples > 0 && sampleRate > 0.0) ? (double) numSamples / sampleRate : 0.0;
    }

    // The share of the block's time budget that one block took: 1.0 means the whole budget.
    inline float loadFraction (double elapsedSeconds, int numSamples, double sampleRate) noexcept
    {
        const double budget = blockSeconds (numSamples, sampleRate);
        return (budget > 0.0 && elapsedSeconds >= 0.0) ? (float) (elapsedSeconds / budget) : 0.0f;
    }

    // One step of an exponential average that settles in about the same time (timeConstant seconds)
    // whatever the block size, so a 32-sample and a 2048-sample buffer read alike.
    inline float smoothStep (float smoothed, float instant, double seconds, double timeConstant = 0.5) noexcept
    {
        const double alpha = seconds > 0.0 ? 1.0 - std::exp (-seconds / timeConstant) : 0.0;
        return smoothed + (float) alpha * (instant - smoothed);
    }

    // "2.7ms (128 samples)". The milliseconds use the sample rate the host is running at.
    inline juce::String latencyText (int samples, double sampleRate)
    {
        const juce::String count (samples);
        const juce::String unit = samples == 1 ? " sample" : " samples";
        if (sampleRate <= 0.0)
            return count + unit;
        return juce::String (1000.0 * (double) samples / sampleRate, 1) + "ms (" + count + unit + ")";
    }

    // Enough digits for light loads: "0.07%", "3.2%", "14%". A genuine zero reads "0%". No
    // measurement reads "Unavailable", never a made-up zero.
    inline juce::String loadText (std::optional<float> fraction)
    {
        if (! fraction.has_value())
            return "Unavailable";
        const double percent = 100.0 * (double) juce::jmax (0.0f, *fraction);
        if (percent <= 0.0)
            return "0%";
        const int decimals = percent < 1.0 ? 2 : (percent < 10.0 ? 1 : 0);
        return juce::String (percent, decimals) + "%";
    }

    // The whole readout beside the Processing Load switch: "CPU 0.35%, Latency 0.0ms (0 samples)".
    inline juce::String readoutText (std::optional<float> fraction, int samples, double sampleRate)
    {
        return "CPU " + loadText (fraction) + ", Latency " + latencyText (samples, sampleRate);
    }

    inline const char* latencyTooltip()
    {
        return "The delay this plugin adds. Your recording software may compensate for it.";
    }

    inline const char* loadTooltip()
    {
        return "Shows how much of the time available for each block of audio this plugin uses. "
               "The value describes this plugin only, and it varies with the computer, the settings and the buffer size.";
    }

    //==============================================================================
    // Measures one plugin instance's processBlock time. Written by the audio thread only; read by the editor.
    class LoadMeter
    {
    public:
        static constexpr double timeConstantSeconds = 0.5;   // how quickly the readout settles
        static constexpr double staleAfterSeconds = 1.0;     // no block for this long: unavailable

        // Message thread: turn timing on or off. Off costs the audio thread one relaxed read per block.
        void setEnabled (bool shouldMeasure) noexcept
        {
            enabled.store (shouldMeasure, std::memory_order_relaxed);
            if (! shouldMeasure)
                lastTicks.store (0, std::memory_order_relaxed);
        }

        bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

        // Audio thread: put one of these at the top of processBlock.
        class Scope
        {
        public:
            Scope() noexcept = default;
            Scope (LoadMeter& m, int samples, double rate) noexcept
                : meter (&m), numSamples (samples), sampleRate (rate), start (juce::Time::getHighResolutionTicks()) {}
            Scope (const Scope&) = delete;
            Scope& operator= (const Scope&) = delete;
            Scope (Scope&& other) noexcept
                : meter (other.meter), numSamples (other.numSamples), sampleRate (other.sampleRate), start (other.start)
            { other.meter = nullptr; }
            ~Scope()
            {
                if (meter != nullptr)
                    meter->record (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start),
                                   numSamples, sampleRate);
            }

        private:
            LoadMeter* meter = nullptr;
            int numSamples = 0;
            double sampleRate = 0.0;
            juce::int64 start = 0;
        };

        Scope measure (int numSamples, double sampleRate) noexcept
        {
            if (! enabled.load (std::memory_order_relaxed))
            {
                hasValue = false;   // start afresh next time it is switched on
                return {};
            }
            return Scope (*this, numSamples, sampleRate);
        }

        // Audio thread. Public so the tests can feed known durations.
        void record (double elapsedSeconds, int numSamples, double sampleRate) noexcept
        {
            const double seconds = blockSeconds (numSamples, sampleRate);
            if (seconds <= 0.0)
                return;

            const float instant = loadFraction (elapsedSeconds, numSamples, sampleRate);
            state = hasValue ? smoothStep (state, instant, seconds, timeConstantSeconds) : instant;
            hasValue = true;

            smoothed.store (state, std::memory_order_relaxed);
            lastTicks.store (juce::Time::getHighResolutionTicks(), std::memory_order_relaxed);
        }

        // Message thread: the smoothed load as a fraction of the block's time (1.0 = all of it), or
        // nothing when timing is off or no block has been measured in the last second.
        std::optional<float> read (juce::int64 nowTicks = juce::Time::getHighResolutionTicks()) const noexcept
        {
            if (! isEnabled())
                return std::nullopt;
            const auto last = lastTicks.load (std::memory_order_relaxed);
            if (last == 0 || juce::Time::highResolutionTicksToSeconds (nowTicks - last) > staleAfterSeconds)
                return std::nullopt;
            return smoothed.load (std::memory_order_relaxed);
        }

    private:
        std::atomic<bool> enabled { false };
        std::atomic<float> smoothed { 0.0f };
        std::atomic<juce::int64> lastTicks { 0 };   // 0: nothing measured yet
        float state = 0.0f;                         // audio thread only
        bool hasValue = false;                      // audio thread only
    };

    //==============================================================================
    // The machine setting, in its own file so it is never part of a session or a preset.
    namespace Settings
    {
        inline juce::File& fileOverrideForTests() { static juce::File f; return f; }

        inline juce::File file (const juce::String& pluginName)
        {
            if (fileOverrideForTests() != juce::File())
                return fileOverrideForTests();
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Songsmith Studio").getChildFile (pluginName).getChildFile ("Diagnostics.xml");
        }

        inline bool loadEnabled (const juce::String& pluginName)
        {
            const auto f = file (pluginName);
            if (! f.existsAsFile())
                return false;   // off by default
            juce::PropertiesFile settings (f, {});
            return settings.getBoolValue ("processingLoad", false);
        }

        inline void saveEnabled (const juce::String& pluginName, bool on)
        {
            const auto f = file (pluginName);
            f.getParentDirectory().createDirectory();
            juce::PropertiesFile::Options options;
            options.millisecondsBeforeSaving = 0;
            juce::PropertiesFile settings (f, options);
            settings.setValue ("processingLoad", on);
            settings.saveIfNeeded();
        }
    }

    //==============================================================================
    // The strip (SUI-12): the Processing Load switch at the far left and, only while it is on, the
    // CPU and latency readout directly to its right. The right end is left free for a plugin's own
    // control that belongs beside what it affects (setTrailingControl: the EQ's Piano button).
    // Laid out at a fixed design size, so it scales with the editor like everything else.
    // SUI-15: the product version, as "v0.5.0". It comes from the build's one version (the root VERSION
    // file, via CMake), never from a second hardcoded number.
    inline juce::String productVersionString()
    {
       #if defined (JucePlugin_VersionString)
        return juce::String (JucePlugin_VersionString);
       #elif defined (JUCE_APPLICATION_VERSION_STRING)
        return juce::String (JUCE_APPLICATION_VERSION_STRING);   // the test executables
       #else
        return {};
       #endif
    }

    inline juce::String versionLabelText() { return "v" + productVersionString(); }

    class Bar : public juce::Component, private juce::Timer
    {
    public:
        static constexpr int designHeight = 28;

        Bar (juce::AudioProcessor& processorToShow, LoadMeter& meterToShow, const juce::String& pluginName)
            : processor (processorToShow), meter (meterToShow), name (pluginName)
        {
            setupLabel (readout, {}, UI::Weight::Regular, Colours::text);
            readout.setTooltip (latencyTooltip());
            readout.setInterceptsMouseClicks (true, false);

            setupLabel (versionLabel, versionLabelText(), UI::Weight::Regular, Colours::textDim);
            versionLabel.setTitle ("Version " + productVersionString());
            versionLabel.setTooltip ("The Songsmith Plugins version you are running.");
            versionLabel.setInterceptsMouseClicks (true, false);
            addAndMakeVisible (versionLabel);

            loadSwitch.setButtonText ("Processing Load");
            loadSwitch.setTitle ("Processing Load");
            loadSwitch.setClickingTogglesState (true);
            loadSwitch.setTooltip (loadTooltip());
            loadSwitch.setToggleState (Settings::loadEnabled (name), juce::dontSendNotification);
            loadSwitch.onClick = [this]
            {
                Settings::saveEnabled (name, loadSwitch.getToggleState());
                meter.setEnabled (loadSwitch.getToggleState());
                refresh();
            };
            addAndMakeVisible (loadSwitch);
            meter.setEnabled (loadSwitch.getToggleState());

            addAndMakeVisible (readout);

            refresh();
            startTimerHz (4);
        }

        ~Bar() override
        {
            stopTimer();
            meter.setEnabled (false);   // nobody is watching: the audio thread stops timing
        }

        // The texts as shown now (the tests compare them with what the processor reports).
        // The latency part is always current, even while the readout is hidden.
        juce::String getLatencyText() const { return latencyText (processor.getLatencySamples(), processor.getSampleRate()); }
        juce::String getLoadText() const    { return meter.isEnabled() ? loadText (meter.read()) : juce::String ("Off"); }
        juce::String getReadoutText() const { return readout.getText(); }
        bool isReadoutShown() const         { return readout.isVisible(); }
        juce::TextButton& getLoadSwitch()   { return loadSwitch; }
        juce::Component& getReadout()       { return readout; }
        juce::Label& getVersionLabel()      { return versionLabel; }

        // Lets the tests check longer versions than today's; the footer lays itself out again.
        void setVersionTextForTesting (const juce::String& text)
        {
            versionLabel.setText (text, juce::dontSendNotification);
            resized();
        }

        // A plugin's own control for the right end of the strip. The bar lays it out right-aligned at
        // the component's current size and does not own it.
        void setTrailingControl (juce::Component* c)
        {
            trailing = c;
            if (c != nullptr)
                addAndMakeVisible (*c);
            resized();
        }

        void refresh()
        {
            const bool show = meter.isEnabled();
            readout.setVisible (show);
            setText (readout, show ? readoutText (meter.read(), processor.getLatencySamples(), processor.getSampleRate())
                                   : juce::String());
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (Colours::edge);
            g.fillRect (0, 0, getWidth(), 1);   // a rule above the strip
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 2).withTrimmedTop (1);
            // version -> Processing Load switch -> readout, in that order from the left
            const int versionWidth = getVersionWidth();
            versionLabel.setBounds (r.removeFromLeft (versionWidth));
            r.removeFromLeft (8);
            loadSwitch.setBounds (r.removeFromLeft (130).withSizeKeepingCentre (130, 22));
            r.removeFromLeft (10);

            if (trailing != nullptr)
            {
                const int w = trailing->getWidth() > 0 ? trailing->getWidth() : 80;
                trailing->setBounds (r.removeFromRight (w).withSizeKeepingCentre (w, 22));
                r.removeFromRight (10);
            }
            readout.setBounds (r);
        }

    private:
        void timerCallback() override { refresh(); }

        int getVersionWidth() const
        {
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText (versionLabel.getFont(), versionLabel.getText(), 0.0f, 0.0f);
            // A juce::Label keeps its own border around the text and truncates with dots when the text
            // area (width minus border) is narrower than the text, so add the border plus a safety margin.
            const auto border = versionLabel.getBorderSize();
            return (int) std::ceil (glyphs.getBoundingBox (0, -1, false).getWidth())
                   + border.getLeftAndRight() + 6;
        }

        void setupLabel (juce::Label& l, const juce::String& text, UI::Weight weight, juce::Colour colour)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (juce::Font (weight == UI::Weight::Regular ? UI::numbers (UI::Type::control, weight)
                                                                 : UI::font (UI::Type::control, weight)));
            l.setColour (juce::Label::textColourId, colour);
            l.setJustificationType (juce::Justification::centredLeft);
            l.setInterceptsMouseClicks (false, false);
            l.setMinimumHorizontalScale (1.0f);
        }

        static void setText (juce::Label& l, const juce::String& text)
        {
            if (l.getText() != text)
                l.setText (text, juce::dontSendNotification);
        }

        juce::AudioProcessor& processor;
        LoadMeter& meter;
        juce::String name;
        juce::Label readout;
        juce::Label versionLabel;
        juce::Component* trailing = nullptr;
        juce::TextButton loadSwitch;
    };
}
