#include "PluginProcessor.h"
#include "PluginEditor.h"
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
#include "dsp/Equalizer.h"

//==============================================================================
OpenRiffBoxProcessor::OpenRiffBoxProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    effectChain.addEffect(std::make_unique<DiodeDrive>());
    effectChain.addEffect(std::make_unique<Distortion>());
    effectChain.addEffect(std::make_unique<AmpSimSilver>());
    effectChain.addEffect(std::make_unique<AmpSimGold>());
    effectChain.addEffect(std::make_unique<AmpSimPlatinum>());
    effectChain.addEffect(std::make_unique<NoiseGate>());
    effectChain.addEffect(std::make_unique<AnalogDelay>());
    effectChain.addEffect(std::make_unique<SpringReverb>());
    effectChain.addEffect(std::make_unique<PlateReverb>());
    effectChain.addEffect(std::make_unique<Chorus>());
    effectChain.addEffect(std::make_unique<Flanger>());
    effectChain.addEffect(std::make_unique<Phaser>());
    effectChain.addEffect(std::make_unique<Vibrato>());
    effectChain.addEffect(std::make_unique<Equalizer>());
}

//==============================================================================
void OpenRiffBoxProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    effectChain.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(effectChain.getTotalLatencySamples());

    tunerEngine.prepare(sampleRate);
}

void OpenRiffBoxProcessor::releaseResources()
{
    effectChain.reset();
    tunerEngine.reset();
}

