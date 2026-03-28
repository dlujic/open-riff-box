#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"
#include "TriodeStage.h"
#include "PentodeStage.h"

class AmpSimPlatinum : public EffectProcessor
{
public:
    AmpSimPlatinum();
    ~AmpSimPlatinum() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Amp Platinum"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setGain(float value);          // 0-1
    void setOvLevel(float value);       // 0-1
    void setBass(float value);          // 0-1
    void setMid(float value);           // 0-1
    void setTreble(float value);        // 0-1
    void setMaster(float value);        // 0-1
    void setSpeakerDrive(float value);  // 0-1
    void setGainMode(int mode);         // 0=GAIN1, 1=GAIN2
    void setCabinetType(int type);      // 0-kNumCabinets (14=no cab, 15=custom)
    void loadCustomIR(const juce::File& irFile);
    void setMicPosition(float value);   // 0-1
    void setCabTrim(float dB);          // -12 to +12

    float getGain()         const { return gainParam; }
    float getOvLevel()      const { return ovLevelParam; }
    float getBass()         const { return bassParam; }
    float getMid()          const { return midParam; }
    float getTreble()       const { return trebleParam; }
    float getMaster()       const { return masterParam; }
    float getSpeakerDrive() const { return speakerDriveParam; }
    int   getGainMode()     const { return gainModeParam.load(std::memory_order_acquire); }
    int   getCabinetType()  const { return cabinetTypeParam.load(std::memory_order_acquire); }
    float getMicPosition()  const { return micPositionParam; }
    float getCabTrim()      const { return cabTrimDb; }

    void setStageLimit(int limit) { stageLimit = limit; }

    static constexpr int kNumCabinets   = 14;
    static constexpr int kNoCabinet     = kNumCabinets;        // index 14
    static constexpr int kCustomCabinet = kNumCabinets + 1;    // index 15
    static const char* getCabinetName(int index);
    juce::File getCustomIRFile() const { return customIRFile; }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    TriodeStage v1aL, v1aR;   // 12AX7, Rp=100K, Rk=1K,   Ck=4.7uF, Bplus=348V
    TriodeStage v1bL, v1bR;   // 12AX7, Rp=100K, Rk=1.8K, Ck=1uF, Cp=470pF, Bplus=343V
    TriodeStage v2aL, v2aR;   // 12AX7, Rp=100K, Rk=4.7K, Ck=0 (unbypassed), Bplus=346V
    TriodeStage v2bL, v2bR;   // 12AX7, Rp=100K, Rk=2.2K, Ck=10uF, Bplus=362V
    TriodeStage v3aL, v3aR;   // 12AX7, Rp=100K, Rk=1K,   Ck=10uF, Bplus=381V

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

    CouplingCap coupCapAL, coupCapAR;  // after V1A, 4.7nF (fc~34Hz)
    CouplingCap coupCapBL, coupCapBR;  // after V1B, 4.7nF (fc~34Hz)
    CouplingCap coupCapCL, coupCapCR;  // after V2A, 22nF  (fc~15Hz)
    CouplingCap coupCapDL, coupCapDR;  // after V2B, 22nF  (fc~15Hz)

    float coupCapR_34Hz = 0.0f;  // R = 1 - 2*pi*34 / fs_oversampled
    float coupCapR_15Hz = 0.0f;  // R = 1 - 2*pi*15 / fs_oversampled

    float v1aMillerLpfL = 0.0f;
    float v1aMillerLpfR = 0.0f;
    float v1aMillerCoeff = 0.0f;

    float odC25LpfL = 0.0f;
    float odC25LpfR = 0.0f;
    float odC25Coeff = 0.0f;

    float v2bMillerLpfL = 0.0f;
    float v2bMillerLpfR = 0.0f;
    float v2bMillerCoeff = 0.0f;

    CouplingCap dcBlockerL, dcBlockerR;
    float dcBlockerR_coeff = 0.0f;

    juce::dsp::IIR::Filter<float> gainFilterL, gainFilterR;

    struct ToneStackFilter
    {
        float b[4] = { 0 };  // numerator coefficients b0..b3
        float a[3] = { 0 };  // denominator coefficients a1..a3
        float x[3] = { 0 };  // input history
        float y[3] = { 0 };  // output history

        void reset()
        {
            std::fill(x, x + 3, 0.0f);
            std::fill(y, y + 3, 0.0f);
        }

