#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

class SqnEarsProcessor : public juce::AudioProcessor
{
public:
    SqnEarsProcessor();

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SQN Ears"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    sqn::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::SpinLock lock;
    std::atomic<float>* pListen = nullptr, * pFix = nullptr, * pSound = nullptr, * pAmount = nullptr, * pConsole = nullptr,
                      * pHarsh = nullptr, * pGlue = nullptr, * pLimiter = nullptr,
                      * pBin = nullptr, * pBinState = nullptr, * pBinCar = nullptr, * pBinVol = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SqnEarsProcessor)
};
