# Effects Guide

Detailed documentation for every effect in Open Riff Box -- what each parameter does, what the values mean, and tips for getting good tones.

All percentage parameters (0-100%) map to internal 0-1 ranges. Default values are shown in **bold**.

---

## Noise Gate

Cuts signal below a threshold to eliminate hum, buzz, and noise between playing. Uses a sidechain high-pass filter (150 Hz) to ignore low-frequency rumble when detecting signal level, plus 6 dB of hysteresis to prevent chattering at the threshold boundary.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Threshold | -80 to 0 dB | **-40 dB** | Signal level below which the gate closes. Lower = more sensitive (lets quieter signals through). |
| Attack | 0.1 to 50 ms | **1 ms** | How quickly the gate opens when signal exceeds threshold. |
| Hold | 0 to 500 ms | **50 ms** | How long the gate stays open after signal drops below threshold before starting to close. |
| Release | 5 to 2000 ms | **100 ms** | How long the gate takes to fully close after hold expires. |
| Range | -90 to 0 dB | **-90 dB** | How much the gate attenuates when closed. -90 dB = full silence. Higher values (e.g. -20 dB) let some signal bleed through for a more natural feel. |

### Tips

- **For high-gain distortion:** Set threshold around -40 to -30 dB, hold 50-100 ms, release 50-100 ms. You want it tight enough to kill buzz between riffs but not so aggressive it clips note tails.
- **For clean/crunch tones:** Lower the threshold to -50 to -60 dB. Clean signals are quieter, so the gate needs to be more sensitive.
- **Natural decay:** If the gate cuts off too abruptly, increase Hold (100-200 ms) and Release (200-500 ms), or raise Range to -30 dB to let some bleed through.
- **Palm mutes:** Keep Hold short (20-50 ms) for tight, staccato palm-muted chugs.

---

## Diode Drive

TS808-style circuit model. Uses Newton-Raphson iterative solving of the actual op-amp + feedback diode clipping circuit, including a feedback capacitor that rolls off treble at higher drive settings (just like the real pedal).

This is a mid-focused overdrive -- it boosts mids, tames lows, and rolls off highs. Placed before Distortion in the chain, just like a real pedalboard where a tubescreamer-style pedal tightens the signal before higher-gain stages.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Drive | 0-100% | **50%** | Controls the feedback resistance, which sets distortion intensity. At low values you get a clean boost with mid emphasis. At high values, full diode clipping. |
| Level | 0-100% | **70%** | Output volume. |

### Tips

- **Clean boost into amp sim:** Drive 10-25%, Level 70-80%. Pushes the Amp Sim's preamp harder without adding much distortion of its own.
- **Classic overdrive:** Drive 40-60%. The sweet spot -- warm, singing sustain with mid emphasis.
- **Maxed out:** Drive 80-100%. Heavy saturation, compressed, thick. Great for leads.
- **Stacking with Distortion:** Keep Drive low (15-30%) and let it feed into Distortion for tighter bass and more mid presence. The chain order does this automatically -- just enable both. This is how many players use a tubescreamer-style pedal.

---

## Distortion

Four distortion modes ranging from mild overdrive to extreme metal. All modes use 4x oversampling to prevent aliasing artifacts.

Signal chain: Pre-HPF (100 Hz) -> Waveshaper -> Post-processing -> DC blocker -> Tone LPF -> Output level -> Dry/wet mix.

### Modes

