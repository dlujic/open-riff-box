#include "MetronomeEngine.h"
#include <cmath>

MetronomeEngine::MetronomeEngine()
{
    accentClick.reserve(kMaxClickSamples);
    regularClick.reserve(kMaxClickSamples);
}

void MetronomeEngine::prepare(double sr)
{
    sampleRate = sr;

    volumeSmoothed.reset(sr, 0.05);
    volumeSmoothed.setCurrentAndTargetValue(atomicVolume.load(std::memory_order_relaxed));

    renderClickBuffers();

    // Full phase reset so the first beat fires immediately after start.
    beatAccumulator = 0.0;
    samplesPerBeat  = sampleRate * 60.0 / static_cast<double>(atomicBpm.load(std::memory_order_relaxed));
    beatIndex       = 0;

    activeClickData = nullptr;
    activeClickLen  = 0;
    activeClickPos  = 0;

    blockBpm   = atomicBpm.load(std::memory_order_relaxed);
    blockBeats = atomicBeats.load(std::memory_order_relaxed);
}

void MetronomeEngine::reset()
{
    beatAccumulator = 0.0;
    beatIndex       = 0;
    activeClickData = nullptr;
    activeClickLen  = 0;
    activeClickPos  = 0;
    pendingStart.store(false, std::memory_order_relaxed);
}

void MetronomeEngine::renderClickBuffers()
{
    // Woodblock-style "tock". Accent sits a fifth above the regular click and louder.
    renderClick(accentClick,  sampleRate, 1200.0f, 0.90f);
    renderClick(regularClick, sampleRate,  800.0f, 0.70f);
}

void MetronomeEngine::renderClick(std::vector<float>& dest, double sr,
                                  float f0, float peakGain)
{
    struct Partial { float ratio, amp, tauMs; };
    // Inharmonic ratios approximate woodblock body modes -- integer ratios
    // would read as a pitched beep, not percussion. The brief noise burst
    // supplies the stick transient.
    constexpr Partial partials[] = {
        { 1.00f, 1.00f, 8.0f },
        { 2.45f, 0.40f, 4.0f },
        { 4.20f, 0.15f, 2.0f },
    };

    const int totalSamples  = static_cast<int>(sr * 0.035);
    const int attackSamples = juce::jmax(1, static_cast<int>(sr * 0.001));
    const int fadeSamples   = static_cast<int>(sr * 0.005);
    dest.assign(static_cast<size_t>(totalSamples), 0.0f);

    juce::Random rng(0x5EED);  // fixed seed: identical clicks across prepares

    // One-pole lowpass on the stick transient -- raw white noise reads as sharp.
    const double noiseLpCoeff = 1.0 - std::exp(-juce::MathConstants<double>::twoPi * 3000.0 / sr);
    double noiseLp = 0.0;

    float maxAbs = 0.0f;
    for (int i = 0; i < totalSamples; ++i)
    {
        const double t = static_cast<double>(i) / sr;

        double s = 0.0;
        for (const auto& p : partials)
            s += p.amp * std::sin(juce::MathConstants<double>::twoPi * f0 * p.ratio * t)
                       * std::exp(-t / (p.tauMs * 0.001));

        noiseLp += noiseLpCoeff * ((rng.nextDouble() * 2.0 - 1.0) - noiseLp);
        s += 0.4 * noiseLp * std::exp(-t / 0.0012);

        if (i < attackSamples)
            s *= static_cast<double>(i + 1) / static_cast<double>(attackSamples);

        // Exponential decay alone never reaches zero -- fade the tail out so
        // the buffer end is not a step.
        if (i >= totalSamples - fadeSamples)
            s *= static_cast<double>(totalSamples - i) / static_cast<double>(fadeSamples);

        dest[static_cast<size_t>(i)] = static_cast<float>(s);
        maxAbs = juce::jmax(maxAbs, std::abs(dest[static_cast<size_t>(i)]));
    }

    if (maxAbs > 0.0f)
        juce::FloatVectorOperations::multiply(dest.data(), peakGain / maxAbs, totalSamples);
}

