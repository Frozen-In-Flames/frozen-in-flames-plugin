# Frozen In Flames v1.0.0

First release. A spectral freeze and multi-model saturation plugin in one box, built from a single codebase for VST3 and AU, with a Standalone app for quick checks outside a session.

## What is new, and why

**Ice stage: spectral freeze with a phase-mode control.** The freezer captures the last analysed frame of audio and sustains it for as long as Freeze is held, with no loop points and no repeated waveform. Why: long evolving pads and drones usually come from looping a buffer, and the loop seam always shows. A spectral hold has no seam. The Glass control blends between randomising the frozen phases (diffuse, chorus-like wash) and keeping them (coherent, glassy and pitched), because a frozen chord wants the wash and a frozen lead wants the pitch. Crystal adds a touch of quantised shimmer so a static hold still moves. Pitch shifts the frozen layer in semitones, Melt crossfades back to the live input, and Thaw lets a held chord decay naturally instead of stopping dead.

**Fire stage: four saturation models with 4x oversampling.** Ember is a soft symmetric drive, Blaze a harder asymmetric clip, Cinder a rectifying grit stage, and Inferno a bounded foldback for outright destruction. Why four models: one drive knob with one curve is one sound; these four cover the range from gentle warmth to broken. Oversampling at 4x keeps the harmonics the shapers create from folding back as aliasing, which is what makes cheap saturation sound cheap. Flicker adds slow random movement to the drive so sustained notes do not sit still, at a rate you control.

**Routing, tone, and honest plumbing.** The stages run Ice into Fire, Fire into Ice, or in parallel. Fire after ice saturates the frozen layer; fire before ice means the freezer captures an already driven signal; parallel keeps the clean freeze and the grit separate. Tone is a matched pair of low and high shelves around the whole chain. The plugin reports its true latency to the host (freeze analysis hop plus oversampling latency, 1029 samples at 48 kHz) and delay-compensates its own dry path to match, because a freeze effect that shifts your track in time is a bug, not a character. Every continuous parameter is smoothed, so knob moves and automation do not zipper. A final soft-clip safety stage keeps the output bounded no matter how the stages stack.

**Measured fixes found during development.** Two defects were caught by the test harness before release, not after. First, the Cinder model's rectification produced a DC offset of +0.446 on a full-scale sine; the fire stage now runs a 12 Hz DC blocker per channel, which measured the offset at 0.00000 on the same signal. Second, the frozen layer level was verified across every phase mode (coherent, diffuse, and the Glass range), pitch shift, and noise input: all measured within about 1 dB of the capture level at one second, held there by the capture gain compensation, so switching modes does not jump in level.

**Six factory presets.** Frozen in Flames (the signature sound, both stages), Permafrost Pad (long ambient freeze), Glass Cathedral (ice alone, coherent and spacious), Ember Drive (fire alone, warm saturation), Inferno Melt (maximum fire melting a live signal), Cinder Thaw (gritty freeze with a slow natural release).

## Measured DSP results (test harness, 48 kHz)

- 17 of 17 automated DSP tests pass.
- FFT round trip reconstructs the input within float tolerance.
- Frozen 220 Hz sine: level at 1 second after capture +1.86 dB relative to capture level, inside the plus or minus 4 dB design band; dominant frequency measured at 220.5 Hz.
- Frozen noise stays finite and bounded.
- Every flame curve is finite across the tested input range; Inferno output is bounded by 1; Ember at zero bias is odd-symmetric.
- Cinder DC offset: +0.4462 raw, 0.00000 after the DC blocker described above.

## Plugin identity

- Name: Frozen In Flames
- Manufacturer: Frozen In Flames
- Manufacturer code: FrIF
- Plugin code: Fif1
- Bundle identifier: com.frozeninflames.frozeninflames
- These codes are stable and will not change between versions; sessions and presets depend on them.

## Build status, honestly

- Built and verified on Linux in this environment: VST3 bundle and Standalone app (Release build, about 12.8 MB and 13.0 MB). The VST3 was loaded through the real Steinberg factory interface by a test host: it exposes the Frozen In Flames audio module and controller, reports 1029 samples of latency, processes audio at the expected level, and round-trips its state (647 bytes).
- AU source complete; builds on macOS from this project, and a ready GitHub Actions workflow (build.yml) is attached to this release for placement at .github/workflows/build.yml, after which release tags build the macOS VST3 and AU automatically. Not yet tested in a Mac host; unsigned until the owner signs with his own developer account.
- Windows: builds from source with the same CMake project (instructions in the README); no prebuilt Windows binary in this release.
- Not yet verified anywhere: loading inside a commercial host application, the live microphone path of the Standalone, and macOS builds. The first macOS build will confirm the macOS artifacts; results will be reported with the release.

## Formats and licensing

VST3 and AU are format names owned by their respective stewards; this plugin is an independent implementation built with the JUCE framework (used under its open source terms for this project) and is not affiliated with or endorsed by the stewards of those formats. Frozen In Flames is released under the Apache License 2.0; see LICENSE.
