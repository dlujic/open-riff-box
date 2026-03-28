#include "ReverbPanel.h"
#include "Theme.h"
#include "dsp/SpringReverb.h"

ReverbPanel::ReverbPanel(SpringReverb& reverb)
    : reverbRef(reverb)
{
    setupKnob(dwellKnob, dwellLabel, "Dwell", 0.0, 1.0, 0.01);
    setupKnob(decayKnob, decayLabel, "Decay", 0.0, 1.0, 0.01);
    setupKnob(mixKnob,   mixLabel,   "Mix",   0.0, 1.0, 0.01);
    setupKnob(dripKnob,  dripLabel,  "Drip",  0.0, 1.0, 0.01);
    setupKnob(toneKnob,  toneLabel,  "Tone",  0.0, 1.0, 0.01);

    dwellKnob.onValueChange = [this] {
        reverbRef.setDwell(static_cast<float>(dwellKnob.getValue()));
        onParameterChanged();
    };
    decayKnob.onValueChange = [this] {
        reverbRef.setDecay(static_cast<float>(decayKnob.getValue()));
        onParameterChanged();
    };
    mixKnob.onValueChange = [this] {
        reverbRef.setMix(static_cast<float>(mixKnob.getValue()));
        onParameterChanged();
    };
    dripKnob.onValueChange = [this] {
        reverbRef.setDrip(static_cast<float>(dripKnob.getValue()));
        onParameterChanged();
    };
    toneKnob.onValueChange = [this] {
        reverbRef.setTone(static_cast<float>(toneKnob.getValue()));
        onParameterChanged();
    };

    // Spring Type selector ComboBox
    springTypeSelector.addItem("Short", 1);
    springTypeSelector.addItem("Medium", 2);
    springTypeSelector.addItem("Long", 3);

    springTypeSelector.onChange = [this] {
        auto id = springTypeSelector.getSelectedId();
        if (id > 0)
        {
            reverbRef.setSpringType(id - 1);
            onParameterChanged();
        }
    };
    springTypeSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(springTypeSelector);

    springTypeLabel.setText("Spring:", juce::dontSendNotification);
    springTypeLabel.setFont(Theme::Fonts::small());
    springTypeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    springTypeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(springTypeLabel);

    // Bypass LED button
    bypassButton.setButtonText("Spring Reverb");
    bypassButton.setToggleState(!reverbRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::timeBased.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        reverbRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        reverbRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    dwellKnob.setTooltip("Input drive into the reverb. Higher = louder, denser reverb.");
    decayKnob.setTooltip("How long the reverb tail lasts.");
    mixKnob.setTooltip("Wet/dry balance.");
    dripKnob.setTooltip("Spring chirp intensity - the metallic \"boing\" character.");
    toneKnob.setTooltip("Damping brightness. Lower = darker reverb, higher = brighter.");
    springTypeSelector.setTooltip("Spring type - Short (surf), Medium (Fender Twin), Long (cavernous).");
    bypassButton.setTooltip("Click to toggle Reverb bypass.");
    resetButton.setTooltip("Reset all Reverb parameters to defaults.");

    syncFromDsp();
}

ReverbPanel::~ReverbPanel()
{
    dwellKnob.setLookAndFeel(nullptr);
    decayKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
    dripKnob.setLookAndFeel(nullptr);
    toneKnob.setLookAndFeel(nullptr);
    springTypeSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void ReverbPanel::syncFromDsp()
{
    dwellKnob.setValue(reverbRef.getDwell(),   juce::dontSendNotification);
    decayKnob.setValue(reverbRef.getDecay(),   juce::dontSendNotification);
    mixKnob.setValue(reverbRef.getMix(),       juce::dontSendNotification);
    dripKnob.setValue(reverbRef.getDrip(),     juce::dontSendNotification);
    toneKnob.setValue(reverbRef.getTone(),     juce::dontSendNotification);
    springTypeSelector.setSelectedId(reverbRef.getSpringType() + 1, juce::dontSendNotification);
    bypassButton.setToggleState(!reverbRef.isBypassed(), juce::dontSendNotification);
}

void ReverbPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void ReverbPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left) | Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Split into left column (main sliders) and right column (drip + tone)
    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    const int leftColWidth = cellWidth * 3 - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);  // gap between columns
    auto rightCol = area;

    // Left column: Dwell / Decay / Mix sliders
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &dwellKnob, &decayKnob, &mixKnob };
    juce::Label*  leftLabels[]  = { &dwellLabel, &decayLabel, &mixLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Spring Type selector below left sliders
    leftCol.removeFromTop(12);
    auto typeRow = leftCol.removeFromTop(28);
    springTypeLabel.setBounds(typeRow.removeFromLeft(50));
    springTypeSelector.setBounds(typeRow);

    // Right column: Drip / Tone sliders
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &dripKnob, &toneKnob };
    juce::Label*  rightLabels[]  = { &dripLabel, &toneLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = rightSliderArea.removeFromLeft(cellWidth);
        rightLabels[i]->setBounds(cell.removeFromTop(labelH));
        rightSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                        .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void ReverbPanel::setupKnob(juce::Slider& knob, juce::Label& label,
                              const juce::String& name, double min, double max,
                              double interval)
{
    knob.setSliderStyle(juce::Slider::LinearVertical);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    knob.setRange(min, max, interval);
    knob.setLookAndFeel(&sliderLF);

    if (min == 0.0 && max == 1.0)
    {
        knob.textFromValueFunction = [](double v) {
            return juce::String(juce::roundToInt(v * 100.0)) + "%";
        };
        knob.valueFromTextFunction = [](const juce::String& text) {
            return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
        };
    }

    addAndMakeVisible(knob);

    label.setText(name, juce::dontSendNotification);
    label.setFont(Theme::Fonts::small());
    label.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}
