#include "PluginEditor.h"
#include "FETEngine.h"

using namespace FET;
using namespace FET::UI;

namespace
{
    juce::String dbText (float v) { return (v > 0.005f ? "+" : "") + juce::String (v, 1) + " dB"; }

    juce::String timeText (double seconds)
    {
        if (seconds < 1.0e-3) return juce::String (juce::roundToInt (seconds * 1.0e6)) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb5s"));
        if (seconds < 1.0)    return juce::String (juce::roundToInt (seconds * 1.0e3)) + " ms";
        return juce::String (seconds, 2) + " s";
    }

    std::vector<std::pair<float, juce::String>> knobMarks()
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int i = 1; i <= 7; ++i)
            m.push_back ({ (float) i, juce::String (i) });
        return m;
    }

    std::vector<std::pair<float, juce::String>> dbMarks (std::initializer_list<int> values)
    {
        std::vector<std::pair<float, juce::String>> m;
        for (int v : values)
            m.push_back ({ (float) v, v > 0 ? "+" + juce::String (v) : juce::String (v) });
        return m;
    }

    // Layout, in design units (FET::designWidth x FET::designHeight).
    const juce::Rectangle<int> inputArea   { 24, 64, 170, 270 };
    const juce::Rectangle<int> outputArea  { 200, 64, 170, 270 };
    const juce::Rectangle<int> attackArea  { 382, 56, 112, 138 };
    const juce::Rectangle<int> releaseArea { 382, 200, 112, 138 };
    const juce::Rectangle<int> ratioArea   { 510, 72, 66, 236 };
    const juce::Rectangle<int> meterArea   { 592, 64, 344, 170 };   // ACOMP-08: about 45 % more face than the original 296 x 132
    const juce::Rectangle<int> meterModes  { 592, 242, 344, 28 };   // a row under the meter
    const juce::Rectangle<int> grOffArea   { 592, 314, 150, 32 };

    // One line each: what the ratio does to the level (above the point where compression starts),
    // and what it is good for. Listening suggestions, not measurements.
    juce::String ratioTip (int index)
    {
        static const char* const what[] = {
            "the gentlest. For every 4 dB the sound rises above the point where compression starts, it comes out 1 dB louder. A good starting point for steady control of vocals and bass.",
            "firmer. 8 dB in, 1 dB out above that point. Often used to keep drums and lively vocals punchy.",
            "strong. 12 dB in, 1 dB out above that point. For heavier control of drums and vocals.",
            "the strongest, close to a limiter. 20 dB in, 1 dB out above that point. For hard peak control or a deliberately squashed sound." };
        return "Ratio " + ratioNames()[index] + ": " + what[index];
    }

    // A fine brushed-metal grain for the faceplate, drawn as vectors so it scales with the editor and
    // stays sharp at every interface size. Built once; the same grain every time.
    struct Grain
    {
        juce::Path light, dark;
        Grain()
        {
            juce::Random random (1176);
            for (float y = (float) Songsmith::UI::titleBarHeight + 2.0f; y < (float) contentHeight - 6.0f; y += 2.0f)
                for (float x = 10.0f; x < (float) designWidth - 10.0f;)
                {
                    const float length = 24.0f + random.nextFloat() * 120.0f;
                    (random.nextBool() ? light : dark).addRectangle (x, y, juce::jmin (length, (float) designWidth - 10.0f - x), 0.8f);
                    x += length + random.nextFloat() * 70.0f;
                }
        }
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
FETKnob::FETKnob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& n,
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

void FETKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (22);
    r.removeFromBottom (24);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    const int size = side - juce::roundToInt ((float) side * 0.32f);   // room for the printed scale
    slider.setBounds (juce::Rectangle<int> (size, size).withCentre (r.getCentre()));
}

void FETKnob::paint (juce::Graphics& g)
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
        const auto t = c.getPointOnCircumference (radius + 18.0f, angle);
        g.drawText (label, juce::Rectangle<float> (24.0f, 14.0f).withCentre (t), juce::Justification::centred);
    }
}

