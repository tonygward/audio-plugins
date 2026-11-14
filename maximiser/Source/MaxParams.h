#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

// Songsmith Maximiser: parameter IDs, ranges and defaults, shared by the processor, the
// editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an
// ID for something else; add new parameters with a new ID and a higher ParameterID version.
namespace Max
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* thresholdId = "threshold";   // dBFS
    inline constexpr const char* ceilingId   = "ceiling";     // dBFS ("Out Ceiling" in the L1 manual)
    inline constexpr const char* releaseId   = "release";     // ms
    inline constexpr const char* bypassId    = "bypass";      // the host's native bypass

    // --- Added in version 2 (step 03) -----------------------------------------------------
    inline constexpr const char* peakModeId    = "peak_mode";      // 0 sample peak, 1 true peak (default)
    inline constexpr const char* autoReleaseId = "auto_release";   // off by default: manual release

    inline const juce::StringArray& getPeakModeNames()
    {
        static const juce::StringArray names { "Sample peak", "True peak" };
        return names;
    }

    // --- Added in version 3 (step 04) -----------------------------------------------------
    inline constexpr const char* ditherId = "dither";   // 0 off (default), 1 16-bit, 2 24-bit TPDF

    inline const juce::StringArray& getDitherNames()
    {
        static const juce::StringArray names { "Off", "16-bit", "24-bit" };
        return names;
    }

    // --- Ranges and defaults -------------------------------------------------------------
    // Threshold and Out Ceiling both span 0 to -30 dB, as in the L1 manual. The defaults (0 / 0)
    // give a gain of 0 dB, so a fresh instance passes audio below 0 dBFS through untouched.
    constexpr float minThresholdDb = -30.0f, maxThresholdDb = 0.0f, defaultThresholdDb = 0.0f;
    constexpr float minCeilingDb   = -30.0f, maxCeilingDb   = 0.0f, defaultCeilingDb   = 0.0f;

    // The manual's release range, 0.01 to 1000 ms, on a logarithmic control. Default 100 ms (MAX-10),
    // the default the current L1 manual gives (p. 17, as quoted by secondary sources; not checked against
    // the PDF). The legacy manual only suggests 1 ms for many sources and 3-7 ms as a mastering start,
    // which the starting points keep; 1 ms as the default put about 17 % distortion on sustained bass
    // (README, "Auto release vs manual"). Saved sessions keep their own Release.
    constexpr float minReleaseMs = 0.01f, maxReleaseMs = 1000.0f, defaultReleaseMs = 100.0f;

    // Practical finite dB floor. The manual shows silence and full attenuation as minus
    // infinity; here every gain/level conversion stops at -120 dB (well below 24-bit noise,
    // -144 dBFS is never reached by real material after dither), so meters, readouts and
    // state never hold -inf or NaN. Gains at or below the floor convert to exactly 0.
    constexpr float dbFloor = -120.0f;

    inline float dbToGain (float db) noexcept
    {
        return db <= dbFloor ? 0.0f : std::pow (10.0f, db * 0.05f);
    }

    inline float gainToDb (float gain) noexcept
    {
        return gain > 0.0f ? juce::jmax (dbFloor, 20.0f * std::log10 (gain)) : dbFloor;
    }

    // Logarithmic 0..1 mapping for the release control: 0.01, 0.1, 1, 10, 100, 1000 ms sit at
    // equal steps, so the useful 1-10 ms region gets a fifth of the travel.
    inline juce::NormalisableRange<float> releaseRange()
    {
        const float logMin = std::log (minReleaseMs), logMax = std::log (maxReleaseMs);
        return { minReleaseMs, maxReleaseMs,
                 [logMin, logMax] (float, float, float v) { return std::exp (logMin + v * (logMax - logMin)); },
                 [logMin, logMax] (float, float, float ms) { return (std::log (ms) - logMin) / (logMax - logMin); },
                 [] (float, float, float ms)
                 {
                     // Two significant figures below 1 ms, then 0.01 ms steps up to 10 ms, then 0.1 ms.
                     ms = juce::jlimit (minReleaseMs, maxReleaseMs, ms);
                     const float step = ms < 1.0f ? 0.001f : (ms < 10.0f ? 0.01f : 0.1f);
                     return juce::jlimit (minReleaseMs, maxReleaseMs, std::round (ms / step) * step);
                 } };
    }
}
