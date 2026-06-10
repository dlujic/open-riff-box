#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SliderLookAndFeel.h"
#include "BypassButtonLookAndFeel.h"
#include "dsp/MetronomeEngine.h"

// Overlay panel for the metronome -- standalone-only.
// Transport is independent of panel visibility: closing the overlay
// does not stop the click. Start/stop lives here.
class MetronomePanel : public juce::Component,
                       private juce::Timer
{
public:
    explicit MetronomePanel(MetronomeEngine& engine);
    ~MetronomePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    MetronomeEngine& metronome;

    // BPM display + steppers
    juce::Label  bpmDisplay;
    juce::TextButton bpmDownButton;
    juce::TextButton bpmUpButton;

    // Quick-set chips for common practice tempos
    juce::OwnedArray<juce::TextButton> presetChips;

    // Start/Stop -- jewel pilot lamp, same idiom as panel-header bypass buttons
    juce::ToggleButton startStopButton;

    // Tap tempo
    juce::TextButton tapButton;

    // Time signature selector
    juce::ComboBox timeSigSelector;
    juce::Label    timeSigLabel;

    // Click volume
    juce::Slider   volumeSlider;
    juce::Label    volumeLabel;

    // --- Tap tempo state (message thread only) ---
    static constexpr int kTapHistorySize = 4;
    double tapTimes[kTapHistorySize] = {};
    int    tapCount = 0;
    double lastTapTime = 0.0;

    // Low-rate timer keeps the start/stop button in sync with engine state.
    void timerCallback() override;

    void updateBpmDisplay();
    void handleTap();
    void applyTimeSigSelection();

    // Button styles: plain textured look using ResetButtonLookAndFeel base.
    struct ActionButtonLF : public juce::LookAndFeel_V4
    {
        juce::Colour bodyColour { 0xff2e2a20 };
        juce::Colour textColour { 0xfff0e8d8 };
        float fontSize = 13.0f;

        void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                  const juce::Colour&, bool over, bool down) override;
        void drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                            bool over, bool down) override;
    } actionLF;

    // Shared themed LFs (same as detail panels).
    SliderLookAndFeel sliderLF;
    BypassButtonLookAndFeel bypassLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetronomePanel)
};