//==============================================================================
SongsmithAnalogCompressorEditor::SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      input (p.apvts, ID::input, "Input", dbMarks ({ -24, 0, 24, 48 }), [] (float v) { return dbText (v); }),
      output (p.apvts, ID::output, "Output", dbMarks ({ -36, -12, 0, 12, 24 }), [] (float v) { return dbText (v); }),
      attack (p.apvts, ID::attack, "Attack", knobMarks(),
              [] (float v) { return juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + timeText (Engine::attackSeconds (v)); }),
      release (p.apvts, ID::release, "Release", knobMarks(),
               [] (float v) { return juce::String (v, 1) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + timeText (Engine::releaseSeconds (v)); }),
      ratioAttachment (*p.apvts.getParameter (ID::ratio), [this] (float v) { ratioIndex = juce::roundToInt (v); refreshButtons(); }),
      allButtonsAttachment (*p.apvts.getParameter (ID::allButtons), [this] (float v) { allButtonsOn = v >= 0.5f; refreshButtons(); }),
      grOffAttachment (p.apvts, ID::grOff, grOffButton),
      bypassAttachment (p.apvts, ID::bypass, bypassButton),
      oversamplingAttachment (p.apvts, ID::oversampling, oversampling)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (auto* k : { &input, &output, &attack, &release })
        content.addAndMakeVisible (*k);

    // Ratio buttons: one press selects that ratio (and leaves All Buttons); "All Buttons" presses all
    // four at once (UM p. 9). Each press is one automation gesture.
    const juce::StringArray ratioText { "4:1", "8:1", "12:1", "20:1" };
    for (int i = 0; i < 4; ++i)
    {
        auto b = std::make_unique<juce::TextButton> (ratioText[i]);
        b->setTooltip (ratioTip (i));
        b->onClick = [this, i]
        {
            if (allButtonsOn)
                allButtonsAttachment.setValueAsCompleteGesture (0.0f);
            ratioAttachment.setValueAsCompleteGesture ((float) i);
        };
        content.addAndMakeVisible (*b);
        ratioButtons[(size_t) i] = std::move (b);
    }
    allButton.setTooltip ("All: presses every ratio button at once, for a more aggressive, harder and more distorted sound. Press a single ratio to go back.");
    allButton.onClick = [this] { allButtonsAttachment.setValueAsCompleteGesture (allButtonsOn ? 0.0f : 1.0f); };
    content.addAndMakeVisible (allButton);

    for (auto* b : { &grOffButton, &bypassButton })
    {
        b->setClickingTogglesState (true);
        content.addAndMakeVisible (*b);
    }
    grOffButton.setTooltip ("Gain Reduction Off: stops compressing, but the sound still passes through the amplifiers and keeps their colour");
    bypassButton.setTooltip ("Bypass: the sound passes through untouched");
    Songsmith::UI::styleBypassButton (bypassButton);   // the suite's Bypass red

    for (int m = 0; m < 3; ++m)
    {
        meterButtons[(size_t) m].onClick = [this, m] { setMeterMode (m); };
        meterButtons[(size_t) m].setTooltip (m == 0 ? "Meter: how much the compressor is turning the level down"
                                                    : "Meter: the output level, with 0 on the scale at " + juce::String (m == 1 ? "+4" : "+8")
                                                          + " dBu (" + juce::String (m == 1 ? "the usual studio level" : "for hotter levels") + ")");
        content.addAndMakeVisible (meterButtons[(size_t) m]);
    }
    content.addAndMakeVisible (meter);

    for (int i = 0; i < oversamplingNames().size(); ++i)
        oversampling.addItem (oversamplingText (i), i + 1);
    oversampling.setTooltip ("Runs the compressor at a higher rate inside the plugin, for a cleaner top end when compressing hard. "
                             "Uses more processing power and adds a little delay.");
    content.addAndMakeVisible (oversampling);
    oversampling.setSelectedItemIndex ((int) p.apvts.getRawParameterValue (ID::oversampling)->load(), juce::dontSendNotification);

    for (int i = 0; i < (int) std::size (uiScales); ++i)
        sizeBox.addItem ("Interface size: " + juce::String (juce::roundToInt (uiScales[i] * 100.0f)) + " %", i + 1);
    sizeBox.setTooltip ("Interface size: makes the whole window larger or smaller");
    sizeBox.onChange = [this] { setUiScale (uiScales[(size_t) juce::jmax (0, sizeBox.getSelectedId() - 1)]); };
    content.addAndMakeVisible (sizeBox);

    presetBox.setTooltip ("Starting points and your own presets");
    presetBox.onChange = [this] { presetChosen(); };
    content.addAndMakeVisible (presetBox);
    rebuildPresetMenu();

    input.slider.setTooltip ("Input: how hard you drive the compressor. Turn it up for more compression; there is no threshold control. "
                             "Double-click for the default.");
    output.slider.setTooltip ("Output: the level after compression. Double-click for the default.");
    attack.slider.setTooltip ("Attack: how fast it reacts when the sound gets louder. Higher numbers are faster. Double-click for the default.");
    release.slider.setTooltip ("Release: how fast it lets go when the sound gets quieter. Higher numbers are faster. Double-click for the default.");

    ratioAttachment.sendInitialUpdate();
    allButtonsAttachment.sendInitialUpdate();
    setMeterMode (proc.meterMode.load());
    meter.ballistics.reset (needleFraction (0.0f));

    diagnostics = std::make_unique<Songsmith::Diagnostics::Bar> (proc, proc.loadMeter, JucePlugin_Name);
    content.addAndMakeVisible (*diagnostics);

    layoutContent();
    setUiScale (proc.uiScale.load());
    startTimerHz (30);
}

