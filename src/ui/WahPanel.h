#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Wah;

class WahPanel : public juce::Component
{
public:
    explicit WahPanel(Wah& wah);
    ~WahPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    Wah& wahRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Sweep position (toe=0 bright, heel=1 dark); role/label depends on Mode
    juce::Slider positionKnob;
    juce::Label  positionLabel;

    // Control source (fork #4)
    juce::ComboBox modeSelector;
    juce::Label    modeLabel;

    // Auto mode: LFO waveform + rate/depth
    juce::ComboBox waveSelector;
    juce::Label    waveLabel;
    juce::Slider   rateKnob;
    juce::Label    rateLabel;
    juce::Slider   depthKnob;
    juce::Label    depthLabel;

    // Envelope mode: sensitivity + attack/release
    juce::Slider sensKnob;
    juce::Label  sensLabel;
    juce::Slider attackKnob;
    juce::Label  attackLabel;
    juce::Slider releaseKnob;
    juce::Label  releaseLabel;

    // Coloration toggle (fork #2, default off)
    juce::ToggleButton colorationToggle;

    // DeadZones / Linear taper select (fork #3)
    juce::ComboBox taperSelector;
    juce::Label    taperLabel;

    void updateControlVisibility();
    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WahPanel)
};
