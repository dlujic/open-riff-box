#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace Theme
{
    //===========================================================================
    // Colours
    //===========================================================================
    namespace Colours
    {
        inline const juce::Colour background      { 0xff1f1c18 };  // warm dark brown
        inline const juce::Colour panelBackground  { 0xff262219 };  // warm panel
        inline const juce::Colour topBar           { 0xff2a2820 };  // warm bar
        inline const juce::Colour selectedRow      { 0xff3a3428 };  // warm highlight
        inline const juce::Colour textPrimary      { 0xfff0e8d8 };  // cream white
        inline const juce::Colour textSecondary    { 0xffa09880 };  // warm grey
        inline const juce::Colour border           { 0xff3e3828 };  // warm border
        inline const juce::Colour chainListBg      { 0xff201d16 };  // darker bg for chain list (notebook depth)

        // Effect type colours
        inline const juce::Colour gate       { 0xff9966cc };   // purple — noise gate
        inline const juce::Colour drive      { 0xffdd8833 };   // orange — diode drive (warm OD)
        inline const juce::Colour distortion { 0xffcc3838 };   // red — distortion (aggressive)
        inline const juce::Colour ampSim     { 0xffc89830 };   // amber/gold — amp sim
        inline const juce::Colour timeBased  { 0xff4488cc };   // blue — delay, reverb
        inline const juce::Colour modulation { 0xff44bb66 };   // green — chorus
        inline const juce::Colour neutral    { 0xffaaaaaa };   // grey — EQ

        // Button colours
        inline const juce::Colour startButton { 0xff33aa33 };
        inline const juce::Colour stopButton  { 0xffcc3333 };

        // Amp faceplate
        inline const juce::Colour topBarTop    { 0xff3e3828 };  // lighter warm metal
        inline const juce::Colour topBarBottom  { 0xff241f16 };  // darker warm edge
        inline const juce::Colour goldText      { 0xffd4a84b };  // warm gold for title
    }

    //===========================================================================
    // Dimensions
    //===========================================================================
    namespace Dims
    {
        inline constexpr int topBarHeight       = 48;
        inline constexpr int chainListWidth     = 170;
        inline constexpr int chainRowHeight     = 36;
        inline constexpr int chainSeparatorH    = 24;
        inline constexpr int sliderWidth        = 50;
        inline constexpr int sliderHeight       = 150;
        inline constexpr int sliderLabelHeight  = 18;
        inline constexpr int sliderSpacing      = 12;
        inline constexpr int sliderTextBoxH     = 18;
        inline constexpr int panelPadding       = 16;
        inline constexpr int settingsPanelHeight = 300;
        inline constexpr int sidebarWidth       = 80;
        inline constexpr int presetBarHeight    = 50;
    }

    //===========================================================================
    // Fonts — Inter (UI) + Metal Mania (display title only)
    //===========================================================================
    namespace Fonts
    {
        // Typeface singletons (created on first use, cached forever)
        inline juce::Typeface::Ptr getInterRegular()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::Inter_18ptRegular_ttf, BinaryData::Inter_18ptRegular_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr getInterSemiBold()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::Inter_18ptSemiBold_ttf, BinaryData::Inter_18ptSemiBold_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr getInterBold()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::Inter_18ptBold_ttf, BinaryData::Inter_18ptBold_ttfSize);
            return tf;
        }

        inline juce::Typeface::Ptr getMetalMania()
        {
            static auto tf = juce::Typeface::createSystemTypefaceFor(
                BinaryData::MetalManiaRegular_ttf, BinaryData::MetalManiaRegular_ttfSize);
            return tf;
        }

        // Default typeface for setDefaultSansSerifTypeface()
        inline juce::Typeface::Ptr getRegular() { return getInterRegular(); }

        inline juce::Font appTitle()  { return juce::Font(juce::FontOptions(getMetalMania()).withHeight(24.0f)); }
        inline juce::Font title()    { return juce::Font(juce::FontOptions(getInterBold()).withHeight(20.0f)); }
        inline juce::Font heading()  { return juce::Font(juce::FontOptions(getInterSemiBold()).withHeight(15.0f)); }
        inline juce::Font body()     { return juce::Font(juce::FontOptions(getInterRegular()).withHeight(13.0f)); }
        inline juce::Font small()    { return juce::Font(juce::FontOptions(getInterRegular()).withHeight(12.0f)); }
    }

    //===========================================================================
    // Paint helpers
    //===========================================================================

    // Paints beveled panel edges — highlight on top/left, shadow on bottom/right.
    // Creates the illusion of a recessed or raised metal panel.
    inline void paintBevel(juce::Graphics& g, juce::Rectangle<int> area, bool raised = false)
    {
        auto light = juce::Colours::white.withAlpha(raised ? 0.08f : 0.04f);
        auto dark  = juce::Colours::black.withAlpha(raised ? 0.25f : 0.15f);

        auto top    = raised ? light : dark;
        auto left   = raised ? light : dark;
        auto bottom = raised ? dark  : light;
        auto right  = raised ? dark  : light;

        float l = static_cast<float>(area.getX());
        float t = static_cast<float>(area.getY());
        float r = static_cast<float>(area.getRight() - 1);
        float b = static_cast<float>(area.getBottom() - 1);

        g.setColour(top);
        g.drawHorizontalLine(static_cast<int>(t), l, r);
        g.setColour(left);
        g.drawVerticalLine(static_cast<int>(l), t, b);
        g.setColour(bottom);
        g.drawHorizontalLine(static_cast<int>(b), l, r);
        g.setColour(right);
        g.drawVerticalLine(static_cast<int>(r), t, b);
    }

    // Paints Phillips-head screw decorations at the four corners of the area.
    inline void paintScrews(juce::Graphics& g, juce::Rectangle<int> area, float inset = 10.0f)
    {
        const float radius = 5.0f;
        const float slotLen = 3.5f;

        float positions[][2] = {
            { area.getX() + inset,                    area.getY() + inset },
            { area.getRight() - inset,                area.getY() + inset },
            { area.getX() + inset,                    area.getBottom() - inset },
            { area.getRight() - inset,                area.getBottom() - inset }
        };

        for (auto& pos : positions)
        {
            float cx = pos[0], cy = pos[1];

            // Screw body (metallic gradient)
            juce::ColourGradient screwGrad(juce::Colour(0xff606058), cx, cy - radius,
                                            juce::Colour(0xff383830), cx, cy + radius, false);
            g.setGradientFill(screwGrad);
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

            // Rim highlight
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 0.5f);

            // Phillips cross slots
            g.setColour(juce::Colour(0xff1a1a18));
            g.drawLine(cx - slotLen, cy, cx + slotLen, cy, 1.0f);
            g.drawLine(cx, cy - slotLen, cx, cy + slotLen, 1.0f);
        }
    }

    // Paints a subtle horizontal groove line across the panel header row,
    // filling the gap between the bypass button and the reset button.
    inline void paintHeaderGroove(juce::Graphics& g, juce::Rectangle<int> area)
    {
        const float y = static_cast<float>(Dims::panelPadding) + 19.0f;  // vertical centre of 38px top row
        const float x1 = static_cast<float>(Dims::panelPadding) + 184.0f; // after bypass button
        const float x2 = static_cast<float>(area.getRight() - Dims::panelPadding) - 64.0f; // before reset button

        if (x2 <= x1) return;

        // Shadow line (recessed groove)
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawLine(x1, y, x2, y, 1.5f);

        // Highlight line (raised edge below shadow)
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawLine(x1, y + 1.5f, x2, y + 1.5f, 1.0f);
    }

    // Returns a cached noise tile image (generated once, reused forever).
    // The tile is 128x128 and gets tiled across the target area.
    inline const juce::Image& getNoiseTile(float opacity = 0.04f)
    {
        static juce::Image tile;
        static float cachedOpacity = -1.0f;

        if (tile.isValid() && cachedOpacity == opacity)
            return tile;

        const int size = 128;
        const int density = 3;
        cachedOpacity = opacity;
        tile = juce::Image(juce::Image::ARGB, size, size, true);

        auto baseAlpha = static_cast<juce::uint8>(opacity * 255.0f);
        juce::Image::BitmapData data(tile, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < size; y += density)
        {
            for (int x = 0; x < size; x += density)
            {
                auto h = static_cast<unsigned int>((x * 73856093) ^ (y * 19349663));
                h = (h >> 13) ^ h;
                h = h * 0x45d9f3b;
                h = (h >> 16) ^ h;

                auto val = static_cast<juce::uint8>(h & 0xFF);
                bool bright = (val > 128);
                auto c = bright ? juce::Colour(255, 255, 255).withAlpha(baseAlpha)
                                : juce::Colour(0, 0, 0).withAlpha(baseAlpha);

                for (int dy = 0; dy < density && (y + dy) < size; ++dy)
                    for (int dx = 0; dx < density && (x + dx) < size; ++dx)
                        data.setPixelColour(x + dx, y + dy, c);
            }
        }

        return tile;
    }

    // Paints a subtle noise/grain texture by tiling a cached image.
    // Call AFTER filling the background colour. Near-zero cost per repaint.
    inline void paintNoise(juce::Graphics& g, juce::Rectangle<int> area,
                           float opacity = 0.04f)
    {
        auto& tile = getNoiseTile(opacity);
        const int tileW = tile.getWidth();
        const int tileH = tile.getHeight();

        for (int y = area.getY(); y < area.getBottom(); y += tileH)
            for (int x = area.getX(); x < area.getRight(); x += tileW)
                g.drawImageAt(tile, x, y);
    }
}
