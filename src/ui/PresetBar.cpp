#include "PresetBar.h"
#include "Theme.h"
#include "preset/PresetManager.h"

PresetBar::PresetBar(PresetManager& manager)
    : presetManager(manager)
{
    for (int i = 0; i < 4; ++i)
    {
        slotButtons[i].setLookAndFeel(&slotLF);
        slotButtons[i].setClickingTogglesState(false);
        slotButtons[i].onClick = [this, i]
        {
            if (presetManager.loadSlot(i))
            {
                refreshSlots();
                onSlotClicked();
            }
        };
        addAndMakeVisible(slotButtons[i]);
    }

    clearButton.setLookAndFeel(&slotLF);
    clearButton.setClickingTogglesState(false);
    clearButton.onClick = [this]
    {
        presetManager.loadInitPreset();
        refreshSlots();
        onSlotClicked();
    };
    addAndMakeVisible(clearButton);

    refreshSlots();
}

PresetBar::~PresetBar()
{
    for (int i = 0; i < 4; ++i)
        slotButtons[i].setLookAndFeel(nullptr);
    clearButton.setLookAndFeel(nullptr);
}

void PresetBar::refreshSlots()
{
    int activeSlot = presetManager.getActiveSlotIndex();

    for (int i = 0; i < 4; ++i)
    {
        int presetIdx = presetManager.getSlotPresetIndex(i);
        auto* preset = presetManager.getPreset(presetIdx);

        if (preset != nullptr)
        {
            juce::String label = juce::String(i + 1) + ". " + preset->name;
            slotButtons[i].setButtonText(label);
        }
        else
        {
            slotButtons[i].setButtonText(juce::String(i + 1) + ". ---");
        }

        slotButtons[i].setToggleState(i == activeSlot, juce::dontSendNotification);
    }

    repaint();
}

void PresetBar::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Dark recessed strip background
    g.setColour(juce::Colour(0xff181610));
    g.fillRect(area);

    // Top edge shadow (recessed look)
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawHorizontalLine(area.getY(), static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.drawHorizontalLine(area.getY() + 1, static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));

    Theme::paintNoise(g, area);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced(6, 4);

    // CLR button at the right edge (narrower)
    auto clrArea = area.removeFromRight(46);
    clearButton.setBounds(clrArea.reduced(2, 0));

    area.removeFromRight(2); // gap

    // 4 slot buttons fill the rest
    int buttonW = area.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto btnArea = area.removeFromLeft(buttonW);
        slotButtons[i].setBounds(btnArea.reduced(2, 0));
    }
}
