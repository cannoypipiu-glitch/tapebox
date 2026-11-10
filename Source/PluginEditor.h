#pragma once
#include "PluginProcessor.h"
#include <array>

class TapeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 11; }
};

class TapeBoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TapeBoxEditor (TapeBoxProcessor&);
    ~TapeBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawVU (juce::Graphics&, juce::Rectangle<float>, float needle, const juce::String& label);
    void drawCassette (juce::Graphics&, juce::Rectangle<float>);
    void drawReel (juce::Graphics&, juce::Point<float> centre, float woundRadius, float angle);

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    struct Knob   { juce::Slider slider; std::unique_ptr<SliderAtt> att; juce::String name; };
    struct Switch { juce::Slider slider; std::unique_ptr<SliderAtt> att; juce::String name; juce::StringArray labels; };

    TapeBoxProcessor& proc;
    TapeLookAndFeel lnf; // declarado antes dos sliders (destruido depois deles)
    std::array<Knob, 6> knobs;
    std::array<Switch, 3> switches;

    float vuL = 0.f, vuR = 0.f, reelAngle = 0.f, tapeProg = 0.f;
    int frame = 0, counter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeBoxEditor)
};
