#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class TapeBoxProcessor : public juce::AudioProcessor
{
public:
    TapeBoxProcessor();
    ~TapeBoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TapeBox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Nivel de saida (pico) lido pela interface para os VU meters
    std::atomic<float> meterL { 0.f }, meterR { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pDrive = nullptr, *pColor = nullptr, *pWow = nullptr, *pNoise = nullptr,
                       *pTone = nullptr, *pOutput = nullptr, *pAge = nullptr, *pTape = nullptr,
                       *pComp = nullptr;

    double sr = 44100.0;
    float baseDelaySamples = 0.f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::dsp::StateVariableTPTFilter<float> lpA, lpB, bump, toneLP;
    juce::SmoothedValue<float> driveSm, outSm;
    juce::Random rnd;

    float wowPhase = 0.f, flutPhase = 0.f, drift = 0.f, env = 0.f;
    float dcX[2] {}, dcY[2] {}, nLP[2] {};
    bool firstBlock = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBoxProcessor)
};
