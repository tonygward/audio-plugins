#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "PluginProcessor.h"

// Compact floating editor for the selected band. Lives inside EQDisplay and
// follows the selected node. All edits go through EQ::EditController so they
// are automatable and undoable.
class BandPanel : public juce::Component, private juce::Timer
{
public:
    explicit BandPanel (ThreeBandEQAudioProcessor&);
    ~BandPanel() override;

    void setBand (int band);
    int getBand() const noexcept { return band; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int panelWidth  = 470;
    static constexpr int panelHeight = 128;

    std::function<void()> onBandRemoved;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        EQ::Param param = EQ::Param::Freq;
        bool dragging = false;
    };

    void setupKnob (Knob&, const juce::String& name, EQ::Param);
    void refreshFromParams();
    void timerCallback() override { refreshFromParams(); }

    ThreeBandEQAudioProcessor& processor;
    int band = -1;
    bool updating = false;

    juce::Label title;
    juce::TextButton powerButton { "ON" };
    juce::ComboBox typeBox, slopeBox, stereoBox;
    Knob freqKnob, gainKnob, qKnob;
    juce::TextButton resetButton { "Reset" }, deleteButton { "Delete" }, soloButton { "Solo" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
