#pragma once

#include <JuceHeader.h>

struct DistRecall
{
    int amount = 0;
    int shape = 0;
};

// empty = success; otherwise a short reason
juce::String parseRecall (const juce::String& text, DistRecall& out);
void applyRecall (juce::AudioProcessorValueTreeState& apvts, const DistRecall& values);
