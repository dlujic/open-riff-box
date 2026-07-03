#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class AmpSimSilver : public EffectProcessor
{
public:
    AmpSimSilver();
    ~AmpSimSilver() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Amp Silver"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setGain(float value);           // 0-1
    void setBass(float value);           // 0-1
    void setMid(float value);            // 0-1
    void setTreble(float value);         // 0-1
    void setPreampBoost(bool on);
    void setSpeakerDrive(float value);   // 0-1
    void setCabinetType(int type);       // 0-kNumCabinets (10 = custom)
    void loadCustomIR(const juce::File& irFile);  // message thread only
    void setBrightness(float value);     // 0-1
    void setMicPosition(float value);    // 0-1
    void setCabTrim(float dB);           // -12 to +12

#if ORB_OFFLINE_TOOLS
    // Offline-only diagnostic overrides for artifact-isolation renders.
    // Call before prepare(). Returns false on unknown key.
    // Keys: osfactor (2/4/8/16/32), osfir (1 = FIR equiripple resampler),
    //       cone1x (1 = legacy base-rate cone), conebypass
    bool setDiagnostic(const juce::String& key, float value);
#endif

    float getGain()         const { return gainParam; }
    float getBass()         const { return bassParam; }
    float getMid()          const { return midParam; }
    float getTreble()       const { return trebleParam; }
    bool  getPreampBoost()  const { return preampBoostParam.load(std::memory_order_acquire); }
    float getSpeakerDrive() const { return speakerDriveParam; }
    int   getCabinetType()  const { return cabinetTypeParam.load(std::memory_order_acquire); }
    float getBrightness()   const { return brightnessParam; }
    float getMicPosition()  const { return micPositionParam; }
    float getCabTrim()      const { return cabTrimDb; }

    static constexpr int kNumCabinets = 14;
    static constexpr int kNoCabinet = kNumCabinets;      // index 14 (bypass convolution)
    static constexpr int kCustomCabinet = kNumCabinets + 1;  // index 15
    static const char* getCabinetName(int index);
    juce::File getCustomIRFile() const { return customIRFile; }

private:
    // 16x: the boost tanh cascade is near-square; at 4x its harmonics fold
    // back audibly (C1 alias bench, 2026-07-03).
    static constexpr size_t kOsStages = 4;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRFilter bassFilter;
    IIRFilter midFilter;
    IIRFilter trebleFilter;

    IIRFilter preampLPF;

    IIRFilter dcBlocker;
    IIRFilter osDcBlocker;

    IIRFilter presenceFilter;

    juce::dsp::Convolution cabinetConvolution;

    IIRFilter brightnessFilter;

    IIRFilter micPositionFilter;

    float gainParam          = 0.3f;
    float bassParam          = 0.5f;
    float midParam           = 0.5f;
    float trebleParam        = 0.5f;
    float speakerDriveParam  = 0.2f;
    float brightnessParam    = 0.5f;
    float micPositionParam   = 0.3f;
    std::atomic<bool> preampBoostParam { false };
    std::atomic<int>  cabinetTypeParam { 0 };

    static constexpr float kLevelMakeupDb = 11.1f;

    float cabTrimDb    = 0.0f;
    juce::SmoothedValue<float> cabMakeupGain { 1.0f };

    juce::SmoothedValue<float> gainSmoothed;
    juce::SmoothedValue<float> speakerDriveSmoothed;

    double currentSampleRate = 44100.0;
    double coneSmoothRate    = 44100.0;
    int lastCabinetType = -1;
    juce::File customIRFile;

#if ORB_OFFLINE_TOOLS
    // Diagnostic toggles; defaults reproduce stock behavior exactly.
    struct Diag
    {
        int  osStages   = static_cast<int>(kOsStages);  // 2 = legacy 4x
        bool osFir      = false;   // FIR equiripple resampler (image-rejection probe)
        bool coneAtBase = false;   // pre-fix cone position (1x, post-downsample)
        bool coneBypass = false;   // skip cone stage entirely
    };
    Diag diag;
#endif

    void updateEQFilters();
    void updatePreampLPF();
    void updatePresenceFilter();
    void updateCabinet();
    void updateBrightnessFilter();
    void updateMicPositionFilter();
    void updateCabGainTarget();

    static void getIRData(int index, const void*& data, int& dataSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimSilver)
};
