// Analogue colour, oversampling and latency tests (step 3). Definitions in analog-compressor/docs/colour.md.
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"
#include "FETColour.h"
#include "EngineMeasure.h"
#include "ColourMeasure.h"

using namespace FET;
using ColourMeasure::Settings;

namespace
{
    juce::AudioProcessor::BusesLayout monoLayout()
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::mono());
        return l;
    }

    // Level-matched pair at the same drive: compressed, then GR off with Output trimmed so the
    // output amplifier sees the same level.
    std::pair<ColourMeasure::Result, ColourMeasure::Result> matchedPair (Settings s)
    {
        const auto comp = ColourMeasure::measure (s);
        Settings o = s;
        o.grOff = true;
        o.outputDb = (float) (comp.fundamentalDb - (s.levelDbfs + s.inputDb));
        return { comp, ColourMeasure::measure (o) };
    }
}

//==============================================================================
struct ColourTests final : juce::UnitTest
{
    ColourTests() : juce::UnitTest ("Analogue colour", "FET") {}

    void runTest() override
    {
        beginTest ("FET cell is linear when it isn't reducing gain");
        for (float y : { -1.0f, -0.1f, 0.0f, 0.3f, 0.9f })
            expectEquals (FetCell::process (y, 1.0f), y);

        beginTest ("output stage: about 0.05 % THD at 0 VU, about 0.5 % at +22 dBm (UG p. 1)");
        {
            Settings s; s.grOff = true; s.oversampling = 2;
            double prev = 0.0;
            for (float level : { -18.0f, -6.0f, 0.0f, 3.0f })
            {
                s.levelDbfs = level;
                const auto r = ColourMeasure::measure (s);
                if (level == -18.0f) expectLessThan (r.thdPercent, 0.1, "0 VU: " + juce::String (r.thdPercent, 3) + " %");
                if (level == 0.0f)   expectWithinAbsoluteError (r.thdPercent, 0.5, 0.2, "+22 dBm: " + juce::String (r.thdPercent, 3) + " %");
                expectGreaterThan (r.thdPercent, prev, "distortion rises with level");
                prev = r.thdPercent;
            }
        }

        beginTest ("compression adds FET distortion at a matched output level");
        {
            Settings s; s.inputDb = 18.0f; s.oversampling = 2;
            const auto [comp, off] = matchedPair (s);
            expectWithinAbsoluteError (comp.fundamentalDb, off.fundamentalDb, 0.3, "levels matched");
            expectGreaterThan (comp.grDb, 10.0f);
            expectGreaterThan (comp.thdPercent, 3.0 * off.thdPercent,
                               "comp " + juce::String (comp.thdPercent, 3) + " % vs GR off " + juce::String (off.thdPercent, 3) + " %");
            expectGreaterThan (comp.h2Db, off.h2Db + 10.0, "mainly 2nd harmonic");
        }

        beginTest ("Gain Reduction Off keeps the driven amplifier path, unlike bypass");
        {
            Settings s; s.grOff = true; s.levelDbfs = 0.0f; s.oversampling = 2;
            float gr = 1.0f;
            const auto out = ColourMeasure::render (s, &gr);
            const auto r = ColourMeasure::analyse (out, s.fs, s.freq);
            expectEquals (gr, 0.0f, "no gain reduction");
            expectGreaterThan (r.thdPercent, 0.2, "the output amplifier still colours");
        }

        beginTest ("the clean engine is still available (colour off = engine only)");
        {
            Settings s; s.colour = false; s.grOff = true; s.levelDbfs = 0.0f;
            const auto r = ColourMeasure::measure (s);
            expectLessThan (r.thdPercent, 0.001);
        }

        beginTest ("frequency response is within ±1 dB, 20 Hz to 20 kHz (UG p. 1)");
        for (double fs : { 44100.0, 48000.0 })
            for (int os : { 0, 1, 2 })
            {
                Settings s; s.fs = fs; s.oversampling = os; s.grOff = true;
                s.freq = 1000.0;
                const double ref = ColourMeasure::measure (s).fundamentalDb;
                for (double f : { 20.0, 50.0, 10000.0, 16000.0, 20000.0 })
                {
                    s.freq = f;
                    const double d = ColourMeasure::measure (s).fundamentalDb - ref;
                    expectWithinAbsoluteError (d, 0.0, 1.0, juce::String (f, 0) + " Hz at " + juce::String (fs, 0)
                                                               + " OS " + juce::String (1 << os) + "x: " + juce::String (d, 2) + " dB");
                }
            }
    }
};

