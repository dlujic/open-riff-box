#include "Phaser.h"
#include <cmath>

Phaser::Phaser() = default;

void Phaser::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate;

    rateSmoothed.reset(sampleRate, 0.02);
    depthSmoothed.reset(sampleRate, 0.02);
    feedbackSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.reset(sampleRate, 0.02);

    float initRate = 0.05f + 9.95f * rateParam * rateParam;
    rateSmoothed.setCurrentAndTargetValue(initRate);
    depthSmoothed.setCurrentAndTargetValue(depthParam);
    feedbackSmoothed.setCurrentAndTargetValue(feedbackParam);
    mixSmoothed.setCurrentAndTargetValue(mixParam);

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < MAX_STAGES; ++s)
            stages[ch][s].reset();
        feedbackState[ch] = 0.0f;
        dcBlocker[ch].reset();
    }

    lfoPhase = 0.0f;
}

void Phaser::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < MAX_STAGES; ++s)
            stages[ch][s].reset();
        feedbackState[ch] = 0.0f;
        dcBlocker[ch].reset();
    }

    rateSmoothed.reset(currentSampleRate, 0.02);
    depthSmoothed.reset(currentSampleRate, 0.02);
    feedbackSmoothed.reset(currentSampleRate, 0.02);
    mixSmoothed.reset(currentSampleRate, 0.02);

    lfoPhase = 0.0f;
}

void Phaser::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    float targetRate = 0.05f + 9.95f * rateParam * rateParam;  // quadratic, 0.05-10 Hz
    rateSmoothed.setTargetValue(targetRate);
    depthSmoothed.setTargetValue(depthParam);
    feedbackSmoothed.setTargetValue(feedbackParam);
    mixSmoothed.setTargetValue(mixParam);

    const float sampleRateF = static_cast<float>(currentSampleRate);
    const int activeStages  = stageCountForPreset(stagesParam);

    const float logRatio = std::log(fcMax / fcMin);

    for (int i = 0; i < numSamples; ++i)
    {
        float rate  = rateSmoothed.getNextValue();
        float depth = depthSmoothed.getNextValue();
        float fb    = feedbackSmoothed.getNextValue();
        float mix   = mixSmoothed.getNextValue();

        float fbCoeff = 0.95f * fb;

        float wetGain = std::sin(mix * juce::MathConstants<float>::halfPi);
        float dryGain = std::cos(mix * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float dry = buffer.getWritePointer(ch)[i];

            float phase = (ch == 0) ? lfoPhase : std::fmod(lfoPhase + 0.05f, 1.0f);

            float lfoValue = 4.0f * std::abs(phase - 0.5f) - 1.0f;

            float normalized = (lfoValue + 1.0f) * 0.5f;  // [0, 1]
            float fc = fcMin * std::exp(logRatio * (0.5f + depth * (normalized - 0.5f)));

            float a = computeCoefficient(fc);

            float cascadeInput = dry + feedbackState[ch];

            float cascadeOutput = cascadeInput;
            for (int s = 0; s < activeStages; ++s)
                cascadeOutput = stages[ch][s].process(cascadeOutput, a);

            float fbSample = dcBlocker[ch].process(cascadeOutput);
            feedbackState[ch] = fbSample * fbCoeff;

            buffer.getWritePointer(ch)[i] = dry * dryGain + cascadeOutput * wetGain;
        }

        lfoPhase += rate / sampleRateF;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

float Phaser::computeCoefficient(float fc) const
{
    float t = std::tan(juce::MathConstants<float>::pi * fc / static_cast<float>(currentSampleRate));
    return (t - 1.0f) / (t + 1.0f);
}

void Phaser::resetToDefaults()
{
    setRate(0.15f);
    setDepth(0.5f);
    setFeedback(0.3f);
    setMix(0.5f);
    setStages(0);
}

// Parameter setters
void Phaser::setRate(float value)     { rateParam     = juce::jlimit(0.0f, 1.0f, value); }
void Phaser::setDepth(float value)    { depthParam    = juce::jlimit(0.0f, 1.0f, value); }
void Phaser::setFeedback(float value) { feedbackParam = juce::jlimit(0.0f, 1.0f, value); }
void Phaser::setMix(float value)      { mixParam      = juce::jlimit(0.0f, 1.0f, value); }
void Phaser::setStages(int value)     { stagesParam   = juce::jlimit(0, 2, value); }
