#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <optional>
#include "SongsmithColours.h"
#include "SongsmithLookAndFeel.h"
#include "SongsmithType.h"

// ADV-01: optional, calm guidance that answers three questions in plain English:
// what matters, why it matters, and whether the musician needs to do anything.
//
// Rules (CLAUDE.md principle 7):
//   - Guidance only appears when the musician asks (the Guidance button). It never pops up by itself,
//     never repeats, and never changes a setting.
//   - Measured facts (gain reduction, ceiling, level) are kept apart from listening suggestions.
//   - With silence or too little evidence it says so and offers no advice.
//   - Nothing here runs on the audio thread. The editors read values their meters already publish.
//   - The on/off choice is a machine setting in its own file (Guidance.xml), like Diagnostics.xml.
//     It is not a parameter and is never saved with sessions or presets, so no stateVersion change.
namespace Songsmith::Guidance
{
    namespace UI = Songsmith::UI;
    namespace Colours = Songsmith::UI::Colours;

    // What the three headings are called on screen.
    inline const char* whatItDoesHeading()  { return "What It Does"; }
    inline const char* listenForHeading()   { return "What To Listen For"; }
    inline const char* tryThisHeading()     { return "Try This"; }

    struct Message
    {
        juce::String measured;      // facts the plugin measured (empty when there are none)
        juce::String whatItDoes;    // what matters and why
        juce::String listenFor;     // a listening suggestion, never a measurement
        juce::String tryThis;       // optional next action; empty means "nothing to do"
        bool hasAdvice = true;      // false for silence or too little evidence
    };

    inline Message noEvidence (const juce::String& what)
    {
        Message m;
        m.hasAdvice = false;
        m.whatItDoes = what;
        m.listenFor = "Play some audio through the plugin and open Guidance again. Until then there is nothing to say.";
        return m;
    }

    //==============================================================================
    // Songsmith EQ. The frequency region comes from the Frequency Guide; a spectrum alone cannot prove
    // that something is muddy, so the wording stays a listening suggestion.
    struct EqInput
    {
        bool hasSelectedBand = false;
        float frequencyHz = 0.0f;
        float gainDb = 0.0f;
        juce::String regionName;      // from the Frequency Guide, for example "Low Mids"; empty if unknown
        juce::String regionPurpose;   // one plain sentence from the Frequency Guide; empty if unknown
    };

    inline Message eqMessage (const EqInput& in)
    {
        if (! in.hasSelectedBand)
            return noEvidence ("Select a band to hear what that part of the sound can do.");

        Message m;
        const juce::String where = in.regionName.isNotEmpty() ? in.regionName : juce::String ("This part of the sound");
        m.measured = "Selected band: " + juce::String (in.frequencyHz, 0) + " Hz, "
                     + (in.gainDb >= 0.0f ? "+" : "") + juce::String (in.gainDb, 1) + " dB.";
        m.whatItDoes = in.regionPurpose.isNotEmpty()
                         ? where + ": " + in.regionPurpose
                         : where + " can change how full, clear or bright the sound feels.";
        m.listenFor = "Listen with the whole mix playing, not just this one sound. Something that seems too boomy on its own may sit well in the mix.";
        if (std::abs (in.gainDb) < 0.1f)
            m.tryThis = "This band is not changing the sound yet. Leaving it alone is often fine.";
        else if (in.gainDb > 0.0f)
            m.tryThis = "If it sounds too much, try a smaller boost. A small change is usually enough.";
        else
            m.tryThis = "If you hear the sound getting thin, try a smaller cut. A small change is usually enough.";
        return m;
    }

    //==============================================================================
    // Songsmith Analog Compressor. The gain reduction is measured; the advice about attack is a listening
    // suggestion. No universal gain reduction target is ever given.
    struct CompressorInput
    {
        bool hasSignal = false;       // audio has played through recently
        float gainReductionDb = 0.0f; // measured, positive dB (the largest recent reduction)
        float ratio = 4.0f;           // the ratio actually selected (4, 8, 12, 20)
        bool allButtons = false;      // all four ratio buttons pushed in
        bool bypassed = false;
    };

