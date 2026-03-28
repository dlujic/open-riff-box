#include "SpringReverb.h"
#include <cmath>

SpringReverb::SpringReverb() = default;

void SpringReverb::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    int minSize = static_cast<int>(std::ceil(0.15 * sampleRate)) + 1;
    fdnBufferSize = 1;
    while (fdnBufferSize < minSize)
        fdnBufferSize <<= 1;

    for (int i = 0; i < 3; ++i)
    {
        fdnBuffer[i].resize(static_cast<size_t>(fdnBufferSize), 0.0f);
        std::fill(fdnBuffer[i].begin(), fdnBuffer[i].end(), 0.0f);
        fdnWritePos[i] = 0;
        dampState[i] = 0.0f;
    }

    lastSpringType = -1;
    updateFDNDelayTimes();

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        outputHPF[ch].prepare(monoSpec);
        outputLPF[ch].prepare(monoSpec);

        *outputHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, 80.0f);
        *outputLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 6000.0f, 0.707f);
    }

    dcBlockState = 0.0f;

    dwellSmoothed.reset(sampleRate, 0.02);
    decaySmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    dwellSmoothed.setCurrentAndTargetValue(0.5f + 2.5f * dwellParam * dwellParam);
    decaySmoothed.setCurrentAndTargetValue(0.3f + 0.65f * decayParam);
    mixSmoothed.setCurrentAndTargetValue(mixParam);

    for (auto& ap : chirpChain)
        ap.reset();

    lfoPhase = 0.0f;
}

void SpringReverb::reset()
{
    for (int i = 0; i < 3; ++i)
    {
        std::fill(fdnBuffer[i].begin(), fdnBuffer[i].end(), 0.0f);
        fdnWritePos[i] = 0;
        dampState[i] = 0.0f;
    }

    for (auto& ap : chirpChain)
        ap.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        outputHPF[ch].reset();
        outputLPF[ch].reset();
    }

    dcBlockState = 0.0f;

    dwellSmoothed.reset(currentSampleRate, 0.02);
    decaySmoothed.reset(currentSampleRate, 0.02);
    mixSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
    lastSpringType = -1;
}

