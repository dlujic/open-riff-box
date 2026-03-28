#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom LookAndFeel for effect bypass toggle buttons.
// Draws a fat rounded-rectangle button with a glowing LED indicator.
// Set the LED colour per-button via: button.getProperties().set("ledColour", (int) colour.getARGB());
class BypassButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BypassButtonLookAndFeel() = default;
    ~BypassButtonLookAndFeel() override = default;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool isOn = button.getToggleState();
        const bool enabled = button.isEnabled();

        // Retrieve per-button LED colour (default to green)
        juce::Colour ledColour(0xff33aa33);
        if (auto* val = button.getProperties()["ledColour"].getArray(); val == nullptr)
        {
            auto raw = button.getProperties()["ledColour"];
            if (!raw.isVoid())
                ledColour = juce::Colour(static_cast<juce::uint32>((int64_t)raw));
        }

        // --- Button body (3D rounded rectangle) ---
        const float cornerR = 5.0f;

        bool selected = button.getProperties().getWithDefault("selected", false);
        bool panelHeader = (bool)button.getProperties().getWithDefault("panelHeader", false);

        juce::Colour bodyTop, bodyBot;
        if (!enabled)
        {
            bodyTop = juce::Colour(0xff2a2720);
            bodyBot = juce::Colour(0xff221f18);
        }
        else if (isOn)
        {
            bodyTop = juce::Colour(0xff3e3828);
            bodyBot = juce::Colour(0xff2a2720);
        }
        else
        {
            bodyTop = juce::Colour(0xff353020);
            bodyBot = juce::Colour(0xff262219);
        }

        if (shouldDrawButtonAsHighlighted && enabled)
        {
            bodyTop = bodyTop.brighter(0.08f);
            bodyBot = bodyBot.brighter(0.08f);
        }

        // Fill full bounds first to prevent parent bleed-through
        g.setColour(bodyBot);
        g.fillRect(button.getLocalBounds());

        juce::ColourGradient bodyGrad(bodyTop, bounds.getX(), bounds.getY(),
                                       bodyBot, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(bounds, cornerR);

        // Top highlight
        g.setColour(juce::Colour(0x10ffffff));
        g.drawLine(bounds.getX() + cornerR, bounds.getY() + 1.0f,
                   bounds.getRight() - cornerR, bounds.getY() + 1.0f, 0.5f);

        // --- Mode flags ---
        bool noLed = (bool)button.getProperties().getWithDefault("noLed", false);

        if (panelHeader)
        {
            // ============================================================
            // PANEL HEADER: Jewel lamp style
            // Large pilot light (Fender amp jewel) + bold effect name.
            // Visually distinct from chain list LEDs.
            // ============================================================
            const float jewelRadius = 8.0f;
            const float jewelX = bounds.getX() + 16.0f;
            const float jewelY = bounds.getCentreY();

            // Jewel bezel (dark metallic ring)
            g.setColour(juce::Colour(0xff2a2518));
            g.fillEllipse(jewelX - jewelRadius - 2.0f, jewelY - jewelRadius - 2.0f,
                          (jewelRadius + 2.0f) * 2.0f, (jewelRadius + 2.0f) * 2.0f);
            g.setColour(juce::Colour(0xff4a4430));
            g.drawEllipse(jewelX - jewelRadius - 2.0f, jewelY - jewelRadius - 2.0f,
                          (jewelRadius + 2.0f) * 2.0f, (jewelRadius + 2.0f) * 2.0f, 1.0f);

            if (isOn && enabled)
            {
                // Wide ambient glow (light spilling onto panel surface)
                for (int ring = 4; ring >= 1; --ring)
                {
                    float spread = jewelRadius + static_cast<float>(ring) * 7.0f;
                    float alpha = 0.06f / static_cast<float>(ring);
                    g.setColour(ledColour.withAlpha(alpha));
                    g.fillEllipse(jewelX - spread, jewelY - spread, spread * 2.0f, spread * 2.0f);
                }

                // Inner halo
                g.setColour(ledColour.withAlpha(0.25f));
                g.fillEllipse(jewelX - jewelRadius - 4.0f, jewelY - jewelRadius - 4.0f,
                              (jewelRadius + 4.0f) * 2.0f, (jewelRadius + 4.0f) * 2.0f);

                // Jewel body (radial-ish gradient via layered fills)
                g.setColour(ledColour);
                g.fillEllipse(jewelX - jewelRadius, jewelY - jewelRadius,
                              jewelRadius * 2.0f, jewelRadius * 2.0f);

                // Bright centre
                g.setColour(ledColour.brighter(0.7f));
                g.fillEllipse(jewelX - jewelRadius * 0.5f, jewelY - jewelRadius * 0.5f,
                              jewelRadius, jewelRadius);

                // Specular highlight (glass reflection)
                g.setColour(juce::Colour(0x99ffffff));
                g.fillEllipse(jewelX - 3.0f, jewelY - jewelRadius + 2.0f, 6.0f, 4.0f);
            }
            else
            {
                // Dark jewel (off state — still visible as a recessed gem)
                g.setColour(enabled ? ledColour.withAlpha(0.15f) : juce::Colour(0xff2a2518));
                g.fillEllipse(jewelX - jewelRadius, jewelY - jewelRadius,
                              jewelRadius * 2.0f, jewelRadius * 2.0f);

                // Subtle inner edge
                g.setColour(juce::Colour(0xff3a3428));
                g.drawEllipse(jewelX - jewelRadius + 1.0f, jewelY - jewelRadius + 1.0f,
                              (jewelRadius - 1.0f) * 2.0f, (jewelRadius - 1.0f) * 2.0f, 0.5f);
            }

            // Effect name (left) + status text (right)
            auto textArea = bounds.withTrimmedLeft(36.0f);

            if (isOn && enabled)
            {
                g.setColour(juce::Colour(0xffe0d8c0));
                g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
                g.drawText(button.getButtonText(), textArea, juce::Justification::centredLeft, true);

                // "ON" right-aligned
                g.setColour(ledColour.brighter(0.3f));
                g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
                g.drawText("ON", textArea.withTrimmedRight(4.0f), juce::Justification::centredRight, true);
            }
            else
            {
                g.setColour(juce::Colour(0xff807868));
                g.setFont(juce::Font(juce::FontOptions(14.5f, juce::Font::bold)));
                g.drawText(button.getButtonText(), textArea, juce::Justification::centredLeft, true);

                // "BYPASSED" right-aligned
                g.setColour(juce::Colour(0xff585048));
                g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::plain)));
                g.drawText("BYPASSED", textArea.withTrimmedRight(4.0f), juce::Justification::centredRight, true);
            }

            // Mouseover: subtle brighten
            if (shouldDrawButtonAsHighlighted && enabled)
            {
                g.setColour(juce::Colour(0x08ffffff));
                g.fillRoundedRectangle(bounds, cornerR);
            }
        }
        else
        {
            // ============================================================
            // CHAIN LIST: Original LED + text style
            // ============================================================
            if (!noLed)
            {
                // --- LED indicator ---
                const float ledRadius = 5.0f;
                const float ledX = bounds.getX() + 12.0f;
                const float ledY = bounds.getCentreY();

                if (isOn && enabled)
                {
                    // Outer soft glow (light bleeding onto surface)
                    for (int ring = 3; ring >= 1; --ring)
                    {
                        float spread = ledRadius + static_cast<float>(ring) * 4.0f;
                        float alpha = 0.08f / static_cast<float>(ring);
                        g.setColour(ledColour.withAlpha(alpha));
                        g.fillEllipse(ledX - spread, ledY - spread, spread * 2.0f, spread * 2.0f);
                    }

                    // Inner glow halo
                    g.setColour(ledColour.withAlpha(0.3f));
                    g.fillEllipse(ledX - ledRadius - 3.0f, ledY - ledRadius - 3.0f,
                                  (ledRadius + 3.0f) * 2.0f, (ledRadius + 3.0f) * 2.0f);

                    // Bright LED body
                    juce::ColourGradient ledGrad(ledColour.brighter(0.5f), ledX, ledY - ledRadius,
                                                  ledColour, ledX, ledY + ledRadius, false);
                    g.setGradientFill(ledGrad);
                    g.fillEllipse(ledX - ledRadius, ledY - ledRadius,
                                  ledRadius * 2.0f, ledRadius * 2.0f);

                    // Specular highlight dot
                    g.setColour(juce::Colour(0x88ffffff));
                    g.fillEllipse(ledX - 2.0f, ledY - ledRadius + 1.5f, 4.0f, 3.0f);
                }
                else
                {
                    // Dark/off LED
                    g.setColour(enabled ? juce::Colour(0xff353020) : juce::Colour(0xff2a2518));
                    g.fillEllipse(ledX - ledRadius, ledY - ledRadius,
                                  ledRadius * 2.0f, ledRadius * 2.0f);

                    // Subtle ring
                    g.setColour(juce::Colour(0xff4a4430));
                    g.drawEllipse(ledX - ledRadius, ledY - ledRadius,
                                  ledRadius * 2.0f, ledRadius * 2.0f, 0.5f);
                }
            }

            // --- Selection indicator (left accent bar) ---
            if (selected)
            {
                // Subtle background brighten for the whole selected row
                g.setColour(juce::Colour(0x0cffffff));
                g.fillRoundedRectangle(bounds, cornerR);

                // Glowing left-edge accent bar in the effect's category colour
                const float barW = 3.0f;
                auto barRect = juce::Rectangle<float>(
                    bounds.getX(), bounds.getY() + 3.0f,
                    barW, bounds.getHeight() - 6.0f);

                // Outer glow (soft bleed onto button surface)
                for (int ring = 3; ring >= 1; --ring)
                {
                    float spread = static_cast<float>(ring) * 3.0f;
                    float alpha = 0.06f / static_cast<float>(ring);
                    g.setColour(ledColour.withAlpha(alpha));
                    g.fillRect(barRect.expanded(spread, spread * 0.5f));
                }

                // Solid bar
                g.setColour(ledColour.withAlpha(0.85f));
                g.fillRoundedRectangle(barRect, 1.5f);

                // Bright center line
                g.setColour(ledColour.brighter(0.4f).withAlpha(0.6f));
                g.fillRoundedRectangle(barRect.reduced(0.5f, 2.0f), 1.0f);
            }

            // --- Label text ---
            auto textArea = noLed ? bounds.withTrimmedLeft(10.0f) : bounds.withTrimmedLeft(26.0f);

            if (selected)
            {
                g.setColour(juce::Colour(0xffe0d8c0));  // bright cream — selected
                g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
            }
            else
            {
                g.setColour(isOn && enabled ? juce::Colour(0xffc0b8a0) : juce::Colour(0xff807868));
                g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::plain)));
            }

            g.drawText(button.getButtonText(), textArea, juce::Justification::centredLeft, true);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BypassButtonLookAndFeel)
};
