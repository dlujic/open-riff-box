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
    void setCabinetType(int type);      // 0-13, kNoCabinet or kCustomCabinet
    void loadCustomIR(const juce::File& irFile);
    void setMicPosition(float value);   // 0-1
    void setCabTrim(float dB);          // -12 to +12

    void setChannel(int value);         // 0=OD (default), 1=Normal
    void setBoost(bool value);          // Normal voicing: false=CLEAN, true=BOOST
    void setInputLow(bool value);       // false=HIGH (default), true=LOW (-6dB pad)
    void setNormalBass(float value);    // 0-1, Normal channel EQ (VR5)
    void setNormalMid(float value);     // 0-1, Normal channel EQ (VR6)
    void setNormalTreble(float value);  // 0-1, Normal channel EQ (VR7)
    void setNormalLevel(float value);   // 0-1, Normal channel LEVEL (VR1)

    // Maps a pre-v2 preset master (flat multiplier) to the knob position that
    // produces the same drive under the VR12 pot law. Used on preset load.
    static float remapLegacyMaster(float oldLinear);

    // Lifts a pre-v3 cabinetType (sentinels were 14/15) onto the pinned values.
    // Used on state load and by the offline --cabinet flag, which kept the old
    // encoding. Factory indices pass through untouched.
    static int remapLegacyCabinet(int stored);

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

    int   getChannel()      const { return channelParam.load(std::memory_order_acquire); }
    bool  getBoost()        const { return boostParam.load(std::memory_order_acquire); }
    bool  getInputLow()     const { return inputLowParam.load(std::memory_order_acquire); }
    float getNormalBass()   const { return normalBassParam; }
    float getNormalMid()    const { return normalMidParam; }
    float getNormalTreble() const { return normalTrebleParam; }
    float getNormalLevel()  const { return normalLevelParam; }

    void setStageLimit(int limit) { stageLimit = limit; }

#if ORB_OFFLINE_TOOLS
    // Offline-only diagnostic overrides for artifact-isolation renders.
    // Call before prepare(). Returns false on unknown key.
    // Keys: osfactor (2/4/8/16/32, stock 4), osfir (1 = FIR equiripple resampler),
    //       noiselevel (V1A noise inject amplitude, 1e-5 = pre-C2 stock),
    //       brightfix (1 = schematic-derived GAIN1 network, default 0),
    //       brightcin (V1B Miller capacitance in pF for brightfix, default 110),
    //       c59 (1 = LTP plate-to-plate 47p, default 0),
    //       mvcircuit (VR12 A-taper + C58 network, default 1; 0 = legacy flat master)
    bool setDiagnostic(const juce::String& key, float value);
#endif

    // The sentinels are pinned, not derived from kNumCabinets: cabinetType is
    // persisted as a raw int, so adding factory IRs must not shift them.
    static constexpr int kNumCabinets   = 14;
    static constexpr int kNoCabinet     = 200;   // bypass convolution
    static constexpr int kCustomCabinet = 201;
    static_assert(kNumCabinets < kNoCabinet, "factory IRs have grown into the sentinel range");

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
    float v1aMillerCoeff = 0.0f;         // OD input corner, 1620 Hz (unchanged)
    float v1aMillerCoeffNormal = 0.0f;   // Normal input corner, 3973 Hz

    float v4bGridLpfL = 0.0f;   // V4B grid input pole (master Rs || grid load into Cin)
    float v4bGridLpfR = 0.0f;

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

    // Normal-channel voicing ladder (JC6 -> V2A_GRID), fixed per CLEAN/BOOST;
    // both coeff sets computed once at prepare() from the s-domain polys,
    // selected by the boost flag at runtime (engine_spec.md sec 2).
    ToneStackFilter ladderCleanL, ladderCleanR;
    ToneStackFilter ladderBoostL, ladderBoostR;

    // VR1 LEVEL + C13 bright cap: one knob-tracked first-order shelf.
    struct ShelfFilter
    {
        float b0 = 1.0f, b1 = 0.0f, a0 = 0.0f;
        float x1 = 0.0f, y1 = 0.0f;

        void reset() { x1 = 0.0f; y1 = 0.0f; }

        float processSample(float input)
        {
            float output = b0 * input + b1 * x1 - a0 * y1;
            x1 = input;
            y1 = output;
            return output;
        }
    };
    ShelfFilter vr1c13L, vr1c13R;

    float v4a_IpPrevL = 0.0f, v4a_IpPrevR = 0.0f;  // V4A NR initial guess
    float v4b_IpPrevL = 0.0f, v4b_IpPrevR = 0.0f;  // V4B NR initial guess

    // Tail-node LF ride reaching the grid bias refs (deviation volts from
    // quiescent, so the sub-10Hz one-poles stay above float LSB at os rates)
    float v4aRideL = 0.0f, v4aRideR = 0.0f;        // grid A: R97/C63, 1.6 Hz
    float v4bRideL = 0.0f, v4bRideR = 0.0f;        // grid B: R92||R91/C58, 8 Hz
    float v4RideCoeffA = 0.0f, v4RideCoeffB = 0.0f;

    float v4_Vk_q     = 0.0f;
    float v4a_Ip_q    = 0.0f, v4b_Ip_q    = 0.0f;
    float v4a_Vp_q    = 0.0f, v4b_Vp_q    = 0.0f;
    float v4_Vtail_q  = 0.0f;
    float v4a_Vg_q    = 0.0f, v4b_Vg_q    = 0.0f;

    PentodeStage v5L, v5R, v6L, v6R;
    float speakerNormFactor = 0.0f;

    CouplingCap coupCapC58L, coupCapC58R;   // Master -> V4B grid (C58, 22nF, knob-tracked fc)
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
    float masterParam       = 0.64f;
    float speakerDriveParam = 0.3f;
    float micPositionParam  = 0.5f;
    std::atomic<int>  gainModeParam    { 0 };
    std::atomic<int>  cabinetTypeParam { 0 };

    std::atomic<int>  channelParam   { 0 };      // 0=OD, 1=Normal
    std::atomic<bool> boostParam     { false };  // Normal CLEAN/BOOST
    std::atomic<bool> inputLowParam  { false };  // false=HIGH, true=LOW

    float normalBassParam   = 0.5f;
    float normalMidParam    = 0.5f;
    float normalTrebleParam = 0.5f;
    float normalLevelParam  = 0.5f;
    juce::SmoothedValue<float> normalLevelSmoothed;

    static constexpr float kLevelMakeupDb = 3.0f;
    static constexpr size_t kOsStages = 2;  // 4x oversampling

