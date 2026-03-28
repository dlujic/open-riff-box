#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

class SettingsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SettingsLookAndFeel()
    {
        // ComboBox
        setColour(juce::ComboBox::backgroundColourId,  Theme::Colours::panelBackground.brighter(0.1f));
        setColour(juce::ComboBox::textColourId,         Theme::Colours::textPrimary);
        setColour(juce::ComboBox::outlineColourId,      Theme::Colours::border);
        setColour(juce::ComboBox::arrowColourId,        Theme::Colours::textSecondary);
        setColour(juce::ComboBox::focusedOutlineColourId, Theme::Colours::border.brighter(0.2f));

        // PopupMenu
        setColour(juce::PopupMenu::backgroundColourId,            Theme::Colours::panelBackground);
        setColour(juce::PopupMenu::textColourId,                   Theme::Colours::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,  Theme::Colours::selectedRow);
        setColour(juce::PopupMenu::highlightedTextColourId,        Theme::Colours::textPrimary);

        // Label
        setColour(juce::Label::textColourId, Theme::Colours::textPrimary);

        // TextButton
        setColour(juce::TextButton::buttonColourId,   Theme::Colours::panelBackground.brighter(0.15f));
        setColour(juce::TextButton::textColourOffId,   Theme::Colours::textPrimary);
        setColour(juce::TextButton::textColourOnId,    Theme::Colours::textPrimary);

        // ListBox
        setColour(juce::ListBox::backgroundColourId, Theme::Colours::panelBackground);
        setColour(juce::ListBox::textColourId,        Theme::Colours::textPrimary);

        // TextEditor
        setColour(juce::TextEditor::backgroundColourId,  Theme::Colours::panelBackground.brighter(0.1f));
        setColour(juce::TextEditor::textColourId,         Theme::Colours::textPrimary);
        setColour(juce::TextEditor::outlineColourId,      Theme::Colours::border);
        setColour(juce::TextEditor::focusedOutlineColourId, Theme::Colours::border.brighter(0.2f));

        // ToggleButton (for checkboxes in audio settings)
        setColour(juce::ToggleButton::textColourId,  Theme::Colours::textPrimary);
        setColour(juce::ToggleButton::tickColourId,  Theme::Colours::textPrimary);

        // Set default typeface
        setDefaultSansSerifTypeface(Theme::Fonts::getRegular());
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsLookAndFeel)
};
