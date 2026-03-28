#include "Distortion.h"
#include "MetalDistortion.h"

#include <cmath>

//==============================================================================
Distortion::Distortion()
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);

    metalProcessor = std::make_unique<MetalDistortion>();
}

Distortion::~Distortion() = default;

//==============================================================================
void Distortion::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    metalProcessor->prepare(sampleRate, samplesPerBlock);

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    oversampledRate = sampleRate * oversampling->getOversamplingFactor();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    *preHighpass.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
        sampleRate, 140.0f);
    preHighpass.prepare(spec);

    *dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(
        sampleRate, 7.0f);
    dcBlocker.prepare(spec);

    updateToneFilter();
    toneFilter.prepare(spec);

    driveSmoothed.reset(sampleRate, 0.02);
    levelSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);
    saturateSmoothed.reset(sampleRate, 0.02);

    driveSmoothed.setCurrentAndTargetValue(
        1.0f + 79.0f * driveParam * driveParam);
    levelSmoothed.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));
    mixSmoothed.setCurrentAndTargetValue(mixParam);
    saturateSmoothed.setCurrentAndTargetValue(saturateParam);

    satAttackCoeff  = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.002f));
    satReleaseCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.100f));
    envLevel[0] = envLevel[1] = 0.0f;

    dryBuffer.setSize(2, samplesPerBlock, false, true, true);

    juce::dsp::ProcessSpec monoOsSpec;
    monoOsSpec.sampleRate = oversampledRate;
    monoOsSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock * oversampling->getOversamplingFactor());
    monoOsSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        *interstageHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, 200.0f);
        interstageHPF[ch].prepare(monoOsSpec);

        *interstageLPF1[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 6000.0f);
        interstageLPF1[ch].prepare(monoOsSpec);

        *interstageLPF2[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 4500.0f);
        interstageLPF2[ch].prepare(monoOsSpec);

        *midHump[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(oversampledRate, 800.0f, 0.9f, juce::Decibels::decibelsToGain(2.0f));
        midHump[ch].prepare(monoOsSpec);

        *preClipLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 7200.0f);
        preClipLPF[ch].prepare(monoOsSpec);

        *autoDarkenLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 7500.0f);
        autoDarkenLPF[ch].prepare(monoOsSpec);
    }

    lastMode = modeParam.load(std::memory_order_acquire);
}

void Distortion::reset()
{
    oversampling->reset();
    preHighpass.reset();
    dcBlocker.reset();
    toneFilter.reset();
    driveSmoothed.reset(currentSampleRate, 0.02);
    levelSmoothed.reset(currentSampleRate, 0.02);
    mixSmoothed.reset(currentSampleRate, 0.02);
    saturateSmoothed.reset(currentSampleRate, 0.02);
    envLevel[0] = envLevel[1] = 0.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        interstageHPF[ch].reset();
        interstageLPF1[ch].reset();
        interstageLPF2[ch].reset();
        midHump[ch].reset();
        preClipLPF[ch].reset();
        autoDarkenLPF[ch].reset();
    }

    metalProcessor->reset();
}

//==============================================================================
int Distortion::getLatencySamples() const
{
    if (static_cast<Mode>(modeParam.load(std::memory_order_acquire)) == Mode::Metal)
        return metalProcessor->getLatencySamples();
    return static_cast<int>(oversampling->getLatencyInSamples());
}

