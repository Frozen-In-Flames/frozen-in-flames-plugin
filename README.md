# Frozen in Flames

**A spectral freeze and flame saturation plugin. VST3 and AU, one codebase.**

Frozen in Flames is an audio effect in two halves that were designed for each other:

- **Ice** is a spectral freeze. It analyses your audio continuously with a phase-vocoder FFT engine. Press Freeze and the sound that is ringing at that moment is captured and re-synthesised indefinitely as a pad, with its instantaneous frequencies tracked so partials stay in tune. Glass blends between diffuse frost and coherent crystal partials, Crystal adds octave and octave-and-a-fifth shimmer voices, Melt lets the held spectrum drift toward whatever you play next, Pitch transposes the frozen layer, and Thaw sets how slowly it fades when you let go. A feed-forward normaliser keeps the frozen layer at the level of the audio it captured, in every mode (measured within about a decibel across phase modes; see the release notes).
- **Fire** is a saturator with four flame models: **Ember** (warm, asymmetric, tube-like), **Blaze** (bright hard-knee clipper), **Inferno** (sine wavefolder) and **Cinder** (rectifying octave fuzz). It runs oversampled at 4x so the harmonics it creates do not fold back as aliasing, with an organic three-layer Flame Flicker modulating the drive character, a tilt Tone control, approximate loudness compensation as Heat rises, a DC blocker after the shaper (the Cinder model rectifies by design and would otherwise put a large DC offset on the output), and a soft-clip safety stage at the very end.

The two halves can be routed **Ice into Fire**, **Fire into Ice**, or **Parallel**, and at Heat = 0 the fire stage is bit-transparent, so ice-alone sounds are genuinely unprocessed by the fire.

**New to the plugin? Read the [User Setup Guide](docs/USER-GUIDE.md)**: what to download, exactly where to install it on each system, how to load it in major hosts, every control in plain language, and troubleshooting.

## Controls

Ice: Freeze, Melt, Glass, Crystal, Pitch (semitones), Thaw (seconds).
Fire: Heat (drive), Flame (Ember / Blaze / Inferno / Cinder), Flicker, Flicker Rate, Tone (tilt).
Session: Route (Ice into Fire / Fire into Ice / Parallel), Mix, Output.

Six factory presets are included, demonstrating ice alone, fire alone, and both stages together.

## Building

Requirements: CMake 3.22 or newer and a C++17 compiler. JUCE 8.0.15 is fetched automatically by CMake (FetchContent) at configure time; there is nothing else to download.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/dsp_tests        # JUCE-free DSP test harness
```

Products land in `build/FrozenInFlames_artefacts/Release/` (VST3, Standalone, and AU on macOS).

**Platform notes**
- **AU builds on macOS only.** The AU target is declared in CMake and produced automatically when building on a Mac. A ready GitHub Actions workflow is attached to the v1.0.0 release (place it at `.github/workflows/build.yml`) so release tags build the macOS VST3 and AU automatically. It cannot be compiled on Windows or Linux.
- **Linux** needs the usual JUCE system packages: `libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev`.
- **macOS release binaries require code signing and notarization** with the owner's Apple Developer account before they load without Gatekeeper warnings. That step is intentionally not part of this repository's automation yet; unsigned builds work with the one-time allowance described in the User Setup Guide.
- **Windows** builds produce the VST3 and Standalone with the same two commands above.

## Architecture

The DSP core is two JUCE-free, header-only files, `Source/FiDsp.h` (FFT, biquads, delay, flame shapes, flicker generator, PRNG) and `Source/SpectralFreezer.h` (the freeze engine). They are unit-tested without any host by `Tests/dsp_test.cpp`. The JUCE layer (`PluginProcessor`, `PluginEditor`) is only a shell: parameters, routing, oversampling, latency reporting and the editor. Reported latency is the oversampler latency plus one spectral hop, and the dry path is delay-compensated by the same amount inside the plugin.

## AU status

AU source complete; builds on macOS (and by macOS CI once the workflow file attached to the v1.0.0 release is placed at .github/workflows/build.yml); not yet tested in a Mac host; unsigned until the owner signs with his own developer account.

## Licensing

Project code: Apache License 2.0 (see `LICENSE`), Copyright 2026 Aries Hilton.

Technology and format notes, stated factually: this project builds with the JUCE framework (used under the terms of its licence, including the GPLv3 option for open-source projects) and produces VST3 and Audio Unit plug-ins. VST3 is a technology of Steinberg Media Technologies GmbH, used under the VST3 licensing terms; Audio Unit is a technology of Apple Inc. Building and distributing plug-ins in these formats is subject to those respective terms.
