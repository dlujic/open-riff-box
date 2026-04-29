#include "Tremolo.h"
#include <cmath>

Tremolo::Tremolo() = default;

void Tremolo::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    rateSmoothed.reset(sampleRate, 0.15);
    depthSmoothed.reset(sampleRate, 0.04);
    widthSmoothed.reset(sampleRate, 0.04);
    outputSmoothed.reset(sampleRate, 0.03);

    rateSmoothed.setCurrentAndTargetValue  (rateParamToHz(rateParam));
    depthSmoothed.setCurrentAndTargetValue (depthParam);
    widthSmoothed.setCurrentAndTargetValue (widthParamToPhaseOffset(widthParam));
    outputSmoothed.setCurrentAndTargetValue(outputParamToGain(outputParam));

    // 35 ms release: physical CdS LDRs run 300-1000 ms but were paired with
    // a near-square neon drive; with our smooth comparator output, that long
    // pins the envelope near its mean and kills the modulation.
    const float fs = static_cast<float>(sampleRate);
    ldrAttackCoeff  = std::exp(-1.0f / (0.010f * fs));
    ldrReleaseCoeff = std::exp(-1.0f / (0.035f * fs));
    ldrEnv[0] = ldrEnv[1] = 0.0f;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampler->reset();

    const float dcR = 1.0f - (juce::MathConstants<float>::twoPi * 5.0f
                              / static_cast<float>(sampleRate));
    for (auto& dc : dcBlocker)
    {
        dc.R = dcR;
        dc.reset();
    }

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate       = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels      = 1;

    // 400 Hz balances guitar fundamentals (LF) against upper harmonics (HF).
    // Higher (e.g. 700 Hz) puts almost all energy in LF and collapses the see-saw.
    for (int ch = 0; ch < 2; ++ch)
    {
        crossLPF[ch].prepare(monoSpec);
        crossHPF[ch].prepare(monoSpec);
        crossLPF[ch].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        crossHPF[ch].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        crossLPF[ch].setCutoffFrequency(400.0f);
        crossHPF[ch].setCutoffFrequency(400.0f);
        crossLPF[ch].reset();
        crossHPF[ch].reset();
    }

    lfoPhase = 0.0f;
}

void Tremolo::reset()
{
    rateSmoothed.reset(currentSampleRate, 0.15);
    depthSmoothed.reset(currentSampleRate, 0.04);
    widthSmoothed.reset(currentSampleRate, 0.04);
    outputSmoothed.reset(currentSampleRate, 0.03);

    ldrEnv[0] = ldrEnv[1] = 0.0f;

    if (oversampler != nullptr) oversampler->reset();
    for (auto& dc : dcBlocker) dc.reset();

    for (int ch = 0; ch < 2; ++ch)
    {
        crossLPF[ch].reset();
        crossHPF[ch].reset();
    }

    lfoPhase = 0.0f;
}

int Tremolo::getLatencySamples() const
{
    if (modeParam == Bias && oversampler != nullptr)
        return static_cast<int>(oversampler->getLatencyInSamples());
    return 0;
}

void Tremolo::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) return;

    rateSmoothed.setTargetValue  (rateParamToHz(rateParam));
    depthSmoothed.setTargetValue (depthParam);
    widthSmoothed.setTargetValue (widthParamToPhaseOffset(widthParam));
    outputSmoothed.setTargetValue(outputParamToGain(outputParam));

    switch (modeParam)
    {
        case Bias:     processBias    (buffer); break;
        case Harmonic: processHarmonic(buffer); break;
        case Photo:
        default:       processPhoto   (buffer); break;
    }
}

