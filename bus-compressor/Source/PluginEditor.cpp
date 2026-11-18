#include "PluginEditor.h"
#include "../../shared/Source/SongsmithGrain.h"

using namespace BusComp;
using namespace BusComp::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;
    constexpr int designHeight = EditorLayout::designHeight;

    juce::String dbText (float v) { return (v > 0.005f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String dialText (float v) { return (v > 0.005f ? "+" : "") + juce::String (juce::roundToInt (v * 2.0f) * 0.5f, 1) + " dB"; }

    std::vector<std::pair<float, juce::String>> stepMarks (const juce::StringArray& names)
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int i = 0; i < names.size(); ++i)
            m.push_back ({ (float) i, names[i].upToFirstOccurrenceOf (" ", false, false) });   // the unit is in the value text below
        return m;
    }

    BusKnob::Formatter stepText (const juce::StringArray& names)
    {
        return [names] (float v) { return names[juce::jlimit (0, names.size() - 1, juce::roundToInt (v))]; };
    }

    // Layout, in design units (EditorLayout::designWidth x EditorLayout::designHeight).
    const juce::Rectangle<int> meterArea { 24, 62, 752, 52 };
    constexpr int knobTop = 146, knobHeight = 236, knobWidth = 144, knobGap = 8, knobLeft = 24;
    juce::Rectangle<int> knobArea (int index) { return { knobLeft + index * (knobWidth + knobGap), knobTop, knobWidth, knobHeight }; }

    // The faceplate's fine brushed-metal grain (shared with the other plugins). Built once; the same grain every time.
    struct Grain : Songsmith::UI::BrushedGrain
    {
        Grain() : BrushedGrain (juce::Rectangle<float>::leftTopRightBottom (10.0f, (float) Songsmith::UI::titleBarHeight + 2.0f,
                                                                         (float) designWidth - 10.0f, (float) contentHeight - 6.0f)) {}
    };

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float slotAngle)
    {
        const float r = 4.6f;
        g.setGradientFill (juce::ColourGradient (Colours::panelLight.brighter (0.35f), c.x - r, c.y - r,
                                                 Colours::recess, c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
        g.setColour (Colours::recess.withAlpha (0.9f));
        g.drawEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r, 0.8f);
        g.drawLine ({ c.getPointOnCircumference (r - 1.2f, slotAngle), c.getPointOnCircumference (r - 1.2f, slotAngle + juce::MathConstants<float>::pi) }, 1.3f);
    }
}

//==============================================================================
BusKnob::BusKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& n,
                  std::vector<std::pair<float, juce::String>> scaleMarks, Formatter format)
    : name (n), marks (std::move (scaleMarks)), formatter (std::move (format)),
      attachment (state, paramId, slider)
{
    auto* p = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                                juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
    slider.setMouseDragSensitivity (220);
    slider.setTitle (name);
    slider.getProperties().set (Songsmith::UI::LookAndFeelBase::valueArcProperty, false);   // the printed scale shows the value
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void BusKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    r.removeFromBottom (24);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    const int size = juce::roundToInt ((float) side * 0.58f);   // room for the printed scale
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (r.getCentre()));
}

void BusKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::text);
    g.setFont (font (Type::heading, Weight::SemiBold));
    g.drawText (name, r.removeFromTop (20.0f), juce::Justification::centred);

    g.setFont (numbers (Type::control));
    g.drawText (getValueText(), r.removeFromBottom (20.0f), juce::Justification::centred);

    // Printed scale around the knob.
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float radius = sb.getWidth() * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto range = slider.getNormalisableRange();
    g.setFont (numbers (Type::caption, Weight::Medium));
    for (const auto& [value, label] : marks)
    {
        const float pos = (float) range.convertTo0to1 (value);
        const float angle = params.startAngleRadians + pos * (params.endAngleRadians - params.startAngleRadians);
        g.setColour (Colours::textDim);
        g.drawLine ({ c.getPointOnCircumference (radius + 2.0f, angle), c.getPointOnCircumference (radius + 6.0f, angle) }, 1.2f);
        const auto t = c.getPointOnCircumference (radius + 16.0f, angle);
        g.drawText (label, juce::Rectangle<float> (28.0f, 14.0f).withCentre (t), juce::Justification::centred);
    }
}

