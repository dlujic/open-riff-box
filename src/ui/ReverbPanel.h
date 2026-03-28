#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class SpringReverb;

class ReverbPanel : public juce::Component
{
public:
    explicit ReverbPanel(SpringReverb& reverb);
    ~ReverbPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    SpringReverb& reverbRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    // Left column: Dwell / Decay / Mix vertical sliders
    juce::Slider dwellKnob;
    juce::Slider decayKnob;
    juce::Slider mixKnob;

    juce::Label dwellLabel;
    juce::Label decayLabel;
    juce::Label mixLabel;

    // Right column: Drip / Tone vertical sliders
    juce::Slider dripKnob;
    juce::Slider toneKnob;

    juce::Label dripLabel;
    juce::Label toneLabel;

    // Spring Type ComboBox (top row, right side)
    juce::ComboBox springTypeSelector;
    juce::Label    springTypeLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbPanel)
};
