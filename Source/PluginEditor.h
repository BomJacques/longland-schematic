#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class LonglandSchematicAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     private juce::Timer
{
public:
    explicit LonglandSchematicAudioProcessorEditor(LonglandSchematicAudioProcessor&);
    ~LonglandSchematicAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void configureDial(juce::Slider&, juce::Label&, const juce::String&, const char* parameterId);
    void drawModule(juce::Graphics&, juce::Rectangle<int>, const juce::String&, const juce::String&);

    LonglandSchematicAudioProcessor& processor;
    juce::ComboBox presetBox, oscillatorBox, filterBox, noiseTypeBox;
    juce::Label presetLabel, oscillatorLabel, filterLabel, noiseTypeLabel;

    juce::Slider age, body, drift, cutoff, resonance, attack, decay, sustain, release, ensemble, compressor, output;
    juce::Label ageLabel, bodyLabel, cutoffLabel, resonanceLabel, attackLabel, decayLabel,
                sustainLabel, releaseLabel, ensembleLabel, compressorLabel, outputLabel, driftLabel;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ComboAttachment> oscillatorAttachment, filterAttachment, noiseTypeAttachment;
    longland::DebugState debugState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LonglandSchematicAudioProcessorEditor)
};
