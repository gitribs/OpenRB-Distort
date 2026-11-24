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

void expectTrue (bool v, const char* msg)
{
    if (! v)
    {
        std::cerr << "FAIL: " << msg << "\n";
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

const juce::String kWithPrint =
    "openrb-distort 1\n"
    "amount: 70\n"
    "shape: 0\n"
    "\n"
    "openrb-print 1\n"
    "ppq: 192\n"
    "0 amount 70\n"
    "768 amount 90\n"
    "1536 shape 20\n";
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    {
        DistRecall rec;
        expectString (parseRecall (kModloader, rec), {}, "parse modloader");
        expectEqual (rec.amount, 70, "modloader amount");
        expectEqual (rec.shape, 0, "modloader shape");
        expectEqual (rec.enabled, 1, "missing enabled defaults on");
    }

    {
        DistRecall rec;
        expectString (parseRecall (kAlt, rec), {}, "parse alt");
        expectEqual (rec.amount, 22, "alt amount");
        expectEqual (rec.shape, 99, "alt shape");
        expectEqual (rec.enabled, 1, "alt enabled default");
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
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "default enabled on");
        expectString (proc.importRecall (kAlt), {}, "import alt");
        expectEqual (paramInt (proc.apvts, "amount"), 22, "apply amount");
        expectEqual (paramInt (proc.apvts, "shape"), 99, "apply shape");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "apply enabled default");
        auto* enabled = proc.apvts.getParameter ("enabled");
        expectTrue (enabled != nullptr, "enabled param exists");
        expectTrue (enabled->convertTo0to1 (0.0f) == 0.0f, "bool convert 0");
        expectTrue (enabled->convertTo0to1 (1.0f) == 1.0f, "bool convert 1");
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

    {
        DistRecall rec;
        expectString (parseRecall (kWithPrint, rec), {}, "recall with print appendix");
        expectEqual (rec.amount, 70, "appendix recall amount");
        expectEqual (rec.shape, 0, "appendix recall shape");

        DistPrint printed;
        expectString (parsePrint (kWithPrint, printed), {}, "parse print appendix");
        expectEqual (printed.ppq, 192, "print ppq");
        expectEqual ((int) printed.events.size(), 3, "print event count");
        expectEqual (printed.events[0].tick, 0, "print tick 0");
        expectString (juce::String (printed.events[0].id), "amount", "print id 0 amount");
        expectEqual (printed.events[0].value, 70, "print amount 70");
        expectEqual (printed.events[1].tick, 768, "print tick amount 90");
        expectString (juce::String (printed.events[1].id), "amount", "print id 1 amount");
        expectEqual (printed.events[1].value, 90, "print amount 90");
        expectEqual (printed.events[2].tick, 1536, "print tick shape");
        expectString (juce::String (printed.events[2].id), "shape", "print id shape");
        expectEqual (printed.events[2].value, 20, "print shape 20");
    }

    {
        DistPrint printed;
        expectString (parsePrint (kModloader, printed), {}, "recall-only print");
        expectEqual ((int) printed.events.size(), 0, "recall-only empty events");
        expectEqual (printed.ppq, 192, "recall-only ppq");
    }

    {
        DistPrint printed;
        const juce::String unknown =
            kModloader
            + "\nopenrb-print 1\n"
              "ppq: 192\n"
              "0 amount 70\n"
              "0 enabled 1\n"
              "768 amount 90\n"
              "100 banana 3\n";
        expectString (parsePrint (unknown, printed), {}, "unknown print params");
        expectEqual ((int) printed.events.size(), 3, "keep enabled, skip banana");
        expectString (juce::String (printed.events[0].id), "amount", "kept amount start");
        expectString (juce::String (printed.events[1].id), "enabled", "kept enabled");
        expectEqual (printed.events[1].value, 1, "enabled value 1");
        expectString (juce::String (printed.events[2].id), "amount", "kept amount move");
        expectEqual (printed.events[2].tick, 768, "kept amount tick");
    }

    {
        DistRecall rec;
        DistPrint printed;
        const juce::String v2 =
            kModloader
            + "\nopenrb-print 2\n"
              "ppq: 192\n"
              "0 amount 70\n";
        expectString (parseRecall (v2, rec), {}, "recall still ok on print v2");
        expectEqual (rec.amount, 70, "print v2 amount snapshot");
        expectString (parsePrint (v2, printed), {}, "print v2 ignored");
        expectEqual ((int) printed.events.size(), 0, "print v2 empty");
    }

    {
        DistPrint printed;
        const juce::String scaled =
            kModloader
            + "\nopenrb-print 1\n"
              "ppq: 96\n"
              "0 amount 70\n"
              "384 amount 90\n";
        expectString (parsePrint (scaled, printed), {}, "scale ppq");
        expectEqual (printed.ppq, 192, "scaled ppq is 192");
        expectEqual ((int) printed.events.size(), 2, "scaled events");
        expectEqual (printed.events[1].tick, 768, "tick scaled 96 to 192");
    }

    {
        DistProcessor proc;
        expectString (proc.importRecall (kWithPrint), {}, "import with print");
        expectTrue (proc.isPrintArmed(), "import arms print");
        expectEqual (paramInt (proc.apvts, "amount"), 70, "import print amount");
        expectEqual (paramInt (proc.apvts, "shape"), 0, "import print shape");
        const auto events = proc.copyPrintEvents();
        expectEqual ((int) events.size(), 3, "import stored print events");
        expectTrue (proc.hasPrintTake(), "import loaded print take");
        expectEqual (events[0].tick, 0, "stored tick 0");
        expectString (juce::String (events[0].id), "amount", "stored amount");
        expectEqual (events[2].value, 20, "stored shape value");

        expectString (proc.importRecall (kModloader), {}, "import recall-only after print");
        expectEqual ((int) proc.copyPrintEvents().size(), 0, "recall-only clears print");
        expectTrue (! proc.hasPrintTake(), "recall-only unloads print");
        expectTrue (! proc.isPrintArmed(), "recall-only disarms print");
        expectEqual (paramInt (proc.apvts, "amount"), 70, "recall-only amount stays");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "recall-only enabled stays on");
    }

    {
        DistRecall rec;
        const juce::String off =
            "openrb-distort 1\n"
            "amount: 70\n"
            "shape: 0\n"
            "enabled: 0\n";
        expectString (parseRecall (off, rec), {}, "parse enabled 0");
        expectEqual (rec.enabled, 0, "snapshot enabled 0");

        DistProcessor proc;
        expectString (proc.importRecall (off), {}, "import enabled 0");
        expectEqual (paramInt (proc.apvts, "enabled"), 0, "apply enabled off");
        expectString (proc.importRecall (kModloader), {}, "import missing enabled");
        expectEqual (paramInt (proc.apvts, "enabled"), 1, "missing enabled restores on");
    }

    {
        DistRecall rec;
        const juce::String clamped =
            "openrb-distort 1\n"
            "amount: 10\n"
            "shape: 20\n"
            "enabled: 5\n";
        expectString (parseRecall (clamped, rec), {}, "clamp enabled");
        expectEqual (rec.enabled, 1, "enabled 5 clamps to 1");
    }

    {
        DistRecall rec;
        expectString (parseRecall ("openrb-distort 1\namount: 1\nshape: 2\nenabled: banana\n", rec),
                      "invalid enabled",
                      "bad enabled");
    }

    {
        const juce::String tick0 =
            "openrb-distort 1\n"
            "amount: 70\n"
            "shape: 0\n"
            "enabled: 1\n"
            "\n"
            "openrb-print 1\n"
            "ppq: 192\n"
            "0 enabled 0\n";
        DistPrint printed;
        expectString (parsePrint (tick0, printed), {}, "parse tick-0 enabled");
        expectEqual ((int) printed.events.size(), 1, "tick-0 enabled event");
        expectString (juce::String (printed.events[0].id), "enabled", "tick-0 id");
        expectEqual (printed.events[0].tick, 0, "tick-0");
        expectEqual (printed.events[0].value, 0, "tick-0 value off");

        DistProcessor proc;
        expectString (proc.importRecall (tick0), {}, "import tick-0 enabled");
        expectTrue (proc.hasPrintTake(), "tick-0 enable loads print");
        expectTrue (proc.isPrintArmed(), "tick-0 enable arms print");
        const auto events = proc.copyPrintEvents();
        expectEqual ((int) events.size(), 1, "stored tick-0 enable");
        expectString (juce::String (events[0].id), "enabled", "stored enable id");
        // Always-lane: seed writes tick 0 even though nothing moves later.
        expectEqual (paramInt (proc.apvts, "enabled"), 0, "tick-0 enable is a moving lane");
    }

    if (gFails != 0)
    {
        std::cerr << gFails << " failure(s)\n";
        return 1;
    }

    std::cout << "All recall import tests passed\n";
    return 0;
}
