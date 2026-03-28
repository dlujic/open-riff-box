#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Reusable waveshaper building block for amp sim stages, distortion effects, etc.
//
// Signal flow (per sample):
//   1. Read from feedback delay buffer (Catmull-Rom interpolated, cosine micro-modulated)
//   2. Mix input with feedback read
//   3. Pre-shape (sinusoidal soft-clip)
//   4. Sign-fold + polynomial waveshaper (10th-order Horner evaluation)
//      Sign-fold mode: y = copysign(poly(|x|), x) - polynomial sees positive input only,
//      output bounded to [-1, +1]. Gain is baked into c1 (~9x). No tanh limiter needed.
//   5. [Post-polynomial hook point for future GRU/nonlinear refinement]
//   6. Write to feedback buffer (full bandwidth - BEFORE post-filter)
//   7. Post-filter (3-band cut: low, mid, high LPFs for fizz control)
//   8. Dry/wet mix
//
// Oversampling (4x) wraps the entire loop internally.
class WaveshaperEngine
{
public:
    WaveshaperEngine();
    ~WaveshaperEngine() = default;

    //===========================================================================
    // Lifecycle
    //===========================================================================

    // Call before first use or when sample rate / block size changes.
    // sampleRate is the BASE rate (e.g. 44100) - oversampling is handled internally.
    void prepare(double sampleRate, int maxBlockSize);

    // Prepare for use with EXTERNAL oversampling (e.g. AmpSimGold owns a shared
    // oversampler). sampleRate should be the OVERSAMPLED rate. No internal
    // oversampler is created; use processRaw() instead of process().
    void prepareRaw(double sampleRate, int maxBlockSize);

    // Process a mono buffer in-place. For stereo, call once per channel.
    // Uses internal 4x oversampling. For external oversampling, use processRaw().
    void process(float* samples, int numSamples);

    // Process a mono buffer in-place WITHOUT internal oversampling.
    // Caller is responsible for providing samples at the correct (oversampled) rate.
    // Must be paired with prepareRaw(), not prepare().
    void processRaw(float* samples, int numSamples);

    // Process a single sample in-place WITHOUT internal oversampling.
    // Does exactly what one iteration of processOversampledBlock() does.
    // Must be paired with prepareRaw(). Does NOT call updateCoefficients.
    float processSample(float input);

    // Clear all internal state (delay buffer, filter history, smoothers).
    void reset();

    // Latency in samples introduced by the 4x oversampling.
    // Returns 0 when prepared with prepareRaw() (no internal oversampling).
    int getLatencySamples() const;

    //===========================================================================
    // Polynomial coefficients
    //===========================================================================

    // Set the 11 coefficients for the 10th-order polynomial (c0..c10).
    // Evaluated via Horner's method: y = c0 + x*(c1 + x*(c2 + ... + x*c10))
    void setCoefficients(const float (&coeffs)[11]);

    // Convenience: load one of the built-in coefficient presets.
    enum class CoefficientPreset
    {
        Default,           // General-purpose sigmoid (legacy, bipolar input, Horner)
        Warm,              // Softer saturation, tape-like (legacy, bipolar input, Horner)
        Aggressive,        // Harder knee, more harmonic content (legacy, bipolar input, Horner)
        SignFoldSigmoid,   // Sigmoid with gain baked into c1 (~9x), Horner evaluation.
                           // Must be used with setSignFold(true). Input range [0, 1].
        SignFoldChebyshev,  // Custom Chebyshev design: gain ~9x, warm rolloff, f(0)=0 exact.
                            // Evaluated via Clenshaw algorithm. Use with setSignFold(true).
        SignFoldCharacter,  // Character/glue stage: gain ~5x, medium knee (warmth=0.6).
                            // Between preamp (hard) and power amp (gentle). Adds
                            // harmonic color without hard-clipping. Use with setSignFold(true).
        SignFoldPowerAmp    // Power amp tube: gain ~3x, soft knee (warmth=0.7).
                            // Models 6L6/EL34-style power tube with gradual compression
                            // instead of hard clipping. Use with setSignFold(true).
    };
    void setCoefficients(CoefficientPreset preset);

