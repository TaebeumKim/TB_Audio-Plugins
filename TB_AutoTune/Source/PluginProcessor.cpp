#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
bool isValidStateValue (const juce::ValueTree& parameter,
                        const juce::String& id) noexcept
{
    const auto value = static_cast<double> (parameter.getProperty ("value"));
    if (! std::isfinite (value))
        return false;
    if (id == "key")
        return value >= 0.0 && value <= 11.0;
    if (id == "scale")
        return value >= 0.0
            && value < static_cast<double> (tbautotune::MusicalNoteSelector::numScales);
    if (id == "autoMusical" || id == "lowLatencyTone")
        return value >= 0.0 && value <= 1.0;
    if (id == "formant")
        return value >= -6.0 && value <= 6.0;
    return false;
}

bool isValidParameterState (const juce::ValueTree& state) noexcept
{
    constexpr std::array<const char*, 5> ids {
        "key", "scale", "autoMusical", "formant", "lowLatencyTone" };

    // Older sessions legitimately omit lowLatencyTone; all other parameters
    // must be present exactly once and no unknown parameter may be restored.
    if (state.getNumChildren() < 4 || state.getNumChildren() > 5)
        return false;

    std::array<bool, ids.size()> seen {};
    for (const auto& child : state)
    {
        const auto id = child.getProperty ("id").toString();
        if (! child.hasType ("PARAM")
            || ! child.hasProperty ("id")
            || ! child.hasProperty ("value")
            || ! isValidStateValue (child, id))
            return false;

        const auto found = std::find_if (
            ids.begin(), ids.end(), [&id] (const auto candidate)
            {
                return id == candidate;
            });
        if (found == ids.end())
            return false;

        const auto index = static_cast<size_t> (found - ids.begin());
        if (seen[index])
            return false;
        seen[index] = true;
    }

    return seen[0] && seen[1] && seen[2] && seen[3];
}
}

TB_AutoTuneAudioProcessor::TB_AutoTuneAudioProcessor()
    : AudioProcessor (createBusesProperties()),
      apvts (*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    keyParameter = apvts.getRawParameterValue (AutoTuneParams::key);
    scaleParameter = apvts.getRawParameterValue (AutoTuneParams::scale);
    autoMusicalParameter = apvts.getRawParameterValue (AutoTuneParams::autoMusical);
    formantParameter = apvts.getRawParameterValue (AutoTuneParams::formant);
    lowLatencyToneParameter = apvts.getRawParameterValue (AutoTuneParams::lowLatencyTone);

    jassert (keyParameter != nullptr
             && scaleParameter != nullptr
             && autoMusicalParameter != nullptr
             && formantParameter != nullptr
             && lowLatencyToneParameter != nullptr);
    apvts.addParameterListener (AutoTuneParams::lowLatencyTone, this);
}

TB_AutoTuneAudioProcessor::~TB_AutoTuneAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (AutoTuneParams::lowLatencyTone, this);
}

juce::AudioProcessor::BusesProperties TB_AutoTuneAudioProcessor::createBusesProperties()
{
    auto properties = BusesProperties();

   #if ! JucePlugin_IsMidiEffect
    #if ! JucePlugin_IsSynth
     properties = properties.withInput ("Input", juce::AudioChannelSet::stereo(), true);
    #endif
     properties = properties.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif

    return properties;
}

const juce::String TB_AutoTuneAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TB_AutoTuneAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TB_AutoTuneAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TB_AutoTuneAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TB_AutoTuneAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TB_AutoTuneAudioProcessor::getNumPrograms() { return 2; }

int TB_AutoTuneAudioProcessor::getCurrentProgram()
{
    return lowLatencyToneParameter != nullptr
            && lowLatencyToneParameter->load() >= 0.5f ? 1 : 0;
}

void TB_AutoTuneAudioProcessor::setCurrentProgram (int index)
{
    applyFactoryPreset (juce::jlimit (0, getNumPrograms() - 1, index));
}

const juce::String TB_AutoTuneAudioProcessor::getProgramName (int index)
{
    if (index == 0)
        return "Init";
    if (index == 1)
        return "Low Latency Tone";
    return {};
}

void TB_AutoTuneAudioProcessor::changeProgramName (int, const juce::String&) {}

void TB_AutoTuneAudioProcessor::applyFactoryPreset (int index)
{
    struct PresetValue
    {
        const juce::String* id;
        float value;
    };

    const auto lowLatencyPreset = index == 1;
    const std::array<PresetValue, 5> values
    {{
        { &AutoTuneParams::key, 0.0f },
        { &AutoTuneParams::scale, lowLatencyPreset ? 12.0f : 0.0f },
        { &AutoTuneParams::autoMusical, 0.0f },
        { &AutoTuneParams::formant, 0.0f },
        { &AutoTuneParams::lowLatencyTone, lowLatencyPreset ? 1.0f : 0.0f }
    }};

    undoManager.beginNewTransaction (
        lowLatencyPreset ? "Load Low Latency Tone preset" : "Load Init preset");

    for (const auto& presetValue : values)
    {
        if (auto* parameter = apvts.getParameter (*presetValue.id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (presetValue.value));
            parameter->endChangeGesture();
        }
    }
}

