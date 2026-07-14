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
    const float kColorBiasTerm  = std::tanh(kColorDrive * kColorBias);

    inline float shape(float x)
    {
        return (std::tanh(kColorDrive * (x + kColorBias)) - kColorBiasTerm) / kColorDrive;
    }

    // jlimit does NOT clamp NaN -- every comparison against NaN is false, so it falls straight
    // through both branches (it does clamp +/-Inf correctly). A NaN parameter reaches
    // coeffsFor(), and a NaN coefficient poisons the SVF's persistent s1/s2 registers
    // PERMANENTLY: NaN is sticky through the recurrence, so it never washes out, and it spills
    // into every effect downstream in the chain. Only reset() clears it. The audio-sample path
    // is already guarded at the detector input; this closes the same hole on the parameter
    // path. A non-finite write is refused, leaving the parameter at its last good value.
    inline void storeParam01(std::atomic<float>& param, float value)
    {
        if (std::isfinite(value))
            param.store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_release);
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

    const float pos = getPosition();

    positionSmoothed.reset(sampleRate, 0.012);
    positionSmoothed.setCurrentAndTargetValue(pos);
    prevColorationOn = colorationOn.load(std::memory_order_acquire);

    colorDcR = 1.0f - 2.0f * juce::MathConstants<float>::pi * 10.0f / static_cast<float>(sampleRate);

    lfoPhase = 0.0f;
    envelope = 0.0f;
    lastCtrl = pos;
    prevMode = modeParam.load(std::memory_order_acquire);
    prevWave = autoWaveParam.load(std::memory_order_acquire);

    // Envelope sidechain HPF: kills LF pump so low notes don't ride the envelope
    // (mirrors NoiseGate's sidechainHPF idiom).
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate       = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    monoSpec.numChannels      = 1;

    for (auto& hpf : envHpf)
    {
        *hpf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 120.0f);
        hpf.prepare(monoSpec);
    }

    prevCoeffs = coeffsFor(pos);
}

void Wah::reset()
{
    s1[0] = s1[1] = 0.0f;
    s2[0] = s2[1] = 0.0f;
    colorDc[0] = {};
    colorDc[1] = {};

    oversampling->reset();
    prevColorationOn = colorationOn.load(std::memory_order_acquire);

    const float pos = getPosition();

    // SmoothedValue::reset collapses the current value onto the value that was ALREADY the
    // target, so it has to be re-anchored to the live position -- otherwise prevCoeffs (seeded
    // from the live position below) and the smoother start from two different places. Reachable:
    // EffectChain calls reset() on the bypass-off edge, so parking Position while bypassed and
    // then re-enabling lands here with a stale target. prepare() already does this.
    positionSmoothed.reset(currentSampleRate, 0.012);
    positionSmoothed.setCurrentAndTargetValue(pos);

    lfoPhase = 0.0f;
    envelope = 0.0f;
    lastCtrl = pos;
    prevMode = modeParam.load(std::memory_order_acquire);
    prevWave = autoWaveParam.load(std::memory_order_acquire);

    envHpf[0].reset();
    envHpf[1].reset();

    prevCoeffs = coeffsFor(pos);
}

int Wah::getLatencySamples() const
{
    return colorationOn.load(std::memory_order_acquire)
               ? static_cast<int>(oversampling->getLatencyInSamples()) : 0;
}

