#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

// Lookahead peak limiter for the master output. Replaces the old chain
// softLimit + brickwall pair - memoryless 1x clippers whose crackle on hot
// program was blind-identified (C1 bench, 2026-07-03).
//
// Gain path: required gain -> one-pole release (slow rise, instant fall) ->
// sliding-window minimum over the lookahead -> boxcar average of the same
// length. The boxcar turns the min into a linear ramp that completes exactly
// as the delayed peak arrives, so the ceiling holds without waveform
// clipping. Stereo-linked so the image never wanders.
class LookaheadLimiter
{
public:
    LookaheadLimiter()  = default;
    ~LookaheadLimiter() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    // The delay is always applied so reported latency stays constant across
    // toggling; gain reduction only happens when apply is true.
    void process(juce::AudioBuffer<float>& buffer, bool apply);

    int getLatencySamples() const { return lookahead; }

    // Worst gain reduction in the last processed block, dB (>= 0). UI meter.
    float getGainReductionDb() const { return grDb.load(std::memory_order_relaxed); }

private:
    static constexpr float  kCeiling     = 0.9886f;  // -0.1 dBFS, as the old brickwall
    static constexpr double kLookaheadMs = 1.0;
    static constexpr double kReleaseMs   = 60.0;

    int   lookahead    = 48;
    float releaseCoeff = 0.999f;
    float releaseState = 1.0f;

    std::vector<std::vector<float>> delayRing;   // per channel, lookahead long
    int delayPos = 0;

    // Monotonic queue for the sliding minimum; capacity lookahead + 1.
    std::vector<float>       minQueueVal;
    std::vector<juce::int64> minQueueIdx;
    juce::int64 qHead = 0, qTail = 0;
    juce::int64 sampleIndex = 0;

    std::vector<float> maRing;
    int    maPos = 0;
    double maSum = 0.0;

    std::atomic<float> grDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LookaheadLimiter)
};
