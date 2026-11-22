#include "PluginEditor.h"
#include "../../shared/Source/SongsmithGrain.h"

using namespace Opt;
using namespace Opt::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;
    constexpr int designHeight = EditorLayout::designHeight;

    juce::String dbText (float v) { return (v > 0.005f ? "+" : "") + juce::String (v, 1) + " dB"; }

    std::vector<std::pair<float, juce::String>> marks (std::initializer_list<int> values)
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int v : values)
            m.push_back ({ (float) v, juce::String (v) });
        return m;
    }

    // Layout, in design units (EditorLayout::designWidth x EditorLayout::designHeight).
    const juce::Rectangle<int> gainArea           { 24, 86, 210, 240 };   // below the Oversampling dropdown's row (songsmithstudio#19)
    const juce::Rectangle<int> meterArea          { 254, 64, 252, 160 };
    const juce::Rectangle<int> meterModes         { 254, 232, 252, 28 };
    const juce::Rectangle<int> modeArea           { 254, 296, 252, 36 };
    const juce::Rectangle<int> peakReductionArea  { 526, 86, 210, 240 };
    constexpr int oversamplingRight = 526 + 210, oversamplingTop = 60, oversamplingHeight = 22;   // top-right of the controls, left of the Output panel

    const char* const compressTip = "Compress: gentle, smooth levelling. A good starting point for most vocals, bass and guitars.";
    const char* const limitTip = "Limit: much firmer control that flattens the loudest sound harder than Compress. It is a stronger setting of the same smooth action, not a hard ceiling. Useful for drums and very uneven playing; "
                                 "if it sounds too obvious, go back to Compress.";

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
juce::Rectangle<float> OptModeButton::iconArea (juce::Rectangle<int> b) noexcept
{
    return juce::Rectangle<float> (7.0f, ((float) b.getHeight() - 14.0f) * 0.5f, 17.0f, 14.0f);   // in the button's own coordinates
}

void OptModeButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    juce::TextButton::paintButton (g, isHighlighted, isDown);

    // The picture, in the button's own coordinates, left of the label.
    const auto area = iconArea (getLocalBounds());
    const auto on = getToggleState();
    const auto ink = findColour (on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);   // the same ink as the label

    // Axes: output up the side, input along the bottom.
    g.setColour (ink.withAlpha (0.45f));
    juce::Path axes;
    axes.startNewSubPath (area.getX(), area.getY());
    axes.lineTo (area.getX(), area.getBottom());
    axes.lineTo (area.getRight(), area.getBottom());
    g.strokePath (axes, juce::PathStrokeType (1.0f));

    // The response: rises evenly up to the threshold, then bends to a shallower slope.
    const float bendX = 0.45f, bendY = 0.45f;   // as fractions of the picture, from the bottom left
    const float after = bendY + (1.0f - bendX) * slopeAfterBend (limit);
    auto at = [&] (float x, float y) { return juce::Point<float> (area.getX() + area.getWidth() * x, area.getBottom() - area.getHeight() * y); };
    juce::Path line;
    line.startNewSubPath (at (0.0f, 0.0f));
    line.lineTo (at (bendX, bendY));
    line.lineTo (at (1.0f, after));
    g.setColour (ink);
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
OptKnob::OptKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& n,
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
    slider.onValueChange = [this] { valueLabel.setText (getValueText(), juce::dontSendNotification); repaint(); };
    addAndMakeVisible (slider);

    valueLabel.setText (getValueText(), juce::dontSendNotification);
    valueLabel.setFont (juce::Font (numbers (Type::control)));
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setColour (juce::Label::textColourId, Colours::text);
    valueLabel.setColour (juce::Label::backgroundColourId, Colours::recess.withAlpha (0.9f));   // a recessed well: it can be typed into
    valueLabel.setColour (juce::Label::backgroundWhenEditingColourId, Colours::recess);
    valueLabel.setColour (juce::Label::textWhenEditingColourId, Colours::text);
    valueLabel.setColour (juce::Label::outlineWhenEditingColourId, Songsmith::UI::Colours::edge);
    valueLabel.setEditable (false, true, false);   // a click or double-click types a value; Enter applies it and Escape cancels
    valueLabel.setTitle (name + " value");
    valueLabel.setTooltip (name + ": click the number to type an exact value, then press Enter. Escape keeps the old value.");
    valueLabel.onTextChange = [this]
    {
        if (! applyTypedText (valueLabel.getText()))
            valueLabel.setText (getValueText(), juce::dontSendNotification);   // not a number: keep the old value
    };
    addAndMakeVisible (valueLabel);
}

