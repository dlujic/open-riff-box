#include "PlateReverbPanel.h"
#include "Theme.h"
#include "dsp/PlateReverb.h"

PlateReverbPanel::PlateReverbPanel(PlateReverb& reverb)
    : reverbRef(reverb)
{
    setupKnob(decayKnob,    decayLabel,    "Decay",     0.0, 1.0, 0.01);
    setupKnob(dampingKnob,  dampingLabel,  "Damping",   0.0, 1.0, 0.01);
    setupKnob(mixKnob,      mixLabel,      "Mix",       0.0, 1.0, 0.01);
    setupKnob(preDelayKnob, preDelayLabel, "Pre-Delay", 0.0, 1.0, 0.01);
    setupKnob(widthKnob,    widthLabel,    "Width",     0.0, 1.0, 0.01);

    decayKnob.onValueChange = [this] {
        reverbRef.setDecay(static_cast<float>(decayKnob.getValue()));
        onParameterChanged();
    };
    dampingKnob.onValueChange = [this] {
        reverbRef.setDamping(static_cast<float>(dampingKnob.getValue()));
        onParameterChanged();
    };
    mixKnob.onValueChange = [this] {
        reverbRef.setMix(static_cast<float>(mixKnob.getValue()));
        onParameterChanged();
    };
    preDelayKnob.onValueChange = [this] {
        reverbRef.setPreDelay(static_cast<float>(preDelayKnob.getValue()));
        onParameterChanged();
    };
    widthKnob.onValueChange = [this] {
        reverbRef.setWidth(static_cast<float>(widthKnob.getValue()));
        onParameterChanged();
    };

    // Plate Type selector ComboBox
    plateTypeSelector.addItem("Studio", 1);
    plateTypeSelector.addItem("Bright", 2);
    plateTypeSelector.addItem("Dark",   3);

    plateTypeSelector.onChange = [this] {
        auto id = plateTypeSelector.getSelectedId();
        if (id > 0)
        {
            reverbRef.setPlateType(id - 1);
            onParameterChanged();
        }
    };
    plateTypeSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(plateTypeSelector);

    plateTypeLabel.setText("Plate:", juce::dontSendNotification);
    plateTypeLabel.setFont(Theme::Fonts::small());
    plateTypeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    plateTypeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(plateTypeLabel);

    // Bypass LED button
    bypassButton.setButtonText("Plate Reverb");
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
    decayKnob.setTooltip("How long the reverb tail lasts.");
    dampingKnob.setTooltip("High-frequency damping. Higher = darker, shorter reflections.");
    mixKnob.setTooltip("Wet/dry balance.");
    preDelayKnob.setTooltip("Time before reverb onset. Adds space and separation.");
    widthKnob.setTooltip("Stereo width of the reverb output.");
    plateTypeSelector.setTooltip("Plate character - Studio (EMT 140), Bright (Ecoplate), Dark (heavily damped).");
    bypassButton.setTooltip("Click to toggle Plate Reverb bypass.");
    resetButton.setTooltip("Reset all Plate Reverb parameters to defaults.");

    syncFromDsp();
}

PlateReverbPanel::~PlateReverbPanel()
{
    decayKnob.setLookAndFeel(nullptr);
    dampingKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
    preDelayKnob.setLookAndFeel(nullptr);
    widthKnob.setLookAndFeel(nullptr);
    plateTypeSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void PlateReverbPanel::syncFromDsp()
{
    decayKnob.setValue(reverbRef.getDecay(),       juce::dontSendNotification);
    dampingKnob.setValue(reverbRef.getDamping(),   juce::dontSendNotification);
    mixKnob.setValue(reverbRef.getMix(),           juce::dontSendNotification);
    preDelayKnob.setValue(reverbRef.getPreDelay(), juce::dontSendNotification);
    widthKnob.setValue(reverbRef.getWidth(),       juce::dontSendNotification);
    plateTypeSelector.setSelectedId(reverbRef.getPlateType() + 1, juce::dontSendNotification);
    bypassButton.setToggleState(!reverbRef.isBypassed(), juce::dontSendNotification);
}

void PlateReverbPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void PlateReverbPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left) | Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Split into left column (main sliders) and right column (pre-delay + width)
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

    // Left column: Decay / Damping / Mix sliders
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &decayKnob, &dampingKnob, &mixKnob };
    juce::Label*  leftLabels[]  = { &decayLabel, &dampingLabel, &mixLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Plate Type selector below left sliders
    leftCol.removeFromTop(12);
    auto typeRow = leftCol.removeFromTop(28);
    plateTypeLabel.setBounds(typeRow.removeFromLeft(50));
    plateTypeSelector.setBounds(typeRow);

    // Right column: Pre-Delay / Width sliders
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &preDelayKnob, &widthKnob };
    juce::Label*  rightLabels[]  = { &preDelayLabel, &widthLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = rightSliderArea.removeFromLeft(cellWidth);
        rightLabels[i]->setBounds(cell.removeFromTop(labelH));
        rightSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                        .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void PlateReverbPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
