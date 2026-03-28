#include "WaveshaperEngine.h"

//==============================================================================
// Built-in polynomial coefficient presets
//==============================================================================

namespace WaveshaperCoefficients
{
    // Designed for POSITIVE input only via sign-fold: y = copysign(poly(|x|), x).
    // c1 ~ 9 bakes gain into the polynomial. Saturates to +1.0 at x ~ 0.5.
    // Flat ceiling from x = 0.85 to 1.2 with gentle rollover above (self-correcting
    // for feedback overshoot). No separate gain stage or tanh limiter needed.
    static constexpr float SignFoldSigmoid[11] = {
        8.11044e-06f,    // c0: DC offset (essentially zero)
        8.943665402f,    // c1: ~9x small-signal gain (baked in)
        -36.83889529f,   // c2
        92.01697887f,    // c3
        -154.337906f,    // c4
        181.6233289f,    // c5
        -151.8651235f,   // c6
        89.09614114f,    // c7
        -35.10298511f,   // c8
        8.388101016f,    // c9
        -0.923313471f    // c10
    };

    // --- Legacy presets (bipolar input, unity c1, require external gain + tanh limiter) ---

    // Feedback-stable sigmoid: smooth saturation with nonzero derivative
    // at the clipping rails. Unlike sin Taylor (whose derivative drops to
    // zero at pi/2, causing per-cycle gain modulation in feedback loops),
    // this polynomial maintains derivative > 0.30 throughout [-pi/2, pi/2].
    // Harmonic spectrum similar to sin Taylor but with stable feedback behavior.
    static constexpr float Default[11] = {
        0.0f,          // c0: no DC offset
        1.0f,          // c1: unity small-signal gain
        0.0f,          // c2: symmetric (no even harmonics)
        -0.2f,         // c3: slightly harder knee than sin Taylor (-1/6)
        0.0f,          // c4: symmetric
        0.032f,        // c5: tuned to keep derivative > 0.1 at pi/2
        0.0f,          // c6: symmetric
        -0.0018f,      // c7: truncated for smoother saturation
        0.0f,          // c8: symmetric
        0.0f,          // c9: truncated (sin Taylor's c9 was negligible)
        0.0f           // c10: symmetric
    };

    // Asymmetric tube-inspired soft clip. Even-order terms (c2, c4)
    // produce 2nd and 4th harmonics - the signature of tube warmth.
    // A triode's transfer curve is asymmetric because the operating
    // point sits off-center on the plate characteristic. Odd terms
    // kept gentle (smaller than Default) so even harmonics dominate
    // the saturation character. Gentlest knee of the three.
    static constexpr float Warm[11] = {
        0.0f,          // c0: no DC offset
        0.97f,         // c1: slightly below unity - warmer feel
        0.08f,         // c2: 2nd harmonic (asymmetric warmth)
        -0.12f,        // c3: gentle cubic (~72% of sin Taylor)
        -0.03f,        // c4: 4th harmonic (subtle asymmetry)
        0.004f,        // c5: suppressed 5th harmonic
        0.0f,          // c6
        -0.00006f,     // c7: suppressed 7th harmonic
        0.0f,          // c8
        0.0f,          // c9: 9th suppressed entirely
        0.0f           // c10
    };

    // tanh(x) Taylor series: hard knee, symmetric, punchy.
    // Transitions to saturation roughly 2x faster than sin(x)
    // (c3 = -1/3 vs -1/6). Higher-order terms are proportionally
    // larger too, producing richer harmonic content across the
    // spectrum. Solid-state / tight distortion character.
    static constexpr float Aggressive[11] = {
        0.0f,          // c0: no DC offset
        1.0f,          // c1: unity small-signal gain
        0.0f,          // c2: symmetric (no even harmonics)
        -0.333333f,    // c3: -1/3 (tanh Taylor, 2x sin's cubic)
        0.0f,          // c4: symmetric
        0.133333f,     // c5: 2/15 (16x sin's quintic)
        0.0f,          // c6: symmetric
        -0.053968f,    // c7: -17/315
        0.0f,          // c8: symmetric
        0.021869f,     // c9: 62/2835
        0.0f           // c10: symmetric
    };

