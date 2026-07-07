#include "WahPanel.h"
#include "Theme.h"
#include "dsp/Wah.h"

WahPanel::WahPanel(Wah& wah)
    : wahRef(wah)
{
    positionKnob.setSliderStyle(juce::Slider::LinearVertical);
    positionKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    positionKnob.setRange(0.0, 1.0, 0.01);
    positionKnob.setLookAndFeel(&sliderLF);
    positionKnob.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    positionKnob.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    positionKnob.onValueChange = [this] {
        wahRef.setPosition(static_cast<float>(positionKnob.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(positionKnob);

    positionLabel.setText("Position", juce::dontSendNotification);
    positionLabel.setFont(Theme::Fonts::small());
    positionLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    positionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(positionLabel);

    // Coloration toggle -- GCB-95 is dominantly linear at guitar levels, so this
    // defaults off; reuse the bypass-LED look for a consistent on/off tell.
    colorationToggle.setButtonText("Color");
    colorationToggle.setLookAndFeel(&bypassLF);
    colorationToggle.setClickingTogglesState(false);
    colorationToggle.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    colorationToggle.onClick = [this] {
        bool newState = !colorationToggle.getToggleState();
        colorationToggle.setToggleState(newState, juce::dontSendNotification);
        wahRef.setColoration(newState);
        onParameterChanged();
    };
    addAndMakeVisible(colorationToggle);

    // Taper select -- DeadZones (pedal feel, default) or Linear (raw electrical range)
    taperSelector.addItem("Dead Zones", 1);
    taperSelector.addItem("Linear", 2);
    taperSelector.onChange = [this] {
        auto id = taperSelector.getSelectedId();
        if (id > 0)
        {
            wahRef.setTaperMode(id - 1);
            onParameterChanged();
        }
    };
    taperSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(taperSelector);

    taperLabel.setText("Taper", juce::dontSendNotification);
    taperLabel.setFont(Theme::Fonts::small());
    taperLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    taperLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(taperLabel);

    // Bypass LED button
    bypassButton.setButtonText("Wah");
    bypassButton.setToggleState(!wahRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        wahRef.setBypassed(!newState);
        onBypassToggled();
    };
    bypassButton.setTooltip("Click to toggle Wah bypass.");
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        wahRef.resetToDefaults();
        syncFromDsp();
    };
    resetButton.setTooltip("Reset all Wah parameters to defaults.");
    addAndMakeVisible(resetButton);

    // Tooltips
    positionKnob.setTooltip("Sweep position. Toe (0%) is bright and high, heel (100%) is dark and low.");
    colorationToggle.setTooltip("Adds a thin, asymmetric warmth when the resonant peak is pushed hard.");
    taperSelector.setTooltip("Dead Zones: pedal-feel taper matching the mechanical pot travel.\nLinear: raw electrical sweep, full range.");

    syncFromDsp();
}

WahPanel::~WahPanel()
{
    positionKnob.setLookAndFeel(nullptr);
    colorationToggle.setLookAndFeel(nullptr);
    taperSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void WahPanel::syncFromDsp()
{
    positionKnob.setValue(wahRef.getPosition(), juce::dontSendNotification);
    colorationToggle.setToggleState(wahRef.getColoration(), juce::dontSendNotification);
    taperSelector.setSelectedId(wahRef.getTaperMode() + 1, juce::dontSendNotification);
    bypassButton.setToggleState(!wahRef.isBypassed(), juce::dontSendNotification);
}

void WahPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void WahPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Left column: Position slider
    const int sliderW = Theme::Dims::sliderWidth;
    const int sliderH = Theme::Dims::sliderHeight;
    const int labelH  = Theme::Dims::sliderLabelHeight;
    const int spacing = Theme::Dims::sliderSpacing;

    auto leftCol = area.removeFromLeft(sliderW + spacing);
    positionLabel.setBounds(leftCol.removeFromTop(labelH));
    positionKnob.setBounds(leftCol.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                  .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));

    area.removeFromLeft(24);

    // Right column: Color toggle, Taper select
    auto rightCol = area;
    rightCol.removeFromTop(8);
    auto colorRow = rightCol.removeFromTop(30);
    colorationToggle.setBounds(colorRow.removeFromLeft(110));

    rightCol.removeFromTop(16);
    taperLabel.setBounds(rightCol.removeFromTop(16));
    taperSelector.setBounds(rightCol.removeFromTop(28).removeFromLeft(160));
}
