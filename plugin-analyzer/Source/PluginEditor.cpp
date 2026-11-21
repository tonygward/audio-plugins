#include "PluginEditor.h"

using namespace PluginAnalyzer;
namespace SUI = Songsmith::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;

    void styleLabel (juce::Label& l, const juce::String& text, float height, SUI::Weight weight, juce::Colour colour,
                     juce::Justification j = juce::Justification::centredLeft)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (SUI::font (height, weight));
        l.setColour (juce::Label::textColourId, colour);
        l.setJustificationType (j);
        l.setBorderSize ({ 0, 0, 0, 0 });
        l.setMinimumHorizontalScale (1.0f);
        l.setInterceptsMouseClicks (false, false);
    }
}

//==============================================================================
SongsmithPluginAnalyzerEditor::SongsmithPluginAnalyzerEditor (SongsmithPluginAnalyzerProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      levelAttachment (p.apvts, levelId, levelSlider),
      bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    const auto dim = SUI::Colours::textDim;
    styleLabel (roleLabel, "Role", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (pairLabel, "Pair With", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (channelLabel, "Channel", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (levelLabel, "Test Level", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (levelValue, {}, SUI::Type::control, SUI::Weight::Regular, SUI::Colours::text, juce::Justification::centredRight);
    styleLabel (rangeLabel, "Range", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (notesTitle, "Notes About the Captures", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (statusLabel, {}, SUI::Type::control, SUI::Weight::Regular, SUI::Colours::text, juce::Justification::topLeft);
    styleLabel (notesLabel, {}, SUI::Type::hint, SUI::Weight::Regular, dim, juce::Justification::topLeft);
    styleLabel (hintLabel, "While measuring, the Before Plugin replaces the sound with the test sweep. At other times both leave the sound untouched.",
                SUI::Type::hint, SUI::Weight::Regular, dim);

    roleBox.addItemList (roleNames(), 1);
    pairBox.addItemList (pairNames(), 1);
    channelBox.addItemList (channelNames(), 1);
    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, roleId, roleBox);
    pairAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, sessionId, pairBox);
    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, channelId, channelBox);
    rangeBox.addItem ("Up and down 6 dB", 1);
    rangeBox.addItem ("Up and down 12 dB", 2);
    rangeBox.addItem ("Up and down 24 dB", 3);
    rangeBox.setSelectedId (2, juce::dontSendNotification);
    rangeBox.onChange = [this]
    {
        const float ranges[] = { 6.0f, 12.0f, 24.0f };
        graph.setRangeDb (ranges[juce::jlimit (0, 2, rangeBox.getSelectedId() - 1)]);
    };

    levelSlider.setTextValueSuffix (" dB");
    levelSlider.setPopupDisplayEnabled (false, false, nullptr);

    roleBox.setTooltip ("Role: put a Before Plugin before the plugin you want to measure, and an After Plugin after it. The two find each other on their own. "
                        "The Before Plugin sends the test sweep; the After Plugin records what arrives and draws the curve. "
                        "While measuring, the Before Plugin replaces the sound with the sweep.");
    pairBox.setTooltip ("Pair With: shown only when the plugins cannot tell which Before Plugin and After Plugin belong together, for example when a "
                        "project has several pairs. Choose the same pair on both plugins. This only works when the host loads both in the same program.");
    channelBox.setTooltip ("Channel: which channel the After Plugin measures. Choose Both Combined to measure the left and right channels added together. "
                           "Only used by the After Plugin.");
    levelSlider.setTooltip ("Test Level: how loud the test sweep is. Quieter is safer for the plugin you are measuring; louder keeps noise out of the curve. "
                            "Set on the Before Plugin, which sends the sweep. Double-click for the default.");
    startButton.setTooltip ("Start: make a capture. The host must be playing. It takes about a quarter of a minute: two sweeps, then the end of each. "
                            "If you already have a capture, you choose whether to replace it or add a second one to compare. Press it on the After Plugin.");
    stopButton.setTooltip ("Stop: end the measurement early. Nothing is kept, and your captures are not changed.");
    clearButton.setTooltip ("Clear All: remove both captures from this plugin. It asks first.");
    exportButton.setTooltip ("Export CSV: save the captures as a table you can open in a spreadsheet. "
                             "Each column is a capture in decibels, with the frequency in the first column.");
    rangeBox.setTooltip ("Range: how many decibels the graph shows above and below 0 dB.");
    statusLabel.setTooltip ({});
    graph.setTooltip ("The measured gain of the plugin at each frequency, from 20 Hz to 20 kHz. Above 0 dB the plugin turns that frequency up; below it, down. "
                      "Move the pointer over the graph to read a value. The first capture is a solid blue line and the second a dashed orange one.");

    startButton.onClick = [this] { pressStart(); };
    stopButton.onClick = [this] { proc.requestStop(); refresh(); };
    clearButton.onClick = [this] { openClearQuestion(); };
    exportButton.onClick = [this] { exportCsv(); };

    const ::juce::Colour colours[] = { PluginAnalyzer::UI::Colours::first, PluginAnalyzer::UI::Colours::second };
    for (int i = 0; i < numSlots; ++i)
    {
        captureRows[(size_t) i] = std::make_unique<PluginAnalyzer::UI::CaptureRow> (i, colours[i], i == 1);
        captureRows[(size_t) i]->onVisibleChange = [this, i] (bool shown) { proc.captureVisible[i] = shown; refresh(); };
        captureRows[(size_t) i]->onNameChange = [this, i] (const juce::String& text)
        {
            const auto name = text.trim();
            proc.getSlot (i).name = name.isEmpty() ? defaultSlotName (i) : name;
            refresh();
        };
    }
    choicePanel.onChoice = [this] (int id, const juce::String& name, bool edited) { applyChoice (id, name, edited); };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: nothing is measured, and the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    SUI::styleBypassButton (bypassButton);

    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after the Analyzer. While the Before Plugin is sending the sweep this shows the sweep; the After Plugin "
                                        "shows what arrives. The number under the warning is the highest sample level held for 1.5 seconds. This is sample peak, not true peak. "
                                        "If the clip warning lights during a measurement, lower Test Level. Click to clear the held number.");

    for (auto* l : { &roleLabel, &pairLabel, &channelLabel, &levelLabel, &levelValue, &rangeLabel, &notesTitle, &statusLabel, &notesLabel, &hintLabel })
        content.addAndMakeVisible (*l);
    for (auto* b : { &roleBox, &pairBox, &channelBox, &rangeBox })
        content.addAndMakeVisible (*b);
    for (auto* b : { &startButton, &stopButton, &clearButton, &exportButton, &bypassButton })
        content.addAndMakeVisible (*b);
    content.addAndMakeVisible (levelSlider);
    content.addAndMakeVisible (progressBar);
    content.addAndMakeVisible (graph);
    for (auto& row : captureRows)
        content.addAndMakeVisible (*row);
    content.addAndMakeVisible (meterColumn);
    content.addChildComponent (choicePanel);   // last, so it sits over the graph when it is open
    progressBar.setTitle ("Progress");
    progressBar.setPercentageDisplay (false);

    roleBox.setTitle ("Role");
    pairBox.setTitle ("Pair With");
    channelBox.setTitle ("Channel");
    levelSlider.setTitle ("Test Level");
    rangeBox.setTitle ("Range");
    statusLabel.setTitle ("Status");
    notesLabel.setTitle ("Notes about the captures");

    // Keyboard order follows the layout: the settings, the level, the buttons, then the captures.
    int order = 1;
    for (auto* c : std::initializer_list<juce::Component*> { &roleBox, &pairBox, &channelBox, &levelSlider, &startButton, &stopButton, &clearButton,
                                                             &captureRows[0]->getVisibleToggle(), &captureRows[0]->getNameEditor(),
                                                             &captureRows[1]->getVisibleToggle(), &captureRows[1]->getNameEditor(),
                                                             &rangeBox, &exportButton, &bypassButton })
        c->setExplicitFocusOrder (order++);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);
    content.addAndMakeVisible (titleHint);

    layoutContent();

    const int savedWidth = proc.editorWidth.load();
    setResizable (true, true);
    setResizeLimits (EditorLayout::minWidth, EditorLayout::heightFor (EditorLayout::minWidth),
                     EditorLayout::maxWidth, EditorLayout::heightFor (EditorLayout::maxWidth));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) EditorLayout::designWidth / (double) EditorLayout::designHeight);
    const int width = EditorLayout::constrainWidth (savedWidth);
    setSize (width, EditorLayout::heightFor (width));
    constructed = true;
    syncSizeFromWidth();
    refresh();
    startTimerHz (30);
}