    // Custom Chebyshev-basis sign-fold sigmoid.
    // Designed with constrained optimization: f(0)=0 exact, f'(0)=9 (baked gain),
    // f(1)=1 (saturation), f'(1)=0 (flat ceiling), monotonic on [0, 1.2].
    // Upper harmonics (T7-T10) penalized for warmth and reduced intermod on chords.
    // Evaluated via Clenshaw algorithm with domain mapping: u = 2*x/1.3 - 1.
    static constexpr float SignFoldChebyshev[11] = {
        +0.8386744007f,  // a0: T_0(u)
        +0.2690902914f,  // a1: T_1(u)
        -0.2472393069f,  // a2: T_2(u)
        +0.1478106528f,  // a3: T_3(u)
        -0.1198327633f,  // a4: T_4(u)
        +0.0349753804f,  // a5: T_5(u)
        -0.0309396584f,  // a6: T_6(u)
        -0.0092157426f,  // a7: T_7(u) - suppressed for warmth
        -0.0033448661f,  // a8: T_8(u) - suppressed for warmth
        -0.0067314603f,  // a9: T_9(u) - suppressed for warmth
        -0.0013886842f   // a10: T_10(u) - suppressed for warmth
    };

    // Character/glue stage Chebyshev polynomial (gain=5, warmth=0.6).
    // Mid-tier between preamp (9x, aggressive) and power amp (3x, gentle).
    // At inputGain=0.3: low preamp gain → poly input 0.15, output ~0.75
    // (entering knee, adds color). High preamp gain → poly input 0.42,
    // output ~0.99 (soft compression, not hard clip). Contributes harmonic
    // complexity without destroying the preamp's waveform shape.
    static constexpr float SignFoldCharacter[11] = {
        +0.7766711550f,  // a0: T_0(u)
        +0.3494050976f,  // a1: T_1(u)
        -0.2638157628f,  // a2: T_2(u)
        +0.0956199218f,  // a3: T_3(u)
        -0.0790389691f,  // a4: T_4(u)
        -0.0229137827f,  // a5: T_5(u)
        -0.0256385246f,  // a6: T_6(u)
        -0.0218408243f,  // a7: T_7(u) - suppressed for warmth
        -0.0156326988f,  // a8: T_8(u) - suppressed for warmth
        -0.0111692464f,  // a9: T_9(u) - suppressed for warmth
        -0.0034440336f   // a10: T_10(u) - suppressed for warmth
    };

    // Power amp tube Chebyshev polynomial (gain=3, warmth=0.7).
    // Models 6L6/EL34-style power tube: low gain (3x vs 9x preamp),
    // soft saturation knee, gradual compression. A real power tube
    // has gain ~2-5x and compresses gently, unlike a preamp tube
    // which hard-clips. The softer knee produces less intermodulation
    // on power chords and lets the preamp's distortion character through.
    static constexpr float SignFoldPowerAmp[11] = {
        +0.7276788288f,  // a0: T_0(u)
        +0.4233730133f,  // a1: T_1(u)
        -0.2435705666f,  // a2: T_2(u)
        +0.0510612485f,  // a3: T_3(u)
        -0.0224322592f,  // a4: T_4(u)
        -0.0229230393f,  // a5: T_5(u)
        -0.0168193619f,  // a6: T_6(u)
        -0.0113215970f,  // a7: T_7(u) - suppressed for warmth
        -0.0094404790f,  // a8: T_8(u) - suppressed for warmth
        -0.0068089599f,  // a9: T_9(u) - suppressed for warmth
        -0.0020354964f   // a10: T_10(u) - suppressed for warmth
    };
}

//==============================================================================
// Construction
//==============================================================================

WaveshaperEngine::WaveshaperEngine()
{
    setCoefficients(CoefficientPreset::Default);

    postFilterLow.frequency  = 8000.0f;  postFilterLow.q  = 0.5f;   postFilterLow.enabled  = true;
    postFilterMid.frequency  = 5500.0f;  postFilterMid.q  = 0.38f;  postFilterMid.enabled  = true;
    postFilterHigh.frequency = 3500.0f;  postFilterHigh.q = 0.5f;   postFilterHigh.enabled = true;
}

//==============================================================================
// Lifecycle
//==============================================================================

