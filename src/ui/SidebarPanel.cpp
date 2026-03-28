#include "SidebarPanel.h"
#include "Theme.h"
#include "PluginProcessor.h"

SidebarPanel::SidebarPanel(OpenRiffBoxProcessor& processor)
    : processorRef(processor)
{
    powerButton.setButtonText("");
    powerButton.setLookAndFeel(&powerLF);
    powerButton.onClick = [this] {
        processorRef.setAudioActive(!processorRef.isAudioActive());
        powerLF.audioActive = processorRef.isAudioActive();
        powerButton.repaint();
    };
    addAndMakeVisible(powerButton);

    powerLF.audioActive = processorRef.isAudioActive();

    // Limiter toggle
    limiterButton.setButtonText("");
    limiterButton.setLookAndFeel(&limiterLF);
    limiterButton.setTooltip("Output limiter - prevents harsh digital clipping.\n"
                             "LED flashes amber when the signal is being limited.");
    limiterButton.onClick = [this] {
        processorRef.setLimiterEnabled(!processorRef.isLimiterEnabled());
        limiterLF.enabled = processorRef.isLimiterEnabled();
        limiterButton.repaint();
    };
    addAndMakeVisible(limiterButton);

    limiterLF.enabled = processorRef.isLimiterEnabled();

    // Master volume knob
    masterKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    masterKnob.setRange(0.0, 1.0, 0.01);
    masterKnob.setValue(processorRef.getMasterVolume(), juce::dontSendNotification);
    masterKnob.setLookAndFeel(&masterKnobLF);
    masterKnob.setTooltip("Master volume - final output level before limiter");
    masterKnob.onValueChange = [this] {
        processorRef.setMasterVolume(static_cast<float>(masterKnob.getValue()));
    };
    addAndMakeVisible(masterKnob);

    // Start timer for meter updates (~30fps)
    startTimerHz(30);
}

SidebarPanel::~SidebarPanel()
{
    powerButton.setLookAndFeel(nullptr);
    limiterButton.setLookAndFeel(nullptr);
    masterKnob.setLookAndFeel(nullptr);
}

void SidebarPanel::timerCallback()
{
    // Read peak levels from processor
    float peakIn  = processorRef.getInputPeak();
    float peakOut = processorRef.getOutputPeak();

    // Ballistic smoothing: fast attack, slow release
    const float attackCoeff = 0.8f;
    const float releaseCoeff = 0.92f;

    displayLevelIn = (peakIn > displayLevelIn)
        ? peakIn * attackCoeff + displayLevelIn * (1.0f - attackCoeff)
        : displayLevelIn * releaseCoeff;

    displayLevelOut = (peakOut > displayLevelOut)
        ? peakOut * attackCoeff + displayLevelOut * (1.0f - attackCoeff)
        : displayLevelOut * releaseCoeff;

    // Sync power button state (in case toggled externally)
    powerLF.audioActive = processorRef.isAudioActive();

    // Sync limiter state
    limiterLF.enabled  = processorRef.isLimiterEnabled();
    limiterLF.clipping = processorRef.isLimiterClipping();
    limiterFlash       = limiterLF.clipping;

    // CPU load smoothing: fast attack, slow release
    float rawCpu = processorRef.getCpuLoad();
    const float cpuAttack  = 0.3f;
    const float cpuRelease = 0.95f;
    displayCpuLoad = (rawCpu > displayCpuLoad)
        ? rawCpu * cpuAttack + displayCpuLoad * (1.0f - cpuAttack)
        : displayCpuLoad * cpuRelease;

    repaint();
}

void SidebarPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());

    // Groove seam on left edge — matches chain list right-edge style
    auto bounds = getLocalBounds();
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawVerticalLine(bounds.getX(), static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawVerticalLine(bounds.getX() + 1, static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));

    // "POWER" label above rocker switch
    {
        auto area = getLocalBounds().reduced(10, 12);
        g.setColour(Theme::Colours::textSecondary);
        g.setFont(Theme::Fonts::small());
        g.drawText("POWER", area.removeFromTop(14), juce::Justification::centred);
    }

    paintJewel(g);

    // "LEVEL" label above meter
    if (!meterArea.isEmpty())
    {
        g.setColour(Theme::Colours::textSecondary);
        g.setFont(Theme::Fonts::small());
        g.drawText("LEVEL", meterArea.getX(), meterArea.getY() - 16,
                   meterArea.getWidth(), 14, juce::Justification::centred);
    }

    paintMeter(g);

    // "MASTER" label above knob
    {
        auto knobBounds = masterKnob.getBounds();
        g.setColour(Theme::Colours::textSecondary);
        g.setFont(Theme::Fonts::small());
        g.drawText("MASTER", knobBounds.getX(), knobBounds.getBottom() + 2,
                   knobBounds.getWidth(), 12, juce::Justification::centred);
    }

    paintCpuMeter(g);
}

