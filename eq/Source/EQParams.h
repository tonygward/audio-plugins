#pragma once

#include <juce_core/juce_core.h>

// Shared definitions for the EQ band bank: parameter IDs, ranges and defaults.
namespace EQ
{
    constexpr int numBands        = 24;
    constexpr int numDefaultBands = 3;   // bands that exist in a fresh instance / v1 sessions

    enum class FilterType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };
    constexpr int numFilterTypes = 6;

    inline const juce::StringArray& getTypeNames()
    {
        static const juce::StringArray names { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
        return names;
    }

    inline FilterType typeFromIndex (int i)
    {
        return static_cast<FilterType> (juce::jlimit (0, numFilterTypes - 1, i));
    }

    inline bool typeHasGain (FilterType t)
    {
        return t == FilterType::Bell || t == FilterType::LowShelf || t == FilterType::HighShelf;
    }

    constexpr float minFreq = 20.0f,   maxFreq = 20000.0f;
    constexpr float minGain = -30.0f,  maxGain = 30.0f;
    constexpr float minQ    = 0.1f,    maxQ    = 40.0f;
    constexpr float defaultQ = 0.707f;

    inline float defaultFreq (int band)
    {
        constexpr float legacy[] = { 100.0f, 1000.0f, 8000.0f };
        return band < 3 ? legacy[band] : 1000.0f;
    }

    inline bool defaultUsed (int band) { return band < numDefaultBands; }

    // Cut slopes (Low Cut / High Cut only). Index 1 (12 dB/oct, one SVF stage) is the
    // default and matches every session saved before slopes existed.
    constexpr int numSlopes = 6;
    constexpr int defaultSlope = 1;
    constexpr int slopeOrders[numSlopes] = { 1, 2, 3, 4, 6, 8 };   // filter order = dB/oct / 6

    inline const juce::StringArray& getSlopeNames()
    {
        static const juce::StringArray names { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
        return names;
    }

    inline int slopeOrder (int slopeIndex) { return slopeOrders[juce::jlimit (0, numSlopes - 1, slopeIndex)]; }

    inline bool typeHasSlope (FilterType t) { return t == FilterType::LowCut || t == FilterType::HighCut; }

    // Per-band host parameters. Append new ones at the end: the index is used for
    // fixed-size arrays and the ID suffix table below.
    // Per-band stereo processing. Stereo (default) processes both channels; Mid/Side bands
    // run on an M/S-encoded signal (M = (L+R)/2, S = (L-R)/2).
    enum class StereoMode { Stereo = 0, Left, Right, Mid, Side };
    constexpr int numStereoModes = 5;

    inline const juce::StringArray& getStereoModeNames()
    {
        static const juce::StringArray names { "Stereo", "Left", "Right", "Mid", "Side" };
        return names;
    }

    inline StereoMode stereoModeFromIndex (int i) { return static_cast<StereoMode> (juce::jlimit (0, numStereoModes - 1, i)); }
    inline bool isMidSide (StereoMode m) { return m == StereoMode::Mid || m == StereoMode::Side; }

    enum class Param { Freq, Gain, Q, Type, Enabled, Slope, Stereo };
    constexpr int numBandParams = 7;

    // IDs for bands 1-3 freq/gain/q are unchanged from v0.1 so automation and
    // saved sessions keep pointing at the same parameters.
    inline juce::String paramId (int band, Param p)
    {
        static const char* const suffix[] = { "freq", "gain", "q", "type", "on", "slope", "stereo" };
        static_assert (std::size (suffix) == (size_t) numBandParams);
        return "band" + juce::String (band + 1) + "_" + suffix[(int) p];
    }

    // Global (non-band) host parameters.
    inline const char* const outputGainId = "output_gain";
    inline const char* const bypassId     = "bypass";
    inline const char* const autoGainId   = "auto_gain";

    constexpr float minOutputGain = -24.0f, maxOutputGain = 24.0f;
}
