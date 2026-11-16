// Tobo Imager. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tobo;

const char* const ToboImagerProcessor::ids[kNumParams] = {
    "xo1", "xo2", "xo3",
    "width1", "width2", "width3", "width4",
    "stereoize1", "stereoize2", "stereoize3", "stereoize4",
    "delay", "type", "guard", "scatter", "solo", "monitor",
    "on1", "on2", "on3", "on4",
    "mute1", "mute2", "mute3", "mute4",
    "safe1", "safe2", "safe3", "safe4",
    "bands"
};

juce::AudioProcessorValueTreeState::ParameterLayout ToboImagerProcessor::layout()
{
    using F = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto pid = [] (int k) { return juce::ParameterID { ids[k], 1 }; };
    // The bands go by number: what a band is called on the panel (LOW, MID, HIGH ...) depends on how many there are.
    const char* bandName[4] = { "Band 1", "Band 2", "Band 3", "Band 4" };
    // every crossover can stand anywhere from 20 Hz to 20 kHz; the middle of the parameter's travel is 632 Hz (the
    // middle of that range in octaves). The panel keeps them in order, the engine sorts them if a host does not.
    const float skew = std::log (0.5f) / std::log ((632.4555f - 20.0f) / (20000.0f - 20.0f));
    const float xdef[3] = { 120.0f, 1000.0f, 5000.0f };
    for (int k = 0; k < 3; ++k)
        l.add (std::make_unique<F> (pid (pXo1 + k), "Crossover " + juce::String (k + 1), R (20.0f, 20000.0f, 1.0f, skew), xdef[k], juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    for (int k = 0; k < 4; ++k)
        l.add (std::make_unique<F> (pid (pW1 + k), juce::String (bandName[k]) + " Width", R (0.0f, 200.0f, 1.0f), 100.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    for (int k = 0; k < 4; ++k)
        l.add (std::make_unique<F> (pid (pA1 + k), juce::String (bandName[k]) + " Stereoize", R (0.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<F> (pid (pDelay), "Stereoize Delay", R (2.0f, 30.0f, 0.1f), 12.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<C> (pid (pType), "Stereoize Type", juce::StringArray { "Delay", "Velvet A", "Velvet B", "FVN" }, 1));
    l.add (std::make_unique<F> (pid (pGuard), "Transient Guard", R (0.0f, 100.0f, 1.0f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<F> (pid (pScatter), "FVN Scatter", R (0.0f, 100.0f, 1.0f), 25.0f, juce::AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<C> (pid (pSolo), "Solo Band", juce::StringArray { "Off", "Band 1", "Band 2", "Band 3", "Band 4" }, 0));
    l.add (std::make_unique<C> (pid (pMonitor), "Monitor", juce::StringArray { "Stereo", "Mid", "Side", "Delta" }, 0));
    for (int k = 0; k < 4; ++k) l.add (std::make_unique<B> (pid (pOn1 + k), juce::String (bandName[k]) + " On", true));
    for (int k = 0; k < 4; ++k) l.add (std::make_unique<B> (pid (pMute1 + k), juce::String (bandName[k]) + " Mute", false));
    for (int k = 0; k < 4; ++k) l.add (std::make_unique<B> (pid (pSafe1 + k), juce::String (bandName[k]) + " Safe", false));
    l.add (std::make_unique<C> (pid (pBands), "Bands", juce::StringArray { "1", "2", "3", "4" }, 3));
    return l;
}

ToboImagerProcessor::ToboImagerProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ToboImager", layout())
{
    for (int k = 0; k < kNumParams; ++k) { params[k] = apvts.getParameter (ids[k]); raw[k] = apvts.getRawParameterValue (ids[k]); }
    engine.prepare (48000.0);
}

ToboImagerProcessor::~ToboImagerProcessor() { cancelPendingUpdate(); }

bool ToboImagerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo() && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ToboImagerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    first = true;
    // the latency is known from the parameters before the first block
    setLatencySamples ((int) std::floor (raw[pType]->load() + 0.5f) == 3 ? engine.fvnLatency() : 0);
}

template <typename Float>
void ToboImagerProcessor::run (juce::AudioBuffer<Float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() < 2) return;
    double v[kNumParams]; bool changed = first;
    for (int k = 0; k < kNumParams; ++k)
    {
        v[k] = (double) raw[k]->load (std::memory_order_relaxed);
        // the crossovers go in whole Hz; a value that has been through the curve of the parameter's travel and back
        // (the default is kept that way) can be a hair off, 119.99998 for 120
        if (k <= pXo3) v[k] = std::floor (v[k] + 0.5);
        changed = changed || v[k] != last[k];
    }
    if (changed)
    {
        for (int k = 0; k < kNumParams; ++k) last[k] = v[k];
        first = false;
        engine.update (v);
        if (engine.latency != wantLatency.load()) { wantLatency = engine.latency; triggerAsyncUpdate(); }
    }
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
}

void ToboImagerProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { bypassed = false; run (b); }
void ToboImagerProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { bypassed = false; run (b); }
void ToboImagerProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { bypassed = true; AudioProcessor::processBlockBypassed (b, m); }
void ToboImagerProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { bypassed = true; AudioProcessor::processBlockBypassed (b, m); }

void ToboImagerProcessor::handleAsyncUpdate()
{
    if (getLatencySamples() != wantLatency.load()) setLatencySamples (wantLatency.load());
}

int ToboImagerProcessor::getGuiSize() const { return juce::jlimit (0, 3, (int) apvts.state.getProperty ("guiSize", 2)); }
void ToboImagerProcessor::setGuiSize (int s) { apvts.state.setProperty ("guiSize", juce::jlimit (0, 3, s), nullptr); }

int ToboImagerProcessor::getOpt (const char* name, int def) const { return (int) apvts.state.getProperty (name, def); }
void ToboImagerProcessor::setOpt (const char* name, int v) { apvts.state.setProperty (name, v, nullptr); }

void ToboImagerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void ToboImagerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ToboImagerProcessor::createEditor() { return new ToboImagerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ToboImagerProcessor(); }
