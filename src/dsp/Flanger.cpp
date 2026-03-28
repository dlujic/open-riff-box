#include "Flanger.h"
#include <cmath>

Flanger::Flanger() = default;

void Flanger::prepare(double sampleRate, int samplesPerBlock)
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

        feedbackState[ch] = 0.0f;
        dcBlocker[ch].reset();
    }

    rateSmoothed.reset(sampleRate, 0.02);
    depthSmoothed.reset(sampleRate, 0.02);
    manualSmoothed.reset(sampleRate, 0.02);
    feedbackSmoothed.reset(sampleRate, 0.02);
    eqSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    float initRate = 0.05f + 9.95f * rateParam * rateParam;
    rateSmoothed.setCurrentAndTargetValue(initRate);
    float initDepthMs = 0.1f + 3.9f * depthParam;
    depthSmoothed.setCurrentAndTargetValue(initDepthMs);
    float initManualMs = 0.5f + 9.5f * manualParam;
    manualSmoothed.setCurrentAndTargetValue(initManualMs);
    feedbackSmoothed.setCurrentAndTargetValue(feedbackParam);
    eqSmoothed.setCurrentAndTargetValue(eqParam);
    mixSmoothed.setCurrentAndTargetValue(mixParam);

    lfoPhase = 0.0f;
}

void Flanger::reset()
{
    for (auto& ch : delayBuffer)
        std::fill(ch.begin(), ch.end(), 0.0f);
    writePos = 0;

    for (int ch = 0; ch < 2; ++ch)
    {
        bbdColorFilter[ch].reset();
        eqFilter[ch].reset();
        feedbackState[ch] = 0.0f;
        dcBlocker[ch].reset();
    }

    rateSmoothed.reset(currentSampleRate, 0.02);
    depthSmoothed.reset(currentSampleRate, 0.02);
    manualSmoothed.reset(currentSampleRate, 0.02);
    feedbackSmoothed.reset(currentSampleRate, 0.02);
    eqSmoothed.reset(currentSampleRate, 0.02);
    mixSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
}

void Flanger::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateEQFilter();

    float targetRate     = 0.05f + 9.95f * rateParam * rateParam;  // quadratic, 0.05-10 Hz
    float targetDepthMs  = 0.1f + 3.9f * depthParam;               // 0.1-4.0 ms
    float targetManualMs = 0.5f + 9.5f * manualParam;              // 0.5-10.0 ms
    rateSmoothed.setTargetValue(targetRate);
    depthSmoothed.setTargetValue(targetDepthMs);
    manualSmoothed.setTargetValue(targetManualMs);
    feedbackSmoothed.setTargetValue(feedbackParam);
    eqSmoothed.setTargetValue(eqParam);
    mixSmoothed.setTargetValue(mixParam);

    const float sampleRateF = static_cast<float>(currentSampleRate);
    const int bufMask = delayBufferSize - 1;

    const float feedbackSign = feedbackPositive ? 1.0f : -1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float rate     = rateSmoothed.getNextValue();
        float depthMs  = depthSmoothed.getNextValue();
        float manualMs = manualSmoothed.getNextValue();
        float fbParam  = feedbackSmoothed.getNextValue();
        float mix      = mixSmoothed.getNextValue();

        float depthSamples       = depthMs  * 0.001f * sampleRateF;
        float manualDelaySamples = manualMs * 0.001f * sampleRateF;

        float fbCoeff = 0.88f * fbParam * feedbackSign;

        float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float dry = buffer.getWritePointer(ch)[i];

            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] =
                dry + feedbackState[ch];

            float phase = (ch == 0) ? lfoPhase : std::fmod(lfoPhase + 0.25f, 1.0f);

            float triangleValue = 4.0f * std::abs(phase - 0.5f) - 1.0f;

            float modulatedDelay = manualDelaySamples + depthSamples * triangleValue;

            modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 2), modulatedDelay);

            float delayed = readDelay(ch, modulatedDelay);

            delayed = bbdColorFilter[ch].processSample(delayed);

            delayed = eqFilter[ch].processSample(delayed);

            float fb = dcBlocker[ch].process(delayed);
            fb = std::tanh(fb * 0.7f) * (1.0f / 0.7f);  // unity gain at low levels
            feedbackState[ch] = fb * fbCoeff;

            buffer.getWritePointer(ch)[i] = dry * dryGain + delayed * wetGain;
        }

        writePos = (writePos + 1) & bufMask;

        lfoPhase += rate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

float Flanger::readDelay(int channel, float delaySamples) const
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

void Flanger::resetToDefaults()
{
    setRate(0.2f);
    setDepth(0.35f);
    setManual(0.3f);
    setFeedback(0.45f);
    setFeedbackPositive(true);
    setEQ(0.7f);
    setMix(0.5f);
}

void Flanger::setRate(float value)     { rateParam     = juce::jlimit(0.0f, 1.0f, value); }
void Flanger::setDepth(float value)    { depthParam    = juce::jlimit(0.0f, 1.0f, value); }
void Flanger::setManual(float value)   { manualParam   = juce::jlimit(0.0f, 1.0f, value); }
void Flanger::setFeedback(float value) { feedbackParam = juce::jlimit(0.0f, 1.0f, value); }
void Flanger::setFeedbackPositive(bool positive) { feedbackPositive = positive; }
void Flanger::setEQ(float value)       { eqParam       = juce::jlimit(0.0f, 1.0f, value); }
void Flanger::setMix(float value)      { mixParam      = juce::jlimit(0.0f, 1.0f, value); }

void Flanger::updateEQFilter()
{
    float cutoff = 800.0f * std::pow(15.0f, eqParam);
    cutoff = juce::jlimit(100.0f, 20000.0f, cutoff);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        currentSampleRate, cutoff);

    for (int ch = 0; ch < 2; ++ch)
        *eqFilter[ch].coefficients = *coeffs;
}
