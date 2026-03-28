#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

#include "ChainListPanel.h"
#include "EffectDetailPanel.h"
#include "AudioSettingsPanel.h"
#include "PlaceholderPanel.h"
#include "TunerPanel.h"
#include "SidebarPanel.h"
#include "PresetBar.h"
#include "PresetBrowserPanel.h"

class OpenRiffBoxProcessor;
class PresetManager;

class MainLayout : public juce::Component,
                   public ChainListPanel::Listener
{
public:
    explicit MainLayout(OpenRiffBoxProcessor& processor);
    ~MainLayout() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // ChainListPanel::Listener
    void effectSelected(int index) override;
    void effectOrderChanged() override;
    void presetsClicked() override;
    void settingsClicked() override;
    void tunerClicked() override;
    void metronomeClicked() override;

    PresetManager* getPresetManager() { return presetManager.get(); }

private:
    OpenRiffBoxProcessor& processorRef;

    ChainListPanel  chainList;
    SidebarPanel    sidebarPanel;
    std::unique_ptr<EffectDetailPanel> detailPanel;

    std::unique_ptr<AudioSettingsPanel> audioSettingsPanel;
    bool settingsPanelVisible = false;

    std::unique_ptr<PresetManager> presetManager;
    std::unique_ptr<PresetBar> presetBar;
    std::unique_ptr<PresetBrowserPanel> presetBrowserPanel;
    bool presetBrowserVisible = false;

    std::unique_ptr<TunerPanel> tunerPanel;
    bool tunerPanelVisible = false;

    std::unique_ptr<PlaceholderPanel> metronomePanel;
    bool metronomePanelVisible = false;

    void refreshAllUI();

    // Translates a chain-order index to the fixed panel index
    // (panels array in EffectDetailPanel is always in default order)
    static int chainIndexToPanelIndex(const juce::String& effectName);

    struct TooltipLookAndFeel : public juce::LookAndFeel_V4
    {
        TooltipLookAndFeel()
        {
            setDefaultSansSerifTypeface(Theme::Fonts::getRegular());
        }

        void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override
        {
            auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                                  static_cast<float>(width),
                                                  static_cast<float>(height));

            // Dark warm background (sharp corners — avoids Windows native window bleed)
            g.setColour(juce::Colour(0xf0201c16));
            g.fillRect(bounds);

            // Subtle border
            g.setColour(juce::Colour(0xff4a4430));
            g.drawRect(bounds.reduced(0.5f), 1.0f);

            // Text with proper word wrap
            auto font = Theme::Fonts::small();
            juce::AttributedString attrStr;
            attrStr.append(text, font, Theme::Colours::textPrimary);
            attrStr.setWordWrap(juce::AttributedString::byWord);

            juce::TextLayout layout;
            layout.createLayout(attrStr, static_cast<float>(width - 16));
            layout.draw(g, bounds.reduced(8.0f, 5.0f));
        }
    } tooltipLF;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainLayout)
};
