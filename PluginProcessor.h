#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "LupitDspCore.h"

class LupitPhaseExpanderAudioProcessor : public juce::AudioProcessor
{
public:
    LupitPhaseExpanderAudioProcessor();
    ~LupitPhaseExpanderAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    float getStage1GR() const noexcept { return dspEngine.getStage1GainReductionDb(); }
    float getStage2GR() const noexcept { return dspEngine.getStage2GainReductionDb(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    Lupit::DualStageProcessor dspEngine;

    // Cached parameter atomic pointers for realtime thread safety
    std::atomic<float>* scHpfFreqParam = nullptr;
    std::atomic<float>* stereoLinkParam = nullptr;

    std::atomic<float>* s1ThreshParam = nullptr;
    std::atomic<float>* s1FloorParam = nullptr;
    std::atomic<float>* s1RatioParam = nullptr;
    std::atomic<float>* s1AttackParam = nullptr;
    std::atomic<float>* s1ReleaseParam = nullptr;

    std::atomic<float>* s2ThreshParam = nullptr;
    std::atomic<float>* s2FloorParam = nullptr;
    std::atomic<float>* s2RatioParam = nullptr;
    std::atomic<float>* s2AttackParam = nullptr;
    std::atomic<float>* s2ReleaseParam = nullptr;

    std::atomic<float>* outGainParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LupitPhaseExpanderAudioProcessor)
};