void Wah::process(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    const int mode = modeParam.load(std::memory_order_acquire);
    const int wave = autoWaveParam.load(std::memory_order_acquire);

    // A mode switch restarts the modulator; seeding the smoother with the control value we
    // last used keeps the handover continuous, since every modulator contributes zero at its
    // start (LFO phase 0, envelope 0). The waveform only feeds the LFO, so only a change to
    // it *while in Auto* counts -- a preset recall carries autoWave in every mode, and
    // firing this in Envelope would stomp a live follower.
    if (mode != prevMode || (mode == static_cast<int>(Mode::Auto) && wave != prevWave))
    {
        positionSmoothed.setCurrentAndTargetValue(lastCtrl);
        lfoPhase = 0.0f;
        envelope = 0.0f;
        envHpf[0].reset();
        envHpf[1].reset();
    }

    positionSmoothed.setTargetValue(getPosition());

    const float fs = static_cast<float>(currentSampleRate);

    // Snapshot each control-source parameter once per block -- they are written from the
    // message thread, and re-reading mid-block would let a knob move split a single buffer.
    const float rateHz   = autoRateToHz(getAutoRate());
    const float depth    = getAutoDepth();
    const float envGain  = envSensToGain(getEnvSens());
    const float attCoeff = std::exp(-1.0f / (envAttackToSec (getEnvAttack())  * fs));
    const float relCoeff = std::exp(-1.0f / (envReleaseToSec(getEnvRelease()) * fs));

    int i = 0;
    while (i < numSamples)
    {
        const int chunkLen = juce::jmin(kControlBlockSize, numSamples - i);

        // The detector needs the dry input; the SVF below overwrites the buffer in place.
        if (mode == static_cast<int>(Mode::Envelope))
        {
            const int detChannels = juce::jmin(numChannels, 2);
            const float* detPtr[2] = { buffer.getReadPointer(0),
                                        detChannels > 1 ? buffer.getReadPointer(1)
                                                         : buffer.getReadPointer(0) };

            for (int n = 0; n < chunkLen; ++n)
            {
                float det = 0.0f;
                for (int ch = 0; ch < detChannels; ++ch)
                {
                    // An inf from upstream makes the follower's own recurrence produce NaN
                    // (inf + -inf), and NaN passes straight through jlimit into the curve
                    // table index. Nothing non-finite gets past here.
                    const float x = detPtr[ch][i + n];
                    det = juce::jmax(det, std::abs(envHpf[ch].processSample(
                                              std::isfinite(x) ? x : 0.0f)));
                }

                const float c = (det > envelope) ? attCoeff : relCoeff;
                envelope = det + (envelope - det) * c;
            }
        }

        const float centre = positionSmoothed.getNextValue();
        positionSmoothed.skip(chunkLen - 1);

        float ctrl = centre;
        if (mode == static_cast<int>(Mode::Auto))
        {
            const float lfo = (wave == static_cast<int>(AutoWave::Triangle))
                                ? 1.0f - 4.0f * std::abs(std::fmod(lfoPhase + 0.25f, 1.0f) - 0.5f)
                                : std::sin(juce::MathConstants<float>::twoPi * lfoPhase);

            ctrl = juce::jlimit(0.0f, 1.0f, centre + 0.5f * depth * lfo);

            lfoPhase += rateHz * static_cast<float>(chunkLen) / fs;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        }
        else if (mode == static_cast<int>(Mode::Envelope))
        {
            // Position 0 is the toe (bright, 2.2 kHz); a pick attack has to pull the control
            // DOWN to sweep the filter up. The knob is the resting/dark end of the sweep.
            ctrl = juce::jlimit(0.0f, 1.0f, centre - envGain * envelope);
        }
        lastCtrl = ctrl;

        const SvfCoeffs target = coeffsFor(ctrl);

        // Ramp to the target across the block instead of stepping at the boundary. A 1 ms
        // envelope attack can slam the control from centre to the rail inside a single block,
        // and scale is a direct output multiplier -- a step there is a click, not a sweep.
        // At a static position target == prevCoeffs, so every delta is exactly 0 and this is
        // bit-identical to holding the coefficients (the Manual regression anchor).
        const float inv    = 1.0f / static_cast<float>(chunkLen);
        const float dG     = (target.g      - prevCoeffs.g)      * inv;
        const float dAHP   = (target.aHP    - prevCoeffs.aHP)    * inv;
        const float dFB    = (target.coefFB - prevCoeffs.coefFB) * inv;
        const float dScale = (target.scale  - prevCoeffs.scale)  * inv;

        for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);

            float g      = prevCoeffs.g;
            float aHP    = prevCoeffs.aHP;
            float coefFB = prevCoeffs.coefFB;
            float scale  = prevCoeffs.scale;

            for (int n = 0; n < chunkLen; ++n)
            {
                g += dG; aHP += dAHP; coefFB += dFB; scale += dScale;

                const float x  = samples[i + n];
                const float hp = (x - coefFB * s1[ch] - s2[ch]) * aHP;
                const float bp = g * hp + s1[ch];
                s1[ch] = g * hp + bp;
                const float lp = g * bp + s2[ch];
                s2[ch] = g * bp + lp;
                samples[i + n] = bp * scale;
            }
        }

        prevCoeffs = target;

        i += chunkLen;
    }

    // Coloration: thin asymmetric waveshaper on the resonance-boosted output, oversampled
    // 4x + DC-blocked (asymmetry adds DC). Full wet -- a wah is an inline filter. Default
    // off; the filter alone is the faithful GCB-95 (NOTES section 6).
    const bool colorOn = colorationOn.load(std::memory_order_acquire);
    if (colorOn)
    {
        // stale oversampler/DC state from an earlier coloration stint clicks on re-enable
        if (!prevColorationOn)
        {
            oversampling->reset();
            colorDc[0] = {};
            colorDc[1] = {};
        }

        juce::dsp::AudioBlock<float> block(buffer);
        auto oversampledBlock = oversampling->processSamplesUp(block);

        const auto osNumSamples  = static_cast<int>(oversampledBlock.getNumSamples());
        const auto osNumChannels = static_cast<int>(oversampledBlock.getNumChannels());

        for (int ch = 0; ch < juce::jmin(osNumChannels, 2); ++ch)
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
    prevMode = mode;
    prevWave = wave;
    prevColorationOn = colorOn;
}

