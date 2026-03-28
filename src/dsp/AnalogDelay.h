#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class AnalogDelay : public EffectProcessor
{
public:
    AnalogDelay();
    ~AnalogDelay() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Delay"; }
    void resetToDefaults() override;

    void setTime(float value);       // 0-1
    void setIntensity(float value);  // 0-1
    void setEcho(float value);       // 0-1
    void setModDepth(float value);   // 0-1
    void setModRate(float value);    // 0-1
    void setTone(float value);       // 0-1

    float getTime()     const { return timeParam; }
    float getIntensity() const { return intensityParam; }
    float getEcho()     const { return echoParam; }
    float getModDepth() const { return modDepthParam; }
    float getModRate()  const { return modRateParam; }
    float getTone()     const { return toneParam; }

private:
    std::vector<std::vector<float>> delayBuffer;
    int writePos = 0;
    int delayBufferSize = 0;

    float feedbackSample[2] = {};

    juce::dsp::IIR::Filter<float> preBBDFilter[2];   // 2nd-order Butterworth LPF, clock-tracking
    juce::dsp::IIR::Filter<float> bbdLossFilter[2];   // 1st-order LPF ~7 kHz (fixed)
    juce::dsp::IIR::Filter<float> postBBDFilter[2];  // 2nd-order Butterworth LPF, clock-tracking
    juce::dsp::IIR::Filter<float> fbLPF[2];          // 1st-order, tone-controlled (1.5-8 kHz)
    juce::dsp::IIR::Filter<float> fbHPF[2];          // 1st-order HPF 100 Hz
    juce::dsp::IIR::Filter<float> dcBlocker[2];      // 1st-order HPF 5 Hz

    float timeParam     = 0.769f;  // maps to ~200ms via log: 20 * pow(20, 0.769) ≈ 200
    float intensityParam = 0.35f;
    float echoParam     = 0.5f;
    float modDepthParam = 0.3f;
    float modRateParam  = 0.3f;
    float toneParam     = 0.5f;

    juce::SmoothedValue<float> timeSmoothed;       // 50ms ramp
    juce::SmoothedValue<float> intensitySmoothed;  // 20ms ramp
    juce::SmoothedValue<float> echoSmoothed;       // 20ms ramp

    float wowPhase = 0.0f;       // Triangle LFO, 0.3-0.5 Hz
    float flutterPhase = 0.0f;   // Sine LFO, ~4 Hz
    float randomState = 0.0f;    // LP-filtered noise

    juce::uint32 randomSeed = 12345;

    double currentSampleRate = 44100.0;

    void updateClockTrackingFilters(float timeMs);
    void updateFeedbackLPF();

    float readDelay(int channel, float delaySamples) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalogDelay)
};
