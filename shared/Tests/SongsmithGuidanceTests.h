#pragma once

// Shared checks for the optional guidance (ADV-01), compiled into every plugin's test executable:
//   static SongsmithGuidanceTests sharedGuidanceTests { "EQ" };   // once per executable

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/SongsmithGuidance.h"
#include "../Source/SongsmithWording.h"

namespace Songsmith::Guidance::Testing
{
    // Points the machine setting at a temporary file for as long as it lives.
    struct TemporarySettings
    {
        TemporarySettings() : previous (Settings::fileOverrideForTests()),
                              folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                          .getChildFile ("songsmith-guidance-test-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt())))
        {
            folder.createDirectory();
            Settings::fileOverrideForTests() = folder.getChildFile ("Guidance.xml");
        }
        ~TemporarySettings() { Settings::fileOverrideForTests() = previous; folder.deleteRecursively(); }
        juce::File previous, folder;
    };

    inline juce::String everything (const Message& m)
    {
        return m.measured + "\n" + m.whatItDoes + "\n" + m.listenFor + "\n" + m.tryThis;
    }
}

class SongsmithGuidanceTests final : public juce::UnitTest
{
public:
    explicit SongsmithGuidanceTests (const juce::String& category) : juce::UnitTest ("Shared Songsmith Guidance", category) {}

