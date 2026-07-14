#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class Wah : public EffectProcessor
{
public:
    enum class TaperMode { DeadZones = 0, Linear = 1 };
    enum class Mode      { Manual = 0, Auto = 1, Envelope = 2 };   // control source
    enum class AutoWave  { Sine = 0, Triangle = 1 };               // Auto mode LFO shape

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

    void setMode(int value);          // Mode: Manual (default), Auto, Envelope
    void setAutoWave(int value);      // AutoWave: Sine (default) or Triangle
    void setAutoRate(float value);    // 0-1 -> 0.1-10 Hz, Auto mode LFO rate
    void setAutoDepth(float value);   // 0-1, Auto mode sweep excursion around position
    void setEnvSens(float value);     // 0-1, Envelope mode pick-attack sensitivity
    void setEnvAttack(float value);   // 0-1 -> 1-50 ms, Envelope mode
    void setEnvRelease(float value);  // 0-1 -> 20-500 ms, Envelope mode

    float getPosition()   const { return positionParam.load(std::memory_order_acquire); }
    bool  getColoration() const { return colorationOn.load(std::memory_order_acquire); }
    int   getTaperMode()  const { return taperMode.load(std::memory_order_acquire); }

    int   getMode()        const { return modeParam.load(std::memory_order_acquire); }
    int   getAutoWave()    const { return autoWaveParam.load(std::memory_order_acquire); }
    float getAutoRate()    const { return autoRateParam.load(std::memory_order_acquire); }
    float getAutoDepth()   const { return autoDepthParam.load(std::memory_order_acquire); }
    float getEnvSens()     const { return envSensParam.load(std::memory_order_acquire); }
    float getEnvAttack()   const { return envAttackParam.load(std::memory_order_acquire); }
    float getEnvRelease()  const { return envReleaseParam.load(std::memory_order_acquire); }

private:
    // Every parameter below is written from the message thread and read from the audio
    // thread, so all of them are atomic -- floats included (cf. PluginProcessor's
    // masterVolume). A plain float here is a data race, and the risk is not a torn read
    // on x86 but the optimiser being licensed to assume the write never happened.
    std::atomic<float> positionParam { 0.5f };  // default: mid-sweep (~750 Hz dead-zones), a parked wah
    std::atomic<bool> colorationOn { false };   // gate validates with this OFF
    std::atomic<int>  taperMode { 0 };          // TaperMode; int for the atomic
    bool prevColorationOn = false;    // audio-thread edge detect for state reset

    // Phase 3: control sources feeding the filter core below (positionToWiper down).
    std::atomic<int>   modeParam       { 0 };      // Mode; int for the atomic
    std::atomic<int>   autoWaveParam   { 0 };      // AutoWave; int for the atomic
    std::atomic<float> autoRateParam   { 0.45f };
    std::atomic<float> autoDepthParam  { 0.5f };
    std::atomic<float> envSensParam    { 0.5f };
    std::atomic<float> envAttackParam  { 0.43f };
    std::atomic<float> envReleaseParam { 0.52f };

    int   prevMode = 0;          // audio-thread edge detect, like prevColorationOn
    int   prevWave = 0;
    float lastCtrl = 0.5f;       // last control value actually used, for continuous handover
    float lfoPhase = 0.0f;
    float envelope = 0.0f;
    juce::dsp::IIR::Filter<float> envHpf[2];   // envelope sidechain, per channel

    juce::SmoothedValue<float> positionSmoothed;
    double currentSampleRate = 44100.0;

    // per-channel SVF state; coefficients are shared (computed per sub-block)
    float s1[2] { 0.0f, 0.0f };
    float s2[2] { 0.0f, 0.0f };

    // Coefficients only move once per control block, so they are ramped across it
    // rather than stepped -- scale is a direct output multiplier and a step is a click.
    struct SvfCoeffs { float g = 0.0f, aHP = 0.0f, coefFB = 0.0f, scale = 0.0f; };
    SvfCoeffs prevCoeffs;             // ramp origin; seeded in prepare()/reset()

    // coloration path (only touched when colorationOn)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    struct DCBlockerState { float x1 = 0.0f, y1 = 0.0f; };
    DCBlockerState colorDc[2];
    float colorDcR = 0.9993f;

    float positionToWiper(float position) const;
    static void lookupCurves(float p, float& fc, float& Q, float& gainDb);
    SvfCoeffs coeffsFor(float ctrl) const;

    // Phase 3 control-source mappings: quadratic knob taper, same shape as Tremolo's
    // rateParamToHz (finer resolution where the ear cares, at the low end of each range).
    static float autoRateToHz    (float v) { return 0.1f + 9.9f * v * v; }             // 0.1 - 10 Hz
    static float envSensToGain   (float v) { return 16.0f * v * v; }                    // exactly 0 at v = 0
    static float envAttackToSec  (float v) { return (1.0f  + 49.0f  * v * v) * 0.001f; } // 1 - 50 ms
    static float envReleaseToSec (float v) { return (20.0f + 480.0f * v * v) * 0.001f; } // 20 - 500 ms

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Wah)
};