void TB_AutoTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    autoTuneEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    updateReportedLatency();
}

void TB_AutoTuneAudioProcessor::releaseResources()
{
    autoTuneEngine.reset();
}

void TB_AutoTuneAudioProcessor::reset()
{
    autoTuneEngine.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TB_AutoTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    const auto output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono()
        && output != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (output != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TB_AutoTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    autoTuneEngine.process (buffer, readParameters());
}

bool TB_AutoTuneAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TB_AutoTuneAudioProcessor::createEditor()
{
    return new TB_AutoTuneAudioProcessorEditor (*this);
}

void TB_AutoTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TB_AutoTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml (*xml);
            if (! restoredState.isValid() || ! isValidParameterState (restoredState))
                return;

            auto hasLowLatencyParameter = false;

            for (const auto& child : restoredState)
                if (child.getProperty ("id").toString()
                    == AutoTuneParams::lowLatencyTone)
                    hasLowLatencyParameter = true;

            if (! hasLowLatencyParameter)
            {
                juce::ValueTree parameterState { "PARAM" };
                parameterState.setProperty (
                    "id", AutoTuneParams::lowLatencyTone, nullptr);
                parameterState.setProperty ("value", 0.0f, nullptr);
                restoredState.appendChild (parameterState, nullptr);
            }

            apvts.replaceState (restoredState);
        }
}

TB_AutoTuneAudioProcessor::APVTS::ParameterLayout
TB_AutoTuneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;

    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { AutoTuneParams::key, 1 },
        "Key",
        getKeyChoices(),
        0));

    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { AutoTuneParams::scale, 1 },
        "Scale",
        getScaleChoices(),
        0));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AutoTuneParams::autoMusical, 1 },
        "Auto Musical",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float value, int)
            {
                return juce::String (juce::roundToInt (value * 100.0f)) + "%";
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto number = text.retainCharacters ("0123456789.-").getFloatValue();
                return juce::jlimit (0.0f, 1.0f, number > 1.0f ? number * 0.01f : number);
            })));

    parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { AutoTuneParams::formant, 1 },
        "Formant Shift",
        juce::NormalisableRange<float> { -6.0f, 6.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("st")));

    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { AutoTuneParams::lowLatencyTone, 2 },
        "Low Latency Tone",
        false,
        juce::AudioParameterBoolAttributes()
            .withAutomatable (false)
            .withStringFromValueFunction ([] (bool enabled, int)
            {
                return enabled ? juce::String ("Live") : juce::String ("Natural");
            })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.equalsIgnoreCase ("Live")
                    || text.equalsIgnoreCase ("On")
                    || text.equalsIgnoreCase ("True")
                    || text.getIntValue() != 0;
            })));

    return { parameters.begin(), parameters.end() };
}

juce::StringArray TB_AutoTuneAudioProcessor::getKeyChoices()
{
    return { "C", "C#", "D", "D#", "E", "F",
             "F#", "G", "G#", "A", "A#", "B" };
}

juce::StringArray TB_AutoTuneAudioProcessor::getScaleChoices()
{
    juce::StringArray choices;

    for (auto index = 0; index < tbautotune::MusicalNoteSelector::numScales; ++index)
    {
        const auto scale = static_cast<tbautotune::MusicalNoteSelector::Scale> (index);
        choices.add (juce::String (
            tbautotune::MusicalNoteSelector::getScaleName (scale).data()));
    }

    return choices;
}

AutoTuneEngine::Parameters TB_AutoTuneAudioProcessor::readParameters() const noexcept
{
    AutoTuneEngine::Parameters parameters;

    if (keyParameter != nullptr)
        parameters.rootPitchClass = juce::jlimit (
            0, 11, juce::roundToInt (keyParameter->load()));

    if (scaleParameter != nullptr)
    {
        const auto index = juce::jlimit (
            0,
            tbautotune::MusicalNoteSelector::numScales - 1,
            juce::roundToInt (scaleParameter->load()));
        parameters.scale = static_cast<tbautotune::MusicalNoteSelector::Scale> (index);
    }

    if (autoMusicalParameter != nullptr)
        parameters.autoMusicalProbability = juce::jlimit (
            0.0f, 1.0f, autoMusicalParameter->load());

    if (formantParameter != nullptr)
        parameters.formantSemitones = juce::jlimit (
            -6.0f, 6.0f, formantParameter->load());

    if (lowLatencyToneParameter != nullptr)
        parameters.lowLatencyTone = lowLatencyToneParameter->load() >= 0.5f;

    return parameters;
}

void TB_AutoTuneAudioProcessor::parameterChanged (
    const juce::String& parameterID, float)
{
    if (parameterID == AutoTuneParams::lowLatencyTone)
        triggerAsyncUpdate();
}

void TB_AutoTuneAudioProcessor::handleAsyncUpdate()
{
    updateReportedLatency();
}

void TB_AutoTuneAudioProcessor::updateReportedLatency()
{
    const auto lowLatency = lowLatencyToneParameter != nullptr
                         && lowLatencyToneParameter->load() >= 0.5f;
    setLatencySamples (autoTuneEngine.getLatencySamples (lowLatency));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB_AutoTuneAudioProcessor();
}
