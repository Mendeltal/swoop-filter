#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class SwoopAudioProcessor : public juce::AudioProcessor
{
public:
    SwoopAudioProcessor();
    ~SwoopAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Swoop Filter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor to animate the curve
    std::atomic<float> modulatedCutoff { 1000.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float tickLfo (int numSamples, int shape, float rate);

    static constexpr int maxStages = 4;
    static constexpr int chunkSize = 32;

    std::array<juce::dsp::StateVariableTPTFilter<float>, maxStages> filters;
    juce::dsp::Reverb reverb;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::AudioBuffer<float> dry { 2, chunkSize };

    double sampleRate = 44100.0;
    float lfoPhase = 0.f, randomValue = 0.f, smoothedAmount = 0.f;
    juce::Random rng;

    std::atomic<float> *pCutoff, *pRes, *pType, *pSlope, *pRate, *pDepth, *pShape,
                       *pBoost, *pMix, *pReverb, *pChorus, *pDrive, *pSpread, *pPhaser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwoopAudioProcessor)
};