- **Overdrive** -- Mild, amp-like breakup. Good for blues and classic rock. Includes a mid-hump EQ and pre-clip LPF to soften the top end.
- **Tube Drive** -- Warmer, more compressed overdrive with tube-like asymmetric clipping. More sustain than Overdrive.
- **Distortion** -- Two-stage cascaded clipping with interstage filtering. Tighter, more aggressive. Includes auto-darkening (the LPF tracks drive amount to prevent fizz at high gain).
- **Metal** -- Three-stage cascaded high-gain distortion. Five post-filters sculpt an aggressive, tight sound. Built-in noise floor expander kills the fizz tail.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Drive | 0-100% | **50%** | Distortion intensity. Higher = more gain, more harmonics, more sustain. |
| Tone | 0-100% | **65%** | Post-distortion low-pass filter. Lower = darker, warmer. Higher = brighter, more bite. |
| Level | 0-100% | **70%** | Output volume after distortion. |
| Mode | Overdrive / Tube Drive / Distortion / Metal | **Overdrive** | Distortion character (see above). |
| Dry Mix | 0-100% | **20%** | Blends clean signal with distorted signal using equal-power crossfade. 0% = fully distorted. |
| Clipping | Gentle / Warm / Sharp / Aggro | **Warm** | Tube clip hardness (Overdrive and Tube Drive modes only). Gentle = more headroom, smooth compression. Aggro = sharp knee, nearly hard-clips. |
| Saturate | On/Off + 0-100% | **Off, 50%** | Pre-distortion compressor. Evens out dynamics before clipping. Useful for tighter, more consistent distortion at any drive level. |

### Tips

- **Classic rock:** Overdrive mode, Drive 40-60%, Tone 50-70%, Clipping Warm.
- **Blues:** Overdrive mode, Drive 25-40%, Tone 40-55%, Clipping Gentle. Keep Dry Mix at 20-30% for note clarity.
- **Hard rock:** Distortion mode, Drive 50-70%, Tone 55-70%.
- **Metal rhythm:** Metal mode, Drive 60-80%, Tone 70-90%. The built-in filtering and expander handle the rest.
- **Metal lead:** Metal mode, Drive 70-90%, Tone 50-65% (back off tone for smoother leads).
- **If it sounds fizzy:** Lower the Tone, or try Distortion mode instead of Metal -- it has auto-darkening that tracks the drive.

---

## Amp Sim

Full amplifier simulation with three engine options: **Silver**, **Gold**, and **Platinum**. Switch between engines using the tabs at the top of the Amp Sim panel. All three share the same 14 cabinet IRs.

### Silver Engine

Lightweight amp sim with 3-band EQ, preamp boost, power amp stage, and cabinet simulation. 2x oversampled. Good for clean to moderate crunch tones with low CPU usage.

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Gain | 0-100% | **30%** | Preamp input drive. Controls how hard you push the amp. |
| Bass | 0-100% | **50%** | Low shelf EQ at 150 Hz. +/-12 dB range. 50% = flat. |
| Mid | 0-100% | **50%** | Peaking EQ at 800 Hz (Q=0.8). +/-12 dB range. 50% = flat. |
| Treble | 0-100% | **50%** | High shelf EQ at 3 kHz. +/-12 dB range. 50% = flat. |
| Pre Boost | On/Off | **Off** | +12 dB preamp boost with soft saturation. Pushes the signal into breakup. |
| Speaker Drive | 0-100% | **20%** | Power amp and speaker distortion amount. Adds warmth, compression, and even harmonics. |
| Cabinet | 14 presets + None + Custom | **Studio 57** | Speaker cabinet impulse response (see list below). |
| Brightness | 0-100% | **50%** | Post-cab brightness. Lower = darker, rolled-off top end. Higher = full brightness. |
| Mic | 0-100% | **30%** | Simulates mic placement. Low = close/dark, high = bright/airy. Neutral at 50%. |

### Gold Engine

Higher-fidelity amp model with a multi-stage waveshaper preamp, circuit-modeled tone stack, push-pull power amp with negative feedback, power supply sag, and output transformer simulation. 4x oversampled. The "full amp experience" for classic rock to high gain.

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Gain | 0-100% | **40%** | Preamp input drive. |
| Bass | 0-100% | **50%** | Tone stack bass. Circuit-modeled 3rd-order IIR filter. |
| Mid | 0-100% | **60%** | Tone stack mid. |
| Treble | 0-100% | **50%** | Tone stack treble. |
| Pre Boost | On/Off | **Off** | Gain stage boost before the preamp. |
| Speaker Drive | 0-100% | **20%** | Power amp saturation with push-pull even harmonics. |
| Presence | 0-100% | **70%** | Negative feedback loop cutoff. Controls upper-mid clarity and bite. |
| Cabinet | 14 presets + None + Custom | **Studio 57** | Speaker cabinet impulse response. |
| Brightness | 0-100% | **60%** | Post-cab brightness overlay. |
| Mic | 0-100% | **50%** | Mic position simulation. |

