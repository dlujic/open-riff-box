#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Equalizer : public EffectProcessor
{
public:
    Equalizer();
    ~Equalizer() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "EQ"; }
    void resetToDefaults() override;

    void setBass(float value);       // -12 to +12 dB
    void setMidGain(float value);    // -12 to +12 dB
    void setMidFreq(float value);    // 0-1 (maps to 200-5000 Hz logarithmic)
    void setTreble(float value);     // -12 to +12 dB
    void setLevel(float value);      // -12 to +6 dB

    float getBass()    const { return bassParam; }
    float getMidGain() const { return midGainParam; }
    float getMidFreq() const { return midFreqParam; }
    float getTreble()  const { return trebleParam; }
    float getLevel()   const { return levelParam; }

private:
    juce::dsp::IIR::Filter<float> lowShelf[2];   // low shelf at 100 Hz
    juce::dsp::IIR::Filter<float> midPeak[2];    // peaking filter, sweepable 200-5000 Hz
    juce::dsp::IIR::Filter<float> highShelf[2];  // high shelf at 4000 Hz

    // Parameters
    float bassParam    = 0.0f;   // low shelf gain in dB (-12 to +12)
    float midGainParam = 0.0f;   // mid peak gain in dB (-12 to +12)
    float midFreqParam = 0.5f;   // mid frequency 0-1 (200-5000 Hz)
    float trebleParam  = 0.0f;   // high shelf gain in dB (-12 to +12)
    float levelParam   = 0.0f;   // output level in dB (-12 to +6)

    juce::SmoothedValue<float> levelSmoothed;

    double currentSampleRate = 44100.0;
    bool   filtersDirty      = true;

    void updateFilters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Equalizer)
};
