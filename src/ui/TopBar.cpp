#include "TopBar.h"
#include "Theme.h"

TopBar::TopBar()
{
    titleLabel.setText("Open Riff Box", juce::dontSendNotification);
    titleLabel.setFont(Theme::Fonts::appTitle());
    titleLabel.setColour(juce::Label::textColourId, Theme::Colours::goldText);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);
}

void TopBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Brushed-metal vertical gradient
    g.setGradientFill(juce::ColourGradient(
        Theme::Colours::topBarTop, 0.0f, 0.0f,
        Theme::Colours::topBarBottom, 0.0f, static_cast<float>(bounds.getHeight()),
        false));
    g.fillRect(bounds);

    // Subtle horizontal "brushed" lines
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 2)
    {
        float alpha = (y % 4 == 0) ? 0.06f : 0.03f;
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                             static_cast<float>(bounds.getRight()));
    }

    Theme::paintNoise(g, bounds, 0.05f);

    // Highlight line at top edge
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // Shadow line at bottom edge
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TopBar::resized()
{
    auto area = getLocalBounds().reduced(12, 0);
    titleLabel.setBounds(area);
}
