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
|  [Comp]->[Diode]->[Dist]->[Amp Sim S/G/P]->  |
|  [Gate]->[Delay]->[Reverb Spr/Plate]->        |
|  [Modulation Cho/Fla/Pha/Vib/Tre]->[EQ]      |
+----------------------------------------------+
|              Audio I/O Layer                   |
|    (JUCE AudioDeviceManager, ASIO/WASAPI)     |
+----------------------------------------------+
```

### Effect Chain (user-reorderable)

Default order (9 visual rows):

```
0: Compressor        (dynamics)
1: Diode Drive       (drive)
2: Distortion        (drive)
3: Amp Sim           (drive)       [Silver / Gold / Platinum]
4: Noise Gate        (dynamics)
5: Delay             (time-based)
6: Reverb            (time-based)  [Spring / Plate]
7: Modulation        (modulation)  [Chorus / Flanger / Phaser / Vibrato / Tremolo]
8: EQ                (utility)
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

## Current State (v0.8.3)

16 effects across 9 slots, all implemented:

- **Compressor** -- feedforward log-domain design, 3 modes: Studio (transparent VCA, program-dependent release), Squeeze (high-ratio Dyna Comp squash), Opto (LA-2A-style two-stage release). Parallel blend, auto makeup, gain reduction meter.
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
- **Tremolo** -- 3 modes: Photo (LDR comparator + asymmetric envelope), Bias (4x oversampled tanh + DC blocker), Harmonic (LR4 split at 400 Hz, antiphase bands). Mono-safe stereo width.
- **EQ** -- 3-band semi-parametric with sweepable mid and output trim

Additional features: built-in tuner, built-in metronome (standalone only; woodblock click, tap tempo, time signatures), preset system (save/load/quick-switch), reorderable signal chain, 14 cabinet IRs + custom IR loading, output limiter.

--

## What's Next (in no specific order)

### More Effects
- **Wah** -- auto-wah, envelope filter, and manual modes

### Sound Quality
- Knob response curves (logarithmic/S-curve mappings for more musical parameter feel)
- Platinum engine polish (master gain curve, parameter response tuning, possible CPU optimization)
- Parallel mix law for Delay/Reverb (low priority, exploratory) -- optional dry-at-unity
  mix mode (aux-send style: dry stays untouched, wet layers on top) instead of the
  equal-power crossfade. Mimics console aux routing for time-based effects. Spends
  headroom, so it wants the input trim to land first. Try it and judge by ear.

### DAW Integration
- VST3 plugin polish (works on Windows, some quirks to iron out)
- Global input trim + meter guidance -- the chain is voiced for instrument-level input
  (peaks around -20 dBFS). Line-level or pre-processed signals (an amp sim upstream in
  the DAW chain, FX-only use behind another rig) run ~20 dB hot, pin the output limiter,
  and overdrive level-sensitive stages like delay/flanger feedback saturation. A trim
  knob ahead of the chain fixes gain staging in both DAW and standalone.
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