    void runTest() override
    {
        using namespace Songsmith::Guidance;

        // Every message that can be produced, so the wording and claims checks cover all of them.
        juce::Array<Message> all;

        beginTest ("Silence and missing evidence give no advice");
        {
            const Message eq = eqMessage ({});
            const Message comp = compressorMessage ({});
            const Message max = maximiserMessage ({});
            for (const auto& m : { eq, comp, max })
            {
                expect (! m.hasAdvice, "no evidence, no advice");
                expect (m.tryThis.isEmpty(), "no next action without evidence");
                expect (m.measured.isEmpty(), "no measured facts without evidence");
                all.add (m);
            }

            CompressorInput bypassed;
            bypassed.hasSignal = true;
            bypassed.bypassed = true;
            bypassed.gainReductionDb = 6.0f;
            expect (! compressorMessage (bypassed).hasAdvice, "a bypassed compressor is not changing the sound");
        }

        beginTest ("Equaliser guidance follows the chosen band");
        {
            EqInput in;
            in.hasSelectedBand = true;
            in.frequencyHz = 250.0f;
            in.regionName = "Low Mids";
            in.regionPurpose = "Adds body.";

            in.gainDb = 0.0f;
            auto flat = eqMessage (in);
            expect (flat.hasAdvice);
            expect (flat.tryThis.containsIgnoreCase ("alone"), "a flat band says leaving it alone is fine");
            expect (flat.measured.contains ("250 Hz"), "the measured band is shown as a fact");

            in.gainDb = 4.0f;
            auto boost = eqMessage (in);
            in.gainDb = -4.0f;
            auto cut = eqMessage (in);
            expect (boost.tryThis != cut.tryThis, "boost and cut get different suggestions");
            expect (boost.whatItDoes.startsWith ("Low Mids"), "uses the Frequency Guide's region name");
            expect (boost.listenFor.containsIgnoreCase ("whole mix"), "listen with the whole mix playing");
            all.add (flat); all.add (boost); all.add (cut);
        }

        beginTest ("Compressor guidance uses the measured reduction and actual ratio");
        {
            CompressorInput in;
            in.hasSignal = true;
            in.ratio = 8.0f;

            in.gainReductionDb = 0.1f;
            auto light = compressorMessage (in);
            expect (light.hasAdvice);
            expect (light.tryThis.containsIgnoreCase ("do not need to do anything"), "light reduction: nothing to do");

            in.gainReductionDb = 5.0f;
            auto working = compressorMessage (in);
            expect (working.measured.contains ("5.0 dB"), "the measured reduction is shown");
            expect (working.measured.contains ("8:1"), "the actual ratio is shown");
            expect (working.listenFor.containsIgnoreCase ("same loudness"), "compare at the same loudness");
            expect (working.tryThis.containsIgnoreCase ("attack"), "the attack suggestion");

            in.allButtons = true;
            expect (compressorMessage (in).measured.contains ("All Buttons"), "all four buttons in is reported as it is");
            all.add (light); all.add (working);
        }

        beginTest ("Maximiser guidance uses measured values and never a loudness target");
        {
            MaximiserInput in;
            in.hasSignal = true;
            in.ceilingDb = -1.0f;
            in.gainReductionDb = 6.0f;
            auto withoutLoudness = maximiserMessage (in);
            expect (! withoutLoudness.measured.contains ("LUFS"), "no loudness reading, none shown");
            expect (withoutLoudness.measured.contains ("-1.0 dB"), "the actual ceiling is shown");

            in.shortTermLufs = -14.2f;
            auto withLoudness = maximiserMessage (in);
            expect (withLoudness.measured.contains ("-14.2 LUFS"), "a validated reading is shown as measured");
            expect (withLoudness.listenFor.containsIgnoreCase ("same volume"), "compare at the same volume");

            in.gainReductionDb = 0.0f;
            auto idle = maximiserMessage (in);
            expect (idle.tryThis.containsIgnoreCase ("do not need to do anything"), "an idle limiter needs nothing");
            all.add (withoutLoudness); all.add (withLoudness); all.add (idle);
        }

        beginTest ("Every message is plain, answers the three questions and makes no forbidden claim");
        {
            const juce::StringArray forbidden { "mud", "streaming", "spotify", "apple music", "youtube", "must", "required", "should be",
                                                "target", "ideal", "perfect", "bit depth", "24-bit", "16-bit", "sample rate", "fix" };
            for (const auto& m : all)
            {
                const auto text = Testing::everything (m);
                expect (text.isNotEmpty());
                expect (m.whatItDoes.isNotEmpty(), "says what matters: " + m.whatItDoes);
                expect (m.listenFor.isNotEmpty(), "says what to listen for: " + m.listenFor);
                if (m.hasAdvice)
                    expect (m.tryThis.isNotEmpty(), "says whether anything needs doing: " + m.whatItDoes);

                const auto bad = Songsmith::UI::Wording::findBanned (text);
                expect (bad.isEmpty(), "plain words: \"" + bad + "\" in \"" + text + "\"");

                for (const auto& word : forbidden)
                    expect (! text.containsIgnoreCase (word), "forbidden claim \"" + word + "\" in \"" + text + "\"");

                // One or two sentences per part, not an essay.
                for (const auto& part : { m.whatItDoes, m.listenFor, m.tryThis })
                    expect (part.length() <= 260, "concise: " + part);

                // British spelling.
                for (const auto& american : { "color", "equalizer", "loudness meter", "gray" })
                    expect (! text.containsIgnoreCase (american), juce::String ("British spelling: ") + american);
            }
        }

        beginTest ("The preference defaults to on, is saved and restored, and is not part of any session");
        {
            Testing::TemporarySettings settings;
            expect (Settings::loadEnabled ("Test"), "on by default");
            Settings::saveEnabled ("Test", false);
            expect (! Settings::loadEnabled ("Test"), "saved as off");
            Settings::saveEnabled ("Test", true);
            expect (Settings::loadEnabled ("Test"), "saved as on");
            expect (Settings::file ("Test").getFileName() == "Guidance.xml", "its own file, never a parameter or preset");
        }

        beginTest ("The card shows the three headings, the switch turns guidance off and on, and nothing opens by itself");
        {
            Testing::TemporarySettings settings;
            CompressorInput in;
            in.hasSignal = true;
            in.gainReductionDb = 4.0f;
            const auto message = compressorMessage (in);

            {
                Card card (message, "Test");
                const auto text = card.getAllText();
                expect (text.contains (whatItDoesHeading()) && text.contains (listenForHeading()) && text.contains (tryThisHeading()),
                        "the three headings");
                expect (text.contains ("(optional)"), "the next action is marked optional");
                expect (text.contains ("Measured:"), "measured facts are labelled as measured");
                expect (card.getToggle().getButtonText() == "Turn Guidance Off");
                expect (card.getHeight() > 0 && card.getWidth() == Card::width);
                for (auto* c : card.getChildren())
                    if (c->isVisible())
                        expect (card.getLocalBounds().contains (c->getBounds()), "inside the card");
                card.getToggle().onClick();
            }
            expect (! Settings::loadEnabled ("Test"), "the switch turned it off");

            {
                Card card (message, "Test");
                expect (card.getAllText().contains ("Guidance is off."));
                expect (! card.getAllText().contains (tryThisHeading()), "no advice while off");
                expect (card.getToggle().getButtonText() == "Turn Guidance On");
                card.getToggle().onClick();
            }
            expect (Settings::loadEnabled ("Test"), "the switch turned it on again");

            bool built = false;
            Button button ("Test", [&] { built = true; return message; });
            expect (! built, "building the button asks for no message and shows nothing");
            expect (button.getLastCardText().isEmpty());
            expect (button.getButtonText() == "Guidance");
            expect (Songsmith::UI::Wording::findBanned (button.getTooltip()).isEmpty());
        }

        beginTest ("Building guidance does not touch parameters or audio");
        {
            // The functions take plain values and return text. They hold no processor, parameter or buffer,
            // so asking for guidance cannot change automation or sound. Same input, same output:
            CompressorInput in;
            in.hasSignal = true;
            in.gainReductionDb = 3.0f;
            const auto a = Testing::everything (compressorMessage (in));
            const auto b = Testing::everything (compressorMessage (in));
            expectEquals (a, b);
        }
    }
};
