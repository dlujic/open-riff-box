# Open Riff Box -- Roadmap

> Free, open-source, lightweight guitar effects processor.
> Plug in your guitar, choose your effects, and play.

--

## Tech Stack

- **Framework:** JUCE 8 (C++17)
- **Build:** CMake -- MSVC on Windows (static CRT `/MT`), Clang on macOS (universal x86_64 + arm64), GCC/Clang on Linux
- **Audio:** ASIO/WASAPI/DirectSound on Windows, CoreAudio on macOS, ALSA + JACK on Linux
- **Distribution:** Portable archives -- no installer, no registry, no admin rights. Windows `.zip`, macOS/Linux `.tar.gz` (beta)
- **Config:** Windows/Linux: stored next to the binary (portable mode), OS config dir as fallback. macOS: `~/Library/Application Support/OpenRiffBox/`
- **Requirements:** Windows 10/11 x64 (ASIO interface recommended) / macOS 10.13+ Intel or Apple Silicon / Linux x86_64

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
|  [Comp]->[Wah]->[Diode]->[Dist]->            |
|  [Amp Sim S/G/P]->[Gate]->[Delay]->          |
|  [Reverb Spr/Plate]->                        |
|  [Modulation Cho/Fla/Pha/Vib/Tre]->[EQ]      |
+----------------------------------------------+
|              Audio I/O Layer                   |
|  (JUCE AudioDeviceManager, native audio APIs) |
+----------------------------------------------+
```

### Effect Chain (user-reorderable)

Default order (10 visual rows):

```
0: Compressor        (dynamics)
1: Wah               (filter)
2: Diode Drive       (drive)
3: Distortion        (drive)
4: Amp Sim           (drive)       [Silver / Gold / Platinum]
5: Noise Gate        (dynamics)
6: Delay             (time-based)
7: Reverb            (time-based)  [Spring / Plate]
8: Modulation        (modulation)  [Chorus / Flanger / Phaser / Vibrato / Tremolo]
9: EQ                (utility)
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

## Current State (v0.8.5)

17 effects across 10 slots, all implemented:

- **Compressor** -- feedforward log-domain design, 3 modes: Studio (transparent VCA, program-dependent release), Squeeze (high-ratio Dyna Comp squash), Opto (LA-2A-style two-stage release). Parallel blend, auto makeup, gain reduction meter.
- **Wah** -- GCB-95-style circuit model: single resonant bandpass with pot-taper dead zones, switchable input coloration. 3 control modes: Manual (fixed/automatable position), Auto (LFO sweep, sine or triangle), Envelope (pick-attack follower with sens/attack/release)
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

Additional features: built-in tuner, built-in metronome (standalone only; woodblock click, tap tempo, time signatures), preset system (save/load, quick-access slots with persisted assignments, modified-preset indicator), 13 loudness-matched factory presets, reorderable signal chain, 14 cabinet IRs + custom IR loading, output limiter.

Platforms: Windows x64 (primary), macOS 10.13+ universal (beta), Linux x86_64 (beta). Standalone + VST3 on all three.

--

## What's Next (in no specific order)

### Sound Quality
- Platinum Normal channel polish -- grid-conduction clamp for noise at high LEVEL settings, channel level matching (factory preset levels recalibrate afterwards)
- Platinum engine polish (parameter response tuning, possible CPU optimization)
- In-house cabinet IR recording to replace the current placeholder set
- Knob response curves (logarithmic/S-curve mappings for more musical parameter feel)
- Parallel mix law for Delay/Reverb (low priority, exploratory) -- optional dry-at-unity
  mix mode (aux-send style: dry stays untouched, wet layers on top) instead of the
  equal-power crossfade. Mimics console aux routing for time-based effects. Spends
  headroom, so it wants the input trim to land first. Try it and judge by ear.

### UI & Workflow
- Default chain order tweak -- modulation block ahead of delay
- Preset browser: mark presets assigned to quick slots
- Settings sub-sections -- keybinds, MIDI mappings, preset categories
- Per-effect documentation (docs/effects.md)

### DAW Integration
- VST3 plugin polish (works on Windows, some quirks to iron out)
- VST3 parameter automation audit (click/zipper noise on parameter changes)
- Persist app settings in VST3 too (quick-slot assignments, tooltips, custom IR paths are standalone-only today)
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
- macOS code signing / notarization (beta ships unsigned with Gatekeeper walkthrough)

--

## Non-Goals

- Mobile support (iOS/Android)
- Cloud anything
- Paid anything (this is free software, period)
- DAW-level features (recording, timeline, mixing, multitrack)

