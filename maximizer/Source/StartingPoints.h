#pragma once

#include <array>

// Factory starting points (step 05). Deliberately conservative: a few dB of gain at most, a
// -1 dB ceiling (true peak where the result leaves the DAW), and dither always off, since it
// belongs only on the very last 16-bit export. Loading one sets these parameters and leaves
// Bypass and Link alone.
namespace Max
{
    struct StartingPoint
    {
        const char* name;
        const char* hint;
        float thresholdDb, ceilingDb, releaseMs;
        int peakMode;       // 0 sample peak, 1 true peak
        bool autoRelease;
        int dither;         // always 0 (off)
    };

    inline const std::array<StartingPoint, 3>& getStartingPoints()
    {
        static const std::array<StartingPoint, 3> points { {
            { "Mix bus",
              "A safety net on the mix bus: +0.5 dB, catches the odd peak at -1 dBFS. Sample peak, so it stays light on CPU.",
              -1.5f, -1.0f, 10.0f, 0, true, 0 },
            { "Demo loudness",
              "Louder demos and reference bounces: +5 dB into a -1 dBTP ceiling, auto release. Listen for pumping on dense mixes.",
              -6.0f, -1.0f, 3.0f, 1, true, 0 },
            { "Final export",
              "The last stage before export: +2 dB into -1 dBTP with a 5 ms release (the L1 manual's 3-7 ms mastering range). Turn Dither to 16-bit only for a 16-bit file.",
              -3.0f, -1.0f, 5.0f, 1, false, 0 },
        } };
        return points;
    }
}
