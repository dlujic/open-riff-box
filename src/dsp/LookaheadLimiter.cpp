#include "LookaheadLimiter.h"

#include <cmath>

void LookaheadLimiter::prepare(double sampleRate, int /*samplesPerBlock*/, int numChannels)
{
    lookahead    = juce::jmax(1, (int) std::round(sampleRate * kLookaheadMs / 1000.0));
    releaseCoeff = (float) std::exp(-1.0 / (sampleRate * kReleaseMs / 1000.0));

    delayRing.assign((size_t) juce::jmax(1, numChannels),
                     std::vector<float>((size_t) lookahead, 0.0f));
    minQueueVal.assign((size_t) lookahead + 1, 1.0f);
    minQueueIdx.assign((size_t) lookahead + 1, 0);
    maRing.assign((size_t) lookahead, 1.0f);

    reset();
}

void LookaheadLimiter::reset()
{
    for (auto& ring : delayRing)
        std::fill(ring.begin(), ring.end(), 0.0f);
    std::fill(maRing.begin(), maRing.end(), 1.0f);

    maSum        = (double) maRing.size();
    maPos        = 0;
    delayPos     = 0;
    qHead        = 0;
    qTail        = 0;
    sampleIndex  = 0;
    releaseState = 1.0f;
    grDb.store(0.0f, std::memory_order_relaxed);
}

void LookaheadLimiter::process(juce::AudioBuffer<float>& buffer, bool apply)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(),
                                       (int) delayRing.size());
    if (numSamples == 0 || numChannels == 0)
        return;

    const juce::int64 cap = (juce::int64) minQueueVal.size();
    const double invLen = 1.0 / (double) lookahead;
    float minGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax(peak, std::abs(buffer.getReadPointer(ch)[i]));

        const float required = (peak > kCeiling) ? kCeiling / peak : 1.0f;

        // Release lives before the min window so a fast-rising tail can't
        // sneak past the lookahead.
        releaseState = (required < releaseState)
                           ? required
                           : releaseCoeff * releaseState
                                 + (1.0f - releaseCoeff) * required;

        while (qTail > qHead && minQueueVal[(size_t)((qTail - 1) % cap)] >= releaseState)
            --qTail;
        minQueueVal[(size_t)(qTail % cap)] = releaseState;
        minQueueIdx[(size_t)(qTail % cap)] = sampleIndex;
        ++qTail;
        while (minQueueIdx[(size_t)(qHead % cap)] <= sampleIndex - lookahead)
            ++qHead;
        const float windowMin = minQueueVal[(size_t)(qHead % cap)];

        maSum += (double) windowMin - (double) maRing[(size_t) maPos];
        maRing[(size_t) maPos] = windowMin;
        maPos = (maPos + 1 == lookahead) ? 0 : maPos + 1;
        const float gain = (float) (maSum * invLen);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            auto& ring = delayRing[(size_t) ch];
            const float delayed = ring[(size_t) delayPos];
            ring[(size_t) delayPos] = data[i];

            // jlimit guards the one boxcar sample the min window can miss;
            // it engages at dust level, never as a clipper.
            data[i] = apply ? juce::jlimit(-1.0f, 1.0f, delayed * gain)
                            : delayed;
        }
        delayPos = (delayPos + 1 == lookahead) ? 0 : delayPos + 1;

        ++sampleIndex;
        if (apply)
            minGain = juce::jmin(minGain, gain);
    }

    grDb.store(apply ? -juce::Decibels::gainToDecibels(juce::jmax(minGain, 1.0e-3f))
                     : 0.0f,
               std::memory_order_relaxed);
}
