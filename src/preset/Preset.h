#pragma once

#include <juce_core/juce_core.h>
#include <map>

struct Preset
{
    juce::String name;
    juce::String author;
    juce::String date;
    bool limiterEnabled = true;
    int  ampSimEngine       = 1;  // 0=Silver, 1=Gold (default Gold)
    int  reverbEngine       = 0;  // 0=Spring, 1=Plate (default Spring)
    int  modulationEngine   = 0;  // 0=Chorus, 1=Flanger, 2=Phaser, 3=Vibrato (default Chorus)

    // Effect name -> DynamicObject with "bypassed" bool + parameter key/value pairs
    // Keys match the XML attribute names in PluginProcessor serialization.
    std::map<juce::String, juce::var> effects;

    // Custom chain order (empty = default order).
    // Uses effect display names from EffectProcessor::getName().
    std::vector<juce::String> chainOrder;

    // Runtime metadata (not serialized to JSON)
    bool isFactory = false;
    juce::File sourceFile;

    juce::var toJson() const;
    static bool fromJson(const juce::var& json, Preset& result);

    static juce::String writeToString(const Preset& preset);
    static bool readFromString(const juce::String& jsonString, Preset& result);

    static bool saveToFile(const Preset& preset, const juce::File& file);
    static bool loadFromFile(const juce::File& file, Preset& result);
};
