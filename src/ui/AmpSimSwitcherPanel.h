#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "EngineSelectorLookAndFeel.h"

class AmpSimSilver;
class AmpSimGold;
class AmpSimPlatinum;
class AmpSimSilverPanel;
class AmpSimGoldPanel;
class AmpSimPlatinumPanel;

// Wrapper panel that shows Silver (AmpSimSilver), Gold (AmpSimGold), or Platinum (AmpSimPlatinum).
// Engine selector buttons sit in the right-side dead space next to the sliders.
// All three DSP engines remain in the chain; the inactive ones are bypassed.
class AmpSimSwitcherPanel : public juce::Component
{
public:
    AmpSimSwitcherPanel(AmpSimSilver& ampSimSilver, AmpSimGold& ampSimGold, AmpSimPlatinum& ampSimPlatinum);
    ~AmpSimSwitcherPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    // 0 = Silver (AmpSimSilver), 1 = Gold (AmpSimGold), 2 = Platinum (AmpSimPlatinum)
    void setActiveEngine(int engine);
    int  getActiveEngine() const { return activeEngine; }

    std::function<void()>    onBypassToggled    = [] {};
    std::function<void()>    onParameterChanged = [] {};
    std::function<void(int)> onEngineChanged    = [](int) {};

private:
    int activeEngine = 1;  // default Gold

    std::unique_ptr<AmpSimSilverPanel>    silverPanel;
    std::unique_ptr<AmpSimGoldPanel>      goldPanel;
    std::unique_ptr<AmpSimPlatinumPanel>  platinumPanel;

    juce::TextButton silverTab;
    juce::TextButton goldTab;
    juce::TextButton platinumTab;

    // Button layout: right-side column overlaying the slider dead space
    static constexpr int btnWidth   = 76;
    static constexpr int btnHeight  = 56;
    static constexpr int btnSpacing = 8;

    EngineSelectorLookAndFeel btnLF;

    void updateTabAppearance();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimSwitcherPanel)
};
