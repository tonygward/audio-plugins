#pragma once

#include <juce_core/juce_core.h>
#include "MaxParams.h"

// Editor behaviour that doesn't need a window, so the tests can check it directly.
namespace Max
{
    // The editor is laid out once at a design size and scaled as a whole (text, controls, meters),
    // so every size looks the same. The window keeps the design aspect ratio.
    namespace EditorLayout
    {
        constexpr int designWidth = 720, contentHeight = 420;   // the controls sit in the top 420
        constexpr int footerHeight = 28;                      // the latency and Processing Load strip below them
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = 540, maxWidth = 2160;   // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }

    // Linked Threshold / Out Ceiling: moving one moves the other by the same amount, so C - T (the
    // gain below threshold) stays exactly the same. The movement stops where either control would
    // leave its range (the common movement range), instead of squashing C - T at the ends.
    struct ThresholdCeiling { float threshold, ceiling; };

    inline ThresholdCeiling moveLinked (float threshold, float ceiling, float newValue, bool movingThreshold) noexcept
    {
        const float lo = juce::jmax (minThresholdDb - threshold, minCeilingDb - ceiling);
        const float hi = juce::jmin (maxThresholdDb - threshold, maxCeilingDb - ceiling);
        const float delta = juce::jlimit (juce::jmin (lo, 0.0f), juce::jmax (hi, 0.0f),
                                          newValue - (movingThreshold ? threshold : ceiling));
        return { threshold + delta, ceiling + delta };
    }

    // Display ballistics for the meters (UI only: the audio thread publishes raw block peaks and
    // never sees these). Instant rise, a constant fall in dB per second, and a peak hold.
    class MeterBallistics
    {
    public:
        static constexpr float fallDbPerSecond = 20.0f, holdSeconds = 1.5f;

        explicit MeterBallistics (float floorDb = -60.0f) : floor (floorDb), level (floorDb), hold (floorDb) {}

        // 'db' is the newest reading, 'elapsed' the seconds since the previous update.
        void update (float db, double elapsed) noexcept
        {
            db = juce::jmax (floor, db);
            level = juce::jmax (db, level - fallDbPerSecond * (float) elapsed);

            holdAge += elapsed;
            if (db >= hold || holdAge > (double) holdSeconds)
            {
                hold = juce::jmax (db, level);
                holdAge = db >= hold ? 0.0 : holdAge;
            }
        }

        void reset() noexcept { level = hold = floor; holdAge = 0.0; }

        float getLevel() const noexcept { return level; }
        float getHold() const noexcept  { return hold; }

    private:
        float floor, level, hold;
        double holdAge = 0.0;
    };
}