### Platinum Engine

Full tube amp circuit model -- five cascaded 12AX7 triode preamp stages, a long-tailed pair phase splitter, and EL34 push-pull power amp with negative feedback loop. Includes coupling cap filters, Miller capacitance, power supply sag, output transformer, and thermal noise. 4x oversampled. The most detailed and CPU-intensive engine.

> **Note:** Platinum is computationally heavy. On modest hardware you may notice higher CPU usage, especially at lower buffer sizes. If you experience audio dropouts, try increasing your buffer size or switching to Gold or Silver.

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Gain | 0-100% | **50%** | Preamp input gain (first triode stage). |
| OV Level | 0-100% | **70%** | Overdrive level attenuator between preamp and tone stack. Controls how much preamp signal hits the power section. |
| Bass | 0-100% | **50%** | Tone stack bass. |
| Mid | 0-100% | **50%** | Tone stack mid. |
| Treble | 0-100% | **50%** | Tone stack treble. |
| Master | 0-100% | **30%** | Master volume before the phase splitter and power amp. |
| Speaker Drive | 0-100% | **30%** | Power amp saturation amount. |
| Gain Mode | GAIN1 / GAIN2 | **GAIN1** | Preamp routing mode. GAIN2 uses an alternate signal path for a different voicing. |
| Cabinet | 14 presets + None + Custom | **Studio 57** | Speaker cabinet impulse response. |
| Mic | 0-100% | **50%** | Mic position simulation. |

### Cabinet IRs

All three engines share the same set of 14 speaker cabinet impulse responses. You can also select "No Cabinet" for a raw amp tone, or load a custom IR from a WAV file.

| # | Name | Character |
|---|------|-----------|
| 0 | Studio 57 | Classic close-miked 4x12. Balanced, mix-ready. |
| 1 | In Your Face | Aggressive, forward presence. Cuts through a mix. |
| 2 | Tight Box | Compact, focused. Good for tight rhythm tones. |
| 3 | Crystal 412 | Flat, transparent 4x12. Even response across all bands. |
| 4 | British Bite | Bright, crunchy British-voiced cab. |
| 5 | Velvet Blues | Warm, dark, fat bass with mid scoop. Smooth blues tone. |
| 6 | Bass Colossus | Extended low-frequency response. For drop-tuned and bass. |
| 7 | Acoustic Body | Extended highs, upper-mid body resonance. For acoustic/piezo DI. |
| 8 | Ambient Hall | Room-miked with natural ambiance. Open, spacious. |
| 9 | Blackface Clean | Warm, round clean tone. Scooped mids, smooth top end. |
| 10 | Plexi Roar | Aggressive upper-mid bark. Classic British rock voicing. |
| 11 | Thunder Box | Deep, heavy low end with tight mids. Modern high-gain cab. |
| 12 | Tweed Gold | Warm, vintage midrange character. Responds well to dynamics. |
| 13 | Vox Chime | Bright, chimey, jangly top end. Great for cleans and light crunch. |

### Tips

- **Clean amp:** Silver engine, Gain 20-40%, all EQ at 50%, Pre Boost off, Speaker Drive 0-10%. Let the cabinet IR do the work.
- **Crunchy rhythm:** Gold engine, Gain 40-60%, Pre Boost on, Speaker Drive 20-40%.
- **High gain:** Use Distortion or Diode Drive *before* the Amp Sim for extra gain. Keep Amp Sim gain moderate (30-50%) and let Speaker Drive add warmth (20-40%).
- **Full tube experience:** Platinum engine, Gain 50-70%, Master 30-50%, Speaker Drive 30-50%. Use OV Level to control how hard the preamp pushes the power section.
- **Low CPU:** Silver engine uses 2x oversampling vs 4x for Gold/Platinum. If CPU is tight, Silver is your friend.