SongsmithPluginAnalyzerEditor::~SongsmithPluginAnalyzerEditor()
{
    stopTimer();
    diagnostics.reset();
    setLookAndFeel (nullptr);
}

void SongsmithPluginAnalyzerEditor::syncSizeFromWidth()
{
    if (constructed)
        proc.editorWidth.store (getWidth());
}

void SongsmithPluginAnalyzerEditor::resized()
{
    content.setBounds (0, 0, designWidth, EditorLayout::designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth, (float) getHeight() / (float) EditorLayout::designHeight));
    syncSizeFromWidth();
}

void SongsmithPluginAnalyzerEditor::layoutContent()
{
    constexpr int left = 24, leftWidth = 268, top = 70;

    roleLabel.setBounds (left, top, leftWidth, 16);          roleBox.setBounds (left, top + 18, leftWidth, 28);
    channelLabel.setBounds (left, top + 54, leftWidth, 16);  channelBox.setBounds (left, top + 72, leftWidth, 28);
    levelLabel.setBounds (left, top + 108, 140, 16);         levelValue.setBounds (left + 140, top + 108, leftWidth - 140, 16);
    levelSlider.setBounds (left, top + 128, leftWidth, 22);

    startButton.setBounds (left, top + 166, 84, 32);
    stopButton.setBounds (left + 92, top + 166, 84, 32);
    clearButton.setBounds (left + 184, top + 166, 84, 32);

    statusLabel.setBounds (left, top + 210, leftWidth, 62);
    progressBar.setBounds (left, top + 280, leftWidth, 14);                        // shown while measuring...
    pairLabel.setBounds (left, top + 280, leftWidth, 16);                          // ...and the Pair With box takes its place when it is needed
    pairBox.setBounds (left, top + 298, leftWidth, 28);
    notesTitle.setBounds (left, top + 336, leftWidth, 16);
    notesLabel.setBounds (left, top + 356, leftWidth, 130);

    const int graphLeft = 312, edge = SUI::MeterColumn::contentRightEdge (designWidth, false), rightWidth = edge - graphLeft;
    graph.setBounds (graphLeft, top, rightWidth, 330);
    choicePanel.setBounds (graph.getBounds());

    const int rowOne = top + 338;
    captureRows[0]->setBounds (graphLeft, rowOne, rightWidth, 28);
    captureRows[1]->setBounds (graphLeft, rowOne + 34, rightWidth, 28);
    const int rowThree = rowOne + 74;
    rangeLabel.setBounds (graphLeft, rowThree + 6, 56, 16);
    rangeBox.setBounds (graphLeft + 60, rowThree, 190, 28);
    exportButton.setBounds (edge - 140, rowThree, 140, 28);
    hintLabel.setBounds (graphLeft, rowThree + 36, rightWidth, 16);

    meterColumn.setBounds (SUI::MeterColumn::boundsFor (designWidth, (int) SUI::titleBarHeight + 8, contentHeight - 12, false));

    const auto header = SUI::Header::layout (SUI::Header::rowIn ((int) SUI::titleInset, designWidth - 16 - (int) SUI::titleInset, (int) SUI::titleBarHeight), JucePlugin_Name);
    titleBounds = header.title;
    titleHint.setBounds (header.title);
    bypassButton.setBounds (header.bypass);

    diagnostics->setBounds (0, contentHeight, designWidth, footerHeight);
}

