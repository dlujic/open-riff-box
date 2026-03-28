#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class MetalDistortion : public EffectProcessor
{
public:
    MetalDistortion();
    ~MetalDistortion() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Metal Distortion"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setDrive(float value);  // 0-1
    void setTone(float value);   // 0-1
    void setLevel(float value);  // 0-1

    float getDrive() const { return driveParam; }
    float getTone()  const { return toneParam; }
    float getLevel() const { return levelParam; }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRFilter preHighpass;    // 80 Hz HPF
    IIRFilter dcBlocker;      // 7 Hz HPF
    IIRFilter toneFilter;     // variable LPF

    juce::dsp::IIR::Filter<float> interstageLPF[2];   // 4800 Hz biquad
    juce::dsp::IIR::Filter<float> interstageHPF[2];   // 140 Hz biquad

    juce::dsp::IIR::Filter<float> postLPF1[2];   // 5600 Hz biquad
    juce::dsp::IIR::Filter<float> postLPF2[2];   // 4800 Hz biquad
    juce::dsp::IIR::Filter<float> postLPF3[2];   // 4100 Hz biquad
    juce::dsp::IIR::Filter<float> postLPF4[2];   // 3500 Hz biquad
    juce::dsp::IIR::Filter<float> postBPF[2];    // 720 Hz +2.5dB peak

    float driveParam = 0.5f;
    float toneParam  = 1.0f;
    float levelParam = 0.85f;

    juce::SmoothedValue<float> levelSmoothed;

    float metalEnv[2] = { 0.0f, 0.0f };
    float expandAttackCoeff  = 0.0f;
    float expandReleaseCoeff = 0.0f;

    double currentSampleRate = 44100.0;
    double oversampledRate   = 44100.0 * 4;

    void updateToneFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetalDistortion)
};
