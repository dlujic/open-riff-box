#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "ResetButtonLookAndFeel.h"

class AmpSimSilver;

class AmpSimSilverPanel : public juce::Component
{
public:
    explicit AmpSimSilverPanel(AmpSimSilver& ampSimSilver);
    ~AmpSimSilverPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void syncFromDsp();

    std::function<void()> onBypassToggled = [] {};
    std::function<void()> onParameterChanged = [] {};

private:
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;
    ResetButtonLookAndFeel resetLF;
    AmpSimSilver& ampSimSilverRef;

    juce::ToggleButton bypassButton;
    juce::TextButton resetButton;

    // Left column: amp sliders
    juce::Slider gainKnob;
    juce::Slider bassKnob;
    juce::Slider midKnob;
    juce::Slider trebleKnob;

    juce::Label gainLabel;
    juce::Label bassLabel;
    juce::Label midLabel;
    juce::Label trebleLabel;

    // Right column: cab/speaker sliders
    juce::Slider brightnessKnob;
    juce::Slider micPositionKnob;

    juce::Label brightnessLabel;
    juce::Label micPositionLabel;

    // Cabinet ComboBox + custom IR loading
    juce::ComboBox cabinetSelector;
    juce::Label    cabinetLabel;
    juce::TextButton loadIRButton;
    std::unique_ptr<juce::FileChooser> fileChooser;  // must outlive async callback

    // Cabinet trim slider
    juce::Slider cabTrimSlider;
    juce::Label  cabTrimLabel;

    // Preamp Boost toggle
    juce::ToggleButton preampBoostToggle;

    // Speaker Drive horizontal slider
    juce::Slider speakerDriveSlider;
    juce::Label  speakerDriveLabel;

    void setupKnob(juce::Slider& knob, juce::Label& label,
                   const juce::String& name, double min, double max,
                   double interval);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmpSimSilverPanel)
};
