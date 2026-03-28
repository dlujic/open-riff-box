#include "VibratoPanel.h"
#include "Theme.h"
#include "dsp/Vibrato.h"

VibratoPanel::VibratoPanel(Vibrato& vibrato)
    : vibratoRef(vibrato)
{
    setupKnob(rateKnob,  rateLabel,  "Rate",  0.0, 1.0, 0.01);
    setupKnob(depthKnob, depthLabel, "Depth", 0.0, 1.0, 0.01);
    setupKnob(toneKnob,  toneLabel,  "Tone",  0.0, 1.0, 0.01);

    // Rate: quadratic mapping 0.5 - 10 Hz
    rateKnob.textFromValueFunction = [](double v) {
        double hz = 0.5 + 9.5 * v * v;
        if (hz < 1.0) return juce::String(hz, 2) + " Hz";
        return juce::String(hz, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.5) return 0.0;
        return std::sqrt((hz - 0.5) / 9.5);
    };

    // Knob callbacks
    rateKnob.onValueChange = [this] {
        vibratoRef.setRate(static_cast<float>(rateKnob.getValue()));
        onParameterChanged();
    };
    depthKnob.onValueChange = [this] {
        vibratoRef.setDepth(static_cast<float>(depthKnob.getValue()));
        onParameterChanged();
    };
    toneKnob.onValueChange = [this] {
        vibratoRef.setTone(static_cast<float>(toneKnob.getValue()));
        onParameterChanged();
    };

    // Bypass LED button
    bypassButton.setButtonText("Vibrato");
    bypassButton.setToggleState(!vibratoRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        vibratoRef.setBypassed(!newState);
        onBypassToggled();
    };
    bypassButton.setTooltip("Click to toggle Vibrato bypass.");
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        vibratoRef.resetToDefaults();
        syncFromDsp();
    };
    resetButton.setTooltip("Reset all Vibrato parameters to defaults.");
    addAndMakeVisible(resetButton);

    // Tooltips
    rateKnob.setTooltip("LFO speed. Controls how fast the pitch wobbles.");
    depthKnob.setTooltip("Pitch deviation amount. Higher values produce wider vibrato.");
    toneKnob.setTooltip("Wet signal brightness. Darkens the vibrato output.");

    syncFromDsp();
}

VibratoPanel::~VibratoPanel()
{
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    toneKnob.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void VibratoPanel::syncFromDsp()
{
    rateKnob.setValue(vibratoRef.getRate(),   juce::dontSendNotification);
    depthKnob.setValue(vibratoRef.getDepth(), juce::dontSendNotification);
    toneKnob.setValue(vibratoRef.getTone(),   juce::dontSendNotification);
    bypassButton.setToggleState(!vibratoRef.isBypassed(), juce::dontSendNotification);
}

void VibratoPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void VibratoPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Three sliders in a row (Rate, Depth, Tone)
    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    auto sliderArea = area.removeFromTop(cellHeight);
    juce::Slider* sliders[] = { &rateKnob, &depthKnob, &toneKnob };
    juce::Label*  labels[]  = { &rateLabel, &depthLabel, &toneLabel };
    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        labels[i]->setBounds(cell.removeFromTop(labelH));
        sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                  .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void VibratoPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
