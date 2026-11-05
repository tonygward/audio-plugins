#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The suite's wording rule (docs/style-guide.md, "Wording"): every word on screen is spelled out.
// One checker for every plugin, so the rule can't drift: each plugin's tests run it on its own
// editor's text, its parameter display names and its preset names.
namespace Songsmith::UI::Wording
{
    // Shorthand that is never allowed, as whole words (case-sensitive). The EQ's list from task 24,
    // the compressor's and maximiser's shorthand replaced in suite task S3, and common ones.
    inline const juce::StringArray& bannedWords()
    {
        static const juce::StringArray banned {
            "SC", "AG", "OS", "GR", "Dyn", "FREQ", "Freq", "THRESH", "Thresh", "Atten", "Attn", "Sidechain",
            "In", "Out", "TPDF", "CPU", "Byp", "Rel", "Att", "Lvl", "Vol", "Amt",
        };
        return banned;
    }

    // Short forms that are allowed: units, key names, format names, the Q control, and the
    // product name "EQ" (Songsmith EQ; generic text says "equaliser" or "band").
    inline bool isAllowedShortForm (const juce::String& word)
    {
        static const juce::StringArray allowed { "Q", "LUFS", "VST3", "AU", "EQ" };
        return allowed.contains (word);
    }

    // Returns the first word in 'text' that breaks the rule, or an empty string. Breaks it:
    //  - a banned word;
    //  - a standalone capital letter other than Q (except a key name after '+', as in Ctrl+Z, and
    //    the words "A" and "I" where a sentence starts);
    //  - a word in capitals ("OFF", "GR"), unless it's an allowed short form.
    // Allowed: units (Hz, kHz, dB, dBFS, dBTP, ms...), note names (A4, C#5: one word with the
    // digit), numbers with units (2x, 16-bit).
    inline juce::String findBanned (const juce::String& text)
    {
        // The one narrow exception (SUI-12): the Processing Load readout starts "CPU 0.35%, Latency ...".
        // "CPU" is allowed there and nowhere else.
        if (text.startsWith ("CPU ") && text.contains (", Latency "))
            return findBanned (text.substring (3));

        juce::String word;
        juce::juce_wchar before = 0, previous = 0;
        bool sentenceStart = true, wordStartsSentence = true;   // at the start, or after . : ! ? (

        auto check = [&]() -> juce::String
        {
            if (word.isEmpty() || isAllowedShortForm (word))
                return {};
            if (bannedWords().contains (word))
                return word;
            if (word.containsAnyOf ("0123456789#"))
                return {};
            if (word.length() == 1 && juce::CharacterFunctions::isUpperCase (word[0]) && before != '+'
                && ! ((word == "A" || word == "I") && wordStartsSentence))
                return word;
            if (word.length() > 1 && word.toUpperCase() == word && word.toLowerCase() != word)
                return word;
            return {};
        };

        for (auto c : text)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '#')
            {
                if (word.isEmpty())
                {
                    before = previous;
                    wordStartsSentence = sentenceStart;
                }
                word << juce::String::charToString (c);
                sentenceStart = false;
            }
            else
            {
                if (auto bad = check(); bad.isNotEmpty())
                    return bad;
                word.clear();
                if (c == '.' || c == ':' || c == '!' || c == '?' || c == '(')
                    sentenceStart = true;
                else if (! juce::CharacterFunctions::isWhitespace (c) && c != '"')
                    sentenceStart = false;
            }
            previous = c;
        }
        return check();
    }

    // Every piece of text a user can read in a component tree: tooltips, button text, labels,
    // combo-box items (and the text shown when nothing is chosen), slider values and titles.
    inline void collect (juce::Component& c, juce::StringArray& out)
    {
        if (auto* tip = dynamic_cast<juce::SettableTooltipClient*> (&c))
            out.add (tip->getTooltip());
        if (auto* b = dynamic_cast<juce::Button*> (&c))
            out.add (b->getButtonText());
        if (auto* l = dynamic_cast<juce::Label*> (&c))
            out.add (l->getText());
        if (auto* box = dynamic_cast<juce::ComboBox*> (&c))
        {
            for (int i = 0; i < box->getNumItems(); ++i)
                out.add (box->getItemText (i));
            out.add (box->getText());
            out.add (box->getTextWhenNothingSelected());
        }
        if (auto* s = dynamic_cast<juce::Slider*> (&c))
            out.add (s->getTextFromValue (s->getValue()));
        out.add (c.getTitle());

        for (auto* child : c.getChildren())
            collect (*child, out);
    }

    inline void collect (const juce::PopupMenu& menu, juce::StringArray& out)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            out.add (it.getItem().text);
            if (auto* sub = it.getItem().subMenu.get())
                collect (*sub, out);
        }
    }

    // Fails the test for every text that breaks the rule. 'what' names where the texts came from.
    inline void expectPlainWords (juce::UnitTest& t, const juce::StringArray& texts, const juce::String& what)
    {
        for (auto& text : texts)
            if (const auto bad = findBanned (text); bad.isNotEmpty())
                t.expect (false, what + ": \"" + bad + "\" in \"" + text + "\" (spell it out)");
    }

    // Every visible label and text button in the tree shows its whole text in its font: each word
    // fits the width, and the wrapped lines fit the height. (Change the layout, never the word.)
    inline void expectTextFits (juce::UnitTest& t, juce::Component& root, juce::LookAndFeel& lf, const juce::String& context)
    {
        auto fits = [] (const juce::Font& f, const juce::String& text, juce::Rectangle<int> area) -> bool
        {
            const float lineH = f.getHeight();
            int lines = 1;
            float lineW = 0.0f;
            const float space = juce::GlyphArrangement::getStringWidth (f, " ");
            for (auto& w : juce::StringArray::fromTokens (text, " ", {}))
            {
                const float ww = juce::GlyphArrangement::getStringWidth (f, w);
                if (ww > (float) area.getWidth() + 0.5f)
                    return false;
                if (lineW > 0.0f && lineW + space + ww > (float) area.getWidth() + 0.5f) { ++lines; lineW = ww; }
                else lineW += (lineW > 0.0f ? space : 0.0f) + ww;
            }
            return (float) lines * lineH <= (float) area.getHeight() + 1.0f;
        };

        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            if (! c.isVisible())
                return;
            if (auto* l = dynamic_cast<juce::Label*> (&c); l != nullptr && l->getText().isNotEmpty() && ! l->getBounds().isEmpty()
                                                           && dynamic_cast<juce::ComboBox*> (c.getParentComponent()) == nullptr
                                                           && dynamic_cast<juce::Slider*> (c.getParentComponent()) == nullptr)
            {
                const auto area = lf.getLabelBorderSize (*l).subtractedFrom (l->getLocalBounds());
                t.expect (fits (lf.getLabelFont (*l), l->getText(), area),
                          context + ": label \"" + l->getText() + "\" fits " + l->getBounds().toString());
            }
            if (auto* b = dynamic_cast<juce::TextButton*> (&c); b != nullptr && b->getButtonText().isNotEmpty() && ! b->getBounds().isEmpty())
                t.expect (fits (lf.getTextButtonFont (*b, b->getHeight()), b->getButtonText(), b->getLocalBounds().reduced (4, 1)),
                          context + ": button \"" + b->getButtonText() + "\" fits " + b->getBounds().toString());
            if (auto* box = dynamic_cast<juce::ComboBox*> (&c); box != nullptr && ! box->getBounds().isEmpty())
            {
                const auto f = lf.getComboBoxFont (*box);
                for (int i = 0; i < box->getNumItems(); ++i)
                    t.expect (juce::GlyphArrangement::getStringWidth (f, box->getItemText (i)) <= (float) box->getWidth() - 34.0f,
                              context + ": combo box item \"" + box->getItemText (i) + "\" fits " + box->getBounds().toString());
            }
            for (auto* child : c.getChildren())
                walk (*child);
        };
        walk (root);
    }
}
