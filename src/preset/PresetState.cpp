#include "PresetState.h"
#include "PluginProcessor.h"
#include "dsp/EffectChain.h"
#include "dsp/Compressor.h"
#include "dsp/Wah.h"
#include "dsp/NoiseGate.h"
#include "dsp/Distortion.h"
#include "dsp/DiodeDrive.h"
#include "dsp/AmpSimSilver.h"
#include "dsp/AmpSimGold.h"
#include "dsp/AmpSimPlatinum.h"
#include "dsp/AnalogDelay.h"
#include "dsp/SpringReverb.h"
#include "dsp/PlateReverb.h"
#include "dsp/Chorus.h"
#include "dsp/Flanger.h"
#include "dsp/Phaser.h"
#include "dsp/Vibrato.h"
#include "dsp/Tremolo.h"
#include "dsp/Equalizer.h"

namespace PresetState
{

Preset capture(OpenRiffBoxProcessor& processor)
{
    Preset preset;
    preset.limiterEnabled   = processor.isLimiterEnabled();
    preset.ampSimEngine     = processor.getAmpSimEngine();
    preset.reverbEngine     = processor.getReverbEngine();
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
    if (auto* comp = dynamic_cast<Compressor*>(chain.getEffectByName("Compressor")))
    {
        preset.effects["Compressor"] = makeEffectVar({
            { "bypassed", comp->isBypassed() },
            { "sustain",  comp->getSustain() },
            { "attack",   comp->getAttack()  },
            { "blend",    comp->getBlend()   },
            { "level",    comp->getLevel()   },
            { "mode",     comp->getMode()    }
        });
    }

    if (auto* wah = dynamic_cast<Wah*>(chain.getEffectByName("Wah")))
    {
        preset.effects["Wah"] = makeEffectVar({
            { "bypassed",   wah->isBypassed() },
            { "position",   wah->getPosition() },
            { "coloration", wah->getColoration() },
            { "taperMode",  wah->getTaperMode() }
        });
    }

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

    if (auto* plat = dynamic_cast<AmpSimPlatinum*>(chain.getEffectByName("Amp Platinum")))
    {
        preset.effects["AmpSimPlatinum"] = makeEffectVar({
            { "bypassed",    plat->isBypassed() },
            { "gain",        plat->getGain() },
            { "ovLevel",     plat->getOvLevel() },
            { "bass",        plat->getBass() },
            { "mid",         plat->getMid() },
            { "treble",      plat->getTreble() },
            { "master",      plat->getMaster() },
            { "gainMode",    plat->getGainMode() },
            { "cabinetType", plat->getCabinetType() },
            { "micPosition", plat->getMicPosition() },
            { "cabTrim",     plat->getCabTrim() },
            { "channel",       plat->getChannel() },
            { "boost",         plat->getBoost() },
            { "inputLow",      plat->getInputLow() },
            { "normalBass",    plat->getNormalBass() },
            { "normalMid",     plat->getNormalMid() },
            { "normalTreble",  plat->getNormalTreble() },
            { "normalLevel",   plat->getNormalLevel() }
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

    if (auto* tremolo = dynamic_cast<Tremolo*>(chain.getEffectByName("Tremolo")))
    {
        preset.effects["Tremolo"] = makeEffectVar({
            { "bypassed", tremolo->isBypassed() },
            { "rate",     tremolo->getRate() },
            { "depth",    tremolo->getDepth() },
            { "mode",     tremolo->getMode() },
            { "width",    tremolo->getWidth() },
            { "output",   tremolo->getOutput() }
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

void apply(const Preset& preset, OpenRiffBoxProcessor& processor)
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
    {
        auto order = preset.chainOrder;
        // Trap 2: old presets lack "Compressor"; appending at the end via
        // setEffectOrder would park the row after EQ. Insert at the front instead.
        bool hasComp = false;
        for (auto& n : order) if (n == "Compressor") { hasComp = true; break; }
        if (!hasComp)
            order.insert(order.begin(), "Compressor");

        // Trap 2b: old presets lack "Wah". Splice it in right after Compressor
        // (its default slot) instead of appending after EQ.
        bool hasWah = false;
        for (auto& n : order) if (n == "Wah") { hasWah = true; break; }
        if (!hasWah)
        {
            int compIdx = 0;
            for (int k = 0; k < static_cast<int>(order.size()); ++k)
                if (order[static_cast<size_t>(k)] == "Compressor") { compIdx = k; break; }
            order.insert(order.begin() + compIdx + 1, "Wah");
        }

        chain.setEffectOrder(order);
    }
    else
        chain.setEffectOrder(EffectChain::getDefaultOrder());

    // Name-based lookup - position-independent
    {
        auto it = preset.effects.find("Compressor");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* comp = dynamic_cast<Compressor*>(chain.getEffectByName("Compressor")))
            {
                comp->setBypassed(getBool(v, "bypassed", true));
                comp->setSustain(getDouble(v, "sustain", 0.35));
                comp->setAttack (getDouble(v, "attack",  0.50));
                comp->setBlend  (getDouble(v, "blend",   1.00));
                comp->setLevel  (getDouble(v, "level",   0.50));
                comp->setMode   (getInt   (v, "mode",    0));
            }
        }
        else
        {
            // Old presets: reset and bypass. Compressor is not engine-managed so
            // setAmpSimEngine/setReverbEngine won't touch it -- must force bypass here.
            if (auto* comp = dynamic_cast<Compressor*>(chain.getEffectByName("Compressor")))
            {
                comp->setBypassed(true);
                comp->resetToDefaults();
            }
        }
    }

    {
        auto it = preset.effects.find("Wah");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* wah = dynamic_cast<Wah*>(chain.getEffectByName("Wah")))
            {
                wah->setBypassed(getBool(v, "bypassed", true));
                wah->setPosition(getDouble(v, "position", 0.5));
                wah->setColoration(getBool(v, "coloration", false));
                wah->setTaperMode(getInt(v, "taperMode", 0));
            }
        }
        else
        {
            // Old presets: reset and bypass. Wah is not engine-managed so no
            // setXEngine call will touch it -- must force bypass here.
            if (auto* wah = dynamic_cast<Wah*>(chain.getEffectByName("Wah")))
            {
                wah->setBypassed(true);
                wah->resetToDefaults();
            }
        }
    }

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
                dd->setLevel(getDouble(v, "level", 0.5));
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
                // Mode before drive/tone/level: those setters route to the Metal
                // engine only when the mode is already Metal.
                dist->setMode(static_cast<Distortion::Mode>(getInt(v, "mode", 0)));
                dist->setDrive(getDouble(v, "drive", 0.5));
                dist->setTone(getDouble(v, "tone", 0.65));
                dist->setLevel(getDouble(v, "level", 0.7));
                dist->setMix(getDouble(v, "mix", 0.2));
                dist->setSaturate(getDouble(v, "saturate", 0.5));
                dist->setSaturateEnabled(getBool(v, "saturateEnabled", false));
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
                amp2->setBass(getDouble(v, "bass", 0.5));
                amp2->setMid(getDouble(v, "mid", 0.6));
                amp2->setTreble(getDouble(v, "treble", 0.5));
                amp2->setPreampBoost(getBool(v, "preampBoost", false));
                amp2->setSpeakerDrive(getDouble(v, "speakerDrive", 0.2));
                amp2->setPresence(getDouble(v, "presence", 0.70));
                amp2->setCabinetType(getInt(v, "cabinetType", 10));
                amp2->setBrightness(getDouble(v, "brightness", 0.6));
                amp2->setMicPosition(getDouble(v, "micPosition", 0.5));
                amp2->setCabTrim(static_cast<float>(getDouble(v, "cabTrim", 0.0)));
            }
        }
    }

