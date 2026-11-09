#include "RecallImport.h"

namespace
{
bool parseIntValue (const juce::String& s, int& out)
{
    const auto t = s.trim();
    if (t.isEmpty())
        return false;

    int i = 0;
    if (t[0] == '-' || t[0] == '+')
        ++i;
    if (i >= t.length())
        return false;

    for (int n = t.length(); i < n; ++i)
        if (! juce::CharacterFunctions::isDigit (t[i]))
            return false;

    out = t.getIntValue();
    return true;
}

void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float actual)
{
    auto* p = apvts.getParameter (id);
    if (p == nullptr)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (actual));
    p->endChangeGesture();
}
} // namespace

juce::String parseRecall (const juce::String& text, DistRecall& out)
{
    const auto lines = juce::StringArray::fromLines (text);
    int line = 0;
    while (line < lines.size() && lines[line].trim().isEmpty())
        ++line;
    if (line >= lines.size())
        return "not OpenRB Distort data";

    const auto header = juce::StringArray::fromTokens (lines[line].trim(), " \t", {});
    if (header.isEmpty() || header[0] != "openrb-distort")
        return "not OpenRB Distort data";
    if (header.size() < 2 || header[1] != "1")
        return "unsupported recall version";

    bool haveAmt = false, haveShape = false;
    DistRecall parsed;

    for (++line; line < lines.size(); ++line)
    {
        const auto trimmed = lines[line].trim();
        if (trimmed.isEmpty())
            continue;

        const int colon = trimmed.indexOfChar (':');
        if (colon < 0)
            continue;

        const auto key = trimmed.substring (0, colon).trim();
        const auto value = trimmed.substring (colon + 1).trim();

        auto takeInt = [&] (int lo, int hi, int& dest, bool& flag) -> juce::String
        {
            int n = 0;
            if (! parseIntValue (value, n))
                return "invalid " + key;
            dest = juce::jlimit (lo, hi, n);
            flag = true;
            return {};
        };

        juce::String err;
        if (key == "amount")
            err = takeInt (0, 127, parsed.amount, haveAmt);
        else if (key == "shape")
            err = takeInt (0, 127, parsed.shape, haveShape);

        if (err.isNotEmpty())
            return err;
    }

    if (! haveAmt)
        return "missing amount";
    if (! haveShape)
        return "missing shape";

    out = parsed;
    return {};
}

void applyRecall (juce::AudioProcessorValueTreeState& apvts, const DistRecall& values)
{
    setParam (apvts, "amount", (float) values.amount);
    setParam (apvts, "shape", (float) values.shape);
}
