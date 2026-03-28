#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Vibrato : public EffectProcessor
{
public:
    Vibrato();
    ~Vibrato() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Vibrato"; }
    void resetToDefaults() override;

    void setRate(float value);       // 0-1
    void setDepth(float value);      // 0-1
    void setTone(float value);       // 0-1

    float getRate()  const { return rateParam; }
    float getDepth() const { return depthParam; }
    float getTone()  const { return toneParam; }

private:
    std::vector<std::vector<float>> delayBuffer;
    int writePos = 0;
    int delayBufferSize = 0;

    float lfoPhase = 0.0f;

    juce::dsp::IIR::Filter<float> bbdColorFilter[2];  // fixed 2nd-order Butterworth LPF at ~8 kHz
    juce::dsp::IIR::Filter<float> toneFilter[2];      // 1st-order variable LPF (800-12000 Hz)

    float rateParam  = 0.5f;   // LFO rate
    float depthParam = 0.3f;   // modulation depth
    float toneParam  = 0.7f;   // wet signal tone cutoff

    juce::SmoothedValue<float> rateSmoothed;
    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> toneSmoothed;

    double currentSampleRate = 44100.0;

    float readDelay(int channel, float delaySamples) const;

    void updateToneFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Vibrato)
};
