#include "LonglandDSP.h"
#include "ParameterSchema.h"

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

std::vector<float> capture(longland::Parameters parameters, int samples=16000, double rate=48000.0)
{
    longland::SynthEngine engine;engine.setParameters(parameters);engine.prepare(rate,256);
    for(int note:{48,60,67,84})engine.noteOn(note,.8f);
    std::vector<float> output(static_cast<std::size_t>(samples)),right(output.size());
    engine.process(output.data(),right.data(),samples);
    expect(std::all_of(output.begin(),output.end(),[](float v){return std::isfinite(v);}),"expanded controls produce finite audio");
    return output;
}

void testExpandedControls()
{
    longland::Parameters base;base.age=.75f;base.waveform=longland::Waveform::square;base.ensemble=.55f;
    base.driftAmountCents=18;base.cutoffHz=4200;
    for(std::size_t i=12;i<longland::floatControls.size();++i)
    {
        const auto& c=longland::floatControls[i];auto low=base,high=base;
        low.*(c.member)=c.minimum;high.*(c.member)=c.maximum;
        auto a=capture(low),b=capture(high);double difference=0;
        for(std::size_t j=0;j<a.size();++j)difference+=std::abs(a[j]-b[j]);
        expect(difference/a.size()>1.0e-7,std::string(c.id)+" has a real DSP effect");
    }
    for(double rate:{44100.0,48000.0,96000.0,192000.0})
    {
        auto maximum=base;for(const auto& c:longland::floatControls)maximum.*(c.member)=c.maximum;
        capture(maximum,12000,rate);
    }
    longland::SynthEngine mono;base.stereoWidth=0;mono.setParameters(base);mono.prepare(48000,256);mono.noteOn(60,.8f);
    std::array<float,4096> left{},right{};mono.process(left.data(),right.data(),4096);
    expect(left==right,"Width zero collapses the ensemble to mono");
    base.expression=0;const auto silent=capture(base);
    expect(std::all_of(silent.begin(),silent.end(),[](float v){return v==0;}),"Expression zero reaches silence");
    base.expression=1;base.noiseAmount=0;longland::SynthEngine quiet;quiet.setParameters(base);quiet.prepare(48000,256);
    quiet.process(left.data(),right.data(),4096);
    expect(std::all_of(left.begin(),left.end(),[](float v){return v==0;}),"Noise zero mutes selected circuit noise");
}

void testHeldNoteDoesNotSwellWhenTailsEnd()
{
    // A barely audible second voice must not turn down the held note, then
    // boost it again seconds later when that voice's release reaches idle.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    for (int tailCount : { 1, 7 })
    {
        longland::Parameters p;
        p.age = 0.0f; p.driftAmountCents = 0.0f; p.ensemble = 0.0f;
        p.noiseType = longland::NoiseType::off; p.compressorAmount = 0.0f;
        p.attackSeconds = 0.001f; p.decaySeconds = 0.005f;
        p.sustain = 1.0f; p.releaseSeconds = 3.0f;
        longland::SynthEngine engine;
        engine.setParameters(p); engine.prepare(rate, 256);
        engine.noteOn(69, 0.7f);
        for (int tail = 0; tail < tailCount; ++tail)
            engine.noteOn(60 + tail, 0.001f);
        render(engine, static_cast<int>(rate));
        for (int tail = 0; tail < tailCount; ++tail)
            engine.noteOff(60 + tail, true);
        auto rms = [&] ()
        {
            std::vector<float> left(static_cast<std::size_t>(rate / 2)), right(left.size());
            engine.process(left.data(), right.data(), static_cast<int>(left.size()));
            double energy = 0.0;
            for (float sample : left) energy += sample * sample;
            return std::sqrt(energy / left.size());
        };
        const double withTail = rms();
        expect(engine.getDebugState().activeVoices == tailCount + 1, "quiet release tails are still active for swell regression");
        render(engine, static_cast<int>(rate * 7));
        const double withoutTail = rms();
        expect(engine.getDebugState().activeVoices == 1, "release tail finishes while first note remains held");
        const double changeDb = 20.0 * std::log10(withoutTail / withTail);
        std::cout << "Held-note level change after " << tailCount << " tail(s) end at " << rate << " Hz: " << changeDb << " dB\n";
        expect(std::abs(changeDb) < 0.1, "held note does not change level when a quiet tail finishes");
    }
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
    testExpandedControls();
    testHeldNoteDoesNotSwellWhenTailsEnd();
    if (failures == 0)
    {
        std::cout << "All Longland DSP tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " assertion(s) failed.\n";
    return EXIT_FAILURE;
}
