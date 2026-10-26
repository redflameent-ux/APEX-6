# APEX-6 Guitar Tone Matrix

A guitar rig plugin in the spirit of Guitar Rig, AmpliTube, Helix and Metallurgy: a drag-and-drop rack of
amps, cabs, pedals and effects. VST3 + stand-alone on Windows, AU + VST3 + stand-alone on Mac.

## What's in the rack (v0.1)

| Unit | Kind | Controls |
|---|---|---|
| **GATEKEEPER** | Noise gate | Threshold, Release (with hold, so chugs stop dead without chopping notes) |
| **TIGHT** | One-knob-style tightener / boost | Tight (cuts low end going into the amp), Edge (mid push), Boost |
| **SCRAPYARD** | Bit crusher / fuzz | Bits, Rate, Fuzz, Mix |
| **APEX HEAD** | 4-channel amp, 4x oversampled | Channel (Clean / Crunch / Lead / Modern), Gain, Bass, Mid, Treble, Presence, Depth, Master |
| **IR MATRIX** | 4-IR blend cab | Four IR slots (A-D), each with its own Mix knob and polarity flip (Ø), plus Low Cut, High Cut, Level. A blend bar shows the mix. Empty slots use a built-in 4x12 |
| **WOW TAPE** | Tape warble | Wow, Flutter, Age (darkening, saturation, hiss), Mix |
| **GLITCH LOOM** | Tempo stutter | Grid, Shred, Chance, Reverse, Mix. Follows your DAW tempo |
| **ECHO CHAMBER** | Dub delay | Sync (or free time), Time, Feedback, Tone, Mix, Ping-pong |
| **VOID HALL** | Reverb | Size, Damping, Width, Mix |

- **Add** units from the left. They drop into a sensible spot in the chain.
- **Drag** a unit by its name to reorder. **x** removes it, the round button turns it on/off (no clicks).
- **Double-click** any knob to reset it.
- **Save / Load** stores the whole rig, including which IR files you loaded.
- 10 factory rigs: Init Rig, Low Tuned Wall, Djent Clock, Lead Scream, Skank Dub Clean, Glass Ambient,
  Rust Engine, Memphis Tape Haze, Void Drone, Broken Fuzz. Presets leave your loaded IRs alone.

## Getting the plugin

Every push to `main` builds Windows and Mac versions online and puts them on the **Releases** page:
download **APEX-6-Setup.exe**, double-click it, then rescan plugins in your DAW.

The stand-alone app mutes the input at first (to stop feedback through speakers). Click the
unmute message / Options and pick your audio interface input.

## Building it yourself (optional)

```
cmake -S . -B build
cmake --build build --config Release
```

The offline test (plays a fake guitar riff through every preset and checks the output):

```
cmake -S . -B build -DAPEX_BUILD_TESTS=ON
cmake --build build --target ApexRender
```
