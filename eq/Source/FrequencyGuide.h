#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// The Instrument Frequency Guide (EQ-32): an optional, see-through set of listening regions drawn
// behind the graph. It is drawing only. It never changes a band, a parameter or the sound.
//
// The words are Joe Gilder's wherever he has given one (Home Studio Corner videos), then Tony's own
// notes, then Songsmith's. The transcripts are auto-generated captions; every figure here was checked
// against the caption text or the note file it cites (see `source` on each region). The frequencies are
// listening regions, not rules, so every tooltip says "around". The guide is not endorsed by Joe Gilder.
//
// Each instrument is a table of regions from low to high. Adding an instrument is adding a table.
namespace EQ::Guide
{
    struct Region
    {
        const char* label;    // a plain word, never shortened
        float lowHz, highHz;  // the listening region, approximately
        const char* tip;      // one to three sentences: what it adds, what too much sounds like
        const char* source;   // where it comes from (not shown on screen; tested, and listed in the README)
    };

    struct Instrument
    {
        const char* name;
        const char* note;               // one sentence about the instrument as a whole
        std::vector<Region> regions;    // low to high
    };

    inline const std::vector<Instrument>& instruments()
    {
        static const std::vector<Instrument> all {
            //----------------------------------------------------------------------------------------------
            { "Acoustic Guitar",
              "A steel-string acoustic guitar recorded with a microphone. These are places to listen, not rules: only cut what you can hear is a problem.",
              {
                { "Rumble", 20.0f, 50.0f,
                  "Nothing useful from the guitar lives here: air conditioning, foot taps and things you only hear on a subwoofer. A low cut here is cheap insurance, but only use one when there is a reason.",
                  "Joe Gilder, How I EQ Acoustic Guitar (2021-12-02, 0:31 and 2:07); How to EQ Acoustic Guitar (2026-09-10, 1:34)" },
                { "Thump", 100.0f, 200.0f,
                  "The knock of each strum, like a drum inside the guitar. Some of it is the warmth; too much is a boom on every chord. Turn it down, never out. With a bass underneath in a full mix, a cut a little higher, around 200 to 350 Hz, can work better.",
                  "Joe Gilder, How I EQ Acoustic Guitar (2021-12-02, 2:37-4:42); How to EQ Acoustic Guitar (2026-09-10, 2:34-4:09). Tony: mixing/acoustic-moods.md, Steel guitar rhythm" },
                { "Plastic", 500.0f, 800.0f,
                  "If this band is strong, a strummed guitar can sound boxy, mid-heavy or a little plastic. Listen with the whole mix playing: it may need no adjustment. If it bothers you, try a small cut, and notice whether the lows and highs sound clearer.",
                  "Joe Gilder, How I EQ Acoustic Guitar (2021-12-02, 4:42-6:26)" },
                { "Cheap", 1000.0f, 2000.0f,
                  "If this band is strong, hard strums can sound thin or cheap. Listen in context: it may need no adjustment. If it bothers you, a small cut, nearer 2 kHz, can warm the guitar without adding low end. A pickup can quack at 1 to 3 kHz, where a dynamic cut may suit it better than a fixed one.",
                  "Joe Gilder, How to EQ Acoustic Guitar (2026-09-10, 4:09). Tony: instruments/tony/yamaha-apx-3.md, takamine-esf-40c.md" },
                { "Air", 8000.0f, 20000.0f,
                  "Pick detail that helps the guitar cut through a busy mix. Too much and it sizzles: a little goes a long way.",
                  "Joe Gilder, How I EQ Acoustic Guitar (2021-12-02, 6:58-8:08)" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Nylon Guitar",
              "A nylon-string guitar playing the tune. Unlike a steel-string guitar, its middle is the melody, not a problem.",
              {
                { "Rumble", 20.0f, 80.0f,
                  "Chair and foot noise, nothing the guitar needs.",
                  "Tony: instruments/chris/nylon.md" },
                { "Boom", 180.0f, 250.0f,
                  "The boom from the sound hole when the microphone is close. Cut it only when it blooms.",
                  "Tony: instruments/chris/nylon.md; mixing/acoustic-moods.md" },
                { "Boxy", 400.0f, 500.0f,
                  "Only worth turning down if it sounds boxy. Usually leave it alone.",
                  "Tony: instruments/chris/nylon.md" },
                { "Melody", 1000.0f, 3000.0f,
                  "Where the tune speaks. Leave it alone, and make room in the other guitars instead.",
                  "Tony: instruments/chris/nylon.md; mixing/acoustic-moods.md" },
                { "String Noise", 2000.0f, 5000.0f,
                  "Squeaks and fret noise. Tame them gently, and only where they jump out. This overlaps the melody on purpose: the two share this range.",
                  "Tony: mixing/acoustic-moods.md" },
                { "Air", 10000.0f, 12000.0f,
                  "A little lift here adds openness.",
                  "Tony: instruments/chris/nylon.md" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Electric Guitar",
              "An electric guitar. Most of its tone is made during the recording, so EQ is usually a finishing touch.",
              {
                { "Boom", 20.0f, 80.0f,
                  "Below the guitar's useful range. A low cut here is common, because there can be boominess.",
                  "Joe Gilder, How to EQ Electric Guitar (2026-09-09, 2:05-2:36)" },
                { "Mud", 150.0f, 200.0f,
                  "A big tone that thickens up and muddies the mix, a rumbly thickness. Common on clean guitars. High-gain tones can build up mud higher, at around 200 to 400 Hz.",
                  "Joe Gilder, How to EQ Electric Guitar (2026-09-09, 2:36-3:09). Tony: instruments/tony/schecter-demon-6.md" },
                { "Warmth", 300.0f, 420.0f,
                  "A little boost around 360 Hz on the guitar bus adds warmth without mud. Do not overdo it.",
                  "Joe Gilder, How to EQ Electric Guitar (2026-09-09, 3:40-4:11); How to EQ Electric Guitar (2021-12-09, 1:32)" },
                { "Aggression", 2000.0f, 3000.0f,
                  "Where the bite is. Too much is really annoying.",
                  "Joe Gilder, How to EQ Electric Guitar (2026-09-09, 3:09-3:40)" },
                { "Crunch", 3000.0f, 5000.0f,
                  "The crunch of a driven tone. Too much is spiky: think of a Telecaster's bridge pickup, or the in-between positions on a Stratocaster.",
                  "Joe Gilder, How to EQ Electric Guitar (2021-12-09, 1:32). Tony: instruments/tony/fender-telecaster.md, fender-stratocaster.md" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Bass Guitar",
              "An electric bass guitar. Three places to listen are the weight, the octave above it, and the click.",
              {
                { "Weight", 40.0f, 65.0f,
                  "The deep part you feel more than hear, a big warm hug of low end. A small boost here if it is missing. A five- or six-string bass goes down to about 31 Hz, so do not cut that low note.",
                  "Joe Gilder, How to EQ Bass Guitar (2026-09-08, 3:48-4:50 and 1:03). Tony: instruments/dan/washburn-6-string.md" },
                { "Mud", 120.0f, 150.0f,
                  "Weird, gross muddiness: the octave above the note, which is often louder than the note itself. Turn this down first.",
                  "Joe Gilder, How to EQ Bass Guitar (2026-09-08, 3:16-3:48); How to EQ Bass (2019-01-10, 7:29)" },
                { "Boxy", 200.0f, 500.0f,
                  "Some basses get boxy somewhere in here. Listen before you cut. An acoustic bass with a pickup also has body boom at 100 to 200 Hz.",
                  "Joe Gilder, How to Mix #Bass (2020-10-23). Tony: instruments/dan/acoustic-bass.md" },
                { "Click", 1000.0f, 3000.0f,
                  "The pick or finger attack that lets the notes be heard on small speakers. A fretless bass's mwah lives at 1 to 2 kHz, so do not cut it.",
                  "Joe Gilder, How to EQ Bass Guitar (2026-09-08, 4:18). Tony: instruments/dan/fretless-5-string.md" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Vocal",
              "A lead vocal. Cut only what you can hear, and be careful with the top.",
              {
                { "Rumble", 20.0f, 120.0f,
                  "Pops from hard p sounds, and stand noise. A low cut here is a common starting point.",
                  "Joe Gilder, How to EQ Vocals (2026-09-21, 2:34); 5 Tips to Nail the Vocal EQ Every Time (2023-08-02, 2:40)" },
                { "Mud", 150.0f, 250.0f,
                  "Where the warmth is, but too much and the vocal will not sit on top of the mix. Modern records are thinner here than you might think.",
                  "Joe Gilder, How to EQ Vocals (2026-09-21, 2:34-4:07); How to EQ Vocals (2024-05-29, 8:53)" },
                { "Piercing", 2400.0f, 2800.0f,
                  "A whistly, piercing ring when the singer pushes. A narrow cut here often lets you turn the vocal up.",
                  "Joe Gilder, The Devil Frequency (2026-09-22, 1:00, 2:02 and 3:03)" },
                { "Harshness", 3600.0f, 4400.0f,
                  "Sometimes a second harsh ring. Only deal with it if you hear it.",
                  "Joe Gilder, How to EQ Vocals (2024-05-29, 12:12); Vocal EQ Avoid These Two Traps (2023-11-15, 6:43)" },
                { "Sibilance", 6000.0f, 8000.0f,
                  "The s and t sounds. A de-esser works better than a fixed cut.",
                  "Joe Gilder, How (and Why) to Use a De-Esser on Vocals (2026-09-20)" },
                { "Air", 8000.0f, 12000.0f,
                  "A little lift if the vocal is still dark. Most people overdo it, so be careful.",
                  "Joe Gilder, How to EQ Vocals (2026-09-21, 4:07-4:38)" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Kick Drum",
              "A kick drum. The boxy part is often worth turning down on drums, if you can hear it.",
              {
                { "Weight", 40.0f, 100.0f,
                  "The punch in the chest, often about 80 Hz. Turn it up only if it is missing.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 3:34-4:05); How to Mix Kick Drum and Bass (2019-04-09, 1:33)" },
                { "Boom", 60.0f, 160.0f,
                  "A ring that hangs on after each hit. Find the one note and cut it narrowly.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 4:05); Mixing Kick Drum - Cutting Low End (2014-05-22, 1:33)" },
                { "Boxy", 320.0f, 420.0f,
                  "Cardboard-sounding, a woo woo. Often worth turning down on drums, if you can hear it.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 3:04-3:34); EQing Kick Drum (2010-02-01)" },
                { "Snap", 2500.0f, 3500.0f,
                  "The click of the beater. A little boost helps the kick come through on small speakers.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 4:35)" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Snare Drum",
              "A snare drum. A single ringing note can sit anywhere, so listen for it rather than trusting a region.",
              {
                { "Boom", 140.0f, 200.0f,
                  "Something happening just below 200 Hz. Bring it down, then lift the low end as a whole so it stays balanced.",
                  "Joe Gilder, Thick Snare With Only 3 EQ's (2018-04-16, 3:43-4:14)" },
                { "Boxy", 330.0f, 400.0f,
                  "The cardboard sound, as on the kick, around 360 Hz.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 3:34)" },
                { "Snap", 2500.0f, 3500.0f,
                  "The crack of the stick. A small boost. If one note rings out on a snare, it can be anywhere, so find it by ear and cut it narrowly.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 4:35); EQ Ringing in the Snare Drum (2021-06-10, 6:10)" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Toms",
              "Toms, with starting points taken from a finished album mix. The moves are bolder than for other drums.",
              {
                { "Body", 100.0f, 165.0f,
                  "The tone of the drum. A boost here is a common starting point.",
                  "Tony: fx-chains/songs/tony-ward.md; eq/docs/starting-points.md, Tom presets" },
                { "Boxy", 300.0f, 950.0f,
                  "The cardboard sound. A deep cut, by 7 to 12 dB, is one starting point.",
                  "Tony: fx-chains/songs/tony-ward.md; eq/docs/starting-points.md, Tom presets" },
                { "Attack", 3500.0f, 4500.0f,
                  "The stick on the head. Adding about 6 dB around 4 kHz is one starting point.",
                  "Tony: fx-chains/songs/tony-ward.md; eq/docs/starting-points.md, Tom presets" },
                { "Air", 10000.0f, 20000.0f,
                  "Cymbal spill lives here too. Cutting the toms above about 10.7 kHz is one starting point.",
                  "Tony: fx-chains/songs/tony-ward.md; eq/docs/starting-points.md, Tom presets" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Drum Bus",
              "All the drums together. Quick moves to try: more weight if needed, less boom, less boxy, a little more snap.",
              {
                { "Weight", 40.0f, 100.0f,
                  "More kick thickness, only if it is needed.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 3:34-4:05)" },
                { "Boom", 110.0f, 150.0f,
                  "Low boom in the kick or snare, around 130 Hz. Cut it narrowly.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 4:05)" },
                { "Boxy", 320.0f, 400.0f,
                  "The boxy zone: muffled and cardboard-like. Often worth turning down on drums, if you can hear it.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 3:04-3:34); Thick Snare With Only 3 EQ's (2018-04-16, 1:02)" },
                { "Snap", 2500.0f, 3500.0f,
                  "A little more snap from the kick and the snare.",
                  "Joe Gilder, Easy Way to EQ and Compress Drums (2026-09-15, 4:35)" },
              } },
            //----------------------------------------------------------------------------------------------
            { "Any Instrument",
              "Five general groups, for anything not in the list: piano, keys, strings. For each group, the first word is what it adds when it is right, and the second is what too much sounds like.",
              {
                { "Lows", 20.0f, 80.0f,
                  "Thump when it is right, boomy when there is too much.",
                  "Joe Gilder, Frequency Fluency (2026-09-06, 1:32-3:36)" },
                { "Low Mids", 80.0f, 300.0f,
                  "Warm when it is right, muddy when there is too much.",
                  "Joe Gilder, Frequency Fluency (2026-09-06, 1:32-3:36)" },
                { "Mids", 300.0f, 1000.0f,
                  "Full when it is right, muffled or lo-fi when there is too much.",
                  "Joe Gilder, Frequency Fluency (2026-09-06, 1:32-3:36)" },
                { "High Mids", 1000.0f, 5000.0f,
                  "Clarity when it is right, annoying or nasal when there is too much.",
                  "Joe Gilder, Frequency Fluency (2026-09-06, 1:32-3:36)" },
                { "Highs", 5000.0f, 20000.0f,
                  "Air when it is right, harsh when there is too much.",
                  "Joe Gilder, Frequency Fluency (2026-09-06, 1:32-3:36)" },
              } },
        };
        return all;
    }

    inline int numInstruments() { return (int) instruments().size(); }

    inline const Instrument& instrument (int index)
    {
        return instruments()[(size_t) juce::jlimit (0, numInstruments() - 1, index)];
    }

    // "1.2 kHz", "800 Hz": units only, no abbreviations beyond Hz and kHz.
    inline juce::String frequencyText (float hz)
    {
        if (hz >= 1000.0f)
        {
            const double k = (double) hz / 1000.0;
            return juce::String (k, std::abs (k - std::round (k)) < 0.05 ? 0 : 1) + " kHz";
        }
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }

    // "around 100 to 200 Hz", "under 50 Hz", "above 8 kHz". Always "around", because these are
    // listening regions, not rules.
    inline juce::String rangeText (const Region& r)
    {
        if (r.lowHz <= 20.0f)
            return "under " + frequencyText (r.highHz);
        if (r.highHz >= 20000.0f)
            return "above " + frequencyText (r.lowHz);
        const auto lo = frequencyText (r.lowHz), hi = frequencyText (r.highHz);
        // Share the unit when both ends use it: "100 to 200 Hz", not "100 Hz to 200 Hz".
        if (lo.endsWithIgnoreCase (" Hz") == hi.endsWithIgnoreCase (" Hz") && lo.endsWithIgnoreCase (" kHz") == hi.endsWithIgnoreCase (" kHz"))
            return "around " + lo.upToLastOccurrenceOf (" ", false, false) + " to " + hi;
        return "around " + lo + " to " + hi;
    }

    // What the tooltip shows for one region: its word and where it is, then what to listen for.
    inline juce::String tooltipFor (const Instrument& instrument, const Region& r)
    {
        return juce::String (r.label) + " (" + rangeText (r) + ", " + instrument.name + ")\n" + r.tip;
    }

    // Every region the frequency lies inside, low to high: regions overlap on purpose.
    inline std::vector<const Region*> regionsAt (const Instrument& instrument, float hz)
    {
        std::vector<const Region*> found;
        for (const auto& r : instrument.regions)
            if (hz >= r.lowHz && hz <= r.highHz)
                found.push_back (&r);
        return found;
    }

    // Colour-blind-safe hues (Okabe and Ito). The word always says what the region is, so colour
    // is only ever a second cue.
    inline juce::uint32 regionColourArgb (int index)
    {
        static constexpr juce::uint32 colours[] = { 0xffe69f00, 0xff56b4e9, 0xff009e73, 0xfff0e442, 0xff0072b2, 0xffd55e00, 0xffcc79a7 };
        return colours[(size_t) juce::jmax (0, index) % std::size (colours)];
    }
}
