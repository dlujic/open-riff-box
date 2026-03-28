#include "MainLayout.h"
#include "AmpSimSwitcherPanel.h"
#include "ReverbSwitcherPanel.h"
#include "ModulationSwitcherPanel.h"
#include "Theme.h"
#include "PluginProcessor.h"
#include "preset/PresetManager.h"
#include "dsp/NoiseGate.h"
#include "dsp/Distortion.h"
#include "dsp/DiodeDrive.h"
#include "dsp/AmpSimSilver.h"
#include "dsp/AmpSimGold.h"
#include "dsp/AmpSimPlatinum.h"
#include "dsp/AnalogDelay.h"
#include "dsp/SpringReverb.h"
#include "dsp/PlateReverb.h"
#include "dsp/Chorus.h"
#include "dsp/Flanger.h"
#include "dsp/Phaser.h"
#include "dsp/Vibrato.h"
#include "dsp/Equalizer.h"

MainLayout::MainLayout(OpenRiffBoxProcessor& processor)
    : processorRef(processor),
      chainList(processor.getEffectChain()),
      sidebarPanel(processor)
{
    // Set Inter as the default font for all JUCE components
    juce::LookAndFeel::getDefaultLookAndFeel()
        .setDefaultSansSerifTypeface(Theme::Fonts::getRegular());

    addAndMakeVisible(chainList);
    chainList.addListener(this);

    addAndMakeVisible(sidebarPanel);

    // Get DSP effect references for the detail panels (name-based, order-independent)
    auto& chain = processorRef.getEffectChain();
    auto* gate       = dynamic_cast<NoiseGate*>(chain.getEffectByName("Noise Gate"));
    auto* diodeDrive = dynamic_cast<DiodeDrive*>(chain.getEffectByName("Diode Drive"));
    auto* distortion = dynamic_cast<Distortion*>(chain.getEffectByName("Distortion"));
    auto* ampSim         = dynamic_cast<AmpSimSilver*>(chain.getEffectByName("Amp Silver"));
    auto* ampSim2        = dynamic_cast<AmpSimGold*>(chain.getEffectByName("Amp Gold"));
    auto* ampSimPlatinum = dynamic_cast<AmpSimPlatinum*>(chain.getEffectByName("Amp Platinum"));
    auto* analogDelay    = dynamic_cast<AnalogDelay*>(chain.getEffectByName("Delay"));
    auto* springReverb = dynamic_cast<SpringReverb*>(chain.getEffectByName("Spring Reverb"));
    auto* plateReverb  = dynamic_cast<PlateReverb*>(chain.getEffectByName("Plate Reverb"));
    auto* chorus = dynamic_cast<Chorus*>(chain.getEffectByName("Chorus"));
    auto* flanger = dynamic_cast<Flanger*>(chain.getEffectByName("Flanger"));
    auto* phaser = dynamic_cast<Phaser*>(chain.getEffectByName("Phaser"));
    auto* vibrato = dynamic_cast<Vibrato*>(chain.getEffectByName("Vibrato"));
    auto* equalizer = dynamic_cast<Equalizer*>(chain.getEffectByName("EQ"));

    if (gate != nullptr && distortion != nullptr && diodeDrive != nullptr && ampSim != nullptr && ampSim2 != nullptr && ampSimPlatinum != nullptr && analogDelay != nullptr && springReverb != nullptr && plateReverb != nullptr && chorus != nullptr && flanger != nullptr && phaser != nullptr && vibrato != nullptr && equalizer != nullptr)
    {
        detailPanel = std::make_unique<EffectDetailPanel>(*gate, *distortion, *diodeDrive, *ampSim, *ampSim2, *ampSimPlatinum, *analogDelay, *springReverb, *plateReverb, *chorus, *flanger, *phaser, *vibrato, *equalizer);
        addAndMakeVisible(*detailPanel);

        // Sync bypass state between chain list and detail panels
        detailPanel->onBypassChanged = [this] {
            chainList.refreshBypassStates();
            if (presetManager) presetManager->clearActivePreset();
        };
        chainList.onBypassChanged = [this] {
            if (detailPanel) detailPanel->syncBypassStates();
            if (presetManager) presetManager->clearActivePreset();
        };

        // Clear active preset when user tweaks any parameter
        detailPanel->onParameterChanged = [this] {
            if (presetManager) presetManager->clearActivePreset();
        };

        // Wire amp sim engine switching
        if (auto* switcher = detailPanel->getAmpSimSwitcher())
        {
            // Restore engine from processor state
            switcher->setActiveEngine(processorRef.getAmpSimEngine());
            chainList.setActiveAmpEngine(processorRef.getAmpSimEngine());

            switcher->onEngineChanged = [this](int engine) {
                processorRef.setAmpSimEngine(engine);
                chainList.setActiveAmpEngine(engine);
                chainList.refreshBypassStates();
                if (auto* s = detailPanel->getAmpSimSwitcher())
                    s->syncFromDsp();
                if (presetManager) presetManager->clearActivePreset();
            };
        }

        // Wire reverb engine switching
        if (auto* reverbSwitcher = detailPanel->getReverbSwitcher())
        {
            // Restore engine from processor state
            reverbSwitcher->setActiveEngine(processorRef.getReverbEngine());
            chainList.setActiveReverbEngine(processorRef.getReverbEngine());

            reverbSwitcher->onEngineChanged = [this](int engine) {
                processorRef.setReverbEngine(engine);
                chainList.setActiveReverbEngine(engine);
                chainList.refreshBypassStates();
                if (auto* rs = detailPanel->getReverbSwitcher())
                    rs->syncFromDsp();
                if (presetManager) presetManager->clearActivePreset();
            };
        }

        // Wire modulation engine switching
        if (auto* modulationSwitcher = detailPanel->getModulationSwitcher())
        {
            // Restore engine from processor state
            modulationSwitcher->setActiveEngine(processorRef.getModulationEngine());
            chainList.setActiveModulationEngine(processorRef.getModulationEngine());

            modulationSwitcher->onEngineChanged = [this](int engine) {
                processorRef.setModulationEngine(engine);
                chainList.setActiveModulationEngine(engine);
                chainList.refreshBypassStates();
                if (auto* ms = detailPanel->getModulationSwitcher())
                    ms->syncFromDsp();
                if (presetManager) presetManager->clearActivePreset();
            };
        }
    }

    // Embedded audio settings panel (wrapper with warm styling + screws)
    audioSettingsPanel = std::make_unique<AudioSettingsPanel>();
    audioSettingsPanel->onTooltipsChanged = [this](bool enabled) {
        if (enabled && tooltipWindow == nullptr)
        {
            tooltipWindow = std::make_unique<juce::TooltipWindow>(nullptr, 600);
            tooltipWindow->setLookAndFeel(&tooltipLF);
            tooltipWindow->setComponentEffect(nullptr);
        }
        else if (!enabled)
        {
            if (tooltipWindow != nullptr)
                tooltipWindow->setLookAndFeel(nullptr);
            tooltipWindow.reset();
        }
    };
    addChildComponent(*audioSettingsPanel);

    // Restore tooltip state from last session
    if (audioSettingsPanel->isTooltipsEnabled())
    {
        tooltipWindow = std::make_unique<juce::TooltipWindow>(nullptr, 600);
        tooltipWindow->setLookAndFeel(&tooltipLF);
        tooltipWindow->setComponentEffect(nullptr);
    }

    // Tuner panel (reads pitch from TunerEngine on a 30 Hz timer)
    tunerPanel = std::make_unique<TunerPanel>(processorRef.getTunerEngine());
    addChildComponent(*tunerPanel);

    metronomePanel = std::make_unique<PlaceholderPanel>("Metronome");
    addChildComponent(*metronomePanel);

    // Preset system
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto presetsDir = exeDir.getChildFile("presets");

    // If running from a build directory, look for presets at project root
    if (!presetsDir.getChildFile("factory").isDirectory())
    {
        auto projectRoot = exeDir;
        for (int i = 0; i < 6; ++i)
        {
            projectRoot = projectRoot.getParentDirectory();
            if (projectRoot.getChildFile("presets").getChildFile("factory").isDirectory())
            {
                presetsDir = projectRoot.getChildFile("presets");
                break;
            }
        }
    }

    presetManager = std::make_unique<PresetManager>(processorRef, presetsDir);

    // Load slot assignments from app settings
    if (auto* app = dynamic_cast<juce::JUCEApplication*>(juce::JUCEApplication::getInstance()))
    {
        // Access ApplicationProperties via the standalone filter window
        // Slot assignments will use defaults (0,1,2,-1) on first run
    }

    presetBar = std::make_unique<PresetBar>(*presetManager);
    addAndMakeVisible(*presetBar);

    presetBar->onSlotClicked = [this] { refreshAllUI(); };

    presetManager->onPresetLoaded = [this]
    {
        refreshAllUI();
        if (presetBar) presetBar->refreshSlots();
    };

    presetManager->onPresetListChanged = [this]
    {
        if (presetBar) presetBar->refreshSlots();
        if (presetBrowserPanel) presetBrowserPanel->refreshList();
    };

    presetBrowserPanel = std::make_unique<PresetBrowserPanel>(*presetManager);
    addChildComponent(*presetBrowserPanel);

    presetBrowserPanel->onPresetLoaded = [this]
    {
        refreshAllUI();
        if (presetBar) presetBar->refreshSlots();
    };

    // Default to Amp Sim on first load (visual index 2 = Amp Sim)
    chainList.selectEffect(2);
}

