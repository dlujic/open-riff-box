#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

struct TS808State
{
    double v_C3     = 0.0;    // Voltage across C3 in ground impedance (Zi)
    double i_g_prev = 0.0;    // Previous ground current through C3+R4
    double w_prev   = 0.0;    // Previous feedback voltage (NR initial guess)
    double i_C4_prev = 0.0;   // Previous C4 (51pF) current
};

struct TS808Coefficients
{
    double inv_R_drive    = 0.0;   // 1 / (R6 + P1*drive)
    double half_dt_over_C3 = 0.0;  // T / (2*C3)
    double alpha_g        = 0.0;   // T / (2*C3*R4), trapezoidal coeff for Zi
    double alpha_g_denom  = 0.0;   // 1 / (1 + alpha_g)
    double alpha_C4       = 0.0;   // 2*C4/T, trapezoidal companion coeff for 51pF
    double two_Is         = 0.0;   // 2 * I_s (diode pair)
    double inv_nVt        = 0.0;   // 1 / (n * V_t)
    double two_Is_inv_nVt = 0.0;   // 2 * I_s / (n * V_t), Jacobian constant

    void compute(double sampleRate, double driveFraction);
};

class DiodeDrive : public EffectProcessor
{
public:
    DiodeDrive();
    ~DiodeDrive() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Diode Drive"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setDrive(float value);   // 0-1
    void setTone(float value);    // 0-1
    void setLevel(float value);   // 0-1

    float getDrive() const { return driveParam; }
    float getTone()  const { return toneParam; }
    float getLevel() const { return levelParam; }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRFilter preHighpass;   // 80 Hz HPF before clipping
    IIRFilter dcBlocker;     // 7 Hz HPF after clipping
    IIRFilter toneFilter;    // 700-4500 Hz variable LPF

    float driveParam = 0.5f;
    float toneParam  = 0.5f;
    float levelParam = 0.7f;

    juce::SmoothedValue<float> levelSmoothed;

    TS808State tsState[2];
    TS808Coefficients tsCoeffs;
    float lastDrive = -1.0f;
    float lastTone  = -1.0f;

    double currentSampleRate = 44100.0;
    double oversampledRate   = 44100.0 * 4;

    void updateToneFilter();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiodeDrive)
};
