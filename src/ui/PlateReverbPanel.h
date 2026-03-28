#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class PlateReverb;

class PlateReverbPanel : public juce::Component
{
public:
    explicit PlateReverbPanel(PlateReverb& reverb);
    ~PlateReverbPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled    = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel      sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    PlateReverb& reverbRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Left column: Decay / Damping / Mix vertical sliders
    juce::Slider decayKnob;
    juce::Slider dampingKnob;
    juce::Slider mixKnob;

    juce::Label decayLabel;
    juce::Label dampingLabel;
    juce::Label mixLabel;

    // Right column: Pre-Delay / Width vertical sliders
    juce::Slider preDelayKnob;
    juce::Slider widthKnob;

    juce::Label preDelayLabel;
    juce::Label widthLabel;

    // Plate Type ComboBox (bottom row, left side)
    juce::ComboBox plateTypeSelector;
    juce::Label    plateTypeLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlateReverbPanel)
};
