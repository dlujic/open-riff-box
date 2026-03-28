#include "NoiseGatePanel.h"
#include "Theme.h"
#include "dsp/NoiseGate.h"

NoiseGatePanel::NoiseGatePanel(NoiseGate& gate)
    : noiseGate(gate)
{
    setupKnob(thresholdKnob, thresholdLabel, "Threshold", -80.0, 0.0, 0.1, " dB");
    setupKnob(attackKnob,    attackLabel,    "Attack",    0.1,   16.0, 0.1, " ms");
    setupKnob(holdKnob,      holdLabel,      "Hold",      0.0,   500.0, 1.0, " ms");
    setupKnob(releaseKnob,   releaseLabel,   "Release",   5.0,   600.0, 1.0, " ms");
    setupKnob(rangeKnob,     rangeLabel,     "Range",     -90.0, 0.0, 0.1, " dB");

    // Wire knob changes to DSP setters (ms → seconds conversion for time params)
    thresholdKnob.onValueChange = [this] {
        noiseGate.setThresholdDb(static_cast<float>(thresholdKnob.getValue()));
        onParameterChanged();
    };
    attackKnob.onValueChange = [this] {
        noiseGate.setAttackSeconds(static_cast<float>(attackKnob.getValue() / 1000.0));
        onParameterChanged();
    };
    holdKnob.onValueChange = [this] {
        noiseGate.setHoldSeconds(static_cast<float>(holdKnob.getValue() / 1000.0));
        onParameterChanged();
    };
    releaseKnob.onValueChange = [this] {
        noiseGate.setReleaseSeconds(static_cast<float>(releaseKnob.getValue() / 1000.0));
        onParameterChanged();
    };
    rangeKnob.onValueChange = [this] {
        noiseGate.setRangeDb(static_cast<float>(rangeKnob.getValue()));
        onParameterChanged();
    };

    // Bypass LED button
    bypassButton.setButtonText("Noise Gate");
    bypassButton.setToggleState(!noiseGate.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::gate.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        noiseGate.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        noiseGate.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    thresholdKnob.setTooltip("Signal level below which the gate closes. Lower = more sensitive.");
    attackKnob.setTooltip("How quickly the gate opens when signal exceeds threshold.");
    holdKnob.setTooltip("How long the gate stays open after signal drops below threshold.");
    releaseKnob.setTooltip("How long the gate takes to fully close after hold expires.");
    rangeKnob.setTooltip("How much the gate attenuates when closed. -90 dB = full silence.");
    bypassButton.setTooltip("Click to toggle Noise Gate bypass.");
    resetButton.setTooltip("Reset all Noise Gate parameters to defaults.");

    syncFromDsp();
}

NoiseGatePanel::~NoiseGatePanel()
{
    thresholdKnob.setLookAndFeel(nullptr);
    attackKnob.setLookAndFeel(nullptr);
    holdKnob.setLookAndFeel(nullptr);
    releaseKnob.setLookAndFeel(nullptr);
    rangeKnob.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void NoiseGatePanel::syncFromDsp()
{
    thresholdKnob.setValue(noiseGate.getThresholdDb(), juce::dontSendNotification);
    attackKnob.setValue(noiseGate.getAttackSeconds() * 1000.0, juce::dontSendNotification);
    holdKnob.setValue(noiseGate.getHoldSeconds() * 1000.0, juce::dontSendNotification);
    releaseKnob.setValue(noiseGate.getReleaseSeconds() * 1000.0, juce::dontSendNotification);
    rangeKnob.setValue(noiseGate.getRangeDb(), juce::dontSendNotification);
    bypassButton.setToggleState(!noiseGate.isBypassed(), juce::dontSendNotification);
}

void NoiseGatePanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void NoiseGatePanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    //--------------------------------------------------------------------------
    // Top row: Bypass LED (left-aligned), Reset (right-aligned)
    //--------------------------------------------------------------------------
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));
    area.removeFromTop(16);

    //--------------------------------------------------------------------------
    // Split into left (Threshold / Attack / Hold) and right (Release / Range)
    //--------------------------------------------------------------------------
    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    const int leftColWidth = cellWidth * 3 - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);
    auto rightCol = area;

    // Left column: Threshold / Attack / Hold
    {
        auto sliderArea = leftCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &thresholdKnob, &attackKnob, &holdKnob };
        juce::Label*  labels[]  = { &thresholdLabel, &attackLabel, &holdLabel };

        for (int i = 0; i < 3; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }

    // Right column: Release / Range
    {
        auto sliderArea = rightCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &releaseKnob, &rangeKnob };
        juce::Label*  labels[]  = { &releaseLabel, &rangeLabel };

        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }
}

void NoiseGatePanel::setupKnob(juce::Slider& knob, juce::Label& label,
                                const juce::String& name, double min, double max,
                                double interval, const juce::String& suffix)
{
    knob.setSliderStyle(juce::Slider::LinearVertical);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    knob.setRange(min, max, interval);
    knob.setTextValueSuffix(suffix);
    knob.setLookAndFeel(&sliderLF);
    addAndMakeVisible(knob);

    label.setText(name, juce::dontSendNotification);
    label.setFont(Theme::Fonts::small());
    label.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}
