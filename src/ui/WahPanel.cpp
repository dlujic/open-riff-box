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

    // Mode select -- control source ahead of the sweep (fork #4)
    modeSelector.addItem("Manual", 1);
    modeSelector.addItem("Auto", 2);
    modeSelector.addItem("Envelope", 3);
    modeSelector.onChange = [this] {
        auto id = modeSelector.getSelectedId();
        if (id > 0)
        {
            wahRef.setMode(id - 1);
            updateControlVisibility();
            onParameterChanged();
        }
    };
    modeSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(modeSelector);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(Theme::Fonts::small());
    modeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    modeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeLabel);

    // Wave select -- Auto mode LFO shape
    waveSelector.addItem("Sine", 1);
    waveSelector.addItem("Triangle", 2);
    waveSelector.onChange = [this] {
        auto id = waveSelector.getSelectedId();
        if (id > 0)
        {
            wahRef.setAutoWave(id - 1);
            onParameterChanged();
        }
    };
    waveSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(waveSelector);

    waveLabel.setText("Wave", juce::dontSendNotification);
    waveLabel.setFont(Theme::Fonts::small());
    waveLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    waveLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(waveLabel);

    // Auto mode: Rate + Depth
    setupKnob(rateKnob,  rateLabel,  "Rate",  0.0, 1.0, 0.01);
    setupKnob(depthKnob, depthLabel, "Depth", 0.0, 1.0, 0.01);

    rateKnob.textFromValueFunction = [](double v) {
        double hz = 0.1 + 9.9 * v * v;
        return juce::String(hz, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.1) return 0.0;
        return std::sqrt((hz - 0.1) / 9.9);
    };

    rateKnob.onValueChange  = [this] { wahRef.setAutoRate (static_cast<float>(rateKnob.getValue()));  onParameterChanged(); };
    depthKnob.onValueChange = [this] { wahRef.setAutoDepth(static_cast<float>(depthKnob.getValue())); onParameterChanged(); };

    // Envelope mode: Sens + Attack + Release
    setupKnob(sensKnob,    sensLabel,    "Sens",    0.0, 1.0, 0.01);
    setupKnob(attackKnob,  attackLabel,  "Attack",  0.0, 1.0, 0.01);
    setupKnob(releaseKnob, releaseLabel, "Release", 0.0, 1.0, 0.01);

    attackKnob.textFromValueFunction = [](double v) {
        double ms = 1.0 + 49.0 * v * v;
        return juce::String(juce::roundToInt(ms)) + " ms";
    };
    attackKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 1.0) return 0.0;
        return std::sqrt((ms - 1.0) / 49.0);
    };

    releaseKnob.textFromValueFunction = [](double v) {
        double ms = 20.0 + 480.0 * v * v;
        return juce::String(juce::roundToInt(ms)) + " ms";
    };
    releaseKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd(" ms").getDoubleValue();
        if (ms <= 20.0) return 0.0;
        return std::sqrt((ms - 20.0) / 480.0);
    };

    sensKnob.onValueChange    = [this] { wahRef.setEnvSens   (static_cast<float>(sensKnob.getValue()));    onParameterChanged(); };
    attackKnob.onValueChange  = [this] { wahRef.setEnvAttack (static_cast<float>(attackKnob.getValue()));  onParameterChanged(); };
    releaseKnob.onValueChange = [this] { wahRef.setEnvRelease(static_cast<float>(releaseKnob.getValue())); onParameterChanged(); };

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
    positionKnob.setTooltip("Sweep position. Toe (0%) is bright and high, heel (100%) is dark and low.\n"
                            "Its role changes with Mode: fixed point (Manual), sweep center (Auto), or resting point (Envelope).");
    colorationToggle.setTooltip("Adds a thin, asymmetric warmth when the resonant peak is pushed hard.");
    taperSelector.setTooltip("Dead Zones: pedal-feel taper matching the mechanical pot travel.\nLinear: raw electrical sweep, full range.");
    modeSelector.setTooltip("Manual: Position sets a fixed filter point, foot-pedal style.\n"
                            "Auto: an LFO sweeps the filter around the Center point.\n"
                            "Envelope: pick attack sweeps the filter away from the Rest point.");
    waveSelector.setTooltip("Sine: smooth, rounded sweep.\nTriangle: linear sweep, sharper turnarounds.");
    rateKnob.setTooltip("Auto sweep speed.");
    depthKnob.setTooltip("Auto sweep excursion around the Center point.");
    sensKnob.setTooltip("Envelope sensitivity. Higher = a pick attack sweeps the filter further from Rest.");
    attackKnob.setTooltip("How quickly the envelope follows a rising pick attack.");
    releaseKnob.setTooltip("How quickly the envelope decays back toward Rest after the attack.");

    syncFromDsp();
}

WahPanel::~WahPanel()
{
    positionKnob.setLookAndFeel(nullptr);
    colorationToggle.setLookAndFeel(nullptr);
    taperSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);

    modeSelector.setLookAndFeel(nullptr);
    waveSelector.setLookAndFeel(nullptr);
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    sensKnob.setLookAndFeel(nullptr);
    attackKnob.setLookAndFeel(nullptr);
    releaseKnob.setLookAndFeel(nullptr);
}

