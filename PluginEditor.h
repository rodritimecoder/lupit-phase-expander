#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class LupitPhaseExpanderEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit LupitPhaseExpanderEditor(LupitPhaseExpanderAudioProcessor&);
    ~LupitPhaseExpanderEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    LupitPhaseExpanderAudioProcessor& audioProcessor;

    // The 2 main big dials (Hero knobs)
    juce::Slider stage1FloorSlider;
    juce::Slider stage2FloorSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> s1FloorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> s2FloorAttachment;

    juce::Label s1FloorLabel;
    juce::Label s2FloorLabel;

    // Secondary controls
    juce::Slider s1ThreshSlider;
    juce::Slider s2ThreshSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> s1ThreshAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> s2ThreshAttachment;
    juce::Label s1ThreshLabel;
    juce::Label s2ThreshLabel;

    juce::Slider outGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttachment;
    juce::Label outGainLabel;

    // Realtime Gain Reduction Meters
    float meterGr1 = 0.0f;
    float meterGr2 = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LupitPhaseExpanderEditor)
};
