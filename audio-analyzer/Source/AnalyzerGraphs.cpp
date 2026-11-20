#include "AnalyzerGraphs.h"
#include "AnalyzerText.h"
#include "../../shared/Source/SongsmithLookAndFeel.h"
#include "../../shared/Source/SongsmithOutputMeter.h"

namespace Songsmith::Analyzer
{
    namespace
    {
        void styleLabel (juce::Label& l, float height, UI::Weight weight, juce::Colour colour, juce::Justification j = juce::Justification::centredLeft)
        {
            l.setFont (UI::font (height, weight));
            l.setColour (juce::Label::textColourId, colour);
            l.setJustificationType (j);
            l.setBorderSize ({ 0, 0, 0, 0 });
            l.setMinimumHorizontalScale (1.0f);
            l.setInterceptsMouseClicks (false, false);
        }

        juce::String signedOneDecimal (double db)
        {
            return (db > 0.05 ? "+" : "") + oneDecimal (db);
        }
    }

    //==============================================================================
    TruePeakDisplay::TruePeakDisplay()
    {
        setTitle ("True peak chart");
        addAndMakeVisible (heading);
        addAndMakeVisible (legend);
        styleLabel (heading, UI::Type::hint, UI::Weight::Medium, UI::Colours::textDim);
        styleLabel (legend, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim);
        heading.setText ("Highest peak in the file", juce::dontSendNotification);
        legend.setText ("Bar: highest true peak. Ticks: highest stored sample.", juce::dontSendNotification);
        describe();
    }

    void TruePeakDisplay::clear()
    {
        bars.clear();
        silent = false;
        describe();
        repaint();
    }

    void TruePeakDisplay::setResult (const FileAnalysis::Result& r)
    {
        bars.clear();
        silent = false;
        const size_t n = r.opened ? r.channelTruePeakDb.size() : 0;
        if (n == 1)
            bars.push_back ({ "Mono", r.channelTruePeakDb[0], r.channelSamplePeakDb[0] });
        else if (n == 2)
        {
            bars.push_back ({ "Left", r.channelTruePeakDb[0], r.channelSamplePeakDb[0] });
            bars.push_back ({ "Right", r.channelTruePeakDb[1], r.channelSamplePeakDb[1] });
        }
        else if (n > 2)
        {
            // More than two channels: one bar for the highest of all of them.
            double t = -std::numeric_limits<double>::infinity(), s = t;
            for (size_t c = 0; c < n; ++c) { t = juce::jmax (t, r.channelTruePeakDb[c]); s = juce::jmax (s, r.channelSamplePeakDb[c]); }
            bars.push_back ({ "All", t, s });
        }
        silent = r.opened && r.silent;
        describe();
        resized();
        repaint();
    }

    juce::String TruePeakDisplay::getBarReadout (int bar) const
    {
        const auto db = bars[(size_t) bar].truePeakDb;
        return std::isfinite (db) ? signedOneDecimal (db) : juce::String ("Silent");
    }

    bool TruePeakDisplay::isOverFullScale() const noexcept
    {
        for (auto& b : bars)
            if (PeakScale::isOverFullScale (b.truePeakDb))
                return true;
        return false;
    }

    void TruePeakDisplay::describe()
    {
        setTooltip ("How high the loudest moment in the whole file reaches, for each channel, on a scale from -48 to +6 dBTP. "
                    "0 dBTP is the top of the scale, and a bar that goes above it turns red. The bar is the highest true peak, which is the highest "
                    "level once the stored samples are turned back into a smooth wave. The small ticks beside it are the highest stored sample, "
                    "which can be a little lower. This is the highest point in the whole file, not a live meter.");
        if (bars.empty())
        {
            setDescription ("True peak chart. There is nothing to show yet.");
            return;
        }
        if (silent)
        {
            setDescription ("True peak chart. The file is silent, so there is no peak.");
            return;
        }
        juce::String text = "True peak chart, highest peak in the whole file. ";
        for (size_t i = 0; i < bars.size(); ++i)
        {
            text << bars[i].name << ": " << getBarReadout ((int) i) << " dBTP, highest stored sample "
                 << (std::isfinite (bars[i].samplePeakDb) ? oneDecimal (bars[i].samplePeakDb) + " dBFS" : juce::String ("none")) << ". ";
            if (PeakScale::isOverFullScale (bars[i].truePeakDb))
                text << "That is above the top of the scale. ";
        }
        setDescription (text.trim());
    }

    //------------------------------------------------------------------------------
    namespace
    {
        constexpr float scaleWidth = 34.0f, scaleGap = 8.0f, barWidth = 44.0f, barGap = 14.0f;
        constexpr float headingTop = 8.0f, namesTop = 30.0f, barsTop = 48.0f, bottomReserve = 60.0f, legendHeight = 30.0f;
    }