MainLayout::~MainLayout()
{
    if (tooltipWindow != nullptr)
        tooltipWindow->setLookAndFeel(nullptr);
    chainList.removeListener(this);
}

void MainLayout::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::background);
    Theme::paintNoise(g, getLocalBounds());
}

void MainLayout::resized()
{
    auto area = getLocalBounds();

    // Chain list on the left
    chainList.setBounds(area.removeFromLeft(Theme::Dims::chainListWidth));

    // Sidebar on the right
    sidebarPanel.setBounds(area.removeFromRight(Theme::Dims::sidebarWidth));

    // PresetBar at the bottom of the detail area (hidden during overlays)
    bool showPresetBar = !settingsPanelVisible && !presetBrowserVisible
                      && !tunerPanelVisible && !metronomePanelVisible;
    if (presetBar != nullptr)
    {
        presetBar->setVisible(showPresetBar);
        if (showPresetBar)
            presetBar->setBounds(area.removeFromBottom(Theme::Dims::presetBarHeight));
    }

    // Detail area: overlays or effect detail panel (fills remaining)
    if (settingsPanelVisible && audioSettingsPanel != nullptr)
        audioSettingsPanel->setBounds(area);

    if (presetBrowserVisible && presetBrowserPanel != nullptr)
        presetBrowserPanel->setBounds(area);

    if (tunerPanelVisible && tunerPanel != nullptr)
        tunerPanel->setBounds(area);

    if (metronomePanelVisible && metronomePanel != nullptr)
        metronomePanel->setBounds(area);

    if (detailPanel != nullptr)
        detailPanel->setBounds(area);
}