---

## Analog Delay

Bucket-brigade delay (BBD) model with clock-tracking anti-aliasing filters, bandwidth loss, feedback saturation, and triple modulation (wow, flutter, and random wander) for authentic analog character.

Uses cubic Hermite interpolation for smooth fractional delay reads.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Time | 20-400 ms (logarithmic) | **~200 ms** | Delay time. The scale is logarithmic -- more resolution at shorter times. |
| Intensity | 0-100% | **35%** | Feedback amount -- how many repeats. Higher values give more echoes. Above ~80% the delay starts to self-oscillate (with saturation). |
| Echo | 0-100% | **50%** | Wet/dry mix. 0% = fully dry, 100% = fully wet. |
| Mod | 0-5 ms | **30%** | Modulation intensity -- how much the delay time wobbles. Adds chorus-like movement. |
| Rate | 0.1-5.0 Hz | **30%** | Modulation speed. Higher = faster wobble. |
| Tone | 0-100% | **50%** | Feedback path tone. Lower = darker repeats (more high-frequency loss per echo). Higher = brighter repeats. |

### Tips

- **Slapback:** Time ~80 ms, Intensity 15-25%, Echo 40-50%, Mod Depth 0%. Quick single repeat.
- **Classic analog:** Time 200-300 ms, Intensity 30-45%, Echo 40-50%, Mod Depth 20-30%. Warm, dark repeats that fade naturally.
- **Ambient wash:** Time 350-400 ms, Intensity 50-65%, Echo 50-60%, Mod Depth 40-50%, Tone 30-40%. Lush, swirling tails.
- **Self-oscillation:** Push Intensity above 80%. The feedback saturates so it won't blow up, but it will create runaway oscillation effects. Fun for noise/ambient.
- **Clean repeats:** Keep Mod Depth at 0% and Tone at 60-70% for more "digital-sounding" repeats from an analog circuit.

---

## Spring Reverb

Perceptual spring reverb model using a 120-section stretched allpass chirp chain for the characteristic metallic "boing", fed into a 3-element feedback delay network (FDN) tank.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Dwell | 0-100% | **50%** | Input drive into the reverb. Higher = more signal hitting the springs, louder and denser reverb. |
| Decay | 0-100% | **50%** | How long the reverb tail lasts. Controls FDN feedback amount. |
| Mix | 0-100% | **30%** | Wet/dry balance. |
| Drip | 0-100% | **40%** | Spring chirp intensity -- the metallic "boing" character. Higher = more splash and drip. |
| Tone | 0-100% | **50%** | Damping brightness. Lower = darker, more muted reverb. Higher = brighter, more present. |
| Spring | Short / Medium / Long | **Medium** | Spring type preset -- controls delay times and damping character. |

### Spring Types

| Type | Character |
|------|-----------|
| Short | Tight, bright. Classic surf reverb. Quick decay. |
| Medium | Balanced warmth. All-purpose. |
| Long | Cavernous, deep. Long, atmospheric tail. |

### Tips

- **Surf:** Short spring, Dwell 60-70%, Decay 40-50%, Drip 60-80%, Mix 40-50%. Splash!
- **Ambient clean:** Long spring, Dwell 30-40%, Decay 60-70%, Drip 20-30%, Mix 30-40%, Tone 40-50%.
- **Just a touch:** Medium spring, Dwell 40%, Decay 40%, Drip 30%, Mix 15-25%. Adds space without muddiness.
- **Drip control:** The Drip knob is the key personality control. At 0% the chirp chain output is minimal and you mostly hear the FDN tank -- smooth, diffuse reverb. At 100% the spring character dominates with metallic transients.
- **Works best with:** Clean and crunch tones. High-gain distortion tends to make any reverb muddy -- use lower Mix and Decay for distorted signals.

---

## Plate Reverb

