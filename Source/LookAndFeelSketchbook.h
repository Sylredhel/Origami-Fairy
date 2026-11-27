#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <array>
#include <cmath>

namespace sylredhel::sketch
{
    static inline const juce::Colour kPaperTop    { 0xfff7f2e6 };
    static inline const juce::Colour kPaperBot    { 0xffece3cf };
    static inline const juce::Colour kCardFill    { 0xfffaf6ec };
    static inline const juce::Colour kPlotBg      { 0xfff3ecd9 };
    static inline const juce::Colour kGraphite    { 0xff2f2c28 };
    static inline const juce::Colour kGraphiteMid { 0xff5a544b };
    static inline const juce::Colour kGraphiteLt  { 0xff9b9283 };
    static inline const juce::Colour kGridLine    { 0xffdfd6c1 };

    static inline const juce::Colour kSageTeal    { 0xff2f8578 };
    static inline const juce::Colour kTerracotta  { 0xffc6523c };
    static inline const juce::Colour kOchre       { 0xffc0842c };
    static inline const juce::Colour kSlateBlue   { 0xff466987 };
    static inline const juce::Colour kPlum        { 0xff7b4d7c };

    static inline void drawSketchBox(juce::Graphics& g, juce::Rectangle<float> r,
                                     float corner = 6.0f, juce::Colour strokeCol = kGraphiteMid)
    {
        g.setColour(strokeCol.withAlpha(0.78f));
        g.drawRoundedRectangle(r, corner, 1.35f);
        g.setColour(strokeCol.withAlpha(0.24f));
        g.drawRoundedRectangle(r.translated(0.9f, 0.7f).reduced(0.4f), corner, 0.9f);
    }

    static inline void drawPencilHatch(juce::Graphics& g, juce::Rectangle<float> r,
                                       juce::Colour col, float step = 7.0f)
    {
        g.saveState();
        g.reduceClipRegion(r.toNearestInt());
        g.setColour(col);
        const float h = r.getHeight();
        for (float x = r.getX() - h; x < r.getRight() + h; x += step)
            g.drawLine(x, r.getBottom(), x + h, r.getY(), 0.7f);
        g.restoreState();
    }
}

class LookAndFeelSketchbook : public juce::LookAndFeel_V4
{
public:
    static constexpr const char* kAccentProp  = "sylAccent";
    static constexpr const char* kLivePosProp = "sylLivePos"; // [0..1] optional live modulated arc

    LookAndFeelSketchbook()
    {
        using namespace sylredhel::sketch;
        setColour(juce::ComboBox::backgroundColourId, kCardFill);
        setColour(juce::ComboBox::textColourId,       kGraphite);
        setColour(juce::ComboBox::outlineColourId,    kGraphiteMid);
        setColour(juce::ComboBox::arrowColourId,      kGraphite);

        setColour(juce::PopupMenu::backgroundColourId,            kPaperTop);
        setColour(juce::PopupMenu::textColourId,                  kGraphite);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kSageTeal.withAlpha(0.22f));
        setColour(juce::PopupMenu::highlightedTextColourId,       kGraphite);

        setColour(juce::TextButton::buttonColourId,   kCardFill);
        setColour(juce::TextButton::buttonOnColourId, kSageTeal.withAlpha(0.25f));
        setColour(juce::TextButton::textColourOffId,  kGraphite);
        setColour(juce::TextButton::textColourOnId,   kGraphite);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        using namespace sylredhel::sketch;
        auto b = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
        g.setColour(box.isMouseOver() ? juce::Colour(0xfffffbf2) : kCardFill);
        g.fillRoundedRectangle(b, 4.5f);
        drawSketchBox(g, b, 4.5f, box.isMouseOver() ? kGraphite : kGraphiteMid);