void SidebarPanel::paintMeter(juce::Graphics& g)
{
    if (meterArea.isEmpty()) return;

    const int barGap = 4;
    const int barW = (meterArea.getWidth() - barGap) / 2;
    const int meterH = meterArea.getHeight();

    auto leftWell = juce::Rectangle<int>(meterArea.getX(), meterArea.getY(), barW, meterH);
    auto rightWell = juce::Rectangle<int>(meterArea.getX() + barW + barGap, meterArea.getY(), barW, meterH);

    // Dark recessed wells
    g.setColour(juce::Colour(0xff0e0d0a));
    g.fillRect(leftWell);
    g.fillRect(rightWell);

    // Subtle inset border
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRect(leftWell);
    g.drawRect(rightWell);

    // Segmented LED meter
    const int ledH   = 3;   // LED segment height
    const int ledGap = 2;   // gap between segments
    const int ledStep = ledH + ledGap;
    const int numLeds = meterH / ledStep;

    auto paintBar = [&](juce::Rectangle<int> well, float level)
    {
        float clamped = juce::jlimit(0.0f, 1.0f, level);
        float displayVal = std::pow(clamped, 0.5f);
        int litCount = juce::roundToInt(displayVal * static_cast<float>(numLeds));

        for (int i = 0; i < numLeds; ++i)
        {
            // LEDs from bottom (i=0) to top (i=numLeds-1)
            int y = well.getBottom() - (i + 1) * ledStep + ledGap;
            auto ledRect = juce::Rectangle<int>(
                well.getX() + 1, y, well.getWidth() - 2, ledH);

            float pos = static_cast<float>(i) / static_cast<float>(numLeds - 1);

            if (i < litCount)
            {
                // Lit LED — signal color with warm amber tint
                // Green at bottom → amber in middle → red at top
                juce::Colour signalColour;
                if (pos < 0.55f)
                    signalColour = juce::Colour(0xff44aa33).interpolatedWith(
                        juce::Colour(0xffbb9922), pos / 0.55f);
                else
                    signalColour = juce::Colour(0xffbb9922).interpolatedWith(
                        juce::Colour(0xffcc3322), (pos - 0.55f) / 0.45f);

                // Warm amber tint blended in
                signalColour = signalColour.interpolatedWith(
                    juce::Colour(0xffcc9944), 0.2f);

                g.setColour(signalColour);
                g.fillRect(ledRect);

                // Subtle bright center highlight for LED glow effect
                g.setColour(signalColour.brighter(0.3f).withAlpha(0.5f));
                g.fillRect(ledRect.reduced(1, 0));
            }
            else
            {
                // Unlit LED — faint dark amber, barely visible
                g.setColour(juce::Colour(0xff1a1610));
                g.fillRect(ledRect);
            }
        }
    };

    paintBar(leftWell, displayLevelIn);
    paintBar(rightWell, displayLevelOut);

    // IN/OUT labels below meter
    g.setColour(Theme::Colours::textSecondary);
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("IN", meterArea.getX(), meterArea.getBottom() + 2, barW, 12, juce::Justification::centred);
    g.drawText("OUT", meterArea.getX() + barW + barGap, meterArea.getBottom() + 2, barW, 12, juce::Justification::centred);
}

