#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class SpringReverb : public EffectProcessor
{
public:
    SpringReverb();
    ~SpringReverb() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Spring Reverb"; }
    void resetToDefaults() override;

    void setDwell(float value);      // 0-1
    void setDecay(float value);      // 0-1
    void setTone(float value);       // 0-1
    void setMix(float value);        // 0-1
    void setDrip(float value);       // 0-1
    void setSpringType(int type);    // 0/1/2 (Short/Medium/Long)

    float getDwell()      const { return dwellParam; }
    float getDecay()      const { return decayParam; }
    float getTone()       const { return toneParam; }
    float getMix()        const { return mixParam; }
    float getDrip()       const { return dripParam; }
    int   getSpringType() const { return springTypeParam.load(std::memory_order_acquire); }

private:
    static constexpr int kChirpSections = 120;

    struct StretchedAllpass
    {
        float v1 = 0.0f;
        float v2 = 0.0f;

        float process(float x, float a)
        {
            float v = x - a * v2;
            float y = a * v + v2;
            v2 = v1;
            v1 = v;
            return y;
        }

        void reset() { v1 = v2 = 0.0f; }
    };

    StretchedAllpass chirpChain[kChirpSections];

    std::vector<float> fdnBuffer[3];   // Circular buffers for delays A, B, C
    int fdnWritePos[3] = {};
    int fdnDelaySamples[3] = {};       // Current delay lengths in samples
    int fdnBufferSize = 0;             // Power-of-2 buffer size

    float dampState[3] = {};           // One-pole filter state

    juce::dsp::IIR::Filter<float> outputHPF[2];   // 80 Hz
    juce::dsp::IIR::Filter<float> outputLPF[2];   // 6 kHz fixed

    float dcBlockState = 0.0f;         // Mono DC blocker state

    float dwellParam = 0.5f;
    float decayParam = 0.5f;
    float toneParam  = 0.5f;
    float mixParam   = 0.3f;
    float dripParam  = 0.4f;
    std::atomic<int> springTypeParam { 1 };  // 0=Short, 1=Medium, 2=Long

    juce::SmoothedValue<float> dwellSmoothed;
    juce::SmoothedValue<float> decaySmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    float lfoPhase = 0.0f;
    static constexpr float kLfoRate = 1.0f;   // ~1 Hz
    static constexpr float kLfoDepthMs = 0.5f; // +-0.5ms

    struct SpringPreset
    {
        float delayA_ms;
        float delayB_ms;
        float delayC_ms;
        float dampLPFBase;   // Base cutoff for tone=0.5
        float dampLPFRange;  // How much tone sweeps
    };

    static constexpr SpringPreset kSpringPresets[3] = {
        { 23.1f, 34.3f, 52.2f,  5250.0f, 2750.0f },  // Short: bright, surf
        { 41.3f, 62.7f, 93.1f,  3250.0f, 1750.0f },  // Medium: Fender Twin (warmer, less regular spacing)
        { 57.7f, 85.7f, 130.4f, 2500.0f, 1500.0f }   // Long: cavernous
    };

    int lastSpringType = -1;
    double currentSampleRate = 44100.0;

    void updateFDNDelayTimes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpringReverb)
};
