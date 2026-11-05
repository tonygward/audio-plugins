#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Editor window limits and the UI size setting, shared by the processor (saved state) and the
// editor. Sizes are window sizes in logical pixels (what the host and OS call the window size;
// the OS display scaling, e.g. Windows 150 % or Retina, comes on top).
namespace EQ::EditorSize
{
    // Layout at UI size 100 %: the top bar, band panel and graph get comfortable room with the
    // fonts from task 24. Window = design size x UI size.
    // The strip along the bottom (latency and Processing Load) takes diagnosticsHeight of it, so the
    // graph keeps the room it had.
    constexpr int diagnosticsHeight = 28;
    constexpr int defaultWidth = 1280, defaultHeight = 760 + diagnosticsHeight;

    // Design-unit minimum: the three-row top bar fits spelled out at 760 (task 24). At UI size
    // s the minimum window is this times s.
    constexpr int minWidth = 760, minHeight = 440 + diagnosticsHeight;

    // Large enough to fill a 4K screen (3840 x 2160 at 100 % display scaling), or a 16:10 one.
    constexpr int maxWidth = 3840, maxHeight = 2400;

    // Width / height range, so the graph is never squashed.
    constexpr double minAspect = 1.3, maxAspect = 2.4;

    // The "UI size" options: everything (text, controls, nodes, strokes, spacing) scales together.
    constexpr float uiScales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    constexpr int numUiScales = 6;

    // The nearest option, so a saved or hand-edited value is always one of them.
    inline float snapUiScale (float s)
    {
        if (! std::isfinite (s))
            return 1.0f;

        float best = 1.0f;
        for (auto option : uiScales)
            if (std::abs (option - s) < std::abs (best - s))
                best = option;
        return best;
    }

    // A window size clamped to the limits for UI size 'uiScale': at least the minimum design
    // size, at most the maximum window, and inside the aspect-ratio range.
    inline juce::Point<int> constrain (int width, int height, float uiScale)
    {
        const int minW = juce::roundToInt ((float) minWidth * uiScale), minH = juce::roundToInt ((float) minHeight * uiScale);
        width  = juce::jlimit (juce::jmin (minW, maxWidth),  maxWidth,  width);
        height = juce::jlimit (juce::jmin (minH, maxHeight), maxHeight, height);

        if ((double) width / height > maxAspect)  width  = juce::jmax (juce::jmin (minW, maxWidth),  (int) std::floor (height * maxAspect));
        if ((double) width / height < minAspect)  height = juce::jmax (juce::jmin (minH, maxHeight), (int) std::floor (width / minAspect));
        return { width, height };
    }
}
