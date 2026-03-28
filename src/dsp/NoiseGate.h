#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class NoiseGate : public EffectProcessor
{
public:
    NoiseGate();
    ~NoiseGate() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Noise Gate"; }
    void resetToDefaults() override;

    void setThresholdDb(float dB);      // -80 to 0, default -40
    void setAttackSeconds(float s);     // 0.0001 to 0.05, default 0.001
    void setHoldSeconds(float s);       // 0 to 0.5, default 0.05
    void setReleaseSeconds(float s);    // 0.005 to 2.0, default 0.1
    void setRangeDb(float dB);          // -90 to 0, default -90

    float getThresholdDb()  const { return thresholdDb; }
    float getAttackSeconds()  const { return attackTime; }
    float getHoldSeconds()    const { return holdTime; }
    float getReleaseSeconds() const { return releaseTime; }
    float getRangeDb()        const { return rangeDb; }

private:
    enum class State { Closed, Attack, Hold, Release };

    float thresholdDb  = -40.0f;
    float attackTime   =  0.001f;
    float holdTime     =  0.05f;
    float releaseTime  =  0.1f;
    float rangeDb      = -90.0f;

    float openThresholdLinear  = 0.0f;
    float closeThresholdLinear = 0.0f;
    float rangeLinear          = 0.0f;

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    double sampleRate = 44100.0;

    float envelope    = 0.0f;   // current envelope follower output
    float currentGain = 0.0f;   // gain applied to audio (0 = closed, 1 = open)
    State gateState   = State::Closed;

    float attackIncrement  = 0.0f;
    float releaseIncrement = 0.0f;

    int holdSamplesRemaining = 0;
    int holdSamplesTotal     = 0;

    juce::dsp::IIR::Filter<float> sidechainHPF;

    void recalculateDerivedValues();
    void recalculateCoefficients();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGate)
};
