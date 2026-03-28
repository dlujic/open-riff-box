#include "TunerPanel.h"
#include "Theme.h"

TunerPanel::TunerPanel(TunerEngine& engine)
    : tunerRef(engine)
{
    startTimerHz(60);
}

void TunerPanel::timerCallback()
{
    auto& r = tunerRef.getResult();
    float conf  = r.confidence.load(std::memory_order_relaxed);
    float cents = r.centsOffset.load(std::memory_order_relaxed);
    float freq  = r.frequency.load(std::memory_order_relaxed);
    int   note  = r.midiNote.load(std::memory_order_relaxed);
    int   nameIdx = r.noteNameIndex.load(std::memory_order_relaxed);

    // Smooth the needle (fast attack, moderate release)
    if (conf > 0.3f)
    {
        displayCents = displayCents * 0.6f + cents * 0.4f;
        displayConfidence = displayConfidence * 0.5f + conf * 0.5f;
        displayNote = note;
        displayOctave = (note >= 0) ? (note / 12 - 1) : -1;
        displayFrequency = freq;
        displayNoteName = TunerResult::noteNames[nameIdx];
    }
    else
    {
        // Fade out smoothly
        displayConfidence *= 0.9f;
        if (displayConfidence < 0.05f)
        {
            displayNote = -1;
            displayOctave = -1;
            displayFrequency = 0.0f;
            displayCents = displayCents * 0.85f;
        }
    }

    repaint();
}

void TunerPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::Colours::panelBackground);
    Theme::paintNoise(g, getLocalBounds());
    Theme::paintBevel(g, getLocalBounds());
    Theme::paintScrews(g, getLocalBounds());

    auto area = getLocalBounds().reduced(Theme::Dims::panelPadding).toFloat();

    // Meter occupies the top ~60% of the panel
    float meterHeight = area.getHeight() * 0.58f;
    auto meterArea = area.removeFromTop(meterHeight);

    // Note + info display below
    area.removeFromTop(8.0f);
    auto noteArea = area.removeFromTop(60.0f);
    area.removeFromTop(4.0f);
    auto infoArea = area.removeFromTop(24.0f);

    paintMeterFace(g, meterArea);
    paintNeedle(g, meterArea, displayCents);
    paintNoteDisplay(g, noteArea);
    paintInfoDisplay(g, infoArea);
}

