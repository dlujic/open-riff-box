#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Flanger : public EffectProcessor
{
public:
    Flanger();
    ~Flanger() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Flanger"; }
    void resetToDefaults() override;

    void setRate(float value);       // 0-1
    void setDepth(float value);      // 0-1
    void setManual(float value);     // 0-1 (center delay offset)
    void setFeedback(float value);   // 0-1 (feedback intensity)
    void setFeedbackPositive(bool positive);  // feedback polarity
    void setEQ(float value);         // 0-1
    void setMix(float value);        // 0-1

    float getRate()     const { return rateParam; }
    float getDepth()    const { return depthParam; }
    float getManual()   const { return manualParam; }
    float getFeedback() const { return feedbackParam; }
    bool  getFeedbackPositive() const { return feedbackPositive; }
    float getEQ()       const { return eqParam; }
    float getMix()      const { return mixParam; }

private:
    std::vector<std::vector<float>> delayBuffer;
    int writePos = 0;
    int delayBufferSize = 0;

    float lfoPhase = 0.0f;

    juce::dsp::IIR::Filter<float> bbdColorFilter[2];  // fixed 2nd-order Butterworth LPF at ~8 kHz
    juce::dsp::IIR::Filter<float> eqFilter[2];        // 1st-order variable LPF (800-12000 Hz)

    float feedbackState[2] = {};

    struct DCBlocker
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
        static constexpr float R = 0.995f;  // ~30 Hz cutoff

        float process(float x)
        {
            float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }

        void reset() { x1 = 0.0f; y1 = 0.0f; }
    };
    DCBlocker dcBlocker[2];

    float rateParam     = 0.2f;
    float depthParam    = 0.35f;
    float manualParam   = 0.3f;
    float feedbackParam = 0.45f;
    bool  feedbackPositive = true;
    float eqParam       = 0.7f;
    float mixParam      = 0.5f;

    juce::SmoothedValue<float> rateSmoothed;
    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> manualSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> eqSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    double currentSampleRate = 44100.0;

    float readDelay(int channel, float delaySamples) const;

    void updateEQFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Flanger)
};