    inline Message compressorMessage (const CompressorInput& in)
    {
        if (in.bypassed)
            return noEvidence ("Bypass is on, so the compressor is not changing the sound.");
        if (! in.hasSignal)
            return noEvidence ("The compressor turns down the louder peaks so the sound feels steadier.");

        Message m;
        const juce::String ratioText = in.allButtons ? juce::String ("All Buttons") : (juce::String ((int) in.ratio) + ":1");
        m.measured = "Gain Reduction: " + juce::String (in.gainReductionDb, 1) + " dB. Ratio: " + ratioText + ".";

        if (in.gainReductionDb < 0.5f)
        {
            m.whatItDoes = "The compressor is hardly turning anything down at the moment, so it is barely changing the sound.";
            m.listenFor = "Switch it off and on at the same loudness. If you cannot hear a difference, it is not doing much.";
            m.tryThis = "You do not need to do anything. Lower the Input only if you want more compression.";
        }
        else
        {
            m.whatItDoes = "This is controlling the louder peaks, which can make the sound steadier.";
            m.listenFor = "Compare with Bypass at the same loudness, because louder often sounds better. Listen for lost punch or a dull, squashed sound.";
            m.tryThis = "If the sound loses its punch, try a slower attack. If it sounds fine, leave it alone.";
        }
        return m;
    }

    //==============================================================================
    // Songsmith Maximiser. Reduction, ceiling and loudness are measured. No streaming service has one
    // required loudness number, and the text never says so.
    struct MaximiserInput
    {
        bool hasSignal = false;
        float gainReductionDb = 0.0f;   // measured, positive dB
        float ceilingDb = -0.3f;        // the ceiling actually set
        std::optional<float> shortTermLufs;   // only when a validated reading exists
    };

    inline Message maximiserMessage (const MaximiserInput& in)
    {
        if (! in.hasSignal)
            return noEvidence ("The Maximiser makes a track louder by limiting its peaks.");

        Message m;
        m.measured = "Gain Reduction: " + juce::String (in.gainReductionDb, 1) + " dB. Output Ceiling: " + juce::String (in.ceilingDb, 1) + " dB.";
        if (in.shortTermLufs.has_value())
            m.measured += " Short Term loudness: " + juce::String (*in.shortTermLufs, 1) + " LUFS.";

        if (in.gainReductionDb < 0.5f)
        {
            m.whatItDoes = "The limiter is hardly working, so the track is not being made much louder.";
            m.listenFor = "Nothing needs changing. Playing the loudest part of the track shows what the limiter does.";
            m.tryThis = "You do not need to do anything. Lower the Threshold only if you want it louder.";
        }
        else
        {
            m.whatItDoes = "This will make your track louder, but too much limiting may reduce punch.";
            m.listenFor = "Compare with Bypass at the same volume, because the louder version can seem better only because it is louder.";
            m.tryThis = "If drums or other hits feel flat, try raising the Threshold a little. If it sounds good, leave it alone.";
        }
        return m;
    }

    //==============================================================================
    // The on/off choice, in its own machine setting file. On by default.
    namespace Settings
    {
        inline juce::File& fileOverrideForTests() { static juce::File f; return f; }

        inline juce::File file (const juce::String& pluginName)
        {
            if (fileOverrideForTests() != juce::File())
                return fileOverrideForTests();
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Songsmith Studio").getChildFile (pluginName).getChildFile ("Guidance.xml");
        }

        inline bool loadEnabled (const juce::String& pluginName)
        {
            const auto f = file (pluginName);
            if (! f.existsAsFile())
                return true;   // on by default: it only ever appears when asked for
            juce::PropertiesFile settings (f, {});
            return settings.getBoolValue ("guidance", true);
        }

        inline void saveEnabled (const juce::String& pluginName, bool on)
        {
            const auto f = file (pluginName);
            f.getParentDirectory().createDirectory();
            juce::PropertiesFile::Options options;
            options.millisecondsBeforeSaving = 0;
            juce::PropertiesFile settings (f, options);
            settings.setValue ("guidance", on);
            settings.saveIfNeeded();
        }
    }

    //==============================================================================
    // The content of the callout: the three headings, the measured facts kept separate, and a switch.
    class Card : public juce::Component
    {
    public:
        static constexpr int width = 340;

