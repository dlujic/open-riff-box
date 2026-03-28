#include "AmpSimSilverPanel.h"
#include "Theme.h"
#include "dsp/AmpSimSilver.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

AmpSimSilverPanel::AmpSimSilverPanel(AmpSimSilver& ampSimSilver)
    : ampSimSilverRef(ampSimSilver)
{
    setupKnob(gainKnob,   gainLabel,   "Gain",   0.0, 1.0, 0.01);
    setupKnob(bassKnob,   bassLabel,   "Bass",   0.0, 1.0, 0.01);
    setupKnob(midKnob,    midLabel,    "Mid",    0.0, 1.0, 0.01);
    setupKnob(trebleKnob, trebleLabel, "Treble", 0.0, 1.0, 0.01);
    setupKnob(brightnessKnob,  brightnessLabel,  "Brightness", 0.0, 1.0, 0.01);
    setupKnob(micPositionKnob, micPositionLabel, "Mic",        0.0, 1.0, 0.01);

    // Bass/Mid/Treble: show ±12 dB (0.5 = 0 dB flat)
    auto eqDisplay = [](double v) {
        double db = (v - 0.5) * 24.0;
        if (std::abs(db) < 0.5) return juce::String("0 dB");
        return juce::String(db, 1) + " dB";
    };
    auto eqFromText = [](const juce::String& text) {
        return text.trimCharactersAtEnd(" dB").getDoubleValue() / 24.0 + 0.5;
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
        ampSimSilverRef.setGain(static_cast<float>(gainKnob.getValue()));
        onParameterChanged();
    };
    bassKnob.onValueChange = [this] {
        ampSimSilverRef.setBass(static_cast<float>(bassKnob.getValue()));
        onParameterChanged();
    };
    midKnob.onValueChange = [this] {
        ampSimSilverRef.setMid(static_cast<float>(midKnob.getValue()));
        onParameterChanged();
    };
    trebleKnob.onValueChange = [this] {
        ampSimSilverRef.setTreble(static_cast<float>(trebleKnob.getValue()));
        onParameterChanged();
    };
    brightnessKnob.onValueChange = [this] {
        ampSimSilverRef.setBrightness(static_cast<float>(brightnessKnob.getValue()));
        onParameterChanged();
    };
    micPositionKnob.onValueChange = [this] {
        ampSimSilverRef.setMicPosition(static_cast<float>(micPositionKnob.getValue()));
        onParameterChanged();
    };

    // Cabinet selector ComboBox
    for (int i = 0; i < AmpSimSilver::kNumCabinets; ++i)
        cabinetSelector.addItem(AmpSimSilver::getCabinetName(i), i + 1);
    cabinetSelector.addItem("No Cabinet", AmpSimSilver::kNoCabinet + 1);

    cabinetSelector.onChange = [this] {
        auto id = cabinetSelector.getSelectedId();
        if (id == AmpSimSilver::kNoCabinet + 1)
            ampSimSilverRef.setCabinetType(AmpSimSilver::kNoCabinet);
        else if (id > 0 && id <= AmpSimSilver::kNumCabinets)
            ampSimSilverRef.setCabinetType(id - 1);
        // Custom item re-selected: reload from stored file
        else if (id == AmpSimSilver::kCustomCabinet + 1)
            ampSimSilverRef.loadCustomIR(ampSimSilverRef.getCustomIRFile());

#if JucePlugin_Build_Standalone
        // Clear stale custom IR path when switching to a factory/no cabinet
        if (id != AmpSimSilver::kCustomCabinet + 1)
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                if (auto* props = holder->settings.get())
                    props->setValue("customIRPath", "");
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
        // Determine default browse directory
        auto exeDir = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile).getParentDirectory();
        auto defaultDir = exeDir.getChildFile("custom-irs");

        // Prefer last-used directory from settings
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

                ampSimSilverRef.loadCustomIR(result);
                syncFromDsp();

#if JucePlugin_Build_Standalone
                if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    if (auto* props = holder->settings.get())
                    {
                        props->setValue("customIRPath",
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
        ampSimSilverRef.setCabTrim(static_cast<float>(cabTrimSlider.getValue()));
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
        ampSimSilverRef.setPreampBoost(newState);
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
        ampSimSilverRef.setSpeakerDrive(static_cast<float>(speakerDriveSlider.getValue()));
        onParameterChanged();
    };
    addAndMakeVisible(speakerDriveSlider);

    speakerDriveLabel.setText("Speaker Drive", juce::dontSendNotification);
    speakerDriveLabel.setFont(Theme::Fonts::small());
    speakerDriveLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    speakerDriveLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(speakerDriveLabel);

    // Bypass LED button
    bypassButton.setButtonText("Amp Silver");
    bypassButton.setToggleState(!ampSimSilverRef.isBypassed(), juce::dontSendNotification);
    bypassButton.setLookAndFeel(&bypassLF);
    bypassButton.setClickingTogglesState(false);
    bypassButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::ampSim.getARGB()));
    bypassButton.getProperties().set("panelHeader", true);
    bypassButton.onClick = [this] {
        bool newState = !bypassButton.getToggleState();
        bypassButton.setToggleState(newState, juce::dontSendNotification);
        ampSimSilverRef.setBypassed(!newState);
        onBypassToggled();
    };
    addAndMakeVisible(bypassButton);

    resetButton.setButtonText("Reset");
    resetButton.setLookAndFeel(&resetLF);
    resetButton.onClick = [this] {
        ampSimSilverRef.resetToDefaults();
        syncFromDsp();
    };
    addAndMakeVisible(resetButton);

    // Tooltips
    gainKnob.setTooltip("Master input gain - controls how hard you drive the amp.");
    bassKnob.setTooltip("Low shelf EQ at 150 Hz. 50% = flat.");
    midKnob.setTooltip("Peaking EQ at 800 Hz. 50% = flat.");
    trebleKnob.setTooltip("High shelf EQ at 3 kHz. 50% = flat.");
    brightnessKnob.setTooltip("Brightness - high shelf at 3.5 kHz. Center = neutral, low = dark, high = bright.");
    micPositionKnob.setTooltip("Mic placement - low = close/dark (on-cone), high = bright/airy (off-axis).");
    speakerDriveSlider.setTooltip("Power amp and speaker cone distortion. Adds warmth and even harmonics.");
    preampBoostToggle.setTooltip("+12 dB preamp boost with tanh saturation. Pushes into breakup.");
    cabinetSelector.setTooltip("Select the speaker cabinet impulse response.");
    cabTrimSlider.setTooltip("Manual cabinet volume trim. Adjusts on top of auto-normalization.");
    loadIRButton.setTooltip("Load a custom cabinet IR (.wav file).");
    bypassButton.setTooltip("Click to toggle Amp Silver bypass.");
    resetButton.setTooltip("Reset all Amp Silver parameters to defaults.");

    // Restore custom IR from settings (if one was saved last session)
