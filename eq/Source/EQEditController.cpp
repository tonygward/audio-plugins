#include "EQEditController.h"
#include "PluginProcessor.h"

namespace EQ
{
    namespace
    {
        class BandAction final : public juce::UndoableAction
        {
        public:
            BandAction (EditController& c, int b, const BandSnapshot& before, const BandSnapshot& after)
                : controller (c), band (b), beforeState (before), afterState (after) {}

            bool perform() override { controller.applySnapshot (band, afterState);  return true; }
            bool undo() override    { controller.applySnapshot (band, beforeState); return true; }
            int getSizeInUnits() override { return 1; }

        private:
            EditController& controller;
            int band;
            BandSnapshot beforeState, afterState;
        };

        void setParamValue (juce::RangedAudioParameter* p, float value)
        {
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        class PresetAction final : public juce::UndoableAction
        {
        public:
            PresetAction (EditController& c, const PresetData& before, const PresetData& after)
                : controller (c), beforeState (before), afterState (after) {}

            bool perform() override { controller.applyPresetData (afterState);  return true; }
            bool undo() override    { controller.applyPresetData (beforeState); return true; }
            int getSizeInUnits() override { return numBands; }

        private:
            EditController& controller;
            PresetData beforeState, afterState;
        };
    }

    PresetData EditController::getPresetData() const
    {
        PresetData d;
        for (int b = 0; b < numBands; ++b)
            d.bands[(size_t) b] = getSnapshot (b);
        d.outputGain = processor.getOutputGainParam()->convertFrom0to1 (processor.getOutputGainParam()->getValue());
        d.gainScale  = processor.getGainScaleParam()->convertFrom0to1 (processor.getGainScaleParam()->getValue());
        return d;
    }

    void EditController::applyPresetData (const PresetData& d)
    {
        for (int b = 0; b < numBands; ++b)
            applySnapshot (b, d.bands[(size_t) b]);

        auto setGlobal = [] (juce::RangedAudioParameter* p, float value)
        {
            if (p->convertFrom0to1 (p->getValue()) == p->convertFrom0to1 (p->convertTo0to1 (value)))
                return;
            p->beginChangeGesture();
            setParamValue (p, value);
            p->endChangeGesture();
        };

        setGlobal (processor.getOutputGainParam(), d.outputGain);
        setGlobal (processor.getGainScaleParam(), d.gainScale);
    }

    void EditController::loadPreset (const PresetData& d, const juce::String& name)
    {
        if (isEditing())
            endEdit();

        const auto before = getPresetData();
        const juce::String title = "Load preset " + name;
        undoManager.beginNewTransaction (title);
        undoManager.perform (new PresetAction (*this, before, d), title);
    }

    BandSnapshot EditController::getSnapshot (int band) const
    {
        BandSnapshot s;
        s.used    = processor.isBandUsed (band);
        s.enabled = processor.getBandValue (band, Param::Enabled) >= 0.5f;
        s.type    = (int) std::lround (processor.getBandValue (band, Param::Type));
        s.freq    = processor.getBandValue (band, Param::Freq);
        s.gain    = processor.getBandValue (band, Param::Gain);
        s.q       = processor.getBandValue (band, Param::Q);
        s.slope   = (int) std::lround (processor.getBandValue (band, Param::Slope));
        s.stereo  = (int) std::lround (processor.getBandValue (band, Param::Stereo));
        s.dynOn       = processor.getBandValue (band, Param::DynOn) >= 0.5f;
        s.dynExternal = processor.getBandValue (band, Param::DynExternal) >= 0.5f;
        s.threshold   = processor.getBandValue (band, Param::Threshold);
        s.range       = processor.getBandValue (band, Param::Range);
        s.attack      = processor.getBandValue (band, Param::Attack);
        s.release     = processor.getBandValue (band, Param::Release);
        return s;
    }

    void EditController::setParamWithGesture (int band, Param p, float value)
    {
        auto* param = processor.getBandParam (band, p);
        const float current = param->convertFrom0to1 (param->getValue());
        if (current == param->convertFrom0to1 (param->convertTo0to1 (value)))
            return;

        param->beginChangeGesture();
        setParamValue (param, value);
        param->endChangeGesture();
    }