#if ORB_OFFLINE_TOOLS
    // Diagnostic toggles; defaults reproduce stock behavior exactly.
    struct Diag
    {
        int   osStages    = static_cast<int>(kOsStages);
        bool  osFir       = false;
        bool  brightFix   = false;
        float brightCinPf = 110.0f;
        bool  c59         = false;
        bool  v4tail      = true;   // dynamic LTP tail; 0 = frozen pre-fix arm
        int   v4outers    = 12;     // dynamic-tail outer NR iterations
        bool  v4rides     = true;   // grid bias ride LPFs
        float v4nfb       = 1.0f;   // NFB scale into dynamic-arm grid A
        int   v4dump      = 0;      // 1=tailDev 2=rideB 3=rideA 4=Vk-VkQ 5=c58Out 6=nfbHpf 7=F
        bool  mvCircuit   = true;   // VR12 A-taper + C58 network; 0 = legacy flat master arm
    };
    Diag diag;

    // Schematic-derived GAIN1 network (brightfix arm), same 3rd-order form
    // as the tone stack. Coefficients set in updateBrightNetworkCoeffs().
    ToneStackFilter brightNetL, brightNetR;

    void updateBrightNetworkCoeffs();

    // C59 arm: one-pole LPF on the LTP differential plate signal.
    float c59LpfCoeff   = 0.0f;
    float c59DiffStateL = 0.0f;
    float c59DiffStateR = 0.0f;
#endif

    float cabTrimDb = 0.0f;
    juce::SmoothedValue<float> cabMakeupGain { 1.0f };

    float lastGainParam        = -1.0f;
    float lastBassParam        = -1.0f;
    float lastMidParam         = -1.0f;
    float lastTrebleParam      = -1.0f;
    float lastMicPositionParam = -1.0f;
    int   lastGainMode         = -1;

    float lastNormalBassParam    = -1.0f;
    float lastNormalMidParam     = -1.0f;
    float lastNormalTrebleParam  = -1.0f;
    int   lastChannel  = 0;
    bool  lastBoost    = false;
    bool  lastInputLow = false;

    // Channel/boost/jack switch click: brief output mute masks the DSP-state
    // discontinuity (real amp mutes ~150ms via JFETs; engine_spec.md sec 7).
    int   muteGapRampSamples = 0;   // one ramp leg, ~8ms at oversampledRate
    int   muteGapRemainingL  = 0;
    int   muteGapRemainingR  = 0;

    int    stageLimit        = 0;  // 0=full chain, 1-9=tap after stage N

    uint32_t noiseStateL = 123456789u;
    uint32_t noiseStateR = 987654321u;
    // V1A input noise amplitude. 1e-7 is in the ballpark of physical 12AX7
    // input noise; the old 1e-5 put the render floor ~2 orders too hot.
    float noiseInjectLevel = 1e-7f;
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
