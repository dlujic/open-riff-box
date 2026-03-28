#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Flanger;

class FlangerPanel : public juce::Component
{
public:
    explicit FlangerPanel(Flanger& flanger);
    ~FlangerPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Flanger& flangerRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Left group: Rate / Depth
    juce::Slider rateKnob;
    juce::Slider depthKnob;

    juce::Label rateLabel;
    juce::Label depthLabel;

    // Middle group: Manual / Feedback
    juce::Slider manualKnob;
    juce::Slider feedbackKnob;

    juce::Label manualLabel;
    juce::Label feedbackLabel;

    // Polarity selector (dropdown, below sliders)
    juce::ComboBox polaritySelector;
    juce::Label    polarityLabel;

    // Right group: Tone / Mix
    juce::Slider eqKnob;
    juce::Slider mixKnob;

    juce::Label eqLabel;
    juce::Label mixLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FlangerPanel)
};
