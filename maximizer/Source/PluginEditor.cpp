#include "PluginEditor.h"

SongsmithMaximiserAudioProcessorEditor::SongsmithMaximiserAudioProcessorEditor (SongsmithMaximiserAudioProcessor& p)
    : AudioProcessorEditor (p), generic (p)
{
    addAndMakeVisible (generic);
    setSize (420, 200);
}

void SongsmithMaximiserAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void SongsmithMaximiserAudioProcessorEditor::resized()
{
    generic.setBounds (getLocalBounds());
}
