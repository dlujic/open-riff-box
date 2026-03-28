#include "EffectDetailPanel.h"
#include "NoiseGatePanel.h"
#include "DistortionPanel.h"
#include "DiodeDrivePanel.h"
#include "AmpSimSwitcherPanel.h"
#include "DelayPanel.h"
#include "ReverbPanel.h"
#include "PlateReverbPanel.h"
#include "ReverbSwitcherPanel.h"
#include "ModulationSwitcherPanel.h"
#include "ChorusPanel.h"
#include "FlangerPanel.h"
#include "EQPanel.h"
#include "Theme.h"
#include "dsp/AmpSimPlatinum.h"
#include "dsp/PlateReverb.h"

EffectDetailPanel::EffectDetailPanel(NoiseGate& gate, Distortion& distortion, DiodeDrive& diodeDrive, AmpSimSilver& ampSimSilver, AmpSimGold& ampSimGold, AmpSimPlatinum& ampSimPlatinum, AnalogDelay& analogDelay, SpringReverb& springReverb, PlateReverb& plateReverb, Chorus& chorus, Flanger& flanger, Phaser& phaser, Vibrato& vibrato, Equalizer& equalizer)
{
    noiseGatePanel       = std::make_unique<NoiseGatePanel>(gate);
    distortionPanel      = std::make_unique<DistortionPanel>(distortion);
    diodeDrivePanel      = std::make_unique<DiodeDrivePanel>(diodeDrive);
    ampSimSwitcherPanel  = std::make_unique<AmpSimSwitcherPanel>(ampSimSilver, ampSimGold, ampSimPlatinum);
    delayPanel           = std::make_unique<DelayPanel>(analogDelay);
    reverbSwitcherPanel      = std::make_unique<ReverbSwitcherPanel>(springReverb, plateReverb);
    modulationSwitcherPanel  = std::make_unique<ModulationSwitcherPanel>(chorus, flanger, phaser, vibrato);
    eqPanel                  = std::make_unique<EQPanel>(equalizer);

    panels[0] = noiseGatePanel.get();
    panels[1] = diodeDrivePanel.get();
    panels[2] = distortionPanel.get();
    panels[3] = ampSimSwitcherPanel.get();
    panels[4] = delayPanel.get();
    panels[5] = reverbSwitcherPanel.get();
    panels[6] = modulationSwitcherPanel.get();
    panels[7] = eqPanel.get();

    // Wire bypass callbacks
    noiseGatePanel->onBypassToggled       = [this] { onBypassChanged(); };
    distortionPanel->onBypassToggled      = [this] { onBypassChanged(); };
    diodeDrivePanel->onBypassToggled      = [this] { onBypassChanged(); };
    ampSimSwitcherPanel->onBypassToggled  = [this] { onBypassChanged(); };
    delayPanel->onBypassToggled           = [this] { onBypassChanged(); };
    reverbSwitcherPanel->onBypassToggled       = [this] { onBypassChanged(); };
    modulationSwitcherPanel->onBypassToggled  = [this] { onBypassChanged(); };
    eqPanel->onBypassToggled                  = [this] { onBypassChanged(); };

    // Wire parameter change callbacks (clears active preset on any tweak)
    auto paramChanged = [this] { onParameterChanged(); };
    noiseGatePanel->onParameterChanged       = paramChanged;
    distortionPanel->onParameterChanged      = paramChanged;
    diodeDrivePanel->onParameterChanged      = paramChanged;
    ampSimSwitcherPanel->onParameterChanged  = paramChanged;
    delayPanel->onParameterChanged           = paramChanged;
    reverbSwitcherPanel->onParameterChanged       = paramChanged;
    modulationSwitcherPanel->onParameterChanged  = paramChanged;
    eqPanel->onParameterChanged                  = paramChanged;

    for (auto* p : panels)
        addChildComponent(p);
}

EffectDetailPanel::~EffectDetailPanel() = default;

AmpSimSwitcherPanel* EffectDetailPanel::getAmpSimSwitcher() const
{
    return ampSimSwitcherPanel.get();
}

ReverbSwitcherPanel* EffectDetailPanel::getReverbSwitcher() const
{
    return reverbSwitcherPanel.get();
}

ModulationSwitcherPanel* EffectDetailPanel::getModulationSwitcher() const
{
    return modulationSwitcherPanel.get();
}

void EffectDetailPanel::syncBypassStates()
{
    noiseGatePanel->syncFromDsp();
    distortionPanel->syncFromDsp();
    diodeDrivePanel->syncFromDsp();
    ampSimSwitcherPanel->syncFromDsp();
    delayPanel->syncFromDsp();
    reverbSwitcherPanel->syncFromDsp();
    modulationSwitcherPanel->syncFromDsp();
    eqPanel->syncFromDsp();
}

void EffectDetailPanel::showEffect(int index)
{
    if (index < 0 || index >= kNumPanels)
        return;

    if (currentIndex >= 0 && currentIndex < kNumPanels)
        panels[currentIndex]->setVisible(false);

    currentIndex = index;
    panels[currentIndex]->setVisible(true);
    resized();
}

void EffectDetailPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
}

void EffectDetailPanel::resized()
{
    auto area = getLocalBounds();

    for (auto* p : panels)
        p->setBounds(area);
}