//==============================================================================
void Distortion::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const auto currentMode = static_cast<Mode>(modeParam.load(std::memory_order_acquire));

    if (currentMode == Mode::Metal)
    {
        metalProcessor->process(buffer);
        return;
    }

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    dryBuffer.makeCopyOf(buffer, true);

    updateToneFilter();
    updateModeFilters();

    if (currentMode == Mode::Distortion)
    {
        const float adCutoff = 7500.0f - driveParam * 3500.0f;
        for (int ch = 0; ch < 2; ++ch)
            *autoDarkenLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, adCutoff);
    }

    driveSmoothed.setTargetValue(1.0f + 79.0f * driveParam * driveParam);
    levelSmoothed.setTargetValue(
        juce::Decibels::decibelsToGain(-60.0f + 66.0f * levelParam));

    if (currentMode == Mode::Distortion)
    {
        const float forcedMix = juce::jmin(1.0f, 0.75f + (driveParam / 0.3f) * 0.25f);
        mixSmoothed.setTargetValue(forcedMix);
    }
    else
    {
        mixSmoothed.setTargetValue(mixParam);
    }
    saturateSmoothed.setTargetValue(saturateParam);

    //--------------------------------------------------------------------------
    // 1. Pre-highpass
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        preHighpass.process(context);
    }

    //--------------------------------------------------------------------------
    // 1b. Saturate
    //--------------------------------------------------------------------------
    if ((currentMode == Mode::Overdrive || currentMode == Mode::TubeDrive) && saturateEnabled.load(std::memory_order_acquire))
    {
        constexpr float ratio = 4.0f;
        constexpr float gainReductionFactor = 1.0f - 1.0f / ratio;  // 0.75

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                const float amount = (ch == 0) ? saturateSmoothed.getNextValue()
                                               : saturateSmoothed.getCurrentValue();

                const float thresholdDB = -30.0f * amount;
                const float makeupGain = std::pow(10.0f, amount * 12.0f / 20.0f);

                const float rectified = samples[i] * samples[i];
                const float coeff = (rectified > envLevel[ch]) ? satAttackCoeff : satReleaseCoeff;
                envLevel[ch] += coeff * (rectified - envLevel[ch]);

                const float envDB = 10.0f * std::log10(envLevel[ch] + 1e-12f);

                const float overDB = envDB - thresholdDB;
                float gain = makeupGain;
                if (overDB > 0.0f)
                    gain *= std::pow(10.0f, -overDB * gainReductionFactor / 20.0f);

                samples[i] *= gain;
            }
        }
    }

    //--------------------------------------------------------------------------
    // 2. Drive gain
    //--------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float gain = driveSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer(ch)[i] *= gain;
    }

    //--------------------------------------------------------------------------
    // 3. Oversampled waveshaping
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        switch (currentMode)
        {
            case Mode::Overdrive:   processOverdriveBlock(oversampledBlock); break;
            case Mode::TubeDrive:   processTubeDriveBlock(oversampledBlock); break;
            case Mode::Distortion:  processDistortionBlock(oversampledBlock); break;
            default: break;
        }

        oversampling->processSamplesDown(block);
    }

    //--------------------------------------------------------------------------
    // 4. DC blocker
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        dcBlocker.process(context);
    }

    //--------------------------------------------------------------------------
    // 5. Tone
    //--------------------------------------------------------------------------
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        toneFilter.process(context);
    }

    //--------------------------------------------------------------------------
    // 6. Output level + dry/wet mix
    //--------------------------------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        const float level = levelSmoothed.getNextValue();
        const float mix   = mixSmoothed.getNextValue();

        const float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        const float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float wet = buffer.getWritePointer(ch)[i] * level;
            const float dry = dryBuffer.getReadPointer(ch)[i];
            buffer.getWritePointer(ch)[i] = dry * dryGain + wet * wetGain;
        }
    }
}

//==============================================================================
//==============================================================================
float Distortion::tubeClip(float x, float hardness, float asymmetry)
{
    if (x >= 0.0f)
        return 1.0f - std::exp(-hardness * x);
    else
        return -(1.0f - std::exp(asymmetry * hardness * x));
}

//==============================================================================
void Distortion::resetToDefaults()
{
    setMode(Mode::Overdrive);
    setClipType(ClipType::Normal);

    driveParam    = 0.5f;
    toneParam     = 0.65f;
    levelParam    = 0.70f;
    mixParam      = 0.7f;
    saturateParam = 0.5f;
    setSaturateEnabled(false);

    metalProcessor->resetToDefaults();
}

//==============================================================================
// Parameter setters
//==============================================================================
void Distortion::setDrive(float value)
{
    value = juce::jlimit(0.0f, 1.0f, value);
    if (getMode() == Mode::Metal)
        metalProcessor->setDrive(value);
    else
        driveParam = value;
}

void Distortion::setTone(float value)
{
    value = juce::jlimit(0.0f, 1.0f, value);
    if (getMode() == Mode::Metal)
        metalProcessor->setTone(value);
    else
        toneParam = value;
}

void Distortion::setLevel(float value)
{
    value = juce::jlimit(0.0f, 1.0f, value);
    if (getMode() == Mode::Metal)
        metalProcessor->setLevel(value);
    else
        levelParam = value;
}

float Distortion::getDrive() const
{
    if (getMode() == Mode::Metal)
        return metalProcessor->getDrive();
    return driveParam;
}

float Distortion::getTone() const
{
    if (getMode() == Mode::Metal)
        return metalProcessor->getTone();
    return toneParam;
}

