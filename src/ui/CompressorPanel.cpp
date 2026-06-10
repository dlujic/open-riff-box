#include "CompressorPanel.h"
#include "Theme.h"
#include "dsp/Compressor.h"

CompressorPanel::CompressorPanel(Compressor& comp)
    : compRef(comp)
{
    setupKnob(sustainKnob, sustainLabel, "Sustain", 0.0, 1.0, 0.01);
    setupKnob(attackKnob,  attackLabel,  "Attack",  0.0, 1.0, 0.01);
    setupKnob(blendKnob,   blendLabel,   "Blend",   0.0, 1.0, 0.01);
    setupKnob(levelKnob,   levelLabel,   "Level",   0.0, 1.0, 0.01);

    // Custom text formatters so knobs show meaningful units.
    sustainKnob.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 30.0)) + " dB";
    };
    sustainKnob.valueFromTextFunction = [](const juce::String& t) {
        return juce::jlimit(0.0, 1.0, t.trimCharactersAtEnd(" dB").getDoubleValue() / 30.0);
    };

    attackKnob.textFromValueFunction = [](double v) {
        // Studio/Squeeze: 2*25^t ms; Opto uses a different range but the panel
        // shows the Studio mapping for consistency (Opto range shifts internally).
        double ms = 2.0 * std::pow(25.0, v);
        return juce::String(ms, 1) + " ms";
    };
    attackKnob.valueFromTextFunction = [](const juce::String& t) {
        double ms = t.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 2.0) return 0.0;
        return juce::jlimit(0.0, 1.0, std::log(ms / 2.0) / std::log(25.0));
    };

    levelKnob.textFromValueFunction = [](double v) {
        double dB = -12.0 + 24.0 * v;
        return juce::String(dB, 1) + " dB";
    };
    levelKnob.valueFromTextFunction = [](const juce::String& t) {
        double dB = t.trimCharactersAtEnd(" dB").getDoubleValue();
        return juce::jlimit(0.0, 1.0, (dB + 12.0) / 24.0);
    };

    sustainKnob.onValueChange = [this] { compRef.setSustain(static_cast<float>(sustainKnob.getValue())); onParameterChanged(); };
    attackKnob .onValueChange = [this] { compRef.setAttack (static_cast<float>(attackKnob.getValue()));  onParameterChanged(); };
    blendKnob  .onValueChange = [this] { compRef.setBlend  (static_cast<float>(blendKnob.getValue()));   onParameterChanged(); };
    levelKnob  .onValueChange = [this] { compRef.setLevel  (static_cast<float>(levelKnob.getValue()));   onParameterChanged(); };

    modeSelector.addItem("Studio",  1);
    modeSelector.addItem("Squeeze", 2);
    modeSelector.addItem("Opto",    3);
    modeSelector.onChange = [this] {
        auto id = modeSelector.getSelectedId();
        if (id > 0)
        {
            compRef.setMode(id - 1);
            onParameterChanged();
        }
    };
    modeSelector.setLookAndFeel(&sliderLF);
    modeSelector.setTooltip("Compressor character. Studio=transparent VCA, Squeeze=high-ratio Dyna Comp spank, Opto=LA-2A-style slow memory release.");
    addAndMakeVisible(modeSelector);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(Theme::Fonts::small());
    modeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    modeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeLabel);

    bypassButton.setButtonText("Compressor");
    bypassButton.setToggleState(!compRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::gate.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        compRef.setBypassed(!newState);
        onBypassToggled();
    };
    bypassButton.setTooltip("Click to toggle Compressor bypass.");
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        compRef.resetToDefaults();
        syncFromDsp();
    };
    resetButton.setTooltip("Reset all Compressor parameters to defaults.");
    addAndMakeVisible(resetButton);

    sustainKnob.setTooltip("Sidechain drive. Controls how much signal is fed into the detector; higher = more compression.");
    attackKnob .setTooltip("Attack time. Shorter lets more pick transient through; longer evens out attack.");
    blendKnob  .setTooltip("Parallel blend. 0=dry, 1=fully compressed. Mix in clean signal to restore dynamics.");
    levelKnob  .setTooltip("Output trim. +/-12 dB around auto makeup gain. Useful for matching bypass level.");

    syncFromDsp();

    // 30 Hz timer for GR meter updates.
    startTimerHz(30);
}

