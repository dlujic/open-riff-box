#include "TremoloPanel.h"
#include "Theme.h"
#include "dsp/Tremolo.h"

TremoloPanel::TremoloPanel(Tremolo& tremolo)
    : tremoloRef(tremolo)
{
    setupKnob(rateKnob,   rateLabel,   "Rate",   0.0, 1.0, 0.01);
    setupKnob(depthKnob,  depthLabel,  "Depth",  0.0, 1.0, 0.01);
    setupKnob(widthKnob,  widthLabel,  "Width",  0.0, 1.0, 0.01);
    setupKnob(outputKnob, outputLabel, "Output", 0.0, 1.0, 0.01);

    rateKnob.textFromValueFunction = [](double v) {
        double hz = 0.3 + 11.7 * v * v;
        if (hz < 1.0) return juce::String(hz, 2) + " Hz";
        return juce::String(hz, 1) + " Hz";
    };
    rateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.3) return 0.0;
        return std::sqrt((hz - 0.3) / 11.7);
    };

    widthKnob.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 150.0)) + juce::String(juce::CharPointer_UTF8("\xc2\xb0"));
    };
    widthKnob.valueFromTextFunction = [](const juce::String& text) {
        double deg = text.upToFirstOccurrenceOf(juce::String(juce::CharPointer_UTF8("\xc2\xb0")), false, false).getDoubleValue();
        return juce::jlimit(0.0, 1.0, deg / 150.0);
    };

    outputKnob.textFromValueFunction = [](double v) {
        double dB = -12.0 + 24.0 * v;
        return juce::String(dB, 1) + " dB";
    };
    outputKnob.valueFromTextFunction = [](const juce::String& text) {
        double dB = text.trimCharactersAtEnd(" dB").getDoubleValue();
        return juce::jlimit(0.0, 1.0, (dB + 12.0) / 24.0);
    };

    rateKnob.onValueChange   = [this] { tremoloRef.setRate  (static_cast<float>(rateKnob.getValue()));   onParameterChanged(); };
    depthKnob.onValueChange  = [this] { tremoloRef.setDepth (static_cast<float>(depthKnob.getValue()));  onParameterChanged(); };
    widthKnob.onValueChange  = [this] { tremoloRef.setWidth (static_cast<float>(widthKnob.getValue()));  onParameterChanged(); };
    outputKnob.onValueChange = [this] { tremoloRef.setOutput(static_cast<float>(outputKnob.getValue())); onParameterChanged(); };

    modeSelector.addItem("Photo",    1);
    modeSelector.addItem("Bias",     2);
    modeSelector.addItem("Harmonic", 3);
    modeSelector.onChange = [this] {
        auto id = modeSelector.getSelectedId();
        if (id > 0)
        {
            tremoloRef.setMode(id - 1);
            onParameterChanged();
        }
    };
    modeSelector.setLookAndFeel(&sliderLF);
    modeSelector.setTooltip("Tremolo topology. Photo=optical (Deluxe Reverb), Bias=power-tube bias modulation (Princeton), Harmonic=dual-band antiphase (brownface Vibroverb).");
    addAndMakeVisible(modeSelector);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(Theme::Fonts::small());
    modeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    modeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeLabel);

    bypassButton.setButtonText("Tremolo");
    bypassButton.setToggleState(!tremoloRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::modulation.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        tremoloRef.setBypassed(!newState);
        onBypassToggled();
    };
    bypassButton.setTooltip("Click to toggle Tremolo bypass.");
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        tremoloRef.resetToDefaults();
        syncFromDsp();
    };
    resetButton.setTooltip("Reset all Tremolo parameters to defaults.");
    addAndMakeVisible(resetButton);

    rateKnob.setTooltip("LFO speed. Controls how fast the volume cycles.");
    depthKnob.setTooltip("Modulation amount. 0=off, 100%=approaches gating at trough.");
    widthKnob.setTooltip("Stereo width. 0=mono, 150 deg=wide stereo (mono-safe; full pan would collapse in mono).");
    outputKnob.setTooltip("Make-up gain. -12 to +12 dB to compensate volume drop or balance preset levels.");

    syncFromDsp();
}

TremoloPanel::~TremoloPanel()
{
    rateKnob.setLookAndFeel(nullptr);
    depthKnob.setLookAndFeel(nullptr);
    widthKnob.setLookAndFeel(nullptr);
    outputKnob.setLookAndFeel(nullptr);
    modeSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void TremoloPanel::syncFromDsp()
{
    rateKnob.setValue  (tremoloRef.getRate(),   juce::dontSendNotification);
    depthKnob.setValue (tremoloRef.getDepth(),  juce::dontSendNotification);
    widthKnob.setValue (tremoloRef.getWidth(),  juce::dontSendNotification);
    outputKnob.setValue(tremoloRef.getOutput(), juce::dontSendNotification);
    bypassButton.setToggleState(!tremoloRef.isBypassed(), juce::dontSendNotification);
    modeSelector.setSelectedId(tremoloRef.getMode() + 1, juce::dontSendNotification);
}

void TremoloPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void TremoloPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Two-column layout (matching PhaserPanel)
    const int sliderW    = Theme::Dims::sliderWidth;
    const int sliderH    = Theme::Dims::sliderHeight;
    const int labelH     = Theme::Dims::sliderLabelHeight;
    const int spacing    = Theme::Dims::sliderSpacing;
    const int cellWidth  = sliderW + spacing;
    const int cellHeight = labelH + sliderH + Theme::Dims::sliderTextBoxH + 4;

    const int leftColWidth = cellWidth * 2 - spacing + 16;
    auto leftCol  = area.removeFromLeft(leftColWidth);
    area.removeFromLeft(16);
    auto rightCol = area;

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

    leftCol.removeFromTop(12);
    auto modeRow = leftCol.removeFromTop(28);
    modeLabel.setBounds(modeRow.removeFromLeft(42));
    modeSelector.setBounds(modeRow.removeFromLeft(110));

    {
        auto sliderArea = rightCol.removeFromTop(cellHeight);
        juce::Slider* sliders[] = { &widthKnob, &outputKnob };
        juce::Label*  labels[]  = { &widthLabel, &outputLabel };
        for (int i = 0; i < 2; ++i)
        {
            auto cell = sliderArea.removeFromLeft(cellWidth);
            labels[i]->setBounds(cell.removeFromTop(labelH));
            sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                      .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
        }
    }
}

void TremoloPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