void SongsmithPluginAnalyzerEditor::paint (juce::Graphics& g) { g.fillAll (SUI::Colours::panel); }

void SongsmithPluginAnalyzerEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) EditorLayout::designHeight);
    SUI::LookAndFeelBase::drawPanel (g, r);
    SUI::LookAndFeelBase::drawTitleBar (g, r.withHeight (SUI::titleBarHeight));
    SUI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, SUI::Colours::text, PluginAnalyzer::UI::Colours::accent);
}

//==============================================================================
bool SongsmithPluginAnalyzerEditor::startNeedsChoice() const
{
    return proc.getRole() == Role::result && proc.numFilledCaptures() > 0;
}

void SongsmithPluginAnalyzerEditor::pressStart()
{
    if (choicePanel.isOpen() || proc.isBypassed())
        return;
    if (startNeedsChoice())
    {
        openChoice();
        return;
    }
    // Nothing to replace: the first capture, or (when the first is empty) the first empty one.
    startMeasurementInto (juce::jmax (0, proc.firstEmptyCapture()), {});
}

void SongsmithPluginAnalyzerEditor::startMeasurementInto (int captureIndex, const juce::String& name)
{
    proc.setNextCapture (captureIndex, name);
    proc.requestStart();
    refresh();
}

void SongsmithPluginAnalyzerEditor::openChoice()
{
    using Option = PluginAnalyzer::UI::ChoicePanel::Option;
    const int filled = proc.numFilledCaptures();
    std::vector<Option> options;
    if (filled == 1)
    {
        int existing = 0;
        for (int i = 0; i < numSlots; ++i)
            if (proc.getSlot (i).filled)
                existing = i;
        pendingAddSlot = proc.firstEmptyCapture();
        options.push_back ({ "Replace \"" + proc.getSlot (existing).name + "\"", replaceFirstId + existing });
        options.push_back ({ "Add to Compare", addSecondId });
        options.push_back ({ "Cancel", PluginAnalyzer::UI::ChoicePanel::cancelId });
        choicePanel.open ("Start a new measurement",
                          "You already have a capture. Replace it with the new measurement, or add the new one as a second capture so both are drawn together. "
                          "Nothing changes until the measurement is finished.",
                          options, true, defaultSlotName (juce::jmax (0, pendingAddSlot)));
    }
    else
    {
        pendingAddSlot = -1;
        for (int i = 0; i < numSlots; ++i)
            options.push_back ({ "Replace \"" + proc.getSlot (i).name + "\"", replaceFirstId + i });
        options.push_back ({ "Cancel", PluginAnalyzer::UI::ChoicePanel::cancelId });
        choicePanel.open ("Start a new measurement",
                          "You have two captures. Choose the one to replace with the new measurement. The other one is kept. "
                          "Nothing changes until the measurement is finished.",
                          options, false, {});
    }
}