void MetronomeEngine::triggerClick(bool accent)
{
    if (accent)
    {
        activeClickData = accentClick.data();
        activeClickLen  = static_cast<int>(accentClick.size());
    }
    else
    {
        activeClickData = regularClick.data();
        activeClickLen  = static_cast<int>(regularClick.size());
    }
    activeClickPos = 0;
}

void MetronomeEngine::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    auto* const* channels = buffer.getArrayOfWritePointers();

    if (!atomicRunning.load(std::memory_order_acquire))
    {
        // Let an in-flight click decay to silence -- truncating it on stop pops.
        for (int i = 0; i < numSamples && activeClickData != nullptr; ++i)
        {
            const float clickSample = activeClickData[activeClickPos] * volumeSmoothed.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] += clickSample;
            if (++activeClickPos >= activeClickLen)
            {
                activeClickData = nullptr;
                activeClickPos  = 0;
            }
        }
        return;
    }

    // Read params once per block.
    const int bpm   = atomicBpm.load(std::memory_order_relaxed);
    const int beats = atomicBeats.load(std::memory_order_relaxed);
    volumeSmoothed.setTargetValue(atomicVolume.load(std::memory_order_relaxed));

    // BPM change takes effect at the next beat: the accumulator finishes its
    // current countdown, only the reload value changes. No retrigger, no glitch.
    if (bpm != blockBpm)
    {
        blockBpm       = bpm;
        samplesPerBeat = sampleRate * 60.0 / static_cast<double>(blockBpm);
    }
    blockBeats = beats;

    if (pendingStart.exchange(false, std::memory_order_acq_rel))
    {
        // Restart: beat 1 fires at sample 0 of this block. Snap the volume
        // smoother -- nothing was sounding, so there is nothing to ramp from.
        volumeSmoothed.setCurrentAndTargetValue(volumeSmoothed.getTargetValue());
        triggerClick(true);
        beatAccumulator = samplesPerBeat;
        beatIndex       = 1 % blockBeats;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // The smoother must advance every sample, click playing or not.
        const float vol = volumeSmoothed.getNextValue();

        if (activeClickData != nullptr)
        {
            const float clickSample = activeClickData[activeClickPos] * vol;
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] += clickSample;
            if (++activeClickPos >= activeClickLen)
            {
                activeClickData = nullptr;
                activeClickPos  = 0;
            }
        }

        // Countdown with fractional carry: reloading via += preserves the
        // sub-sample grid position. Truncating instead would drift ~0.4 ms/min
        // at 130 BPM / 44.1 kHz.
        beatAccumulator -= 1.0;
        if (beatAccumulator <= 0.0)
        {
            beatAccumulator += samplesPerBeat;

            // Bar accent only -- 6/8 gets no secondary accent on beat 4.
            // Retrigger cuts any unfinished tail; new beat wins.
            triggerClick(beatIndex == 0);
            beatIndex = (beatIndex + 1) % blockBeats;
        }
    }
}

//===========================================================================
// Parameter setters
//===========================================================================
void MetronomeEngine::setBpm(int bpm)
{
    atomicBpm.store(juce::jlimit(30, 300, bpm), std::memory_order_relaxed);
}

void MetronomeEngine::setBeatsPerBar(int beats)
{
    int clamped = beats;
    if (clamped != 2 && clamped != 3 && clamped != 4 && clamped != 6)
        clamped = 4;
    atomicBeats.store(clamped, std::memory_order_relaxed);
}

void MetronomeEngine::setVolume(float volume)
{
    atomicVolume.store(juce::jlimit(0.0f, 1.0f, volume), std::memory_order_relaxed);
}

void MetronomeEngine::setRunning(bool running)
{
    if (running)
        pendingStart.store(true, std::memory_order_release);
    atomicRunning.store(running, std::memory_order_release);
}
