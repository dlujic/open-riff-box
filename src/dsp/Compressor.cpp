#include "Compressor.h"
#include <cmath>
#include <algorithm>

Compressor::Compressor()
{
    recalcCoefficients();
    recalcAutoMakeup();
}

//==============================================================================
void Compressor::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate       = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels      = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        *sidechainHPF[ch].coefficients =
            *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 100.0f, 0.707f);
        sidechainHPF[ch].prepare(monoSpec);
    }

    // ~25 ms ramps for sustain drive, blend, and makeup trim.
    sustainDbSmoothed.reset(sampleRate, 0.025);
    blendSmoothed    .reset(sampleRate, 0.025);
    makeupDbSmoothed .reset(sampleRate, 0.025);

    recalcCoefficients();
    recalcAutoMakeup();

    // Initialise current values so there is no ramp from 0 on first block.
    sustainDbSmoothed.setCurrentAndTargetValue(sustainParam * sustainRangeDb);
    blendSmoothed    .setCurrentAndTargetValue(blendParam);
    makeupDbSmoothed .setCurrentAndTargetValue(makeupDbSmoothed.getTargetValue());

    reset();
}

void Compressor::reset()
{
    for (int ch = 0; ch < 2; ++ch)
        sidechainHPF[ch].reset();

    envY1    = 0.0f;
    envY     = 0.0f;
    peak2    = 0.0f;
    rms2     = 0.0f;
    envFast  = 0.0f;
    envSlow  = 0.0f;
    grMemory = 0.0f;

    currentGainReductionDb.store(0.0f, std::memory_order_relaxed);
}

//==============================================================================
void Compressor::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    const int mode         = modeParam;
    const auto& mt         = modeTables[mode];
    const float fs         = static_cast<float>(currentSampleRate);
    const float aCoeff     = attackCoeff;
    float       rCoeff     = releaseCoeff;

    // Opto per-block constants (tau_s varies per-sample, but the fixed coefficients
    // can be hoisted out of the loop).
    const float optoFastCoeff = (mode == 2) ? std::exp(-1.0f / (0.06f * fs)) : 0.0f;
    const float optoMemCoeff  = (mode == 2) ? std::exp(-1.0f / (2.0f  * fs)) : 0.0f;

    // Studio attack in seconds (same per block; only recalced when attackParam changes,
    // but keeping it local is cheap and avoids a stale-state bug on the first block).
    const float studioAttackSec = (mode == 0)
        ? mt.attackMinMs * 0.001f * std::pow(mt.attackMaxMs / mt.attackMinMs, attackParam)
        : 0.0f;

    sustainDbSmoothed.setTargetValue(sustainParam * sustainRangeDb);
    blendSmoothed    .setTargetValue(blendParam);

    float blockMinGr = 0.0f;   // meter: most negative GR this block

    for (int i = 0; i < numSamples; ++i)
    {
        float xL = sidechainHPF[0].processSample(buffer.getSample(0, i));
        float xR = (numChannels > 1) ? sidechainHPF[1].processSample(buffer.getSample(1, i))
                                     : xL;
        float xAbs = std::max(std::abs(xL), std::abs(xR));

        float sustainDb = sustainDbSmoothed.getNextValue();
        // Sustain drive applied as a linear gain before the log; avoids log(0) on silence.
        float xDriven   = xAbs * std::pow(10.0f, sustainDb * (1.0f / 20.0f));

        float xDb = (xDriven > 1e-7f) ? 20.0f * std::log10(xDriven) : -140.0f;

        if (mode == 0)
        {
            float x2 = xAbs * xAbs;
            // Smooth peak/ms estimators over 200 ms; c2 = peak2/rms2 is the
            // squared crest factor (Giannoulis 2013 eq. 6/7).
            peak2 = std::max(x2, crestCoeff * peak2 + (1.0f - crestCoeff) * x2);
            rms2  = crestCoeff * rms2 + (1.0f - crestCoeff) * x2;

            float c2 = (rms2 > 1e-20f) ? (peak2 / rms2) : 1.0f;
            // Eq. 8 with maxRelease 0.5 s (guitar-range choice); the -attackSec
            // term compensates the detector's tau_A + tau_R measured release.
            float relSec = juce::jlimit(releaseFloorSec, 0.5f,
                                        2.0f * 0.5f / c2 - studioAttackSec);
            rCoeff = std::exp(-1.0f / (relSec * fs));
        }

        float g = computeStaticGainDb(xDb);   // gain reduction in dB (<= 0)

        // Smooth decoupled detector (Giannoulis 2012 eq. 17) in the GR domain:
        // stage 1 captures deeper GR instantly and leaks back at the release
        // time, stage 2 is the unconditional attack one-pole. Measured release
        // is tau_A + tau_R, which the Studio auto-release formula compensates.
        if (mode == 2)
        {
            // Opto stage 1 is two parallel release leaks blended 50/50:
            // 60 ms fast recovery plus a 0.6-2.5 s memory tail (LA-2A T4).
            envFast = std::min(g, optoFastCoeff * envFast + (1.0f - optoFastCoeff) * g);

            // Deeper/longer GR pushes the slow tau toward 2.5 s (per-sample
            // exp because grMemory moves every sample).
            float tauSlow   = 0.6f + 1.9f * juce::jlimit(0.0f, 1.0f, grMemory / 20.0f);
            float slowCoeff = std::exp(-1.0f / (tauSlow * fs));
            envSlow = std::min(g, slowCoeff * envSlow + (1.0f - slowCoeff) * g);

            // 2 s one-pole integrator of the applied GR depth.
            grMemory = optoMemCoeff * grMemory + (1.0f - optoMemCoeff) * (-envY);

            envY1 = 0.5f * envFast + 0.5f * envSlow;
        }
        else
        {
            envY1 = std::min(g, rCoeff * envY1 + (1.0f - rCoeff) * g);
        }

        envY = aCoeff * envY + (1.0f - aCoeff) * envY1;

        float makeupDb = makeupDbSmoothed.getNextValue();
        float gain     = std::pow(10.0f, (envY + makeupDb) * (1.0f / 20.0f));

        if (envY < blockMinGr)
            blockMinGr = envY;

        float blend = blendSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float dry = buffer.getSample(ch, i);
            // LINEAR crossfade: dry and wet are phase-coherent at zero latency
            // (same signal, one copy gain-modulated). Constant-amplitude is correct
            // here; equal-power is for decorrelated paths (reverbs, modulated delays).
            float wet = dry * gain;
            buffer.setSample(ch, i, (1.0f - blend) * dry + blend * wet);
        }
    }

    currentGainReductionDb.store(blockMinGr, std::memory_order_relaxed);
}