//==============================================================================
void OpenRiffBoxProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // When stopped, output silence.
    if (!audioActive.load(std::memory_order_acquire))
    {
        buffer.clear();
        inputPeak.store(0.0f, std::memory_order_relaxed);
        outputPeak.store(0.0f, std::memory_order_relaxed);
        cpuLoad.store(0.0f, std::memory_order_relaxed);
        return;
    }

    auto startTicks = juce::Time::getHighResolutionTicks();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that have no corresponding input channel
    // (e.g. when running as mono-in / stereo-out).
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    // If mono input, copy channel 0 to channel 1 for stereo passthrough.
    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());

    // Measure input peak (before effects processing)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
        inputPeak.store(peak, std::memory_order_relaxed);
    }

    // Feed tuner from raw input (channel 0, before effects).
    // When tuning, skip the effect chain but pass clean signal through
    // (master volume + limiter still apply below).
    bool tuning = tunerActive.load(std::memory_order_acquire);
    if (tuning)
        tunerEngine.processSamples(buffer.getReadPointer(0), buffer.getNumSamples());

    // Run the effect chain (skipped while tuning - clean passthrough).
    if (!tuning)
        effectChain.process(buffer);

    // Update reported latency when bypass states change (e.g. amp sim engine swap).
    // Only active (non-bypassed) effects contribute latency to the signal path.
    {
        int latency = effectChain.getTotalLatencySamples();
        if (latency != lastReportedLatency)
        {
            setLatencySamples(latency);
            lastReportedLatency = latency;
        }
    }

    // Master volume (after effects, before limiter)
    {
        float vol = masterVolume.load(std::memory_order_relaxed);
        if (vol < 0.999f)
            buffer.applyGain(vol);
    }

    // Output limiter — brickwall at -0.1 dBFS (protects headphones/speakers)
    bool clipped = false;
    if (limiterEnabled.load(std::memory_order_acquire))
    {
        constexpr float threshold = 0.9886f; // -0.1 dBFS
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* samples = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (samples[i] > threshold || samples[i] < -threshold)
                {
                    samples[i] = juce::jlimit(-threshold, threshold, samples[i]);
                    clipped = true;
                }
            }
        }
    }
    limiterClipping.store(clipped, std::memory_order_relaxed);

    // Measure output peak (after effects + limiter)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            peak = juce::jmax(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
        outputPeak.store(peak, std::memory_order_relaxed);
    }

    // CPU load: processing time / available budget
    auto endTicks = juce::Time::getHighResolutionTicks();
    double elapsed = juce::Time::highResolutionTicksToSeconds(endTicks - startTicks);
    double budget  = static_cast<double>(buffer.getNumSamples()) / currentSampleRate;
    cpuLoad.store(static_cast<float>(elapsed / budget), std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* OpenRiffBoxProcessor::createEditor()
{
    return new OpenRiffBoxEditor(*this);
}

//==============================================================================
void OpenRiffBoxProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = std::make_unique<juce::XmlElement>("OpenRiffBoxState");
    // audioActive intentionally not saved - always start stopped
    xml->setAttribute("limiterEnabled", limiterEnabled.load(std::memory_order_acquire));
    xml->setAttribute("masterVolume", static_cast<double>(masterVolume.load(std::memory_order_relaxed)));
    xml->setAttribute("ampSimEngine", ampSimEngine);
    xml->setAttribute("reverbEngine", reverbEngine);
    xml->setAttribute("modulationEngine", modulationEngine);

    // Save chain order (only if non-default)
    if (!effectChain.isDefaultOrder())
    {
        auto order = effectChain.getEffectOrder();
        juce::String orderStr;
        for (size_t i = 0; i < order.size(); ++i)
        {
            if (i > 0) orderStr += ";";
            orderStr += order[i];
        }
        xml->setAttribute("chainOrder", orderStr);
    }
    // (default order: nothing to save)

    // Name-based lookup - position-independent
    if (auto* ng = dynamic_cast<NoiseGate*>(effectChain.getEffectByName("Noise Gate")))
    {
        auto* e = xml->createNewChildElement("NoiseGate");
        e->setAttribute("bypassed",  ng->isBypassed());
        e->setAttribute("threshold", ng->getThresholdDb());
        e->setAttribute("attack",    ng->getAttackSeconds());
        e->setAttribute("hold",      ng->getHoldSeconds());
        e->setAttribute("release",   ng->getReleaseSeconds());
        e->setAttribute("range",     ng->getRangeDb());
    }

    if (auto* dd = dynamic_cast<DiodeDrive*>(effectChain.getEffectByName("Diode Drive")))
    {
        auto* e = xml->createNewChildElement("DiodeDrive");
        e->setAttribute("bypassed", dd->isBypassed());
        e->setAttribute("drive",    dd->getDrive());
        e->setAttribute("tone",     dd->getTone());
        e->setAttribute("level",    dd->getLevel());
    }

    if (auto* dist = dynamic_cast<Distortion*>(effectChain.getEffectByName("Distortion")))
    {
        auto* e = xml->createNewChildElement("Distortion");
        e->setAttribute("bypassed",        dist->isBypassed());
        e->setAttribute("drive",           dist->getDrive());
        e->setAttribute("tone",            dist->getTone());
        e->setAttribute("level",           dist->getLevel());
        e->setAttribute("mix",             dist->getMix());
        e->setAttribute("saturate",        dist->getSaturate());
        e->setAttribute("saturateEnabled", dist->getSaturateEnabled());
        e->setAttribute("mode",            static_cast<int>(dist->getMode()));
        e->setAttribute("clipType",        static_cast<int>(dist->getClipType()));
    }

    if (auto* amp = dynamic_cast<AmpSimSilver*>(effectChain.getEffectByName("Amp Silver")))
    {
        auto* e = xml->createNewChildElement("AmpSimSilver");
        e->setAttribute("bypassed",      amp->isBypassed());
        e->setAttribute("gain",          amp->getGain());
        e->setAttribute("bass",          amp->getBass());
        e->setAttribute("mid",           amp->getMid());
        e->setAttribute("treble",        amp->getTreble());
        e->setAttribute("preampBoost",   amp->getPreampBoost());
        e->setAttribute("speakerDrive",  amp->getSpeakerDrive());
        e->setAttribute("cabinetType",   amp->getCabinetType());
        e->setAttribute("brightness",    amp->getBrightness());
        e->setAttribute("micPosition",   amp->getMicPosition());
        e->setAttribute("cabTrim",       amp->getCabTrim());
    }

    if (auto* amp2 = dynamic_cast<AmpSimGold*>(effectChain.getEffectByName("Amp Gold")))
    {
        auto* e = xml->createNewChildElement("AmpSimGold");
        e->setAttribute("bypassed",      amp2->isBypassed());
        e->setAttribute("gain",          amp2->getGain());
        e->setAttribute("bass",          amp2->getBass());
        e->setAttribute("mid",           amp2->getMid());
        e->setAttribute("treble",        amp2->getTreble());
        e->setAttribute("preampBoost",   amp2->getPreampBoost());
        e->setAttribute("speakerDrive",  amp2->getSpeakerDrive());
        e->setAttribute("presence",      amp2->getPresence());
        e->setAttribute("cabinetType",   amp2->getCabinetType());
        e->setAttribute("brightness",    amp2->getBrightness());
        e->setAttribute("micPosition",   amp2->getMicPosition());
        e->setAttribute("cabTrim",       amp2->getCabTrim());
    }

    if (auto* plat = dynamic_cast<AmpSimPlatinum*>(effectChain.getEffectByName("Amp Platinum")))
    {
        auto* e = xml->createNewChildElement("AmpSimPlatinum");
        e->setAttribute("bypassed",      plat->isBypassed());
        e->setAttribute("gain",          plat->getGain());
        e->setAttribute("ovLevel",       plat->getOvLevel());
        e->setAttribute("bass",          plat->getBass());
        e->setAttribute("mid",           plat->getMid());
        e->setAttribute("treble",        plat->getTreble());
        e->setAttribute("master",        plat->getMaster());
        e->setAttribute("gainMode",      plat->getGainMode());
        e->setAttribute("cabinetType",   plat->getCabinetType());
        e->setAttribute("micPosition",   plat->getMicPosition());
        e->setAttribute("cabTrim",       plat->getCabTrim());
    }

    if (auto* delay = dynamic_cast<AnalogDelay*>(effectChain.getEffectByName("Delay")))
    {
        auto* e = xml->createNewChildElement("AnalogDelay");
        e->setAttribute("bypassed",  delay->isBypassed());
        e->setAttribute("time",      delay->getTime());
        e->setAttribute("intensity", delay->getIntensity());
        e->setAttribute("echo",      delay->getEcho());
        e->setAttribute("modDepth",  delay->getModDepth());
        e->setAttribute("modRate",   delay->getModRate());
        e->setAttribute("tone",      delay->getTone());
    }

    if (auto* reverb = dynamic_cast<SpringReverb*>(effectChain.getEffectByName("Spring Reverb")))
    {
        auto* e = xml->createNewChildElement("SpringReverb");
        e->setAttribute("bypassed",    reverb->isBypassed());
        e->setAttribute("dwell",       reverb->getDwell());
        e->setAttribute("decay",       reverb->getDecay());
        e->setAttribute("tone",        reverb->getTone());
        e->setAttribute("mix",         reverb->getMix());
        e->setAttribute("drip",        reverb->getDrip());
        e->setAttribute("springType",  reverb->getSpringType());
    }

    if (auto* plate = dynamic_cast<PlateReverb*>(effectChain.getEffectByName("Plate Reverb")))
    {
        auto* e = xml->createNewChildElement("PlateReverb");
        e->setAttribute("bypassed",   plate->isBypassed());
        e->setAttribute("decay",      plate->getDecay());
        e->setAttribute("damping",    plate->getDamping());
        e->setAttribute("preDelay",   plate->getPreDelay());
        e->setAttribute("mix",        plate->getMix());
        e->setAttribute("width",      plate->getWidth());
        e->setAttribute("plateType",  plate->getPlateType());
    }

    if (auto* chorus = dynamic_cast<Chorus*>(effectChain.getEffectByName("Chorus")))
    {
        auto* e = xml->createNewChildElement("Chorus");
        e->setAttribute("bypassed",  chorus->isBypassed());
        e->setAttribute("rate",      chorus->getRate());
        e->setAttribute("depth",     chorus->getDepth());
        e->setAttribute("eq",        chorus->getEQ());
        e->setAttribute("eLevel",    chorus->getELevel());
    }

    if (auto* flanger = dynamic_cast<Flanger*>(effectChain.getEffectByName("Flanger")))
    {
        auto* e = xml->createNewChildElement("Flanger");
        e->setAttribute("bypassed",          flanger->isBypassed());
        e->setAttribute("rate",              flanger->getRate());
        e->setAttribute("depth",             flanger->getDepth());
        e->setAttribute("manual",            flanger->getManual());
        e->setAttribute("feedback",          flanger->getFeedback());
        e->setAttribute("feedbackPositive",  flanger->getFeedbackPositive());
        e->setAttribute("eq",               flanger->getEQ());
        e->setAttribute("mix",              flanger->getMix());
    }

    if (auto* phaser = dynamic_cast<Phaser*>(effectChain.getEffectByName("Phaser")))
    {
        auto* e = xml->createNewChildElement("Phaser");
        e->setAttribute("bypassed",   phaser->isBypassed());
        e->setAttribute("rate",       phaser->getRate());
        e->setAttribute("depth",      phaser->getDepth());
        e->setAttribute("feedback",   phaser->getFeedback());
        e->setAttribute("mix",        phaser->getMix());
        e->setAttribute("stages",     phaser->getStages());
    }

    if (auto* vibrato = dynamic_cast<Vibrato*>(effectChain.getEffectByName("Vibrato")))
    {
        auto* e = xml->createNewChildElement("Vibrato");
        e->setAttribute("bypassed",  vibrato->isBypassed());
        e->setAttribute("rate",      vibrato->getRate());
        e->setAttribute("depth",     vibrato->getDepth());
        e->setAttribute("tone",      vibrato->getTone());
    }

    if (auto* eq = dynamic_cast<Equalizer*>(effectChain.getEffectByName("EQ")))
    {
        auto* e = xml->createNewChildElement("Equalizer");
        e->setAttribute("bypassed",  eq->isBypassed());
        e->setAttribute("bass",     eq->getBass());
        e->setAttribute("midGain",  eq->getMidGain());
        e->setAttribute("midFreq",  eq->getMidFreq());
        e->setAttribute("treble",   eq->getTreble());
        e->setAttribute("level",    eq->getLevel());
    }

    copyXmlToBinary(*xml, destData);
}

void OpenRiffBoxProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("OpenRiffBoxState"))
        return;

    // audioActive always starts false - don't restore it
    limiterEnabled.store(xml->getBoolAttribute("limiterEnabled", true), std::memory_order_release);
    masterVolume.store(static_cast<float>(xml->getDoubleAttribute("masterVolume", 1.0)), std::memory_order_relaxed);
    ampSimEngine = juce::jlimit(0, 2, xml->getIntAttribute("ampSimEngine", 1));
    reverbEngine = juce::jlimit(0, 1, xml->getIntAttribute("reverbEngine", 0));
    modulationEngine = juce::jlimit(0, 3, xml->getIntAttribute("modulationEngine", 0));

    // Restore chain order (if saved)
    auto orderStr = xml->getStringAttribute("chainOrder", "");
    if (orderStr.isNotEmpty())
    {
        std::vector<juce::String> order;
        auto tokens = juce::StringArray::fromTokens(orderStr, ";", "");
        for (auto& t : tokens)
            if (t.isNotEmpty())
                order.push_back(t);
        if (!order.empty())
            effectChain.setEffectOrder(order);
    }

    // Name-based lookup - position-independent
    if (auto* ngXml = xml->getChildByName("NoiseGate"))
    {
        if (auto* ng = dynamic_cast<NoiseGate*>(effectChain.getEffectByName("Noise Gate")))
        {
            ng->setBypassed(ngXml->getBoolAttribute("bypassed", true));
            ng->setThresholdDb(static_cast<float>(ngXml->getDoubleAttribute("threshold", -40.0)));
            ng->setAttackSeconds(static_cast<float>(ngXml->getDoubleAttribute("attack", 0.001)));
            ng->setHoldSeconds(static_cast<float>(ngXml->getDoubleAttribute("hold", 0.05)));
            ng->setReleaseSeconds(static_cast<float>(ngXml->getDoubleAttribute("release", 0.1)));
            ng->setRangeDb(static_cast<float>(ngXml->getDoubleAttribute("range", -90.0)));
        }
    }

    if (auto* ddXml = xml->getChildByName("DiodeDrive"))
    {
        if (auto* dd = dynamic_cast<DiodeDrive*>(effectChain.getEffectByName("Diode Drive")))
        {
            dd->setBypassed(ddXml->getBoolAttribute("bypassed", true));
            dd->setDrive(static_cast<float>(ddXml->getDoubleAttribute("drive", 0.5)));
            dd->setTone(static_cast<float>(ddXml->getDoubleAttribute("tone", 0.5)));
            dd->setLevel(static_cast<float>(ddXml->getDoubleAttribute("level", 0.7)));
        }
    }

    if (auto* distXml = xml->getChildByName("Distortion"))
    {
        if (auto* dist = dynamic_cast<Distortion*>(effectChain.getEffectByName("Distortion")))
        {
            dist->setBypassed(distXml->getBoolAttribute("bypassed", true));
            dist->setDrive(static_cast<float>(distXml->getDoubleAttribute("drive", 0.5)));
            dist->setTone(static_cast<float>(distXml->getDoubleAttribute("tone", 0.65)));
            dist->setLevel(static_cast<float>(distXml->getDoubleAttribute("level", 0.7)));
            dist->setMix(static_cast<float>(distXml->getDoubleAttribute("mix", 0.2)));
            dist->setSaturate(static_cast<float>(distXml->getDoubleAttribute("saturate", 0.5)));
            dist->setSaturateEnabled(distXml->getBoolAttribute("saturateEnabled", false));
            dist->setMode(static_cast<Distortion::Mode>(distXml->getIntAttribute("mode", 0)));
            dist->setClipType(static_cast<Distortion::ClipType>(distXml->getIntAttribute("clipType", 1)));
        }
    }

    if (auto* ampXml = xml->getChildByName("AmpSimSilver"))
    {
        if (auto* amp = dynamic_cast<AmpSimSilver*>(effectChain.getEffectByName("Amp Silver")))
        {
            amp->setBypassed(ampXml->getBoolAttribute("bypassed", true));
            amp->setGain(static_cast<float>(ampXml->getDoubleAttribute("gain", 0.3)));
            amp->setBass(static_cast<float>(ampXml->getDoubleAttribute("bass", 0.5)));
            amp->setMid(static_cast<float>(ampXml->getDoubleAttribute("mid", 0.5)));
            amp->setTreble(static_cast<float>(ampXml->getDoubleAttribute("treble", 0.5)));
            amp->setPreampBoost(ampXml->getBoolAttribute("preampBoost", false));
            amp->setSpeakerDrive(static_cast<float>(ampXml->getDoubleAttribute("speakerDrive", 0.2)));
            amp->setCabinetType(ampXml->getIntAttribute("cabinetType", 0));
            amp->setBrightness(static_cast<float>(ampXml->getDoubleAttribute("brightness", 0.5)));
            amp->setMicPosition(static_cast<float>(ampXml->getDoubleAttribute("micPosition", 0.3)));
            amp->setCabTrim(static_cast<float>(ampXml->getDoubleAttribute("cabTrim", 0.0)));
        }
    }

    if (auto* amp2Xml = xml->getChildByName("AmpSimGold"))
    {
        if (auto* amp2 = dynamic_cast<AmpSimGold*>(effectChain.getEffectByName("Amp Gold")))
        {
            amp2->setBypassed(amp2Xml->getBoolAttribute("bypassed", true));
            amp2->setGain(static_cast<float>(amp2Xml->getDoubleAttribute("gain", 0.4)));
            amp2->setBass(static_cast<float>(amp2Xml->getDoubleAttribute("bass", 0.85)));
            amp2->setMid(static_cast<float>(amp2Xml->getDoubleAttribute("mid", 0.85)));
            amp2->setTreble(static_cast<float>(amp2Xml->getDoubleAttribute("treble", 0.85)));
            amp2->setPreampBoost(amp2Xml->getBoolAttribute("preampBoost", false));
            amp2->setSpeakerDrive(static_cast<float>(amp2Xml->getDoubleAttribute("speakerDrive", 0.2)));
            amp2->setPresence(static_cast<float>(amp2Xml->getDoubleAttribute("presence", 0.92)));
            amp2->setCabinetType(amp2Xml->getIntAttribute("cabinetType", 0));
            amp2->setBrightness(static_cast<float>(amp2Xml->getDoubleAttribute("brightness", 0.6)));
            amp2->setMicPosition(static_cast<float>(amp2Xml->getDoubleAttribute("micPosition", 0.5)));
            amp2->setCabTrim(static_cast<float>(amp2Xml->getDoubleAttribute("cabTrim", 0.0)));
        }
    }

    if (auto* platXml = xml->getChildByName("AmpSimPlatinum"))
    {
        if (auto* plat = dynamic_cast<AmpSimPlatinum*>(effectChain.getEffectByName("Amp Platinum")))
        {
            plat->setBypassed(platXml->getBoolAttribute("bypassed", true));
            plat->setGain(static_cast<float>(platXml->getDoubleAttribute("gain", 0.5)));
            plat->setOvLevel(static_cast<float>(platXml->getDoubleAttribute("ovLevel", 0.7)));
            plat->setBass(static_cast<float>(platXml->getDoubleAttribute("bass", 0.5)));
            plat->setMid(static_cast<float>(platXml->getDoubleAttribute("mid", 0.5)));
            plat->setTreble(static_cast<float>(platXml->getDoubleAttribute("treble", 0.5)));
            plat->setMaster(static_cast<float>(platXml->getDoubleAttribute("master", 0.3)));
            plat->setGainMode(platXml->getIntAttribute("gainMode", 0));
            plat->setCabinetType(platXml->getIntAttribute("cabinetType", 0));
            plat->setMicPosition(static_cast<float>(platXml->getDoubleAttribute("micPosition", 0.5)));
            plat->setCabTrim(static_cast<float>(platXml->getDoubleAttribute("cabTrim", 0.0)));
        }
    }

    if (auto* delayXml = xml->getChildByName("AnalogDelay"))
    {
        if (auto* delay = dynamic_cast<AnalogDelay*>(effectChain.getEffectByName("Delay")))
        {
            delay->setBypassed(delayXml->getBoolAttribute("bypassed", true));
            delay->setTime(static_cast<float>(delayXml->getDoubleAttribute("time", 0.769)));
            delay->setIntensity(static_cast<float>(delayXml->getDoubleAttribute("intensity", 0.35)));
            delay->setEcho(static_cast<float>(delayXml->getDoubleAttribute("echo", 0.5)));
            delay->setModDepth(static_cast<float>(delayXml->getDoubleAttribute("modDepth", 0.3)));
            delay->setModRate(static_cast<float>(delayXml->getDoubleAttribute("modRate", 0.3)));
            delay->setTone(static_cast<float>(delayXml->getDoubleAttribute("tone", 0.5)));
        }
    }

    if (auto* reverbXml = xml->getChildByName("SpringReverb"))
    {
        if (auto* reverb = dynamic_cast<SpringReverb*>(effectChain.getEffectByName("Spring Reverb")))
        {
            reverb->setBypassed(reverbXml->getBoolAttribute("bypassed", true));
            reverb->setDwell(static_cast<float>(reverbXml->getDoubleAttribute("dwell", 0.5)));
            reverb->setDecay(static_cast<float>(reverbXml->getDoubleAttribute("decay", 0.5)));
            reverb->setTone(static_cast<float>(reverbXml->getDoubleAttribute("tone", 0.5)));
            reverb->setMix(static_cast<float>(reverbXml->getDoubleAttribute("mix", 0.3)));
            reverb->setDrip(static_cast<float>(reverbXml->getDoubleAttribute("drip", 0.4)));
            reverb->setSpringType(reverbXml->getIntAttribute("springType", 1));
        }
    }

    if (auto* plateXml = xml->getChildByName("PlateReverb"))
    {
        if (auto* plate = dynamic_cast<PlateReverb*>(effectChain.getEffectByName("Plate Reverb")))
        {
            plate->setBypassed(plateXml->getBoolAttribute("bypassed", true));
            plate->setDecay(static_cast<float>(plateXml->getDoubleAttribute("decay", 0.5)));
            plate->setDamping(static_cast<float>(plateXml->getDoubleAttribute("damping", 0.3)));
            plate->setPreDelay(static_cast<float>(plateXml->getDoubleAttribute("preDelay", 0.0)));
            plate->setMix(static_cast<float>(plateXml->getDoubleAttribute("mix", 0.3)));
            plate->setWidth(static_cast<float>(plateXml->getDoubleAttribute("width", 1.0)));
            plate->setPlateType(plateXml->getIntAttribute("plateType", 0));
        }
    }

    // Apply engine bypass states after all effects are loaded
    setAmpSimEngine(ampSimEngine);
    setReverbEngine(reverbEngine);
    setModulationEngine(modulationEngine);

    if (auto* chorusXml = xml->getChildByName("Chorus"))
    {
        if (auto* chorus = dynamic_cast<Chorus*>(effectChain.getEffectByName("Chorus")))
        {
            chorus->setBypassed(chorusXml->getBoolAttribute("bypassed", true));
            chorus->setRate(static_cast<float>(chorusXml->getDoubleAttribute("rate", 0.3)));
            chorus->setDepth(static_cast<float>(chorusXml->getDoubleAttribute("depth", 0.4)));
            chorus->setEQ(static_cast<float>(chorusXml->getDoubleAttribute("eq", 0.7)));
            chorus->setELevel(static_cast<float>(chorusXml->getDoubleAttribute("eLevel", 0.5)));
        }
    }

    if (auto* flangerXml = xml->getChildByName("Flanger"))
    {
        if (auto* flanger = dynamic_cast<Flanger*>(effectChain.getEffectByName("Flanger")))
        {
            flanger->setBypassed(flangerXml->getBoolAttribute("bypassed", true));
            flanger->setRate(static_cast<float>(flangerXml->getDoubleAttribute("rate", 0.2)));
            flanger->setDepth(static_cast<float>(flangerXml->getDoubleAttribute("depth", 0.35)));
            flanger->setManual(static_cast<float>(flangerXml->getDoubleAttribute("manual", 0.3)));
            flanger->setFeedback(static_cast<float>(flangerXml->getDoubleAttribute("feedback", 0.45)));
            flanger->setFeedbackPositive(flangerXml->getBoolAttribute("feedbackPositive", true));
            flanger->setEQ(static_cast<float>(flangerXml->getDoubleAttribute("eq", 0.7)));
            flanger->setMix(static_cast<float>(flangerXml->getDoubleAttribute("mix", 0.5)));
        }
    }

    if (auto* phaserXml = xml->getChildByName("Phaser"))
    {
        if (auto* phaser = dynamic_cast<Phaser*>(effectChain.getEffectByName("Phaser")))
        {
            phaser->setBypassed(phaserXml->getBoolAttribute("bypassed", true));
            phaser->setRate(static_cast<float>(phaserXml->getDoubleAttribute("rate", 0.15)));
            phaser->setDepth(static_cast<float>(phaserXml->getDoubleAttribute("depth", 0.5)));
            phaser->setFeedback(static_cast<float>(phaserXml->getDoubleAttribute("feedback", 0.3)));
            phaser->setMix(static_cast<float>(phaserXml->getDoubleAttribute("mix", 0.5)));
            phaser->setStages(phaserXml->getIntAttribute("stages", 0));
        }
    }

    if (auto* vibratoXml = xml->getChildByName("Vibrato"))
    {
        if (auto* vibrato = dynamic_cast<Vibrato*>(effectChain.getEffectByName("Vibrato")))
        {
            vibrato->setBypassed(vibratoXml->getBoolAttribute("bypassed", true));
            vibrato->setRate(static_cast<float>(vibratoXml->getDoubleAttribute("rate", 0.5)));
            vibrato->setDepth(static_cast<float>(vibratoXml->getDoubleAttribute("depth", 0.3)));
            vibrato->setTone(static_cast<float>(vibratoXml->getDoubleAttribute("tone", 0.7)));
        }
    }

    if (auto* eqXml = xml->getChildByName("Equalizer"))
    {
        if (auto* eq = dynamic_cast<Equalizer*>(effectChain.getEffectByName("EQ")))
        {
            eq->setBypassed(eqXml->getBoolAttribute("bypassed", true));
            eq->setBass(static_cast<float>(eqXml->getDoubleAttribute("bass", 0.0)));
            eq->setMidGain(static_cast<float>(eqXml->getDoubleAttribute("midGain", 0.0)));
            eq->setMidFreq(static_cast<float>(eqXml->getDoubleAttribute("midFreq", 0.5)));
            eq->setTreble(static_cast<float>(eqXml->getDoubleAttribute("treble", 0.0)));
            eq->setLevel(static_cast<float>(eqXml->getDoubleAttribute("level", 0.0)));
        }
    }
}