void WaveshaperEngine::prepare(double sampleRate, int maxBlockSize)
{
    baseSampleRate = sampleRate;

    // 4x oversampling, mono, IIR half-band filters
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        1,          // mono (caller handles stereo by using two engines)
        2,          // 2 stages = 4x oversampling
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true        // max quality
    );
    oversampling->initProcessing(static_cast<size_t>(maxBlockSize));

    oversampledRate = sampleRate * 4.0;

    // Smoothed params: 20ms ramp at OVERSAMPLED rate, since getNextValue()
    // is called per oversampled sample in processOversampledBlock.
    gainSmoothed.reset(oversampledRate, 0.02);
    preShapeSmoothed.reset(oversampledRate, 0.02);
    feedbackSmoothed.reset(oversampledRate, 0.02);
    dryWetSmoothed.reset(oversampledRate, 0.02);

    // Set initial smoothed values
    gainSmoothed.setCurrentAndTargetValue(gainSmoothed.getTargetValue());
    preShapeSmoothed.setCurrentAndTargetValue(preShapeSmoothed.getTargetValue());
    feedbackSmoothed.setCurrentAndTargetValue(feedbackSmoothed.getTargetValue());
    dryWetSmoothed.setCurrentAndTargetValue(dryWetSmoothed.getTargetValue());

    // Modulation phase increment at oversampled rate
    modPhaseInc = juce::MathConstants<float>::twoPi * modRateHz / static_cast<float>(oversampledRate);

    // Feedback read LPF: ~10 kHz cutoff smooths per-sample gain modulation
    feedbackLpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 10000.0f
                                        / static_cast<float>(oversampledRate));

    // Prepare post-filter bands at oversampled rate
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = oversampledRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize * 4);
    monoSpec.numChannels = 1;

    postFilterLow.filter.prepare(monoSpec);
    postFilterMid.filter.prepare(monoSpec);
    postFilterHigh.filter.prepare(monoSpec);

    postFilterLow.needsUpdate  = true;
    postFilterMid.needsUpdate  = true;
    postFilterHigh.needsUpdate = true;
    updatePostFilterCoefficients();

    reset();
}

void WaveshaperEngine::prepareRaw(double sampleRate, int maxBlockSize)
{
    baseSampleRate = sampleRate;
    oversampledRate = sampleRate;  // caller provides the actual (oversampled) rate
    oversampling.reset();          // no internal oversampler

    // Smoothed params: 20ms ramp at the provided rate
    gainSmoothed.reset(sampleRate, 0.02);
    preShapeSmoothed.reset(sampleRate, 0.02);
    feedbackSmoothed.reset(sampleRate, 0.02);
    dryWetSmoothed.reset(sampleRate, 0.02);

    // Set initial smoothed values
    gainSmoothed.setCurrentAndTargetValue(gainSmoothed.getTargetValue());
    preShapeSmoothed.setCurrentAndTargetValue(preShapeSmoothed.getTargetValue());
    feedbackSmoothed.setCurrentAndTargetValue(feedbackSmoothed.getTargetValue());
    dryWetSmoothed.setCurrentAndTargetValue(dryWetSmoothed.getTargetValue());

    // Modulation phase increment at provided rate
    modPhaseInc = juce::MathConstants<float>::twoPi * modRateHz / static_cast<float>(sampleRate);

    // Feedback read LPF: ~10 kHz cutoff smooths per-sample gain modulation
    feedbackLpfCoeff = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * 10000.0f
                                        / static_cast<float>(sampleRate));

    // Prepare post-filter bands at provided rate
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = static_cast<juce::uint32>(maxBlockSize);
    monoSpec.numChannels = 1;

    postFilterLow.filter.prepare(monoSpec);
    postFilterMid.filter.prepare(monoSpec);
    postFilterHigh.filter.prepare(monoSpec);

    postFilterLow.needsUpdate  = true;
    postFilterMid.needsUpdate  = true;
    postFilterHigh.needsUpdate = true;
    updatePostFilterCoefficients();

    reset();
}

void WaveshaperEngine::reset()
{
    // Clear feedback buffer
    std::fill(std::begin(feedbackBuffer), std::end(feedbackBuffer), 0.0f);
    writePos = 0;
    modPhase = 0.0f;

    // Reset feedback LPF
    feedbackLpfState = 0.0f;

    // Reset preShape cache
    cachedPreShapeIntensity = -1.0f;
    cachedPreShapeNorm = 1.0f;
    cachedPreShapeScale = 0.0f;

    // Reset filters
    postFilterLow.filter.reset();
    postFilterMid.filter.reset();
    postFilterHigh.filter.reset();

    // Reset oversampling
    if (oversampling)
        oversampling->reset();
}

int WaveshaperEngine::getLatencySamples() const
{
    if (oversampling)
        return static_cast<int>(oversampling->getLatencyInSamples());
    return 0;
}

//==============================================================================
// Coefficient setters
//==============================================================================

void WaveshaperEngine::setCoefficients(const float (&coeffs)[11])
{
    std::copy(std::begin(coeffs), std::end(coeffs), std::begin(coefficients));
}

