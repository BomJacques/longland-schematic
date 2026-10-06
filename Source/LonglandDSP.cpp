#include "LonglandDSP.h"
#include "ParameterSchema.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace longland
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr float twoPi = 2.0f * pi;

float readDelayLinear(const std::array<float, 4096>& buffer, int writeIndex, float delay)
{
    float read = static_cast<float>(writeIndex) - delay;
    while (read < 0.0f)
        read += static_cast<float>(buffer.size());
    const auto indexA = static_cast<int>(read) % static_cast<int>(buffer.size());
    const auto indexB = (indexA + 1) % static_cast<int>(buffer.size());
    const float fraction = read - std::floor(read);
    return buffer[static_cast<std::size_t>(indexA)] * (1.0f - fraction)
         + buffer[static_cast<std::size_t>(indexB)] * fraction;
}
}

std::uint32_t SynthEngine::Random::nextU32()
{
    std::uint32_t value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value;
    return value;
}

float SynthEngine::Random::bipolar()
{
    constexpr float scale = 1.0f / 2147483648.0f;
    return static_cast<float>(static_cast<std::int32_t>(nextU32())) * scale;
}

SynthEngine::SynthEngine()
{
    initialiseComponents();
    currentParameters = targetParameters;
}

void SynthEngine::prepare(double newSampleRate, int)
{
    sampleRate = std::max(8000.0, newSampleRate);
    inverseSampleRate = static_cast<float>(1.0 / sampleRate);
    reset();
}

void SynthEngine::reset()
{
    for (auto& voice : voices)
    {
        const auto component = voice.component;
        const auto randomState = voice.random.state;
        voice = Voice {};
        voice.component = component;
        voice.random.state = randomState;
        voice.temperature = 0.15f;
    }
    currentParameters = targetParameters;
    supplyVoltage = 1.0f;
    allocationCounter = 0;
    totalSamples = 0;
    outputDcInput = outputDcState = outputLowPass = 0.0f;
    pinkNoiseA = pinkNoiseB = pinkNoiseC = controlNoiseState = 0.0f;
    supplyRipplePhase = 0.0f;
    detectorHpfInput = detectorHpfState = detectorEnvelope = 0.0f;
    compressorGain = 1.0f;
    masterGainReductionDb = 0.0f;
    transformerMemoryLeft = transformerMemoryRight = 0.0f;
    ensembleBuffer.fill(0.0f);
    ensembleWriteIndex = 0;
    ensemblePhaseA = 0.0f;
    ensemblePhaseB = 0.37f;
}

void SynthEngine::setParameters(const Parameters& parameters)
{
    targetParameters = parameters;
    targetParameters.age = clamp(targetParameters.age, 0.0f, 1.0f);
    targetParameters.body = clamp(targetParameters.body, 0.0f, 1.0f);
    targetParameters.cutoffHz = clamp(targetParameters.cutoffHz, 40.0f, 18000.0f);
    targetParameters.resonance = clamp(targetParameters.resonance, 0.0f, 0.92f);
    targetParameters.filterMode = clamp(targetParameters.filterMode, 0.0f, 1.0f);
    targetParameters.attackSeconds = clamp(targetParameters.attackSeconds, 0.001f, 8.0f);
    targetParameters.decaySeconds = clamp(targetParameters.decaySeconds, 0.005f, 8.0f);
    targetParameters.sustain = clamp(targetParameters.sustain, 0.0f, 1.0f);
    targetParameters.releaseSeconds = clamp(targetParameters.releaseSeconds, 0.005f, 12.0f);
    targetParameters.ensemble = clamp(targetParameters.ensemble, 0.0f, 1.0f);
    targetParameters.outputGain = clamp(targetParameters.outputGain, 0.0f, 1.5f);
    targetParameters.driftAmountCents = clamp(targetParameters.driftAmountCents, 0.0f, 24.0f);
    targetParameters.compressorAmount = clamp(targetParameters.compressorAmount, 0.0f, 1.0f);
    for (const auto& control : floatControls)
        targetParameters.*(control.member) = clamp(targetParameters.*(control.member), control.minimum, control.maximum);
    targetParameters.registerOctaves = std::round(targetParameters.registerOctaves);
}

