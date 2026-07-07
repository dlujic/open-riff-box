#include "Wah.h"

#include <cmath>

namespace
{
    constexpr int kControlBlockSize = 32;   // coefficient recompute cadence (section 4)

    // GCB-95 fc/Q/gain vs wiper fraction, p = k/32 for k = 0..32. Baked from the Phase-1
    // MNA reference (tools/results/wah_bench/curves.csv) -- do not hand-edit without
    // re-checking against that file.
    constexpr int kNumCurvePoints = 33;

    constexpr float kWahFc[kNumCurvePoints] = {
        2201.847f, 1541.667f, 1253.094f, 1086.064f,  973.909f,  892.142f,  829.186f,
         778.904f,  737.312f,  702.497f,  672.462f,  646.524f,  623.603f,  603.196f,
         584.800f,  568.142f,  553.225f,  539.208f,  526.582f,  514.824f,  503.660f,
         493.386f,  483.807f,  474.780f,  466.233f,  458.116f,  450.385f,  442.999f,
         435.924f,  429.132f,  422.618f,  416.405f,  410.748f
    };
    constexpr float kWahQ[kNumCurvePoints] = {
         2.6789f,  3.8009f,  4.6367f,  5.3064f,  5.8677f,  6.3568f,  6.7915f,  7.1786f,
         7.5294f,  7.8541f,  8.1648f,  8.4429f,  8.7047f,  8.9548f,  9.1895f,  9.4036f,
         9.6303f,  9.8219f, 10.0467f, 10.2096f, 10.3889f, 10.6044f, 10.7981f, 10.9764f,
        11.1492f, 11.3216f, 11.4950f, 11.6687f, 11.8402f, 12.0062f, 12.1645f, 12.3177f,
        12.4983f
    };
    constexpr float kWahGainDb[kNumCurvePoints] = {
        18.570f, 18.536f, 18.562f, 18.598f, 18.634f, 18.669f, 18.703f, 18.730f,
        18.757f, 18.782f, 18.810f, 18.830f, 18.848f, 18.868f, 18.888f, 18.899f,
        18.919f, 18.930f, 18.952f, 18.958f, 18.970f, 18.993f, 19.012f, 19.027f,
        19.042f, 19.057f, 19.074f, 19.092f, 19.110f, 19.127f, 19.140f, 19.148f,
        19.167f
    };

    // GCB-95 is dominantly linear at guitar levels (NOTES section 6) -- at most a thin,
    // asymmetric even-harmonic warmth for the pushed case. Ear-tunable (fork #2).
    constexpr float kColorDrive = 1.0f;
    constexpr float kColorBias  = 0.20f;

    inline float shape(float x)
    {
        float y = std::tanh(kColorDrive * (x + kColorBias)) - std::tanh(kColorDrive * kColorBias);
        return y / kColorDrive;
    }
}

Wah::Wah()
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
}

void Wah::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    s1[0] = s1[1] = 0.0f;
    s2[0] = s2[1] = 0.0f;
    colorDc[0] = {};
    colorDc[1] = {};

    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));
    oversampling->reset();

    positionSmoothed.reset(sampleRate, 0.012);
    positionSmoothed.setCurrentAndTargetValue(positionParam);

    colorDcR = 1.0f - 2.0f * juce::MathConstants<float>::pi * 10.0f / static_cast<float>(sampleRate);
}

void Wah::reset()
{
    s1[0] = s1[1] = 0.0f;
    s2[0] = s2[1] = 0.0f;
    colorDc[0] = {};
    colorDc[1] = {};

    oversampling->reset();
    positionSmoothed.reset(currentSampleRate, 0.012);
}

int Wah::getLatencySamples() const
{
    return colorationOn ? static_cast<int>(oversampling->getLatencyInSamples()) : 0;
}

