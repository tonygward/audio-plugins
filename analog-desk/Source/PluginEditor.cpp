#include "PluginEditor.h"
#include "../../shared/Source/SongsmithGrain.h"

using namespace Desk;
using namespace Desk::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;
    constexpr int designHeight = EditorLayout::designHeight;
    constexpr int leftEdge = EditorLayout::leftMargin, cellWidth = EditorLayout::cellWidth;

    // Rows, in design units: a heading line, then (for the equaliser and the dynamics) a line of group captions, then the knobs.
    constexpr int headingY[3] = { 56, 194, 346 };
    constexpr int captionY[3] = { 0, 216, 368 };
    constexpr int knobY[3] = { 78, 238, 390 };
    constexpr int knobH[3] = { 108, 100, 100 };
    constexpr int lineHeight = 22;

    juce::String signedDb (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String plainDb (float v) { return juce::String (v, 1) + " dB"; }
    juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
    juce::String msText (float v) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }
    juce::String secondsText (float v) { return juce::String (v, 2) + " s"; }

    juce::Rectangle<int> cellBounds (int row, int cell) { return { leftEdge + cell * cellWidth, knobY[row], cellWidth, knobH[row] }; }

    // The faceplate's fine brushed-metal grain (shared with the other plugins). Built once; the same grain every time.
    struct Grain : Songsmith::UI::BrushedGrain
    {
        Grain() : BrushedGrain (juce::Rectangle<float>::leftTopRightBottom (10.0f, (float) Songsmith::UI::titleBarHeight + 2.0f,
                                                                         (float) EditorLayout::controlsRight + 8.0f, (float) contentHeight - 6.0f)) {}
    };
}

//==============================================================================
DeskKnob::DeskKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& name, const juce::String& title,
                    Formatter format)
    : shownName (name), formatter (std::move (format)), attachment (state, paramId, slider)
{
    auto* p = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                                juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
    slider.setMouseDragSensitivity (220);
    slider.setTitle (title);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void DeskKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    r.removeFromBottom (22);
    const int side = juce::jmin (r.getWidth() - 20, r.getHeight());
    slider.setBounds (juce::Rectangle<int> (side, side).withCentre (r.getCentre()));
}

void DeskKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::text);
    g.setFont (font (Type::control, Weight::SemiBold));
    g.drawText (shownName, r.removeFromTop (20.0f), juce::Justification::centred);

    g.setFont (numbers (Type::control));
    g.drawText (getValueText(), r.removeFromBottom (22.0f), juce::Justification::centred);
}

//==============================================================================
juce::StringArray SongsmithAnalogDeskEditor::paintedTexts()
{
    return { "Input and Filters", "Output", "Equaliser", "Low", "Low-Mid", "High-Mid", "High", "Dynamics", "Compressor", "Gate" };
}

DeskKnob& SongsmithAnalogDeskEditor::addKnob (const char* paramId, const juce::String& name, const juce::String& title, const juce::String& tip,
                                              DeskKnob::Formatter format, int row, int cell)
{
    knobs.emplace_back (proc.apvts, paramId, name, title, std::move (format));
    auto& k = knobs.back();
    k.slider.setTooltip (tip + " Double-click for the default.");
    k.setBounds (cellBounds (row, cell));
    content.addAndMakeVisible (k);
    knobIds.push_back ({ paramId, (int) knobs.size() - 1 });
    return k;
}

DeskToggle& SongsmithAnalogDeskEditor::addToggle (const char* paramId, const juce::String& text, const juce::String& title, const juce::String& tip,
                                                  juce::Rectangle<int> bounds)
{
    toggles.emplace_back (proc.apvts, paramId, text, title);
    auto& t = toggles.back();
    t.button.setTooltip (tip);
    t.button.setBounds (bounds);
    content.addAndMakeVisible (t.button);
    return t;
}

