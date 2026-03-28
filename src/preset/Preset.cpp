#include "Preset.h"

juce::var Preset::toJson() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty("format", "OpenRiffBox");
    root->setProperty("version", 1);
    root->setProperty("name", name);
    root->setProperty("author", author);
    root->setProperty("date", date);
    root->setProperty("limiterEnabled", limiterEnabled);
    root->setProperty("ampSimEngine", ampSimEngine);
    root->setProperty("reverbEngine", reverbEngine);
    root->setProperty("modulationEngine", modulationEngine);

    auto* effectsObj = new juce::DynamicObject();

    for (auto& [effectName, params] : effects)
        effectsObj->setProperty(juce::Identifier(effectName), params);

    root->setProperty("effects", juce::var(effectsObj));

    // Only emit "chain" if order is non-default (keeps factory presets clean)
    if (!chainOrder.empty())
    {
        juce::Array<juce::var> chainArray;
        for (const auto& name : chainOrder)
            chainArray.add(name);
        root->setProperty("chain", juce::var(chainArray));
    }

    return juce::var(root);
}

bool Preset::fromJson(const juce::var& json, Preset& result)
{
    auto* root = json.getDynamicObject();
    if (root == nullptr)
        return false;

    if (root->getProperty("format").toString() != "OpenRiffBox")
        return false;

    result.name = root->getProperty("name").toString();
    result.author = root->getProperty("author").toString();
    result.date = root->getProperty("date").toString();
    result.limiterEnabled = root->getProperty("limiterEnabled");
    result.ampSimEngine = static_cast<int>(root->getProperty("ampSimEngine"));
    if (result.ampSimEngine < 0 || result.ampSimEngine > 2)
        result.ampSimEngine = 1;  // default Gold for old presets

    result.reverbEngine = static_cast<int>(root->getProperty("reverbEngine"));
    if (result.reverbEngine < 0 || result.reverbEngine > 1)
        result.reverbEngine = 0;  // default Spring for old presets

    result.modulationEngine = static_cast<int>(root->getProperty("modulationEngine"));
    if (result.modulationEngine < 0 || result.modulationEngine > 3)
        result.modulationEngine = 0;  // default Chorus for old presets

    auto effectsVar = root->getProperty("effects");
    auto* effectsObj = effectsVar.getDynamicObject();
    if (effectsObj == nullptr)
        return false;

    result.effects.clear();
    for (auto& prop : effectsObj->getProperties())
        result.effects[prop.name.toString()] = prop.value;

    // Parse chain order: absent or "default" = empty (default order)
    result.chainOrder.clear();
    auto chainVar = root->getProperty("chain");
    if (auto* chainArray = chainVar.getArray())
    {
        for (const auto& item : *chainArray)
            result.chainOrder.push_back(item.toString());
    }

    return true;
}

juce::String Preset::writeToString(const Preset& preset)
{
    return juce::JSON::toString(preset.toJson(), true);
}

bool Preset::readFromString(const juce::String& jsonString, Preset& result)
{
    auto parsed = juce::JSON::parse(jsonString);
    if (parsed.isVoid())
        return false;

    return fromJson(parsed, result);
}

bool Preset::saveToFile(const Preset& preset, const juce::File& file)
{
    auto jsonString = writeToString(preset);
    return file.replaceWithText(jsonString);
}

bool Preset::loadFromFile(const juce::File& file, Preset& result)
{
    if (!file.existsAsFile())
        return false;

    auto jsonString = file.loadFileAsString();
    if (!readFromString(jsonString, result))
        return false;

    result.sourceFile = file;
    return true;
}