void SidebarPanel::resized()
{
    auto area = getLocalBounds().reduced(10, 12);

    // "POWER" label above the switch (painted in paint())
    area.removeFromTop(14);

    // Rocker switch — wide rectangular toggle
    auto btnArea = area.removeFromTop(28);
    int btnW = juce::jmin(area.getWidth(), 52);
    powerButton.setBounds(btnArea.withSizeKeepingCentre(btnW, 26));

    area.removeFromTop(10); // gap

    // Jewel indicator light below the switch
    jewelArea = area.removeFromTop(14).withSizeKeepingCentre(14, 14);

    area.removeFromTop(16); // gap

    // "LEVEL" label takes 16px (painted in paint())
    area.removeFromTop(16);

    // CPU meter at the very bottom
    cpuMeterArea = area.removeFromBottom(20);
    area.removeFromBottom(8);  // gap before CPU

    // Limiter toggle
    auto limiterArea = area.removeFromBottom(22);
    area.removeFromBottom(4);  // gap above limiter

    // Master volume knob
    area.removeFromBottom(14); // "MASTER" label (painted in paint())
    int knobSize = juce::jmin(area.getWidth(), 44);
    auto knobArea = area.removeFromBottom(knobSize);
    masterKnob.setBounds(knobArea.withSizeKeepingCentre(knobSize, knobSize));
    area.removeFromBottom(4);  // gap above master knob

    area.removeFromBottom(16); // space for IN/OUT labels

    // Meter fills remaining height
    meterArea = area;

    // Center the limiter button
    limiterButton.setBounds(limiterArea.withSizeKeepingCentre(
        juce::jmin(limiterArea.getWidth(), 56), 20));
}

// Jewel indicator light — Fender-style red lens
void SidebarPanel::paintJewel(juce::Graphics& g)
{
    if (jewelArea.isEmpty()) return;

    auto jf = jewelArea.toFloat();
    float cx = jf.getCentreX();
    float cy = jf.getCentreY();
    float r = jf.getWidth() * 0.5f;

    // Chrome bezel ring
    juce::ColourGradient bezelGrad(
        juce::Colour(0xff888880), cx, cy - r,
        juce::Colour(0xff3a3830), cx, cy + r, false);
    g.setGradientFill(bezelGrad);
    g.fillEllipse(jf);

    // Inner jewel lens (slightly inset)
    auto lens = jf.reduced(2.0f);
    float lr = lens.getWidth() * 0.5f;

    if (powerLF.audioActive)
    {
        // Outer glow
        for (int ring = 3; ring >= 1; --ring)
        {
            float spread = r + static_cast<float>(ring) * 4.0f;
            float alpha = 0.08f / static_cast<float>(ring);
            g.setColour(juce::Colour(0xffdd3333).withAlpha(alpha));
            g.fillEllipse(cx - spread, cy - spread, spread * 2.0f, spread * 2.0f);
        }

        // Glowing red jewel
        juce::ColourGradient jewGrad(
            juce::Colour(0xffff4444), cx, cy - lr,
            juce::Colour(0xffbb2222), cx, cy + lr, false);
        g.setGradientFill(jewGrad);
        g.fillEllipse(lens);

        // Specular dot
        g.setColour(juce::Colour(0x66ffffff));
        g.fillEllipse(cx - lr * 0.25f, cy - lr * 0.5f, lr * 0.5f, lr * 0.35f);
    }
    else
    {
        // Dark ruby — unlit
        juce::ColourGradient jewGrad(
            juce::Colour(0xff3a1818), cx, cy - lr,
            juce::Colour(0xff1e0c0c), cx, cy + lr, false);
        g.setGradientFill(jewGrad);
        g.fillEllipse(lens);

        // Faint sheen
        g.setColour(juce::Colour(0x0cffffff));
        g.fillEllipse(cx - lr * 0.25f, cy - lr * 0.5f, lr * 0.5f, lr * 0.35f);
    }

    // Bezel border
    g.setColour(juce::Colour(0xff1a1814));
    g.drawEllipse(jf, 1.0f);
}

