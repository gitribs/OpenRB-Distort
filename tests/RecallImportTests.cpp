#include "PluginProcessor.h"
#include "RecallImport.h"

#include <iostream>

namespace
{
int gFails = 0;

void expectEqual (int a, int b, const char* msg)
{
    if (a != b)
    {
        std::cerr << "FAIL: " << msg << " (" << a << " != " << b << ")\n";
        ++gFails;
    }
}

void expectString (const juce::String& a, const juce::String& b, const char* msg)
{
    if (a != b)
    {
        std::cerr << "FAIL: " << msg << " (\"" << a << "\" != \"" << b << "\")\n";
        ++gFails;
    }
}

int paramInt (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    if (auto* v = apvts.getRawParameterValue (id))
        return juce::roundToInt (v->load());
    return -999;
}

const juce::String kModloader =
    "openrb-distort 1\n"
    "amount: 70\n"
    "shape: 0\n";

const juce::String kAlt =
    "openrb-distort 1\n"
    "amount: 22\n"
    "shape: 99\n";
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    {
        DistRecall rec;
        expectString (parseRecall (kModloader, rec), {}, "parse modloader");
        expectEqual (rec.amount, 70, "modloader amount");
        expectEqual (rec.shape, 0, "modloader shape");
    }

    {
        DistRecall rec;
        expectString (parseRecall (kAlt, rec), {}, "parse alt");
        expectEqual (rec.amount, 22, "alt amount");
        expectEqual (rec.shape, 99, "alt shape");
    }

    {
        DistRecall rec;
        expectString (parseRecall ("openrb-delay 1\nmix: 22\n", rec),
                      "not OpenRB Distort data",
                      "wrong header");
    }

    {
        DistRecall rec;
        expectString (parseRecall ("openrb-distort 2\namount: 1\nshape: 2\n", rec),
                      "unsupported recall version",
                      "bad version");
    }

    {
        DistRecall rec;
        expectString (parseRecall ("openrb-distort 1\namount: 10\n", rec),
                      "missing shape",
                      "missing key");
    }

    {
        DistRecall rec;
        const juce::String banana =
            "openrb-distort 1\n"
            "amount: 10\n"
            "shape: 20\n"
            "mode: banana\n";
        expectString (parseRecall (banana, rec), {}, "mode banana ignored");
        expectEqual (rec.amount, 10, "banana amount");
        expectEqual (rec.shape, 20, "banana shape");
    }

    {
        DistRecall rec;
        const juce::String clamped =
            "openrb-distort 1\n"
            "amount: 200\n"
            "shape: -3\n"
            "unknown: 1\n";
        expectString (parseRecall (clamped, rec), {}, "clamp parse");
        expectEqual (rec.amount, 127, "clamp amount");
        expectEqual (rec.shape, 0, "clamp shape");
    }

    {
        DistProcessor proc;
        expectString (proc.importRecall (kAlt), {}, "import alt");
        expectEqual (paramInt (proc.apvts, "amount"), 22, "apply amount");
        expectEqual (paramInt (proc.apvts, "shape"), 99, "apply shape");
    }

    {
        DistProcessor proc;
        const int amount = paramInt (proc.apvts, "amount");
        const int shape = paramInt (proc.apvts, "shape");
        expectString (proc.importRecall ("openrb-pcf 1\nfrequency: 1\n"),
                      "not OpenRB Distort data",
                      "cross plugin");
        expectEqual (paramInt (proc.apvts, "amount"), amount, "no amount change on fail");
        expectEqual (paramInt (proc.apvts, "shape"), shape, "no shape change on fail");
    }

    if (gFails != 0)
    {
        std::cerr << gFails << " failure(s)\n";
        return 1;
    }

    std::cout << "All recall import tests passed\n";
    return 0;
}
