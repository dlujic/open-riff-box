#include "MetalDistortion.h"

#include <cmath>

//==============================================================================
MetalDistortion::MetalDistortion()
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
}

//==============================================================================
void MetalDistortion::prepare(double sampleRate, int samplesPerBlock)
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
        sampleRate, 75.0f);
    preHighpass.prepare(spec);

    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
        sampleRate, 7.0f);
    dcBlocker.prepare(spec);

    updateToneFilter();
    toneFilter.prepare(spec);

    levelSmoothed.reset(sampleRate, 0.02);
    levelSmoothed.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));

    juce::dsp::ProcessSpec monoOsSpec;
    monoOsSpec.sampleRate       = oversampledRate;
    monoOsSpec.maximumBlockSize = static_cast<juce::uint32>(
        samplesPerBlock * oversampling->getOversamplingFactor());
    monoOsSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        *interstageLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            oversampledRate, 4800.0f, 0.707f);
        interstageLPF[ch].prepare(monoOsSpec);

        *interstageHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
            oversampledRate, 140.0f, 0.707f);
        interstageHPF[ch].prepare(monoOsSpec);

        *postLPF1[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            oversampledRate, 5600.0f, 0.707f);
        postLPF1[ch].prepare(monoOsSpec);

        *postLPF2[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            oversampledRate, 4800.0f, 0.707f);
        postLPF2[ch].prepare(monoOsSpec);

        *postLPF3[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            oversampledRate, 4100.0f, 0.707f);
        postLPF3[ch].prepare(monoOsSpec);

        *postLPF4[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
            oversampledRate, 3500.0f, 0.707f);
        postLPF4[ch].prepare(monoOsSpec);

        *postBPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            oversampledRate, 720.0f, 1.2f, juce::Decibels::decibelsToGain(2.5f));
        postBPF[ch].prepare(monoOsSpec);
    }

    expandAttackCoeff  = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.001f));
    expandReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(oversampledRate) * 0.150f));
    metalEnv[0] = metalEnv[1] = 0.0f;
}

void MetalDistortion::reset()
{
    oversampling->reset();
    preHighpass.reset();
    dcBlocker.reset();
    toneFilter.reset();
    levelSmoothed.reset(currentSampleRate, 0.02);
    metalEnv[0] = metalEnv[1] = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        interstageLPF[ch].reset();
        interstageHPF[ch].reset();
        postLPF1[ch].reset();
        postLPF2[ch].reset();
        postLPF3[ch].reset();
        postLPF4[ch].reset();
        postBPF[ch].reset();
    }
}

//==============================================================================
int MetalDistortion::getLatencySamples() const
{
    return static_cast<int>(oversampling->getLatencyInSamples());
}

//==============================================================================
void MetalDistortion::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    updateToneFilter();

    levelSmoothed.setTargetValue(
        juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));

    const float stage2Gain = 6.0f * std::pow(48.0f, driveParam);

    //----------------------------------------------------------------------
    // 1. Pre-highpass (75 Hz)
    //----------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        preHighpass.process(ctx);
    }

    //----------------------------------------------------------------------
    // 2. Oversampled waveshaping
    //----------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto osBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples  = static_cast<int>(osBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(osBlock.getNumChannels());

        for (int ch = 0; ch < osNumChannels; ++ch)
        {
            auto* samples = osBlock.getChannelPointer(static_cast<size_t>(ch));

            for (int i = 0; i < osNumSamples; ++i)
            {
                float x = samples[i];

                x = std::tanh(12.0f * x);

                x = interstageLPF[ch].processSample(x);
                x = interstageHPF[ch].processSample(x);

                x = std::tanh(stage2Gain * x);

                x = postLPF1[ch].processSample(x);
                x = postLPF2[ch].processSample(x);
                x = postLPF3[ch].processSample(x);
                x = postLPF4[ch].processSample(x);
                x = postBPF[ch].processSample(x);

                x = std::tanh(x / 2.0f);
                x *= 0.58f;

                const float absX = std::abs(x);
                const float envCoeff = (absX > metalEnv[ch]) ? expandAttackCoeff : expandReleaseCoeff;
                metalEnv[ch] += envCoeff * (absX - metalEnv[ch]);

                constexpr float expandThresh = 0.005f;
                if (metalEnv[ch] < expandThresh)
                {
                    const float atten = metalEnv[ch] / expandThresh;
                    x *= atten * atten;
                }

                samples[i] = x;
            }
        }

        oversampling->processSamplesDown(block);
    }

    //----------------------------------------------------------------------
    // 3. DC blocker
    //----------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        dcBlocker.process(ctx);
    }

    //----------------------------------------------------------------------
    // 4. Tone filter
    //----------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        toneFilter.process(ctx);
    }

    //----------------------------------------------------------------------
    // 5. Output level
    //----------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float level = levelSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= level;
    }
}

//==============================================================================
void MetalDistortion::resetToDefaults()
{
    setDrive(0.5f);
    setTone(0.7f);
    setLevel(0.75f);
}

void MetalDistortion::setDrive(float value) { driveParam = juce::jlimit(0.0f, 1.0f, value); }
void MetalDistortion::setTone(float value)  { toneParam  = juce::jlimit(0.0f, 1.0f, value); }
void MetalDistortion::setLevel(float value) { levelParam = juce::jlimit(0.0f, 1.0f, value); }

//==============================================================================
void MetalDistortion::updateToneFilter()
{
    const float cutoff = 500.0f * std::pow(10.0f, toneParam);
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, juce::jlimit(20.0f, 20000.0f, cutoff), 0.707f);
}