void WaveshaperEngine::setCoefficients(CoefficientPreset preset)
{
    useChebyshevBasis = false;  // default to power basis
    switch (preset)
    {
        case CoefficientPreset::Default:         setCoefficients(WaveshaperCoefficients::Default);         break;
        case CoefficientPreset::Warm:            setCoefficients(WaveshaperCoefficients::Warm);            break;
        case CoefficientPreset::Aggressive:      setCoefficients(WaveshaperCoefficients::Aggressive);      break;
        case CoefficientPreset::SignFoldSigmoid:  setCoefficients(WaveshaperCoefficients::SignFoldSigmoid);  break;
        case CoefficientPreset::SignFoldChebyshev:
            setCoefficients(WaveshaperCoefficients::SignFoldChebyshev);
            useChebyshevBasis = true;
            break;
        case CoefficientPreset::SignFoldCharacter:
            setCoefficients(WaveshaperCoefficients::SignFoldCharacter);
            useChebyshevBasis = true;
            break;
        case CoefficientPreset::SignFoldPowerAmp:
            setCoefficients(WaveshaperCoefficients::SignFoldPowerAmp);
            useChebyshevBasis = true;
            break;
    }
}

void WaveshaperEngine::setSignFold(bool enabled)
{
    signFoldEnabled = enabled;
}

//==============================================================================
// Parameter setters
//==============================================================================

void WaveshaperEngine::setInputGain(float linearGain)
{
    gainSmoothed.setTargetValue(linearGain);
}

void WaveshaperEngine::setPreShapeIntensity(float amount)
{
    // Capped at 0.85 to keep preShape output bounded within the polynomial's
    // well-behaved region. At 0.9+, 1/cos(0.9*pi/2) > 6.4, which can push
    // the polynomial past its convergence range and break feedback stability.
    preShapeSmoothed.setTargetValue(juce::jlimit(0.0f, 0.85f, amount));
}

void WaveshaperEngine::setFeedbackAmount(float amount)
{
    feedbackSmoothed.setTargetValue(juce::jlimit(0.0f, 0.95f, amount));
}

void WaveshaperEngine::setModulationRate(float hz)
{
    modRateHz = hz;
    modPhaseInc = juce::MathConstants<float>::twoPi * hz / static_cast<float>(oversampledRate);
}

void WaveshaperEngine::setModulationDepth(float samples)
{
    modDepth = samples;
}

void WaveshaperEngine::setFeedbackDelayLength(int samples)
{
    feedbackDelayLength = juce::jlimit(2, kFeedbackBufferSize, samples);
}

void WaveshaperEngine::setDryWetMix(float wet)
{
    dryWetSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, wet));
}

void WaveshaperEngine::setPostFilterLow(float freqHz, float q)
{
    postFilterLow.frequency = freqHz;
    postFilterLow.q = q;
    postFilterLow.needsUpdate = true;
}

void WaveshaperEngine::setPostFilterMid(float freqHz, float q)
{
    postFilterMid.frequency = freqHz;
    postFilterMid.q = q;
    postFilterMid.needsUpdate = true;
}

void WaveshaperEngine::setPostFilterHigh(float freqHz, float q)
{
    postFilterHigh.frequency = freqHz;
    postFilterHigh.q = q;
    postFilterHigh.needsUpdate = true;
}

void WaveshaperEngine::setPostFilterLowEnabled(bool enabled)  { postFilterLow.enabled = enabled; }
void WaveshaperEngine::setPostFilterMidEnabled(bool enabled)   { postFilterMid.enabled = enabled; }
void WaveshaperEngine::setPostFilterHighEnabled(bool enabled)  { postFilterHigh.enabled = enabled; }

//==============================================================================
// Post-filter coefficient update
//==============================================================================

void WaveshaperEngine::updatePostFilterCoefficients()
{
    auto updateBand = [this](PostFilterBand& band)
    {
        if (!band.needsUpdate) return;

        float freq = juce::jlimit(20.0f, static_cast<float>(oversampledRate * 0.49), band.frequency);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(oversampledRate, freq, band.q);
        *band.filter.coefficients = *coeffs;
        band.needsUpdate = false;
    };

    updateBand(postFilterLow);
    updateBand(postFilterMid);
    updateBand(postFilterHigh);
}

//==============================================================================
// Catmull-Rom (Hermite) interpolation
//==============================================================================

