# Frozen in Flames: User Setup Guide

Frozen in Flames is one effect with two halves. **Ice** is a spectral freeze: play something, press Freeze, and the sound hangs in the air as a living pad you can melt, transpose and shimmer. **Fire** is a flame saturator with four models that add heat, bite and fuzz. You can run either half alone or both together.

This guide gets you from download to first sound. No developer knowledge needed.

## What to download

Grab the file that matches your computer from the release page:

| Your computer | Download | You get |
| --- | --- | --- |
| macOS | `FrozenInFlames-macOS` | `Frozen In Flames.vst3` and `Frozen In Flames.component` (the AU) |
| Windows | `FrozenInFlames-Windows` | `Frozen In Flames.vst3` |
| Linux | `FrozenInFlames-Linux` | `Frozen In Flames.vst3` |

AU (Audio Unit) is a Mac-only format. If you are on Windows or Linux, the VST3 is your version, and every control is identical.

## Where to install it

Copy the bundle (the whole folder ending in `.vst3` or `.component`) into the matching folder below, then rescan in your DAW (see the next section).

**VST3**

| System | System folder (all users) | User folder (just you) |
| --- | --- | --- |
| Windows | `C:\Program Files\Common Files\VST3` | `C:\Users\<you>\AppData\Local\Programs\Common\VST3` |
| macOS | `/Library/Audio/Plug-Ins/VST3` | `~/Library/Audio/Plug-Ins/VST3` |
| Linux | `/usr/lib/vst3` (or `/usr/local/lib/vst3`) | `~/.vst3` |

**AU (macOS only)**

| System folder | User folder |
| --- | --- |
| `/Library/Audio/Plug-Ins/Components` | `~/Library/Audio/Plug-Ins/Components` |

Tip: the user folders are the easiest choice. They need no admin password and every DAW looks there.

### macOS Gatekeeper note (unsigned builds)

Frozen in Flames is currently **unsigned**: it has not been signed with an Apple Developer certificate yet. macOS may refuse to load it the first time, or warn that it cannot check it for malicious software. To allow it: right-click (or Control-click) the bundle in Finder and choose **Open**, then confirm; or open **System Settings, Privacy & Security**, scroll to the message about the plugin, and choose **Open Anyway**. This is a one-time step per bundle. Signed builds will remove this step in a later release.

## Make your DAW find it, and load it

Every host works the same way at heart: install the bundle, **rescan** so the host notices it, then insert it on a track. After a successful scan the plugin appears in your effects list as **Frozen In Flames**, usually under the manufacturer name **Frozen In Flames** or in the Distortion / FX categories.

### VST3 hosts

**Ableton Live**
1. Preferences, Plug-Ins: make sure "Use VST3 Plug-In System Folders" is on (or add the user folder as a custom folder).
2. Click **Rescan**.
3. Drag Frozen In Flames from the browser's Plug-Ins section onto a track.

**FL Studio**
1. Options, Manage plugins, then **Find installed plugins**.
2. In the plugin list, select Frozen In Flames and tick it as a favorite if you like.
3. Add it from the mixer: click an empty effect slot on a mixer track and choose it from the list.

**Cubase**
1. Studio, VST Plug-in Manager, then click the **Rescan** (circular arrow) button.
2. Find Frozen In Flames in the VST Effects list.
3. On a track, click an Insert slot and choose it.

**Reaper**
1. Options, Preferences, Plug-ins, VST: confirm the VST3 folder is in the path list, then **Re-scan**.
2. On a track, click **FX**, find Frozen In Flames in the list, and double-click it.

**Studio One**
1. The browser scans on startup; if it is missing, go to Studio One, Options (Preferences), Locations, VST Plug-ins and confirm the folder, then restart.
2. Drag Frozen In Flames from the Effects browser onto a track.

### AU hosts (macOS)

**Logic Pro**
1. The AU is validated automatically the first time Logic starts after install. If it does not appear, open Logic Pro, Settings, Plug-in Manager and click **Reset & Rescan Selection**.
2. On a channel strip, click an Audio FX slot and choose **Audio Units, Frozen In Flames, Frozen In Flames**.

**GarageBand**
1. Restart GarageBand after installing so it rescans Audio Units.
2. On a track, open Smart Controls, then Plug-ins, and choose **AU Effects, Frozen In Flames**.

**Ableton Live (AU)**
1. Preferences, Plug-Ins: enable "Use Audio Units".
2. Rescan, then find Frozen In Flames under Audio Units in the browser and drag it onto a track.

## Quick start (30 seconds)

Insert Frozen in Flames on a track with something sustained on it: a guitar chord, a synth pad, a piano with the pedal down. Play the chord, and while it rings, press **FREEZE**. The chord hangs in the air and keeps sounding after the instrument stops. Now turn **HEAT** up and the hanging chord starts to glow with saturation. That is the whole idea: freeze a moment with ice, then decide how much fire it gets.

