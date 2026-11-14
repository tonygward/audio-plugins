#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include "EQParams.h"

class SongsmithEQAudioProcessor;

namespace EQ
{
    // Complete user-visible state of one band (used for undo/redo).
    struct BandSnapshot
    {
        bool used = false, enabled = true;
        int type = 0, slope = defaultSlope, stereo = 0;
        float freq = 1000.0f, gain = 0.0f, q = defaultQ;
        bool dynOn = false, dynExternal = false;
        float threshold = defaultThreshold, range = defaultRange, attack = defaultAttackMs, release = defaultReleaseMs;

        bool operator== (const BandSnapshot& o) const noexcept
        {
            return used == o.used && enabled == o.enabled && type == o.type && slope == o.slope && stereo == o.stereo
                && freq == o.freq && gain == o.gain && q == o.q
                && dynOn == o.dynOn && dynExternal == o.dynExternal && threshold == o.threshold
                && range == o.range && attack == o.attack && release == o.release;
        }
        bool operator!= (const BandSnapshot& o) const noexcept { return ! (*this == o); }
    };

    // Everything a preset holds: all 24 bands plus the global sound-shaping parameters.
    // (Not bypass, and no UI state.)
    struct PresetData
    {
        std::array<BandSnapshot, numBands> bands {};
        float outputGain = 0.0f;
        float gainScale = 100.0f;   // percent
        bool accurateHighs = true;  // EQ-44: the filter design the preset was made with (an older preset loads as false)

        // Same sound: unused bands only have to agree on being unused.
        bool sameSound (const PresetData& o) const noexcept
        {
            if (std::abs (outputGain - o.outputGain) > 0.001f || std::abs (gainScale - o.gainScale) > 0.05f || accurateHighs != o.accurateHighs)
                return false;

            // Tolerant float compare: values go through the parameters' normalised ranges.
            for (size_t b = 0; b < bands.size(); ++b)
            {
                const auto& x = bands[b];
                const auto& y = o.bands[b];
                if (x.used != y.used)
                    return false;
                if (x.used && (x.enabled != y.enabled || x.type != y.type || x.slope != y.slope || x.stereo != y.stereo
                               || std::abs (x.freq - y.freq) > x.freq * 1.0e-4f
                               || std::abs (x.gain - y.gain) > 0.005f
                               || std::abs (x.q - y.q) > 0.005f
                               || x.dynOn != y.dynOn || x.dynExternal != y.dynExternal
                               || (x.dynOn && (std::abs (x.threshold - y.threshold) > 0.005f || std::abs (x.range - y.range) > 0.005f
                                              || std::abs (x.attack - y.attack) > x.attack * 1.0e-3f
                                              || std::abs (x.release - y.release) > x.release * 1.0e-3f))))
                    return false;
            }
            return true;
        }
    };

    // All user-driven edits go through here (message thread only), so that:
    //  * the host sees proper begin/end gestures (automation write / touch),
    //  * every drag / wheel burst / panel edit becomes exactly one undo step,
    //  * host automation never pollutes the undo history.
    class EditController
    {
    public:
        explicit EditController (SongsmithEQAudioProcessor& p) : processor (p) {}

        BandSnapshot getSnapshot (int band) const;
        void applySnapshot (int band, const BandSnapshot&);

        // Continuous edits (drag, wheel burst): one undo transaction.
        void beginEdit (int band, const juce::String& name);
        void setParam (Param p, float value);     // applies to the band being edited
        void endEdit();
        bool isEditing() const noexcept { return editBand >= 0; }
        int  getEditBand() const noexcept { return editBand; }

        // Discrete edits: each is one undo transaction.
        int  addBand (float freq, float gain, FilterType type);   // returns band index or -1 when full
        void removeBand (int band);
        void resetBand (int band);
        void setType (int band, FilterType type);
        void setSlope (int band, int slopeIndex);
        void setStereoMode (int band, int mode);
        void setDynamic (int band, bool on);
        void setEnabled (int band, bool enabled);
        void setValue (int band, Param p, float value);

        // Whole-EQ state (presets). loadPreset is one undo transaction.
        PresetData getPresetData() const;
        void applyPresetData (const PresetData&);   // no undo; used by the undo action itself
        void loadPreset (const PresetData&, const juce::String& name);

        juce::UndoManager& getUndoManager() noexcept { return undoManager; }
        bool undo();
        bool redo();

    private:
        void commit (int band, const BandSnapshot& before, const BandSnapshot& after, const juce::String& name);
        void setParamWithGesture (int band, Param p, float value);

        SongsmithEQAudioProcessor& processor;
        juce::UndoManager undoManager { 30000, 30 };

        int editBand = -1;
        juce::String editName;
        BandSnapshot editBefore;
        std::array<bool, numBandParams> gestureOpen {};
    };
}