//==============================================================================
SongsmithBusCompressorEditor::SongsmithBusCompressorEditor (SongsmithBusCompressorProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      thresholdKnob (p.apvts, thresholdId, "Threshold",
                     { { -15.0f, "-15" }, { -10.0f, "-10" }, { -5.0f, "-5" }, { 0.0f, "0" }, { 5.0f, "+5" }, { 10.0f, "+10" } }, [] (float v) { return dialText (v); }),
      ratioKnob (p.apvts, ratioId, "Ratio", stepMarks (ratioNames()), stepText (ratioNames())),
      attackKnob (p.apvts, attackId, "Attack", stepMarks (attackNames()), stepText (attackNames())),
      releaseKnob (p.apvts, releaseId, "Release", stepMarks (releaseNames()), stepText (releaseNames())),
      makeupKnob (p.apvts, makeupId, "Make-up Gain", { { 0.0f, "0" }, { 5.0f, "5" }, { 10.0f, "10" }, { 15.0f, "15" } }, [] (float v) { return dbText (v); }),
      bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* knob : { &thresholdKnob, &ratioKnob, &attackKnob, &releaseKnob, &makeupKnob })
        content.addAndMakeVisible (*knob);

    thresholdKnob.slider.setTooltip ("Threshold: the level where the compressor starts turning the sound down. Turn it down and more of the sound is "
                                     "compressed. Double-click for the default.");
    ratioKnob.slider.setTooltip ("Ratio: how firmly the sound above the threshold is turned down. 2:1 is gentle, 4:1 is firm and 10:1 is very firm. "
                                 "Double-click for the default.");
    attackKnob.slider.setTooltip ("Attack: how quickly the compressor reacts to a loud sound. A slower attack lets more of the first hit through, "
                                  "so drums keep their punch. Double-click for the default.");
    releaseKnob.slider.setTooltip ("Release: how quickly the compressor lets go after a loud sound. Auto adjusts the speed to the music: quick after "
                                   "a short hit, slower after a long loud passage. Double-click for the default.");
    makeupKnob.slider.setTooltip ("Make-up Gain: the level added after compressing. Turn it up to bring the sound back to the level it had before. "
                                  "Double-click for the default.");

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

    meter.setTooltip ("Gain Reduction: how much the compressor is turning the level down, in decibels. Each lamp is one decibel.");
    meter.setTitle ("Gain Reduction");
    content.addAndMakeVisible (meter);
    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after the compressor, Make-up Gain and Bypass. The number under the warning is the highest sample level held for 1.5 seconds. "
                                        "This is sample peak, not true peak. Nothing to do unless the clip warning lights: then lower Make-up Gain. Click to clear the held number.");
    content.addAndMakeVisible (meterColumn);

    // The window is resized by dragging the bottom-right corner; the size is saved with the session.
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

    meter.ballistics.reset();

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
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

SongsmithBusCompressorEditor::~SongsmithBusCompressorEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    setLookAndFeel (nullptr);
}

// The window width is the truth; it is saved with the session.
void SongsmithBusCompressorEditor::syncSizeFromWidth()
{
    if (! constructed)
        return;
    proc.editorWidth.store (getWidth());
}

void SongsmithBusCompressorEditor::refreshHistoryButtons()
{
    Songsmith::UI::Header::refreshHistoryButtons (undoButton, redoButton, proc.history);
}

bool SongsmithBusCompressorEditor::keyPressed (const juce::KeyPress& key)
{
    const bool used = Songsmith::UI::Header::handleHistoryKey (key, proc.history);
    if (used)
        refreshHistoryButtons();
    return used;
}

void SongsmithBusCompressorEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
    syncSizeFromWidth();
}

