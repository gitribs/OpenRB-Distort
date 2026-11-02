#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kEngineRate = 44100.0;
constexpr int kDistBlock = 64;

// Starting values for a newly inserted effect. Hosts restore project and preset
// state through setStateInformation, which replaces these after construction.
constexpr int kDefaultAmount = 70;
constexpr int kDefaultShape = 0;

bool ratesMatch (double a, double b)
{
    return std::abs (a - b) < 1.0e-6;
}
} // namespace

DistProcessor::DistProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      dist (orb_dist_new())
{
}

DistProcessor::~DistProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout DistProcessor::createLayout()
{
    using juce::AudioParameterInt;
    using juce::ParameterID;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "amount", 1 }, "Amount", 0, 127, kDefaultAmount));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "shape", 1 }, "Shape", 0, 127, kDefaultShape));
    return { p.begin(), p.end() };
}

void DistProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate > 0.0 ? sampleRate : kEngineRate;
    exact44100 = ratesMatch (hostRate, kEngineRate);
    maxHostBlock = std::max (samplesPerBlock, 1);

    const int extra = 256 + (int) juce::WindowedSincInterpolator::getBaseLatency();
    const int maxEngine = (int) std::ceil ((double) maxHostBlock * kEngineRate / std::min (hostRate, kEngineRate))
                          + extra + kDistBlock;

    hostFifo.setSize (2, maxHostBlock + extra);
    engineBuf.setSize (2, maxEngine);
    engineFifo.setSize (2, maxEngine + extra);
    monoScratch.setSize (1, maxEngine);
    hostFifo.clear();
    engineBuf.clear();
    engineFifo.clear();
    monoScratch.clear();
    hostFifoFill = 0;
    engineFifoFill = 0;

    for (auto& i : toEngine)
        i.reset();
    for (auto& i : toHost)
        i.reset();

    if (dist)
        orb_dist_reset (dist.get());

    setLatencySamples (latencySamples());
}

void DistProcessor::releaseResources() {}

void DistProcessor::reset()
{
    hostFifo.clear();
    engineBuf.clear();
    engineFifo.clear();
    hostFifoFill = 0;
    engineFifoFill = 0;
    for (auto& i : toEngine)
        i.reset();
    for (auto& i : toHost)
        i.reset();
    if (dist)
        orb_dist_reset (dist.get());
}

int DistProcessor::latencySamples() const
{
    const int distLatency = (int) std::llround ((double) kDistBlock * hostRate / kEngineRate);
    if (exact44100)
        return distLatency;

    const double base = juce::WindowedSincInterpolator::getBaseLatency();
    const double upHost = base;
    const double downHost = base * hostRate / kEngineRate;
    return distLatency + (int) std::llround (upHost + downHost);
}

bool DistProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

OrbDistParams DistProcessor::readParams (bool bypass) const
{
    auto asInt = [this] (const char* id, int fallback) -> int
    {
        if (auto* v = apvts.getRawParameterValue (id))
            return juce::roundToInt (v->load());
        return fallback;
    };

    OrbDistParams params {};
    params.amount = (uint8_t) juce::jlimit (0, 127, asInt ("amount", kDefaultAmount));
    params.shape = (uint8_t) juce::jlimit (0, 127, asInt ("shape", kDefaultShape));
    params.bypass = bypass ? 1 : 0;
    return params;
}

void DistProcessor::shiftFifo (juce::AudioBuffer<float>& fifo, int& fill, int used)
{
    if (used <= 0)
        return;
    if (used >= fill)
    {
        fill = 0;
        return;
    }
    const int remain = fill - used;
    for (int ch = 0; ch < fifo.getNumChannels(); ++ch)
        std::memmove (fifo.getWritePointer (ch), fifo.getReadPointer (ch) + used, (size_t) remain * sizeof (float));
    fill = remain;
}

int DistProcessor::hostToEngine (const float* left, const float* right, int nHost)
{
    const int cap = hostFifo.getNumSamples();
    const int take = juce::jmin (nHost, cap - hostFifoFill);
    if (take > 0)
    {
        std::memcpy (hostFifo.getWritePointer (0) + hostFifoFill, left, (size_t) take * sizeof (float));
        std::memcpy (hostFifo.getWritePointer (1) + hostFifoFill, right, (size_t) take * sizeof (float));
        hostFifoFill += take;
    }

    const double speed = hostRate / kEngineRate;
    int nEngine = (int) std::floor ((double) hostFifoFill / speed);
    nEngine = juce::jlimit (0, engineBuf.getNumSamples(), nEngine);
    if (nEngine <= 0)
        return 0;

    int used = 0;
    for (int ch = 0; ch < 2; ++ch)
        used = toEngine[ch].process (speed, hostFifo.getReadPointer (ch), engineBuf.getWritePointer (ch), nEngine);

    shiftFifo (hostFifo, hostFifoFill, used);
    return nEngine;
}

void DistProcessor::engineToHost (float* left, float* right, int nHost, int nEngine)
{
    const int cap = engineFifo.getNumSamples();
    const int take = juce::jmin (nEngine, cap - engineFifoFill);
    if (take > 0)
    {
        std::memcpy (engineFifo.getWritePointer (0) + engineFifoFill, engineBuf.getReadPointer (0),
                     (size_t) take * sizeof (float));
        std::memcpy (engineFifo.getWritePointer (1) + engineFifoFill, engineBuf.getReadPointer (1),
                     (size_t) take * sizeof (float));
        engineFifoFill += take;
    }

    const double speed = kEngineRate / hostRate;
    float* outs[2] = { left, right };
    int used = 0;
    for (int ch = 0; ch < 2; ++ch)
        used = toHost[ch].process (speed, engineFifo.getReadPointer (ch), outs[ch], nHost, engineFifoFill, 0);

    shiftFifo (engineFifo, engineFifoFill, juce::jmin (used, engineFifoFill));
}

void DistProcessor::processBuffer (juce::AudioBuffer<float>& buffer, bool bypass)
{
    juce::ScopedNoDenormals noDenormals;

    const int nHost = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (nHost <= 0 || nCh <= 0 || dist == nullptr)
        return;

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, nHost);

    float* left = buffer.getWritePointer (0);
    float* right = nCh > 1 ? buffer.getWritePointer (1) : monoScratch.getWritePointer (0);
    if (nCh == 1)
        std::memcpy (right, left, (size_t) nHost * sizeof (float));

    const auto params = readParams (bypass);

    if (exact44100)
    {
        orb_dist_process (dist.get(), left, right, nHost, &params);
        if (nCh == 1)
            std::memcpy (buffer.getWritePointer (0), left, (size_t) nHost * sizeof (float));
        return;
    }

    const int nEngine = hostToEngine (left, right, nHost);
    if (nEngine > 0)
        orb_dist_process (dist.get(), engineBuf.getWritePointer (0), engineBuf.getWritePointer (1), nEngine, &params);
    engineToHost (left, right, nHost, nEngine);
    if (nCh == 1)
        std::memcpy (buffer.getWritePointer (0), left, (size_t) nHost * sizeof (float));
}

void DistProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    processBuffer (buffer, false);
}

void DistProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    processBuffer (buffer, true);
}

juce::AudioProcessorEditor* DistProcessor::createEditor()
{
    return new DistEditor (*this);
}

void DistProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DistProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistProcessor();
}
