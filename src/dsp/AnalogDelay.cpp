#include "AnalogDelay.h"
#include <cmath>

AnalogDelay::AnalogDelay() = default;

void AnalogDelay::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    int minSize = static_cast<int>(std::ceil(0.4 * sampleRate)) + 1;
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

    feedbackSample[0] = 0.0f;
    feedbackSample[1] = 0.0f;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        preBBDFilter[ch].prepare(monoSpec);
        bbdLossFilter[ch].prepare(monoSpec);
        postBBDFilter[ch].prepare(monoSpec);
        fbLPF[ch].prepare(monoSpec);
        fbHPF[ch].prepare(monoSpec);
        dcBlocker[ch].prepare(monoSpec);

        *bbdLossFilter[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, 7000.0f);
        *fbHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, 100.0f);
        *dcBlocker[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, 5.0f);
    }

    float defaultTimeMs = 20.0f * std::pow(20.0f, timeParam);
    updateClockTrackingFilters(defaultTimeMs);
    updateFeedbackLPF();

    timeSmoothed.reset(sampleRate, 0.05);
    intensitySmoothed.reset(sampleRate, 0.02);
    echoSmoothed.reset(sampleRate, 0.02);

    float initTimeMs = 20.0f * std::pow(20.0f, timeParam);
    float initTimeSamples = initTimeMs * 0.001f * static_cast<float>(sampleRate);
    timeSmoothed.setCurrentAndTargetValue(initTimeSamples);
    intensitySmoothed.setCurrentAndTargetValue(intensityParam * 0.95f);
    echoSmoothed.setCurrentAndTargetValue(echoParam);

    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    randomState = 0.0f;
}

void AnalogDelay::reset()
{
    for (auto& ch : delayBuffer)
        std::fill(ch.begin(), ch.end(), 0.0f);
    writePos = 0;

    feedbackSample[0] = 0.0f;
    feedbackSample[1] = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        preBBDFilter[ch].reset();
        bbdLossFilter[ch].reset();
        postBBDFilter[ch].reset();
        fbLPF[ch].reset();
        fbHPF[ch].reset();
        dcBlocker[ch].reset();
    }

    timeSmoothed.reset(currentSampleRate, 0.05);
    intensitySmoothed.reset(currentSampleRate, 0.02);
    echoSmoothed.reset(currentSampleRate, 0.02);

    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    randomState = 0.0f;
}

void AnalogDelay::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float targetTimeMs = 20.0f * std::pow(20.0f, timeParam);
    updateClockTrackingFilters(targetTimeMs);
    updateFeedbackLPF();

    float targetTimeSamples = targetTimeMs * 0.001f * static_cast<float>(currentSampleRate);
    timeSmoothed.setTargetValue(targetTimeSamples);
    intensitySmoothed.setTargetValue(intensityParam * 0.95f);
    echoSmoothed.setTargetValue(echoParam);

    float wowRate = 0.1f + 1.9f * modRateParam;
    float flutterRate = 4.0f;

    const float sampleRateF = static_cast<float>(currentSampleRate);
    const int bufMask = delayBufferSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        float currentTimeSamples = timeSmoothed.getNextValue();
        float feedback = intensitySmoothed.getNextValue();
        float echoLevel = echoSmoothed.getNextValue();

        float currentTimeMs = currentTimeSamples / sampleRateF * 1000.0f;
        float depthScale = modDepthParam * (currentTimeMs / 400.0f);

        float wowVal = 2.0f * std::abs(2.0f * wowPhase - 1.0f) - 1.0f;
        float wowOffset = wowVal * (0.2f + 0.3f * depthScale) * sampleRateF * 0.001f;

        float flutterVal = std::sin(2.0f * juce::MathConstants<float>::pi * flutterPhase);
        float flutterOffset = flutterVal * (0.05f + 0.05f * depthScale) * sampleRateF * 0.001f;

        randomSeed = randomSeed * 1664525u + 1013904223u;
        float noise = (static_cast<float>(randomSeed) / 4294967296.0f) * 2.0f - 1.0f;
        float lpAlpha = 2.0f * juce::MathConstants<float>::pi * 0.1f / sampleRateF;
        randomState += lpAlpha * (noise - randomState);
        float randomOffset = randomState * (0.1f + 0.1f * depthScale) * sampleRateF * 0.001f;

        float totalModOffset = wowOffset + flutterOffset + randomOffset;
        float modulatedDelay = currentTimeSamples + totalModOffset;

        modulatedDelay = juce::jlimit(1.0f, static_cast<float>(delayBufferSize - 4), modulatedDelay);

        wowPhase += wowRate / sampleRateF;
        if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += flutterRate / sampleRateF;
        if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float dry = buffer.getWritePointer(ch)[i];

            float delayInput = dry + feedbackSample[ch];
            delayInput = preBBDFilter[ch].processSample(delayInput);
            delayBuffer[static_cast<size_t>(ch)][static_cast<size_t>(writePos)] = delayInput;

            float delayed = readDelay(ch, modulatedDelay);
            delayed = bbdLossFilter[ch].processSample(delayed);
            delayed = postBBDFilter[ch].processSample(delayed);

            float fb = delayed * feedback;
            fb = std::tanh(fb * 1.5f) / 1.5f;
            fb = fbLPF[ch].processSample(fb);
            fb = fbHPF[ch].processSample(fb);
            fb = dcBlocker[ch].processSample(fb);

            feedbackSample[ch] = fb;

            buffer.getWritePointer(ch)[i] = dry + delayed * echoLevel;
        }

        writePos = (writePos + 1) & bufMask;
    }
}

float AnalogDelay::readDelay(int channel, float delaySamples) const
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

void AnalogDelay::resetToDefaults()
{
    setTime(0.769f);
    setIntensity(0.35f);
    setEcho(0.35f);
    setModDepth(0.3f);
    setModRate(0.3f);
    setTone(0.5f);
}

void AnalogDelay::setTime(float value)      { timeParam = juce::jlimit(0.0f, 1.0f, value); }
void AnalogDelay::setIntensity(float value) { intensityParam = juce::jlimit(0.0f, 1.0f, value); }
void AnalogDelay::setEcho(float value)      { echoParam = juce::jlimit(0.0f, 1.0f, value); }
void AnalogDelay::setModDepth(float value)  { modDepthParam = juce::jlimit(0.0f, 1.0f, value); }
void AnalogDelay::setModRate(float value)   { modRateParam = juce::jlimit(0.0f, 1.0f, value); }
void AnalogDelay::setTone(float value)      { toneParam = juce::jlimit(0.0f, 1.0f, value); }

void AnalogDelay::updateClockTrackingFilters(float timeMs)
{
    float t = juce::jlimit(0.0f, 1.0f, (timeMs - 20.0f) / 380.0f);
    float cutoff = 10000.0f - 7000.0f * t;
    cutoff = juce::jlimit(100.0f, 20000.0f, cutoff);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, cutoff, 0.707f);

    for (int ch = 0; ch < 2; ++ch)
    {
        *preBBDFilter[ch].coefficients = *coeffs;
        *postBBDFilter[ch].coefficients = *coeffs;
    }
}

void AnalogDelay::updateFeedbackLPF()
{
    float cutoff = 1500.0f + 6500.0f * toneParam;
    cutoff = juce::jlimit(100.0f, 20000.0f, cutoff);

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(
        currentSampleRate, cutoff);

    for (int ch = 0; ch < 2; ++ch)
        *fbLPF[ch].coefficients = *coeffs;
}
