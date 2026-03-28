#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom LookAndFeel for engine selector buttons (Silver/Gold/Platinum, Spring/Plate).
// Draws a flat recessed button with small font, matching the panel aesthetic.
class EngineSelectorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EngineSelectorLookAndFeel() = default;
    ~EngineSelectorLookAndFeel() override = default;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float cornerR = 3.0f;

        juce::Colour body = backgroundColour;
        if (isButtonDown)
            body = body.darker(0.3f);
        else if (isMouseOverButton)
            body = body.brighter(0.15f);

        g.setColour(body);
        g.fillRoundedRectangle(bounds, cornerR);

        // Subtle recessed bevel
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.drawLine(bounds.getX() + cornerR, bounds.getY(),
                   bounds.getRight() - cornerR, bounds.getY(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getY() + cornerR,
                   bounds.getX(), bounds.getBottom() - cornerR, 0.5f);

        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawLine(bounds.getX() + cornerR, bounds.getBottom(),
                   bounds.getRight() - cornerR, bounds.getBottom(), 0.5f);
        g.drawLine(bounds.getRight(), bounds.getY() + cornerR,
                   bounds.getRight(), bounds.getBottom() - cornerR, 0.5f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        if (isButtonDown)
            bounds.translate(0.0f, 0.5f);

        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EngineSelectorLookAndFeel)
};
