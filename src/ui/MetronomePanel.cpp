#include "MetronomePanel.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <numeric>

MetronomePanel::MetronomePanel(MetronomeEngine& engine)
    : metronome(engine)
{
    // BPM display
    bpmDisplay.setFont(juce::Font(juce::FontOptions(Theme::Fonts::getInterBold()).withHeight(52.0f)));
    bpmDisplay.setColour(juce::Label::textColourId, Theme::Colours::textPrimary);
    bpmDisplay.setJustificationType(juce::Justification::centred);
    bpmDisplay.setText(juce::String(metronome.getBpm()), juce::dontSendNotification);
    addAndMakeVisible(bpmDisplay);

    // BPM stepper buttons
    bpmDownButton.setButtonText("-");
    bpmDownButton.setLookAndFeel(&actionLF);
    bpmDownButton.onClick = [this] {
        int newBpm = juce::jlimit(30, 300, metronome.getBpm() - 1);
        metronome.setBpm(newBpm);
        updateBpmDisplay();
    };
    addAndMakeVisible(bpmDownButton);

    bpmUpButton.setButtonText("+");
    bpmUpButton.setLookAndFeel(&actionLF);
    bpmUpButton.onClick = [this] {
        int newBpm = juce::jlimit(30, 300, metronome.getBpm() + 1);
        metronome.setBpm(newBpm);
        updateBpmDisplay();
    };
    addAndMakeVisible(bpmUpButton);

    // Hold-to-repeat on the steppers, accelerating once held.
    bpmDownButton.setRepeatSpeed(400, 100, 30);
    bpmUpButton.setRepeatSpeed(400, 100, 30);

    // Quick-set chips for common practice tempos.
    for (int bpm : { 60, 80, 100, 120, 140, 160, 180 })
    {
        auto* chip = presetChips.add(new juce::TextButton(juce::String(bpm)));
        chip->setLookAndFeel(&actionLF);
        chip->onClick = [this, bpm] {
            metronome.setBpm(bpm);
            updateBpmDisplay();
        };
        addAndMakeVisible(chip);
    }

    // Start/Stop
    startStopButton.setButtonText("Metronome");
    startStopButton.setLookAndFeel(&bypassLF);
    startStopButton.setClickingTogglesState(false);
    startStopButton.getProperties().set("ledColour",
        static_cast<juce::int64>(Theme::Colours::goldText.getARGB()));
    startStopButton.getProperties().set("panelHeader", true);
    startStopButton.getProperties().set("onText", "RUNNING");
    startStopButton.getProperties().set("offText", "STOPPED");
    startStopButton.onClick = [this] {
        bool nowRunning = !metronome.isRunning();
        metronome.setRunning(nowRunning);
        startStopButton.setToggleState(nowRunning, juce::dontSendNotification);
    };
    addAndMakeVisible(startStopButton);

    // Tap tempo
    tapButton.setButtonText("Tap");
    tapButton.setLookAndFeel(&actionLF);
    tapButton.onClick = [this] { handleTap(); };
    addAndMakeVisible(tapButton);

    // Time signature selector
    timeSigLabel.setText("Time Sig", juce::dontSendNotification);
    timeSigLabel.setFont(Theme::Fonts::small());
    timeSigLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    timeSigLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeSigLabel);

    timeSigSelector.addItem("2/4", 1);
    timeSigSelector.addItem("3/4", 2);
    timeSigSelector.addItem("4/4", 3);
    timeSigSelector.addItem("6/8", 4);

    // Default to 4/4 (item 3)
    int beatsNow = metronome.getBeatsPerBar();
    if (beatsNow == 2) timeSigSelector.setSelectedId(1, juce::dontSendNotification);
    else if (beatsNow == 3) timeSigSelector.setSelectedId(2, juce::dontSendNotification);
    else if (beatsNow == 6) timeSigSelector.setSelectedId(4, juce::dontSendNotification);
    else timeSigSelector.setSelectedId(3, juce::dontSendNotification);

    timeSigSelector.onChange = [this] { applyTimeSigSelection(); };
    timeSigSelector.setLookAndFeel(&sliderLF);
    addAndMakeVisible(timeSigSelector);

    // Volume
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setFont(Theme::Fonts::small());
    volumeLabel.setColour(juce::Label::textColourId, Theme::Colours::textSecondary);
    volumeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(volumeLabel);

    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setRange(0.0, 1.0, 0.001);
    // Square-law taper: a linear gain fader crams the audible range into the
    // bottom third of the travel; this spreads it evenly.
    volumeSlider.setSkewFactor(0.5);
    volumeSlider.setValue(static_cast<double>(metronome.getVolume()), juce::dontSendNotification);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setLookAndFeel(&sliderLF);
    volumeSlider.onValueChange = [this] {
        metronome.setVolume(static_cast<float>(volumeSlider.getValue()));
    };
    addAndMakeVisible(volumeSlider);

    startTimerHz(10);
}

