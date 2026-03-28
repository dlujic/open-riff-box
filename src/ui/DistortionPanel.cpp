#include "DistortionPanel.h"
#include "Theme.h"
#include "dsp/Distortion.h"

DistortionPanel::DistortionPanel(Distortion& distortion)
    : distortionRef(distortion)
{
    setupKnob(driveKnob, driveLabel, "Drive", 0.0, 1.0, 0.01);
    setupKnob(toneKnob,  toneLabel,  "Tone",  0.0, 1.0, 0.01);
    setupKnob(levelKnob, levelLabel, "Level", 0.0, 1.0, 0.01);

    // Tone: show cutoff frequency (mode-dependent, but roughly 500-8000 Hz)
    toneKnob.textFromValueFunction = [](double v) {
        double freq = 500.0 * std::pow(16.0, v);
        if (freq >= 1000.0) return juce::String(freq / 1000.0, 1) + " kHz";
        return juce::String(juce::roundToInt(freq)) + " Hz";
    };
    toneKnob.valueFromTextFunction = [](const juce::String& text) {
        double val = 0.0;
        if (text.containsIgnoreCase("kHz"))
            val = text.trimCharactersAtEnd(" kHz").getDoubleValue() * 1000.0;
        else
            val = text.trimCharactersAtEnd(" Hz").getDoubleValue();
        if (val <= 500.0) return 0.0;
        return std::log(val / 500.0) / std::log(16.0);
    };

    // Level: show dB (-60 to +6)
    levelKnob.textFromValueFunction = [](double v) {
        double db = -60.0 + 66.0 * v;
        if (db <= -59.0) return juce::String("-inf");
        return juce::String(db, 1) + " dB";
    };
    levelKnob.valueFromTextFunction = [](const juce::String& text) {
        if (text.containsIgnoreCase("inf")) return 0.0;
        return (text.trimCharactersAtEnd(" dB").getDoubleValue() + 60.0) / 66.0;
    };

    driveKnob.onValueChange = [this] {
        distortionRef.setDrive(static_cast<float>(driveKnob.getValue()));
        onParameterChanged();
    };
    toneKnob.onValueChange = [this] {
        distortionRef.setTone(static_cast<float>(toneKnob.getValue()));
        onParameterChanged();
    };
    levelKnob.onValueChange = [this] {
        distortionRef.setLevel(static_cast<float>(levelKnob.getValue()));
        onParameterChanged();
    };

    // Mode selector
    modeSelector.addItem("Overdrive", 1);
    modeSelector.addItem("Tube Drive", 2);
    modeSelector.addItem("Distortion", 3);
    modeSelector.addItem("Metal", 4);
    modeSelector.onChange = [this] {
        auto id = modeSelector.getSelectedId();
        if (id > 0)
        {
            distortionRef.setMode(static_cast<Distortion::Mode>(id - 1));
            updateControlVisibility();
            onParameterChanged();
        }
    };
    addAndMakeVisible(modeSelector);

    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setFont(Theme::Fonts::small());
    modeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    modeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeLabel);

    // Mode selector uses slider LF for themed ComboBox colours
    modeSelector.setLookAndFeel(&sliderLF);

    // Mix slider (horizontal, themed)
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
    mixSlider.setRange(0.0, 1.0, 0.01);
    mixSlider.setLookAndFeel(&sliderLF);
    mixSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    mixSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    mixSlider.onValueChange = [this] {
        distortionRef.setMix(static_cast<float>(mixSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(mixSlider);

    mixSliderLabel.setText("Dry Mix", juce::dontSendNotification);
    mixSliderLabel.setFont(Theme::Fonts::small());
    mixSliderLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    mixSliderLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(mixSliderLabel);

    // Saturate toggle + amount slider
    saturateToggle.setButtonText("Saturate");
    saturateToggle.setLookAndFeel(&bypassLF);
    saturateToggle.setClickingTogglesState(false);
    saturateToggle.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::distortion.getARGB()));
    saturateToggle.onClick = [this] {
        bool newState = !saturateToggle.getToggleState();
        saturateToggle.setToggleState(newState, juce::dontSendNotification);
        distortionRef.setSaturateEnabled(newState);
        onParameterChanged();
    };
    addAndMakeVisible(saturateToggle);

    saturateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    saturateSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 20);
    saturateSlider.setRange(0.0, 1.0, 0.01);
    saturateSlider.setLookAndFeel(&sliderLF);
    saturateSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    saturateSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    saturateSlider.onValueChange = [this] {
        distortionRef.setSaturate(static_cast<float>(saturateSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(saturateSlider);

    // Clip type LED radio buttons
    auto setupClipButton = [this](juce::ToggleButton& btn, const juce::String& name,
                                   Distortion::ClipType clipType)
    {
        btn.setButtonText(name);
        btn.setLookAndFeel(&bypassLF);
        btn.setClickingTogglesState(false);
        btn.getProperties().set("ledColour",
            static_cast<juce::int64>(Theme::Colours::distortion.getARGB()));
        btn.onClick = [this, clipType] { selectClipType(clipType); };
        addAndMakeVisible(btn);
    };

    setupClipButton(clipSoftBtn,   "Gentle",  Distortion::ClipType::Soft);
    setupClipButton(clipNormalBtn, "Warm",    Distortion::ClipType::Normal);
    setupClipButton(clipHardBtn,   "Sharp",   Distortion::ClipType::Hard);
    setupClipButton(clipXHardBtn,  "Aggro",   Distortion::ClipType::XHard);

    clipTypeLabel.setText("Clipping", juce::dontSendNotification);
    clipTypeLabel.setFont(Theme::Fonts::small());
    clipTypeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    clipTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(clipTypeLabel);

    // Bypass LED button
    bypassButton.setButtonText("Distortion");
    bypassButton.setToggleState(!distortionRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::distortion.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        distortionRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        distortionRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    driveKnob.setTooltip("Distortion intensity. Higher = more gain, more harmonics, more sustain.");
    toneKnob.setTooltip("Post-distortion low-pass filter. Lower = darker, higher = brighter.");
    levelKnob.setTooltip("Output volume after distortion.");
    mixSlider.setTooltip("Blends clean signal with distorted signal. 0% = fully distorted.");
    saturateToggle.setTooltip("Pre-distortion compressor. Evens out dynamics before clipping.");
    saturateSlider.setTooltip("Saturate amount - how much compression before the distortion stage.");
    clipSoftBtn.setTooltip("Gentle clipping - more headroom, smooth compression.");
    clipNormalBtn.setTooltip("Warm clipping - balanced tube-style saturation.");
    clipHardBtn.setTooltip("Sharp clipping - tighter knee, more aggressive.");
    clipXHardBtn.setTooltip("Aggro clipping - near hard-clip, maximum bite.");
    modeSelector.setTooltip("Overdrive: Mild amp-like breakup, good for blues/classic rock.\n"
                            "Tube Drive: Warmer, more compressed with asymmetric clipping.\n"
                            "Distortion: Two-stage cascaded clipping, auto-darkening at high gain.\n"
                            "Metal: Three-stage high-gain with 5 post-filters, built-in expander.");
    bypassButton.setTooltip("Click to toggle Distortion bypass.");
    resetButton.setTooltip("Reset all Distortion parameters to defaults.");

    syncFromDsp();
}

DistortionPanel::~DistortionPanel()
{
    driveKnob.setLookAndFeel(nullptr);
    toneKnob.setLookAndFeel(nullptr);
    levelKnob.setLookAndFeel(nullptr);
    mixSlider.setLookAndFeel(nullptr);
    saturateSlider.setLookAndFeel(nullptr);
    saturateToggle.setLookAndFeel(nullptr);
    modeSelector.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
    clipSoftBtn.setLookAndFeel(nullptr);
    clipNormalBtn.setLookAndFeel(nullptr);
    clipHardBtn.setLookAndFeel(nullptr);
    clipXHardBtn.setLookAndFeel(nullptr);
}

void DistortionPanel::syncFromDsp()
{
    driveKnob.setValue(distortionRef.getDrive(), juce::dontSendNotification);
    toneKnob.setValue(distortionRef.getTone(),   juce::dontSendNotification);
    levelKnob.setValue(distortionRef.getLevel(), juce::dontSendNotification);
    mixSlider.setValue(distortionRef.getMix(),    juce::dontSendNotification);
    saturateSlider.setValue(distortionRef.getSaturate(), juce::dontSendNotification);
    saturateToggle.setToggleState(distortionRef.getSaturateEnabled(), juce::dontSendNotification);
    modeSelector.setSelectedId(static_cast<int>(distortionRef.getMode()) + 1,
                               juce::dontSendNotification);

    // Sync clip type radio buttons
    auto ct = static_cast<int>(distortionRef.getClipType());
    juce::ToggleButton* clipBtns[] = { &clipSoftBtn, &clipNormalBtn, &clipHardBtn, &clipXHardBtn };
    for (int i = 0; i < 4; ++i)
        clipBtns[i]->setToggleState(i == ct, juce::dontSendNotification);

    bypassButton.setToggleState(!distortionRef.isBypassed(), juce::dontSendNotification);

    updateControlVisibility();
}

void DistortionPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());

    // Brushed metal bezel around clip type buttons (Overdrive only)
    if (!clipBezelBounds.isEmpty() && (distortionRef.getMode() == Distortion::Mode::Overdrive || distortionRef.getMode() == Distortion::Mode::TubeDrive))
    {
        auto bf = clipBezelBounds.toFloat();
        const float cornerR = 4.0f;

        // Brushed-metal gradient fill
        g.setGradientFill(juce::ColourGradient(
            Theme::Colours::topBarTop, bf.getX(), bf.getY(),
            Theme::Colours::topBarBottom, bf.getX(), bf.getBottom(), false));
        g.fillRoundedRectangle(bf, cornerR);

        // Subtle horizontal brushed lines
        g.saveState();
        g.reduceClipRegion(clipBezelBounds);
        for (int y = clipBezelBounds.getY(); y < clipBezelBounds.getBottom(); y += 2)
        {
            float alpha = (y % 4 == 0) ? 0.06f : 0.03f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawHorizontalLine(y, bf.getX(), bf.getRight());
        }
        g.restoreState();

        // Highlight on top, shadow on bottom
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(bf.getX() + cornerR, bf.getY() + 0.5f,
                   bf.getRight() - cornerR, bf.getY() + 0.5f, 0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(bf.getX() + cornerR, bf.getBottom() - 0.5f,
                   bf.getRight() - cornerR, bf.getBottom() - 0.5f, 0.5f);

        // Outer border
        g.setColour(juce::Colour(0xff1a1814));
        g.drawRoundedRectangle(bf, cornerR, 1.0f);
    }
}

void DistortionPanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    //--------------------------------------------------------------------------
    // Top row: Bypass LED (left) | Reset (right)
    //--------------------------------------------------------------------------
    auto topRow = area.removeFromTop(38);
    bypassButton.setBounds(topRow.removeFromLeft(180).withSizeKeepingCentre(180, 34));
    resetButton.setBounds(topRow.removeFromRight(60).withSizeKeepingCentre(56, 34));

    area.removeFromTop(16);

    //--------------------------------------------------------------------------
    // Split into left column (sliders) and right column (workspace)
    //--------------------------------------------------------------------------
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

    //--------------------------------------------------------------------------
    // Left column: Drive / Tone / Level sliders + Mode selector below
    //--------------------------------------------------------------------------
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* sliders[] = { &driveKnob, &toneKnob, &levelKnob };
    juce::Label*  labels[]  = { &driveLabel, &toneLabel, &levelLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        labels[i]->setBounds(cell.removeFromTop(labelH));
        sliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                   .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    // Mode selector below left sliders
    leftCol.removeFromTop(12);
    auto modeRow = leftCol.removeFromTop(28);
    modeLabel.setBounds(modeRow.removeFromLeft(42));
    modeSelector.setBounds(modeRow);

    //--------------------------------------------------------------------------
    // Right column: workspace (Saturate, Dry Mix, Clipping, + future space)
    //--------------------------------------------------------------------------

    // Saturate toggle + slider
    rightCol.removeFromTop(8);
    auto satArea = rightCol.removeFromTop(30);
    saturateToggle.setBounds(satArea.removeFromLeft(110));
    saturateSlider.setBounds(satArea);

    rightCol.removeFromTop(16);

    // Dry Mix label + horizontal slider
    mixSliderLabel.setBounds(rightCol.removeFromTop(16));
    mixSlider.setBounds(rightCol.removeFromTop(28));

    rightCol.removeFromTop(54);

    // Clipping label + 2x2 LED radio buttons inside brushed metal bezel
    clipTypeLabel.setBounds(rightCol.removeFromTop(16));

    const int btnSpacing = 2;
    const int btnH = 26;
    const int bezelPad = 4;

    int gridH = btnH * 2 + btnSpacing;
    clipBezelBounds = rightCol.removeFromTop(gridH + bezelPad * 2);
    auto gridArea = clipBezelBounds.reduced(bezelPad);

    auto clipTopRow = gridArea.removeFromTop(btnH);
    int btnW = (clipTopRow.getWidth() - btnSpacing) / 2;
    clipSoftBtn.setBounds(clipTopRow.removeFromLeft(btnW));
    clipTopRow.removeFromLeft(btnSpacing);
    clipNormalBtn.setBounds(clipTopRow);

    gridArea.removeFromTop(btnSpacing);
    auto clipBotRow = gridArea.removeFromTop(btnH);
    clipHardBtn.setBounds(clipBotRow.removeFromLeft(btnW));
    clipBotRow.removeFromLeft(btnSpacing);
    clipXHardBtn.setBounds(clipBotRow);

    // Remaining rightCol space is reserved for future controls
}

void DistortionPanel::selectClipType(Distortion::ClipType type)
{
    distortionRef.setClipType(type);

    int idx = static_cast<int>(type);
    juce::ToggleButton* clipBtns[] = { &clipSoftBtn, &clipNormalBtn, &clipHardBtn, &clipXHardBtn };
    for (int i = 0; i < 4; ++i)
        clipBtns[i]->setToggleState(i == idx, juce::dontSendNotification);

    onParameterChanged();
}

void DistortionPanel::updateControlVisibility()
{
    auto mode = distortionRef.getMode();
    bool isOverdrive = (mode == Distortion::Mode::Overdrive || mode == Distortion::Mode::TubeDrive);

    saturateToggle.setVisible(isOverdrive);
    saturateSlider.setVisible(isOverdrive);
    mixSliderLabel.setVisible(isOverdrive);
    mixSlider.setVisible(isOverdrive);
    clipTypeLabel.setVisible(isOverdrive);
    clipSoftBtn.setVisible(isOverdrive);
    clipNormalBtn.setVisible(isOverdrive);
    clipHardBtn.setVisible(isOverdrive);
    clipXHardBtn.setVisible(isOverdrive);

    repaint();
}

void DistortionPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
