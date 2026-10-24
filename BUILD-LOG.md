# Frozen in Flames plugin: build log

## Step 1: DSP core transcription (verbatim)
- `Source/FiDsp.h` and `Source/SpectralFreezer.h` transcribed exactly as supplied by the owner (SpectralFreezer.h was supplied twice, identical; one copy used). No transcription changes were needed: both headers compile clean with `g++ -std=c++17 -Wall -Wextra` with zero warnings.

## Step 2: DSP test harness (JUCE-free, Tests/dsp_test.cpp)
First run: **15 passed, 0 failed.** Measured values behind the checks (48 kHz, 220 Hz sine at 0.5 amplitude):
- FFT round trip reconstructs within float tolerance (tolerance 1e-3, actual error far below).
- Freeze sustain (glass 0.85): input RMS 0.3536, frozen RMS at 1.0 s = 0.4380 (+1.86 dB vs capture, inside the stated +/-4 dB AGC band), RMS at 2.25 s = 0.3872 (ratio 1.095), Goertzel peak 220.5 Hz. FFT size 4096, hop 1024 at 48 kHz.
- Diffuse freeze (glass 0.15) sustains inside the same level band; frozen white noise stays finite and bounded.

## Owner steering applied: correctness improvements with measurements
1. **Phase-mode level match, checked, NOT a defect.** Frozen-level ratio at 2.25 s after capture across modes: coherent (glass 0.85) 1.095, diffuse (glass 0.15) 1.029, default glass 0.4 = 1.020, pitch +12 = 1.021, noise source = 0.938. All within about +/-0.8 dB of unity: the feed-forward AGC in the pasted design genuinely holds level between phase modes. No change made; numbers recorded so the claim is evidence, not opinion.
2. **Cinder DC offset: real defect, fixed.** The Cinder shaper rectifies by design; measured raw DC mean on a 0.8-amplitude 100 Hz sine = **+0.4462** (44% of full scale), which would reach the output uncorrected. Fix: every fire-stage channel runs through `fif::DcBlocker` (12 Hz) after the shaper, in the processor's fire stage. After the blocker the measured mean is **+0.00000**. Locked in by a new harness check (raw path must show the DC, blocked path must not): harness now **17 passed, 0 failed**.
3. **Latency reporting.** Reported latency = oversampler latency (from `juce::dsp::Oversampling::getLatencyInSamples()`) + one freeze hop (the spectral engine's update quantum: captures and spectral updates land on hop boundaries). The internal dry path is delayed by exactly the reported amount so dry and wet stay coherent inside the plugin and host PDC lines up with what the plugin declares.
4. **Parameter smoothing.** Mix and Output gain ramp per sample; Heat (drive) ramps per sample on the oversampled domain; Tone is smoothed and its shelf coefficients refresh in 64-sample chunks; the freezer's own pitch is already smoothed internally (pitchSm). No unsmoothed continuous parameter reaches the audio path.
5. **Fire transparency at Heat = 0.** The flame shapes are not transparent at drive 1 (tanh still colours), which would make an "ice alone" preset dishonest. The fire stage therefore crossfades from bypass to fully engaged across Heat 0.00 to 0.08, so Heat at 0 is bit-transparent and ice-alone presets are real.
6. **Efficiency notes.** Tone shelves and flicker run at base rate (flicker advances once per input sample and is held across the oversampled block); only the shaper itself runs oversampled. No allocations in processBlock: all scratch buffers are pre-sized in prepareToPlay and processing is chunked to that capacity.

## JUCE version pinned
- **JUCE 8.0.15** via CMake FetchContent (verified as the latest tag of the 8.0 series against the official repository; 9.0.x exists but the 8.0 line is the widely deployed stable series, needs only C++17 which matches the JUCE-free DSP core, and avoids churn while this v1.0.0 is being established).

## Plugin identity (stable, do not change after release)
- Name: Frozen In Flames; Manufacturer: Frozen In Flames
- Bundle ID: com.frozeninflames.frozeninflames
- Manufacturer code: `FrIF`; Plugin code: `Fif1`
- Formats: VST3 + Standalone everywhere, AU added on Apple platforms (CMake-guarded, the standard JUCE arrangement)

## Step 4: build on this Linux VM (done)

Environment notes. The first CMake configure failed inside JUCE's juceaide
build: `fatal error: X11/extensions/Xrandr.h: No such file or directory`. A
plain `apt-get update` against the default source list hung (third party
mirrors in the list stalled); a restricted source list containing only the
Ubuntu archive (noble, noble-updates, noble-security) updated in about 10
seconds and the development packages installed cleanly: libxrandr-dev,
libxinerama-dev, libxcursor-dev, libxi-dev, libxext-dev, libx11-dev,
libasound2-dev, libfreetype6-dev, libfontconfig1-dev, libgl1-mesa-dev. One
caution: system state outside the home directory proved volatile in this
environment (an installed header disappeared once and had to be reinstalled);
the combined install plus configure plus build was therefore run as one
unbroken sequence, which succeeded.

Code fixes at first compile of the plugin sources (both in
Source/PluginProcessor.cpp, both errors in new code, not in the pasted DSP):
1. The fire stage called `fif::shape (flameType, x * drive + bias)`, merging
   the driven sample and the bias into one argument; shape takes them
   separately. Fixed to `fif::shape (flameType, x * drive, bias)`.
2. Three `auto* ptrs[2] = {...}` pointer arrays failed template deduction;
   declared as `float*` explicitly.

Build outcome (Release, this VM):
- VST3 bundle built: build/FrozenInFlames_artefacts/Release/VST3/
  "Frozen In Flames.vst3"; module "Frozen In Flames.so" is 12,792,792 bytes.
- Standalone built: build/FrozenInFlames_artefacts/Release/Standalone/
  "Frozen In Flames", 12,977,008 bytes.
- The CMake dsp_tests target also passes: 17 passed, 0 failed.

Host-interface verification (custom minimal VST3 host harness using dlopen
and the real Steinberg interfaces):
- Factory exposes 2 classes: Audio Module Class "Frozen In Flames" and
  Component Controller Class "Frozen In Flames". GetPluginFactory is
  exported from the module.
- getLatencySamples after activation: 1029 (1024 freeze hop + 5 oversampler),
  matching the design calculation.
- 40 blocks of 512 samples of stereo 220 Hz sine processed: all output
  samples finite, final block RMS 0.3288, identical to the same measurement
  taken by instantiating the processor class directly. That direct test also
  engaged Freeze through the real parameter and measured a frozen sustain
  RMS of 0.3387 about half a second after the input went silent.
- State round trip through IBStream: 647 bytes; getState and setState both
  return kResultOk.
- Harness lesson worth recording: a VST3 host must call
  IComponent::activateBus for input and output before setActive, or the
  plugin correctly receives inactive buses and outputs silence.

## Not yet verified

- AU build: impossible on Linux by platform; produced by the macOS CI job on
  release. The first CI run happens after the ship push; its result is the
  real AU status.
- Loading inside a commercial host application on any platform.
- Windows build (source build only in this release).
- Standalone live microphone path (no audio device in this environment).

## Ship note (v1.0.0, Oct 23, 2026)
The first push was rejected by GitHub: the gh OAuth token in this environment lacks the workflow scope, so no commit containing .github/workflows/build.yml can be pushed through it. Resolution, with the copy corrected to match: the workflow file ships as a release asset (build.yml) for placement at .github/workflows/build.yml, the single commit was amended to remove it from the pushed tree, and README and release notes state the placement step plainly. Everything else in this log stands as measured.
