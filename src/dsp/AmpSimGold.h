#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"
#include "WaveshaperEngine.h"

class AmpSimGold : public EffectProcessor
{
public:
    AmpSimGold();
    ~AmpSimGold() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Amp Gold"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setGain(float value);           // 0-1
    void setBass(float value);           // 0-1
    void setMid(float value);            // 0-1
    void setTreble(float value);         // 0-1
    void setPreampBoost(bool on);
    void setSpeakerDrive(float value);   // 0-1
    void setCabinetType(int type);       // 0-kNumCabinets (14 = custom)
    void loadCustomIR(const juce::File& irFile);  // message thread only
    void setBrightness(float value);     // 0-1
    void setMicPosition(float value);    // 0-1
    void setPresence(float value);       // 0-1 (sweeps NFB LPF: 0=tight/dark, 1=open/present)
    void setCabTrim(float dB);           // -12 to +12

    void setInternalCharacterGain(float g);   // character stage inputGain (default 0.3)
    void setInternalDriveExponent(float e);   // preamp drive curve exponent (default 2.0 = quadratic)
    void setInternalNfbGain(float g);         // push-pull NFB gain (default 0.05)

    float getGain()         const { return gainParam; }
    float getBass()         const { return bassParam; }
    float getMid()          const { return midParam; }
    float getTreble()       const { return trebleParam; }
    bool  getPreampBoost()  const { return preampBoostParam.load(std::memory_order_acquire); }
    float getSpeakerDrive() const { return speakerDriveParam; }
    int   getCabinetType()  const { return cabinetTypeParam.load(std::memory_order_acquire); }
    float getBrightness()   const { return brightnessParam; }
    float getMicPosition()  const { return micPositionParam; }
    float getPresence()     const { return presenceParam; }
    float getCabTrim()      const { return cabTrimDb; }

    static constexpr int kNumCabinets = 14;
    static constexpr int kNoCabinet = kNumCabinets;      // index 14 (bypass convolution)
    static constexpr int kCustomCabinet = kNumCabinets + 1;  // index 15
    static const char* getCabinetName(int index);
    juce::File getCustomIRFile() const { return customIRFile; }

private:
    //==========================================================================
    // Oversampling (4x stereo, IIR half-band polyphase)
    //==========================================================================
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    //==========================================================================
    // WaveshaperEngine instances (8 total: 4 per channel)
    //==========================================================================
    WaveshaperEngine preampL, preampR;
    WaveshaperEngine characterL, characterR;
    WaveshaperEngine powerAmpPosL, powerAmpPosR;
    WaveshaperEngine powerAmpNegL, powerAmpNegR;

    //==========================================================================
    // Coupling cap HPFs (1st-order DC blocker at ~20 Hz)
    //==========================================================================
    struct CouplingCap
    {
        float x1 = 0.0f;
        float y1 = 0.0f;

        void reset() { x1 = 0.0f; y1 = 0.0f; }

        float processSample(float x, float R)
        {
            float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }
    };

    CouplingCap couplingCap1L, couplingCap1R;  // after preamp
    CouplingCap couplingCap2L, couplingCap2R;  // after character

    float couplingCapR = 0.0f;

    //==========================================================================
    // Gain-dependent tone shaping (mono IIR at oversampled rate)
    //==========================================================================
    juce::dsp::IIR::Filter<float> toneLpfL, toneLpfR;
    juce::dsp::IIR::Filter<float> toneMidScoopL, toneMidScoopR;

    //==========================================================================
    // Tone stack (circuit-derived 3rd-order IIR)
    //==========================================================================
    struct ToneStackFilter
    {
        float b[4] = {0};  // numerator: b0, b1, b2, b3
        float a[3] = {0};  // denominator: a1, a2, a3 (a0 normalized to 1)
        float x[3] = {0};  // input delay line
        float y[3] = {0};  // output delay line
        void reset() { std::fill(x, x+3, 0.0f); std::fill(y, y+3, 0.0f); }
        float processSample(float input)
        {
            float output = b[0]*input + b[1]*x[0] + b[2]*x[1] + b[3]*x[2]
                            - a[0]*y[0] - a[1]*y[1] - a[2]*y[2];
            x[2] = x[1]; x[1] = x[0]; x[0] = input;
            y[2] = y[1]; y[1] = y[0]; y[0] = output;
            return output;
        }
    };

    ToneStackFilter toneStackL, toneStackR;

    //==========================================================================
    // Power supply sag (replaces compressor)
    //==========================================================================
    float sagEnvelopeL = 0.0f;
    float sagEnvelopeR = 0.0f;
    float sagAttackCoeff  = 0.0f;
    float sagReleaseCoeff = 0.0f;

    //==========================================================================
    // Output transformer (mono IIR at oversampled rate)
    //==========================================================================
    juce::dsp::IIR::Filter<float> xfmrHpfL, xfmrHpfR;
    juce::dsp::IIR::Filter<float> xfmrLpfL, xfmrLpfR;

    //==========================================================================
    // Push-pull power amp with per-sample NFB
    //==========================================================================
    juce::AudioBuffer<float> powerAmpTempBuffer;
    float nfbStateL = 0.0f;
    float nfbStateR = 0.0f;
    float nfbFilteredStateL = 0.0f;
    float nfbFilteredStateR = 0.0f;
    float nfbLpfCoeff = 0.0f;

    //==========================================================================
    // DC blocker after power amp (mono, oversampled rate)
    //==========================================================================
    CouplingCap dcBlockerL, dcBlockerR;
    float dcBlockerR_coeff = 0.0f;

    //==========================================================================
    // Post-downsample: Cabinet IR (stereo, base rate)
    //==========================================================================
    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    juce::dsp::Convolution cabinetConvolution;

    IIRFilter brightnessFilter;

    IIRFilter micPositionFilter;

    //==========================================================================
    // Parameters
    //==========================================================================
    float gainParam          = 0.4f;
    float bassParam          = 0.5f;
    float midParam           = 0.6f;
    float trebleParam        = 0.5f;
    float speakerDriveParam  = 0.2f;
    float brightnessParam    = 0.6f;
    float micPositionParam   = 0.5f;
    float presenceParam      = 0.70f;
    std::atomic<bool> preampBoostParam { false };
    std::atomic<int>  cabinetTypeParam { 0 };

    float cabTrimDb    = 0.0f; 

    float internalCharacterGain = 0.93f;
    float internalDriveExponent = 0.75f;
    float internalNfbGain       = 0.12f;
    juce::SmoothedValue<float> cabMakeupGain { 1.0f };

    float lastGainParam          = -1.0f;
    float lastBassParam          = -1.0f;
    float lastMidParam           = -1.0f;
    float lastTrebleParam        = -1.0f;
    float lastSpeakerDriveParam  = -1.0f;
    float lastBrightnessParam    = -1.0f;
    float lastMicPositionParam   = -1.0f;
    float lastPresenceParam      = -1.0f;
    bool  lastPreampBoost        = false;

    double currentSampleRate = 44100.0;
    double oversampledRate = 44100.0 * 4.0;
    int lastCabinetType = -1;
    juce::File customIRFile;

    //==========================================================================
    // Private helpers
    //==========================================================================
    void updateToneShapingFilters(float gain01);
    void updateToneStackCoeffs();
    void updateCabinet();
    void updateBrightnessFilter();
    void updateMicPositionFilter();
    void updatePresenceNfbCoeff();
    void updateCabGainTarget();
    void configurePreampEngines();
    void configureCharacterEngines();
    void configurePowerAmpEngines();

    static void getIRData(int index, const void*& data, int& dataSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimGold)
};