void MainLayout::settingsClicked()
{
    settingsPanelVisible = !settingsPanelVisible;

    // Close other overlays
    if (settingsPanelVisible)
    {
        if (presetBrowserVisible) { presetBrowserVisible = false; if (presetBrowserPanel) presetBrowserPanel->setVisible(false); }
        if (tunerPanelVisible)    { tunerPanelVisible = false; processorRef.setTunerActive(false); if (tunerPanel) tunerPanel->setVisible(false); }
        if (metronomePanelVisible){ metronomePanelVisible = false;if (metronomePanel) metronomePanel->setVisible(false); }
    }

    chainList.setSettingsToggle(settingsPanelVisible);

    if (audioSettingsPanel != nullptr)
        audioSettingsPanel->setVisible(settingsPanelVisible);

    // Hide effect detail when any overlay is shown
    bool anyOverlay = settingsPanelVisible || presetBrowserVisible || tunerPanelVisible || metronomePanelVisible;
    if (detailPanel != nullptr)
        detailPanel->setVisible(!anyOverlay);

    resized();
}

void MainLayout::presetsClicked()
{
    presetBrowserVisible = !presetBrowserVisible;

    // Close other overlays
    if (presetBrowserVisible)
    {
        if (settingsPanelVisible) { settingsPanelVisible = false; if (audioSettingsPanel) audioSettingsPanel->setVisible(false); }
        if (tunerPanelVisible)    { tunerPanelVisible = false; processorRef.setTunerActive(false); if (tunerPanel) tunerPanel->setVisible(false); }
        if (metronomePanelVisible){ metronomePanelVisible = false;if (metronomePanel) metronomePanel->setVisible(false); }
    }

    chainList.setPresetsToggle(presetBrowserVisible);

    if (presetBrowserPanel != nullptr)
    {
        presetBrowserPanel->setVisible(presetBrowserVisible);
        if (presetBrowserVisible)
            presetBrowserPanel->refreshList();
    }

    bool anyOverlay = settingsPanelVisible || presetBrowserVisible || tunerPanelVisible || metronomePanelVisible;
    if (detailPanel != nullptr)
        detailPanel->setVisible(!anyOverlay);

    resized();
}

