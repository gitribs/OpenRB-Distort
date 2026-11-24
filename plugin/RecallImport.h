#pragma once

#include <JuceHeader.h>

#include <vector>

struct DistRecall
{
    int amount = 0;
    int shape = 0;
    int enabled = 1;
};

struct DistPrintEvent
{
    int tick = 0;
    const char* id = "";
    int value = 0;
};

struct DistPrint
{
    int ppq = 192;
    std::vector<DistPrintEvent> events;
};

// empty = success; otherwise a short reason
juce::String parseRecall (const juce::String& text, DistRecall& out);
void applyRecall (juce::AudioProcessorValueTreeState& apvts, const DistRecall& values);

// empty = success. Missing or non-v1 print is success with no events.
juce::String parsePrint (const juce::String& text, DistPrint& out);
