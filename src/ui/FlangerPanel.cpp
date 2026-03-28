#include "FlangerPanel.h"
#include "Theme.h"
#include "dsp/Flanger.h"

FlangerPanel::FlangerPanel(Flanger& flanger)
    : flangerRef(flanger)
{
    setupKnob(rateKnob,     rateLabel,     "Rate",     0.0, 1.0, 0.01);
    setupKnob(depthKnob,    depthLabel,    "Depth",    0.0, 1.0, 0.01);
    setupKnob(manualKnob,   manualLabel,   "Manual",   0.0, 1.0, 0.01);
    setupKnob(feedbackKnob, feedbackLabel, "Feedback", 0.0, 1.0, 0.01);
    setupKnob(eqKnob,       eqLabel,       "Tone",     0.0, 1.0, 0.01);
    setupKnob(mixKnob,      mixLabel,      "Mix",      0.0, 1.0, 0.01);

    // Rate: quadratic mapping 0.05 - 10 Hz
    rateKnob.textFromValueFunction = [](double v) {
        return juce::String(0.05 + 9.95 * v * v, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.05) return 0.0;
        return std::sqrt((hz - 0.05) / 9.95);
    };

    // Depth: linear 0.1 - 4.0 ms
    depthKnob.textFromValueFunction = [](double v) {
        return juce::String(0.1 + 3.9 * v, 1) + " ms";
    };
    depthKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 0.1) return 0.0;
        return (ms - 0.1) / 3.9;
    };

    // Manual: linear 0.5 - 10.0 ms
    manualKnob.textFromValueFunction = [](double v) {
        return juce::String(0.5 + 9.5 * v, 1) + " ms";
    };
    manualKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 0.5) return 0.0;
        return (ms - 0.5) / 9.5;
    };

    // Feedback: percentage display (setupKnob default 0-1 % handling covers this)

    // EQ/Tone: 800 Hz to 12 kHz logarithmic
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

    // Mix: percentage display (setupKnob default handles this)

    // Knob callbacks
    rateKnob.onValueChange = [this] {
        flangerRef.setRate(static_cast<float>(rateKnob.getValue()));
        onParameterChanged();
    };
    depthKnob.onValueChange = [this] {
        flangerRef.setDepth(static_cast<float>(depthKnob.getValue()));
        onParameterChanged();
    };
    manualKnob.onValueChange = [this] {
        flangerRef.setManual(static_cast<float>(manualKnob.getValue()));
        onParameterChanged();
    };
    feedbackKnob.onValueChange = [this] {
        flangerRef.setFeedback(static_cast<float>(feedbackKnob.getValue()));
        onParameterChanged();
    };
    eqKnob.onValueChange = [this] {
        flangerRef.setEQ(static_cast<float>(eqKnob.getValue()));
        onParameterChanged();
    };
    mixKnob.onValueChange = [this] {
        flangerRef.setMix(static_cast<float>(mixKnob.getValue()));
        onParameterChanged();
    };

    // Polarity dropdown (replaces the old +/- toggle button)
    polaritySelector.addItem("Positive (jet)",  1);
    polaritySelector.addItem("Negative (hollow)", 2);
    polaritySelector.onChange = [this] {
        auto id = polaritySelector.getSelectedId();
        if (id > 0)
        {
            flangerRef.setFeedbackPositive(id == 1);
            onParameterChanged();
        }
    };
    polaritySelector.setLookAndFeel(&sliderLF);
    polaritySelector.setTooltip("Feedback polarity.\n"
                                "Positive: metallic jet sweep, classic flanger sound.\n"
                                "Negative: hollow, nasal, inverted comb filter.");
    addAndMakeVisible(polaritySelector);

    polarityLabel.setText("Polarity", juce::dontSendNotification);
    polarityLabel.setFont(Theme::Fonts::small());
    polarityLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    polarityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(polarityLabel);

    // Bypass LED button
    bypassButton.setButtonText("Flanger");
    bypassButton.setToggleState(!flangerRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        flangerRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        flangerRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    rateKnob.setTooltip("LFO speed. Slower = majestic jet sweep, faster = rapid flutter.");
    depthKnob.setTooltip("Sweep range - how far the delay sweeps from center. More = wider jet effect.");
    manualKnob.setTooltip("Center delay position. At Rate=0, this sets the static comb filter frequency.");
    feedbackKnob.setTooltip("Resonance intensity. Higher = more metallic/ringy character.");
    eqKnob.setTooltip("Wet signal brightness filter (800 Hz to 12 kHz). Only affects the flanger, not dry signal.");
    mixKnob.setTooltip("Wet/dry mix. 50% gives maximum comb filter depth.");
    bypassButton.setTooltip("Click to toggle Flanger bypass.");
    resetButton.setTooltip("Reset all Flanger parameters to defaults.");

    syncFromDsp();
}

FlangerPanel::~FlangerPanel()
{
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    manualKnob.setLookAndFeel(nullptr);
    feedbackKnob.setLookAndFeel(nullptr);
    eqKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
    polaritySelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void FlangerPanel::syncFromDsp()
{
    rateKnob.setValue(flangerRef.getRate(),         juce::dontSendNotification);
    depthKnob.setValue(flangerRef.getDepth(),        juce::dontSendNotification);
    manualKnob.setValue(flangerRef.getManual(),      juce::dontSendNotification);
    feedbackKnob.setValue(flangerRef.getFeedback(),  juce::dontSendNotification);
    eqKnob.setValue(flangerRef.getEQ(),              juce::dontSendNotification);
    mixKnob.setValue(flangerRef.getMix(),            juce::dontSendNotification);
    bypassButton.setToggleState(!flangerRef.isBypassed(), juce::dontSendNotification);
    polaritySelector.setSelectedId(flangerRef.getFeedbackPositive() ? 1 : 2, juce::dontSendNotification);
}

void FlangerPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void FlangerPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Two-column layout (matching Chorus and Distortion panels)
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

    // Left column: Rate, Depth, Manual (3 sliders)
    {
        auto sliderArea = leftCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &rateKnob, &depthKnob, &manualKnob };
        juce::Label*  labels[]  = { &rateLabel, &depthLabel, &manualLabel };
        for (int i = 0; i < 3; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }

    // Polarity selector below left column (same placement as Distortion mode selector)
    leftCol.removeFromTop(12);
    auto polarityRow = leftCol.removeFromTop(28);
    polarityLabel.setBounds(polarityRow.removeFromLeft(48));
    polaritySelector.setBounds(polarityRow.removeFromLeft(140));

    // Right column: Feedback, Tone, Mix (3 sliders)
    {
        auto sliderArea = rightCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &feedbackKnob, &eqKnob, &mixKnob };
        juce::Label*  labels[]  = { &feedbackLabel, &eqLabel, &mixLabel };
        for (int i = 0; i < 3; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }
}

void FlangerPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
