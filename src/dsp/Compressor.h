#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "EffectProcessor.h"

// Feedforward log-domain compressor, Giannoulis/Massberg/Reiss 2012 architecture.
// Three modes (Studio/Squeeze/Opto) are parameter tables on ONE shared engine.
// No lookahead, no oversampling -- zero latency.
class Compressor : public EffectProcessor
{
public:
    Compressor();
    ~Compressor() override = default;

    void prepare(double sampleRate, int samplesPerBlock) override;
    void process(juce::AudioBuffer<float>& buffer) override;
    void reset() override;
    juce::String getName() const override { return "Compressor"; }
    void resetToDefaults() override;

    void setSustain(float v);   // 0-1
    void setAttack(float v);    // 0-1
    void setBlend(float v);     // 0-1
    void setLevel(float v);     // 0-1
    void setMode(int v);        // 0=Studio, 1=Squeeze, 2=Opto

    float getSustain() const { return sustainParam; }
    float getAttack()  const { return attackParam; }
    float getBlend()   const { return blendParam; }
    float getLevel()   const { return levelParam; }
    int   getMode()    const { return modeParam; }

    // GR meter: audio thread writes once per block; UI polls at 30 Hz.
    // Sign: negative dB (e.g. -6.0 means 6 dB of gain reduction).
    float getGainReductionDb() const
        { return currentGainReductionDb.load(std::memory_order_relaxed); }

private:
    struct ModeTable
    {
        float ratio;
        float kneeDb;
        float attackMinMs;
        float attackMaxMs;
    };

    // Mode tables: ratio / soft-knee width / attack range.
    // Studio 4:1/12 dB = textbook Giannoulis ratio and educator-chart center.
    // Squeeze 10:1/6 dB = Helix-documented Dyna Comp ratio, narrower knee for spank.
    // Opto 3:1/20 dB = LA-2A Compress mode; wide knee + slow attack floor match the T4.
    static constexpr ModeTable modeTables[3] = {
        {  4.0f, 12.0f,  2.0f,  50.0f },   // Studio
        { 10.0f,  6.0f,  2.0f,  50.0f },   // Squeeze
        {  3.0f, 20.0f, 10.0f, 120.0f },   // Opto
    };

    // Fixed internal threshold: sidechain drive (Sustain) sweeps effective
    // threshold without exposing an absolute threshold knob.
    static constexpr float internalThresholdDb  = -20.0f;
    // Nominal DI guitar peak used for auto makeup reference (community standard).
    static constexpr float nominalPeakDb        = -15.0f;
    // Sustain maps 0-1 linearly to 0..+30 dB sidechain drive.
    static constexpr float sustainRangeDb       =  30.0f;
    // 60 ms release floor in all modes: suppresses envelope ripple on sustained notes.
    static constexpr float releaseFloorSec      =  0.06f;

    float sustainParam = 0.35f;
    float attackParam  = 0.50f;
    float blendParam   = 1.00f;
    float levelParam   = 0.50f;
    int   modeParam    = 0;

    // sustainDbSmoothed: tracks sustain drive in dB domain (avoids zipper on knob moves).
    // blendSmoothed: avoids clicks on blend changes.
    // makeupDbSmoothed: smooths auto makeup + level trim transitions (audible on mode switch).
    juce::SmoothedValue<float> sustainDbSmoothed;
    juce::SmoothedValue<float> blendSmoothed;
    juce::SmoothedValue<float> makeupDbSmoothed;

    // Sidechain HPF: removes low-end pumping bias on kick/bass; detection path only.
    juce::dsp::IIR::Filter<float> sidechainHPF[2];

    // Smooth decoupled detector state (Giannoulis eq. 17), operating in dB GR domain.
    // envY1: release-leak stage output. envY: running gain reduction (dB, <= 0).
    float envY1 = 0.0f;
    float envY  = 0.0f;

    float attackCoeff   = 0.0f;
    // releaseCoeff recomputed per-mode and per-block in Studio mode.
    float releaseCoeff  = 0.0f;

    // Studio: crest-factor auto release.
    // peak2/rms2 are one-pole leaky integrators of x^2.
    float peak2      = 0.0f;
    float rms2       = 0.0f;
    float crestCoeff = 0.0f;   // 1/e, 200 ms window

    // Opto: two-stage release (replaces the single release leak).
    // grMemory: one-pole integrator of |GR|, scales the slow tau toward 2.5 s.
    float envFast   = 0.0f;
    float envSlow   = 0.0f;
    float grMemory  = 0.0f;

    std::atomic<float> currentGainReductionDb { 0.0f };

    double currentSampleRate = 44100.0;

    // Recomputes attackCoeff, releaseCoeff, crestCoeff from current params and mode.
    void recalcCoefficients();

    // M_auto = -0.5 * staticGainDb(nominalPeakDb), half-compensation per Giannoulis 2013.
    void recalcAutoMakeup();

    // Soft-knee gain computer (Giannoulis eq. 4/5). Returns dB GR (<= 0).
    float computeStaticGainDb(float levelDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Compressor)
};