CompressorPanel::~CompressorPanel()
{
    stopTimer();
    sustainKnob.setLookAndFeel(nullptr);
    attackKnob .setLookAndFeel(nullptr);
    blendKnob  .setLookAndFeel(nullptr);
    levelKnob  .setLookAndFeel(nullptr);
    modeSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton .setLookAndFeel(nullptr);
}

void CompressorPanel::timerCallback()
{
    float gr = compRef.getGainReductionDb();   // negative dB or 0

    // UI-side decay: ~12 dB/s at 30 Hz means ~0.4 dB per frame decay.
    // Grab fast (instant attack), release slowly.
    if (gr < displayGrDb)
        displayGrDb = gr;                          // fast attack
    else
        displayGrDb += 0.4f;                       // ~12 dB/s decay at 30 Hz

    if (displayGrDb > 0.0f)
        displayGrDb = 0.0f;

    repaint();
}

void CompressorPanel::syncFromDsp()
{
    sustainKnob.setValue(compRef.getSustain(), juce::dontSendNotification);
    attackKnob .setValue(compRef.getAttack(),  juce::dontSendNotification);
    blendKnob  .setValue(compRef.getBlend(),   juce::dontSendNotification);
    levelKnob  .setValue(compRef.getLevel(),   juce::dontSendNotification);
    bypassButton.setToggleState(!compRef.isBypassed(), juce::dontSendNotification);
    modeSelector.setSelectedId(compRef.getMode() + 1, juce::dontSendNotification);
}

void CompressorPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());

    // GR meter: a horizontal bar beneath the knob area.
    // 0 dB = right edge (no reduction); negative dB fills left from right edge.
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);
    // Paint below the knob rows; approximate position.
    auto meterArea = area.removeFromBottom(14).reduced(0, 2);

    // Background track
    g.setColour(juce::Colour(0xff1a1610));
    g.fillRect(meterArea);

    // Active GR fill: gate colour at low opacity, brighter at higher reduction.
    const float maxGrDisplay = -24.0f;
    float fraction = juce::jlimit(0.0f, 1.0f, displayGrDb / maxGrDisplay);
    if (fraction > 0.0f)
    {
        auto fillArea = meterArea;
        fillArea.setWidth(juce::roundToInt(meterArea.getWidth() * fraction));
        // Position flush right (fill grows left from right edge as GR increases).
        fillArea.setX(meterArea.getRight() - fillArea.getWidth());
        g.setColour(Theme::Colours::gate.withAlpha(0.55f + 0.35f * fraction));
        g.fillRect(fillArea);
    }

    // Label
    g.setFont(Theme::Fonts::small());
    g.setColour(Theme::Colours::textSecondary);
    g.drawText("GR", meterArea.removeFromLeft(22), juce::Justification::centredLeft, false);
}

void CompressorPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton .setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Reserve bottom strip for GR meter (painted in paint()).
    area.removeFromBottom(14);
    area.removeFromBottom(4);   // spacing

    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    // Left column: Sustain + Attack + Mode selector
    const int leftColWidth = cellWidth * 2 - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);
    auto rightCol = area;

    {
        auto sliderArea = leftCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &sustainKnob, &attackKnob };
        juce::Label*  labels[]  = { &sustainLabel, &attackLabel };
        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }

    leftCol.removeFromTop(12);
    auto modeRow = leftCol.removeFromTop(28);
    modeLabel   .setBounds(modeRow.removeFromLeft(42));
    modeSelector.setBounds(modeRow.removeFromLeft(110));

    // Right column: Blend + Level
    {
        auto sliderArea = rightCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &blendKnob, &levelKnob };
        juce::Label*  labels[]  = { &blendLabel, &levelLabel };
        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }
}

void CompressorPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
        knob.valueFromTextFunction = [](const juce::String& t) {
            return t.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
        };
    }

    addAndMakeVisible(knob);

    label.setText(name, juce::dontSendNotification);
    label.setFont(Theme::Fonts::small());
    label.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
}