void MainLayout::tunerClicked()
{
    tunerPanelVisible = !tunerPanelVisible;

    // Close other overlays
    if (tunerPanelVisible)
    {
        if (settingsPanelVisible) { settingsPanelVisible = false; if (audioSettingsPanel) audioSettingsPanel->setVisible(false); }
        if (presetBrowserVisible) { presetBrowserVisible = false; if (presetBrowserPanel) presetBrowserPanel->setVisible(false); }
        if (metronomePanelVisible){ metronomePanelVisible = false;if (metronomePanel) metronomePanel->setVisible(false); }
    }

    chainList.setTunerToggle(tunerPanelVisible);

    // Activate/deactivate tuner DSP (mutes output when active)
    processorRef.setTunerActive(tunerPanelVisible);

    if (tunerPanel != nullptr)
        tunerPanel->setVisible(tunerPanelVisible);

    bool anyOverlay = settingsPanelVisible || presetBrowserVisible || tunerPanelVisible || metronomePanelVisible;
    if (detailPanel != nullptr)
        detailPanel->setVisible(!anyOverlay);

    resized();
}

void MainLayout::metronomeClicked()
{
    metronomePanelVisible = !metronomePanelVisible;

    // Close other overlays
    if (metronomePanelVisible)
    {
        if (settingsPanelVisible) { settingsPanelVisible = false; if (audioSettingsPanel) audioSettingsPanel->setVisible(false); }
        if (presetBrowserVisible) { presetBrowserVisible = false; if (presetBrowserPanel) presetBrowserPanel->setVisible(false); }
        if (tunerPanelVisible)    { tunerPanelVisible = false; processorRef.setTunerActive(false); if (tunerPanel) tunerPanel->setVisible(false); }
    }

    chainList.setMetronomeToggle(metronomePanelVisible);

    if (metronomePanel != nullptr)
        metronomePanel->setVisible(metronomePanelVisible);

    bool anyOverlay = settingsPanelVisible || presetBrowserVisible || tunerPanelVisible || metronomePanelVisible;
    if (detailPanel != nullptr)
        detailPanel->setVisible(!anyOverlay);

    resized();
}

