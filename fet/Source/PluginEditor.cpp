#include "PluginEditor.h"

using namespace FET;

SongsmithAnalogCompressorEditor::SongsmithAnalogCompressorEditor (SongsmithAnalogCompressorProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    const auto setUpKnob = [this] (Knob& k, const char* id, const juce::String& name)
    {
        k.label.setText (name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<SliderAttachment> (proc.apvts, id, k.slider);
    };

    setUpKnob (input,   ID::input,   "Input");
    setUpKnob (output,  ID::output,  "Output");
    setUpKnob (attack,  ID::attack,  "Attack");
    setUpKnob (release, ID::release, "Release");

    ratio.addItemList (ratioNames(), 1);
    ratioLabel.setText ("Ratio", juce::dontSendNotification);
    ratioLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (ratio);
    addAndMakeVisible (ratioLabel);
    ratioAttachment = std::make_unique<ComboAttachment> (proc.apvts, ID::ratio, ratio);

    for (auto* b : { &allButtons, &grOff, &bypass })
        addAndMakeVisible (*b);

    allButtonsAttachment = std::make_unique<ButtonAttachment> (proc.apvts, ID::allButtons, allButtons);
    grOffAttachment      = std::make_unique<ButtonAttachment> (proc.apvts, ID::grOff, grOff);
    bypassAttachment     = std::make_unique<ButtonAttachment> (proc.apvts, ID::bypass, bypass);

    grLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (grLabel);
    startTimerHz (30);

    setSize (620, 260);
}

void SongsmithAnalogCompressorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c1f));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (13.0f);
    g.drawText (juce::String (JucePlugin_Name) + "  (debug editor)", getLocalBounds().removeFromTop (28).reduced (12, 0),
                juce::Justification::centredLeft);
}

void SongsmithAnalogCompressorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    grLabel.setBounds (area.removeFromTop (20).removeFromRight (200));

    auto buttons = area.removeFromBottom (32);
    for (auto* b : { &allButtons, &grOff, &bypass })
        b->setBounds (buttons.removeFromLeft (170));

    const int w = area.getWidth() / 5;
    for (auto* k : { &input, &attack, &release, &output })
    {
        auto col = area.removeFromLeft (w);
        k->label.setBounds (col.removeFromTop (20));
        k->slider.setBounds (col.reduced (4));
    }

    auto col = area;
    ratioLabel.setBounds (col.removeFromTop (20));
    ratio.setBounds (col.removeFromTop (28).reduced (4, 0));
}

void SongsmithAnalogCompressorEditor::timerCallback()
{
    // Debug readout: instant attack, ~300 ms fall, so short peaks stay readable.
    const float gr = proc.takeGainReductionDb();
    grDisplayDb = gr > grDisplayDb ? gr : grDisplayDb * 0.9f + gr * 0.1f;
    grLabel.setText ("GR " + juce::String (-grDisplayDb, 1) + " dB", juce::dontSendNotification);
}
