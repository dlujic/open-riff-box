#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Phaser : public EffectProcessor
{
public:
    Phaser();
    ~Phaser() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Phaser"; }
    void resetToDefaults() override;

    void setRate(float value);       // 0-1
    void setDepth(float value);      // 0-1
    void setFeedback(float value);   // 0-1
    void setMix(float value);        // 0-1
    void setStages(int value);       // 0=Classic(4), 1=Rich(8), 2=Deep(12)

    float getRate()     const { return rateParam; }
    float getDepth()    const { return depthParam; }
    float getFeedback() const { return feedbackParam; }
    float getMix()      const { return mixParam; }
    int   getStages()   const { return stagesParam; }

    static constexpr int stageCountForPreset(int preset)
    {
        return (preset == 2) ? 12 : (preset == 1) ? 8 : 4;
    }

private:
    struct AllpassStage
    {
        float x1 = 0.0f;  // previous input
        float y1 = 0.0f;  // previous output

        float process(float input, float a)
        {
            float output = a * input + x1 - a * y1;
            x1 = input;
            y1 = output;
            return output;
        }

        void reset() { x1 = 0.0f; y1 = 0.0f; }
    };

    static constexpr int MAX_STAGES = 12;

    AllpassStage stages[2][MAX_STAGES];

    float lfoPhase = 0.0f;

    float feedbackState[2] = {};

    struct DCBlocker
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
        static constexpr float R = 0.995f;

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

    float rateParam     = 0.15f;
    float depthParam    = 0.5f;
    float feedbackParam = 0.3f;
    float mixParam      = 0.5f;
    int   stagesParam   = 0;     // 0=Classic(4), 1=Rich(8), 2=Deep(12)

    juce::SmoothedValue<float> rateSmoothed;
    juce::SmoothedValue<float> depthSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    double currentSampleRate = 44100.0;

    static constexpr float fcMin = 100.0f;
    static constexpr float fcMax = 4000.0f;

    float computeCoefficient(float fc) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Phaser)
};
