#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/TunerEngine.h"

class TunerPanel : public juce::Component,
                   private juce::Timer
{
public:
    explicit TunerPanel(TunerEngine& engine);
    ~TunerPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    TunerEngine& tunerRef;

    // Smoothed display values
    float displayCents = 0.0f;
    float displayConfidence = 0.0f;
    int   displayNote = -1;
    int   displayOctave = -1;
    float displayFrequency = 0.0f;
    const char* displayNoteName = "";

    void timerCallback() override;

    void paintMeterFace(juce::Graphics& g, juce::Rectangle<float> area);
    void paintNeedle(juce::Graphics& g, juce::Rectangle<float> area, float cents);
    void paintNoteDisplay(juce::Graphics& g, juce::Rectangle<float> area);
    void paintInfoDisplay(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerPanel)
};
