#include "CabinetIR.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <limits>

namespace
{
    // Picked on 65 IRs (factory, windowed and raw REW exports): -50 dBc / 10 ms
    // leaves every clean IR starting where JUCE starts it and puts raw REW
    // exports within ~4 ms of their peak.
    constexpr float  kKeepDb     = -80.0f;   // JUCE's trim level, made peak-relative
    constexpr float  kQuietDb    = -50.0f;
    constexpr double kGapSec     = 0.010;
    constexpr double kPreRollSec = 0.001;
}

juce::AudioBuffer<float> CabinetIR::loadTrimmed(const juce::File& file, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));

    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        return {};

    const int length = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> raw(1, length);
    reader->read(&raw, 0, length, 0, true, false);

    const float* x = raw.getReadPointer(0);
    int peakIndex = 0;
    for (int i = 1; i < length; ++i)
        if (std::abs(x[i]) > std::abs(x[peakIndex]))
            peakIndex = i;

    const float peak = std::abs(x[peakIndex]);
    if (peak <= 0.0f)
        return {};

    const float keep    = peak * juce::Decibels::decibelsToGain(kKeepDb);
    const float quiet   = peak * juce::Decibels::decibelsToGain(kQuietDb);
    const int   gap     = juce::jmax(1, juce::roundToInt(kGapSec * reader->sampleRate));
    const int   preRoll = juce::roundToInt(kPreRollSec * reader->sampleRate);

    // 10 ms all 50 dB down is silence; whatever sits before it (sweep
    // harmonics, interface junk) is not part of the impulse.
    int onset = 0;
    for (int i = peakIndex, run = 0; i >= 0; --i)
    {
        run = std::abs(x[i]) < quiet ? run + 1 : 0;
        if (run == gap)
        {
            onset = i + gap;
            break;
        }
    }

    int start = juce::jmax(0, onset - preRoll);
    while (start < peakIndex && std::abs(x[start]) < keep)
        ++start;

    int end = length;
    while (end > peakIndex + 1 && std::abs(x[end - 1]) < keep)
        --end;

    juce::AudioBuffer<float> trimmed(1, end - start);
    trimmed.copyFrom(0, 0, raw, 0, start, end - start);
    sampleRate = reader->sampleRate;
    return trimmed;
}

bool CabinetIR::load(juce::dsp::Convolution& convolution, const juce::File& file)
{
    double sampleRate = 0.0;
    auto ir = loadTrimmed(file, sampleRate);
    if (ir.getNumSamples() == 0)
        return false;

    convolution.loadImpulseResponse(std::move(ir), sampleRate,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::yes);
    return true;
}
