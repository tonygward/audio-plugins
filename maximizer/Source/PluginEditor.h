#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Temporary editor (step 01): JUCE's generic parameter list, so every parameter can be
// tried in a host. The Songsmith editor replaces it in step 04.
class SongsmithMaximiserAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SongsmithMaximiserAudioProcessorEditor (SongsmithMaximiserAudioProcessor&);
    ~SongsmithMaximiserAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SongsmithMaximiserAudioProcessorEditor)
};
