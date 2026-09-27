#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace ids
{
constexpr auto waveform = "waveform";
constexpr auto age = "age";
constexpr auto body = "body";
constexpr auto cutoff = "cutoff";
constexpr auto resonance = "resonance";
constexpr auto filterMode = "filterMode";
constexpr auto attack = "attack";
constexpr auto decay = "decay";
constexpr auto sustain = "sustain";
constexpr auto release = "release";
constexpr auto ensemble = "ensemble";
constexpr auto output = "output";
constexpr auto drift = "drift";
constexpr auto noiseType = "noiseType";
constexpr auto compressor = "compressor";
}

LonglandSchematicAudioProcessor::LonglandSchematicAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LonglandSchematicAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::waveform, 1 }, "Oscillator",
        juce::StringArray { "Saw", "Square", "Narrow Pulse", "Triangle", "Organ" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::age, 1 }, "Age", 0.0f, 1.0f, 0.38f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::body, 1 }, "Body", 0.0f, 1.0f, 0.42f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::cutoff, 1 }, "Cutoff",
        juce::NormalisableRange<float> { 60.0f, 16000.0f, 0.0f, 0.27f }, 3400.0f, "Hz"));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::resonance, 1 }, "Resonance", 0.0f, 0.92f, 0.12f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::filterMode, 1 }, "Filter",
        juce::StringArray { "Low-pass", "Band-pass" }, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::attack, 1 }, "Attack",
        juce::NormalisableRange<float> { 0.001f, 8.0f, 0.0f, 0.25f }, 0.015f, "s"));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::decay, 1 }, "Decay",
        juce::NormalisableRange<float> { 0.005f, 8.0f, 0.0f, 0.25f }, 0.42f, "s"));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::sustain, 1 }, "Sustain", 0.0f, 1.0f, 0.72f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::release, 1 }, "Release",
        juce::NormalisableRange<float> { 0.005f, 12.0f, 0.0f, 0.25f }, 0.55f, "s"));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::ensemble, 1 }, "Ensemble", 0.0f, 1.0f, 0.12f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::output, 1 }, "Output", 0.0f, 1.5f, 0.72f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::drift, 1 }, "Pitch Drift",
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.01f, 0.72f }, 8.0f, "cents"));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { ids::noiseType, 1 }, "Noise Type",
        juce::StringArray { "Off", "Thermal", "Pink", "Supply Ripple", "Control Voltage" }, 1));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { ids::compressor, 1 }, "Master Compressor",
        0.0f, 1.0f, 0.48f));
    return { parameters.begin(), parameters.end() };
}

void LonglandSchematicAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    engine.prepare(newSampleRate, samplesPerBlock);
}

void LonglandSchematicAudioProcessor::releaseResources()
{
}

bool LonglandSchematicAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void LonglandSchematicAudioProcessor::updateEngineParameters()
{
    longland::Parameters parameters;
    parameters.waveform = static_cast<longland::Waveform>(juce::roundToInt(state.getRawParameterValue(ids::waveform)->load()));
    parameters.age = state.getRawParameterValue(ids::age)->load();
    parameters.body = state.getRawParameterValue(ids::body)->load();
    parameters.cutoffHz = state.getRawParameterValue(ids::cutoff)->load();
    parameters.resonance = state.getRawParameterValue(ids::resonance)->load();
    parameters.filterMode = state.getRawParameterValue(ids::filterMode)->load();
    parameters.attackSeconds = state.getRawParameterValue(ids::attack)->load();
    parameters.decaySeconds = state.getRawParameterValue(ids::decay)->load();
    parameters.sustain = state.getRawParameterValue(ids::sustain)->load();
    parameters.releaseSeconds = state.getRawParameterValue(ids::release)->load();
    parameters.ensemble = state.getRawParameterValue(ids::ensemble)->load();
    parameters.outputGain = state.getRawParameterValue(ids::output)->load();
    parameters.driftAmountCents = state.getRawParameterValue(ids::drift)->load();
    parameters.noiseType = static_cast<longland::NoiseType>(juce::roundToInt(state.getRawParameterValue(ids::noiseType)->load()));
    parameters.compressorAmount = state.getRawParameterValue(ids::compressor)->load();
    engine.setParameters(parameters);
}

void LonglandSchematicAudioProcessor::renderSpan(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;
    auto* left = buffer.getWritePointer(0, startSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample) : nullptr;
    engine.process(left, right, numSamples);
}

void LonglandSchematicAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    updateEngineParameters();

    int rendered = 0;
    for (const auto metadata : midi)
    {
        const int position = juce::jlimit(rendered, buffer.getNumSamples(), metadata.samplePosition);
        renderSpan(buffer, rendered, position - rendered);
        rendered = position;

        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            engine.noteOn(message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            engine.noteOff(message.getNoteNumber(), true);
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            engine.allNotesOff();
    }
    renderSpan(buffer, rendered, buffer.getNumSamples() - rendered);
}

int LonglandSchematicAudioProcessor::getNumPrograms()
{
    return static_cast<int>(longland::factoryPresets().size());
}

const juce::String LonglandSchematicAudioProcessor::getProgramName(int index)
{
    if (juce::isPositiveAndBelow(index, getNumPrograms()))
        return longland::factoryPresets()[static_cast<std::size_t>(index)].name;
    return {};
}

void LonglandSchematicAudioProcessor::setCurrentProgram(int index)
{
    if (!juce::isPositiveAndBelow(index, getNumPrograms()))
        return;
    currentProgram = index;
    const auto& p = longland::factoryPresets()[static_cast<std::size_t>(index)].parameters;

    auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = state.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    set(ids::waveform, static_cast<float>(p.waveform));
    set(ids::age, p.age); set(ids::body, p.body); set(ids::cutoff, p.cutoffHz);
    set(ids::resonance, p.resonance); set(ids::filterMode, p.filterMode);
    set(ids::attack, p.attackSeconds); set(ids::decay, p.decaySeconds);
    set(ids::sustain, p.sustain); set(ids::release, p.releaseSeconds);
    set(ids::ensemble, p.ensemble); set(ids::output, p.outputGain);
    set(ids::drift, p.driftAmountCents); set(ids::noiseType, static_cast<float>(p.noiseType));
    set(ids::compressor, p.compressorAmount);
}

void LonglandSchematicAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto tree = state.copyState();
    tree.setProperty("program", currentProgram, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, destination);
}

void LonglandSchematicAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(state.state.getType()))
        {
            state.replaceState(tree);
            currentProgram = juce::jlimit(0, getNumPrograms() - 1, static_cast<int>(tree.getProperty("program", 0)));
        }
    }
}

juce::AudioProcessorEditor* LonglandSchematicAudioProcessor::createEditor()
{
    return new LonglandSchematicAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LonglandSchematicAudioProcessor();
}