void TunerPanel::paintMeterFace(juce::Graphics& g, juce::Rectangle<float> area)
{
    // Center the meter face horizontally, constrain width
    float maxW = juce::jmin(area.getWidth(), 420.0f);
    auto face = area.withSizeKeepingCentre(maxW, area.getHeight());

    // Cream meter background (like the Gibson tuner reference)
    juce::ColourGradient faceBg(
        juce::Colour(0xfff0e8d0), face.getCentreX(), face.getY(),
        juce::Colour(0xffe0d8c0), face.getCentreX(), face.getBottom(), false);
    g.setGradientFill(faceBg);
    g.fillRoundedRectangle(face, 6.0f);

    // Dark inset border
    g.setColour(juce::Colour(0xff2a2620));
    g.drawRoundedRectangle(face, 6.0f, 2.0f);

    // Inner shadow at top
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.drawLine(face.getX() + 8.0f, face.getY() + 3.0f,
               face.getRight() - 8.0f, face.getY() + 3.0f, 1.0f);

    // Arc parameters
    float cx = face.getCentreX();
    float cy = face.getBottom() - 20.0f;  // pivot point near bottom
    float arcRadius = face.getWidth() * 0.42f;

    // Arc angles: -40 to +40 cents mapped to ~140 degrees sweep
    // Center is straight up (pi/2 from right = -pi/2 in JUCE coords)
    constexpr float sweepHalf = 1.22f;  // ~70 degrees each side
    float startAngle = -juce::MathConstants<float>::halfPi - sweepHalf;
    float endAngle   = -juce::MathConstants<float>::halfPi + sweepHalf;

    // Green "in tune" zone at center (+-5 cents = +-12.5% of sweep)
    {
        float greenHalf = sweepHalf * 0.125f;
        float greenStart = -juce::MathConstants<float>::halfPi - greenHalf;
        float greenEnd   = -juce::MathConstants<float>::halfPi + greenHalf;

        juce::Path greenArc;
        greenArc.addCentredArc(cx, cy, arcRadius - 10.0f, arcRadius - 10.0f,
                               0.0f, greenStart, greenEnd, true);
        g.setColour(juce::Colour(0x3044bb44));
        g.strokePath(greenArc, juce::PathStrokeType(12.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    // Tick marks and labels
    const int numMajorTicks = 9;  // -40, -30, -20, -10, 0, +10, +20, +30, +40
    g.setFont(juce::Font(juce::FontOptions(10.0f)));

    for (int i = 0; i < numMajorTicks; ++i)
    {
        int centValue = -40 + i * 10;
        float t = static_cast<float>(i) / static_cast<float>(numMajorTicks - 1);
        float angle = startAngle + t * (endAngle - startAngle);

        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // Outer tick
        float outerR = arcRadius + 2.0f;
        float innerR = arcRadius - 6.0f;
        float labelR = arcRadius + 14.0f;

        float x1 = cx + outerR * cosA;
        float y1 = cy + outerR * sinA;
        float x2 = cx + innerR * cosA;
        float y2 = cy + innerR * sinA;

        g.setColour(juce::Colour(0xff3a3428));
        g.drawLine(x1, y1, x2, y2, (centValue == 0) ? 2.0f : 1.0f);

        // Label
        float lx = cx + labelR * cosA;
        float ly = cy + labelR * sinA;

        juce::String label;
        if (centValue > 0) label = "+" + juce::String(centValue);
        else label = juce::String(centValue);

        g.setColour(juce::Colour(0xff4a4438));
        g.drawText(label, static_cast<int>(lx - 16), static_cast<int>(ly - 7),
                   32, 14, juce::Justification::centred);

        // Minor ticks between major ticks (except after last)
        if (i < numMajorTicks - 1)
        {
            float tMid = (static_cast<float>(i) + 0.5f) / static_cast<float>(numMajorTicks - 1);
            float midAngle = startAngle + tMid * (endAngle - startAngle);
            float mx1 = cx + (arcRadius + 1.0f) * std::cos(midAngle);
            float my1 = cy + (arcRadius + 1.0f) * std::sin(midAngle);
            float mx2 = cx + (arcRadius - 3.0f) * std::cos(midAngle);
            float my2 = cy + (arcRadius - 3.0f) * std::sin(midAngle);
            g.setColour(juce::Colour(0xff5a5448));
            g.drawLine(mx1, my1, mx2, my2, 0.5f);
        }
    }

    // "cents" label at top center of arc
    g.setColour(juce::Colour(0xff6a6458));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("cents", static_cast<int>(cx - 20),
               static_cast<int>(cy - arcRadius - 28), 40, 12,
               juce::Justification::centred);
}

void TunerPanel::paintNeedle(juce::Graphics& g, juce::Rectangle<float> area, float cents)
{
    float maxW = juce::jmin(area.getWidth(), 420.0f);
    auto face = area.withSizeKeepingCentre(maxW, area.getHeight());

    float cx = face.getCentreX();
    float cy = face.getBottom() - 20.0f;
    float arcRadius = face.getWidth() * 0.42f;

    constexpr float sweepHalf = 1.22f;
    constexpr float maxCents = 40.0f;

    // Clamp cents to display range
    float clampedCents = juce::jlimit(-maxCents, maxCents, cents);
    float normalised = clampedCents / maxCents;  // -1 to +1
    float angle = -juce::MathConstants<float>::halfPi + normalised * sweepHalf;

    float needleLen = arcRadius + 5.0f;
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);

    // Needle shadow
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.drawLine(cx + 1.0f, cy + 1.0f,
               cx + needleLen * cosA + 1.0f,
               cy + needleLen * sinA + 1.0f, 2.5f);

    // Needle body - dark when no signal, red/dark when signal
    juce::Colour needleColour = (displayConfidence > 0.1f)
        ? juce::Colour(0xff882222)
        : juce::Colour(0xff5a5448);

    g.setColour(needleColour);
    g.drawLine(cx, cy, cx + needleLen * cosA, cy + needleLen * sinA, 2.0f);

    // Pivot cap
    g.setColour(juce::Colour(0xff3a3428));
    g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colour(0xff5a5448));
    g.drawEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.0f);

    // Specular highlight on pivot
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillEllipse(cx - 2.0f, cy - 3.0f, 4.0f, 3.0f);

    // "In tune" glow when within +-3 cents and confident
    if (displayConfidence > 0.3f && std::abs(clampedCents) < 3.0f)
    {
        float glowAlpha = 0.3f * (1.0f - std::abs(clampedCents) / 3.0f);
        g.setColour(juce::Colour(0xff44cc44).withAlpha(glowAlpha));
        float glowR = 30.0f;
        g.fillEllipse(cx - glowR, cy - arcRadius - glowR,
                      glowR * 2.0f, glowR * 2.0f);
    }
}