void Wah::resetToDefaults()
{
    setPosition(0.5f);
    setColoration(false);
    setTaperMode(0);
    setMode(0);
    setAutoWave(0);
    setAutoRate(0.45f);
    setAutoDepth(0.5f);
    setEnvSens(0.5f);
    setEnvAttack(0.43f);
    setEnvRelease(0.52f);
}

void Wah::setPosition(float value)   { storeParam01(positionParam, value); }
void Wah::setColoration(bool on)     { colorationOn.store(on, std::memory_order_release); }
void Wah::setTaperMode(int mode)     { taperMode.store(juce::jlimit(0, 1, mode), std::memory_order_release); }

void Wah::setMode(int value)         { modeParam.store(juce::jlimit(0, 2, value), std::memory_order_release); }
void Wah::setAutoWave(int value)     { autoWaveParam.store(juce::jlimit(0, 1, value), std::memory_order_release); }
void Wah::setAutoRate(float value)   { storeParam01(autoRateParam,   value); }
void Wah::setAutoDepth(float value)  { storeParam01(autoDepthParam,  value); }
void Wah::setEnvSens(float value)    { storeParam01(envSensParam,    value); }
void Wah::setEnvAttack(float value)  { storeParam01(envAttackParam,  value); }
void Wah::setEnvRelease(float value) { storeParam01(envReleaseParam, value); }

float Wah::positionToWiper(float position) const
{
    position = juce::jlimit(0.0f, 1.0f, position);
    if (taperMode.load(std::memory_order_acquire) == static_cast<int>(TaperMode::Linear))
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
    // Clamp both ends. A non-finite p casts to INT_MIN, which a top-only clamp waves through
    // and which then indexes gigabytes below the table.
    const int   i0   = juce::jlimit(0, kNumCurvePoints - 2, static_cast<int>(fp));
    const int   i1   = i0 + 1;
    const float frac = fp - static_cast<float>(i0);

    // log-interp fc (the curve bends hard toward the bare tank near p=0);
    // linear Q + gain (both smooth and near-linear between points).
    fc     = kWahFc[i0] * std::pow(kWahFc[i1] / kWahFc[i0], frac);
    Q      = kWahQ[i0]     + frac * (kWahQ[i1]     - kWahQ[i0]);
    gainDb = kWahGainDb[i0] + frac * (kWahGainDb[i1] - kWahGainDb[i0]);
}

Wah::SvfCoeffs Wah::coeffsFor(float ctrl) const
{
    const float fs = static_cast<float>(currentSampleRate);

    const float p = positionToWiper(ctrl);
    float fc = 0.0f, Q = 0.0f, gainDb = 0.0f;
    lookupCurves(p, fc, Q, gainDb);
    fc = juce::jlimit(20.0f, 0.45f * fs, fc);

    // ZDF/TPT resonant bandpass -- tan-prewarped cutoff, R = 1/(2Q) damping.
    // scale normalizes the bandpass's natural Q-peak to unity, then sets the
    // target gain: get this wrong (miss the /Q) and the peak overshoots by ~Q.
    SvfCoeffs c;
    c.g              = std::tan(juce::MathConstants<float>::pi * fc / fs);
    const float twoR = 1.0f / Q;
    const float den  = 1.0f + twoR * c.g + c.g * c.g;
    c.aHP    = 1.0f / den;
    c.coefFB = twoR + c.g;
    c.scale  = std::pow(10.0f, gainDb / 20.0f) / Q;
    return c;
}
