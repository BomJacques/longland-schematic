#pragma once

#include <array>
#include <cstdint>

namespace longland
{
constexpr int voiceCount = 8;

enum class Waveform : int
{
    saw = 0,
    square,
    narrowPulse,
    triangle,
    organ
};

enum class NoiseType : int
{
    off = 0,
    thermal,
    pink,
    supplyRipple,
    controlVoltage
};

struct Parameters
{
    Waveform waveform = Waveform::saw;
    float age = 0.38f;
    float body = 0.42f;
    float cutoffHz = 3400.0f;
    float resonance = 0.12f;
    float filterMode = 0.0f; // 0 = low-pass, 1 = band-pass
    float attackSeconds = 0.015f;
    float decaySeconds = 0.42f;
    float sustain = 0.72f;
    float releaseSeconds = 0.55f;
    float ensemble = 0.12f;
    float outputGain = 0.72f;
    float driftAmountCents = 8.0f;
    NoiseType noiseType = NoiseType::thermal;
    float compressorAmount = 0.48f;
    // Appended fields preserve aggregate factory presets and legacy defaults.
    float registerOctaves = 0.0f;
    float pulseWidth = 0.5f;
    float subBalance = 0.0f;
    float sourceTone = 1.0f;
    float keyTracking = 0.0f;
    float settlingTime = 1.0f;
    float thermalAmount = 1.0f;
    float voiceVariation = 1.0f;
    float supplySag = 1.0f;
    float circuitBias = 0.0f;
    float noiseAmount = 1.0f;
    float stereoWidth = 1.0f;
    float driveDb = 0.0f;
    float tuneCents = 0.0f;
    float character = 1.0f;
    float expression = 1.0f;
};

struct VoiceDebugState
{
    bool active = false;
    int midiNote = -1;
    float centsOffset = 0.0f;
    float temperature = 0.0f;
    float filterMismatch = 1.0f;
    float capacitorAge = 1.0f;
    float amplitudeMismatch = 1.0f;
    std::uint64_t allocationId = 0;
};

struct DebugState
{
    std::array<VoiceDebugState, voiceCount> voices {};
    float supplyVoltage = 1.0f;
    int activeVoices = 0;
    float masterGainReductionDb = 0.0f;
};

class SynthEngine
{
public:
    SynthEngine();

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void setParameters(const Parameters& newParameters);
    const Parameters& getParameters() const noexcept { return targetParameters; }

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote, bool allowTail = true);
    void allNotesOff();

    void process(float* left, float* right, int numSamples);
    DebugState getDebugState() const;

private:
    struct Random
    {
        explicit Random(std::uint32_t seed = 0x9e3779b9u) : state(seed) {}
        std::uint32_t nextU32();
        float bipolar();
        std::uint32_t state;
    };

    struct ComponentModel
    {
        float oscillatorCalibration = 0.0f;
        float filterCalibration = 1.0f;
        float envelopeCapacitor = 1.0f;
        float transistorMismatch = 0.0f;
        float vcaGain = 1.0f;
        float waveformAsymmetry = 0.0f;
    };

    struct Voice
    {
        enum class EnvelopeStage { idle, attack, decay, sustain, release };

        ComponentModel component;
        Random random {};
        bool active = false;
        bool gate = false;
        int midiNote = -1;
        int previousNote = 60;
        float velocity = 0.0f;
        float phase = 0.0f;
        float subPhase = 0.0f;
        float triangleState = 0.0f;
        float sourceToneState = 0.0f;
        float envelope = 0.0f;
        EnvelopeStage envelopeStage = EnvelopeStage::idle;
        float noteErrorCents = 0.0f;
        float settledErrorCents = 0.0f;
        float microDriftCents = 0.0f;
        float driftVelocity = 0.0f;
        float temperature = 0.15f;
        float svfLow = 0.0f;
        float svfBand = 0.0f;
        float ageLowPass = 0.0f;
        float dcBlockInput = 0.0f;
        float dcBlockOutput = 0.0f;
        float lastOutput = 0.0f;
        std::uint64_t allocationId = 0;
        std::uint64_t samplesSinceUse = 0;
    };

    static float clamp(float value, float minimum, float maximum);
    static float midiToHz(float midiNote);
    static float polyBlep(float phase, float phaseIncrement);
    static float fastTanh(float value);

    void initialiseComponents();
    Voice& chooseVoice(int midiNote);
    float renderVoice(Voice& voice);
    float renderOscillator(Voice& voice, float phaseIncrement);
    void updateEnvelope(Voice& voice);
    void updateSlowState(Voice& voice);
    float applyFilter(Voice& voice, float input);
    float applyOutputCharacter(float input);
    void applyMaster(float& left, float& right);
    float applyEnsemble(float dry, bool rightChannel);
    void advanceSmoothedParameters();

    std::array<Voice, voiceCount> voices;
    Parameters targetParameters;
    Parameters currentParameters;
    Random globalRandom { 0x4c4f4e47u };
    double sampleRate = 44100.0;
    float inverseSampleRate = 1.0f / 44100.0f;
    std::uint64_t allocationCounter = 0;
    std::uint64_t totalSamples = 0;
    float supplyVoltage = 1.0f;
    float outputDcInput = 0.0f;
    float outputDcState = 0.0f;
    float outputLowPass = 0.0f;
    float pinkNoiseA = 0.0f;
    float pinkNoiseB = 0.0f;
    float pinkNoiseC = 0.0f;
    float controlNoiseState = 0.0f;
    float supplyRipplePhase = 0.0f;
    float detectorHpfInput = 0.0f;
    float detectorHpfState = 0.0f;
    float detectorEnvelope = 0.0f;
    float compressorGain = 1.0f;
    float masterGainReductionDb = 0.0f;
    float transformerMemoryLeft = 0.0f;
    float transformerMemoryRight = 0.0f;

    static constexpr int ensembleBufferSize = 4096;
    std::array<float, ensembleBufferSize> ensembleBuffer {};
    int ensembleWriteIndex = 0;
    float ensemblePhaseA = 0.0f;
    float ensemblePhaseB = 0.37f;
};
}
