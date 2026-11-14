#pragma once

#include <array>

// Factory presets. Loading one sets every sound parameter and leaves Bypass alone. They are starting
// points for a first listen, named for what they are used for. The Acoustic Guitar values follow the
// published idea of very gentle, steady compression (a low threshold with a ratio near 1.3 to 1,
// about 30 ms attack and 50 ms release); the others are Songsmith starting points, not copies of
// any other person's settings. Sources and the test for each are in compressor/docs/presets.md.
namespace Comp
{
    struct Preset
    {
        const char* name;
        const char* hint;
        float thresholdDb, ratio, attackMs, releaseMs, kneeDb, makeupDb, mixPercent;
        bool autoMakeup;
    };

    inline const std::array<Preset, 4>& getPresets()
    {
        static const std::array<Preset, 4> presets { {
            { "Acoustic Guitar",
              "Gentle, steady smoothing for acoustic guitar. A low threshold with a very low ratio turns the level down a little all the time instead of only catching the loudest notes. Lower Threshold until Gain Reduction moves a little; if it never moves, nothing is being changed.",
              -30.0f, 1.3f, 30.0f, 50.0f, 12.0f, 0.0f, 100.0f, true },
            { "Lead Vocal Control",
              "Keeps a lead vocal more even. Raise Threshold if it sounds squashed, lower it if the loud words still jump out. Gain Reduction of a few decibels on the loudest words is usually plenty.",
              -24.0f, 3.0f, 8.0f, 120.0f, 9.0f, 0.0f, 100.0f, true },
            { "Drum Bus Glue",
              "Holds a group of drums together. A slowish attack lets the first hit of each beat through. Listen for pumping and lengthen Release if the level seems to breathe.",
              -18.0f, 2.0f, 30.0f, 100.0f, 6.0f, 0.0f, 100.0f, true },
            { "Gentle Level Smoothing",
              "A light, all-round setting that evens out small level differences on almost any single track or mix. Often a good place to start.",
              -20.0f, 1.5f, 20.0f, 150.0f, 12.0f, 0.0f, 100.0f, true },
        } };
        return presets;
    }
}