void OpenRiffBoxProcessor::setAmpSimEngine(int engine)
{
    auto* ampSim   = effectChain.getEffectByName("Amp Silver");
    auto* ampSim2  = effectChain.getEffectByName("Amp Gold");
    auto* platinum = effectChain.getEffectByName("Amp Platinum");
    if (ampSim == nullptr || ampSim2 == nullptr || platinum == nullptr) return;

    // Transfer the "slot bypass" state from the old active engine to the new one
    bool slotBypassed = false;
    if (ampSimEngine == 0)      slotBypassed = ampSim->isBypassed();
    else if (ampSimEngine == 1) slotBypassed = ampSim2->isBypassed();
    else                        slotBypassed = platinum->isBypassed();

    ampSimEngine = juce::jlimit(0, 2, engine);

    if (ampSimEngine == 0)
    {
        ampSim->setBypassed(slotBypassed);
        ampSim2->setBypassed(true);
        platinum->setBypassed(true);
    }
    else if (ampSimEngine == 1)
    {
        ampSim->setBypassed(true);
        ampSim2->setBypassed(slotBypassed);
        platinum->setBypassed(true);
    }
    else
    {
        ampSim->setBypassed(true);
        ampSim2->setBypassed(true);
        platinum->setBypassed(slotBypassed);
    }
}

