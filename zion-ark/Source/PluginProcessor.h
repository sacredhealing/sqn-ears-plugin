#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include "Engine.h"

class ZionArkProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ZionArkProcessor();
    ~ZionArkProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zion Ark"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static const juce::StringArray& knobIds();
    static const juce::StringArray& toggleIds();
    static const juce::StringArray& choiceIds();

    juce::AudioProcessorValueTreeState state;
    za::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void timerCallback() override;
    void loadSpring (float decayNorm);

    juce::dsp::Convolution conv;
    juce::AudioBuffer<float> work;
    double sr = 44100; int maxBlock = 512;
    float loadedDecay = -1;
    std::map<juce::String, std::atomic<float>*> raw;
    float r (const char* id) const { return raw.at (id)->load(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZionArkProcessor)
};
