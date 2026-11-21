#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "ResponseGraph.h"
#include "../../shared/Source/SongsmithColours.h"
#include "../../shared/Source/SongsmithLookAndFeel.h"
#include "../../shared/Source/SongsmithType.h"

// ANALYZER-02: the two small pieces of the Plugin Analyzer's capture workflow.
//
//  - CaptureRow: one line of the legend under the graph. A Show switch (hiding a capture never deletes it), a sample of the
//    line it is drawn with (solid for the first capture, dashed for the second, so colour is never the only difference),
//    and its name, which can be changed at any time.
//  - ChoicePanel: the question that appears over the graph when pressing Start (or Clear All Captures) would change a capture you
//    already have. It says exactly which capture would be replaced, offers to add a second one instead, and does nothing until
//    you choose. It is part of the editor, not a separate window, so it scales with it and works from the keyboard.
namespace PluginAnalyzer::UI
{
    class CaptureRow : public juce::Component
    {
    public:
        CaptureRow (int slotIndex, juce::Colour lineColour, bool dashedLine) : index (slotIndex), colour (lineColour), dashed (dashedLine)
        {
            namespace U = Songsmith::UI;
            addAndMakeVisible (visibleToggle);
            addAndMakeVisible (name);
            addAndMakeVisible (info);
            visibleToggle.setButtonText ("Show");
            visibleToggle.setTitle ("Show " + defaultName());
            visibleToggle.setTooltip ("Show or hide this capture on the graph. Hiding it does not delete it.");
            visibleToggle.onClick = [this] { if (onVisibleChange) onVisibleChange (visibleToggle.getToggleState()); };
            name.setMultiLine (false);
            name.setInputRestrictions (40);
            name.setFont (U::font (U::Type::control));
            name.setTitle ("Name of " + defaultName());
            name.setTooltip ("The name of this capture. It appears on the graph and in the CSV file, so you can tell two captures apart. Click to change it.");
            name.onTextChange = [this]
            {
                if (settingText)
                    return;
                if (onNameChange)
                    onNameChange (name.getText());
                lastShown = name.getText();   // what was typed has been taken, so the next refresh may show the capture's own name
            };
            info.setFont (U::font (U::Type::hint));
            info.setColour (juce::Label::textColourId, U::Colours::textDim);
            info.setJustificationType (juce::Justification::centredLeft);
            info.setInterceptsMouseClicks (false, false);
            info.setMinimumHorizontalScale (1.0f);
            info.setBorderSize ({ 0, 0, 0, 0 });
        }

        // 'filled' false shows the row as waiting for its capture.
        void setCapture (bool filled, const juce::String& captureName, const juce::String& detail, bool shown)
        {
            hasCapture = filled;
            settingText = true;
            // A name being typed is never overwritten, even in the moment before it is reported (text changes arrive on the message loop).
            if (! name.hasKeyboardFocus (true) && name.getText() == lastShown)
            {
                name.setText (captureName, juce::dontSendNotification);
                lastShown = captureName;
            }
            settingText = false;
            visibleToggle.setToggleState (shown, juce::dontSendNotification);
            visibleToggle.setEnabled (filled);
            name.setEnabled (filled);
            info.setText (filled ? detail : juce::String ("Not made yet"), juce::dontSendNotification);
            repaint();
        }

        juce::String defaultName() const { return "Capture " + juce::String (index + 1); }
        bool isFilled() const noexcept { return hasCapture; }
        juce::ToggleButton& getVisibleToggle() noexcept { return visibleToggle; }
        juce::TextEditor& getNameEditor() noexcept { return name; }
        juce::Label& getInfoLabel() noexcept { return info; }
        juce::Rectangle<int> getSwatchBounds() const noexcept { return swatch; }

        std::function<void (bool)> onVisibleChange;
        std::function<void (const juce::String&)> onNameChange;

        void resized() override
        {
            auto r = getLocalBounds();
            visibleToggle.setBounds (r.removeFromLeft (84));
            r.removeFromLeft (8);
            swatch = r.removeFromLeft (44);
            r.removeFromLeft (8);
            name.setBounds (r.removeFromLeft (250));
            r.removeFromLeft (12);
            info.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            // A sample of the line this capture is drawn with on the graph.
            const float y = (float) swatch.getCentreY();
            const float alpha = hasCapture && visibleToggle.getToggleState() ? 1.0f : 0.35f;
            g.setColour (colour.withAlpha (alpha));
            if (dashed)
            {
                g.fillRect (juce::Rectangle<float> ((float) swatch.getX(), y - 1.0f, 12.0f, 2.0f));
                g.fillRect (juce::Rectangle<float> ((float) swatch.getX() + 17.0f, y - 1.0f, 12.0f, 2.0f));
                g.fillRect (juce::Rectangle<float> ((float) swatch.getX() + 34.0f, y - 1.0f, 10.0f, 2.0f));
            }
            else
                g.fillRect (juce::Rectangle<float> ((float) swatch.getX(), y - 1.2f, (float) swatch.getWidth(), 2.4f));
        }

    private:
        int index;
        juce::Colour colour;
        bool dashed;
        bool hasCapture = false, settingText = false;
        juce::String lastShown;
        juce::Rectangle<int> swatch;
        juce::ToggleButton visibleToggle;
        juce::TextEditor name;
        juce::Label info;
    };

    class ChoicePanel : public juce::Component
    {
    public:
        struct Option { juce::String label; int id; };
        static constexpr int cancelId = 0;