        float processSample(float input)
        {
            float output = b[0] * input + b[1] * x[0] + b[2] * x[1] + b[3] * x[2]
                         - a[0] * y[0] - a[1] * y[1] - a[2] * y[2];
            x[2] = x[1]; x[1] = x[0]; x[0] = input;
            y[2] = y[1]; y[1] = y[0]; y[0] = output;
            return output;
        }
    };

    ToneStackFilter toneStackL, toneStackR;

    float v4_cathodeL = 0.0f, v4_cathodeR = 0.0f;  // Shared cathode voltage
    float v4a_IpPrevL = 0.0f, v4a_IpPrevR = 0.0f;  // V4A NR initial guess
    float v4b_IpPrevL = 0.0f, v4b_IpPrevR = 0.0f;  // V4B NR initial guess

    float v4_Vk_q     = 0.0f;
    float v4a_Ip_q    = 0.0f, v4b_Ip_q    = 0.0f;
    float v4a_Vp_q    = 0.0f, v4b_Vp_q    = 0.0f;
    float v4_Vtail_q  = 0.0f;
    float v4a_Vg_q    = 0.0f, v4b_Vg_q    = 0.0f;

    PentodeStage v5L, v5R, v6L, v6R;
    float speakerNormFactor = 0.0f;

    CouplingCap coupCapC58L, coupCapC58R;   // Master -> V4B grid (C58, 22nF, 34Hz)
    CouplingCap coupCapC61L, coupCapC61R;   // V4A plate -> V5 supp (C61, 22nF, 34Hz)
    CouplingCap coupCapC62L, coupCapC62R;   // V4B plate -> V6 supp (C62, 22nF, 34Hz)
    CouplingCap coupCapNfbL, coupCapNfbR;   // NFB -> V4A grid (C63, 0.1uF, 339Hz)
    float coupCapR_339Hz = 0.0f;            // R coeff for C63

    juce::dsp::IIR::Filter<float> xfmrHpfL, xfmrHpfR;

    float leakageLpfStateL = 0.0f, leakageLpfStateR = 0.0f;
    float leakageLpfCoeff  = 0.0f;

    float nfbFilteredStateL = 0.0f;
    float nfbFilteredStateR = 0.0f;
    float nfbLpfCoeff       = 0.0f;
    float prevSpeakerOutL   = 0.0f;
    float prevSpeakerOutR   = 0.0f;

    float sagEnvelopeL     = 0.0f;
    float sagEnvelopeR     = 0.0f;
    float sagAttackCoeff   = 0.0f;
    float sagReleaseCoeff  = 0.0f;

    juce::SmoothedValue<float> ovLevelSmoothed;
    juce::SmoothedValue<float> masterSmoothed;

    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    juce::dsp::Convolution cabinetConvolution;
    IIRFilter               micPositionFilter;

    float gainParam         = 0.5f;
    float ovLevelParam      = 0.7f;
    float bassParam         = 0.5f;
    float midParam          = 0.5f;
    float trebleParam       = 0.5f;
    float masterParam       = 0.3f;
    float speakerDriveParam = 0.3f;
    float micPositionParam  = 0.5f;
    std::atomic<int>  gainModeParam    { 0 };
    std::atomic<int>  cabinetTypeParam { 0 };

    float cabTrimDb = 0.0f;
    juce::SmoothedValue<float> cabMakeupGain { 1.0f };

    float lastGainParam        = -1.0f;
    float lastBassParam        = -1.0f;
    float lastMidParam         = -1.0f;
    float lastTrebleParam      = -1.0f;
    float lastMicPositionParam = -1.0f;
    int   lastGainMode         = -1;

    int    stageLimit        = 0;  // 0=full chain, 1-9=tap after stage N

    uint32_t noiseStateL = 123456789u;
    uint32_t noiseStateR = 987654321u;
    double currentSampleRate = 44100.0;
    double oversampledRate   = 44100.0 * 4.0;
    int    lastCabinetType   = -1;
    juce::File customIRFile;

    void configureTriodeStages();
    void configurePowerAmpStages();
    void applyGainMode();
    void updateGainFilter();
    void updateToneStackCoeffs();
    void updateCabinet();
    void updateMicPositionFilter();
    void updateCabGainTarget();

    static void getIRData(int index, const void*& data, int& dataSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimPlatinum)
};
