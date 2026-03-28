#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom LookAndFeel for the "Reset" button on effect panels.
// Draws a small, flat, recessed momentary button like those on vintage amp heads.
class ResetButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ResetButtonLookAndFeel() = default;
    ~ResetButtonLookAndFeel() override = default;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const float cornerR = 3.0f;

        // Body colour: dark recessed look
        juce::Colour body(0xff1e1b14);
        if (isButtonDown)
            body = body.darker(0.3f);
        else if (isMouseOverButton)
            body = body.brighter(0.1f);

        g.setColour(body);
        g.fillRoundedRectangle(bounds, cornerR);

        // Recessed bevel: dark on top/left, light on bottom/right (opposite of raised)
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(bounds.getX() + cornerR, bounds.getY(),
                   bounds.getRight() - cornerR, bounds.getY(), 0.5f);
        g.drawLine(bounds.getX(), bounds.getY() + cornerR,
                   bounds.getX(), bounds.getBottom() - cornerR, 0.5f);

        g.setColour(juce::Colours::white.withAlpha(0.06f));
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
            bounds.translate(0.0f, 0.5f);  // subtle press-down shift

        g.setColour(juce::Colour(0xffa09880));  // Theme::Colours::textSecondary
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResetButtonLookAndFeel)
};
