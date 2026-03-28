#include "PlateReverb.h"
#include <cmath>

PlateReverb::PlateReverb() = default;

void PlateReverb::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    float maxScale = 1.2f;
    int excursionSamp = static_cast<int>(std::ceil(kLfoExcursion_ms * 0.001f
                        * static_cast<float>(sampleRate))) + 4;

    for (int i = 0; i < 4; ++i)
        inputAP[i].allocate(msToSamples(kInputAP_ms[i], sampleRate) + 2);

    for (int i = 0; i < kNumTankBufs; ++i)
    {
        int maxDelay = msToSamples(kTankBase_ms[i] * maxScale, sampleRate);
        if (i == kModAP_L || i == kModAP_R)
            maxDelay += excursionSamp;
        tank[i].allocate(maxDelay + 4);
    }

    preDelayBuf.allocate(msToSamples(100.0f, sampleRate) + 2);

    lastPlateType = -1;
    updateDelayTimes();

    bwAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
              * 14000.0f / static_cast<float>(sampleRate));

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        outputHPF[ch].prepare(monoSpec);
        outputLPF[ch].prepare(monoSpec);

        *outputHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>
            ::makeFirstOrderHighPass(sampleRate, 40.0);
        *outputLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>
            ::makeLowPass(sampleRate, 12000.0f, 0.707f);
    }

    decaySmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
    widthSmoothed.reset(sampleRate, 0.02);

    decaySmoothed.setCurrentAndTargetValue(0.3f + 0.7f * decayParam);
    mixSmoothed.setCurrentAndTargetValue(mixParam);
    widthSmoothed.setCurrentAndTargetValue(widthParam);

    dampState[0] = dampState[1] = 0.0f;
    bwState = 0.0f;
    leftTankOut = rightTankOut = 0.0f;
    lfoPhase = 0.0f;
}

void PlateReverb::reset()
{
    for (int i = 0; i < 4; ++i)
        inputAP[i].clear();

    for (int i = 0; i < kNumTankBufs; ++i)
        tank[i].clear();

    preDelayBuf.clear();

    dampState[0] = dampState[1] = 0.0f;
    bwState = 0.0f;
    leftTankOut = rightTankOut = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        outputHPF[ch].reset();
        outputLPF[ch].reset();
    }

    decaySmoothed.reset(currentSampleRate, 0.02);
    mixSmoothed.reset(currentSampleRate, 0.02);
    widthSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
    lastPlateType = -1;
}