SongsmithAnalogDeskEditor::SongsmithAnalogDeskEditor (SongsmithAnalogDeskProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    auto db = [] (float v) { return signedDb (v); };
    auto hz = [] (float v) { return hzText (v); };

    // Input, drive and filters
    addKnob (inputTrimId, "Input Trim", "Input Trim", "Input Trim: the level going into the strip, before anything else. Turn it down if the sound is clipping or the Drive is too strong.", db, 0, 0);
    addKnob (driveId, "Drive", "Drive", "Drive: adds a soft, warm edge by gently rounding off the peaks. Zero leaves the sound exactly as it is.",
            [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }, 0, 1);
    addKnob (highPassId, "High-Pass", "High-Pass Filter", "High-Pass Filter: removes the low sounds below this frequency, such as rumble. All the way down is off.",
            [] (float v) { return v < highPassOffBelowHz ? juce::String ("Off") : hzText (v); }, 0, 2);
    addKnob (lowPassId, "Low-Pass", "Low-Pass Filter", "Low-Pass Filter: removes the high sounds above this frequency, such as hiss. All the way up is off.",
            [] (float v) { return v > lowPassOffAboveHz ? juce::String ("Off") : hzText (v); }, 0, 3);
    addKnob (outputTrimId, "Output Trim", "Output Trim", "Output Trim: the level leaving the strip, after everything else.", db, 0, 9);

    // Equaliser
    addToggle (eqOnId, "Equaliser On", "Equaliser On", "Equaliser On: switches the four equaliser bands on and off together. The filters and dynamics are not affected.", { 130, headingY[1], 110, lineHeight });
    addKnob (lfFreqId, "Frequency", "Low Frequency", "Low Frequency: the pitch the low band works on.", hz, 1, 0);
    addKnob (lfGainId, "Gain", "Low Gain", "Low Gain: how much the low band lifts or lowers the sound. Zero changes nothing.", db, 1, 1);
    addToggle (lfBellId, "Bell", "Low Bell", "Low Bell: makes the low band a bell around its frequency instead of a shelf that lifts or lowers everything below it.", { 140, captionY[1], 76, lineHeight });
    addKnob (lmfFreqId, "Frequency", "Low-Mid Frequency", "Low-Mid Frequency: the pitch the low-mid band works on.", hz, 1, 2);
    addKnob (lmfGainId, "Gain", "Low-Mid Gain", "Low-Mid Gain: how much the low-mid band lifts or lowers the sound. Zero changes nothing.", db, 1, 3);
    addKnob (lmfQId, "Q", "Low-Mid Q", "Low-Mid Q: how narrow the low-mid band is. A higher number affects a narrower range of pitches.",
            [] (float v) { return juce::String (v, 2); }, 1, 4);
    addKnob (hmfFreqId, "Frequency", "High-Mid Frequency", "High-Mid Frequency: the pitch the high-mid band works on.", hz, 1, 5);
    addKnob (hmfGainId, "Gain", "High-Mid Gain", "High-Mid Gain: how much the high-mid band lifts or lowers the sound. Zero changes nothing.", db, 1, 6);
    addKnob (hmfQId, "Q", "High-Mid Q", "High-Mid Q: how narrow the high-mid band is. A higher number affects a narrower range of pitches.",
            [] (float v) { return juce::String (v, 2); }, 1, 7);
    addKnob (hfFreqId, "Frequency", "High Frequency", "High Frequency: the pitch the high band works on.", hz, 1, 8);
    addKnob (hfGainId, "Gain", "High Gain", "High Gain: how much the high band lifts or lowers the sound. Zero changes nothing.", db, 1, 9);
    addToggle (hfBellId, "Bell", "High Bell", "High Bell: makes the high band a bell around its frequency instead of a shelf that lifts or lowers everything above it.", { 940, captionY[1], 76, lineHeight });

    // Dynamics
    addToggle (dynamicsOnId, "Dynamics On", "Dynamics On", "Dynamics On: switches the gate and the compressor on and off together. The filters and equaliser are not affected.", { 130, headingY[2], 110, lineHeight });
    addToggle (afterEqId, "After Equaliser", "Dynamics After Equaliser", "After Equaliser: puts the gate and compressor after the equaliser instead of before it. Boosting before a compressor makes it work harder on those sounds.", { 250, headingY[2], 130, lineHeight });
    addKnob (compThresholdId, "Threshold", "Compressor Threshold", "Compressor Threshold: the level where the compressor starts turning the sound down. Turn it down and more of the sound is compressed.", plainDb, 2, 0);
    addKnob (compRatioId, "Ratio", "Compressor Ratio", "Compressor Ratio: how firmly the sound above the threshold is turned down. All the way down is off, 4 is firm and 20 is very firm.",
            [] (float v) { return v < 1.05f ? juce::String ("Off") : juce::String (v, 1) + ":1"; }, 2, 1);
    addKnob (compAttackId, "Attack", "Compressor Attack", "Compressor Attack: how quickly the compressor reacts to a loud sound. A slower attack lets more of the first hit through, so drums keep their punch.", msText, 2, 2);
    addKnob (compReleaseId, "Release", "Compressor Release", "Compressor Release: how quickly the compressor lets go after a loud sound.", secondsText, 2, 3);
    addKnob (compMakeupId, "Make-up Gain", "Make-up Gain", "Make-up Gain: the level added after compressing. Turn it up to bring the sound back to the level it had before.", plainDb, 2, 4);
    addToggle (compKneeId, "Soft Knee", "Soft Knee", "Soft Knee: eases the compressor in gradually around the threshold instead of all at once.", { 434, captionY[2], 80, lineHeight });
    addToggle (gateOnId, "Gate On", "Gate On", "Gate On: turns the sound down when it falls below the gate's threshold, for example the quiet between drum hits.", { 584, captionY[2], 80, lineHeight });
    addToggle (gateExpanderId, "Expander", "Expander", "Expander: turns the quiet sound down gently instead of cutting it. Use it when the gate sounds abrupt.", { 672, captionY[2], 90, lineHeight });
    addKnob (gateThresholdId, "Threshold", "Gate Threshold", "Gate Threshold: the level the sound must rise above to open the gate. Raise it until the quiet sounds you want to remove are turned down.", plainDb, 2, 5);
    addKnob (gateRangeId, "Range", "Gate Range", "Gate Range: how far the gate turns the quiet sound down. A small range is gentler.", plainDb, 2, 6);
    addKnob (gateAttackId, "Attack", "Gate Attack", "Gate Attack: how quickly the gate opens when the sound rises above the threshold.", msText, 2, 7);
    addKnob (gateHoldId, "Hold", "Gate Hold", "Gate Hold: how long the gate stays open after the sound falls below the threshold, so short gaps do not chatter.", secondsText, 2, 8);
    addKnob (gateReleaseId, "Release", "Gate Release", "Gate Release: how quickly the gate closes after the hold.", secondsText, 2, 9);

    for (auto& t : toggles)
        t.button.setColour (juce::TextButton::textColourOffId, Colours::text);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after everything including Output Trim and Bypass. The number under the warning is the highest sample level held for 1.5 seconds. "
                                        "This is sample peak, not true peak. Nothing to do unless the clip warning lights: then lower Output Trim. Click to clear the held number.");
    content.addAndMakeVisible (meterColumn);

    setWantsKeyboardFocus (true);
    undoButton.setTitle ("Undo");
    redoButton.setTitle ("Redo");
    undoButton.onClick = [this] { proc.history.undo(); refreshHistoryButtons(); };
    redoButton.onClick = [this] { proc.history.redo(); refreshHistoryButtons(); };
    content.addAndMakeVisible (undoButton);
    content.addAndMakeVisible (redoButton);
    refreshHistoryButtons();
    // Keyboard order follows the title bar, left to right: preset, Undo, Redo, Bypass.
    presetBox.setExplicitFocusOrder (1);
    undoButton.setExplicitFocusOrder (2);
    redoButton.setExplicitFocusOrder (3);
    bypassButton.setExplicitFocusOrder (4);

    presetBox.setTitle ("Preset");
    presetBox.onChange = [this] { presetChosen(); };
    content.addAndMakeVisible (presetBox);
    rebuildPresetMenu();

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    diagnostics->setBeforeAfter (&proc.beforeAfter);   // songsmithstudio#7
    content.addAndMakeVisible (*diagnostics);

    content.addAndMakeVisible (titleHint);   // the plugin name explains the plugin

    layoutContent();

    // Drag-resizing keeps the design proportions. The saved width is read first: setting the limits resizes the window.
    const int savedWidth = proc.editorWidth.load();
    setResizable (true, true);
    setResizeLimits (EditorLayout::minWidth, EditorLayout::heightFor (EditorLayout::minWidth),
                     EditorLayout::maxWidth, EditorLayout::heightFor (EditorLayout::maxWidth));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    const int width = EditorLayout::constrainWidth (savedWidth);
    setSize (width, EditorLayout::heightFor (width));
    constructed = true;
    syncSizeFromWidth();
    startTimerHz (30);
}

