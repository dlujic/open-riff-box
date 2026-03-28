#include "ChorusPanel.h"
#include "Theme.h"
#include "dsp/Chorus.h"

ChorusPanel::ChorusPanel(Chorus& chorus)
    : chorusRef(chorus)
{
    setupKnob(rateKnob,   rateLabel,   "Rate",    0.0, 1.0, 0.01);
    setupKnob(depthKnob,  depthLabel,  "Depth",   0.0, 1.0, 0.01);
    setupKnob(eqKnob,     eqLabel,     "Tone",    0.0, 1.0, 0.01);

    // EQ/Tone: show cutoff frequency (800 Hz to 12 kHz)
    eqKnob.textFromValueFunction = [](double v) {
        double freq = 800.0 * std::pow(15.0, v);
        if (freq >= 1000.0) return juce::String(freq / 1000.0, 1) + " kHz";
        return juce::String(juce::roundToInt(freq)) + " Hz";
    };
    eqKnob.valueFromTextFunction = [](const juce::String& text) {
        double val = 0.0;
        if (text.containsIgnoreCase("kHz"))
            val = text.trimCharactersAtEnd(" kHz").getDoubleValue() * 1000.0;
        else
            val = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (val <= 800.0) return 0.0;
        return std::log(val / 800.0) / std::log(15.0);
    };
    setupKnob(eLevelKnob, eLevelLabel, "Mix", 0.0, 1.0, 0.01);

    // Rate knob: display in Hz (exponential: 0.3 Hz at 0, 4 Hz at 1)
    rateKnob.textFromValueFunction = [](double v) {
        return juce::String(0.3 + 3.7 * v * v, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.3) return 0.0;
        return std::sqrt((hz - 0.3) / 3.7);
    };

    // Depth knob: display in ms (linear: 0.5 ms at 0, 5.0 ms at 1)
    depthKnob.textFromValueFunction = [](double v) {
        return juce::String(0.5 + 4.5 * v, 1) + " ms";
    };
    depthKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 0.5) return 0.0;
        return (ms - 0.5) / 4.5;
    };

    rateKnob.onValueChange = [this] {
        chorusRef.setRate(static_cast<float>(rateKnob.getValue()));
        onParameterChanged();
    };
    depthKnob.onValueChange = [this] {
        chorusRef.setDepth(static_cast<float>(depthKnob.getValue()));
        onParameterChanged();
    };
    eqKnob.onValueChange = [this] {
        chorusRef.setEQ(static_cast<float>(eqKnob.getValue()));
        onParameterChanged();
    };
    eLevelKnob.onValueChange = [this] {
        chorusRef.setELevel(static_cast<float>(eLevelKnob.getValue()));
        onParameterChanged();
    };

    // Bypass LED button
    bypassButton.setButtonText("Chorus");
    bypassButton.setToggleState(!chorusRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        chorusRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        chorusRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    rateKnob.setTooltip("LFO speed. Slower = gentle swaying, faster = vibrato-like warble.");
    depthKnob.setTooltip("Modulation depth - how far the delay time sweeps. More = stronger detuning.");
    eqKnob.setTooltip("Wet signal brightness filter (800 Hz to 12 kHz). Only affects the chorus, not dry signal.");
    eLevelKnob.setTooltip("Wet/dry mix - effect level blended with dry signal.");
    bypassButton.setTooltip("Click to toggle Chorus bypass.");
    resetButton.setTooltip("Reset all Chorus parameters to defaults.");

    syncFromDsp();
}

ChorusPanel::~ChorusPanel()
{
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    eqKnob.setLookAndFeel(nullptr);
    eLevelKnob.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void ChorusPanel::syncFromDsp()
{
    rateKnob.setValue(chorusRef.getRate(),     juce::dontSendNotification);
    depthKnob.setValue(chorusRef.getDepth(),   juce::dontSendNotification);
    eqKnob.setValue(chorusRef.getEQ(),         juce::dontSendNotification);
    eLevelKnob.setValue(chorusRef.getELevel(), juce::dontSendNotification);
    bypassButton.setToggleState(!chorusRef.isBypassed(), juce::dontSendNotification);
}

void ChorusPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void ChorusPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // 2 columns of 2 sliders each
    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    // Left column: Rate / Depth (standard 3-slider width for consistency)
    const int leftColWidth = cellWidth * 3 - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);  // gap between columns
    auto rightCol = area;

    auto leftSliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &rateKnob, &depthKnob };
    juce::Label*  leftLabels[]  = { &rateLabel, &depthLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = leftSliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Right column: EQ / E.Level
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &eqKnob, &eLevelKnob };
    juce::Label*  rightLabels[]  = { &eqLabel, &eLevelLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = rightSliderArea.removeFromLeft(cellWidth);
        rightLabels[i]->setBounds(cell.removeFromTop(labelH));
        rightSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                        .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void ChorusPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