SongsmithAnalogCompressorEditor::~SongsmithAnalogCompressorEditor()
{
    stopTimer();
    diagnostics.reset();   // also stops the audio thread timing the blocks
    setLookAndFeel (nullptr);
}

void SongsmithAnalogCompressorEditor::setUiScale (float scale)
{
    uiScale = snapUiScale (scale);
    proc.uiScale.store (uiScale);
    for (int i = 0; i < (int) std::size (uiScales); ++i)
        if (uiScales[i] == uiScale)
            sizeBox.setSelectedId (i + 1, juce::dontSendNotification);
    setSize (juce::roundToInt ((float) designWidth * uiScale), juce::roundToInt ((float) designHeight * uiScale));
}

void SongsmithAnalogCompressorEditor::resized()
{
    content.setBounds (0, 0, designWidth, designHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth,
                                                        (float) getHeight() / (float) designHeight));
}

void SongsmithAnalogCompressorEditor::layoutContent()
{
    input.setBounds (inputArea);
    output.setBounds (outputArea);
    attack.setBounds (attackArea);
    release.setBounds (releaseArea);

    // Hardware order, top to bottom: 20, 12, 8, 4 (then All).
    auto col = ratioArea;
    const int h = 40, gap = 7;
    for (int i = 3; i >= 0; --i)
    {
        ratioButtons[(size_t) i]->setBounds (col.removeFromTop (h));
        col.removeFromTop (gap);
    }
    allButton.setBounds (col.removeFromTop (h));

    meter.setBounds (meterArea);
    auto modes = meterModes;
    const int gainReductionWidth = 116, modeGap = 8;
    meterButtons[0].setBounds (modes.removeFromLeft (gainReductionWidth));
    modes.removeFromLeft (modeGap);
    const int outputWidth = (modes.getWidth() - modeGap) / 2;
    meterButtons[1].setBounds (modes.removeFromLeft (outputWidth));
    modes.removeFromLeft (modeGap);
    meterButtons[2].setBounds (modes);

    grOffButton.setBounds (grOffArea);   // the Low Cut control will sit to its right (FET::lowCutX), when ACOMP-07 adds it

    // Top bar: global Bypass at the top-right, in the same place and size as in the other plugins (SUI-13).
    bypassButton.setBounds (designWidth - 16 - 84, 14, 84, 26);
    presetBox.setBounds (292, 14, 170, 26);
    oversampling.setBounds (presetBox.getRight() + 8, 14, 160, 26);
    sizeBox.setBounds (oversampling.getRight() + 8, 14, 156, 26);

    diagnostics->setBounds (0, FET::contentHeight, FET::designWidth, FET::footerHeight);
}

void SongsmithAnalogCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panel);
}

