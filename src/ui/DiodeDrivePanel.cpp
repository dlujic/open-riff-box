#include "DiodeDrivePanel.h"
#include "Theme.h"
#include "dsp/DiodeDrive.h"

DiodeDrivePanel::DiodeDrivePanel(DiodeDrive& diodeDrive)
    : diodeDriveRef(diodeDrive)
{
    setupKnob(driveKnob, driveLabel, "Drive");
    setupKnob(levelKnob, levelLabel, "Level");

    // Tone knob hidden until the tone filter is properly reimplemented.
    // DSP stays at default 0.5 (set in resetToDefaults). Knob/label exist
    // in the class but are not added to the component tree.
    setupKnob(toneKnob, toneLabel, "Tone");
    toneKnob.setVisible(false);
    toneLabel.setVisible(false);
    removeChildComponent(&toneKnob);
    removeChildComponent(&toneLabel);

    // Level: show dB (-60 to +6)
    levelKnob.textFromValueFunction = [](double v) {
        double db = -60.0 + 66.0 * v;
        if (db <= -59.0) return juce::String("-inf");
        return juce::String(db, 1) + " dB";
    };
    levelKnob.valueFromTextFunction = [](const juce::String& text) {
        if (text.containsIgnoreCase("inf")) return 0.0;
        return (text.trimCharactersAtEnd(" dB").getDoubleValue() + 60.0) / 66.0;
    };

    driveKnob.onValueChange = [this] {
        diodeDriveRef.setDrive(static_cast<float>(driveKnob.getValue()));
        onParameterChanged();
    };
    toneKnob.onValueChange = [this] {
        diodeDriveRef.setTone(static_cast<float>(toneKnob.getValue()));
        onParameterChanged();
    };
    levelKnob.onValueChange = [this] {
        diodeDriveRef.setLevel(static_cast<float>(levelKnob.getValue()));
        onParameterChanged();
    };

    // Bypass LED button
    bypassButton.setButtonText("Diode Drive");
    bypassButton.setToggleState(!diodeDriveRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::drive.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        diodeDriveRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        diodeDriveRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    driveKnob.setTooltip("Feedback resistance - sets distortion intensity. Low = clean boost, high = full diode clipping.");
    toneKnob.setTooltip("TS808 tone circuit - crossfade between low-pass (dark) and high-pass (bright) at 723 Hz.");
    levelKnob.setTooltip("Output volume.");
    bypassButton.setTooltip("Click to toggle Diode Drive bypass.");
    resetButton.setTooltip("Reset all Diode Drive parameters to defaults.");

    syncFromDsp();
}

DiodeDrivePanel::~DiodeDrivePanel()
{
    driveKnob.setLookAndFeel(nullptr);
    toneKnob.setLookAndFeel(nullptr);
    levelKnob.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void DiodeDrivePanel::syncFromDsp()
{
    driveKnob.setValue(diodeDriveRef.getDrive(), juce::dontSendNotification);
    toneKnob.setValue(diodeDriveRef.getTone(),   juce::dontSendNotification);
    levelKnob.setValue(diodeDriveRef.getLevel(), juce::dontSendNotification);
    bypassButton.setToggleState(!diodeDriveRef.isBypassed(), juce::dontSendNotification);
}

void DiodeDrivePanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void DiodeDrivePanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    const int numKnobs = 2;  // Drive + Level (Tone hidden for now)
    const int leftColWidth = cellWidth * numKnobs - spacing + 16;
    auto leftCol = area.removeFromLeft(leftColWidth);
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* sliders[] = { &driveKnob, &levelKnob };
    juce::Label*  labels[]  = { &driveLabel, &levelLabel };

    for (int i = 0; i < numKnobs; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        labels[i]->setBounds(cell.removeFromTop(labelH));
        sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                   .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void DiodeDrivePanel::setupKnob(juce::Slider& knob, juce::Label& label,
                                  const juce::String& name)
{
    knob.setSliderStyle(juce::Slider::LinearVertical);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    knob.setRange(0.0, 1.0, 0.01);
    knob.setLookAndFeel(&sliderLF);

    knob.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    knob.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };

    addAndMakeVisible(knob);

    label.setText(name, juce::dontSendNotification);
    label.setFont(Theme::Fonts::small());
    label.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}
