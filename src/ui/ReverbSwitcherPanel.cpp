#include "ReverbSwitcherPanel.h"
#include "ReverbPanel.h"
#include "PlateReverbPanel.h"
#include "Theme.h"

ReverbSwitcherPanel::ReverbSwitcherPanel(SpringReverb& springReverb, PlateReverb& plateReverb)
{
    springPanel = std::make_unique<ReverbPanel>(springReverb);
    platePanel  = std::make_unique<PlateReverbPanel>(plateReverb);

    // Wire sub-panel callbacks through
    springPanel->onBypassToggled    = [this] { onBypassToggled(); };
    springPanel->onParameterChanged = [this] { onParameterChanged(); };
    platePanel->onBypassToggled     = [this] { onBypassToggled(); };
    platePanel->onParameterChanged  = [this] { onParameterChanged(); };

    addChildComponent(springPanel.get());
    addChildComponent(platePanel.get());

    // Engine selector buttons (positioned over the sub-panel's right dead space)
    springTab.setButtonText("Spring");
    springTab.setClickingTogglesState(false);
    springTab.setLookAndFeel(&btnLF);
    springTab.onClick = [this] { setActiveEngine(0); onEngineChanged(0); };
    addAndMakeVisible(springTab);

    plateTab.setButtonText("Plate");
    plateTab.setClickingTogglesState(false);
    plateTab.setLookAndFeel(&btnLF);
    plateTab.onClick = [this] { setActiveEngine(1); onEngineChanged(1); };
    addAndMakeVisible(plateTab);

    // Default to Spring
    setActiveEngine(0);
}

ReverbSwitcherPanel::~ReverbSwitcherPanel() = default;

void ReverbSwitcherPanel::setActiveEngine(int engine)
{
    activeEngine = juce::jlimit(0, 1, engine);

    springPanel->setVisible(activeEngine == 0);
    platePanel->setVisible(activeEngine == 1);

    updateTabAppearance();
    resized();
}

void ReverbSwitcherPanel::syncFromDsp()
{
    springPanel->syncFromDsp();
    platePanel->syncFromDsp();
}

void ReverbSwitcherPanel::paint(juce::Graphics&)
{
}

void ReverbSwitcherPanel::resized()
{
    auto area = getLocalBounds();

    // Sub-panels get full area (buttons overlay the dead space on the right)
    if (springPanel != nullptr) springPanel->setBounds(area);
    if (platePanel  != nullptr) platePanel->setBounds(area);

    // Engine buttons in the right-side dead space, vertically centred in slider zone.
    const int pad = Theme::Dims::panelPadding;
    const int btnX = area.getRight() - pad - btnWidth;
    const int btnTopY = pad + 38 + 16;

    const int sliderZoneH = Theme::Dims::sliderLabelHeight + Theme::Dims::sliderHeight
                          + Theme::Dims::sliderTextBoxH + 4;
    const int totalBtnH = 2 * btnHeight + btnSpacing;
    const int btnY = btnTopY + (sliderZoneH - totalBtnH) / 2;

    springTab.setBounds(btnX, btnY, btnWidth, btnHeight);
    plateTab.setBounds(btnX, btnY + btnHeight + btnSpacing, btnWidth, btnHeight);

    springTab.toFront(false);
    plateTab.toFront(false);
}

void ReverbSwitcherPanel::updateTabAppearance()
{
    auto springColour = juce::Colour(0xff5090c0);
    auto plateColour  = juce::Colour(0xff9898b8);

    auto dimBg = juce::Colour(0xff2a2720);

    if (activeEngine == 0)
    {
        springTab.setColour(juce::TextButton::buttonColourId, springColour.withAlpha(0.35f));
        springTab.setColour(juce::TextButton::textColourOffId, springColour.brighter(0.5f));
    }
    else
    {
        springTab.setColour(juce::TextButton::buttonColourId, dimBg);
        springTab.setColour(juce::TextButton::textColourOffId, springColour.withAlpha(0.45f));
    }

    if (activeEngine == 1)
    {
        plateTab.setColour(juce::TextButton::buttonColourId, plateColour.withAlpha(0.35f));
        plateTab.setColour(juce::TextButton::textColourOffId, plateColour.brighter(0.5f));
    }
    else
    {
        plateTab.setColour(juce::TextButton::buttonColourId, dimBg);
        plateTab.setColour(juce::TextButton::textColourOffId, plateColour.withAlpha(0.45f));
    }
}
