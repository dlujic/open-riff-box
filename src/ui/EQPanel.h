#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Equalizer;

class EQPanel : public juce::Component
{
public:
    explicit EQPanel(Equalizer& eq);
    ~EQPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Equalizer& eqRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    // Left column: Bass / Mid / Treble vertical sliders
    juce::Slider bassKnob;
    juce::Slider midGainKnob;
    juce::Slider trebleKnob;

    juce::Label bassLabel;
    juce::Label midGainLabel;
    juce::Label trebleLabel;

    // Right column: Level vertical slider + Mid Freq horizontal slider
    juce::Slider levelKnob;
    juce::Label  levelLabel;

    juce::Slider midFreqSlider;
    juce::Label  midFreqLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPanel)
};
