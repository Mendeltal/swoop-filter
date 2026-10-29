#pragma once
#include "PluginProcessor.h"

namespace swoop
{
    static const juce::Colour bg1     { 0xff0a0a14 };
    static const juce::Colour bg2     { 0xff1c1038 };
    static const juce::Colour cyan    { 0xff00e5ff };
    static const juce::Colour magenta { 0xffff3df2 };
    static const juce::Colour text    { 0xffcfd3ff };
    static const juce::Colour panel   { 0xff262645 };
}

//==============================================================================
// Neon look: glowing arc knobs, pill toggles, dark combo boxes
class NeonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel()
    {
        using namespace juce;
        setColour (ComboBox::backgroundColourId, Colour (0xff15152a));
        setColour (ComboBox::textColourId, swoop::text);
        setColour (ComboBox::outlineColourId, swoop::panel);
        setColour (ComboBox::arrowColourId, swoop::cyan);
        setColour (PopupMenu::backgroundColourId, Colour (0xff15152a));
        setColour (PopupMenu::textColourId, swoop::text);
        setColour (PopupMenu::highlightedBackgroundColourId, swoop::magenta.withAlpha (0.35f));
        setColour (Slider::textBoxTextColourId, swoop::text);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Label::textColourId, swoop::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startA, float endA, juce::Slider&) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.f);
        const float d = jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;
        const float ang = startA + pos * (endA - startA);

        Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.f, startA, endA, true);
        g.setColour (swoop::panel);
        g.strokePath (track, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));

        Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.f, startA, ang, true);
        g.setColour (swoop::magenta.withAlpha (0.18f));
        g.strokePath (val, PathStrokeType (12.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (ColourGradient (swoop::cyan, b.getX(), b.getBottom(),
                                           swoop::magenta, b.getRight(), b.getY(), false));
        g.strokePath (val, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::rounded));

        // knob body
        const float br = r * 0.68f;
        g.setGradientFill (ColourGradient (Colour (0xff2e2e55), c.x, c.y - br,
                                           Colour (0xff12122a), c.x, c.y + br, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour (swoop::panel.brighter (0.2f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.f);

        // pointer
        Path p;
        p.addRoundedRectangle (-1.8f, -br + 4.f, 3.6f, br * 0.45f, 1.8f);
        g.setColour (swoop::cyan);
        g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& btn, bool hover, bool) override
    {
        using namespace juce;
        auto b = btn.getLocalBounds().toFloat().reduced (2.f);
        const bool on = btn.getToggleState();

        if (on)
        {
            g.setColour (swoop::cyan.withAlpha (0.2f));
            g.fillRoundedRectangle (b.expanded (2.f), b.getHeight() * 0.5f + 2.f);
            g.setGradientFill (ColourGradient (swoop::cyan, b.getX(), 0.f,
                                               swoop::magenta, b.getRight(), 0.f, false));
            g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
            g.setColour (Colour (0xff0a0a14));
        }
        else
        {
            g.setColour (hover ? swoop::panel.brighter (0.15f) : swoop::panel);
            g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.5f);
            g.setColour (swoop::text.withAlpha (0.7f));
        }
        g.setFont (13.f);
        g.drawText (btn.getButtonText(), b.toNearestInt(), Justification::centred);
    }
};

//==============================================================================
// Live filter response curve; follows the LFO-modulated cutoff
class ResponseView : public juce::Component, private juce::Timer
{
public:
    explicit ResponseView (SwoopAudioProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0x66000000));
        g.fillRoundedRectangle (b, 14.f);
        g.setColour (swoop::panel);
        g.drawRoundedRectangle (b.reduced (0.5f), 14.f, 1.f);

        auto ib = b.reduced (12.f, 14.f);

        g.setColour (Colours::white.withAlpha (0.06f));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            g.drawVerticalLine ((int) fToX (f, ib), ib.getY(), ib.getBottom());
        g.drawHorizontalLine ((int) dbToY (0.f, ib), ib.getX(), ib.getRight());

        const int   type   = (int) proc.apvts.getRawParameterValue ("type")->load();
        const int   stages = (int) proc.apvts.getRawParameterValue ("slope")->load() + 1;
        const float q      = proc.apvts.getRawParameterValue ("res")->load();
        const float fc     = proc.modulatedCutoff.load();

        Path curve;
        const int steps = jmax (2, (int) ib.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float f = 20.f * std::pow (1000.f, t);
            const float db = 20.f * std::log10 (jmax (magnitude (type, stages, f, fc, q), 1.0e-5f));
            const float x = ib.getX() + t * ib.getWidth();
            const float y = dbToY (db, ib);
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }

        Path fill (curve);
        fill.lineTo (ib.getRight(), ib.getBottom());
        fill.lineTo (ib.getX(), ib.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (swoop::cyan.withAlpha (0.35f), 0.f, ib.getY(),
                                           swoop::magenta.withAlpha (0.02f), 0.f, ib.getBottom(), false));
        g.fillPath (fill);

        g.setColour (swoop::cyan.withAlpha (0.25f));
        g.strokePath (curve, PathStrokeType (7.f));
        g.setColour (swoop::cyan);
        g.strokePath (curve, PathStrokeType (2.2f));

        // cutoff dot
        const float dx = fToX (fc, ib);
        const float dy = dbToY (20.f * std::log10 (jmax (magnitude (type, stages, fc, fc, q), 1.0e-5f)), ib);
        g.setColour (swoop::magenta.withAlpha (0.3f));
        g.fillEllipse (dx - 11.f, dy - 11.f, 22.f, 22.f);
        g.setColour (swoop::magenta);
        g.fillEllipse (dx - 5.f, dy - 5.f, 10.f, 10.f);
    }