    juce::Rectangle<float> TruePeakDisplay::getScaleBounds() const
    {
        const int count = juce::jmax (2, getNumBars());
        const float total = scaleWidth + scaleGap + (float) count * barWidth + (float) (count - 1) * barGap;
        const float x0 = std::floor (((float) getWidth() - total) * 0.5f);
        return { x0, barsTop, scaleWidth, (float) getHeight() - bottomReserve - barsTop };
    }

    juce::Rectangle<float> TruePeakDisplay::getBarBounds (int bar) const
    {
        const auto s = getScaleBounds();
        return { s.getRight() + scaleGap + (float) bar * (barWidth + barGap), s.getY(), barWidth, s.getHeight() };
    }

    float TruePeakDisplay::yForDb (double db) const noexcept
    {
        const auto s = getScaleBounds();
        return s.getBottom() - s.getHeight() * PeakScale::proportion (db);
    }

    void TruePeakDisplay::resized()
    {
        heading.setBounds (12, (int) headingTop, getWidth() - 24, 16);
        legend.setBounds (12, getHeight() - (int) legendHeight - 6, getWidth() - 24, (int) legendHeight);
    }

    void TruePeakDisplay::paint (juce::Graphics& g)
    {
        UI::LookAndFeelBase::drawWell (g, getLocalBounds().toFloat().reduced (0.5f), UI::Colours::recess);

        const auto scale = getScaleBounds();
        const auto labelFont = juce::Font (UI::numbers (UI::Type::caption));
        const float zeroY = yForDb (0.0);
        const int count = juce::jmax (2, getNumBars());
        const float barsRight = getBarBounds (count - 1).getRight();

        // The scale: ticks and labels to the left of the bars, 0 dBTP drawn across them.
        g.setFont (labelFont);
        for (auto db : PeakScale::marks)
        {
            const float y = yForDb (db);
            g.setColour (db == 0.0f ? UI::zeroLineColour.withMultipliedAlpha (2.0f) : UI::gridMajorColour);
            g.fillRect (juce::Rectangle<float> (scale.getRight() - 4.0f, y - 0.5f, 4.0f, 1.0f));
            g.setColour (db == 0.0f ? UI::Colours::text : UI::Colours::textDim);
            g.drawText (db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db),
                        juce::Rectangle<float> (scale.getX(), y - 7.0f, scale.getWidth() - 6.0f, 14.0f), juce::Justification::centredRight, false);
        }

        for (int i = 0; i < getNumBars(); ++i)
        {
            const auto bar = getBarBounds (i);
            UI::Tube::paintHousing (g, bar, 2.0f, 1.0f);

            const double level = bars[(size_t) i].truePeakDb;
            if (std::isfinite (level))
            {
                const float y = yForDb (level);
                const auto fill = bar.withTop (y).reduced (1.0f);
                // The suite's meter colours: teal rising to yellow at 0 dBTP, and red above it.
                g.setGradientFill (juce::ColourGradient (PeakScale::low, 0.0f, bar.getBottom(), PeakScale::high, 0.0f, zeroY, false));
                g.fillRect (fill.withTop (juce::jmax (fill.getY(), zeroY)));
                if (fill.getY() < zeroY)
                {
                    g.setColour (PeakScale::over);
                    g.fillRect (fill.withBottom (zeroY));
                }
                // The highest point, held.
                g.setColour (PeakScale::isOverFullScale (level) ? PeakScale::over : UI::Colours::text);
                g.fillRect (juce::Rectangle<float> (bar.getX() + 1.0f, y - 1.0f, bar.getWidth() - 2.0f, 2.0f));

                // The highest stored sample: a small tick on each side, a different thing from the bar.
                const double sample = bars[(size_t) i].samplePeakDb;
                if (std::isfinite (sample))
                {
                    const float sy = yForDb (sample);
                    g.setColour (UI::Colours::text);
                    g.fillRect (juce::Rectangle<float> (bar.getX() - 5.0f, sy - 0.5f, 5.0f, 1.0f));
                    g.fillRect (juce::Rectangle<float> (bar.getRight(), sy - 0.5f, 5.0f, 1.0f));
                }
            }
            UI::Tube::paintFinish (g, bar, bar.reduced (1.0f), 2.0f);

            g.setFont (labelFont);
            g.setColour (UI::Colours::textDim);
            g.drawText (bars[(size_t) i].name, juce::Rectangle<float> (bar.getX() - 8.0f, namesTop, bar.getWidth() + 16.0f, 14.0f), juce::Justification::centred, false);
            g.setColour (PeakScale::isOverFullScale (level) ? PeakScale::over : UI::Colours::text);
            g.drawText (getBarReadout (i), juce::Rectangle<float> (bar.getX() - 8.0f, bar.getBottom() + 6.0f, bar.getWidth() + 16.0f, 14.0f), juce::Justification::centred, false);
        }

