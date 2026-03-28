#include "TunerEngine.h"

#include <cmath>

constexpr const char* TunerResult::noteNames[12];

TunerEngine::TunerEngine() {}

void TunerEngine::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;
    minTau = std::max(2, static_cast<int>(std::floor(sampleRate / kMaxFrequency)));
    maxTau = std::min(kHalfBuffer - 1, static_cast<int>(std::ceil(sampleRate / kMinFrequency)));
    computeLPFCoefficients(sampleRate);
    reset();
}

void TunerEngine::reset()
{
    inputBuffer.fill(0.0f);
    yinBuffer.fill(0.0f);
    writeIndex = 0;
    samplesSinceLastDetection = 0;
    lpfState = {};

    result.frequency.store(0.0f, std::memory_order_relaxed);
    result.midiNote.store(-1, std::memory_order_relaxed);
    result.centsOffset.store(0.0f, std::memory_order_relaxed);
    result.confidence.store(0.0f, std::memory_order_relaxed);
    result.noteNameIndex.store(0, std::memory_order_relaxed);
}

void TunerEngine::processSamples(const float* samples, int numSamples)
{
    if (!engineActive.load(std::memory_order_acquire))
        return;

    juce::ScopedNoDenormals noDenormals;

    for (int i = 0; i < numSamples; ++i)
    {
        inputBuffer[writeIndex] = processLPF(samples[i]);
        writeIndex = (writeIndex + 1) % kBufferSize;

        if (++samplesSinceLastDetection >= kHopSize)
        {
            samplesSinceLastDetection = 0;
            runDetection();
        }
    }
}

void TunerEngine::runDetection()
{
    for (int i = 0; i < kBufferSize; ++i)
        linearBuffer[i] = inputBuffer[(writeIndex + i) & (kBufferSize - 1)];

    float rms = computeRMS();
    if (rms < kSilenceThreshold)
    {
        result.frequency.store(0.0f, std::memory_order_relaxed);
        result.midiNote.store(-1, std::memory_order_relaxed);
        result.centsOffset.store(0.0f, std::memory_order_relaxed);
        result.confidence.store(0.0f, std::memory_order_relaxed);
        return;
    }

    computeDifference();
    cumulativeMeanNormalize();

    int bestTau = absoluteThreshold();
    if (bestTau < 0)
    {
        result.frequency.store(0.0f, std::memory_order_relaxed);
        result.midiNote.store(-1, std::memory_order_relaxed);
        result.centsOffset.store(0.0f, std::memory_order_relaxed);
        result.confidence.store(0.0f, std::memory_order_relaxed);
        return;
    }

    float refinedTau = parabolicInterpolation(bestTau);
    float detectedFreq = static_cast<float>(currentSampleRate) / refinedTau;

    if (detectedFreq < kMinFrequency || detectedFreq > kMaxFrequency)
    {
        result.frequency.store(0.0f, std::memory_order_relaxed);
        result.midiNote.store(-1, std::memory_order_relaxed);
        result.centsOffset.store(0.0f, std::memory_order_relaxed);
        result.confidence.store(0.0f, std::memory_order_relaxed);
        return;
    }

    float conf = juce::jlimit(0.0f, 1.0f, 1.0f - yinBuffer[bestTau]);

    int midi = 0;
    float cents = 0.0f;
    int nameIdx = 0;
    frequencyToNote(detectedFreq, midi, cents, nameIdx);

    result.frequency.store(detectedFreq, std::memory_order_relaxed);
    result.midiNote.store(midi, std::memory_order_relaxed);
    result.centsOffset.store(cents, std::memory_order_relaxed);
    result.confidence.store(conf, std::memory_order_relaxed);
    result.noteNameIndex.store(nameIdx, std::memory_order_relaxed);
}

void TunerEngine::computeDifference()
{
    yinBuffer[0] = 1.0f;

    const float* buf = linearBuffer.data();
    const int limit = maxTau + 1;

    for (int tau = 1; tau < limit; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < kHalfBuffer; ++j)
        {
            float delta = buf[j] - buf[j + tau];
            sum += delta * delta;
        }
        yinBuffer[tau] = sum;
    }
}

