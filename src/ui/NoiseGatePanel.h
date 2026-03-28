#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class NoiseGate;

class NoiseGatePanel : public juce::Component
{
public:
    explicit NoiseGatePanel(NoiseGate& gate);
    ~NoiseGatePanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    NoiseGate& noiseGate;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    juce::Slider thresholdKnob;
    juce::Slider attackKnob;
    juce::Slider holdKnob;
    juce::Slider releaseKnob;
    juce::Slider rangeKnob;

    juce::Label thresholdLabel;
    juce::Label attackLabel;
    juce::Label holdLabel;
    juce::Label releaseLabel;
    juce::Label rangeLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGatePanel)
};
