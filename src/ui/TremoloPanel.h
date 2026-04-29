#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Tremolo;

class TremoloPanel : public juce::Component
{
public:
    explicit TremoloPanel(Tremolo& tremolo);
    ~TremoloPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled    = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel       sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel  resetLF;
    Tremolo& tremoloRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    juce::Slider rateKnob;
    juce::Slider depthKnob;
    juce::Slider widthKnob;
    juce::Slider outputKnob;

    juce::Label rateLabel;
    juce::Label depthLabel;
    juce::Label widthLabel;
    juce::Label outputLabel;

    juce::ComboBox modeSelector;
    juce::Label    modeLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TremoloPanel)
};
