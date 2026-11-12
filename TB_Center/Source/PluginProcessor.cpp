#include "PluginProcessor.h"

#if ! TB_CENTER_HEADLESS_TEST
 #include "PluginEditor.h"
#endif

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto presetRootTag = "TB_CENTER_PRESET";
constexpr auto presetPluginId = "com.TeamImpulseImpact.TB_Center";
constexpr juce::int64 maximumPresetBytes = 1024 * 1024;

bool isValidProcessorState (const juce::ValueTree& state) noexcept
{
    // Earlier versions contain only the controls that existed then. Keep
    // those sparse states valid while rejecting duplicate or unknown entries.
    if (state.getNumChildren() <= 0
        || state.getNumChildren()
             > static_cast<int> (TB_CenterAudioProcessor::parameterIds.size()))
        return false;

    std::array<bool, TB_CenterAudioProcessor::parameterIds.size()> seen {};
    for (const auto& child : state)
    {
        if (! child.hasType ("PARAM")
            || ! child.hasProperty ("id")
            || ! child.hasProperty ("value"))
            return false;

        const auto id = child.getProperty ("id").toString();
        const auto found = std::find_if (
            TB_CenterAudioProcessor::parameterIds.begin(),
            TB_CenterAudioProcessor::parameterIds.end(),
            [&id] (const auto candidate) { return id == candidate; });
        if (found == TB_CenterAudioProcessor::parameterIds.end())
            return false;

        const auto value = static_cast<double> (child.getProperty ("value"));
        if (! std::isfinite (value))
            return false;

        const auto inRange = [&id, value]
        {
            if (id == TB_CenterAudioProcessor::intensityParamId
                || id == TB_CenterAudioProcessor::widthParamId)
                return value >= 0.0 && value <= 200.0;
            if (id == TB_CenterAudioProcessor::multibandParamId)
                return value >= 0.0 && value <= 1.0;
            return value >= 0.0 && value <= 100.0;
        };
        if (! inRange())
            return false;

        const auto index = static_cast<size_t> (
            found - TB_CenterAudioProcessor::parameterIds.begin());
        if (seen[index])
            return false;
        seen[index] = true;
    }

    return true;
}

const std::array<TB_CenterAudioProcessor::FactoryPreset, 6> factoryPresets {{
    //  id                 name               int    resp   look  width  time   MB     low   midlo  midhi  high
    { "init",             "Init",            100.0f, 75.0f, 20.0f, 100.0f, 100.0f, false, 100.0f, 100.0f, 100.0f, 100.0f },
    { "locked_dialogue",  "Locked Dialogue", 120.0f, 82.0f, 40.0f,  80.0f, 100.0f, false, 100.0f, 100.0f, 100.0f, 100.0f },
    { "natural_center",   "Natural Center",  100.0f, 58.0f, 24.0f, 100.0f,  60.0f, false, 100.0f, 100.0f, 100.0f, 100.0f },
    { "fast_motion",      "Fast Motion",     110.0f, 95.0f, 30.0f, 100.0f,  80.0f, false, 100.0f, 100.0f, 100.0f, 100.0f },
    { "mix_recenter",     "Mix Recenter",    110.0f, 78.0f, 28.0f, 100.0f, 100.0f, true,  100.0f, 100.0f,  80.0f,   0.0f },
    { "wide_keep_air",    "Wide Keep Air",   100.0f, 70.0f, 30.0f, 130.0f,  60.0f, true,  100.0f,  90.0f,  55.0f,   0.0f }
}};
}

TB_CenterAudioProcessor::TB_CenterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    for (size_t index = 0; index < parameterIds.size(); ++index)
    {
        const auto* id = parameterIds[index];
        parameterValues[index] = apvts.getRawParameterValue (id);
        jassert (parameterValues[index] != nullptr);
        apvts.addParameterListener (id, this);
    }
    startTimerHz (60);
}

TB_CenterAudioProcessor::~TB_CenterAudioProcessor()
{
    stopTimer();
    for (const auto* id : parameterIds)
        apvts.removeParameterListener (id, this);
}