//==============================================================================
void Compressor::resetToDefaults()
{
    setSustain(0.35f);
    setAttack (0.50f);
    setBlend  (1.00f);
    setLevel  (0.50f);
    setMode   (0);
}

//==============================================================================
void Compressor::setSustain(float v)
{
    sustainParam = juce::jlimit(0.0f, 1.0f, v);
    recalcAutoMakeup();
}

void Compressor::setAttack(float v)
{
    attackParam = juce::jlimit(0.0f, 1.0f, v);
    recalcCoefficients();
}

void Compressor::setBlend(float v)
{
    blendParam = juce::jlimit(0.0f, 1.0f, v);
}

void Compressor::setLevel(float v)
{
    levelParam = juce::jlimit(0.0f, 1.0f, v);
    recalcAutoMakeup();
}

void Compressor::setMode(int v)
{
    modeParam = juce::jlimit(0, 2, v);
    recalcCoefficients();
    recalcAutoMakeup();
    // Reset envelope state on mode switch; character step is expected behaviour.
    envY1    = 0.0f;
    envY     = 0.0f;
    envFast  = 0.0f;
    envSlow  = 0.0f;
    grMemory = 0.0f;
    peak2    = 0.0f;
    rms2     = 0.0f;
}

//==============================================================================
void Compressor::recalcCoefficients()
{
    if (currentSampleRate <= 0.0)
        return;

    const float fs  = static_cast<float>(currentSampleRate);
    const auto& mt  = modeTables[modeParam];

    // Attack: log taper per mode (Giannoulis 1/e convention, matching NoiseGate).
    float attackMs  = mt.attackMinMs * std::pow(mt.attackMaxMs / mt.attackMinMs, attackParam);
    attackCoeff     = std::exp(-1.0f / ((attackMs * 0.001f) * fs));

    // Release: mode-dependent.
    if (modeParam == 1)
    {
        // Squeeze: fixed 350 ms.
        releaseCoeff = std::exp(-1.0f / (0.35f * fs));
    }
    else if (modeParam == 2)
    {
        // Opto: two-stage; releaseCoeff not used directly (computed per-sample).
        // Set a sane fallback so the variable is never uninitialised.
        releaseCoeff = std::exp(-1.0f / (0.6f * fs));
    }
    else
    {
        // Studio: release is recomputed per-block from crest factor; store floor.
        releaseCoeff = std::exp(-1.0f / (releaseFloorSec * fs));
    }

    // Crest-factor integrator: 200 ms window (Giannoulis 2013 eq. 7).
    crestCoeff = std::exp(-1.0f / (0.2f * fs));
}

void Compressor::recalcAutoMakeup()
{
    // Half-compensation: M_auto = -0.5 * staticGainDb(nominalPeakDb).
    // Traced to Giannoulis 2013 c_Est = T(1-1/R)/2.
    float sustainDb     = sustainParam * sustainRangeDb;
    float drivenNominal = nominalPeakDb + sustainDb;
    float staticGr      = computeStaticGainDb(drivenNominal);
    float mAuto         = -0.5f * staticGr;

    // Level param: 0-1 maps to -12..+12 dB trim around auto makeup.
    float levelTrimDb = -12.0f + 24.0f * levelParam;

    makeupDbSmoothed.setTargetValue(mAuto + levelTrimDb);
}

float Compressor::computeStaticGainDb(float xDb) const
{
    const auto& mt = modeTables[modeParam];
    const float T  = internalThresholdDb;
    const float R  = mt.ratio;
    const float W  = mt.kneeDb;

    // Giannoulis 2012 eq. 4: quadratic soft knee between the hard-knee segments.
    float diff = xDb - T;

    if (diff < -W * 0.5f)
        return 0.0f;

    if (diff <= W * 0.5f)
    {
        float knee = diff + W * 0.5f;
        return (1.0f / R - 1.0f) * (knee * knee) / (2.0f * W);
    }

    return T + diff / R - xDb;
}
