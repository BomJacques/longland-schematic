#include "LonglandDSP.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void expect(bool condition, const std::string& description)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

void render(longland::SynthEngine& engine, int samples, bool stereo = true)
{
    std::vector<float> left(static_cast<std::size_t>(samples));
    std::vector<float> right(static_cast<std::size_t>(samples));
    engine.process(left.data(), stereo ? right.data() : nullptr, samples);
    for (const auto sample : left)
        expect(std::isfinite(sample), "left channel remains finite");
    if (stereo)
        for (const auto sample : right)
            expect(std::isfinite(sample), "right channel remains finite");
}

void testPitchAndVoiceMemory()
{
    longland::SynthEngine engine;
    engine.prepare(48000.0, 512);
    longland::Parameters clean;
    clean.age = 0.0f;
    clean.ensemble = 0.0f;
    engine.setParameters(clean);
    engine.noteOn(69, 1.0f);
    render(engine, 4096);
    auto state = engine.getDebugState();
    expect(std::abs(state.voices[0].centsOffset) < 0.8f, "young circuit starts close to concert pitch");

    clean.age = 1.0f;
    engine.setParameters(clean);
    render(engine, 48000 * 8);
    state = engine.getDebugState();
    expect(std::abs(state.voices[0].centsOffset) <= 24.0f, "default aged pitch motion stays bounded");

    const float firstCalibration = state.voices[0].filterMismatch;
    engine.noteOff(69, false);
    engine.noteOn(72, 1.0f);
    render(engine, 256);
    state = engine.getDebugState();
    expect(std::abs(state.voices[0].filterMismatch - firstCalibration) < 0.000001f,
           "voice card calibration persists between notes");
}

void testAllocation()
{
    longland::SynthEngine engine;
    engine.prepare(44100.0, 128);
    for (int note = 60; note < 69; ++note)
        engine.noteOn(note, 0.8f);
    const auto state = engine.getDebugState();
    expect(state.activeVoices == 8, "polyphony is limited to eight voices");
    bool oldestStillPresent = false;
    bool newestPresent = false;
    for (const auto& voice : state.voices)
    {
        oldestStillPresent |= voice.midiNote == 60;
        newestPresent |= voice.midiNote == 68;
    }
    expect(!oldestStillPresent && newestPresent, "ninth note steals the oldest voice deterministically");
}

void testEnvelopeAndSmoothing()
{
    longland::SynthEngine engine;
    engine.prepare(48000.0, 256);
    longland::Parameters parameters;
    parameters.attackSeconds = 0.01f;
    parameters.releaseSeconds = 0.04f;
    parameters.ensemble = 0.0f;
    engine.setParameters(parameters);
    engine.noteOn(64, 1.0f);

    std::vector<float> left(4096), right(4096);
    engine.process(left.data(), right.data(), static_cast<int>(left.size()));
    float peak = 0.0f;
    for (float value : left)
        peak = std::max(peak, std::abs(value));
    expect(peak > 0.02f && peak < 1.5f, "envelope produces a bounded audible signal");

    parameters.cutoffHz = 90.0f;
    engine.setParameters(parameters);
    std::vector<float> changed(2048), changedRight(2048);
    engine.process(changed.data(), changedRight.data(), static_cast<int>(changed.size()));
    float maximumStep = 0.0f;
    for (std::size_t i = 1; i < changed.size(); ++i)
        maximumStep = std::max(maximumStep, std::abs(changed[i] - changed[i - 1]));
    expect(maximumStep < 1.25f, "automated cutoff is smoothed without an impulse");

    engine.noteOff(64, true);
    render(engine, 48000);
    expect(engine.getDebugState().activeVoices == 0, "release reaches idle");
}

