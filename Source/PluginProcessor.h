#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "RiseDSP.h"
#include "Params.h"

class RiseKitProcessor : public juce::AudioProcessor
{
public:
    RiseKitProcessor();
    ~RiseKitProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return (int) params::presets.size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor for the live display.
    std::atomic<float> liveSmile { 0.0f };
    std::atomic<float> riseProgress { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    rise::Engine engine;
    rise::AutoRamp ramp;
    int currentProgram = 0;

    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }

    std::atomic<float>* pSmile = nullptr;
    std::atomic<float>* pRise = nullptr;
    std::atomic<float>* pLength = nullptr;
    std::atomic<float>* pCurve = nullptr;
    std::atomic<float>* pAutoDrop = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pResonance = nullptr;
    std::atomic<float>* pPhaser = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pWidth = nullptr;
    std::atomic<float>* pChop = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pReverbType = nullptr;
    std::atomic<float>* pDelay = nullptr;
    std::atomic<float>* pFeedback = nullptr;
    std::atomic<float>* pDelayTime = nullptr;
    std::atomic<float>* pPingPong = nullptr;
    std::atomic<float>* pWash = nullptr;
    std::atomic<float>* pRide = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pSafety = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiseKitProcessor)
};
