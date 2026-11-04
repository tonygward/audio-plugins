#include "PresetBar.h"
#include "EQLookAndFeel.h"

PresetBar::PresetBar (SongsmithEQAudioProcessor& p) : processor (p)
{
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    prevButton.onClick = [this] { juce::String e; report (processor.presets.loadNext (-1, e), e); };
    nextButton.onClick = [this] { juce::String e; report (processor.presets.loadNext (+1, e), e); };

    nameButton.setTooltip ("Presets: load, save, delete");
    nameButton.onClick = [this] { showMenu(); };
    nameButton.getProperties().set (Songsmith::UI::LookAndFeelBase::wellProperty, true);   // a field: the current preset

    for (auto* b : { &prevButton, &nextButton, &nameButton })
        addAndMakeVisible (b);

    refresh();
}

void PresetBar::refresh()
{
    auto& pm = processor.presets;
    juce::String text = pm.hasCurrent() ? pm.getCurrent().name : juce::String ("No preset");
    if (pm.isModified())
        text << " *";
    text << "  " << juce::String::charToString (0x25be);   // small down triangle

    if (nameButton.getButtonText() != text)
        nameButton.setButtonText (text);

    // A starting-point template shows its usage hint as the tooltip.
    juce::String tip ("Presets: load, save, delete");
    if (pm.hasCurrent() && pm.getCurrent().isFactory())
        if (const auto hint = EQ::PresetManager::getFactoryHint (pm.getCurrent().factoryIndex); hint.isNotEmpty())
            tip = hint;

    if (nameButton.getTooltip() != tip)
        nameButton.setTooltip (tip);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    prevButton.setBounds (r.removeFromLeft (26));
    nextButton.setBounds (r.removeFromRight (26));
    r.reduce (4, 0);
    nameButton.setBounds (r);
}

void PresetBar::report (bool ok, const juce::String& error)
{
    if (! ok && error.isNotEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", error, {}, this);
    refresh();
}

void PresetBar::showMenu()
{
    auto& pm = processor.presets;
    const auto all = pm.getAllPresets();

    juce::PopupMenu menu;
    menu.addItem (1, "Save", pm.canSaveInPlace());
    menu.addItem (2, "Save As...");
    menu.addItem (3, "Delete", pm.hasCurrent() && ! pm.getCurrent().isFactory());
    menu.addItem (4, "Reveal preset folder");

    const auto isCurrent = [&] (const EQ::PresetManager::Entry& e)
    {
        const auto& c = pm.getCurrent();
        return pm.hasCurrent() && e.factoryIndex == c.factoryIndex && (e.isFactory() || e.file == c.file);
    };

    menu.addSectionHeader ("Factory");
    bool anyUser = false, startingPoints = false;
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].isFactory() && ! startingPoints && EQ::PresetManager::isStartingPoint (all[i].factoryIndex))
        {
            menu.addSectionHeader ("Starting points");
            startingPoints = true;
        }

        if (! all[i].isFactory() && ! anyUser)
        {
            menu.addSectionHeader ("User");
            anyUser = true;
        }
        menu.addItem (100 + (int) i, all[i].name, true, isCurrent (all[i]));
    }
    if (! anyUser)
    {
        menu.addSectionHeader ("User");
        menu.addItem (99, "(none yet: use Save As)", false);
    }

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                        [safe, all] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            auto& pm = safe->processor.presets;
                            juce::String error;

                            if (result == 1)
                                safe->report (pm.saveAs (pm.getCurrent().name, error), error);
                            else if (result == 2)
                                safe->saveAsDialog();
                            else if (result == 3)
                                safe->confirmDelete();
                            else if (result == 4)
                            {
                                const auto folder = pm.getUserFolder();
                                folder.createDirectory();
                                folder.revealToUser();
                            }
                            else if (result >= 100 && result - 100 < (int) all.size())
                                safe->report (pm.load (all[(size_t) (result - 100)], error), error);
                        });
}

void PresetBar::saveAsDialog()
{
    auto& pm = processor.presets;
    const juce::String suggested = (pm.hasCurrent() && ! pm.getCurrent().isFactory()) ? pm.getCurrent().name : juce::String();

    dialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("name", suggested);
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->dialog == nullptr)
            return;

        const auto name = safe->dialog->getTextEditorContents ("name");
        safe->dialog.reset();

        if (result == 1)
        {
            juce::String error;
            safe->report (safe->processor.presets.saveAs (name, error), error);
        }
    }), false);
}

void PresetBar::confirmDelete()
{
    auto& pm = processor.presets;
    if (! pm.hasCurrent() || pm.getCurrent().isFactory())
        return;

    juce::Component::SafePointer<PresetBar> safe (this);
    const auto options = juce::MessageBoxOptions{}
                             .withIconType (juce::MessageBoxIconType::QuestionIcon)
                             .withTitle ("Delete preset")
                             .withMessage ("Delete the user preset \"" + pm.getCurrent().name + "\"?")
                             .withButton ("Delete", 1)
                             .withButton ("Cancel", 0)
                             .withAssociatedComponent (this);

    messageBox = juce::AlertWindow::showScopedAsync (options, [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        juce::String error;
        safe->report (safe->processor.presets.deleteCurrent (error), error);
    });
}
