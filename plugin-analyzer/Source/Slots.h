#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "AnalysisMath.h"
#include "AnalyzerParams.h"

// ANALYZER-01: the four named measurement slots, how they are saved with the DAW session, and the CSV file.
//
// A slot holds one finished measurement and its name (for example "Hardware" and "Songsmith"), so two can be drawn over each other.
// Only the smoothed curve is kept, not the recorded audio.
namespace PluginAnalyzer
{
    struct Slot
    {
        juce::String name;
        bool filled = false;
        Measurement measurement;
        juce::String channel;       // which channel was measured ("Left", "Right", "Both Combined")
        juce::String measuredAt;    // an ISO 8601 date and time

        void clear (int index)
        {
            name = defaultSlotName (index);
            filled = false;
            measurement = {};
            channel = {};
            measuredAt = {};
        }
    };

    // --- Saving a slot ----------------------------------------------------------------------------
    inline juce::ValueTree slotToTree (const Slot& s, int index)
    {
        juce::ValueTree t ("SLOT");
        t.setProperty ("index", index, nullptr);
        t.setProperty ("name", s.name, nullptr);
        if (! s.filled)
            return t;
        const auto& m = s.measurement;
        t.setProperty ("channel", s.channel, nullptr);
        t.setProperty ("measuredAt", s.measuredAt, nullptr);
        t.setProperty ("sampleRate", m.sampleRate, nullptr);
        t.setProperty ("delaySamples", m.delaySamples, nullptr);
        t.setProperty ("testLevelDb", (double) m.testLevelDb, nullptr);
        t.setProperty ("repeatDifferenceDb", (double) m.repeatDifferenceDb, nullptr);
        t.setProperty ("capturePeak", (double) m.capturePeak, nullptr);
        t.setProperty ("alignmentQuality", (double) m.alignmentQuality, nullptr);
        t.setProperty ("flags", (int) m.flags, nullptr);
        t.setProperty ("sweeps", m.sweeps, nullptr);
        // The curve: one 32-bit float per point, then one byte per point saying whether it was measured.
        juce::MemoryBlock block (m.magnitudeDb.size() * sizeof (float) + m.valid.size());
        std::memcpy (block.getData(), m.magnitudeDb.data(), m.magnitudeDb.size() * sizeof (float));
        std::memcpy (static_cast<char*> (block.getData()) + m.magnitudeDb.size() * sizeof (float), m.valid.data(), m.valid.size());
        t.setProperty ("curve", juce::Base64::toBase64 (block.getData(), block.getSize()), nullptr);
        return t;
    }

    // Loads a slot, tolerating anything missing or damaged: a bad slot is simply empty.
    inline void slotFromTree (Slot& s, const juce::ValueTree& t, int index)
    {
        s.clear (index);
        const auto name = t.getProperty ("name", juce::String()).toString();
        if (name.isNotEmpty())
            s.name = name.substring (0, 40);
        const auto curve = t.getProperty ("curve", juce::String()).toString();
        if (curve.isEmpty())
            return;

        juce::MemoryOutputStream decoded;
        if (! juce::Base64::convertFromBase64 (decoded, curve))
            return;
        const size_t expected = (size_t) gridPoints * sizeof (float) + (size_t) gridPoints;
        if (decoded.getDataSize() != expected)
            return;

        Measurement m;
        m.frequencyHz.resize ((size_t) gridPoints);
        for (int i = 0; i < gridPoints; ++i)
            m.frequencyHz[(size_t) i] = (float) gridFrequency (i);
        m.magnitudeDb.resize ((size_t) gridPoints);
        m.valid.resize ((size_t) gridPoints);
        std::memcpy (m.magnitudeDb.data(), decoded.getData(), (size_t) gridPoints * sizeof (float));
        std::memcpy (m.valid.data(), static_cast<const char*> (decoded.getData()) + (size_t) gridPoints * sizeof (float), (size_t) gridPoints);
        for (int i = 0; i < gridPoints; ++i)
            if (! std::isfinite (m.magnitudeDb[(size_t) i]))
            {
                m.magnitudeDb[(size_t) i] = 0.0f;
                m.valid[(size_t) i] = 0;
            }
        m.sampleRate = (double) t.getProperty ("sampleRate", 0.0);
        m.delaySamples = (double) t.getProperty ("delaySamples", 0.0);
        m.testLevelDb = (float) (double) t.getProperty ("testLevelDb", 0.0);
        m.repeatDifferenceDb = (float) (double) t.getProperty ("repeatDifferenceDb", 0.0);
        m.capturePeak = (float) (double) t.getProperty ("capturePeak", 0.0);
        m.alignmentQuality = (float) (double) t.getProperty ("alignmentQuality", 0.0);
        m.flags = (unsigned) (int) t.getProperty ("flags", 0);
        m.sweeps = (int) t.getProperty ("sweeps", 0);
        s.measurement = std::move (m);
        s.channel = t.getProperty ("channel", juce::String()).toString();
        s.measuredAt = t.getProperty ("measuredAt", juce::String()).toString();
        s.filled = true;
    }