        ChoicePanel()
        {
            namespace U = Songsmith::UI;
            setVisible (false);
            setWantsKeyboardFocus (true);
            setTitle ("Choose what to do");
            for (auto* l : { &titleLabel, &messageLabel, &nameLabel })
            {
                addChildComponent (*l);
                l->setBorderSize ({ 0, 0, 0, 0 });
                l->setMinimumHorizontalScale (1.0f);
                l->setInterceptsMouseClicks (false, false);
                l->setJustificationType (juce::Justification::topLeft);
            }
            titleLabel.setFont (U::font (U::Type::heading, U::Weight::SemiBold));
            titleLabel.setColour (juce::Label::textColourId, U::Colours::text);
            messageLabel.setFont (U::font (U::Type::control));
            messageLabel.setColour (juce::Label::textColourId, U::Colours::textDim);
            nameLabel.setFont (U::font (U::Type::control, U::Weight::Medium));
            nameLabel.setColour (juce::Label::textColourId, U::Colours::textDim);
            addChildComponent (nameEditor);
            nameEditor.setMultiLine (false);
            nameEditor.setInputRestrictions (40);
            nameEditor.setFont (U::font (U::Type::control));
            nameEditor.setTitle ("Name for the new capture");
            nameEditor.onTextChange = [this] { nameEdited = true; };
            for (auto& b : buttons)
            {
                addChildComponent (b);
                b.onClick = [this, &b] { choose (b.getComponentID().getIntValue()); };
            }
        }

        // Shows the question. 'defaultName' is offered in the name box when 'askName' is true (an empty one keeps names as they are).
        void open (const juce::String& title, const juce::String& message, std::vector<Option> choices, bool askName, const juce::String& defaultName)
        {
            options = std::move (choices);
            if (options.size() > buttons.size())
                options.resize (buttons.size());
            titleLabel.setText (title, juce::dontSendNotification);
            messageLabel.setText (message, juce::dontSendNotification);
            nameLabel.setText ("Name for the new capture", juce::dontSendNotification);
            nameEditor.setText (defaultName, juce::dontSendNotification);
            nameEdited = false;
            wantsName = askName;
            setTitle (title);
            setDescription (message);
            for (size_t i = 0; i < buttons.size(); ++i)
            {
                const bool used = i < options.size();
                buttons[i].setVisible (used);
                if (used)
                {
                    buttons[i].setButtonText (options[i].label);
                    buttons[i].setComponentID (juce::String (options[i].id));
                    buttons[i].setTitle (options[i].label);
                    buttons[i].setTooltip (options[i].id == cancelId ? "Close this and change nothing." : options[i].label);
                }
            }
            for (auto* c : std::initializer_list<juce::Component*> { &titleLabel, &messageLabel })
                c->setVisible (true);
            nameLabel.setVisible (askName);
            nameEditor.setVisible (askName);
            layout();
            setVisible (true);
            toFront (false);
            if (! options.empty())
                buttons[0].grabKeyboardFocus();
            repaint();
        }

        void close() { setVisible (false); }
        bool isOpen() const noexcept { return isVisible(); }
        const std::vector<Option>& getOptions() const noexcept { return options; }
        juce::TextButton& getButton (int i) noexcept { return buttons[(size_t) i]; }
        juce::TextEditor& getNameEditor() noexcept { return nameEditor; }
        juce::Label& getTitleLabel() noexcept { return titleLabel; }
        juce::Label& getMessageLabel() noexcept { return messageLabel; }
        juce::Label& getNameLabel() noexcept { return nameLabel; }
        juce::Rectangle<int> getBoxBounds() const noexcept { return box; }
        bool wasNameEdited() const noexcept { return nameEdited; }

        // Called with the chosen option's id (cancelId for Cancel or Escape), and the name that was typed (empty if the box was
        // not shown or left empty), and whether it was changed from the one offered.
        std::function<void (int, const juce::String&, bool)> onChoice;

        void resized() override { layout(); }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey)
            {
                choose (cancelId);
                return true;
            }
            return true;   // while the question is open, keys do nothing else
        }

        void mouseDown (const juce::MouseEvent&) override {}   // the graph underneath is covered, so a click on it does nothing

        void paint (juce::Graphics& g) override
        {
            namespace U = Songsmith::UI;
            g.fillAll (juce::Colours::black.withAlpha (0.55f));
            U::LookAndFeelBase::drawWell (g, box.toFloat(), U::Colours::panelLight);
            g.setColour (U::Colours::edge);
            g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 4.0f, 1.0f);
        }

    private:
        void layout()
        {
            const int w = juce::jmin (getWidth() - 40, 620);
            const int h = wantsName ? 232 : 184;
            box = juce::Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre());
            auto r = box.reduced (20, 16);
            titleLabel.setBounds (r.removeFromTop (20));
            r.removeFromTop (6);
            messageLabel.setBounds (r.removeFromTop (54));
            if (wantsName)
            {
                r.removeFromTop (4);
                nameLabel.setBounds (r.removeFromTop (16));
                nameEditor.setBounds (r.removeFromTop (28));
            }
            auto row = box.reduced (20, 16).removeFromBottom (32);
            const int count = (int) options.size();
            const int gap = 10, bw = count > 0 ? (row.getWidth() - (count - 1) * gap) / count : 0;
            for (int i = 0; i < count; ++i)
                buttons[(size_t) i].setBounds (row.getX() + i * (bw + gap), row.getY(), bw, row.getHeight());
        }

        void choose (int id)
        {
            const auto typed = wantsName ? nameEditor.getText().trim() : juce::String();
            const bool edited = nameEdited;
            close();
            if (onChoice)
                onChoice (id, typed, edited);
        }

        std::vector<Option> options;
        bool wantsName = false, nameEdited = false;
        juce::Rectangle<int> box;
        juce::Label titleLabel, messageLabel, nameLabel;
        juce::TextEditor nameEditor;
        std::array<juce::TextButton, 3> buttons;
    };
}