        // 0 dBTP across all the bars.
        g.setColour (UI::zeroLineColour);
        g.fillRect (juce::Rectangle<float> (getBarBounds (0).getX(), zeroY, barsRight - getBarBounds (0).getX(), 1.0f));
    }

    //==============================================================================
    WaveformDisplay::WaveformDisplay()
    {
        setTitle ("Waveform");
        for (auto* l : { &heading, &startLabel, &endLabel, &note })
            addAndMakeVisible (*l);
        styleLabel (heading, UI::Type::hint, UI::Weight::Medium, UI::Colours::textDim);
        styleLabel (startLabel, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim);
        styleLabel (endLabel, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim, juce::Justification::centredRight);
        styleLabel (note, UI::Type::hint, UI::Weight::Regular, UI::Colours::textDim);
        heading.setText ("Waveform: how big the sound is through the song", juce::dontSendNotification);
        note.setText ("Taller means a bigger sound. This is not a loudness chart.", juce::dontSendNotification);
        describe();
    }

    void WaveformDisplay::clear()
    {
        peaks.clear();
        durationSeconds = 0.0;
        silent = false;
        startLabel.setText ({}, juce::dontSendNotification);
        endLabel.setText ({}, juce::dontSendNotification);
        describe();
        repaint();
    }

    void WaveformDisplay::setResult (const FileAnalysis::Result& r)
    {
        peaks = r.opened ? r.waveformPeaks : std::vector<float>();
        durationSeconds = r.opened ? r.durationSeconds : 0.0;
        silent = r.opened && r.silent;
        startLabel.setText (peaks.empty() ? juce::String() : "0:00", juce::dontSendNotification);
        endLabel.setText (peaks.empty() ? juce::String() : formatLength (durationSeconds), juce::dontSendNotification);
        note.setText (silent ? "The file is silent, so there is no waveform to draw." : "Taller means a bigger sound. This is not a loudness chart.", juce::dontSendNotification);
        describe();
        repaint();
    }

    double WaveformDisplay::getLoudestPosition() const noexcept
    {
        if (peaks.empty())
            return 0.0;
        size_t best = 0;
        for (size_t i = 1; i < peaks.size(); ++i)
            if (peaks[i] > peaks[best])
                best = i;
        return ((double) best + 0.5) / (double) peaks.size();
    }

    void WaveformDisplay::describe()
    {
        setTooltip ("A picture of how big the sound is from the start of the file to the end. Taller means a bigger sound at that moment. "
                    "It is not a loudness chart, and it does not show true peak. Red marks a moment that goes past the top.");
        if (peaks.empty())
        {
            setDescription ("Waveform. There is nothing to show yet.");
            return;
        }
        if (silent)
        {
            setDescription ("Waveform. The file is silent.");
            return;
        }
        setDescription ("Waveform of the whole file, " + formatLength (durationSeconds) + " long. The biggest sound is about "
                        + juce::String (juce::roundToInt (getLoudestPosition() * 100.0)) + " percent of the way through. This shows size, not loudness.");
    }

    juce::Rectangle<float> WaveformDisplay::getPlotBounds() const
    {
        return { 12.0f, 30.0f, (float) getWidth() - 24.0f, (float) getHeight() - 30.0f - 40.0f };
    }

    float WaveformDisplay::heightOfSlice (int index) const
    {
        const float p = peaks[(size_t) index];
        if (p <= 0.0f)
            return 0.0f;
        const auto plot = getPlotBounds();
        return juce::jmax (1.0f, plot.getHeight() * 0.5f * juce::jmin (p, 1.0f));
    }

    void WaveformDisplay::resized()
    {
        heading.setBounds (12, 8, getWidth() - 24, 16);
        startLabel.setBounds (12, getHeight() - 38, 90, 14);
        endLabel.setBounds (getWidth() - 102, getHeight() - 38, 90, 14);
        note.setBounds (12, getHeight() - 22, getWidth() - 24, 14);
    }

    void WaveformDisplay::paint (juce::Graphics& g)
    {
        UI::LookAndFeelBase::drawWell (g, getLocalBounds().toFloat().reduced (0.5f), UI::Colours::recess);
        const auto plot = getPlotBounds();

        g.setColour (UI::gridMajorColour);
        g.fillRect (plot.withHeight (1.0f));                       // the top edge: full scale
        g.fillRect (plot.withTop (plot.getBottom() - 1.0f));       // and the bottom edge
        const float mid = plot.getCentreY();
        g.setColour (UI::Colours::edge);
        g.fillRect (juce::Rectangle<float> (plot.getX(), mid - 0.5f, plot.getWidth(), 1.0f));

        if (peaks.empty() || silent)
            return;

        const float step = plot.getWidth() / (float) peaks.size();
        for (int i = 0; i < (int) peaks.size(); ++i)
        {
            const float h = heightOfSlice (i);
            if (h <= 0.0f)
                continue;
            g.setColour (peaks[(size_t) i] > 1.0f ? PeakScale::over : Colours::accent.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (plot.getX() + (float) i * step, mid - h, step + 0.4f, h * 2.0f));
        }
    }
}