// Power button LookAndFeel -- Fender-style rocker switch
void SidebarPanel::PowerButtonLF::drawButtonBackground(
    juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool isOver, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

    // Chrome bezel (outer rounded rect)
    juce::ColourGradient bezelGrad(
        juce::Colour(0xff787870), bounds.getX(), bounds.getY(),
        juce::Colour(0xff2e2c24), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bezelGrad);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Switch body (inset)
    auto body = bounds.reduced(2.5f);

    if (audioActive)
    {
        // "On" position — slight tilt: brighter top, darker bottom
        juce::ColourGradient bodyGrad(
            juce::Colour(0xff4a4840), body.getX(), body.getY(),
            juce::Colour(0xff1e1c16), body.getX(), body.getBottom(), false);
        if (isDown)
            bodyGrad = juce::ColourGradient(
                juce::Colour(0xff3a3830), body.getX(), body.getY(),
                juce::Colour(0xff161410), body.getX(), body.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(body, 2.5f);

        // Rocker ridge line (top third — pressed down on top)
        float ridgeY = body.getY() + body.getHeight() * 0.35f;
        g.setColour(juce::Colour(0x20ffffff));
        g.drawHorizontalLine(juce::roundToInt(ridgeY),
                             body.getX() + 4.0f, body.getRight() - 4.0f);
        g.setColour(juce::Colour(0x18000000));
        g.drawHorizontalLine(juce::roundToInt(ridgeY) + 1,
                             body.getX() + 4.0f, body.getRight() - 4.0f);
    }
    else
    {
        // "Off" position — reversed tilt: darker top, lighter bottom
        juce::ColourGradient bodyGrad(
            juce::Colour(0xff1e1c16), body.getX(), body.getY(),
            juce::Colour(0xff4a4840), body.getX(), body.getBottom(), false);
        if (isOver)
            bodyGrad = juce::ColourGradient(
                juce::Colour(0xff262420), body.getX(), body.getY(),
                juce::Colour(0xff524e44), body.getX(), body.getBottom(), false);
        g.setGradientFill(bodyGrad);
        g.fillRoundedRectangle(body, 2.5f);

        // Rocker ridge line (bottom third — pressed down on bottom)
        float ridgeY = body.getY() + body.getHeight() * 0.65f;
        g.setColour(juce::Colour(0x20ffffff));
        g.drawHorizontalLine(juce::roundToInt(ridgeY),
                             body.getX() + 4.0f, body.getRight() - 4.0f);
        g.setColour(juce::Colour(0x18000000));
        g.drawHorizontalLine(juce::roundToInt(ridgeY) + 1,
                             body.getX() + 4.0f, body.getRight() - 4.0f);
    }

    // Inset shadow on the bezel
    g.setColour(juce::Colour(0xff1a1814));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

// Limiter button LookAndFeel — small LED + "LIMIT" text
void SidebarPanel::LimiterButtonLF::drawButtonBackground(
    juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool isOver, bool /*isDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();

    // Subtle recessed background
    g.setColour(juce::Colour(isOver ? 0xff2a2820 : 0xff1e1c16));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour(0x30000000));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    // LED dot (left side)
    float ledSize = 6.0f;
    float ledX = bounds.getX() + 6.0f;
    float ledY = bounds.getCentreY() - ledSize * 0.5f;

    if (enabled && clipping)
    {
        // Actively clipping — amber/red flash
        g.setColour(juce::Colour(0xffee6622));
        g.fillEllipse(ledX - 2.0f, ledY - 2.0f, ledSize + 4.0f, ledSize + 4.0f); // glow
        g.setColour(juce::Colour(0xffff8833));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);
    }
    else if (enabled)
    {
        // Enabled, not clipping — green
        g.setColour(juce::Colour(0xff22aa44));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);
        // Specular
        g.setColour(juce::Colour(0x44ffffff));
        g.fillEllipse(ledX + 1.0f, ledY + 1.0f, 2.5f, 2.0f);
    }
    else
    {
        // Disabled — dark
        g.setColour(juce::Colour(0xff2a2a22));
        g.fillEllipse(ledX, ledY, ledSize, ledSize);
    }

    // "LIMIT" text
    g.setColour(enabled ? juce::Colour(0xffc0beb0) : juce::Colour(0xff605e50));
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    auto textArea = bounds.withTrimmedLeft(ledX - bounds.getX() + ledSize + 3.0f);
    g.drawText("LIMIT", textArea, juce::Justification::centredLeft);
}

void SidebarPanel::paintCpuMeter(juce::Graphics& g)
{
    if (cpuMeterArea.isEmpty()) return;

    auto area = cpuMeterArea;
    float clamped = juce::jlimit(0.0f, 1.0f, displayCpuLoad);
    int pct = juce::roundToInt(clamped * 100.0f);

    // "CPU XX%" label
    g.setColour(Theme::Colours::textSecondary);
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    auto labelArea = area.removeFromTop(12);
    g.drawText("CPU " + juce::String(pct) + "%", labelArea, juce::Justification::centred);

    area.removeFromTop(2);

    // Horizontal bar
    auto barArea = area.toFloat();
    float barH = juce::jmin(barArea.getHeight(), 6.0f);
    auto bar = barArea.withHeight(barH);

    // Recessed well
    g.setColour(juce::Colour(0xff0e0d0a));
    g.fillRoundedRectangle(bar, 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawRoundedRectangle(bar, 2.0f, 0.5f);

    // Filled portion
    if (clamped > 0.001f)
    {
        auto filled = bar.withWidth(bar.getWidth() * clamped).reduced(0.5f);

        // Color: green -> amber -> red
        juce::Colour barColour;
        if (clamped < 0.5f)
            barColour = juce::Colour(0xff44aa33).interpolatedWith(
                juce::Colour(0xffbb9922), clamped / 0.5f);
        else
            barColour = juce::Colour(0xffbb9922).interpolatedWith(
                juce::Colour(0xffcc3322), (clamped - 0.5f) / 0.5f);

        g.setColour(barColour);
        g.fillRoundedRectangle(filled, 1.5f);

        // Center highlight for LED glow
        g.setColour(barColour.brighter(0.3f).withAlpha(0.4f));
        g.fillRoundedRectangle(filled.reduced(0.5f, 1.0f), 1.0f);
    }
}

// Master volume knob LookAndFeel - metallic rotary with arc sweep indicator
void SidebarPanel::MasterKnobLF::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float>(
        static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(width), static_cast<float>(height));
    auto centre = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    // -- Arc track (dark background arc) --
    float arcRadius = radius - 2.0f;
    float trackWidth = 3.0f;
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius,
                           0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff1a1814));
    g.strokePath(trackArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // -- Arc fill (gold sweep from start to current position) --
    float currentAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    if (sliderPos > 0.01f)
    {
        juce::Path fillArc;
        fillArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius,
                              0.0f, rotaryStartAngle, currentAngle, true);
        g.setColour(Theme::Colours::goldText.withAlpha(0.7f));
        g.strokePath(fillArc, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        // Brighter inner stroke
        g.setColour(Theme::Colours::goldText.withAlpha(0.3f));
        g.strokePath(fillArc, juce::PathStrokeType(trackWidth - 1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    // -- Knob body --
    float knobRadius = radius - 7.0f;

    // Shadow behind knob
    g.setColour(juce::Colour(0xff0a0908));
    g.fillEllipse(centre.x - knobRadius - 1.0f, centre.y - knobRadius + 1.0f,
                  knobRadius * 2.0f + 2.0f, knobRadius * 2.0f + 2.0f);

    // Knob body gradient (metallic top-light)
    juce::ColourGradient knobGrad(
        juce::Colour(0xff5a5448), centre.x, centre.y - knobRadius,
        juce::Colour(0xff2a2620), centre.x, centre.y + knobRadius, false);
    g.setGradientFill(knobGrad);
    g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius,
                  knobRadius * 2.0f, knobRadius * 2.0f);

    // Rim
    g.setColour(juce::Colour(0xff1a1814));
    g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius,
                  knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);

    // Top highlight arc
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    juce::Path topArc;
    topArc.addCentredArc(centre.x, centre.y, knobRadius - 1.0f, knobRadius - 1.0f,
                         0.0f, -2.2f, -0.9f, true);
    g.strokePath(topArc, juce::PathStrokeType(1.0f));

    // -- Pointer line --
    float pointerLen = knobRadius - 3.0f;
    float px = centre.x + pointerLen * std::sin(currentAngle);
    float py = centre.y - pointerLen * std::cos(currentAngle);
    float ix = centre.x + 4.0f * std::sin(currentAngle);
    float iy = centre.y - 4.0f * std::cos(currentAngle);

    g.setColour(Theme::Colours::goldText);
    g.drawLine(ix, iy, px, py, 2.0f);

    // Pointer dot at tip
    g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);
}
