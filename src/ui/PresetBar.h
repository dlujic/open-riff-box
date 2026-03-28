#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetBarLookAndFeel.h"
#include <functional>

class PresetManager;

class PresetBar : public juce::Component
{
public:
    explicit PresetBar(PresetManager& manager);
    ~PresetBar() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void refreshSlots();

    std::function<void()> onSlotClicked = [] {};

private:
    PresetManager& presetManager;
    PresetBarLookAndFeel slotLF;

    juce::TextButton slotButtons[4];
    juce::TextButton clearButton { "CLR" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
