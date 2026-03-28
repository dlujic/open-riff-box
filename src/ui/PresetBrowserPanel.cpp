#include "PresetBrowserPanel.h"
#include "Theme.h"
#include "preset/PresetManager.h"
#include "preset/Preset.h"

PresetBrowserPanel::PresetBrowserPanel(PresetManager& manager)
    : presetManager(manager)
{
    presetList.setModel(this);
    presetList.setLookAndFeel(&settingsLF);
    presetList.setRowHeight(28);
    presetList.setColour(juce::ListBox::backgroundColourId, Theme::Colours::panelBackground.brighter(0.05f));
    presetList.setColour(juce::ListBox::outlineColourId, Theme::Colours::border);
    addAndMakeVisible(presetList);

    // Info labels (read-only display)
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(Theme::Fonts::body());
        label.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
        addAndMakeVisible(label);
    };

    auto setupField = [this](juce::Label& field)
    {
        field.setFont(Theme::Fonts::body());
        field.setColour(juce::Label::textColourId, Theme::Colours::textPrimary);
        addAndMakeVisible(field);
    };

    setupLabel(nameLabel, "Name:");
    setupField(nameField);
    setupLabel(authorLabel, "Author:");
    setupField(authorField);
    setupLabel(dateLabel, "Date:");
    setupField(dateField);

    // Buttons
    auto setupButton = [this](juce::TextButton& btn)
    {
        btn.setLookAndFeel(&settingsLF);
        addAndMakeVisible(btn);
    };

    setupButton(loadButton);
    setupButton(saveAsButton);
    setupButton(deleteButton);
    setupButton(assignButton);

    loadButton.onClick = [this]
    {
        if (selectedPresetIndex >= 0)
        {
            if (presetManager.loadPreset(selectedPresetIndex))
                onPresetLoaded();
        }
    };
    saveAsButton.onClick = [this] { showSaveDialog(true); };
    deleteButton.onClick = [this]
    {
        if (selectedPresetIndex >= 0)
        {
            presetManager.deletePreset(selectedPresetIndex);
            refreshList();
        }
    };
    assignButton.onClick = [this] { showAssignPicker(!assignPickerVisible); };

    // Assign-to-slot picker (hidden by default)
    for (int i = 0; i < 4; ++i)
    {
        assignSlotButtons[i].setButtonText(juce::String(i + 1));
        assignSlotButtons[i].setLookAndFeel(&settingsLF);
        assignSlotButtons[i].onClick = [this, i]
        {
            if (selectedPresetIndex >= 0)
            {
                presetManager.assignSlot(i, selectedPresetIndex);
                showAssignPicker(false);
                presetManager.onPresetListChanged();
            }
        };
        addChildComponent(assignSlotButtons[i]);
    }

    // Save As dialog fields (hidden by default)
    saveNameEditor.setLookAndFeel(&settingsLF);
    saveNameEditor.setTextToShowWhenEmpty("Preset name...", Theme::Colours::textSecondary);
    addChildComponent(saveNameEditor);

    saveAuthorEditor.setLookAndFeel(&settingsLF);
    saveAuthorEditor.setTextToShowWhenEmpty("Author (optional)...", Theme::Colours::textSecondary);
    addChildComponent(saveAuthorEditor);

    saveConfirmButton.setLookAndFeel(&settingsLF);
    saveConfirmButton.onClick = [this]
    {
        auto name = saveNameEditor.getText().trim();
        if (name.isNotEmpty())
        {
            auto author = saveAuthorEditor.getText().trim();
            presetManager.savePresetAs(name, author);
            showSaveDialog(false);
            refreshList();
        }
    };
    addChildComponent(saveConfirmButton);

    saveCancelButton.setLookAndFeel(&settingsLF);
    saveCancelButton.onClick = [this] { showSaveDialog(false); };
    addChildComponent(saveCancelButton);

    refreshList();
}

PresetBrowserPanel::~PresetBrowserPanel()
{
    presetList.setLookAndFeel(nullptr);
    loadButton.setLookAndFeel(nullptr);
    saveAsButton.setLookAndFeel(nullptr);
    deleteButton.setLookAndFeel(nullptr);
    assignButton.setLookAndFeel(nullptr);
    saveNameEditor.setLookAndFeel(nullptr);
    saveAuthorEditor.setLookAndFeel(nullptr);
    saveConfirmButton.setLookAndFeel(nullptr);
    saveCancelButton.setLookAndFeel(nullptr);

    for (int i = 0; i < 4; ++i)
        assignSlotButtons[i].setLookAndFeel(nullptr);
}

void PresetBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());

    // Title
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);
    g.setColour(Theme::Colours::textPrimary);
    g.setFont(Theme::Fonts::title());
    g.drawText("Presets", area.removeFromTop(30), juce::Justification::centredLeft);
}

void PresetBrowserPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);
    area.removeFromTop(36); // title

    // Left: preset list (40% width)
    auto leftPanel = area.removeFromLeft(area.getWidth() * 4 / 10);
    presetList.setBounds(leftPanel);

    // Right: info + buttons (with left margin)
    auto rightPanel = area.withTrimmedLeft(16);
    const int rowH = 26;
    const int gap = 6;

    // Info fields
    auto nameRow = rightPanel.removeFromTop(rowH);
    nameLabel.setBounds(nameRow.removeFromLeft(55));
    nameField.setBounds(nameRow);
    rightPanel.removeFromTop(gap);

    auto authorRow = rightPanel.removeFromTop(rowH);
    authorLabel.setBounds(authorRow.removeFromLeft(55));
    authorField.setBounds(authorRow);
    rightPanel.removeFromTop(gap);

    auto dateRow = rightPanel.removeFromTop(rowH);
    dateLabel.setBounds(dateRow.removeFromLeft(55));
    dateField.setBounds(dateRow);
    rightPanel.removeFromTop(gap * 3);

    // Action buttons
    const int btnH = 30;
    const int btnW = juce::jmin(rightPanel.getWidth(), 120);

    auto loadRow = rightPanel.removeFromTop(btnH);
    loadButton.setBounds(loadRow.removeFromLeft(btnW));
    rightPanel.removeFromTop(gap);

    auto btnRow = rightPanel.removeFromTop(btnH);
    saveAsButton.setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(8);
    deleteButton.setBounds(btnRow.removeFromLeft(btnW));
    rightPanel.removeFromTop(gap);

    auto assignRow = rightPanel.removeFromTop(btnH);
    assignButton.setBounds(assignRow.removeFromLeft(btnW));
    rightPanel.removeFromTop(gap);

    // Assign picker (4 small buttons inline)
    auto pickerRow = rightPanel.removeFromTop(btnH);
    int slotBtnW = 36;
    for (int i = 0; i < 4; ++i)
    {
        assignSlotButtons[i].setBounds(pickerRow.removeFromLeft(slotBtnW));
        pickerRow.removeFromLeft(4);
    }
    rightPanel.removeFromTop(gap);

    // Save dialog fields (overlay at the bottom of rightPanel)
    auto saveArea = rightPanel;
    auto saveNameRow = saveArea.removeFromTop(28);
    saveNameEditor.setBounds(saveNameRow);
    saveArea.removeFromTop(gap);

    auto saveAuthorRow = saveArea.removeFromTop(28);
    saveAuthorEditor.setBounds(saveAuthorRow);
    saveArea.removeFromTop(gap);

    auto saveBtnRow = saveArea.removeFromTop(btnH);
    saveConfirmButton.setBounds(saveBtnRow.removeFromLeft(80));
    saveBtnRow.removeFromLeft(8);
    saveCancelButton.setBounds(saveBtnRow.removeFromLeft(80));
}

void PresetBrowserPanel::refreshList()
{
    presetList.updateContent();

    int active = presetManager.getActivePresetIndex();
    if (active >= 0)
        presetList.selectRow(active);

    updateInfoFields();
    repaint();
}

// ListBoxModel implementation
int PresetBrowserPanel::getNumRows()
{
    return presetManager.getNumPresets();
}

void PresetBrowserPanel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                           int width, int height, bool rowIsSelected)
{
    auto* preset = presetManager.getPreset(rowNumber);
    if (preset == nullptr) return;

    // Background
    if (rowIsSelected)
    {
        g.setColour(Theme::Colours::selectedRow);
        g.fillRect(0, 0, width, height);
    }

    // Factory badge
    int textX = 8;
    if (preset->isFactory)
    {
        g.setColour(Theme::Colours::textSecondary.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.drawText("[F]", width - 32, 0, 28, height, juce::Justification::centredRight);
    }

    // Preset name
    g.setColour(rowIsSelected ? Theme::Colours::textPrimary : Theme::Colours::textSecondary.brighter(0.3f));
    g.setFont(Theme::Fonts::body());
    g.drawText(preset->name, textX, 0, width - textX - 36, height,
               juce::Justification::centredLeft, true);
}

void PresetBrowserPanel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    selectedPresetIndex = row;
    updateInfoFields();
}

void PresetBrowserPanel::selectedRowsChanged(int lastRowSelected)
{
    selectedPresetIndex = lastRowSelected;
    updateInfoFields();
}

void PresetBrowserPanel::updateInfoFields()
{
    auto* preset = presetManager.getPreset(selectedPresetIndex);
    if (preset != nullptr)
    {
        nameField.setText(preset->name, juce::dontSendNotification);
        authorField.setText(preset->author.isEmpty() ? "-" : preset->author,
                            juce::dontSendNotification);
        dateField.setText(preset->date.isEmpty() ? "-" : preset->date,
                          juce::dontSendNotification);

        // Can't delete factory presets
        deleteButton.setEnabled(!preset->isFactory);
    }
    else
    {
        nameField.setText("-", juce::dontSendNotification);
        authorField.setText("-", juce::dontSendNotification);
        dateField.setText("-", juce::dontSendNotification);
        deleteButton.setEnabled(false);
    }
}

void PresetBrowserPanel::showAssignPicker(bool show)
{
    assignPickerVisible = show;
    for (int i = 0; i < 4; ++i)
        assignSlotButtons[i].setVisible(show);
}

void PresetBrowserPanel::showSaveDialog(bool show)
{
    saveDialogVisible = show;
    saveNameEditor.setVisible(show);
    saveAuthorEditor.setVisible(show);
    saveConfirmButton.setVisible(show);
    saveCancelButton.setVisible(show);

    if (show)
    {
        saveNameEditor.clear();
        saveAuthorEditor.clear();
        saveNameEditor.grabKeyboardFocus();
    }
}
