#include "PluginEditor.h"
#include "../../shared/Source/SongsmithGrain.h"

using namespace Peq;
using namespace Peq::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;
    constexpr int designHeight = EditorLayout::designHeight;

    using Marks = std::vector<std::pair<float, juce::String>>;

    Marks marks (std::initializer_list<int> values)
    {
        Marks m;
        for (int v : values)
            m.push_back ({ (float) v, juce::String (v) });
        return m;
    }

    // Layout, in design units (EditorLayout::designWidth x EditorLayout::designHeight).
    const juce::Rectangle<int> curveArea     { 24, 60, 912, 144 };
    const juce::Rectangle<int> lowGroup      { 24, 216, 280, 224 };
    const juce::Rectangle<int> highBoostGroup { 316, 216, 380, 224 };
    const juce::Rectangle<int> highCutGroup  { 708, 216, 228, 224 };
    // The output meter runs the full height of the curve and the groups, on the right edge.
    const juce::Rectangle<int> meterArea     = Songsmith::UI::MeterColumn::boundsFor (designWidth, 60, 440, false);   // SUI-23: the suite's margin and place
    constexpr int groupInset = 12, switchTop = 34, switchHeight = 28, knobTop = 70;

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

    juce::Rectangle<int> inside (juce::Rectangle<int> group) { return group.reduced (groupInset, 0).withTrimmedTop (switchTop).withHeight (switchHeight); }
    juce::Rectangle<int> knobRow (juce::Rectangle<int> group) { return group.reduced (groupInset, 0).withTrimmedTop (knobTop).withTrimmedBottom (6); }
}

//==============================================================================
PeqKnob::PeqKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& l, const juce::String& title,
                  Marks scaleMarks, Formatter format)
    : label (l), marks (std::move (scaleMarks)), formatter (std::move (format)),
      attachment (state, paramId, slider)
{
    auto* p = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f) + juce::MathConstants<float>::twoPi,
                                juce::degreesToRadians (135.0f) + juce::MathConstants<float>::twoPi, true);
    slider.setMouseDragSensitivity (220);
    slider.setTitle (title);
    slider.getProperties().set (Songsmith::UI::LookAndFeelBase::valueArcProperty, false);   // the printed scale shows the value
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void PeqKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    r.removeFromBottom (24);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    const int size = side - juce::roundToInt ((float) side * 0.32f);   // room for the printed scale
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (r.getCentre()));
}

void PeqKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::text);
    g.setFont (font (Type::heading, Weight::SemiBold));
    g.drawText (label, r.removeFromTop (20.0f), juce::Justification::centred);

    g.setFont (numbers (Type::control));
    g.drawText (getValueText(), r.removeFromBottom (20.0f), juce::Justification::centred);

    // Printed scale around the knob.
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float radius = sb.getWidth() * 0.5f;
    const auto params = slider.getRotaryParameters();
    const auto range = slider.getNormalisableRange();
    g.setFont (numbers (Type::caption, Weight::Medium));
    for (const auto& [value, text] : marks)
    {
        const float pos = (float) range.convertTo0to1 (value);
        const float angle = params.startAngleRadians + pos * (params.endAngleRadians - params.startAngleRadians);
        g.setColour (Colours::textDim);
        g.drawLine ({ c.getPointOnCircumference (radius + 2.0f, angle), c.getPointOnCircumference (radius + 6.0f, angle) }, 1.2f);
        const auto t = c.getPointOnCircumference (radius + 18.0f, angle);
        g.drawText (text, juce::Rectangle<float> (24.0f, 14.0f).withCentre (t), juce::Justification::centred);
    }
}

//==============================================================================
PeqSwitch::PeqSwitch (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::StringArray& names,
                      const juce::String& title, const juce::String& tooltip)
    : attachment (*state.getParameter (paramId), [this] (float v) { selected = juce::roundToInt (v); refresh(); })
{
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i]));
        b->setTitle (title + " " + names[i]);
        b->setTooltip (tooltip);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void PeqSwitch::resized()
{
    const int gap = 4, n = juce::jmax (1, buttons.size());
    const int width = (getWidth() - gap * (n - 1)) / n;
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (i * (width + gap), 0, width, getHeight());
}

void PeqSwitch::refresh()
{
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
}