MetronomePanel::~MetronomePanel()
{
    stopTimer();
    bpmDownButton.setLookAndFeel(nullptr);
    bpmUpButton.setLookAndFeel(nullptr);
    for (auto* chip : presetChips)
        chip->setLookAndFeel(nullptr);
    startStopButton.setLookAndFeel(nullptr);
    tapButton.setLookAndFeel(nullptr);
    timeSigSelector.setLookAndFeel(nullptr);
    volumeSlider.setLookAndFeel(nullptr);
}

void MetronomePanel::timerCallback()
{
    // Sync the pilot lamp with engine state.
    bool running = metronome.isRunning();
    if (startStopButton.getToggleState() != running)
        startStopButton.setToggleState(running, juce::dontSendNotification);
}

void MetronomePanel::updateBpmDisplay()
{
    bpmDisplay.setText(juce::String(metronome.getBpm()), juce::dontSendNotification);
}

void MetronomePanel::handleTap()
{
    double now = juce::Time::getMillisecondCounterHiRes();

    // Reset tap history if more than 2 seconds since last tap.
    if (tapCount > 0 && (now - lastTapTime) > 2000.0)
        tapCount = 0;

    tapTimes[tapCount % kTapHistorySize] = now;
    ++tapCount;
    lastTapTime = now;

    if (tapCount < 2)
        return;  // need at least one interval

    // Median of the most recent intervals. tapTimes holds the last 4 taps, so at
    // most 3 consecutive intervals are valid -- a 4th would span the slot the
    // newest tap just overwrote, yielding a garbage (negative) interval.
    int available = juce::jmin(tapCount - 1, kTapHistorySize - 1);
    double intervals[kTapHistorySize];
    for (int i = 0; i < available; ++i)
    {
        // Circular indices for tap (tapCount-1-i) and (tapCount-2-i)
        int idxB = ((tapCount - 1 - i) % kTapHistorySize + kTapHistorySize) % kTapHistorySize;
        int idxA = ((tapCount - 2 - i) % kTapHistorySize + kTapHistorySize) % kTapHistorySize;
        intervals[i] = tapTimes[idxB] - tapTimes[idxA];
    }

    // Median of available intervals (sort then pick middle).
    std::sort(intervals, intervals + available);
    double medianInterval = (available % 2 == 0)
        ? (intervals[available / 2 - 1] + intervals[available / 2]) * 0.5
        : intervals[available / 2];

    if (medianInterval > 10.0)  // sanity: ignore sub-10 ms intervals
    {
        int newBpm = juce::jlimit(30, 300, static_cast<int>(std::round(60000.0 / medianInterval)));
        metronome.setBpm(newBpm);
        updateBpmDisplay();
    }
}

