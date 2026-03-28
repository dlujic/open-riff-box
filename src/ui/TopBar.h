#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class TopBar : public juce::Component
{
public:
    TopBar();
    ~TopBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TopBar)
};
