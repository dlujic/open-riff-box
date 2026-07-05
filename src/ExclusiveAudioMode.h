#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

// Policy for the "Exclusive audio" setting (default off).
//
// Off: only shared WASAPI device types are offered, so the app can never take
// the audio device away from other applications. On: ASIO and WASAPI exclusive
// become available for low-latency playing.
//
// ASIO etiquette: JUCE forces whatever sample rate the setup asks for onto the
// driver, and reclocking a multi-client interface (UA Apollo) kills every other
// app's audio. AudioDeviceManager::chooseBestSampleRate treats rate 0 as "use
// the device's current clock", so automatic opens (startup, toggle restore)
// deliberately carry no rate. Explicit rate picks in the selector still work.
namespace ExclusiveAudioMode
{
    inline constexpr const char* settingKey = "exclusiveAudio";
    inline constexpr const char* stashKey   = "audioSetupExclusive";

    inline bool isEnabled(const juce::PropertySet* props)
    {
        return props != nullptr && props->getBoolValue(settingKey, false);
    }

    inline bool isGatedType(const juce::String& typeName)
    {
        return typeName == "ASIO" || typeName == "Windows Audio (Exclusive Mode)";
    }

    // DirectSound is a legacy emulation layer, strictly worse than WASAPI shared.
    inline bool isAllowedType(const juce::String& typeName, bool exclusiveOn)
    {
        if (typeName == "DirectSound")
            return false;

        return exclusiveOn || ! isGatedType(typeName);
    }

    // Must run before StandalonePluginHolder is constructed: the holder opens
    // the saved device inside its own constructor, so anything we don't want
    // opened has to be gone from the settings by then.
    inline void sanitizeSavedSetup(juce::PropertySet& props)
    {
        auto setup = props.getXmlValue("audioSetup");

        if (setup == nullptr)
            return;

        const auto type = setup->getStringAttribute("deviceType");

        if (type == "DirectSound")
        {
            props.removeValue("audioSetup");
            return;
        }

        if (! isEnabled(&props))
        {
            if (isGatedType(type))
            {
                // Park the exclusive config so toggling back on restores it.
                props.setValue(stashKey, setup.get());
                props.removeValue("audioSetup");
            }
        }
        else if (type == "ASIO" && setup->hasAttribute("audioDeviceRate"))
        {
            setup->removeAttribute("audioDeviceRate");
            props.setValue("audioSetup", setup.get());
        }
    }

    inline void applyTypeGate(juce::AudioDeviceManager& dm, bool exclusiveOn)
    {
        juce::Array<juce::AudioIODeviceType*> doomed;

        for (auto* type : dm.getAvailableDeviceTypes())
            if (! isAllowedType(type->getTypeName(), exclusiveOn))
                doomed.add(type);

        for (auto* type : doomed)
            dm.removeAudioDeviceType(type);
    }

    inline void restoreGatedTypes(juce::AudioDeviceManager& dm)
    {
        auto exists = [&dm](const juce::String& name)
        {
            for (auto* t : dm.getAvailableDeviceTypes())
                if (t->getTypeName() == name)
                    return true;

            return false;
        };

        auto addAndScan = [&dm](juce::AudioIODeviceType* raw)
        {
            if (raw != nullptr)
            {
                // The manager only auto-scans types present at first initialise.
                raw->scanForDevices();
                dm.addAudioDeviceType(std::unique_ptr<juce::AudioIODeviceType>(raw));
            }
        };

        if (! exists("ASIO"))
            addAndScan(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());

        if (! exists("Windows Audio (Exclusive Mode)"))
            addAndScan(juce::AudioIODeviceType::createAudioIODeviceType_WASAPI(
                juce::WASAPIDeviceMode::exclusive));
    }

    inline void enterExclusiveMode(juce::AudioDeviceManager& dm, juce::PropertySet& props)
    {
        restoreGatedTypes(dm);

        auto stash = props.getXmlValue(stashKey);

        if (stash == nullptr)
            return;

        const auto type = stash->getStringAttribute("deviceType");

        if (isGatedType(type))
        {
            dm.setCurrentAudioDeviceType(type, true);

            auto setup = dm.getAudioDeviceSetup();
            setup.outputDeviceName = stash->getStringAttribute("audioOutputDeviceName");
            setup.inputDeviceName  = stash->getStringAttribute("audioInputDeviceName");
            setup.bufferSize = stash->getIntAttribute("audioDeviceBufferSize", setup.bufferSize);

            // WASAPI exclusive owns the endpoint outright, so its saved rate is
            // fair game; ASIO follows the device clock (see header comment).
            if (type != "ASIO")
                setup.sampleRate = stash->getDoubleAttribute("audioDeviceRate", setup.sampleRate);

            setup.inputChannels.parseString(stash->getStringAttribute("audioDeviceInChans", "11"), 2);
            setup.outputChannels.parseString(stash->getStringAttribute("audioDeviceOutChans", "11"), 2);
            setup.useDefaultInputChannels  = ! stash->hasAttribute("audioDeviceInChans");
            setup.useDefaultOutputChannels = ! stash->hasAttribute("audioDeviceOutChans");

            // Keep the parked config if the device isn't reachable right now
            // (e.g. interface unplugged) so the next attempt can retry it.
            if (dm.setAudioDeviceSetup(setup, true).isNotEmpty())
                return;
        }

        props.removeValue(stashKey);
    }

    inline void exitExclusiveMode(juce::AudioDeviceManager& dm, juce::PropertySet& props)
    {
        if (isGatedType(dm.getCurrentAudioDeviceType()))
            if (auto xml = dm.createStateXml())
                props.setValue(stashKey, xml.get());

        dm.setCurrentAudioDeviceType("Windows Audio", true);
        applyTypeGate(dm, false);
    }
}
