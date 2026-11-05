#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "PluginProcessor.h"
#include "FilterShapeIcons.h"

// Compact floating editor for the selected band. Lives inside EQDisplay and
// follows the selected node. All edits go through EQ::EditController so they
// are automatable and undoable.
class BandPanel : public juce::Component, private juce::Timer
{
public:
    explicit BandPanel (SongsmithEQAudioProcessor&);
    ~BandPanel() override;

    void setBand (int band);
    int getBand() const noexcept { return band; }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int panelWidth  = 640;
    static constexpr int panelHeight = 150;          // without the dynamics row
    static constexpr int dynamicsRowHeight = 104;
    static constexpr int knobTextWidth = 130, knobTextHeight = 20, knobLabelHeight = 18;
    static constexpr int leftColumnWidth = 150, rightColumnWidth = 92;
    int getPreferredHeight() const noexcept { return panelHeight + (showDynamics ? dynamicsRowHeight : 0); }

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

    SongsmithEQAudioProcessor& processor;
    int band = -1;
    bool updating = false;

    juce::Label title;
    juce::TextButton powerButton { "On" };
    juce::ComboBox typeBox, slopeBox, stereoBox;
    FilterShapeIcon typeIcon;   // decorative picture of the selected type, drawn over the type box
    Knob freqKnob, gainKnob, qKnob;
    Knob thresholdKnob, rangeKnob, attackKnob, releaseKnob;   // dynamics row (shown while dynamics are on)
    juce::TextButton dynButton { "Dynamic" }, dynExternalButton { "Side Chain" };
    juce::Label dynGainLabel;
    bool showDynamics = false;
    juce::TextButton resetButton { "Reset" }, deleteButton { "Delete" }, soloButton { "Solo" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