void TB_CenterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock lock (latencyControlLock);
    const auto preparedSampleRate = juce::jmax (1.0, sampleRate);
    auto initialParameters = readEngineParameters();
    engine.prepare (preparedSampleRate, samplesPerBlock, initialParameters);
    setLatencySamples (engine.getLookaheadSamples());
    currentSampleRate.store (preparedSampleRate, std::memory_order_release);
}

void TB_CenterAudioProcessor::releaseResources()
{
    const juce::ScopedLock lock (latencyControlLock);
    currentSampleRate.store (0.0, std::memory_order_release);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TB_CenterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

void TB_CenterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    processBuffer (buffer, false);
}

void TB_CenterAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    processBuffer (buffer, true);
}

void TB_CenterAudioProcessor::processBuffer (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0)
        return;

    engine.setParameters (readEngineParameters());
    engine.process (buffer, bypassed);
}

bool TB_CenterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TB_CenterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TB_CenterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

juce::AudioProcessorEditor* TB_CenterAudioProcessor::createEditor()
{
   #if TB_CENTER_HEADLESS_TEST
    return nullptr;
   #else
    return new TB_CenterAudioProcessorEditor (*this);
   #endif
}

int TB_CenterAudioProcessor::getNumPrograms()
{
    return static_cast<int> (factoryPresets.size());
}

int TB_CenterAudioProcessor::getCurrentProgram()
{
    return juce::jlimit (0, getNumPrograms() - 1,
                         currentFactoryPresetIndex.load (std::memory_order_relaxed));
}

void TB_CenterAudioProcessor::setCurrentProgram (int index)
{
    loadFactoryPreset (index);
}

const juce::String TB_CenterAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return factoryPresets[static_cast<size_t> (index)].name;

    return {};
}

void TB_CenterAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    state.setProperty (stateSchemaVersionProperty,
                       currentStateSchemaVersion,
                       nullptr);
    state.setProperty (currentFactoryPresetProperty,
                       currentFactoryPresetIndex.load (std::memory_order_relaxed),
                       nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void TB_CenterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml (*xml);

            if (! restoredState.isValid() || ! isValidProcessorState (restoredState))
                return;

            if (restoredState.hasProperty (stateSchemaVersionProperty))
            {
                const auto versionText =
                    restoredState.getProperty (
                        stateSchemaVersionProperty).toString();

                if (versionText != "0"
                    && versionText
                        != juce::String (
                            currentStateSchemaVersion))
                    return;
            }

            restoredState.setProperty (stateSchemaVersionProperty,
                                       currentStateSchemaVersion,
                                       nullptr);
            const auto restoredPresetIndex = juce::jlimit (
                -1,
                getNumPrograms() - 1,
                static_cast<int> (restoredState.getProperty (
                    currentFactoryPresetProperty, -1)));
            apvts.replaceState (restoredState);
            currentFactoryPresetIndex.store (restoredPresetIndex,
                                             std::memory_order_relaxed);
        }
    }
}

const std::array<TB_CenterAudioProcessor::FactoryPreset, 6>&
TB_CenterAudioProcessor::getFactoryPresets() noexcept
{
    return factoryPresets;
}

void TB_CenterAudioProcessor::loadFactoryPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (factoryPresets.size())))
        return;

    const auto& preset = factoryPresets[static_cast<size_t> (index)];
    applyPresetValues (preset, "Load preset " + juce::String (preset.name), index);
}

void TB_CenterAudioProcessor::setParameterPlainValue (const char* parameterId, float value)
{
    if (auto* parameter = apvts.getParameter (parameterId))
    {
        parameter->beginChangeGesture();
        apvts.getParameterAsValue (parameterId).setValue (value);
        parameter->endChangeGesture();
    }
}

