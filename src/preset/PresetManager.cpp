#include "PresetManager.h"
#include "PresetState.h"
#include "PluginProcessor.h"
#include "dsp/EffectChain.h"

PresetManager::PresetManager(OpenRiffBoxProcessor& proc,
                             const juce::File& factoryPresetDir, const juce::File& userPresetDir)
    : processor(proc),
      factoryDir(factoryPresetDir),
      userDir(userPresetDir)
{
    if (!userDir.isDirectory())
        userDir.createDirectory();

    scanPresets();

    for (int s = 0; s < numSlots; ++s)
    {
        if (defaultSlotNames[s] != nullptr)
            slotAssignments[s] = findPresetByName(defaultSlotNames[s]);
    }
}

void PresetManager::scanPresets()
{
    presets.clear();

    // Factory presets first (sorted by name)
    if (factoryDir.isDirectory())
    {
        auto factoryFiles = factoryDir.findChildFiles(
            juce::File::findFiles, false, "*.json");
        factoryFiles.sort();

        for (auto& file : factoryFiles)
        {
            Preset p;
            if (Preset::loadFromFile(file, p))
            {
                p.isFactory = true;
                presets.push_back(std::move(p));
            }
        }
    }

    // User presets (sorted by name)
    if (userDir.isDirectory())
    {
        auto userFiles = userDir.findChildFiles(
            juce::File::findFiles, false, "*.json");
        userFiles.sort();

        for (auto& file : userFiles)
        {
            Preset p;
            if (Preset::loadFromFile(file, p))
            {
                p.isFactory = false;
                presets.push_back(std::move(p));
            }
        }
    }
}

const Preset* PresetManager::getPreset(int index) const
{
    if (index >= 0 && index < static_cast<int>(presets.size()))
        return &presets[static_cast<size_t>(index)];
    return nullptr;
}

bool PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return false;

    applyPreset(presets[static_cast<size_t>(index)]);
    activePresetIndex = index;
    activeDirty = false;

    // Update active slot if this preset is assigned to one
    activeSlot = -1;
    for (int s = 0; s < numSlots; ++s)
    {
        if (slotAssignments[s] == index)
        {
            activeSlot = s;
            break;
        }
    }

    onPresetLoaded();
    return true;
}

void PresetManager::markActiveDirty()
{
    // Guard matters: edit paths call this on every knob tick, but the
    // notification should only fire once per load (first edit after it).
    if (activePresetIndex < 0 || activeDirty) return;
    activeDirty = true;
    onPresetDirtyChanged();
}

bool PresetManager::savePresetAs(const juce::String& name, const juce::String& author)
{
    Preset preset = captureCurrentState();
    preset.name = name;
    preset.author = author;
    preset.date = juce::Time::getCurrentTime().formatted("%Y-%m-%d");

    // The dir is created at startup, but may have failed there (or been removed
    // since) - without this, replaceWithText fails silently and the preset is lost.
    if (!userDir.isDirectory() && !userDir.createDirectory().wasOk())
        return false;

    // Sanitize filename
    auto safeName = juce::File::createLegalFileName(name);
    auto file = userDir.getChildFile(safeName + ".json");

    // Avoid overwriting: append number if needed
    int counter = 1;
    while (file.existsAsFile())
    {
        file = userDir.getChildFile(safeName + " (" + juce::String(counter) + ").json");
        ++counter;
    }

    if (!Preset::saveToFile(preset, file))
        return false;

    preset.sourceFile = file;
    preset.isFactory = false;
    presets.push_back(std::move(preset));

    activePresetIndex = static_cast<int>(presets.size()) - 1;
    activeDirty = false;
    onPresetListChanged();
    return true;
}

bool PresetManager::deletePreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return false;

    auto& preset = presets[static_cast<size_t>(index)];
    if (preset.isFactory)
        return false;

    // Keep list and disk consistent: a failed delete must not drop the entry,
    // or the file resurrects as a "new" preset on the next scan.
    if (preset.sourceFile.existsAsFile() && !preset.sourceFile.deleteFile())
        return false;

    // Update slot assignments that pointed to this or later presets
    for (int s = 0; s < numSlots; ++s)
    {
        if (slotAssignments[s] == index)
            slotAssignments[s] = -1;
        else if (slotAssignments[s] > index)
            slotAssignments[s]--;
    }

    if (activePresetIndex == index)
    {
        activePresetIndex = -1;
        activeDirty = false;
    }
    else if (activePresetIndex > index)
        activePresetIndex--;

    presets.erase(presets.begin() + index);
    onPresetListChanged();
    return true;
}

Preset PresetManager::captureCurrentState() const
{
    return PresetState::capture(processor);
}

void PresetManager::applyPreset(const Preset& preset)
{
    PresetState::apply(preset, processor);
}

bool PresetManager::loadInitPreset()
{
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        if (presets[static_cast<size_t>(i)].isFactory && presets[static_cast<size_t>(i)].name == "Init")
        {
            applyPreset(presets[static_cast<size_t>(i)]);
            activeSlot = -1;
            activePresetIndex = -1;
            activeDirty = false;
            onPresetLoaded();
            return true;
        }
    }
    return false;
}

int PresetManager::getSlotPresetIndex(int slot) const
{
    if (slot < 0 || slot >= numSlots)
        return -1;
    return slotAssignments[slot];
}

void PresetManager::assignSlot(int slot, int presetIndex)
{
    if (slot < 0 || slot >= numSlots)
        return;

    // Slots 0-2 are factory-locked by default, but allow reassignment
    // if the caller explicitly requests it (the UI should guard this)
    slotAssignments[slot] = presetIndex;
}

bool PresetManager::loadSlot(int slot)
{
    if (slot < 0 || slot >= numSlots)
        return false;

    int presetIdx = slotAssignments[slot];
    if (presetIdx < 0 || presetIdx >= static_cast<int>(presets.size()))
        return false;

    applyPreset(presets[static_cast<size_t>(presetIdx)]);
    activeSlot = slot;
    activePresetIndex = presetIdx;
    activeDirty = false;
    onPresetLoaded();
    return true;
}

void PresetManager::saveSlotAssignments(juce::PropertiesFile* props)
{
    if (props == nullptr) return;

    for (int s = 0; s < numSlots; ++s)
    {
        juce::String key = "presetSlot" + juce::String(s);
        int idx = slotAssignments[s];

        if (idx >= 0 && idx < static_cast<int>(presets.size()))
            props->setValue(key, presets[static_cast<size_t>(idx)].sourceFile.getFullPathName());
        else
            props->setValue(key, "");
    }
}

void PresetManager::loadSlotAssignments(juce::PropertiesFile* props)
{
    if (props == nullptr) return;

    for (int s = 0; s < numSlots; ++s)
    {
        juce::String key = "presetSlot" + juce::String(s);
        auto path = props->getValue(key, "");

        if (path.isEmpty())
        {
            if (defaultSlotNames[s] != nullptr)
                slotAssignments[s] = findPresetByName(defaultSlotNames[s]);
            else
                slotAssignments[s] = -1;
        }
        else
        {
            slotAssignments[s] = findPresetByFile(juce::File(path));
        }
    }
}

int PresetManager::findPresetByFile(const juce::File& file) const
{
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        if (presets[static_cast<size_t>(i)].sourceFile == file)
            return i;
    }
    return -1;
}

int PresetManager::findPresetByName(const juce::String& name) const
{
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        if (presets[static_cast<size_t>(i)].name == name)
            return i;
    }
    return -1;
}
