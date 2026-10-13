#include "PluginProcessor.h"

LupitPhaseExpanderAudioProcessor::LupitPhaseExpanderAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    scHpfFreqParam = apvts.getRawParameterValue("sc_hpf");
    stereoLinkParam = apvts.getRawParameterValue("stereo_link");

    s1ThreshParam = apvts.getRawParameterValue("s1_thresh");
    s1FloorParam = apvts.getRawParameterValue("s1_floor");
    s1RatioParam = apvts.getRawParameterValue("s1_ratio");
    s1AttackParam = apvts.getRawParameterValue("s1_attack");
    s1ReleaseParam = apvts.getRawParameterValue("s1_release");

    s2ThreshParam = apvts.getRawParameterValue("s2_thresh");
    s2FloorParam = apvts.getRawParameterValue("s2_floor");
    s2RatioParam = apvts.getRawParameterValue("s2_ratio");
    s2AttackParam = apvts.getRawParameterValue("s2_attack");
    s2ReleaseParam = apvts.getRawParameterValue("s2_release");

    outGainParam = apvts.getRawParameterValue("out_gain");
}

juce::AudioProcessorValueTreeState::ParameterLayout LupitPhaseExpanderAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Global / Sidechain Controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"sc_hpf", 1}, "Detector HPF",
        juce::NormalisableRange<float>(20.0f, 250.0f, 1.0f, 0.4f), 90.0f, "Hz"));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"stereo_link", 1}, "Stereo Link", true));

    // Stage 1 (Coarse Gate)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s1_thresh", 1}, "Stage 1 Thresh",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -32.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s1_floor", 1}, "Stage 1 Bedrock Floor",
        juce::NormalisableRange<float>(-40.0f, -3.0f, 0.1f), -14.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s1_ratio", 1}, "Stage 1 Ratio",
        juce::NormalisableRange<float>(1.2f, 8.0f, 0.1f), 3.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s1_attack", 1}, "Stage 1 Attack",
        juce::NormalisableRange<float>(0.1f, 30.0f, 0.05f, 0.35f), 1.5f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s1_release", 1}, "Stage 1 Release",
        juce::NormalisableRange<float>(10.0f, 500.0f, 0.1f, 0.4f), 80.0f, "ms"));

    // Stage 2 (Fine Trim)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s2_thresh", 1}, "Stage 2 Thresh",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -42.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s2_floor", 1}, "Stage 2 Bedrock Floor",
        juce::NormalisableRange<float>(-30.0f, -2.0f, 0.1f), -10.0f, "dB"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s2_ratio", 1}, "Stage 2 Ratio",
        juce::NormalisableRange<float>(1.1f, 4.0f, 0.1f), 2.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s2_attack", 1}, "Stage 2 Attack",
        juce::NormalisableRange<float>(0.5f, 50.0f, 0.05f, 0.35f), 4.0f, "ms"));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"s2_release", 1}, "Stage 2 Release",
        juce::NormalisableRange<float>(20.0f, 800.0f, 0.1f, 0.4f), 160.0f, "ms"));

    // Output Trim
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"out_gain", 1}, "Output Makeup",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

void LupitPhaseExpanderAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    dspEngine.prepare(sampleRate);
}

void LupitPhaseExpanderAudioProcessor::releaseResources()
{
    dspEngine.reset();
}

bool LupitPhaseExpanderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void LupitPhaseExpanderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    if (totalNumInputChannels == 0 || numSamples == 0)
        return;

    // Pull parameter values atomically
    Lupit::DualStageProcessor::Params p;
    p.sidechainHpfEnabled = true;
    p.sidechainHpfFreq = scHpfFreqParam->load(std::memory_order_relaxed);
    p.stereoLink = stereoLinkParam->load(std::memory_order_relaxed) > 0.5f;

    p.stage1Thresh = s1ThreshParam->load(std::memory_order_relaxed);
    p.stage1Floor = s1FloorParam->load(std::memory_order_relaxed);
    p.stage1Ratio = s1RatioParam->load(std::memory_order_relaxed);
    p.stage1AttackMs = s1AttackParam->load(std::memory_order_relaxed);
    p.stage1ReleaseMs = s1ReleaseParam->load(std::memory_order_relaxed);

    p.stage2Thresh = s2ThreshParam->load(std::memory_order_relaxed);
    p.stage2Floor = s2FloorParam->load(std::memory_order_relaxed);
    p.stage2Ratio = s2RatioParam->load(std::memory_order_relaxed);
    p.stage2AttackMs = s2AttackParam->load(std::memory_order_relaxed);
    p.stage2ReleaseMs = s2ReleaseParam->load(std::memory_order_relaxed);

    p.outputGainDb = outGainParam->load(std::memory_order_relaxed);

    dspEngine.setParameters(p);

    // Feed buffer into DSP core
    dspEngine.process(buffer.getArrayOfWritePointers(), totalNumInputChannels, numSamples);
}

const juce::String LupitPhaseExpanderAudioProcessor::getName() const { return "Lupit Phase Expander"; }
bool LupitPhaseExpanderAudioProcessor::acceptsMidi() const { return false; }
bool LupitPhaseExpanderAudioProcessor::producesMidi() const { return false; }
bool LupitPhaseExpanderAudioProcessor::isMidiEffect() const { return false; }
double LupitPhaseExpanderAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int LupitPhaseExpanderAudioProcessor::getNumPrograms() { return 1; }
int LupitPhaseExpanderAudioProcessor::getCurrentProgram() { return 0; }
void LupitPhaseExpanderAudioProcessor::setCurrentProgram(int) {}
const juce::String LupitPhaseExpanderAudioProcessor::getProgramName(int) { return {}; }
void LupitPhaseExpanderAudioProcessor::changeProgramName(int, const juce::String&) {}

bool LupitPhaseExpanderAudioProcessor::hasEditor() const { return true; }

#include "PluginEditor.h"

juce::AudioProcessorEditor* LupitPhaseExpanderAudioProcessor::createEditor()
{
    return new LupitPhaseExpanderEditor(*this);
}

void LupitPhaseExpanderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LupitPhaseExpanderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LupitPhaseExpanderAudioProcessor();
}
