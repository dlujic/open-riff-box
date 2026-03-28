#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

struct TunerResult
{
    std::atomic<float> frequency   { 0.0f };    // Hz (0 = no detection)
    std::atomic<int>   midiNote    { -1 };       // MIDI note number (0-127, -1 = none)
    std::atomic<float> centsOffset { 0.0f };    // -50 to +50 cents from nearest note
    std::atomic<float> confidence  { 0.0f };    // 0-1 (0 = no signal / unreliable)
    std::atomic<int>   noteNameIndex { 0 };     // 0-11 index into note name table

    static constexpr const char* noteNames[12] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B"
    };

    const char* getNoteName() const { return noteNames[noteNameIndex.load(std::memory_order_relaxed)]; }
    int getOctave() const
    {
        int note = midiNote.load(std::memory_order_relaxed);
        return (note >= 0) ? (note / 12 - 1) : -1;
    }
};

class TunerEngine
{
public:
    TunerEngine();
    ~TunerEngine() = default;

    void prepare(double sampleRate);
    void processSamples(const float* samples, int numSamples);
    void reset();

    const TunerResult& getResult() const { return result; }

    void setActive(bool active) { engineActive.store(active, std::memory_order_release); }
    bool isActive() const       { return engineActive.load(std::memory_order_acquire); }

private:
    static constexpr int kBufferSize = 4096;
    static constexpr int kHalfBuffer = kBufferSize / 2;
    static constexpr float kYinThreshold = 0.15f;
    static constexpr float kMinFrequency = 70.0f;
    static constexpr float kMaxFrequency = 1400.0f;
    static constexpr float kSilenceThreshold = 0.001f;
    static constexpr int kHopSize = 2048;

    std::array<float, kBufferSize> inputBuffer {};
    int writeIndex = 0;
    int samplesSinceLastDetection = 0;

    std::array<float, kHalfBuffer> yinBuffer {};
    std::array<float, kBufferSize> linearBuffer {};  // linearized copy for cache-friendly access

    struct BiquadState { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    struct BiquadCoeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    BiquadState  lpfState {};
    BiquadCoeffs lpfCoeffs {};

    double currentSampleRate = 44100.0;
    int minTau = 0;
    int maxTau = 0;

    std::atomic<bool> engineActive { false };
    TunerResult result;

    void computeDifference();
    void cumulativeMeanNormalize();
    int  absoluteThreshold() const;
    float parabolicInterpolation(int tauEstimate) const;
    void runDetection();
    float computeRMS() const;

    static void frequencyToNote(float freqHz, int& midiNote,
                                float& centsOffset, int& noteNameIndex);
    void computeLPFCoefficients(double sampleRate);
    float processLPF(float sample);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerEngine)
};
