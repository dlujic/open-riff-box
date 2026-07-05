#pragma once

// Offline file processor for DSP testing.
// Enabled in Debug builds automatically; also enabled in any config when
// built with -DORB_OFFLINE_TOOLS=ON. One render per process invocation.
//
// Mode 1 - legacy amp-direct (--process-file input output [options]):
//   Processes audio through AmpSimGold or AmpSimPlatinum with individual flags.
//   GPU harness depends on these flags; they must not change.
//
//   --engine <gold|platinum>   Engine (default: gold)
//   --gain <0-1>               Preamp gain (default: 1.0)
//   --bass <0-1>               Bass (default: 0.5)
//   --mid <0-1>                Mid (default: 0.6)
//   --treble <0-1>             Treble (default: 0.5)
//   --speaker-drive <0-1>      Speaker drive (default: 0.2)
//   --brightness <0-1>         Brightness (default: 0.6)
//   --mic-position <0-1>       Mic position (default: 0.5)
//   --cabinet <0-15>           Cabinet index (0-13=named, 14=none, 15=custom)
//   --boost                    Enable preamp boost (Gold); Normal CLEAN/BOOST (Platinum)
//   --ov-level <0-1>           OV Level (Platinum only, default: 0.7)
//   --master <0-1>             Master knob position (Platinum only, default: 0.64;
//                              VR12 A-taper + loading applied inside the engine)
//   --gain-mode <0-1>          0=GAIN1, 1=GAIN2 (Platinum only)
//   --stage-limit <1-9>        Tap after stage N (Platinum debug)
//   --channel <od|normal>      Platinum channel switch (default: od)
//   --input-jack <high|low>    Platinum input jack (default: high; low = -6dB pad)
//   --normal-level <0-1>       Platinum Normal channel LEVEL, VR1 (default: 0.5).
//                              --bass/--mid/--treble double as the Normal EQ
//                              (VR5/6/7) when --channel normal is set.
//
// Mode 2 - full chain via preset file (--process-file input output --chain-config preset.json):
//   Loads a preset JSON and runs the complete OpenRiffBoxProcessor chain.
//   Legacy amp flags must not be combined with --chain-config.
//   Prints applied config between ===CHAIN-CONFIG-BEGIN=== and ===CHAIN-CONFIG-END=== markers.
//   The preset's limiterEnabled drives the output lookahead limiter; false
//   renders the raw chain (no other limiting exists since the softLimit
//   removal, C1 2026-07-03).
//
//   --gold-diag k=v[,k=v...]    Gold diagnostic overrides (see --help output
//                               for keys), applied after the preset.
//   --silver-diag k=v[,k=v...]  Silver diagnostic overrides: osfactor
//                               (2/4/8/16/32, stock 16), osfir (1 = FIR
//                               equiripple resampler), cone1x (1 = legacy
//                               base-rate cone), conebypass.
//                               Pre-C1-fix engine: osfactor=4,cone1x=1.
//   --plat-diag k=v[,k=v...]    Platinum diagnostic overrides (valid in both
//                               modes): osfactor (2/4/8/16/32, stock 4),
//                               osfir (1 = FIR equiripple resampler),
//                               noiselevel (V1A noise amplitude, default 1e-7,
//                               1e-5 = pre-C2 stock), brightfix (1 = schematic
//                               GAIN1 network, default 0), brightcin (V1B
//                               Miller capacitance in pF, default 110),
//                               c59 (1 = LTP plate-to-plate 47p, 113.4 kHz
//                               differential corner, default 0), v4tail
//                               (1 = dynamic LTP tail, default; 0 = frozen
//                               pre-fix tail for A/B), mvcircuit (1 = VR12
//                               A-taper + C58 network, default; 0 = legacy
//                               flat master for A/B). Dynamic-tail solver
//                               diagnostics: v4outers (NR budget, default 12),
//                               v4rides (0 = freeze grid bias rides), v4nfb
//                               (NFB scale into grid A), v4dump (1-7: replace
//                               output with internal LTP signals x0.1, see
//                               AmpSimPlatinum.h Diag).
//
// Supports FLAC and WAV input/output (detected by file extension).
// Output format: 24-bit FLAC or 32-bit float WAV (raw renders can exceed
// full scale; an integer writer would clip them).

#if ORB_OFFLINE_TOOLS

#include <juce_core/juce_core.h>

// Returns true if --process-file was found and handled (caller should quit).
bool runOfflineProcessor(const juce::StringArray& args);

#endif