Canonical plate reverb algorithm with 4 input allpass diffusers, a cross-coupled tank with modulated allpass sections, and multi-tap stereo output. Produces a dense, smooth, non-metallic reverb that works well for both clean and driven tones.

Mono input, stereo output -- the stereo image emerges from the multi-tap output extraction.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Decay | 0-100% | **50%** | Reverb tail length. Higher = longer, more sustained tail. |
| Damping | 0-100% | **30%** | High-frequency absorption. Higher = darker, more damped reverb that decays faster in the highs. |
| Pre-Delay | 0-100 ms | **0 ms** | Gap before the reverb onset. Adds separation between dry signal and reverb. |
| Mix | 0-100% | **30%** | Wet/dry balance. |
| Width | 0-100% | **100%** | Stereo width of the reverb output. 0% = mono, 100% = full stereo spread. |
| Plate | Studio / Bright / Dark | **Studio** | Plate type preset -- controls tank size and damping voicing. |

### Plate Types

| Type | Character |
|------|-----------|
| Studio | Classic warm plate. Balanced frequency response, medium density. All-purpose. |
| Bright | Shorter, airier plate with more high-frequency content. Adds shimmer and sparkle. |
| Dark | Longer, thicker plate with heavy high-frequency damping. Warm, enveloping tail. |

### Tips

- **Vocal-style shimmer:** Bright plate, Decay 50-60%, Damping 20-30%, Pre-Delay 20-40 ms, Mix 25-35%.
- **Ambient pad:** Dark plate, Decay 70-85%, Damping 30-40%, Width 100%, Mix 40-50%. Lush, slow-building wash.
- **Tight studio:** Studio plate, Decay 30-40%, Pre-Delay 10-20 ms, Mix 20-30%. Professional, unobtrusive space.
- **Pre-Delay trick:** Adding 20-40 ms of pre-delay helps keep the dry attack clean and separate from the reverb. Especially useful for rhythmic playing.
- **Spring vs Plate:** Spring has metallic "boing" character and works great for surf and vintage tones. Plate is smoother, denser, and sits better behind distorted tones.

---

## Chorus

BBD-style chorus. Single-pass modulated delay with triangle LFO, 180-degree stereo phase offset, and a fixed 8 kHz color filter for analog warmth.

No feedback -- this is a clean, single-pass chorus (not a flanger).

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Rate | 0.3-4.0 Hz (logarithmic) | **~0.9 Hz** | LFO speed. Slower = gentle swaying. Faster = vibrato-like warble. |
| Depth | 0.5-5.0 ms | **~2.3 ms** | Modulation depth -- how far the delay time sweeps. More depth = more dramatic detuning. |
| Tone | 800 Hz-12 kHz | **70%** | Wet signal filter. Variable LPF from 800 Hz (dark) to 12 kHz (bright). Controls the brightness of the chorus effect only, not the dry signal. |
| Mix | 0-100% | **50%** | Wet/dry mix. Effect level blended with dry signal. |

### Tips

- **Subtle shimmer:** Rate 0.5-0.8 Hz, Depth 1.0-2.0 ms, Mix 30-40%. Gentle, always-on widening.
- **Classic 80s:** Rate 0.8-1.2 Hz, Depth 2.5-3.5 ms, Mix 50-60%, Tone 60-70%.
- **Deep and watery:** Rate 0.4-0.6 Hz, Depth 4.0-5.0 ms, Mix 50-60%. Slow, seasick modulation.
- **Stereo width:** The chorus produces stereo output with 180-degree LFO phase offset between channels. Headphones or stereo speakers will reveal the full width.

---

## Flanger

BBD-style flanger with a feedback path for pronounced comb filtering. Features a DC blocker and soft saturation in the feedback loop to keep self-oscillation musical, plus selectable feedback polarity for different tonal characters.