void TunerEngine::cumulativeMeanNormalize()
{
    const int limit = maxTau + 1;
    float runningSum = 0.0f;
    for (int tau = 1; tau < limit; ++tau)
    {
        runningSum += yinBuffer[tau];
        if (runningSum < 1e-10f)
            yinBuffer[tau] = 1.0f;
        else
            yinBuffer[tau] = yinBuffer[tau] * static_cast<float>(tau) / runningSum;
    }
}

int TunerEngine::absoluteThreshold() const
{
    int tau = minTau;
    while (tau < maxTau)
    {
        if (yinBuffer[tau] < kYinThreshold)
        {
            while (tau + 1 < maxTau && yinBuffer[tau + 1] < yinBuffer[tau])
                ++tau;
            return tau;
        }
        ++tau;
    }

    int bestTau = minTau;
    float bestVal = yinBuffer[minTau];
    for (int t = minTau + 1; t < maxTau; ++t)
    {
        if (yinBuffer[t] < bestVal)
        {
            bestVal = yinBuffer[t];
            bestTau = t;
        }
    }
    return (bestVal < 0.3f) ? bestTau : -1;
}

float TunerEngine::parabolicInterpolation(int tauEstimate) const
{
    if (tauEstimate < 1 || tauEstimate >= kHalfBuffer - 1)
        return static_cast<float>(tauEstimate);

    float s0 = yinBuffer[tauEstimate - 1];
    float s1 = yinBuffer[tauEstimate];
    float s2 = yinBuffer[tauEstimate + 1];

    float denom = 2.0f * (s0 - 2.0f * s1 + s2);
    if (std::abs(denom) < 1e-10f)
        return static_cast<float>(tauEstimate);

    float offset = juce::jlimit(-1.0f, 1.0f, (s0 - s2) / denom);
    return static_cast<float>(tauEstimate) + offset;
}

float TunerEngine::computeRMS() const
{
    const float* buf = linearBuffer.data();
    float sum = 0.0f;
    for (int j = 0; j < kHalfBuffer; ++j)
        sum += buf[j] * buf[j];
    return std::sqrt(sum / static_cast<float>(kHalfBuffer));
}

void TunerEngine::frequencyToNote(float freqHz, int& midiNote,
                                   float& centsOffset, int& noteNameIndex)
{
    float semitones = 12.0f * std::log2(freqHz / 440.0f);
    int nearest = juce::jlimit(0, 127, 69 + static_cast<int>(std::round(semitones)));

    float nearestFreq = 440.0f * std::pow(2.0f, static_cast<float>(nearest - 69) / 12.0f);
    float cents = juce::jlimit(-50.0f, 50.0f, 1200.0f * std::log2(freqHz / nearestFreq));

    midiNote = nearest;
    centsOffset = cents;
    noteNameIndex = nearest % 12;
}

void TunerEngine::computeLPFCoefficients(double sampleRate)
{
    constexpr double cutoffHz = 1500.0;
    const double omega = 2.0 * juce::MathConstants<double>::pi * cutoffHz / sampleRate;
    const double cosOmega = std::cos(omega);
    const double sinOmega = std::sin(omega);
    const double alpha = sinOmega / (2.0 * std::sqrt(2.0));

    const double a0 = 1.0 + alpha;
    lpfCoeffs.b0 = static_cast<float>((1.0 - cosOmega) / 2.0 / a0);
    lpfCoeffs.b1 = static_cast<float>((1.0 - cosOmega) / a0);
    lpfCoeffs.b2 = lpfCoeffs.b0;
    lpfCoeffs.a1 = static_cast<float>(-2.0 * cosOmega / a0);
    lpfCoeffs.a2 = static_cast<float>((1.0 - alpha) / a0);
}

float TunerEngine::processLPF(float sample)
{
    float out = lpfCoeffs.b0 * sample
              + lpfCoeffs.b1 * lpfState.x1
              + lpfCoeffs.b2 * lpfState.x2
              - lpfCoeffs.a1 * lpfState.y1
              - lpfCoeffs.a2 * lpfState.y2;

    lpfState.x2 = lpfState.x1;
    lpfState.x1 = sample;
    lpfState.y2 = lpfState.y1;
    lpfState.y1 = out;
    return out;
}
