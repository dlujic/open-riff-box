#include "PluginEditor.h"
#include "ui/MainLayout.h"
#include "ui/Theme.h"

OpenRiffBoxEditor::OpenRiffBoxEditor(OpenRiffBoxProcessor& processor)
    : AudioProcessorEditor(&processor),
      processorRef(processor)
{
    mainLayout = std::make_unique<MainLayout>(processorRef);
    addAndMakeVisible(*mainLayout);

    setSize(770, 530);
    setResizable(true, true);
    setResizeLimits(750, 530, 1920, 1080);
}

OpenRiffBoxEditor::~OpenRiffBoxEditor() = default;

void OpenRiffBoxEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::background);
}

void OpenRiffBoxEditor::resized()
{
    mainLayout->setBounds(getLocalBounds());
}
