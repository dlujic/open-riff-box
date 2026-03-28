#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class DiodeDrive;

class DiodeDrivePanel : public juce::Component
{
public:
    explicit DiodeDrivePanel(DiodeDrive& diodeDrive);
    ~DiodeDrivePanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    DiodeDrive& diodeDriveRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    juce::Slider driveKnob;
    juce::Slider toneKnob;
    juce::Slider levelKnob;

    juce::Label driveLabel;
    juce::Label toneLabel;
    juce::Label levelLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiodeDrivePanel)
};
