#include "AmpSimGoldPanel.h"
#include "Theme.h"
#include "dsp/AmpSimGold.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

AmpSimGoldPanel::AmpSimGoldPanel(AmpSimGold& ampSimGold)
    : ampSimGoldRef(ampSimGold)
{
    setupKnob(gainKnob,   gainLabel,   "Gain",   0.0, 1.0, 0.01);
    setupKnob(bassKnob,   bassLabel,   "Bass",   0.0, 1.0, 0.01);
    setupKnob(midKnob,    midLabel,    "Mid",    0.0, 1.0, 0.01);
    setupKnob(trebleKnob, trebleLabel, "Treble", 0.0, 1.0, 0.01);
    setupKnob(brightnessKnob,  brightnessLabel,  "Brightness", 0.0, 1.0, 0.01);
    setupKnob(micPositionKnob, micPositionLabel, "Mic",        0.0, 1.0, 0.01);

    // Bass/Mid/Treble: subtractive cut-to-flat (-15 dB to 0 dB)
    auto eqDisplay = [](double v) {
        double db = -15.0 * (1.0 - v);
        if (std::abs(db) < 0.5) return juce::String("0 dB");
        return juce::String(db, 1) + " dB";
    };
    auto eqFromText = [](const juce::String& text) {
        double db = text.trimCharactersAtEnd(" dB").getDoubleValue();
        return 1.0 + db / 15.0;
    };
    bassKnob.textFromValueFunction = eqDisplay;
    bassKnob.valueFromTextFunction = eqFromText;
    midKnob.textFromValueFunction = eqDisplay;
    midKnob.valueFromTextFunction = eqFromText;
    trebleKnob.textFromValueFunction = eqDisplay;
    trebleKnob.valueFromTextFunction = eqFromText;

    // Brightness: show cutoff frequency (3-10 kHz)
    brightnessKnob.textFromValueFunction = [](double v) {
        double freq = 3000.0 + v * 7000.0;
        return juce::String(freq / 1000.0, 1) + " kHz";
    };
    brightnessKnob.valueFromTextFunction = [](const juce::String& text) {
        return (text.trimCharactersAtEnd(" kHz").getDoubleValue() * 1000.0 - 3000.0) / 7000.0;
    };

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
        ampSimGoldRef.setGain(static_cast<float>(gainKnob.getValue()));
        onParameterChanged();
    };
    bassKnob.onValueChange = [this] {
        ampSimGoldRef.setBass(static_cast<float>(bassKnob.getValue()));
        onParameterChanged();
    };
    midKnob.onValueChange = [this] {
        ampSimGoldRef.setMid(static_cast<float>(midKnob.getValue()));
        onParameterChanged();
    };
    trebleKnob.onValueChange = [this] {
        ampSimGoldRef.setTreble(static_cast<float>(trebleKnob.getValue()));
        onParameterChanged();
    };
    brightnessKnob.onValueChange = [this] {
        ampSimGoldRef.setBrightness(static_cast<float>(brightnessKnob.getValue()));
        onParameterChanged();
    };
    micPositionKnob.onValueChange = [this] {
        ampSimGoldRef.setMicPosition(static_cast<float>(micPositionKnob.getValue()));
        onParameterChanged();
    };

    // Cabinet selector ComboBox
    for (int i = 0; i < AmpSimGold::kNumCabinets; ++i)
        cabinetSelector.addItem(AmpSimGold::getCabinetName(i), i + 1);
    cabinetSelector.addItem("No Cabinet", AmpSimGold::kNoCabinet + 1);

    cabinetSelector.onChange = [this] {
        auto id = cabinetSelector.getSelectedId();
        if (id == AmpSimGold::kNoCabinet + 1)
            ampSimGoldRef.setCabinetType(AmpSimGold::kNoCabinet);
        else if (id > 0 && id <= AmpSimGold::kNumCabinets)
            ampSimGoldRef.setCabinetType(id - 1);
        else if (id == AmpSimGold::kCustomCabinet + 1)
            ampSimGoldRef.loadCustomIR(ampSimGoldRef.getCustomIRFile());

#if JucePlugin_Build_Standalone
        // Clear stale custom IR path when switching to a factory/no cabinet
        if (id != AmpSimGold::kCustomCabinet + 1)
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                if (auto* props = holder->settings.get())
                    props->setValue("customIRPath2", "");
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

                ampSimGoldRef.loadCustomIR(result);
                syncFromDsp();

#if JucePlugin_Build_Standalone
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    if (auto* props = holder->settings.get())
                    {
                        props->setValue("customIRPath2",
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
        ampSimGoldRef.setCabTrim(static_cast<float>(cabTrimSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(cabTrimSlider);

    cabTrimLabel.setText("Cab Trim:", juce::dontSendNotification);
    cabTrimLabel.setFont(Theme::Fonts::small());
    cabTrimLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    cabTrimLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(cabTrimLabel);

    // Preamp Boost toggle
    preampBoostToggle.setButtonText("Pre Boost");
    preampBoostToggle.setLookAndFeel(&bypassLF);
    preampBoostToggle.setClickingTogglesState(false);
    preampBoostToggle.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::ampSim.getARGB()));
    preampBoostToggle.onClick = [this] {
        bool newState = !preampBoostToggle.getToggleState();
        preampBoostToggle.setToggleState(newState, juce::dontSendNotification);
        ampSimGoldRef.setPreampBoost(newState);
        onParameterChanged();
    };
    addAndMakeVisible(preampBoostToggle);

    // Speaker Drive horizontal slider
    speakerDriveSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    speakerDriveSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    speakerDriveSlider.setRange(0.0, 1.0, 0.01);
    speakerDriveSlider.setLookAndFeel(&sliderLF);
    speakerDriveSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    speakerDriveSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    speakerDriveSlider.onValueChange = [this] {
        ampSimGoldRef.setSpeakerDrive(static_cast<float>(speakerDriveSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(speakerDriveSlider);

    speakerDriveLabel.setText("Speaker Drive", juce::dontSendNotification);
    speakerDriveLabel.setFont(Theme::Fonts::small());
    speakerDriveLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    speakerDriveLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(speakerDriveLabel);

    // Presence slider (NFB frequency shaping)
    presenceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    presenceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    presenceSlider.setRange(0.0, 1.0, 0.01);
    presenceSlider.setLookAndFeel(&sliderLF);
    presenceSlider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v * 100.0)) + "%";
    };
    presenceSlider.valueFromTextFunction = [](const juce::String& text) {
        return text.trimCharactersAtEnd("%").getDoubleValue() / 100.0;
    };
    presenceSlider.onValueChange = [this] {
        ampSimGoldRef.setPresence(static_cast<float>(presenceSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(presenceSlider);

    presenceLabel.setText("Presence", juce::dontSendNotification);
    presenceLabel.setFont(Theme::Fonts::small());
    presenceLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    presenceLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(presenceLabel);

    // Bypass LED button
    bypassButton.setButtonText("Amp Gold");
    bypassButton.setToggleState(!ampSimGoldRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::ampSim.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        ampSimGoldRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        ampSimGoldRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    gainKnob.setTooltip("Master input gain - controls how hard you drive the preamp.");
    bassKnob.setTooltip("Low shelf cut at 150 Hz. 100% = flat (no cut).");
    midKnob.setTooltip("Peaking cut at 800 Hz. 100% = flat (no cut).");
    trebleKnob.setTooltip("High shelf cut at 3 kHz. 100% = flat (no cut).");
    brightnessKnob.setTooltip("Brightness - high shelf at 3.5 kHz. Center = neutral, low = dark, high = bright.");
    micPositionKnob.setTooltip("Mic placement - low = close/dark (on-cone), high = bright/airy (off-axis).");
    speakerDriveSlider.setTooltip("Power amp tube saturation. Push-pull topology with global negative feedback.");
    presenceSlider.setTooltip("Presence - shapes power amp feedback. Low = tight/dark (full NFB), high = open/bright (HF freed from NFB).");
    preampBoostToggle.setTooltip("+12 dB preamp boost. Pushes harder into the waveshaper engine.");
    cabinetSelector.setTooltip("Select the speaker cabinet impulse response.");
    cabTrimSlider.setTooltip("Manual cabinet volume trim. Adjusts on top of auto-normalization.");
    loadIRButton.setTooltip("Load a custom cabinet IR (.wav file).");
    bypassButton.setTooltip("Click to toggle Amp Gold bypass.");
    resetButton.setTooltip("Reset all Amp Gold parameters to defaults.");

    // Restore custom IR from settings (if one was saved last session)
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        if (auto* props = holder->settings.get())
        {
            auto savedIR = props->getValue("customIRPath2", "");
            if (savedIR.isNotEmpty() && ampSimGoldRef.getCabinetType() == AmpSimGold::kCustomCabinet)
            {
                juce::File irFile(savedIR);
                if (irFile.existsAsFile())
                    ampSimGoldRef.loadCustomIR(irFile);
            }
        }
#endif

    syncFromDsp();
}

AmpSimGoldPanel::~AmpSimGoldPanel()
{
    gainKnob.setLookAndFeel(nullptr);
    bassKnob.setLookAndFeel(nullptr);
    midKnob.setLookAndFeel(nullptr);
    trebleKnob.setLookAndFeel(nullptr);
    brightnessKnob.setLookAndFeel(nullptr);
    micPositionKnob.setLookAndFeel(nullptr);
    speakerDriveSlider.setLookAndFeel(nullptr);
    presenceSlider.setLookAndFeel(nullptr);
    cabTrimSlider.setLookAndFeel(nullptr);
    cabinetSelector.setLookAndFeel(nullptr);
    loadIRButton.setLookAndFeel(nullptr);
    preampBoostToggle.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void AmpSimGoldPanel::syncFromDsp()
{
    gainKnob.setValue(ampSimGoldRef.getGain(),               juce::dontSendNotification);
    bassKnob.setValue(ampSimGoldRef.getBass(),               juce::dontSendNotification);
    midKnob.setValue(ampSimGoldRef.getMid(),                 juce::dontSendNotification);
    trebleKnob.setValue(ampSimGoldRef.getTreble(),           juce::dontSendNotification);
    brightnessKnob.setValue(ampSimGoldRef.getBrightness(),   juce::dontSendNotification);
    micPositionKnob.setValue(ampSimGoldRef.getMicPosition(), juce::dontSendNotification);
    speakerDriveSlider.setValue(ampSimGoldRef.getSpeakerDrive(), juce::dontSendNotification);
    presenceSlider.setValue(ampSimGoldRef.getPresence(), juce::dontSendNotification);
    cabTrimSlider.setValue(ampSimGoldRef.getCabTrim(), juce::dontSendNotification);
    int cabType = ampSimGoldRef.getCabinetType();
    if (cabType == AmpSimGold::kCustomCabinet)
    {
        auto irName = ampSimGoldRef.getCustomIRFile().getFileNameWithoutExtension();
        int customId = AmpSimGold::kCustomCabinet + 1;

        // 14 IRs + No Cabinet = 15 built-in items
        if (cabinetSelector.getNumItems() <= AmpSimGold::kNumCabinets + 1)
            cabinetSelector.addItem("Custom: " + irName, customId);
        else
            cabinetSelector.changeItemText(customId, "Custom: " + irName);

        cabinetSelector.setSelectedId(customId, juce::dontSendNotification);
    }
    else
    {
        cabinetSelector.setSelectedId(cabType + 1, juce::dontSendNotification);
    }
    preampBoostToggle.setToggleState(ampSimGoldRef.getPreampBoost(), juce::dontSendNotification);
    bypassButton.setToggleState(!ampSimGoldRef.isBypassed(), juce::dontSendNotification);
}

void AmpSimGoldPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void AmpSimGoldPanel::resized()
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
    // Split into left column (Gain/Bass/Mid) and right column (Treble/Bright/Mic)
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
    // Right column: Treble / Brightness / Mic Pos sliders
    //--------------------------------------------------------------------------
    auto rightSliderArea = rightCol.removeFromTop(cellHeight);

    juce::Slider* rightSliders[] = { &trebleKnob, &brightnessKnob, &micPositionKnob };
    juce::Label*  rightLabels[]  = { &trebleLabel, &brightnessLabel, &micPositionLabel };

    for (int i = 0; i < 3; ++i)
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

    // Preamp Boost + Speaker Drive side by side
    auto driveRow = belowArea.removeFromTop(30);
    preampBoostToggle.setBounds(driveRow.removeFromLeft(130));
    driveRow.removeFromLeft(16);
    speakerDriveLabel.setBounds(driveRow.removeFromLeft(90));
    speakerDriveSlider.setBounds(driveRow);

    belowArea.removeFromTop(4);
    auto presRow = belowArea.removeFromTop(26);
    presenceLabel.setBounds(presRow.removeFromLeft(60));
    presenceSlider.setBounds(presRow);
}

void AmpSimGoldPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
