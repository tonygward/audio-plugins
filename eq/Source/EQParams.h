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

    enum class Param { Freq, Gain, Q, Type, Enabled };

    // IDs for bands 1-3 freq/gain/q are unchanged from v0.1 so automation and
    // saved sessions keep pointing at the same parameters.
    inline juce::String paramId (int band, Param p)
    {
        static const char* const suffix[] = { "freq", "gain", "q", "type", "on" };
        return "band" + juce::String (band + 1) + "_" + suffix[(int) p];
    }

    // Global (non-band) host parameters.
    inline const char* const outputGainId = "output_gain";
    inline const char* const bypassId     = "bypass";

    constexpr float minOutputGain = -24.0f, maxOutputGain = 24.0f;
}
