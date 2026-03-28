#include "EffectChain.h"

#include <algorithm>
#include <cmath>

//==============================================================================
void EffectChain::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    isPrepared        = true;

    dcBlockerR = 1.0f - (2.0f * juce::MathConstants<float>::pi * 5.0f
                         / static_cast<float>(sampleRate));
    dcBlocker[0] = {};
    dcBlocker[1] = {};

    for (auto& effect : effects)
        effect->prepare(sampleRate, samplesPerBlock);

    wasBypassed.resize(effects.size());
    for (size_t i = 0; i < effects.size(); ++i)
        wasBypassed[i] = effects[i]->isBypassed();
}

void EffectChain::process(juce::AudioBuffer<float>& buffer)
{
    const juce::SpinLock::ScopedTryLockType lock(chainLock);
    if (!lock.isLocked())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        auto& dc = dcBlocker[ch];
        const float R = dcBlockerR;

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = samples[i];
            const float y = x - dc.x1 + R * dc.y1;
            dc.x1 = x;
            dc.y1 = y;
            samples[i] = y;
        }
    }

    for (size_t i = 0; i < effects.size(); ++i)
    {
        const bool bypassed = effects[i]->isBypassed();

        if (!bypassed)
            effects[i]->process(buffer);
        else if (i < wasBypassed.size() && !wasBypassed[i])
            effects[i]->reset();

        if (i < wasBypassed.size())
            wasBypassed[i] = bypassed;
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            samples[i] = softLimit(samples[i]);
    }
}

void EffectChain::reset()
{
    for (auto& effect : effects)
        effect->reset();

    dcBlocker[0] = {};
    dcBlocker[1] = {};

    isPrepared = false;
}

//==============================================================================
void EffectChain::addEffect(std::unique_ptr<EffectProcessor> effect)
{
    if (effect == nullptr)
        return;

    if (isPrepared)
        effect->prepare(currentSampleRate, currentBlockSize);

    const juce::SpinLock::ScopedLockType lock(chainLock);
    effects.push_back(std::move(effect));
    wasBypassed.push_back(effects.back()->isBypassed());
}

void EffectChain::removeEffect(int index)
{
    const juce::SpinLock::ScopedLockType lock(chainLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return;

    effects.erase(effects.begin() + index);
    if (index < static_cast<int>(wasBypassed.size()))
        wasBypassed.erase(wasBypassed.begin() + index);
}

void EffectChain::moveEffect(int fromIndex, int toIndex)
{
    const juce::SpinLock::ScopedLockType lock(chainLock);

    const int numEffects = static_cast<int>(effects.size());

    if (numEffects < 2)
        return;

    fromIndex = juce::jlimit(0, numEffects - 1, fromIndex);
    toIndex   = juce::jlimit(0, numEffects - 1, toIndex);

    if (fromIndex == toIndex)
        return;

    if (fromIndex < toIndex)
    {
        std::rotate(effects.begin() + fromIndex,
                    effects.begin() + fromIndex + 1,
                    effects.begin() + toIndex + 1);
        std::rotate(wasBypassed.begin() + fromIndex,
                    wasBypassed.begin() + fromIndex + 1,
                    wasBypassed.begin() + toIndex + 1);
    }
    else
    {
        std::rotate(effects.begin() + toIndex,
                    effects.begin() + fromIndex,
                    effects.begin() + fromIndex + 1);
        std::rotate(wasBypassed.begin() + toIndex,
                    wasBypassed.begin() + fromIndex,
                    wasBypassed.begin() + fromIndex + 1);
    }

}

//==============================================================================
EffectProcessor* EffectChain::getEffect(int index)
{
    if (index < 0 || index >= static_cast<int>(effects.size()))
        return nullptr;

    return effects[static_cast<std::size_t>(index)].get();
}

EffectProcessor* EffectChain::getEffectByName(const juce::String& name)
{
    for (auto& effect : effects)
    {
        if (effect->getName() == name)
            return effect.get();
    }
    return nullptr;
}

std::vector<juce::String> EffectChain::getEffectOrder() const
{
    std::vector<juce::String> order;
    order.reserve(effects.size());
    for (const auto& effect : effects)
        order.push_back(effect->getName());
    return order;
}

void EffectChain::setEffectOrder(const std::vector<juce::String>& order)
{
    const juce::SpinLock::ScopedLockType lock(chainLock);

    std::vector<std::unique_ptr<EffectProcessor>> reordered;
    std::vector<bool> reorderedBypassed;
    std::vector<bool> placed(effects.size(), false);

    reordered.reserve(effects.size());
    reorderedBypassed.reserve(effects.size());

    for (const auto& name : order)
    {
        for (size_t i = 0; i < effects.size(); ++i)
        {
            if (!placed[i] && effects[i]->getName() == name)
            {
                reordered.push_back(std::move(effects[i]));
                reorderedBypassed.push_back(i < wasBypassed.size() ? wasBypassed[i] : false);
                placed[i] = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < effects.size(); ++i)
    {
        if (!placed[i])
        {
            reordered.push_back(std::move(effects[i]));
            reorderedBypassed.push_back(i < wasBypassed.size() ? wasBypassed[i] : false);
        }
    }

    effects = std::move(reordered);
    wasBypassed = std::move(reorderedBypassed);
}

const std::vector<juce::String>& EffectChain::getDefaultOrder()
{
    static const std::vector<juce::String> order = {
        "Diode Drive", "Distortion", "Amp Silver", "Amp Gold", "Amp Platinum",
        "Noise Gate", "Delay", "Spring Reverb", "Plate Reverb", "Chorus", "Flanger", "Phaser", "Vibrato", "EQ"
    };
    return order;
}

bool EffectChain::isDefaultOrder() const
{
    const auto& def = getDefaultOrder();
    if (effects.size() != def.size())
        return false;

    for (size_t i = 0; i < effects.size(); ++i)
    {
        if (effects[i]->getName() != def[i])
            return false;
    }
    return true;
}

int EffectChain::getTotalLatencySamples() const
{
    int total = 0;
    for (const auto& effect : effects)
    {
        if (!effect->isBypassed())
            total += effect->getLatencySamples();
    }
    return total;
}

//==============================================================================
float EffectChain::softLimit(float x)
{
    constexpr float threshold = 0.97f;
    constexpr float headroom  = 1.0f - threshold;  // 0.03

    if (x > threshold)
        return threshold + headroom * std::tanh((x - threshold) / headroom);
    if (x < -threshold)
        return -threshold - headroom * std::tanh((-x - threshold) / headroom);
    return x;
}
