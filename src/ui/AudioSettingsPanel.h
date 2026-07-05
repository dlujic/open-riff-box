#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "ExclusiveAudioMode.h"
#include "Theme.h"
#include "SettingsLookAndFeel.h"

class AudioSettingsPanel : public juce::Component
{
public:
    AudioSettingsPanel()
    {
        setLookAndFeel(&settingsLF);

        if (auto* standalone = juce::StandalonePluginHolder::getInstance())
        {
            auto* props = standalone->settings.get();

            // Gate before the selector is built - it snapshots the type list.
            ExclusiveAudioMode::applyTypeGate(standalone->deviceManager,
                                              ExclusiveAudioMode::isEnabled(props));
            rebuildDeviceSelector();

            exclusiveToggle.setButtonText("Exclusive audio (ASIO, lowest latency)");
            exclusiveToggle.setTooltip("Allows ASIO and exclusive-mode drivers for the lowest "
                                       "latency. May take over the audio device and interrupt "
                                       "other applications' sound.");
            exclusiveToggle.setToggleState(ExclusiveAudioMode::isEnabled(props),
                                           juce::dontSendNotification);
            exclusiveToggle.onClick = [this] { applyExclusiveToggle(); };
            addAndMakeVisible(exclusiveToggle);
        }

        tooltipToggle.setButtonText("Show parameter tooltips on hover");

        // Load persisted state
        bool savedState = false;
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            if (auto* props = holder->settings.get())
                savedState = props->getBoolValue("tooltipsEnabled", false);
        tooltipToggle.setToggleState(savedState, juce::dontSendNotification);

        tooltipToggle.onClick = [this] {
            bool enabled = tooltipToggle.getToggleState();

            // Persist the setting
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                if (auto* props = holder->settings.get())
                    props->setValue("tooltipsEnabled", enabled);

            if (onTooltipsChanged)
                onTooltipsChanged(enabled);
        };
        addAndMakeVisible(tooltipToggle);
    }

    ~AudioSettingsPanel() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();

        g.fillAll(Theme::Colours::panelBackground);
        Theme::paintNoise(g, area);

        // Title heading
        auto titleArea = area.removeFromTop(44);
        g.setColour(Theme::Colours::textPrimary);
        g.setFont(Theme::Fonts::heading());
        g.drawText("Audio Settings", titleArea, juce::Justification::centred, true);

        Theme::paintBevel(g, getLocalBounds());
        Theme::paintScrews(g, getLocalBounds());
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const int topInset = 48;
        const int sideInset = 24;
        const int bottomInset = 24;

        auto content = area.reduced(sideInset, 0)
                           .withTrimmedTop(topInset)
                           .withTrimmedBottom(bottomInset);

        // Toggles at the bottom
        tooltipToggle.setBounds(content.removeFromBottom(28));
        content.removeFromBottom(4);

        if (exclusiveToggle.getParentComponent() != nullptr)
        {
            exclusiveToggle.setBounds(content.removeFromBottom(28));
            content.removeFromBottom(8);
        }
        else
        {
            content.removeFromBottom(4);
        }

        if (deviceSelector != nullptr)
            deviceSelector->setBounds(content);
    }

    std::function<void(bool)> onTooltipsChanged;

    bool isTooltipsEnabled() const { return tooltipToggle.getToggleState(); }

private:
    void rebuildDeviceSelector()
    {
        auto* standalone = juce::StandalonePluginHolder::getInstance();
        if (standalone == nullptr)
            return;

        deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
            standalone->deviceManager,
            0, 2,   // min/max input channels
            2, 2,   // min/max output channels
            false, false,  // show MIDI input/output
            false, false);

        addAndMakeVisible(*deviceSelector);
        resized();
    }

    void applyExclusiveToggle()
    {
        auto* standalone = juce::StandalonePluginHolder::getInstance();
        if (standalone == nullptr)
            return;

        auto* props = standalone->settings.get();
        if (props == nullptr)
        {
            exclusiveToggle.setToggleState(! exclusiveToggle.getToggleState(),
                                           juce::dontSendNotification);
            return;
        }

        const bool enabled = exclusiveToggle.getToggleState();
        props->setValue(ExclusiveAudioMode::settingKey, enabled);

        if (enabled)
            ExclusiveAudioMode::enterExclusiveMode(standalone->deviceManager, *props);
        else
            ExclusiveAudioMode::exitExclusiveMode(standalone->deviceManager, *props);

        rebuildDeviceSelector();
    }

    SettingsLookAndFeel settingsLF;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::ToggleButton exclusiveToggle;
    juce::ToggleButton tooltipToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};
