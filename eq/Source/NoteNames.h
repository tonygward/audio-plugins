#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <optional>

// Musical note helpers (equal temperament, A4 = 440 Hz, MIDI note 69, C-1 = MIDI 0).
namespace EQ::Notes
{
    constexpr double a4Hz = 440.0;
    constexpr int a4Midi = 69;
    constexpr int minOctave = -1, maxOctave = 9;

    struct Note
    {
        int midi = a4Midi;      // nearest note
        double cents = 0.0;     // offset from it, -50..+50
    };

    inline double midiToFreq (int midi)
    {
        return a4Hz * std::pow (2.0, (midi - a4Midi) / 12.0);
    }

    inline Note freqToNote (double hz)
    {
        const double exact = a4Midi + 12.0 * std::log2 (std::max (hz, 1.0e-6) / a4Hz);
        Note n;
        n.midi = (int) std::lround (exact);
        n.cents = (exact - n.midi) * 100.0;
        return n;
    }

    // Exact semitone frequency nearest to 'hz' (note snapping).
    inline double snapToSemitone (double hz)
    {
        return midiToFreq (freqToNote (hz).midi);
    }

    inline juce::String noteName (int midi)   // sharps: C, C#, D, ... ; octave as in "A4"
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int pc = ((midi % 12) + 12) % 12;
        const int octave = (midi - pc) / 12 - 1;
        return juce::String (names[pc]) + juce::String (octave);
    }

    // "A4 +0¢", "C#5 -14¢"
    inline juce::String formatNote (double hz)
    {
        const auto n = freqToNote (hz);
        const int cents = (int) std::lround (n.cents);
        return noteName (n.midi) + " " + (cents >= 0 ? "+" : "") + juce::String (cents) + juce::String::charToString (0x00a2);
    }

    // Parses "A2", "C#5", "Eb3", "a4", "Bb-1", "B#3" (= C4). Letter A-G, optional '#' or 'b',
    // octave -1..9. Returns the note's frequency, or nothing for anything else.
    inline std::optional<double> parseNote (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.length() < 2)
            return std::nullopt;

        static const int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
        if (letter < 'A' || letter > 'G')
            return std::nullopt;

        int pc = letterPc[letter - 'A'];
        int pos = 1;
        if (t[pos] == '#')                     { ++pc; ++pos; }
        else if (t[pos] == 'b' && t.length() > 2) { --pc; ++pos; }

        const auto octaveText = t.substring (pos);
        if (octaveText.isEmpty() || ! octaveText.containsOnly ("-0123456789")
            || octaveText.lastIndexOfChar ('-') > 0 || octaveText == "-")
            return std::nullopt;

        const int octave = octaveText.getIntValue();
        if (octave < minOctave || octave > maxOctave)
            return std::nullopt;

        return midiToFreq ((octave + 1) * 12 + pc);
    }
}
