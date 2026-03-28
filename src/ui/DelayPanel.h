#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class AnalogDelay;

class DelayPanel : public juce::Component
{
public:
    explicit DelayPanel(AnalogDelay& delay);
    ~DelayPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    AnalogDelay& delayRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    // Left column: Time / Intensity / Echo vertical sliders
    juce::Slider timeKnob;
    juce::Slider intensityKnob;
    juce::Slider echoKnob;

    juce::Label timeLabel;
    juce::Label intensityLabel;
    juce::Label echoLabel;

    // Right column: Mod Depth / Mod Rate vertical sliders
    juce::Slider modDepthKnob;
    juce::Slider modRateKnob;

    juce::Label modDepthLabel;
    juce::Label modRateLabel;

    // Tone horizontal slider
    juce::Slider toneSlider;
    juce::Label  toneLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPanel)
};