Triangle LFO with 90-degree stereo offset.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Rate | 0-100% | **20%** | LFO speed (0.05-10 Hz, quadratic scaling for more resolution at slow speeds). |
| Depth | 0-100% | **35%** | Modulation depth (0.1-4.0 ms). How far the delay sweeps around the center point. |
| Manual | 0-100% | **30%** | Center delay time (0.5-10 ms). Sets the base frequency of the comb filter. Lower = more metallic, higher = more chorus-like. |
| Feedback | 0-100% | **45%** | Feedback intensity. Higher values create sharper, more resonant comb filter peaks. |
| Polarity | +/- | **+** | Feedback sign. Positive (+) reinforces the fundamental for a fuller sound. Negative (-) cancels it for a hollow, nasal "through-zero" character. |
| EQ | 0-100% | **70%** | Wet signal LPF (800 Hz-12 kHz). Tames brightness in the flanged signal. |
| Mix | 0-100% | **50%** | Wet/dry mix with equal-power crossfade. |

### Tips

- **Classic jet sweep:** Rate 15-25%, Depth 30-50%, Manual 20-30%, Feedback 40-60%, Polarity +.
- **Through-zero hollow:** Same as above but Polarity -. The sound thins out dramatically at the sweep bottom.
- **Slow and dramatic:** Rate 5-10%, Depth 40-60%, Feedback 50-70%. Long, sweeping jet-plane effect.
- **Metallic resonance:** Manual 10-20% (short center delay), Feedback 60-80%. Creates pitched, ringing tones.
- **Subtle width:** Rate 10-15%, Depth 15-25%, Feedback 20-30%, Mix 30-40%. Gentle stereo movement without obvious flanging.

---

## Phaser

Allpass filter cascade with selectable stage count. Each pair of allpass stages creates one notch in the frequency response, swept by a triangle LFO. Feedback deepens the effect by reinforcing the notch pattern.

Triangle LFO with 5% stereo offset for subtle width.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Rate | 0-100% | **15%** | LFO speed (0.05-10 Hz, quadratic scaling). |
| Depth | 0-100% | **50%** | Sweep depth -- how wide the notch frequencies move. 0% = static, 100% = full 100-4000 Hz sweep. |
| Feedback | 0-100% | **30%** | Feedback intensity. Sharpens the notches for a more pronounced, resonant effect. |
| Mix | 0-100% | **50%** | Wet/dry mix with equal-power crossfade. |
| Stages | Classic / Rich / Deep | **Classic** | Number of allpass stages: Classic = 4 (2 notches), Rich = 8 (4 notches), Deep = 12 (6 notches). |

### Tips

- **Classic phaser:** Classic stages, Rate 10-20%, Depth 40-60%, Feedback 30-50%. Smooth, musical sweep.
- **Thick and swirly:** Rich stages, Rate 10-15%, Depth 50-70%, Feedback 40-60%. Denser notch pattern.
- **Deep space:** Deep stages, Rate 5-10%, Depth 60-80%, Feedback 50-70%. Dramatic, complex modulation.
- **Subtle shimmer:** Classic stages, Rate 8-12%, Depth 20-30%, Feedback 15-25%, Mix 30-40%. Gentle movement that doesn't overwhelm the tone.
- **Resonant sweep:** Any stage count, Feedback 60-80%. Creates sharper, more vocal-like resonant peaks. Careful -- high feedback can get intense.

---

## Vibrato

BBD-style pitch vibrato. Pure pitch modulation via a modulated delay line with sine LFO -- no dry signal mixed in. Uses a fixed 4 ms center delay with a BBD color filter for warmth.

This is a 100% wet effect by design -- it modulates pitch, not amplitude. For pitch + dry blend, use Chorus instead.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Rate | 0-100% | **50%** | LFO speed (0.5-10 Hz, quadratic scaling). |
| Depth | 0-100% | **30%** | Pitch excursion (0.1-3.0 ms). Higher = wider pitch wobble. |
| Tone | 0-100% | **70%** | Output LPF (800 Hz-12 kHz). Controls brightness of the vibrato signal. |

### Tips

