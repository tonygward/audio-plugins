#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CompressorEngine.h"

// Songsmith Compressor: parameter IDs, ranges, defaults and layout, shared by the processor, the
// editor and the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for
// something else; add new parameters with a new ID and a higher ParameterID version.
namespace Comp
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* thresholdId  = "threshold";     // dBFS
    inline constexpr const char* ratioId      = "ratio";         // n : 1
    inline constexpr const char* attackId     = "attack";        // ms
    inline constexpr const char* releaseId    = "release";       // ms
    inline constexpr const char* kneeId       = "knee";          // dB
    inline constexpr const char* makeupId     = "makeup";        // dB
    inline constexpr const char* autoMakeupId = "auto_makeup";   // on: level-match instead of Makeup Gain
    inline constexpr const char* mixId        = "mix";           // %
    inline constexpr const char* bypassId     = "bypass";        // the host's native bypass

    // --- Added in version 2 (Analog Warmth) ---------------------------------------------
    inline constexpr const char* warmthId     = "warmth";        // off by default: the clean compressor

    // --- Defaults: gentle and musically useful -------------------------------------------
    constexpr float defaultThresholdDb = -18.0f, defaultRatio = 2.0f, defaultAttackMs = 10.0f, defaultReleaseMs = 100.0f,
                    defaultKneeDb = 6.0f, defaultMakeupDb = 0.0f, defaultMixPercent = 100.0f;
    constexpr bool defaultAutoMakeup = true;

    // A logarithmic 0..1 mapping with a snap grid.
    inline juce::NormalisableRange<float> logRange (float lo, float hi, float step)
    {
        const float logLo = std::log (lo), logHi = std::log (hi);
        return { lo, hi,
                 [logLo, logHi] (float, float, float v) { return std::exp (logLo + v * (logHi - logLo)); },
                 [logLo, logHi] (float, float, float x) { return (std::log (juce::jmax (1.0e-6f, x)) - logLo) / (logHi - logLo); },
                 [lo, hi, step] (float, float, float x) { return juce::jlimit (lo, hi, std::round (x / step) * step); } };
    }

    // Ratio is finest between 1 and 3 (where gentle compression lives): the control travel is
    // 55 % for 1 to 3, the rest for 3 to 20.
    inline juce::NormalisableRange<float> ratioRange()
    {
        constexpr float knee = 3.0f, share = 0.55f;
        return { SongsmithComp::minRatio, SongsmithComp::maxRatio,
                 [] (float lo, float hi, float v)
                 {
                     return v <= share ? lo + (knee - lo) * (v / share)
                                       : knee + (hi - knee) * ((v - share) / (1.0f - share));
                 },
                 [] (float lo, float hi, float x)
                 {
                     return x <= knee ? share * (x - lo) / (knee - lo)
                                      : share + (1.0f - share) * (x - knee) / (hi - knee);
                 },
                 [] (float lo, float hi, float x) { return juce::jlimit (lo, hi, std::round (x * 100.0f) / 100.0f); } };
    }

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int designWidth = 900, contentHeight = 372;   // the controls sit in the top 372
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 675, maxWidth = 2700;           // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }

    // Display ballistics for the meters (UI only). Instant rise, a constant fall in dB per second, and a peak hold.
    class MeterBallistics
    {
    public:
        static constexpr float fallDbPerSecond = 20.0f, holdSeconds = 1.5f;

        explicit MeterBallistics (float floorDb = -60.0f) : floor (floorDb), level (floorDb), hold (floorDb) {}

        void update (float db, double elapsed) noexcept
        {
            db = juce::jmax (floor, db);
            level = juce::jmax (db, level - fallDbPerSecond * (float) elapsed);
            holdAge += elapsed;
            if (db >= hold || holdAge > (double) holdSeconds)
            {
                hold = juce::jmax (db, level);
                holdAge = 0.0;
            }
        }

        void reset() noexcept { level = hold = floor; holdAge = 0.0; }
        float getLevel() const noexcept { return level; }
        float getHold() const noexcept  { return hold; }

    private:
        float floor, level, hold;
        double holdAge = 0.0;
    };

    inline float gainToDb (float gain) noexcept
    {
        return gain > 0.0f ? juce::jmax (-120.0f, 20.0f * std::log10 (gain)) : -120.0f;
    }
}
