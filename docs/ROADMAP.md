# Open Riff Box -- Roadmap

> Free, open-source, lightweight guitar effects processor.
> Plug in your guitar, choose your effects, and play.

--

## Tech Stack

- **Framework:** JUCE 8 (C++17)
- **Build:** CMake + MSVC, static CRT (`/MT`) -- zero runtime dependencies
- **Audio:** ASIO primary, WASAPI/DirectSound fallback
- **Distribution:** Portable `.zip` -- unzip anywhere, run. No installer, no registry, no admin rights.
- **Config:** Stored next to `.exe` (portable mode). Falls back to `%APPDATA%/OpenRiffBox/` if exe directory is read-only.
- **Requirements:** Windows 10/11 x64, audio interface with ASIO drivers recommended.

--

## Architecture

```
+----------------------------------------------+
|                   UI Layer                    |
|  TopBar | ChainList | EffectDetailPanel |     |
|  SidebarPanel (power + meter)                 |
+----------------------------------------------+
|              Effect Chain Manager              |
|   (ordered list of effects, bypass, reorder)  |
+----------------------------------------------+
|         Effect Processors (DSP)               |
|  [Diode]->[Dist]->[Amp Sim S/G/P]->          |
|  [Gate]->[Delay]->[Reverb Spr/Plate]->        |
|  [Modulation Cho/Fla/Pha/Vib]->[EQ]          |
+----------------------------------------------+
|              Audio I/O Layer                   |
|    (JUCE AudioDeviceManager, ASIO/WASAPI)     |
+----------------------------------------------+
```

### Effect Chain (user-reorderable)

Default order (8 visual rows):

```
0: Diode Drive       (drive)
1: Distortion        (drive)
2: Amp Sim           (drive)       [Silver / Gold / Platinum]
3: Noise Gate        (dynamics)
4: Delay             (time-based)
5: Reverb            (time-based)  [Spring / Plate]
6: Modulation        (modulation)  [Chorus / Flanger / Phaser / Vibrato]
7: EQ                (utility)
```

Effects with multiple engines (Amp Sim, Reverb, Modulation) use tabbed selectors. The inactive engine is always bypassed. Grouped engines move together during reorder.

Users can reorder effects freely via the chain list's reorder mode. Custom order is saved in presets and plugin state. "Reset Order" restores the default.

### Design Principles

1. **Each effect is a self-contained processor.** Base class `EffectProcessor` with prepare/process/reset/bypass. Atomic bypass, safe for cross-thread access.

2. **Effect chain is an ordered vector.** Audio flows sequentially. Reordering is thread-safe (SpinLock with try-lock on audio thread).

3. **UI is decoupled from DSP.** DSP code in `src/dsp/` has zero UI dependencies. This separation makes VST export straightforward.

4. **Presets use JSON.** Per-effect parameters, metadata, and optionally chain order. Plugin state (XML) also persists chain order.

### Project Structure

```
openriffbox/
+-- src/
|   +-- dsp/          # Effect processors (no UI dependencies)
|   +-- ui/           # JUCE UI components
|   +-- preset/       # Preset management
+-- presets/           # Factory and user presets (JSON)
+-- resources/
|   +-- fonts/        # Inter font family
|   +-- irs/          # 14 cabinet impulse responses
+-- docs/             # Documentation and roadmap
```

--

## Current State (v0.8)

14 effects across 8 slots, all implemented:

- **Diode Drive** -- TS808-style circuit model with Newton-Raphson solver
- **Distortion** -- 4 modes (Overdrive, Tube Drive, Distortion, Metal)
- **Amp Sim** -- 3 engines:
  - Silver: lightweight, 2x oversampled, clean to crunch
  - Gold: multi-stage waveshaper preamp, circuit-modeled tone stack, push-pull power amp with NFB, power supply sag
  - Platinum: 5-stage tube preamp cascade, phase splitter, push-pull power amp, output transformer, thermal noise (CPU-intensive)
- **Noise Gate** -- full gate with sidechain HPF and hysteresis
- **Analog Delay** -- BBD model with feedback saturation and triple modulation
- **Spring Reverb** -- allpass chirp chain + FDN tank, 3 spring types
- **Plate Reverb** -- cross-coupled tank, multi-tap stereo output, 3 plate types
- **Chorus** -- BBD-style, triangle LFO, 180-deg stereo
- **Flanger** -- comb filter feedback with +/- polarity, soft saturation
- **Phaser** -- 4/8/12-stage allpass cascade, 100-4000 Hz sweep
- **Vibrato** -- sine LFO, 100% wet pitch modulation
- **EQ** -- 3-band semi-parametric with sweepable mid and output trim

Additional features: built-in tuner, preset system (save/load/quick-switch), reorderable signal chain, 14 cabinet IRs + custom IR loading, output limiter.

--

## What's Next (in no specific order)

### More Effects
- **Compressor** -- dynamics slot before drives for even saturation
- **Tremolo** -- LFO amplitude modulation (5th Modulation tab)
- **Wah** -- auto-wah, envelope filter, and manual modes
- **Metronome** -- built-in click track (UI placeholder exists)

### Sound Quality
- Knob response curves (logarithmic/S-curve mappings for more musical parameter feel)
- Platinum engine polish (master gain curve, parameter response tuning, possible CPU optimization)

### DAW Integration
- VST3 plugin polish (works on Windows, some quirks to iron out)
- Migrate all parameters to JUCE AudioProcessorValueTreeState
- Full DAW automation support
- MIDI CC mapping
- Undo/redo

### Stretch Goals
- Scaled UI (window resizes but UI elements don't scale yet)
- NAM / AIDA-X model loading
- Cross-platform builds (Mac, Linux)

--

## Non-Goals

- Mobile support (iOS/Android)
- Cloud anything
- Paid anything (this is free software, period)
- DAW-level features (recording, timeline, mixing, multitrack)