bool OptKnob::applyTypedText (const juce::String& text)
{
    const auto digits = text.retainCharacters ("0123456789.-+");
    if (! digits.containsAnyOf ("0123456789"))
        return false;
    const auto range = slider.getRange();
    slider.setValue (juce::jlimit (range.getStart(), range.getEnd(), digits.getDoubleValue()), juce::sendNotificationSync);
    valueLabel.setText (getValueText(), juce::dontSendNotification);   // shows the snapped, clamped value
    return true;
}

void OptKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    valueLabel.setBounds (r.removeFromBottom (24).withSizeKeepingCentre (96, 22));
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    const int size = side - juce::roundToInt ((float) side * 0.32f);   // room for the printed scale
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (r.getCentre()));
}

void OptKnob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::plateInk);
    g.setFont (font (Type::heading, Weight::SemiBold));
    g.drawText (name, r.removeFromTop (20.0f), juce::Justification::centred);

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
        g.setColour (Colours::plateInkDim);
        g.drawLine ({ c.getPointOnCircumference (radius + 2.0f, angle), c.getPointOnCircumference (radius + 6.0f, angle) }, 1.2f);
        const auto t = c.getPointOnCircumference (radius + 18.0f, angle);
        g.drawText (label, juce::Rectangle<float> (24.0f, 14.0f).withCentre (t), juce::Justification::centred);
    }
}

//==============================================================================
SongsmithOpticalLevellerEditor::SongsmithOpticalLevellerEditor (SongsmithOpticalLevellerProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      gainKnob (p.apvts, gainId, "Gain", marks ({ 0, 10, 20, 30, 40 }), [] (float v) { return dbText (v); }),
      peakReductionKnob (p.apvts, peakReductionId, "Peak Reduction", marks ({ 0, 25, 50, 75, 100 }),
                         [] (float v) { return juce::String (v, 1); }),
      modeAttachment (*p.apvts.getParameter (modeId), [this] (float v) { modeIndex = juce::roundToInt (v); refreshButtons(); }),
      bypassAttachment (p.apvts, bypassId, bypassButton)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    content.addAndMakeVisible (gainKnob);
    content.addAndMakeVisible (peakReductionKnob);
    gainKnob.slider.setTooltip ("Gain: the level after levelling. Turn it up to bring the sound back to the level it had before. "
                                "Double-click for the default.");
    peakReductionKnob.slider.setTooltip ("Peak Reduction: how much levelling you get. Turn it up and more of the louder sound is turned down. "
                                         "Double-click for the default.");

    // Compress and Limit: one press selects that mode. Each press is one automation gesture.
    for (int i = 0; i < 2; ++i)
    {
        modeButtons[(size_t) i].setTooltip (i == 0 ? compressTip : limitTip);
        modeButtons[(size_t) i].setTitle (modeButtons[(size_t) i].getButtonText());
        modeButtons[(size_t) i].onClick = [this, i] { modeAttachment.setValueAsCompleteGesture ((float) i); };
        content.addAndMakeVisible (modeButtons[(size_t) i]);
    }

    for (int i = 0; i < Opt::oversamplingNames().size(); ++i)
        oversamplingBox.addItem (Songsmith::UI::Dropdown::oversamplingText (i), i + 1);
    oversamplingBox.setTitle ("Oversampling");
    oversamplingBox.setTooltip ("Oversampling: runs the leveller at two or four times the sample rate inside the plugin, so fast level changes are followed more precisely. "
                                "Off is how earlier versions sounded. It uses more processing power and adds a little delay, which the plugin tells your recording program about. Nothing to do unless you want it.");
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, oversamplingId, oversamplingBox);
    content.addAndMakeVisible (oversamplingBox);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

    for (int m = 0; m < 2; ++m)
    {
        meterButtons[(size_t) m].onClick = [this, m] { setMeterMode (m); };
        meterButtons[(size_t) m].setTitle (meterButtons[(size_t) m].getButtonText());
        meterButtons[(size_t) m].setTooltip (m == 0 ? "Meter: how much the leveller is turning the level down"
                                                    : "Meter: the output level, with 0 on the scale at -18 dBFS");
        content.addAndMakeVisible (meterButtons[(size_t) m]);
    }
    content.addAndMakeVisible (meter);
    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after the leveller and Bypass. The number under the warning is the highest sample level held for 1.5 seconds. "
                                        "This is sample peak, not true peak. Nothing to do unless the clip warning lights: then lower Gain. Click to clear the held number.");
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

    modeAttachment.sendInitialUpdate();
    setMeterMode (proc.meterMode.load());
    meter.ballistics.reset (needleFraction (0.0f));

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

SongsmithOpticalLevellerEditor::~SongsmithOpticalLevellerEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    setLookAndFeel (nullptr);
}

// The window width is the truth; it is saved with the session.
void SongsmithOpticalLevellerEditor::syncSizeFromWidth()
{
    if (! constructed)
        return;
    proc.editorWidth.store (getWidth());
}

