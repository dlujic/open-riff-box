#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// LookAndFeel for preset slot buttons - 3D foot-pedal style with
// top-down glow for the active slot.
class PresetBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PresetBarLookAndFeel() = default;
    ~PresetBarLookAndFeel() override = default;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour&, bool isOver, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        bool active = button.getToggleState();
        const float cornerR = 5.0f;

        // -- Recess pit (dark cavity the pedal sits in) --
        g.setColour(juce::Colour(0xff0c0a06));
        g.fillRoundedRectangle(bounds, 3.0f);

        auto face = bounds.reduced(1.5f);

        // -- Extrusion shadow (pedal thickness at bottom edge) --
        // Drawn at a fixed offset; when pressed, the face moves toward it
        g.setColour(juce::Colour(0xff100e08));
        g.fillRoundedRectangle(face.translated(0.0f, 3.0f), cornerR);

        // Shift face down when pressed (closer to shadow = less 3D)
        if (isDown) face.translate(0.0f, 1.0f);

        // -- Pedal face (main body gradient) --
        juce::Colour faceTop, faceBot;
        if (active)
        {
            faceTop = juce::Colour(0xff4a4232);
            faceBot = juce::Colour(0xff2e2820);
        }
        else
        {
            faceTop = juce::Colour(0xff3a3428);
            faceBot = juce::Colour(0xff201c14);
        }

        if (isOver && !isDown)
        {
            faceTop = faceTop.brighter(0.08f);
            faceBot = faceBot.brighter(0.04f);
        }
        if (isDown)
        {
            faceTop = faceTop.darker(0.12f);
            faceBot = faceBot.darker(0.06f);
        }

        juce::ColourGradient faceGrad(faceTop, face.getX(), face.getY(),
                                       faceBot, face.getX(), face.getBottom(), false);
        g.setGradientFill(faceGrad);
        g.fillRoundedRectangle(face, cornerR);

        // -- Bevels for 3D depth --
        if (!isDown)
        {
            // Top highlight (light source above)
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.drawLine(face.getX() + cornerR, face.getY() + 1.0f,
                       face.getRight() - cornerR, face.getY() + 1.0f, 1.0f);
            // Second softer highlight line
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.drawLine(face.getX() + cornerR, face.getY() + 2.0f,
                       face.getRight() - cornerR, face.getY() + 2.0f, 1.0f);
        }

        // Left edge highlight
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawLine(face.getX() + 1.0f, face.getY() + cornerR,
                   face.getX() + 1.0f, face.getBottom() - cornerR, 1.0f);

        // Bottom edge shadow
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(face.getX() + cornerR, face.getBottom() - 1.0f,
                   face.getRight() - cornerR, face.getBottom() - 1.0f, 1.0f);

        // Right edge shadow
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.drawLine(face.getRight() - 1.0f, face.getY() + cornerR,
                   face.getRight() - 1.0f, face.getBottom() - cornerR, 1.0f);

        // -- Active glow (colored light from top, fading downward) --
        if (active && !isDown)
        {
            auto grad = juce::ColourGradient::vertical(
                Theme::Colours::goldText.withAlpha(0.21f),
                face.getRelativePoint(0.0f, 0.0f).getY(),
                Theme::Colours::goldText.withAlpha(0.0f), 
                face.getRelativePoint(0.0f, 0.7f).getY());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(face, cornerR);
        }

        // -- Outline --
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawRoundedRectangle(face, cornerR, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*isOver*/, bool /*isDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(6.0f, 2.0f);
        bool active = button.getToggleState();

        auto font = active
            ? juce::Font(juce::FontOptions(11.5f).withTypeface(Theme::Fonts::getInterSemiBold()))
            : Theme::Fonts::small();
        g.setFont(font);

        // Drop shadow for readability on textured surface
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawText(button.getButtonText(), bounds.translated(0.5f, 0.5f),
                   juce::Justification::centred, true);

        // Main text
        g.setColour(active ? Theme::Colours::textPrimary : Theme::Colours::textSecondary);
        g.drawText(button.getButtonText(), bounds,
                   juce::Justification::centred, true);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBarLookAndFeel)
};