## The controls in plain language

**Ice (the freeze)**
- **FREEZE**: the big button. Captures the sound that is playing right now and holds it as a pad. Press again to release it.
- **MELT**: how fast the held sound drifts toward whatever you are playing now. At zero the freeze is permanent; turn it up and the pad slowly follows you.
- **GLASS**: the texture of the pad. Low is smooth and frosty, like a wash. High is crystalline, with the individual notes standing out like glass.
- **CRYSTAL**: adds shimmering voices an octave and an octave-and-a-fifth above the frozen sound. The classic angelic-pad control.
- **PITCH**: transposes the frozen layer, from 12 semitones down to 12 up. Try +12 for an instant octave shimmer.
- **THAW**: how long the pad takes to fade away after you release Freeze. Short is a tidy exit; long is a slow goodbye.

**Fire (the saturation)**
- **HEAT**: how hard the saturator is driven. At zero the fire stage is completely transparent (useful when you only want the ice). Small amounts warm, large amounts roar.
- **FLAME**: which saturator model. **Ember** is warm and tube-like. **Blaze** is bright with a transistor bite. **Inferno** folds the wave for dense, metallic harmonics. **Cinder** is a rectifying octave fuzz with a strong octave-up character.
- **FLICKER**: how much the flame wobbles organically, like a real flame moving. Adds life and movement to held sounds.
- **FLICK RATE**: how fast that wobble moves.
- **TONE**: a tilt control. Left darkens (more lows, fewer highs), right brightens.

**Session**
- **ROUTE**: the order of the two halves. **Ice into Fire**: freeze first, then saturate the pad together with your live signal. **Fire into Ice**: saturate your playing first, and the freezer captures the fired sound. **Parallel**: the frozen layer and its saturated copy are blended half and half over your live signal.
- **MIX**: balance between your dry signal and the full processed path.
- **OUTPUT**: final level trim, from silence-adjacent to a healthy boost. Frozen in Flames compensates its own level as you raise Heat, so this is for taste, not damage control.

## Factory presets

Choose one from the **PRESET** menu to hear a whole idea at once. Every control still moves afterwards; presets are starting points, not cages.

- **Frozen in Flames**: the signature sound. A glass-leaning freeze with crystal shimmer, into a warm Ember fire. Start here.
- **Permafrost Pad**: ice alone, no fire at all. A smooth, slow frozen pad with a long thaw. Shows what the freeze does by itself.
- **Glass Cathedral**: ice alone, pitched up an octave with heavy crystal and high glass. Instant ethereal wash for quiet passages.
- **Ember Drive**: fire alone, no freeze. Warm drive with a gentle flicker. Good on guitars, keys, and anything that needs glow without grit.
- **Inferno Melt**: both halves at full drama. Your playing is fired first, then frozen while it melts fast toward whatever you play next.
- **Cinder Thaw**: both halves, warmer and fuzzier. A frozen pad with Cinder octave fuzz and a slow two-second thaw.

## Troubleshooting

**The plugin does not show up in my DAW.**
Almost always one of three things: the bundle is in the wrong folder (compare with the tables above; the user folders are the safest), the host has not rescanned since you installed (find the rescan button in your host's plugin manager), or, on a Mac, Gatekeeper blocked the AU and it failed validation silently (allow it in Privacy & Security as described above, then rescan). In Logic, the Plug-in Manager shows validation results if you need detail.

**I press Freeze and hear silence, or almost nothing.**
The freeze captures the sound that is playing at the moment you press it. If you press Freeze during silence, it faithfully freezes silence. Play something audible, let it ring, and press Freeze while it sounds. Also check MIX: at low Mix you mostly hear your dry signal.

**The frozen pad is much quieter or louder than what I played.**
Frozen in Flames normalises the frozen layer to the level of what it captured, so levels should be close. Very short or very percussive captures (a single drum hit) have less sustained energy to hold; freeze chords and pads for the fullest result, and use OUTPUT to trim.

**High CPU use.**
The freeze engine's FFT size follows your sample rate: 2048 points at low rates (22.05 kHz), 4096 points at 44.1 and 48 kHz, and 8192 points at 88.2 kHz and above, per channel. At very high sample rates with many instances, that adds up. Freezing tracks, raising the buffer size, or running the session at 48 kHz keeps it comfortable. One instance on a track is modest; a wall of instances at 192 kHz is not.

**There is a short delay on the processed sound.**
The plugin honestly reports its latency to the host (one spectral hop plus the oversampler's latency), and hosts with delay compensation line everything up automatically. If your host has delay compensation off, turn it on for sample-accurate alignment.
