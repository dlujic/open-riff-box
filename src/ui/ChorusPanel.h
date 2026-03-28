#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Chorus;

class ChorusPanel : public juce::Component
{
public:
    explicit ChorusPanel(Chorus& chorus);
    ~ChorusPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Chorus& chorusRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    // Left column: Rate / Depth vertical sliders
    juce::Slider rateKnob;
    juce::Slider depthKnob;

    juce::Label rateLabel;
    juce::Label depthLabel;

    // Right column: EQ / E.Level vertical sliders
    juce::Slider eqKnob;
    juce::Slider eLevelKnob;

    juce::Label eqLabel;
    juce::Label eLevelLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChorusPanel)
};