//==============================================================================
juce::String SongsmithPassiveEQEditor::boostText (float db) { return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB"; }
juce::String SongsmithPassiveEQEditor::cutText (float db)   { return (db > 0.05f ? "-" : "") + juce::String (db, 1) + " dB"; }

juce::String SongsmithPassiveEQEditor::bandwidthText (float value)
{
    if (value <= 0.05f)
        return "Broad";
    if (value >= Peq::maxBandwidth - 0.05f)
        return "Narrow";
    return juce::String (value, 1);
}

juce::Rectangle<int> SongsmithPassiveEQEditor::getLowGroup() const noexcept       { return lowGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getHighBoostGroup() const noexcept { return highBoostGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getHighCutGroup() const noexcept   { return highCutGroup; }

SongsmithPassiveEQEditor::SongsmithPassiveEQEditor (SongsmithPassiveEQProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      lowSwitch (p.apvts, lowFrequencyId, frequencyNames (SongsmithPassive::lowFrequencies), "Low Frequency",
                 "Low frequency: the pitch the low boost and the low cut work around. 60 Hz is a good place to start for kick drums and bass."),
      highBoostSwitch (p.apvts, highBoostFrequencyId, frequencyNames (SongsmithPassive::highBoostFrequencies), "High Boost Frequency",
                       "High boost frequency: the pitch the high boost is centred on. Lower numbers add presence, higher numbers add air."),
      highCutSwitch (p.apvts, highCutFrequencyId, frequencyNames (SongsmithPassive::highCutFrequencies), "High Cut Frequency",
                     "High cut frequency: the pitch above which the top end is turned down."),
      lowBoostKnob (p.apvts, lowBoostId, "Boost", "Low Boost", marks ({ 0, 5, 10 }), [] (float v) { return boostText (v); }),
      lowCutKnob (p.apvts, lowCutId, "Cut", "Low Cut", marks ({ 0, 5, 10, 15 }), [] (float v) { return cutText (v); }),
      highBoostKnob (p.apvts, highBoostId, "Boost", "High Boost", marks ({ 0, 6, 12, 18 }), [] (float v) { return boostText (v); }),
      bandwidthKnob (p.apvts, highBandwidthId, "Bandwidth", "High Boost Bandwidth", marks ({ 0, 5, 10 }), [] (float v) { return bandwidthText (v); }),
      highCutKnob (p.apvts, highCutId, "Cut", "High Cut", marks ({ 0, 4, 8, 12, 16 }), [] (float v) { return cutText (v); }),
      bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (curve);
    outputMeter.setTitle ("Output level");
    outputMeter.setTooltip ("Output level of the left and right channels, after the equaliser and Bypass. This equaliser has no output control, so a boost makes the sound louder: "
                            "if the clip warning lights, lower the boosts. The number under the warning is the highest sample level held for 1.5 seconds. "
                            "This is sample peak, not true peak. Click to clear the held number.");
    content.addAndMakeVisible (outputMeter);
    for (auto* c : std::initializer_list<juce::Component*> { &lowSwitch, &highBoostSwitch, &highCutSwitch,
                                                              &lowBoostKnob, &lowCutKnob, &highBoostKnob, &bandwidthKnob, &highCutKnob })
        content.addAndMakeVisible (*c);

    lowBoostKnob.slider.setTooltip ("Low Boost: adds weight below the chosen low frequency. Using it together with Low Cut at the same "
                                    "frequency adds weight while keeping the low end tidy. Double-click for the default.");
    lowCutKnob.slider.setTooltip ("Low Cut: turns down the low end a little higher up than the boost, which keeps a boosted low end from sounding boomy. "
                                  "Double-click for the default.");
    highBoostKnob.slider.setTooltip ("High Boost: lifts the top end around the chosen high boost frequency. Small amounts are often enough. "
                                     "Double-click for the default.");
    bandwidthKnob.slider.setTooltip ("Bandwidth: how wide the high boost is. Broad lifts a wide area gently, narrow lifts a smaller area. "
                                     "Double-click for the default.");
    highCutKnob.slider.setTooltip ("High Cut: turns down everything above the chosen high cut frequency. Useful for taking the edge off a harsh sound. "
                                   "Double-click for the default.");

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

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

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);

    content.addAndMakeVisible (titleHint);   // the plugin name explains the plugin

    layoutContent();
    refreshCurve();

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

SongsmithPassiveEQEditor::~SongsmithPassiveEQEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    setLookAndFeel (nullptr);
}

// The window width is the truth; it is saved with the session.
void SongsmithPassiveEQEditor::syncSizeFromWidth()
{
    if (! constructed)
        return;
    proc.editorWidth.store (getWidth());
}

void SongsmithPassiveEQEditor::refreshCurve()
{
    curve.setParams (proc.getEngineParams());
}

void SongsmithPassiveEQEditor::refreshHistoryButtons()
{
    Songsmith::UI::Header::refreshHistoryButtons (undoButton, redoButton, proc.history);
}

bool SongsmithPassiveEQEditor::keyPressed (const juce::KeyPress& key)
{
    const bool used = Songsmith::UI::Header::handleHistoryKey (key, proc.history);
    if (used)
        refreshHistoryButtons();
    return used;
}

void SongsmithPassiveEQEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
    syncSizeFromWidth();
}

void SongsmithPassiveEQEditor::advanceMeter (double elapsedSeconds)
{
    auto& m = proc.outputMeter;
    m.readAndReset();   // the all-channel peak isn't shown; the per-channel ones are
    m.clipped.store (false, std::memory_order_relaxed);
    outputMeter.update (m.readAndResetChannel (0), m.readAndResetChannel (1),
                        m.channels.load (std::memory_order_relaxed),
                        m.clipLatched.load (std::memory_order_relaxed),
                        juce::Time::getMillisecondCounterHiRes() * 0.001, elapsedSeconds);
}

void SongsmithPassiveEQEditor::layoutContent()
{
    curve.setBounds (curveArea);
    outputMeter.setBounds (meterArea);

    lowSwitch.setBounds (inside (lowGroup));
    highBoostSwitch.setBounds (inside (highBoostGroup));
    highCutSwitch.setBounds (inside (highCutGroup));

    auto low = knobRow (lowGroup);
    lowBoostKnob.setBounds (low.removeFromLeft (low.getWidth() / 2));
    lowCutKnob.setBounds (low);

    auto high = knobRow (highBoostGroup);
    highBoostKnob.setBounds (high.removeFromLeft (high.getWidth() / 2));
    bandwidthKnob.setBounds (high);

    highCutKnob.setBounds (knobRow (highCutGroup));

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

void SongsmithPassiveEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithPassiveEQEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, r);

    // Faceplate: blue, with the suite's fine brushed grain over it, a lit inner edge and four screws. All vector, so it scales.
    {
        const auto plate = juce::Rectangle<float> (6.0f, (float) Songsmith::UI::titleBarHeight + 4.0f, (float) designWidth - 12.0f,
                                                   (float) contentHeight - (float) Songsmith::UI::titleBarHeight - 10.0f);
        g.setGradientFill (juce::ColourGradient (Colours::plateTop, 0.0f, plate.getY(), Colours::plateBottom, 0.0f, plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 8.0f);

        static const Grain grain;
        grain.draw (g);
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

    // Each group sits in its own softly recessed panel with its name, so what belongs together reads together.
    const std::pair<juce::Rectangle<int>, const char*> groups[] = { { lowGroup, "Low" }, { highBoostGroup, "High Boost" }, { highCutGroup, "High Cut" } };
    for (const auto& [area, name] : groups)
    {
        g.setColour (Colours::recess.withAlpha (0.45f));
        g.fillRoundedRectangle (area.toFloat(), 8.0f);
        g.setColour (Colours::panelLight.withAlpha (0.5f));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::text);
        g.setFont (font (Type::heading, Weight::SemiBold));
        g.drawText (name, area.withTrimmedLeft (groupInset + 2).withHeight (switchTop - 4).withTrimmedTop (8), juce::Justification::centredLeft);
    }
}

//==============================================================================
void SongsmithPassiveEQEditor::rebuildPresetMenu()
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

void SongsmithPassiveEQEditor::presetChosen()
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
    refreshCurve();
    content.repaint();
}

void SongsmithPassiveEQEditor::savePresetAsync()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    // Starting points are read-only, so don't suggest saving over one's name.
    const auto suggested = Presets::isFactoryName (proc.presets.currentName) ? juce::String() : proc.presets.currentName;
    saveDialog->addTextEditor ("name", suggested, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SongsmithPassiveEQEditor> (this)] (int result)
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
