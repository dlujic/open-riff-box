#include "ModulationSwitcherPanel.h"
#include "ChorusPanel.h"
#include "FlangerPanel.h"
#include "PhaserPanel.h"
#include "VibratoPanel.h"
#include "Theme.h"

ModulationSwitcherPanel::ModulationSwitcherPanel(Chorus& chorus, Flanger& flanger, Phaser& phaser, Vibrato& vibrato)
{
    chorusPanel  = std::make_unique<ChorusPanel>(chorus);
    flangerPanel = std::make_unique<FlangerPanel>(flanger);
    phaserPanel  = std::make_unique<PhaserPanel>(phaser);
    vibratoPanel = std::make_unique<VibratoPanel>(vibrato);

    // Wire sub-panel callbacks through
    chorusPanel->onBypassToggled     = [this] { onBypassToggled(); };
    chorusPanel->onParameterChanged  = [this] { onParameterChanged(); };
    flangerPanel->onBypassToggled    = [this] { onBypassToggled(); };
    flangerPanel->onParameterChanged = [this] { onParameterChanged(); };
    phaserPanel->onBypassToggled     = [this] { onBypassToggled(); };
    phaserPanel->onParameterChanged  = [this] { onParameterChanged(); };
    vibratoPanel->onBypassToggled    = [this] { onBypassToggled(); };
    vibratoPanel->onParameterChanged = [this] { onParameterChanged(); };

    addChildComponent(chorusPanel.get());
    addChildComponent(flangerPanel.get());
    addChildComponent(phaserPanel.get());
    addChildComponent(vibratoPanel.get());

    // Engine selector buttons (positioned over the sub-panel's right dead space)
    chorusTab.setButtonText("Chorus");
    chorusTab.setClickingTogglesState(false);
    chorusTab.setLookAndFeel(&btnLF);
    chorusTab.onClick = [this] { setActiveEngine(0); onEngineChanged(0); };
    addAndMakeVisible(chorusTab);

    flangerTab.setButtonText("Flanger");
    flangerTab.setClickingTogglesState(false);
    flangerTab.setLookAndFeel(&btnLF);
    flangerTab.onClick = [this] { setActiveEngine(1); onEngineChanged(1); };
    addAndMakeVisible(flangerTab);

    phaserTab.setButtonText("Phaser");
    phaserTab.setClickingTogglesState(false);
    phaserTab.setLookAndFeel(&btnLF);
    phaserTab.onClick = [this] { setActiveEngine(2); onEngineChanged(2); };
    addAndMakeVisible(phaserTab);

    vibratoTab.setButtonText("Vibrato");
    vibratoTab.setClickingTogglesState(false);
    vibratoTab.setLookAndFeel(&btnLF);
    vibratoTab.onClick = [this] { setActiveEngine(3); onEngineChanged(3); };
    addAndMakeVisible(vibratoTab);

    // Default to Chorus
    setActiveEngine(0);
}

ModulationSwitcherPanel::~ModulationSwitcherPanel() = default;

void ModulationSwitcherPanel::setActiveEngine(int engine)
{
    activeEngine = juce::jlimit(0, 3, engine);

    chorusPanel->setVisible(activeEngine == 0);
    flangerPanel->setVisible(activeEngine == 1);
    phaserPanel->setVisible(activeEngine == 2);
    vibratoPanel->setVisible(activeEngine == 3);

    updateTabAppearance();
    resized();
}

void ModulationSwitcherPanel::syncFromDsp()
{
    chorusPanel->syncFromDsp();
    flangerPanel->syncFromDsp();
    phaserPanel->syncFromDsp();
    vibratoPanel->syncFromDsp();
}

void ModulationSwitcherPanel::paint(juce::Graphics&)
{
}

void ModulationSwitcherPanel::resized()
{
    auto area = getLocalBounds();

    // Sub-panels get full area (buttons overlay the dead space on the right)
    if (chorusPanel  != nullptr) chorusPanel->setBounds(area);
    if (flangerPanel != nullptr) flangerPanel->setBounds(area);
    if (phaserPanel  != nullptr) phaserPanel->setBounds(area);
    if (vibratoPanel != nullptr) vibratoPanel->setBounds(area);

    // Engine buttons in the right-side dead space, vertically centred in slider zone.
    const int pad = Theme::Dims::panelPadding;
    const int btnX = area.getRight() - pad - btnWidth;
    const int btnTopY = pad + 38 + 16;

    const int sliderZoneH = Theme::Dims::sliderLabelHeight + Theme::Dims::sliderHeight
                          + Theme::Dims::sliderTextBoxH + 4;
    const int totalBtnH = 4 * btnHeight + 3 * btnSpacing;
    const int btnY = btnTopY + (sliderZoneH - totalBtnH) / 2;

    chorusTab.setBounds(btnX, btnY, btnWidth, btnHeight);
    flangerTab.setBounds(btnX, btnY + (btnHeight + btnSpacing), btnWidth, btnHeight);
    phaserTab.setBounds(btnX, btnY + 2 * (btnHeight + btnSpacing), btnWidth, btnHeight);
    vibratoTab.setBounds(btnX, btnY + 3 * (btnHeight + btnSpacing), btnWidth, btnHeight);

    chorusTab.toFront(false);
    flangerTab.toFront(false);
    phaserTab.toFront(false);
    vibratoTab.toFront(false);
}

void ModulationSwitcherPanel::updateTabAppearance()
{
    auto chorusColour  = juce::Colour(0xff40b0a0);
    auto flangerColour = juce::Colour(0xff60a8d0);
    auto phaserColour  = juce::Colour(0xff9080d0);
    auto vibratoColour = juce::Colour(0xffe0a040);

    auto dimBg = juce::Colour(0xff2a2720);

    auto setTab = [&](juce::TextButton& tab, const juce::Colour& colour, int engineIdx)
    {
        if (activeEngine == engineIdx)
        {
            tab.setColour(juce::TextButton::buttonColourId, colour.withAlpha(0.35f));
            tab.setColour(juce::TextButton::textColourOffId, colour.brighter(0.5f));
        }
        else
        {
            tab.setColour(juce::TextButton::buttonColourId, dimBg);
            tab.setColour(juce::TextButton::textColourOffId, colour.withAlpha(0.45f));
        }
    };

    setTab(chorusTab,  chorusColour,  0);
    setTab(flangerTab, flangerColour, 1);
    setTab(phaserTab,  phaserColour,  2);
    setTab(vibratoTab, vibratoColour, 3);
}
