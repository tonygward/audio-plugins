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
      roleAttachment (p.apvts, roleId, roleBox),
      sessionAttachment (p.apvts, sessionId, sessionBox),
      channelAttachment (p.apvts, channelId, channelBox),
      levelAttachment (p.apvts, levelId, levelSlider),
      bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    const auto dim = SUI::Colours::textDim;
    styleLabel (roleLabel, "Role", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (sessionLabel, "Session", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (channelLabel, "Channel", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (levelLabel, "Test Level", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (levelValue, {}, SUI::Type::control, SUI::Weight::Regular, SUI::Colours::text, juce::Justification::centredRight);
    styleLabel (measureIntoLabel, "Measure Into", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (showLabel, "Show", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (compareLabel, "Compare With", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (rangeLabel, "Range", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (nameLabel, "Name for the Next Measurement", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (notesTitle, "Notes About This Measurement", SUI::Type::control, SUI::Weight::Medium, dim);
    styleLabel (statusLabel, {}, SUI::Type::control, SUI::Weight::Regular, SUI::Colours::text, juce::Justification::topLeft);
    styleLabel (notesLabel, {}, SUI::Type::hint, SUI::Weight::Regular, dim, juce::Justification::topLeft);
    styleLabel (hintLabel, "While measuring, the Reference replaces the sound with the test sweep. At other times both leave the sound untouched.",
                SUI::Type::hint, SUI::Weight::Regular, dim);

    roleBox.addItemList (roleNames(), 1);
    sessionBox.addItemList (sessionNames(), 1);
    channelBox.addItemList (channelNames(), 1);
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

    roleBox.setTooltip ("Role: put a Reference before the plugin you want to measure, and a Result after it. The Reference sends the test sweep; the Result "
                        "records what arrives and draws the curve. While measuring, the Reference replaces the sound with the sweep.");
    sessionBox.setTooltip ("Session: a Reference and a Result with the same session belong together. Use a different session for each pair, so two "
                           "measurements in one project never mix. This only works when the host loads both in the same program.");
    channelBox.setTooltip ("Channel: which channel the Result measures. Choose Both Combined to measure the left and right channels added together. "
                           "Only used by the Result.");
    levelSlider.setTooltip ("Test Level: how loud the test sweep is. Quieter is safer for the plugin you are measuring; louder keeps noise out of the curve. "
                            "Only used by the Reference. Double-click for the default.");
    startButton.setTooltip ("Start: begin a measurement. The host must be playing. It takes about a quarter of a minute: two sweeps, then the end of each.");
    stopButton.setTooltip ("Stop: end the measurement early. Nothing is kept.");
    resetButton.setTooltip ("Reset: stop, and empty the measurement that the next one would go into.");
    exportButton.setTooltip ("Export CSV: save the curve you are looking at, and the one you are comparing with, as a table you can open in a spreadsheet. "
                             "Each column is a measurement in decibels, with the frequency in the first column.");
    measureIntoBox.setTooltip ("Measure Into: which of the four measurement slots the next measurement fills. Choose a different slot to keep the one you have.");
    showBox.setTooltip ("Show: which measurement is drawn on the graph.");
    compareBox.setTooltip ("Compare With: draw a second measurement over it, for example a hardware unit against a Songsmith plugin. Choose None to hide it.");
    rangeBox.setTooltip ("Range: how many decibels the graph shows above and below 0 dB.");
    nameEditor.setTooltip ("Name for the next measurement: it appears on the graph and in the CSV file, so you can tell two measurements apart.");
    statusLabel.setTooltip ({});
    graph.setTooltip ("The measured gain of the plugin at each frequency, from 20 Hz to 20 kHz. Above 0 dB the plugin turns that frequency up; below it, down. "
                      "Move the pointer over the graph to read a value.");

    nameEditor.setMultiLine (false);
    nameEditor.setInputRestrictions (40);
    nameEditor.setFont (SUI::font (SUI::Type::control));
    nameEditor.setTitle ("Name for the next measurement");
    nameEditor.onTextChange = [this]
    {
        auto name = nameEditor.getText().trim();
        proc.getSlot (proc.measureSlot).name = name.isEmpty() ? defaultSlotName (proc.measureSlot) : name;
        rebuildSlotBoxes();
    };
    measureIntoBox.onChange = [this]
    {
        proc.measureSlot = juce::jlimit (0, numSlots - 1, measureIntoBox.getSelectedId() - 1);
        nameEditor.setText (proc.getSlot (proc.measureSlot).name, juce::dontSendNotification);
    };
    showBox.onChange = [this] { proc.shownSlot = juce::jlimit (0, numSlots - 1, showBox.getSelectedId() - 1); refresh(); };
    compareBox.onChange = [this] { proc.comparedSlot = compareBox.getSelectedId() - 2; refresh(); };

    startButton.onClick = [this] { proc.requestStart(); refresh(); };
    stopButton.onClick = [this] { proc.requestStop(); refresh(); };
    resetButton.onClick = [this] { proc.resetMeasurement(); rebuildSlotBoxes(); refresh(); };
    exportButton.onClick = [this] { exportCsv(); };

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: nothing is measured, and the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    SUI::styleBypassButton (bypassButton);

    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after the Analyzer. While the Reference is sending the sweep this shows the sweep; the Result "
                                        "shows what arrives. The number under the warning is the highest sample level held for 1.5 seconds. This is sample peak, not true peak. "
                                        "If the clip warning lights during a measurement, lower Test Level. Click to clear the held number.");

    for (auto* l : { &roleLabel, &sessionLabel, &channelLabel, &levelLabel, &levelValue, &measureIntoLabel, &showLabel, &compareLabel, &rangeLabel, &nameLabel,
                     &notesTitle, &statusLabel, &notesLabel, &hintLabel })
        content.addAndMakeVisible (*l);
    for (auto* b : { &roleBox, &sessionBox, &channelBox, &measureIntoBox, &showBox, &compareBox, &rangeBox })
        content.addAndMakeVisible (*b);
    for (auto* b : { &startButton, &stopButton, &resetButton, &exportButton, &bypassButton })
        content.addAndMakeVisible (*b);
    content.addAndMakeVisible (levelSlider);
    content.addAndMakeVisible (nameEditor);
    content.addAndMakeVisible (progressBar);
    content.addAndMakeVisible (graph);
    content.addAndMakeVisible (meterColumn);
    progressBar.setTitle ("Progress");
    progressBar.setPercentageDisplay (false);

    roleBox.setTitle ("Role");
    sessionBox.setTitle ("Session");
    channelBox.setTitle ("Channel");
    levelSlider.setTitle ("Test Level");
    measureIntoBox.setTitle ("Measure Into");
    showBox.setTitle ("Show");
    compareBox.setTitle ("Compare With");
    rangeBox.setTitle ("Range");
    statusLabel.setTitle ("Status");
    notesLabel.setTitle ("Notes about this measurement");

    // Keyboard order follows the layout: the three settings, the level, the buttons, then the measurement boxes.
    int order = 1;
    for (auto* c : std::initializer_list<juce::Component*> { &roleBox, &sessionBox, &channelBox, &levelSlider, &startButton, &stopButton, &resetButton,
                                                             &measureIntoBox, &showBox, &compareBox, &rangeBox, &nameEditor, &exportButton, &bypassButton })
        c->setExplicitFocusOrder (order++);

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);
    content.addAndMakeVisible (titleHint);

    rebuildSlotBoxes();
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
    using juce::Rectangle;
    constexpr int left = 24, leftWidth = 268, top = 70;

    roleLabel.setBounds (left, top, leftWidth, 16);        roleBox.setBounds (left, top + 18, leftWidth, 28);
    sessionLabel.setBounds (left, top + 54, leftWidth, 16); sessionBox.setBounds (left, top + 72, leftWidth, 28);
    channelLabel.setBounds (left, top + 108, leftWidth, 16); channelBox.setBounds (left, top + 126, leftWidth, 28);
    levelLabel.setBounds (left, top + 162, 140, 16);        levelValue.setBounds (left + 140, top + 162, leftWidth - 140, 16);
    levelSlider.setBounds (left, top + 182, leftWidth, 22);

    startButton.setBounds (left, top + 222, 84, 32);
    stopButton.setBounds (left + 92, top + 222, 84, 32);
    resetButton.setBounds (left + 184, top + 222, 84, 32);

    statusLabel.setBounds (left, top + 268, leftWidth, 62);
    progressBar.setBounds (left, top + 334, leftWidth, 14);
    notesTitle.setBounds (left, top + 362, leftWidth, 16);
    notesLabel.setBounds (left, top + 382, leftWidth, 106);

    const int graphLeft = 312, edge = SUI::MeterColumn::contentRightEdge (designWidth, false);
    graph.setBounds (graphLeft, top, edge - graphLeft, 350);

    const int rowOne = top + 362, rowTwo = rowOne + 58, boxWidth = 146, gap = 12;
    for (int i = 0; i < 4; ++i)
    {
        juce::Label* labels[] = { &measureIntoLabel, &showLabel, &compareLabel, &rangeLabel };
        juce::ComboBox* boxes[] = { &measureIntoBox, &showBox, &compareBox, &rangeBox };
        labels[i]->setBounds (graphLeft + i * (boxWidth + gap), rowOne, boxWidth, 16);
        boxes[i]->setBounds (graphLeft + i * (boxWidth + gap), rowOne + 18, boxWidth, 28);
    }
    nameLabel.setBounds (graphLeft, rowTwo, 300, 16);
    nameEditor.setBounds (graphLeft, rowTwo + 18, 300, 28);
    exportButton.setBounds (edge - 140, rowTwo + 18, 140, 28);
    hintLabel.setBounds (graphLeft, rowTwo + 50, edge - graphLeft, 16);

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
void SongsmithPluginAnalyzerEditor::rebuildSlotBoxes()
{
    for (int i = 0; i < numSlots; ++i)
        shownNames[i] = proc.getSlot (i).name + (proc.getSlot (i).filled ? "" : " (empty)");

    auto fill = [this] (juce::ComboBox& box, bool withNone, int selected)
    {
        box.clear (juce::dontSendNotification);
        if (withNone)
            box.addItem ("None", 1);
        for (int i = 0; i < numSlots; ++i)
            box.addItem (shownNames[i], i + 1 + (withNone ? 1 : 0));
        box.setSelectedId (selected, juce::dontSendNotification);
    };
    fill (measureIntoBox, false, proc.measureSlot + 1);
    fill (showBox, false, proc.shownSlot + 1);
    fill (compareBox, true, proc.comparedSlot + 2);
    if (! nameEditor.hasKeyboardFocus (false))
        nameEditor.setText (proc.getSlot (proc.measureSlot).name, juce::dontSendNotification);
}

void SongsmithPluginAnalyzerEditor::refresh()
{
    const bool changedByProcessor = proc.poll();
    const bool isResult = proc.getRole() == Role::result;

    // The words and the bar.
    const auto info = proc.getStatus();
    statusLabel.setText (info.text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, info.status == SongsmithPluginAnalyzerProcessor::Status::problem ? PluginAnalyzer::UI::Colours::compare
                                                                                                                        : SUI::Colours::text);
    progress = (double) info.progress;
    const bool showProgress = info.status == SongsmithPluginAnalyzerProcessor::Status::sending || info.status == SongsmithPluginAnalyzerProcessor::Status::recording;
    progressBar.setVisible (showProgress);

    const bool busy = showProgress || info.status == SongsmithPluginAnalyzerProcessor::Status::working;
    startButton.setEnabled (! busy && ! proc.isBypassed());
    stopButton.setEnabled (busy);

    // What applies to which role.
    levelSlider.setEnabled (! isResult);
    levelLabel.setEnabled (! isResult);
    channelBox.setEnabled (isResult);
    for (auto* c : std::initializer_list<juce::Component*> { &measureIntoBox, &showBox, &compareBox, &rangeBox, &nameEditor })
        c->setEnabled (isResult);
    exportButton.setEnabled (isResult && proc.makeCsvText().isNotEmpty());
    levelValue.setText (juce::String (proc.getTestLevelDb(), 1) + " dB", juce::dontSendNotification);

    // Slot names and fill state may have changed (a measurement finished, Reset, a restored session).
    bool namesChanged = false;
    for (int i = 0; i < numSlots; ++i)
        namesChanged = namesChanged || shownNames[i] != proc.getSlot (i).name + (proc.getSlot (i).filled ? "" : " (empty)");
    if (namesChanged || changedByProcessor)
        rebuildSlotBoxes();
    if (showBox.getSelectedId() != proc.shownSlot + 1)
        showBox.setSelectedId (proc.shownSlot + 1, juce::dontSendNotification);
    if (compareBox.getSelectedId() != proc.comparedSlot + 2)
        compareBox.setSelectedId (proc.comparedSlot + 2, juce::dontSendNotification);

    // The graph, and the notes under the controls.
    const auto& shown = proc.getSlot (proc.shownSlot);
    PluginAnalyzer::UI::ResponseGraph::Curve first, second;
    juce::String message;
    juce::String notes;
    if (! isResult)
    {
        message = "This is the Reference. It sends the test sweeps. Put a Result after the plugin you want to measure, in the same session, to see its curve there.";
    }
    else
    {
        if (shown.filled)
        {
            first = { &shown.measurement, shown.name, PluginAnalyzer::UI::Colours::accent };
            const auto flagNotes = describeFlags (shown.measurement);
            notes = flagNotes.joinIntoString ("\n\n");
            if (! shown.measurement.isValidCurve())
                message = flagNotes.isEmpty() ? juce::String ("No curve could be measured.") : flagNotes[0];
            else if (notes.isEmpty())
                notes = "Nothing to flag. Delay between the Reference and the Result: " + juce::String (shown.measurement.delaySamples / shown.measurement.sampleRate * 1000.0, 1) + " ms.";
        }
        else
            message = "No measurement yet. Start the host playing, then press Start.";

        const auto& other = proc.getSlot (juce::jmax (0, proc.comparedSlot));
        if (proc.comparedSlot >= 0 && proc.comparedSlot != proc.shownSlot && other.filled)
            second = { &other.measurement, other.name, PluginAnalyzer::UI::Colours::compare };
    }
    graph.setCurves (first, second);
    graph.setMessage (message);
    notesLabel.setText (notes.isEmpty() ? (isResult ? juce::String ("None yet.") : juce::String()) : notes, juce::dontSendNotification);

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
    const auto suggested = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                               .getChildFile (juce::File::createLegalFileName (proc.getSlot (proc.shownSlot).name) + ".csv");
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
