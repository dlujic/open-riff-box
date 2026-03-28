#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

#include "dsp/Distortion.h"

class DistortionPanel : public juce::Component
{
public:
    explicit DistortionPanel(Distortion& distortion);
    ~DistortionPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Distortion& distortionRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    juce::Slider driveKnob;
    juce::Slider toneKnob;
    juce::Slider levelKnob;

    juce::Label driveLabel;
    juce::Label toneLabel;
    juce::Label levelLabel;

    juce::ComboBox modeSelector;
    juce::Label    modeLabel;

    juce::Slider   mixSlider;
    juce::Label    mixSliderLabel;

    juce::ToggleButton saturateToggle;
    juce::Slider       saturateSlider;

    juce::ToggleButton clipSoftBtn;
    juce::ToggleButton clipNormalBtn;
    juce::ToggleButton clipHardBtn;
    juce::ToggleButton clipXHardBtn;
    juce::Label        clipTypeLabel;

    juce::Rectangle<int> clipBezelBounds;

    void selectClipType(Distortion::ClipType type);
    void updateControlVisibility();
    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionPanel)
};
