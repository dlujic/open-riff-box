#include "AmpSimSwitcherPanel.h"
#include "AmpSimSilverPanel.h"
#include "AmpSimGoldPanel.h"
#include "AmpSimPlatinumPanel.h"
#include "Theme.h"

AmpSimSwitcherPanel::AmpSimSwitcherPanel(AmpSimSilver& ampSimSilver, AmpSimGold& ampSimGold,
                                          AmpSimPlatinum& ampSimPlatinum)
{
    silverPanel   = std::make_unique<AmpSimSilverPanel>(ampSimSilver);
    goldPanel     = std::make_unique<AmpSimGoldPanel>(ampSimGold);
    platinumPanel = std::make_unique<AmpSimPlatinumPanel>(ampSimPlatinum);

    // Wire sub-panel callbacks through
    silverPanel->onBypassToggled      = [this] { onBypassToggled(); };
    silverPanel->onParameterChanged   = [this] { onParameterChanged(); };
    goldPanel->onBypassToggled        = [this] { onBypassToggled(); };
    goldPanel->onParameterChanged     = [this] { onParameterChanged(); };
    platinumPanel->onBypassToggled    = [this] { onBypassToggled(); };
    platinumPanel->onParameterChanged = [this] { onParameterChanged(); };

    addChildComponent(silverPanel.get());
    addChildComponent(goldPanel.get());
    addChildComponent(platinumPanel.get());

    // Engine selector buttons (positioned over the sub-panel's right dead space)
    silverTab.setButtonText("Silver");
    silverTab.setClickingTogglesState(false);
    silverTab.setLookAndFeel(&btnLF);
    silverTab.onClick = [this] { setActiveEngine(0); onEngineChanged(0); };
    addAndMakeVisible(silverTab);

    goldTab.setButtonText("Gold");
    goldTab.setClickingTogglesState(false);
    goldTab.setLookAndFeel(&btnLF);
    goldTab.onClick = [this] { setActiveEngine(1); onEngineChanged(1); };
    addAndMakeVisible(goldTab);

    platinumTab.setButtonText("Platinum");
    platinumTab.setClickingTogglesState(false);
    platinumTab.setLookAndFeel(&btnLF);
    platinumTab.onClick = [this] { setActiveEngine(2); onEngineChanged(2); };
    addAndMakeVisible(platinumTab);

    // Default to Gold
    setActiveEngine(1);
}

AmpSimSwitcherPanel::~AmpSimSwitcherPanel() = default;

void AmpSimSwitcherPanel::setActiveEngine(int engine)
{
    activeEngine = juce::jlimit(0, 2, engine);

    silverPanel->setVisible(activeEngine == 0);
    goldPanel->setVisible(activeEngine == 1);
    platinumPanel->setVisible(activeEngine == 2);

    updateTabAppearance();
    resized();
}

void AmpSimSwitcherPanel::syncFromDsp()
{
    silverPanel->syncFromDsp();
    goldPanel->syncFromDsp();
    platinumPanel->syncFromDsp();
}

void AmpSimSwitcherPanel::paint(juce::Graphics&)
{
}

void AmpSimSwitcherPanel::resized()
{
    auto area = getLocalBounds();

    // Sub-panels get full area (buttons overlay the dead space on the right)
    if (silverPanel   != nullptr) silverPanel->setBounds(area);
    if (goldPanel     != nullptr) goldPanel->setBounds(area);
    if (platinumPanel != nullptr) platinumPanel->setBounds(area);

    // Engine buttons in the right-side dead space, vertically filling the slider zone.
    // Sub-panel layout: pad(16) + topRow(38) + gap(16) = 70px before sliders start.
    const int pad = Theme::Dims::panelPadding;
    const int btnX = area.getRight() - pad - btnWidth;
    const int btnTopY = pad + 38 + 16;  // aligned with slider zone top
    const int totalBtnH = 3 * btnHeight + 2 * btnSpacing;

    // Centre the button stack vertically in the slider zone (190px tall)
    const int sliderZoneH = Theme::Dims::sliderLabelHeight + Theme::Dims::sliderHeight
                          + Theme::Dims::sliderTextBoxH + 4;
    const int btnY = btnTopY + (sliderZoneH - totalBtnH) / 2;

    silverTab.setBounds(btnX, btnY, btnWidth, btnHeight);
    goldTab.setBounds(btnX, btnY + btnHeight + btnSpacing, btnWidth, btnHeight);
    platinumTab.setBounds(btnX, btnY + (btnHeight + btnSpacing) * 2, btnWidth, btnHeight);

    // Bring buttons to front so they paint over the sub-panel
    silverTab.toFront(false);
    goldTab.toFront(false);
    platinumTab.toFront(false);
}

void AmpSimSwitcherPanel::updateTabAppearance()
{
    auto silverColour   = juce::Colour(0xff8898a8);
    auto goldColour     = juce::Colour(0xffc89830);
    auto platinumColour = juce::Colour(0xffb0b8c0);

    auto dimBg = juce::Colour(0xff2a2720);

    struct BtnStyle { juce::TextButton* btn; juce::Colour col; int idx; };
    BtnStyle styles[] = {
        { &silverTab,   silverColour,   0 },
        { &goldTab,     goldColour,     1 },
        { &platinumTab, platinumColour, 2 }
    };

    for (auto& s : styles)
    {
        if (activeEngine == s.idx)
        {
            s.btn->setColour(juce::TextButton::buttonColourId, s.col.withAlpha(0.35f));
            s.btn->setColour(juce::TextButton::textColourOffId, s.col.brighter(0.5f));
        }
        else
        {
            s.btn->setColour(juce::TextButton::buttonColourId, dimBg);
            s.btn->setColour(juce::TextButton::textColourOffId, s.col.withAlpha(0.45f));
        }
    }
}
