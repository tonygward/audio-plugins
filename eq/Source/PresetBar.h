#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Top-strip preset selector: [<] [ name * v ] [>]
// Clicking the name opens the menu (Save, Save As, Delete, Reveal folder, Factory, User).
// '*' marks settings that differ from the loaded preset.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (ThreeBandEQAudioProcessor&);

    void refresh();   // call from the editor timer
    void resized() override;

private:
    void showMenu();
    void saveAsDialog();
    void confirmDelete();
    void report (bool ok, const juce::String& error);

    ThreeBandEQAudioProcessor& processor;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, nameButton;
    std::unique_ptr<juce::AlertWindow> dialog;
    juce::ScopedMessageBox messageBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};
