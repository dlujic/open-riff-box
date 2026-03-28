#include "Chorus.h"
#include <cmath>

Chorus::Chorus() = default;

void Chorus::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    int minSize = static_cast<int>(std::ceil(0.02 * sampleRate)) + 1;
    delayBufferSize = 1;
    while (delayBufferSize < minSize)
        delayBufferSize <<= 1;

    delayBuffer.resize(2);
    for (auto& ch : delayBuffer)
    {
        ch.resize(static_cast<size_t>(delayBufferSize), 0.0f);
        std::fill(ch.begin(), ch.end(), 0.0f);
    }
    writePos = 0;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        bbdColorFilter[ch].prepare(monoSpec);
        eqFilter[ch].prepare(monoSpec);

        *bbdColorFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, 8000.0f, 0.707f);
        *eqFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
            sampleRate, 8000.0f);
    }

    rateSmoothed.reset(sampleRate, 0.02);
    depthSmoothed.reset(sampleRate, 0.02);
    eqSmoothed.reset(sampleRate, 0.02);
    eLevelSmoothed.reset(sampleRate, 0.02);

    float initRate = 0.3f + 3.7f * rateParam * rateParam;
    rateSmoothed.setCurrentAndTargetValue(initRate);
    float initDepth = 0.5f + 4.5f * depthParam;
    depthSmoothed.setCurrentAndTargetValue(initDepth);
    eqSmoothed.setCurrentAndTargetValue(eqParam);
    eLevelSmoothed.setCurrentAndTargetValue(eLevelParam);

    lfoPhase = 0.0f;
}

void Chorus::reset()
{
    for (auto& ch : delayBuffer)
        std::fill(ch.begin(), ch.end(), 0.0f);
    writePos = 0;

    for (int ch = 0; ch < 2; ++ch)
    {
        bbdColorFilter[ch].reset();
        eqFilter[ch].reset();
    }

    rateSmoothed.reset(currentSampleRate, 0.02);
    depthSmoothed.reset(currentSampleRate, 0.02);
    eqSmoothed.reset(currentSampleRate, 0.02);
    eLevelSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
}

void Chorus::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateEQFilter();

    float targetRate = 0.3f + 3.7f * rateParam * rateParam;
    float targetDepthMs = 0.5f + 4.5f * depthParam;
    rateSmoothed.setTargetValue(targetRate);
    depthSmoothed.setTargetValue(targetDepthMs);
    eqSmoothed.setTargetValue(eqParam);
    eLevelSmoothed.setTargetValue(eLevelParam);

    const float baseDelayMs = 7.0f;
    const float sampleRateF = static_cast<float>(currentSampleRate);
    const float baseDelaySamples = baseDelayMs * 0.001f * sampleRateF;
    const int bufMask = delayBufferSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        float rate = rateSmoothed.getNextValue();
        float depthMs = depthSmoothed.getNextValue();
        float eLevel = eLevelSmoothed.getNextValue();

        float depthSamples = depthMs * 0.001f * sampleRateF;

        float wetGain = std::sin(eLevel * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(eLevel * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float dry = buffer.getWritePointer(ch)[i];

            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = dry;

            float phase = (ch == 0) ? lfoPhase : std::fmod(lfoPhase + 0.5f, 1.0f);
            float triangleValue = 4.0f * std::abs(phase - 0.5f) - 1.0f;
            float modulatedDelay = baseDelaySamples + depthSamples * triangleValue;
            modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 2), modulatedDelay);

            float delayed = readDelay(ch, modulatedDelay);
            delayed = bbdColorFilter[ch].processSample(delayed);
            delayed = eqFilter[ch].processSample(delayed);

            buffer.getWritePointer(ch)[i] = dry * dryGain + delayed * wetGain;
        }

        writePos = (writePos + 1) & bufMask;
        lfoPhase += rate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

float Chorus::readDelay(int channel, float delaySamples) const
{
    float readPosF = static_cast<float>(writePos) - delaySamples;
    if (readPosF < 0.0f)
        readPosF += static_cast<float>(delayBufferSize);

    int pos0 = static_cast<int>(readPosF);
    float frac = readPosF - static_cast<float>(pos0);

    const int mask = delayBufferSize - 1;
    const auto& buf = delayBuffer[static_cast<size_t>(channel)];

    float ym1 = buf[static_cast<size_t>((pos0 - 1) & mask)];
    float y0  = buf[static_cast<size_t>(pos0 & mask)];
    float y1  = buf[static_cast<size_t>((pos0 + 1) & mask)];
    float y2  = buf[static_cast<size_t>((pos0 + 2) & mask)];

    float c0 = y0;
    float c1 = 0.5f * (y1 - ym1);
    float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void Chorus::resetToDefaults()
{
    setRate(0.3f);
    setDepth(0.4f);
    setEQ(0.7f);
    setELevel(0.35f);
}

void Chorus::setRate(float value)   { rateParam = juce::jlimit(0.0f, 1.0f, value); }
void Chorus::setDepth(float value)  { depthParam = juce::jlimit(0.0f, 1.0f, value); }
void Chorus::setEQ(float value)     { eqParam = juce::jlimit(0.0f, 1.0f, value); }
void Chorus::setELevel(float value) { eLevelParam = juce::jlimit(0.0f, 1.0f, value); }

void Chorus::updateEQFilter()
{
    float cutoff = 800.0f * std::pow(15.0f, eqParam);
    cutoff = juce::jlimit(100.0f, 20000.0f, cutoff);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        currentSampleRate, cutoff);

    for (int ch = 0; ch < 2; ++ch)
        *eqFilter[ch].coefficients = *coeffs;
}
