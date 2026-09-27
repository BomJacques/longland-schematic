# Longland Schematic

**A memory synthesizer.** An eight-voice VST3 whose character comes from persistent virtual voice cards rather than from a layer of retro effects.

This development build intentionally has no standalone target. The production target is a VST3 instrument; the DSP library and its tests have no JUCE dependency.

## What is implemented

- Five compact, band-limited oscillator models: saw, square, narrow pulse, triangle and an organ-like pulse combination.
- Eight deterministic voice cards with persistent oscillator calibration, filter calibration, envelope capacitor tolerance, transistor mismatch, waveform asymmetry and VCA gain.
- Pitch memory made from note-on discrepancy, interval/rest history, note register, stochastic micro drift, per-voice thermal state, settling and imperfect tracking. It is not a random pitch LFO. The automatable **Drift** control runs from calibrated (0 cents) to conspicuously janky (24 cents).
- A shared virtual supply whose tiny sag depends on active polyphony.
- Age and Body macros that act on component behaviour, bandwidth, asymmetry, circuit noise, low-frequency coupling and mismatch.
- A simple two-pole state-variable filter with a low-pass/band-pass blend and voice-dependent tracking.
- Imperfect ADSR envelopes with capacitor tolerance, history-sensitive release and age-dependent leakage.
- A restrained two-rate, bandwidth-limited ensemble.
- Selectable circuit noise: Off, Thermal, Pink, 50/100 Hz Supply Ripple, or slowly wandering Control Voltage noise.
- A physical-behaviour master console path: 90 Hz side-chain coupling, stereo-linked RMS detector, timing-capacitor attack/release, soft-knee VCA gain cell, automatic makeup and a transformer-coupled line amplifier. **Comp** moves the linked threshold/ratio/drive behaviour together; **Volume** is the true final output level. It draws on classic British-console practice without cloning a proprietary SSL or Neve circuit.
- Sample-accurate MIDI event rendering, parameter smoothing, denormal protection in the plug-in wrapper, deterministic allocation and state restoration.
- Twenty-four curated factory states and a white schematic-style development panel with live scientific voice readouts.

## Build the DSP tests

```powershell
cmake -S . -B build-dsp -DLONGLAND_BUILD_PLUGIN=OFF
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release --output-on-failure
```

## Build the VST3

JUCE 9.0.0 is fetched at configure time from the official repository.

```powershell
cmake -S . -B build -DLONGLAND_BUILD_PLUGIN=ON
cmake --build build --config Release --target LonglandSchematic_VST3
```

The VST3 build is deliberately not copied into the system plug-in directory. This avoids requiring administrator access and makes signing/packaging an explicit release step.

## Source map

- `Source/LonglandDSP.*` — framework-independent instrument and circuit-history model.
- `Source/PluginProcessor.*` — JUCE VST3, automation, MIDI, programs and state.
- `Source/PluginEditor.*` — deliberately plain development/debug interface.
- `Source/FactoryPresets.h` — 24 compact instrument states.
- `Tests/DspTests.cpp` — tuning/drift, persistence, allocation, envelope, smoothing, determinism, mono/stereo and sample-rate stress checks.

## Release notes

Before commercial distribution, provide an appropriate JUCE licence, validate the built VST3 with Steinberg's validator and `pluginval`, test automation/state recall in target DAWs, add code signing/notarisation, and run listening/aliasing measurements in addition to the automated stress suite.
