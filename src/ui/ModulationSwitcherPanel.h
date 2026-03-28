#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "EngineSelectorLookAndFeel.h"

class Chorus;
class Flanger;
class Phaser;
class Vibrato;
class ChorusPanel;
class FlangerPanel;
class PhaserPanel;
class VibratoPanel;

// Wrapper panel that shows Chorus, Flanger, or Phaser via toggle tabs.
// All DSP engines remain in the chain; the inactive ones are bypassed.
class ModulationSwitcherPanel : public juce::Component
{
public:
    ModulationSwitcherPanel(Chorus& chorus, Flanger& flanger, Phaser& phaser, Vibrato& vibrato);
    ~ModulationSwitcherPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    // 0 = Chorus, 1 = Flanger, 2 = Phaser, 3 = Vibrato
    void setActiveEngine(int engine);
    int  getActiveEngine() const { return activeEngine; }

    std::function<void()>    onBypassToggled    = [] {};
    std::function<void()>    onParameterChanged = [] {};
    std::function<void(int)> onEngineChanged    = [](int) {};

private:
    int activeEngine = 0;  // default Chorus

    std::unique_ptr<ChorusPanel>  chorusPanel;
    std::unique_ptr<FlangerPanel> flangerPanel;
    std::unique_ptr<PhaserPanel>  phaserPanel;
    std::unique_ptr<VibratoPanel> vibratoPanel;

    juce::TextButton chorusTab;
    juce::TextButton flangerTab;
    juce::TextButton phaserTab;
    juce::TextButton vibratoTab;

    // Button layout: right-side column overlaying the slider dead space
    static constexpr int btnWidth   = 76;
    static constexpr int btnHeight  = 46;
    static constexpr int btnSpacing = 6;

    EngineSelectorLookAndFeel btnLF;

    void updateTabAppearance();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationSwitcherPanel)
};