    // Enable sign-fold mode: y = copysign(poly(|x|), x).
    // Polynomial only sees positive input. Output bounded to [-1, +1].
    // Removes the need for the tanh soft-limiter. Use with SignFoldSigmoid coefficients.
    void setSignFold(bool enabled);

    //===========================================================================
    // Parameters (all safe to call from message thread, smoothed on audio thread)
    //===========================================================================

    void setInputGain(float linearGain);     // Pre-waveshaper gain (linear, e.g. 1.0-50.0)
    void setPreShapeIntensity(float amount);  // 0.0 = bypass, max 0.85 (clamped for stability)
    void setFeedbackAmount(float amount);     // 0.0 = single-pass, 0.1 = standard richness
    void setModulationRate(float hz);         // Cosine LFO rate for feedback read position (e.g. 2.0 Hz)
    void setModulationDepth(float samples);   // Micro-modulation depth in samples (e.g. 0.01)
    void setFeedbackDelayLength(int samples); // Feedback buffer delay in samples (4-256, default 256)
    void setDryWetMix(float wet);             // 0.0 = fully dry, 1.0 = fully wet

    // Post-filter: 3-band cut (frequencies and Q values for each band).
    // Each band is a 2nd-order LPF that removes harsh harmonics after waveshaping.
    void setPostFilterLow(float freqHz, float q);
    void setPostFilterMid(float freqHz, float q);
    void setPostFilterHigh(float freqHz, float q);

    // Enable/disable individual post-filter bands (all enabled by default).
    void setPostFilterLowEnabled(bool enabled);
    void setPostFilterMidEnabled(bool enabled);
    void setPostFilterHighEnabled(bool enabled);

private:
    //===========================================================================
    // Polynomial evaluation
    //===========================================================================
    bool signFoldEnabled = false;
    bool useChebyshevBasis = false;
    float coefficients[11] = {};

    // Domain mapping for Chebyshev evaluation
    static constexpr float kChebXMax = 1.3f;
    static constexpr float kChebXMaxInv2 = 2.0f / 1.3f;  // precomputed 2/xMax

    inline float evaluatePolynomial(float x) const
    {
        return useChebyshevBasis ? evaluateClenshaw(x) : evaluateHorner(x);
    }

    // Horner's method (power basis): c0 + x*(c1 + x*(c2 + ... + x*c10))
    inline float evaluateHorner(float x) const
    {
        float y = coefficients[10];
        for (int i = 9; i >= 0; --i)
            y = coefficients[i] + x * y;
        return y;
    }

    // Clenshaw algorithm (Chebyshev basis): sum(a[k] * T_k(u))
    // Maps x from [0, kChebXMax] to u in [-1, 1], then evaluates.
    // Same O(N) cost as Horner, one extra multiply for domain mapping.
    inline float evaluateClenshaw(float x) const
    {
        const float u = kChebXMaxInv2 * x - 1.0f;
        const float u2 = 2.0f * u;
        float b2 = 0.0f, b1 = 0.0f;
        for (int k = 10; k >= 1; --k)
        {
            float b0 = coefficients[k] + u2 * b1 - b2;
            b2 = b1;
            b1 = b0;
        }
        return coefficients[0] + u * b1 - b2;
    }

    //===========================================================================
    // Pre-shaper (sinusoidal soft-clip)
    //===========================================================================
    // Sinusoidal pre-shaper with dry/wet crossfade.
    // shaped = sin(x * scale) * norm, where scale = pi/2 * intensity, norm = 1/cos(scale)
    // output = x * (1 - intensity) + shaped * intensity
    //
    // Previously this was a FULL REPLACEMENT (output = shaped), which meant low
    // intensities had massive gain loss: sin(x * small_scale) ~ x * small_scale.
    // The crossfade fixes this: at intensity 0.15, output = 85% dry + 15% shaped,
    // giving ~-1 dB instead of ~-12 dB. High intensities still produce strong
    // even-harmonic warmth with norm-compensated peaks.
    //
    // The cos() denominator is cached and only recomputed when intensity changes
    // (see updatePreShapeCache). This saves ~1.5M cos() calls/sec at 4x oversampled rate.
    float cachedPreShapeIntensity = -1.0f;
    float cachedPreShapeNorm = 1.0f;
    float cachedPreShapeScale = 0.0f;

