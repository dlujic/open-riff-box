#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>

#include "EffectProcessor.h"

class EffectChain
{
public:
    EffectChain()  = default;
    ~EffectChain() = default;

    //===========================================================================
    // Lifecycle — mirror AudioProcessor callbacks
    //===========================================================================
    void prepare(double sampleRate, int samplesPerBlock);
    void process(juce::AudioBuffer<float>& buffer);
    void reset();

    //===========================================================================
    // Chain management (call from message thread only)
    //===========================================================================
    void addEffect(std::unique_ptr<EffectProcessor> effect);

    void removeEffect(int index);

    void moveEffect(int fromIndex, int toIndex);

    //===========================================================================
    // Accessors
    //===========================================================================
    int             getNumEffects()  const { return static_cast<int>(effects.size()); }
    EffectProcessor* getEffect(int index);
    EffectProcessor* getEffectByName(const juce::String& name);
    int             getTotalLatencySamples() const;

    std::vector<juce::String> getEffectOrder() const;

    void setEffectOrder(const std::vector<juce::String>& order);

    static const std::vector<juce::String>& getDefaultOrder();

    bool isDefaultOrder() const;

private:
    std::vector<std::unique_ptr<EffectProcessor>> effects;

    double currentSampleRate  = 44100.0;
    int    currentBlockSize   = 512;
    bool   isPrepared         = false;

    //===========================================================================
    // Input DC blocker (per-channel)
    //===========================================================================
    struct DCBlockerState { float x1 = 0.0f, y1 = 0.0f; };
    DCBlockerState dcBlocker[2];
    float dcBlockerR = 0.9995f;

    //===========================================================================
    // Reset-on-disable: previous bypass state per effect
    //===========================================================================
    std::vector<bool> wasBypassed;

    //===========================================================================
    // Output soft limiter
    //===========================================================================
    static float softLimit(float x);

    // Protects the effects vector from concurrent audio/message thread access.
    juce::SpinLock chainLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectChain)
};