void TunerPanel::paintNoteDisplay(juce::Graphics& g, juce::Rectangle<float> area)
{
    float centreX = area.getCentreX();

    if (displayNote >= 0 && displayConfidence > 0.1f)
    {
        // Big note name
        g.setColour(Theme::Colours::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(42.0f).withTypeface(Theme::Fonts::getInterBold())));

        juce::String noteStr(displayNoteName);
        float noteW = g.getCurrentFont().getStringWidthFloat(noteStr);

        float noteX = centreX - noteW * 0.5f - 4.0f;  // shift left to make room for octave
        g.drawText(noteStr, static_cast<int>(noteX), static_cast<int>(area.getY()),
                   static_cast<int>(noteW + 8), 48, juce::Justification::centred);

        // Octave number (smaller, subscript style)
        g.setFont(juce::Font(juce::FontOptions(22.0f).withTypeface(Theme::Fonts::getInterRegular())));
        g.setColour(Theme::Colours::textSecondary);
        g.drawText(juce::String(displayOctave),
                   static_cast<int>(noteX + noteW + 4), static_cast<int>(area.getY() + 18),
                   30, 24, juce::Justification::centredLeft);
    }
    else
    {
        // No signal
        g.setColour(Theme::Colours::textSecondary.withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions(36.0f).withTypeface(Theme::Fonts::getInterBold())));
        g.drawText("--", area.toNearestInt(), juce::Justification::centred);
    }
}

void TunerPanel::paintInfoDisplay(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setFont(juce::Font(juce::FontOptions(12.0f).withTypeface(Theme::Fonts::getInterRegular())));

    if (displayNote >= 0 && displayConfidence > 0.1f)
    {
        // Frequency on left
        g.setColour(Theme::Colours::textSecondary);
        juce::String freqStr = juce::String(displayFrequency, 1) + " Hz";
        g.drawText(freqStr, area.toNearestInt(), juce::Justification::centredLeft);

        // Cents on right - colored by accuracy
        float absCents = std::abs(displayCents);
        juce::Colour centsColour;
        if (absCents < 3.0f)
            centsColour = juce::Colour(0xff44cc44);  // green = in tune
        else if (absCents < 10.0f)
            centsColour = juce::Colour(0xffccaa33);  // amber = close
        else
            centsColour = juce::Colour(0xffcc5533);  // red = far

        g.setColour(centsColour);
        juce::String centsStr;
        if (displayCents >= 0.0f)
            centsStr = "+" + juce::String(displayCents, 1) + " cents";
        else
            centsStr = juce::String(displayCents, 1) + " cents";
        g.drawText(centsStr, area.toNearestInt(), juce::Justification::centredRight);
    }
    else
    {
        g.setColour(Theme::Colours::textSecondary.withAlpha(0.3f));
        g.drawText("Play a note to tune", area.toNearestInt(), juce::Justification::centred);
    }
}