        Card (const Message& message, const juce::String& pluginName) : name (pluginName), shown (message)
        {
            const bool on = Settings::loadEnabled (name);
            setupText (measuredLabel, on && shown.measured.isNotEmpty() ? "Measured: " + shown.measured : juce::String(), Colours::text);
            setupText (whatLabel, on ? section (whatItDoesHeading(), shown.whatItDoes) : "Guidance is off.", Colours::text);
            setupText (listenLabel, on ? section (listenForHeading(), shown.listenFor) : "Turn it on again whenever you would like a hint.", Colours::textDim);
            setupText (tryLabel, on && shown.tryThis.isNotEmpty() ? section (tryThisHeading(), shown.tryThis + " (optional)") : juce::String(), Colours::textDim);

            toggle.setButtonText (on ? "Turn Guidance Off" : "Turn Guidance On");
            toggle.setTitle (toggle.getButtonText());
            toggle.setTooltip ("Guidance only appears when you press the Guidance button. It never changes your settings.");
            toggle.onClick = [this]
            {
                Settings::saveEnabled (name, ! Settings::loadEnabled (name));
                if (onChanged) onChanged();
            };
            addAndMakeVisible (toggle);
            setSize (width, computeHeight());
        }

        std::function<void()> onChanged;

        juce::String getAllText() const
        {
            return (measuredLabel.getText() + "\n" + whatLabel.getText() + "\n" + listenLabel.getText() + "\n" + tryLabel.getText()
                    + "\n" + toggle.getButtonText()).trim();
        }
        juce::TextButton& getToggle() { return toggle; }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 10);
            for (auto* l : { &measuredLabel, &whatLabel, &listenLabel, &tryLabel })
            {
                if (l->getText().isEmpty())
                    continue;
                l->setBounds (r.removeFromTop (heightOf (*l, r.getWidth())));
                r.removeFromTop (6);
            }
            toggle.setBounds (r.removeFromBottom (24).removeFromLeft (170));
        }

        void paint (juce::Graphics& g) override { g.fillAll (Colours::panel); }

    private:
        static juce::String section (const juce::String& heading, const juce::String& text) { return heading + ": " + text; }

        void setupText (juce::Label& l, const juce::String& text, juce::Colour colour)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (juce::Font (UI::font (UI::Type::control, UI::Weight::Regular)));
            l.setColour (juce::Label::textColourId, colour);
            l.setJustificationType (juce::Justification::topLeft);
            l.setMinimumHorizontalScale (1.0f);
            l.setBorderSize ({ 0, 0, 0, 0 });
            addChildComponent (l);
            l.setVisible (text.isNotEmpty());
        }

        int heightOf (juce::Label& l, int w) const
        {
            juce::AttributedString s;
            s.append (l.getText(), l.getFont());
            juce::TextLayout layout;
            layout.createLayout (s, (float) w);
            return (int) std::ceil (layout.getHeight()) + 2;
        }

        int computeHeight()
        {
            int h = 20 + 6 + 24;   // margins, gap, switch
            for (auto* l : { &measuredLabel, &whatLabel, &listenLabel, &tryLabel })
                if (l->getText().isNotEmpty())
                    h += heightOf (*l, width - 24) + 6;
            return h;
        }

        juce::String name;
        Message shown;
        juce::Label measuredLabel, whatLabel, listenLabel, tryLabel;
        juce::TextButton toggle;
    };

    // The button the editors place beside what the advice is about. Pressing it opens the card; nothing
    // opens by itself.
    class Button : public juce::TextButton
    {
    public:
        Button (const juce::String& pluginName, std::function<Message()> messageSource)
            : juce::TextButton ("Guidance"), name (pluginName), source (std::move (messageSource))
        {
            setTitle ("Guidance");
            setTooltip ("Optional plain-language help about what you are hearing and whether you need to change anything.");
            onClick = [this] { open(); };
        }

        void open()
        {
            auto card = std::make_unique<Card> (source(), name);
            auto* raw = card.get();
            card->onChanged = [raw] { if (auto* box = raw->findParentComponentOfClass<juce::CallOutBox>()) box->dismiss(); };
            lastCardText = card->getAllText();
            juce::CallOutBox::launchAsynchronously (std::move (card), getScreenBounds(), nullptr);
        }

        const juce::String& getLastCardText() const { return lastCardText; }

    private:
        juce::String name;
        std::function<Message()> source;
        juce::String lastCardText;
    };
}
