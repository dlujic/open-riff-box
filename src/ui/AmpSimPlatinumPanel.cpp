#include "AmpSimPlatinumPanel.h"
#include "Theme.h"
#include "dsp/AmpSimPlatinum.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

AmpSimPlatinumPanel::AmpSimPlatinumPanel(AmpSimPlatinum& ampSimPlatinum)
    : platinumRef(ampSimPlatinum)
{
    setupKnob(gainKnob,         gainLabel,         "Gain",       0.0, 1.0, 0.01);
    setupKnob(bassKnob,         bassLabel,         "Bass",       0.0, 1.0, 0.01);
    setupKnob(midKnob,          midLabel,          "Mid",        0.0, 1.0, 0.01);
    setupKnob(trebleKnob,       trebleLabel,       "Treble",     0.0, 1.0, 0.01);
    setupKnob(micPositionKnob,  micPositionLabel,  "Mic",        0.0, 1.0, 0.01);

    // Mic position: show dB (-4 to +4)
    micPositionKnob.textFromValueFunction = [](double v) {
        double db = -4.0 + 8.0 * v;
        if (std::abs(db) < 0.5) return juce::String("0 dB");
        return juce::String(db, 1) + " dB";
    };
    micPositionKnob.valueFromTextFunction = [](const juce::String& text) {
        return (text.trimCharactersAtEnd(" dB").getDoubleValue() + 4.0) / 8.0;
    };

    gainKnob.onValueChange = [this] {
        platinumRef.setGain(static_cast<float>(gainKnob.getValue()));
        onParameterChanged();
    };
    bassKnob.onValueChange = [this] {
        platinumRef.setBass(static_cast<float>(bassKnob.getValue()));
        onParameterChanged();
    };
    midKnob.onValueChange = [this] {
        platinumRef.setMid(static_cast<float>(midKnob.getValue()));
        onParameterChanged();
    };
    trebleKnob.onValueChange = [this] {
        platinumRef.setTreble(static_cast<float>(trebleKnob.getValue()));
        onParameterChanged();
    };
    micPositionKnob.onValueChange = [this] {
        platinumRef.setMicPosition(static_cast<float>(micPositionKnob.getValue()));
        onParameterChanged();
    };

    // OV Level horizontal slider
    ovLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    ovLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    ovLevelSlider.setRange(0.0, 1.0, 0.01);
    ovLevelSlider.setLookAndFeel(&sliderLF);
    ovLevelSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    ovLevelSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    ovLevelSlider.onValueChange = [this] {
        platinumRef.setOvLevel(static_cast<float>(ovLevelSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(ovLevelSlider);

    ovLevelLabel.setText("OV Level", juce::dontSendNotification);
    ovLevelLabel.setFont(Theme::Fonts::small());
    ovLevelLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    ovLevelLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(ovLevelLabel);

    // Master horizontal slider
    masterSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    masterSlider.setRange(0.0, 1.0, 0.01);
    masterSlider.setLookAndFeel(&sliderLF);
    masterSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    masterSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    masterSlider.onValueChange = [this] {
        platinumRef.setMaster(static_cast<float>(masterSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(masterSlider);

    masterLabel.setText("Master", juce::dontSendNotification);
    masterLabel.setFont(Theme::Fonts::small());
    masterLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    masterLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(masterLabel);

    // GAIN1/GAIN2 toggle button
    gainModeButton.setButtonText("GAIN1");
    gainModeButton.setLookAndFeel(&resetLF);
    gainModeButton.setClickingTogglesState(false);
    gainModeButton.onClick = [this] {
        int current = platinumRef.getGainMode();
        int next = (current == 0) ? 1 : 0;
        platinumRef.setGainMode(next);
        gainModeButton.setButtonText(next == 0 ? "GAIN1" : "GAIN2");
        onParameterChanged();
    };
    addAndMakeVisible(gainModeButton);

    // Cabinet selector ComboBox
    for (int i = 0; i < AmpSimPlatinum::kNumCabinets; ++i)
        cabinetSelector.addItem(AmpSimPlatinum::getCabinetName(i), i + 1);
    cabinetSelector.addItem("No Cabinet", AmpSimPlatinum::kNoCabinet + 1);

    cabinetSelector.onChange = [this] {
        auto id = cabinetSelector.getSelectedId();
        if (id == AmpSimPlatinum::kNoCabinet + 1)
            platinumRef.setCabinetType(AmpSimPlatinum::kNoCabinet);
        else if (id > 0 && id <= AmpSimPlatinum::kNumCabinets)
            platinumRef.setCabinetType(id - 1);
        else if (id == AmpSimPlatinum::kCustomCabinet + 1)
            platinumRef.loadCustomIR(platinumRef.getCustomIRFile());

#if JucePlugin_Build_Standalone
        // Clear stale custom IR path when switching to a factory/no cabinet
        if (id != AmpSimPlatinum::kCustomCabinet + 1)
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                if (auto* props = holder->settings.get())
                    props->setValue("customIRPathPlatinum", "");
#endif
        onParameterChanged();
    };
    cabinetSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(cabinetSelector);

    cabinetLabel.setText("Cabinet:", juce::dontSendNotification);
    cabinetLabel.setFont(Theme::Fonts::small());
    cabinetLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    cabinetLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cabinetLabel);

    // Load IR button
    loadIRButton.setButtonText("Load IR...");
    loadIRButton.setLookAndFeel(&resetLF);
    loadIRButton.onClick = [this] {
        auto exeDir = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory();
        auto defaultDir = exeDir.getChildFile("custom-irs");

#if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            if (auto* props = holder->settings.get())
            {
                auto saved = props->getValue("lastIRBrowseDir", "");
                if (saved.isNotEmpty() && juce::File(saved).isDirectory())
                    defaultDir = juce::File(saved);
            }
#endif

        if (!defaultDir.isDirectory())
            defaultDir = exeDir;

        fileChooser = std::make_unique<juce::FileChooser>(
            "Load Cabinet IR", defaultDir, "*.wav");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (!result.existsAsFile())
                    return;

                platinumRef.loadCustomIR(result);
                syncFromDsp();

#if JucePlugin_Build_Standalone
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    if (auto* props = holder->settings.get())
                    {
                        props->setValue("customIRPathPlatinum",
                                        result.getFullPathName());
                        props->setValue("lastIRBrowseDir",
                                        result.getParentDirectory().getFullPathName());
                    }
#endif
            });
    };
    addAndMakeVisible(loadIRButton);

    // Cabinet trim slider (-12 to +12 dB)
    cabTrimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cabTrimSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    cabTrimSlider.setRange(-12.0, 12.0, 0.1);
    cabTrimSlider.setLookAndFeel(&sliderLF);
    cabTrimSlider.textFromValueFunction = [](double v) {
        if (std::abs(v) < 0.05) return juce::String("0 dB");
        return juce::String(v, 1) + " dB";
    };
    cabTrimSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd(" dB").getDoubleValue();
    };
    cabTrimSlider.onValueChange = [this] {
        platinumRef.setCabTrim(static_cast<float>(cabTrimSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(cabTrimSlider);

    cabTrimLabel.setText("Cab Trim:", juce::dontSendNotification);
    cabTrimLabel.setFont(Theme::Fonts::small());
    cabTrimLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    cabTrimLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cabTrimLabel);

    // Bypass LED button
    bypassButton.setButtonText("Amp Platinum");
    bypassButton.setToggleState(!platinumRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::ampSim.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        platinumRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        platinumRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    gainKnob.setTooltip("Input gain into the preamp stages.");
    bassKnob.setTooltip("Bass control - tone stack.");
    midKnob.setTooltip("Mid control - tone stack.");
    trebleKnob.setTooltip("Treble control - tone stack.");
    micPositionKnob.setTooltip("Mic placement - low = close/dark (on-cone), high = bright/airy (off-axis).");
    ovLevelSlider.setTooltip("OV Level - attenuator after the overdrive channel (V3 stage input level).");
    masterSlider.setTooltip("Master volume - controls push-pull power amp output level.");
    gainModeButton.setTooltip("GAIN1 = lower gain (clean to crunch). GAIN2 = high gain channel.");
    cabinetSelector.setTooltip("Select the speaker cabinet impulse response.");
    cabTrimSlider.setTooltip("Manual cabinet volume trim. Adjusts on top of auto-normalization.");
    loadIRButton.setTooltip("Load a custom cabinet IR (.wav file).");
    bypassButton.setTooltip("Click to toggle Amp Sim Platinum bypass.");
    resetButton.setTooltip("Reset all Amp Sim Platinum parameters to defaults.");

    // Restore custom IR from settings (if one was saved last session)
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        if (auto* props = holder->settings.get())
        {
            auto savedIR = props->getValue("customIRPathPlatinum", "");
            if (savedIR.isNotEmpty() && platinumRef.getCabinetType() == AmpSimPlatinum::kCustomCabinet)
            {
                juce::File irFile(savedIR);
                if (irFile.existsAsFile())
                    platinumRef.loadCustomIR(irFile);
            }
        }
#endif

    syncFromDsp();
}

AmpSimPlatinumPanel::~AmpSimPlatinumPanel()
{
    gainKnob.setLookAndFeel(nullptr);
    bassKnob.setLookAndFeel(nullptr);
    midKnob.setLookAndFeel(nullptr);
    trebleKnob.setLookAndFeel(nullptr);
    micPositionKnob.setLookAndFeel(nullptr);
    ovLevelSlider.setLookAndFeel(nullptr);
    masterSlider.setLookAndFeel(nullptr);
    cabTrimSlider.setLookAndFeel(nullptr);
    cabinetSelector.setLookAndFeel(nullptr);
    loadIRButton.setLookAndFeel(nullptr);
    gainModeButton.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void AmpSimPlatinumPanel::syncFromDsp()
{
    gainKnob.setValue(platinumRef.getGain(),               juce::dontSendNotification);
    bassKnob.setValue(platinumRef.getBass(),               juce::dontSendNotification);
    midKnob.setValue(platinumRef.getMid(),                 juce::dontSendNotification);
    trebleKnob.setValue(platinumRef.getTreble(),           juce::dontSendNotification);
    micPositionKnob.setValue(platinumRef.getMicPosition(), juce::dontSendNotification);
    ovLevelSlider.setValue(platinumRef.getOvLevel(),       juce::dontSendNotification);
    masterSlider.setValue(platinumRef.getMaster(),         juce::dontSendNotification);
    cabTrimSlider.setValue(platinumRef.getCabTrim(),       juce::dontSendNotification);

    gainModeButton.setButtonText(platinumRef.getGainMode() == 0 ? "GAIN1" : "GAIN2");

    int cabType = platinumRef.getCabinetType();
    if (cabType == AmpSimPlatinum::kCustomCabinet)
    {
        auto irName = platinumRef.getCustomIRFile().getFileNameWithoutExtension();
        int customId = AmpSimPlatinum::kCustomCabinet + 1;

        // 14 IRs + No Cabinet = 15 built-in items
        if (cabinetSelector.getNumItems() <= AmpSimPlatinum::kNumCabinets + 1)
            cabinetSelector.addItem("Custom: " + irName, customId);
        else
            cabinetSelector.changeItemText(customId, "Custom: " + irName);

        cabinetSelector.setSelectedId(customId, juce::dontSendNotification);
    }
    else
    {
        cabinetSelector.setSelectedId(cabType + 1, juce::dontSendNotification);
    }

    bypassButton.setToggleState(!platinumRef.isBypassed(), juce::dontSendNotification);
}

void AmpSimPlatinumPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void AmpSimPlatinumPanel::resized()
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
    // Split into left column (Gain/Bass/Mid) and right column (Treble/Mic)
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

    //--------------------------------------------------------------------------
    // Left column: Gain / Bass / Mid sliders
    //--------------------------------------------------------------------------
    auto sliderArea = leftCol.removeFromTop(cellHeight);

    juce::Slider* leftSliders[] = { &gainKnob, &bassKnob, &midKnob };
    juce::Label*  leftLabels[]  = { &gainLabel, &bassLabel, &midLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto cell = sliderArea.removeFromLeft(cellWidth);
        leftLabels[i]->setBounds(cell.removeFromTop(labelH));
        leftSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                       .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    //--------------------------------------------------------------------------
    // Right column: Treble / Mic sliders
    //--------------------------------------------------------------------------
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &trebleKnob, &micPositionKnob };
    juce::Label*  rightLabels[]  = { &trebleLabel, &micPositionLabel };

    for (int i = 0; i < 2; ++i)
    {
        auto cell = rightSliderArea.removeFromLeft(cellWidth);
        rightLabels[i]->setBounds(cell.removeFromTop(labelH));
        rightSliders[i]->setBounds(cell.removeFromTop(sliderH + Theme::Dims::sliderTextBoxH)
                                        .withSizeKeepingCentre(sliderW, sliderH + Theme::Dims::sliderTextBoxH));
    }

    //--------------------------------------------------------------------------
    // Below sliders: full-width controls
    //--------------------------------------------------------------------------
    auto belowArea = getLocalBounds().reduced(Theme::Dims::panelPadding);
    belowArea.removeFromTop(38 + 16 + cellHeight);

    belowArea.removeFromTop(12);

    // Cabinet selector + Load IR button
    auto cabRow = belowArea.removeFromTop(28);
    cabinetLabel.setBounds(cabRow.removeFromLeft(60));
    loadIRButton.setBounds(cabRow.removeFromRight(70).withSizeKeepingCentre(66, 26));
    cabRow.removeFromRight(6);
    cabinetSelector.setBounds(cabRow);

    belowArea.removeFromTop(8);

    // Cabinet trim slider
    auto trimRow = belowArea.removeFromTop(26);
    cabTrimLabel.setBounds(trimRow.removeFromLeft(60));
    cabTrimSlider.setBounds(trimRow);

    belowArea.removeFromTop(8);

    // OV Level + Master side by side
    auto ovMasterRow = belowArea.removeFromTop(26);
    auto ovHalf = ovMasterRow.removeFromLeft(ovMasterRow.getWidth() / 2);
    ovHalf.removeFromRight(6);
    ovLevelLabel.setBounds(ovHalf.removeFromLeft(60));
    ovLevelSlider.setBounds(ovHalf);

    masterLabel.setBounds(ovMasterRow.removeFromLeft(54));
    masterSlider.setBounds(ovMasterRow);

    belowArea.removeFromTop(8);

    // GAIN1/GAIN2 toggle
    auto gainModeRow = belowArea.removeFromTop(30);
    gainModeButton.setBounds(gainModeRow.removeFromLeft(70).withSizeKeepingCentre(66, 26));
}

void AmpSimPlatinumPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
