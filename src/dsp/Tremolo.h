#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Tremolo : public EffectProcessor
{
public:
    enum Mode { Photo = 0, Bias = 1, Harmonic = 2 };

    Tremolo();
    ~Tremolo() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Tremolo"; }
    void resetToDefaults() override;
    int  getLatencySamples() const override;

    void setRate(float value);    // 0-1
    void setDepth(float value);   // 0-1
    void setMode(int value);      // 0=Photo, 1=Bias, 2=Harmonic
    void setWidth(float value);   // 0-1, mono-safe (1 = 150 deg, not 180)
    void setOutput(float value);  // 0-1 mapped to -12..+12 dB make-up gain

    float getRate()   const { return rateParam; }
    float getDepth()  const { return depthParam; }
    int   getMode()   const { return modeParam; }
    float getWidth()  const { return widthParam; }
    float getOutput() const { return outputParam; }

private:
    float rateParam   = 0.4f;
    float depthParam  = 0.5f;
    int   modeParam   = Photo;
    float widthParam  = 0.0f;
    float outputParam = 0.5f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> rateSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         depthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         widthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         outputSmoothed;

    float lfoPhase = 0.0f;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    float ldrEnv[2] = { 0.0f, 0.0f };  // Photo: asymmetric one-pole envelope state
    float ldrAttackCoeff  = 0.0f;
    float ldrReleaseCoeff = 0.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;  // Bias: 4x
    struct DCBlocker
    {
        float x1 = 0.0f, y1 = 0.0f;
        float R = 0.9993f;  // recomputed in prepare() for ~5 Hz cutoff at any sample rate

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

    juce::dsp::LinkwitzRileyFilter<float> crossLPF[2];  // Harmonic: LR4 split @ 400 Hz
    juce::dsp::LinkwitzRileyFilter<float> crossHPF[2];

    void processPhoto    (juce::AudioBuffer<float>& buffer);
    void processBias     (juce::AudioBuffer<float>& buffer);
    void processHarmonic (juce::AudioBuffer<float>& buffer);

    static float rateParamToHz(float v) { return 0.3f + 11.7f * v * v; }

    static float outputParamToGain(float v)
    {
        float dB = -12.0f + 24.0f * v;
        return std::pow(10.0f, dB * (1.0f / 20.0f));
    }

    static float widthParamToPhaseOffset(float v) { return 0.4166667f * v; }  // 150/360

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Tremolo)
};
