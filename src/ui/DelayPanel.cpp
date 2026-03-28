#include "DelayPanel.h"
#include "Theme.h"
#include "dsp/AnalogDelay.h"

DelayPanel::DelayPanel(AnalogDelay& delay)
    : delayRef(delay)
{
    setupKnob(timeKnob,      timeLabel,      "Time",      0.0, 1.0, 0.01);
    setupKnob(intensityKnob, intensityLabel, "Intensity", 0.0, 1.0, 0.01);
    setupKnob(echoKnob,      echoLabel,      "Echo",      0.0, 1.0, 0.01);
    setupKnob(modDepthKnob,  modDepthLabel,  "Mod",       0.0, 1.0, 0.01);
    setupKnob(modRateKnob,   modRateLabel,   "Rate",      0.0, 1.0, 0.01);

    // Mod depth: show in ms (0 to ~5 ms)
    modDepthKnob.textFromValueFunction = [](double v) {
        return juce::String(v * 5.0, 1) + " ms";
    };
    modDepthKnob.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd(" ms").getDoubleValue() / 5.0;
    };

    // Mod rate: show in Hz (0.1 to ~5 Hz)
    modRateKnob.textFromValueFunction = [](double v) {
        double hz = 0.1 + 4.9 * v;
        return juce::String(hz, 1) + " Hz";
    };
    modRateKnob.valueFromTextFunction = [](const juce::String& text) {
        double hz = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (hz <= 0.1) return 0.0;
        return (hz - 0.1) / 4.9;
    };

    // Time slider: display in ms (logarithmic: 20ms at 0, 400ms at 1)
    timeKnob.textFromValueFunction = [](double v) {
        int ms = juce::roundToInt(20.0 * std::pow(20.0, v));
        return juce::String(ms) + "ms";
    };
    timeKnob.valueFromTextFunction = [](const juce::String& text) {
        double ms = text.trimCharactersAtEnd("ms").getDoubleValue();
        if (ms <= 20.0) return 0.0;
        return std::log(ms / 20.0) / std::log(20.0);
    };

    timeKnob.onValueChange = [this] {
        delayRef.setTime(static_cast<float>(timeKnob.getValue()));
        onParameterChanged();
    };
    intensityKnob.onValueChange = [this] {
        delayRef.setIntensity(static_cast<float>(intensityKnob.getValue()));
        onParameterChanged();
    };
    echoKnob.onValueChange = [this] {
        delayRef.setEcho(static_cast<float>(echoKnob.getValue()));
        onParameterChanged();
    };
    modDepthKnob.onValueChange = [this] {
        delayRef.setModDepth(static_cast<float>(modDepthKnob.getValue()));
        onParameterChanged();
    };
    modRateKnob.onValueChange = [this] {
        delayRef.setModRate(static_cast<float>(modRateKnob.getValue()));
        onParameterChanged();
    };

    // Tone horizontal slider
    toneSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    toneSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
    toneSlider.setRange(0.0, 1.0, 0.01);
    toneSlider.setLookAndFeel(&sliderLF);
    toneSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    toneSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    toneSlider.onValueChange = [this] {
        delayRef.setTone(static_cast<float>(toneSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(toneSlider);

    toneLabel.setText("Tone", juce::dontSendNotification);
    toneLabel.setFont(Theme::Fonts::small());
    toneLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    toneLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(toneLabel);

    // Bypass LED button
    bypassButton.setButtonText("Delay");
    bypassButton.setToggleState(!delayRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::timeBased.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        delayRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        delayRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    timeKnob.setTooltip("Delay time (logarithmic). More resolution at shorter times.");
    intensityKnob.setTooltip("Feedback - how many repeats. Above ~80% the delay self-oscillates.");
    echoKnob.setTooltip("Wet/dry mix. 0% = fully dry, 100% = fully wet.");
    modDepthKnob.setTooltip("Modulation intensity - how much the delay time wobbles.");
    modRateKnob.setTooltip("Modulation speed. Higher = faster wobble.");
    toneSlider.setTooltip("Feedback path tone. Lower = darker repeats, higher = brighter.");
    bypassButton.setTooltip("Click to toggle Delay bypass.");
    resetButton.setTooltip("Reset all Delay parameters to defaults.");

    syncFromDsp();
}

DelayPanel::~DelayPanel()
{
    timeKnob.setLookAndFeel(nullptr);
    intensityKnob.setLookAndFeel(nullptr);
    echoKnob.setLookAndFeel(nullptr);
    modDepthKnob.setLookAndFeel(nullptr);
    modRateKnob.setLookAndFeel(nullptr);
    toneSlider.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void DelayPanel::syncFromDsp()
{
    timeKnob.setValue(delayRef.getTime(),           juce::dontSendNotification);
    intensityKnob.setValue(delayRef.getIntensity(), juce::dontSendNotification);
    echoKnob.setValue(delayRef.getEcho(),           juce::dontSendNotification);
    modDepthKnob.setValue(delayRef.getModDepth(),   juce::dontSendNotification);
    modRateKnob.setValue(delayRef.getModRate(),     juce::dontSendNotification);
    toneSlider.setValue(delayRef.getTone(),         juce::dontSendNotification);
    bypassButton.setToggleState(!delayRef.isBypassed(), juce::dontSendNotification);
}

void DelayPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void DelayPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // Top row: Bypass LED (left), Reset (right)
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    // Split into left column (main sliders) and right column (mod + tone)
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

    // Left column: Time / Intensity / Echo sliders
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &timeKnob, &intensityKnob, &echoKnob };
    juce::Label*  leftLabels[]  = { &timeLabel, &intensityLabel, &echoLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Tone label + horizontal slider below left sliders
    leftCol.removeFromTop(12);
    toneLabel.setBounds(leftCol.removeFromTop(16));
    toneSlider.setBounds(leftCol.removeFromTop(28));

    // Right column: Mod Depth / Mod Rate sliders
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &modDepthKnob, &modRateKnob };
    juce::Label*  rightLabels[]  = { &modDepthLabel, &modRateLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = rightSliderArea.removeFromLeft(cellWidth);
        rightLabels[i]->setBounds(cell.removeFromTop(labelH));
        rightSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                        .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }
}

void DelayPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