float Distortion::getLevel() const
{
    if (getMode() == Mode::Metal)
        return metalProcessor->getLevel();
    return levelParam;
}

void Distortion::setMix(float value)
{
    mixParam = juce::jlimit(0.0f, 1.0f, value);
}

void Distortion::setSaturate(float value)
{
    saturateParam = juce::jlimit(0.0f, 1.0f, value);
}

void Distortion::setSaturateEnabled(bool on)
{
    saturateEnabled.store(on, std::memory_order_release);
}

void Distortion::setMode(Mode mode)
{
    modeParam.store(static_cast<int>(mode), std::memory_order_release);
}

void Distortion::setClipType(ClipType type)
{
    clipTypeParam.store(static_cast<int>(type), std::memory_order_release);
}

MetalDistortion* Distortion::getMetalProcessor()
{
    return metalProcessor.get();
}

//==============================================================================
// Private helpers
//==============================================================================
void Distortion::updateToneFilter()
{
    auto mode = static_cast<Mode>(modeParam.load(std::memory_order_acquire));
    float base = 500.0f, range = 1.15f;   // OD/Tube default: max ~7 kHz

    switch (mode)
    {
        case Mode::Overdrive:
        case Mode::TubeDrive:
            break;
        case Mode::Distortion:
            range = 1.08f;   // max ~6 kHz
            break;
        case Mode::Metal:
            return;
    }

    const float cutoff = base * std::pow(10.0f, toneParam * range);
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, juce::jlimit(20.0f, 20000.0f, cutoff), 0.707f);
}

void Distortion::updateModeFilters()
{
    int currentModeInt = modeParam.load(std::memory_order_acquire);
    if (currentModeInt != lastMode)
    {
        lastMode = currentModeInt;
        auto mode = static_cast<Mode>(currentModeInt);

        switch (mode)
        {
            case Mode::Overdrive:
                *preHighpass.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, 140.0f);
                for (int ch = 0; ch < 2; ++ch)
                    *preClipLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 7200.0f);
                break;
            case Mode::TubeDrive:
                *preHighpass.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(currentSampleRate, 100.0f);
                for (int ch = 0; ch < 2; ++ch)
                    *preClipLPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 5800.0f);
                break;
            case Mode::Distortion:
                *preHighpass.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 250.0f, 0.707f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    *interstageHPF[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(oversampledRate, 200.0f);
                    *interstageLPF1[ch].coefficients = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(oversampledRate, 6000.0f);
                }
                break;
            case Mode::Metal:
                break;
        }
    }
}

void Distortion::processOverdriveBlock(juce::dsp::AudioBlock<float>& oversampledBlock)
{
    const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());
    const auto currentClip = static_cast<ClipType>(clipTypeParam.load(std::memory_order_acquire));
    const float h = hardnessTable[static_cast<int>(currentClip)];

    for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = midHump[ch].processSample(samples[i]);

        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = preClipLPF[ch].processSample(samples[i]);

        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = tubeClip(samples[i], h);
    }
}

void Distortion::processTubeDriveBlock(juce::dsp::AudioBlock<float>& oversampledBlock)
{
    const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());
    const auto currentClip = static_cast<ClipType>(clipTypeParam.load(std::memory_order_acquire));
    const float h = hardnessTable[static_cast<int>(currentClip)];

    for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = preClipLPF[ch].processSample(samples[i]);

        for (int i = 0; i < osNumSamples; ++i)
            samples[i] = tubeClip(samples[i], h, 1.4f);
    }
}

void Distortion::processDistortionBlock(juce::dsp::AudioBlock<float>& oversampledBlock)
{
    const auto osNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());

    const float totalGain = driveSmoothed.getCurrentValue();
    const float stageGain = std::cbrt(totalGain);

    for (int ch = 0; ch < osNumChannels; ++ch)
    {
        auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));

        for (int i = 0; i < osNumSamples; ++i)
        {
            float x = samples[i];

            x = std::tanh(stageGain * x);
            x = interstageHPF[ch].processSample(x);
            x = interstageLPF1[ch].processSample(x);

            x = juce::jlimit(-1.0f, 1.0f, stageGain * x);
            x = interstageLPF2[ch].processSample(x);

            x = std::tanh(stageGain * 0.5f * x);

            x = autoDarkenLPF[ch].processSample(x);

            samples[i] = x;
        }
    }
}