void Wah::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    positionSmoothed.setTargetValue(positionParam);

    const float fs = static_cast<float>(currentSampleRate);

    int i = 0;
    while (i < numSamples)
    {
        const int chunkLen = juce::jmin(kControlBlockSize, numSamples - i);

        const float ctrl = positionSmoothed.getNextValue();
        positionSmoothed.skip(chunkLen - 1);

        const float p = positionToWiper(ctrl);
        float fc = 0.0f, Q = 0.0f, gainDb = 0.0f;
        lookupCurves(p, fc, Q, gainDb);
        fc = juce::jlimit(20.0f, 0.45f * fs, fc);

        // ZDF/TPT resonant bandpass -- tan-prewarped cutoff, R = 1/(2Q) damping.
        // scale normalizes the bandpass's natural Q-peak to unity, then sets the
        // target gain: get this wrong (miss the /Q) and the peak overshoots by ~Q.
        const float g      = std::tan(juce::MathConstants<float>::pi * fc / fs);
        const float twoR   = 1.0f / Q;
        const float den    = 1.0f + twoR * g + g * g;
        const float aHP    = 1.0f / den;
        const float coefFB = twoR + g;
        const float scale  = std::pow(10.0f, gainDb / 20.0f) / Q;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);

            for (int n = 0; n < chunkLen; ++n)
            {
                const float x  = samples[i + n];
                const float hp = (x - coefFB * s1[ch] - s2[ch]) * aHP;
                const float bp = g * hp + s1[ch];
                s1[ch] = g * hp + bp;
                const float lp = g * bp + s2[ch];
                s2[ch] = g * bp + lp;
                samples[i + n] = bp * scale;
            }
        }

        i += chunkLen;
    }

    // Coloration: thin asymmetric waveshaper on the resonance-boosted output, oversampled
    // 4x + DC-blocked (asymmetry adds DC). Full wet -- a wah is an inline filter. Default
    // off; the filter alone is the faithful GCB-95 (NOTES section 6).
    if (colorationOn)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples  = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());

        for (int ch = 0; ch < osNumChannels; ++ch)
        {
            auto* samples = oversampledBlock.getChannelPointer(static_cast<size_t>(ch));
            for (int n = 0; n < osNumSamples; ++n)
                samples[n] = shape(samples[n]);
        }

        oversampling->processSamplesDown(block);

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);
            auto& dc = colorDc[ch];

            for (int n = 0; n < numSamples; ++n)
            {
                const float x = samples[n];
                const float y = x - dc.x1 + colorDcR * dc.y1;
                dc.x1 = x;
                dc.y1 = y;
                samples[n] = y;
            }
        }
    }
}

void Wah::resetToDefaults()
{
    setPosition(0.5f);
    setColoration(false);
    setTaperMode(0);
}

void Wah::setPosition(float value)   { positionParam = juce::jlimit(0.0f, 1.0f, value); }
void Wah::setColoration(bool on)     { colorationOn = on; }
void Wah::setTaperMode(int mode)     { taperMode = static_cast<TaperMode>(juce::jlimit(0, 1, mode)); }

float Wah::positionToWiper(float position) const
{
    position = juce::jlimit(0.0f, 1.0f, position);
    if (taperMode == TaperMode::Linear)
        return position;                       // raw electrical / validation: p == position

    // Hot Potz dead zones: ~15-20% mechanical dead travel at each end never rotates the
    // wiper to the pot extremes. Lands toe/mid/heel on ~1600/750/450 Hz (NOTES section 4).
    // Voicing map, not baked into the ground truth.
    return juce::jlimit(0.0f, 1.0f, 0.033f + 0.767f * std::pow(position, 1.95f));
}

void Wah::lookupCurves(float p, float& fc, float& Q, float& gainDb)
{
    p = juce::jlimit(0.0f, 1.0f, p);
    const float fp = p * static_cast<float>(kNumCurvePoints - 1);   // 0..32
    int   i0   = static_cast<int>(fp);
    if (i0 > kNumCurvePoints - 2) i0 = kNumCurvePoints - 2;
    const int   i1   = i0 + 1;
    const float frac = fp - static_cast<float>(i0);

    // log-interp fc (the curve bends hard toward the bare tank near p=0);
    // linear Q + gain (both smooth and near-linear between points).
    fc     = kWahFc[i0] * std::pow(kWahFc[i1] / kWahFc[i0], frac);
    Q      = kWahQ[i0]     + frac * (kWahQ[i1]     - kWahQ[i0]);
    gainDb = kWahGainDb[i0] + frac * (kWahGainDb[i1] - kWahGainDb[i0]);
}