void PlateReverb::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateDelayTimes();

    decaySmoothed.setTargetValue(0.3f + 0.7f * decayParam);
    mixSmoothed.setTargetValue(mixParam);
    widthSmoothed.setTargetValue(widthParam);

    const float sampleRateF = static_cast<float>(currentSampleRate);
    int preDelSamp = static_cast<int>(preDelayParam * 100.0f * 0.001f * sampleRateF);
    preDelSamp = juce::jlimit(0, static_cast<int>(100.0f * 0.001f * sampleRateF), preDelSamp);

    int currentType = plateTypeParam.load(std::memory_order_acquire);
    currentType = juce::jlimit(0, 2, currentType);
    const auto& preset = kPlatePresets[currentType];

    float dampCutoff = (preset.dampCutoffBase + preset.dampCutoffRange)
                     - 2.0f * preset.dampCutoffRange * dampingParam;
    dampCutoff = juce::jlimit(200.0f, 20000.0f, dampCutoff);
    float dampAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                      * dampCutoff / sampleRateF);

    float lfoDepthSamp = kLfoExcursion_ms * 0.001f * sampleRateF;

    const int numCh = juce::jmin(numChannels, 2);
    const float halfPi = juce::MathConstants<float>::halfPi;
    const float twoPi  = juce::MathConstants<float>::twoPi;

    for (int i = 0; i < numSamples; ++i)
    {
        float decayGain = decaySmoothed.getNextValue();
        float mix       = mixSmoothed.getNextValue();
        float width     = widthSmoothed.getNextValue();

        float wetGain = std::sin(mix * halfPi);
        float dryGain = std::cos(mix * halfPi);

        float monoIn = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            monoIn += buffer.getReadPointer(ch)[i];
        monoIn /= static_cast<float>(numCh);

        // =============== PRE-DELAY ===============
        preDelayBuf.write(monoIn);
        float preDelOut = (preDelSamp > 0) ? preDelayBuf.read(preDelSamp) : monoIn;

        // =============== BANDWIDTH LPF ===============
        bwState += bwAlpha * (preDelOut - bwState);

        // =============== INPUT DIFFUSION (4 allpasses) ===============
        float diffused = bwState;
        diffused = processAllpass(inputAP[0], inputAPSamples[0], diffused, kInputDiff1);
        diffused = processAllpass(inputAP[1], inputAPSamples[1], diffused, kInputDiff1);
        diffused = processAllpass(inputAP[2], inputAPSamples[2], diffused, kInputDiff2);
        diffused = processAllpass(inputAP[3], inputAPSamples[3], diffused, kInputDiff2);

        // =============== LFO ===============
        float lfoSin = std::sin(twoPi * lfoPhase);
        float lfoCos = std::cos(twoPi * lfoPhase);
        lfoPhase += kLfoRate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        // =============== LEFT TANK HALF ===============
        float leftIn = diffused + decayGain * rightTankOut;

        float modDelay_L = static_cast<float>(tankSamples[kModAP_L])
                         + lfoSin * lfoDepthSamp;
        modDelay_L = juce::jlimit(1.0f,
                     static_cast<float>(tank[kModAP_L].bufSize - 2), modDelay_L);
        float temp = processModAllpass(tank[kModAP_L], modDelay_L, leftIn, kDecayDiff1);

        float d1Out = tank[kD1].read(tankSamples[kD1]);
        tank[kD1].write(temp);

        dampState[0] += dampAlpha * (d1Out - dampState[0]);
        temp = dampState[0];

        temp *= decayGain;

        temp = processAllpass(tank[kAP_L], tankSamples[kAP_L], temp, kDecayDiff2);

        float newLeftOut = tank[kD2].read(tankSamples[kD2]);
        tank[kD2].write(temp);

        // =============== RIGHT TANK HALF ===============
        float rightIn = diffused + decayGain * leftTankOut;

        float modDelay_R = static_cast<float>(tankSamples[kModAP_R])
                         + lfoCos * lfoDepthSamp;
        modDelay_R = juce::jlimit(1.0f,
                     static_cast<float>(tank[kModAP_R].bufSize - 2), modDelay_R);
        temp = processModAllpass(tank[kModAP_R], modDelay_R, rightIn, kDecayDiff1);

        float d3Out = tank[kD3].read(tankSamples[kD3]);
        tank[kD3].write(temp);

        dampState[1] += dampAlpha * (d3Out - dampState[1]);
        temp = dampState[1];

        temp *= decayGain;

        temp = processAllpass(tank[kAP_R], tankSamples[kAP_R], temp, kDecayDiff2);

        float newRightOut = tank[kD4].read(tankSamples[kD4]);
        tank[kD4].write(temp);

        leftTankOut  = newLeftOut;
        rightTankOut = newRightOut;

        // =============== 14-TAP STEREO OUTPUT ===============
        float outL = 0.0f;
        for (int t = 0; t < kNumTaps; ++t)
            outL += kLeftTaps[t].sign
                  * tank[kLeftTaps[t].bufIndex].read(leftTapSamples[t]);

        float outR = 0.0f;
        for (int t = 0; t < kNumTaps; ++t)
            outR += kRightTaps[t].sign
                  * tank[kRightTaps[t].bufIndex].read(rightTapSamples[t]);

        outL *= 0.6f;
        outR *= 0.6f;

        // =============== STEREO WIDTH ===============
        float mid  = (outL + outR) * 0.5f;
        float side = (outL - outR) * 0.5f;
        outL = mid + width * side;
        outR = mid - width * side;

        // =============== OUTPUT + MIX ===============
        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getReadPointer(ch)[i];
            float wet = (ch == 0) ? outL : outR;

            wet = outputHPF[ch].processSample(wet);
            wet = outputLPF[ch].processSample(wet);

            buffer.getWritePointer(ch)[i] = dry * dryGain + wet * wetGain;
        }
    }
}

void PlateReverb::resetToDefaults()
{
    setDecay(0.5f);
    setDamping(0.3f);
    setPreDelay(0.0f);
    setMix(0.3f);
    setWidth(1.0f);
    setPlateType(0);
}

void PlateReverb::setDecay(float value)    { decayParam    = juce::jlimit(0.0f, 1.0f, value); }
void PlateReverb::setDamping(float value)  { dampingParam  = juce::jlimit(0.0f, 1.0f, value); }
void PlateReverb::setPreDelay(float value) { preDelayParam = juce::jlimit(0.0f, 1.0f, value); }
void PlateReverb::setMix(float value)      { mixParam      = juce::jlimit(0.0f, 1.0f, value); }
void PlateReverb::setWidth(float value)    { widthParam    = juce::jlimit(0.0f, 1.0f, value); }

void PlateReverb::setPlateType(int type)
{
    plateTypeParam.store(juce::jlimit(0, 2, type), std::memory_order_release);
}

void PlateReverb::updateDelayTimes()
{
    int currentType = plateTypeParam.load(std::memory_order_acquire);
    currentType = juce::jlimit(0, 2, currentType);

    if (currentType == lastPlateType)
        return;

    lastPlateType = currentType;
    const auto& preset = kPlatePresets[currentType];

    for (int i = 0; i < 4; ++i)
        inputAPSamples[i] = msToSamples(kInputAP_ms[i], currentSampleRate);

    for (int i = 0; i < kNumTankBufs; ++i)
        tankSamples[i] = msToSamples(kTankBase_ms[i] * preset.delayScale,
                                     currentSampleRate);

    for (int t = 0; t < kNumTaps; ++t)
    {
        leftTapSamples[t]  = msToSamples(kLeftTaps[t].offset_ms * preset.delayScale,
                                         currentSampleRate);
        rightTapSamples[t] = msToSamples(kRightTaps[t].offset_ms * preset.delayScale,
                                         currentSampleRate);
    }
}
