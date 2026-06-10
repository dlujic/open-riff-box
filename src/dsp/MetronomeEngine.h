#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>
#include <vector>

// Non-effect audio citizen owned by the processor.
// Never enters the EffectChain. Atomics for all cross-thread params.
class MetronomeEngine
{
public:
    MetronomeEngine();
    ~MetronomeEngine() = default;

    // Called on message thread before playback starts (or on sample rate change).
    void prepare(double sampleRate);

    // Called on audio thread inside processBlock, after effectChain.process()
    // and before master volume. Sums click into all channels.
    void process(juce::AudioBuffer<float>& buffer);

    void reset();

    //===========================================================================
    // Parameters -- set from message thread, read on audio thread via atomics
    //===========================================================================
    void setBpm(int bpm);
    void setBeatsPerBar(int beats);   // 2, 3, 4, or 6
    void setVolume(float volume);     // 0-1
    void setRunning(bool running);

    int   getBpm()         const { return atomicBpm.load(std::memory_order_relaxed); }
    int   getBeatsPerBar() const { return atomicBeats.load(std::memory_order_relaxed); }
    float getVolume()      const { return atomicVolume.load(std::memory_order_relaxed); }
    bool  isRunning()      const { return atomicRunning.load(std::memory_order_acquire); }

private:
    static constexpr int kMaxClickSamples = 4096;  // enough for ~90 ms at 44.1 kHz

    // Pre-rendered click waveforms (message-thread, in prepare())
    std::vector<float> accentClick;
    std::vector<float> regularClick;

    // Audio-thread scheduling state
    double sampleRate      = 44100.0;
    double beatAccumulator = 0.0;  // fractional samples remaining to the next beat
    double samplesPerBeat  = 0.0;

    int beatIndex = 0;             // 0-based beat within the current bar

    // Active click playback state
    const float* activeClickData = nullptr;
    int          activeClickLen  = 0;
    int          activeClickPos  = 0;  // current read position within the active click

    // Cached param snapshot: read once per block, not per sample
    int blockBpm   = 120;
    int blockBeats = 4;

    // Volume smoothing -- prevents zipper noise on volume changes
    juce::SmoothedValue<float> volumeSmoothed;

    std::atomic<int>   atomicBpm     { 120 };
    std::atomic<int>   atomicBeats   { 4 };
    std::atomic<float> atomicVolume  { 0.2f };
    std::atomic<bool>  atomicRunning { false };

    // Set when setRunning(true) is called. Audio thread fires beat 1 immediately.
    std::atomic<bool>  pendingStart  { false };

    void renderClickBuffers();

    // Renders a woodblock-style click (inharmonic partials + stick transient).
    static void renderClick(std::vector<float>& dest, double sr,
                            float f0, float peakGain);

    void triggerClick(bool accent);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomeEngine)
};