        const float cx = static_cast<float>(buttonX) + static_cast<float>(buttonW) * 0.5f;
        const float cy = static_cast<float>(buttonY) + static_cast<float>(buttonH) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
        arrow.lineTo(cx, cy + 2.8f);
        arrow.lineTo(cx + 4.0f, cy - 2.0f);
        g.setColour(kGraphite);
        g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
                              const juce::Colour&, bool isHighlighted, bool isDown) override
    {
        using namespace sylredhel::sketch;
        auto b = btn.getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(isDown ? kGridLine : (isHighlighted ? juce::Colour(0xfffffbf2) : kCardFill));
        g.fillRoundedRectangle(b, 4.5f);
        drawSketchBox(g, b, 4.5f, isHighlighted ? kGraphite : kGraphiteMid);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        using namespace sylredhel::sketch;
        const auto b = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(width), static_cast<float>(height));
        const float cx = b.getCentreX();
        const float cy = b.getCentreY() + 1.0f;
        const float r  = juce::jmin(b.getWidth(), b.getHeight()) * 0.34f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        const uint32_t colHex = static_cast<uint32_t>(
            static_cast<int>(slider.getProperties().getWithDefault(kAccentProp, static_cast<int>(0xff2f8578))));
        const juce::Colour accent(colHex);

        // Optional live dynamic-drive indicator position (-1 means disabled)
        const float livePos = static_cast<float>(slider.getProperties().getWithDefault(kLivePosProp, -1.0));

        // Hand-sketched dial ticks around perimeter
        const float tickR1 = r + 3.5f;
        const float tickR2 = r + 6.5f;
        g.setColour(kGraphiteLt.withAlpha(0.75f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (static_cast<float>(i) / 10.0f) * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
            g.drawLine(cx + std::cos(a) * tickR1, cy + std::sin(a) * tickR1,
                       cx + std::cos(a) * tickR2, cy + std::sin(a) * tickR2, (i == 0 || i == 5 || i == 10) ? 1.4f : 0.9f);
        }

        // Colored-pencil value arc
        const float arcR = r + 5.0f;
        if (sliderPos > 0.002f)
        {
            juce::Path valArc;
            valArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
            const float alpha = (livePos >= 0.0f) ? 0.30f : 0.88f;
            g.setColour(accent.withAlpha(alpha));
            g.strokePath(valArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (livePos < 0.0f)
            {
                g.setColour(accent.withAlpha(0.28f));
                g.strokePath(valArc, juce::PathStrokeType(4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // If Envelope Follower is active, draw live effective drive arc + glowing pencil dot!
        if (livePos >= 0.0f)
        {
            const float clampedLive = juce::jlimit(0.0f, 1.0f, livePos);
            const float liveAngle = startAngle + clampedLive * (endAngle - startAngle);
            if (clampedLive > 0.002f)
            {
                juce::Path liveArc;
                liveArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, liveAngle, true);
                g.setColour(kTerracotta.withAlpha(0.92f));
                g.strokePath(liveArc, juce::PathStrokeType(3.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            const float dotA = liveAngle - juce::MathConstants<float>::halfPi;
            const float dx = cx + std::cos(dotA) * arcR;
            const float dy = cy + std::sin(dotA) * arcR;
            g.setColour(kTerracotta);
            g.fillEllipse(dx - 3.2f, dy - 3.2f, 6.4f, 6.4f);
        }

        // Soft paper-pencil drop shadow
        g.setColour(kGraphite.withAlpha(0.10f));
        g.fillEllipse(cx - r + 1.8f, cy - r + 2.4f, r * 2.0f, r * 2.0f);

        // Cream sketchbook knob face with double-pass graphite outline
        juce::ColourGradient cap(juce::Colour(0xfffffcf5), cx - r * 0.4f, cy - r * 0.5f,
                                 juce::Colour(0xffe9dfc9), cx + r * 0.6f, cy + r * 0.6f, false);
        g.setGradientFill(cap);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

        g.setColour(kGraphite.withAlpha(0.85f));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);
        g.setColour(kGraphite.withAlpha(0.28f));
        g.drawEllipse(cx - r + 0.8f, cy - r - 0.5f, r * 2.0f - 0.5f, r * 2.0f + 0.3f, 0.9f);

        // Bold graphite + colored-pencil pointer line
        const float ptrA = angle - juce::MathConstants<float>::halfPi;
        g.setColour(accent);
        g.drawLine(cx + std::cos(ptrA) * (r * 0.22f), cy + std::sin(ptrA) * (r * 0.22f),
                   cx + std::cos(ptrA) * (r * 0.84f), cy + std::sin(ptrA) * (r * 0.84f), 2.4f);
        g.setColour(kGraphite);
        g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool isHighlighted, bool) override
    {
        using namespace sylredhel::sketch;
        auto b = btn.getLocalBounds().toFloat().reduced(1.5f);
        const bool isOn = btn.getToggleState();
        const bool isBypass = btn.getButtonText().containsIgnoreCase("BYPASS");

        const juce::Colour fillCol = isOn
            ? (isBypass ? kTerracotta.withAlpha(0.26f) : kSageTeal.withAlpha(0.24f))
            : (isHighlighted ? juce::Colour(0xfffffbf2) : kCardFill);
        const juce::Colour rimCol = isOn
            ? (isBypass ? kTerracotta : kSageTeal)
            : (isHighlighted ? kGraphite : kGraphiteMid);

        g.setColour(fillCol);
        g.fillRoundedRectangle(b, 4.5f);
        if (isOn)
            drawPencilHatch(g, b.reduced(1.5f), rimCol.withAlpha(0.18f), 6.0f);
        drawSketchBox(g, b, 4.5f, rimCol);

        g.setColour(isOn ? (isBypass ? kTerracotta.darker(0.35f) : kSageTeal.darker(0.35f)) : kGraphite);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText(btn.getButtonText(), b.toNearestInt(), juce::Justification::centred);
    }
};

class OrigamiFairyMascot : public juce::Component
{
public:
    void updateActivity(float foldDbIn, float detailSaved01In)
    {
        const float target = juce::jlimit(0.0f, 1.0f, (foldDbIn / 8.0f) * 0.7f + detailSaved01In * 0.6f);
        activity = activity * 0.78f + target * 0.22f;
        phase += 0.14f + 0.28f * activity;
        if (phase > 6.283185f) phase -= 6.283185f;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        using namespace sylredhel::sketch;
        auto b = getLocalBounds().toFloat();

        g.setColour(kPlotBg);
        g.fillRoundedRectangle(b, 6.0f);
        drawSketchBox(g, b.reduced(0.6f), 6.0f, kGraphiteMid);

        g.setColour(kOchre.withAlpha(0.24f));
        g.fillRect(b.getCentreX() - 22.0f, b.getY() - 1.0f, 44.0f, 10.0f);

        const float cx = b.getCentreX() + 4.0f;
        const float bob = std::sin(phase) * (1.5f + 2.8f * activity);
        const float cy = b.getCentreY() + 14.0f + bob;
        const float wingTilt = std::sin(phase * 2.0f) * (3.0f + 9.0f * activity);

        g.setColour(kGraphite.withAlpha(0.11f));
        g.fillEllipse(cx - 28.0f, b.getBottom() - 34.0f, 56.0f, 10.0f);

        juce::Path wingL, wingR;
        wingL.startNewSubPath(cx - 10.0f, cy + 4.0f);
        wingL.lineTo(cx - 52.0f, cy - 22.0f - wingTilt);
        wingL.lineTo(cx - 34.0f, cy + 16.0f - wingTilt * 0.4f);
        wingL.closeSubPath();

        wingR.startNewSubPath(cx + 12.0f, cy + 4.0f);
        wingR.lineTo(cx + 50.0f, cy - 18.0f - wingTilt);
        wingR.lineTo(cx + 34.0f, cy + 18.0f - wingTilt * 0.4f);
        wingR.closeSubPath();

        g.setColour(kCardFill);
        g.fillPath(wingL);
        g.fillPath(wingR);
        g.setColour(kSageTeal.withAlpha(0.16f + 0.14f * activity));
        g.fillPath(wingL);
        g.fillPath(wingR);

        g.setColour(kGraphite);
        g.strokePath(wingL, juce::PathStrokeType(1.5f));
        g.strokePath(wingR, juce::PathStrokeType(1.5f));
        g.setColour(kGraphiteMid.withAlpha(0.7f));
        g.drawLine(cx - 10.0f, cy + 4.0f, cx - 42.0f, cy - 2.0f - wingTilt * 0.7f, 1.0f);
        g.drawLine(cx + 12.0f, cy + 4.0f, cx + 41.0f, cy + 0.0f - wingTilt * 0.7f, 1.0f);

        juce::Path cloak;
        cloak.startNewSubPath(cx - 22.0f, cy + 32.0f);
        cloak.quadraticTo(cx - 24.0f, cy - 6.0f, cx, cy - 12.0f);
        cloak.quadraticTo(cx + 24.0f, cy - 6.0f, cx + 22.0f, cy + 32.0f);
        cloak.lineTo(cx + 6.0f, cy + 26.0f);
        cloak.lineTo(cx - 6.0f, cy + 33.0f);
        cloak.closeSubPath();

        g.setColour(juce::Colour(0xfffffbf0));
        g.fillPath(cloak);
        g.setColour(kGraphite);
        g.strokePath(cloak, juce::PathStrokeType(1.6f));
        g.setColour(kGraphiteLt);
        g.drawLine(cx, cy + 2.0f, cx - 2.0f, cy + 28.0f, 1.0f);

        const float hx = cx;
        const float hy = cy - 18.0f;
        g.setColour(juce::Colour(0xfffffcf5));
        g.fillEllipse(hx - 21.0f, hy - 18.0f, 42.0f, 36.0f);
        g.setColour(kGraphite);
        g.drawEllipse(hx - 21.0f, hy - 18.0f, 42.0f, 36.0f, 1.6f);
        g.setColour(kGraphite.withAlpha(0.28f));
        g.drawEllipse(hx - 20.0f, hy - 17.5f, 41.5f, 35.0f, 0.9f);

        g.setColour(kGraphite);
        g.drawLine(hx - 14.0f, hy - 14.0f, hx - 20.0f, hy - 20.0f, 1.2f);
        g.drawLine(hx + 15.0f, hy - 12.0f, hx + 22.0f, hy - 16.0f, 1.2f);

        juce::Path hat;
        hat.startNewSubPath(hx - 18.0f, hy - 14.0f);
        hat.lineTo(hx - 2.0f,  hy - 34.0f);
        hat.lineTo(hx + 17.0f, hy - 14.0f);
        hat.closeSubPath();
        g.setColour(kOchre.withAlpha(0.22f));
        g.fillPath(hat);
        g.setColour(kGraphite);
        g.strokePath(hat, juce::PathStrokeType(1.5f));
        g.drawLine(hx - 14.0f, hy - 18.0f, hx + 13.0f, hy - 18.0f, 1.1f);

        const float lookX = -2.6f - 1.2f * activity;
        const float lookY = -0.6f - 1.5f * activity;

        g.setColour(juce::Colours::white);
        g.fillEllipse(hx - 12.5f, hy - 5.5f, 8.5f, 9.5f);
        g.fillEllipse(hx + 2.0f,  hy - 5.5f, 8.5f, 9.5f);
        g.setColour(kGraphite);
        g.drawEllipse(hx - 12.5f, hy - 5.5f, 8.5f, 9.5f, 1.2f);
        g.drawEllipse(hx + 2.0f,  hy - 5.5f, 8.5f, 9.5f, 1.2f);

        g.setColour(kGraphite);
        g.fillEllipse(hx - 8.5f + lookX, hy - 1.2f + lookY, 3.6f, 4.0f);
        g.fillEllipse(hx + 6.0f + lookX, hy - 1.2f + lookY, 3.6f, 4.0f);

        g.setColour(kGraphiteMid);
        g.drawLine(hx - 13.0f, hy - 8.0f, hx - 6.0f, hy - 9.5f, 1.1f);
        g.drawLine(hx + 4.0f,  hy - 9.5f, hx + 11.0f, hy - 8.0f, 1.1f);

        g.setColour(kTerracotta.withAlpha(0.24f + 0.16f * activity));
        g.fillEllipse(hx - 16.5f, hy + 4.0f, 7.0f, 4.0f);
        g.fillEllipse(hx + 8.5f,  hy + 4.0f, 7.0f, 4.0f);

        g.setColour(kGraphite);
        if (activity > 0.35f)
            g.drawEllipse(hx - 2.4f, hy + 5.0f, 4.4f, 4.6f, 1.2f);
        else
            g.drawLine(hx - 3.5f, hy + 7.2f, hx + 2.5f, hy + 6.8f, 1.3f);

        if (activity > 0.12f)
        {
            const float spX = b.getX() + 24.0f;
            const float spY = b.getY() + 34.0f - 4.0f * activity;
            g.setColour(kSageTeal.withAlpha(std::min(0.9f, activity * 1.4f)));
            g.drawLine(spX - 5.0f, spY, spX + 5.0f, spY, 1.3f);
            g.drawLine(spX, spY - 5.0f, spX, spY + 5.0f, 1.3f);
            g.drawLine(spX + 10.0f, spY + 12.0f, spX + 16.0f, spY + 12.0f, 1.0f);
            g.drawLine(spX + 13.0f, spY + 9.0f,  spX + 13.0f, spY + 15.0f, 1.0f);
        }

        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::italic)));
        g.setColour(kGraphiteMid);
        const juce::String mood = (activity > 0.45f) ? "folding carefully..."
                                : (activity > 0.10f) ? "tucking peaks in..."
                                                     : "watching quietly";
        g.drawText(mood, static_cast<int>(b.getX() + 6), static_cast<int>(b.getBottom() - 20),
                   static_cast<int>(b.getWidth() - 12), 14, juce::Justification::centred);
    }

private:
    float activity = 0.0f;
    float phase    = 0.0f;
};

class OrigamiScopeView : public juce::Component
{
public:
    explicit OrigamiScopeView(OrigamiFairyAudioProcessor& proc) : processorRef(proc) {}

    void updateFrame(float ceilingDb, int engineModeIdx, int phaseModeIdx, int osModeIdx, float envFollowPct)
    {
        ceilingLin = std::pow(10.0f, ceilingDb * 0.05f);
        engineIdx  = engineModeIdx;
        phaseIdx   = phaseModeIdx;
        osIdx      = osModeIdx;
        envActive  = (envFollowPct > 0.5f);
        foldDb     = processorRef.getFoldActivityDb();
        liveDrvDb  = processorRef.getLiveDriveDb();

        const double fs = std::max(processorRef.getCurrentSampleRate(), 8000.0);
        latencyMs = static_cast<float>(1000.0 * static_cast<double>(processorRef.getEngineLatencySamples()) / fs);

        processorRef.getWaveScope().copyLatest(inTrace.data(), outTrace.data(), kViewSamples);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        using namespace sylredhel::sketch;
        auto b = getLocalBounds().toFloat();

        g.setColour(kPlotBg);
        g.fillRoundedRectangle(b, 6.0f);
        drawSketchBox(g, b.reduced(0.6f), 6.0f, kGraphiteMid);

        auto plot = b.reduced(12.0f, 10.0f);
        plot.removeFromTop(21.0f);

        const float midY = plot.getCentreY();
        const float yScale = (plot.getHeight() * 0.46f) / 1.6f;
        auto valToY = [&](float v) -> float
        {
            return midY - juce::jlimit(-1.65f, 1.65f, v) * yScale;
        };

        g.setColour(kGridLine);
        for (int i = 1; i < 8; ++i)
            g.drawVerticalLine(static_cast<int>(plot.getX() + plot.getWidth() * (static_cast<float>(i) / 8.0f)),
                               plot.getY(), plot.getBottom());
        g.drawHorizontalLine(static_cast<int>(valToY(1.0f)),  plot.getX(), plot.getRight());
        g.drawHorizontalLine(static_cast<int>(valToY(-1.0f)), plot.getX(), plot.getRight());
        g.setColour(kGraphiteLt.withAlpha(0.55f));
        g.drawHorizontalLine(static_cast<int>(midY), plot.getX(), plot.getRight());

        g.saveState();
        g.reduceClipRegion(plot.toNearestInt());

        int trig = 0;
        for (int i = 1; i < kViewSamples / 2; ++i)
        {
            if (inTrace[static_cast<size_t>(i - 1)] <= 0.0f && inTrace[static_cast<size_t>(i)] > 0.0f)
            {
                trig = i;
                break;
            }
        }
        const int drawCount = kViewSamples / 2;

        juce::Path pIn, pOut;
        for (int i = 0; i < drawCount; ++i)
        {
            const float x  = plot.getX() + (static_cast<float>(i) / static_cast<float>(drawCount - 1)) * plot.getWidth();
            const float yi = valToY(inTrace[static_cast<size_t>(trig + i)]);
            const float yo = valToY(outTrace[static_cast<size_t>(trig + i)]);
            if (i == 0) { pIn.startNewSubPath(x, yi); pOut.startNewSubPath(x, yo); }
            else        { pIn.lineTo(x, yi);          pOut.lineTo(x, yo); }
        }

        g.setColour(kOchre.withAlpha(0.55f));
        g.strokePath(pIn, juce::PathStrokeType(1.4f));

        g.setColour(kSageTeal);
        g.strokePath(pOut, juce::PathStrokeType(2.0f));

        const float yPosC = valToY(ceilingLin);
        const float yNegC = valToY(-ceilingLin);
        const float dashes[2] = { 5.0f, 4.0f };
        g.setColour(kTerracotta.withAlpha(0.85f));
        g.drawDashedLine(juce::Line<float>(plot.getX(), yPosC, plot.getRight(), yPosC), dashes, 2, 1.3f);
        g.drawDashedLine(juce::Line<float>(plot.getX(), yNegC, plot.getRight(), yNegC), dashes, 2, 1.3f);

        g.restoreState();

        const auto top = juce::Rectangle<float>(b.getX() + 12.0f, b.getY() + 6.0f, b.getWidth() - 24.0f, 18.0f);
        static constexpr const char* modeTags[3] = { "NEST", "FOLD", "DPC" };
        static constexpr const char* phTags[2]   = { "LIN FIR", "MIN LR4" };
        static constexpr const char* osTags[5]   = { "1x", "2x", "4x", "8x", "16x" };

        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour(kGraphite);
        g.drawText(juce::String(modeTags[juce::jlimit(0, 2, engineIdx)]) + " - " + juce::String(phTags[juce::jlimit(0, 1, phaseIdx)]),
                   static_cast<int>(top.getX()), static_cast<int>(top.getY()), 125, 18,
                   juce::Justification::centredLeft);

        g.setColour(foldDb > 0.3f ? kTerracotta : kGraphiteLt);
        g.drawText("FOLD " + juce::String(foldDb, 1) + "dB",
                   static_cast<int>(top.getX() + 128), static_cast<int>(top.getY()), 86, 18,
                   juce::Justification::centredLeft);

        if (envActive)
        {
            g.setColour(kPlum);
            g.drawText("ENV DRV +" + juce::String(liveDrvDb, 1) + "dB",
                       static_cast<int>(top.getX() + 216), static_cast<int>(top.getY()), 116, 18,
                       juce::Justification::centredLeft);
        }

        g.setFont(juce::Font(juce::FontOptions(9.1f, juce::Font::bold)));
        g.setColour(kSlateBlue);
        g.drawText(juce::String(osTags[juce::jlimit(0, 4, osIdx)]) + " | PDC " + juce::String(latencyMs, 1) + "ms",
                   static_cast<int>(top.getRight() - 172), static_cast<int>(top.getY()), 108, 18,
                   juce::Justification::centredRight);

        g.setColour(kOchre);
        g.drawText("IN", static_cast<int>(top.getRight() - 58), static_cast<int>(top.getY()), 24, 18, juce::Justification::centredRight);
        g.setColour(kSageTeal);
        g.drawText("OUT", static_cast<int>(top.getRight() - 30), static_cast<int>(top.getY()), 30, 18, juce::Justification::centredRight);
    }

private:
    static constexpr int kViewSamples = 1024;
    OrigamiFairyAudioProcessor& processorRef;
    std::array<float, kViewSamples> inTrace {};
    std::array<float, kViewSamples> outTrace {};
    float ceilingLin = 0.966f;
    float foldDb     = 0.0f;
    float liveDrvDb  = 4.0f;
    float latencyMs  = 6.5f;
    int   engineIdx  = 0;
    int   phaseIdx   = 0;
    int   osIdx      = 3;
    bool  envActive  = false;
};

class OrigamiTransferView : public juce::Component
{
public:
    void updateParams(int satCurveIdx, int foldShapeIdx, float foldDepthPct, float ceilingDb)
    {
        const auto nextCurve = static_cast<sylredhel::origami::SatCurve>(juce::jlimit(0, 3, satCurveIdx));
        const auto nextShape = static_cast<sylredhel::origami::FoldShape>(juce::jlimit(0, 2, foldShapeIdx));
        const float nextDepth = foldDepthPct * 0.01f;
        const float nextCeil  = std::pow(10.0f, ceilingDb * 0.05f);

        if (nextCurve != curve || nextShape != shape
            || std::abs(nextDepth - depth) > 0.002f || std::abs(nextCeil - ceilLin) > 0.002f)
        {
            curve = nextCurve; shape = nextShape; depth = nextDepth; ceilLin = nextCeil;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        using namespace sylredhel::sketch;
        auto b = getLocalBounds().toFloat();

        g.setColour(kPlotBg);
        g.fillRoundedRectangle(b, 6.0f);
        drawSketchBox(g, b.reduced(0.6f), 6.0f, kGraphiteMid);

        g.setFont(juce::Font(juce::FontOptions(9.8f, juce::Font::bold)));
        g.setColour(kGraphite);
        g.drawText("TRANSFER CURVE", static_cast<int>(b.getX() + 10), static_cast<int>(b.getY() + 6),
                   static_cast<int>(b.getWidth() - 20), 16, juce::Justification::centred);

        auto plot = b.reduced(14.0f, 12.0f);
        plot.removeFromTop(18.0f);
        const float cx = plot.getCentreX();
        const float cy = plot.getCentreY();
        const float halfW = plot.getWidth() * 0.48f;
        const float halfH = plot.getHeight() * 0.48f;

        // Wide [-2.6 .. +2.6] input span so multiple continuous periodic folds are always clearly visible!
        auto mapX = [&](float x) { return cx + (x / 2.6f) * halfW; };
        auto mapY = [&](float y) { return cy - (y / 1.25f) * halfH; };

        g.setColour(kGridLine);
        g.drawHorizontalLine(static_cast<int>(cy), plot.getX(), plot.getRight());
        g.drawVerticalLine(static_cast<int>(cx), plot.getY(), plot.getBottom());

        g.setColour(kGraphiteLt.withAlpha(0.6f));
        g.drawLine(mapX(-1.25f), mapY(-1.25f), mapX(1.25f), mapY(1.25f), 1.0f);

        const float yC = mapY(ceilLin);
        const float yNC = mapY(-ceilLin);
        const float dashes[2] = { 4.0f, 3.0f };
        g.setColour(kTerracotta.withAlpha(0.75f));
        g.drawDashedLine(juce::Line<float>(plot.getX(), yC,  plot.getRight(), yC),  dashes, 2, 1.1f);
        g.drawDashedLine(juce::Line<float>(plot.getX(), yNC, plot.getRight(), yNC), dashes, 2, 1.1f);

        g.saveState();
        g.reduceClipRegion(plot.toNearestInt());

        juce::Path pSat, pFold;
        const int pts = 240;
        for (int i = 0; i <= pts; ++i)
        {
            const float x = -2.6f + (static_cast<float>(i) / static_cast<float>(pts)) * 5.2f;
            const float yS = static_cast<float>(sylredhel::origami::OrigamiWaveshaper::evalSatStatic(x, ceilLin, curve));
            const float yF = static_cast<float>(sylredhel::origami::OrigamiWaveshaper::evalOrigamiFold(x, ceilLin, shape, depth));
            const float px = mapX(x);
            if (i == 0) { pSat.startNewSubPath(px, mapY(yS)); pFold.startNewSubPath(px, mapY(yF)); }
            else        { pSat.lineTo(px, mapY(yS));          pFold.lineTo(px, mapY(yF)); }
        }

        g.setColour(kOchre.withAlpha(0.75f));
        g.strokePath(pSat, juce::PathStrokeType(1.65f));
        g.setColour(kSageTeal);
        g.strokePath(pFold, juce::PathStrokeType(2.15f));

        g.restoreState();

        g.setFont(juce::Font(juce::FontOptions(8.4f, juce::Font::bold)));
        g.setColour(kOchre);
        g.drawText("SAT BODY", static_cast<int>(plot.getX()), static_cast<int>(plot.getBottom() - 10), 68, 12, juce::Justification::centredLeft);
        g.setColour(kSageTeal);
        g.drawText("ORIGAMI FOLD", static_cast<int>(plot.getRight() - 88), static_cast<int>(plot.getBottom() - 10), 88, 12, juce::Justification::centredRight);
    }

private:
    sylredhel::origami::SatCurve  curve   = sylredhel::origami::SatCurve::Tape;
    sylredhel::origami::FoldShape shape   = sylredhel::origami::FoldShape::SmoothFold;
    float depth   = 1.0f;
    float ceilLin = 0.966f;
};
