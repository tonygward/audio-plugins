#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// SUI-17: the plugin title explains the plugin.
//
// This replaces the ADV-01 Guidance button, card, messages and Guidance.xml setting. Hovering over the
// plugin name shows one short, fixed explanation. It never depends on a meter, so it cannot claim
// anything was measured. Live numbers stay in their own meters and readouts.
//
// The title itself is painted (Songsmith::UI::drawTitle), so a transparent TitleHint sits exactly over
// the painted text. Blank space beside the title is not part of it and shows no tooltip.
namespace Songsmith::Guidance
{
    // Fixed wording: what the plugin does, then one listening suggestion. No measurements, no targets.
    inline juce::String titleTooltipFor (const juce::String& pluginName)
    {
        if (pluginName.containsIgnoreCase ("Analog Compressor"))
            return "Evens out louder and quieter parts and can add analogue-style character. "
                   "Listen for lost punch if you compress too hard.";
        if (pluginName.containsIgnoreCase ("Compressor"))   // after the Analog Compressor above
            return "Evens out loud and quiet parts with plain controls. "
                   "Make small changes and listen for lost punch.";
        if (pluginName.containsIgnoreCase ("Maximiser"))
            return "Controls peaks and raises loudness. "
                   "Compare at the same listening level, so that louder does not automatically seem better.";
        if (pluginName.containsIgnoreCase ("EQ"))
            return "Shape the tone by boosting or reducing selected frequencies. "
                   "Make small changes while listening to the whole mix.";
        return {};
    }

    // Invisible hit target over the painted title. Gives the tooltip on hover and the same text to
    // screen readers and keyboard focus (as the accessible description). It does nothing on click.
    class TitleHint final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit TitleHint (const juce::String& pluginName)
        {
            const auto text = titleTooltipFor (pluginName);
            setTooltip (text);
            setTitle (pluginName);
            setDescription (text);
            setWantsKeyboardFocus (true);
            setMouseClickGrabsKeyboardFocus (false);
            setAccessible (true);
        }

        // Not tied to the paint: the title text is drawn by the editor, exactly beneath this.
        void paint (juce::Graphics&) override {}

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
        {
            return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::label);
        }
    };
}