void SynthEngine::initialiseComponents()
{
    Random componentRandom { 0x53434845u };
    for (int index = 0; index < voiceCount; ++index)
    {
        auto& voice = voices[static_cast<std::size_t>(index)];
        voice.random.state = 0x10001u + static_cast<std::uint32_t>(index * 7919);
        voice.component.oscillatorCalibration = componentRandom.bipolar() * 2.1f;
        voice.component.filterCalibration = 1.0f + componentRandom.bipolar() * 0.045f;
        voice.component.envelopeCapacitor = 1.0f + componentRandom.bipolar() * 0.065f;
        voice.component.transistorMismatch = componentRandom.bipolar() * 0.12f;
        voice.component.vcaGain = 1.0f + componentRandom.bipolar() * 0.055f;
        voice.component.waveformAsymmetry = componentRandom.bipolar() * 0.035f;
    }
}

SynthEngine::Voice& SynthEngine::chooseVoice(int midiNote)
{
    for (auto& voice : voices)
        if (voice.active && voice.midiNote == midiNote)
            return voice;

    for (auto& voice : voices)
        if (!voice.active)
            return voice;

    return *std::min_element(voices.begin(), voices.end(), [] (const Voice& a, const Voice& b)
    {
        if (a.gate != b.gate)
            return !a.gate;
        return a.allocationId < b.allocationId;
    });
}

void SynthEngine::noteOn(int midiNote, float velocity)
{
    midiNote = static_cast<int>(clamp(static_cast<float>(midiNote), 0.0f, 127.0f));
    auto& voice = chooseVoice(midiNote);

    const float elapsedSeconds = static_cast<float>(voice.samplesSinceUse) * inverseSampleRate;
    const float pitchDistance = static_cast<float>(midiNote - voice.previousNote);
    const float history = clamp(pitchDistance / 36.0f, -1.0f, 1.0f);
    const float restMemory = std::exp(-elapsedSeconds * 0.28f);
    const float registerBias = (static_cast<float>(midiNote) - 60.0f) / 67.0f;
    const float componentBias = voice.component.oscillatorCalibration * 0.36f * targetParameters.voiceVariation;
    const float stochastic = voice.random.bipolar() * 1.25f;
    const float driftScale = targetParameters.driftAmountCents / 3.0f;
    const float ageScale = (0.22f + 1.78f * targetParameters.age * targetParameters.age) * driftScale;

    voice.noteErrorCents = (0.6f * history * restMemory + 0.28f * registerBias
                          + componentBias + stochastic) * ageScale;
    voice.settledErrorCents = voice.noteErrorCents;
    voice.previousNote = midiNote;
    voice.midiNote = midiNote;
    voice.velocity = clamp(velocity, 0.0f, 1.0f);
    voice.active = true;
    voice.gate = true;
    voice.envelopeStage = Voice::EnvelopeStage::attack;
    voice.allocationId = ++allocationCounter;
    voice.samplesSinceUse = 0;
}

void SynthEngine::noteOff(int midiNote, bool allowTail)
{
    for (auto& voice : voices)
    {
        if (voice.active && voice.midiNote == midiNote)
        {
            voice.gate = false;
            if (allowTail)
                voice.envelopeStage = Voice::EnvelopeStage::release;
            else
            {
                voice.envelope = 0.0f;
                voice.envelopeStage = Voice::EnvelopeStage::idle;
                voice.active = false;
            }
        }
    }
}

void SynthEngine::allNotesOff()
{
    for (auto& voice : voices)
    {
        voice.gate = false;
        voice.envelopeStage = Voice::EnvelopeStage::release;
    }
}

