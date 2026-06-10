#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class Compressor;

class CompressorPanel : public juce::Component,
                        private juce::Timer
{
public:
    explicit CompressorPanel(Compressor& comp);
    ~CompressorPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled    = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel       sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel  resetLF;
    Compressor& compRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    juce::Slider sustainKnob;
    juce::Slider attackKnob;
    juce::Slider blendKnob;
    juce::Slider levelKnob;

    juce::Label sustainLabel;
    juce::Label attackLabel;
    juce::Label blendLabel;
    juce::Label levelLabel;

    juce::ComboBox modeSelector;
    juce::Label    modeLabel;

    // GR meter: bar drawn in paint(), updated by 30 Hz timer.
    // displayGrDb: current display value (UI-side decay applied).
    float displayGrDb = 0.0f;

    void timerCallback() override;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPanel)
};