void MetronomePanel::applyTimeSigSelection()
{
    // Time signature change takes effect at the next beat boundary.
    // The engine reads atomicBeats at the top of each block, so the new
    // value lands cleanly on the next blockBeats cache update.
    int id = timeSigSelector.getSelectedId();
    switch (id)
    {
        case 1: metronome.setBeatsPerBar(2); break;
        case 2: metronome.setBeatsPerBar(3); break;
        case 3: metronome.setBeatsPerBar(4); break;
        case 4: metronome.setBeatsPerBar(6); break;
        default: break;
    }
}

void MetronomePanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());

    // Section label: "BPM"
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding).toFloat();
    g.setFont(Theme::Fonts::small());
    g.setColour(Theme::Colours::textSecondary);
    g.drawText("BPM", area.removeFromTop(16.0f), juce::Justification::centred);
}

void MetronomePanel::resized()
{
    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding);

    // "BPM" label row is painted in paint(), leave 16 px gap
    area.removeFromTop(16);

    //--------------------------------------------------------------------------
    // BPM row: [-] [big number] [+]
    //--------------------------------------------------------------------------
    auto bpmRow = area.removeFromTop(70);
    bpmDownButton.setBounds(bpmRow.removeFromLeft(50).withSizeKeepingCentre(44, 50));
    bpmUpButton.setBounds(bpmRow.removeFromRight(50).withSizeKeepingCentre(44, 50));
    bpmDisplay.setBounds(bpmRow);

    area.removeFromTop(8);

    //--------------------------------------------------------------------------
    // Quick-set tempo chips
    //--------------------------------------------------------------------------
    auto chipRow = area.removeFromTop(26);
    const int chipW = chipRow.getWidth() / juce::jmax(1, presetChips.size());
    for (auto* chip : presetChips)
        chip->setBounds(chipRow.removeFromLeft(chipW).reduced(3, 0));

    area.removeFromTop(12);

    //--------------------------------------------------------------------------
    // Start/Stop + Tap row
    //--------------------------------------------------------------------------
    auto controlRow = area.removeFromTop(40);
    startStopButton.setBounds(controlRow.removeFromLeft(controlRow.getWidth() / 2).reduced(4, 0));
    tapButton.setBounds(controlRow.reduced(4, 0));

    area.removeFromTop(16);

    //--------------------------------------------------------------------------
    // Time signature row
    //--------------------------------------------------------------------------
    auto timeSigRow = area.removeFromTop(42);
    timeSigLabel.setBounds(timeSigRow.removeFromTop(16));
    // Center the combo box
    timeSigSelector.setBounds(timeSigRow.withSizeKeepingCentre(
        juce::jmin(160, timeSigRow.getWidth()), 26));

    area.removeFromTop(16);

    //--------------------------------------------------------------------------
    // Volume row
    //--------------------------------------------------------------------------
    auto volRow = area.removeFromTop(42);
    volumeLabel.setBounds(volRow.removeFromTop(16));
    volumeSlider.setBounds(volRow.reduced(8, 0));
}

//==============================================================================
// ActionButtonLF
//==============================================================================
void MetronomePanel::ActionButtonLF::drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                                                          const juce::Colour&, bool over, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour body = bodyColour;
    if (down) body = body.darker(0.2f);
    else if (over) body = body.brighter(0.1f);

    g.setColour(body);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Subtle bevel
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 0.5f);
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawLine(bounds.getX() + 4.0f, bounds.getY() + 1.0f,
               bounds.getRight() - 4.0f, bounds.getY() + 1.0f, 0.5f);
}

void MetronomePanel::ActionButtonLF::drawButtonText(juce::Graphics& g, juce::TextButton& btn,
                                                    bool /*over*/, bool down)
{
    auto bounds = btn.getLocalBounds().toFloat();
    if (down) bounds.translate(0.0f, 0.5f);
    g.setColour(textColour);
    g.setFont(juce::Font(juce::FontOptions(Theme::Fonts::getInterSemiBold()).withHeight(fontSize)));
    g.drawText(btn.getButtonText(), bounds, juce::Justification::centred, false);
}