void SongsmithOpticalLevellerEditor::refreshHistoryButtons()
{
    Songsmith::UI::Header::refreshHistoryButtons (undoButton, redoButton, proc.history);
}

bool SongsmithOpticalLevellerEditor::keyPressed (const juce::KeyPress& key)
{
    const bool used = Songsmith::UI::Header::handleHistoryKey (key, proc.history);
    if (used)
        refreshHistoryButtons();
    return used;
}

void SongsmithOpticalLevellerEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
    syncSizeFromWidth();
}

void SongsmithOpticalLevellerEditor::layoutContent()
{
    gainKnob.setBounds (gainArea);
    peakReductionKnob.setBounds (peakReductionArea);

    meter.setBounds (meterArea);
    meterColumn.setBounds (Songsmith::UI::MeterColumn::boundsFor (designWidth, (int) Songsmith::UI::titleBarHeight + 8, contentHeight - 12, true));
    auto modes = meterModes;
    const int gainReductionWidth = 128, modeGap = 8;
    meterButtons[0].setBounds (modes.removeFromLeft (gainReductionWidth));
    modes.removeFromLeft (modeGap);
    meterButtons[1].setBounds (modes);

    auto row = modeArea;
    modeButtons[0].setBounds (row.removeFromLeft ((row.getWidth() - 8) / 2));
    row.removeFromLeft (8);
    modeButtons[1].setBounds (row);

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
    {   // Oversampling: the suite-standard dropdown at the top-right of the controls, beside the Output meters.
        const int right = Songsmith::UI::Dropdown::place (oversamplingBox, 0, oversamplingTop, oversamplingHeight);
        oversamplingBox.setTopLeftPosition (oversamplingRight - right, oversamplingTop);
    }

    diagnostics->setBounds (0, contentHeight, designWidth, footerHeight);
}

void SongsmithOpticalLevellerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithOpticalLevellerEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, r);

    // Faceplate: grey, with the suite's fine brushed grain over it, a lit inner edge and four screws. All vector, so it scales.
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

    // The right-hand meters sit on their own dark panel, so their light labels stay readable on the lighter plate.
    g.setColour (Colours::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (meterColumn.getBounds().expanded (8, 6).toFloat(), 6.0f);
    g.setColour (Songsmith::UI::Colours::edge);
    g.drawRoundedRectangle (meterColumn.getBounds().expanded (8, 6).toFloat().reduced (0.5f), 6.0f, 1.0f);

    // Section rules between the knobs and the meter.
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (float x : { 244.0f, 516.0f })
        g.drawLine (x, 70.0f, x, 336.0f, 1.0f);

    // Numeric gain reduction under the needle (the needle has VU ballistics; this shows the value).
    g.setColour (Colours::plateInk);
    g.setFont (numbers (Type::heading, Weight::Medium));
    g.drawText (gainReductionText (grDisplayDb),
                juce::Rectangle<float> ((float) meterArea.getX(), (float) meterModes.getBottom() + 6.0f, (float) meterArea.getWidth(), 20.0f),
                juce::Justification::centred);
}

juce::String SongsmithOpticalLevellerEditor::gainReductionText (float db)
{
    return "Gain Reduction " + juce::String (juce::jmax (0.0f, db), 1) + " dB";
}

void SongsmithOpticalLevellerEditor::refreshButtons()
{
    for (int i = 0; i < 2; ++i)
        modeButtons[(size_t) i].setToggleState (i == modeIndex, juce::dontSendNotification);
    content.repaint();
}

void SongsmithOpticalLevellerEditor::setMeterMode (int mode)
{
    mode = juce::jlimit (0, 1, mode);
    proc.meterMode.store (mode);
    for (int m = 0; m < 2; ++m)
        meterButtons[(size_t) m].setToggleState (m == mode, juce::dontSendNotification);
    meter.setModeLabel (meterModeName (mode));
}

void SongsmithOpticalLevellerEditor::advanceMeter (float seconds)
{
    const float gr = proc.takeGainReductionDb();
    const float outDb = proc.takeOutputRmsDb();
    meter.setReading (meterReadingVu (proc.meterMode.load(), gr, outDb), seconds);

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

    content.repaint (meterArea.getX(), meterArea.getY(), meterArea.getWidth(), meterModes.getBottom() + 40 - meterArea.getY());
}

//==============================================================================
void SongsmithOpticalLevellerEditor::rebuildPresetMenu()
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

void SongsmithOpticalLevellerEditor::presetChosen()
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

void SongsmithOpticalLevellerEditor::savePresetAsync()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    // Starting points are read-only, so don't suggest saving over one's name.
    const auto suggested = Presets::isFactoryName (proc.presets.currentName) ? juce::String() : proc.presets.currentName;
    saveDialog->addTextEditor ("name", suggested, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SongsmithOpticalLevellerEditor> (this)] (int result)
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