void WahPanel::syncFromDsp()
{
    positionKnob.setValue(wahRef.getPosition(), juce::dontSendNotification);
    colorationToggle.setToggleState(wahRef.getColoration(), juce::dontSendNotification);
    taperSelector.setSelectedId(wahRef.getTaperMode() + 1, juce::dontSendNotification);
    bypassButton.setToggleState(!wahRef.isBypassed(), juce::dontSendNotification);

    modeSelector.setSelectedId(wahRef.getMode() + 1, juce::dontSendNotification);
    waveSelector.setSelectedId(wahRef.getAutoWave() + 1, juce::dontSendNotification);
    rateKnob.setValue(wahRef.getAutoRate(), juce::dontSendNotification);
    depthKnob.setValue(wahRef.getAutoDepth(), juce::dontSendNotification);
    sensKnob.setValue(wahRef.getEnvSens(), juce::dontSendNotification);
    attackKnob.setValue(wahRef.getEnvAttack(), juce::dontSendNotification);
    releaseKnob.setValue(wahRef.getEnvRelease(), juce::dontSendNotification);

    updateControlVisibility();
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

    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    const int mode = wahRef.getMode();

    // Currently-visible {slider, label} cells, packed left-to-right with no holes:
    // Position always first, then the mode-specific controls. Give a mode a fourth knob
    // and this constant has to grow with it -- addCell asserts rather than running off
    // the end of the arrays.
    constexpr int kMaxKnobCells = 4;   // Position + the widest mode's controls (Envelope's 3)

    juce::Slider* sliders[kMaxKnobCells] = {};
    juce::Label*  labels[kMaxKnobCells]  = {};
    int numVisible = 0;

    auto addCell = [&](juce::Slider& slider, juce::Label& label)
    {
        jassert(numVisible < kMaxKnobCells);
        if (numVisible >= kMaxKnobCells) return;
        sliders[numVisible] = &slider;
        labels[numVisible]  = &label;
        ++numVisible;
    };

    addCell(positionKnob, positionLabel);

    if (mode == static_cast<int>(Wah::Mode::Auto))
    {
        addCell(rateKnob,  rateLabel);
        addCell(depthKnob, depthLabel);
    }
    else if (mode == static_cast<int>(Wah::Mode::Envelope))
    {
        addCell(sensKnob,    sensLabel);
        addCell(attackKnob,  attackLabel);
        addCell(releaseKnob, releaseLabel);
    }

    // Pinned to the widest mode rather than sized to the visible count, so the Mode combo
    // below doesn't shift under the cursor when the knob count changes.
    const int leftColWidth = cellWidth * kMaxKnobCells - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);  // gap between columns
    auto rightCol = area;

    auto sliderArea = leftCol.removeFromTop(cellHeight);
    for (int i = 0; i < numVisible; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        labels[i]->setBounds(cell.removeFromTop(labelH));
        sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                   .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Mode selector below the slider row
    leftCol.removeFromTop(12);
    auto modeRow = leftCol.removeFromTop(28);
    modeLabel.setBounds(modeRow.removeFromLeft(42));
    modeSelector.setBounds(modeRow);

    // Wave selector below Mode, laid out (and visible) in Auto only
    if (mode == static_cast<int>(Wah::Mode::Auto))
    {
        leftCol.removeFromTop(8);
        auto waveRow = leftCol.removeFromTop(28);
        waveLabel.setBounds(waveRow.removeFromLeft(42));
        waveSelector.setBounds(waveRow);
    }

    // Right column: Color toggle, Taper select
    rightCol.removeFromTop(8);
    auto colorRow = rightCol.removeFromTop(30);
    colorationToggle.setBounds(colorRow.removeFromLeft(110));

    rightCol.removeFromTop(16);
    taperLabel.setBounds(rightCol.removeFromTop(16));
    taperSelector.setBounds(rightCol.removeFromTop(28).removeFromLeft(160));
}

void WahPanel::updateControlVisibility()
{
    const int mode = wahRef.getMode();
    const bool isAuto     = (mode == static_cast<int>(Wah::Mode::Auto));
    const bool isEnvelope = (mode == static_cast<int>(Wah::Mode::Envelope));

    rateKnob.setVisible(isAuto);
    rateLabel.setVisible(isAuto);
    depthKnob.setVisible(isAuto);
    depthLabel.setVisible(isAuto);
    waveSelector.setVisible(isAuto);
    waveLabel.setVisible(isAuto);

    sensKnob.setVisible(isEnvelope);
    sensLabel.setVisible(isEnvelope);
    attackKnob.setVisible(isEnvelope);
    attackLabel.setVisible(isEnvelope);
    releaseKnob.setVisible(isEnvelope);
    releaseLabel.setVisible(isEnvelope);

    // The knob's role genuinely changes with the control source.
    positionLabel.setText(isAuto ? "Center" : (isEnvelope ? "Rest" : "Position"),
                           juce::dontSendNotification);

    resized();
    repaint();
}

void WahPanel::setupKnob(juce::Slider& knob, juce::Label& label,
                          const juce::String& name, double min, double max,
                          double interval)
{
    knob.setSliderStyle(juce::Slider::LinearVertical);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, Theme::Dims::sliderTextBoxH);
    knob.setRange(min, max, interval);
    knob.setLookAndFeel(&sliderLF);

    // Display 0-1 range as percentage
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