void TB_CenterAudioProcessor::applyPresetValues (const FactoryPreset& preset,
                                                  const juce::String& transactionName,
                                                  int factoryIndex)
{
    applyingPreset.store (true, std::memory_order_relaxed);
    undoManager.beginNewTransaction (transactionName);
    setParameterPlainValue (intensityParamId, preset.intensityPercent);
    setParameterPlainValue (responseParamId, preset.responsePercent);
    setParameterPlainValue (lookaheadParamId, preset.lookaheadMs);
    setParameterPlainValue (widthParamId, preset.widthPercent);
    setParameterPlainValue (timeAlignParamId, preset.timeAlignPercent);
    setParameterPlainValue (multibandParamId, preset.multibandEnabled ? 1.0f : 0.0f);
    setParameterPlainValue (bandLowParamId, preset.bandLowPercent);
    setParameterPlainValue (bandMidLowParamId, preset.bandMidLowPercent);
    setParameterPlainValue (bandMidHighParamId, preset.bandMidHighPercent);
    setParameterPlainValue (bandHighParamId, preset.bandHighPercent);
    currentFactoryPresetIndex.store (factoryIndex, std::memory_order_relaxed);
    applyingPreset.store (false, std::memory_order_relaxed);
}

bool TB_CenterAudioProcessor::savePresetToFile (const juce::File& file) const
{
    juce::XmlElement presetXml (presetRootTag);
    presetXml.setAttribute ("formatVersion", currentPresetFormatVersion);
    presetXml.setAttribute ("pluginId", presetPluginId);
    presetXml.setAttribute (stateSchemaVersionProperty,
                            currentStateSchemaVersion);
    presetXml.setAttribute (intensityParamId,
                            apvts.getRawParameterValue (intensityParamId)->load());
    presetXml.setAttribute (responseParamId,
                            apvts.getRawParameterValue (responseParamId)->load());
    presetXml.setAttribute (lookaheadParamId,
                            apvts.getRawParameterValue (lookaheadParamId)->load());
    presetXml.setAttribute (widthParamId,
                            apvts.getRawParameterValue (widthParamId)->load());
    presetXml.setAttribute (timeAlignParamId,
                            apvts.getRawParameterValue (timeAlignParamId)->load());
    presetXml.setAttribute (multibandParamId,
                            apvts.getRawParameterValue (multibandParamId)->load());
    presetXml.setAttribute (bandLowParamId,
                            apvts.getRawParameterValue (bandLowParamId)->load());
    presetXml.setAttribute (bandMidLowParamId,
                            apvts.getRawParameterValue (bandMidLowParamId)->load());
    presetXml.setAttribute (bandMidHighParamId,
                            apvts.getRawParameterValue (bandMidHighParamId)->load());
    presetXml.setAttribute (bandHighParamId,
                            apvts.getRawParameterValue (bandHighParamId)->load());
    return presetXml.writeTo (file);
}