void MainLayout::effectSelected(int chainIndex)
{
    // Selecting an effect dismisses all overlays
    if (settingsPanelVisible)
    {
        settingsPanelVisible = false;
        if (audioSettingsPanel != nullptr)
            audioSettingsPanel->setVisible(false);
    }

    if (presetBrowserVisible)
    {
        presetBrowserVisible = false;
        if (presetBrowserPanel != nullptr)
            presetBrowserPanel->setVisible(false);
    }

    if (tunerPanelVisible)
    {
        tunerPanelVisible = false;
        processorRef.setTunerActive(false);
        if (tunerPanel != nullptr)
            tunerPanel->setVisible(false);
    }

    if (metronomePanelVisible)
    {
        metronomePanelVisible = false;
        if (metronomePanel != nullptr)
            metronomePanel->setVisible(false);
    }

    if (detailPanel != nullptr)
    {
        detailPanel->setVisible(true);

        // Translate chain index to fixed panel index
        auto& chain = processorRef.getEffectChain();
        auto* effect = chain.getEffect(chainIndex);
        int panelIndex = (effect != nullptr) ? chainIndexToPanelIndex(effect->getName()) : 0;
        detailPanel->showEffect(panelIndex);
    }

    resized();
}

void MainLayout::effectOrderChanged()
{
    if (presetManager)
        presetManager->clearActivePreset();
}

void MainLayout::refreshAllUI()
{
    // Sync amp sim engine state from processor
    int engine = processorRef.getAmpSimEngine();
    chainList.setActiveAmpEngine(engine);
    if (detailPanel != nullptr)
    {
        if (auto* switcher = detailPanel->getAmpSimSwitcher())
            switcher->setActiveEngine(engine);
    }

    // Sync reverb engine state from processor
    int reverbEngine = processorRef.getReverbEngine();
    chainList.setActiveReverbEngine(reverbEngine);
    if (detailPanel != nullptr)
    {
        if (auto* reverbSwitcher = detailPanel->getReverbSwitcher())
            reverbSwitcher->setActiveEngine(reverbEngine);
    }

    // Sync modulation engine state from processor
    int modulationEngine = processorRef.getModulationEngine();
    chainList.setActiveModulationEngine(modulationEngine);
    if (detailPanel != nullptr)
    {
        if (auto* modulationSwitcher = detailPanel->getModulationSwitcher())
            modulationSwitcher->setActiveEngine(modulationEngine);
    }

    // Rebuild chain list from current order, then refresh bypass states
    chainList.rebuildFromChain();
    chainList.refreshBypassStates();
    if (detailPanel != nullptr)
        detailPanel->syncBypassStates();
}

int MainLayout::chainIndexToPanelIndex(const juce::String& effectName)
{
    // Maps effect display name to its fixed index in EffectDetailPanel::panels[]
    // (panels are always created in default order, with amp sims merged)
    if (effectName == "Noise Gate")   return 0;
    if (effectName == "Diode Drive")  return 1;
    if (effectName == "Distortion")   return 2;
    if (effectName == "Amp Silver")       return 3;
    if (effectName == "Amp Gold")         return 3;  // same panel (switcher)
    if (effectName == "Amp Platinum") return 3;  // same panel (switcher)
    if (effectName == "Delay")          return 4;
    if (effectName == "Spring Reverb")  return 5;
    if (effectName == "Plate Reverb")   return 5;  // same panel (switcher)
    if (effectName == "Chorus")       return 6;
    if (effectName == "Flanger")      return 6;  // same panel (switcher)
    if (effectName == "Phaser")       return 6;  // same panel (switcher)
    if (effectName == "Vibrato")      return 6;  // same panel (switcher)
    if (effectName == "EQ")           return 7;
    return 0;
}
