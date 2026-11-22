#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "SongsmithAutoGain.h"

// songsmithstudio#16: the processing behind the shared Output panel's Output Trim and Auto Gain controls, for every
// plugin that adds them. One implementation, so Auto Gain means the same thing everywhere.
//
// Order, which is the whole contract:  the plugin's own processing  ->  Auto Gain  ->  Output Trim  ->  Bypass mix.
//   - Auto Gain compares the loudness going in with the loudness coming out of the plugin's own processing (K-weighted,
//     like a loudness meter, over two seconds) and corrects the difference, by at most 12 dB either way. It freezes in
//     silence, so fades and gaps never pump.
//   - Output Trim is applied after Auto Gain and is not measured, so the musician's own setting always has the last
//     word and Auto Gain can never fight it.
//   - With Output Trim at 0 dB and Auto Gain off the audio is left untouched (bit exact), so sessions saved before
//     these controls existed sound exactly as they did.
//
// Nothing here allocates, locks or logs after construction. Changes are smoothed, so nothing clicks.
namespace Songsmith
{
    class OutputStage
    {
    public:
        static constexpr double smoothingSeconds = 0.05;
        static constexpr double autoGainSmoothingSeconds = 0.25;

        void prepare (double sampleRate)
        {
            autoGain.prepare (sampleRate);
            trimSmoothed.reset (sampleRate, smoothingSeconds);
            trimSmoothed.setCurrentAndTargetValue (1.0f);
            autoSmoothed.reset (sampleRate, autoGainSmoothingSeconds);
            autoSmoothed.setCurrentAndTargetValue (1.0f);
            wasOn = false;
            appliedDb.store (0.0f, std::memory_order_relaxed);
        }

        // Sets the trim to start from, without gliding (prepareToPlay, so a loaded session does not fade in from 0 dB).
        void setTrimImmediately (float trimDb) noexcept
        {
            trimSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (trimDb));
        }

        // Call at the start of each block with the Auto Gain setting, before anything is measured.
        void beginBlock (bool autoGainOn) noexcept
        {
            if (autoGainOn && ! wasOn)
            {
                autoGain.reset();                // a fresh measurement window
                autoGain.setCorrectionDb (0.0);  // start from unity and glide to the match
            }
            wasOn = autoGainOn;
        }

        // The loudness going in: call on the input chunk, before the plugin changes it. Does nothing while Auto Gain is off.
        void measureInput (const float* const* ch, int numChannels, int num) noexcept
        {
            if (wasOn)
                autoGain.measureInput (ch, numChannels, num);
        }

        // The loudness coming out of the plugin's own processing, then Auto Gain and Output Trim. Call on the same chunk
        // after the plugin's processing and before the Bypass mix.
        void process (float* const* ch, int numChannels, int num, float trimDb) noexcept
        {
            if (wasOn)
            {
                autoGain.measureOutput (ch, numChannels, num);
                autoGain.advance (num);
                autoSmoothed.setTargetValue ((float) juce::Decibels::decibelsToGain (autoGain.getCorrectionDb()));
            }
            else
            {
                autoSmoothed.setTargetValue (1.0f);
            }
            trimSmoothed.setTargetValue (juce::Decibels::decibelsToGain (trimDb));

            if (autoSmoothed.isSmoothing() || trimSmoothed.isSmoothing())
            {
                for (int i = 0; i < num; ++i)
                {
                    const float g = autoSmoothed.getNextValue() * trimSmoothed.getNextValue();
                    for (int c = 0; c < numChannels; ++c)
                        ch[c][i] *= g;
                }
            }
            else if (const float g = autoSmoothed.getTargetValue() * trimSmoothed.getTargetValue(); g != 1.0f)
            {
                for (int c = 0; c < numChannels; ++c)
                    juce::FloatVectorOperations::multiply (ch[c], g, num);
            }

            appliedDb.store (juce::Decibels::gainToDecibels (autoSmoothed.getCurrentValue(), -100.0f), std::memory_order_relaxed);
        }

        // For the editor: the correction Auto Gain is applying right now, in dB (0 when off). Safe from any thread.
        float getAutoGainAppliedDb() const noexcept { return appliedDb.load (std::memory_order_relaxed); }

    private:
        AutoGain autoGain;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> trimSmoothed { 1.0f }, autoSmoothed { 1.0f };
        std::atomic<float> appliedDb { 0.0f };
        bool wasOn = false;
    };
}
