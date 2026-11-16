// Tobo Imager. Copyright (C) 2026 Tobokegao. Licensed under the GNU AGPL v3 or later.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class ToboImagerProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    ToboImagerProcessor();
    ~ToboImagerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    std::atomic<bool> bypassed { false };      // the host runs the plugin bypassed (the panel shows it at once)
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tobo Imager"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.25; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // the parameters in the order of tobo::Param; ids[k] is the id of parameter k
    static const char* const ids[tobo::kNumParams];
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* params[tobo::kNumParams] = {};
    std::atomic<float>* raw[tobo::kNumParams] = {};
    tobo::Engine engine;

    int getGuiSize() const;          // 0..3 = 1x, 1.25x, 1.5x, 2x
    void setGuiSize (int);
    int getOpt (const char* name, int def) const;     // other choices of the panel, kept with the state
    void setOpt (const char* name, int v);

private:
    template <typename Float> void run (juce::AudioBuffer<Float>&);
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    double last[tobo::kNumParams];
    bool first = true;
    std::atomic<int> wantLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToboImagerProcessor)
};
