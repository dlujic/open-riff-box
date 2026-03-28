#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <functional>

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
class Equalizer;
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
    EffectDetailPanel(NoiseGate& gate, Distortion& distortion, DiodeDrive& diodeDrive, AmpSimSilver& ampSimSilver, AmpSimGold& ampSimGold, AmpSimPlatinum& ampSimPlatinum, AnalogDelay& analogDelay, SpringReverb& springReverb, PlateReverb& plateReverb, Chorus& chorus, Flanger& flanger, Phaser& phaser, Vibrato& vibrato, Equalizer& equalizer);
    ~EffectDetailPanel() override;

    // Panel indices: 0=Gate, 1=DiodeDrive, 2=Distortion, 3=AmpSim(Switcher),
    //                4=Delay, 5=Reverb, 6=Chorus, 7=EQ
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
    std::unique_ptr<NoiseGatePanel>       noiseGatePanel;
    std::unique_ptr<DistortionPanel>      distortionPanel;
    std::unique_ptr<DiodeDrivePanel>      diodeDrivePanel;
    std::unique_ptr<AmpSimSwitcherPanel>  ampSimSwitcherPanel;
    std::unique_ptr<DelayPanel>           delayPanel;
    std::unique_ptr<ReverbSwitcherPanel>       reverbSwitcherPanel;
    std::unique_ptr<ModulationSwitcherPanel>  modulationSwitcherPanel;
    std::unique_ptr<EQPanel>                  eqPanel;

    static constexpr int kNumPanels = 8;
    juce::Component* panels[kNumPanels] = {};
    int currentIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectDetailPanel)
};
