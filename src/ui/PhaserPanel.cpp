#include "PhaserPanel.h"
#include "Theme.h"
#include "dsp/Phaser.h"

PhaserPanel::PhaserPanel(Phaser& phaser)
    : phaserRef(phaser)
{
    setupKnob(rateKnob,     rateLabel,     "Rate",     0.0, 1.0, 0.01);
    setupKnob(depthKnob,    depthLabel,    "Depth",    0.0, 1.0, 0.01);
    setupKnob(feedbackKnob, feedbackLabel, "Feedback", 0.0, 1.0, 0.01);
    setupKnob(mixKnob,      mixLabel,      "Mix",      0.0, 1.0, 0.01);

    // Rate: quadratic mapping 0.05 - 10 Hz
    rateKnob.textFromValueFunction = [](double v) {
        double hz = 0.05 + 9.95 * v * v;
        if (hz < 1.0) return juce::String(hz, 2) + " Hz";
        return juce::String(hz, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.05) return 0.0;
        return std::sqrt((hz - 0.05) / 9.95);
    };

    // Knob callbacks
    rateKnob.onValueChange = [this] {
        phaserRef.setRate(static_cast<float>(rateKnob.getValue()));
        onParameterChanged();
    };
    depthKnob.onValueChange = [this] {
        phaserRef.setDepth(static_cast<float>(depthKnob.getValue()));
        onParameterChanged();
    };
    feedbackKnob.onValueChange = [this] {
        phaserRef.setFeedback(static_cast<float>(feedbackKnob.getValue()));
        onParameterChanged();
    };
    mixKnob.onValueChange = [this] {
        phaserRef.setMix(static_cast<float>(mixKnob.getValue()));
        onParameterChanged();
    };

    // Stages dropdown (same pattern as Distortion mode selector)
    stagesSelector.addItem("Classic", 1);
    stagesSelector.addItem("Rich",    2);
    stagesSelector.addItem("Deep",    3);
    stagesSelector.onChange = [this] {
        auto id = stagesSelector.getSelectedId();
        if (id > 0)
        {
            phaserRef.setStages(id - 1);
            onParameterChanged();
        }
    };
    stagesSelector.setLookAndFeel(&sliderLF);
    stagesSelector.setTooltip("Allpass stages. Classic=4 (2 notches), Rich=8 (4 notches), Deep=12 (6 notches).");
    addAndMakeVisible(stagesSelector);

    stagesLabel.setText("Stages", juce::dontSendNotification);
    stagesLabel.setFont(Theme::Fonts::small());
    stagesLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    stagesLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(stagesLabel);

    // Bypass LED button
    bypassButton.setButtonText("Phaser");
    bypassButton.setToggleState(!phaserRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        phaserRef.setBypassed(!newState);
        onBypassToggled();
    };
    bypassButton.setTooltip("Click to toggle Phaser bypass.");
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        phaserRef.resetToDefaults();
        syncFromDsp();
    };
    resetButton.setTooltip("Reset all Phaser parameters to defaults.");
    addAndMakeVisible(resetButton);

    // Tooltips
    rateKnob.setTooltip("LFO speed. Controls how fast the phase sweep cycles.");
    depthKnob.setTooltip("Sweep range. How far the allpass frequencies sweep from center.");
    feedbackKnob.setTooltip("Resonance intensity. Higher values sharpen the peaks between notches.");
    mixKnob.setTooltip("Wet/dry mix. 50% gives maximum notch depth.");

    syncFromDsp();
}

PhaserPanel::~PhaserPanel()
{
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    feedbackKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
    stagesSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void PhaserPanel::syncFromDsp()
{
    rateKnob.setValue(phaserRef.getRate(),         juce::dontSendNotification);
    depthKnob.setValue(phaserRef.getDepth(),        juce::dontSendNotification);
    feedbackKnob.setValue(phaserRef.getFeedback(),  juce::dontSendNotification);
    mixKnob.setValue(phaserRef.getMix(),            juce::dontSendNotification);
    bypassButton.setToggleState(!phaserRef.isBypassed(), juce::dontSendNotification);
    stagesSelector.setSelectedId(phaserRef.getStages() + 1, juce::dontSendNotification);
}

void PhaserPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void PhaserPanel::resized()
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

    // Left column: Rate, Depth (2 sliders, matching Chorus layout)
    {
        auto sliderArea = leftCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &rateKnob, &depthKnob };
        juce::Label*  labels[]  = { &rateLabel, &depthLabel };
        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }

    // Stages selector below left column (same placement as Distortion mode selector)
    leftCol.removeFromTop(12);
    auto stagesRow = leftCol.removeFromTop(28);
    stagesLabel.setBounds(stagesRow.removeFromLeft(42));
    stagesSelector.setBounds(stagesRow.removeFromLeft(100));

    // Right column: Feedback, Mix (2 sliders, matching Chorus layout)
    {
        auto sliderArea = rightCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &feedbackKnob, &mixKnob };
        juce::Label*  labels[]  = { &feedbackLabel, &mixLabel };
        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }
}

void PhaserPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