void OpenRiffBoxProcessor::setReverbEngine(int engine)
{
    auto* spring = effectChain.getEffectByName("Spring Reverb");
    auto* plate  = effectChain.getEffectByName("Plate Reverb");
    if (spring == nullptr || plate == nullptr) return;

    // Transfer the "slot bypass" state from the old active engine to the new one
    bool slotBypassed = false;
    if (reverbEngine == 0) slotBypassed = spring->isBypassed();
    else                   slotBypassed = plate->isBypassed();

    reverbEngine = juce::jlimit(0, 1, engine);

    if (reverbEngine == 0)
    {
        spring->setBypassed(slotBypassed);
        plate->setBypassed(true);
    }
    else
    {
        spring->setBypassed(true);
        plate->setBypassed(slotBypassed);
    }
}

void OpenRiffBoxProcessor::setModulationEngine(int engine)
{
    auto* chorus  = effectChain.getEffectByName("Chorus");
    auto* flanger = effectChain.getEffectByName("Flanger");
    auto* phaser  = effectChain.getEffectByName("Phaser");
    auto* vibrato = effectChain.getEffectByName("Vibrato");
    if (chorus == nullptr || flanger == nullptr || phaser == nullptr || vibrato == nullptr) return;

    // Transfer the "slot bypass" state from the old active engine to the new one
    bool slotBypassed = false;
    if (modulationEngine == 0)      slotBypassed = chorus->isBypassed();
    else if (modulationEngine == 1) slotBypassed = flanger->isBypassed();
    else if (modulationEngine == 2) slotBypassed = phaser->isBypassed();
    else                            slotBypassed = vibrato->isBypassed();

    modulationEngine = juce::jlimit(0, 3, engine);

    // Force-bypass all inactive engines, enable the active one
    chorus->setBypassed(modulationEngine == 0 ? slotBypassed : true);
    flanger->setBypassed(modulationEngine == 1 ? slotBypassed : true);
    phaser->setBypassed(modulationEngine == 2 ? slotBypassed : true);
    vibrato->setBypassed(modulationEngine == 3 ? slotBypassed : true);
}

void OpenRiffBoxProcessor::setTunerActive(bool active)
{
    tunerActive.store(active, std::memory_order_release);
    tunerEngine.setActive(active);
    if (!active)
        tunerEngine.reset();
}

//==============================================================================
// JUCE standalone wrapper entry point — must be defined exactly once.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenRiffBoxProcessor();
}
