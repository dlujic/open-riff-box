#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

#include "PluginProcessor.h"

class MainLayout;

class OpenRiffBoxEditor : public juce::AudioProcessorEditor
{
public:
    explicit OpenRiffBoxEditor(OpenRiffBoxProcessor& processor);
    ~OpenRiffBoxEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    OpenRiffBoxProcessor& processorRef;
    std::unique_ptr<MainLayout> mainLayout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenRiffBoxEditor)
};
