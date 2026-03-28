#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OpenRiffBoxProcessor;

class SidebarPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit SidebarPanel(OpenRiffBoxProcessor& processor);
    ~SidebarPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    OpenRiffBoxProcessor& processorRef;

    juce::TextButton powerButton;
    juce::TextButton limiterButton;
    juce::Slider     masterKnob;

    float displayLevelIn  = 0.0f;
    float displayLevelOut = 0.0f;
    float displayCpuLoad  = 0.0f;
    bool  limiterFlash  = false;

    juce::Rectangle<int> meterArea;
    juce::Rectangle<int> jewelArea;
    juce::Rectangle<int> cpuMeterArea;

    void timerCallback() override;
    void paintMeter(juce::Graphics& g);
    void paintJewel(juce::Graphics& g);
    void paintCpuMeter(juce::Graphics& g);

    struct PowerButtonLF : public juce::LookAndFeel_V4
    {
        bool audioActive = false;
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour&, bool isOver, bool isDown) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override {}
    };

    struct LimiterButtonLF : public juce::LookAndFeel_V4
    {
        bool enabled  = true;
        bool clipping = false;
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour&, bool isOver, bool isDown) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override {}
    };

    struct MasterKnobLF : public juce::LookAndFeel_V4
    {
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider& slider) override;
    };

    PowerButtonLF   powerLF;
    LimiterButtonLF limiterLF;
    MasterKnobLF    masterKnobLF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarPanel)
};
