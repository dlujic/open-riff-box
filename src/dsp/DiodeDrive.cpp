#include "DiodeDrive.h"

#include <cmath>

//==============================================================================
namespace TS808
{
    static constexpr double R6      = 51000.0;
    static constexpr double P1      = 500000.0;
    static constexpr double C4      = 51.0e-12;

    static constexpr double R4      = 4700.0;
    static constexpr double C3      = 0.047e-6;
    static constexpr double inv_R4  = 1.0 / R4;

    static constexpr double I_s     = 5.0e-9;
    static constexpr double n_d     = 1.3;
    static constexpr double V_t     = 0.02585;
    static constexpr double inv_nVt = 1.0 / (n_d * V_t);
    static constexpr double two_Is  = 2.0 * I_s;
}

void TS808Coefficients::compute(double sampleRate, double driveFraction)
{
    const double dt = 1.0 / sampleRate;
    const double R_drive = TS808::R6 + driveFraction * TS808::P1;

    inv_R_drive     = 1.0 / R_drive;

    half_dt_over_C3 = dt / (2.0 * TS808::C3);
    alpha_g         = half_dt_over_C3 * TS808::inv_R4;
    alpha_g_denom   = 1.0 / (1.0 + alpha_g);

    alpha_C4        = 2.0 * TS808::C4 / dt;

    two_Is          = TS808::two_Is;
    inv_nVt         = TS808::inv_nVt;
    two_Is_inv_nVt  = TS808::two_Is * TS808::inv_nVt;
}

//==============================================================================
static inline float ts808ProcessSample(float v_in, TS808State& state,
                                        const TS808Coefficients& coeff)
{
    const double vin = static_cast<double>(v_in);

    // 1. Ground impedance
    const double v_C3_new = (state.v_C3 + coeff.alpha_g * vin
                            + coeff.half_dt_over_C3 * state.i_g_prev) * coeff.alpha_g_denom;
    const double i_g = (vin - v_C3_new) * TS808::inv_R4;

    // 2. C4 companion history
    const double hist_C4 = -coeff.alpha_C4 * state.w_prev - state.i_C4_prev;

    // 3. Newton-Raphson solve
    double w = state.w_prev;

    for (int iter = 0; iter < 8; ++iter)
    {
        const double scaled_w = juce::jlimit(-80.0, 80.0, w * coeff.inv_nVt);
        const double e  = std::exp(scaled_w);
        const double ei = 1.0 / e;
        const double sinh_w = (e - ei) * 0.5;
        const double cosh_w = (e + ei) * 0.5;

        const double f_w  = w * coeff.inv_R_drive
                          + coeff.two_Is * sinh_w
                          + coeff.alpha_C4 * w + hist_C4
                          - i_g;
        const double fp_w = coeff.inv_R_drive
                          + coeff.two_Is_inv_nVt * cosh_w
                          + coeff.alpha_C4;

        const double delta = f_w / fp_w;
        w -= delta;

        w = juce::jlimit(-0.5, 0.5, w);

        if (std::abs(delta) < 1.0e-6)
            break;
    }

    // 4. Update C4 state
    const double i_C4 = coeff.alpha_C4 * w + hist_C4;

    // 5. Update state
    state.v_C3     = v_C3_new;
    state.i_g_prev = i_g;
    state.w_prev   = w;
    state.i_C4_prev = i_C4;

    // 6. Output
    return static_cast<float>(vin + w);
}

//==============================================================================
DiodeDrive::DiodeDrive()
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
}

//==============================================================================
void DiodeDrive::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    oversampledRate = sampleRate * oversampling->getOversamplingFactor();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    *preHighpass.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
        sampleRate, 80.0f);
    preHighpass.prepare(spec);

    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
        sampleRate, 7.0f);
    dcBlocker.prepare(spec);

    updateToneFilter();
    toneFilter.prepare(spec);

    levelSmoothed.reset(sampleRate, 0.02);
    levelSmoothed.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));

    tsState[0] = {};
    tsState[1] = {};
    lastDrive = -1.0f;
    lastTone  = -1.0f;
    tsCoeffs.compute(oversampledRate, static_cast<double>(driveParam));
}

void DiodeDrive::reset()
{
    oversampling->reset();
    preHighpass.reset();
    dcBlocker.reset();
    toneFilter.reset();
    levelSmoothed.reset(currentSampleRate, 0.02);

    tsState[0] = {};
    tsState[1] = {};
    lastDrive = -1.0f;
    lastTone  = -1.0f;
}

//==============================================================================
int DiodeDrive::getLatencySamples() const
{
    return static_cast<int>(oversampling->getLatencyInSamples());
}

//==============================================================================
void DiodeDrive::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateToneFilter();

    levelSmoothed.setTargetValue(juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));

    // 1. Pre-HPF
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        preHighpass.process(context);
    }

    // 2. Input scaling
    constexpr float inputScale = 0.15f;
    for (int i = 0; i < numSamples; ++i)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= inputScale;

    // 3. Oversampled circuit model
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());

        if (driveParam != lastDrive)
        {
            lastDrive = driveParam;
            tsCoeffs.compute(oversampledRate, static_cast<double>(driveParam));
        }

        constexpr float outputScale = 2.5f;

        for (int ch = 0; ch < osNumChannels; ++ch)
        {
            auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

            for (int i = 0; i < osNumSamples; ++i)
                samples[i] = ts808ProcessSample(samples[i], tsState[ch], tsCoeffs) * outputScale;
        }

        oversampling->processSamplesDown(block);
    }

    // 4. DC blocker
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dcBlocker.process(context);
    }

    // 5. Tone
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        toneFilter.process(context);
    }

    // 6. Output level
    for (int i = 0; i < numSamples; ++i)
    {
        const float level = levelSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= level;
    }
}

//==============================================================================
void DiodeDrive::resetToDefaults()
{
    setDrive(0.5f);
    setTone(0.5f);
    setLevel(0.70f);
}

//==============================================================================
// Parameter setters
//==============================================================================
void DiodeDrive::setDrive(float value)
{
    driveParam = juce::jlimit(0.0f, 1.0f, value);
}

void DiodeDrive::setTone(float value)
{
    toneParam = juce::jlimit(0.0f, 1.0f, value);
}

void DiodeDrive::setLevel(float value)
{
    levelParam = juce::jlimit(0.0f, 1.0f, value);
}

//==============================================================================
// Private helpers
//==============================================================================
void DiodeDrive::updateToneFilter()
{
    if (toneParam == lastTone)
        return;
    lastTone = toneParam;

    constexpr double R_s    = 1000.0;
    constexpr double C_tone = 0.22e-6;
    const double omega_p    = 1.0 / (R_s * C_tone);

    const double c = omega_p / std::tan(omega_p / (2.0 * currentSampleRate));

    const double alpha = static_cast<double>(toneParam);
    const double oneMinusAlpha = 1.0 - alpha;

    const double normGain = 1.0 / std::sqrt(alpha * alpha
                                           + oneMinusAlpha * oneMinusAlpha);

    const double b0_raw =  alpha * c + oneMinusAlpha * omega_p;
    const double b1_raw = -alpha * c + oneMinusAlpha * omega_p;
    const double a0_raw =  c + omega_p;
    const double a1_raw = -c + omega_p;

    const double inv_a0 = normGain / a0_raw;

    auto* coeff = toneFilter.state->getRawCoefficients();
    coeff[0] = static_cast<float>(b0_raw * inv_a0);
    coeff[1] = static_cast<float>(b1_raw * inv_a0);
    coeff[2] = 1.0f;
    coeff[3] = static_cast<float>(a1_raw / a0_raw);
}
