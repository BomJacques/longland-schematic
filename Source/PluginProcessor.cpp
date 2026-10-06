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
    for (std::size_t i = 0; i < longland::floatControls.size(); ++i)
        parameterValues[i] = state.getRawParameterValue(longland::floatControls[i].id);
    for (auto& serial : noteOnSerial) serial.store(0,std::memory_order_relaxed);
    setCurrentProgram(0); // The initial sound must match the displayed factory state.
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
    const longland::Parameters defaults;
    for (std::size_t i = 12; i < longland::floatControls.size(); ++i)
    {
        const auto& c = longland::floatControls[i];
        parameters.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { c.id, 1 }, c.name,
            juce::NormalisableRange<float> { c.minimum, c.maximum, c.interval, c.skew }, defaults.*(c.member), c.unit));
    }
    return { parameters.begin(), parameters.end() };
}

void LonglandSchematicAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    keyboardState.reset();
    heldNotes.fill(0); sustainedNotes.fill(0); sustainPedal.fill(false);
    meterPower = 0.0f; midiExpression = 1.0f;
    outputLevel.store(0.0f); gainReduction.store(0.0f);
    updateEngineParameters();
    engine.prepare(newSampleRate, samplesPerBlock);
}

void LonglandSchematicAudioProcessor::releaseResources()
{
    keyboardState.reset();
    outputLevel.store(0.0f); gainReduction.store(0.0f);
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
    for (std::size_t i = 0; i < longland::floatControls.size(); ++i)
        parameters.*(longland::floatControls[i].member) = parameterValues[i]->load(std::memory_order_relaxed);
    parameters.filterMode = state.getRawParameterValue(ids::filterMode)->load();
    parameters.noiseType = static_cast<longland::NoiseType>(juce::roundToInt(state.getRawParameterValue(ids::noiseType)->load()));
    parameters.expression *= midiExpression;
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
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);
    if (panicRequested.exchange(false,std::memory_order_relaxed))
    {
        keyboardState.reset(); heldNotes.fill(0); sustainedNotes.fill(0); sustainPedal.fill(false);
        for (int note=0;note<128;++note) engine.noteOff(note,false);
        midi.clear();
    }
    updateEngineParameters();

    int rendered = 0;
    for (const auto metadata : midi)
    {
        const int position = juce::jlimit(rendered, buffer.getNumSamples(), metadata.samplePosition);
        renderSpan(buffer, rendered, position - rendered);
        rendered = position;

        handleMidi(metadata.getMessage());
    }
    renderSpan(buffer, rendered, buffer.getNumSamples() - rendered);
    double sum = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getReadPointer(channel)[sample];
            sum += static_cast<double>(value) * value;
        }
    const auto power = static_cast<float>(sum / (buffer.getNumChannels() * buffer.getNumSamples()));
    const auto coefficient = static_cast<float>(1.0 - std::exp(-buffer.getNumSamples() / (std::max(8000.0, getSampleRate()) * .3)));
    meterPower += (power - meterPower) * coefficient;
    outputLevel.store(std::sqrt(std::max(0.0f, meterPower)), std::memory_order_relaxed);
    gainReduction.store(engine.getDebugState().masterGainReductionDb, std::memory_order_relaxed);
    midi.clear(); // This instrument consumes MIDI and does not advertise MIDI output.
}

void LonglandSchematicAudioProcessor::handleMidi(const juce::MidiMessage& message)
{
    const auto channel = juce::jlimit(0, 15, message.getChannel() - 1);
    const auto bit = static_cast<std::uint16_t>(1u << channel);
    const auto releaseUnheld = [this] (int note, bool tail = true)
    {
        if ((heldNotes[static_cast<std::size_t>(note)] | sustainedNotes[static_cast<std::size_t>(note)]) == 0)
            engine.noteOff(note, tail);
    };
    if (message.isNoteOn())
    {
        const auto note = static_cast<std::size_t>(message.getNoteNumber());
        heldNotes[note] |= bit; sustainedNotes[note] &= static_cast<std::uint16_t>(~bit);
        engine.noteOn(static_cast<int>(note), message.getFloatVelocity());
        noteOnSerial[note].fetch_add(1,std::memory_order_relaxed);
    }
    else if (message.isNoteOff())
    {
        const auto note = static_cast<std::size_t>(message.getNoteNumber());
        heldNotes[note] &= static_cast<std::uint16_t>(~bit);
        if (sustainPedal[static_cast<std::size_t>(channel)]) sustainedNotes[note] |= bit;
        releaseUnheld(static_cast<int>(note));
    }
    else if (message.isControllerOfType(64))
    {
        sustainPedal[static_cast<std::size_t>(channel)] = message.getControllerValue() >= 64;
        if (!sustainPedal[static_cast<std::size_t>(channel)])
            for (int note = 0; note < 128; ++note)
            {
                sustainedNotes[static_cast<std::size_t>(note)] &= static_cast<std::uint16_t>(~bit);
                releaseUnheld(note);
            }
    }
    else if (message.isControllerOfType(11))
    {
        midiExpression = message.getControllerValue() / 127.0f;
        updateEngineParameters();
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        sustainPedal[static_cast<std::size_t>(channel)] = false;
        for (int note = 0; note < 128; ++note)
        {
            heldNotes[static_cast<std::size_t>(note)] &= static_cast<std::uint16_t>(~bit);
            sustainedNotes[static_cast<std::size_t>(note)] &= static_cast<std::uint16_t>(~bit);
            releaseUnheld(note, !message.isAllSoundOff());
        }
    }
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
    for (std::size_t i = 12; i < longland::floatControls.size(); ++i)
    {
        const auto& c = longland::floatControls[i];
        set(c.id, p.*(c.member));
    }
}

void LonglandSchematicAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto tree = state.copyState();
    tree.setProperty("program", currentProgram.load(), nullptr);
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
            // Old v0.3 sessions have no added controls. Restore their neutral defaults,
            // not whatever settings happen to be left in this plug-in instance.
            const longland::Parameters defaults;
            for (const auto& c : longland::floatControls)
                if (!tree.getChildWithProperty("id", c.id).isValid())
                {
                    juce::ValueTree child("PARAM");
                    child.setProperty("id", c.id, nullptr);
                    child.setProperty("value", defaults.*(c.member), nullptr);
                    tree.appendChild(child, nullptr);
                }
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