    void updatePreShapeCache(float intensity)
    {
        if (intensity == cachedPreShapeIntensity) return;
        cachedPreShapeIntensity = intensity;
        if (intensity < 0.001f)
        {
            cachedPreShapeScale = 0.0f;
            cachedPreShapeNorm = 1.0f;
        }
        else
        {
            cachedPreShapeScale = juce::MathConstants<float>::halfPi * intensity;
            float cosVal = std::cos(cachedPreShapeScale);
            cachedPreShapeNorm = (std::abs(cosVal) < 1e-6f) ? 1.0f : 1.0f / cosVal;
        }
    }

    inline float preShape(float x) const
    {
        if (cachedPreShapeIntensity < 0.001f) return x;
        // Clamp argument to [-pi/2, pi/2] to prevent wavefolding.
        // Without this, high intensity + high input causes sin() to wrap past
        // its peak, inverting peaks vs mid-range (non-monotonic transfer function).
        float arg = x * cachedPreShapeScale;
        arg = juce::jlimit(-juce::MathConstants<float>::halfPi,
                            juce::MathConstants<float>::halfPi, arg);
        float shaped = std::sin(arg) * cachedPreShapeNorm;
        return x + cachedPreShapeIntensity * (shaped - x);
    }

    //===========================================================================
    // Feedback delay buffer
    //===========================================================================
    static constexpr int kFeedbackBufferSize = 256;  // physical array size (max delay)
    float feedbackBuffer[kFeedbackBufferSize] = {};
    int writePos = 0;
    int feedbackDelayLength = 3;  // short delay for feedback recirculation. 3 samples
                                  // ensures all 4 Catmull-Rom interpolation points are
                                  // valid history (delay=2 could read stale data at idx3).
                                  // Previous default (256 = full buffer) created a comb
                                  // filter at ~750 Hz causing buzzing on specific notes.

    // One-pole LPF on feedback read to smooth per-sample gain modulation.
    // Prevents crackling from rapid feedback loop gain changes.
    float feedbackLpfState = 0.0f;
    float feedbackLpfCoeff = 0.3f;  // computed in prepare/prepareRaw

    // Cosine micro-modulation state
    float modPhase = 0.0f;
    float modPhaseInc = 0.0f;  // radians per (oversampled) sample

    // Catmull-Rom (Hermite) interpolation for fractional read position
    static float hermiteInterpolate(const float* buffer, float position, int bufferSize);

    //===========================================================================
    // Post-filter (3-band LPF cut)
    //===========================================================================
    struct PostFilterBand
    {
        juce::dsp::IIR::Filter<float> filter;
        float frequency = 5500.0f;
        float q = 0.38f;
        bool enabled = true;
        bool needsUpdate = true;
    };

    PostFilterBand postFilterLow;
    PostFilterBand postFilterMid;
    PostFilterBand postFilterHigh;

    void updatePostFilterCoefficients();

    //===========================================================================
    // Oversampling (4x, IIR half-band)
    //===========================================================================
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    //===========================================================================
    // Smoothed parameters (audio-rate, ramp over ~20ms)
    //===========================================================================
    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> preShapeSmoothed;
    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> dryWetSmoothed;

    //===========================================================================
    // Modulation parameters (updated from setters, read on audio thread)
    //===========================================================================
    float modRateHz = 2.0f;
    float modDepth = 0.01f;

    //===========================================================================
    // State
    //===========================================================================
    double baseSampleRate = 44100.0;
    double oversampledRate = 44100.0 * 4.0;

    // Process the oversampled block (the core inner loop)
    void processOversampledBlock(float* samples, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveshaperEngine)
};
