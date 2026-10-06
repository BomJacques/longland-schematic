#pragma once

#include <JuceHeader.h>
#include "LonglandDSP.h"
#include "ParameterSchema.h"

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
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboardState;
    float getOutputLevel() const noexcept { return outputLevel.load(std::memory_order_relaxed); }
    float getGainReduction() const noexcept { return gainReduction.load(std::memory_order_relaxed); }
    std::uint32_t getNoteOnSerial(int note) const noexcept { return noteOnSerial[static_cast<std::size_t>(note)].load(std::memory_order_relaxed); }
    void requestPanic() noexcept { panicRequested.store(true,std::memory_order_relaxed); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateEngineParameters();
    void renderSpan(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void handleMidi(const juce::MidiMessage&);

    longland::SynthEngine engine;
    std::atomic<int> currentProgram { 0 };
    std::array<std::atomic<float>*, longland::floatControls.size()> parameterValues {};
    std::array<std::uint16_t, 128> heldNotes {}, sustainedNotes {};
    std::array<bool, 16> sustainPedal {};
    std::atomic<float> outputLevel { 0.0f }, gainReduction { 0.0f };
    std::array<std::atomic<std::uint32_t>,128> noteOnSerial {};
    std::atomic<bool> panicRequested { false };
    float meterPower = 0.0f, midiExpression = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LonglandSchematicAudioProcessor)
};

