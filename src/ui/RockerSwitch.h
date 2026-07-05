#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Theme.h"

// Two-legend rocker switch for the amp panels. Both states are always shown;
// the live half lights in the accent colour and the other recesses. Honors
// setEnabled() by dimming, so a channel-irrelevant switch greys like the knobs
// do. state == false picks the first legend, true the second.
class RockerSwitch : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    RockerSwitch() = default;

    void setLabels(juce::String first, juce::String second)
    {
        legendA = std::move(first);
        legendB = std::move(second);
        repaint();
    }

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }

    void setState(bool second, juce::NotificationType notify = juce::dontSendNotification)
    {
        // No-op on an unchanged state so clicking the live half stays inert and
        // syncFromDsp() re-pushes don't fire spurious onChange callbacks.
        if (state == second)
            return;

        state = second;
        repaint();

        if (notify != juce::dontSendNotification && onChange)
            onChange();
    }

    bool getState() const noexcept { return state; }

    std::function<void()> onChange;

    void paint(juce::Graphics& g) override
    {
        const bool enabled = isEnabled();
        auto full = getLocalBounds().toFloat();
        auto bounds = full.reduced(2.0f);        // margin lets the lit half bleed onto the panel
        const float cornerR = 5.0f;
        const float half = bounds.getWidth() * 0.5f;
        auto leftR  = bounds.withWidth(half);
        auto rightR = bounds.withTrimmedLeft(half);
        auto activeR = state ? rightR : leftR;

        if (enabled)
        {
            for (int ring = 3; ring >= 1; --ring)
            {
                const float spread = static_cast<float>(ring) * 1.6f;
                g.setColour(accent.withAlpha(0.07f / static_cast<float>(ring)));
                g.fillRoundedRectangle(activeR.expanded(spread), cornerR + spread);
            }
        }

        // Recessed body base (also stops parent bleed-through at the corners).
        g.setColour(juce::Colour(0xff17140d));
        g.fillRoundedRectangle(bounds, cornerR);

        {
            juce::Graphics::ScopedSaveState clip(g);
            juce::Path body;
            body.addRoundedRectangle(bounds, cornerR);
            g.reduceClipRegion(body);

            drawSegment(g, leftR,  !state, legendA, enabled);
            drawSegment(g, rightR,  state, legendB, enabled);
        }

        g.setColour(juce::Colour(0xff100d08));
        g.fillRect(bounds.getCentreX() - 0.5f, bounds.getY() + 2.0f, 1.0f, bounds.getHeight() - 4.0f);

        if (hovering && enabled)
        {
            g.setColour(juce::Colour(0x0affffff));
            g.fillRoundedRectangle(bounds, cornerR);
        }

        g.setColour(juce::Colour(0xff100d08));
        g.drawRoundedRectangle(bounds, cornerR, 1.0f);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        // Disabled components still receive mouse events in JUCE, so gate here.
        if (!isEnabled() || !getLocalBounds().contains(e.getPosition()))
            return;

        setState(e.position.x >= static_cast<float>(getWidth()) * 0.5f, juce::sendNotification);
    }

    void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hovering = false; repaint(); }
    void enablementChanged() override { repaint(); }

private:
    void drawSegment(juce::Graphics& g, juce::Rectangle<float> r, bool active,
                     const juce::String& text, bool enabled)
    {
        if (active)
        {
            juce::Colour top = enabled ? accent.brighter(0.35f)
                                       : accent.withSaturation(0.35f).withBrightness(0.30f);
            juce::Colour bot = enabled ? accent
                                       : accent.withSaturation(0.30f).withBrightness(0.22f);
            juce::ColourGradient grad(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRect(r);

            g.setColour(juce::Colours::white.withAlpha(enabled ? 0.30f : 0.08f));
            g.drawLine(r.getX() + 2.0f, r.getY() + 1.0f, r.getRight() - 2.0f, r.getY() + 1.0f, 1.0f);
        }
        else
        {
            juce::ColourGradient grad(juce::Colour(0xff2a2518), r.getX(), r.getY(),
                                       juce::Colour(0xff201c12), r.getX(), r.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRect(r);
        }

        juce::Colour textColour = active
            ? (enabled ? juce::Colour(0xff1c1509) : juce::Colour(0xff4a4030))
            : (enabled ? juce::Colour(0xffa09880) : juce::Colour(0xff5a5648));
        g.setColour(textColour);
        g.setFont(juce::Font(juce::FontOptions(11.0f, active ? juce::Font::bold : juce::Font::plain)));
        g.drawText(text, r.toNearestInt(), juce::Justification::centred, false);
    }

    juce::String legendA { "A" }, legendB { "B" };
    juce::Colour accent { Theme::Colours::ampSim };
    bool state = false;
    bool hovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RockerSwitch)
};
