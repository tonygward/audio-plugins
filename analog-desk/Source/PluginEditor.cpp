#include "PluginEditor.h"
#include "../../shared/Source/SongsmithGrain.h"

using namespace Desk;
using namespace Desk::UI;

namespace
{
    constexpr int designWidth = EditorLayout::designWidth, contentHeight = EditorLayout::contentHeight, footerHeight = EditorLayout::footerHeight;
    constexpr int designHeight = EditorLayout::designHeight;
    constexpr int lineHeight = 22;
    namespace EL = EditorLayout;

    juce::String signedDb (float v) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String plainDb (float v) { return juce::String (v, 1) + " dB"; }
    juce::String hzText (float hz) { return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz"; }
    juce::String msText (float v) { return juce::String (v, v < 10.0f ? 1 : 0) + " ms"; }
    juce::String secondsText (float v) { return juce::String (v, 2) + " s"; }

    // Where each control sits. Every module's knobs fill the room under its header (and its row of switches), in the order of the strip.
    int moduleKnobsTop (int toggleRows) { return EL::moduleTop + EL::headerHeight + toggleRows * EL::toggleRowHeight + 4; }

    juce::Rectangle<int> cellIn (int x, int col, int row, int rows, int toggleRows)
    {
        const int top = moduleKnobsTop (toggleRows), height = (EL::moduleBottom - 8 - top) / rows;
        return { x + 10 + col * EL::cellWidth, top + row * height, EL::cellWidth, height };
    }

    const juce::Colour lowBand { 0xffa3acb8 }, lowMidBand { 0xff4fb9e0 }, highMidBand { 0xff6cc57c }, highBand { 0xffe3665a };   // dark neutral, cyan, green, red

    juce::Rectangle<int> knobRect (const juce::String& id)
    {
        // Input and Filters: one column
        if (id == inputTrimId) return cellIn (EL::inputX, 0, 0, 4, 0);
        if (id == driveId)     return cellIn (EL::inputX, 0, 1, 4, 0);
        if (id == highPassId)  return cellIn (EL::inputX, 0, 2, 4, 0);
        if (id == lowPassId)   return cellIn (EL::inputX, 0, 3, 4, 0);
        // Dynamics: the Gate (left) then the Compressor (right), five knobs each, under a row of names and two rows of switches
        if (id == gateThresholdId) return cellIn (EL::dynamicsX, 0, 0, 5, 3);
        if (id == gateRangeId)     return cellIn (EL::dynamicsX, 0, 1, 5, 3);
        if (id == gateAttackId)    return cellIn (EL::dynamicsX, 0, 2, 5, 3);
        if (id == gateHoldId)      return cellIn (EL::dynamicsX, 0, 3, 5, 3);
        if (id == gateReleaseId)   return cellIn (EL::dynamicsX, 0, 4, 5, 3);
        if (id == compThresholdId) return cellIn (EL::dynamicsX, 1, 0, 5, 3);
        if (id == compRatioId)     return cellIn (EL::dynamicsX, 1, 1, 5, 3);
        if (id == compAttackId)    return cellIn (EL::dynamicsX, 1, 2, 5, 3);
        if (id == compReleaseId)   return cellIn (EL::dynamicsX, 1, 3, 5, 3);
        if (id == compMakeupId)    return cellIn (EL::dynamicsX, 1, 4, 5, 3);
        // Equaliser: a column for each band, low to high; Gain, Frequency, Q down each
        if (id == lfGainId)   return cellIn (EL::eqX, 0, 0, 3, 1);
        if (id == lfFreqId)   return cellIn (EL::eqX, 0, 1, 3, 1);
        if (id == lmfGainId)  return cellIn (EL::eqX, 1, 0, 3, 1);
        if (id == lmfFreqId)  return cellIn (EL::eqX, 1, 1, 3, 1);
        if (id == lmfQId)     return cellIn (EL::eqX, 1, 2, 3, 1);
        if (id == hmfGainId)  return cellIn (EL::eqX, 2, 0, 3, 1);
        if (id == hmfFreqId)  return cellIn (EL::eqX, 2, 1, 3, 1);
        if (id == hmfQId)     return cellIn (EL::eqX, 2, 2, 3, 1);
        if (id == hfGainId)   return cellIn (EL::eqX, 3, 0, 3, 1);
        if (id == hfFreqId)   return cellIn (EL::eqX, 3, 1, 3, 1);
        jassertfalse;
        return {};
    }

    juce::Rectangle<int> toggleRect (const juce::String& id)
    {
        const int h = lineHeight, w = EL::cellWidth - 4;
        const int base = EL::moduleTop + EL::headerHeight + 3, row1 = base + EL::toggleRowHeight, row2 = row1 + EL::toggleRowHeight;   // base is the row of band and column names
        // module headers: the module's own switch at the right
        const int headerY = EL::moduleTop + 2;
        if (id == eqOnId)       return { EL::eqX + EL::eqWidth - 118, headerY, 110, h };
        if (id == dynamicsOnId) return { EL::dynamicsX + EL::dynamicsWidth - 112, headerY, 106, h };
        // Dynamics: Gate On and Expander on the left; Soft Knee and After Equaliser on the right
        if (id == gateOnId)       return { EL::dynamicsX + 10, row1, w, h };
        if (id == gateExpanderId) return { EL::dynamicsX + 10, row2, w, h };
        if (id == compKneeId)     return { EL::dynamicsX + 10 + EL::cellWidth, row1, w, h };
        if (id == afterEqId)      return { EL::dynamicsX + 10 + EL::cellWidth, row2, w, h };
        // Equaliser: Bell on the Low and High bands, under the band names
        if (id == lfBellId) return { EL::eqX + 10 + 46, base, 52, h };
        if (id == hfBellId) return { EL::eqX + 10 + 3 * EL::cellWidth + 46, base, 52, h };
        jassertfalse;
        return {};
    }

    // The faceplate's fine brushed-metal grain (shared with the other plugins). Built once; the same grain every time.
    struct Grain : Songsmith::UI::BrushedGrain
    {
        Grain() : BrushedGrain (juce::Rectangle<float>::leftTopRightBottom ((float) EL::margin, (float) EL::moduleTop,
                                                                         (float) EL::moduleRight, (float) EL::moduleBottom)) {}
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
    return { "Input and Filters", "Dynamics", "Gate", "Compressor", "Equaliser", "Low", "Low-Mid", "High-Mid", "High" };   // the Output panel paints its own heading
}

DeskKnob& SongsmithAnalogDeskEditor::addKnob (const char* paramId, const juce::String& name, const juce::String& title, const juce::String& tip,
                                              DeskKnob::Formatter format, juce::Rectangle<int> bounds)
{
    knobs.emplace_back (proc.apvts, paramId, name, title, std::move (format));
    auto& k = knobs.back();
    k.slider.setTooltip (tip + " Double-click for the default.");
    k.setBounds (bounds);
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
    : juce::AudioProcessorEditor (&p), proc (p), bypassAttachment (p.apvts, bypassId, bypassButton),
      outputTrimAttachment (p.apvts, outputTrimId, outputPanel.getTrim()),
      autoGainAttachment (p.apvts, autoGainId, outputPanel.getAutoGain())
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    auto db = [] (float v) { return signedDb (v); };
    auto hz = [] (float v) { return hzText (v); };

    // Input, drive and filters
    addKnob (inputTrimId, "Input Trim", "Input Trim", "Input Trim: the level going into the strip, before anything else. Turn it down if the sound is clipping or the Drive is too strong.", db, knobRect (inputTrimId));
    addKnob (driveId, "Drive", "Drive", "Drive: adds a soft, warm edge by gently rounding off the peaks. Zero leaves the sound exactly as it is.",
            [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }, knobRect (driveId));
    addKnob (highPassId, "High-Pass", "High-Pass Filter", "High-Pass Filter: removes the low sounds below this frequency, such as rumble. All the way down is off.",
            [] (float v) { return v < highPassOffBelowHz ? juce::String ("Off") : hzText (v); }, knobRect (highPassId));
    addKnob (lowPassId, "Low-Pass", "Low-Pass Filter", "Low-Pass Filter: removes the high sounds above this frequency, such as hiss. All the way up is off.",
            [] (float v) { return v > lowPassOffAboveHz ? juce::String ("Off") : hzText (v); }, knobRect (lowPassId));

    // Equaliser
    addToggle (eqOnId, "Equaliser On", "Equaliser On", "Equaliser On: switches the four equaliser bands on and off together. The filters and dynamics are not affected.", toggleRect (eqOnId));
    addKnob (lfFreqId, "Frequency", "Low Frequency", "Low Frequency: the pitch the low band works on.", hz, knobRect (lfFreqId));
    addKnob (lfGainId, "Gain", "Low Gain", "Low Gain: how much the low band lifts or lowers the sound. Zero changes nothing.", db, knobRect (lfGainId));
    addToggle (lfBellId, "Bell", "Low Bell", "Low Bell: makes the low band a bell around its frequency instead of a shelf that lifts or lowers everything below it.", toggleRect (lfBellId));
    addKnob (lmfFreqId, "Frequency", "Low-Mid Frequency", "Low-Mid Frequency: the pitch the low-mid band works on.", hz, knobRect (lmfFreqId));
    addKnob (lmfGainId, "Gain", "Low-Mid Gain", "Low-Mid Gain: how much the low-mid band lifts or lowers the sound. Zero changes nothing.", db, knobRect (lmfGainId));
    addKnob (lmfQId, "Q", "Low-Mid Q", "Low-Mid Q: how narrow the low-mid band is. A higher number affects a narrower range of pitches.",
            [] (float v) { return juce::String (v, 2); }, knobRect (lmfQId));
    addKnob (hmfFreqId, "Frequency", "High-Mid Frequency", "High-Mid Frequency: the pitch the high-mid band works on.", hz, knobRect (hmfFreqId));
    addKnob (hmfGainId, "Gain", "High-Mid Gain", "High-Mid Gain: how much the high-mid band lifts or lowers the sound. Zero changes nothing.", db, knobRect (hmfGainId));
    addKnob (hmfQId, "Q", "High-Mid Q", "High-Mid Q: how narrow the high-mid band is. A higher number affects a narrower range of pitches.",
            [] (float v) { return juce::String (v, 2); }, knobRect (hmfQId));
    addKnob (hfFreqId, "Frequency", "High Frequency", "High Frequency: the pitch the high band works on.", hz, knobRect (hfFreqId));
    addKnob (hfGainId, "Gain", "High Gain", "High Gain: how much the high band lifts or lowers the sound. Zero changes nothing.", db, knobRect (hfGainId));
    addToggle (hfBellId, "Bell", "High Bell", "High Bell: makes the high band a bell around its frequency instead of a shelf that lifts or lowers everything above it.", toggleRect (hfBellId));

    // Dynamics
    addToggle (dynamicsOnId, "Dynamics On", "Dynamics On", "Dynamics On: switches the gate and the compressor on and off together. The filters and equaliser are not affected.", toggleRect (dynamicsOnId));
    addToggle (afterEqId, "After Equaliser", "Dynamics After Equaliser", "After Equaliser: puts the gate and compressor after the equaliser instead of before it. Boosting before a compressor makes it work harder on those sounds.", toggleRect (afterEqId));
    addKnob (compThresholdId, "Threshold", "Compressor Threshold", "Compressor Threshold: the level where the compressor starts turning the sound down. Turn it down and more of the sound is compressed.", plainDb, knobRect (compThresholdId));
    addKnob (compRatioId, "Ratio", "Compressor Ratio", "Compressor Ratio: how firmly the sound above the threshold is turned down. All the way down is off, 4 is firm and 20 is very firm.",
            [] (float v) { return v < 1.05f ? juce::String ("Off") : juce::String (v, 1) + ":1"; }, knobRect (compRatioId));
    addKnob (compAttackId, "Attack", "Compressor Attack", "Compressor Attack: how quickly the compressor reacts to a loud sound. A slower attack lets more of the first hit through, so drums keep their punch.", msText, knobRect (compAttackId));
    addKnob (compReleaseId, "Release", "Compressor Release", "Compressor Release: how quickly the compressor lets go after a loud sound.", secondsText, knobRect (compReleaseId));
    addKnob (compMakeupId, "Make-up Gain", "Make-up Gain", "Make-up Gain: the level added after compressing. Turn it up to bring the sound back to the level it had before.", plainDb, knobRect (compMakeupId));
    addToggle (compKneeId, "Soft Knee", "Soft Knee", "Soft Knee: eases the compressor in gradually around the threshold instead of all at once.", toggleRect (compKneeId));
    addToggle (gateOnId, "Gate On", "Gate On", "Gate On: turns the sound down when it falls below the gate's threshold, for example the quiet between drum hits.", toggleRect (gateOnId));
    addToggle (gateExpanderId, "Expander", "Expander", "Expander: turns the quiet sound down gently instead of cutting it. Use it when the gate sounds abrupt.", toggleRect (gateExpanderId));
    addKnob (gateThresholdId, "Threshold", "Gate Threshold", "Gate Threshold: the level the sound must rise above to open the gate. Raise it until the quiet sounds you want to remove are turned down.", plainDb, knobRect (gateThresholdId));
    addKnob (gateRangeId, "Range", "Gate Range", "Gate Range: how far the gate turns the quiet sound down. A small range is gentler.", plainDb, knobRect (gateRangeId));
    addKnob (gateAttackId, "Attack", "Gate Attack", "Gate Attack: how quickly the gate opens when the sound rises above the threshold.", msText, knobRect (gateAttackId));
    addKnob (gateHoldId, "Hold", "Gate Hold", "Gate Hold: how long the gate stays open after the sound falls below the threshold, so short gaps do not chatter.", secondsText, knobRect (gateHoldId));
    addKnob (gateReleaseId, "Release", "Gate Release", "Gate Release: how quickly the gate closes after the hold.", secondsText, knobRect (gateReleaseId));

    for (auto& t : toggles)
        t.button.setColour (juce::TextButton::textColourOffId, Colours::text);

    // songsmithstudio#9: the four equaliser bands are colour coded, as on a console: low dark neutral, low-mid cyan, high-mid green, high red.
    for (auto& [id, index] : knobIds)
    {
        const juce::String name (id);
        auto& k = knobs[(size_t) index];
        if (name.startsWith ("lf_"))        k.setAccent (lowBand);
        else if (name.startsWith ("lmf_"))  k.setAccent (lowMidBand);
        else if (name.startsWith ("hmf_"))  k.setAccent (highMidBand);
        else if (name.startsWith ("hf_"))   k.setAccent (highBand);
    }

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    bypassButton.setTitle ("Bypass");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red
    content.addAndMakeVisible (bypassButton);

    meterColumn.getOutput().setTooltip ("Output level of the left and right channels, after everything including Output Trim and Bypass. The number under the warning is the highest sample level held for 1.5 seconds. "
                                        "This is sample peak, not true peak. Nothing to do unless the clip warning lights: then lower Output Trim. Click to clear the held number.");
    outputPanel.getTrim().textFromValueFunction = [] (double v) { return signedDb ((float) v); };
    outputPanel.getTrim().valueFromTextFunction = [] (const juce::String& t) { return t.retainCharacters ("0123456789.-+").getDoubleValue(); };
    outputPanel.getTrim().setTextBoxIsEditable (true);
    outputPanel.getTrim().setRange (SongsmithDesk::minTrimDb, SongsmithDesk::maxTrimDb, 0.1);
    outputPanel.getTrim().updateText();
    outputPanel.setAutoGainExplanation ("On the strip it compares the loudness going in with the loudness coming out of the filters, equaliser and dynamics, and corrects the difference by up to 12 dB either way, so a boost is not judged better just because it is louder.");
    content.addAndMakeVisible (outputPanel);

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

juce::Rectangle<int> SongsmithAnalogDeskEditor::getModuleBounds (int index) const noexcept
{
    const int xs[4] = { EL::inputX, EL::dynamicsX, EL::eqX, EL::outputX };
    const int ws[4] = { EL::inputWidth, EL::dynamicsWidth, EL::eqWidth, EL::outputWidth };
    index = juce::jlimit (0, 3, index);
    return { xs[index], EL::moduleTop, ws[index], EL::moduleBottom - EL::moduleTop };
}

void SongsmithAnalogDeskEditor::layoutContent()
{
    // The Output panel fills the Output module.
    outputPanel.setBounds (getModuleBounds (3).reduced (8, 4));

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

    // songsmithstudio#9: a console channel strip. Four modules in signal-flow order on muted navy-grey metal, with steel dividers between them, a
    // lit upper edge, a fine brushed grain and a screw at each corner. All vector, so it scales.
    const juce::Colour metalTop { 0xff3a4454 }, metalBottom { 0xff252c38 }, steel { 0xff66707f };
    for (int m = 0; m < 4; ++m)
    {
        const auto module = getModuleBounds (m).toFloat();
        g.setGradientFill (juce::ColourGradient (metalTop, module.getX(), module.getY(), metalBottom, module.getX(), module.getBottom(), false));
        g.fillRoundedRectangle (module, 7.0f);
    }
    {
        static const Grain grain;
        grain.draw (g);
    }
    for (int m = 0; m < 4; ++m)
    {
        const auto module = getModuleBounds (m).toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawRoundedRectangle (module.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (module.translated (0.0f, 1.0f).reduced (0.5f), 7.0f, 1.0f);

        // the module's header strip: a steel rule under the title
        g.setColour (steel.withAlpha (0.65f));
        g.fillRect (module.getX() + 8.0f, module.getY() + (float) EL::headerHeight, module.getWidth() - 16.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (module.getX() + 8.0f, module.getY() + (float) EL::headerHeight + 1.5f, module.getWidth() - 16.0f, 1.0f);

        for (auto p : { juce::Point<float> (module.getX() + 7.0f, module.getBottom() - 7.0f), juce::Point<float> (module.getRight() - 7.0f, module.getBottom() - 7.0f) })
        {
            g.setColour (steel.withAlpha (0.55f));
            g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine (p.x - 2.0f, p.y + 1.0f, p.x + 2.0f, p.y - 1.0f, 1.0f);
        }
    }
    // steel dividers in the gaps between the modules
    for (int m = 0; m < 3; ++m)
    {
        const auto left = getModuleBounds (m), right = getModuleBounds (m + 1);
        const float x = (float) (left.getRight() + right.getX()) * 0.5f;
        g.setColour (steel.withAlpha (0.8f));
        g.fillRect (x - 1.0f, (float) EL::moduleTop + 4.0f, 1.0f, (float) (EL::moduleBottom - EL::moduleTop) - 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (x, (float) EL::moduleTop + 4.0f, 1.0f, (float) (EL::moduleBottom - EL::moduleTop) - 8.0f);
    }

    Songsmith::UI::LookAndFeelBase::drawTitleBar (g, r.withHeight (Songsmith::UI::titleBarHeight));
    Songsmith::UI::drawTitle (g, titleBounds.toFloat(), JucePlugin_Name, Colours::text, Colours::accent);

    auto moduleTitle = [&g] (const juce::String& text, int moduleIndex, const SongsmithAnalogDeskEditor& ed)
    {
        const auto m = ed.getModuleBounds (moduleIndex);
        g.setColour (Colours::text);
        g.setFont (font (Type::heading, Weight::SemiBold));
        g.drawText (text, m.getX() + 10, m.getY() + 2, m.getWidth() - 20, lineHeight, juce::Justification::centredLeft);
    };
    moduleTitle ("Input and Filters", 0, *this);
    moduleTitle ("Dynamics", 1, *this);
    moduleTitle ("Equaliser", 2, *this);

    auto caption = [&g] (const juce::String& text, int x, int y, int width, juce::Colour colour)
    {
        g.setColour (colour);
        g.setFont (font (Type::control, Weight::SemiBold));
        g.drawText (text, x, y, width, lineHeight, juce::Justification::centredLeft);
    };
    const int captionY = EL::moduleTop + EL::headerHeight + 3;
    caption ("Gate", EL::dynamicsX + 14, captionY, EL::cellWidth - 8, Colours::accent);
    caption ("Compressor", EL::dynamicsX + 14 + EL::cellWidth, captionY, EL::cellWidth - 8, Colours::accent);

    // The four equaliser bands, colour coded: a bar of the band's colour above its name, and the name in that colour.
    const juce::String bandNames[4] = { "Low", "Low-Mid", "High-Mid", "High" };
    const juce::Colour bandColours[4] = { lowBand, lowMidBand, highMidBand, highBand };
    for (int i = 0; i < 4; ++i)
    {
        const int x = EL::eqX + 10 + i * EL::cellWidth;
        g.setColour (bandColours[i]);
        g.fillRoundedRectangle ((float) x + 2.0f, (float) EL::moduleTop + (float) EL::headerHeight + 6.0f - 4.0f, (float) EL::cellWidth - 8.0f, 2.5f, 1.2f);
        caption (bandNames[i], x + 4, captionY + 2, i == 0 || i == 3 ? 46 : EL::cellWidth - 8, bandColours[i]);
    }
}

void SongsmithAnalogDeskEditor::advanceMeter (float seconds)
{
    {   // The correction Auto Gain is applying is a measured fact, shown in the tooltip (the button is too small for it).
        juce::String ag ("On the strip it compares the loudness going in with the loudness coming out of the filters, equaliser and dynamics, and corrects the difference by up to 12 dB either way, so a boost is not judged better just because it is louder.");
        if (proc.isAutoGainOn())
        {
            const float db = proc.getAutoGainAppliedDb();
            ag << " Right now it is adding " << (db > 0.05f ? "+" : "") << juce::String (std::abs (db) < 0.05f ? 0.0f : db, 1) << " dB.";
        }
        if (ag != lastAutoGainText)
        {
            lastAutoGainText = ag;
            outputPanel.setAutoGainExplanation (ag);
        }
    }
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
