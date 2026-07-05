#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"
#include "RockerSwitch.h"

class AmpSimPlatinum;

// UI panel for the Platinum amp engine.
class AmpSimPlatinumPanel : public juce::Component
{
public:
    explicit AmpSimPlatinumPanel(AmpSimPlatinum& ampSimPlatinum);
    ~AmpSimPlatinumPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled    = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel      sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel  resetLF;

    AmpSimPlatinum& platinumRef;

    juce::ToggleButton bypassButton;
    juce::TextButton   resetButton;

    // Left column: Gain / Bass / Mid
    juce::Slider gainKnob;
    juce::Slider bassKnob;
    juce::Slider midKnob;

    juce::Label gainLabel;
    juce::Label bassLabel;
    juce::Label midLabel;

    // Right column: Treble / Mic / (Normal) Level
    juce::Slider trebleKnob;
    juce::Slider micPositionKnob;
    juce::Slider normalLevelKnob;

    juce::Label trebleLabel;
    juce::Label micPositionLabel;
    juce::Label normalLevelLabel;

    // OV Level horizontal slider
    juce::Slider ovLevelSlider;
    juce::Label  ovLevelLabel;

    // Master horizontal slider
    juce::Slider masterSlider;
    juce::Label  masterLabel;

    // Channel (master) / Gain Mode / Boost / Jack rocker switches.
    // Order matches the panel row, left to right.
    RockerSwitch channelSwitch;
    RockerSwitch gainModeSwitch;
    RockerSwitch boostSwitch;
    RockerSwitch jackSwitch;

    // Cabinet ComboBox + custom IR loading
    juce::ComboBox cabinetSelector;
    juce::Label    cabinetLabel;
    juce::TextButton loadIRButton;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Cabinet trim slider
    juce::Slider cabTrimSlider;
    juce::Label  cabTrimLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimPlatinumPanel)
};
