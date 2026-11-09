#pragma once

#include "PluginProcessor.h"

#include <memory>

class DistLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DistLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class GearButton final : public juce::Button
{
public:
    GearButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class RecallImportPanel;

class DistEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DistEditor (DistProcessor&);
    ~DistEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob (juce::Slider& s);
    void setImportOpen (bool open);

    DistProcessor& owner;
    DistLookAndFeel lnf;

    juce::Slider amount, shape;
    juce::Label title, amountN, shapeN;
    GearButton gear;
    std::unique_ptr<RecallImportPanel> importPanel;

    juce::AudioProcessorValueTreeState::SliderAttachment amountA, shapeA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistEditor)
};