float SynthEngine::polyBlep(float phase, float increment)
{
    if (phase < increment)
    {
        const float t = phase / increment;
        return t + t - t * t - 1.0f;
    }
    if (phase > 1.0f - increment)
    {
        const float t = (phase - 1.0f) / increment;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float SynthEngine::renderOscillator(Voice& voice, float increment)
{
    const float phase = voice.phase;
    const float asymmetry = voice.component.waveformAsymmetry * currentParameters.age * currentParameters.voiceVariation;
    const float subIncrement = increment * 0.5f;
    float sub = voice.subPhase < 0.5f ? 1.0f : -1.0f;
    sub += polyBlep(voice.subPhase, subIncrement);
    float subEdge = voice.subPhase - 0.5f;
    if (subEdge < 0.0f) subEdge += 1.0f;
    sub -= polyBlep(subEdge, subIncrement);
    float output = 0.0f;

    switch (currentParameters.waveform)
    {
        case Waveform::saw:
            output = 2.0f * phase - 1.0f - polyBlep(phase, increment);
            output = output * 0.84f + asymmetry * (output * output - 0.34f);
            break;

        case Waveform::square:
        case Waveform::narrowPulse:
        {
            const float nominalWidth = currentParameters.waveform == Waveform::square ? 0.5f : 0.23f;
            const float width = clamp(nominalWidth + currentParameters.pulseWidth - 0.5f + asymmetry, 0.08f, 0.92f);
            output = phase < width ? 1.0f : -1.0f;
            output += polyBlep(phase, increment);
            float fallingPhase = phase - width;
            if (fallingPhase < 0.0f)
                fallingPhase += 1.0f;
            output -= polyBlep(fallingPhase, increment);
            output *= currentParameters.waveform == Waveform::square ? 0.72f : 0.64f;
            break;
        }

        case Waveform::triangle:
        {
            float square = phase < 0.5f ? 1.0f : -1.0f;
            square += polyBlep(phase, increment);
            float falling = phase - 0.5f;
            if (falling < 0.0f)
                falling += 1.0f;
            square -= polyBlep(falling, increment);
            voice.triangleState = increment * square + (1.0f - increment) * voice.triangleState;
            output = voice.triangleState * 3.8f;
            break;
        }

        case Waveform::organ:
        {
            const float width = clamp(.42f + currentParameters.pulseWidth - .5f, .08f, .92f);
            float pulse = phase < width ? 1.0f : -1.0f;
            pulse += polyBlep(phase, increment);
            float edge = phase - width;
            if (edge < 0.0f)
                edge += 1.0f;
            pulse -= polyBlep(edge, increment);

            output = 0.46f * pulse + 0.18f * sub;
            break;
        }
    }

    voice.phase += increment;
    voice.phase -= std::floor(voice.phase);
    voice.subPhase += subIncrement;
    voice.subPhase -= std::floor(voice.subPhase);
    output = output * (1.0f - currentParameters.subBalance * .55f) + sub * currentParameters.subBalance * .55f;
    return output;
}

void SynthEngine::updateEnvelope(Voice& voice)
{
    const float ageError = 1.0f + (voice.component.envelopeCapacitor - 1.0f) * currentParameters.age * currentParameters.voiceVariation;
    const float leakage = currentParameters.age * currentParameters.age * 0.018f;

    switch (voice.envelopeStage)
    {
        case Voice::EnvelopeStage::idle:
            voice.envelope = 0.0f;
            break;
        case Voice::EnvelopeStage::attack:
        {
            const float time = std::max(0.001f, currentParameters.attackSeconds * ageError);
            voice.envelope += (1.02f - voice.envelope) * clamp(5.0f * inverseSampleRate / time, 0.0f, 1.0f);
            if (voice.envelope >= 0.999f)
            {
                voice.envelope = 1.0f;
                voice.envelopeStage = Voice::EnvelopeStage::decay;
            }
            break;
        }
        case Voice::EnvelopeStage::decay:
        {
            const float target = currentParameters.sustain * (1.0f - leakage);
            const float time = std::max(0.005f, currentParameters.decaySeconds * ageError);
            voice.envelope += (target - voice.envelope) * clamp(5.0f * inverseSampleRate / time, 0.0f, 1.0f);
            if (std::abs(voice.envelope - target) < 0.0005f)
                voice.envelopeStage = Voice::EnvelopeStage::sustain;
            break;
        }
        case Voice::EnvelopeStage::sustain:
            voice.envelope += (currentParameters.sustain * (1.0f - leakage) - voice.envelope)
                            * inverseSampleRate * (0.6f + 0.5f * currentParameters.age);
            break;
        case Voice::EnvelopeStage::release:
        {
            const float historyFactor = 0.9f + 0.2f * voice.envelope;
            const float time = std::max(0.005f, currentParameters.releaseSeconds * ageError * historyFactor);
            voice.envelope += (0.0f - voice.envelope) * clamp(5.0f * inverseSampleRate / time, 0.0f, 1.0f);
            if (voice.envelope < 0.00008f)
            {
                voice.envelope = 0.0f;
                voice.envelopeStage = Voice::EnvelopeStage::idle;
                voice.active = false;
                voice.midiNote = -1;
            }
            break;
        }
    }
}

void SynthEngine::updateSlowState(Voice& voice)
{
    const float activityTarget = voice.gate ? 1.0f : 0.08f;
    const float thermalTime = (voice.gate ? 38.0f : 95.0f) / (.25f + .75f * currentParameters.thermalAmount);
    voice.temperature += (activityTarget - voice.temperature) * inverseSampleRate / thermalTime;

    const float driftScale = currentParameters.driftAmountCents / 3.0f;
    const float randomForce = voice.random.bipolar() * (0.004f + 0.019f * currentParameters.age) * driftScale;
    voice.driftVelocity += randomForce * inverseSampleRate;
    voice.driftVelocity *= std::exp(-inverseSampleRate * 0.45f);
    voice.microDriftCents += voice.driftVelocity;
    voice.microDriftCents += (-voice.microDriftCents) * inverseSampleRate * 0.035f;
    const float limit = currentParameters.driftAmountCents * (0.22f + 0.78f * currentParameters.age);
    voice.microDriftCents = clamp(voice.microDriftCents, -limit, limit);

    const float settlingMs = (30.0f + currentParameters.age * 170.0f) * currentParameters.settlingTime;
    voice.settledErrorCents *= std::exp(-inverseSampleRate * 1000.0f / settlingMs);
}

float SynthEngine::applyFilter(Voice& voice, float input)
{
    const float ageCutoff = 1.0f - 0.28f * currentParameters.age * currentParameters.age;
    const float mismatch = 1.0f + (voice.component.filterCalibration - 1.0f) * currentParameters.age * currentParameters.voiceVariation;
    const float trackingError = 1.0f + ((voice.midiNote - 60.0f) / 67.0f)
                                      * voice.component.transistorMismatch * currentParameters.age * 0.18f * currentParameters.voiceVariation;
    const float keyFollow = std::pow(2.0f, (voice.midiNote - 60.0f) * currentParameters.keyTracking / 12.0f);
    const float cutoff = clamp(currentParameters.cutoffHz * ageCutoff * mismatch * trackingError * keyFollow,
                               35.0f, static_cast<float>(sampleRate) * 0.42f);
    const float g = std::tan(pi * cutoff * inverseSampleRate);
    const float resonanceWander = 1.0f + voice.component.transistorMismatch * currentParameters.age * 0.35f * currentParameters.voiceVariation;
    const float damping = clamp(1.55f - currentParameters.resonance * 1.42f * resonanceWander, 0.16f, 1.7f);

    // Topology-preserving integrators stay stable when key tracking drives the
    // cutoff close to Nyquist. The stored values are equivalent integrator states.
    const float high = (input - (g + damping) * voice.svfBand - voice.svfLow)
                       / (1.0f + damping * g + g * g);
    const float band = g * high + voice.svfBand;
    voice.svfBand = g * high + band;
    const float low = g * band + voice.svfLow;
    voice.svfLow = g * band + low;
    const float mixed = low * (1.0f - currentParameters.filterMode)
                      + band * currentParameters.filterMode;
    return fastTanh(mixed * (1.0f + 0.22f * currentParameters.resonance));
}

float SynthEngine::renderVoice(Voice& voice)
{
    updateEnvelope(voice);
    if (!voice.active)
        return 0.0f;

    updateSlowState(voice);
    const float driftScale = clamp(currentParameters.driftAmountCents / 4.0f, 0.0f, 3.0f);
    const float fixedOffset = voice.component.oscillatorCalibration * currentParameters.age * driftScale * currentParameters.voiceVariation;
    const float thermalOffset = (voice.temperature - 0.55f) * voice.component.oscillatorCalibration
                              * currentParameters.age * 0.42f * driftScale * currentParameters.thermalAmount * currentParameters.voiceVariation;
    const float supplyOffset = (supplyVoltage - 1.0f) * 18.0f * currentParameters.age * driftScale;
    const float trackingError = (voice.midiNote - 60.0f) * voice.component.transistorMismatch
                              * currentParameters.age * 0.022f * driftScale * currentParameters.voiceVariation;
    const float cents = fixedOffset + voice.microDriftCents + voice.settledErrorCents
                      + thermalOffset + supplyOffset + trackingError;
    const float frequency = midiToHz(static_cast<float>(voice.midiNote) + currentParameters.registerOctaves * 12.0f
                                    + (cents + currentParameters.tuneCents) / 100.0f);
    const float increment = clamp(frequency * inverseSampleRate, 0.0f, 0.45f);
    float signal = renderOscillator(voice, increment);
    const float toneHz = 500.0f + 16500.0f * currentParameters.sourceTone * currentParameters.sourceTone;
    voice.sourceToneState += (signal - voice.sourceToneState) * (1.0f - std::exp(-twoPi * toneHz * inverseSampleRate));
    signal += (voice.sourceToneState - signal) * (1.0f - currentParameters.sourceTone);

    const float asymDrive = 1.0f + currentParameters.age * 0.8f;
    signal = fastTanh(signal * asymDrive
                    + (voice.component.transistorMismatch * currentParameters.age * currentParameters.voiceVariation
                       + currentParameters.circuitBias) * signal * signal);
    signal = applyFilter(voice, signal);

    const float ageBandwidth = 12000.0f - currentParameters.age * currentParameters.age * 6600.0f;
    const float lowPassCoefficient = 1.0f - std::exp(-twoPi * ageBandwidth * inverseSampleRate);
    voice.ageLowPass += (signal - voice.ageLowPass) * lowPassCoefficient;

    const float bodyHighPassHz = 38.0f + (1.0f - currentParameters.body) * 410.0f;
    const float dcCoefficient = std::exp(-twoPi * bodyHighPassHz * inverseSampleRate);
    const float highPassed = voice.ageLowPass - voice.dcBlockInput + dcCoefficient * voice.dcBlockOutput;
    voice.dcBlockInput = voice.ageLowPass;
    voice.dcBlockOutput = highPassed;

    const float vcaMismatch = 1.0f + (voice.component.vcaGain - 1.0f) * currentParameters.age * currentParameters.voiceVariation;
    const float amplitudeFlutter = 1.0f + voice.microDriftCents * currentParameters.age * 0.006f;
    const float output = highPassed * voice.envelope * voice.velocity * vcaMismatch * amplitudeFlutter;
    voice.lastOutput = output;
    ++voice.samplesSinceUse;
    return output;
}

float SynthEngine::applyOutputCharacter(float input)
{
    const float supplyHeadroom = 1.35f - 0.32f * currentParameters.age + (supplyVoltage - 1.0f) * 1.8f;
    const float colour = currentParameters.character;
    float output = colour < .0001f ? input : fastTanh(input * colour / std::max(0.45f, supplyHeadroom)) * supplyHeadroom / colour;

    const float noiseLevel = 0.00005f + (0.18f + 0.82f * currentParameters.age * currentParameters.age) * 0.00105f;
    float noise = 0.0f;
    switch (currentParameters.noiseType)
    {
        case NoiseType::off:
            break;
        case NoiseType::thermal:
            noise = globalRandom.bipolar() * noiseLevel;
            break;
        case NoiseType::pink:
        {
            const float white = globalRandom.bipolar();
            pinkNoiseA = 0.99765f * pinkNoiseA + white * 0.0990460f;
            pinkNoiseB = 0.96300f * pinkNoiseB + white * 0.2965164f;
            pinkNoiseC = 0.57000f * pinkNoiseC + white * 1.0526913f;
            noise = (pinkNoiseA + pinkNoiseB + pinkNoiseC + white * 0.1848f) * noiseLevel * 0.22f;
            break;
        }
        case NoiseType::supplyRipple:
            supplyRipplePhase += 50.0f * inverseSampleRate;
            supplyRipplePhase -= std::floor(supplyRipplePhase);
            noise = (std::sin(twoPi * supplyRipplePhase) * 0.68f
                   + std::sin(twoPi * supplyRipplePhase * 2.0f + 0.31f) * 0.32f) * noiseLevel * 2.1f;
            break;
        case NoiseType::controlVoltage:
        {
            const float coefficient = 1.0f - std::exp(-twoPi * 21.0f * inverseSampleRate);
            controlNoiseState += (globalRandom.bipolar() - controlNoiseState) * coefficient;
            noise = controlNoiseState * noiseLevel * 5.5f;
            break;
        }
    }
    output += noise * currentParameters.noiseAmount;

    const float dcCoefficient = std::exp(-twoPi * 18.0f * inverseSampleRate);
    const float dcBlocked = output - outputDcInput + dcCoefficient * outputDcState;
    outputDcInput = output;
    outputDcState = dcBlocked;
    return dcBlocked;
}

void SynthEngine::applyMaster(float& left, float& right)
{
    // A compact behavioural console bus path: side-chain coupling capacitor,
    // RMS detector, timing capacitor, VCA gain cell, then transformer/line amp.
    const float linked = 0.5f * (left + right);
    const float sidechainCoefficient = std::exp(-twoPi * 90.0f * inverseSampleRate);
    const float sidechain = linked - detectorHpfInput + sidechainCoefficient * detectorHpfState;
    detectorHpfInput = linked;
    detectorHpfState = sidechain;

    const float squared = sidechain * sidechain;
    const float attackSeconds = 0.010f;
    const float releaseSeconds = 0.22f + 0.24f * clamp(detectorEnvelope * 8.0f, 0.0f, 1.0f);
    const float envelopeTime = squared > detectorEnvelope ? attackSeconds : releaseSeconds;
    const float envelopeCoefficient = 1.0f - std::exp(-inverseSampleRate / envelopeTime);
    detectorEnvelope += (squared - detectorEnvelope) * envelopeCoefficient;

    const float amount = currentParameters.compressorAmount;
    const float levelDb = 10.0f * std::log10(std::max(detectorEnvelope, 1.0e-12f));
    const float thresholdDb = -5.0f - 21.0f * amount;
    const float ratio = 1.0f + 5.0f * amount;
    const float kneeDb = 5.5f;
    const float overDb = levelDb - thresholdDb;
    float reductionDb = 0.0f;
    if (overDb > -0.5f * kneeDb)
    {
        const float slope = 1.0f - 1.0f / ratio;
        if (overDb < 0.5f * kneeDb)
        {
            const float kneePosition = overDb + 0.5f * kneeDb;
            reductionDb = slope * kneePosition * kneePosition / (2.0f * kneeDb);
        }
        else
            reductionDb = slope * overDb;
    }

    const float makeupDb = amount * 6.2f;
    const float targetGain = std::pow(10.0f, (makeupDb - reductionDb) / 20.0f);
    const float vcaAttack = 1.0f - std::exp(-inverseSampleRate / 0.006f);
    const float vcaRelease = 1.0f - std::exp(-inverseSampleRate / 0.095f);
    compressorGain += (targetGain - compressorGain)
                    * (targetGain < compressorGain ? vcaAttack : vcaRelease);
    masterGainReductionDb += (reductionDb - masterGainReductionDb) * envelopeCoefficient;

    left *= compressorGain;
    right *= compressorGain;

    const float transformerDrive = (1.0f + amount * 1.25f + currentParameters.age * 0.18f)
                                  * std::pow(10.0f, currentParameters.driveDb / 20.0f);
    const float memoryCoefficient = 1.0f - std::exp(-twoPi * 34.0f * inverseSampleRate);
    transformerMemoryLeft += (left - transformerMemoryLeft) * memoryCoefficient;
    transformerMemoryRight += (right - transformerMemoryRight) * memoryCoefficient;
    const float leftBiased = left + transformerMemoryLeft * 0.045f * transformerDrive;
    const float rightBiased = right + transformerMemoryRight * 0.045f * transformerDrive;
    const float normaliser = 1.0f / std::tanh(transformerDrive);
    left = std::tanh(leftBiased * transformerDrive) * normaliser * currentParameters.outputGain * currentParameters.expression;
    right = std::tanh(rightBiased * transformerDrive) * normaliser * currentParameters.outputGain * currentParameters.expression;
}

float SynthEngine::applyEnsemble(float dry, bool rightChannel)
{
    if (!rightChannel)
    {
        ensembleBuffer[static_cast<std::size_t>(ensembleWriteIndex)] = dry;
        const float rateA = 0.47f;
        const float rateB = 0.71f;
        ensemblePhaseA += rateA * inverseSampleRate;
        ensemblePhaseB += rateB * inverseSampleRate;
        ensemblePhaseA -= std::floor(ensemblePhaseA);
        ensemblePhaseB -= std::floor(ensemblePhaseB);
    }

    const float phaseA = rightChannel ? ensemblePhaseA + 0.31f * currentParameters.stereoWidth : ensemblePhaseA;
    const float phaseB = rightChannel ? ensemblePhaseB + 0.57f * currentParameters.stereoWidth : ensemblePhaseB;
    const float centreSamples = static_cast<float>(sampleRate) * 0.011f;
    const float depthSamples = static_cast<float>(sampleRate) * 0.00125f;
    const float delayA = centreSamples + depthSamples * std::sin(twoPi * phaseA);
    const float delayB = centreSamples * 1.42f + depthSamples * 0.72f * std::sin(twoPi * phaseB);
    const float wet = 0.54f * readDelayLinear(ensembleBuffer, ensembleWriteIndex, delayA)
                    + 0.46f * readDelayLinear(ensembleBuffer, ensembleWriteIndex, delayB);
    return dry + (wet - dry * 0.2f) * currentParameters.ensemble * 0.54f;
}

void SynthEngine::advanceSmoothedParameters()
{
    const float coefficient = 1.0f - std::exp(-inverseSampleRate / 0.018f);
    auto smooth = [coefficient] (float& current, float target)
    {
        current += (target - current) * coefficient;
    };
    for (const auto& control : floatControls)
        smooth(currentParameters.*(control.member), targetParameters.*(control.member));
    smooth(currentParameters.filterMode, targetParameters.filterMode);
    currentParameters.waveform = targetParameters.waveform;
    currentParameters.noiseType = targetParameters.noiseType;
}

void SynthEngine::process(float* left, float* right, int numSamples)
{
    if (left == nullptr || numSamples <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceSmoothedParameters();
        int active = 0;
        float mix = 0.0f;
        for (auto& voice : voices)
        {
            mix += renderVoice(voice);
            active += voice.active ? 1 : 0;
            if (!voice.active)
                ++voice.samplesSinceUse;
        }

        const float supplyTarget = 1.0f - currentParameters.age * 0.0016f * static_cast<float>(active) * currentParameters.supplySag;
        const float supplyRate = supplyTarget < supplyVoltage ? 0.36f : 0.055f;
        supplyVoltage += (supplyTarget - supplyVoltage) * inverseSampleRate * supplyRate;

        // Fixed summing gain: counting release tails here made held notes jump
        // louder as other voices reached idle (almost +3 dB for two -> one).
        // Preserve the single-note level; the console bus handles chord peaks.
        constexpr float voiceBusGain = 0.52f;
        const float dry = applyOutputCharacter(mix * voiceBusGain);
        float masterLeft = applyEnsemble(dry, false);
        float masterRight = right != nullptr ? applyEnsemble(dry, true) : masterLeft;
        applyMaster(masterLeft, masterRight);
        left[sample] = masterLeft;
        if (right != nullptr)
            right[sample] = masterRight;
        ensembleWriteIndex = (ensembleWriteIndex + 1) % ensembleBufferSize;
        ++totalSamples;
    }
}

DebugState SynthEngine::getDebugState() const
{
    DebugState result;
    result.supplyVoltage = supplyVoltage;
    result.masterGainReductionDb = masterGainReductionDb;
    for (int index = 0; index < voiceCount; ++index)
    {
        const auto& voice = voices[static_cast<std::size_t>(index)];
        auto& output = result.voices[static_cast<std::size_t>(index)];
        output.active = voice.active;
        output.midiNote = voice.midiNote;
        const float driftScale = clamp(currentParameters.driftAmountCents / 4.0f, 0.0f, 3.0f);
        output.centsOffset = voice.component.oscillatorCalibration * currentParameters.age * driftScale
                           + voice.microDriftCents + voice.settledErrorCents;
        output.temperature = voice.temperature;
        output.filterMismatch = 1.0f + (voice.component.filterCalibration - 1.0f) * currentParameters.age;
        output.capacitorAge = 1.0f + (voice.component.envelopeCapacitor - 1.0f) * currentParameters.age;
        output.amplitudeMismatch = 1.0f + (voice.component.vcaGain - 1.0f) * currentParameters.age;
        output.allocationId = voice.allocationId;
        result.activeVoices += voice.active ? 1 : 0;
    }
    return result;
}

float SynthEngine::clamp(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

float SynthEngine::midiToHz(float midiNote)
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

float SynthEngine::fastTanh(float value)
{
    return std::tanh(value);
}
}
