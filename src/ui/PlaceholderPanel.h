#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

class PlaceholderPanel : public juce::Component
{
public:
    PlaceholderPanel(const juce::String& title, const juce::String& subtitle = "Coming Soon")
        : titleText(title), subtitleText(subtitle) {}

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Theme::Colours::panelBackground);
        Theme::paintNoise(g, getLocalBounds());
        Theme::paintBevel(g, getLocalBounds());
        Theme::paintScrews(g, getLocalBounds());

        auto area = getLocalBounds();

        g.setColour(Theme::Colours::textPrimary);
        g.setFont(Theme::Fonts::heading());
        g.drawText(titleText, area.withTrimmedBottom(20), juce::Justification::centred, true);

        g.setColour(Theme::Colours::textSecondary);
        g.setFont(Theme::Fonts::body());
        g.drawText(subtitleText, area.withTrimmedTop(20), juce::Justification::centred, true);
    }

    void resized() override {}

private:
    juce::String titleText;
    juce::String subtitleText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaceholderPanel)
};
