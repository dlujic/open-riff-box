#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Wah : public EffectProcessor
{
public:
    enum class TaperMode { DeadZones = 0, Linear = 1 };

    Wah();
    ~Wah() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Wah"; }
    void resetToDefaults() override;
    int  getLatencySamples() const override;

    void setPosition(float value);    // 0-1, the sweep control (toe=0 bright, heel=1 dark)
    void setColoration(bool on);      // asymmetric waveshaper toggle (fork #2)
    void setTaperMode(int mode);      // 0 = DeadZones (default), 1 = Linear (raw/validation)

    float getPosition()   const { return positionParam; }
    bool  getColoration() const { return colorationOn; }
    int   getTaperMode()  const { return static_cast<int>(taperMode); }

private:
    float positionParam = 0.5f;       // default: mid-sweep (~750 Hz dead-zones), a parked wah
    bool  colorationOn  = false;      // gate validates with this OFF
    TaperMode taperMode = TaperMode::DeadZones;

    juce::SmoothedValue<float> positionSmoothed;
    double currentSampleRate = 44100.0;

    // per-channel SVF state; coefficients are shared (computed per sub-block)
    float s1[2] { 0.0f, 0.0f };
    float s2[2] { 0.0f, 0.0f };

    // coloration path (only touched when colorationOn)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    struct DCBlockerState { float x1 = 0.0f, y1 = 0.0f; };
    DCBlockerState colorDc[2];
    float colorDcR = 0.9993f;

    float positionToWiper(float position) const;
    static void lookupCurves(float p, float& fc, float& Q, float& gainDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Wah)
};