    void EditController::applySnapshot (int band, const BandSnapshot& s)
    {
        setParamWithGesture (band, Param::Enabled, s.enabled ? 1.0f : 0.0f);
        setParamWithGesture (band, Param::Type, (float) s.type);
        setParamWithGesture (band, Param::Slope, (float) s.slope);
        setParamWithGesture (band, Param::Stereo, (float) s.stereo);
        setParamWithGesture (band, Param::Threshold, s.threshold);
        setParamWithGesture (band, Param::Range, s.range);
        setParamWithGesture (band, Param::Attack, s.attack);
        setParamWithGesture (band, Param::Release, s.release);
        setParamWithGesture (band, Param::DynExternal, s.dynExternal ? 1.0f : 0.0f);
        setParamWithGesture (band, Param::DynOn, s.dynOn ? 1.0f : 0.0f);
        setParamWithGesture (band, Param::Freq, s.freq);
        setParamWithGesture (band, Param::Gain, s.gain);
        setParamWithGesture (band, Param::Q, s.q);

        if (processor.isBandUsed (band) != s.used)
        {
            // Adding/removing a band changes no host parameter, so tell the host
            // explicitly; otherwise deleting a band would not mark the project dirty.
            processor.setBandUsed (band, s.used);
            processor.notifyNonParameterStateChanged();
        }
    }

    void EditController::commit (int band, const BandSnapshot& before, const BandSnapshot& after, const juce::String& name)
    {
        if (before == after)
            return;

        undoManager.beginNewTransaction (name);
        undoManager.perform (new BandAction (*this, band, before, after), name);
    }

    //==========================================================================
    void EditController::beginEdit (int band, const juce::String& name)
    {
        if (isEditing())
            endEdit();

        editBand = band;
        editName = name;
        editBefore = getSnapshot (band);
        gestureOpen.fill (false);
    }

    void EditController::setParam (Param p, float value)
    {
        if (! isEditing())
            return;

        auto* param = processor.getBandParam (editBand, p);

        if (! gestureOpen[(size_t) p])
        {
            param->beginChangeGesture();
            gestureOpen[(size_t) p] = true;
        }

        setParamValue (param, value);
    }

    void EditController::endEdit()
    {
        if (! isEditing())
            return;

        for (int p = 0; p < numBandParams; ++p)
            if (gestureOpen[(size_t) p])
                processor.getBandParam (editBand, (Param) p)->endChangeGesture();

        gestureOpen.fill (false);

        const int band = editBand;
        editBand = -1;
        commit (band, editBefore, getSnapshot (band), editName);
    }

    //==========================================================================
    int EditController::addBand (float freq, float gain, FilterType type)
    {
        const int band = processor.findFreeBand();
        if (band < 0)
            return -1;

        const auto before = getSnapshot (band);
        BandSnapshot after;
        after.used = true;
        after.enabled = true;
        after.type = (int) type;
        after.freq = juce::jlimit (minFreq, maxFreq, freq);
        after.gain = typeHasGain (type) ? juce::jlimit (minGain, maxGain, gain) : 0.0f;
        after.q = defaultQ;

        undoManager.beginNewTransaction ("Add band");
        undoManager.perform (new BandAction (*this, band, before, after), "Add band");
        return band;
    }

    void EditController::removeBand (int band)
    {
        auto after = getSnapshot (band);
        const auto before = after;
        after.used = false;
        commit (band, before, after, "Remove band");
    }

    void EditController::resetBand (int band)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.gain = 0.0f;
        after.q = defaultQ;
        after.enabled = true;
        commit (band, before, after, "Reset band");
    }

    void EditController::setType (int band, FilterType type)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.type = (int) type;
        commit (band, before, after, "Change filter type");
    }

    void EditController::setSlope (int band, int slopeIndex)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.slope = juce::jlimit (0, numSlopes - 1, slopeIndex);
        commit (band, before, after, "Change slope");
    }

    void EditController::setStereoMode (int band, int mode)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.stereo = juce::jlimit (0, numStereoModes - 1, mode);
        commit (band, before, after, "Change stereo mode");
    }

    void EditController::setDynamic (int band, bool on)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.dynOn = on;
        commit (band, before, after, on ? "Dynamic on" : "Dynamic off");
    }

    void EditController::setEnabled (int band, bool enabled)
    {
        const auto before = getSnapshot (band);
        auto after = before;
        after.enabled = enabled;
        commit (band, before, after, enabled ? "Enable band" : "Bypass band");
    }

    void EditController::setValue (int band, Param p, float value)
    {
        const auto before = getSnapshot (band);
        setParamWithGesture (band, p, value);
        commit (band, before, getSnapshot (band), "Edit band");
    }

    bool EditController::undo()
    {
        if (isEditing())
            endEdit();
        return undoManager.undo();
    }

    bool EditController::redo()
    {
        if (isEditing())
            endEdit();
        return undoManager.redo();
    }
}
