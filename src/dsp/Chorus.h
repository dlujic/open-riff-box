#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Chorus : public EffectProcessor
{
public:
    Chorus();
    ~Chorus() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Chorus"; }
    void resetToDefaults() override;

    void setRate(float value);       // 0-1
    void setDepth(float value);      // 0-1
    void setEQ(float value);         // 0-1
    void setELevel(float value);     // 0-1

    float getRate()   const { return rateParam; }
    float getDepth()  const { return depthParam; }
    float getEQ()     const { return eqParam; }
    float getELevel() const { return eLevelParam; }

private:
    std::vector<std::vector<float>> delayBuffer;
    int writePos = 0;
    int delayBufferSize = 0;

    float lfoPhase = 0.0f;

    juce::dsp::IIR::Filter<float> bbdColorFilter[2];  // fixed 2nd-order Butterworth LPF at ~8 kHz
    juce::dsp::IIR::Filter<float> eqFilter[2];        // 1st-order variable LPF (800-12000 Hz)

    float rateParam   = 0.3f;   // LFO rate
    float depthParam  = 0.4f;   // modulation depth
    float eqParam     = 0.7f;   // wet signal EQ cutoff
    float eLevelParam = 0.5f;   // wet/dry mix

    juce::SmoothedValue<float> rateSmoothed;
    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> eqSmoothed;
    juce::SmoothedValue<float> eLevelSmoothed;

    double currentSampleRate = 44100.0;

    float readDelay(int channel, float delaySamples) const;

    void updateEQFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Chorus)
};
