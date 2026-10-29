#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SwoopAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };
    auto addChoice = [&] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };
    auto addBool = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    addFloat  ("cutoff",   "Cutoff",    { 20.f, 20000.f, 0.01f, 0.3f }, 1000.f);
    addFloat  ("res",      "Resonance", { 0.5f, 12.f, 0.01f, 0.5f }, 0.707f);
    addChoice ("type",     "Type",      { "Low Pass", "High Pass", "Band Pass", "No Pass" }, 0);
    addChoice ("slope",    "Slope",     { "12 dB", "24 dB", "36 dB", "48 dB" }, 1);
    addFloat  ("lfoRate",  "LFO Rate",  { 0.05f, 20.f, 0.01f, 0.4f }, 1.f);
    addFloat  ("lfoDepth", "LFO Depth", { 0.f, 1.f }, 0.f);
    addChoice ("lfoShape", "LFO Shape", { "Sine", "Triangle", "Square", "Saw", "Random" }, 0);
    addFloat  ("fxBoost",  "FX Boost",  { 0.f, 1.f }, 0.3f);
    addFloat  ("mix",      "Mix",       { 0.f, 1.f }, 1.f);
    addBool   ("reverb",   "Reverb",    true);
    addBool   ("chorus",   "Chorus",    true);
    addBool   ("drive",    "Saturate",  false);
    addBool   ("spread",   "Spread",    false);
    addBool   ("phaser",   "Phaser",    false);

    return { p.begin(), p.end() };
}

SwoopAudioProcessor::SwoopAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pCutoff = apvts.getRawParameterValue ("cutoff");
    pRes    = apvts.getRawParameterValue ("res");
    pType   = apvts.getRawParameterValue ("type");
    pSlope  = apvts.getRawParameterValue ("slope");
    pRate   = apvts.getRawParameterValue ("lfoRate");
    pDepth  = apvts.getRawParameterValue ("lfoDepth");
    pShape  = apvts.getRawParameterValue ("lfoShape");
    pBoost  = apvts.getRawParameterValue ("fxBoost");
    pMix    = apvts.getRawParameterValue ("mix");
    pReverb = apvts.getRawParameterValue ("reverb");
    pChorus = apvts.getRawParameterValue ("chorus");
    pDrive  = apvts.getRawParameterValue ("drive");
    pSpread = apvts.getRawParameterValue ("spread");
    pPhaser = apvts.getRawParameterValue ("phaser");
}

bool SwoopAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SwoopAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) chunkSize, 2 };

    for (auto& f : filters) { f.prepare (spec); f.reset(); }
    reverb.prepare (spec);  reverb.reset();
    chorus.prepare (spec);  chorus.reset();
    phaser.prepare (spec);  phaser.reset();

    lfoPhase = 0.f;
    smoothedAmount = 0.f;
}

float SwoopAudioProcessor::tickLfo (int n, int shape, float rate)
{
    float v = 0.f;
    switch (shape)
    {
        case 0:  v = std::sin (juce::MathConstants<float>::twoPi * lfoPhase); break;
        case 1:  v = 1.f - 4.f * std::abs (lfoPhase - 0.5f); break;
        case 2:  v = lfoPhase < 0.5f ? 1.f : -1.f; break;
        case 3:  v = 2.f * lfoPhase - 1.f; break;
        default: v = randomValue; break;
    }

    lfoPhase += rate * (float) n / (float) sampleRate;
    while (lfoPhase >= 1.f)
    {
        lfoPhase -= 1.f;
        randomValue = rng.nextFloat() * 2.f - 1.f;
    }
    return v;
}

void SwoopAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;

    const float cutoff = pCutoff->load();
    const float res    = pRes->load();
    const int   type   = (int) pType->load();
    const int   stages = juce::jlimit (1, maxStages, (int) pSlope->load() + 1);
    const float rate   = pRate->load();
    const float depth  = pDepth->load();
    const int   shape  = (int) pShape->load();
    const float boost  = pBoost->load();
    const float mix    = pMix->load();
    const bool  fxRev  = pReverb->load() > 0.5f;
    const bool  fxCho  = pChorus->load() > 0.5f;
    const bool  fxDrv  = pDrive->load()  > 0.5f;
    const bool  fxSpr  = pSpread->load() > 0.5f;
    const bool  fxPha  = pPhaser->load() > 0.5f;

    const float resNorm = juce::jlimit (0.f, 1.f, (res - 0.5f) / 11.5f);
    const int total = buffer.getNumSamples();
    juce::dsp::AudioBlock<float> full (buffer);

    for (int start = 0; start < total; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, total - start);
        auto blk = full.getSubsetChannelBlock (0, 2).getSubBlock ((size_t) start, (size_t) n);

        for (int ch = 0; ch < 2; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, start, n);

        // ---- LFO -> cutoff (up to 3 octaves each way)
        const float lfo = tickLfo (n, shape, rate);
        const float modCut = juce::jlimit (20.f, (float) (0.45 * sampleRate),
                                           cutoff * std::pow (2.f, lfo * depth * 3.f));
        modulatedCutoff.store (modCut);

        // ---- filter
        if (type < 3)
        {
            const auto t = type == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                         : type == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                                     : juce::dsp::StateVariableTPTFilterType::bandpass;

            for (int s = 0; s < stages; ++s)
            {
                filters[(size_t) s].setType (t);
                filters[(size_t) s].setCutoffFrequency (modCut);
                filters[(size_t) s].setResonance (s == 0 ? res : 0.707f);
                juce::dsp::ProcessContextReplacing<float> ctx (blk);
                filters[(size_t) s].process (ctx);
            }
        }

        // ---- the "macro": FX amount follows cutoff + resonance (+ boost)
        const float norm = juce::jlimit (0.f, 1.f, std::log (modCut / 20.f) / std::log (1000.f));
        float macro = norm;
        if (type == 0) macro = 1.f - norm;
        if (type == 2) macro = 1.f - std::abs (norm - 0.5f) * 2.f;

        const float target = juce::jlimit (0.f, 1.f, (macro * 0.8f + resNorm * 0.35f) * (1.f + boost * 1.5f));
        smoothedAmount += 0.2f * (target - smoothedAmount);
        const float amt = smoothedAmount;

        // ---- FX rack
        {
            juce::Reverb::Parameters rp;
            rp.roomSize = 0.75f; rp.damping = 0.5f; rp.width = 1.f;
            rp.dryLevel = 1.f;   rp.wetLevel = fxRev ? amt * 0.35f : 0.f;
            reverb.setParameters (rp);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            reverb.process (ctx);
        }
        {
            chorus.setRate (0.9f); chorus.setDepth (0.35f);
            chorus.setCentreDelay (8.f); chorus.setFeedback (0.1f);
            chorus.setMix (fxCho ? amt * 0.6f : 0.f);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            chorus.process (ctx);
        }
        {
            phaser.setRate (0.35f); phaser.setDepth (0.8f);
            phaser.setCentreFrequency (900.f); phaser.setFeedback (0.55f);
            phaser.setMix (fxPha ? amt * 0.7f : 0.f);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            phaser.process (ctx);
        }

        if (fxDrv)
        {
            const float drive = 1.f + amt * 8.f;
            const float comp = 1.f / std::sqrt (drive);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = blk.getChannelPointer ((size_t) ch);
                for (int i = 0; i < n; ++i)
                    w[i] = w[i] * (1.f - amt) + std::tanh (w[i] * drive) * comp * amt;
            }
        }

        if (fxSpr)
        {
            auto* l = blk.getChannelPointer (0);
            auto* r = blk.getChannelPointer (1);
            const float widen = 1.f + amt * 1.5f;
            for (int i = 0; i < n; ++i)
            {
                const float mid  = (l[i] + r[i]) * 0.5f;
                const float side = (l[i] - r[i]) * 0.5f * widen;
                l[i] = mid + side;
                r[i] = mid - side;
            }
        }

        // ---- dry / wet
        if (mix < 0.999f)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* w = blk.getChannelPointer ((size_t) ch);
                auto* d = dry.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                    w[i] = w[i] * mix + d[i] * (1.f - mix);
            }
        }
    }
}

juce::AudioProcessorEditor* SwoopAudioProcessor::createEditor()
{
    return new SwoopAudioProcessorEditor (*this);
}

void SwoopAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void SwoopAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwoopAudioProcessor();
}
