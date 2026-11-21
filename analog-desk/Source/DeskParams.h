#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DeskEngine.h"
#include "../../shared/Source/SongsmithMeterColumn.h"
#include "../../shared/Source/SongsmithHeader.h"

// Songsmith Analog Desk: parameter IDs, ranges, defaults, and the editor's layout, shared by the processor, the editor and
// the tests.
//
// The IDs are what hosts store automation and sessions under. NEVER rename one or reuse an ID for something else;
// add new parameters with a new ID and a higher ParameterID version.
namespace Desk
{
    // --- Stable parameter IDs (version 1) ------------------------------------------------
    inline constexpr const char* inputTrimId   = "input_trim";       // dB
    inline constexpr const char* outputTrimId  = "output_trim";      // dB
    inline constexpr const char* driveId       = "drive";            // 0 to 1
    inline constexpr const char* highPassId    = "high_pass";        // Hz; the lowest setting is off
    inline constexpr const char* lowPassId     = "low_pass";         // Hz; the highest setting is off

    inline constexpr const char* eqOnId        = "eq_on";
    inline constexpr const char* lfFreqId      = "lf_frequency";
    inline constexpr const char* lfGainId      = "lf_gain";
    inline constexpr const char* lfBellId      = "lf_bell";          // false = shelf
    inline constexpr const char* lmfFreqId     = "lmf_frequency";
    inline constexpr const char* lmfGainId     = "lmf_gain";
    inline constexpr const char* lmfQId        = "lmf_q";
    inline constexpr const char* hmfFreqId     = "hmf_frequency";
    inline constexpr const char* hmfGainId     = "hmf_gain";
    inline constexpr const char* hmfQId        = "hmf_q";
    inline constexpr const char* hfFreqId      = "hf_frequency";
    inline constexpr const char* hfGainId      = "hf_gain";
    inline constexpr const char* hfBellId      = "hf_bell";          // false = shelf

    inline constexpr const char* dynamicsOnId  = "dynamics_on";
    inline constexpr const char* afterEqId     = "dynamics_after_eq";
    inline constexpr const char* compThresholdId = "comp_threshold"; // dBFS
    inline constexpr const char* compRatioId   = "comp_ratio";       // 1 = off
    inline constexpr const char* compAttackId  = "comp_attack";      // ms
    inline constexpr const char* compReleaseId = "comp_release";     // s
    inline constexpr const char* compKneeId    = "comp_soft_knee";
    inline constexpr const char* compMakeupId  = "comp_makeup";      // dB
    inline constexpr const char* gateOnId      = "gate_on";
    inline constexpr const char* gateExpanderId = "gate_expander";   // false = gate, true = expander
    inline constexpr const char* gateThresholdId = "gate_threshold"; // dBFS
    inline constexpr const char* gateRangeId   = "gate_range";       // dB
    inline constexpr const char* gateAttackId  = "gate_attack";      // ms
    inline constexpr const char* gateHoldId    = "gate_hold";        // s
    inline constexpr const char* gateReleaseId = "gate_release";     // s

    inline constexpr const char* bypassId      = "bypass";           // the host's native bypass

    // --- Ranges: the engine's, and the two filters' "off" ends ------------------------------
    // The High-Pass dial's lowest setting (below the engine's lowest frequency) means off; so does the Low-Pass dial's highest.
    inline constexpr float highPassOffBelowHz = SongsmithDesk::minHighPassHz, highPassDialMinHz = 15.0f;
    inline constexpr float lowPassOffAboveHz = SongsmithDesk::maxLowPassHz, lowPassDialMaxHz = 22500.0f;

    inline float highPassFromDial (float hz) noexcept { return hz < highPassOffBelowHz ? 0.0f : hz; }
    inline float lowPassFromDial (float hz) noexcept  { return hz > lowPassOffAboveHz ? 0.0f : hz; }

    // --- Defaults: a strip that leaves the sound exactly as it is -----------------------------
    constexpr float defaultLfHz = 100.0f, defaultLmfHz = 600.0f, defaultHmfHz = 2500.0f, defaultHfHz = 8000.0f;
    constexpr float defaultQ = 1.0f;
    constexpr float defaultGateThresholdDb = -50.0f, defaultGateRangeDb = 20.0f;
    constexpr float defaultGateAttackMs = 1.0f, defaultGateHoldSeconds = 0.1f, defaultGateReleaseSeconds = 0.3f;
    constexpr float defaultCompAttackMs = 3.0f, defaultCompReleaseSeconds = 0.3f;

    // The editor is laid out once at a design size and scaled as a whole, so every size looks the same.
    namespace EditorLayout
    {
        constexpr int leftMargin = 24, cellWidth = 100, cellsAcross = 10;
        constexpr int controlsRight = leftMargin + cellWidth * cellsAcross;
        constexpr int designWidth = controlsRight + Songsmith::UI::MeterColumn::contentGap
                                    + Songsmith::UI::MeterColumn::widthFor (true) + Songsmith::UI::MeterColumn::margin;
        constexpr int contentHeight = 500;                      // the controls; the footer strip sits below
        constexpr int footerHeight = 28;                         // the latency and Processing Load strip
        constexpr int designHeight = contentHeight + footerHeight;
        constexpr int defaultWidth = designWidth;
        constexpr int minWidth = (designWidth * 3) / 4, maxWidth = designWidth * 3;   // 75 % to 300 %

        inline int constrainWidth (int width) noexcept { return juce::jlimit (minWidth, maxWidth, width); }
        inline int heightFor (int width) noexcept      { return juce::roundToInt ((double) width * designHeight / designWidth); }
    }
}
