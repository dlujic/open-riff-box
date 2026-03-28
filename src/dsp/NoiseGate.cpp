#include "NoiseGate.h"

#include <cmath>

//==============================================================================
NoiseGate::NoiseGate()
{
    recalculateDerivedValues();
}

//==============================================================================
void NoiseGate::prepare(double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 1;

    *sidechainHPF.coefficients =
        *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 150.0f);

    sidechainHPF.prepare(spec);

    recalculateDerivedValues();
    recalculateCoefficients();

    reset();
}

void NoiseGate::reset()
{
    sidechainHPF.reset();

    envelope             = 0.0f;
    currentGain          = 0.0f;
    gateState            = State::Closed;
    holdSamplesRemaining = 0;
}

//==============================================================================
void NoiseGate::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    const float* detectionSrc = buffer.getReadPointer(0);

    for (int i = 0; i < numSamples; ++i)
    {
        //----------------------------------------------------------------------
        // 1. Grab detection sample and apply sidechain HPF.
        //----------------------------------------------------------------------
        float detectionSample = std::abs(sidechainHPF.processSample(detectionSrc[i]));

        //----------------------------------------------------------------------
        // 2. Envelope follower — peak detection with attack/release smoothing.
        //----------------------------------------------------------------------
        if (detectionSample > envelope)
            envelope = attackCoeff  * envelope + (1.0f - attackCoeff)  * detectionSample;
        else
            envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * detectionSample;

        //----------------------------------------------------------------------
        // 3. State machine.
        //----------------------------------------------------------------------
        switch (gateState)
        {
            case State::Closed:
                if (envelope >= openThresholdLinear)
                {
                    gateState = State::Attack;
                }
                break;

            case State::Attack:
                currentGain += attackIncrement;
                if (currentGain >= 1.0f)
                {
                    currentGain          = 1.0f;
                    gateState            = State::Hold;
                    holdSamplesRemaining = holdSamplesTotal;
                }
                // If signal drops below close threshold during attack, go back.
                else if (envelope < closeThresholdLinear)
                {
                    gateState = State::Release;
                }
                break;

            case State::Hold:
                if (envelope < closeThresholdLinear)
                {
                    if (holdSamplesRemaining > 0)
                    {
                        --holdSamplesRemaining;
                    }
                    else
                    {
                        gateState = State::Release;
                    }
                }
                else
                {
                    // Signal is still above threshold — keep refreshing the hold.
                    holdSamplesRemaining = holdSamplesTotal;
                }
                break;

            case State::Release:
                currentGain -= releaseIncrement;
                if (currentGain <= rangeLinear)
                {
                    currentGain = rangeLinear;
                    gateState   = State::Closed;
                }
                // Signal came back up — re-open immediately.
                if (envelope >= openThresholdLinear)
                {
                    gateState = State::Attack;
                }
                break;
        }

        //----------------------------------------------------------------------
        // 4. Apply currentGain to every channel.
        //----------------------------------------------------------------------
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= currentGain;
    }
}

//==============================================================================
void NoiseGate::resetToDefaults()
{
    setThresholdDb(-40.0f);
    setAttackSeconds(0.001f);
    setHoldSeconds(0.10f);
    setReleaseSeconds(0.20f);
    setRangeDb(-90.0f);
}

//==============================================================================
// Parameter setters
//==============================================================================
void NoiseGate::setThresholdDb(float dB)
{
    thresholdDb = juce::jlimit(-80.0f, 0.0f, dB);
    recalculateDerivedValues();
}

void NoiseGate::setAttackSeconds(float s)
{
    attackTime = juce::jlimit(0.0001f, 0.016f, s);
    recalculateCoefficients();
}

void NoiseGate::setHoldSeconds(float s)
{
    holdTime = juce::jlimit(0.0f, 0.5f, s);
    recalculateCoefficients();
}

void NoiseGate::setReleaseSeconds(float s)
{
    releaseTime = juce::jlimit(0.005f, 0.6f, s);
    recalculateCoefficients();
}

void NoiseGate::setRangeDb(float dB)
{
    rangeDb = juce::jlimit(-90.0f, 0.0f, dB);
    recalculateDerivedValues();
    recalculateCoefficients();  // releaseIncrement depends on rangeLinear
}

//==============================================================================
// Private helpers
//==============================================================================
void NoiseGate::recalculateDerivedValues()
{
    // Open threshold: user-set value.
    openThresholdLinear = std::pow(10.0f, thresholdDb / 20.0f);

    // Close threshold: 6 dB below open threshold (hysteresis).
    closeThresholdLinear = std::pow(10.0f, (thresholdDb - 6.0f) / 20.0f);

    // Range: attenuation floor when gate is fully closed.
    rangeLinear = std::pow(10.0f, rangeDb / 20.0f);
}

void NoiseGate::recalculateCoefficients()
{
    if (sampleRate <= 0.0)
        return;

    // One-pole lowpass coefficients for envelope follower.
    attackCoeff  = static_cast<float>(std::exp(-1.0 / (attackTime  * sampleRate)));
    releaseCoeff = static_cast<float>(std::exp(-1.0 / (releaseTime * sampleRate)));

    attackIncrement  = (attackTime  * static_cast<float>(sampleRate)) > 0.0f
                       ? 1.0f / (attackTime  * static_cast<float>(sampleRate))
                       : 1.0f;

    releaseIncrement = (releaseTime * static_cast<float>(sampleRate)) > 0.0f
                       ? (1.0f - rangeLinear) / (releaseTime * static_cast<float>(sampleRate))
                       : 1.0f;

    // Hold counter in samples.
    holdSamplesTotal = static_cast<int>(holdTime * static_cast<float>(sampleRate));
}
