#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "SettingsLookAndFeel.h"

class PresetManager;

class PresetBrowserPanel : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit PresetBrowserPanel(PresetManager& manager);
    ~PresetBrowserPanel() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void refreshList();

    // Called when a preset is loaded from this panel
    std::function<void()> onPresetLoaded = [] {};

private:
    PresetManager& presetManager;

    juce::ListBox presetList;
    juce::Label nameLabel, nameField;
    juce::Label authorLabel, authorField;
    juce::Label dateLabel, dateField;
    juce::TextButton loadButton { "Load" };
    juce::TextButton saveAsButton { "Save As" };
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton assignButton { "Assign to Slot" };

    // Assign-to-slot picker (4 mini buttons)
    juce::TextButton assignSlotButtons[4];
    bool assignPickerVisible = false;

    // Save As dialog fields
    juce::TextEditor saveNameEditor;
    juce::TextEditor saveAuthorEditor;
    juce::TextButton saveConfirmButton { "Save" };
    juce::TextButton saveCancelButton { "Cancel" };
    bool saveDialogVisible = false;

    SettingsLookAndFeel settingsLF;

    int selectedPresetIndex = -1;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void selectedRowsChanged(int lastRowSelected) override;

    void updateInfoFields();
    void showAssignPicker(bool show);
    void showSaveDialog(bool show);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowserPanel)
};
