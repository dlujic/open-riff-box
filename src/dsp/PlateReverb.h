#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class PlateReverb : public EffectProcessor
{
public:
    PlateReverb();
    ~PlateReverb() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Plate Reverb"; }
    void resetToDefaults() override;

    void setDecay(float value);       // 0-1
    void setDamping(float value);     // 0-1
    void setPreDelay(float value);    // 0-1 (maps to 0-100ms)
    void setMix(float value);         // 0-1
    void setWidth(float value);       // 0-1
    void setPlateType(int type);      // 0/1/2 (Studio/Bright/Dark)

    float getDecay()     const { return decayParam; }
    float getDamping()   const { return dampingParam; }
    float getPreDelay()  const { return preDelayParam; }
    float getMix()       const { return mixParam; }
    float getWidth()     const { return widthParam; }
    int   getPlateType() const { return plateTypeParam.load(std::memory_order_acquire); }

private:
    struct DelayBuffer
    {
        std::vector<float> data;
        int writePos = 0;
        int bufSize = 0;    // power of 2
        int mask = 0;       // bufSize - 1

        void allocate(int maxSamples)
        {
            bufSize = 1;
            while (bufSize < maxSamples + 2) bufSize <<= 1;
            data.assign(static_cast<size_t>(bufSize), 0.0f);
            mask = bufSize - 1;
            writePos = 0;
        }

        void write(float x)
        {
            data[static_cast<size_t>(writePos)] = x;
            writePos = (writePos + 1) & mask;
        }

        float read(int delaySamples) const
        {
            return data[static_cast<size_t>((writePos - delaySamples + bufSize) & mask)];
        }

        float readInterp(float delaySamples) const
        {
            float readPos = static_cast<float>(writePos) - delaySamples
                          + static_cast<float>(bufSize);
            int pos0 = static_cast<int>(readPos);
            float frac = readPos - static_cast<float>(pos0);
            float s0 = data[static_cast<size_t>(pos0 & mask)];
            float s1 = data[static_cast<size_t>((pos0 + 1) & mask)];
            return s0 + frac * (s1 - s0);
        }

        void clear()
        {
            std::fill(data.begin(), data.end(), 0.0f);
            writePos = 0;
        }
    };

    static float processAllpass(DelayBuffer& buf, int delaySamples,
                                float x, float coeff)
    {
        float delayed = buf.read(delaySamples);
        float v = x - coeff * delayed;
        float y = coeff * v + delayed;
        buf.write(v);
        return y;
    }

    static float processModAllpass(DelayBuffer& buf, float delaySamples,
                                   float x, float coeff)
    {
        float delayed = buf.readInterp(delaySamples);
        float v = x - coeff * delayed;
        float y = coeff * v + delayed;
        buf.write(v);
        return y;
    }

    static int msToSamples(float ms, double sr)
    {
        return juce::jmax(1, static_cast<int>(ms * 0.001f * static_cast<float>(sr)));
    }

    DelayBuffer preDelayBuf;

    DelayBuffer inputAP[4];
    int inputAPSamples[4] = {};

    enum TankIndex
    {
        kModAP_L = 0, kD1, kAP_L, kD2,
        kModAP_R, kD3, kAP_R, kD4,
        kNumTankBufs
    };

    DelayBuffer tank[kNumTankBufs];
    int tankSamples[kNumTankBufs] = {};

    float dampState[2] = {};

    float bwState = 0.0f;
    float bwAlpha = 0.0f;   // computed in prepare()

    float leftTankOut = 0.0f;    // previous d2 read
    float rightTankOut = 0.0f;   // previous d4 read

    juce::dsp::IIR::Filter<float> outputHPF[2];   // 40 Hz
    juce::dsp::IIR::Filter<float> outputLPF[2];   // 12 kHz

    float lfoPhase = 0.0f;
    static constexpr float kLfoRate = 1.0f;            // Hz
    static constexpr float kLfoExcursion_ms = 0.54f;   // +/- ms

    float decayParam    = 0.5f;
    float dampingParam  = 0.3f;
    float preDelayParam = 0.0f;
    float mixParam      = 0.3f;
    float widthParam    = 1.0f;
    std::atomic<int> plateTypeParam { 0 };  // 0=Studio, 1=Bright, 2=Dark

    juce::SmoothedValue<float> decaySmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> widthSmoothed;

    static constexpr float kInputAP_ms[4] = { 4.77f, 3.60f, 12.73f, 9.31f };

    static constexpr float kTankBase_ms[kNumTankBufs] = {
        22.58f, 149.63f, 60.48f, 125.00f,
        30.51f, 141.70f, 89.24f, 106.28f
    };

    static constexpr float kInputDiff1 = 0.750f;   // AP1, AP2
    static constexpr float kInputDiff2 = 0.625f;   // AP3, AP4
    static constexpr float kDecayDiff1 = 0.700f;   // tank modulated allpasses
    static constexpr float kDecayDiff2 = 0.500f;   // tank fixed allpasses

    struct OutputTap
    {
        int   bufIndex;    // TankIndex
        float offset_ms;
        float sign;        // +1 or -1
    };

    static constexpr int kNumTaps = 7;

    static constexpr OutputTap kLeftTaps[kNumTaps] = {
        { kD3,   8.94f,   1.0f },
        { kD3,   99.93f,  1.0f },
        { kAP_R, 64.28f, -1.0f },
        { kD4,   67.07f,  1.0f },
        { kD1,   66.87f, -1.0f },
        { kAP_L, 6.28f,  -1.0f },
        { kD2,   35.82f, -1.0f }
    };

    static constexpr OutputTap kRightTaps[kNumTaps] = {
        { kD1,   11.86f,   1.0f },
        { kD1,   121.88f,  1.0f },
        { kAP_L, 41.26f,  -1.0f },
        { kD2,   89.81f,   1.0f },
        { kD3,   70.93f,  -1.0f },
        { kAP_R, 11.26f,  -1.0f },
        { kD4,   4.07f,   -1.0f }
    };

    int leftTapSamples[kNumTaps] = {};
    int rightTapSamples[kNumTaps] = {};

    struct PlatePreset
    {
        float delayScale;      // multiplier on tank delay times
        float dampCutoffBase;  // base damping LPF cutoff (Hz)
        float dampCutoffRange; // +/- range (Hz)
    };

    static constexpr PlatePreset kPlatePresets[3] = {
        { 1.00f, 8000.0f, 6000.0f },   // Studio (EMT 140): warm, classic
        { 0.85f, 10000.0f, 6000.0f },  // Bright (Ecoplate): airy, shimmery
        { 1.20f, 4500.0f, 3500.0f }    // Dark (heavy damp): thick, warm
    };

    int lastPlateType = -1;
    double currentSampleRate = 44100.0;

    void updateDelayTimes();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlateReverb)
};
