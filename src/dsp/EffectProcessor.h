#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// Abstract base class for all audio effects in the OpenRiffBox chain.
// Concrete implementations must live in src/dsp/ and must not include
// any UI headers.
class EffectProcessor
{
public:
    EffectProcessor() = default;
    virtual ~EffectProcessor() = default;

    // Called before processing begins or when the stream format changes.
    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

    // Process one block of audio in-place.
    // Only called when isBypassed() returns false.
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;

    // Reset internal state (e.g. clear delay lines, reset filters).
    virtual void reset() = 0;

    // Human-readable name used for display and serialisation.
    virtual juce::String getName() const = 0;

    // Latency introduced by this effect (e.g. from oversampling).
    // Default returns 0 — override in effects that add latency.
    virtual int getLatencySamples() const { return 0; }

    // Reset all parameters to factory defaults.
    // Does NOT change bypass state.
    virtual void resetToDefaults() {}

    //===========================================================================
    // Bypass (atomic — safe for UI thread write / audio thread read)
    //===========================================================================
    bool isBypassed() const  { return bypassed.load(std::memory_order_acquire); }
    void setBypassed(bool b) { bypassed.store(b, std::memory_order_release); }

private:
    std::atomic<bool> bypassed { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectProcessor)
};
