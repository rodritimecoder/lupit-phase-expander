#include "PluginEditor.h"

LupitPhaseExpanderEditor::LupitPhaseExpanderEditor(LupitPhaseExpanderAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(520, 360);

    auto setupHeroKnob = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 22);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00d2ff));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe0e0e0));
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(14.0f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xffb0c4de));
        addAndMakeVisible(label);
    };

    auto setupSmallKnob = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4a90e2));
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffcccccc));
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f, juce::Font::plain));
        label.setColour(juce::Label::textColourId, juce::Colour(0xff8a9ba8));
        addAndMakeVisible(label);
    };

    // Hero Knob 1: Stage 1 Bedrock Floor
    setupHeroKnob(stage1FloorSlider, s1FloorLabel, "STAGE 1 FLOOR");
    s1FloorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "s1_floor", stage1FloorSlider);

    // Hero Knob 2: Stage 2 Bedrock Floor
    setupHeroKnob(stage2FloorSlider, s2FloorLabel, "STAGE 2 FLOOR");
    s2FloorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "s2_floor", stage2FloorSlider);

    // Sensitivity / Threshold Controls
    setupSmallKnob(s1ThreshSlider, s1ThreshLabel, "Thresh 1");
    s1ThreshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "s1_thresh", s1ThreshSlider);

    setupSmallKnob(s2ThreshSlider, s2ThreshLabel, "Thresh 2");
    s2ThreshAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "s2_thresh", s2ThreshSlider);

    setupSmallKnob(outGainSlider, outGainLabel, "Output Trim");
    outGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "out_gain", outGainSlider);

    startTimerHz(30); // 30 FPS meter refresh
}

LupitPhaseExpanderEditor::~LupitPhaseExpanderEditor()
{
    stopTimer();
}

void LupitPhaseExpanderEditor::timerCallback()
{
    meterGr1 = audioProcessor.getStage1GR();
    meterGr2 = audioProcessor.getStage2GR();
    repaint();
}

void LupitPhaseExpanderEditor::paint(juce::Graphics& g)
{
    // Background gradient
    g.fillAll(juce::Colour(0xff12161c));

    // Outer card container
    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff1a212b));
    g.fillRoundedRectangle(bounds, 10.0f);

    g.setColour(juce::Colour(0xff2a3646));
    g.drawRoundedRectangle(bounds, 10.0f, 1.5f);

    // Title Header
    g.setColour(juce::Colour(0xffe6f1ff));
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("LUPIT PHASE EXPANDER", 0, 18, getWidth(), 24, juce::Justification::centred);

    g.setColour(juce::Colour(0xff5f7d95));
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText("ZERO-LATENCY DUAL BEDROCK SUPPRESSOR", 0, 40, getWidth(), 16, juce::Justification::centred);

    // Draw Live Gain Reduction Meters between knobs
    auto drawMeter = [&g](juce::Rectangle<float> meterArea, float grDb, const juce::String& label)
    {
        g.setColour(juce::Colour(0xff10151c));
        g.fillRoundedRectangle(meterArea, 4.0f);

        // Map -30dB .. 0dB to fill height
        float clampedGr = std::clamp(grDb, -30.0f, 0.0f);
        float fillRatio = std::abs(clampedGr) / 30.0f; // 0 = no reduction, 1 = max reduction
        float fillHeight = meterArea.getHeight() * fillRatio;

        juce::Rectangle<float> fillArea(meterArea.getX(), meterArea.getY(), meterArea.getWidth(), fillHeight);
        g.setColour(juce::Colour(0xffff5252));
        g.fillRoundedRectangle(fillArea, 4.0f);

        g.setColour(juce::Colour(0xff8a9ba8));
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText(label, meterArea.getX() - 15, meterArea.getBottom() + 4, meterArea.getWidth() + 30, 12, juce::Justification::centred);
    };

    drawMeter(juce::Rectangle<float>(140, 100, 12, 110), meterGr1, "GR 1");
    drawMeter(juce::Rectangle<float>(368, 100, 12, 110), meterGr2, "GR 2");
}

void LupitPhaseExpanderEditor::resized()
{
    // Hero Knobs (Main 2 Floor Dials)
    const int knobSize = 130;
    stage1FloorSlider.setBounds(30, 85, knobSize, knobSize);
    s1FloorLabel.setBounds(30, 220, knobSize, 20);

    stage2FloorSlider.setBounds(260, 85, knobSize, knobSize);
    s2FloorLabel.setBounds(260, 220, knobSize, 20);

    // Bottom secondary controls (Thresh 1, Thresh 2, Output Trim)
    const int smallSize = 75;
    const int bottomY = 250;

    s1ThreshSlider.setBounds(60, bottomY, smallSize, smallSize);
    s1ThreshLabel.setBounds(50, bottomY + smallSize - 2, 95, 16);

    s2ThreshSlider.setBounds(220, bottomY, smallSize, smallSize);
    s2ThreshLabel.setBounds(210, bottomY + smallSize - 2, 95, 16);

    outGainSlider.setBounds(380, bottomY, smallSize, smallSize);
    outGainLabel.setBounds(370, bottomY + smallSize - 2, 95, 16);
}
