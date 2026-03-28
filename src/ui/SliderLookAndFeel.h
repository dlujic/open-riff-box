#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Custom LookAndFeel for vertical sliders with a fat rounded-rectangle thumb
// and 3D shading (light accent on top, shadow on bottom).
class SliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SliderLookAndFeel()
    {
        // Slider thumb / track colours (Vox cream)
        setColour(juce::Slider::thumbColourId,             juce::Colour(0xffd4c8a0));
        setColour(juce::Slider::trackColourId,             juce::Colour(0xffd4c8a0));
        setColour(juce::Slider::backgroundColourId,        juce::Colour(0xff1a1814));

        // Slider text box colours
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xffc0b8a0));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff262219));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xff1a1814));
        setColour(juce::Slider::textBoxHighlightColourId,  juce::Colour(0xff3e3828));

        // TextEditor colours (when editing slider values)
        setColour(juce::TextEditor::backgroundColourId,       juce::Colour(0xff262219));
        setColour(juce::TextEditor::textColourId,              juce::Colour(0xffc0b8a0));
        setColour(juce::TextEditor::outlineColourId,           juce::Colour(0xff1a1814));
        setColour(juce::TextEditor::focusedOutlineColourId,    juce::Colour(0xff3e3828));
        setColour(juce::TextEditor::highlightColourId,         juce::Colour(0xff3e3828));
        setColour(juce::TextEditor::highlightedTextColourId,   juce::Colour(0xffe0d8c0));

        // Label colours (slider text boxes are Labels)
        setColour(juce::Label::textColourId,       juce::Colour(0xffc0b8a0));
        setColour(juce::Label::backgroundColourId, juce::Colour(0x00000000));
        setColour(juce::Label::outlineColourId,    juce::Colour(0x00000000));

        // CaretComponent
        setColour(juce::CaretComponent::caretColourId, juce::Colour(0xffc0b8a0));

        // ComboBox colours (for mode selectors etc. sharing this LF)
        setColour(juce::ComboBox::backgroundColourId,       Theme::Colours::panelBackground.brighter(0.1f));
        setColour(juce::ComboBox::textColourId,              Theme::Colours::textPrimary);
        setColour(juce::ComboBox::outlineColourId,           Theme::Colours::border);
        setColour(juce::ComboBox::arrowColourId,             Theme::Colours::textSecondary);
        setColour(juce::ComboBox::focusedOutlineColourId,    Theme::Colours::border.brighter(0.2f));

        // PopupMenu colours
        setColour(juce::PopupMenu::backgroundColourId,           Theme::Colours::panelBackground);
        setColour(juce::PopupMenu::textColourId,                  Theme::Colours::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::Colours::selectedRow);
        setColour(juce::PopupMenu::highlightedTextColourId,       Theme::Colours::textPrimary);
    }

    ~SliderLookAndFeel() override = default;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            drawHorizontalSlider(g, x, y, width, height, sliderPos, slider);
            return;
        }

        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height,
                                              sliderPos, 0, 0, style, slider);
            return;
        }

        const float trackWidth = 4.0f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float topY = static_cast<float>(y);
        const float bottomY = static_cast<float>(y + height);

        // --- Tick marks (amp-style notch lines) ---
        const int numTicks = 11;  // 0 through 10
        const float tickLength = 6.0f;
        const float tickRight = centreX - trackWidth * 0.5f - 3.0f;

        for (int i = 0; i < numTicks; ++i)
        {
            float frac = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            float tickY = bottomY - frac * (bottomY - topY);

            bool isMajor = (i % 5 == 0);  // 0, 5, 10 are major ticks
            float alpha = isMajor ? 0.35f : 0.18f;
            float thickness = isMajor ? 1.2f : 0.8f;
            float len = isMajor ? tickLength : tickLength * 0.6f;

            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(tickRight - len, tickY, tickRight, tickY, thickness);
        }

        // --- Track (routed groove/slot) ---
        const float grooveWidth = 8.0f;
        auto grooveBounds = juce::Rectangle<float>(centreX - grooveWidth * 0.5f, topY,
                                                    grooveWidth, bottomY - topY);

        // Dark recessed fill
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRoundedRectangle(grooveBounds, 3.0f);

        // Inner shadow on left+top edges (depth illusion)
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawLine(grooveBounds.getX() + 1.0f, grooveBounds.getY() + 3.0f,
                   grooveBounds.getX() + 1.0f, grooveBounds.getBottom() - 3.0f, 1.0f);
        g.drawLine(grooveBounds.getX() + 3.0f, grooveBounds.getY() + 1.0f,
                   grooveBounds.getRight() - 3.0f, grooveBounds.getY() + 1.0f, 1.0f);

        // Light edge on right+bottom (catch light)
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(grooveBounds.getRight() - 1.0f, grooveBounds.getY() + 3.0f,
                   grooveBounds.getRight() - 1.0f, grooveBounds.getBottom() - 3.0f, 1.0f);
        g.drawLine(grooveBounds.getX() + 3.0f, grooveBounds.getBottom() - 1.0f,
                   grooveBounds.getRight() - 3.0f, grooveBounds.getBottom() - 1.0f, 1.0f);

        // Filled portion below thumb (active range)
        auto filledBounds = juce::Rectangle<float>(centreX - grooveWidth * 0.5f + 1.5f, sliderPos,
                                                    grooveWidth - 3.0f, bottomY - sliderPos);
        g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(0.35f));
        g.fillRoundedRectangle(filledBounds, 2.0f);

        // Groove outer border
        g.setColour(juce::Colour(0xff2a2518));
        g.drawRoundedRectangle(grooveBounds, 3.0f, 0.8f);

        // --- Thumb (fat rounded rectangle with 3D shading) ---
        const float thumbW = 32.0f;
        const float thumbH = 16.0f;
        const float cornerR = 4.0f;

        auto thumbBounds = juce::Rectangle<float>(centreX - thumbW * 0.5f,
                                                    sliderPos - thumbH * 0.5f,
                                                    thumbW, thumbH);

        // Base gradient: light top → dark bottom (3D depth)
        juce::ColourGradient bodyGrad(juce::Colour(0xffbbbbbb), thumbBounds.getX(), thumbBounds.getY(),
                                       juce::Colour(0xff666666), thumbBounds.getX(), thumbBounds.getBottom(),
                                       false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(thumbBounds, cornerR);

        // Top highlight line (light accent for 3D pop)
        g.setColour(juce::Colour(0x55ffffff));
        g.drawLine(thumbBounds.getX() + cornerR, thumbBounds.getY() + 1.5f,
                   thumbBounds.getRight() - cornerR, thumbBounds.getY() + 1.5f, 1.0f);

        // Centre groove line (like a real fader cap)
        g.setColour(juce::Colour(0x33000000));
        const float centreY = thumbBounds.getCentreY();
        g.drawLine(thumbBounds.getX() + 6.0f, centreY,
                   thumbBounds.getRight() - 6.0f, centreY, 1.0f);

        // Bottom shadow edge
        g.setColour(juce::Colour(0x33000000));
        g.drawLine(thumbBounds.getX() + cornerR, thumbBounds.getBottom() - 1.0f,
                   thumbBounds.getRight() - cornerR, thumbBounds.getBottom() - 1.0f, 0.5f);

        // Outer border
        g.setColour(juce::Colour(0xff444444));
        g.drawRoundedRectangle(thumbBounds, cornerR, 1.0f);
    }

    //======================================================================
    // Slider text box — recessed value display
    //======================================================================
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(juce::Font(juce::FontOptions(11.0f)));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffc0b8a0));
        label->setColour(juce::Label::backgroundColourId, juce::Colour(0x00000000));
        label->setColour(juce::Label::outlineColourId, juce::Colour(0x00000000));
        label->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff262219));
        label->setColour(juce::TextEditor::textColourId, juce::Colour(0xffc0b8a0));
        label->setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1a1814));
        label->setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff3e3828));
        label->setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xffe0d8c0));
        return label;
    }

    //======================================================================
    // ComboBox — recessed hardware-style selector
    //======================================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                              static_cast<float>(width),
                                              static_cast<float>(height));
        const float cornerR = 4.0f;

        // Recessed body — same gradient style as bypass buttons
        juce::Colour bodyTop(isButtonDown ? 0xff2a2720 : 0xff353020);
        juce::Colour bodyBot(isButtonDown ? 0xff1e1c16 : 0xff262219);

        if (box.isMouseOver())
        {
            bodyTop = bodyTop.brighter(0.06f);
            bodyBot = bodyBot.brighter(0.06f);
        }

        juce::ColourGradient bodyGrad(bodyTop, 0.0f, 0.0f,
                                       bodyBot, 0.0f, bounds.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(bounds.reduced(0.5f), cornerR);

        // Inset shadow on top edge
        g.setColour(juce::Colour(0x18000000));
        g.drawLine(bounds.getX() + cornerR, bounds.getY() + 1.0f,
                   bounds.getRight() - cornerR, bounds.getY() + 1.0f, 0.5f);

        // Bottom catch light
        g.setColour(juce::Colour(0x0cffffff));
        g.drawLine(bounds.getX() + cornerR, bounds.getBottom() - 1.0f,
                   bounds.getRight() - cornerR, bounds.getBottom() - 1.0f, 0.5f);

        // Border
        g.setColour(juce::Colour(0xff1a1814));
        g.drawRoundedRectangle(bounds.reduced(0.5f), cornerR, 1.0f);

        // Arrow chevron (right side)
        const float arrowSize = 5.0f;
        const float arrowX = bounds.getRight() - 14.0f;
        const float arrowY = bounds.getCentreY();

        juce::Path arrow;
        arrow.addTriangle(arrowX - arrowSize, arrowY - arrowSize * 0.5f,
                          arrowX + arrowSize, arrowY - arrowSize * 0.5f,
                          arrowX, arrowY + arrowSize * 0.5f);

        g.setColour(juce::Colour(0xff908878));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(8, 0, box.getWidth() - 26, box.getHeight());
        label.setFont(juce::Font(juce::FontOptions(12.0f)));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffc0b8a0));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions(12.0f));
    }

    //======================================================================
    // PopupMenu — dark hardware-style dropdown
    //======================================================================
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                              static_cast<float>(width),
                                              static_cast<float>(height));

        // Dark body matching panel background
        g.setColour(juce::Colour(0xff2a2720));
        g.fillRoundedRectangle(bounds, 3.0f);

        // Subtle border
        g.setColour(juce::Colour(0xff1a1814));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool /*hasSubMenu*/,
                           const juce::String& text, const juce::String& /*shortcutKeyText*/,
                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColourToUse*/) override
    {
        if (isSeparator)
        {
            auto sepArea = area.reduced(8, 0);
            g.setColour(juce::Colour(0xff3a3830));
            g.fillRect(sepArea.getX(), sepArea.getCentreY(), sepArea.getWidth(), 1);
            return;
        }

        auto bounds = area.toFloat();

        if (isHighlighted && isActive)
        {
            // Highlighted item — subtle warm highlight
            g.setColour(juce::Colour(0xff3e3828));
            g.fillRect(bounds);
        }

        // Text
        if (isActive)
            g.setColour(isTicked ? juce::Colour(0xffe0d8c0) : juce::Colour(0xffc0b8a0));
        else
            g.setColour(juce::Colour(0xff605848));

        auto font = juce::Font(juce::FontOptions(12.0f));
        if (isTicked)
            font = font.boldened();
        g.setFont(font);

        auto textArea = area.reduced(12, 0);
        g.drawText(text, textArea, juce::Justification::centredLeft, true);

        // Tick indicator — small dot on the left
        if (isTicked)
        {
            float dotR = 3.0f;
            float dotX = bounds.getX() + 5.0f;
            float dotY = bounds.getCentreY();
            g.setColour(juce::Colour(0xffc0b8a0));
            g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                   int /*standardMenuItemHeight*/, int& idealWidth,
                                   int& idealHeight) override
    {
        if (isSeparator)
        {
            idealWidth = 50;
            idealHeight = 8;
            return;
        }

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(juce::Font(juce::FontOptions(12.0f)), text, 0.0f, 0.0f);
        idealWidth = juce::roundToInt(glyphs.getBoundingBox(0, -1, false).getWidth()) + 32;
        idealHeight = 26;
    }

private:
    void drawHorizontalSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, juce::Slider& slider)
    {
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float leftX   = static_cast<float>(x);
        const float rightX  = static_cast<float>(x + width);

        // --- Track (horizontal groove) ---
        const float grooveH = 8.0f;
        auto grooveBounds = juce::Rectangle<float>(leftX, centreY - grooveH * 0.5f,
                                                    rightX - leftX, grooveH);

        // Dark recessed fill
        g.setColour(juce::Colour(0xff0a0908));
        g.fillRoundedRectangle(grooveBounds, 3.0f);

        // Inner shadow on top+left edges
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawLine(grooveBounds.getX() + 3.0f, grooveBounds.getY() + 1.0f,
                   grooveBounds.getRight() - 3.0f, grooveBounds.getY() + 1.0f, 1.0f);
        g.drawLine(grooveBounds.getX() + 1.0f, grooveBounds.getY() + 3.0f,
                   grooveBounds.getX() + 1.0f, grooveBounds.getBottom() - 3.0f, 1.0f);

        // Light edge on bottom+right
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(grooveBounds.getX() + 3.0f, grooveBounds.getBottom() - 1.0f,
                   grooveBounds.getRight() - 3.0f, grooveBounds.getBottom() - 1.0f, 1.0f);
        g.drawLine(grooveBounds.getRight() - 1.0f, grooveBounds.getY() + 3.0f,
                   grooveBounds.getRight() - 1.0f, grooveBounds.getBottom() - 3.0f, 1.0f);

        // Filled portion left of thumb
        float fillWidth = sliderPos - leftX - 1.5f;
        if (fillWidth > 0.0f)
        {
            auto filledBounds = juce::Rectangle<float>(leftX + 1.5f, centreY - grooveH * 0.5f + 1.5f,
                                                        fillWidth, grooveH - 3.0f);
            g.setColour(slider.findColour(juce::Slider::thumbColourId).withAlpha(0.35f));
            g.fillRoundedRectangle(filledBounds, 2.0f);
        }

        // Groove outer border
        g.setColour(juce::Colour(0xff2a2518));
        g.drawRoundedRectangle(grooveBounds, 3.0f, 0.8f);

        // --- Thumb (rounded rectangle, taller than wide) ---
        const float thumbW = 14.0f;
        const float thumbH = 26.0f;
        const float cornerR = 4.0f;

        auto thumbBounds = juce::Rectangle<float>(sliderPos - thumbW * 0.5f,
                                                    centreY - thumbH * 0.5f,
                                                    thumbW, thumbH);

        // Base gradient: light top → dark bottom
        juce::ColourGradient bodyGrad(juce::Colour(0xffbbbbbb), thumbBounds.getX(), thumbBounds.getY(),
                                       juce::Colour(0xff666666), thumbBounds.getX(), thumbBounds.getBottom(),
                                       false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(thumbBounds, cornerR);

        // Top highlight
        g.setColour(juce::Colour(0x55ffffff));
        g.drawLine(thumbBounds.getX() + cornerR, thumbBounds.getY() + 1.5f,
                   thumbBounds.getRight() - cornerR, thumbBounds.getY() + 1.5f, 1.0f);

        // Centre groove line (vertical for horizontal slider)
        g.setColour(juce::Colour(0x33000000));
        const float centreX = thumbBounds.getCentreX();
        g.drawLine(centreX, thumbBounds.getY() + 6.0f,
                   centreX, thumbBounds.getBottom() - 6.0f, 1.0f);

        // Bottom shadow edge
        g.setColour(juce::Colour(0x33000000));
        g.drawLine(thumbBounds.getX() + cornerR, thumbBounds.getBottom() - 1.0f,
                   thumbBounds.getRight() - cornerR, thumbBounds.getBottom() - 1.0f, 0.5f);

        // Outer border
        g.setColour(juce::Colour(0xff444444));
        g.drawRoundedRectangle(thumbBounds, cornerR, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliderLookAndFeel)
};
