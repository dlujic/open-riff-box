#include "PresetManager.h"
#include "PluginProcessor.h"
#include "dsp/EffectChain.h"
#include "dsp/NoiseGate.h"
#include "dsp/Distortion.h"
#include "dsp/DiodeDrive.h"
#include "dsp/AmpSimSilver.h"
#include "dsp/AmpSimGold.h"
#include "dsp/AnalogDelay.h"
#include "dsp/SpringReverb.h"
#include "dsp/PlateReverb.h"
#include "dsp/Chorus.h"
#include "dsp/Flanger.h"
#include "dsp/Phaser.h"
#include "dsp/Vibrato.h"
#include "dsp/Equalizer.h"

PresetManager::PresetManager(OpenRiffBoxProcessor& proc, const juce::File& presetsRoot)
    : processor(proc),
      factoryDir(presetsRoot.getChildFile("factory")),
      userDir(presetsRoot.getChildFile("user"))
{
    if (!userDir.isDirectory())
        userDir.createDirectory();

    scanPresets();

    // Set default slot assignments by name (stable across preset additions)
    static const char* defaultNames[numSlots] = { "Clean", "Blues", "Classic Rock", nullptr };
    for (int s = 0; s < numSlots; ++s)
    {
        if (defaultNames[s] != nullptr)
            slotAssignments[s] = findPresetByName(defaultNames[s]);
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

void PresetManager::clearActivePreset()
{
    if (activePresetIndex < 0) return;  // already clear
    activePresetIndex = -1;
    activeSlot = -1;
    onPresetLoaded();  // reuse existing callback to refresh UI
}

bool PresetManager::savePresetAs(const juce::String& name, const juce::String& author)
{
    Preset preset = captureCurrentState();
    preset.name = name;
    preset.author = author;
    preset.date = juce::Time::getCurrentTime().formatted("%Y-%m-%d");

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

    if (preset.sourceFile.existsAsFile())
        preset.sourceFile.deleteFile();

    // Update slot assignments that pointed to this or later presets
    for (int s = 0; s < numSlots; ++s)
    {
        if (slotAssignments[s] == index)
            slotAssignments[s] = -1;
        else if (slotAssignments[s] > index)
            slotAssignments[s]--;
    }

    if (activePresetIndex == index)
        activePresetIndex = -1;
    else if (activePresetIndex > index)
        activePresetIndex--;

    presets.erase(presets.begin() + index);
    onPresetListChanged();
    return true;
}

Preset PresetManager::captureCurrentState() const
{
    Preset preset;
    preset.limiterEnabled = processor.isLimiterEnabled();
    preset.ampSimEngine = processor.getAmpSimEngine();
    preset.reverbEngine = processor.getReverbEngine();
    preset.modulationEngine = processor.getModulationEngine();

    auto& chain = processor.getEffectChain();

    // Helper to create a DynamicObject for effect params
    auto makeEffectVar = [](std::initializer_list<std::pair<const char*, juce::var>> params) -> juce::var
    {
        auto* obj = new juce::DynamicObject();
        for (auto& [key, value] : params)
            obj->setProperty(juce::Identifier(key), value);
        return juce::var(obj);
    };

    // Name-based lookup - position-independent (works with any chain order)
    if (auto* ng = dynamic_cast<NoiseGate*>(chain.getEffectByName("Noise Gate")))
    {
        preset.effects["NoiseGate"] = makeEffectVar({
            { "bypassed",  ng->isBypassed() },
            { "threshold", ng->getThresholdDb() },
            { "attack",    ng->getAttackSeconds() },
            { "hold",      ng->getHoldSeconds() },
            { "release",   ng->getReleaseSeconds() },
            { "range",     ng->getRangeDb() }
        });
    }

    if (auto* dd = dynamic_cast<DiodeDrive*>(chain.getEffectByName("Diode Drive")))
    {
        preset.effects["DiodeDrive"] = makeEffectVar({
            { "bypassed", dd->isBypassed() },
            { "drive",    dd->getDrive() },
            { "tone",     dd->getTone() },
            { "level",    dd->getLevel() }
        });
    }

    if (auto* dist = dynamic_cast<Distortion*>(chain.getEffectByName("Distortion")))
    {
        preset.effects["Distortion"] = makeEffectVar({
            { "bypassed",        dist->isBypassed() },
            { "drive",           dist->getDrive() },
            { "tone",            dist->getTone() },
            { "level",           dist->getLevel() },
            { "mix",             dist->getMix() },
            { "saturate",        dist->getSaturate() },
            { "saturateEnabled", dist->getSaturateEnabled() },
            { "mode",            static_cast<int>(dist->getMode()) },
            { "clipType",        static_cast<int>(dist->getClipType()) }
        });
    }

    if (auto* amp = dynamic_cast<AmpSimSilver*>(chain.getEffectByName("Amp Silver")))
    {
        preset.effects["AmpSimSilver"] = makeEffectVar({
            { "bypassed",      amp->isBypassed() },
            { "gain",          amp->getGain() },
            { "bass",          amp->getBass() },
            { "mid",           amp->getMid() },
            { "treble",        amp->getTreble() },
            { "preampBoost",   amp->getPreampBoost() },
            { "speakerDrive",  amp->getSpeakerDrive() },
            { "cabinetType",   amp->getCabinetType() },
            { "brightness",    amp->getBrightness() },
            { "micPosition",   amp->getMicPosition() },
            { "cabTrim",       amp->getCabTrim() }
        });
    }

    if (auto* amp2 = dynamic_cast<AmpSimGold*>(chain.getEffectByName("Amp Gold")))
    {
        preset.effects["AmpSimGold"] = makeEffectVar({
            { "bypassed",      amp2->isBypassed() },
            { "gain",          amp2->getGain() },
            { "bass",          amp2->getBass() },
            { "mid",           amp2->getMid() },
            { "treble",        amp2->getTreble() },
            { "preampBoost",   amp2->getPreampBoost() },
            { "speakerDrive",  amp2->getSpeakerDrive() },
            { "presence",      amp2->getPresence() },
            { "cabinetType",   amp2->getCabinetType() },
            { "brightness",    amp2->getBrightness() },
            { "micPosition",   amp2->getMicPosition() },
            { "cabTrim",       amp2->getCabTrim() }
        });
    }

    if (auto* delay = dynamic_cast<AnalogDelay*>(chain.getEffectByName("Delay")))
    {
        preset.effects["AnalogDelay"] = makeEffectVar({
            { "bypassed",  delay->isBypassed() },
            { "time",      delay->getTime() },
            { "intensity", delay->getIntensity() },
            { "echo",      delay->getEcho() },
            { "modDepth",  delay->getModDepth() },
            { "modRate",   delay->getModRate() },
            { "tone",      delay->getTone() }
        });
    }

    if (auto* reverb = dynamic_cast<SpringReverb*>(chain.getEffectByName("Spring Reverb")))
    {
        preset.effects["SpringReverb"] = makeEffectVar({
            { "bypassed",    reverb->isBypassed() },
            { "dwell",       reverb->getDwell() },
            { "decay",       reverb->getDecay() },
            { "tone",        reverb->getTone() },
            { "mix",         reverb->getMix() },
            { "drip",        reverb->getDrip() },
            { "springType",  reverb->getSpringType() }
        });
    }

    if (auto* plate = dynamic_cast<PlateReverb*>(chain.getEffectByName("Plate Reverb")))
    {
        preset.effects["PlateReverb"] = makeEffectVar({
            { "bypassed",   plate->isBypassed() },
            { "decay",      plate->getDecay() },
            { "damping",    plate->getDamping() },
            { "preDelay",   plate->getPreDelay() },
            { "mix",        plate->getMix() },
            { "width",      plate->getWidth() },
            { "plateType",  plate->getPlateType() }
        });
    }

    if (auto* chorus = dynamic_cast<Chorus*>(chain.getEffectByName("Chorus")))
    {
        preset.effects["Chorus"] = makeEffectVar({
            { "bypassed",  chorus->isBypassed() },
            { "rate",      chorus->getRate() },
            { "depth",     chorus->getDepth() },
            { "eq",        chorus->getEQ() },
            { "eLevel",    chorus->getELevel() }
        });
    }

    if (auto* flanger = dynamic_cast<Flanger*>(chain.getEffectByName("Flanger")))
    {
        preset.effects["Flanger"] = makeEffectVar({
            { "bypassed",          flanger->isBypassed() },
            { "rate",              flanger->getRate() },
            { "depth",             flanger->getDepth() },
            { "manual",            flanger->getManual() },
            { "feedback",          flanger->getFeedback() },
            { "feedbackPositive",  flanger->getFeedbackPositive() },
            { "eq",               flanger->getEQ() },
            { "mix",              flanger->getMix() }
        });
    }

    if (auto* phaser = dynamic_cast<Phaser*>(chain.getEffectByName("Phaser")))
    {
        preset.effects["Phaser"] = makeEffectVar({
            { "bypassed",  phaser->isBypassed() },
            { "rate",      phaser->getRate() },
            { "depth",     phaser->getDepth() },
            { "feedback",  phaser->getFeedback() },
            { "mix",       phaser->getMix() },
            { "stages",    phaser->getStages() }
        });
    }

    if (auto* vibrato = dynamic_cast<Vibrato*>(chain.getEffectByName("Vibrato")))
    {
        preset.effects["Vibrato"] = makeEffectVar({
            { "bypassed",  vibrato->isBypassed() },
            { "rate",      vibrato->getRate() },
            { "depth",     vibrato->getDepth() },
            { "tone",      vibrato->getTone() }
        });
    }

    if (auto* eq = dynamic_cast<Equalizer*>(chain.getEffectByName("EQ")))
    {
        preset.effects["Equalizer"] = makeEffectVar({
            { "bypassed",  eq->isBypassed() },
            { "bass",     eq->getBass() },
            { "midGain",  eq->getMidGain() },
            { "midFreq",  eq->getMidFreq() },
            { "treble",   eq->getTreble() },
            { "level",    eq->getLevel() }
        });
    }

    // Capture chain order (empty if default, saves space in JSON)
    if (!chain.isDefaultOrder())
        preset.chainOrder = chain.getEffectOrder();

    return preset;
}

void PresetManager::applyPreset(const Preset& preset)
{
    processor.setLimiterEnabled(preset.limiterEnabled);
    processor.setAmpSimEngine(preset.ampSimEngine);
    processor.setReverbEngine(preset.reverbEngine);
    processor.setModulationEngine(preset.modulationEngine);

    auto& chain = processor.getEffectChain();

    auto getDouble = [](const juce::var& obj, const char* key, double def) -> float
    {
        auto* dyn = obj.getDynamicObject();
        if (dyn == nullptr) return static_cast<float>(def);
        auto val = dyn->getProperty(juce::Identifier(key));
        return val.isVoid() ? static_cast<float>(def) : static_cast<float>((double)val);
    };

    auto getBool = [](const juce::var& obj, const char* key, bool def) -> bool
    {
        auto* dyn = obj.getDynamicObject();
        if (dyn == nullptr) return def;
        auto val = dyn->getProperty(juce::Identifier(key));
        return val.isVoid() ? def : (bool)val;
    };

    auto getInt = [](const juce::var& obj, const char* key, int def) -> int
    {
        auto* dyn = obj.getDynamicObject();
        if (dyn == nullptr) return def;
        auto val = dyn->getProperty(juce::Identifier(key));
        return val.isVoid() ? def : (int)val;
    };

    // Apply chain order before setting params (so effects are in the right slots)
    if (!preset.chainOrder.empty())
        chain.setEffectOrder(preset.chainOrder);
    else
        chain.setEffectOrder(EffectChain::getDefaultOrder());

    // Name-based lookup - position-independent
    {
        auto it = preset.effects.find("NoiseGate");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* ng = dynamic_cast<NoiseGate*>(chain.getEffectByName("Noise Gate")))
            {
                ng->setBypassed(getBool(v, "bypassed", true));
                ng->setThresholdDb(getDouble(v, "threshold", -40.0));
                ng->setAttackSeconds(getDouble(v, "attack", 0.001));
                ng->setHoldSeconds(getDouble(v, "hold", 0.05));
                ng->setReleaseSeconds(getDouble(v, "release", 0.1));
                ng->setRangeDb(getDouble(v, "range", -90.0));
            }
        }
    }

    {
        auto it = preset.effects.find("DiodeDrive");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* dd = dynamic_cast<DiodeDrive*>(chain.getEffectByName("Diode Drive")))
            {
                dd->setBypassed(getBool(v, "bypassed", true));
                dd->setDrive(getDouble(v, "drive", 0.5));
                dd->setTone(getDouble(v, "tone", 0.5));
                dd->setLevel(getDouble(v, "level", 0.7));
            }
        }
    }

    {
        auto it = preset.effects.find("Distortion");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* dist = dynamic_cast<Distortion*>(chain.getEffectByName("Distortion")))
            {
                dist->setBypassed(getBool(v, "bypassed", true));
                dist->setDrive(getDouble(v, "drive", 0.5));
                dist->setTone(getDouble(v, "tone", 0.65));
                dist->setLevel(getDouble(v, "level", 0.7));
                dist->setMix(getDouble(v, "mix", 0.2));
                dist->setSaturate(getDouble(v, "saturate", 0.5));
                dist->setSaturateEnabled(getBool(v, "saturateEnabled", false));
                dist->setMode(static_cast<Distortion::Mode>(getInt(v, "mode", 0)));
                dist->setClipType(static_cast<Distortion::ClipType>(getInt(v, "clipType", 1)));
            }
        }
    }

    {
        auto it = preset.effects.find("AmpSimSilver");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* amp = dynamic_cast<AmpSimSilver*>(chain.getEffectByName("Amp Silver")))
            {
                amp->setBypassed(getBool(v, "bypassed", true));
                amp->setGain(getDouble(v, "gain", 0.3));
                amp->setBass(getDouble(v, "bass", 0.5));
                amp->setMid(getDouble(v, "mid", 0.5));
                amp->setTreble(getDouble(v, "treble", 0.5));
                amp->setPreampBoost(getBool(v, "preampBoost", false));
                amp->setSpeakerDrive(getDouble(v, "speakerDrive", 0.2));
                amp->setCabinetType(getInt(v, "cabinetType", 0));
                amp->setBrightness(getDouble(v, "brightness", 0.5));
                amp->setMicPosition(getDouble(v, "micPosition", 0.3));
                amp->setCabTrim(static_cast<float>(getDouble(v, "cabTrim", 0.0)));
            }
        }
    }

    {
        auto it = preset.effects.find("AmpSimGold");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* amp2 = dynamic_cast<AmpSimGold*>(chain.getEffectByName("Amp Gold")))
            {
                amp2->setBypassed(getBool(v, "bypassed", true));
                amp2->setGain(getDouble(v, "gain", 0.4));
                amp2->setBass(getDouble(v, "bass", 0.85));
                amp2->setMid(getDouble(v, "mid", 0.85));
                amp2->setTreble(getDouble(v, "treble", 0.85));
                amp2->setPreampBoost(getBool(v, "preampBoost", false));
                amp2->setSpeakerDrive(getDouble(v, "speakerDrive", 0.2));
                amp2->setPresence(getDouble(v, "presence", 0.92));
                amp2->setCabinetType(getInt(v, "cabinetType", 0));
                amp2->setBrightness(getDouble(v, "brightness", 0.6));
                amp2->setMicPosition(getDouble(v, "micPosition", 0.5));
                amp2->setCabTrim(static_cast<float>(getDouble(v, "cabTrim", 0.0)));
            }
        }
    }

    {
        auto it = preset.effects.find("AnalogDelay");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* delay = dynamic_cast<AnalogDelay*>(chain.getEffectByName("Delay")))
            {
                delay->setBypassed(getBool(v, "bypassed", true));
                delay->setTime(getDouble(v, "time", 0.769));
                delay->setIntensity(getDouble(v, "intensity", 0.35));
                delay->setEcho(getDouble(v, "echo", 0.5));
                delay->setModDepth(getDouble(v, "modDepth", 0.3));
                delay->setModRate(getDouble(v, "modRate", 0.3));
                delay->setTone(getDouble(v, "tone", 0.5));
            }
        }
    }

    {
        auto it = preset.effects.find("SpringReverb");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* reverb = dynamic_cast<SpringReverb*>(chain.getEffectByName("Spring Reverb")))
            {
                reverb->setBypassed(getBool(v, "bypassed", true));
                reverb->setDwell(getDouble(v, "dwell", 0.5));
                reverb->setDecay(getDouble(v, "decay", 0.5));
                reverb->setTone(getDouble(v, "tone", 0.5));
                reverb->setMix(getDouble(v, "mix", 0.3));
                reverb->setDrip(getDouble(v, "drip", 0.4));
                reverb->setSpringType(getInt(v, "springType", 1));
            }
        }
    }

    {
        auto it = preset.effects.find("PlateReverb");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* plate = dynamic_cast<PlateReverb*>(chain.getEffectByName("Plate Reverb")))
            {
                plate->setBypassed(getBool(v, "bypassed", true));
                plate->setDecay(getDouble(v, "decay", 0.5));
                plate->setDamping(getDouble(v, "damping", 0.3));
                plate->setPreDelay(getDouble(v, "preDelay", 0.0));
                plate->setMix(getDouble(v, "mix", 0.3));
                plate->setWidth(getDouble(v, "width", 1.0));
                plate->setPlateType(getInt(v, "plateType", 0));
            }
        }
    }

    {
        auto it = preset.effects.find("Chorus");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* chorus = dynamic_cast<Chorus*>(chain.getEffectByName("Chorus")))
            {
                chorus->setBypassed(getBool(v, "bypassed", true));
                chorus->setRate(getDouble(v, "rate", 0.3));
                chorus->setDepth(getDouble(v, "depth", 0.4));
                chorus->setEQ(getDouble(v, "eq", 0.7));
                chorus->setELevel(getDouble(v, "eLevel", 0.5));
            }
        }
    }

    {
        auto it = preset.effects.find("Flanger");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* flanger = dynamic_cast<Flanger*>(chain.getEffectByName("Flanger")))
            {
                flanger->setBypassed(getBool(v, "bypassed", true));
                flanger->setRate(getDouble(v, "rate", 0.2));
                flanger->setDepth(getDouble(v, "depth", 0.35));
                flanger->setManual(getDouble(v, "manual", 0.3));
                flanger->setFeedback(getDouble(v, "feedback", 0.45));
                flanger->setFeedbackPositive(getBool(v, "feedbackPositive", true));
                flanger->setEQ(getDouble(v, "eq", 0.7));
                flanger->setMix(getDouble(v, "mix", 0.5));
            }
        }
    }

    {
        auto it = preset.effects.find("Phaser");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* phaser = dynamic_cast<Phaser*>(chain.getEffectByName("Phaser")))
            {
                phaser->setBypassed(getBool(v, "bypassed", true));
                phaser->setRate(getDouble(v, "rate", 0.15));
                phaser->setDepth(getDouble(v, "depth", 0.5));
                phaser->setFeedback(getDouble(v, "feedback", 0.3));
                phaser->setMix(getDouble(v, "mix", 0.5));
                phaser->setStages(static_cast<int>(getDouble(v, "stages", 0)));
            }
        }
    }

    {
        auto it = preset.effects.find("Vibrato");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* vibrato = dynamic_cast<Vibrato*>(chain.getEffectByName("Vibrato")))
            {
                vibrato->setBypassed(getBool(v, "bypassed", true));
                vibrato->setRate(getDouble(v, "rate", 0.5));
                vibrato->setDepth(getDouble(v, "depth", 0.3));
                vibrato->setTone(getDouble(v, "tone", 0.7));
            }
        }
    }

    {
        auto it = preset.effects.find("Equalizer");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* eq = dynamic_cast<Equalizer*>(chain.getEffectByName("EQ")))
            {
                eq->setBypassed(getBool(v, "bypassed", true));
                eq->setBass(getDouble(v, "bass", 0.0));
                eq->setMidGain(getDouble(v, "midGain", 0.0));
                eq->setMidFreq(getDouble(v, "midFreq", 0.5));
                eq->setTreble(getDouble(v, "treble", 0.0));
                eq->setLevel(getDouble(v, "level", 0.0));
            }
        }
    }
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
            // Default slots by name (stable across preset additions)
            static const char* defaultNames[numSlots] = { "Clean", "Blues", "Classic Rock", nullptr };
            if (defaultNames[s] != nullptr)
                slotAssignments[s] = findPresetByName(defaultNames[s]);
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
