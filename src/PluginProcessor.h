#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "dsp/EffectChain.h"
#include "dsp/TunerEngine.h"

class OpenRiffBoxProcessor : public juce::AudioProcessor
{
public:
    OpenRiffBoxProcessor();
    ~OpenRiffBoxProcessor() override = default;

    //===========================================================================
    // AudioProcessor interface
    //===========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;

    //===========================================================================
    // Editor
    //===========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //===========================================================================
    // Metadata
    //===========================================================================
    const juce::String getName() const override { return "OpenRiffBox"; }

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                          override { return 1; }
    int  getCurrentProgram()                       override { return 0; }
    void setCurrentProgram(int)                    override {}
    const juce::String getProgramName(int)         override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //===========================================================================
    // State
    //===========================================================================
    void getStateInformation(juce::MemoryBlock& destData)         override;
    void setStateInformation(const void* data, int sizeInBytes)   override;

    //===========================================================================
    // Audio active (Start/Stop)
    //===========================================================================
    bool isAudioActive() const { return audioActive.load(std::memory_order_acquire); }
    void setAudioActive(bool active) { audioActive.store(active, std::memory_order_release); }

    //===========================================================================
    // Peak metering (input / output)
    //===========================================================================
    float getInputPeak()  const { return inputPeak.load(std::memory_order_relaxed); }
    float getOutputPeak() const { return outputPeak.load(std::memory_order_relaxed); }

    //===========================================================================
    // CPU load metering
    //===========================================================================
    float getCpuLoad() const { return cpuLoad.load(std::memory_order_relaxed); }

    //===========================================================================
    // Master volume (0-1, applied after effects, before limiter)
    //===========================================================================
    float getMasterVolume() const { return masterVolume.load(std::memory_order_relaxed); }
    void  setMasterVolume(float v) { masterVolume.store(juce::jlimit(0.0f, 1.0f, v), std::memory_order_relaxed); }

    //===========================================================================
    // Output limiter
    //===========================================================================
    bool isLimiterEnabled() const { return limiterEnabled.load(std::memory_order_acquire); }
    void setLimiterEnabled(bool enabled) { limiterEnabled.store(enabled, std::memory_order_release); }
    bool isLimiterClipping() const { return limiterClipping.load(std::memory_order_relaxed); }

    //===========================================================================
    // Tuner
    //===========================================================================
    TunerEngine& getTunerEngine() { return tunerEngine; }
    void setTunerActive(bool active);
    bool isTunerActive() const { return tunerActive.load(std::memory_order_acquire); }

    //===========================================================================
    // Amp Sim engine selection (0=Silver/AmpSimSilver, 1=Gold/AmpSimGold, 2=Platinum/AmpSimPlatinum)
    //===========================================================================
    int  getAmpSimEngine() const { return ampSimEngine; }
    void setAmpSimEngine(int engine);

    //===========================================================================
    // Reverb engine selection (0=Spring, 1=Plate)
    //===========================================================================
    int  getReverbEngine() const { return reverbEngine; }
    void setReverbEngine(int engine);

    //===========================================================================
    // Modulation engine selection (0=Chorus, 1=Flanger, 2=Phaser, 3=Vibrato)
    //===========================================================================
    int  getModulationEngine() const { return modulationEngine; }
    void setModulationEngine(int engine);

    //===========================================================================
    // Accessors for the editor
    //===========================================================================
    EffectChain& getEffectChain() { return effectChain; }

private:
    EffectChain effectChain;
    TunerEngine tunerEngine;
    std::atomic<bool> tunerActive { false };

    double currentSampleRate  = 44100.0;
    int    currentBlockSize   = 512;

    std::atomic<bool>  audioActive   { false };
    std::atomic<float> masterVolume  { 1.0f };   // 0-1, default 100% (soft limiter at 0.97 protects output)

    // Output limiter (brickwall at -0.1 dBFS)
    std::atomic<bool> limiterEnabled  { true };
    std::atomic<bool> limiterClipping { false };

    // CPU load (audio thread writes, UI reads)
    std::atomic<float> cpuLoad { 0.0f };

    // Peak levels for metering (audio thread writes, UI reads)
    std::atomic<float> inputPeak  { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    // Amp sim engine: 0=Silver (AmpSimSilver), 1=Gold (AmpSimGold), 2=Platinum (AmpSimPlatinum)
    int ampSimEngine = 1;

    // Reverb engine: 0=Spring, 1=Plate
    int reverbEngine = 0;

    // Modulation engine: 0=Chorus, 1=Flanger, 2=Phaser, 3=Vibrato
    int modulationEngine = 0;

    // Cached latency: updated each processBlock so bypass changes are reflected
    int lastReportedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenRiffBoxProcessor)
};
