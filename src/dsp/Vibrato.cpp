#include "Vibrato.h"
#include <cmath>

Vibrato::Vibrato() = default;

void Vibrato::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    int minSize = static_cast<int>(std::ceil(0.01 * sampleRate)) + 1;
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
        toneFilter[ch].prepare(monoSpec);

        *bbdColorFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            sampleRate, 8000.0f, 0.707f);

        *toneFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
            sampleRate, 8000.0f);
    }

    rateSmoothed.reset(sampleRate, 0.02);
    depthSmoothed.reset(sampleRate, 0.02);
    toneSmoothed.reset(sampleRate, 0.02);

    float initRate = 0.5f + 9.5f * rateParam * rateParam;
    rateSmoothed.setCurrentAndTargetValue(initRate);
    float initDepth = 0.1f + 2.9f * depthParam;
    depthSmoothed.setCurrentAndTargetValue(initDepth);
    toneSmoothed.setCurrentAndTargetValue(toneParam);

    lfoPhase = 0.0f;
}

void Vibrato::reset()
{
    for (auto& ch : delayBuffer)
        std::fill(ch.begin(), ch.end(), 0.0f);
    writePos = 0;

    for (int ch = 0; ch < 2; ++ch)
    {
        bbdColorFilter[ch].reset();
        toneFilter[ch].reset();
    }

    rateSmoothed.reset(currentSampleRate, 0.02);
    depthSmoothed.reset(currentSampleRate, 0.02);
    toneSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
}

void Vibrato::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateToneFilter();

    float targetRate = 0.5f + 9.5f * rateParam * rateParam;    // quadratic, 0.5-10 Hz
    float targetDepthMs = 0.1f + 2.9f * depthParam;             // linear, 0.1-3.0 ms
    rateSmoothed.setTargetValue(targetRate);
    depthSmoothed.setTargetValue(targetDepthMs);
    toneSmoothed.setTargetValue(toneParam);

    const float baseDelayMs = 4.0f;
    const float sampleRateF = static_cast<float>(currentSampleRate);
    const float baseDelaySamples = baseDelayMs * 0.001f * sampleRateF;
    const int bufMask = delayBufferSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        float rate = rateSmoothed.getNextValue();
        float depthMs = depthSmoothed.getNextValue();

        float depthSamples = depthMs * 0.001f * sampleRateF;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float input = buffer.getWritePointer(ch)[i];

            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = input;

            float phase = (ch == 0) ? lfoPhase : std::fmod(lfoPhase + 0.25f, 1.0f);
            float sineValue = std::sin(phase * juce::MathConstants<float>::twoPi);

            float modulatedDelay = baseDelaySamples + depthSamples * sineValue;

            modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 2), modulatedDelay);

            float delayed = readDelay(ch, modulatedDelay);

            delayed = bbdColorFilter[ch].processSample(delayed);

            delayed = toneFilter[ch].processSample(delayed);

            buffer.getWritePointer(ch)[i] = delayed;
        }

        writePos = (writePos + 1) & bufMask;

        lfoPhase += rate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

float Vibrato::readDelay(int channel, float delaySamples) const
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

void Vibrato::resetToDefaults()
{
    setRate(0.5f);
    setDepth(0.3f);
    setTone(0.7f);
}

void Vibrato::setRate(float value)  { rateParam = juce::jlimit(0.0f, 1.0f, value); }
void Vibrato::setDepth(float value) { depthParam = juce::jlimit(0.0f, 1.0f, value); }
void Vibrato::setTone(float value)  { toneParam = juce::jlimit(0.0f, 1.0f, value); }

void Vibrato::updateToneFilter()
{
    float cutoff = 800.0f * std::pow(15.0f, toneParam);
    cutoff = juce::jlimit(100.0f, 20000.0f, cutoff);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        currentSampleRate, cutoff);

    for (int ch = 0; ch < 2; ++ch)
        *toneFilter[ch].coefficients = *coeffs;
}