#if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        if (auto* props = holder->settings.get())
        {
            auto savedIR = props->getValue("customIRPath", "");
            if (savedIR.isNotEmpty() && ampSimSilverRef.getCabinetType() == AmpSimSilver::kCustomCabinet)
            {
                juce::File irFile(savedIR);
                if (irFile.existsAsFile())
                    ampSimSilverRef.loadCustomIR(irFile);
            }
        }
#endif

    syncFromDsp();
}

AmpSimSilverPanel::~AmpSimSilverPanel()
{
    gainKnob.setLookAndFeel(nullptr);
    bassKnob.setLookAndFeel(nullptr);
    midKnob.setLookAndFeel(nullptr);
    trebleKnob.setLookAndFeel(nullptr);
    brightnessKnob.setLookAndFeel(nullptr);
    micPositionKnob.setLookAndFeel(nullptr);
    speakerDriveSlider.setLookAndFeel(nullptr);
    cabTrimSlider.setLookAndFeel(nullptr);
    cabinetSelector.setLookAndFeel(nullptr);
    loadIRButton.setLookAndFeel(nullptr);
    preampBoostToggle.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    resetButton.setLookAndFeel(nullptr);
}

void AmpSimSilverPanel::syncFromDsp()
{
    gainKnob.setValue(ampSimSilverRef.getGain(),               juce::dontSendNotification);
    bassKnob.setValue(ampSimSilverRef.getBass(),               juce::dontSendNotification);
    midKnob.setValue(ampSimSilverRef.getMid(),                 juce::dontSendNotification);
    trebleKnob.setValue(ampSimSilverRef.getTreble(),           juce::dontSendNotification);
    brightnessKnob.setValue(ampSimSilverRef.getBrightness(),   juce::dontSendNotification);
    micPositionKnob.setValue(ampSimSilverRef.getMicPosition(), juce::dontSendNotification);
    speakerDriveSlider.setValue(ampSimSilverRef.getSpeakerDrive(), juce::dontSendNotification);
    cabTrimSlider.setValue(ampSimSilverRef.getCabTrim(), juce::dontSendNotification);
    int cabType = ampSimSilverRef.getCabinetType();
    if (cabType == AmpSimSilver::kCustomCabinet)
    {
        // Show "Custom: filename" in the combo box
        auto irName = ampSimSilverRef.getCustomIRFile().getFileNameWithoutExtension();
        int customId = AmpSimSilver::kCustomCabinet + 1;

        // Update or add the custom item (14 IRs + No Cabinet = 15 built-in items)
        if (cabinetSelector.getNumItems() <= AmpSimSilver::kNumCabinets + 1)
            cabinetSelector.addItem("Custom: " + irName, customId);
        else
            cabinetSelector.changeItemText(customId, "Custom: " + irName);

        cabinetSelector.setSelectedId(customId, juce::dontSendNotification);
    }
    else
    {
        cabinetSelector.setSelectedId(cabType + 1, juce::dontSendNotification);
    }
    preampBoostToggle.setToggleState(ampSimSilverRef.getPreampBoost(), juce::dontSendNotification);
    bypassButton.setToggleState(!ampSimSilverRef.isBypassed(), juce::dontSendNotification);
}

void AmpSimSilverPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());
    Theme::paintHeaderGroove(g, getLocalBounds());
}

void AmpSimSilverPanel::resized()
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
    area.removeFromLeft(16);  // gap between columns
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
    // Recombine columns for full-width layout below the sliders
    auto belowArea = getLocalBounds().reduced(Theme::Dims::panelPadding);
    belowArea.removeFromTop(38 + 16 + cellHeight);  // skip top row + gap + sliders

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
}

void AmpSimSilverPanel::setupKnob(juce::Slider& knob, juce::Label& label,
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
