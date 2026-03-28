#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "EngineSelectorLookAndFeel.h"

class SpringReverb;
class PlateReverb;
class ReverbPanel;
class PlateReverbPanel;

// Wrapper panel that shows Spring or Plate reverb via toggle tabs.
// Both DSP engines remain in the chain; the inactive one is bypassed.
class ReverbSwitcherPanel : public juce::Component
{
public:
    ReverbSwitcherPanel(SpringReverb& springReverb, PlateReverb& plateReverb);
    ~ReverbSwitcherPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    // 0 = Spring, 1 = Plate
    void setActiveEngine(int engine);
    int  getActiveEngine() const { return activeEngine; }

    std::function<void()>    onBypassToggled    = [] {};
    std::function<void()>    onParameterChanged = [] {};
    std::function<void(int)> onEngineChanged    = [](int) {};

private:
    int activeEngine = 0;  // default Spring

    std::unique_ptr<ReverbPanel>      springPanel;
    std::unique_ptr<PlateReverbPanel> platePanel;

    juce::TextButton springTab;
    juce::TextButton plateTab;

    // Button layout: right-side column overlaying the slider dead space
    static constexpr int btnWidth   = 76;
    static constexpr int btnHeight  = 56;
    static constexpr int btnSpacing = 8;

    EngineSelectorLookAndFeel btnLF;

    void updateTabAppearance();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbSwitcherPanel)
};