void SongsmithPluginAnalyzerEditor::openClearQuestion()
{
    using Option = PluginAnalyzer::UI::ChoicePanel::Option;
    choicePanel.open ("Clear all captures?",
                      "This removes every capture from this plugin, with its name. Export a CSV first if you want to keep them.",
                      { Option { "Clear All Captures", clearAllId }, Option { "Keep Captures", PluginAnalyzer::UI::ChoicePanel::cancelId } }, false, {});
}

void SongsmithPluginAnalyzerEditor::applyChoice (int id, const juce::String& typedName, bool nameEdited)
{
    if (id == PluginAnalyzer::UI::ChoicePanel::cancelId)
    {
        refresh();
        return;
    }
    if (id == clearAllId)
    {
        proc.clearCaptures();
        refresh();
        return;
    }
    if (id == addSecondId)
    {
        startMeasurementInto (juce::jmax (0, pendingAddSlot), typedName);
        return;
    }
    if (id == replaceFirstId || id == replaceSecondId)
        startMeasurementInto (id - replaceFirstId, nameEdited ? typedName : juce::String());   // a replaced capture keeps its name unless you typed one
}

void SongsmithPluginAnalyzerEditor::refresh()
{
    const bool changedByProcessor = proc.poll();
    juce::ignoreUnused (changedByProcessor);
    const bool isAfter = proc.getRole() == Role::result;

    // The words and the bar.
    const auto info = proc.getStatus();
    statusLabel.setText (info.text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, info.status == SongsmithPluginAnalyzerProcessor::Status::problem ? PluginAnalyzer::UI::Colours::compare
                                                                                                                        : SUI::Colours::text);
    progress = (double) info.progress;
    const bool showProgress = info.status == SongsmithPluginAnalyzerProcessor::Status::sending || info.status == SongsmithPluginAnalyzerProcessor::Status::recording;
    const bool offersPair = proc.offersPairing();
    progressBar.setVisible (showProgress && ! offersPair);
    pairLabel.setVisible (offersPair);
    pairBox.setVisible (offersPair);

    const bool busy = showProgress || info.status == SongsmithPluginAnalyzerProcessor::Status::working;
    const bool choiceOpen = choicePanel.isOpen();
    if (choiceOpen && ! isAfter)
        choicePanel.close();   // the role changed under it
    startButton.setVisible (isAfter);
    clearButton.setVisible (isAfter);
    startButton.setEnabled (! busy && ! proc.isBypassed() && ! choiceOpen);
    stopButton.setEnabled (busy);
    clearButton.setEnabled (! busy && ! choiceOpen && proc.numFilledCaptures() > 0);

    // What applies to which role.
    levelSlider.setEnabled (! isAfter);
    levelLabel.setEnabled (! isAfter);
    channelBox.setEnabled (isAfter);
    for (auto* c : std::initializer_list<juce::Component*> { &rangeLabel, &rangeBox, &exportButton, &notesTitle, &notesLabel })
        c->setVisible (isAfter);
    for (auto& row : captureRows)
        row->setVisible (isAfter);
    exportButton.setEnabled (isAfter && proc.makeCsvText().isNotEmpty());
    levelValue.setText (juce::String (proc.getTestLevelDb(), 1) + " dB", juce::dontSendNotification);

    // The two captures: their names, whether each is drawn, and a short line about each.
    juce::String notes = proc.getKeptNotice();
    PluginAnalyzer::UI::ResponseGraph::Curve curves[numSlots];
    const ::juce::Colour colours[] = { PluginAnalyzer::UI::Colours::first, PluginAnalyzer::UI::Colours::second };
    int filled = 0, drawn = 0;
    for (int i = 0; i < numSlots; ++i)
    {
        const auto& slot = proc.getSlot (i);
        const auto detail = slot.channel + (slot.measurement.isValidCurve() ? juce::String() : juce::String (", no curve"));
        captureRows[(size_t) i]->setCapture (slot.filled, slot.name, detail, proc.captureVisible[i]);
        if (! slot.filled)
            continue;
        ++filled;
        if (proc.captureVisible[i])
        {
            curves[i] = { &slot.measurement, slot.name, colours[i], i == 1 };
            ++drawn;
        }
        const auto flagNotes = describeFlags (slot.measurement);
        juce::String line = "\"" + slot.name + "\": ";
        if (! flagNotes.isEmpty())
            line << flagNotes.joinIntoString (" ");
        else if (slot.measurement.sampleRate > 0.0)
            line << "Nothing to flag. Delay between the Before Plugin and the After Plugin: "
                 << juce::String (slot.measurement.delaySamples / slot.measurement.sampleRate * 1000.0, 1) << " ms.";
        notes << (notes.isEmpty() ? "" : "\n\n") << line;
    }

    // The graph, and the notes under the controls.
    juce::String message;
    if (! isAfter)
        message = "This is the Before Plugin. It sends the test sweeps. Put an After Plugin after the plugin you want to measure, and press Start there.";
    else if (filled == 0)
        message = "No capture yet. Start the host playing, then press Start.";
    else if (drawn == 0)
        message = "Both captures are hidden. Turn on Show under the graph to see one.";
    else
        for (int i = 0; i < numSlots; ++i)
            if (curves[i].measurement != nullptr && ! curves[i].measurement->isValidCurve() && message.isEmpty() && drawn == 1)
            {
                const auto flagNotes = describeFlags (*curves[i].measurement);
                message = flagNotes.isEmpty() ? juce::String ("No curve could be measured.") : flagNotes[0];
            }
    graph.setCurves (curves[0], curves[1]);
    graph.setMessage (message);
    notesLabel.setText (notes.isEmpty() ? juce::String ("None yet.") : notes, juce::dontSendNotification);

    // The Output meter.
    {
        auto& m = proc.outputMeter;
        m.readAndReset();
        m.clipped.store (false, std::memory_order_relaxed);
        meterColumn.update (m.readAndResetChannel (0), m.readAndResetChannel (1), m.channels.load (std::memory_order_relaxed),
                            m.clipLatched.load (std::memory_order_relaxed), 0.0f, juce::Time::getMillisecondCounterHiRes() * 0.001, 1.0 / 30.0);
    }
}

void SongsmithPluginAnalyzerEditor::exportCsv()
{
    const auto text = proc.makeCsvText();
    if (text.isEmpty() || chooser != nullptr)
        return;
    int firstFilled = 0;
    for (int i = numSlots - 1; i >= 0; --i)
        if (proc.getSlot (i).filled)
            firstFilled = i;
    const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile (juce::File::createLegalFileName (proc.getSlot (firstFilled).name) + ".csv");
    chooser = std::make_unique<juce::FileChooser> ("Export CSV", suggested, "*.csv");
    juce::Component::SafePointer<SongsmithPluginAnalyzerEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, text] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;
                              const auto file = fc.getResult();
                              safe->chooser.reset();
                              if (file == juce::File())
                                  return;
                              if (! file.withFileExtension ("csv").replaceWithText (text))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export CSV",
                                                                          "The file could not be saved. Check that you can write to that folder, and try another place.");
                          });
}