bool TB_CenterAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    if (! file.existsAsFile() || file.getSize() <= 0 || file.getSize() > maximumPresetBytes)
        return false;

    const auto presetXml = juce::XmlDocument::parse (file);
    if (presetXml == nullptr
        || ! presetXml->hasTagName (presetRootTag)
        || presetXml->getIntAttribute ("formatVersion") != currentPresetFormatVersion
        || presetXml->getStringAttribute ("pluginId") != presetPluginId)
        return false;

    if (presetXml->hasAttribute (stateSchemaVersionProperty))
    {
        const auto versionText =
            presetXml->getStringAttribute (
                stateSchemaVersionProperty);

        if (versionText != "0"
            && versionText
                != juce::String (
                    currentStateSchemaVersion))
            return false;
    }

    for (const auto* id : { intensityParamId, responseParamId, lookaheadParamId, widthParamId,
                            timeAlignParamId, multibandParamId, bandLowParamId, bandMidLowParamId,
                            bandMidHighParamId, bandHighParamId })
        if (! presetXml->hasAttribute (id))
            return false;

    FactoryPreset preset {
        "user",
        "User",
        static_cast<float> (presetXml->getDoubleAttribute (intensityParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (responseParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (lookaheadParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (widthParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (timeAlignParamId)),
        presetXml->getDoubleAttribute (multibandParamId) >= 0.5,
        static_cast<float> (presetXml->getDoubleAttribute (bandLowParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (bandMidLowParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (bandMidHighParamId)),
        static_cast<float> (presetXml->getDoubleAttribute (bandHighParamId))
    };

    const auto bandInRange = [] (float value)
    {
        return std::isfinite (value) && value >= 0.0f && value <= 100.0f;
    };

    if (! std::isfinite (preset.intensityPercent)
        || ! std::isfinite (preset.responsePercent)
        || ! std::isfinite (preset.lookaheadMs)
        || ! std::isfinite (preset.widthPercent)
        || ! std::isfinite (preset.timeAlignPercent)
        || preset.intensityPercent < 0.0f || preset.intensityPercent > 200.0f
        || preset.responsePercent < 0.0f || preset.responsePercent > 100.0f
        || preset.lookaheadMs < 0.0f || preset.lookaheadMs > RecenterEngine::maxLookaheadMs
        || preset.widthPercent < 0.0f || preset.widthPercent > 200.0f
        || preset.timeAlignPercent < 0.0f || preset.timeAlignPercent > 100.0f
        || ! bandInRange (preset.bandLowPercent)
        || ! bandInRange (preset.bandMidLowPercent)
        || ! bandInRange (preset.bandMidHighPercent)
        || ! bandInRange (preset.bandHighPercent))
        return false;

    applyPresetValues (preset, "Load user preset", -1);
    return true;
}

void TB_CenterAudioProcessor::parameterChanged (const juce::String&, float)
{
    if (! applyingPreset.load (std::memory_order_relaxed))
        currentFactoryPresetIndex.store (-1, std::memory_order_relaxed);
}

void TB_CenterAudioProcessor::timerCallback()
{
    const juce::ScopedLock lock (latencyControlLock);
    const auto sampleRate = currentSampleRate.load (std::memory_order_acquire);
    if (sampleRate <= 0.0)
        return;

    const auto pendingLatency = engine.getPendingLatencySamples();
    if (pendingLatency < 0)
        return;

    if (getLatencySamples() != pendingLatency)
        setLatencySamples (pendingLatency);

    engine.confirmLatencyChange (pendingLatency);
}

RecenterEngine::Parameters TB_CenterAudioProcessor::readEngineParameters() const noexcept
{
    const auto value = [this] (size_t index) noexcept
    {
        const auto* parameter = parameterValues[index];
        return parameter != nullptr ? parameter->load() : 0.0f;
    };

    RecenterEngine::Parameters result;
    result.intensityPercent = value (0);
    result.responsePercent = value (1);
    result.lookaheadMs = value (2);
    result.widthPercent = value (3);
    result.timeAlignPercent = value (4);
    result.multibandEnabled = value (5) >= 0.5f;
    result.bandAmountPercent[0] = value (6);
    result.bandAmountPercent[1] = value (7);
    result.bandAmountPercent[2] = value (8);
    result.bandAmountPercent[3] = value (9);
    return result;
}

TB_CenterAudioProcessor::APVTS::ParameterLayout
TB_CenterAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { intensityParamId, 1 }, "Intensity",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { responseParamId, 1 }, "Response",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 75.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { lookaheadParamId, 1 }, "Lookahead",
        juce::NormalisableRange<float> (0.0f, RecenterEngine::maxLookaheadMs, 1.0f), 20.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms").withAutomatable (false)));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { widthParamId, 1 }, "M/S Width",
        juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { timeAlignParamId, 1 }, "Time Center",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { multibandParamId, 1 }, "Multiband", false));

    const auto addBand = [&layout] (const char* id, const juce::String& name)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
    };
    addBand (bandLowParamId, "Band Low");
    addBand (bandMidLowParamId, "Band Mid-Low");
    addBand (bandMidHighParamId, "Band Mid-High");
    addBand (bandHighParamId, "Band High");
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB_CenterAudioProcessor();
}
