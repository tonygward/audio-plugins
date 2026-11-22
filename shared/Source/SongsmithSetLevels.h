#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SongsmithColours.h"
#include "SongsmithType.h"

#if JUCE_WINDOWS
 // Declared here instead of including <windows.h>, whose min and max macros break other code.
extern "C" __declspec (dllimport) int __stdcall SystemParametersInfoW (unsigned int, unsigned int, void*, unsigned int);
#endif

// The Set Levels dialog, shared by every plugin that has one (songsmithstudio#19): the Analog Compressor's look and
// interaction, the same listening ear, and an optional editable target.
//
// It fills the whole editor area. While it is open the area behind it is dimmed and takes no clicks, so only the
// panel can be used. The panel is a subtly see-through dark surface with an accent outline. Stages:
//   idle      closed (or showing a one-off notice with a Close button)
//   listening Finish Listening / Cancel, with the ear showing whether sound is arriving
//   proposal  Apply / Cancel. Apply writes the plugin's real settings and closes the panel at once: there is no
//             second message to dismiss. Cancel and Escape change nothing.
//
// The plugin owns the measuring and the parameter writes (it knows its own DSP); this component owns the look, the
// stages, the keyboard and the listening indicator, so they cannot drift apart between plugins.
namespace Songsmith::UI
{
    // True when the person has asked their computer to cut back on animation. Only Windows can be asked from here;
    // elsewhere the ear never animates, which is the safe answer.
    inline bool prefersReducedMotion()
    {
       #if JUCE_WINDOWS
        int animations = 1;
        if (SystemParametersInfoW (0x1042 /* SPI_GETCLIENTAREAANIMATION */, 0, &animations, 0))
            return animations == 0;
       #endif
        return false;
    }

    // The listening indicator: a vector ear with a short plain-words status beside it. It listens to the plugin's own
    // audio stream, not a microphone, and says so in its tooltip.
    class ListeningEar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        enum class State { hidden, waitingForAudio, listening };

        static constexpr int iconSize = 22;

        ListeningEar()
        {
            setInterceptsMouseClicks (true, false);
            setTitle ("Listening status");
            setTooltip ("Set Levels listens to the sound passing through this plugin, not to a microphone. It is only listening while this ear is lit.");
            setVisible (false);
        }

        void setState (State s)
        {
            if (s == state)
                return;
            state = s;
            setVisible (s != State::hidden);
            setTitle (statusText());
            repaint();
        }

        State getState() const noexcept { return state; }

        // The words shown beside the ear, also its accessible name.
        juce::String statusText() const
        {
            return state == State::listening ? juce::String ("Listening") + juce::String::charToString (0x2026)
                   : state == State::waitingForAudio ? juce::String ("Waiting for audio") + juce::String::charToString (0x2026)
                   : juce::String();
        }

        // Called from the editor's timer (about 30 times a second): moves the pulse a little.
        void tick()
        {
            if (state != State::listening || reducedMotion)
                return;
            phase += 0.12f;
            repaint();
        }

        void setReducedMotion (bool reduced) noexcept { reducedMotion = reduced; }
        void setAccent (juce::Colour c) { accentColour = c; repaint(); }
        bool isAnimating() const noexcept { return state == State::listening && ! reducedMotion; }

        // The ear's outline in a unit square (0 to 1 on both axes).
        static juce::Path earPath()
        {
            juce::Path p;
            // The outer rim: from the lobe, up round the top, down the back of the ear.
            p.startNewSubPath (0.50f, 0.96f);
            p.cubicTo (0.30f, 0.86f, 0.20f, 0.60f, 0.18f, 0.42f);
            p.cubicTo (0.14f, 0.16f, 0.36f, 0.04f, 0.56f, 0.07f);
            p.cubicTo (0.80f, 0.11f, 0.90f, 0.34f, 0.80f, 0.54f);
            p.cubicTo (0.72f, 0.70f, 0.58f, 0.74f, 0.56f, 0.86f);
            p.cubicTo (0.55f, 0.93f, 0.62f, 0.97f, 0.68f, 0.93f);
            // The inner curl.
            p.startNewSubPath (0.38f, 0.50f);
            p.cubicTo (0.36f, 0.30f, 0.54f, 0.22f, 0.64f, 0.34f);
            p.cubicTo (0.70f, 0.42f, 0.62f, 0.52f, 0.54f, 0.60f);
            return p;
        }