void Tremolo::processPhoto(juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const float fs = static_cast<float>(currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float rateHz       = rateSmoothed.getNextValue();
        const float depth        = depthSmoothed.getNextValue();
        const float widthOffset  = widthSmoothed.getNextValue();
        const float outputGain   = outputSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float chPhase = (ch == 0) ? lfoPhase
                                            : std::fmod(lfoPhase + widthOffset, 1.0f);
            const float lfo = std::sin(chPhase * juce::MathConstants<float>::twoPi);

            // Hard-knee comparator (neon strike). Without it the envelope has
            // nothing to round off and the modulation is inaudible.
            const float drive = (lfo > 0.0f) ? 1.0f : 0.0f;

            float& env = ldrEnv[ch];
            const float coeff = (drive > env) ? ldrAttackCoeff : ldrReleaseCoeff;
            env = drive + (env - drive) * coeff;

            const float gain = (1.0f - depth) + depth * env;

            float* w = buffer.getWritePointer(ch);
            w[i] = w[i] * gain * outputGain;
        }

        lfoPhase += rateHz / fs;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

void Tremolo::processBias(juce::AudioBuffer<float>& buffer)
{
    if (oversampler == nullptr) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const float fs = static_cast<float>(currentSampleRate);

    juce::dsp::AudioBlock<float> block(buffer);
    auto osBlock = oversampler->processSamplesUp(block);

    const int osNum = static_cast<int>(osBlock.getNumSamples());
    const float osFs = fs * 4.0f;

    int osIdx = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float rateHz       = rateSmoothed.getNextValue();
        const float depth        = depthSmoothed.getNextValue();
        const float widthOffset  = widthSmoothed.getNextValue();
        const float outputGain   = outputSmoothed.getNextValue();

        // Drive 0.6x..1.4x gives audible THD variation without clean->fuzz alternation.
        // Gain-mod scaled to 30% so it doesn't double up on the drive's perceived intensity.
        constexpr float driveBase    = 1.0f;
        constexpr float driveMod     = 0.4f;
        constexpr float gainModScale = 0.3f;

        for (int j = 0; j < 4; ++j)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float chPhase = (ch == 0) ? lfoPhase
                                                : std::fmod(lfoPhase + widthOffset, 1.0f);
                const float lfo = std::sin(chPhase * juce::MathConstants<float>::twoPi);

                const float drive   = driveBase + driveMod * lfo;
                const float scaledD = depth * gainModScale;
                const float gainMod = (1.0f - scaledD) + scaledD * 0.5f * (1.0f + lfo);

                float* w = osBlock.getChannelPointer(static_cast<size_t>(ch));
                float x = w[osIdx + j];
                x = std::tanh(x * drive) * gainMod;
                x = dcBlocker[ch].process(x);
                w[osIdx + j] = x * outputGain;
            }

            lfoPhase += rateHz / osFs;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }

        osIdx += 4;
        if (osIdx >= osNum) break;
    }

    oversampler->processSamplesDown(block);
}

void Tremolo::processHarmonic(juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const float fs = static_cast<float>(currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float rateHz       = rateSmoothed.getNextValue();
        const float depth        = depthSmoothed.getNextValue();
        const float widthOffset  = widthSmoothed.getNextValue();
        const float outputGain   = outputSmoothed.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float chPhase = (ch == 0) ? lfoPhase
                                            : std::fmod(lfoPhase + widthOffset, 1.0f);
            const float lfo = std::sin(chPhase * juce::MathConstants<float>::twoPi);

            const float gainLow  = (1.0f - depth) + depth * 0.5f * (1.0f - lfo);
            const float gainHigh = (1.0f - depth) + depth * 0.5f * (1.0f + lfo);

            float* w = buffer.getWritePointer(ch);
            const float in = w[i];
            const float lo = crossLPF[ch].processSample(0, in);
            const float hi = crossHPF[ch].processSample(0, in);
            w[i] = (lo * gainLow + hi * gainHigh) * outputGain;
        }

        lfoPhase += rateHz / fs;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

void Tremolo::resetToDefaults()
{
    setRate(0.4f);
    setDepth(0.5f);
    setMode(Photo);
    setWidth(0.0f);
    setOutput(0.5f);
}

void Tremolo::setRate(float value)   { rateParam   = juce::jlimit(0.0f, 1.0f, value); }
void Tremolo::setDepth(float value)  { depthParam  = juce::jlimit(0.0f, 1.0f, value); }
void Tremolo::setWidth(float value)  { widthParam  = juce::jlimit(0.0f, 1.0f, value); }
void Tremolo::setOutput(float value) { outputParam = juce::jlimit(0.0f, 1.0f, value); }

void Tremolo::setMode(int value) { modeParam = juce::jlimit(0, 2, value); }