float WaveshaperEngine::hermiteInterpolate(const float* buffer, float position, int bufferSize)
{
    // Wrap position into valid range
    while (position < 0.0f) position += static_cast<float>(bufferSize);
    while (position >= static_cast<float>(bufferSize)) position -= static_cast<float>(bufferSize);

    int idx1 = static_cast<int>(position);
    float frac = position - static_cast<float>(idx1);

    // Four sample points for cubic interpolation
    int idx0 = (idx1 - 1 + bufferSize) % bufferSize;
    idx1 = idx1 % bufferSize;
    int idx2 = (idx1 + 1) % bufferSize;
    int idx3 = (idx1 + 2) % bufferSize;

    float y0 = buffer[idx0];
    float y1 = buffer[idx1];
    float y2 = buffer[idx2];
    float y3 = buffer[idx3];

    // Catmull-Rom spline coefficients
    float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c = -0.5f * y0 + 0.5f * y2;
    float d = y1;

    return ((a * frac + b) * frac + c) * frac + d;
}

//==============================================================================
// Core processing
//==============================================================================

void WaveshaperEngine::process(float* samples, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    if (oversampling == nullptr) return;

    // Update post-filter coefficients if any changed
    updatePostFilterCoefficients();

    // Wrap input in a single-channel AudioBlock for the oversampler
    juce::AudioBuffer<float> monoBuffer(&samples, 1, numSamples);
    juce::dsp::AudioBlock<float> inputBlock(monoBuffer);

    // Upsample
    auto oversampledBlock = oversampling->processSamplesUp(inputBlock);
    auto* oversampledData = oversampledBlock.getChannelPointer(0);
    int oversampledLength = static_cast<int>(oversampledBlock.getNumSamples());

    // Process the core loop at oversampled rate
    processOversampledBlock(oversampledData, oversampledLength);

    // Downsample back into the original buffer
    oversampling->processSamplesDown(inputBlock);
}

void WaveshaperEngine::processRaw(float* samples, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;

    updatePostFilterCoefficients();
    processOversampledBlock(samples, numSamples);
}

float WaveshaperEngine::processSample(float input)
{
    // Advance smoothed parameters
    float gain         = gainSmoothed.getNextValue();
    float preShapeAmt  = preShapeSmoothed.getNextValue();
    float feedback     = feedbackSmoothed.getNextValue();
    float wet          = dryWetSmoothed.getNextValue();
    float dry          = 1.0f - wet;

    updatePreShapeCache(preShapeAmt);

    // 1. Read from feedback buffer with cosine-modulated position
    float modOffset = std::cos(modPhase) * modDepth;
    float readPos = static_cast<float>(writePos) - static_cast<float>(feedbackDelayLength) + modOffset;
    if (readPos < 0.0f) readPos += static_cast<float>(kFeedbackBufferSize);
    float delayed = hermiteInterpolate(feedbackBuffer, readPos, kFeedbackBufferSize);

    delayed = feedbackLpfState + feedbackLpfCoeff * (delayed - feedbackLpfState);
    feedbackLpfState = delayed;

    // 2. Mix input with feedback
    float signal = input * gain + delayed * feedback;

    if (signFoldEnabled)
    {
        // 3. Pre-shape on full (signed) signal
        signal = preShape(signal);

        // 4. Sign-fold + polynomial (with Chebyshev domain clamp)
        float sign = (signal >= 0.0f) ? 1.0f : -1.0f;
        float absSignal = std::fabs(signal);
        if (useChebyshevBasis)
            absSignal = juce::jmin(absSignal, kChebXMax);
        signal = sign * evaluatePolynomial(absSignal);
    }
    else
    {
        static constexpr float kPreShapeMaxInput = juce::MathConstants<float>::halfPi;
        signal = std::tanh(signal / kPreShapeMaxInput) * kPreShapeMaxInput;
        signal = preShape(signal);
        signal = evaluatePolynomial(signal);
    }

    // 6. Write to feedback buffer BEFORE post-filter
    feedbackBuffer[writePos] = signal;
    writePos = (writePos + 1) % kFeedbackBufferSize;

    // 7. Post-filter
    if (postFilterLow.enabled)
        signal = postFilterLow.filter.processSample(signal);
    if (postFilterMid.enabled)
        signal = postFilterMid.filter.processSample(signal);
    if (postFilterHigh.enabled)
        signal = postFilterHigh.filter.processSample(signal);

    // 8. Dry/wet mix
    float output = input * dry + signal * wet;

    // Advance modulation phase
    modPhase += modPhaseInc;
    if (modPhase >= juce::MathConstants<float>::twoPi)
        modPhase -= juce::MathConstants<float>::twoPi;

    return output;
}