- **Gentle warble:** Rate 30-40%, Depth 15-25%. Subtle pitch movement -- nice for cleans.
- **Classic vibrato:** Rate 45-60%, Depth 25-40%. Noticeable pitch wobble, musical and expressive.
- **Seasick:** Rate 15-25%, Depth 50-70%. Slow, deep pitch swings. Lo-fi and woozy.
- **Fast shimmer:** Rate 70-85%, Depth 10-20%. Rapid, tight vibrato -- almost Leslie-like at the extremes.
- **Chorus vs Vibrato:** Chorus blends wet + dry (creating detuning). Vibrato is 100% wet (pure pitch modulation). Vibrato has a more direct, pronounced pitch effect.

---

## EQ (3-Band Equalizer)

3-band semi-parametric equalizer with a sweepable mid frequency and output level trim. Placed last in the chain as a master tone shaper.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Bass | -12 to +12 dB | **0 dB** | Low shelf gain at 100 Hz. Boost or cut the low end. |
| Mid | -12 to +12 dB | **0 dB** | Peaking filter gain (Q=1.0) at the frequency set by Mid Freq. |
| Mid Freq | 200-5000 Hz (logarithmic) | **~1000 Hz** | Center frequency for the Mid band. Sweep to find the frequency you want to boost or cut. |
| Treble | -12 to +12 dB | **0 dB** | High shelf gain at 4000 Hz. Boost or cut the highs. |
| Level | -12 to +6 dB | **0 dB** | Output level trim. Use to compensate for volume changes from EQ adjustments. |

### Tips

- **Cut mud:** Mid at -3 to -6 dB, Mid Freq at 300-400 Hz. Clears up boomy or boxy tones.
- **Add presence:** Mid at +3 to +6 dB, Mid Freq at 2000-3500 Hz. Helps guitar cut through a mix.
- **Scoop for metal:** Bass +3, Mid -4 to -6 at 500-800 Hz, Treble +3. Classic scooped metal tone.
- **Warm up a bright signal:** Treble -3 to -6 dB, Bass +2 to +3 dB.
- **Volume boost for solos:** Leave all bands flat, set Level to +3 to +6 dB. Instant clean volume boost.
- **Find problem frequencies:** Boost Mid to +12 dB and sweep Mid Freq slowly. The frequency that sounds worst is the one you should *cut*.

---

## Always-On Processing

These are handled automatically by the effect chain and don't have user controls:

- **Input DC Blocker** -- 5 Hz high-pass filter removes DC offset from the input signal.
- **Output Soft Limiter** -- Brickwall limiter at -0.1 dBFS prevents clipping. Can be toggled from the sidebar (enabled by default).

---

## Effect Chain Order

The default signal chain runs top to bottom:

1. **Diode Drive** -- Mid-boost overdrive
2. **Distortion** -- Main distortion/gain stage
3. **Amp Sim** -- Amplifier simulation (Silver / Gold / Platinum)
4. **Noise Gate** -- Placed after all drive stages to gate their noise
5. **Delay** -- Analog delay
6. **Reverb** -- Spring or Plate reverb
7. **Modulation** -- Chorus, Flanger, Phaser, or Vibrato
8. **EQ** -- Final tone shaping

The chain order can be rearranged using the reorder controls in the chain list sidebar.

---

## General Tips

- **Gain staging matters.** If your signal is too hot going into distortion, it'll sound harsh. Use the Noise Gate to clean up between drive stages, and the EQ (last in chain) to shape the final output.
- **Less is more with high gain.** Back off the Drive and let the tone controls and cabinet IR do the heavy lifting. Real amps don't need Drive at 100% to sound heavy.
- **Use the presets as starting points.** Load a factory preset and tweak from there rather than starting from scratch.
- **ASIO for serious playing.** WASAPI adds latency. If you notice a delay between picking and hearing, switch to an ASIO driver in the audio settings. Buffer size of 128 or 256 samples at 44.1 kHz is a good target.
- **The cabinet IR makes a huge difference.** Distortion effects are designed to sound good standalone, but pairing them with the right cabinet IR makes a big difference in how finished they sound.
- **Reset to defaults.** Every effect panel has a reset button. If you've tweaked yourself into a corner, hit reset and start fresh.