private:
    void timerCallback() override { repaint(); }

    static float fToX (float f, juce::Rectangle<float> r)
    {
        return r.getX() + std::log (f / 20.f) / std::log (1000.f) * r.getWidth();
    }
    static float dbToY (float db, juce::Rectangle<float> r)
    {
        db = juce::jlimit (-48.f, 24.f, db);
        return r.getBottom() - (db + 48.f) / 72.f * r.getHeight();
    }
    static float magnitude (int type, int stages, float f, float fc, float q)
    {
        if (type >= 3) return 1.f;
        const float w = f / fc, w2 = w * w;
        float m = 1.f;
        for (int s = 0; s < stages; ++s)
        {
            const float Q = s == 0 ? q : 0.707f;
            const float den = std::sqrt ((1.f - w2) * (1.f - w2) + (w / Q) * (w / Q));
            const float num = type == 0 ? 1.f : type == 1 ? w2 : w / Q;
            m *= num / den;
        }
        return m;
    }

    SwoopAudioProcessor& proc;
};

//==============================================================================
class SwoopAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SwoopAudioProcessorEditor (SwoopAudioProcessor&);
    ~SwoopAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob  { juce::Slider slider; juce::Label label; std::unique_ptr<SliderAtt> att; };
    struct Combo { juce::ComboBox box;  juce::Label label; std::unique_ptr<ComboAtt> att; };

    void setupKnob  (Knob&, const juce::String& name, const char* id, const juce::String& suffix);
    void setupCombo (Combo&, const juce::String& name, const char* id, const juce::StringArray& items);
    void setupPill  (juce::ToggleButton&, const char* id, std::unique_ptr<ButtonAtt>&);

    SwoopAudioProcessor& proc;
    NeonLookAndFeel laf;
    ResponseView view;

    Knob cutoff, res, boost, mix, rate, depth;
    Combo type, slope, shape;
    juce::ToggleButton btnReverb { "REVERB" }, btnChorus { "CHORUS" }, btnDrive { "SATURATE" },
                       btnSpread { "SPREAD" }, btnPhaser { "PHASER" };
    std::unique_ptr<ButtonAtt> aRev, aCho, aDrv, aSpr, aPha;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwoopAudioProcessorEditor)
};