void WaveshaperEngine::processOversampledBlock(float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Advance smoothed parameters
        float gain         = gainSmoothed.getNextValue();
        float preShapeAmt  = preShapeSmoothed.getNextValue();
        float feedback     = feedbackSmoothed.getNextValue();
        float wet          = dryWetSmoothed.getNextValue();
        float dry          = 1.0f - wet;

        // Update cached cos() denominator only when intensity changes
        updatePreShapeCache(preShapeAmt);

        float inputSample = samples[i];

        //----------------------------------------------------------------------
        // 1. Read from feedback buffer with cosine-modulated position
        //----------------------------------------------------------------------
        float modOffset = std::cos(modPhase) * modDepth;
        float readPos = static_cast<float>(writePos) - static_cast<float>(feedbackDelayLength) + modOffset;
        if (readPos < 0.0f) readPos += static_cast<float>(kFeedbackBufferSize);
        float delayed = hermiteInterpolate(feedbackBuffer, readPos, kFeedbackBufferSize);

        // One-pole LPF on feedback read: smooths per-sample gain modulation
        // from the polynomial's varying derivative across the waveform cycle.
        delayed = feedbackLpfState + feedbackLpfCoeff * (delayed - feedbackLpfState);
        feedbackLpfState = delayed;

        //----------------------------------------------------------------------
        // 2. Mix input with feedback
        //----------------------------------------------------------------------
        float signal = inputSample * gain + delayed * feedback;

        if (signFoldEnabled)
        {
            //------------------------------------------------------------------
            // Sign-fold path: polynomial handles positive input only.
            // Output bounded to [-1, +1] by the polynomial's built-in sigmoid.
            // No tanh limiter needed - c1 ~ 9 provides gain, polynomial saturates
            // naturally. Even harmonics come from topology (bias, push-pull),
            // not polynomial coefficients.
            //------------------------------------------------------------------

            // 3. Pre-shape on full (signed) signal
            signal = preShape(signal);

            // 4. Sign-fold + polynomial: poly sees |x|, sign is restored after
            float sign = (signal >= 0.0f) ? 1.0f : -1.0f;
            float absSignal = std::fabs(signal);
            // Clamp to Chebyshev domain boundary: the polynomial diverges
            // rapidly for |x| > kChebXMax (1.3). The plateau ceiling is
            // already reached at x ~ 0.85, so this clamp is inaudible
            // under normal conditions but prevents output spikes on
            // transient peaks (e.g. palm mutes with boost + hot pickups).
            if (useChebyshevBasis)
                absSignal = juce::jmin(absSignal, kChebXMax);
            signal = sign * evaluatePolynomial(absSignal);
        }
        else
        {
            //------------------------------------------------------------------
            // Legacy path: tanh limiter + bipolar polynomial (for old presets)
            //------------------------------------------------------------------
            static constexpr float kPreShapeMaxInput = juce::MathConstants<float>::halfPi;
            signal = std::tanh(signal / kPreShapeMaxInput) * kPreShapeMaxInput;

            // 3. Pre-shape
            signal = preShape(signal);

            // 4. Polynomial (bipolar)
            signal = evaluatePolynomial(signal);
        }

        //----------------------------------------------------------------------
        // 5. [Hook point: future GRU / nonlinear post-processor goes here]
        //----------------------------------------------------------------------

        //----------------------------------------------------------------------
        // 6. Write to feedback buffer BEFORE post-filter
        //----------------------------------------------------------------------
        feedbackBuffer[writePos] = signal;
        writePos = (writePos + 1) % kFeedbackBufferSize;

        //----------------------------------------------------------------------
        // 7. Post-filter (3-band cascaded LPFs, output only)
        //----------------------------------------------------------------------
        if (postFilterLow.enabled)
            signal = postFilterLow.filter.processSample(signal);
        if (postFilterMid.enabled)
            signal = postFilterMid.filter.processSample(signal);
        if (postFilterHigh.enabled)
            signal = postFilterHigh.filter.processSample(signal);

        //----------------------------------------------------------------------
        // 8. Dry/wet mix
        //----------------------------------------------------------------------
        samples[i] = inputSample * dry + signal * wet;

        //----------------------------------------------------------------------
        // Advance modulation phase
        //----------------------------------------------------------------------
        modPhase += modPhaseInc;
        if (modPhase >= juce::MathConstants<float>::twoPi)
            modPhase -= juce::MathConstants<float>::twoPi;
    }
}