    {
        auto it = preset.effects.find("AmpSimPlatinum");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* plat = dynamic_cast<AmpSimPlatinum*>(chain.getEffectByName("Amp Platinum")))
            {
                plat->setBypassed(getBool(v, "bypassed", true));
                plat->setGain(getDouble(v, "gain", 0.5));
                plat->setOvLevel(getDouble(v, "ovLevel", 0.7));
                plat->setBass(getDouble(v, "bass", 0.5));
                plat->setMid(getDouble(v, "mid", 0.5));
                plat->setTreble(getDouble(v, "treble", 0.5));
                plat->setMaster(getDouble(v, "master", 0.64));
                plat->setGainMode(getInt(v, "gainMode", 0));
                plat->setCabinetType(getInt(v, "cabinetType", 0));
                plat->setMicPosition(getDouble(v, "micPosition", 0.5));
                plat->setCabTrim(static_cast<float>(getDouble(v, "cabTrim", 0.0)));
                plat->setChannel(getInt(v, "channel", 0));
                plat->setBoost(getBool(v, "boost", false));
                plat->setInputLow(getBool(v, "inputLow", false));
                plat->setNormalBass(getDouble(v, "normalBass", 0.5));
                plat->setNormalMid(getDouble(v, "normalMid", 0.5));
                plat->setNormalTreble(getDouble(v, "normalTreble", 0.5));
                plat->setNormalLevel(getDouble(v, "normalLevel", 0.5));
            }
        }
        else
        {
            // Old preset: reset params to ctor defaults. Do NOT touch bypassed --
            // setAmpSimEngine (called above) owns amp bypass states. An old preset
            // with ampSimEngine==2 must still be audible after load.
            if (auto* plat = dynamic_cast<AmpSimPlatinum*>(chain.getEffectByName("Amp Platinum")))
            {
                plat->setGain(0.5f);
                plat->setOvLevel(0.7f);
                plat->setBass(0.5f);
                plat->setMid(0.5f);
                plat->setTreble(0.5f);
                plat->setMaster(0.64f);
                plat->setGainMode(0);
                plat->setCabinetType(0);
                plat->setMicPosition(0.5f);
                plat->setCabTrim(0.0f);
                plat->setChannel(0);
                plat->setBoost(false);
                plat->setInputLow(false);
                plat->setNormalBass(0.5f);
                plat->setNormalMid(0.5f);
                plat->setNormalTreble(0.5f);
                plat->setNormalLevel(0.5f);
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
                phaser->setStages(getInt(v, "stages", 0));
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
        auto it = preset.effects.find("Tremolo");
        if (it != preset.effects.end())
        {
            auto& v = it->second;
            if (auto* tremolo = dynamic_cast<Tremolo*>(chain.getEffectByName("Tremolo")))
            {
                tremolo->setBypassed(getBool(v, "bypassed", true));
                tremolo->setRate(getDouble(v, "rate", 0.4));
                tremolo->setDepth(getDouble(v, "depth", 0.5));
                tremolo->setMode(getInt(v, "mode", 0));
                tremolo->setWidth(getDouble(v, "width", 0.0));
                tremolo->setOutput(getDouble(v, "output", 0.5));
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

} // namespace PresetState