void SpringReverb::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateFDNDelayTimes();

    dwellSmoothed.setTargetValue(0.5f + 2.5f * dwellParam * dwellParam);
    decaySmoothed.setTargetValue(0.3f + 0.65f * decayParam);
    mixSmoothed.setTargetValue(mixParam);

    float dripCoeff = 0.05f + 0.65f * dripParam;  // 0.05-0.7

    int currentType = springTypeParam.load(std::memory_order_acquire);
    currentType = juce::jlimit(0, 2, currentType);
    const auto& preset = kSpringPresets[currentType];
    float dampCutoff = (preset.dampLPFBase - preset.dampLPFRange) + 2.0f * preset.dampLPFRange * toneParam;
    dampCutoff = juce::jlimit(100.0f, 20000.0f, dampCutoff);
    float dampAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * dampCutoff / static_cast<float>(currentSampleRate));

    const float sampleRateF = static_cast<float>(currentSampleRate);
    const int bufMask = fdnBufferSize - 1;

    constexpr float h_diag = 1.0f / 3.0f;
    constexpr float h_off  = -2.0f / 3.0f;

    float lfoDepthSamples = kLfoDepthMs * 0.001f * sampleRateF;

    float dcLpAlpha = 2.0f * juce::MathConstants<float>::pi * 5.0f / sampleRateF;

    for (int i = 0; i < numSamples; ++i)
    {
        float dwell = dwellSmoothed.getNextValue();
        float decay = decaySmoothed.getNextValue();
        float mix   = mixSmoothed.getNextValue();

        float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        float monoIn = 0.0f;
        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
            monoIn += buffer.getReadPointer(ch)[i];
        monoIn /= static_cast<float>(juce::jmin(numChannels, 2));

        float dwellIn = monoIn * dwell;

        float satDrive = 1.0f + 1.5f * dwellParam;
        dwellIn = std::tanh(dwellIn * satDrive) / satDrive;

        float chirped = dwellIn;
        for (int s = 0; s < kChirpSections; ++s)
            chirped = chirpChain[s].process(chirped, dripCoeff);
		
        dcBlockState += dcLpAlpha * (chirped - dcBlockState);
        chirped = chirped - dcBlockState;

        float lfoVal = std::sin(2.0f * juce::MathConstants<float>::pi * lfoPhase);
        float lfoOffset = lfoVal * lfoDepthSamples;
        lfoPhase += kLfoRate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        float delayOut[3];
        for (int d = 0; d < 3; ++d)
        {
            float delaySamp = static_cast<float>(fdnDelaySamples[d]);
            if (d == 1) delaySamp += lfoOffset;  // Modulate delay B

            delaySamp = juce::jlimit(1.0f, static_cast<float>(fdnBufferSize - 2), delaySamp);

            float readPosF = static_cast<float>(fdnWritePos[d]) - delaySamp;
            if (readPosF < 0.0f) readPosF += static_cast<float>(fdnBufferSize);

            int pos0 = static_cast<int>(readPosF);
            float frac = readPosF - static_cast<float>(pos0);

            float s0 = fdnBuffer[d][static_cast<size_t>(pos0 & bufMask)];
            float s1 = fdnBuffer[d][static_cast<size_t>((pos0 + 1) & bufMask)];
            delayOut[d] = s0 + frac * (s1 - s0);
        }

        for (int d = 0; d < 3; ++d)
        {
            dampState[d] += dampAlpha * (delayOut[d] - dampState[d]);
            delayOut[d] = dampState[d];
        }

        float fb[3];
        fb[0] = (h_diag * delayOut[0] + h_off * delayOut[1] + h_off * delayOut[2]) * decay;
        fb[1] = (h_off * delayOut[0] + h_diag * delayOut[1] + h_off * delayOut[2]) * decay;
        fb[2] = (h_off * delayOut[0] + h_off * delayOut[1] + h_diag * delayOut[2]) * decay;

        fdnBuffer[0][static_cast<size_t>(fdnWritePos[0])] = chirped * 0.5f + fb[0];
        fdnBuffer[1][static_cast<size_t>(fdnWritePos[1])] = chirped * 0.5f + fb[1];
        fdnBuffer[2][static_cast<size_t>(fdnWritePos[2])] = fb[2];  // No direct input

        for (int d = 0; d < 3; ++d)
            fdnWritePos[d] = (fdnWritePos[d] + 1) & bufMask;

        float wet = delayOut[2];

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float dry = buffer.getReadPointer(ch)[i];

            float filtered = outputHPF[ch].processSample(wet);
            filtered = outputLPF[ch].processSample(filtered);

            buffer.getWritePointer(ch)[i] = dry * dryGain + filtered * wetGain;
        }
    }
}

void SpringReverb::resetToDefaults()
{
    setDwell(0.5f);
    setDecay(0.5f);
    setTone(0.5f);
    setMix(0.3f);
    setDrip(0.4f);
    setSpringType(1);
}

void SpringReverb::setDwell(float value)      { dwellParam = juce::jlimit(0.0f, 1.0f, value); }
void SpringReverb::setDecay(float value)      { decayParam = juce::jlimit(0.0f, 1.0f, value); }
void SpringReverb::setTone(float value)       { toneParam = juce::jlimit(0.0f, 1.0f, value); }
void SpringReverb::setMix(float value)        { mixParam = juce::jlimit(0.0f, 1.0f, value); }
void SpringReverb::setDrip(float value)       { dripParam = juce::jlimit(0.0f, 1.0f, value); }
void SpringReverb::setSpringType(int type)    { springTypeParam.store(juce::jlimit(0, 2, type), std::memory_order_release); }

void SpringReverb::updateFDNDelayTimes()
{
    int currentType = springTypeParam.load(std::memory_order_acquire);
    currentType = juce::jlimit(0, 2, currentType);

    if (currentType == lastSpringType)
        return;

    lastSpringType = currentType;
    const auto& preset = kSpringPresets[currentType];

    fdnDelaySamples[0] = juce::jmax(1, static_cast<int>(preset.delayA_ms * 0.001f * static_cast<float>(currentSampleRate)));
    fdnDelaySamples[1] = juce::jmax(1, static_cast<int>(preset.delayB_ms * 0.001f * static_cast<float>(currentSampleRate)));
    fdnDelaySamples[2] = juce::jmax(1, static_cast<int>(preset.delayC_ms * 0.001f * static_cast<float>(currentSampleRate)));
}

