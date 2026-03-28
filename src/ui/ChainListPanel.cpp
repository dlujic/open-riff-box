#include "ChainListPanel.h"
#include "Theme.h"
#include "dsp/EffectChain.h"
#include "dsp/EffectProcessor.h"

#include <algorithm>

//==============================================================================
// Icon button LookAndFeel: paints vector icons based on button name
//==============================================================================
void ChainListPanel::IconButtonLF::drawButtonBackground(
    juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool highlighted, bool /*down*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    bool isOn = button.getToggleState();

    // Subtle background
    auto bg = highlighted ? juce::Colour(0xff3a3428) : juce::Colour(0xff2a2720);
    if (isOn) bg = bg.brighter(0.12f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 3.0f);

    auto iconColour = isOn ? juce::Colour(0xffe0d8c0) : juce::Colour(0xff807868);
    if (highlighted) iconColour = iconColour.brighter(0.15f);

    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();
    float h = bounds.getHeight();

    if (button.getName() == "reorderToggle")
    {
        // Three horizontal lines with up/down arrows (sortable list icon)
        g.setColour(iconColour);

        float lineW = 10.0f;
        float lineX = cx - lineW * 0.5f;
        float lineGap = 3.5f;
        float arrowSize = 2.5f;

        // Three horizontal lines (centred)
        for (int i = -1; i <= 1; ++i)
        {
            float ly = cy + static_cast<float>(i) * lineGap;
            g.drawLine(lineX, ly, lineX + lineW, ly, 1.5f);
        }

        // Up arrow above the lines
        float arrowY = cy - lineGap - 4.5f;
        juce::Path up;
        up.addTriangle(cx, arrowY - arrowSize,
                        cx - arrowSize, arrowY + arrowSize * 0.5f,
                        cx + arrowSize, arrowY + arrowSize * 0.5f);
        g.fillPath(up);

        // Down arrow below the lines
        float arrowY2 = cy + lineGap + 4.5f;
        juce::Path down;
        down.addTriangle(cx, arrowY2 + arrowSize,
                          cx - arrowSize, arrowY2 - arrowSize * 0.5f,
                          cx + arrowSize, arrowY2 - arrowSize * 0.5f);
        g.fillPath(down);

        // Active state: bright left accent bar
        if (isOn)
        {
            g.setColour(juce::Colour(0xffc0b8a0).withAlpha(0.7f));
            g.fillRoundedRectangle(bounds.getX() + 1.0f, bounds.getY() + 3.0f,
                                    2.0f, bounds.getHeight() - 6.0f, 1.0f);
        }
    }
    else if (button.getName() == "resetOrder")
    {
        // Circular reset arrow
        float radius = h * 0.28f;
        juce::Path arc;
        // Draw ~270 degree arc
        arc.addCentredArc(cx, cy, radius, radius,
                           0.0f,
                           -juce::MathConstants<float>::pi * 0.8f,
                           juce::MathConstants<float>::pi * 0.7f,
                           true);
        g.setColour(iconColour);
        g.strokePath(arc, juce::PathStrokeType(1.5f));

        // Arrowhead at the end of the arc
        float endAngle = juce::MathConstants<float>::pi * 0.7f;
        float ex = cx + radius * std::cos(endAngle);
        float ey = cy + radius * std::sin(endAngle);
        float arrowSz = h * 0.18f;

        juce::Path arrow;
        arrow.addTriangle(ex + arrowSz, ey - arrowSz * 0.3f,
                           ex - arrowSz * 0.3f, ey - arrowSz,
                           ex + arrowSz * 0.2f, ey + arrowSz * 0.6f);
        g.fillPath(arrow);
    }
}

juce::Colour ChainListPanel::getColourForEffect(const juce::String& name)
{
    if (name == "Noise Gate")         return Theme::Colours::gate;
    if (name == "Diode Drive")        return Theme::Colours::drive;
    if (name == "Distortion")         return Theme::Colours::distortion;
    if (name == "Amp Silver")         return Theme::Colours::ampSim;
    if (name == "Amp Gold")           return Theme::Colours::ampSim;
    if (name == "Amp Platinum")   return Theme::Colours::ampSim;
    if (name == "Delay")              return Theme::Colours::timeBased;
    if (name == "Spring Reverb")      return Theme::Colours::timeBased;
    if (name == "Plate Reverb")       return Theme::Colours::timeBased;
    if (name == "Chorus")             return Theme::Colours::modulation;
    if (name == "Flanger")            return Theme::Colours::modulation;
    if (name == "Phaser")             return Theme::Colours::modulation;
    if (name == "Vibrato")            return Theme::Colours::modulation;
    if (name == "EQ")                 return Theme::Colours::neutral;
    return Theme::Colours::neutral;
}

ChainListPanel::ChainListPanel(EffectChain& chain)
    : effectChain(chain)
{
    // Build initial entries (visual rows, skipping "Amp Gold" and "Amp Platinum")
    rebuildFromChain();

    for (int i = 0; i < kNumVisualSlots; ++i)
    {
        if (i < numVisualSlots)
        {
            // Display "Amp Sim" for the merged amp row, "Reverb" for the merged reverb row,
            // "Modulation" for the merged chorus/flanger row
            auto displayName = entries[static_cast<size_t>(i)].name;
            if (displayName == "Amp Silver")    displayName = "Amp Sim";
            if (displayName == "Spring Reverb") displayName = "Reverb";
            if (displayName == "Chorus")        displayName = "Modulation";
            bypassButtons[i].setButtonText(displayName);

            int chainIdx = visualToChain[i];
            auto* effect = effectChain.getEffect(chainIdx);

            // For the Amp Sim row, show the active amp engine's bypass state
            if (entries[static_cast<size_t>(i)].name == "Amp Silver")
            {
                juce::String activeName = (activeAmpEngine == 0) ? "Amp Silver"
                                        : (activeAmpEngine == 1) ? "Amp Gold"
                                                                  : "Amp Platinum";
                auto* activeEffect = effectChain.getEffectByName(activeName);
                if (activeEffect != nullptr)
                    bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
            }
            // For the Spring Reverb row, show the active reverb engine's bypass state
            else if (entries[static_cast<size_t>(i)].name == "Spring Reverb")
            {
                juce::String activeName = (activeReverbEngine == 0) ? "Spring Reverb" : "Plate Reverb";
                auto* activeEffect = effectChain.getEffectByName(activeName);
                if (activeEffect != nullptr)
                    bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
            }
            // For the Chorus row, show the active modulation engine's bypass state
            else if (entries[static_cast<size_t>(i)].name == "Chorus")
            {
                juce::String activeName = (activeModulationEngine == 0) ? "Chorus"
                                        : (activeModulationEngine == 1) ? "Flanger" : (activeModulationEngine == 2) ? "Phaser" : "Vibrato";
                auto* activeEffect = effectChain.getEffectByName(activeName);
                if (activeEffect != nullptr)
                    bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
            }
            else if (effect != nullptr)
            {
                bypassButtons[i].setToggleState(!effect->isBypassed(), juce::dontSendNotification);
            }

            bypassButtons[i].setEnabled(entries[static_cast<size_t>(i)].hasDsp);
        }

        bypassButtons[i].setLookAndFeel(&bypassLF);

        if (i < numVisualSlots)
        {
            bypassButtons[i].getProperties().set(
                "ledColour",
                static_cast<juce::int64>(entries[static_cast<size_t>(i)].colour.getARGB()));
        }

        bypassButtons[i].setClickingTogglesState(false);
        bypassButtons[i].onClick = [this, i] {
            if (reorderMode)
            {
                // In reorder mode: click selects only, never toggles bypass
                selectEffect(i);
                return;
            }

            if (i != selectedIndex || settingsActive || presetsActive || tunerActive || metronomeActive)
            {
                // First click (or returning from overlay): select only
                selectEffect(i);
            }
            else
            {
                // Already selected: toggle bypass
                // For "Amp Silver" row, toggle the active amp engine
                if (i < numVisualSlots && entries[static_cast<size_t>(i)].name == "Amp Silver")
                {
                    juce::String activeName = (activeAmpEngine == 0) ? "Amp Silver"
                                            : (activeAmpEngine == 1) ? "Amp Gold"
                                                                      : "Amp Platinum";
                    auto* activeEffect = effectChain.getEffectByName(activeName);
                    if (activeEffect != nullptr)
                    {
                        bool newState = !bypassButtons[i].getToggleState();
                        bypassButtons[i].setToggleState(newState, juce::dontSendNotification);
                        activeEffect->setBypassed(!newState);
                        onBypassChanged();
                    }
                }
                // For "Spring Reverb" row, toggle the active reverb engine
                else if (i < numVisualSlots && entries[static_cast<size_t>(i)].name == "Spring Reverb")
                {
                    juce::String activeName = (activeReverbEngine == 0) ? "Spring Reverb" : "Plate Reverb";
                    auto* activeEffect = effectChain.getEffectByName(activeName);
                    if (activeEffect != nullptr)
                    {
                        bool newState = !bypassButtons[i].getToggleState();
                        bypassButtons[i].setToggleState(newState, juce::dontSendNotification);
                        activeEffect->setBypassed(!newState);
                        onBypassChanged();
                    }
                }
                // For "Chorus" row, toggle the active modulation engine
                else if (i < numVisualSlots && entries[static_cast<size_t>(i)].name == "Chorus")
                {
                    juce::String activeName = (activeModulationEngine == 0) ? "Chorus"
                                        : (activeModulationEngine == 1) ? "Flanger" : (activeModulationEngine == 2) ? "Phaser" : "Vibrato";
                    auto* activeEffect = effectChain.getEffectByName(activeName);
                    if (activeEffect != nullptr)
                    {
                        bool newState = !bypassButtons[i].getToggleState();
                        bypassButtons[i].setToggleState(newState, juce::dontSendNotification);
                        activeEffect->setBypassed(!newState);
                        onBypassChanged();
                    }
                }
                else
                {
                    int chainIdx = visualToChain[i];
                    auto* effect = effectChain.getEffect(chainIdx);
                    if (effect != nullptr)
                    {
                        bool newState = !bypassButtons[i].getToggleState();
                        bypassButtons[i].setToggleState(newState, juce::dontSendNotification);
                        effect->setBypassed(!newState);
                        onBypassChanged();
                    }
                }
            }
        };

        addAndMakeVisible(bypassButtons[i]);

        // Move up/down buttons for reorder mode
        moveUpButtons[i].setButtonText(juce::CharPointer_UTF8("\xe2\x96\xb2"));  // BLACK UP-POINTING TRIANGLE
        moveUpButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        moveUpButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc0b8a0));
        moveUpButtons[i].onClick = [this, i] {
            if (i > 0 && i < numVisualSlots)
            {
                // Swap visual row i with visual row i-1 using group-aware rotate.
                // visualToChain[] gives group boundaries -- each visual row owns
                // chain indices [visualToChain[row], visualToChain[row+1]).
                auto order = effectChain.getEffectOrder();
                int groupAStart = visualToChain[i - 1];
                int midpoint    = visualToChain[i];
                int groupBEnd   = (i + 1 < numVisualSlots) ? visualToChain[i + 1]
                                                           : effectChain.getNumEffects();

                std::rotate(order.begin() + groupAStart,
                            order.begin() + midpoint,
                            order.begin() + groupBEnd);
                effectChain.setEffectOrder(order);

                // Track selection
                if (selectedIndex == i) selectedIndex = i - 1;
                else if (selectedIndex == i - 1) selectedIndex = i;
                rebuildFromChain();
                listeners.call(&Listener::effectOrderChanged);
            }
        };
        addChildComponent(moveUpButtons[i]);

        moveDownButtons[i].setButtonText(juce::CharPointer_UTF8("\xe2\x96\xbc"));  // BLACK DOWN-POINTING TRIANGLE
        moveDownButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        moveDownButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc0b8a0));
        moveDownButtons[i].onClick = [this, i] {
            if (i >= 0 && i < numVisualSlots - 1)
            {
                // Swap visual row i with visual row i+1 using group-aware rotate.
                auto order = effectChain.getEffectOrder();
                int groupAStart = visualToChain[i];
                int midpoint    = visualToChain[i + 1];
                int groupBEnd   = (i + 2 < numVisualSlots) ? visualToChain[i + 2]
                                                           : effectChain.getNumEffects();

                std::rotate(order.begin() + groupAStart,
                            order.begin() + midpoint,
                            order.begin() + groupBEnd);
                effectChain.setEffectOrder(order);

                // Track selection
                if (selectedIndex == i) selectedIndex = i + 1;
                else if (selectedIndex == i + 1) selectedIndex = i;
                rebuildFromChain();
                listeners.call(&Listener::effectOrderChanged);
            }
        };
        addChildComponent(moveDownButtons[i]);
    }

    // Nothing selected on startup
    selectedIndex = -1;

    // Reorder toggle button (icon-based, lives in the separator area)
    reorderToggle.setName("reorderToggle");
    reorderToggle.setLookAndFeel(&iconLF);
    reorderToggle.setClickingTogglesState(true);
    reorderToggle.onClick = [this] {
        reorderMode = reorderToggle.getToggleState();
        updateReorderButtons();
        repaint();
    };
    addAndMakeVisible(reorderToggle);

    // Reset Order button (icon-based, visible in reorder mode when order is non-default)
    resetOrderButton.setName("resetOrder");
    resetOrderButton.setLookAndFeel(&iconLF);
    resetOrderButton.onClick = [this] {
        effectChain.setEffectOrder(EffectChain::getDefaultOrder());
        rebuildFromChain();
        listeners.call(&Listener::effectOrderChanged);
    };
    addChildComponent(resetOrderButton);

    // Tuner button
    tunerButton.setButtonText("Tuner");
    tunerButton.setLookAndFeel(&bypassLF);
    tunerButton.getProperties().set("noLed", true);
    tunerButton.setClickingTogglesState(false);
    tunerButton.onClick = [this] {
        listeners.call(&Listener::tunerClicked);
    };
    addAndMakeVisible(tunerButton);

    // Metronome button
    metronomeButton.setButtonText("Metronome");
    metronomeButton.setLookAndFeel(&bypassLF);
    metronomeButton.getProperties().set("noLed", true);
    metronomeButton.setClickingTogglesState(false);
    metronomeButton.onClick = [this] {
        listeners.call(&Listener::metronomeClicked);
    };
    addAndMakeVisible(metronomeButton);

    // Presets button
    presetsButton.setButtonText("Presets");
    presetsButton.setLookAndFeel(&bypassLF);
    presetsButton.getProperties().set("noLed", true);
    presetsButton.setClickingTogglesState(false);
    presetsButton.onClick = [this] {
        listeners.call(&Listener::presetsClicked);
    };
    addAndMakeVisible(presetsButton);

    // Settings button
    settingsButton.setButtonText("Settings");
    settingsButton.setLookAndFeel(&bypassLF);
    settingsButton.getProperties().set("noLed", true);
    settingsButton.setClickingTogglesState(false);
    settingsButton.onClick = [this] {
        listeners.call(&Listener::settingsClicked);
    };
    addAndMakeVisible(settingsButton);
}

ChainListPanel::~ChainListPanel()
{
    for (int i = 0; i < kNumVisualSlots; ++i)
        bypassButtons[i].setLookAndFeel(nullptr);
    reorderToggle.setLookAndFeel(nullptr);
    resetOrderButton.setLookAndFeel(nullptr);
    tunerButton.setLookAndFeel(nullptr);
    metronomeButton.setLookAndFeel(nullptr);
    presetsButton.setLookAndFeel(nullptr);
    settingsButton.setLookAndFeel(nullptr);
}

void ChainListPanel::addListener(Listener* listener)    { listeners.add(listener); }
void ChainListPanel::removeListener(Listener* listener)  { listeners.remove(listener); }

void ChainListPanel::rebuildFromChain()
{
    // Refresh entries from current chain order, skipping "Amp Gold" and "Amp Platinum"
    entries.clear();
    numVisualSlots = 0;

    for (int i = 0; i < effectChain.getNumEffects() && numVisualSlots < kNumVisualSlots; ++i)
    {
        auto* effect = effectChain.getEffect(i);
        if (effect == nullptr) continue;

        // Skip Amp Gold and Amp Platinum - merged into the single Amp Silver visual row
        // Skip Plate Reverb - merged into the single Spring Reverb visual row
        // Skip Flanger and Phaser - merged into the single Chorus visual row
        if (effect->getName() == "Amp Gold" || effect->getName() == "Amp Platinum"
            || effect->getName() == "Plate Reverb" || effect->getName() == "Flanger"
            || effect->getName() == "Phaser" || effect->getName() == "Vibrato")
            continue;

        entries.push_back({
            effect->getName(),
            getColourForEffect(effect->getName()),
            true
        });
        visualToChain[numVisualSlots] = i;
        ++numVisualSlots;
    }

    // Update button text, LED colours, and bypass state
    for (int i = 0; i < numVisualSlots; ++i)
    {
        // Display "Amp Sim" for the merged amp row, "Reverb" for the merged reverb row,
        // "Modulation" for the merged chorus/flanger row
        auto displayName = entries[static_cast<size_t>(i)].name;
        if (displayName == "Amp Silver")    displayName = "Amp Sim";
        if (displayName == "Spring Reverb") displayName = "Reverb";
        if (displayName == "Chorus")        displayName = "Modulation";
        bypassButtons[i].setButtonText(displayName);
        bypassButtons[i].getProperties().set(
            "ledColour",
            static_cast<juce::int64>(entries[static_cast<size_t>(i)].colour.getARGB()));

        // For Amp Silver row, show active amp engine's bypass state
        if (entries[static_cast<size_t>(i)].name == "Amp Silver")
        {
            juce::String activeName = (activeAmpEngine == 0) ? "Amp Silver"
                                    : (activeAmpEngine == 1) ? "Amp Gold"
                                                              : "Amp Platinum";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        // For Spring Reverb row, show active reverb engine's bypass state
        else if (entries[static_cast<size_t>(i)].name == "Spring Reverb")
        {
            juce::String activeName = (activeReverbEngine == 0) ? "Spring Reverb" : "Plate Reverb";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        // For Chorus row, show active modulation engine's bypass state
        else if (entries[static_cast<size_t>(i)].name == "Chorus")
        {
            juce::String activeName = (activeModulationEngine == 0) ? "Chorus"
                                    : (activeModulationEngine == 1) ? "Flanger" : (activeModulationEngine == 2) ? "Phaser" : "Vibrato";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        else
        {
            int chainIdx = visualToChain[i];
            auto* effect = effectChain.getEffect(chainIdx);
            if (effect != nullptr)
                bypassButtons[i].setToggleState(!effect->isBypassed(), juce::dontSendNotification);
        }

        bypassButtons[i].setVisible(true);
    }

    // Hide unused buttons
    for (int i = numVisualSlots; i < kNumVisualSlots; ++i)
        bypassButtons[i].setVisible(false);

    updateReorderButtons();
    resized();
    repaint();
}

void ChainListPanel::updateReorderButtons()
{
    for (int i = 0; i < kNumVisualSlots; ++i)
    {
        bool showArrows = reorderMode && i < numVisualSlots;
        moveUpButtons[i].setVisible(showArrows && i > 0);
        moveDownButtons[i].setVisible(showArrows && i < numVisualSlots - 1);

        // In reorder mode, hide LEDs by setting noLed property
        if (i < numVisualSlots)
            bypassButtons[i].getProperties().set("noLed", reorderMode);
    }

    // Show reset button only in reorder mode when order is non-default
    resetOrderButton.setVisible(reorderMode && !effectChain.isDefaultOrder());

    resized();
}

void ChainListPanel::setActiveAmpEngine(int engine)
{
    activeAmpEngine = juce::jlimit(0, 2, engine);
    refreshBypassStates();
}

int ChainListPanel::findAmpSimChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Amp Silver")
            return i;
    }
    return -1;
}

int ChainListPanel::findAmpSim2ChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Amp Gold")
            return i;
    }
    return -1;
}

int ChainListPanel::findAmpSimPlatinumChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Amp Platinum")
            return i;
    }
    return -1;
}

int ChainListPanel::findSpringReverbChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Spring Reverb")
            return i;
    }
    return -1;
}

int ChainListPanel::findPlateReverbChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Plate Reverb")
            return i;
    }
    return -1;
}

int ChainListPanel::findFlangerChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Flanger")
            return i;
    }
    return -1;
}

int ChainListPanel::findPhaserChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Phaser")
            return i;
    }
    return -1;
}

int ChainListPanel::findVibratoChainIndex() const
{
    for (int i = 0; i < effectChain.getNumEffects(); ++i)
    {
        auto* e = effectChain.getEffect(i);
        if (e != nullptr && e->getName() == "Vibrato")
            return i;
    }
    return -1;
}

void ChainListPanel::setActiveModulationEngine(int engine)
{
    activeModulationEngine = juce::jlimit(0, 3, engine);
    refreshBypassStates();
}

void ChainListPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Same background as panels
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, bounds);

    // Separator groove at bottom of separator area (between reorder button and utility section)
    const int rowH = Theme::Dims::chainRowHeight;
    const int sepH = Theme::Dims::chainSeparatorH;

    float grooveY1 = static_cast<float>(numVisualSlots * rowH + sepH - 1);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawHorizontalLine(static_cast<int>(grooveY1), 10.0f, static_cast<float>(getWidth() - 10));
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawHorizontalLine(static_cast<int>(grooveY1) + 1, 10.0f, static_cast<float>(getWidth() - 10));

    // Right edge: groove seam between chain list and detail panel
    // Split around active tab to create a "connected" gap
    auto activeRange = getActiveRowYRange();
    int rx = getWidth() - 2;
    float t = 0.0f;
    float b = static_cast<float>(getHeight());

    if (!activeRange.isEmpty())
    {
        float tabTop = static_cast<float>(activeRange.getStart());
        float tabBot = static_cast<float>(activeRange.getEnd());

        if (tabTop > 0.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawVerticalLine(rx, t, tabTop);
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawVerticalLine(rx + 1, t, tabTop);
        }

        if (tabBot < b)
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawVerticalLine(rx, tabBot, b);
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawVerticalLine(rx + 1, tabBot, b);
        }

        const int btnMarginR = 4;
        float gapLeft = static_cast<float>(getWidth() - btnMarginR);
        g.setColour(juce::Colour(0x0cffffff));
        g.fillRect(gapLeft, tabTop, static_cast<float>(btnMarginR), tabBot - tabTop);
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawVerticalLine(rx, t, b);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine(rx + 1, t, b);
    }

    // Standard bevel on other 3 edges
    float rEdge = static_cast<float>(getWidth() - 1);
    auto dark  = juce::Colours::black.withAlpha(0.15f);
    auto light = juce::Colours::white.withAlpha(0.04f);
    g.setColour(dark);
    g.drawHorizontalLine(0, 0.0f, rEdge);
    g.setColour(dark);
    g.drawVerticalLine(0, t, b);
    g.setColour(light);
    g.drawHorizontalLine(static_cast<int>(b) - 1, 0.0f, rEdge);
}

void ChainListPanel::resized()
{
    const int rowH = Theme::Dims::chainRowHeight;
    const int sepH = Theme::Dims::chainSeparatorH;
    const int btnMarginL = 8;
    const int btnMarginR = 4;
    const int btnMarginV = 3;

    const int arrowW = reorderMode ? 20 : 0;

    // Effects section (8 visual slots)
    for (int i = 0; i < kNumVisualSlots; ++i)
    {
        int y = i * rowH + btnMarginV;
        int h = rowH - btnMarginV * 2;
        int btnRight = getWidth() - btnMarginR - arrowW;

        bypassButtons[i].setBounds(btnMarginL, y, btnRight - btnMarginL, h);

        if (reorderMode && i < numVisualSlots)
        {
            int arrowX = btnRight;
            int halfH = h / 2;
            moveUpButtons[i].setBounds(arrowX, y, arrowW, halfH);
            moveDownButtons[i].setBounds(arrowX, y + halfH, arrowW, h - halfH);
        }
    }

    // Icon buttons in the separator area (2px top margin for spacing)
    {
        int reorderY = numVisualSlots * rowH + 2;
        int reorderH = sepH - 4;
        int iconW = reorderH + 10;  // slightly wider than tall for the checkbox+arrows

        reorderToggle.setBounds(btnMarginL, reorderY, iconW, reorderH);

        if (reorderMode)
        {
            int resetW = reorderH;  // square for the circular arrow
            resetOrderButton.setBounds(btnMarginL + iconW + 4, reorderY, resetW, reorderH);
        }
    }

    // All utility/system buttons pinned to the bottom (4 rows)
    int bottomW = getWidth() - btnMarginL - btnMarginR;
    tunerButton.setBounds(btnMarginL, getHeight() - rowH * 4 + btnMarginV, bottomW, rowH - btnMarginV * 2);
    metronomeButton.setBounds(btnMarginL, getHeight() - rowH * 3 + btnMarginV, bottomW, rowH - btnMarginV * 2);
    presetsButton.setBounds(btnMarginL, getHeight() - rowH * 2 + btnMarginV, bottomW, rowH - btnMarginV * 2);
    settingsButton.setBounds(btnMarginL, getHeight() - rowH + btnMarginV, bottomW, rowH - btnMarginV * 2);
}

void ChainListPanel::refreshBypassStates()
{
    for (int i = 0; i < numVisualSlots; ++i)
    {
        if (entries[static_cast<size_t>(i)].name == "Amp Silver")
        {
            juce::String activeName = (activeAmpEngine == 0) ? "Amp Silver"
                                    : (activeAmpEngine == 1) ? "Amp Gold"
                                                              : "Amp Platinum";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        else if (entries[static_cast<size_t>(i)].name == "Spring Reverb")
        {
            juce::String activeName = (activeReverbEngine == 0) ? "Spring Reverb" : "Plate Reverb";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        else if (entries[static_cast<size_t>(i)].name == "Chorus")
        {
            juce::String activeName = (activeModulationEngine == 0) ? "Chorus"
                                    : (activeModulationEngine == 1) ? "Flanger" : (activeModulationEngine == 2) ? "Phaser" : "Vibrato";
            auto* activeEffect = effectChain.getEffectByName(activeName);
            if (activeEffect != nullptr)
                bypassButtons[i].setToggleState(!activeEffect->isBypassed(), juce::dontSendNotification);
        }
        else
        {
            int chainIdx = visualToChain[i];
            auto* effect = effectChain.getEffect(chainIdx);
            if (effect != nullptr)
                bypassButtons[i].setToggleState(!effect->isBypassed(), juce::dontSendNotification);
        }
    }
}

void ChainListPanel::setActiveReverbEngine(int engine)
{
    activeReverbEngine = juce::jlimit(0, 1, engine);
    refreshBypassStates();
}

void ChainListPanel::clearAllOverlays()
{
    presetsActive = false;
    presetsButton.setToggleState(false, juce::dontSendNotification);
    presetsButton.getProperties().set("selected", false);

    settingsActive = false;
    settingsButton.setToggleState(false, juce::dontSendNotification);
    settingsButton.getProperties().set("selected", false);

    tunerActive = false;
    tunerButton.setToggleState(false, juce::dontSendNotification);
    tunerButton.getProperties().set("selected", false);

    metronomeActive = false;
    metronomeButton.setToggleState(false, juce::dontSendNotification);
    metronomeButton.getProperties().set("selected", false);
}

juce::Range<int> ChainListPanel::getActiveRowYRange() const
{
    const int rowH = Theme::Dims::chainRowHeight;

    // Effect selected (and no overlay active)
    if (selectedIndex >= 0 && selectedIndex < numVisualSlots
        && !presetsActive && !settingsActive && !tunerActive && !metronomeActive)
    {
        return { selectedIndex * rowH, (selectedIndex + 1) * rowH };
    }

    if (tunerActive)
        return { getHeight() - rowH * 4, getHeight() - rowH * 3 };

    if (metronomeActive)
        return { getHeight() - rowH * 3, getHeight() - rowH * 2 };

    if (presetsActive)
        return { getHeight() - rowH * 2, getHeight() - rowH };

    if (settingsActive)
        return { getHeight() - rowH, getHeight() };

    return {};
}

void ChainListPanel::setPresetsToggle(bool on)
{
    if (on)
    {
        clearAllOverlays();
        presetsActive = true;
        presetsButton.setToggleState(true, juce::dontSendNotification);
        presetsButton.getProperties().set("selected", true);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
            bypassButtons[selectedIndex].getProperties().set("selected", false);
    }
    else
    {
        presetsActive = false;
        presetsButton.setToggleState(false, juce::dontSendNotification);
        presetsButton.getProperties().set("selected", false);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots && !settingsActive && !tunerActive && !metronomeActive)
            bypassButtons[selectedIndex].getProperties().set("selected", true);
    }

    repaint();
}

void ChainListPanel::setSettingsToggle(bool on)
{
    if (on)
    {
        clearAllOverlays();
        settingsActive = true;
        settingsButton.setToggleState(true, juce::dontSendNotification);
        settingsButton.getProperties().set("selected", true);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
            bypassButtons[selectedIndex].getProperties().set("selected", false);
    }
    else
    {
        settingsActive = false;
        settingsButton.setToggleState(false, juce::dontSendNotification);
        settingsButton.getProperties().set("selected", false);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots && !presetsActive && !tunerActive && !metronomeActive)
            bypassButtons[selectedIndex].getProperties().set("selected", true);
    }

    repaint();
}

void ChainListPanel::setTunerToggle(bool on)
{
    if (on)
    {
        clearAllOverlays();
        tunerActive = true;
        tunerButton.setToggleState(true, juce::dontSendNotification);
        tunerButton.getProperties().set("selected", true);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
            bypassButtons[selectedIndex].getProperties().set("selected", false);
    }
    else
    {
        tunerActive = false;
        tunerButton.setToggleState(false, juce::dontSendNotification);
        tunerButton.getProperties().set("selected", false);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots && !presetsActive && !settingsActive && !metronomeActive)
            bypassButtons[selectedIndex].getProperties().set("selected", true);
    }

    repaint();
}

void ChainListPanel::setMetronomeToggle(bool on)
{
    if (on)
    {
        clearAllOverlays();
        metronomeActive = true;
        metronomeButton.setToggleState(true, juce::dontSendNotification);
        metronomeButton.getProperties().set("selected", true);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
            bypassButtons[selectedIndex].getProperties().set("selected", false);
    }
    else
    {
        metronomeActive = false;
        metronomeButton.setToggleState(false, juce::dontSendNotification);
        metronomeButton.getProperties().set("selected", false);

        if (selectedIndex >= 0 && selectedIndex < numVisualSlots && !presetsActive && !settingsActive && !tunerActive)
            bypassButtons[selectedIndex].getProperties().set("selected", true);
    }

    repaint();
}

void ChainListPanel::selectEffect(int index)
{
    if (index == selectedIndex && !settingsActive && !presetsActive && !tunerActive && !metronomeActive)
        return;

    // Clear old selection
    if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
        bypassButtons[selectedIndex].getProperties().set("selected", false);

    selectedIndex = index;

    // Mark new selection
    if (selectedIndex >= 0 && selectedIndex < numVisualSlots)
        bypassButtons[selectedIndex].getProperties().set("selected", true);

    // Clicking an effect clears all overlays
    clearAllOverlays();

    repaint();

    // Pass the chain index to the listener (MainLayout needs the chain index
    // to resolve which panel to show via chainIndexToPanelIndex)
    if (index >= 0 && index < numVisualSlots)
        listeners.call(&Listener::effectSelected, visualToChain[index]);
}
