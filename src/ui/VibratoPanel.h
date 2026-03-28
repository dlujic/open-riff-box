#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Vibrato;

class VibratoPanel : public juce::Component
{
public:
    explicit VibratoPanel(Vibrato& vibrato);
    ~VibratoPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Vibrato& vibratoRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Rate / Depth / Tone
    juce::Slider rateKnob;
    juce::Slider depthKnob;
    juce::Slider toneKnob;

    juce::Label rateLabel;
    juce::Label depthLabel;
    juce::Label toneLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibratoPanel)
};
