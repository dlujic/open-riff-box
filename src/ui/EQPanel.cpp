#include "EQPanel.h"
#include "Theme.h"
#include "dsp/Equalizer.h"

EQPanel::EQPanel(Equalizer& eq)
    : eqRef(eq)
{
    setupKnob(bassKnob,    bassLabel,    "Bass",   -12.0, 12.0, 0.1);
    setupKnob(midGainKnob, midGainLabel, "Mid",    -12.0, 12.0, 0.1);
    setupKnob(trebleKnob,  trebleLabel,  "Treble", -12.0, 12.0, 0.1);
    setupKnob(levelKnob,   levelLabel,   "Level",  -12.0, 6.0,  0.1);

    // Override text display for dB knobs (setupKnob's percentage display
    // only triggers for 0-1 range, so these need explicit formatting)
    auto dbTextFromValue = [](double v) {
        return juce::String(v, 1) + " dB";
    };
    auto dbValueFromText = [](const juce::String& text) {
        return text.trimCharactersAtEnd(" dB").getDoubleValue();
    };

    bassKnob.textFromValueFunction    = dbTextFromValue;
    bassKnob.valueFromTextFunction    = dbValueFromText;
    midGainKnob.textFromValueFunction = dbTextFromValue;
    midGainKnob.valueFromTextFunction = dbValueFromText;
    trebleKnob.textFromValueFunction  = dbTextFromValue;
    trebleKnob.valueFromTextFunction  = dbValueFromText;
    levelKnob.textFromValueFunction   = dbTextFromValue;
    levelKnob.valueFromTextFunction   = dbValueFromText;

    bassKnob.onValueChange = [this] {
        eqRef.setBass(static_cast<float>(bassKnob.getValue()));
        onParameterChanged();
    };
    midGainKnob.onValueChange = [this] {
        eqRef.setMidGain(static_cast<float>(midGainKnob.getValue()));
        onParameterChanged();
    };
    trebleKnob.onValueChange = [this] {
        eqRef.setTreble(static_cast<float>(trebleKnob.getValue()));
        onParameterChanged();
    };
    levelKnob.onValueChange = [this] {
        eqRef.setLevel(static_cast<float>(levelKnob.getValue()));
        onParameterChanged();
    };

    // Mid Freq horizontal slider
    midFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    midFreqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    midFreqSlider.setRange(0.0, 1.0, 0.01);
    midFreqSlider.setLookAndFeel(&sliderLF);
    midFreqSlider.textFromValueFunction = [](double v) {
        int hz = juce::roundToInt(200.0 * std::pow(25.0, v));
        if (hz >= 1000) return juce::String(hz / 1000.0, 1) + " kHz";
        return juce::String(hz) + " Hz";
    };
    midFreqSlider.valueFromTextFunction = [](const juce::String& text) {
        double val = 0.0;
        if (text.containsIgnoreCase("kHz"))
            val = text.trimCharactersAtEnd(" kHz").getDoubleValue() * 1000.0;
        else
            val = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (val <= 200.0) return 0.0;
        return std::log(val / 200.0) / std::log(25.0);
    };
    midFreqSlider.onValueChange = [this] {
        eqRef.setMidFreq(static_cast<float>(midFreqSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(midFreqSlider);

    midFreqLabel.setText("Mid Freq", juce::dontSendNotification);
    midFreqLabel.setFont(Theme::Fonts::small());
    midFreqLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    midFreqLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(midFreqLabel);

    // Bypass LED button
    bypassButton.setButtonText("EQ");
    bypassButton.setToggleState(!eqRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::neutral.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        eqRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        eqRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    bassKnob.setTooltip("Low shelf at 100 Hz. Boost or cut the low end.");
    midGainKnob.setTooltip("Peaking filter at the frequency set by Mid Freq.");
    trebleKnob.setTooltip("High shelf at 4 kHz. Boost or cut the highs.");
    levelKnob.setTooltip("Output level trim. Compensate for volume changes from EQ.");
    midFreqSlider.setTooltip("Center frequency for the Mid band (200-5000 Hz). Sweep to find what to boost or cut.");
    bypassButton.setTooltip("Click to toggle EQ bypass.");
    resetButton.setTooltip("Reset all EQ parameters to defaults.");

    syncFromDsp();
}

EQPanel::~EQPanel()
{
    bassKnob.setLookAndFeel(nullptr);
    midGainKnob.setLookAndFeel(nullptr);
    trebleKnob.setLookAndFeel(nullptr);
    levelKnob.setLookAndFeel(nullptr);
    midFreqSlider.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void EQPanel::syncFromDsp()
{
    bassKnob.setValue(eqRef.getBass(),        juce::dontSendNotification);
    midGainKnob.setValue(eqRef.getMidGain(),  juce::dontSendNotification);
    trebleKnob.setValue(eqRef.getTreble(),    juce::dontSendNotification);
    levelKnob.setValue(eqRef.getLevel(),      juce::dontSendNotification);
    midFreqSlider.setValue(eqRef.getMidFreq(), juce::dontSendNotification);
    bypassButton.setToggleState(!eqRef.isBypassed(), juce::dontSendNotification);
}

void EQPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void EQPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Split into left column (Bass / Mid / Treble) and right column (Level + Mid Freq)
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

    // Left column: Bass / Mid / Treble sliders
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &bassKnob, &midGainKnob, &trebleKnob };
    juce::Label*  leftLabels[]  = { &bassLabel, &midGainLabel, &trebleLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Mid Freq label + horizontal slider below left sliders
    leftCol.removeFromTop(12);
    midFreqLabel.setBounds(leftCol.removeFromTop(16));
    midFreqSlider.setBounds(leftCol.removeFromTop(28));

    // Right column: Level slider
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    auto levelCell = rightSliderArea.removeFromLeft(cellWidth);
    levelLabel.setBounds(levelCell.removeFromTop(labelH));
    levelKnob.setBounds(levelCell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                  .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
}

void EQPanel::setupKnob(juce::Slider& knob, juce::Label& label,
                           const juce::String& name, double min, double max,
                           double interval)
{
    knob.setSliderStyle(juce::Slider::LinearVertical);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    knob.setRange(min, max, interval);
    knob.setLookAndFeel(&sliderLF);

    // Display 0-1 range as percentage (can be overridden after setupKnob)
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