void SongsmithBusCompressorEditor::layoutContent()
{
    meter.setBounds (meterArea);
    meterColumn.setBounds (Songsmith::UI::MeterColumn::boundsFor (designWidth, (int) Songsmith::UI::titleBarHeight + 8, contentHeight - 12, true));
    thresholdKnob.setBounds (knobArea (0));
    ratioKnob.setBounds (knobArea (1));
    attackKnob.setBounds (knobArea (2));
    releaseKnob.setBounds (knobArea (3));
    makeupKnob.setBounds (knobArea (4));

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

void SongsmithBusCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithBusCompressorEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, r);

    // Faceplate: a fine brushed grain, a lit inner edge and four screws. All vector, so it scales.
    {
        static const Grain grain;
        grain.draw (g);

        const auto plate = juce::Rectangle<float> (6.0f, (float) Songsmith::UI::titleBarHeight + 4.0f, (float) designWidth - 12.0f,
                                                   (float) contentHeight - (float) Songsmith::UI::titleBarHeight - 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (plate, 8.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (plate.translated (1.0f, 1.0f), 8.0f, 1.0f);
        const float inset = 10.0f;
        drawScrew (g, { plate.getX() + inset, plate.getY() + inset }, 0.4f);
        drawScrew (g, { plate.getRight() - inset, plate.getY() + inset }, 1.9f);
        drawScrew (g, { plate.getX() + inset, plate.getBottom() - inset }, 2.6f);
        drawScrew (g, { plate.getRight() - inset, plate.getBottom() - inset }, 0.9f);
    }
    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, r.withHeight (Songsmith::UI::titleBarHeight));
    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, Colours::text, Colours::accent);

    // Numeric gain reduction under the lamps (the lamps have a quick rise and a smooth fall; this shows the value).
    g.setColour (Colours::text);
    g.setFont (numbers (Type::heading, Weight::Medium));
    g.drawText (gainReductionText (grDisplayDb),
                juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getBottom() + 4.0f, (float) meterArea.getWidth(), 20.0f),
                juce::Justification::centred);

    // A rule between the meter and the controls.
    g.setColour (Colours::panelLight.brighter (0.1f));
    g.drawLine ((float) meterArea.getX(), 142.0f, (float) meterArea.getRight(), 142.0f, 1.0f);
}

juce::String SongsmithBusCompressorEditor::gainReductionText (float db)
{
    return "Gain Reduction " + juce::String (juce::jmax (0.0f, db), 1) + " dB";
}

void SongsmithBusCompressorEditor::advanceMeter (float seconds)
{
    const float gr = proc.takeGainReductionDb();
    meter.setReading (gr, seconds);

    // The numeric readout: instant rise, about 300 ms fall.
    grDisplayDb = gr > grDisplayDb ? gr : grDisplayDb * 0.9f + gr * 0.1f;

    {
        auto& m = proc.outputMeter;
        m.readAndReset();   // the all-channel peak isn't shown; the per-channel ones are
        m.clipped.store (false, std::memory_order_relaxed);
        meterColumn.update (m.readAndResetChannel (0), m.readAndResetChannel (1), m.channels.load (std::memory_order_relaxed),
                            m.clipLatched.load (std::memory_order_relaxed), gr,
                            juce::Time::getMillisecondCounterHiRes() * 0.001, seconds);
    }

    content.repaint (meterArea.getX(), meterArea.getY(), meterArea.getWidth(), meterArea.getHeight() + 30);
}

//==============================================================================
void SongsmithBusCompressorEditor::rebuildPresetMenu()
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

void SongsmithBusCompressorEditor::presetChosen()
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

void SongsmithBusCompressorEditor::savePresetAsync()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    // Starting points are read-only, so don't suggest saving over one's name.
    const auto suggested = Presets::isFactoryName (proc.presets.currentName) ? juce::String() : proc.presets.currentName;
    saveDialog->addTextEditor ("name", suggested, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SongsmithBusCompressorEditor> (this)] (int result)
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
