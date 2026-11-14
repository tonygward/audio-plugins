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

    // Tooltip for the band panel's Dynamic button. When the button is greyed out it says why and
    // what to change, never offering a route that would not enable it (EQ-36).
    inline juce::String dynamicTooltip (FilterType t, bool linearPhase)
    {
        const bool filterOk = typeHasGain (t);
        if (filterOk && ! linearPhase)
            return "Dynamic EQ changes this band's gain in response to the sound level in its frequency range.";

        const auto name = getTypeNames()[(int) t];
        if (! filterOk && linearPhase)
            return "Dynamic EQ isn't available on " + name + " filters or in Linear phase mode. "
                   "Select Bell, Low Shelf or High Shelf and switch to Zero latency mode.";
        if (! filterOk)
            return "Dynamic EQ isn't available on " + name + " filters. "
                   "Use Bell, Low Shelf or High Shelf to enable dynamic processing.";
        return "Dynamic EQ isn't available in Linear phase mode. "
               "Switch to Zero latency mode to use it with Bell, Low Shelf or High Shelf filters.";
    }

    // Tooltip for the processing-mode selector (EQ-34): what Linear phase does, when to try it,
    // why it adds delay (the filter has to look ahead) and why Zero latency suits recording.
    inline const char* linearPhaseTooltip()
    {
        return "Try Linear phase when blending a filtered track with a copy of the same sound. "
               "It avoids shifting the timing of some frequencies more than others, but adds delay "
               "(about 43 to 186 ms at 44.1 or 48 kHz, longer for higher quality) and turns off "
               "Dynamic EQ and Solo. Leave it on Zero latency for recording and most mixing.";
    }
    constexpr float minFreq = 20.0f,   maxFreq = 20000.0f;
    constexpr float minGain = -30.0f,  maxGain = 30.0f;
    constexpr float minQ    = 0.1f,    maxQ    = 40.0f;
    constexpr float defaultQ = 0.707f;

    // Adding a band from the graph (task 29): a click below lowCutZoneHz adds a Low Cut,
    // above highCutZoneHz a High Cut, anywhere else a Bell. By frequency, so the zones are the
    // same at every window and UI size (about the outer 10 % / 7 % of the 20 Hz-20 kHz axis).
    constexpr float lowCutZoneHz = 40.0f, highCutZoneHz = 12000.0f;

    inline FilterType typeForNewBand (float freq, bool forceBell)
    {
        if (! forceBell && freq < lowCutZoneHz)  return FilterType::LowCut;
        if (! forceBell && freq > highCutZoneHz) return FilterType::HighCut;
        return FilterType::Bell;
    }

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

    enum class Param { Freq, Gain, Q, Type, Enabled, Slope, Stereo,
                       DynOn, Threshold, Range, Attack, Release, DynExternal };
    constexpr int numBandParams = 13;

    // Dynamic EQ parameter ranges and defaults (dynamics off by default).
    constexpr float minThreshold = -60.0f, maxThreshold = 0.0f, defaultThreshold = -30.0f;
    constexpr float minRange = -24.0f, maxRange = 24.0f, defaultRange = -6.0f;
    constexpr float minAttackMs = 0.1f, maxAttackMs = 100.0f, defaultAttackMs = 10.0f;
    constexpr float minReleaseMs = 10.0f, maxReleaseMs = 1000.0f, defaultReleaseMs = 150.0f;

    // IDs for bands 1-3 freq/gain/q are unchanged from v0.1 so automation and
    // saved sessions keep pointing at the same parameters.
    inline juce::String paramId (int band, Param p)
    {
        static const char* const suffix[] = { "freq", "gain", "q", "type", "on", "slope", "stereo",
                                              "dyn_on", "threshold", "range", "attack", "release", "dyn_ext" };
        static_assert (std::size (suffix) == (size_t) numBandParams);
        return "band" + juce::String (band + 1) + "_" + suffix[(int) p];
    }

    // Global (non-band) host parameters.
    inline const char* const outputGainId = "output_gain";
    inline const char* const bypassId     = "bypass";
    inline const char* const autoGainId   = "auto_gain";
    inline const char* const gainScaleId  = "gain_scale";
    inline const char* const phaseModeId  = "phase_mode";   // 0 zero latency, 1 linear phase
    inline const char* const lpQualityId  = "lp_quality";   // linear-phase FIR length: low, medium, high
    inline const char* const oversamplingId = "oversampling"; // 0 off, 1 = 2x, 2 = 4x
    inline const char* const hfAccuracyId = "hf_accuracy";    // EQ-44 "Accurate Highs": matched bells and shelves (on for new instances)

    constexpr float minGainScale = -100.0f, maxGainScale = 200.0f;   // percent; 100 = as set

    constexpr float minOutputGain = -24.0f, maxOutputGain = 24.0f;
}
