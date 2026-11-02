#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include "EQParams.h"

class ThreeBandEQAudioProcessor;

namespace EQ
{
    // Complete user-visible state of one band (used for undo/redo).
    struct BandSnapshot
    {
        bool used = false, enabled = true;
        int type = 0;
        float freq = 1000.0f, gain = 0.0f, q = defaultQ;

        bool operator== (const BandSnapshot& o) const noexcept
        {
            return used == o.used && enabled == o.enabled && type == o.type
                && freq == o.freq && gain == o.gain && q == o.q;
        }
        bool operator!= (const BandSnapshot& o) const noexcept { return ! (*this == o); }
    };

    // All user-driven edits go through here (message thread only), so that:
    //  * the host sees proper begin/end gestures (automation write / touch),
    //  * every drag / wheel burst / panel edit becomes exactly one undo step,
    //  * host automation never pollutes the undo history.
    class EditController
    {
    public:
        explicit EditController (ThreeBandEQAudioProcessor& p) : processor (p) {}

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
        void setEnabled (int band, bool enabled);
        void setValue (int band, Param p, float value);

        juce::UndoManager& getUndoManager() noexcept { return undoManager; }
        bool undo();
        bool redo();

    private:
        void commit (int band, const BandSnapshot& before, const BandSnapshot& after, const juce::String& name);
        void setParamWithGesture (int band, Param p, float value);

        ThreeBandEQAudioProcessor& processor;
        juce::UndoManager undoManager { 30000, 30 };

        int editBand = -1;
        juce::String editName;
        BandSnapshot editBefore;
        std::array<bool, 5> gestureOpen {};
    };
}
