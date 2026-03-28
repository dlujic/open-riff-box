#pragma once

// Offline file processor for DSP testing (debug builds only).
// Processes audio files through AmpSimGold/AmpSimPlatinum without launching the GUI.
//
// Usage: OpenRiffBox --process-file input.flac output.flac [options]
// Options:
//   --engine <gold|platinum> Engine selection (default: gold)
//   --gain <0-1>           Preamp gain (default: 1.0)
//   --bass <0-1>           Bass (default: 0.5)
//   --mid <0-1>            Mid (default: 0.6)
//   --treble <0-1>         Treble (default: 0.5)
//   --speaker-drive <0-1>  Speaker drive (default: 0.2)
//   --brightness <0-1>     Brightness (default: 0.6)
//   --mic-position <0-1>   Mic position (default: 0.5)
//   --cabinet <0-15>       Cabinet index (0-13=named, 14=none, 15=custom)
//   --boost                Enable preamp boost (Gold only)
//   --ov-level <0-1>       OV Level (Platinum only, default: 0.7)
//   --master <0-1>         Master volume (Platinum only, default: 0.3)
//   --gain-mode <0-1>      Gain mode 0=GAIN1, 1=GAIN2 (Platinum only)
//
// Supports FLAC and WAV input/output (detected by file extension).
// Output format: 24-bit FLAC or input-matching WAV.

#if JUCE_DEBUG

#include <juce_core/juce_core.h>

// Returns true if --process-file was found and handled (caller should quit).
bool runOfflineProcessor(const juce::StringArray& args);

#endif