    // --- CSV ------------------------------------------------------------------------------------------
    // One header block of "# name: value" lines per measurement, then a table: frequency in hertz, then one column of decibels per
    // measurement. Frequencies that could not be measured are left empty. Plain text, comma separated, one row per line.
    inline juce::String csvCell (const juce::String& text)
    {
        // A name goes in a header line or a column title: no line breaks, and quoted if it holds a comma or a quote.
        auto clean = text.replaceCharacters ("\r\n", "  ").trim();
        if (clean.containsAnyOf (",\""))
            clean = "\"" + clean.replace ("\"", "\"\"") + "\"";
        return clean;
    }

    inline juce::String makeCsv (const std::vector<const Slot*>& slots, const juce::String& productVersion)
    {
        juce::String out;
        out << "# Songsmith Plugin Analyzer " << productVersion << "\r\n";
        out << "# What is measured: gain in decibels against frequency, from two logarithmic sweeps (20 Hz to 20 kHz), smoothed to a sixth of an octave.\r\n";
        out << "# Only the size of the response is measured, not its phase. A compressor, saturation or anything that changes with level is measured at the test level only.\r\n";
        int column = 0;
        for (auto* s : slots)
        {
            ++column;
            const auto& m = s->measurement;
            const auto prefix = "# Column " + juce::String (column) + " ";
            out << prefix << "name: " << csvCell (s->name) << "\r\n";
            out << prefix << "channel: " << csvCell (s->channel) << "\r\n";
            out << prefix << "sample rate hz: " << juce::String (m.sampleRate, 0) << "\r\n";
            out << prefix << "test level db: " << juce::String (m.testLevelDb, 1) << "\r\n";
            out << prefix << "sweeps: " << m.sweeps << "\r\n";
            out << prefix << "difference between sweeps db: " << juce::String (m.repeatDifferenceDb, 2) << "\r\n";
            out << prefix << "delay between the Reference and the Result, samples: " << juce::String (m.delaySamples, 1) << "\r\n";
            out << prefix << "measured: " << csvCell (s->measuredAt) << "\r\n";
            const auto notes = describeFlags (m);
            for (auto& note : notes)
                out << prefix << "warning: " << csvCell (note) << "\r\n";
        }
        out << "frequency_hz";
        for (auto* s : slots)
            out << "," << csvCell (s->name + " (db)");
        out << "\r\n";
        for (int i = 0; i < gridPoints; ++i)
        {
            out << juce::String (gridFrequency (i), 2);
            for (auto* s : slots)
            {
                out << ",";
                if (s->measurement.valid.size() == (size_t) gridPoints && s->measurement.valid[(size_t) i])
                    out << juce::String (s->measurement.magnitudeDb[(size_t) i], 3);
            }
            out << "\r\n";
        }
        return out;
    }
}