void SongsmithAnalogCompressorEditor::paintContent (juce::Graphics& g)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) designHeight);
    Songsmith::UI::LookAndFeelBase::drawPanel (g, r);

    // Faceplate: a fine brushed grain, a lit inner edge and four screws. All vector, so it scales.
    {
        static const Grain grain;
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.fillPath (grain.light);
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        g.fillPath (grain.dark);

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
    Songsmith::UI::drawTitle (g, { Songsmith::UI::titleInset, 0.0f, 360.0f, Songsmith::UI::titleBarHeight },
                              JucePlugin_Name, Colours::text, Colours::accent);

    // Section rules and captions.
    g.setColour (Colours::panelLight.brighter (0.1f));
    for (float x : { 376.0f, 502.0f })
        g.drawLine (x, 70.0f, x, 340.0f, 1.0f);
    g.drawLine ((float) meterArea.getX() - 8.0f, 70.0f, (float) meterArea.getX() - 8.0f, 340.0f, 1.0f);

    g.setColour (Colours::textDim);
    g.setFont (font (Type::heading, Weight::Medium));
    g.drawText ("Ratio", juce::Rectangle<float> ((float) ratioArea.getX(), 52.0f, (float) ratioArea.getWidth(), 18.0f), juce::Justification::centred);

    // Numeric GR under the needle (the needle has VU ballistics; this shows the value).
    g.setColour (Colours::text);
    g.setFont (numbers (Type::heading, Weight::Medium));
    g.drawText (gainReductionText (grDisplayDb),
                juce::Rectangle<float> ((float) meterArea.getX(), (float) meterModes.getBottom() + 6.0f, (float) meterArea.getWidth(), 20.0f),
                juce::Justification::centred);

    if (allButtonsOn)
    {
        g.setColour (Colours::accent);
        g.setFont (font (Type::hint, Weight::SemiBold));
        g.drawText ("All ratio buttons on", juce::Rectangle<float> ((float) meterArea.getX(), (float) meterModes.getBottom() + 26.0f, (float) meterArea.getWidth(), 16.0f),
                    juce::Justification::centred);
    }

    // The listening cue of the loaded starting point.
    g.setColour (Colours::textDim);
    g.setFont (font (Type::hint));
    const auto hint = proc.presets.currentHint.isNotEmpty() ? proc.presets.currentHint
                                                             : juce::String ("Double-click a knob for its default.");
    g.drawFittedText (hint, juce::Rectangle<int> (24, 356, designWidth - 48, 36), juce::Justification::centredLeft, 2);
}

juce::String SongsmithAnalogCompressorEditor::gainReductionText (float db)
{
    return "Gain Reduction " + juce::String (juce::jmax (0.0f, db), 1) + " dB";
}

juce::String SongsmithAnalogCompressorEditor::meterModeName (int mode)
{
    return mode == 0 ? "Gain Reduction" : mode == 1 ? "Output +4" : "Output +8";
}

juce::String SongsmithAnalogCompressorEditor::oversamplingText (int index)
{
    const auto times = juce::String::charToString (0x00d7);   // multiplication sign
    return index <= 0 ? juce::String ("Oversampling: off") : "Oversampling: " + juce::String (1 << index) + times;
}

void SongsmithAnalogCompressorEditor::refreshButtons()
{
    for (int i = 0; i < 4; ++i)
        ratioButtons[(size_t) i]->setToggleState (allButtonsOn || i == ratioIndex, juce::dontSendNotification);
    allButton.setToggleState (allButtonsOn, juce::dontSendNotification);
    content.repaint();
}

void SongsmithAnalogCompressorEditor::setMeterMode (int mode)
{
    mode = juce::jlimit (0, 2, mode);
    proc.meterMode.store (mode);
    for (int m = 0; m < 3; ++m)
        meterButtons[(size_t) m].setToggleState (m == mode, juce::dontSendNotification);
    meter.setModeLabel (meterModeName (mode));
}

void SongsmithAnalogCompressorEditor::advanceMeter (float seconds)
{
    const float gr = proc.takeGainReductionDb();
    const float outDb = proc.takeOutputRmsDb();
    const auto mode = (MeterMode) proc.meterMode.load();
    meter.setReading (meterReadingVu (mode, gr, outDb, allButtonsOn), seconds);

    // The numeric readout: instant rise, ~300 ms fall.
    grDisplayDb = gr > grDisplayDb ? gr : grDisplayDb * 0.9f + gr * 0.1f;
    content.repaint (meterArea.getX(), meterArea.getY(), meterArea.getWidth(), meterModes.getBottom() + 50 - meterArea.getY());
}

FETKnob& SongsmithAnalogCompressorEditor::getKnob (const juce::String& id)
{
    if (id == ID::input)  return input;
    if (id == ID::output) return output;
    if (id == ID::attack) return attack;
    return release;
}

//==============================================================================
void SongsmithAnalogCompressorEditor::rebuildPresetMenu()
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
}

void SongsmithAnalogCompressorEditor::presetChosen()
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
        if (! proc.presets.loadFile (userPresetFiles[id - firstUserId], error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset", error);
    }
    else if (id >= 1 && id <= (int) Presets::factory().size())
    {
        proc.presets.loadFactory (id - 1);
    }

    rebuildPresetMenu();
    content.repaint();
}

void SongsmithAnalogCompressorEditor::savePresetAsync()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon, this);
    // Starting points are read-only, so don't suggest saving over one's name.
    const auto suggested = Presets::isFactoryName (proc.presets.currentName) ? juce::String() : proc.presets.currentName;
    saveDialog->addTextEditor ("name", suggested, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SongsmithAnalogCompressorEditor> (this)] (int result)
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
                                                  .withButton ("Replace")
                                                  .withButton ("Cancel")
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
