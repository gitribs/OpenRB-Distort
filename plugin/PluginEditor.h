#pragma once

#include "PluginProcessor.h"

class DistLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DistLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class DistEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DistEditor (DistProcessor&);
    ~DistEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob (juce::Slider& s);

    DistLookAndFeel lnf;

    juce::Slider amount, shape;
    juce::Label title, amountN, shapeN;

    juce::AudioProcessorValueTreeState::SliderAttachment amountA, shapeA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistEditor)
};
