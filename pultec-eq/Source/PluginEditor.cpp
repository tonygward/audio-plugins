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
    // PEQ-04: the EQ Mode row is above the blue panel; the panel starts at plateTop and everything in it is below that.
    constexpr int plateTop = EditorLayout::plateTop;
    constexpr int modeRowY = EditorLayout::modeRowY, modeRowHeight = EditorLayout::modeRowHeight;
    constexpr int modeLabelX = 16, modeLabelWidth = 76, modeBoxX = modeLabelX + modeLabelWidth + 4, modeBoxWidth = 220;
    constexpr int curveTop = plateTop + 4, curveHeight = 144, groupsTop = curveTop + curveHeight + 12, groupsHeight = 240;
    const juce::Rectangle<int> curveArea     { 24, curveTop, 912, curveHeight };
    // Tone Shaper
    const juce::Rectangle<int> lowGroup      { 24, groupsTop, 280, groupsHeight };
    const juce::Rectangle<int> highBoostGroup { 316, groupsTop, 380, groupsHeight };
    const juce::Rectangle<int> highCutGroup  { 708, groupsTop, 228, groupsHeight };
    // Mid Shaper: three groups, the middle one wider because its switch has ten positions.
    const juce::Rectangle<int> midLowGroup   { 24, groupsTop, 268, groupsHeight };
    const juce::Rectangle<int> midDipGroup   { 304, groupsTop, 352, groupsHeight };
    const juce::Rectangle<int> midHighGroup  { 668, groupsTop, 268, groupsHeight };
    // Frequency Filter: two groups.
    const juce::Rectangle<int> highPassGroup { 24, groupsTop, 450, groupsHeight };
    const juce::Rectangle<int> lowPassGroup  { 486, groupsTop, 450, groupsHeight };
    // The output meter runs the full height of the curve and the groups, on the right edge.
    const juce::Rectangle<int> meterArea     = Songsmith::UI::MeterColumn::boundsFor (designWidth, curveTop, groupsTop + groupsHeight, false);   // SUI-23: the suite's margin and place
    constexpr int groupInset = 12, switchTop = 34, switchHeight = 28, knobTop = 70;
    constexpr int twoRowSwitchHeight = 52, midKnobTop = switchTop + twoRowSwitchHeight + 6;   // the Mid Shaper's switches have two rows

    // The faceplate's fine brushed-metal grain (shared with the other plugins). Built once; the same grain every time.
    struct Grain : Songsmith::UI::BrushedGrain
    {
        Grain() : BrushedGrain (juce::Rectangle<float>::leftTopRightBottom (10.0f, (float) plateTop + 2.0f,
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

    juce::Rectangle<int> inside (juce::Rectangle<int> group, int height = switchHeight) { return group.reduced (groupInset, 0).withTrimmedTop (switchTop).withHeight (height); }
    juce::Rectangle<int> knobRow (juce::Rectangle<int> group, int top = knobTop) { return group.reduced (groupInset, 0).withTrimmedTop (top).withTrimmedBottom (6); }
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
    const int perRow = (n + rows - 1) / rows;
    const int width = (getWidth() - gap * (perRow - 1)) / perRow;
    const int height = (getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds ((i % perRow) * (width + gap), (i / perRow) * (height + gap), width, height);
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
juce::Rectangle<int> SongsmithPassiveEQEditor::getMidLowGroup() const noexcept    { return midLowGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getMidDipGroup() const noexcept    { return midDipGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getMidHighGroup() const noexcept   { return midHighGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getHighPassGroup() const noexcept  { return highPassGroup; }
juce::Rectangle<int> SongsmithPassiveEQEditor::getLowPassGroup() const noexcept   { return lowPassGroup; }

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
      midLowSwitch (p.apvts, midLowFrequencyId, frequencyNames (SongsmithPassive::midLowFrequencies), "Low-Mid Boost Frequency",
                    "Low-mid boost frequency: the pitch the low-mid boost is centred on. Lower numbers add body, higher numbers add fullness to the middle."),
      midDipSwitch (p.apvts, midDipFrequencyId, frequencyNames (SongsmithPassive::midDipFrequencies), "Mid Dip Frequency",
                    "Mid dip frequency: the pitch the mid dip is centred on. Dipping the middle can clear a boxy or crowded sound."),
      midHighSwitch (p.apvts, midHighFrequencyId, frequencyNames (SongsmithPassive::midHighFrequencies), "High-Mid Boost Frequency",
                     "High-mid boost frequency: the pitch the high-mid boost is centred on. Lower numbers add presence, higher numbers add clarity."),
      highPassSwitch (p.apvts, highPassFrequencyId, frequencyNamesWithOff (SongsmithPassive::highPassFrequencies), "High-Pass Frequency",
                      "High-pass frequency: the high-pass filter turns down sound below this pitch. Off leaves the low end untouched."),
      lowPassSwitch (p.apvts, lowPassFrequencyId, frequencyNamesWithOff (SongsmithPassive::lowPassFrequencies), "Low-Pass Frequency",
                     "Low-pass frequency: the low-pass filter turns down sound above this pitch. Off leaves the top end untouched."),
      midLowKnob (p.apvts, midLowBoostId, "Boost", "Low-Mid Boost", marks ({ 0, 5, 10 }), [] (float v) { return boostText (v); }),
      midDipKnob (p.apvts, midDipId, "Dip", "Mid Dip", marks ({ 0, 5, 10 }), [] (float v) { return cutText (v); }),
      midHighKnob (p.apvts, midHighBoostId, "Boost", "High-Mid Boost", marks ({ 0, 5, 10 }), [] (float v) { return boostText (v); }),
      modeAttachment (*p.apvts.getParameter (modeId), [this] (float v) { shownMode = juce::jlimit (0, SongsmithPassive::numModes - 1, juce::roundToInt (v)); refreshMode(); }),
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
                                                              &lowBoostKnob, &lowCutKnob, &highBoostKnob, &bandwidthKnob, &highCutKnob,
                                                              &midLowSwitch, &midDipSwitch, &midHighSwitch, &highPassSwitch, &lowPassSwitch,
                                                              &midLowKnob, &midDipKnob, &midHighKnob })
        content.addChildComponent (*c);   // refreshMode() shows the ones for the mode in use
    midLowSwitch.setRows (2);
    midDipSwitch.setRows (2);
    midHighSwitch.setRows (2);

    // PEQ-04: the EQ Mode dropdown, in its own row under the title bar and outside the blue panel.
    for (int i = 0; i < modeNames().size(); ++i)
        modeBox.addItem (modeNames()[i], i + 1);
    modeBox.setTitle ("EQ Mode");
    modeBox.setTooltip ("EQ Mode: choose what this equaliser does. Each mode keeps its own settings, so you can switch back and forth.\n"
                        "Tone Shaper: low and high shaping inspired by the Pultec EQP-1A: a low boost and cut that work together, a high boost with adjustable width, and a high cut.\n"
                        "Mid Shaper: broad midrange shaping inspired by the Pultec MEQ-5: a low-mid boost, a mid dip and a high-mid boost, each with its own frequency.\n"
                        "Frequency Filter: a high-pass and a low-pass filter inspired by the Pultec HLF-3C, each with its own frequency. Off leaves that end untouched.");
    modeBox.onChange = [this]
    {
        const int i = modeBox.getSelectedItemIndex();
        if (i >= 0 && i != shownMode)
            modeAttachment.setValueAsCompleteGesture ((float) i);
    };
    content.addAndMakeVisible (modeBox);
    modeLabel.setText ("EQ Mode", juce::dontSendNotification);
    modeLabel.setFont (juce::Font (font (Type::heading, Weight::Medium)));
    modeLabel.setColour (juce::Label::textColourId, Colours::textDim);
    modeLabel.setJustificationType (juce::Justification::centredRight);
    modeLabel.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (modeLabel);

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
    midLowKnob.slider.setTooltip ("Low-Mid Boost: a broad lift around the chosen low-mid frequency. Adds body and warmth; small amounts are often enough. "
                                  "Double-click for the default.");
    midDipKnob.slider.setTooltip ("Mid Dip: a broad dip around the chosen mid frequency. Turning it up takes more of that middle out, which can clear a boxy or crowded sound. "
                                  "Double-click for the default.");
    midHighKnob.slider.setTooltip ("High-Mid Boost: a broad lift around the chosen high-mid frequency. Adds presence and clarity; if it sounds harsh, turn it down. "
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

    modeAttachment.sendInitialUpdate();   // shows the controls of the saved mode
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

// The mode changed: show its controls, hide the others, and redraw the panel and the curve.
void SongsmithPassiveEQEditor::refreshMode()
{
    const bool tone = shownMode == 0, mid = shownMode == 1, filter = shownMode == 2;
    for (auto* c : std::initializer_list<juce::Component*> { &lowSwitch, &highBoostSwitch, &highCutSwitch,
                                                              &lowBoostKnob, &lowCutKnob, &highBoostKnob, &bandwidthKnob, &highCutKnob })
        c->setVisible (tone);
    for (auto* c : std::initializer_list<juce::Component*> { &midLowSwitch, &midDipSwitch, &midHighSwitch, &midLowKnob, &midDipKnob, &midHighKnob })
        c->setVisible (mid);
    for (auto* c : std::initializer_list<juce::Component*> { &highPassSwitch, &lowPassSwitch })
        c->setVisible (filter);
    if (modeBox.getSelectedItemIndex() != shownMode)
        modeBox.setSelectedItemIndex (shownMode, juce::dontSendNotification);
    content.repaint();
    refreshCurve();
}

void SongsmithPassiveEQEditor::refreshCurve()
{
    const auto params = proc.getEngineParams();
    curve.setParams (params);
    const int key = params.highPassIndex * 16 + params.lowPassIndex;
    if (key != filterKey)
    {
        filterKey = key;
        if (shownMode == 2)
            content.repaint();
    }
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

    // PEQ-04: the Mid Shaper, with two-row switches so every frequency has room, and the Frequency Filter.
    midLowSwitch.setBounds (inside (midLowGroup, twoRowSwitchHeight));
    midDipSwitch.setBounds (inside (midDipGroup, twoRowSwitchHeight));
    midHighSwitch.setBounds (inside (midHighGroup, twoRowSwitchHeight));
    midLowKnob.setBounds (knobRow (midLowGroup, midKnobTop));
    midDipKnob.setBounds (knobRow (midDipGroup, midKnobTop));
    midHighKnob.setBounds (knobRow (midHighGroup, midKnobTop));
    highPassSwitch.setBounds (inside (highPassGroup));
    lowPassSwitch.setBounds (inside (lowPassGroup));

    // The configuration row, directly under the title bar and outside the panel.
    modeLabel.setBounds (modeLabelX, modeRowY, modeLabelWidth, modeRowHeight);
    modeBox.setBounds (modeBoxX, modeRowY, modeBoxWidth, modeRowHeight);

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
        // PEQ-04: the blue panel begins below the EQ Mode row.
        const auto plate = juce::Rectangle<float> (6.0f, (float) plateTop, (float) designWidth - 12.0f,
                                                   (float) contentHeight - (float) plateTop - 6.0f);
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
    using Group = std::pair<juce::Rectangle<int>, const char*>;
    std::vector<Group> groups;
    if (shownMode == 1)
        groups = { { midLowGroup, "Low-Mid Boost" }, { midDipGroup, "Mid Dip" }, { midHighGroup, "High-Mid Boost" } };
    else if (shownMode == 2)
        groups = { { highPassGroup, "High-Pass Filter" }, { lowPassGroup, "Low-Pass Filter" } };
    else
        groups = { { lowGroup, "Low" }, { highBoostGroup, "High Boost" }, { highCutGroup, "High Cut" } };
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

    // The Frequency Filter has nothing to turn, only a frequency to choose, so each group shows the one chosen, large, with
    // what the filter does in plain words.
    if (shownMode == 2)
    {
        const auto paintFilter = [&] (juce::Rectangle<int> area, const juce::String& frequency, const juce::String& what)
        {
            auto body = area.reduced (groupInset + 6, 0).withTrimmedTop (switchTop + switchHeight + 14).withTrimmedBottom (14);
            g.setColour (Colours::text);
            g.setFont (numbers (Type::title * 1.8f, Weight::SemiBold));
            g.drawText (frequency, body.removeFromTop (60), juce::Justification::centred);
            g.setColour (Colours::textDim);
            g.setFont (font (Type::control));
            g.drawFittedText (what, body, juce::Justification::centredTop, 3);
        };
        const auto hp = proc.getEngineParams().highPassIndex, lp = proc.getEngineParams().lowPassIndex;
        paintFilter (highPassGroup, hp > 0 ? frequencyName (SongsmithPassive::highPassFrequencies[(size_t) hp]) : juce::String ("Off"),
                     hp > 0 ? "Turns down sound below this pitch, at 12 dB per octave. Useful for clearing rumble you do not need."
                            : "Off: the low end is untouched. Choose a frequency to turn down sound below it.");
        paintFilter (lowPassGroup, lp > 0 ? frequencyName (SongsmithPassive::lowPassFrequencies[(size_t) lp]) : juce::String ("Off"),
                     lp > 0 ? "Turns down sound above this pitch, at 12 dB per octave. Useful for softening hiss or a brittle top."
                            : "Off: the top end is untouched. Choose a frequency to turn down sound above it.");
    }
}

//==============================================================================
void SongsmithPassiveEQEditor::rebuildPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    // PEQ-06: the starting points under group headings (Drums, Bass, Guitar, Vocals, Mix Bus and Mastering, Utility). A heading
    // cannot be chosen and the arrow keys skip it. A starting point's menu item number is still its index plus one, as before.
    const auto& factory = Presets::factory();
    for (auto& group : Presets::withoutEmptyCategories (Presets::categories()))
    {
        presetBox.addSectionHeading (group.name);
        for (int index : group.presets)
            presetBox.addItem (factory[(size_t) index].name, index + 1);
    }

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
