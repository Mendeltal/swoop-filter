#include "PluginEditor.h"

SwoopAudioProcessorEditor::SwoopAudioProcessorEditor (SwoopAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    setLookAndFeel (&laf);

    addAndMakeVisible (view);

    setupKnob (cutoff, "CUTOFF",    "cutoff",   " Hz");
    setupKnob (res,    "RESONANCE", "res",      "");
    setupKnob (boost,  "FX BOOST",  "fxBoost",  "");
    setupKnob (mix,    "MIX",       "mix",      "");
    setupKnob (rate,   "LFO RATE",  "lfoRate",  " Hz");
    setupKnob (depth,  "LFO DEPTH", "lfoDepth", "");

    setupCombo (type,  "TYPE",  "type",     { "Low Pass", "High Pass", "Band Pass", "No Pass" });
    setupCombo (slope, "SLOPE", "slope",    { "12 dB", "24 dB", "36 dB", "48 dB" });
    setupCombo (shape, "LFO",   "lfoShape", { "Sine", "Triangle", "Square", "Saw", "Random" });

    setupPill (btnReverb, "reverb", aRev);
    setupPill (btnChorus, "chorus", aCho);
    setupPill (btnDrive,  "drive",  aDrv);
    setupPill (btnSpread, "spread", aSpr);
    setupPill (btnPhaser, "phaser", aPha);

    setSize (720, 500);
}

SwoopAudioProcessorEditor::~SwoopAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SwoopAudioProcessorEditor::setupKnob (Knob& k, const juce::String& name, const char* id,
                                           const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, swoop::text.withAlpha (0.75f));
    addAndMakeVisible (k.label);

    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void SwoopAudioProcessorEditor::setupCombo (Combo& c, const juce::String& name, const char* id,
                                            const juce::StringArray& items)
{
    c.box.addItemList (items, 1);
    addAndMakeVisible (c.box);

    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredRight);
    c.label.setColour (juce::Label::textColourId, swoop::text.withAlpha (0.75f));
    addAndMakeVisible (c.label);

    c.att = std::make_unique<ComboAtt> (proc.apvts, id, c.box);
}

void SwoopAudioProcessorEditor::setupPill (juce::ToggleButton& b, const char* id,
                                           std::unique_ptr<ButtonAtt>& att)
{
    addAndMakeVisible (b);
    att = std::make_unique<ButtonAtt> (proc.apvts, id, b);
}

void SwoopAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (swoop::bg2, 0.f, 0.f, swoop::bg1, 0.f, (float) getHeight(), false));
    g.fillAll();

    // soft glow blobs
    auto blob = [&] (float cx, float cy, float r, Colour c)
    {
        g.setGradientFill (ColourGradient (c.withAlpha (0.16f), cx, cy, c.withAlpha (0.f), cx + r, cy, true));
        g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    };
    blob (80.f, 60.f, 260.f, swoop::magenta);
    blob ((float) getWidth() - 60.f, (float) getHeight() - 40.f, 300.f, swoop::cyan);

    // title
    g.setColour (swoop::text);
    g.setFont (24.f);
    g.drawText ("SWOOP FILTER", 24, 14, 300, 30, Justification::centredLeft);
    g.setGradientFill (ColourGradient (swoop::cyan, 24.f, 0.f, swoop::magenta, 190.f, 0.f, false));
    g.fillRoundedRectangle (24.f, 46.f, 166.f, 3.f, 1.5f);
}

void SwoopAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (40);

    view.setBounds (r.removeFromTop (170));
    r.removeFromTop (8);

    // knob row
    auto row = r.removeFromTop (150);
    Knob* knobs[] = { &cutoff, &res, &boost, &mix, &rate, &depth };
    const int kw = row.getWidth() / 6;
    for (auto* k : knobs)
    {
        auto a = row.removeFromLeft (kw);
        k->label.setBounds (a.removeFromTop (18));
        k->slider.setBounds (a);
    }

    r.removeFromTop (8);

    // combos
    auto crow = r.removeFromTop (28);
    Combo* combos[] = { &type, &slope, &shape };
    const int cw = crow.getWidth() / 3;
    for (auto* c : combos)
    {
        auto a = crow.removeFromLeft (cw).reduced (6, 0);
        c->label.setBounds (a.removeFromLeft (52));
        c->box.setBounds (a.reduced (4, 0));
    }

    r.removeFromTop (14);

    // fx pills
    auto prow = r.removeFromTop (30);
    juce::ToggleButton* pills[] = { &btnReverb, &btnChorus, &btnDrive, &btnSpread, &btnPhaser };
    const int pw = prow.getWidth() / 5;
    for (auto* b : pills)
        b->setBounds (prow.removeFromLeft (pw).reduced (4, 0));
}
