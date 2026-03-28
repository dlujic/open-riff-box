#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Phaser;

class PhaserPanel : public juce::Component
{
public:
    explicit PhaserPanel(Phaser& phaser);
    ~PhaserPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Phaser& phaserRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Left group: Rate / Depth
    juce::Slider rateKnob;
    juce::Slider depthKnob;

    juce::Label rateLabel;
    juce::Label depthLabel;

    // Right group: Feedback / Mix
    juce::Slider feedbackKnob;
    juce::Slider mixKnob;

    juce::Label feedbackLabel;
    juce::Label mixLabel;

    // Stages selector (dropdown, positioned in top row next to bypass/reset)
    juce::ComboBox stagesSelector;
    juce::Label    stagesLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaserPanel)
};
