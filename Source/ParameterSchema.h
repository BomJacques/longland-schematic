#pragma once
#include "LonglandDSP.h"
#include <array>

namespace longland
{
struct FloatControl
{
    const char* id;
    const char* name;
    float Parameters::* member;
    float minimum, maximum, interval, skew;
    const char* unit;
    const char* description;
};

// Single source of truth for DSP wiring, defaults, host automation and tooltips.
inline constexpr std::array<FloatControl, 28> floatControls {{
    { "age", "Age", &Parameters::age, 0, 1, 0, 1, "", "Component ageing and circuit imperfections" },
    { "body", "Body", &Parameters::body, 0, 1, 0, 1, "", "Low-frequency coupling and weight" },
    { "cutoff", "Cutoff", &Parameters::cutoffHz, 60, 16000, 0, .27f, "Hz", "Filter cutoff frequency" },
    { "resonance", "Resonance", &Parameters::resonance, 0, .92f, 0, 1, "", "Filter resonance" },
    { "attack", "Attack", &Parameters::attackSeconds, .001f, 8, 0, .25f, "s", "Envelope attack" },
    { "decay", "Decay", &Parameters::decaySeconds, .005f, 8, 0, .25f, "s", "Envelope decay" },
    { "sustain", "Sustain", &Parameters::sustain, 0, 1, 0, 1, "", "Envelope sustain level" },
    { "release", "Release", &Parameters::releaseSeconds, .005f, 12, 0, .25f, "s", "Envelope release" },
    { "ensemble", "Ensemble", &Parameters::ensemble, 0, 1, 0, 1, "", "Dual-rate ensemble depth" },
    { "output", "Volume", &Parameters::outputGain, 0, 1.5f, 0, 1, "", "Final master output level" },
    { "drift", "Pitch Drift", &Parameters::driftAmountCents, 0, 24, .01f, .72f, "cents", "Pitch memory and stochastic drift amount" },
    { "compressor", "Master Compressor", &Parameters::compressorAmount, 0, 1, 0, 1, "", "Stereo-linked console VCA compression" },
    { "register", "Register", &Parameters::registerOctaves, -2, 2, 1, 1, "oct", "Oscillator register in whole octaves" },
    { "pulse", "Pulse Width", &Parameters::pulseWidth, .1f, .9f, 0, 1, "", "Duty-cycle trim for Square, Narrow Pulse and Organ" },
    { "balance", "Sub Balance", &Parameters::subBalance, 0, 1, 0, 1, "", "Blend in the octave-down oscillator" },
    { "tone", "Source Tone", &Parameters::sourceTone, 0, 1, 0, 1, "", "Pre-filter oscillator brightness" },
    { "keyTrack", "Key Tracking", &Parameters::keyTracking, 0, 1, 0, 1, "", "Filter keyboard tracking; full is one octave per octave" },
    { "settle", "Pitch Settling", &Parameters::settlingTime, .1f, 5, 0, .5f, "x", "Time for note-on pitch error to settle" },
    { "warmth", "Thermal Memory", &Parameters::thermalAmount, 0, 3, 0, 1, "x", "Voice-card heating and thermal pitch offset" },
    { "voice", "Voice Variation", &Parameters::voiceVariation, 0, 2, 0, 1, "x", "Persistent component differences between voice cards" },
    { "supply", "Supply Sag", &Parameters::supplySag, 0, 4, 0, 1, "x", "Shared supply sag under polyphonic load" },
    { "bias", "Circuit Bias", &Parameters::circuitBias, -.5f, .5f, 0, 1, "", "Bipolar transistor asymmetry before the filter" },
    { "noise", "Noise Amount", &Parameters::noiseAmount, 0, 8, 0, .5f, "x", "Level of the selected circuit noise source" },
    { "width", "Stereo Width", &Parameters::stereoWidth, 0, 1, 0, 1, "", "Ensemble stereo width; zero is mono" },
    { "drive", "Master Drive", &Parameters::driveDb, 0, 18, 0, 1, "dB", "Additional transformer/line amplifier drive" },
    { "tune", "Fine Tune", &Parameters::tuneCents, -100, 100, .01f, 1, "cents", "Global fine tuning" },
    { "character", "Line Character", &Parameters::character, 0, 2, 0, 1, "x", "Pre-master nonlinear circuit colour; zero is clean" },
    { "expression", "Expression", &Parameters::expression, 0, 1, 0, 1, "", "Performance level, multiplied by MIDI CC11" }
}};
}
