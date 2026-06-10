#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <functional>

class Compressor;
class NoiseGate;
class Distortion;
class DiodeDrive;
class AmpSimSilver;
class AmpSimGold;
class AmpSimPlatinum;
class AnalogDelay;
class SpringReverb;
class Chorus;
class Flanger;
class Phaser;
class Vibrato;
class Tremolo;
class Equalizer;
class CompressorPanel;
class NoiseGatePanel;
class DistortionPanel;
class DiodeDrivePanel;
class AmpSimSwitcherPanel;
class DelayPanel;
class PlateReverbPanel;
class ReverbSwitcherPanel;
class ModulationSwitcherPanel;
class ChorusPanel;
class EQPanel;

class PlateReverb;

class EffectDetailPanel : public juce::Component
{
public:
    EffectDetailPanel(Compressor& compressor, NoiseGate& gate, Distortion& distortion, DiodeDrive& diodeDrive, AmpSimSilver& ampSimSilver, AmpSimGold& ampSimGold, AmpSimPlatinum& ampSimPlatinum, AnalogDelay& analogDelay, SpringReverb& springReverb, PlateReverb& plateReverb, Chorus& chorus, Flanger& flanger, Phaser& phaser, Vibrato& vibrato, Tremolo& tremolo, Equalizer& equalizer);
    ~EffectDetailPanel() override;

    // Panel indices: 0=Compressor, 1=Gate, 2=DiodeDrive, 3=Distortion, 4=AmpSim(Switcher),
    //                5=Delay, 6=Reverb, 7=Modulation, 8=EQ
    void showEffect(int index);
    void syncBypassStates();

    // Access the amp sim switcher (for engine selection wiring)
    AmpSimSwitcherPanel* getAmpSimSwitcher() const;

    // Access the reverb switcher (for engine selection wiring)
    ReverbSwitcherPanel* getReverbSwitcher() const;

    // Access the modulation switcher (for engine selection wiring)
    ModulationSwitcherPanel* getModulationSwitcher() const;

    // Called when bypass is toggled from any detail panel
    std::function<void()> onBypassChanged = [] {};

    // Called when any effect parameter changes (slider moved by user)
    std::function<void()> onParameterChanged = [] {};

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    std::unique_ptr<CompressorPanel>      compressorPanel;
    std::unique_ptr<NoiseGatePanel>       noiseGatePanel;
    std::unique_ptr<DistortionPanel>      distortionPanel;
    std::unique_ptr<DiodeDrivePanel>      diodeDrivePanel;
    std::unique_ptr<AmpSimSwitcherPanel>  ampSimSwitcherPanel;
    std::unique_ptr<DelayPanel>           delayPanel;
    std::unique_ptr<ReverbSwitcherPanel>       reverbSwitcherPanel;
    std::unique_ptr<ModulationSwitcherPanel>  modulationSwitcherPanel;
    std::unique_ptr<EQPanel>                  eqPanel;

    static constexpr int kNumPanels = 9;
    juce::Component* panels[kNumPanels] = {};
    int currentIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectDetailPanel)
};
