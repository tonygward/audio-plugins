#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SongsmithColours.h"
#include "SongsmithMeterColumn.h"
#include "SongsmithType.h"

// songsmithstudio#16: the one Output panel at the far right of every Songsmith plugin that changes the
// sound. One visually bounded box, headed "Output", holding (top to bottom) the meters (the shared
// MeterColumn: Gain Reduction where the plugin turns the level down, then the Output meter), a compact
// Output Trim knob, and the Auto Gain button immediately beside it.
//
// The panel owns the controls and the meters; the plugin attaches its own parameters to getTrim() and
// getAutoGain() (a SliderAttachment and a ButtonAttachment), so existing parameter IDs and sessions are
// untouched. A plugin with no Auto Gain yet calls setAutoGainAvailable (false) and the button is shown
// switched off with a tooltip saying so, never hidden, so the layout is the same suite-wide.
namespace Songsmith::UI
{
    class OutputPanel : public juce::Component
    {
    public:
        // Layout, in design units.
        static constexpr int margin = MeterColumn::margin;   // from the editor's right edge
        static constexpr int padding = 10;                   // inside the panel
        static constexpr int headerHeight = 24;
        static constexpr int trimSize = 56;                  // the knob, about Analog Desk's compact Range and Threshold knobs
        static constexpr int trimRowHeight = 92;             // knob, its value and its name
        static constexpr int autoGainWidth = 86, autoGainHeight = 26;
        static constexpr int rowGap = 8;
        static constexpr int contentGap = MeterColumn::contentGap;

        static constexpr int innerWidthFor (bool withGainReduction) noexcept
        {
            return juce::jmax (MeterColumn::widthFor (withGainReduction), trimSize + 16 + autoGainWidth);
        }

        static constexpr int widthFor (bool withGainReduction) noexcept
        {
            return innerWidthFor (withGainReduction) + 2 * padding;
        }

        // Where to put the panel: the right edge, from top to bottom.
        static juce::Rectangle<int> boundsFor (int designWidth, int top, int bottom, bool withGainReduction) noexcept
        {
            const int w = widthFor (withGainReduction);
            return { designWidth - margin - w, top, w, bottom - top };
        }

        // The plugin's own controls end here.
        static int contentRightEdge (int designWidth, bool withGainReduction) noexcept
        {
            return designWidth - margin - widthFor (withGainReduction) - contentGap;
        }

        explicit OutputPanel (bool withGainReduction) : meters (withGainReduction)
        {
            setTitle ("Output");
            addAndMakeVisible (meters);

            trim.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            trim.setTextBoxStyle (juce::Slider::TextBoxBelow, false, trimSize + 16, 18);
            trim.setTitle ("Output Trim");
            trim.setTooltip ("Output Trim: the last level change before the sound leaves the plugin, after Auto Gain. "
                             "Zero leaves the level as it is. Nothing to do unless the plugin is too loud or too quiet.");
            trim.setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (trim);

            trimName.setText ("Output Trim", juce::dontSendNotification);
            trimName.setFont (juce::Font (font (Type::control, Weight::Medium)));
            trimName.setColour (juce::Label::textColourId, Colours::textDim);
            trimName.setJustificationType (juce::Justification::centred);
            trimName.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (trimName);

            autoGain.setButtonText ("Auto Gain");
            autoGain.setTitle ("Auto Gain");
            autoGain.setClickingTogglesState (true);
            setAutoGainTooltip();
            addAndMakeVisible (autoGain);
        }

        bool hasReduction() const noexcept { return meters.hasReduction(); }
        MeterColumn& getMeters() noexcept { return meters; }
        juce::Slider& getTrim() noexcept { return trim; }
        juce::Label& getTrimName() noexcept { return trimName; }
        juce::TextButton& getAutoGain() noexcept { return autoGain; }

        // Auto Gain is off and cannot be switched on until the plugin has the processing for it.
        void setAutoGainAvailable (bool available)
        {
            autoGainAvailable = available;
            autoGain.setEnabled (available);
            setAutoGainTooltip();
        }

        // What Auto Gain does for this plugin, in one or two plain sentences (sentence case).
        void setAutoGainExplanation (const juce::String& sentence)
        {
            autoGainExplanation = sentence;
            setAutoGainTooltip();
        }

        // The panel's rectangles, so tests can check nothing overlaps and the order is right.
        juce::Rectangle<int> getMetersBounds() const { return meters.getBounds(); }
        juce::Rectangle<int> getTrimBounds() const   { return trim.getBounds().getUnion (trimName.getBounds()); }
        juce::Rectangle<int> getAutoGainBounds() const { return autoGain.getBounds(); }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            g.setGradientFill (juce::ColourGradient (Colours::panelLight.withAlpha (0.55f), 0.0f, r.getY(),
                                                     Colours::panel.withAlpha (0.55f), 0.0f, r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Colours::edge);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);

            g.setColour (Colours::text);
            g.setFont (juce::Font (font (Type::heading, Weight::SemiBold)));
            g.drawText ("Output", getLocalBounds().withHeight (headerHeight).reduced (padding, 0),
                        juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (padding, 0);
            area.removeFromTop (headerHeight);
            area.removeFromBottom (padding);

            auto lower = area.removeFromBottom (trimRowHeight);
            area.removeFromBottom (rowGap);
            meters.setBounds (area.withTrimmedLeft (area.getWidth() - MeterColumn::widthFor (meters.hasReduction())));

            // The trim knob at the left of the row, Auto Gain immediately beside it.
            auto knobArea = lower.removeFromLeft (trimSize + 16);
            trim.setBounds (knobArea.removeFromTop (trimRowHeight - 18));
            trimName.setBounds (knobArea.withHeight (18));
            lower.removeFromLeft (rowGap - 8);
            autoGain.setBounds (lower.withSizeKeepingCentre (juce::jmin (lower.getWidth(), autoGainWidth), autoGainHeight)
                                     .withY (trim.getY() + (trimSize - autoGainHeight) / 2));
        }

    private:
        void setAutoGainTooltip()
        {
            juce::String tip = "Auto Gain: keeps the loudness the same with the plugin on as with it off, "
                               "so you can judge the sound rather than the volume. ";
            if (autoGainExplanation.isNotEmpty())
                tip += autoGainExplanation + " ";
            tip += autoGainAvailable ? "Output Trim is applied after it, so your own setting always has the last word. Off by default."
                                     : "Not available in this plugin yet.";
            autoGain.setTooltip (tip);
        }

        MeterColumn meters;
        juce::Slider trim;
        juce::Label trimName;
        juce::TextButton autoGain;
        bool autoGainAvailable = true;
        juce::String autoGainExplanation;
    };
}