void testRatesAndModes()
{
    constexpr std::array<double, 5> rates { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    for (const double rate : rates)
    {
        longland::SynthEngine engine;
        engine.prepare(rate, 1024);
        longland::Parameters stress;
        stress.age = 1.0f;
        stress.cutoffHz = 16000.0f;
        stress.resonance = 0.92f;
        stress.filterMode = 1.0f;
        stress.ensemble = 0.7f;
        engine.setParameters(stress);
        for (int note = 36; note < 100; note += 8)
            engine.noteOn(note, 1.0f);
        render(engine, static_cast<int>(rate * 0.2));
        render(engine, 1024, false);
    }
}

void testDeterminism()
{
    longland::SynthEngine first, second;
    first.prepare(48000.0, 256);
    second.prepare(48000.0, 256);
    longland::Parameters parameters;
    parameters.age = 0.73f;
    parameters.waveform = longland::Waveform::organ;
    first.setParameters(parameters);
    second.setParameters(parameters);
    first.noteOn(57, 0.71f);
    second.noteOn(57, 0.71f);
    std::array<float, 2048> leftA {}, rightA {}, leftB {}, rightB {};
    first.process(leftA.data(), rightA.data(), static_cast<int>(leftA.size()));
    second.process(leftB.data(), rightB.data(), static_cast<int>(leftB.size()));
    expect(leftA == leftB && rightA == rightB, "identical state and events render deterministically");
}

void testDriftControlAndNoiseTypes()
{
    longland::Parameters stableParameters;
    stableParameters.age = 1.0f;
    stableParameters.driftAmountCents = 0.0f;
    stableParameters.noiseType = longland::NoiseType::off;
    stableParameters.ensemble = 0.0f;
    longland::SynthEngine stable;
    stable.setParameters(stableParameters);
    stable.prepare(48000.0, 256);
    stable.noteOn(72, 1.0f);
    render(stable, 32);
    const float stableOffset = std::abs(stable.getDebugState().voices[0].centsOffset);

    longland::Parameters jankyParameters = stableParameters;
    jankyParameters.driftAmountCents = 24.0f;
    longland::SynthEngine janky;
    janky.setParameters(jankyParameters);
    janky.prepare(48000.0, 256);
    janky.noteOn(72, 1.0f);
    render(janky, 32);
    const float jankyOffset = std::abs(janky.getDebugState().voices[0].centsOffset);
    expect(stableOffset < 0.05f, "zero drift provides a calibration setting");
    expect(jankyOffset > stableOffset + 1.0f, "maximum drift makes note memory conspicuously unstable");

    longland::SynthEngine silentCircuit;
    silentCircuit.setParameters(stableParameters);
    silentCircuit.prepare(48000.0, 256);
    std::array<float, 1024> offLeft {}, offRight {};
    silentCircuit.process(offLeft.data(), offRight.data(), static_cast<int>(offLeft.size()));
    const float offPeak = *std::max_element(offLeft.begin(), offLeft.end(), [] (float a, float b)
    {
        return std::abs(a) < std::abs(b);
    });
    expect(std::abs(offPeak) < 0.000001f, "noise Off is silent without playing voices");

    longland::Parameters noisyParameters = stableParameters;
    noisyParameters.noiseType = longland::NoiseType::thermal;
    longland::SynthEngine noisyCircuit;
    noisyCircuit.setParameters(noisyParameters);
    noisyCircuit.prepare(48000.0, 256);
    std::array<float, 1024> noiseLeft {}, noiseRight {};
    noisyCircuit.process(noiseLeft.data(), noiseRight.data(), static_cast<int>(noiseLeft.size()));
    const bool hasNoise = std::any_of(noiseLeft.begin(), noiseLeft.end(), [] (float value)
    {
        return std::abs(value) > 0.00001f;
    });
    expect(hasNoise, "selected circuit noise is audible without a note");
}

void testPhysicalMasterStage()
{
    longland::Parameters parameters;
    parameters.age = 0.55f;
    parameters.noiseType = longland::NoiseType::off;
    parameters.ensemble = 0.0f;
    parameters.compressorAmount = 1.0f;
    parameters.outputGain = 1.0f;

    longland::SynthEngine compressed;
    compressed.setParameters(parameters);
    compressed.prepare(48000.0, 512);
    for (int note : { 48, 52, 55, 59, 62, 67, 71, 74 })
        compressed.noteOn(note, 1.0f);
    std::vector<float> left(48000), right(48000);
    compressed.process(left.data(), right.data(), static_cast<int>(left.size()));
    float peak = 0.0f;
    for (float value : left)
        peak = std::max(peak, std::abs(value));
    const auto debug = compressed.getDebugState();
    expect(debug.masterGainReductionDb > 0.5f, "master VCA detector produces gain reduction on a full chord");
    expect(peak > 0.1f && peak < 1.05f, "transformer line stage produces a strong bounded master level");

    parameters.outputGain = 0.0f;
    longland::SynthEngine muted;
    muted.setParameters(parameters);
    muted.prepare(48000.0, 512);
    muted.noteOn(60, 1.0f);
    std::array<float, 2048> mutedLeft {}, mutedRight {};
    muted.process(mutedLeft.data(), mutedRight.data(), static_cast<int>(mutedLeft.size()));
    const bool isSilent = std::all_of(mutedLeft.begin(), mutedLeft.end(), [] (float value)
    {
        return value == 0.0f;
    });
    expect(isSilent, "final Volume control reaches true silence");
}
}

int main()
{
    testPitchAndVoiceMemory();
    testAllocation();
    testEnvelopeAndSmoothing();
    testRatesAndModes();
    testDeterminism();
    testDriftControlAndNoiseTypes();
    testPhysicalMasterStage();
    if (failures == 0)
    {
        std::cout << "All Longland DSP tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " assertion(s) failed.\n";
    return EXIT_FAILURE;
}
