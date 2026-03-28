#include "Equalizer.h"
#include <cmath>

Equalizer::Equalizer() = default;

void Equalizer::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].prepare(monoSpec);
        midPeak[ch].prepare(monoSpec);
        highShelf[ch].prepare(monoSpec);

        *lowShelf[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(
            sampleRate, 100.0f, 0.707f, 1.0f);

        *midPeak[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
            sampleRate, 800.0f, 1.0f, 1.0f);

        *highShelf[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
            sampleRate, 4000.0f, 0.707f, 1.0f);
    }

    levelSmoothed.reset(sampleRate, 0.02);
    levelSmoothed.setCurrentAndTargetValue(levelParam);

    filtersDirty = true;
}

void Equalizer::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].reset();
        midPeak[ch].reset();
        highShelf[ch].reset();
    }

    levelSmoothed.reset(currentSampleRate, 0.02);
}

void Equalizer::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    updateFilters();
    levelSmoothed.setTargetValue(levelParam);

    for (int i = 0; i < numSamples; ++i)
    {
        float levelDb = levelSmoothed.getNextValue();
        float levelGain = juce::Decibels::decibelsToGain(levelDb);

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            float sample = buffer.getWritePointer(ch)[i];

            sample = lowShelf[ch].processSample(sample);
            sample = midPeak[ch].processSample(sample);
            sample = highShelf[ch].processSample(sample);

            sample *= levelGain;

            buffer.getWritePointer(ch)[i] = sample;
        }
    }
}

void Equalizer::updateFilters()
{
    if (!filtersDirty)
        return;
    filtersDirty = false;

    float bassGainFactor   = juce::Decibels::decibelsToGain(bassParam);
    float midGainFactor    = juce::Decibels::decibelsToGain(midGainParam);
    float trebleGainFactor = juce::Decibels::decibelsToGain(trebleParam);

    float midFreqHz = 200.0f * std::pow(25.0f, midFreqParam);

    auto lowShelfCoeffs  = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf(
        currentSampleRate, 100.0f, 0.707f, bassGainFactor);

    auto midPeakCoeffs   = juce::dsp::IIR::ArrayCoefficients<float>::makePeakFilter(
        currentSampleRate, midFreqHz, 1.0f, midGainFactor);

    auto highShelfCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf(
        currentSampleRate, 4000.0f, 0.707f, trebleGainFactor);

    for (int ch = 0; ch < 2; ++ch)
    {
        *lowShelf[ch].coefficients  = lowShelfCoeffs;
        *midPeak[ch].coefficients   = midPeakCoeffs;
        *highShelf[ch].coefficients = highShelfCoeffs;
    }
}

void Equalizer::resetToDefaults()
{
    setBass(0.0f);
    setMidGain(0.0f);
    setMidFreq(0.5f);
    setTreble(0.0f);
    setLevel(0.0f);
}

void Equalizer::setBass(float value)    { bassParam = juce::jlimit(-12.0f, 12.0f, value); filtersDirty = true; }
void Equalizer::setMidGain(float value) { midGainParam = juce::jlimit(-12.0f, 12.0f, value); filtersDirty = true; }
void Equalizer::setMidFreq(float value) { midFreqParam = juce::jlimit(0.0f, 1.0f, value); filtersDirty = true; }
void Equalizer::setTreble(float value)  { trebleParam = juce::jlimit(-12.0f, 12.0f, value); filtersDirty = true; }
void Equalizer::setLevel(float value)   { levelParam = juce::jlimit(-12.0f, 6.0f, value); }
