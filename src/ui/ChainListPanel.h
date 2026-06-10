#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BypassButtonLookAndFeel.h"

class EffectChain;

class ChainListPanel : public juce::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void effectSelected(int index) = 0;
        virtual void effectOrderChanged() = 0;
        virtual void presetsClicked() = 0;
        virtual void settingsClicked() = 0;
        virtual void tunerClicked() = 0;
        virtual void metronomeClicked() = 0;
    };

    explicit ChainListPanel(EffectChain& chain);
    ~ChainListPanel() override;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    void paint(juce::Graphics& g) override;
    void resized() override;

    int getSelectedIndex() const { return selectedIndex; }

    void refreshBypassStates();
    void setPresetsToggle(bool on);
    void setSettingsToggle(bool on);
    void setTunerToggle(bool on);
    void setMetronomeToggle(bool on);

    // Rebuild the chain list entries from the current EffectChain order.
    // Call after chain reorder, preset load, or state restore.
    void rebuildFromChain();

    // Called when bypass is toggled from this panel - MainLayout uses this to sync detail panels
    std::function<void()> onBypassChanged = [] {};

    void selectEffect(int index);

    // Set the active amp sim engine (0=Silver, 1=Gold, 2=Platinum) for bypass display
    void setActiveAmpEngine(int engine);

    // Set the active reverb engine (0=Spring, 1=Plate) for bypass display
    void setActiveReverbEngine(int engine);

    // Set the active modulation engine (0=Chorus, 1=Flanger, 2=Phaser, 3=Vibrato, 4=Tremolo) for bypass display
    void setActiveModulationEngine(int engine);

private:
    struct EffectEntry
    {
        juce::String name;
        juce::Colour colour;
        bool hasDsp;
    };

    BypassButtonLookAndFeel bypassLF;

    // LookAndFeel for small icon buttons (reorder toggle, reset order)
    struct IconButtonLF : public juce::LookAndFeel_V4
    {
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                   const juce::Colour&, bool highlighted, bool down) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override {}
    } iconLF;

    EffectChain& effectChain;
    std::vector<EffectEntry> entries;
    int selectedIndex = -1;
    bool presetsActive = false;
    bool settingsActive = false;
    bool tunerActive = false;
    bool metronomeActive = false;

    // Visual-to-chain index mapping (entries skip "Amp Gold", "Amp Platinum", "Plate Reverb", "Flanger", "Phaser", "Vibrato", "Tremolo")
    static constexpr int kMaxSlots = 16;      // max chain effects (16 with Compressor)
    static constexpr int kNumVisualSlots = 9; // displayed rows (AmpGold, Platinum, PlateReverb, Flanger, Phaser, Vibrato, Tremolo hidden)
    int visualToChain[kMaxSlots] = {};
    int numVisualSlots = 0;

    // Active amp engine for bypass button targeting (0=Silver, 1=Gold, 2=Platinum)
    int activeAmpEngine = 1;

    // Active reverb engine for bypass button targeting (0=Spring, 1=Plate)
    int activeReverbEngine = 0;

    // Active modulation engine for bypass button targeting (0=Chorus, 1=Flanger, 2=Phaser, 3=Vibrato, 4=Tremolo)
    int activeModulationEngine = 0;

    // Reorder mode
    bool reorderMode = false;
    juce::TextButton reorderToggle;
    juce::TextButton resetOrderButton;
    juce::TextButton moveUpButtons[kNumVisualSlots];
    juce::TextButton moveDownButtons[kNumVisualSlots];

    juce::ToggleButton bypassButtons[kNumVisualSlots];
    juce::ToggleButton tunerButton;
    juce::ToggleButton metronomeButton;
    juce::ToggleButton presetsButton;
    juce::ToggleButton settingsButton;

    juce::ListenerList<Listener> listeners;

    // Helper to clear all overlay states (presets, settings, tuner, metronome)
    void clearAllOverlays();

    // Returns the y-range of the currently selected/active row for notebook tab painting
    juce::Range<int> getActiveRowYRange() const;

    // Maps effect display name to its category colour
    static juce::Colour getColourForEffect(const juce::String& name);

    // Update reorder mode button visibility
    void updateReorderButtons();

    // Returns the display name of the active modulation engine
    juce::String activeModulationEffectName() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainListPanel)
};