//==============================================================================
struct OversamplingTests final : juce::UnitTest
{
    OversamplingTests() : juce::UnitTest ("Oversampling and latency", "FET") {}

    void runTest() override
    {
        beginTest ("oversampling reduces aliasing from compression at 44.1 and 48 kHz");
        for (double fs : { 44100.0, 48000.0 })
        {
            double alias[3] {};
            for (int os : { 0, 1, 2 })
            {
                Settings s; s.fs = fs; s.oversampling = os; s.inputDb = 18.0f; s.freq = 7000.0;
                alias[os] = ColourMeasure::measure (s).aliasDb;
            }
            const auto txt = juce::String (alias[0], 1) + " / " + juce::String (alias[1], 1) + " / " + juce::String (alias[2], 1) + " dB";
            expectLessThan (alias[1], alias[0], "2x better than off at " + juce::String (fs, 0) + ": " + txt);
            expectLessThan (alias[2], alias[0] - 10.0, "4x at least 10 dB better than off: " + txt);
        }

        beginTest ("envelope timing is the same in every rate domain");
        for (double fs : { 88200.0, 96000.0, 176400.0, 192000.0 })
            for (float knob : { 1.0f, 4.0f })
            {
                const double a = Measure::attackSeconds (fs, 0, knob, 20.0f);
                const double r = Measure::releaseSeconds (fs, 0, knob, 20.0f);
                expectWithinAbsoluteError (a, Engine::attackSeconds (knob), 0.25 * Engine::attackSeconds (knob),
                                           "attack at " + juce::String (fs, 0));
                expectWithinAbsoluteError (r, Engine::releaseSeconds (knob), 0.05 * Engine::releaseSeconds (knob),
                                           "release at " + juce::String (fs, 0));
            }

        beginTest ("processor GR settles the same with oversampling off, 2x and 4x");
        {
            float settled[3] {};
            for (int os : { 0, 1, 2 })
            {
                Settings s; s.oversampling = os; s.inputDb = 12.0f;
                settled[os] = ColourMeasure::measure (s).grDb;
            }
            expectWithinAbsoluteError (settled[1], settled[0], 0.5f);
            expectWithinAbsoluteError (settled[2], settled[0], 0.5f);
        }

        for (double fs : { 44100.0, 48000.0 })
            for (int os : { 1, 2 })
            {
                beginTest ("reported latency matches the signal and bypass stays aligned (" + juce::String (fs, 0)
                           + " Hz, " + juce::String (1 << os) + "x)");
                SongsmithAnalogCompressorProcessor p;
                p.setColourEnabled (false);
                ColourMeasure::set (p, ID::oversampling, (float) os);
                ColourMeasure::set (p, ID::grOff, 1.0f);
                p.setBusesLayout (monoLayout());
                p.prepareToPlay (fs, 512);
                const int latency = p.getLatencySamples();
                expectEquals (latency, p.getOversamplingLatency (os));
                expectGreaterThan (latency, 0);

                // Delay of a 200 Hz sine (well inside the passband): zero crossing positions.
                const double f = 200.0;
                const int total = (int) fs;
                std::vector<float> in ((size_t) total), out ((size_t) total);
                for (int i = 0; i < total; ++i)
                    in[(size_t) i] = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * f * i / fs);
                juce::AudioBuffer<float> b (1, 512);
                juce::MidiBuffer m;
                for (int pos = 0; pos + 512 <= total; pos += 512)
                {
                    b.copyFrom (0, 0, in.data() + pos, 512);
                    p.processBlock (b, m);
                    std::copy (b.getReadPointer (0), b.getReadPointer (0) + 512, out.begin() + pos);
                }
                const auto firstRise = [&] (const std::vector<float>& x, int from)
                {
                    for (int i = from; i + 1 < (int) x.size(); ++i)
                        if (x[(size_t) i] <= 0.0f && x[(size_t) i + 1] > 0.0f)
                            return i + x[(size_t) i] / (x[(size_t) i] - x[(size_t) i + 1]);
                    return -1.0f;
                };
                const int from = (int) (fs * 0.5);
                const float tIn = firstRise (in, from), tOut = firstRise (out, from);
                const float period = (float) (fs / f);
                float delay = std::fmod (tOut - tIn + period, period);   // the same cycle, not the next one
                if (delay > period / 2) delay -= period;
                expectWithinAbsoluteError (delay, (float) latency, 0.6f, "measured delay " + juce::String (delay, 2) + " samples");

                // Engage bypass mid-stream: during the 20 ms fade, wet and dry must not cancel.
                ColourMeasure::set (p, ID::bypass, 1.0f);
                float lo = 1.0f, hi = 0.0f;
                for (int k = 0; k < 8; ++k)
                {
                    for (int i = 0; i < 512; ++i)
                        b.setSample (0, i, 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * f * (total + k * 512 + i) / fs));
                    p.processBlock (b, m);
                    // Peak per 5 ms window.
                    for (int w = 0; w + 220 <= 512; w += 128)
                    {
                        const float mag = b.getMagnitude (0, w, juce::jmin (240, 512 - w));
                        if (k > 0) { lo = juce::jmin (lo, mag); hi = juce::jmax (hi, mag); }
                    }
                }
                expectGreaterThan (lo, 0.1f * 0.89f, "no comb dip during the bypass fade (> -1 dB)");
                expectLessThan (hi, 0.1f * 1.12f);
            }

        beginTest ("switching oversampling mid-stream is click-free and updates the latency");
        {
            SongsmithAnalogCompressorProcessor p;
            ColourMeasure::set (p, ID::oversampling, 0.0f);
            p.setBusesLayout (monoLayout());
            p.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> b (1, 256);
            juce::MidiBuffer m;
            float prev = 0.0f, maxStep = 0.0f;
            int n = 0;
            for (int k = 0; k < 400; ++k)
            {
                if (k == 100) ColourMeasure::set (p, ID::oversampling, 2.0f);
                if (k == 250) ColourMeasure::set (p, ID::oversampling, 1.0f);
                for (int i = 0; i < 256; ++i, ++n)
                    b.setSample (0, i, 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 500.0 * n / 48000.0));
                p.processBlock (b, m);
                for (int i = 0; i < 256; ++i)
                {
                    if (k > 10) maxStep = juce::jmax (maxStep, std::abs (b.getSample (0, i) - prev));
                    prev = b.getSample (0, i);
                }
            }
            // A 500 Hz sine at 0.3 moves at most 0.02 per sample; a click would be far larger.
            expectLessThan (maxStep, 0.04f, "largest step " + juce::String (maxStep, 4));
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), p.getOversamplingLatency (1));
        }

        beginTest ("state version 1 sessions load with the default oversampling");
        {
            SongsmithAnalogCompressorProcessor p;
            ColourMeasure::set (p, ID::oversampling, 2.0f);
            juce::XmlElement v1 ("SongsmithAnalogCompressor");
            v1.setAttribute ("stateVersion", 1);
            auto* c = v1.createNewChildElement ("PARAM");
            c->setAttribute ("id", "input");
            c->setAttribute ("value", 3.0);
            juce::MemoryBlock mb;
            juce::AudioProcessor::copyXmlToBinary (v1, mb);
            p.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals ((int) p.apvts.getRawParameterValue (ID::oversampling)->load(), oversamplingDefault);
            expectEquals ((int) p.apvts.getRawParameterValue (ID::oversampling)->load(), 0, "version 1 sessions load as Off");
        }

        beginTest ("a fresh instance has oversampling Off and reports 0 samples of latency");
        {
            SongsmithAnalogCompressorProcessor p;
            expectEquals ((int) p.apvts.getRawParameterValue (ID::oversampling)->load(), 0);
            p.setBusesLayout (monoLayout());
            p.prepareToPlay (48000.0, 512);
            p.updateLatencyNow();
            expectEquals (p.getLatencySamples(), 0);
        }

        beginTest ("a state version 2 session saved with 4x loads as 4x and reports its latency");
        {
            SongsmithAnalogCompressorProcessor a;
            ColourMeasure::set (a, ID::oversampling, 2.0f);
            juce::MemoryBlock mb;
            a.getStateInformation (mb);

            SongsmithAnalogCompressorProcessor b;
            b.setStateInformation (mb.getData(), (int) mb.getSize());
            expectEquals ((int) b.apvts.getRawParameterValue (ID::oversampling)->load(), 2);
            b.setBusesLayout (monoLayout());
            b.prepareToPlay (48000.0, 512);
            b.updateLatencyNow();
            expectGreaterThan (b.getLatencySamples(), 0);
            expectEquals (b.getLatencySamples(), b.getOversamplingLatency (2));
        }
    }
};

static ColourTests colourTests;
static OversamplingTests oversamplingTests;
