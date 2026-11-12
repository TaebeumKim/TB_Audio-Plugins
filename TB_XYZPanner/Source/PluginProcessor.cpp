/*
  ==============================================================================

    Plugin processor: buses, parameters, and state persistence.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>

NewProjectAudioProcessor::NewProjectAudioProcessor()
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      parameters (*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    panParameter = parameters.getRawParameterValue ("pan");
    frontBackParameter = parameters.getRawParameterValue ("frontBack");
    heightParameter = parameters.getRawParameterValue ("height");
    distanceParameter = parameters.getRawParameterValue ("distance");
    roomParameter = parameters.getRawParameterValue ("room");
    distanceModelParameter = parameters.getRawParameterValue ("distanceModel");
    renderModeParameter = parameters.getRawParameterValue ("renderMode");
    outputTrimParameter = parameters.getRawParameterValue ("outputTrim");
}

NewProjectAudioProcessor::~NewProjectAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NewProjectAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percentAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("%")
        .withStringFromValueFunction ([] (float value, int)
        {
            return juce::String (juce::roundToInt (value * 100.0f)) + " %";
        })
        .withValueFromStringFunction ([] (const juce::String& value)
        {
            return value.getFloatValue() / 100.0f;
        });
    const auto decibelAttributes = juce::AudioParameterFloatAttributes()
        .withLabel ("dB")
        .withStringFromValueFunction ([] (float value, int)
        {
            return juce::String (value, 1) + " dB";
        })
        .withValueFromStringFunction ([] (const juce::String& value)
        {
            return value.getFloatValue();
        });

    layout.add (std::make_unique<juce::AudioParameterFloat> ("pan", "Pan",
                                                             juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                                                             0.0f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("frontBack", "Front Back",
                                                             juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                                                             1.0f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("height", "Height",
                                                             juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                                                             0.0f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("distance", "Distance",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                             0.0f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("room", "Room",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                             0.35f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("distanceModel", "Distance Model",
                                                               juce::StringArray { "Physical", "Near Field", "Linear" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> ("renderMode", "Render Mode",
                                                               juce::StringArray { "Stereo", "Headphones" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("outputTrim", "Output Trim",
                                                             juce::NormalisableRange<float> (-24.0f, 0.0f, 0.1f),
                                                             0.0f, decibelAttributes));
    return layout;
}

SpatialRendererParameters NewProjectAudioProcessor::getRendererParameters() const
{
    return { panParameter->load(), frontBackParameter->load(), heightParameter->load(),
             distanceParameter->load(), roomParameter->load(),
             juce::roundToInt (distanceModelParameter->load()),
             juce::roundToInt (renderModeParameter->load()),
             juce::Decibels::decibelsToGain (outputTrimParameter->load()) };
}

const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return SpatialRenderer::maximumTailLengthSeconds;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String NewProjectAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void NewProjectAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    renderer.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    renderer.setInputLayout (getBusesLayout().getMainInputChannelSet());
    renderer.setSurroundLayout (getBusesLayout().getMainOutputChannelSet());
    renderer.setParameters (getRendererParameters(), true);
}

void NewProjectAudioProcessor::releaseResources()
{
    renderer.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (output == juce::AudioChannelSet::stereo())
        return input == juce::AudioChannelSet::mono()
            || input == juce::AudioChannelSet::stereo()
            || input == juce::AudioChannelSet::create5point1()
            || input == juce::AudioChannelSet::create7point1();

    const auto isSupportedSurroundOutput = output == juce::AudioChannelSet::create5point1()
                                        || output == juce::AudioChannelSet::create7point1();

    return isSupportedSurroundOutput
        && (input == juce::AudioChannelSet::mono()
            || input == juce::AudioChannelSet::stereo()
            || input == output);
  #endif
}
#endif

void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (totalNumOutputChannels < 2 || totalNumInputChannels == 0)
        return;

    renderer.setParameters (getRendererParameters());

    if (totalNumOutputChannels > 2)
    {
        std::array<float, SpatialRenderer::maximumOutputChannels> inputSamples {};
        std::array<float, SpatialRenderer::maximumOutputChannels> outputSamples {};

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
                inputSamples[static_cast<size_t> (channel)] = buffer.getSample (channel, sample);

            renderer.processSurround (inputSamples.data(), outputSamples.data(),
                                      totalNumInputChannels, totalNumOutputChannels);

            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
                buffer.setSample (channel, sample, outputSamples[static_cast<size_t> (channel)]);
        }

        return;
    }

    auto* outputLeft = buffer.getWritePointer (0);
    auto* outputRight = buffer.getWritePointer (1);

    if (totalNumInputChannels > 2)
    {
        std::array<float, SpatialRenderer::maximumOutputChannels> inputSamples {};

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // Capture every source channel before overwriting the shared L/R output buffers.
            for (int channel = 0; channel < totalNumInputChannels; ++channel)
                inputSamples[static_cast<size_t> (channel)] = buffer.getSample (channel, sample);

            renderer.processStereoOutput (inputSamples.data(), totalNumInputChannels,
                                           outputLeft[sample], outputRight[sample]);
        }

        return;
    }

    const auto hasStereoInput = totalNumInputChannels > 1;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inputLeft = outputLeft[sample];
        const auto inputRight = hasStereoInput ? outputRight[sample] : inputLeft;
        renderer.process (inputLeft, inputRight, hasStereoInput, outputLeft[sample], outputRight[sample]);
    }
}

bool NewProjectAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor (*this);
}

void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void NewProjectAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}