        void paint (juce::Graphics& g) override
        {
            if (state == State::hidden)
                return;

            auto area = getLocalBounds().toFloat();
            const auto icon = area.removeFromLeft ((float) iconSize).withSizeKeepingCentre ((float) iconSize, (float) iconSize);
            const bool on = state == State::listening;
            const float pulse = isAnimating() ? 0.5f + 0.5f * std::sin (phase) : 1.0f;

            const auto accent = accentColour;
            if (on)
            {
                g.setColour (accent.withAlpha (0.16f + 0.14f * pulse));
                g.fillEllipse (icon.expanded (3.0f));
            }
            g.setColour (on ? accent.withAlpha (0.75f + 0.25f * pulse) : Colours::textDim);
            juce::Path ear = earPath();
            ear.applyTransform (juce::AffineTransform::scale (icon.getWidth() * 0.82f, icon.getHeight() * 0.82f)
                                    .translated (icon.getX() + icon.getWidth() * 0.09f, icon.getY() + icon.getHeight() * 0.09f));
            g.strokePath (ear, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (on ? Colours::text : Colours::textDim);
            g.setFont (juce::Font (font (Type::control, Weight::Medium)));
            g.drawText (statusText(), area.withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
        }

    private:
        State state = State::hidden;
        float phase = 0.0f;
        juce::Colour accentColour { 0xff7fb2e5 };
        bool reducedMotion = prefersReducedMotion();
    };

    class SetLevelsDialog : public juce::Component
    {
    public:
        enum class Stage { idle, listening, proposal };

        static constexpr float dimAlpha = 0.22f;     // the editor behind the panel
        static constexpr float fillAlpha = 0.88f;    // the panel: 12 % see-through
        static constexpr int panelWidth = 340, panelHeightBase = 192, targetRowHeight = 30;
        static constexpr int noticeTicks = 8 * 30;   // a one-off notice stays eight seconds

        SetLevelsDialog()
        {
            setInterceptsMouseClicks (true, true);   // the scrim takes every click that is not on the panel
            setWantsKeyboardFocus (true);
            setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);   // Tab stays inside while open
            setTitle ("Set Levels panel");
            setVisible (false);

            heading.setFont (juce::Font (font (Type::heading, Weight::SemiBold)));
            heading.setColour (juce::Label::textColourId, Colours::text);
            heading.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (heading);

            message.setTitle ("Set Levels message");
            message.setFont (juce::Font (font (Type::hint, Weight::Regular)));
            message.setColour (juce::Label::textColourId, Colours::text);
            message.setJustificationType (juce::Justification::topLeft);
            message.setMinimumHorizontalScale (1.0f);   // wrap, never squash
            addAndMakeVisible (message);

            action.onClick = [this] { if (stage == Stage::listening) { if (onFinish) onFinish(); } else if (stage == Stage::proposal) { if (onApply) onApply(); } };
            cancel.onClick = [this] { if (stage == Stage::idle) closeNow(); else if (onCancel) onCancel(); };
            addAndMakeVisible (action);
            addAndMakeVisible (cancel);
            addAndMakeVisible (ear);

            targetCaption.setFont (juce::Font (font (Type::control, Weight::Medium)));
            targetCaption.setColour (juce::Label::textColourId, Colours::textDim);
            targetCaption.setInterceptsMouseClicks (false, false);
            targetField.setFont (juce::Font (numbers (Type::control)));
            targetField.setJustificationType (juce::Justification::centred);
            targetField.setColour (juce::Label::textColourId, Colours::text);
            targetField.setColour (juce::Label::backgroundColourId, Colours::recess.withAlpha (0.9f));
            targetField.setColour (juce::Label::backgroundWhenEditingColourId, Colours::recess);
            targetField.setColour (juce::Label::textWhenEditingColourId, Colours::text);
            targetField.setColour (juce::Label::outlineWhenEditingColourId, Colours::edge);
            targetField.setEditable (false, true, false);   // click or double-click to type, Enter applies, Escape cancels
            targetField.onTextChange = [this] { commitTargetText(); };
            addChildComponent (targetCaption);
            addChildComponent (targetField);

            setStage (Stage::idle);
        }

        // ---- The optional editable target -------------------------------------------------------------------------
        // format: value to the text shown. parse: typed text to a value (false = not valid, the old value stays).
        void enableTarget (const juce::String& caption, const juce::String& tooltip, double initialValue,
                           std::function<juce::String (double)> format, std::function<bool (const juce::String&, double&)> parse)
        {
            hasTarget = true;
            formatTarget = std::move (format);
            parseTarget = std::move (parse);
            targetCaption.setText (caption, juce::dontSendNotification);
            targetCaption.setTooltip (tooltip);
            targetField.setTooltip (tooltip);
            targetField.setTitle (caption);
            targetValue = initialValue;
            targetField.setText (formatTarget (targetValue), juce::dontSendNotification);
            targetCaption.setVisible (true);
            targetField.setVisible (true);
            layoutPanel();
        }

        double getTargetValue() const noexcept { return targetValue; }

        void setTargetValue (double v)
        {
            targetValue = v;
            if (formatTarget)
                targetField.setText (formatTarget (v), juce::dontSendNotification);
        }

        // Types a value as the musician would (also used by tests). Returns false if the text is not a valid value.
        bool commitTargetText (const juce::String& text)
        {
            double v = targetValue;
            if (! parseTarget || ! parseTarget (text, v))
            {
                targetField.setText (formatTarget (targetValue), juce::dontSendNotification);   // not valid: keep the old value
                return false;
            }
            targetValue = v;
            targetField.setText (formatTarget (targetValue), juce::dontSendNotification);   // shows the checked value
            if (onTargetEdited)
                onTargetEdited (targetValue);
            return true;
        }

        // ---- Stages -----------------------------------------------------------------------------------------------
        Stage getStage() const noexcept { return stage; }
        bool isOpen() const noexcept { return isVisible(); }

        void beginListening (const juce::String& text)
        {
            message.setText (text, juce::dontSendNotification);
            ear.setState (ListeningEar::State::waitingForAudio);
            setStage (Stage::listening);
            grabKeyboardFocus();   // Escape cancels, Tab reaches the panel's buttons
        }

        // Called from the editor's timer while listening. heardSound: audio above silence has arrived.
        void updateListening (bool heardSound, const juce::String& text)
        {
            if (stage != Stage::listening)
                return;
            ear.setState (heardSound ? ListeningEar::State::listening : ListeningEar::State::waitingForAudio);
            if (text != message.getText())
                message.setText (text, juce::dontSendNotification);
        }

        void showProposal (const juce::String& text, bool usable)
        {
            message.setText (text, juce::dontSendNotification);
            ear.setState (ListeningEar::State::hidden);
            applyUsable = usable;
            setStage (Stage::proposal);
        }

        // A result with nothing to apply (silence, too short, settings changed): a Close button, then it goes by itself.
        void showNotice (const juce::String& text)
        {
            message.setText (text, juce::dontSendNotification);
            ear.setState (ListeningEar::State::hidden);
            setStage (Stage::idle);
            noticeLeft = noticeTicks;
            setVisible (true);
            toFront (false);
        }

        // Closes at once, changing nothing (Apply, Cancel and Escape end here).
        void closeNow()
        {
            noticeLeft = 0;
            message.setText ({}, juce::dontSendNotification);
            ear.setState (ListeningEar::State::hidden);
            setStage (Stage::idle);
            setVisible (false);
        }

        // About 30 times a second from the editor's timer.
        void tick()
        {
            ear.tick();
            if (stage == Stage::idle && noticeLeft > 0 && --noticeLeft == 0)
                closeNow();
        }

        // Where the panel sits, in this component's own coordinates (the dialog fills the editor area).
        void setPanelAnchor (juce::Rectangle<int> bounds) { anchor = bounds; layoutPanel(); }

        // Escape: cancel while listening or with a proposal, close a notice.
        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key != juce::KeyPress::escapeKey)
                return false;
            if (stage == Stage::idle)
                closeNow();
            else if (onCancel)
                onCancel();
            return true;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (dimAlpha));
            const auto r = panelBounds.toFloat().reduced (1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 8.0f);
            g.setColour (Colours::panel.brighter (0.12f).withAlpha (fillAlpha));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (accentColour.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 8.0f, 1.2f);
        }

        void resized() override { layoutPanel(); }

        void setAccent (juce::Colour c) { accentColour = c; ear.setAccent (c); }

        // The panel's parts, for the plugin and its tests.
        juce::Rectangle<int> getPanelBounds() const noexcept { return panelBounds; }
        juce::TextButton& getActionButton() noexcept { return action; }
        juce::TextButton& getCancelButton() noexcept { return cancel; }
        juce::Label& getMessage() noexcept { return message; }
        juce::Label& getHeading() noexcept { return heading; }
        juce::Label& getTargetField() noexcept { return targetField; }
        ListeningEar& getEar() noexcept { return ear; }

        std::function<void()> onFinish, onApply, onCancel;
        std::function<void (double)> onTargetEdited;

        // Wording a plugin may adjust: what Apply changes.
        juce::String applyTooltip = "Apply: use the suggestion. Undo restores the old settings.";
        juce::String finishTooltip = "Finish Listening: stop listening and see the suggestion. Nothing changes until you press Apply.";

    private:
        void commitTargetText() { commitTargetText (targetField.getText()); }

        void setStage (Stage s)
        {
            stage = s;
            const bool idle = s == Stage::idle;
            action.setButtonText (s == Stage::proposal ? "Apply" : "Finish Listening");
            action.setTitle (action.getButtonText());
            action.setTooltip (s == Stage::proposal ? applyTooltip : finishTooltip);
            action.setVisible (! idle);
            action.setEnabled (s != Stage::proposal || applyUsable);
            cancel.setButtonText (idle ? "Close" : "Cancel");
            cancel.setTitle (idle ? "Close Set Levels" : "Cancel Set Levels");
            cancel.setTooltip (idle ? "Close: nothing was changed." : "Cancel: leave every setting exactly as it is.");
            message.setTooltip (message.getText());
            heading.setText (s == Stage::listening ? "Set Levels: listening" : s == Stage::proposal ? "Set Levels: proposal" : "Set Levels",
                             juce::dontSendNotification);
            const bool open = ! idle || message.getText().isNotEmpty();
            if (open && ! isVisible())
            {
                layoutPanel();
                setVisible (true);
                toFront (false);
            }
            else if (! open)
            {
                setVisible (false);
            }
            repaint();
        }

        void layoutPanel()
        {
            const int height = panelHeightBase + (hasTarget ? targetRowHeight : 0);
            juce::Rectangle<int> p (0, 0, panelWidth, height);
            p.setPosition (anchor.getRight() - panelWidth, anchor.getY() - height);
            if (anchor.isEmpty())
                p = juce::Rectangle<int> (panelWidth, height).withCentre (getLocalBounds().getCentre());
            const auto limit = getLocalBounds().reduced (6);
            panelBounds = limit.isEmpty() ? p : p.constrainedWithin (limit);

            auto r = panelBounds.reduced (12, 10);
            heading.setBounds (r.removeFromTop (22));
            if (hasTarget)
            {
                auto row = r.removeFromTop (targetRowHeight);
                targetCaption.setBounds (row.removeFromLeft (row.getWidth() - 96));
                targetField.setBounds (row.withSizeKeepingCentre (96, 24));
                r.removeFromTop (4);
            }
            auto buttons = r.removeFromBottom (28);
            r.removeFromBottom (6);
            auto earRow = r.removeFromBottom (ListeningEar::iconSize + 2);
            ear.setBounds (earRow);
            message.setBounds (r);
            action.setBounds (buttons.removeFromLeft ((buttons.getWidth() - 8) / 2));
            buttons.removeFromLeft (8);
            cancel.setBounds (buttons);
        }

        Stage stage = Stage::idle;
        juce::Rectangle<int> anchor, panelBounds;
        juce::Colour accentColour { 0xff7fb2e5 };
        juce::Label heading, message, targetCaption, targetField;
        juce::TextButton action { "Finish Listening" }, cancel { "Cancel" };
        ListeningEar ear;
        bool hasTarget = false, applyUsable = false;
        double targetValue = 0.0;
        int noticeLeft = 0;
        std::function<juce::String (double)> formatTarget;
        std::function<bool (const juce::String&, double&)> parseTarget;
    };
}
