#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Custom cabinet IR loading, shared by the three amp engines. JUCE's own
// Trim::yes cuts at an absolute -80 dBFS, so where an IR starts depends on how
// loud it was exported: a hot REW export keeps its sweep harmonics as up to
// half a second of pre-delay, a quiet one loses its tail. Trimming here is
// relative to the IR's own peak instead.
namespace CabinetIR
{
    // First channel only (the cab convolution runs Stereo::no). Returns an
    // empty buffer if the file can't be read or is silent. Does file I/O.
    juce::AudioBuffer<float> loadTrimmed(const juce::File& file, double& sampleRate);

    // Hands the trimmed IR to the convolution. False, with the convolution
    // left as it was, if the file can't be used.
    bool load(juce::dsp::Convolution& convolution, const juce::File& file);
}
