#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "EffectProcessor.h"

class MetalDistortion;

class Distortion : public EffectProcessor
{
public:
    enum class Mode { Overdrive, TubeDrive, Distortion, Metal };
    enum class ClipType { Soft, Normal, Hard, XHard };

    Distortion();
    ~Distortion() override;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Distortion"; }
    int getLatencySamples() const override;
    void resetToDefaults() override;

    void setDrive(float value);          // 0-1
    void setTone(float value);           // 0-1
    void setLevel(float value);          // 0-1
    void setMix(float value);            // 0-1
    void setSaturate(float value);       // 0-1 amount
    void setSaturateEnabled(bool on);
    void setMode(Mode mode);
    void setClipType(ClipType type);

    float    getDrive()          const;
    float    getTone()           const;
    float    getLevel()          const;
    float    getMix()            const { return mixParam; }
    float    getSaturate()       const { return saturateParam; }
    bool     getSaturateEnabled() const { return saturateEnabled.load(std::memory_order_acquire); }
    Mode     getMode()           const { return static_cast<Mode>(modeParam.load(std::memory_order_acquire)); }
    ClipType getClipType()       const { return static_cast<ClipType>(clipTypeParam.load(std::memory_order_acquire)); }

    MetalDistortion* getMetalProcessor();

private:
    std::unique_ptr<MetalDistortion> metalProcessor;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    using IIRFilter = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    IIRFilter preHighpass;   // 100 Hz HPF before clipping
    IIRFilter dcBlocker;     // 7 Hz HPF after clipping
    IIRFilter toneFilter;    // variable LPF

    float driveParam    = 0.5f;
    float toneParam     = 0.65f;
    float levelParam    = 0.7f;
    float mixParam      = 0.2f;
    float saturateParam = 0.5f;
    std::atomic<bool> saturateEnabled { false };
    std::atomic<int> modeParam     { 0 };  // Mode::Overdrive
    std::atomic<int> clipTypeParam { 1 };  // ClipType::Normal

    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> levelSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> saturateSmoothed;

    float envLevel[2] = { 0.0f, 0.0f };
    float satAttackCoeff  = 0.0f;
    float satReleaseCoeff = 0.0f;

    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;

    static constexpr float hardnessTable[] = { 0.7f, 1.0f, 2.0f, 4.0f };
    static float tubeClip(float x, float hardness, float asymmetry = 1.2f);

    void updateToneFilter();

    void processOverdriveBlock(juce::dsp::AudioBlock<float>& oversampledBlock);
    void processTubeDriveBlock(juce::dsp::AudioBlock<float>& oversampledBlock);
    void processDistortionBlock(juce::dsp::AudioBlock<float>& oversampledBlock);
    void updateModeFilters();

    juce::dsp::IIR::Filter<float> interstageHPF[2];
    juce::dsp::IIR::Filter<float> interstageLPF1[2];
    juce::dsp::IIR::Filter<float> interstageLPF2[2];

    juce::dsp::IIR::Filter<float> midHump[2];
    juce::dsp::IIR::Filter<float> preClipLPF[2];      // ~7 kHz LPF before waveshaper
    juce::dsp::IIR::Filter<float> autoDarkenLPF[2];   // drive-tracking LPF (Distortion mode)

    double oversampledRate = 44100.0 * 4;
    int lastMode = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Distortion)
};
