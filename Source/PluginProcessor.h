#pragma once

#include <JuceHeader.h>
#include "LonglandDSP.h"

class LonglandSchematicAudioProcessor final : public juce::AudioProcessor
{
public:
    LonglandSchematicAudioProcessor();
    ~LonglandSchematicAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;
    longland::DebugState getDebugState() const { return engine.getDebugState(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateEngineParameters();
    void renderSpan(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    longland::SynthEngine engine;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LonglandSchematicAudioProcessor)
};