SongsmithAnalogDeskEditor::~SongsmithAnalogDeskEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    setLookAndFeel (nullptr);
}

DeskKnob* SongsmithAnalogDeskEditor::findKnob (const juce::String& paramId)
{
    for (auto& [id, index] : knobIds)
        if (id == paramId)
            return &knobs[(size_t) index];
    return nullptr;
}

std::vector<DeskKnob*> SongsmithAnalogDeskEditor::getKnobs()
{
    std::vector<DeskKnob*> out;
    for (auto& k : knobs)
        out.push_back (&k);
    return out;
}

std::vector<juce::Button*> SongsmithAnalogDeskEditor::getToggles()
{
    std::vector<juce::Button*> out;
    for (auto& t : toggles)
        out.push_back (&t.button);
    return out;
}

// The window width is the truth; it is saved with the session.
void SongsmithAnalogDeskEditor::syncSizeFromWidth()
{
    if (! constructed)
        return;
    proc.editorWidth.store (getWidth());
}

void SongsmithAnalogDeskEditor::refreshHistoryButtons()
{
    Songsmith::UI::Header::refreshHistoryButtons (undoButton, redoButton, proc.history);
}

bool SongsmithAnalogDeskEditor::keyPressed (const juce::KeyPress& key)
{
    const bool used = Songsmith::UI::Header::handleHistoryKey (key, proc.history);
    if (used)
        refreshHistoryButtons();
    return used;
}

void SongsmithAnalogDeskEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
    syncSizeFromWidth();
}

void SongsmithAnalogDeskEditor::layoutContent()
{
    meterColumn.setBounds (Songsmith::UI::MeterColumn::boundsFor (designWidth, (int) Songsmith::UI::titleBarHeight + 8, contentHeight - 12, true));

    // Title bar (the same in every plugin): name | preset | Undo | Redo ... Bypass (far right).
    const auto header = Songsmith::UI::Header::layout (
        Songsmith::UI::Header::rowIn ((int) Songsmith::UI::titleInset, designWidth - 16 - (int) Songsmith::UI::titleInset,
                                      (int) Songsmith::UI::titleBarHeight),
        JucePlugin_Name);
    titleBounds = header.title;
    titleHint.setBounds (header.title);   // exactly over the painted name
    presetBox.setBounds (header.preset);
    undoButton.setBounds (header.undo);
    redoButton.setBounds (header.redo);
    bypassButton.setBounds (header.bypass);

    diagnostics->setBounds (0, contentHeight, designWidth, footerHeight);
}

void SongsmithAnalogDeskEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithAnalogDeskEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, r);

    // Faceplate: a fine brushed grain and a lit inner edge. All vector, so it scales.
    {
        static const Grain grain;
        grain.draw (g);

        const auto plate = juce::Rectangle<float> (6.0f, (float) Songsmith::UI::titleBarHeight + 4.0f, (float) EditorLayout::controlsRight + 14.0f,
                                                   (float) contentHeight - (float) Songsmith::UI::titleBarHeight - 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (plate, 8.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (plate.translated (1.0f, 1.0f), 8.0f, 1.0f);
    }
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, r.withHeight (Songsmith::UI::titleBarHeight));
    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, Colours::text, Colours::accent);

    auto heading = [&g] (const juce::String& text, int x, int y, int width)
    {
        g.setColour (Colours::text);
        g.setFont (font (Type::heading, Weight::SemiBold));
        g.drawText (text, x, y, width, lineHeight, juce::Justification::centredLeft);
    };
    auto caption = [&g] (const juce::String& text, int x, int y)
    {
        g.setColour (Colours::accent);
        g.setFont (font (Type::control, Weight::SemiBold));
        g.drawText (text, x, y, 100, lineHeight, juce::Justification::centredLeft);
    };

    heading ("Input and Filters", leftEdge, headingY[0], 200);
    heading ("Output", leftEdge + 9 * cellWidth, headingY[0], cellWidth);
    heading ("Equaliser", leftEdge, headingY[1], 100);
    caption ("Low", leftEdge, captionY[1]);
    caption ("Low-Mid", leftEdge + 2 * cellWidth, captionY[1]);
    caption ("High-Mid", leftEdge + 5 * cellWidth, captionY[1]);
    caption ("High", leftEdge + 8 * cellWidth, captionY[1]);
    heading ("Dynamics", leftEdge, headingY[2], 100);
    caption ("Compressor", leftEdge, captionY[2]);
    caption ("Gate", leftEdge + 5 * cellWidth, captionY[2]);

    // Rules between the three sections.
    g.setColour (Colours::panelLight.brighter (0.1f));
    for (int y : { 190, 342 })
        g.drawLine ((float) leftEdge, (float) y, (float) EditorLayout::controlsRight, (float) y, 1.0f);
}

