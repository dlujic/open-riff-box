#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "Preset.h"
#include <vector>
#include <functional>

class OpenRiffBoxProcessor;

class PresetManager
{
public:
    PresetManager(OpenRiffBoxProcessor& processor, const juce::File& presetsRoot);

    void scanPresets();

    const std::vector<Preset>& getPresets() const { return presets; }
    int getNumPresets() const { return static_cast<int>(presets.size()); }
    const Preset* getPreset(int index) const;

    bool loadPreset(int index);
    bool savePresetAs(const juce::String& name, const juce::String& author);
    bool deletePreset(int index);

    Preset captureCurrentState() const;

    // Slot management (4 quick-access slots)
    static constexpr int numSlots = 4;
    int getSlotPresetIndex(int slot) const;
    void assignSlot(int slot, int presetIndex);
    bool loadSlot(int slot);

    int getActiveSlotIndex() const { return activeSlot; }
    int getActivePresetIndex() const { return activePresetIndex; }

    // Clear active selection (called when user changes any parameter after loading a preset)
    void clearActivePreset();
    bool hasActivePreset() const { return activePresetIndex >= 0; }

    // Loads the "Init" factory preset (bypasses everything). Returns false if not found.
    bool loadInitPreset();

    void saveSlotAssignments(juce::PropertiesFile* props);
    void loadSlotAssignments(juce::PropertiesFile* props);

    const juce::File& getUserDir() const { return userDir; }

    std::function<void()> onPresetLoaded = [] {};
    std::function<void()> onPresetListChanged = [] {};

private:
    OpenRiffBoxProcessor& processor;
    juce::File factoryDir;
    juce::File userDir;

    std::vector<Preset> presets;
    int slotAssignments[numSlots] = { -1, -1, -1, -1 };
    int activeSlot = -1;
    int activePresetIndex = -1;

    void applyPreset(const Preset& preset);
    int findPresetByFile(const juce::File& file) const;
    int findPresetByName(const juce::String& name) const;
};
