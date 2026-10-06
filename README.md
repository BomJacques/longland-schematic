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
- Twenty-four curated factory states and an orthographic, Blender-rendered instrument interface based on the saved user-adjusted model.
- All 30 model knobs control real, host-automatable parameters. Each has its own 49-frame render with moving highlights, shadows and independent wear. Drag vertically, scroll, or double-click to reset; hover shows its name/value in the utility bar.
- A 49-frame VU needle follows post-master stereo RMS with a 300 ms detector (0 VU = -18 dBFS). The utility bar also reports compressor gain reduction.
- All 37 keys (MIDI 48–84) have five rendered rear-hinge travel poses. They respond to incoming MIDI and mouse playing/glissando. Sustain holds sound, not the displayed key; short note events are latched briefly so they remain visible.
- MIDI CC64 sustain, CC11 expression, multi-channel note release bookkeeping and a Panic control. No Blender installation is needed to run the VST3; the 14.9 MB of compressed image assets are embedded.

## Model controls added in v0.4.0

| Panel | Controls and behaviour |
| --- | --- |
| Source | Wave selects the existing oscillator; Register shifts ±2 octaves; Pulse trims square/pulse/organ duty cycle; Balance adds an octave-down oscillator; Tone filters the source before the main filter; Key Track adds 0–100% filter tracking. |
| Memory | Drift sets pitch drift depth; Age retains the component-age macro; Settle scales note-on pitch settling time; Warmth controls thermal memory; Voice scales persistent voice-card mismatch; Supply scales polyphonic supply sag. |
| Circuit | Cutoff, Resonance and Body retain their existing functions; Bias adds bipolar transistor asymmetry; Noise sets the selected noise source level; Noise Type selects the circuit-noise model. |
| Contour | ADSR and Ensemble retain their functions; Width controls ensemble stereo spread (zero is mono). |
| Master | Comp retains the console compressor macro; Drive adds up to 18 dB of transformer drive; Volume remains the final output control. |
| Performance | Tune adjusts ±100 cents; Character controls pre-master nonlinear colour; Expression scales performance level and is multiplied by CC11. |

The existing 15 parameter IDs and ordering are retained. New parameters are appended, saved with the plug-in state, and reset to neutral defaults when loading an older state. Factory presets use those neutral defaults. The filter now uses stable topology-preserving integrators to handle full key tracking at high cutoff; this is a numerical/filter-response refinement, so old audio is not promised to be bit-identical.

The Mains hardware and pilot lamp remain visual elements in this build. Presets live in the small utility bar; the LP/BP button beside Cutoff switches the filter mode on the Circuit panel.

## v0.4.2: corrected scales and panel filter switch

- The 297 scale marks around the 27 main-panel knobs now follow the pointer's clockwise 270-degree sweep, with the open part of each scale at the bottom. Two static before/after reference renders were used to patch only the markings in the cabinet and existing knob strips. No turning-animation frames were re-rendered; key and VU assets are byte-identical to v0.4.1.
- A host-automatable LP/BP button beside Cutoff replaces the filter dropdown. It retains the existing `filterMode` ID and state format. LP means low-pass; BP means band-pass. The button updates when host automation or a preset changes the mode.
- The preset dropdown and popup list use a shared warm dark-brown/ivory/umber palette scoped to this editor, rather than the JUCE default colours. No global look-and-feel changes affect other plug-ins.
- `Design/Longland-Schematic-Corrected-Scales-v007.blend` preserves the corrected editable geometry. `Tools/render_scale_patch_v042.py` and `Tools/patch_static_scales.py` record the static-only update workflow (requires the v0.4.1 source assets and Blender scene). Fully re-rendering that corrected scene with the standard exporter also works.

## v0.4.1: stable held-note level and larger labels

- Removed active-voice-count gain compensation. Previously, even a nearly silent release tail attenuated the entire mix; when it ended, a remaining held note jumped about 2.9 dB in the two-voice regression case. The bus now has fixed gain, preserving the previous single-note level. Chords sum naturally into the existing compressor/transformer, so their level and saturation can be stronger than in v0.4.0.
- Enlarged all 42 editable Blender text objects in the latest user-adjusted orthographic scene. Main knob labels are 2.3x larger, section headings 2x, and performance labels 3x. Side labels now use dark ink on the ivory blocks. Meter lettering is repositioned inside its dial.
- Re-rendered every embedded control/key/VU asset from `Longland-Schematic-v041-Integration-Source.blend`, an exact snapshot of the latest user-saved `Longland-Schematic-Large-Labels-v006.blend`. This includes the final chassis and side-control positioning changes, with Expression shortened to EXP. The pre-edit user scene is preserved separately; the label pass did not alter model geometry, wear, lighting or camera framing.
- Added a held-note/release-tail level regression at 44.1, 48 and 96 kHz. Parameter IDs and preset/state formats remain unchanged.
- Animation frames are decoded into independent images once per shared skin, preventing neighbouring filmstrip frames from bleeding into control edges at fractional UI scales.

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
- `Source/PluginEditor.*` — rendered filmstrip UI, parameter attachments, MIDI keys and VU animation.
- `Source/ParameterSchema.h` — shared float-control definitions, DSP members, limits, defaults and descriptions.
- `Assets/` — embedded panel, 30 knob strips, 37 key strips, VU strip and pixel-layout manifest.
- `Source/FactoryPresets.h` — 24 compact instrument states.
- `Tests/DspTests.cpp` — tuning/drift, persistence, allocation, envelope, smoothing, determinism, mono/stereo and sample-rate stress checks.
- `Tests/PluginTests.cpp` — automation, 30 UI bindings, old/new state recall, MIDI/CC64, on-screen notes, meter decay, snapshots and optional actual-VST3 loading/audio/editor checks. This console test helper is not a standalone instrument and is not shipped in the plug-in package.

## Re-render after model adjustments

Keep the editable model separate from the export copy. The supplied animation rig has a 49-frame knob/VU sweep and key press/release keyframes. The export script performs independent key-layer renders with the actual model lighting and camera.

```powershell
blender --factory-startup --background "USER-COPY.blend" --python Tools/export_vst_assets.py -- --output-root "PROJECT" --render-folder render-source-v2
python Tools/pack_vst_assets.py "PROJECT" render-source-v2
cmake --build build --config Release --target LonglandSchematic_VST3 LonglandDSPTests LonglandPluginTests
ctest --test-dir build -C Release --output-on-failure
```

Packing requires Pillow. Use a fresh render folder after changing the source; fingerprint validation prevents accidentally mixing old frames with a new model. The camera, model names and control parent structure must remain intact. Output uses Cycles/CUDA at 1600×960; adjust the exporter device selection for a different workstation. Runtime UI sizes range from 960×600 to 1600×1000, preserving aspect ratio. Full-resolution source renders and `.blend` files are not required at runtime.

## Release notes

Before commercial distribution, provide an appropriate JUCE licence, validate the built VST3 with Steinberg's validator and `pluginval`, test automation/state recall in target DAWs, add code signing/notarisation, and run listening/aliasing measurements in addition to the automated stress suite.