void SongsmithAnalogDeskEditor::advanceMeter (float seconds)
{
    const float gr = proc.takeGainReductionDb();
    auto& m = proc.outputMeter;
    m.readAndReset();   // the all-channel peak isn't shown; the per-channel ones are
    m.clipped.store (false, std::memory_order_relaxed);
    meterColumn.update (m.readAndResetChannel (0), m.readAndResetChannel (1), m.channels.load (std::memory_order_relaxed),
                        m.clipLatched.load (std::memory_order_relaxed), gr,
                        juce::Time::getMillisecondCounterHiRes() * 0.001, seconds);
}

//==============================================================================
void SongsmithAnalogDeskEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& factory = Presets::factory();
    presetBox.addSectionHeading ("Starting points");
    for (int i = 0; i < (int) factory.size(); ++i)
        presetBox.addItem (factory[(size_t) i].name, i + 1);

    userPresetFiles = proc.presets.getUserPresetFiles();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("Your presets");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), firstUserId + i);
    }
    presetBox.addSeparator();
    presetBox.addItem ("Save preset...", savePresetId);
    presetBox.addItem ("Open presets folder", openFolderId);

    presetBox.setText (proc.presets.currentName.isNotEmpty() ? proc.presets.currentName : juce::String ("Preset"),
                       juce::dontSendNotification);

    // The loaded starting point's listening cue is available on hover instead of as permanent text.
    juce::String tip ("Preset: choose a starting point or one of your own, or save the current settings. Loading a preset keeps the window size.");
    if (proc.presets.currentHint.isNotEmpty())
        tip << " " << proc.presets.currentHint;
    presetBox.setTooltip (tip);
}

void SongsmithAnalogDeskEditor::presetChosen()
{
    const int id = presetBox.getSelectedId();
    if (id == 0)
        return;

    if (id == savePresetId)
    {
        savePresetAsync();
    }
    else if (id == openFolderId)
    {
        const auto folder = proc.presets.getUserFolder();
        folder.createDirectory();
        folder.startAsProcess();
    }
    else if (id >= firstUserId && id - firstUserId < userPresetFiles.size())
    {
        juce::String error;
        bool loaded = false;
        proc.history.runStep ("preset load", [&] { loaded = proc.presets.loadFile (userPresetFiles[id - firstUserId], error); });
        if (! loaded)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", error);
    }
    else if (id >= 1 && id <= (int) Presets::factory().size())
    {
        proc.history.runStep ("preset load", [&] { proc.presets.loadFactory (id - 1); });
    }

    refreshHistoryButtons();
    rebuildPresetMenu();
    content.repaint();
}

void SongsmithAnalogDeskEditor::savePresetAsync()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    // Starting points are read-only, so don't suggest saving over one's name.
    const auto suggested = Presets::isFactoryName (proc.presets.currentName) ? juce::String() : proc.presets.currentName;
    saveDialog->addTextEditor ("name", suggested, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SongsmithAnalogDeskEditor> (this)] (int result)
    {
        if (safe == nullptr)
            return;

        const auto name = safe->saveDialog != nullptr ? safe->saveDialog->getTextEditorContents ("name") : juce::String();
        safe->saveDialog.reset();

        if (result == 1)
        {
            if (safe->proc.presets.userPresetExists (name))
            {
                // Overwriting is allowed, but only on purpose.
                juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                  .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                  .withTitle ("Replace preset")
                                                  .withMessage ("A preset called \"" + name.trim() + "\" already exists. Replace it with the current settings?")
                                                  .withButton ("Replace", 1)
                                                  .withButton ("Cancel", 0)
                                                  .withAssociatedComponent (safe.getComponent()),
                                              [safe, name] (int choice)
                                              {
                                                  if (safe == nullptr || choice != 1)
                                                      return;
                                                  juce::String error;
                                                  if (! safe->proc.presets.saveAs (name, error))
                                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", error);
                                                  safe->rebuildPresetMenu();
                                                  safe->content.repaint();
                                              });
                return;
            }

            juce::String error;
            if (! safe->proc.presets.saveAs (name, error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", error);
        }
        safe->rebuildPresetMenu();
        safe->content.repaint();
    }), false);
}
