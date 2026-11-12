#include "BoardAudioEngine.h"
#include "BoardAgentEndpoint.h"
#include "BoardPresetLibrary.h"
#include "DualMonoProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <tuple>

struct CatalogScanResult
{
    juce::File scannedFolder;
    juce::Array<juce::PluginDescription> pluginTypes;
    juce::StringArray blacklistedFiles;
    juce::StringArray failedFiles;
    bool completed = false;
};

namespace
{
constexpr auto pluginListKey = "knownPlugins";
constexpr auto lastAdditionalPluginFolderKey = "lastAdditionalPluginFolder";
constexpr auto nodeXKey = "boardX";
constexpr auto nodeYKey = "boardY";
constexpr auto boardStateTag = "TB_BOARD_STATE";
constexpr auto nodeTag = "NODE";
constexpr auto connectionTag = "CONNECTION";
constexpr auto inputCardTag = "INPUT_CARD";
constexpr auto childStateTag = "STATE";
constexpr auto modulationTag = "MODULATION";
constexpr auto modulationSourceTag = "SOURCE";
constexpr auto modulationLinkTag = "LINK";
constexpr auto modulationControlLinkTag = "CONTROL_LINK";
constexpr auto modulationCurvePointTag = "CURVE_POINT";
constexpr int boardStateSchemaVersion = 4;
constexpr int oldestSupportedBoardStateSchemaVersion = 1;
constexpr int maximumRestoredNodes = 128;
constexpr int maximumRestoredConnections = 512;
constexpr size_t maximumModulationCurvePoints = 64;
constexpr size_t maximumHistoryEntries = 64;
constexpr size_t maximumHistoryStateBytes = 256u * 1024u * 1024u;
constexpr size_t maximumBoardStateBytes = 128u * 1024u * 1024u;
constexpr size_t maximumChildStateBytes = 64u * 1024u * 1024u;
constexpr double modulationTriggerRefractorySeconds = 0.005;
constexpr float outputSafetyLimiterCeiling = 0.89125094f; // -1 dBFS
constexpr double outputSafetyLimiterReleaseSeconds = 0.050;
constexpr juce::uint32 physicalInputNodeUID = 0;
constexpr juce::uint32 inputNodeUID = 1;
constexpr juce::uint32 outputNodeUID = 2;
constexpr int stereoInputSource = 0;
constexpr int firstMonoInputSource = 1;
constexpr int secondMonoInputSource = 2;
constexpr int localMediaInputSource = 3;
constexpr int youtubeInputSource = 4;
constexpr float minimumLevelDb = -60.0f;
constexpr float maximumLevelDb = 12.0f;
constexpr double levelSmoothingSeconds = 0.010;
thread_local const BoardAudioEngine* activeModulationWriter = nullptr;

float sanitiseLevelDb (float decibels) noexcept
{
    return std::isfinite (decibels)
               ? juce::jlimit (minimumLevelDb, maximumLevelDb, decibels)
               : 0.0f;
}

float levelDbToGain (float decibels) noexcept
{
    const auto value = sanitiseLevelDb (decibels);
    return value <= minimumLevelDb
               ? 0.0f
               : juce::Decibels::decibelsToGain (value);
}

class SmoothedLevel
{
public:
    void prepare (double sampleRate)
    {
        smoother.reset (juce::jmax (1.0, sampleRate), levelSmoothingSeconds);
        smoother.setCurrentAndTargetValue (
            levelDbToGain (targetDb.load (std::memory_order_relaxed)));
    }

    void reset() noexcept
    {
        smoother.setCurrentAndTargetValue (
            levelDbToGain (targetDb.load (std::memory_order_relaxed)));
    }

    void setDb (float decibels) noexcept
    {
        targetDb.store (sanitiseLevelDb (decibels), std::memory_order_relaxed);
    }

    float getDb() const noexcept
    {
        return targetDb.load (std::memory_order_relaxed);
    }

    void updateTarget() noexcept
    {
        const auto target = levelDbToGain (getDb());
        if (! juce::approximatelyEqual (smoother.getTargetValue(), target))
            smoother.setTargetValue (target);
    }

    float getNextGain() noexcept
    {
        return smoother.getNextValue();
    }

    void apply (juce::AudioBuffer<float>& buffer) noexcept
    {
        updateTarget();
        const auto channels = buffer.getNumChannels();
        const auto samples = buffer.getNumSamples();
        auto* const* data = buffer.getArrayOfWritePointers();

        for (auto sample = 0; sample < samples; ++sample)
        {
            const auto gain = smoother.getNextValue();
            for (auto channel = 0; channel < channels; ++channel)
                data[channel][sample] *= gain;
        }
    }

private:
    std::atomic<float> targetDb { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        smoother { 1.0f };
};

class InputRouterProcessor final : public juce::AudioProcessor
{
public:
    InputRouterProcessor()
        : AudioProcessor (BusesProperties()
                              .withInput ("Physical Input",
                                          juce::AudioChannelSet::stereo(),
                                          true)
                              .withOutput (
                                  "Board Sources",
                                  juce::AudioChannelSet::discreteChannels (6),
                                  true))
    {
    }

    void setSourceLevelDb (int sourceIndex, float decibels) noexcept
    {
        if (juce::isPositiveAndBelow (
                sourceIndex, static_cast<int> (sourceLevels.size())))
            sourceLevels[static_cast<size_t> (sourceIndex)].setDb (decibels);
    }

    float getSourceLevelDb (int sourceIndex) const noexcept
    {
        return juce::isPositiveAndBelow (
                   sourceIndex, static_cast<int> (sourceLevels.size()))
                   ? sourceLevels[static_cast<size_t> (sourceIndex)].getDb()
                   : 0.0f;
    }

    const juce::String getName() const override { return "Input"; }

    void prepareToPlay (double sampleRate, int) override
    {
        for (auto& level : sourceLevels)
            level.prepare (sampleRate);
    }

    void releaseResources() override {}

    void reset() override
    {
        for (auto& level : sourceLevels)
            level.reset();
    }

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        midi.clear();

        if (buffer.getNumChannels() < 6)
        {
            buffer.clear();
            return;
        }

        for (auto& level : sourceLevels)
            level.updateTarget();

        auto* const* data = buffer.getArrayOfWritePointers();
        for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // A malformed host buffer must not inject NaN/Inf into every
            // routed source and the hosted graph. The final output limiter
            // sanitises the stereo output, but doing it at the physical input
            // keeps intermediate plug-in state finite as well.
            const auto left = std::isfinite (data[0][sample])
                                  ? data[0][sample] : 0.0f;
            const auto right = std::isfinite (data[1][sample])
                                   ? data[1][sample] : 0.0f;
            const auto stereoGain = sourceLevels[0].getNextGain();
            const auto firstMonoGain = sourceLevels[1].getNextGain();
            const auto secondMonoGain = sourceLevels[2].getNextGain();

            data[0][sample] = left * stereoGain;
            data[1][sample] = right * stereoGain;
            data[2][sample] = left * firstMonoGain;
            data[3][sample] = left * firstMonoGain;
            data[4][sample] = right * secondMonoGain;
            data[5][sample] = right * secondMonoGain;
        }
    }

    void processBlockBypassed (juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& midi) override
    {
        processBlock (buffer, midi);
    }

    bool supportsDoublePrecisionProcessing() const override { return false; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet()
                   == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet()
                   == juce::AudioChannelSet::discreteChannels (6);
    }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::array<SmoothedLevel, 3> sourceLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputRouterProcessor)
};

class NodeGainProcessor;

class NodeGainEditor final : public juce::AudioProcessorEditor
{
public:
    NodeGainEditor (NodeGainProcessor&, juce::AudioProcessor&);
    ~NodeGainEditor() override;

    void resized() override
    {
        if (childEditor != nullptr)
            childEditor->setBounds (getLocalBounds());
    }

private:
    juce::AudioProcessor& child;
    std::unique_ptr<juce::AudioProcessorEditor> childEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeGainEditor)
};

class NodeGainProcessor final : public juce::AudioProcessor,
                                private juce::AudioProcessorListener,
                                private juce::AsyncUpdater
{
public:
    explicit NodeGainProcessor (std::unique_ptr<juce::AudioProcessor> processor,
                                float inputDb = 0.0f,
                                float outputDb = 0.0f)
        : AudioProcessor (BusesProperties()
                              .withInput ("Stereo Input",
                                          juce::AudioChannelSet::stereo(),
                                          true)
                              .withOutput ("Stereo Output",
                                           juce::AudioChannelSet::stereo(),
                                           true)),
          child (std::move (processor))
    {
        jassert (child != nullptr);
        inputLevel.setDb (inputDb);
        outputLevel.setDb (outputDb);
        child->addListener (this);
        setLatencySamples (child->getLatencySamples());
    }

    ~NodeGainProcessor() override
    {
        cancelPendingUpdate();
        child->removeListener (this);
    }

    juce::AudioProcessor& getWrappedProcessor() noexcept { return *child; }
    const juce::AudioProcessor& getWrappedProcessor() const noexcept
    {
        return *child;
    }

    float getLevelDb (BoardAudioEngine::NodeLevelStage stage) const noexcept
    {
        return stage == BoardAudioEngine::NodeLevelStage::input
                   ? inputLevel.getDb()
                   : outputLevel.getDb();
    }

    void setLevelDb (BoardAudioEngine::NodeLevelStage stage,
                     float decibels) noexcept
    {
        (stage == BoardAudioEngine::NodeLevelStage::input
             ? inputLevel
             : outputLevel).setDb (decibels);
    }

    const juce::String getName() const override { return child->getName(); }

    void prepareToPlay (double sampleRate, int maximumBlockSize) override
    {
        child->setNonRealtime (isNonRealtime());
        child->setProcessingPrecision (juce::AudioProcessor::singlePrecision);
        child->setRateAndBufferSizeDetails (sampleRate, maximumBlockSize);
        child->prepareToPlay (sampleRate, maximumBlockSize);
        inputLevel.prepare (sampleRate);
        outputLevel.prepare (sampleRate);
        setLatencySamples (child->getLatencySamples());
    }

    void releaseResources() override { child->releaseResources(); }

    void reset() override
    {
        child->reset();
        inputLevel.reset();
        outputLevel.reset();
    }

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        inputLevel.apply (buffer);
        {
            const juce::ScopedLock lock (child->getCallbackLock());
            child->setPlayHead (getPlayHead());
            if (child->isSuspended())
            {
                buffer.clear();
                midi.clear();
            }
            else
            {
                child->processBlock (buffer, midi);
            }
        }
        outputLevel.apply (buffer);
    }

    void processBlockBypassed (juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;
        inputLevel.apply (buffer);
        {
            const juce::ScopedLock lock (child->getCallbackLock());
            child->setPlayHead (getPlayHead());
            child->processBlockBypassed (buffer, midi);
        }
        outputLevel.apply (buffer);
    }

    void setNonRealtime (bool shouldBeNonRealtime) noexcept override
    {
        juce::AudioProcessor::setNonRealtime (shouldBeNonRealtime);
        child->setNonRealtime (shouldBeNonRealtime);
    }

    bool supportsDoublePrecisionProcessing() const override { return false; }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet()
                   == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet()
                   == juce::AudioChannelSet::stereo();
    }

    bool acceptsMidi() const override { return child->acceptsMidi(); }
    bool producesMidi() const override { return child->producesMidi(); }
    double getTailLengthSeconds() const override
    {
        return child->getTailLengthSeconds();
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new NodeGainEditor (*this, *child);
    }

    int getNumPrograms() override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        return child->getNumPrograms();
    }

    int getCurrentProgram() override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        return child->getCurrentProgram();
    }

    void setCurrentProgram (int index) override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        child->setCurrentProgram (index);
    }

    const juce::String getProgramName (int index) override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        return child->getProgramName (index);
    }

    void changeProgramName (int index, const juce::String& name) override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        child->changeProgramName (index, name);
    }

    void getStateInformation (juce::MemoryBlock& destination) override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        child->getStateInformation (destination);
    }

    void setStateInformation (const void* data, int size) override
    {
        const juce::ScopedLock lock (child->getCallbackLock());
        child->setStateInformation (data, size);
    }

private:
    std::unique_ptr<juce::AudioProcessor> child;
    SmoothedLevel inputLevel;
    SmoothedLevel outputLevel;

    void handleAsyncUpdate() override
    {
        setLatencySamples (child->getLatencySamples());
    }

    void audioProcessorParameterChanged (
        juce::AudioProcessor*, int, float) override {}

    void audioProcessorChanged (
        juce::AudioProcessor*,
        const juce::AudioProcessorListener::ChangeDetails& details) override
    {
        if (details.latencyChanged
            || details.programChanged
            || details.nonParameterStateChanged)
            triggerAsyncUpdate();

        if (details.programChanged || details.nonParameterStateChanged)
            updateHostDisplay (
                juce::AudioProcessorListener::ChangeDetails {}
                    .withNonParameterStateChanged (true));
    }

    void audioProcessorParameterChangeGestureBegin (
        juce::AudioProcessor*, int) override {}
    void audioProcessorParameterChangeGestureEnd (
        juce::AudioProcessor*, int) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeGainProcessor)
};

NodeGainEditor::NodeGainEditor (NodeGainProcessor& owner,
                                juce::AudioProcessor& processor)
    : AudioProcessorEditor (owner), child (processor)
{
    if (child.hasEditor())
        childEditor.reset (child.createEditorIfNeeded());
    if (childEditor == nullptr)
        childEditor = std::make_unique<juce::GenericAudioProcessorEditor> (child);

    addAndMakeVisible (*childEditor);
    setResizable (childEditor->isResizable(), false);
    setSize (juce::jmax (320, childEditor->getWidth()),
             juce::jmax (180, childEditor->getHeight()));
}

NodeGainEditor::~NodeGainEditor()
{
    if (childEditor != nullptr)
        child.editorBeingDeleted (childEditor.get());
}

std::unique_ptr<juce::AudioProcessor> wrapNodeProcessor (
    std::unique_ptr<juce::AudioProcessor> processor,
    float inputGainDb = 0.0f,
    float outputGainDb = 0.0f)
{
    if (processor == nullptr)
        return nullptr;

    if (auto* existing = dynamic_cast<NodeGainProcessor*> (processor.get()))
    {
        existing->setLevelDb (
            BoardAudioEngine::NodeLevelStage::input, inputGainDb);
        existing->setLevelDb (
            BoardAudioEngine::NodeLevelStage::output, outputGainDb);
        return processor;
    }

    return std::make_unique<NodeGainProcessor> (
        std::move (processor), inputGainDb, outputGainDb);
}

juce::String modulationTypeToString (BoardAudioEngine::ModulationType type)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:         return "lfo";
        case BoardAudioEngine::ModulationType::curve:       return "curve";
        case BoardAudioEngine::ModulationType::envelope:    return "envelope";
        case BoardAudioEngine::ModulationType::audioFollow: return "audioFollow";
        case BoardAudioEngine::ModulationType::random:      return "random";
    }

    return {};
}

juce::String getModulationTypeName (BoardAudioEngine::ModulationType type)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:         return "LFO";
        case BoardAudioEngine::ModulationType::curve:       return "Curve";
        case BoardAudioEngine::ModulationType::envelope:    return "Env";
        case BoardAudioEngine::ModulationType::audioFollow: return "Audio Follow";
        case BoardAudioEngine::ModulationType::random:      return "Rand";
    }

    return "Modulator";
}

juce::String getModulationControlTargetName (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationControlTarget target)
{
    if (target == BoardAudioEngine::ModulationControlTarget::trigger)
        return "Trigger";
    if (target
        == BoardAudioEngine::ModulationControlTarget::envelopeDecay)
        return "Decay";
    if (target
        == BoardAudioEngine::ModulationControlTarget::envelopeSustain)
        return "Sustain";
    if (target == BoardAudioEngine::ModulationControlTarget::jitter)
        return "Events/Cycle";
    if (target == BoardAudioEngine::ModulationControlTarget::jitterRange)
        return "Range";
    if (target == BoardAudioEngine::ModulationControlTarget::primary)
        return type == BoardAudioEngine::ModulationType::envelope
                    || type == BoardAudioEngine::ModulationType::audioFollow
               ? "Attack"
               : "Rate";

    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:         return "Shape";
        case BoardAudioEngine::ModulationType::envelope:
        case BoardAudioEngine::ModulationType::audioFollow: return "Release";
        case BoardAudioEngine::ModulationType::random:      return "Smooth";
        case BoardAudioEngine::ModulationType::curve:       break;
    }

    return "Secondary";
}

std::optional<BoardAudioEngine::ModulationType> modulationTypeFromString (
    const juce::String& type)
{
    if (type == "lfo")         return BoardAudioEngine::ModulationType::lfo;
    if (type == "curve")       return BoardAudioEngine::ModulationType::curve;
    if (type == "envelope")    return BoardAudioEngine::ModulationType::envelope;
    if (type == "audioFollow") return BoardAudioEngine::ModulationType::audioFollow;
    if (type == "random")      return BoardAudioEngine::ModulationType::random;
    return std::nullopt;
}

juce::String lfoWaveformToString (
    BoardAudioEngine::LfoWaveform waveform)
{
    switch (waveform)
    {
        case BoardAudioEngine::LfoWaveform::sine:          return "sine";
        case BoardAudioEngine::LfoWaveform::triangle:      return "triangle";
        case BoardAudioEngine::LfoWaveform::sawUp:         return "sawUp";
        case BoardAudioEngine::LfoWaveform::sawDown:       return "sawDown";
        case BoardAudioEngine::LfoWaveform::square:        return "square";
        case BoardAudioEngine::LfoWaveform::pulse:         return "pulse";
        case BoardAudioEngine::LfoWaveform::steps:         return "steps";
        case BoardAudioEngine::LfoWaveform::sampleAndHold: return "sampleAndHold";
        case BoardAudioEngine::LfoWaveform::smoothRandom:  return "smoothRandom";
        case BoardAudioEngine::LfoWaveform::legacyMorph:   return "legacyMorph";
    }

    return "sine";
}

std::optional<BoardAudioEngine::LfoWaveform> lfoWaveformFromString (
    const juce::String& waveform)
{
    if (waveform == "sine")
        return BoardAudioEngine::LfoWaveform::sine;
    if (waveform == "triangle")
        return BoardAudioEngine::LfoWaveform::triangle;
    if (waveform == "sawUp")
        return BoardAudioEngine::LfoWaveform::sawUp;
    if (waveform == "sawDown")
        return BoardAudioEngine::LfoWaveform::sawDown;
    if (waveform == "square")
        return BoardAudioEngine::LfoWaveform::square;
    if (waveform == "pulse")
        return BoardAudioEngine::LfoWaveform::pulse;
    if (waveform == "steps")
        return BoardAudioEngine::LfoWaveform::steps;
    if (waveform == "sampleAndHold")
        return BoardAudioEngine::LfoWaveform::sampleAndHold;
    if (waveform == "smoothRandom")
        return BoardAudioEngine::LfoWaveform::smoothRandom;
    if (waveform == "legacyMorph")
        return BoardAudioEngine::LfoWaveform::legacyMorph;
    return std::nullopt;
}

juce::String modulationPolarityToString (
    BoardAudioEngine::ModulationPolarity polarity)
{
    switch (polarity)
    {
        case BoardAudioEngine::ModulationPolarity::positive:
            return "positive";
        case BoardAudioEngine::ModulationPolarity::inverted:
            return "inverted";
        case BoardAudioEngine::ModulationPolarity::bipolar:
            return "bipolar";
    }

    return "bipolar";
}

std::optional<BoardAudioEngine::ModulationPolarity>
modulationPolarityFromString (const juce::String& polarity)
{
    if (polarity == "positive")
        return BoardAudioEngine::ModulationPolarity::positive;
    if (polarity == "inverted")
        return BoardAudioEngine::ModulationPolarity::inverted;
    if (polarity == "bipolar")
        return BoardAudioEngine::ModulationPolarity::bipolar;
    return std::nullopt;
}

BoardAudioEngine::ModulationPolarity getDefaultModulationPolarity (
    BoardAudioEngine::ModulationType type)
{
    return type == BoardAudioEngine::ModulationType::envelope
                || type == BoardAudioEngine::ModulationType::audioFollow
           ? BoardAudioEngine::ModulationPolarity::positive
           : BoardAudioEngine::ModulationPolarity::bipolar;
}

float applyModulationPolarity (
    float unipolarValue,
    BoardAudioEngine::ModulationPolarity polarity)
{
    const auto value = juce::jlimit (0.0f, 1.0f, unipolarValue);
    switch (polarity)
    {
        case BoardAudioEngine::ModulationPolarity::positive:
            return value;
        case BoardAudioEngine::ModulationPolarity::inverted:
            return value - 1.0f;
        case BoardAudioEngine::ModulationPolarity::bipolar:
            return value * 2.0f - 1.0f;
    }

    return value;
}

juce::String modulationControlTargetToString (
    BoardAudioEngine::ModulationControlTarget target)
{
    switch (target)
    {
        case BoardAudioEngine::ModulationControlTarget::primary:
            return "primary";
        case BoardAudioEngine::ModulationControlTarget::secondary:
            return "secondary";
        case BoardAudioEngine::ModulationControlTarget::envelopeDecay:
            return "envelopeDecay";
        case BoardAudioEngine::ModulationControlTarget::envelopeSustain:
            return "envelopeSustain";
        case BoardAudioEngine::ModulationControlTarget::jitter:
            return "jitter";
        case BoardAudioEngine::ModulationControlTarget::jitterRange:
            return "jitterRange";
        case BoardAudioEngine::ModulationControlTarget::trigger:
            return "trigger";
    }

    return {};
}

std::optional<BoardAudioEngine::ModulationControlTarget>
modulationControlTargetFromString (const juce::String& target)
{
    if (target == "primary")
        return BoardAudioEngine::ModulationControlTarget::primary;
    if (target == "secondary")
        return BoardAudioEngine::ModulationControlTarget::secondary;
    if (target == "envelopeDecay")
        return BoardAudioEngine::ModulationControlTarget::envelopeDecay;
    if (target == "envelopeSustain")
        return BoardAudioEngine::ModulationControlTarget::envelopeSustain;
    if (target == "jitter")
        return BoardAudioEngine::ModulationControlTarget::jitter;
    if (target == "jitterRange")
        return BoardAudioEngine::ModulationControlTarget::jitterRange;
    if (target == "trigger")
        return BoardAudioEngine::ModulationControlTarget::trigger;
    return std::nullopt;
}

bool isModulationControlTargetSupported (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationControlTarget target)
{
    if (target == BoardAudioEngine::ModulationControlTarget::envelopeDecay
        || target
               == BoardAudioEngine::ModulationControlTarget::envelopeSustain)
        return type == BoardAudioEngine::ModulationType::envelope;

    if (target == BoardAudioEngine::ModulationControlTarget::jitter
        || target == BoardAudioEngine::ModulationControlTarget::jitterRange)
        return type == BoardAudioEngine::ModulationType::random;

    if (target == BoardAudioEngine::ModulationControlTarget::secondary
        && type == BoardAudioEngine::ModulationType::curve)
        return false;

    return target != BoardAudioEngine::ModulationControlTarget::trigger
        || type == BoardAudioEngine::ModulationType::curve
        || type == BoardAudioEngine::ModulationType::envelope;
}

juce::String curveLoopModeToString (
    BoardAudioEngine::CurveLoopMode mode)
{
    switch (mode)
    {
        case BoardAudioEngine::CurveLoopMode::oneTime:     return "oneTime";
        case BoardAudioEngine::CurveLoopMode::fullLoop:    return "fullLoop";
        case BoardAudioEngine::CurveLoopMode::sectionLoop: return "sectionLoop";
        case BoardAudioEngine::CurveLoopMode::yoYo:        return "yoYo";
    }

    return "fullLoop";
}

std::optional<BoardAudioEngine::CurveLoopMode> curveLoopModeFromString (
    const juce::String& mode)
{
    if (mode == "oneTime")
        return BoardAudioEngine::CurveLoopMode::oneTime;
    if (mode == "fullLoop")
        return BoardAudioEngine::CurveLoopMode::fullLoop;
    if (mode == "sectionLoop")
        return BoardAudioEngine::CurveLoopMode::sectionLoop;
    if (mode == "yoYo")
        return BoardAudioEngine::CurveLoopMode::yoYo;
    return std::nullopt;
}

std::vector<BoardAudioEngine::CurvePoint> makeDefaultCurvePoints()
{
    std::vector<BoardAudioEngine::CurvePoint> points;
    points.reserve (8);
    for (int index = 0; index < 8; ++index)
    {
        const auto value = static_cast<float> (index) / 7.0f;
        points.push_back ({ value, value });
    }
    return points;
}

std::optional<std::vector<BoardAudioEngine::CurvePoint>>
normaliseCurvePoints (
    const std::vector<BoardAudioEngine::CurvePoint>& input)
{
    if (input.size() < 2 || input.size() > maximumModulationCurvePoints)
        return std::nullopt;

    auto points = input;
    for (auto& point : points)
    {
        if (! std::isfinite (point.x)
            || ! std::isfinite (point.y)
            || ! std::isfinite (point.leftPadding)
            || ! std::isfinite (point.rightPadding))
            return std::nullopt;

        point.x = juce::jlimit (0.0f, 1.0f, point.x);
        point.y = juce::jlimit (0.0f, 1.0f, point.y);
        point.leftPadding =
            juce::jlimit (0.0f, 0.49f, point.leftPadding);
        point.rightPadding =
            juce::jlimit (0.0f, 0.49f, point.rightPadding);
    }

    std::stable_sort (
        points.begin(),
        points.end(),
        [] (const auto& first, const auto& second)
        {
            return first.x < second.x;
        });

    std::vector<BoardAudioEngine::CurvePoint> uniquePoints;
    uniquePoints.reserve (points.size());
    constexpr auto minimumPointDistance = 0.000001f;
    for (const auto& point : points)
    {
        if (! uniquePoints.empty()
            && point.x - uniquePoints.back().x < minimumPointDistance)
        {
            uniquePoints.back() = point;
            continue;
        }

        uniquePoints.push_back (point);
    }

    if (uniquePoints.size() < 2)
        return std::nullopt;

    uniquePoints.front().x = 0.0f;
    uniquePoints.back().x = 1.0f;
    return uniquePoints;
}

float sampleCurvePoints (
    const std::vector<BoardAudioEngine::CurvePoint>& points,
    double position)
{
    if (points.empty())
        return 0.5f;

    const auto x = static_cast<float> (
        juce::jlimit (0.0, 1.0, position));
    if (x <= points.front().x)
        return points.front().y;
    if (x >= points.back().x)
        return points.back().y;

    const auto upper = std::upper_bound (
        points.begin(),
        points.end(),
        x,
        [] (float value, const auto& point)
        {
            return value < point.x;
        });
    const auto& second = *upper;
    const auto& first = *std::prev (upper);
    const auto span = juce::jmax (0.000001f, second.x - first.x);
    const auto controlX1 =
        first.x + span * first.rightPadding;
    const auto controlX2 =
        second.x - span * second.leftPadding;
    const auto sampleCubic =
        [] (float start, float control1, float control2, float end, float t)
        {
            const auto inverse = 1.0f - t;
            return inverse * inverse * inverse * start
                + 3.0f * inverse * inverse * t * control1
                + 3.0f * inverse * t * t * control2
                + t * t * t * end;
        };

    auto lowerT = 0.0f;
    auto upperT = 1.0f;
    for (int iteration = 0; iteration < 20; ++iteration)
    {
        const auto middleT = (lowerT + upperT) * 0.5f;
        if (sampleCubic (
                first.x, controlX1, controlX2, second.x, middleT)
            < x)
            lowerT = middleT;
        else
            upperT = middleT;
    }

    return sampleCubic (
        first.y,
        first.y,
        second.y,
        second.y,
        (lowerT + upperT) * 0.5f);
}

double wrapPositive (double value, double length)
{
    if (length <= 0.0)
        return 0.0;

    const auto wrapped = std::fmod (value, length);
    return wrapped < 0.0 ? wrapped + length : wrapped;
}

float advanceBipolarRandom (juce::uint32& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    if (state == 0)
        state = 1;

    return juce::jlimit (
        -1.0f,
        1.0f,
        static_cast<float> (state & 0x00ffffffu)
            / static_cast<float> (0x007fffffu)
            - 1.0f);
}

double advanceExponentialRandom (juce::uint32& state)
{
    const auto unit = juce::jlimit (
        0.000001f,
        0.999999f,
        advanceBipolarRandom (state) * 0.5f + 0.5f);
    return -std::log (1.0 - static_cast<double> (unit));
}

float getDefaultModulationPrimary (BoardAudioEngine::ModulationType type)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:         return 1.0f;
        case BoardAudioEngine::ModulationType::curve:       return 0.5f;
        case BoardAudioEngine::ModulationType::envelope:    return 20.0f;
        case BoardAudioEngine::ModulationType::audioFollow: return 8.0f;
        case BoardAudioEngine::ModulationType::random:      return 2.0f;
    }

    return 0.0f;
}

float getDefaultModulationSecondary (BoardAudioEngine::ModulationType type)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:         return 0.0f;
        case BoardAudioEngine::ModulationType::curve:       return 0.0f;
        case BoardAudioEngine::ModulationType::envelope:    return 350.0f;
        case BoardAudioEngine::ModulationType::audioFollow: return 180.0f;
        case BoardAudioEngine::ModulationType::random:      return 0.25f;
    }

    return 0.0f;
}

float clampModulationPrimary (
    BoardAudioEngine::ModulationType type,
    float value)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:
        case BoardAudioEngine::ModulationType::curve:
        case BoardAudioEngine::ModulationType::random:
            return juce::jlimit (0.01f, 40.0f, value);

        case BoardAudioEngine::ModulationType::envelope:
        case BoardAudioEngine::ModulationType::audioFollow:
            return juce::jlimit (1.0f, 2000.0f, value);
    }

    return value;
}

float clampModulationSecondary (
    BoardAudioEngine::ModulationType type,
    float value)
{
    switch (type)
    {
        case BoardAudioEngine::ModulationType::lfo:
        case BoardAudioEngine::ModulationType::random:
            return juce::jlimit (0.0f, 1.0f, value);

        case BoardAudioEngine::ModulationType::curve:
            return 0.0f;

        case BoardAudioEngine::ModulationType::envelope:
        case BoardAudioEngine::ModulationType::audioFollow:
            return juce::jlimit (5.0f, 5000.0f, value);
    }

    return value;
}

float clampModulationJitter (float value)
{
    return juce::jlimit (0.0f, 8.0f, value);
}

float clampModulationJitterRange (float value)
{
    return juce::jlimit (0.0f, 1.0f, value);
}

float clampModulationEnvelopeDecay (float value)
{
    return juce::jlimit (5.0f, 5000.0f, value);
}

float clampModulationEnvelopeSustain (float value)
{
    return juce::jlimit (0.0f, 1.0f, value);
}

juce::NormalisableRange<float> getModulationControlRange (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationControlTarget target)
{
    if (target
        == BoardAudioEngine::ModulationControlTarget::envelopeDecay)
    {
        juce::NormalisableRange<float> range (5.0f, 5000.0f);
        range.setSkewForCentre (300.0f);
        return range;
    }

    if (target
        == BoardAudioEngine::ModulationControlTarget::envelopeSustain)
        return { 0.0f, 1.0f };

    if (target == BoardAudioEngine::ModulationControlTarget::jitter)
    {
        juce::NormalisableRange<float> range (0.0f, 8.0f);
        range.setSkewForCentre (1.0f);
        return range;
    }

    if (target == BoardAudioEngine::ModulationControlTarget::jitterRange)
        return { 0.0f, 1.0f };

    if (target == BoardAudioEngine::ModulationControlTarget::primary)
    {
        if (type == BoardAudioEngine::ModulationType::envelope
            || type == BoardAudioEngine::ModulationType::audioFollow)
        {
            juce::NormalisableRange<float> range (1.0f, 2000.0f);
            range.setSkewForCentre (100.0f);
            return range;
        }

        juce::NormalisableRange<float> range (0.01f, 40.0f);
        range.setSkewForCentre (1.0f);
        return range;
    }

    if (type == BoardAudioEngine::ModulationType::envelope
        || type == BoardAudioEngine::ModulationType::audioFollow)
    {
        juce::NormalisableRange<float> range (5.0f, 5000.0f);
        range.setSkewForCentre (300.0f);
        return range;
    }

    return { 0.0f, 1.0f };
}

float modulationControlToNormalised (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationControlTarget target,
    float physicalValue)
{
    return juce::jlimit (
        0.0f,
        1.0f,
        getModulationControlRange (type, target)
            .convertTo0to1 (physicalValue));
}

float modulationControlFromNormalised (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationControlTarget target,
    float normalisedValue)
{
    return getModulationControlRange (type, target)
        .convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalisedValue));
}

float modulationOutputToNormalisedForTrigger (
    BoardAudioEngine::ModulationType type,
    BoardAudioEngine::ModulationPolarity polarity,
    float output)
{
    if (polarity == BoardAudioEngine::ModulationPolarity::positive)
        return juce::jlimit (0.0f, 1.0f, output);

    if (polarity == BoardAudioEngine::ModulationPolarity::inverted)
        return juce::jlimit (
            0.0f,
            1.0f,
            type == BoardAudioEngine::ModulationType::envelope
                    || type
                           == BoardAudioEngine::ModulationType::audioFollow
                ? -output
                : output + 1.0f);

    return juce::jlimit (0.0f, 1.0f, output * 0.5f + 0.5f);
}

BoardAudioEngine::ModulationID readModulationID (
    const juce::XmlElement& xml,
    const juce::String& attribute)
{
    const auto value =
        xml.getStringAttribute (attribute).getLargeIntValue();
    return value > 0
               ? static_cast<BoardAudioEngine::ModulationID> (value)
               : 0;
}

bool setMainLayout (juce::AudioPluginInstance& instance,
                    const juce::AudioChannelSet& mainLayout)
{
    if (instance.getBusCount (true) == 0 || instance.getBusCount (false) == 0)
        return false;

    auto layout = instance.getBusesLayout();

    for (int bus = 0; bus < layout.inputBuses.size(); ++bus)
        layout.inputBuses.set (bus, bus == 0 ? mainLayout
                                             : juce::AudioChannelSet::disabled());

    for (int bus = 0; bus < layout.outputBuses.size(); ++bus)
        layout.outputBuses.set (bus, bus == 0 ? mainLayout
                                              : juce::AudioChannelSet::disabled());

    return instance.checkBusesLayoutSupported (layout)
        && instance.setBusesLayout (layout)
        && instance.getMainBusNumInputChannels() == mainLayout.size()
        && instance.getMainBusNumOutputChannels() == mainLayout.size()
        && instance.getTotalNumInputChannels() == mainLayout.size()
        && instance.getTotalNumOutputChannels() == mainLayout.size();
}

bool hasNativeStereoMainBus (const juce::AudioPluginInstance& instance)
{
    return instance.getMainBusNumInputChannels() == 2
        && instance.getMainBusNumOutputChannels() == 2;
}

juce::String describeMainLayout (const juce::AudioPluginInstance& instance)
{
    return juce::String (instance.getMainBusNumInputChannels())
         + "-in/"
         + juce::String (instance.getMainBusNumOutputChannels())
         + "-out main bus";
}

juce::String normalisePluginName (juce::String text)
{
    return text.removeCharacters (" _-.").toLowerCase();
}

bool factoryPresetManufacturerMatches (
    const BoardPresetLibrary::Node& recipe,
    const juce::PluginDescription& candidate)
{
    if (candidate.manufacturerName.equalsIgnoreCase (recipe.manufacturer))
        return true;

    if (! recipe.pluginName.equalsIgnoreCase ("TB_EQ"))
        return false;

    const auto isKnownEqManufacturer = [] (const juce::String& name)
    {
        return name.equalsIgnoreCase ("yourcompany")
            || name.containsIgnoreCase ("Team Impulse Impact");
    };
    return isKnownEqManufacturer (recipe.manufacturer)
        && isKnownEqManufacturer (candidate.manufacturerName);
}

bool isTBBoardDescription (const juce::PluginDescription& description)
{
    const auto nameMatches =
        normalisePluginName (description.name) == "tbboard"
        || normalisePluginName (description.descriptiveName) == "tbboard"
        || normalisePluginName (
               juce::File (description.fileOrIdentifier).getFileNameWithoutExtension())
               == "tbboard";

    if (! nameMatches)
        return false;

    return description.manufacturerName.containsIgnoreCase ("Team Impulse")
        || description.fileOrIdentifier.containsIgnoreCase ("TB_Board.vst3");
}

bool getPluginDescriptionForProcessor (juce::AudioProcessor& processor,
                                       juce::PluginDescription& description,
                                       bool& isDualMono)
{
    if (auto* gainProcessor = dynamic_cast<NodeGainProcessor*> (&processor))
        return getPluginDescriptionForProcessor (
            gainProcessor->getWrappedProcessor(), description, isDualMono);

    if (auto* dualMono = dynamic_cast<DualMonoProcessor*> (&processor))
    {
        description = dualMono->getLeftInstance().getPluginDescription();
        isDualMono = true;
        return true;
    }

    if (auto* instance = dynamic_cast<juce::AudioPluginInstance*> (&processor))
    {
        description = instance->getPluginDescription();
        isDualMono = false;
        return true;
    }

    return false;
}

juce::AudioProcessor* getParameterProcessor (juce::AudioProcessor* processor)
{
    if (auto* gainProcessor = dynamic_cast<NodeGainProcessor*> (processor))
        processor = &gainProcessor->getWrappedProcessor();

    if (auto* dualMono = dynamic_cast<DualMonoProcessor*> (processor))
        return &dualMono->getLeftInstance();

    return processor;
}

DualMonoProcessor* getDualMonoProcessor (juce::AudioProcessor* processor)
{
    if (auto* gainProcessor = dynamic_cast<NodeGainProcessor*> (processor))
        processor = &gainProcessor->getWrappedProcessor();

    return dynamic_cast<DualMonoProcessor*> (processor);
}

NodeGainProcessor* getNodeGainProcessor (juce::AudioProcessor* processor)
{
    return dynamic_cast<NodeGainProcessor*> (processor);
}

juce::String normalisePresetParameterName (juce::String name)
{
    return name.trim().removeCharacters (" _-.").toLowerCase();
}

bool configureFactoryPresetProcessor (
    juce::AudioProcessor& processor,
    const BoardPresetLibrary::Node& recipe,
    juce::String& error)
{
    if (recipe.programName.isNotEmpty())
    {
        auto programIndex = -1;
        for (auto index = 0; index < processor.getNumPrograms(); ++index)
            if (processor.getProgramName (index).trim().equalsIgnoreCase (
                    recipe.programName))
            {
                programIndex = index;
                break;
            }

        if (programIndex < 0)
        {
            error = recipe.pluginName + " does not expose the required \""
                  + recipe.programName + "\" host preset.";
            return false;
        }

        processor.setCurrentProgram (programIndex);
        if (processor.getCurrentProgram() != programIndex)
        {
            error = recipe.pluginName + " did not select the required \""
                  + recipe.programName + "\" host preset.";
            return false;
        }
    }

    auto* parameterProcessor = getParameterProcessor (&processor);
    if (parameterProcessor == nullptr)
    {
        error = recipe.pluginName + " exposes no controllable parameter set.";
        return false;
    }

    const auto& parameters = parameterProcessor->getParameters();
    for (const auto& setting : recipe.parameters)
    {
        const auto wantedName =
            normalisePresetParameterName (setting.name);
        auto parameterIndex = -1;
        for (auto index = 0; index < parameters.size(); ++index)
            if (normalisePresetParameterName (
                    parameters.getUnchecked (index)->getName (96))
                == wantedName)
            {
                parameterIndex = index;
                break;
            }

        if (parameterIndex < 0)
        {
            error = recipe.pluginName + " does not expose the required \""
                  + setting.name + "\" parameter.";
            return false;
        }

        const auto requestedValue =
            juce::jlimit (0.0f, 1.0f, setting.normalisedValue);
        if (auto* dualMono = getDualMonoProcessor (&processor))
            dualMono->setLinkedParameterValue (
                parameterIndex, requestedValue);
        else
            parameters.getUnchecked (parameterIndex)
                ->setValueNotifyingHost (requestedValue);

        const auto* parameter =
            parameters.getUnchecked (parameterIndex);
        auto expectedValue = requestedValue;
        const auto steps = parameter->getNumSteps();
        auto tolerance = 0.001f;
        if (steps > 1
            && steps < std::numeric_limits<int>::max())
        {
            expectedValue =
                std::round (
                    requestedValue
                    * static_cast<float> (steps - 1))
                / static_cast<float> (steps - 1);
            tolerance = juce::jmax (
                tolerance,
                0.51f / static_cast<float> (steps - 1));
        }

        if (std::abs (parameter->getValue() - expectedValue)
            > tolerance)
        {
            error = recipe.pluginName + " did not retain the requested \""
                  + setting.name + "\" value.";
            return false;
        }
    }

    return true;
}

juce::uint32 readNodeUID (const juce::XmlElement& xml, const juce::String& attribute)
{
    const auto value = xml.getStringAttribute (attribute).getLargeIntValue();
    return value > 0 && value <= std::numeric_limits<juce::uint32>::max()
               ? static_cast<juce::uint32> (value)
               : 0;
}

void logRestoreFailure (const juce::String& reason)
{
    juce::Logger::writeToLog ("TB_Board state restore rejected: " + reason);
}

juce::AudioPluginFormat* findVst3Format (
    juce::AudioPluginFormatManager& manager)
{
    for (auto* format : manager.getFormats())
        if (format != nullptr
            && format->getName().equalsIgnoreCase ("VST3"))
            return format;

    return nullptr;
}

CatalogScanResult scanVst3Folder (
    const juce::File& folder,
    const juce::File& deadMansPedalFile,
    const juce::StringArray& existingBlacklistedFiles,
    const std::function<bool()>& shouldStop)
{
    CatalogScanResult result;
    result.scannedFolder = folder;
    if (! folder.isDirectory())
        return result;

    juce::AudioPluginFormatManager scannerFormatManager;
    juce::addDefaultFormatsToManager (scannerFormatManager);
    auto* vst3Format = findVst3Format (scannerFormatManager);
    if (vst3Format == nullptr)
        return result;

    juce::KnownPluginList scannedPlugins;
    for (const auto& file : existingBlacklistedFiles)
        scannedPlugins.addToBlacklist (file);

    juce::FileSearchPath searchPath;
    searchPath.add (folder);
    juce::PluginDirectoryScanner scanner (
        scannedPlugins,
        *vst3Format,
        searchPath,
        true,
        deadMansPedalFile,
        true);

    juce::String pluginBeingScanned;
    for (;;)
    {
        if (shouldStop())
            break;

        if (! scanner.scanNextFile (false, pluginBeingScanned))
        {
            result.completed = true;
            break;
        }
    }

    result.pluginTypes = scannedPlugins.getTypes();
    result.blacklistedFiles = scannedPlugins.getBlacklistedFiles();
    result.failedFiles = scanner.getFailedFiles();
    return result;
}

juce::StringArray findMissingFactoryPresetPlugins (
    const juce::KnownPluginList& knownPlugins,
    const BoardPresetLibrary::Preset& preset)
{
    const auto knownTypes = knownPlugins.getTypes();
    juce::StringArray missing;

    for (const auto& recipeNode : preset.nodes)
    {
        const auto match = std::find_if (
            knownTypes.begin(),
            knownTypes.end(),
            [&recipeNode] (const auto& candidate)
            {
                return candidate.pluginFormatName.equalsIgnoreCase ("VST3")
                    && candidate.name.equalsIgnoreCase (
                        recipeNode.pluginName)
                    && factoryPresetManufacturerMatches (
                        recipeNode, candidate)
                    && ! candidate.isInstrument
                    && ! isTBBoardDescription (candidate)
                    && juce::File (
                           candidate.fileOrIdentifier).exists();
            });

        if (match == knownTypes.end())
            missing.addIfNotAlreadyThere (recipeNode.pluginName);
    }

    return missing;
}

CatalogScanResult scanFactoryPresetDependencies (
    const juce::File& folder,
    const juce::StringArray& pluginNames,
    const std::function<bool()>& shouldStop)
{
    CatalogScanResult result;
    result.scannedFolder = folder;
    if (! folder.isDirectory())
        return result;

    juce::AudioPluginFormatManager scannerFormatManager;
    juce::addDefaultFormatsToManager (scannerFormatManager);
    auto* vst3Format = findVst3Format (scannerFormatManager);
    if (vst3Format == nullptr)
        return result;

    for (const auto& pluginName : pluginNames)
    {
        if (shouldStop())
            return result;

        const auto bundle =
            folder.getChildFile (pluginName + ".vst3");
        if (! bundle.exists())
        {
            result.failedFiles.add (bundle.getFullPathName());
            continue;
        }

        juce::OwnedArray<juce::PluginDescription> foundTypes;
        vst3Format->findAllTypesForFile (
            foundTypes, bundle.getFullPathName());
        if (foundTypes.isEmpty())
        {
            result.failedFiles.add (bundle.getFullPathName());
            continue;
        }

        for (const auto* description : foundTypes)
            if (description != nullptr)
                result.pluginTypes.add (*description);
    }

    result.completed = true;
    return result;
}

class CatalogScanJob final : public juce::ThreadPoolJob
{
public:
    using Completion = std::function<void (CatalogScanResult)>;

    CatalogScanJob (juce::File folderToScan,
                    juce::File deadMansPedalFileToUse,
                    juce::StringArray existingBlacklistedFilesToUse,
                    Completion completionToUse)
        : ThreadPoolJob ("TB_Board VST3 folder scan"),
          folder (std::move (folderToScan)),
          deadMansPedalFile (std::move (deadMansPedalFileToUse)),
          existingBlacklistedFiles (
              std::move (existingBlacklistedFilesToUse)),
          completion (std::move (completionToUse))
    {
    }

    JobStatus runJob() override
    {
        auto result = scanVst3Folder (
            folder,
            deadMansPedalFile,
            existingBlacklistedFiles,
            [this] { return shouldExit(); });

        if (result.completed && ! shouldExit())
        {
            juce::MessageManager::callAsync (
                [completionToCall = std::move (completion),
                 resultToDeliver = std::move (result)] () mutable
                {
                    if (completionToCall != nullptr)
                        completionToCall (std::move (resultToDeliver));
                });
        }

        return jobHasFinished;
    }

private:
    juce::File folder;
    juce::File deadMansPedalFile;
    juce::StringArray existingBlacklistedFiles;
    Completion completion;
};

class FactoryPresetCatalogScanJob final : public juce::ThreadPoolJob
{
public:
    using Completion = std::function<void (CatalogScanResult)>;

    FactoryPresetCatalogScanJob (
        juce::File folderToScan,
        juce::StringArray pluginNamesToScan,
        Completion completionToUse)
        : ThreadPoolJob ("TB_Board factory preset VST3 bootstrap"),
          folder (std::move (folderToScan)),
          pluginNames (std::move (pluginNamesToScan)),
          completion (std::move (completionToUse))
    {
    }

    JobStatus runJob() override
    {
        auto result = scanFactoryPresetDependencies (
            folder,
            pluginNames,
            [this] { return shouldExit(); });

        if (result.completed && ! shouldExit())
        {
            juce::MessageManager::callAsync (
                [completionToCall = std::move (completion),
                 resultToDeliver = std::move (result)] () mutable
                {
                    if (completionToCall != nullptr)
                        completionToCall (std::move (resultToDeliver));
                });
        }

        return jobHasFinished;
    }

private:
    juce::File folder;
    juce::StringArray pluginNames;
    Completion completion;
};
}

struct BoardAudioEngine::CallbackGate
{
    juce::CriticalSection lock;
    BoardAudioEngine* owner = nullptr;
};

struct BoardAudioEngine::FactoryPresetLoad
{
    const BoardPresetLibrary::Preset* preset = nullptr;
    std::vector<juce::PluginDescription> descriptions;
    std::vector<std::unique_ptr<juce::AudioProcessor>> processors;
    std::vector<InstanceMode> modes;
    std::vector<NodeID> existingNodeIDs;
    FactoryPresetCallback completion;
    size_t nextProcessor = 0;
    juce::uint64 generation = 0;
    juce::uint64 operationToken = 0;
    bool replaceExisting = false;
};

struct BoardAudioEngine::CurveState
{
    std::vector<CurvePoint> points;
    CurveLoopMode loopMode = CurveLoopMode::fullLoop;
    float loopStart = 0.25f;
    float loopEnd = 0.75f;
};

struct BoardAudioEngine::ModulationSource
{
    enum class EnvelopeStage
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    ModulationSource (ModulationID sourceID, ModulationType sourceType)
        : id (sourceID),
          type (sourceType),
          polarity (getDefaultModulationPolarity (sourceType)),
          primary (getDefaultModulationPrimary (sourceType)),
          secondary (getDefaultModulationSecondary (sourceType)),
          effectiveSecondary (getDefaultModulationSecondary (sourceType)),
          randomState (static_cast<juce::uint32> (
              sourceID ^ (sourceID >> 32) ^ 0x9e3779b9ULL)),
          randomJitterState (static_cast<juce::uint32> (
              sourceID ^ (sourceID >> 32) ^ 0x85ebca6bULL))
    {
        auto initialCurve = std::make_unique<CurveState>();
        initialCurve->points = makeDefaultCurvePoints();
        curveState = std::move (initialCurve);

        randomTarget =
            static_cast<float> (randomState & 0x00ffffffu)
                / static_cast<float> (0x007fffffu)
            - 1.0f;
        randomValue = randomTarget;
        randomTransitionStart = randomTarget;
        randomJitterHazardRemaining =
            advanceExponentialRandom (randomJitterState);
        lfoRandomStart.store (randomValue, std::memory_order_relaxed);
        lfoRandomEnd.store (randomTarget, std::memory_order_relaxed);
    }

    const ModulationID id;
    const ModulationType type;
    std::atomic<LfoWaveform> lfoWaveform { LfoWaveform::sine };
    std::atomic<ModulationPolarity> polarity;
    std::atomic<float> primary;
    std::atomic<float> secondary;
    std::atomic<float> effectiveSecondary;
    std::atomic<float> envelopeDecay { 200.0f };
    std::atomic<float> envelopeSustain { 0.70f };
    std::atomic<float> jitter { 0.0f };
    std::atomic<float> jitterRange { 0.25f };
    std::atomic<bool> autoTrigger { true };
    std::atomic<bool> manualTriggerPending { false };
    std::atomic<bool> manualGateHigh { false };
    std::atomic<float> output { 0.0f };
    std::atomic<float> displayPhase { 0.0f };
    std::atomic<float> lfoRandomStart { 0.0f };
    std::atomic<float> lfoRandomEnd { 0.0f };
    std::atomic<juce::uint32> displayStateVersion { 0 };
    std::unique_ptr<const CurveState> curveState;

    float previousBlockOutput = 0.0f;
    double phase = 0.0;
    float envelopeValue = 0.0f;
    float envelopeReleaseStartValue = 0.0f;
    float envelopeBipolarValue = 0.0f;
    float envelopeBipolarStageStartValue = 0.0f;
    double envelopeStageSeconds = 0.0;
    EnvelopeStage envelopeStage = EnvelopeStage::idle;
    bool envelopeGateHeld = false;
    bool envelopeTriggeredThisBlock = false;
    float followerValue = 0.0f;
    float randomValue = 0.0f;
    float randomTarget = 0.0f;
    float randomTransitionStart = 0.0f;
    double randomTransitionPhase = 1.0;
    juce::uint32 randomState = 1;
    juce::uint32 randomJitterState = 1;
    double randomJitterHazardRemaining = 1.0;
    bool triggerRunning = false;
    double triggerRefractorySeconds = 0.0;
};

struct BoardAudioEngine::ModulationTarget
{
    juce::AudioProcessorGraph::Node::Ptr node;
    juce::AudioProcessor* processor = nullptr;
    juce::AudioProcessorParameter* parameter = nullptr;
    int parameterIndex = -1;
    juce::String parameterID;
    juce::String parameterName;
    juce::String targetName;
    std::atomic<float> baseValue { 0.0f };
    std::atomic<float> lastAppliedValue { -1.0f };
    std::atomic<bool> userEditing { false };
    std::atomic<float> gestureStartValue { 0.0f };
    std::atomic<float> gestureStartBaseValue { 0.0f };
    std::atomic<int> holdBlocks { 0 };
};

struct BoardAudioEngine::ModulationLink
{
    ModulationID id = 0;
    std::shared_ptr<ModulationSource> source;
    std::shared_ptr<ModulationTarget> target;
    std::atomic<float> amount { 1.0f };
};

struct BoardAudioEngine::ModulationControlLink
{
    ModulationID id = 0;
    std::shared_ptr<ModulationSource> source;
    std::shared_ptr<ModulationSource> targetSource;
    ModulationControlTarget target = ModulationControlTarget::primary;
    std::atomic<float> amount { 1.0f };
    bool triggerGateHigh = false;
    bool gateResyncPending = false;
};

struct BoardAudioEngine::ModulationRuntime
{
    struct TargetEntry
    {
        std::shared_ptr<ModulationTarget> target;
        std::vector<std::shared_ptr<ModulationLink>> links;
    };

    std::vector<std::shared_ptr<ModulationSource>> sources;
    std::vector<TargetEntry> targets;
    std::vector<std::shared_ptr<ModulationControlLink>> controlLinks;
};

struct BoardAudioEngine::RestorePlan
{
    struct Node
    {
        NodeID nodeID;
        juce::Point<int> position;
        bool bypassed = false;
        float inputGainDb = 0.0f;
        float outputGainDb = 0.0f;
        InstanceMode mode = InstanceMode::nativeStereo;
        juce::PluginDescription description;
        juce::MemoryBlock childState;
        std::unique_ptr<juce::AudioProcessor> processor;
    };

    struct ModulationSourceState
    {
        ModulationID id = 0;
        ModulationType type = ModulationType::lfo;
        LfoWaveform lfoWaveform = LfoWaveform::sine;
        ModulationPolarity polarity = ModulationPolarity::bipolar;
        float primary = 0.0f;
        float secondary = 0.0f;
        float envelopeDecay = 200.0f;
        float envelopeSustain = 0.70f;
        float jitter = 0.0f;
        float jitterRange = 0.25f;
        bool autoTrigger = true;
        std::vector<CurvePoint> curvePoints;
        CurveLoopMode loopMode = CurveLoopMode::fullLoop;
        float loopStart = 0.25f;
        float loopEnd = 0.75f;
    };

    struct ModulationLinkState
    {
        ModulationID id = 0;
        ModulationID sourceID = 0;
        NodeID nodeID;
        juce::String parameterID;
        juce::String parameterName;
        int parameterIndex = -1;
        float baseValue = 0.0f;
        float amount = 1.0f;
    };

    struct ModulationControlLinkState
    {
        ModulationID id = 0;
        ModulationID sourceID = 0;
        ModulationID targetSourceID = 0;
        ModulationControlTarget target = ModulationControlTarget::primary;
        float amount = 1.0f;
    };

    juce::uint64 generation = 0;
    float outputInputGainDb = 0.0f;
    std::vector<Node> nodes;
    std::vector<InputCardInfo> inputCards;
    std::vector<juce::AudioProcessorGraph::Connection> connections;
    std::vector<ModulationSourceState> modulationSources;
    std::vector<ModulationLinkState> modulationLinks;
    std::vector<ModulationControlLinkState> modulationControlLinks;
};

BoardAudioEngine::BoardAudioEngine (bool catalogIsWritable)
    : AudioProcessor (BusesProperties()
                          .withInput ("Stereo Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Stereo Output", juce::AudioChannelSet::stereo(), true)),
      formatManager (std::make_shared<juce::AudioPluginFormatManager>()),
      catalogWritable (catalogIsWritable),
      callbackGate (std::make_shared<CallbackGate>())
{
    callbackGate->owner = this;
    initialiseProperties();
    juce::addDefaultFormatsToManager (*formatManager);
    loadPluginList();
    knownPlugins.addChangeListener (this);

    if (catalogWritable)
    {
        catalogScanPool = std::make_unique<juce::ThreadPool> (1);
        scanDefaultPluginFolder();
    }

    graph.setPlayConfigDetails (2, 2, currentSampleRate, currentBlockSize);
    createDefaultGraph (juce::AudioProcessorGraph::UpdateKind::sync);
    if (const auto defaultInputCard =
            makeInputCardInfo (stereoInputSource, { 35, 130 }))
        inputCards.push_back (*defaultInputCard);
    rebuildModulationRuntime();
    rebuildStateCache();
}

BoardAudioEngine::~BoardAudioEngine()
{
    agentEndpoint.reset();

    {
        const juce::ScopedLock gateLock (callbackGate->lock);
        callbackGate->owner = nullptr;
    }

    if (catalogScanPool != nullptr
        && ! catalogScanPool->removeAllJobs (true, 5000))
    {
        juce::Logger::writeToLog (
            "TB_Board abandoned a hung third-party VST3 scan during shutdown.");
        catalogScanPool.release();
    }

    restoreGeneration.fetch_add (1, std::memory_order_relaxed);
    cancelPendingUpdate();
    knownPlugins.removeChangeListener (this);

    for (auto* node : graph.getNodes())
        if (! isIONode (node->nodeID))
            detachProcessorListeners (*node->getProcessor());

    clearModulation();

    if (prepared)
        graph.releaseResources();

    if (catalogWritable)
        savePluginList();

    applicationProperties.saveIfNeeded();
}

juce::AudioPluginFormatManager& BoardAudioEngine::getFormatManager() noexcept
{
    return *formatManager;
}

juce::KnownPluginList& BoardAudioEngine::getKnownPlugins() noexcept
{
    return knownPlugins;
}

juce::AudioProcessorGraph& BoardAudioEngine::getGraph() noexcept
{
    return graph;
}

juce::PropertiesFile* BoardAudioEngine::getSettings()
{
    return applicationProperties.getUserSettings();
}

void BoardAudioEngine::enableAgentControl()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (agentEndpoint == nullptr)
    {
        setAgentStatusText ("Starting AI agent control...");
        agentEndpoint = std::make_unique<BoardAgentEndpoint> (*this);
    }
}

juce::String BoardAudioEngine::getAgentInstanceID() const
{
    return agentEndpoint != nullptr
               ? agentEndpoint->getInstanceID()
               : juce::String {};
}

bool BoardAudioEngine::isAgentConnected() const
{
    return agentEndpoint != nullptr && agentEndpoint->isConnected();
}

bool BoardAudioEngine::isAgentWorking() const
{
    return agentEndpoint != nullptr && agentEndpoint->isWorking();
}

juce::String BoardAudioEngine::getAgentStatusText() const
{
    const juce::ScopedLock lock (agentStatusLock);
    return agentStatusText;
}

bool BoardAudioEngine::submitAgentPrompt (const juce::String& prompt)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (agentEndpoint == nullptr)
    {
        setAgentStatusText ("AI agent control is not enabled");
        return false;
    }

    return agentEndpoint->submitPrompt (prompt);
}

bool BoardAudioEngine::cancelAgentPrompt()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (agentEndpoint == nullptr)
    {
        setAgentStatusText ("No AI request is running");
        return false;
    }

    return agentEndpoint->cancelPrompt();
}

bool BoardAudioEngine::canManageCatalog() const noexcept
{
    return catalogWritable;
}

bool BoardAudioEngine::isCatalogBusy() const noexcept
{
    return catalogScanJobsPending.load (std::memory_order_acquire) > 0
        || catalogManagerActive.load (std::memory_order_acquire)
        || boardEditsBlocked();
}

bool BoardAudioEngine::beginCatalogManagement()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! catalogWritable || isCatalogBusy())
        return false;

    catalogManagerActive.store (true, std::memory_order_release);
    sendChangeMessage();
    return true;
}

void BoardAudioEngine::endCatalogManagement()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (catalogManagerActive.exchange (false, std::memory_order_acq_rel))
        sendChangeMessage();
}

juce::File BoardAudioEngine::getDeadMansPedalFile()
{
    if (auto* settings = getSettings())
        return settings->getFile().getSiblingFile ("TB_Board_scan_deadman.txt");

    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("TB_Board_scan_deadman.txt");
}

juce::File BoardAudioEngine::getBackgroundScanDeadMansPedalFile()
{
    if (auto* settings = getSettings())
        return settings->getFile().getSiblingFile (
            "TB_Board_background_scan_deadman.txt");

    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("TB_Board_background_scan_deadman.txt");
}

juce::File BoardAudioEngine::getDefaultPluginFolder()
{
   #if JUCE_WINDOWS
    return juce::File (
        R"(C:\Program Files\Common Files\VST3\TB_Plugins)");
   #else
    return {};
   #endif
}

juce::File BoardAudioEngine::getLastAdditionalPluginFolder()
{
    if (auto* settings = getSettings())
    {
        const auto storedPath =
            settings->getValue (lastAdditionalPluginFolderKey);
        if (storedPath.isNotEmpty())
        {
            const juce::File storedFolder (storedPath);
            if (storedFolder.isDirectory())
                return storedFolder;
        }
    }

    return juce::File::getSpecialLocation (
        juce::File::userHomeDirectory);
}

void BoardAudioEngine::scanAdditionalPluginFolder (
    const juce::File& folder)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! catalogWritable
        || catalogScanPool == nullptr
        || ! folder.isDirectory()
        || isCatalogBusy())
        return;

    if (auto* settings = getSettings())
    {
        settings->setValue (
            lastAdditionalPluginFolderKey,
            folder.getFullPathName());
        settings->saveIfNeeded();
    }

    startCatalogScan (folder);
}

bool BoardAudioEngine::hasBoardContent() const
{
    for (auto* node : graph.getNodes())
        if (! isIONode (node->nodeID))
            return true;

    for (const auto& connection : graph.getConnections())
        if (! connection.source.isMIDI()
            && ! connection.destination.isMIDI()
            && ! isInternalConnection (connection))
            return true;

    return ! modulationSources.empty();
}

void BoardAudioEngine::applyFactoryPreset (
    const juce::String& presetID,
    bool replaceExisting,
    FactoryPresetCallback completion)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (completion == nullptr)
        return;

    if (isCatalogBusy())
    {
        completion (
            false,
            "Wait for the current scan or board edit to finish.");
        return;
    }

    const auto* preset = BoardPresetLibrary::findPreset (presetID);
    if (preset == nullptr)
    {
        completion (false, "The selected board preset is unavailable.");
        return;
    }

    if (hasBoardContent() && ! replaceExisting)
    {
        completion (
            false,
            "This preset needs permission to replace the current board.");
        return;
    }

    if (! findMissingFactoryPresetPlugins (
             knownPlugins, *preset).isEmpty())
    {
        bootstrapFactoryPresetCatalog (
            presetID,
            [this,
             presetID,
             replaceExisting,
             completionToRetry = std::move (completion)] (
                bool succeeded,
                const juce::String& message) mutable
            {
                if (! succeeded)
                {
                    completionToRetry (false, message);
                    return;
                }

                applyFactoryPreset (
                    presetID,
                    replaceExisting,
                    std::move (completionToRetry));
            });
        return;
    }

    auto load = std::make_shared<FactoryPresetLoad>();
    load->preset = preset;
    load->replaceExisting = replaceExisting;
    load->completion = std::move (completion);
    load->generation =
        restoreGeneration.load (std::memory_order_relaxed);
    load->descriptions.reserve (preset->nodes.size());
    load->processors.reserve (preset->nodes.size());
    load->modes.reserve (preset->nodes.size());

    const auto knownTypes = knownPlugins.getTypes();
    for (const auto& recipeNode : preset->nodes)
    {
        const auto match = std::find_if (
            knownTypes.begin(),
            knownTypes.end(),
            [&recipeNode] (const auto& candidate)
            {
                return candidate.pluginFormatName.equalsIgnoreCase ("VST3")
                    && candidate.name.equalsIgnoreCase (
                        recipeNode.pluginName)
                    && factoryPresetManufacturerMatches (
                        recipeNode, candidate)
                    && ! candidate.isInstrument
                    && ! isTBBoardDescription (candidate);
            });

        if (match == knownTypes.end())
        {
            load->completion (
                false,
                "Missing required VST3: " + recipeNode.pluginName
                    + ". Scan the default TB_Plugins folder and try again.");
            return;
        }

        load->descriptions.push_back (*match);
    }

    for (auto* node : graph.getNodes())
        if (! isIONode (node->nodeID))
            load->existingNodeIDs.push_back (node->nodeID);

    load->operationToken = beginHistoryOperation();
    loadNextFactoryPresetProcessor (load);
}

void BoardAudioEngine::loadNextFactoryPresetProcessor (
    const std::shared_ptr<FactoryPresetLoad>& load)
{
    if (load == nullptr || load->preset == nullptr)
        return;

    if (load->nextProcessor >= load->preset->nodes.size())
    {
        commitFactoryPreset (load);
        return;
    }

    const auto index = load->nextProcessor;
    const auto gate = callbackGate;
    createProcessorAsync (
        load->descriptions[index],
        InstanceMode::automatic,
        [gate, load, index]
        (std::unique_ptr<juce::AudioProcessor> processor,
         InstanceMode mode,
         const juce::String& creationError)
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr)
                return;

            auto& owner = *gate->owner;
            if (load->generation
                    != owner.restoreGeneration.load (
                        std::memory_order_relaxed)
                || ! owner.isCurrentHistoryOperation (
                    load->operationToken)
                || owner.externalStateRestorePending.load (
                    std::memory_order_acquire))
            {
                owner.failFactoryPreset (
                    load,
                    "Preset loading was cancelled by a board state restore.");
                return;
            }

            if (processor == nullptr)
            {
                owner.failFactoryPreset (
                    load,
                    load->preset->nodes[index].pluginName
                        + " could not be loaded"
                        + (creationError.isNotEmpty()
                               ? ": " + creationError
                               : juce::String (".")));
                return;
            }

            juce::String configurationError;
            if (! configureFactoryPresetProcessor (
                    *processor,
                    load->preset->nodes[index],
                    configurationError))
            {
                owner.failFactoryPreset (
                    load, configurationError);
                return;
            }

            load->processors.push_back (std::move (processor));
            load->modes.push_back (mode);
            ++load->nextProcessor;
            owner.loadNextFactoryPresetProcessor (load);
        });
}

void BoardAudioEngine::commitFactoryPreset (
    const std::shared_ptr<FactoryPresetLoad>& load)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (load == nullptr
        || load->preset == nullptr
        || load->processors.size() != load->preset->nodes.size()
        || load->modes.size() != load->preset->nodes.size())
    {
        failFactoryPreset (load, "The board preset load was incomplete.");
        return;
    }

    std::vector<BoardEdit> oldNodeEdits;
    std::set<juce::uint32> earlierNodeUIDs;
    for (const auto nodeID : load->existingNodeIDs)
    {
        SavedNode savedNode;
        juce::String captureError;
        if (! captureSavedNode (nodeID, savedNode, captureError))
        {
            failFactoryPreset (
                load,
                captureError.isNotEmpty()
                    ? captureError
                    : "The current board could not be saved for Undo.");
            return;
        }

        savedNode.incidentConnections.erase (
            std::remove_if (
                savedNode.incidentConnections.begin(),
                savedNode.incidentConnections.end(),
                [&earlierNodeUIDs] (const auto& connection)
                {
                    return earlierNodeUIDs.count (
                               connection.source.nodeID.uid)
                               > 0
                        || earlierNodeUIDs.count (
                               connection.destination.nodeID.uid)
                               > 0;
                }),
            savedNode.incidentConnections.end());
        savedNode.modulationLinks.clear();

        BoardEdit edit;
        edit.kind = BoardEdit::Kind::deleteNode;
        edit.description =
            "Delete " + savedNode.description.name;
        edit.savedNode = std::move (savedNode);
        oldNodeEdits.push_back (std::move (edit));
        earlierNodeUIDs.insert (nodeID.uid);
    }

    std::vector<juce::AudioProcessorGraph::Connection>
        oldIONodeConnections;
    for (const auto& connection : graph.getConnections())
        if (! connection.source.isMIDI()
            && ! connection.destination.isMIDI()
            && ! isInternalConnection (connection)
            && earlierNodeUIDs.count (
                   connection.source.nodeID.uid)
                   == 0
            && earlierNodeUIDs.count (
                   connection.destination.nodeID.uid)
                   == 0)
            oldIONodeConnections.push_back (connection);

    const auto oldOutputPosition =
        getNodePosition (outputNodeID, { 1008, 130 });

    auto transaction = std::make_shared<BoardEdit>();
    transaction->kind = BoardEdit::Kind::compound;
    transaction->description =
        "Apply " + load->preset->category + " / "
        + load->preset->name;

    std::vector<NodeID> addedNodeIDs;
    beginBoardUpdateBatch();

    for (size_t index = 0;
         index < load->preset->nodes.size();
         ++index)
    {
        auto node = graph.addNode (
            wrapNodeProcessor (std::move (load->processors[index])),
            std::nullopt,
            juce::AudioProcessorGraph::UpdateKind::none);
        if (node == nullptr)
        {
            rollbackFactoryPresetCommit (
                load,
                transaction,
                load->preset->nodes[index].pluginName
                    + " could not be added to the board.");
            return;
        }

        node->properties.set (
            nodeXKey, load->preset->nodes[index].position.x);
        node->properties.set (
            nodeYKey, load->preset->nodes[index].position.y);
        attachProcessorListeners (*node->getProcessor());
        addedNodeIDs.push_back (node->nodeID);

        SavedNode savedNode;
        juce::String captureError;
        if (! captureSavedNode (
                node->nodeID, savedNode, captureError))
        {
            const auto removed = removeNodeRaw (node->nodeID);
            rollbackFactoryPresetCommit (
                load,
                transaction,
                captureError.isNotEmpty()
                    ? captureError
                    : "A configured preset node could not be saved."
                        + juce::String (
                            removed
                                ? ""
                                : " The incomplete node could not be removed."));
            return;
        }

        BoardEdit edit;
        edit.kind = BoardEdit::Kind::addNode;
        edit.description =
            "Add " + savedNode.description.name;
        edit.savedNode = std::move (savedNode);
        transaction->compoundEdits.push_back (
            std::move (edit));
    }

    size_t transactionStateBytes = 0;
    for (const auto& edit : oldNodeEdits)
        transactionStateBytes +=
            edit.savedNode.childState.getSize()
            + edit.savedNode.parameterValues.size()
                  * sizeof (float);
    for (const auto& edit : transaction->compoundEdits)
        transactionStateBytes +=
            edit.savedNode.childState.getSize()
            + edit.savedNode.parameterValues.size()
                  * sizeof (float);

    if (transactionStateBytes > maximumHistoryStateBytes)
    {
        rollbackFactoryPresetCommit (
            load,
            transaction,
            "The current board and preset require too much plug-in state "
            "to store as one safe Undo step.");
        return;
    }

    const auto resolveRouteEndpoint =
        [this, &addedNodeIDs] (int endpoint) -> NodeID
        {
            if (endpoint == BoardPresetLibrary::inputRouteEndpoint)
                return inputNodeID;
            if (endpoint == BoardPresetLibrary::outputRouteEndpoint)
                return outputNodeID;
            if (endpoint >= 0
                && static_cast<size_t> (endpoint)
                       < addedNodeIDs.size())
                return addedNodeIDs[
                    static_cast<size_t> (endpoint)];
            return {};
        };

    std::vector<juce::AudioProcessorGraph::Connection>
        presetConnections;
    for (const auto& route : load->preset->routes)
    {
        const auto source =
            resolveRouteEndpoint (route.source);
        const auto destination =
            resolveRouteEndpoint (route.destination);
        const auto stereoConnections =
            makeStereoConnections (source, destination);
        if (source.uid == 0
            || destination.uid == 0
            || stereoConnections.size() != 2)
        {
            rollbackFactoryPresetCommit (
                load,
                transaction,
                "The preset contains an invalid stereo route.");
            return;
        }

        presetConnections.insert (
            presetConnections.end(),
            stereoConnections.begin(),
            stereoConnections.end());
    }

    const auto presetUsesBoardInput = std::any_of (
        presetConnections.begin(), presetConnections.end(),
        [this] (const auto& connection)
        {
            return connection.source.nodeID == inputNodeID;
        });
    if (presetUsesBoardInput && ! hasInputCard (stereoInputSource))
    {
        const auto card = makeInputCardInfo (
            stereoInputSource,
            { 35, 130 + static_cast<int> (inputCards.size()) * 188 });
        if (! card.has_value() || ! addInputCardRaw (*card, false))
        {
            rollbackFactoryPresetCommit (
                load,
                transaction,
                "The preset input could not be shown on the board.");
            return;
        }

        BoardEdit cardEdit;
        cardEdit.kind = BoardEdit::Kind::addInputCard;
        cardEdit.description = "Add " + card->name + " input";
        cardEdit.inputCard = *card;
        transaction->compoundEdits.push_back (std::move (cardEdit));
    }

    juce::String routeError;
    if (! addConnectionsRaw (
            presetConnections, routeError, false))
    {
        rollbackFactoryPresetCommit (
            load,
            transaction,
            routeError.isNotEmpty()
                ? routeError
                : "The preset route could not be created.");
        return;
    }

    BoardEdit connectionEdit;
    connectionEdit.kind = BoardEdit::Kind::addConnections;
    connectionEdit.description = "Connect preset nodes";
    connectionEdit.connections = presetConnections;
    transaction->compoundEdits.push_back (
        std::move (connectionEdit));

    while (! modulationSources.empty())
    {
        const auto source = modulationSources.front();
        BoardEdit edit;
        edit.kind = BoardEdit::Kind::removeModulationSource;
        edit.description =
            "Delete " + getModulationTypeName (source->type);
        edit.modulationSource =
            captureModulationSource (source);

        for (const auto& link : modulationLinks)
            if (link->source == source)
                edit.modulationLinks.push_back (
                    captureModulationLink (link));
        for (const auto& link : modulationControlLinks)
            if (link->source == source
                || link->targetSource == source)
                edit.modulationControlLinks.push_back (
                    captureModulationControlLink (link));

        if (! removeModulationSourceRaw (source->id))
        {
            jassertfalse;
            rollbackFactoryPresetCommit (
                load,
                transaction,
                "The previous modulation graph could not be replaced.");
            return;
        }

        transaction->compoundEdits.push_back (
            std::move (edit));
    }

    if (! oldIONodeConnections.empty())
    {
        if (! removeConnectionsRaw (
                oldIONodeConnections, true, false))
        {
            jassertfalse;
            rollbackFactoryPresetCommit (
                load,
                transaction,
                "The previous input/output route could not be replaced.");
            return;
        }

        BoardEdit edit;
        edit.kind = BoardEdit::Kind::removeConnections;
        edit.description = "Replace input/output route";
        edit.connections = std::move (oldIONodeConnections);
        transaction->compoundEdits.push_back (
            std::move (edit));
    }

    for (auto& edit : oldNodeEdits)
    {
        if (! removeNodeRaw (edit.savedNode.nodeID))
        {
            jassertfalse;
            rollbackFactoryPresetCommit (
                load,
                transaction,
                "A previous plug-in node could not be replaced.");
            return;
        }

        transaction->compoundEdits.push_back (
            std::move (edit));
    }

    if (oldOutputPosition != load->preset->outputPosition)
    {
        setNodePositionRaw (
            outputNodeID, load->preset->outputPosition);
        BoardEdit moveEdit;
        moveEdit.kind = BoardEdit::Kind::moveNode;
        moveEdit.description = "Move Audio Output";
        moveEdit.nodeID = outputNodeID;
        moveEdit.oldPosition = oldOutputPosition;
        moveEdit.newPosition =
            load->preset->outputPosition;
        transaction->compoundEdits.push_back (
            std::move (moveEdit));
    }

    scheduleBoardUpdate (true);
    recordPerformedEdit (std::move (*transaction));
    endBoardUpdateBatch();
    if (! finishHistoryOperation (load->operationToken))
    {
        if (load->completion != nullptr)
            load->completion (
                false,
                "The preset was applied but its Undo transaction was interrupted.");
        return;
    }

    if (load->completion != nullptr)
        load->completion (
            true,
            load->preset->category + " / "
                + load->preset->name + " is ready.");
}

void BoardAudioEngine::rollbackFactoryPresetCommit (
    const std::shared_ptr<FactoryPresetLoad>& load,
    const std::shared_ptr<BoardEdit>& transaction,
    const juce::String& error)
{
    if (transaction == nullptr
        || transaction->compoundEdits.empty())
    {
        endBoardUpdateBatch();
        failFactoryPreset (load, error);
        return;
    }

    rollbackCompoundHistoryEdit (
        *transaction,
        true,
        transaction->compoundEdits.size(),
        [this, load, transaction, error] (
            bool rollbackSucceeded)
        {
            endBoardUpdateBatch();
            failFactoryPreset (
                load,
                rollbackSucceeded
                    ? error
                    : error
                        + " The previous board could not be fully restored.");
        });
}

void BoardAudioEngine::failFactoryPreset (
    const std::shared_ptr<FactoryPresetLoad>& load,
    const juce::String& error)
{
    if (load == nullptr)
        return;

    finishHistoryOperation (load->operationToken);
    if (load->completion != nullptr)
        load->completion (
            false,
            error.isNotEmpty()
                ? error
                : "The board preset could not be applied.");
}

BoardAudioEngine::NodeID BoardAudioEngine::getInputNodeID() const noexcept
{
    return inputNodeID;
}

BoardAudioEngine::NodeID BoardAudioEngine::getOutputNodeID() const noexcept
{
    return outputNodeID;
}

bool BoardAudioEngine::isIONode (NodeID nodeID) const noexcept
{
    return nodeID == physicalInputNodeID
        || nodeID == inputNodeID
        || nodeID == outputNodeID;
}

bool BoardAudioEngine::isInternalNode (NodeID nodeID) const noexcept
{
    return nodeID == physicalInputNodeID;
}

bool BoardAudioEngine::isDualMonoNode (NodeID nodeID) const noexcept
{
    if (const auto node = graph.getNodeForId (nodeID))
        return getDualMonoProcessor (node->getProcessor()) != nullptr;

    return false;
}

std::vector<BoardAudioEngine::InputSourceInfo>
BoardAudioEngine::getInputSources() const
{
    return {
        { stereoInputSource, "Stereo 1-2", "Physical / host input", true },
        { firstMonoInputSource, "Input 1", "(Mono) -> dual mono", true },
        { secondMonoInputSource, "Input 2", "(Mono) -> dual mono", true },
        { localMediaInputSource, "Local", "Preview only", false },
        { youtubeInputSource, "YouTube", "External / async", false }
    };
}

std::vector<BoardAudioEngine::InputCardInfo>
BoardAudioEngine::getInputCards() const
{
    return inputCards;
}

bool BoardAudioEngine::hasInputCard (int sourceIndex) const
{
    return std::any_of (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& card)
        {
            return card.sourceIndex == sourceIndex;
        });
}

bool BoardAudioEngine::addInputCard (int sourceIndex,
                                     juce::Point<int> position,
                                     juce::String& error)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
    {
        error = "The board is busy restoring another edit.";
        return false;
    }

    if (hasInputCard (sourceIndex))
    {
        error = "That input is already on the board.";
        return false;
    }

    const auto card = makeInputCardInfo (sourceIndex, position);
    if (! card.has_value())
    {
        error = "This input source is not routable yet.";
        return false;
    }

    if (! addInputCardRaw (*card, true))
    {
        error = "That input could not be added to the board.";
        return false;
    }

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::addInputCard;
    edit.description = "Add " + card->name + " input";
    edit.inputCard = *card;
    recordPerformedEdit (std::move (edit));
    return true;
}

bool BoardAudioEngine::removeInputCard (int sourceIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    if (card == inputCards.end())
        return false;

    std::vector<juce::AudioProcessorGraph::Connection> routes;
    for (const auto destination : getInputSourceDestinations (sourceIndex))
    {
        const auto destinationRoute =
            makeInputSourceConnections (sourceIndex, destination);
        routes.insert (
            routes.end(), destinationRoute.begin(), destinationRoute.end());
    }

    if (! removeConnectionsRaw (routes, true, false))
        return false;

    const auto savedCard = *card;
    if (! removeInputCardRaw (sourceIndex, false))
    {
        juce::String ignoredError;
        addConnectionsRaw (routes, ignoredError, false);
        return false;
    }

    scheduleBoardUpdate (! routes.empty());

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeInputCard;
    edit.description = "Delete " + savedCard.name + " input";
    edit.inputCard = savedCard;
    edit.connections = std::move (routes);
    recordPerformedEdit (std::move (edit));
    return true;
}

void BoardAudioEngine::setInputCardPosition (
    int sourceIndex,
    juce::Point<int> position)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    if (card == inputCards.end() || card->position == position)
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::moveInputCard;
    edit.description = "Move " + card->name + " input";
    edit.inputCard = *card;
    edit.oldPosition = card->position;
    edit.newPosition = position;

    setInputCardPositionRaw (sourceIndex, position, true);
    recordPerformedEdit (std::move (edit));
}

std::optional<int> BoardAudioEngine::getInputSourceForDestination (
    NodeID destination) const
{
    if (destination == inputNodeID || isInternalNode (destination)
        || graph.getNodeForId (destination) == nullptr)
        return std::nullopt;

    std::vector<juce::AudioProcessorGraph::Connection> existing;
    for (const auto& connection : graph.getConnections())
        if (connection.source.nodeID == inputNodeID
            && connection.destination.nodeID == destination
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            existing.push_back (connection);

    for (auto sourceIndex = stereoInputSource;
         sourceIndex <= secondMonoInputSource;
         ++sourceIndex)
    {
        auto expected = makeInputSourceConnections (sourceIndex, destination);
        if (expected.empty() || expected.size() != existing.size())
            continue;

        const auto matches = std::all_of (
            expected.begin(), expected.end(),
            [&existing] (const auto& connection)
            {
                return std::find (
                           existing.begin(), existing.end(), connection)
                       != existing.end();
            });
        if (matches)
            return sourceIndex;
    }

    return std::nullopt;
}

void BoardAudioEngine::addPlugin (const juce::String& identifier,
                                  juce::Point<int> position,
                                  AddPluginCallback callback)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (boardEditsBlocked())
    {
        callback ({}, "The board is busy restoring another edit.");
        return;
    }

    if (isCatalogBusy())
    {
        callback ({}, "Close the catalog manager or wait for scanning to finish.");
        return;
    }

    const auto descriptionFromCatalog = knownPlugins.getTypeForIdentifierString (identifier);

    if (descriptionFromCatalog == nullptr)
    {
        callback ({}, "The selected VST3 is no longer available in the catalog.");
        return;
    }

    const auto description = *descriptionFromCatalog;

    if (isTBBoardDescription (description))
    {
        callback ({}, "TB_Board cannot host itself recursively.");
        return;
    }

    const auto gate = callbackGate;
    const auto generation = restoreGeneration.load (std::memory_order_relaxed);
    const auto operationToken = beginHistoryOperation();

    createProcessorAsync (
        description,
        InstanceMode::automatic,
        [gate, position, generation, operationToken,
         callback = std::move (callback)]
        (std::unique_ptr<juce::AudioProcessor> processor,
         InstanceMode mode,
         const juce::String& error)
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr)
                return;

            if (generation
                    != gate->owner->restoreGeneration.load (
                        std::memory_order_relaxed)
                || ! gate->owner->isCurrentHistoryOperation (
                    operationToken)
                || gate->owner->externalStateRestorePending.load (
                    std::memory_order_acquire))
            {
                gate->owner->finishHistoryOperation (operationToken);
                callback ({}, "VST3 loading was cancelled by a board state restore.");
                return;
            }

            if (processor == nullptr)
            {
                gate->owner->finishHistoryOperation (operationToken);
                callback ({}, error);
                return;
            }

            gate->owner->addProcessorNode (
                std::move (processor),
                position,
                callback,
                mode == InstanceMode::linkedDualMono
                    ? "Mono VST3 loaded as linked dual mono (2 instances)."
                    : juce::String(),
                generation,
                operationToken);
        });
}

void BoardAudioEngine::createProcessorAsync (
    const juce::PluginDescription& description,
    InstanceMode requestedMode,
    CreateProcessorCallback callback)
{
    if (isTBBoardDescription (description))
    {
        callback (nullptr, requestedMode, "TB_Board cannot host itself recursively.");
        return;
    }

    const auto manager = formatManager;
    const auto sampleRate = currentSampleRate;
    const auto initialBlockSize = currentBlockSize;

    manager->createPluginInstanceAsync (
        description,
        sampleRate,
        initialBlockSize,
        [manager, description, requestedMode, sampleRate, initialBlockSize,
         callback = std::move (callback)]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            if (instance == nullptr)
            {
                callback (nullptr,
                          requestedMode,
                          error.isNotEmpty()
                              ? error
                              : "The VST3 instance could not be created.");
                return;
            }

            const auto wantsNativeStereo =
                requestedMode == InstanceMode::automatic
                || requestedMode == InstanceMode::nativeStereo;

            if (wantsNativeStereo
                && (setMainLayout (*instance, juce::AudioChannelSet::stereo())
                    || hasNativeStereoMainBus (*instance)))
            {
                callback (std::move (instance), InstanceMode::nativeStereo, {});
                return;
            }

            if (requestedMode == InstanceMode::nativeStereo)
            {
                callback (nullptr,
                          requestedMode,
                          "The saved VST3 no longer supports its stereo layout.");
                return;
            }

            if (! setMainLayout (*instance, juce::AudioChannelSet::mono()))
            {
                callback (nullptr,
                          requestedMode,
                          "Unsupported VST3 layout: " + describeMainLayout (*instance) + ".");
                return;
            }

            auto leftInstance =
                std::make_shared<std::unique_ptr<juce::AudioPluginInstance>> (
                    std::move (instance));

            manager->createPluginInstanceAsync (
                description,
                sampleRate,
                initialBlockSize,
                [leftInstance, requestedMode, callback]
                (std::unique_ptr<juce::AudioPluginInstance> rightInstance,
                 const juce::String& rightError)
                {
                    if (rightInstance == nullptr)
                    {
                        callback (
                            nullptr,
                            requestedMode,
                            rightError.isNotEmpty()
                                ? "Dual-mono right instance failed: " + rightError
                                : "Dual-mono right instance could not be created.");
                        return;
                    }

                    if (! setMainLayout (*rightInstance, juce::AudioChannelSet::mono()))
                    {
                        callback (
                            nullptr,
                            requestedMode,
                            "The right instance does not support a fixed mono layout.");
                        return;
                    }

                    callback (
                        std::make_unique<DualMonoProcessor> (
                            std::move (*leftInstance), std::move (rightInstance)),
                        InstanceMode::linkedDualMono,
                        {});
                });
        });
}

void BoardAudioEngine::addProcessorNode (
    std::unique_ptr<juce::AudioProcessor> processor,
    juce::Point<int> position,
    AddPluginCallback callback,
    juce::String resultMessage,
    juce::uint64 generation,
    juce::uint64 operationToken)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (generation != restoreGeneration.load (std::memory_order_relaxed)
        || ! isCurrentHistoryOperation (operationToken)
        || externalStateRestorePending.load (std::memory_order_acquire))
    {
        finishHistoryOperation (operationToken);
        callback ({}, "VST3 loading was cancelled by a board state restore.");
        return;
    }

    auto node = graph.addNode (
        wrapNodeProcessor (std::move (processor)),
        std::nullopt,
        juce::AudioProcessorGraph::UpdateKind::none);

    if (node == nullptr)
    {
        finishHistoryOperation (operationToken);
        callback ({}, "The VST3 instance could not be added to the graph.");
        return;
    }

    node->properties.set (nodeXKey, position.x);
    node->properties.set (nodeYKey, position.y);
    attachProcessorListeners (*node->getProcessor());

    SavedNode savedNode;
    juce::String snapshotError;
    if (! captureSavedNode (node->nodeID, savedNode, snapshotError))
    {
        detachProcessorListeners (*node->getProcessor());
        graph.removeNode (
            node->nodeID,
            juce::AudioProcessorGraph::UpdateKind::none);
        finishHistoryOperation (operationToken);
        callback ({}, snapshotError);
        return;
    }

    if (generation != restoreGeneration.load (std::memory_order_relaxed)
        || ! isCurrentHistoryOperation (operationToken)
        || externalStateRestorePending.load (std::memory_order_acquire))
    {
        detachProcessorListeners (*node->getProcessor());
        graph.removeNode (
            node->nodeID,
            juce::AudioProcessorGraph::UpdateKind::none);
        finishHistoryOperation (operationToken);
        callback ({}, "VST3 loading was cancelled by a board state restore.");
        return;
    }

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::addNode;
    edit.description = "Add " + node->getProcessor()->getName();
    edit.savedNode = std::move (savedNode);

    scheduleBoardUpdate (true);
    recordPerformedEdit (std::move (edit));
    if (! finishHistoryOperation (operationToken))
    {
        callback ({}, "VST3 loading was superseded by a newer board operation.");
        return;
    }

    callback (node->nodeID, resultMessage);
}

void BoardAudioEngine::removeNode (NodeID nodeID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (isCatalogBusy()
        || boardEditsBlocked()
        || isIONode (nodeID))
        return;

    SavedNode savedNode;
    juce::String error;
    if (! captureSavedNode (nodeID, savedNode, error))
    {
        if (error.isNotEmpty())
            juce::Logger::writeToLog ("TB_Board delete rejected: " + error);
        return;
    }

    const auto nodeName = savedNode.description.name.isNotEmpty()
                              ? savedNode.description.name
                              : juce::String ("node");
    if (! removeNodeRaw (nodeID))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::deleteNode;
    edit.description = "Delete " + nodeName;
    edit.savedNode = std::move (savedNode);
    recordPerformedEdit (std::move (edit));
}

bool BoardAudioEngine::connectStereo (NodeID source,
                                       NodeID destination,
                                       juce::String& error)
{
    if (isInternalNode (source) || isInternalNode (destination))
    {
        error = "The physical input bridge is not a board endpoint.";
        return false;
    }
    if (destination == inputNodeID || source == outputNodeID)
    {
        error = "That board I/O direction is not connectable.";
        return false;
    }

    if (source == inputNodeID)
        return connectInputSource (
            stereoInputSource, destination, error);

    return connectAudioRoute (
        source,
        destination,
        makeStereoConnections (source, destination),
        error);
}

bool BoardAudioEngine::connectInputSource (int sourceIndex,
                                           NodeID destination,
                                           juce::String& error)
{
    if (sourceIndex < stereoInputSource
        || sourceIndex > secondMonoInputSource)
    {
        error = "This input source is not routable yet.";
        return false;
    }

    std::optional<InputCardInfo> addedCard;
    if (! hasInputCard (sourceIndex))
    {
        const auto card = makeInputCardInfo (
            sourceIndex,
            { 35, 130 + static_cast<int> (inputCards.size()) * 188 });
        if (! card.has_value() || ! addInputCardRaw (*card, false))
        {
            error = "This input could not be shown on the board.";
            return false;
        }
        addedCard = *card;
    }

    const auto route = makeInputSourceConnections (sourceIndex, destination);
    std::vector<juce::AudioProcessorGraph::Connection> addedConnections;
    const auto connected = connectAudioRoute (
        inputNodeID,
        destination,
        route,
        error,
        ! addedCard.has_value(),
        addedCard.has_value() ? &addedConnections : nullptr);
    if (! connected)
    {
        if (addedCard.has_value())
            removeInputCardRaw (sourceIndex, false);
        return false;
    }

    if (! addedCard.has_value())
        return true;

    if (addedConnections.empty())
        scheduleBoardUpdate (false);

    BoardEdit cardEdit;
    cardEdit.kind = BoardEdit::Kind::addInputCard;
    cardEdit.description = "Add " + addedCard->name + " input";
    cardEdit.inputCard = *addedCard;

    if (addedConnections.empty())
    {
        recordPerformedEdit (std::move (cardEdit));
        return true;
    }

    BoardEdit connectionEdit;
    connectionEdit.kind = BoardEdit::Kind::addConnections;
    connectionEdit.description = "Connect input source";
    connectionEdit.connections = std::move (addedConnections);

    BoardEdit compoundEdit;
    compoundEdit.kind = BoardEdit::Kind::compound;
    compoundEdit.description = "Connect " + addedCard->name + " input";
    compoundEdit.compoundEdits.push_back (std::move (cardEdit));
    compoundEdit.compoundEdits.push_back (std::move (connectionEdit));
    recordPerformedEdit (std::move (compoundEdit));
    return true;
}

bool BoardAudioEngine::connectAudioRoute (
    NodeID source,
    NodeID destination,
    const std::vector<juce::AudioProcessorGraph::Connection>& connections,
    juce::String& error,
    bool recordHistory,
    std::vector<juce::AudioProcessorGraph::Connection>* addedConnectionsOut)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (addedConnectionsOut != nullptr)
        addedConnectionsOut->clear();

    if (boardEditsBlocked())
    {
        error = "The board is busy restoring another edit.";
        return false;
    }

    if (isInternalNode (source) || isInternalNode (destination))
    {
        error = "The physical input bridge is not a board endpoint.";
        return false;
    }
    if (destination == inputNodeID || source == outputNodeID)
    {
        error = "That board I/O direction is not connectable.";
        return false;
    }

    const auto sourceNode = graph.getNodeForId (source);
    const auto destinationNode = graph.getNodeForId (destination);

    if (sourceNode == nullptr || destinationNode == nullptr)
    {
        error = "A connection endpoint no longer exists.";
        return false;
    }

    if (source == destination)
    {
        error = "A node cannot connect to itself.";
        return false;
    }

    if (graph.isAnInputTo (destination, source))
    {
        error = "Feedback cycles are disabled in the manual MVP.";
        return false;
    }

    if (connections.empty())
    {
        error = "This connection requires stereo audio input and output.";
        return false;
    }

    if (source == inputNodeID)
    {
        std::vector<juce::AudioProcessorGraph::Connection> existingConnections;
        for (const auto& connection : graph.getConnections())
            if (connection.source.nodeID == inputNodeID
                && connection.destination.nodeID == destination
                && ! connection.source.isMIDI()
                && ! connection.destination.isMIDI())
                existingConnections.push_back (connection);

        const auto isRequestedConnection = [&connections] (const auto& existing)
        {
            return std::find (
                       connections.begin(), connections.end(), existing)
                   != connections.end();
        };
        if (std::any_of (
                existingConnections.begin(),
                existingConnections.end(),
                [&isRequestedConnection] (const auto& existing)
                {
                    return ! isRequestedConnection (existing);
                }))
        {
            error = "This destination already uses another Input source. "
                    "Route each source to its own branch before mixing.";
            return false;
        }
    }

    for (const auto& connection : connections)
    {
        if (! graph.isConnected (connection) && ! graph.canConnect (connection))
        {
            error = "The selected audio pins are incompatible.";
            return false;
        }
    }

    std::vector<juce::AudioProcessorGraph::Connection> addedConnections;
    for (const auto& connection : connections)
        if (! graph.isConnected (connection))
            addedConnections.push_back (connection);

    if (addedConnections.empty())
        return true;

    if (! addConnectionsRaw (addedConnections, error, true))
        return false;

    if (addedConnectionsOut != nullptr)
        *addedConnectionsOut = addedConnections;

    if (recordHistory)
    {
        BoardEdit edit;
        edit.kind = BoardEdit::Kind::addConnections;
        edit.description = "Connect nodes";
        edit.connections = std::move (addedConnections);
        recordPerformedEdit (std::move (edit));
    }
    return true;
}

void BoardAudioEngine::disconnectInputSource (int sourceIndex,
                                              NodeID destination)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    const auto route = makeInputSourceConnections (sourceIndex, destination);
    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : route)
        if (graph.isConnected (connection))
            removedConnections.push_back (connection);

    if (removedConnections.size() != route.size()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect input source";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
}

bool BoardAudioEngine::disconnectInputSourceRoutes (int sourceIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto destination : getInputSourceDestinations (sourceIndex))
    {
        const auto route = makeInputSourceConnections (
            sourceIndex, destination);
        removedConnections.insert (
            removedConnections.end(), route.begin(), route.end());
    }

    if (removedConnections.empty()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return false;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect input source routes";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
    return true;
}

void BoardAudioEngine::disconnectStereo (NodeID source, NodeID destination)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    if (source == inputNodeID)
    {
        if (const auto inputSource =
                getInputSourceForDestination (destination))
        {
            disconnectInputSource (*inputSource, destination);
            return;
        }
    }

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : graph.getConnections())
        if (connection.source.nodeID == source
            && connection.destination.nodeID == destination
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            removedConnections.push_back (connection);

    if (removedConnections.empty()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect nodes";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
}

bool BoardAudioEngine::disconnectNodeRoutes (NodeID nodeID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || isInternalNode (nodeID))
        return false;

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : graph.getConnections())
        if ((connection.source.nodeID == nodeID
             || connection.destination.nodeID == nodeID)
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            removedConnections.push_back (connection);

    if (removedConnections.empty()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return false;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect node routes";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
    return true;
}

void BoardAudioEngine::disconnectInputs (NodeID nodeID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || isInternalNode (nodeID))
        return;

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : graph.getConnections())
        if (connection.destination.nodeID == nodeID
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            removedConnections.push_back (connection);

    if (removedConnections.empty()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect input routes";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
}

void BoardAudioEngine::disconnectOutputs (NodeID nodeID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || isInternalNode (nodeID))
        return;

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : graph.getConnections())
        if (connection.source.nodeID == nodeID
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            removedConnections.push_back (connection);

    if (removedConnections.empty()
        || ! removeConnectionsRaw (removedConnections, true, true))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeConnections;
    edit.description = "Disconnect output routes";
    edit.connections = std::move (removedConnections);
    recordPerformedEdit (std::move (edit));
}

int BoardAudioEngine::getFanInCount (NodeID nodeID) const
{
    std::set<juce::uint32> sourceNodes;

    for (const auto& connection : graph.getConnections())
        if (connection.destination.nodeID == nodeID)
            sourceNodes.insert (connection.source.nodeID.uid);

    return static_cast<int> (sourceNodes.size());
}

int BoardAudioEngine::getFanOutCount (NodeID nodeID) const
{
    std::set<juce::uint32> destinationNodes;

    for (const auto& connection : graph.getConnections())
        if (connection.source.nodeID == nodeID)
            destinationNodes.insert (connection.destination.nodeID.uid);

    return static_cast<int> (destinationNodes.size());
}

std::vector<BoardAudioEngine::NodeID> BoardAudioEngine::getInputSourceNodes (
    NodeID nodeID) const
{
    std::set<NodeID> sourceNodes;

    for (const auto& connection : graph.getConnections())
        if (connection.destination.nodeID == nodeID
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            sourceNodes.insert (connection.source.nodeID);

    return { sourceNodes.begin(), sourceNodes.end() };
}

std::vector<BoardAudioEngine::NodeID> BoardAudioEngine::getOutputDestinationNodes (
    NodeID nodeID) const
{
    std::set<NodeID> destinationNodes;

    for (const auto& connection : graph.getConnections())
        if (connection.source.nodeID == nodeID
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            destinationNodes.insert (connection.destination.nodeID);

    return { destinationNodes.begin(), destinationNodes.end() };
}

std::vector<BoardAudioEngine::NodeID>
BoardAudioEngine::getInputSourceDestinations (int sourceIndex) const
{
    std::set<NodeID> destinations;

    for (const auto& connection : graph.getConnections())
    {
        if (connection.source.nodeID != inputNodeID
            || connection.source.isMIDI()
            || connection.destination.isMIDI())
            continue;

        const auto destination = connection.destination.nodeID;
        const auto route = getInputSourceForDestination (destination);
        if (route.has_value() && *route == sourceIndex)
            destinations.insert (destination);
    }

    return { destinations.begin(), destinations.end() };
}

void BoardAudioEngine::setNodePosition (NodeID nodeID,
                                        juce::Point<int> position)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr)
        return;

    const auto fallbackPosition =
        nodeID == inputNodeID
            ? juce::Point<int> { 60, 130 }
            : nodeID == outputNodeID
                ? juce::Point<int> { 1008, 130 }
                : position;
    const auto oldPosition =
        getNodePosition (nodeID, fallbackPosition);
    if (oldPosition == position)
        return;

    setNodePositionRaw (nodeID, position);

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::moveNode;
    edit.description = "Move " + node->getProcessor()->getName();
    edit.nodeID = nodeID;
    edit.oldPosition = oldPosition;
    edit.newPosition = position;
    recordPerformedEdit (std::move (edit));
}

juce::Point<int> BoardAudioEngine::getNodePosition (
    NodeID nodeID,
    juce::Point<int> fallback) const
{
    if (auto node = graph.getNodeForId (nodeID))
    {
        if (node->properties.contains (nodeXKey)
            && node->properties.contains (nodeYKey))
            return { static_cast<int> (node->properties[nodeXKey]),
                     static_cast<int> (node->properties[nodeYKey]) };
    }

    return fallback;
}

void BoardAudioEngine::setNodeBypassed (NodeID nodeID, bool shouldBypass)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    if (auto node = graph.getNodeForId (nodeID))
    {
        node->setBypassed (shouldBypass);
        scheduleBoardUpdate (false);
    }
}

float BoardAudioEngine::getNodeLevelDb (
    NodeID nodeID,
    NodeLevelStage stage) const
{
    if (nodeID == outputNodeID)
        return stage == NodeLevelStage::input ? outputInputGainDb : 0.0f;

    if (isIONode (nodeID))
        return 0.0f;

    if (const auto node = graph.getNodeForId (nodeID))
        if (const auto* gain = getNodeGainProcessor (node->getProcessor()))
            return gain->getLevelDb (stage);

    return 0.0f;
}

bool BoardAudioEngine::beginNodeLevelChange (
    NodeID nodeID,
    NodeLevelStage stage)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || activeNodeLevelEdit.has_value())
        return false;

    const auto validOutputLevel =
        nodeID == outputNodeID && stage == NodeLevelStage::input;
    const auto node = graph.getNodeForId (nodeID);
    const auto validPluginLevel =
        node != nullptr
        && ! isIONode (nodeID)
        && getNodeGainProcessor (node->getProcessor()) != nullptr;
    if (! validOutputLevel && ! validPluginLevel)
        return false;

    activeNodeLevelEdit = NodeLevelEdit {
        nodeID, stage, getNodeLevelDb (nodeID, stage)
    };
    return true;
}

bool BoardAudioEngine::setNodeLevelDb (
    NodeID nodeID,
    NodeLevelStage stage,
    float decibels)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    return setNodeLevelDbRaw (nodeID, stage, decibels, true);
}

void BoardAudioEngine::endNodeLevelChange (
    NodeID nodeID,
    NodeLevelStage stage)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! activeNodeLevelEdit.has_value()
        || activeNodeLevelEdit->nodeID != nodeID
        || activeNodeLevelEdit->stage != stage)
        return;

    const auto original = activeNodeLevelEdit->originalDb;
    activeNodeLevelEdit.reset();
    const auto current = getNodeLevelDb (nodeID, stage);
    if (juce::approximatelyEqual (original, current))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::changeNodeLevel;
    edit.description = nodeID == outputNodeID
                           ? "Change Output input level"
                           : juce::String ("Change plug-in ")
                                 + (stage == NodeLevelStage::input
                                        ? "input"
                                        : "output")
                                 + " level";
    edit.nodeID = nodeID;
    edit.nodeLevelStage = stage;
    edit.oldLevelDb = original;
    edit.newLevelDb = current;
    recordPerformedEdit (std::move (edit));
}

float BoardAudioEngine::getInputCardOutputLevelDb (
    int sourceIndex) const
{
    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    return card != inputCards.end() ? card->outputGainDb : 0.0f;
}

bool BoardAudioEngine::beginInputCardOutputLevelChange (
    int sourceIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || activeInputCardLevelEdit.has_value()
        || ! hasInputCard (sourceIndex))
        return false;

    activeInputCardLevelEdit = InputCardLevelEdit {
        sourceIndex, getInputCardOutputLevelDb (sourceIndex)
    };
    return true;
}

bool BoardAudioEngine::setInputCardOutputLevelDb (
    int sourceIndex,
    float decibels)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    return setInputCardOutputLevelDbRaw (
        sourceIndex, decibels, true);
}

void BoardAudioEngine::endInputCardOutputLevelChange (
    int sourceIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! activeInputCardLevelEdit.has_value()
        || activeInputCardLevelEdit->sourceIndex != sourceIndex)
        return;

    const auto original = activeInputCardLevelEdit->originalDb;
    activeInputCardLevelEdit.reset();
    const auto current = getInputCardOutputLevelDb (sourceIndex);
    if (juce::approximatelyEqual (original, current))
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::changeInputCardOutputLevel;
    edit.description = "Change input output level";
    edit.inputCard.sourceIndex = sourceIndex;
    edit.oldLevelDb = original;
    edit.newLevelDb = current;
    recordPerformedEdit (std::move (edit));
}

bool BoardAudioEngine::setNodeProgram (NodeID nodeID, int programIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return false;

    auto* processor = node->getProcessor();
    {
        const juce::ScopedLock processorLock (
            processor->getCallbackLock());

        if (! juce::isPositiveAndBelow (
                programIndex, processor->getNumPrograms()))
            return false;

        if (processor->getCurrentProgram() == programIndex)
            return true;
    }

    const juce::ScopedLock engineCallbackLock (getCallbackLock());
    std::vector<std::shared_ptr<ModulationTarget>> affectedTargets;
    for (const auto& link : modulationLinks)
        if (link->target->node == node
            && std::find (
                   affectedTargets.begin(),
                   affectedTargets.end(),
                   link->target)
                   == affectedTargets.end())
        {
            link->target->userEditing.store (
                true, std::memory_order_release);
            affectedTargets.push_back (link->target);
        }

    for (const auto& target : affectedTargets)
        setModulationTargetValue (
            target, target->baseValue.load (std::memory_order_relaxed));

    {
        const juce::ScopedLock processorLock (
            processor->getCallbackLock());
        processor->setCurrentProgram (programIndex);
    }

    for (const auto& target : affectedTargets)
    {
        const auto value = target->parameter->getValue();
        target->baseValue.store (value, std::memory_order_relaxed);
        target->lastAppliedValue.store (value, std::memory_order_relaxed);
        target->holdBlocks.store (2, std::memory_order_relaxed);
        target->userEditing.store (false, std::memory_order_release);
    }

    scheduleBoardUpdate (false);
    return true;
}

std::vector<BoardAudioEngine::NodeParameterInfo>
BoardAudioEngine::getNodeParameters (NodeID nodeID) const
{
    std::vector<NodeParameterInfo> result;
    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return result;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return result;

    const auto& parameters = processor->getParameters();
    result.reserve (static_cast<size_t> (parameters.size()));

    for (int index = 0; index < parameters.size(); ++index)
    {
        auto* parameter = parameters.getUnchecked (index);
        const auto category = static_cast<int> (parameter->getCategory());
        const auto isMeter = (category & 0xffff0000) == (2 << 16);
        if (! parameter->isAutomatable() || isMeter)
            continue;

        NodeParameterInfo info;
        info.parameterIndex = index;
        if (auto* hosted =
                dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            info.parameterID = hosted->getParameterID();

        info.name = parameter->getName (96).trim();
        if (info.name.isEmpty())
            info.name = "Parameter " + juce::String (index + 1);

        info.normalisedValue =
            juce::jlimit (0.0f, 1.0f, parameter->getValue());
        info.displayText = parameter->getText (
            info.normalisedValue, 48).trim();
        if (info.displayText.isEmpty())
            info.displayText = juce::String (
                static_cast<double> (info.normalisedValue), 3);
        info.numSteps = parameter->getNumSteps();
        for (const auto& link : modulationLinks)
            if (link->target != nullptr
                && link->target->processor == processor
                && link->target->parameterIndex == index)
            {
                NodeParameterLinkInfo linkInfo;
                linkInfo.linkID = link->id;
                if (link->source != nullptr)
                {
                    linkInfo.sourceID = link->source->id;
                    linkInfo.sourceType = link->source->type;
                }
                linkInfo.amount =
                    link->amount.load (std::memory_order_relaxed);
                info.modulationLinks.push_back (linkInfo);
            }
        info.linked = ! info.modulationLinks.empty();
        result.push_back (std::move (info));
    }

    return result;
}

bool BoardAudioEngine::getNodeParameterValue (
    NodeID nodeID,
    int parameterIndex,
    float& normalisedValue,
    juce::String& displayText) const
{
    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return false;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return false;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (
            parameterIndex, parameters.size()))
        return false;

    auto* parameter = parameters.getUnchecked (parameterIndex);
    normalisedValue =
        juce::jlimit (0.0f, 1.0f, parameter->getValue());
    displayText =
        parameter->getText (normalisedValue, 48).trim();
    if (displayText.isEmpty())
        displayText = juce::String (
            static_cast<double> (normalisedValue), 3);
    return true;
}

bool BoardAudioEngine::beginNodeParameterGesture (
    NodeID nodeID,
    int parameterIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return false;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return false;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        return false;

    auto* parameter = parameters.getUnchecked (parameterIndex);
    const auto category = static_cast<int> (parameter->getCategory());
    if (! parameter->isAutomatable()
        || (category & 0xffff0000) == (2 << 16))
        return false;

    parameter->beginChangeGesture();
    return true;
}

bool BoardAudioEngine::setNodeParameterValue (
    NodeID nodeID,
    int parameterIndex,
    float normalisedValue)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return false;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return false;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        return false;

    parameters.getUnchecked (parameterIndex)->setValueNotifyingHost (
        juce::jlimit (0.0f, 1.0f, normalisedValue));
    return true;
}

void BoardAudioEngine::endNodeParameterGesture (
    NodeID nodeID,
    int parameterIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return;

    const auto& parameters = processor->getParameters();
    if (juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        parameters.getUnchecked (parameterIndex)->endChangeGesture();
}

BoardAudioEngine::ModulationID BoardAudioEngine::addModulationSource (
    ModulationType type)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return 0;
    if (pendingModulationChange.has_value())
        commitModulationChanges();

    SavedModulationSource savedSource;
    savedSource.id = nextModulationSourceID++;
    savedSource.type = type;
    savedSource.lfoWaveform = LfoWaveform::sine;
    savedSource.polarity = getDefaultModulationPolarity (type);
    savedSource.primary = getDefaultModulationPrimary (type);
    savedSource.secondary = getDefaultModulationSecondary (type);
    savedSource.envelopeDecay = 200.0f;
    savedSource.envelopeSustain = 0.70f;
    savedSource.jitter = 0.0f;
    savedSource.jitterRange = 0.25f;
    savedSource.autoTrigger = true;
    savedSource.index = modulationSources.size();
    savedSource.curvePoints = makeDefaultCurvePoints();

    if (! addModulationSourceRaw (savedSource, {}, {}))
        return 0;

    modulationStatus =
        getModulationTypeName (type) + " added. Press + to learn a target.";
    scheduleBoardUpdate (false);

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::addModulationSource;
    edit.description = "Add " + getModulationTypeName (type);
    edit.modulationSource = savedSource;
    recordPerformedEdit (std::move (edit));
    return savedSource.id;
}

void BoardAudioEngine::removeModulationSource (ModulationID sourceID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;
    if (pendingModulationChange.has_value())
        commitModulationChanges();

    const auto source = findModulationSource (sourceID);
    if (source == nullptr)
        return;

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeModulationSource;
    edit.description = "Remove " + getModulationTypeName (source->type);
    edit.modulationSource = captureModulationSource (source);
    for (const auto& link : modulationLinks)
        if (link->source->id == sourceID)
            edit.modulationLinks.push_back (captureModulationLink (link));
    for (const auto& link : modulationControlLinks)
        if (link->source->id == sourceID
            || link->targetSource->id == sourceID)
            edit.modulationControlLinks.push_back (
                captureModulationControlLink (link));

    if (! removeModulationSourceRaw (sourceID))
        return;

    modulationStatus = getModulationTypeName (source->type) + " removed";
    scheduleBoardUpdate (false);
    recordPerformedEdit (std::move (edit));
}

std::vector<BoardAudioEngine::ModulationSourceInfo>
BoardAudioEngine::getModulationSources() const
{
    std::vector<ModulationSourceInfo> result;
    result.reserve (modulationSources.size());
    const auto learning =
        learningModulationSource.load (std::memory_order_acquire);

    for (const auto& source : modulationSources)
    {
        ModulationSourceInfo info;
        info.id = source->id;
        info.type = source->type;
        info.lfoWaveform =
            source->lfoWaveform.load (std::memory_order_relaxed);
        info.polarity =
            source->polarity.load (std::memory_order_relaxed);
        info.primaryValue =
            source->primary.load (std::memory_order_relaxed);
        info.secondaryValue =
            source->secondary.load (std::memory_order_relaxed);
        info.envelopeDecayValue =
            source->envelopeDecay.load (std::memory_order_relaxed);
        info.envelopeSustainValue =
            source->envelopeSustain.load (std::memory_order_relaxed);
        info.jitterValue =
            source->jitter.load (std::memory_order_relaxed);
        info.jitterRangeValue =
            source->jitterRange.load (std::memory_order_relaxed);
        for (;;)
        {
            const auto versionBefore =
                source->displayStateVersion.load (
                    std::memory_order_seq_cst);
            if ((versionBefore & 1u) != 0u)
                continue;

            info.outputValue =
                source->output.load (std::memory_order_relaxed);
            info.displayPhase =
                source->displayPhase.load (std::memory_order_relaxed);
            info.effectiveSecondaryValue =
                source->effectiveSecondary.load (
                    std::memory_order_relaxed);
            info.lfoRandomStartValue =
                source->lfoRandomStart.load (
                    std::memory_order_relaxed);
            info.lfoRandomEndValue =
                source->lfoRandomEnd.load (
                    std::memory_order_relaxed);

            std::atomic_thread_fence (std::memory_order_seq_cst);
            if (versionBefore
                == source->displayStateVersion.load (
                    std::memory_order_seq_cst))
                break;
        }
        info.autoTrigger =
            source->autoTrigger.load (std::memory_order_relaxed);
        info.learning = learning == source->id;

        if (source->curveState != nullptr)
        {
            info.curvePoints = source->curveState->points;
            info.loopMode = source->curveState->loopMode;
            info.loopStart = source->curveState->loopStart;
            info.loopEnd = source->curveState->loopEnd;
        }

        for (const auto& link : modulationLinks)
        {
            if (link->source != source)
                continue;

            ModulationLinkInfo linkInfo;
            linkInfo.id = link->id;
            linkInfo.sourceID = source->id;
            linkInfo.nodeID = link->target->node->nodeID;
            linkInfo.parameterID = link->target->parameterID;
            linkInfo.parameterIndex = link->target->parameterIndex;
            linkInfo.targetName = link->target->targetName;
            linkInfo.amount =
                link->amount.load (std::memory_order_relaxed);
            info.links.push_back (std::move (linkInfo));
        }

        for (const auto& link : modulationControlLinks)
        {
            ModulationControlLinkInfo linkInfo;
            linkInfo.id = link->id;
            linkInfo.sourceID = link->source->id;
            linkInfo.targetSourceID = link->targetSource->id;
            linkInfo.target = link->target;
            linkInfo.targetName =
                getModulationTypeName (link->targetSource->type)
                + " " + juce::String (link->targetSource->id)
                + " / " + getModulationControlTargetName (
                    link->targetSource->type, link->target);
            linkInfo.amount =
                link->target == ModulationControlTarget::trigger
                    ? 1.0f
                    : link->amount.load (std::memory_order_relaxed);

            if (link->source == source)
                info.outgoingControlLinks.push_back (linkInfo);

            if (link->targetSource != source)
                continue;

            switch (link->target)
            {
                case ModulationControlTarget::primary:
                    info.primaryInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::secondary:
                    info.secondaryInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::envelopeDecay:
                    info.envelopeDecayInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::envelopeSustain:
                    info.envelopeSustainInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::jitter:
                    info.jitterInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::jitterRange:
                    info.jitterRangeInputLinks.push_back (linkInfo);
                    break;
                case ModulationControlTarget::trigger:
                    info.triggerInputLinks.push_back (linkInfo);
                    break;
            }
        }

        result.push_back (std::move (info));
    }

    return result;
}

void BoardAudioEngine::setModulationSourceValues (
    ModulationID sourceID,
    float primary,
    float secondary)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
    {
        source->primary.store (
            clampModulationPrimary (source->type, primary),
            std::memory_order_relaxed);
        source->secondary.store (
            clampModulationSecondary (source->type, secondary),
            std::memory_order_relaxed);
        stateSnapshotPending.store (true, std::memory_order_relaxed);
    }
}

void BoardAudioEngine::setModulationEnvelopeADSR (
    ModulationID sourceID,
    float attack,
    float decay,
    float sustain,
    float release)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::envelope)
        {
            source->primary.store (
                clampModulationPrimary (
                    ModulationType::envelope, attack),
                std::memory_order_relaxed);
            source->envelopeDecay.store (
                clampModulationEnvelopeDecay (decay),
                std::memory_order_relaxed);
            source->envelopeSustain.store (
                clampModulationEnvelopeSustain (sustain),
                std::memory_order_relaxed);
            source->secondary.store (
                clampModulationSecondary (
                    ModulationType::envelope, release),
                std::memory_order_relaxed);
            stateSnapshotPending.store (
                true, std::memory_order_relaxed);
        }
}

void BoardAudioEngine::setModulationRandomJitter (
    ModulationID sourceID,
    float jitter)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::random)
        {
            source->jitter.store (
                clampModulationJitter (jitter),
                std::memory_order_relaxed);
            stateSnapshotPending.store (
                true, std::memory_order_relaxed);
        }
}

void BoardAudioEngine::setModulationRandomJitterRange (
    ModulationID sourceID,
    float range)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::random)
        {
            source->jitterRange.store (
                clampModulationJitterRange (range),
                std::memory_order_relaxed);
            stateSnapshotPending.store (
                true, std::memory_order_relaxed);
        }
}

void BoardAudioEngine::setModulationLfoWaveform (
    ModulationID sourceID,
    LfoWaveform waveform)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::lfo)
        {
            source->lfoWaveform.store (
                waveform, std::memory_order_relaxed);
            stateSnapshotPending.store (
                true, std::memory_order_relaxed);
        }
}

void BoardAudioEngine::setModulationSourcePolarity (
    ModulationID sourceID,
    ModulationPolarity polarity)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
    {
        {
            const juce::SpinLock::ScopedLockType lock (
                modulationRuntimeLock);
            source->polarity.store (
                polarity, std::memory_order_relaxed);
            for (const auto& link : modulationControlLinks)
                if (link->source == source
                    && link->target
                           == ModulationControlTarget::trigger)
                    link->gateResyncPending = true;
        }
        stateSnapshotPending.store (true, std::memory_order_relaxed);
    }
}

void BoardAudioEngine::setModulationSourceAutoTrigger (
    ModulationID sourceID,
    bool shouldAutoTrigger)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::curve
            || source->type == ModulationType::envelope)
        {
            source->autoTrigger.store (
                shouldAutoTrigger, std::memory_order_relaxed);
            stateSnapshotPending.store (
                true, std::memory_order_relaxed);
        }
}

void BoardAudioEngine::triggerModulationSource (
    ModulationID sourceID) noexcept
{
    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::curve
            || source->type == ModulationType::envelope)
            source->manualTriggerPending.store (
                true, std::memory_order_release);
}

void BoardAudioEngine::setModulationSourceManualGate (
    ModulationID sourceID,
    bool gateHigh) noexcept
{
    if (const auto source = findModulationSource (sourceID))
        if (source->type == ModulationType::envelope)
        {
            const auto wasHigh = source->manualGateHigh.exchange (
                gateHigh, std::memory_order_acq_rel);
            if (gateHigh && ! wasHigh)
                source->manualTriggerPending.store (
                    true, std::memory_order_release);
        }
}

void BoardAudioEngine::setModulationCurvePoints (
    ModulationID sourceID,
    const std::vector<CurvePoint>& points)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto source = findModulationSource (sourceID);
    if (source == nullptr || source->type != ModulationType::curve
        || source->curveState == nullptr)
        return;

    auto replacement = createCurveState (
        points,
        source->curveState->loopMode,
        source->curveState->loopStart,
        source->curveState->loopEnd);
    if (replacement == nullptr)
        return;

    std::unique_ptr<const CurveState> retiredCurve;
    {
        const juce::SpinLock::ScopedLockType lock (modulationRuntimeLock);
        retiredCurve = std::move (source->curveState);
        source->curveState = std::move (replacement);
    }

    stateSnapshotPending.store (true, std::memory_order_relaxed);
}

void BoardAudioEngine::setModulationCurveLoop (
    ModulationID sourceID,
    CurveLoopMode mode,
    float loopStart,
    float loopEnd)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto source = findModulationSource (sourceID);
    if (source == nullptr || source->type != ModulationType::curve
        || source->curveState == nullptr)
        return;

    auto replacement = createCurveState (
        source->curveState->points, mode, loopStart, loopEnd);
    if (replacement == nullptr)
        return;

    const auto initialPhase =
        replacement->loopMode == CurveLoopMode::sectionLoop
            ? replacement->loopStart
            : 0.0f;
    std::unique_ptr<const CurveState> retiredCurve;
    {
        const juce::SpinLock::ScopedLockType lock (modulationRuntimeLock);
        retiredCurve = std::move (source->curveState);
        source->curveState = std::move (replacement);
        source->phase = initialPhase;
        source->displayPhase.store (
            initialPhase, std::memory_order_relaxed);
    }

    stateSnapshotPending.store (true, std::memory_order_relaxed);
}

void BoardAudioEngine::setModulationCurvePoint (
    ModulationID sourceID,
    int pointIndex,
    float value)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto source = findModulationSource (sourceID);
    if (source == nullptr || source->type != ModulationType::curve
        || source->curveState == nullptr
        || ! juce::isPositiveAndBelow (
            pointIndex,
            static_cast<int> (source->curveState->points.size())))
        return;

    auto points = source->curveState->points;
    points[static_cast<size_t> (pointIndex)].y =
        juce::jlimit (0.0f, 1.0f, value);
    setModulationCurvePoints (sourceID, points);
}

void BoardAudioEngine::beginModulationChanges (
    ModulationID sourceID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    if (pendingModulationChange.has_value())
    {
        if (pendingModulationChange->id == sourceID)
            return;
        commitModulationChanges();
    }

    if (const auto source = findModulationSource (sourceID))
        pendingModulationChange =
            captureModulationSource (source);
}

void BoardAudioEngine::commitModulationChanges()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (pendingModulationChange.has_value())
    {
        auto before = std::move (*pendingModulationChange);
        pendingModulationChange.reset();

        if (const auto source = findModulationSource (before.id))
        {
            const auto after = captureModulationSource (source);
            const auto pointsMatch =
                before.curvePoints.size() == after.curvePoints.size()
                && std::equal (
                    before.curvePoints.begin(),
                    before.curvePoints.end(),
                    after.curvePoints.begin(),
                    [] (const CurvePoint& first,
                        const CurvePoint& second)
                    {
                        return std::abs (first.x - second.x) < 0.000001f
                            && std::abs (first.y - second.y) < 0.000001f
                            && std::abs (
                                   first.leftPadding
                                   - second.leftPadding)
                                   < 0.000001f
                            && std::abs (
                                   first.rightPadding
                                   - second.rightPadding)
                                   < 0.000001f;
                    });
            const auto settingsMatch =
                before.lfoWaveform == after.lfoWaveform
                && before.polarity == after.polarity
                && std::abs (before.primary - after.primary) < 0.000001f
                && std::abs (before.secondary - after.secondary) < 0.000001f
                && std::abs (
                       before.envelopeDecay - after.envelopeDecay)
                       < 0.000001f
                && std::abs (
                       before.envelopeSustain - after.envelopeSustain)
                       < 0.000001f
                && std::abs (before.jitter - after.jitter) < 0.000001f
                && std::abs (before.jitterRange - after.jitterRange)
                       < 0.000001f
                && before.autoTrigger == after.autoTrigger
                && before.loopMode == after.loopMode
                && std::abs (before.loopStart - after.loopStart)
                       < 0.000001f
                && std::abs (before.loopEnd - after.loopEnd)
                       < 0.000001f
                && pointsMatch;

            if (! settingsMatch)
            {
                BoardEdit edit;
                edit.kind =
                    BoardEdit::Kind::changeModulationSource;
                edit.description =
                    "Edit " + getModulationTypeName (source->type);
                edit.modulationSource = std::move (before);
                recordPerformedEdit (std::move (edit));
            }
        }
    }

    scheduleBoardUpdate (false);
}

void BoardAudioEngine::beginModulationLearn (ModulationID sourceID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;

    const auto source = findModulationSource (sourceID);
    if (source == nullptr)
        return;

    {
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        pendingLearnPriority = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
        pendingLearnSource = 0;
    }

    learningModulationSource.store (sourceID, std::memory_order_release);
    modulationStatus =
        "Learning " + getModulationTypeName (source->type)
        + ": choose a plug-in or modulator target";
    sendChangeMessage();
}

void BoardAudioEngine::cancelModulationLearn()
{
    learningModulationSource.store (0, std::memory_order_release);
    {
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        pendingLearnPriority = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
        pendingLearnSource = 0;
    }

    modulationStatus = modulationSources.empty()
                           ? "Add a modulator to get started"
                           : "Ready";
    sendChangeMessage();
}

BoardAudioEngine::ModulationID
BoardAudioEngine::getLearningModulationSource() const noexcept
{
    return learningModulationSource.load (std::memory_order_acquire);
}

juce::String BoardAudioEngine::getModulationStatus() const
{
    return modulationStatus;
}

float BoardAudioEngine::getModulationSourceOutput (
    ModulationID sourceID) const noexcept
{
    if (const auto source = findModulationSource (sourceID))
        return source->output.load (std::memory_order_relaxed);

    return 0.0f;
}

bool BoardAudioEngine::connectModulationToParameter (
    ModulationID sourceID,
    NodeID nodeID,
    int parameterIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked() || findModulationSource (sourceID) == nullptr)
        return false;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr || isIONode (nodeID))
        return false;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return false;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        return false;

    auto* parameter = parameters.getUnchecked (parameterIndex);
    const auto category = static_cast<int> (parameter->getCategory());
    if (! parameter->isAutomatable()
        || (category & 0xffff0000) == (2 << 16))
        return false;

    const auto previousLinkCount = modulationLinks.size();
    beginModulationLearn (sourceID);
    if (getLearningModulationSource() != sourceID)
        return false;

    {
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        pendingLearnSource = sourceID;
        pendingLearnProcessor = processor;
        pendingLearnParameter = parameterIndex;
        pendingLearnPriority = 2;
    }

    completePendingModulationLearn();
    return modulationLinks.size() > previousLinkCount;
}

bool BoardAudioEngine::connectModulationToSource (
    ModulationID sourceID,
    ModulationID targetSourceID,
    ModulationControlTarget target)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;
    if (sourceID == 0)
        sourceID =
            learningModulationSource.load (std::memory_order_acquire);
    if (sourceID == 0 || targetSourceID == 0
        || sourceID == targetSourceID)
        return false;

    const auto source = findModulationSource (sourceID);
    const auto targetSource = findModulationSource (targetSourceID);
    if (source == nullptr || targetSource == nullptr
        || ! isModulationControlTargetSupported (
            targetSource->type, target))
        return false;

    if (std::any_of (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [sourceID, targetSourceID, target] (const auto& link)
            {
                return link->source->id == sourceID
                    && link->targetSource->id == targetSourceID
                    && link->target == target;
            }))
        return false;

    if (pendingModulationChange.has_value())
        commitModulationChanges();
    if (activeModulationLinkAmountEdit.has_value())
        commitModulationLinkAmountEdit (
            activeModulationLinkAmountEdit->linkID);

    SavedModulationControlLink saved;
    saved.id = nextModulationLinkID++;
    saved.sourceID = sourceID;
    saved.targetSourceID = targetSourceID;
    saved.target = target;
    saved.amount =
        target == ModulationControlTarget::trigger ? 1.0f : 0.0f;
    saved.index = modulationControlLinks.size();
    if (! addModulationControlLinkRaw (saved))
        return false;

    if (learningModulationSource.load (std::memory_order_acquire)
        == sourceID)
    {
        learningModulationSource.store (0, std::memory_order_release);
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        pendingLearnSource = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
        pendingLearnPriority = 0;
    }

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::addModulationControlLink;
    edit.description = "Link modulation control";
    edit.modulationControlLink = saved;
    recordPerformedEdit (std::move (edit));

    modulationStatus =
        getModulationTypeName (source->type)
        + " linked to " + getModulationTypeName (targetSource->type)
        + " " + modulationControlTargetToString (target);
    scheduleBoardUpdate (false);
    return true;
}

void BoardAudioEngine::removeModulationLinks (
    NodeID nodeID,
    int parameterIndex)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    std::vector<ModulationID> linkIDs;
    for (const auto& link : modulationLinks)
        if (link->target != nullptr
            && link->target->node != nullptr
            && link->target->node->nodeID == nodeID
            && link->target->parameterIndex == parameterIndex)
            linkIDs.push_back (link->id);

    for (const auto linkID : linkIDs)
        removeModulationLink (linkID);
}

bool BoardAudioEngine::beginModulationLinkAmountEdit (
    ModulationID linkID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return false;

    if (activeModulationLinkAmountEdit.has_value())
    {
        if (activeModulationLinkAmountEdit->linkID == linkID)
            return true;
        commitModulationLinkAmountEdit (
            activeModulationLinkAmountEdit->linkID);
    }

    const auto iterator = std::find_if (
        modulationLinks.begin(),
        modulationLinks.end(),
        [linkID] (const auto& link)
        {
            return link->id == linkID;
        });
    auto originalAmount = 0.0f;
    if (iterator != modulationLinks.end())
    {
        originalAmount =
            (*iterator)->amount.load (std::memory_order_relaxed);
    }
    else
    {
        const auto controlIterator = std::find_if (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [linkID] (const auto& link)
            {
                return link->id == linkID;
            });
        if (controlIterator == modulationControlLinks.end()
            || (*controlIterator)->target
                   == ModulationControlTarget::trigger)
            return false;
        originalAmount =
            (*controlIterator)->amount.load (std::memory_order_relaxed);
    }

    activeModulationLinkAmountEdit =
        ModulationLinkAmountEdit { linkID, originalAmount };
    return true;
}

void BoardAudioEngine::setModulationLinkAmount (
    ModulationID linkID,
    float amount)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    for (const auto& link : modulationLinks)
        if (link->id == linkID)
        {
            link->amount.store (
                juce::jlimit (-1.0f, 1.0f, amount),
                std::memory_order_relaxed);
            stateSnapshotPending.store (true, std::memory_order_relaxed);
            return;
        }

    for (const auto& link : modulationControlLinks)
        if (link->id == linkID
            && link->target != ModulationControlTarget::trigger)
        {
            link->amount.store (
                juce::jlimit (-1.0f, 1.0f, amount),
                std::memory_order_relaxed);
            stateSnapshotPending.store (true, std::memory_order_relaxed);
            return;
        }
}

void BoardAudioEngine::commitModulationLinkAmountEdit (
    ModulationID linkID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! activeModulationLinkAmountEdit.has_value()
        || activeModulationLinkAmountEdit->linkID != linkID)
        return;

    const auto originalAmount =
        activeModulationLinkAmountEdit->originalAmount;
    activeModulationLinkAmountEdit.reset();

    const auto iterator = std::find_if (
        modulationLinks.begin(),
        modulationLinks.end(),
        [linkID] (const auto& link)
        {
            return link->id == linkID;
        });
    auto currentAmount = 0.0f;
    if (iterator != modulationLinks.end())
    {
        currentAmount =
            (*iterator)->amount.load (std::memory_order_relaxed);
    }
    else
    {
        const auto controlIterator = std::find_if (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [linkID] (const auto& link)
            {
                return link->id == linkID;
            });
        if (controlIterator == modulationControlLinks.end()
            || (*controlIterator)->target
                   == ModulationControlTarget::trigger)
            return;
        currentAmount =
            (*controlIterator)->amount.load (std::memory_order_relaxed);
    }
    if (std::abs (currentAmount - originalAmount) >= 0.000001f)
    {
        BoardEdit edit;
        edit.kind = BoardEdit::Kind::changeModulationLinkAmount;
        edit.description = "Adjust modulation amount";
        edit.modulationLink.id = linkID;
        edit.oldModulationLinkAmount = originalAmount;
        edit.newModulationLinkAmount = currentAmount;
        recordPerformedEdit (std::move (edit));
    }

    scheduleBoardUpdate (false);
}

void BoardAudioEngine::removeModulationLink (ModulationID linkID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (boardEditsBlocked())
        return;
    if (activeModulationLinkAmountEdit.has_value())
        commitModulationLinkAmountEdit (
            activeModulationLinkAmountEdit->linkID);

    const auto iterator = std::find_if (
        modulationLinks.begin(),
        modulationLinks.end(),
        [linkID] (const auto& link)
        {
            return link->id == linkID;
        });

    if (iterator == modulationLinks.end())
    {
        const auto controlIterator = std::find_if (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [linkID] (const auto& link)
            {
                return link->id == linkID;
            });
        if (controlIterator == modulationControlLinks.end())
            return;

        BoardEdit edit;
        edit.kind = BoardEdit::Kind::removeModulationControlLink;
        edit.description = "Remove modulation control link";
        edit.modulationControlLink =
            captureModulationControlLink (*controlIterator);
        if (! removeModulationLinkRaw (linkID))
            return;

        modulationStatus = "Modulation control link removed";
        scheduleBoardUpdate (false);
        recordPerformedEdit (std::move (edit));
        return;
    }

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::removeModulationLink;
    edit.description = "Remove modulation link";
    edit.modulationLink = captureModulationLink (*iterator);

    if (! removeModulationLinkRaw (linkID))
        return;

    modulationStatus = "Modulation link removed";
    scheduleBoardUpdate (false);
    recordPerformedEdit (std::move (edit));
}

bool BoardAudioEngine::canUndo() const noexcept
{
    return ! isCatalogBusy()
        && ! boardEditsBlocked()
        && historyCursor > 0;
}

bool BoardAudioEngine::canRedo() const noexcept
{
    return ! isCatalogBusy()
        && ! boardEditsBlocked()
        && historyCursor < editHistory.size();
}

juce::String BoardAudioEngine::getUndoDescription() const
{
    return canUndo() ? editHistory[historyCursor - 1].description
                     : juce::String();
}

juce::String BoardAudioEngine::getRedoDescription() const
{
    return canRedo() ? editHistory[historyCursor].description
                     : juce::String();
}

void BoardAudioEngine::undo()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (pendingModulationChange.has_value())
        commitModulationChanges();
    if (activeModulationLinkAmountEdit.has_value())
        commitModulationLinkAmountEdit (
            activeModulationLinkAmountEdit->linkID);
    if (! canUndo())
        return;

    const auto editIndex = historyCursor - 1;
    const auto generation = restoreGeneration.load (std::memory_order_relaxed);
    const auto gate = callbackGate;
    const auto operationToken = beginHistoryOperation();

    applyHistoryEdit (
        editHistory[editIndex],
        false,
        [gate, editIndex, generation, operationToken] (bool success)
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr)
                return;

            auto& owner = *gate->owner;
            if (! owner.isCurrentHistoryOperation (operationToken))
                return;

            if (success
                && generation
                       == owner.restoreGeneration.load (
                           std::memory_order_relaxed)
                && editIndex < owner.editHistory.size())
                owner.historyCursor = editIndex;

            owner.finishHistoryOperation (operationToken);
        });
}

void BoardAudioEngine::redo()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (pendingModulationChange.has_value())
        commitModulationChanges();
    if (activeModulationLinkAmountEdit.has_value())
        commitModulationLinkAmountEdit (
            activeModulationLinkAmountEdit->linkID);
    if (! canRedo())
        return;

    const auto editIndex = historyCursor;
    const auto generation = restoreGeneration.load (std::memory_order_relaxed);
    const auto gate = callbackGate;
    const auto operationToken = beginHistoryOperation();

    applyHistoryEdit (
        editHistory[editIndex],
        true,
        [gate, editIndex, generation, operationToken] (bool success)
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr)
                return;

            auto& owner = *gate->owner;
            if (! owner.isCurrentHistoryOperation (operationToken))
                return;

            if (success
                && generation
                       == owner.restoreGeneration.load (
                           std::memory_order_relaxed)
                && editIndex < owner.editHistory.size())
                owner.historyCursor = editIndex + 1;

            owner.finishHistoryOperation (operationToken);
        });
}

const juce::String BoardAudioEngine::getName() const
{
    return "TB_Board";
}

void BoardAudioEngine::prepareToPlay (double sampleRate,
                                      int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmax (1, maximumExpectedSamplesPerBlock);

    graph.setPlayConfigDetails (2, 2, currentSampleRate, currentBlockSize);
    graph.setProcessingPrecision (juce::AudioProcessor::singlePrecision);
    graph.setNonRealtime (isNonRealtime());
    graph.setPlayHead (getPlayHead());
    graph.prepareToPlay (currentSampleRate, currentBlockSize);
    prepareOutputInputGain();
    prepareOutputSafetyLimiter();
    prepared = true;
    for (const auto& source : modulationSources)
    {
        source->manualTriggerPending.store (
            false, std::memory_order_relaxed);
        source->manualGateHigh.store (
            false, std::memory_order_relaxed);
    }
    transportWasPlaying.store (false, std::memory_order_release);
    transportSamplePositionValid.store (false, std::memory_order_release);
    oneTimeRetriggerPending.store (false, std::memory_order_release);
    firstProcessAfterPrepare.store (true, std::memory_order_release);
    modulationResetPending.store (true, std::memory_order_release);
    updateLatencyAndBypassCapacity();
}

void BoardAudioEngine::releaseResources()
{
    prepared = false;
    graph.releaseResources();
    for (const auto& source : modulationSources)
    {
        source->manualTriggerPending.store (
            false, std::memory_order_relaxed);
        source->manualGateHigh.store (
            false, std::memory_order_relaxed);
    }
    bypassDelay.reset();
    activeBypassDelaySamples = -1;
    resetOutputInputGain();
    resetOutputSafetyLimiter();
    transportWasPlaying.store (false, std::memory_order_release);
    transportSamplePositionValid.store (false, std::memory_order_release);
    oneTimeRetriggerPending.store (false, std::memory_order_release);
    firstProcessAfterPrepare.store (false, std::memory_order_release);
}

void BoardAudioEngine::reset()
{
    graph.reset();
    for (const auto& source : modulationSources)
    {
        source->manualTriggerPending.store (
            false, std::memory_order_relaxed);
        source->manualGateHigh.store (
            false, std::memory_order_relaxed);
    }
    bypassDelay.reset();
    activeBypassDelaySamples = -1;
    resetOutputInputGain();
    resetOutputSafetyLimiter();
    transportWasPlaying.store (false, std::memory_order_release);
    transportSamplePositionValid.store (false, std::memory_order_release);
    oneTimeRetriggerPending.store (false, std::memory_order_release);

    const juce::SpinLock::ScopedTryLockType lock (
        modulationRuntimeLock);
    if (lock.isLocked() && modulationRuntime != nullptr)
    {
        resetModulationSources();
        modulationResetPending.store (false, std::memory_order_release);
    }
    else
    {
        modulationResetPending.store (true, std::memory_order_release);
    }
}

void BoardAudioEngine::resetModulationSources()
{
    if (modulationRuntime == nullptr)
        return;

    for (const auto& source : modulationRuntime->sources)
    {
        source->displayStateVersion.fetch_add (
            1, std::memory_order_seq_cst);
        source->phase =
            source->type == ModulationType::curve
                && source->curveState != nullptr
                && source->curveState->loopMode
                       == CurveLoopMode::sectionLoop
                ? source->curveState->loopStart
                : 0.0;
        source->displayPhase.store (
            static_cast<float> (source->phase),
            std::memory_order_relaxed);

        source->envelopeValue = 0.0f;
        source->envelopeReleaseStartValue = 0.0f;
        source->envelopeBipolarValue = 0.0f;
        source->envelopeBipolarStageStartValue = 0.0f;
        source->envelopeStageSeconds = 0.0;
        source->envelopeStage =
            ModulationSource::EnvelopeStage::idle;
        source->envelopeGateHeld = false;
        source->envelopeTriggeredThisBlock = false;
        source->followerValue = 0.0f;
        source->randomValue = source->randomTarget;
        source->randomTransitionStart = source->randomTarget;
        source->randomTransitionPhase = 1.0;
        source->randomJitterHazardRemaining =
            advanceExponentialRandom (source->randomJitterState);
        source->effectiveSecondary.store (
            source->secondary.load (std::memory_order_relaxed),
            std::memory_order_relaxed);
        source->lfoRandomStart.store (
            source->randomValue, std::memory_order_relaxed);
        source->lfoRandomEnd.store (
            source->randomTarget, std::memory_order_relaxed);
        source->previousBlockOutput = 0.0f;
        source->triggerRunning = false;
        source->triggerRefractorySeconds = 0.0;
        source->output.store (0.0f, std::memory_order_relaxed);
        source->displayStateVersion.fetch_add (
            1, std::memory_order_seq_cst);
    }

    for (const auto& link : modulationRuntime->controlLinks)
        if (link->target == ModulationControlTarget::trigger)
        {
            link->triggerGateHigh = false;
            link->gateResyncPending = true;
        }
}

bool BoardAudioEngine::triggerModulationSourceNow (
    const std::shared_ptr<ModulationSource>& source) noexcept
{
    if (source == nullptr
        || (source->type != ModulationType::curve
            && source->type != ModulationType::envelope)
        || (source->type == ModulationType::curve
            && source->curveState == nullptr)
        || source->triggerRefractorySeconds > 0.0)
        return false;

    source->triggerRefractorySeconds =
        modulationTriggerRefractorySeconds;
    source->triggerRunning = true;

    if (source->type == ModulationType::curve)
    {
        source->phase =
            source->curveState->loopMode == CurveLoopMode::sectionLoop
                ? source->curveState->loopStart
                : 0.0;
        source->displayPhase.store (
            static_cast<float> (source->phase),
            std::memory_order_relaxed);
    }
    else
    {
        source->envelopeTriggeredThisBlock = true;
        source->envelopeBipolarStageStartValue =
            juce::jlimit (
                -1.0f, 1.0f, source->envelopeBipolarValue);
        source->envelopeValue = 0.0f;
        source->envelopeReleaseStartValue = 0.0f;
        source->envelopeStageSeconds = 0.0;
        source->envelopeStage =
            ModulationSource::EnvelopeStage::attack;
    }

    return true;
}

void BoardAudioEngine::updateTransportState (int blockSamples) noexcept
{
    const auto firstBlockAfterPrepare =
        firstProcessAfterPrepare.exchange (
            false, std::memory_order_acq_rel);
    auto isPlaying = false;
    auto hasSamplePosition = false;
    auto samplePosition = juce::int64 {};
    auto* currentPlayHead = getPlayHead();
    if (currentPlayHead != nullptr)
        if (const auto position = currentPlayHead->getPosition())
        {
            isPlaying = position->getIsPlaying();
            if (const auto samples = position->getTimeInSamples())
            {
                samplePosition = *samples;
                hasSamplePosition = true;
            }
        }

    if (currentPlayHead == nullptr && firstBlockAfterPrepare)
        oneTimeRetriggerPending.store (
            true, std::memory_order_release);

    const auto wasPlaying =
        transportWasPlaying.exchange (
            isPlaying, std::memory_order_acq_rel);
    const auto previousPositionWasValid =
        transportSamplePositionValid.exchange (
            isPlaying && hasSamplePosition,
            std::memory_order_acq_rel);
    const auto expectedPosition =
        expectedTransportSamplePosition.load (std::memory_order_relaxed);
    const auto positionDiscontinuity =
        isPlaying && wasPlaying && hasSamplePosition
        && previousPositionWasValid
        && samplePosition != expectedPosition;

    if (isPlaying && hasSamplePosition)
        expectedTransportSamplePosition.store (
            samplePosition + juce::jmax (0, blockSamples),
            std::memory_order_relaxed);

    if (isPlaying && (! wasPlaying || positionDiscontinuity))
        oneTimeRetriggerPending.store (true, std::memory_order_release);
}

void BoardAudioEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateTransportState (buffer.getNumSamples());

    if (buffer.getNumChannels() < 2)
    {
        oneTimeRetriggerPending.store (false, std::memory_order_release);
        buffer.clear();
        midiMessages.clear();
        return;
    }

    processModulation (buffer);
    updateDryBypassHistory (buffer, false);
    graph.setPlayHead (getPlayHead());
    graph.processBlock (buffer, midiMessages);
    processOutputInputGain (buffer);
    processOutputSafetyLimiter (buffer);
    midiMessages.clear();

    for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

void BoardAudioEngine::processBlockBypassed (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateTransportState (buffer.getNumSamples());
    oneTimeRetriggerPending.store (false, std::memory_order_release);
    midiMessages.clear();
    updateDryBypassHistory (buffer, true);
    resetOutputSafetyLimiter();

    for (int channel = 2; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

void BoardAudioEngine::setNonRealtime (bool shouldBeNonRealtime) noexcept
{
    juce::AudioProcessor::setNonRealtime (shouldBeNonRealtime);
    graph.setNonRealtime (shouldBeNonRealtime);
}

bool BoardAudioEngine::supportsDoublePrecisionProcessing() const
{
    return false;
}

bool BoardAudioEngine::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

bool BoardAudioEngine::acceptsMidi() const
{
    return false;
}

bool BoardAudioEngine::producesMidi() const
{
    return false;
}

bool BoardAudioEngine::isMidiEffect() const
{
    return false;
}

double BoardAudioEngine::getTailLengthSeconds() const
{
    // A host may suspend an effect once its reported tail has elapsed while
    // the input is silent.  Modulation sources are time-varying even in that
    // situation, so report an infinite tail while any source exists to keep
    // their phase and linked parameters advancing.
    if (! modulationSources.empty())
        return std::numeric_limits<double>::infinity();

    auto longestTail = 0.0;

    for (auto* node : graph.getNodes())
        if (! isIONode (node->nodeID))
            longestTail =
                juce::jmax (longestTail,
                            node->getProcessor()->getTailLengthSeconds());

    return longestTail;
}

bool BoardAudioEngine::hasEditor() const
{
    return true;
}

int BoardAudioEngine::getNumPrograms()
{
    return 1;
}

int BoardAudioEngine::getCurrentProgram()
{
    return 0;
}

void BoardAudioEngine::setCurrentProgram (int)
{
}

const juce::String BoardAudioEngine::getProgramName (int)
{
    return {};
}

void BoardAudioEngine::changeProgramName (int, const juce::String&)
{
}

void BoardAudioEngine::prepareBypassDelay (int requiredMaximumDelay)
{
    maximumBypassDelaySamples = juce::jmax (1, requiredMaximumDelay);
    bypassDelay.setMaximumDelayInSamples (maximumBypassDelaySamples);
    bypassDelay.prepare ({ currentSampleRate,
                           static_cast<juce::uint32> (currentBlockSize),
                           2 });
    bypassDelay.reset();
    activeBypassDelaySamples = -1;
}

void BoardAudioEngine::updateDryBypassHistory (
    juce::AudioBuffer<float>& buffer,
    bool writeOutput)
{
    if (! prepared || buffer.getNumChannels() < 2)
        return;

    const auto requestedDelay =
        juce::jlimit (0, maximumBypassDelaySamples, getLatencySamples());

    if (requestedDelay != activeBypassDelaySamples)
    {
        bypassDelay.setDelay (static_cast<float> (requestedDelay));
        activeBypassDelaySamples = requestedDelay;
    }

    auto* const* channelData = buffer.getArrayOfWritePointers();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            bypassDelay.pushSample (channel, channelData[channel][sample]);
            const auto delayed = bypassDelay.popSample (channel);

            if (writeOutput)
                channelData[channel][sample] = delayed;
        }
    }
}

void BoardAudioEngine::prepareOutputInputGain() noexcept
{
    outputInputGainSmoother.reset (
        juce::jmax (1.0, currentSampleRate), levelSmoothingSeconds);
    const auto target = outputInputGainTarget.load (std::memory_order_relaxed);
    outputInputGainSmoother.setCurrentAndTargetValue (target);
}

void BoardAudioEngine::resetOutputInputGain() noexcept
{
    outputInputGainSmoother.setCurrentAndTargetValue (
        outputInputGainTarget.load (std::memory_order_relaxed));
}

void BoardAudioEngine::processOutputInputGain (
    juce::AudioBuffer<float>& buffer) noexcept
{
    const auto target = outputInputGainTarget.load (std::memory_order_relaxed);
    if (! juce::approximatelyEqual (
            outputInputGainSmoother.getTargetValue(), target))
        outputInputGainSmoother.setTargetValue (target);

    const auto channels = juce::jmin (2, buffer.getNumChannels());
    auto* const* data = buffer.getArrayOfWritePointers();
    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = outputInputGainSmoother.getNextValue();
        for (auto channel = 0; channel < channels; ++channel)
            data[channel][sample] *= gain;
    }
}

void BoardAudioEngine::prepareOutputSafetyLimiter() noexcept
{
    const auto releaseSamples = juce::jmax (
        1.0, currentSampleRate * outputSafetyLimiterReleaseSeconds);
    outputSafetyLimiterReleaseCoefficient = static_cast<float> (
        std::exp (-1.0 / releaseSamples));
    resetOutputSafetyLimiter();
}

void BoardAudioEngine::resetOutputSafetyLimiter() noexcept
{
    outputSafetyLimiterGain = 1.0f;
}

void BoardAudioEngine::processOutputSafetyLimiter (
    juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2)
        return;

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (auto sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        if (! std::isfinite (left[sample]))
            left[sample] = 0.0f;
        if (! std::isfinite (right[sample]))
            right[sample] = 0.0f;

        const auto peak = juce::jmax (
            std::abs (left[sample]), std::abs (right[sample]));
        const auto requiredGain =
            peak > outputSafetyLimiterCeiling
                ? outputSafetyLimiterCeiling / peak
                : 1.0f;

        if (requiredGain < outputSafetyLimiterGain)
        {
            outputSafetyLimiterGain = requiredGain;
        }
        else
        {
            const auto releasedGain =
                1.0f
                - (1.0f - outputSafetyLimiterGain)
                      * outputSafetyLimiterReleaseCoefficient;
            outputSafetyLimiterGain =
                juce::jmin (requiredGain, releasedGain);
        }

        left[sample] *= outputSafetyLimiterGain;
        right[sample] *= outputSafetyLimiterGain;
    }
}

void BoardAudioEngine::updateLatencyAndBypassCapacity()
{
    const auto graphLatency = graph.getLatencySamples();
    setLatencySamples (graphLatency);

    if (! prepared)
        return;

    const auto requiredCapacity =
        juce::jmax (graphLatency + currentBlockSize + 1,
                    juce::roundToInt (currentSampleRate * 2.0));

    if (requiredCapacity > maximumBypassDelaySamples
        || maximumBypassDelaySamples <= 1)
    {
        const juce::ScopedLock engineCallbackLock (getCallbackLock());
        prepareBypassDelay (requiredCapacity);
    }
}

std::shared_ptr<BoardAudioEngine::ModulationSource>
BoardAudioEngine::findModulationSource (ModulationID sourceID) const
{
    const auto iterator = std::find_if (
        modulationSources.begin(),
        modulationSources.end(),
        [sourceID] (const auto& source)
        {
            return source->id == sourceID;
        });

    return iterator != modulationSources.end() ? *iterator : nullptr;
}

std::unique_ptr<const BoardAudioEngine::CurveState>
BoardAudioEngine::createCurveState (
    const std::vector<CurvePoint>& points,
    CurveLoopMode mode,
    float loopStart,
    float loopEnd)
{
    const auto normalisedPoints = normaliseCurvePoints (points);
    if (! normalisedPoints.has_value())
        return {};

    auto state = std::make_unique<CurveState>();
    state->points = *normalisedPoints;
    state->loopMode = mode;

    const auto validStart =
        std::isfinite (loopStart)
            ? juce::jlimit (0.0f, 1.0f, loopStart)
            : 0.25f;
    const auto validEnd =
        std::isfinite (loopEnd)
            ? juce::jlimit (0.0f, 1.0f, loopEnd)
            : 0.75f;
    constexpr auto minimumLoopLength = 0.001f;
    if (validEnd - validStart >= minimumLoopLength)
    {
        state->loopStart = validStart;
        state->loopEnd = validEnd;
    }

    return state;
}

BoardAudioEngine::SavedModulationSource
BoardAudioEngine::captureModulationSource (
    const std::shared_ptr<ModulationSource>& source) const
{
    SavedModulationSource saved;
    if (source == nullptr)
        return saved;

    saved.id = source->id;
    saved.type = source->type;
    saved.lfoWaveform =
        source->lfoWaveform.load (std::memory_order_relaxed);
    saved.polarity =
        source->polarity.load (std::memory_order_relaxed);
    saved.primary = source->primary.load (std::memory_order_relaxed);
    saved.secondary = source->secondary.load (std::memory_order_relaxed);
    saved.envelopeDecay =
        source->envelopeDecay.load (std::memory_order_relaxed);
    saved.envelopeSustain =
        source->envelopeSustain.load (std::memory_order_relaxed);
    saved.jitter = source->jitter.load (std::memory_order_relaxed);
    saved.jitterRange =
        source->jitterRange.load (std::memory_order_relaxed);
    saved.autoTrigger =
        source->autoTrigger.load (std::memory_order_relaxed);
    const auto sourceIterator = std::find (
        modulationSources.begin(), modulationSources.end(), source);
    saved.index = static_cast<size_t> (
        std::distance (modulationSources.begin(), sourceIterator));
    if (source->curveState != nullptr)
    {
        saved.curvePoints = source->curveState->points;
        saved.loopMode = source->curveState->loopMode;
        saved.loopStart = source->curveState->loopStart;
        saved.loopEnd = source->curveState->loopEnd;
    }
    return saved;
}

bool BoardAudioEngine::restoreModulationSourceSettingsRaw (
    const SavedModulationSource& saved)
{
    const auto source = findModulationSource (saved.id);
    if (source == nullptr || source->type != saved.type)
        return false;

    auto curveState = createCurveState (
        saved.curvePoints,
        saved.loopMode,
        saved.loopStart,
        saved.loopEnd);
    if (curveState == nullptr)
        return false;

    source->primary.store (
        clampModulationPrimary (source->type, saved.primary),
        std::memory_order_relaxed);
    source->secondary.store (
        clampModulationSecondary (source->type, saved.secondary),
        std::memory_order_relaxed);
    source->effectiveSecondary.store (
        source->secondary.load (std::memory_order_relaxed),
        std::memory_order_relaxed);
    source->envelopeDecay.store (
        clampModulationEnvelopeDecay (saved.envelopeDecay),
        std::memory_order_relaxed);
    source->envelopeSustain.store (
        clampModulationEnvelopeSustain (saved.envelopeSustain),
        std::memory_order_relaxed);
    source->jitter.store (
        clampModulationJitter (saved.jitter),
        std::memory_order_relaxed);
    source->jitterRange.store (
        clampModulationJitterRange (saved.jitterRange),
        std::memory_order_relaxed);
    source->autoTrigger.store (
        saved.autoTrigger, std::memory_order_relaxed);
    source->lfoWaveform.store (
        saved.lfoWaveform, std::memory_order_relaxed);
    source->polarity.store (
        saved.polarity, std::memory_order_relaxed);

    std::unique_ptr<const CurveState> retiredCurve;
    {
        const juce::SpinLock::ScopedLockType lock (
            modulationRuntimeLock);
        retiredCurve = std::move (source->curveState);
        source->curveState = std::move (curveState);
        if (source->type == ModulationType::curve)
        {
            source->phase =
                source->curveState->loopMode
                        == CurveLoopMode::sectionLoop
                    ? source->curveState->loopStart
                    : 0.0;
            source->displayPhase.store (
                static_cast<float> (source->phase),
                std::memory_order_relaxed);
        }
    }

    stateSnapshotPending.store (true, std::memory_order_relaxed);
    return true;
}

BoardAudioEngine::SavedModulationLink
BoardAudioEngine::captureModulationLink (
    const std::shared_ptr<ModulationLink>& link) const
{
    SavedModulationLink saved;
    if (link == nullptr || link->source == nullptr || link->target == nullptr
        || link->target->node == nullptr)
        return saved;

    saved.id = link->id;
    saved.sourceID = link->source->id;
    saved.nodeID = link->target->node->nodeID;
    saved.parameterID = link->target->parameterID;
    saved.parameterName = link->target->parameterName;
    saved.parameterIndex = link->target->parameterIndex;
    saved.baseValue =
        link->target->baseValue.load (std::memory_order_relaxed);
    saved.amount = link->amount.load (std::memory_order_relaxed);
    const auto linkIterator = std::find (
        modulationLinks.begin(), modulationLinks.end(), link);
    saved.index = static_cast<size_t> (
        std::distance (modulationLinks.begin(), linkIterator));
    return saved;
}

BoardAudioEngine::SavedModulationControlLink
BoardAudioEngine::captureModulationControlLink (
    const std::shared_ptr<ModulationControlLink>& link) const
{
    SavedModulationControlLink saved;
    if (link == nullptr || link->source == nullptr
        || link->targetSource == nullptr)
        return saved;

    saved.id = link->id;
    saved.sourceID = link->source->id;
    saved.targetSourceID = link->targetSource->id;
    saved.target = link->target;
    saved.amount =
        link->target == ModulationControlTarget::trigger
            ? 1.0f
            : link->amount.load (std::memory_order_relaxed);
    const auto iterator = std::find (
        modulationControlLinks.begin(),
        modulationControlLinks.end(),
        link);
    saved.index = static_cast<size_t> (
        std::distance (modulationControlLinks.begin(), iterator));
    return saved;
}

bool BoardAudioEngine::addModulationSourceRaw (
    const SavedModulationSource& saved,
    const std::vector<SavedModulationLink>& savedLinks,
    const std::vector<SavedModulationControlLink>& savedControlLinks)
{
    const juce::ScopedLock engineCallbackLock (getCallbackLock());

    if (saved.id == 0 || findModulationSource (saved.id) != nullptr)
        return false;

    for (const auto& savedLink : savedLinks)
        if (savedLink.sourceID != saved.id)
            return false;
    for (const auto& savedLink : savedControlLinks)
        if (savedLink.sourceID != saved.id
            && savedLink.targetSourceID != saved.id)
            return false;

    auto source = std::make_shared<ModulationSource> (saved.id, saved.type);
    source->lfoWaveform.store (
        saved.lfoWaveform, std::memory_order_relaxed);
    source->polarity.store (
        saved.polarity, std::memory_order_relaxed);
    source->primary.store (
        clampModulationPrimary (saved.type, saved.primary),
        std::memory_order_relaxed);
    source->secondary.store (
        clampModulationSecondary (saved.type, saved.secondary),
        std::memory_order_relaxed);
    source->effectiveSecondary.store (
        source->secondary.load (std::memory_order_relaxed),
        std::memory_order_relaxed);
    source->envelopeDecay.store (
        clampModulationEnvelopeDecay (saved.envelopeDecay),
        std::memory_order_relaxed);
    source->envelopeSustain.store (
        clampModulationEnvelopeSustain (saved.envelopeSustain),
        std::memory_order_relaxed);
    source->jitter.store (
        clampModulationJitter (saved.jitter),
        std::memory_order_relaxed);
    source->jitterRange.store (
        clampModulationJitterRange (saved.jitterRange),
        std::memory_order_relaxed);
    source->autoTrigger.store (
        saved.autoTrigger, std::memory_order_relaxed);
    auto curveState = createCurveState (
        saved.curvePoints,
        saved.loopMode,
        saved.loopStart,
        saved.loopEnd);
    if (curveState == nullptr)
        curveState = createCurveState (
            makeDefaultCurvePoints(),
            CurveLoopMode::fullLoop,
            0.25f,
            0.75f);
    source->curveState = std::move (curveState);
    if (source->curveState->loopMode == CurveLoopMode::sectionLoop)
        source->phase = source->curveState->loopStart;
    source->displayPhase.store (
        static_cast<float> (source->phase), std::memory_order_relaxed);

    const auto insertionIndex =
        juce::jmin (saved.index, modulationSources.size());
    modulationSources.insert (
        modulationSources.begin()
            + static_cast<std::ptrdiff_t> (insertionIndex),
        source);
    nextModulationSourceID =
        juce::jmax (nextModulationSourceID, saved.id + 1);

    if (savedLinks.empty() && savedControlLinks.empty())
    {
        rebuildModulationRuntime();
        return true;
    }

    for (const auto& savedLink : savedLinks)
        if (! addModulationLinkRaw (savedLink, false))
        {
            removeModulationSourceRaw (saved.id, false);
            return false;
        }

    for (const auto& savedLink : savedControlLinks)
        if (! addModulationControlLinkRaw (savedLink))
        {
            removeModulationSourceRaw (saved.id, false);
            return false;
        }

    std::vector<std::shared_ptr<ModulationTarget>> restoredTargets;
    for (const auto& savedLink : savedLinks)
    {
        const auto iterator = std::find_if (
            modulationLinks.begin(),
            modulationLinks.end(),
            [&savedLink] (const auto& link)
            {
                return link->id == savedLink.id;
            });
        if (iterator == modulationLinks.end())
            continue;

        const auto& target = (*iterator)->target;
        target->baseValue.store (
            juce::jlimit (0.0f, 1.0f, savedLink.baseValue),
            std::memory_order_relaxed);
        if (std::find (
                restoredTargets.begin(), restoredTargets.end(), target)
            == restoredTargets.end())
            restoredTargets.push_back (target);
    }

    for (const auto& target : restoredTargets)
    {
        target->holdBlocks.store (0, std::memory_order_relaxed);
        target->userEditing.store (false, std::memory_order_release);
        setModulationTargetValue (
            target, target->baseValue.load (std::memory_order_relaxed));
    }
    return true;
}

bool BoardAudioEngine::removeModulationSourceRaw (
    ModulationID sourceID,
    bool restoreTargets)
{
    const auto source = findModulationSource (sourceID);
    if (source == nullptr)
        return false;

    if (activeModulationLinkAmountEdit.has_value())
    {
        for (const auto& link : modulationLinks)
            if (link->id == activeModulationLinkAmountEdit->linkID
                && link->source->id == sourceID)
            {
                activeModulationLinkAmountEdit.reset();
                break;
            }
        if (activeModulationLinkAmountEdit.has_value())
            for (const auto& link : modulationControlLinks)
                if (link->id
                        == activeModulationLinkAmountEdit->linkID
                    && (link->source->id == sourceID
                        || link->targetSource->id == sourceID))
                {
                    activeModulationLinkAmountEdit.reset();
                    break;
                }
    }

    std::vector<std::shared_ptr<ModulationTarget>> affectedTargets;
    for (const auto& link : modulationLinks)
        if (link->source->id == sourceID
            && std::find (
                   affectedTargets.begin(),
                   affectedTargets.end(),
                   link->target)
                   == affectedTargets.end())
            affectedTargets.push_back (link->target);

    modulationLinks.erase (
        std::remove_if (
            modulationLinks.begin(),
            modulationLinks.end(),
            [sourceID] (const auto& link)
            {
                return link->source->id == sourceID;
            }),
        modulationLinks.end());
    modulationControlLinks.erase (
        std::remove_if (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [sourceID] (const auto& link)
            {
                return link->source->id == sourceID
                    || link->targetSource->id == sourceID;
            }),
        modulationControlLinks.end());
    modulationSources.erase (
        std::remove_if (
            modulationSources.begin(),
            modulationSources.end(),
            [sourceID] (const auto& candidate)
            {
                return candidate->id == sourceID;
            }),
        modulationSources.end());

    if (learningModulationSource.load (std::memory_order_acquire) == sourceID)
    {
        learningModulationSource.store (0, std::memory_order_release);
        const juce::SpinLock::ScopedLockType learnLock (pendingLearnLock);
        pendingLearnSource = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
        pendingLearnPriority = 0;
    }

    std::vector<std::shared_ptr<ModulationTarget>> retiredTargets;
    for (const auto& target : affectedTargets)
    {
        const auto stillLinked =
            std::any_of (
                modulationLinks.begin(),
                modulationLinks.end(),
                [&target] (const auto& link)
                {
                    return link->target == target;
                });

        if (! stillLinked && restoreTargets)
        {
            target->userEditing.store (true, std::memory_order_release);
            retiredTargets.push_back (target);
        }
    }

    rebuildModulationRuntime();
    for (const auto& target : retiredTargets)
        setModulationTargetValue (
            target, target->baseValue.load (std::memory_order_relaxed));
    return true;
}

bool BoardAudioEngine::addModulationLinkRaw (
    const SavedModulationLink& saved,
    bool applyTargetValue)
{
    const juce::ScopedLock engineCallbackLock (getCallbackLock());

    if (saved.id == 0 || saved.sourceID == 0)
        return false;

    if (std::any_of (
            modulationLinks.begin(),
            modulationLinks.end(),
            [&saved] (const auto& link)
            {
                return link->id == saved.id;
            }))
        return false;
    if (std::any_of (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [&saved] (const auto& link)
            {
                return link->id == saved.id;
            }))
        return false;

    const auto source = findModulationSource (saved.sourceID);
    const auto node = graph.getNodeForId (saved.nodeID);
    if (source == nullptr || node == nullptr || isIONode (saved.nodeID))
        return false;

    auto* processor = getParameterProcessor (node->getProcessor());
    if (processor == nullptr)
        return false;

    const auto& parameters = processor->getParameters();
    auto parameterIndex = -1;
    if (saved.parameterID.isNotEmpty())
    {
        for (int index = 0; index < parameters.size(); ++index)
            if (auto* hosted =
                    dynamic_cast<juce::HostedAudioProcessorParameter*> (
                        parameters.getUnchecked (index)))
                if (hosted->getParameterID() == saved.parameterID)
                {
                    parameterIndex = index;
                    break;
                }
    }
    else if (juce::isPositiveAndBelow (
                 saved.parameterIndex, parameters.size()))
    {
        parameterIndex = saved.parameterIndex;
    }

    if (! juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        return false;

    auto* parameter = parameters.getUnchecked (parameterIndex);
    const auto category = static_cast<int> (parameter->getCategory());
    if (! parameter->isAutomatable()
        || (category & 0xffff0000) == (2 << 16))
        return false;

    for (const auto& existingLink : modulationLinks)
        if (existingLink->source == source
            && existingLink->target->processor == processor
            && existingLink->target->parameterIndex == parameterIndex)
            return false;

    std::shared_ptr<ModulationTarget> target;
    for (const auto& existingLink : modulationLinks)
        if (existingLink->target->processor == processor
            && existingLink->target->parameterIndex == parameterIndex)
        {
            target = existingLink->target;
            break;
        }

    const auto targetWasCreated = target == nullptr;
    if (targetWasCreated)
    {
        target = std::make_shared<ModulationTarget>();
        target->node = node;
        target->processor = processor;
        target->parameter = parameter;
        target->parameterIndex = parameterIndex;
        target->parameterID = saved.parameterID;
        if (target->parameterID.isEmpty())
            if (auto* hosted =
                    dynamic_cast<juce::HostedAudioProcessorParameter*> (
                        parameter))
                target->parameterID = hosted->getParameterID();

        target->parameterName = saved.parameterName.isNotEmpty()
                                    ? saved.parameterName
                                    : parameter->getName (96);
        if (target->parameterName.isEmpty())
            target->parameterName =
                "Parameter " + juce::String (parameterIndex + 1);
        target->targetName =
            node->getProcessor()->getName()
            + " #" + juce::String (node->nodeID.uid)
            + "  /  " + target->parameterName;
    }

    if (targetWasCreated)
        target->baseValue.store (
            juce::jlimit (0.0f, 1.0f, saved.baseValue),
            std::memory_order_relaxed);

    auto link = std::make_shared<ModulationLink>();
    link->id = saved.id;
    link->source = source;
    link->target = target;
    link->amount.store (
        juce::jlimit (-1.0f, 1.0f, saved.amount),
        std::memory_order_relaxed);
    const auto insertionIndex =
        juce::jmin (saved.index, modulationLinks.size());
    modulationLinks.insert (
        modulationLinks.begin()
            + static_cast<std::ptrdiff_t> (insertionIndex),
        std::move (link));
    nextModulationLinkID =
        juce::jmax (nextModulationLinkID, saved.id + 1);
    rebuildModulationRuntime();
    if (applyTargetValue)
    {
        target->baseValue.store (
            juce::jlimit (0.0f, 1.0f, saved.baseValue),
            std::memory_order_relaxed);
        target->holdBlocks.store (0, std::memory_order_relaxed);
        target->userEditing.store (false, std::memory_order_release);
        setModulationTargetValue (
            target, target->baseValue.load (std::memory_order_relaxed));
    }
    return true;
}

bool BoardAudioEngine::addModulationControlLinkRaw (
    const SavedModulationControlLink& saved)
{
    const juce::ScopedLock engineCallbackLock (getCallbackLock());
    if (saved.id == 0 || saved.sourceID == 0
        || saved.targetSourceID == 0
        || saved.sourceID == saved.targetSourceID)
        return false;

    const auto idIsUsed =
        std::any_of (
            modulationLinks.begin(),
            modulationLinks.end(),
            [&saved] (const auto& link)
            {
                return link->id == saved.id;
            })
        || std::any_of (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [&saved] (const auto& link)
            {
                return link->id == saved.id;
            });
    if (idIsUsed)
        return false;

    const auto source = findModulationSource (saved.sourceID);
    const auto targetSource =
        findModulationSource (saved.targetSourceID);
    if (source == nullptr || targetSource == nullptr
        || ! isModulationControlTargetSupported (
            targetSource->type, saved.target))
        return false;

    if (std::any_of (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [&saved] (const auto& link)
            {
                return link->source->id == saved.sourceID
                    && link->targetSource->id == saved.targetSourceID
                    && link->target == saved.target;
            }))
        return false;

    auto link = std::make_shared<ModulationControlLink>();
    link->id = saved.id;
    link->source = source;
    link->targetSource = targetSource;
    link->target = saved.target;
    link->amount.store (
        saved.target == ModulationControlTarget::trigger
            ? 1.0f
            : juce::jlimit (-1.0f, 1.0f, saved.amount),
        std::memory_order_relaxed);
    link->gateResyncPending =
        saved.target == ModulationControlTarget::trigger;
    const auto insertionIndex =
        juce::jmin (saved.index, modulationControlLinks.size());
    modulationControlLinks.insert (
        modulationControlLinks.begin()
            + static_cast<std::ptrdiff_t> (insertionIndex),
        std::move (link));
    nextModulationLinkID =
        juce::jmax (nextModulationLinkID, saved.id + 1);
    rebuildModulationRuntime();
    return true;
}

bool BoardAudioEngine::removeModulationLinkRaw (ModulationID linkID)
{
    const auto iterator = std::find_if (
        modulationLinks.begin(),
        modulationLinks.end(),
        [linkID] (const auto& link)
        {
            return link->id == linkID;
        });
    if (iterator == modulationLinks.end())
    {
        const auto controlIterator = std::find_if (
            modulationControlLinks.begin(),
            modulationControlLinks.end(),
            [linkID] (const auto& link)
            {
                return link->id == linkID;
            });
        if (controlIterator == modulationControlLinks.end())
            return false;

        if (activeModulationLinkAmountEdit.has_value()
            && activeModulationLinkAmountEdit->linkID == linkID)
            activeModulationLinkAmountEdit.reset();
        modulationControlLinks.erase (controlIterator);
        rebuildModulationRuntime();
        return true;
    }

    if (activeModulationLinkAmountEdit.has_value()
        && activeModulationLinkAmountEdit->linkID == linkID)
        activeModulationLinkAmountEdit.reset();

    const auto target = (*iterator)->target;
    modulationLinks.erase (iterator);
    const auto stillLinked =
        std::any_of (
            modulationLinks.begin(),
            modulationLinks.end(),
            [&target] (const auto& link)
            {
                return link->target == target;
            });
    if (! stillLinked)
        target->userEditing.store (true, std::memory_order_release);

    rebuildModulationRuntime();
    if (! stillLinked)
        setModulationTargetValue (
            target, target->baseValue.load (std::memory_order_relaxed));
    return true;
}

void BoardAudioEngine::rebuildModulationRuntime()
{
    auto runtime = std::make_unique<ModulationRuntime>();
    runtime->sources = modulationSources;
    runtime->controlLinks = modulationControlLinks;

    for (const auto& link : modulationLinks)
    {
        auto target = std::find_if (
            runtime->targets.begin(),
            runtime->targets.end(),
            [&link] (const auto& entry)
            {
                return entry.target == link->target;
            });

        if (target == runtime->targets.end())
        {
            ModulationRuntime::TargetEntry entry;
            entry.target = link->target;
            runtime->targets.push_back (std::move (entry));
            target = std::prev (runtime->targets.end());
        }

        target->links.push_back (link);
    }

    {
        const juce::SpinLock::ScopedLockType lock (
            modulationRuntimeLock);
        modulationRuntime.swap (runtime);
    }

    if (runtime != nullptr)
    {
        retiredModulationRuntimes.push_back (std::move (runtime));
        retiredModulationRuntimeReapPending.store (
            true, std::memory_order_release);
    }
    reapRetiredModulationRuntimes();
}

void BoardAudioEngine::reapRetiredModulationRuntimes()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (modulationTargetApplyReaders.load (std::memory_order_acquire) == 0)
    {
        retiredModulationRuntimes.clear();
        retiredModulationRuntimeReapPending.store (
            false, std::memory_order_release);
    }
}

void BoardAudioEngine::removeModulationForNode (NodeID nodeID)
{
    if (activeModulationLinkAmountEdit.has_value())
        for (const auto& link : modulationLinks)
            if (link->id == activeModulationLinkAmountEdit->linkID
                && link->target->node->nodeID == nodeID)
            {
                activeModulationLinkAmountEdit.reset();
                break;
            }

    for (const auto& link : modulationLinks)
        if (link->target->node->nodeID == nodeID)
            link->target->userEditing.store (
                true, std::memory_order_release);

    const auto oldSize = modulationLinks.size();
    modulationLinks.erase (
        std::remove_if (
            modulationLinks.begin(),
            modulationLinks.end(),
            [nodeID] (const auto& link)
            {
                return link->target->node->nodeID == nodeID;
            }),
        modulationLinks.end());

    if (modulationLinks.size() != oldSize)
    {
        modulationStatus = "Links to the deleted plug-in were removed";
        rebuildModulationRuntime();
    }
}

void BoardAudioEngine::clearModulation()
{
    pendingModulationChange.reset();
    activeModulationLinkAmountEdit.reset();
    activeNodeLevelEdit.reset();
    activeInputCardLevelEdit.reset();
    for (const auto& link : modulationLinks)
        link->target->userEditing.store (
            true, std::memory_order_release);

    learningModulationSource.store (0, std::memory_order_release);
    {
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        pendingLearnSource = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
        pendingLearnPriority = 0;
    }

    modulationLinks.clear();
    modulationControlLinks.clear();
    modulationSources.clear();
    nextModulationSourceID = 1;
    nextModulationLinkID = 1;
    modulationStatus = "Add a modulator to get started";
    rebuildModulationRuntime();
}

void BoardAudioEngine::queueModulationLearnCandidate (
    juce::AudioProcessor* processor,
    int parameterIndex,
    bool gesture)
{
    if (processor == nullptr || parameterIndex < 0)
        return;

    const auto sourceID =
        learningModulationSource.load (std::memory_order_acquire);
    if (sourceID == 0 || activeModulationWriter == this)
        return;

    if (! gesture)
    {
        auto* messageManager =
            juce::MessageManager::getInstanceWithoutCreating();
        if (messageManager == nullptr
            || ! messageManager->isThisTheMessageThread())
            return;
    }

    {
        const juce::SpinLock::ScopedTryLockType lock (pendingLearnLock);
        if (! lock.isLocked()
            || (! gesture && pendingLearnPriority != 0))
            return;

        pendingLearnSource = sourceID;
        pendingLearnProcessor = processor;
        pendingLearnParameter = parameterIndex;
        pendingLearnPriority = gesture ? 2 : 1;
    }

    triggerAsyncUpdate();
}

bool BoardAudioEngine::completePendingModulationLearn()
{
    ModulationID sourceID = 0;
    juce::AudioProcessor* processor = nullptr;
    auto parameterIndex = -1;
    auto priority = 0;

    {
        const juce::SpinLock::ScopedLockType lock (pendingLearnLock);
        priority = pendingLearnPriority;
        sourceID = pendingLearnSource;
        processor = pendingLearnProcessor;
        parameterIndex = pendingLearnParameter;
        pendingLearnPriority = 0;
        pendingLearnSource = 0;
        pendingLearnProcessor = nullptr;
        pendingLearnParameter = -1;
    }

    if (priority == 0)
        return false;

    if (boardEditsBlocked())
    {
        learningModulationSource.store (0, std::memory_order_release);
        modulationStatus = "Modulation learn cancelled while the board was busy";
        return true;
    }

    const auto source = findModulationSource (sourceID);

    if (source == nullptr
        || sourceID
               != learningModulationSource.load (std::memory_order_acquire)
        || processor == nullptr
        || parameterIndex < 0)
        return true;

    juce::AudioProcessorGraph::Node::Ptr graphNode;
    for (auto* candidate : graph.getNodes())
    {
        if (isIONode (candidate->nodeID))
            continue;

        auto* nodeProcessor = candidate->getProcessor();
        const auto matchesNode = nodeProcessor == processor;
        auto* dualMono = getDualMonoProcessor (nodeProcessor);
        const auto matchesDualMono =
            dualMono != nullptr
            && &dualMono->getLeftInstance() == processor;
        const auto matchesWrappedParameterProcessor =
            getParameterProcessor (nodeProcessor) == processor;

        if (matchesNode || matchesDualMono
            || matchesWrappedParameterProcessor)
        {
            graphNode = graph.getNodeForId (candidate->nodeID);
            break;
        }
    }

    if (graphNode == nullptr)
    {
        modulationStatus = "That plug-in is no longer on the board";
        return true;
    }

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (parameterIndex, parameters.size()))
        return true;

    auto* parameter = parameters.getUnchecked (parameterIndex);
    const auto category = static_cast<int> (parameter->getCategory());
    const auto isMeter = (category & 0xffff0000) == (2 << 16);
    if (! parameter->isAutomatable() || isMeter)
    {
        modulationStatus = "That control cannot be automated. Try another knob.";
        return true;
    }

    for (const auto& link : modulationLinks)
    {
        if (link->source == source
            && link->target->processor == processor
            && link->target->parameterIndex == parameterIndex)
        {
            learningModulationSource.store (0, std::memory_order_release);
            modulationStatus =
                "Already linked to " + link->target->targetName;
            return true;
        }
    }

    std::shared_ptr<ModulationTarget> target;
    for (const auto& link : modulationLinks)
        if (link->target->processor == processor
            && link->target->parameterIndex == parameterIndex)
        {
            target = link->target;
            break;
        }

    if (target == nullptr)
    {
        target = std::make_shared<ModulationTarget>();
        target->node = graphNode;
        target->processor = processor;
        target->parameter = parameter;
        target->parameterIndex = parameterIndex;
        target->parameterName = parameter->getName (96);

        if (auto* hosted =
                dynamic_cast<juce::HostedAudioProcessorParameter*> (parameter))
            target->parameterID = hosted->getParameterID();

        if (target->parameterName.isEmpty())
            target->parameterName =
                "Parameter " + juce::String (parameterIndex + 1);

        target->targetName =
            graphNode->getProcessor()->getName()
            + " #" + juce::String (graphNode->nodeID.uid)
            + "  /  " + target->parameterName;
        target->baseValue.store (
            parameter->getValue(), std::memory_order_relaxed);
    }

    auto link = std::make_shared<ModulationLink>();
    link->id = nextModulationLinkID++;
    link->source = source;
    link->target = target;
    link->amount.store (0.0f, std::memory_order_relaxed);
    modulationLinks.push_back (link);

    BoardEdit edit;
    edit.kind = BoardEdit::Kind::addModulationLink;
    edit.description = "Learn modulation target";
    edit.modulationLink = captureModulationLink (link);

    learningModulationSource.store (0, std::memory_order_release);
    modulationStatus =
        getModulationTypeName (source->type)
        + " linked to " + target->targetName;
    rebuildModulationRuntime();
    stateSnapshotPending.store (true, std::memory_order_relaxed);
    recordPerformedEdit (std::move (edit));
    return true;
}

bool BoardAudioEngine::updateModulationTargetFromUser (
    juce::AudioProcessor* processor,
    int parameterIndex,
    float value,
    bool gestureBeginning,
    bool gestureEnding)
{
    const auto updateTarget = [&]() -> bool
    {
        if (modulationRuntime == nullptr)
            return false;

        for (const auto& entry : modulationRuntime->targets)
        {
            const auto& target = entry.target;
            if (target->processor != processor
                || target->parameterIndex != parameterIndex)
                continue;

            if (gestureBeginning)
            {
                target->gestureStartValue.store (
                    value, std::memory_order_relaxed);
                target->gestureStartBaseValue.store (
                    target->baseValue.load (std::memory_order_relaxed),
                    std::memory_order_relaxed);
                target->userEditing.store (true, std::memory_order_release);
                return false;
            }

            if (gestureEnding)
            {
                const auto startValue =
                    target->gestureStartValue.load (
                        std::memory_order_relaxed);
                if (std::abs (value - startValue) >= 0.00001f)
                    target->baseValue.store (
                        value, std::memory_order_relaxed);
                else
                    target->baseValue.store (
                        target->gestureStartBaseValue.load (
                            std::memory_order_relaxed),
                        std::memory_order_relaxed);

                target->userEditing.store (false, std::memory_order_release);
                target->holdBlocks.store (2, std::memory_order_relaxed);
                return false;
            }

            const auto lastApplied =
                target->lastAppliedValue.load (std::memory_order_relaxed);
            if (std::abs (value - lastApplied) < 0.00001f)
                return true;

            target->baseValue.store (value, std::memory_order_relaxed);

            const auto holdBlocks = juce::jlimit (
                2,
                256,
                juce::roundToInt (
                    0.2 * currentSampleRate
                    / static_cast<double> (juce::jmax (1, currentBlockSize))));
            target->holdBlocks.store (holdBlocks, std::memory_order_relaxed);
            return false;
        }

        return false;
    };

    auto* messageManager =
        juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager != nullptr
        && messageManager->isThisTheMessageThread())
    {
        const juce::SpinLock::ScopedLockType lock (
            modulationRuntimeLock);
        return updateTarget();
    }

    const juce::SpinLock::ScopedTryLockType lock (
        modulationRuntimeLock);
    return lock.isLocked() ? updateTarget() : false;
}

void BoardAudioEngine::refreshModulationTargetBasesForProcessor (
    juce::AudioProcessor* processor)
{
    if (processor == nullptr)
        return;

    const auto refreshTargets = [&]
    {
        if (modulationRuntime == nullptr)
            return;

        for (const auto& entry : modulationRuntime->targets)
        {
            const auto& target = entry.target;
            if (target->processor != processor
                && (target->node == nullptr
                    || target->node->getProcessor() != processor))
                continue;

            const auto value = target->parameter->getValue();
            const auto lastApplied =
                target->lastAppliedValue.load (std::memory_order_relaxed);
            if (std::abs (value - lastApplied) >= 0.00001f)
                target->baseValue.store (value, std::memory_order_relaxed);
            target->lastAppliedValue.store (value, std::memory_order_relaxed);
            target->holdBlocks.store (2, std::memory_order_relaxed);
        }
    };

    auto* messageManager =
        juce::MessageManager::getInstanceWithoutCreating();
    if (messageManager != nullptr
        && messageManager->isThisTheMessageThread())
    {
        const juce::SpinLock::ScopedLockType lock (
            modulationRuntimeLock);
        refreshTargets();
        return;
    }

    const juce::SpinLock::ScopedTryLockType lock (
        modulationRuntimeLock);
    if (lock.isLocked())
        refreshTargets();
}

void BoardAudioEngine::setModulationTargetValue (
    const std::shared_ptr<ModulationTarget>& target,
    float value)
{
    setModulationTargetValue (target.get(), value);
}

void BoardAudioEngine::setModulationTargetValue (
    ModulationTarget* target,
    float value)
{
    if (target == nullptr || target->node == nullptr
        || target->parameter == nullptr)
        return;

    const auto normalisedValue = juce::jlimit (0.0f, 1.0f, value);
    target->lastAppliedValue.store (
        normalisedValue, std::memory_order_relaxed);
    const juce::ScopedValueSetter<const BoardAudioEngine*> writerScope (
        activeModulationWriter, this);

    if (auto* dualMono = getDualMonoProcessor (
            target->node->getProcessor()))
        dualMono->setLinkedParameterValue (
            target->parameterIndex, normalisedValue);
    else
        target->parameter->setValue (normalisedValue);
}

void BoardAudioEngine::processModulation (
    const juce::AudioBuffer<float>& buffer)
{
    auto inputPeak = 0.0f;
    const auto channels = juce::jmin (2, buffer.getNumChannels());
    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* samples = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            inputPeak = juce::jmax (inputPeak, std::abs (samples[sample]));
    }

    const auto sampleRate = juce::jmax (1.0, currentSampleRate);
    const auto blockSeconds =
        static_cast<double> (buffer.getNumSamples()) / sampleRate;
    constexpr auto twoPi = juce::MathConstants<double>::twoPi;

    auto targetCount = size_t {};
    {
        const juce::SpinLock::ScopedTryLockType lock (
            modulationRuntimeLock);
        if (! lock.isLocked())
            return;

        if (modulationRuntime == nullptr
            || modulationRuntime->sources.empty())
        {
            oneTimeRetriggerPending.store (
                false, std::memory_order_release);
            return;
        }

        const auto transportTriggerWasPending =
            oneTimeRetriggerPending.exchange (
                false, std::memory_order_acq_rel);
        if (modulationResetPending.exchange (
                false, std::memory_order_acq_rel))
            resetModulationSources();

        for (const auto& source : modulationRuntime->sources)
        {
            source->previousBlockOutput =
                source->output.load (std::memory_order_relaxed);
            source->envelopeTriggeredThisBlock = false;
            source->triggerRefractorySeconds = juce::jmax (
                0.0,
                source->triggerRefractorySeconds - blockSeconds);
        }

        if (transportTriggerWasPending)
            for (const auto& source : modulationRuntime->sources)
                if (source->autoTrigger.load (
                        std::memory_order_relaxed))
                    triggerModulationSourceNow (source);

        for (const auto& source : modulationRuntime->sources)
            if (source->manualTriggerPending.exchange (
                    false, std::memory_order_acq_rel))
                triggerModulationSourceNow (source);

        const auto getEffectiveControlValue =
            [&] (const std::shared_ptr<ModulationSource>& targetSource,
                 ModulationControlTarget target,
                 float basePhysical)
        {
            auto normalised = modulationControlToNormalised (
                targetSource->type, target, basePhysical);
            for (const auto& link : modulationRuntime->controlLinks)
                if (link->targetSource == targetSource
                    && link->target == target)
                    normalised +=
                        link->source->previousBlockOutput
                        * link->amount.load (
                            std::memory_order_relaxed);

            return modulationControlFromNormalised (
                targetSource->type,
                target,
                juce::jlimit (0.0f, 1.0f, normalised));
        };

        const auto beginEnvelopeRelease =
            [] (const std::shared_ptr<ModulationSource>& source)
        {
            if (source->type != ModulationType::envelope
                || source->envelopeStage
                       == ModulationSource::EnvelopeStage::idle
                || source->envelopeStage
                       == ModulationSource::EnvelopeStage::release)
                return;

            source->envelopeReleaseStartValue =
                juce::jlimit (0.0f, 1.0f, source->envelopeValue);
            source->envelopeBipolarStageStartValue =
                juce::jlimit (
                    -1.0f,
                    1.0f,
                    source->envelopeBipolarValue);
            source->envelopeStageSeconds = 0.0;
            source->envelopeStage =
                ModulationSource::EnvelopeStage::release;
        };

        for (const auto& link : modulationRuntime->controlLinks)
        {
            if (link->target != ModulationControlTarget::trigger)
                continue;
            if (link->gateResyncPending)
                continue;

            const auto gateValue =
                modulationOutputToNormalisedForTrigger (
                    link->source->type,
                    link->source->polarity.load (
                        std::memory_order_relaxed),
                    link->source->previousBlockOutput);
            if (link->triggerGateHigh)
            {
                if (gateValue <= 0.45f)
                    link->triggerGateHigh = false;
                continue;
            }
            if (gateValue < 0.55f)
                continue;

            link->triggerGateHigh = true;
            triggerModulationSourceNow (link->targetSource);
        }

        for (const auto& source : modulationRuntime->sources)
        {
            if (source->type != ModulationType::envelope)
                continue;

            auto gateHeld = source->manualGateHigh.load (
                std::memory_order_acquire);
            if (! gateHeld)
                for (const auto& link : modulationRuntime->controlLinks)
                    if (link->target
                            == ModulationControlTarget::trigger
                        && link->targetSource == source
                        && link->triggerGateHigh)
                    {
                        gateHeld = true;
                        break;
                    }

            if (source->envelopeGateHeld
                && ! gateHeld
                && ! source->envelopeTriggeredThisBlock)
                beginEnvelopeRelease (source);
            source->envelopeGateHeld = gateHeld;
        }

        for (const auto& source : modulationRuntime->sources)
        {
        const auto primary =
            getEffectiveControlValue (
                source,
                ModulationControlTarget::primary,
                source->primary.load (std::memory_order_relaxed));
        const auto secondary =
            getEffectiveControlValue (
                source,
                ModulationControlTarget::secondary,
                source->secondary.load (std::memory_order_relaxed));
        const auto envelopeDecay =
            source->type == ModulationType::envelope
                ? getEffectiveControlValue (
                      source,
                      ModulationControlTarget::envelopeDecay,
                      source->envelopeDecay.load (
                          std::memory_order_relaxed))
                : 0.0f;
        const auto envelopeSustain =
            source->type == ModulationType::envelope
                ? getEffectiveControlValue (
                      source,
                      ModulationControlTarget::envelopeSustain,
                      source->envelopeSustain.load (
                          std::memory_order_relaxed))
                : 0.0f;
        const auto jitter =
            source->type == ModulationType::random
                ? getEffectiveControlValue (
                      source,
                      ModulationControlTarget::jitter,
                      source->jitter.load (
                          std::memory_order_relaxed))
                : 0.0f;
        const auto jitterRange =
            source->type == ModulationType::random
                ? getEffectiveControlValue (
                      source,
                      ModulationControlTarget::jitterRange,
                      source->jitterRange.load (
                          std::memory_order_relaxed))
                : 0.0f;
        auto unipolarOutput = 0.5f;
        auto displayPhase =
            source->displayPhase.load (std::memory_order_relaxed);

        switch (source->type)
        {
            case ModulationType::lfo:
            {
                const auto rate = juce::jlimit (0.01f, 40.0f, primary);
                const auto nextPhase =
                    source->phase + rate * blockSeconds;
                const auto wrapCount = juce::jmax (
                    0,
                    static_cast<int> (std::floor (nextPhase)));
                source->phase = wrapPositive (nextPhase, 1.0);
                displayPhase = static_cast<float> (source->phase);
                const auto sine = std::sin (twoPi * source->phase);
                const auto triangle =
                    2.0 / juce::MathConstants<double>::pi
                    * std::asin (std::sin (twoPi * source->phase));
                const auto shape =
                    juce::jlimit (0.0f, 1.0f, secondary);
                const auto centredSaw =
                    source->phase < 0.5
                        ? source->phase * 2.0
                        : source->phase * 2.0 - 2.0;
                auto bipolarOutput = 0.0;

                switch (source->lfoWaveform.load (
                    std::memory_order_relaxed))
                {
                    case LfoWaveform::sine:
                        bipolarOutput = sine;
                        break;
                    case LfoWaveform::triangle:
                        bipolarOutput = triangle;
                        break;
                    case LfoWaveform::sawUp:
                        bipolarOutput = centredSaw;
                        break;
                    case LfoWaveform::sawDown:
                        bipolarOutput = -centredSaw;
                        break;
                    case LfoWaveform::square:
                        bipolarOutput =
                            source->phase < 0.5 ? 1.0 : -1.0;
                        break;
                    case LfoWaveform::pulse:
                    {
                        const auto duty = 0.50 - 0.45 * shape;
                        bipolarOutput =
                            source->phase < duty ? 1.0 : -1.0;
                        break;
                    }
                    case LfoWaveform::steps:
                    {
                        const auto steps =
                            2 + juce::roundToInt (30.0f * shape);
                        const auto bin = juce::jmin (
                            steps - 1,
                            static_cast<int> (
                                std::floor (
                                    source->phase * steps)));
                        bipolarOutput =
                            -1.0
                            + 2.0 * static_cast<double> (bin)
                                / static_cast<double> (steps - 1);
                        break;
                    }
                    case LfoWaveform::sampleAndHold:
                        for (int wrap = 0; wrap < wrapCount; ++wrap)
                            source->randomTarget =
                                advanceBipolarRandom (
                                    source->randomState);
                        bipolarOutput = source->randomTarget;
                        break;
                    case LfoWaveform::smoothRandom:
                        for (int wrap = 0; wrap < wrapCount; ++wrap)
                        {
                            source->randomValue =
                                source->randomTarget;
                            source->randomTarget =
                                advanceBipolarRandom (
                                    source->randomState);
                        }
                        {
                            const auto phase =
                                static_cast<float> (source->phase);
                            const auto interpolation =
                                phase * phase * (3.0f - 2.0f * phase);
                            bipolarOutput = juce::jmap (
                                interpolation,
                                source->randomValue,
                                source->randomTarget);
                        }
                        break;
                    case LfoWaveform::legacyMorph:
                        bipolarOutput = juce::jmap (
                            static_cast<double> (shape),
                            sine,
                            triangle);
                        break;
                }

                unipolarOutput =
                    static_cast<float> (bipolarOutput * 0.5 + 0.5);
                break;
            }

            case ModulationType::curve:
            {
                if (source->curveState == nullptr)
                    break;

                const auto& curve = *source->curveState;
                const auto rate = juce::jlimit (0.01f, 40.0f, primary);
                const auto advance =
                    static_cast<double> (rate) * blockSeconds;
                auto samplePosition = source->phase;

                if (source->triggerRunning)
                {
                    switch (curve.loopMode)
                    {
                        case CurveLoopMode::oneTime:
                            source->phase =
                                juce::jmin (
                                    1.0, source->phase + advance);
                            samplePosition = source->phase;
                            if (source->phase >= 1.0)
                                source->triggerRunning = false;
                            break;

                        case CurveLoopMode::fullLoop:
                            source->phase =
                                wrapPositive (
                                    source->phase + advance, 1.0);
                            samplePosition = source->phase;
                            break;

                        case CurveLoopMode::sectionLoop:
                        {
                            const auto start =
                                static_cast<double> (curve.loopStart);
                            const auto length =
                                static_cast<double> (
                                    curve.loopEnd - curve.loopStart);
                            source->phase =
                                start
                                + wrapPositive (
                                    source->phase - start + advance,
                                    length);
                            samplePosition = source->phase;
                            break;
                        }

                        case CurveLoopMode::yoYo:
                        {
                            source->phase =
                                wrapPositive (
                                    source->phase + advance, 2.0);
                            samplePosition =
                                source->phase <= 1.0
                                    ? source->phase
                                    : 2.0 - source->phase;
                            break;
                        }
                    }
                }

                displayPhase = static_cast<float> (samplePosition);
                unipolarOutput =
                    sampleCurvePoints (curve.points, samplePosition);
                break;
            }

            case ModulationType::envelope:
            {
                const auto attackSeconds =
                    juce::jmax (0.001f, primary * 0.001f);
                const auto decaySeconds =
                    juce::jmax (0.001f, envelopeDecay * 0.001f);
                const auto sustain =
                    juce::jlimit (0.0f, 1.0f, envelopeSustain);
                const auto releaseSeconds =
                    juce::jmax (0.001f, secondary * 0.001f);

                auto remainingSeconds = blockSeconds;
                for (int transition = 0;
                     transition < 8 && remainingSeconds > 0.0;
                     ++transition)
                {
                    switch (source->envelopeStage)
                    {
                        case ModulationSource::EnvelopeStage::idle:
                            source->envelopeValue = 0.0f;
                            source->envelopeBipolarValue = 0.0f;
                            source->envelopeBipolarStageStartValue = 0.0f;
                            source->triggerRunning = false;
                            remainingSeconds = 0.0;
                            break;

                        case ModulationSource::EnvelopeStage::attack:
                        {
                            const auto stageRemaining = juce::jmax (
                                0.0,
                                static_cast<double> (attackSeconds)
                                    - source->envelopeStageSeconds);
                            const auto consumed =
                                juce::jmin (
                                    remainingSeconds, stageRemaining);
                            source->envelopeStageSeconds += consumed;
                            remainingSeconds -= consumed;
                            source->envelopeValue =
                                juce::jlimit (
                                    0.0f,
                                    1.0f,
                                    static_cast<float> (
                                        source->envelopeStageSeconds
                                        / attackSeconds));
                            const auto attackProgress =
                                source->envelopeValue;
                            source->envelopeBipolarValue =
                                juce::jmap (
                                    attackProgress,
                                    source->envelopeBipolarStageStartValue,
                                    1.0f);

                            if (source->envelopeStageSeconds
                                + 0.000000001
                                < attackSeconds)
                                remainingSeconds = 0.0;
                            else
                            {
                                source->envelopeValue = 1.0f;
                                source->envelopeBipolarValue = 1.0f;
                                source->envelopeBipolarStageStartValue = 1.0f;
                                source->envelopeStageSeconds = 0.0;
                                source->envelopeStage =
                                    ModulationSource::EnvelopeStage::decay;
                            }
                            break;
                        }

                        case ModulationSource::EnvelopeStage::decay:
                        {
                            const auto stageRemaining = juce::jmax (
                                0.0,
                                static_cast<double> (decaySeconds)
                                    - source->envelopeStageSeconds);
                            const auto consumed =
                                juce::jmin (
                                    remainingSeconds, stageRemaining);
                            source->envelopeStageSeconds += consumed;
                            remainingSeconds -= consumed;
                            const auto progress = juce::jlimit (
                                0.0f,
                                1.0f,
                                static_cast<float> (
                                    source->envelopeStageSeconds
                                    / decaySeconds));
                            source->envelopeValue =
                                1.0f + (sustain - 1.0f) * progress;
                            source->envelopeBipolarValue = 1.0f - progress;

                            if (source->envelopeStageSeconds
                                + 0.000000001
                                < decaySeconds)
                                remainingSeconds = 0.0;
                            else
                            {
                                source->envelopeValue = sustain;
                                source->envelopeBipolarValue = 0.0f;
                                source->envelopeBipolarStageStartValue = 0.0f;
                                source->envelopeStageSeconds = 0.0;
                                source->envelopeStage =
                                    ModulationSource::EnvelopeStage::sustain;
                            }
                            break;
                        }

                        case ModulationSource::EnvelopeStage::sustain:
                            source->envelopeValue = sustain;
                            source->envelopeBipolarValue = 0.0f;
                            if (source->envelopeGateHeld)
                            {
                                remainingSeconds = 0.0;
                                break;
                            }

                            source->envelopeReleaseStartValue = sustain;
                            source->envelopeBipolarStageStartValue = 0.0f;
                            source->envelopeStageSeconds = 0.0;
                            source->envelopeStage =
                                ModulationSource::EnvelopeStage::release;
                            break;

                        case ModulationSource::EnvelopeStage::release:
                        {
                            const auto stageRemaining = juce::jmax (
                                0.0,
                                static_cast<double> (releaseSeconds)
                                    - source->envelopeStageSeconds);
                            const auto consumed =
                                juce::jmin (
                                    remainingSeconds, stageRemaining);
                            source->envelopeStageSeconds += consumed;
                            remainingSeconds -= consumed;
                            const auto progress = juce::jlimit (
                                0.0f,
                                1.0f,
                                static_cast<float> (
                                    source->envelopeStageSeconds
                                    / releaseSeconds));
                            source->envelopeValue =
                                source->envelopeReleaseStartValue
                                * (1.0f - progress);
                            const auto halfProgress =
                                juce::jlimit (
                                    0.0f,
                                    1.0f,
                                    progress <= 0.5f
                                        ? progress * 2.0f
                                        : (progress - 0.5f) * 2.0f);
                            const auto smoothedProgress =
                                halfProgress * halfProgress
                                * (3.0f - 2.0f * halfProgress);
                            source->envelopeBipolarValue =
                                progress <= 0.5f
                                    ? juce::jmap (
                                          smoothedProgress,
                                          source
                                              ->envelopeBipolarStageStartValue,
                                          -1.0f)
                                    : -1.0f + smoothedProgress;

                            if (source->envelopeStageSeconds
                                + 0.000000001
                                < releaseSeconds)
                                remainingSeconds = 0.0;
                            else
                            {
                                source->envelopeValue = 0.0f;
                                source->envelopeBipolarValue = 0.0f;
                                source->envelopeBipolarStageStartValue = 0.0f;
                                source->envelopeStageSeconds = 0.0;
                                source->envelopeStage =
                                    ModulationSource::EnvelopeStage::idle;
                                source->triggerRunning = false;
                            }
                            break;
                        }
                    }
                }

                unipolarOutput = source->envelopeValue;
                break;
            }

            case ModulationType::audioFollow:
            {
                const auto attackSeconds =
                    juce::jmax (0.001f, primary * 0.001f);
                const auto releaseSeconds =
                    juce::jmax (0.001f, secondary * 0.001f);
                const auto time = inputPeak > source->followerValue
                                      ? attackSeconds
                                      : releaseSeconds;
                const auto coefficient =
                    static_cast<float> (
                        std::exp (-blockSeconds / time));
                source->followerValue =
                    inputPeak
                    + coefficient * (source->followerValue - inputPeak);
                unipolarOutput = juce::jlimit (
                    0.0f, 1.0f, source->followerValue);
                break;
            }

            case ModulationType::random:
            {
                const auto rate = juce::jlimit (0.01f, 40.0f, primary);
                const auto eventsPerCycle =
                    juce::jlimit (0.0f, 8.0f, jitter);
                const auto range =
                    juce::jlimit (0.0f, 1.0f, jitterRange);
                const auto smooth =
                    juce::jlimit (0.0f, 1.0f, secondary);

                const auto advanceTransition =
                    [&] (double elapsedCycles)
                {
                    if (source->randomTransitionPhase >= 1.0)
                        return;

                    if (smooth <= 0.0001f)
                    {
                        source->randomTransitionPhase = 1.0;
                        source->randomValue = source->randomTarget;
                        return;
                    }

                    source->randomTransitionPhase = juce::jmin (
                        1.0,
                        source->randomTransitionPhase
                            + elapsedCycles
                                / static_cast<double> (smooth));
                    const auto phase = static_cast<float> (
                        source->randomTransitionPhase);
                    const auto interpolation =
                        phase * phase * (3.0f - 2.0f * phase);
                    source->randomValue = juce::jmap (
                        interpolation,
                        source->randomTransitionStart,
                        source->randomTarget);
                };

                const auto beginTransition =
                    [&] (float nextTarget)
                {
                    source->randomTransitionStart = source->randomValue;
                    source->randomTarget =
                        juce::jlimit (-1.0f, 1.0f, nextTarget);
                    source->randomTransitionPhase = 0.0;
                    if (smooth <= 0.0001f)
                    {
                        source->randomTransitionPhase = 1.0;
                        source->randomValue = source->randomTarget;
                    }
                };

                auto remainingCycles =
                    static_cast<double> (rate) * blockSeconds;
                constexpr auto eventEpsilon = 0.000000001;
                constexpr auto maximumJitterEventsPerBlock = 128;
                auto jitterEventsProcessed = 0;
                while (remainingCycles > eventEpsilon)
                {
                    const auto canProcessJitter =
                        eventsPerCycle > 0.0001f
                        && jitterEventsProcessed
                               < maximumJitterEventsPerBlock;
                    const auto cyclesToBaseEvent =
                        juce::jmax (0.0, 1.0 - source->phase);
                    const auto cyclesToJitterEvent =
                        canProcessJitter
                            ? juce::jmax (
                                  0.0,
                                  source->randomJitterHazardRemaining
                                      / static_cast<double> (
                                          eventsPerCycle))
                            : std::numeric_limits<double>::infinity();
                    const auto elapsedCycles =
                        juce::jmin (
                            remainingCycles,
                            juce::jmin (
                                cyclesToBaseEvent,
                                cyclesToJitterEvent));

                    advanceTransition (elapsedCycles);
                    source->phase += elapsedCycles;
                    if (eventsPerCycle > 0.0001f)
                        source->randomJitterHazardRemaining -=
                            elapsedCycles
                            * static_cast<double> (eventsPerCycle);
                    remainingCycles -= elapsedCycles;

                    const auto baseEventDue =
                        source->phase >= 1.0 - eventEpsilon;
                    const auto jitterEventDue =
                        canProcessJitter
                        && source->randomJitterHazardRemaining
                               <= eventEpsilon;
                    if (! baseEventDue && ! jitterEventDue)
                        break;

                    if (baseEventDue)
                    {
                        source->phase = 0.0;
                        beginTransition (
                            advanceBipolarRandom (
                                source->randomState));
                    }

                    if (jitterEventDue)
                    {
                        ++jitterEventsProcessed;
                        source->randomJitterHazardRemaining +=
                            advanceExponentialRandom (
                                source->randomJitterState);
                        if (range > 0.0001f)
                        {
                            const auto minimumDelta = juce::jmax (
                                -range,
                                -1.0f - source->randomValue);
                            const auto maximumDelta = juce::jmin (
                                range,
                                1.0f - source->randomValue);
                            const auto randomPosition =
                                advanceBipolarRandom (
                                    source->randomJitterState)
                                    * 0.5f
                                + 0.5f;
                            const auto nextTarget =
                                source->randomValue
                                + juce::jmap (
                                    randomPosition,
                                    minimumDelta,
                                    maximumDelta);
                            if (std::abs (
                                    nextTarget - source->randomValue)
                                > 0.000001f)
                                beginTransition (nextTarget);
                        }
                    }
                }

                displayPhase = static_cast<float> (source->phase);
                unipolarOutput =
                    juce::jlimit (
                        -1.0f, 1.0f, source->randomValue)
                        * 0.5f
                    + 0.5f;
                break;
            }
        }

        source->displayStateVersion.fetch_add (
            1, std::memory_order_seq_cst);
        source->effectiveSecondary.store (
            secondary, std::memory_order_relaxed);
        source->lfoRandomStart.store (
            source->randomValue, std::memory_order_relaxed);
        source->lfoRandomEnd.store (
            source->randomTarget, std::memory_order_relaxed);
        const auto polarity = source->polarity.load (
            std::memory_order_relaxed);
        const auto output =
            source->type == ModulationType::envelope
                ? polarity == ModulationPolarity::positive
                      ? unipolarOutput
                      : polarity == ModulationPolarity::inverted
                            ? -unipolarOutput
                            : juce::jlimit (
                                  -1.0f,
                                  1.0f,
                                  source->envelopeBipolarValue)
                : source->type == ModulationType::audioFollow
                          && polarity == ModulationPolarity::inverted
                      ? -unipolarOutput
                      : applyModulationPolarity (unipolarOutput, polarity);
        source->output.store (
            output,
            std::memory_order_relaxed);
        source->displayPhase.store (
            displayPhase, std::memory_order_relaxed);
        source->displayStateVersion.fetch_add (
            1, std::memory_order_seq_cst);
    }

        for (const auto& link : modulationRuntime->controlLinks)
        {
            if (link->target != ModulationControlTarget::trigger
                || ! link->gateResyncPending)
                continue;

            const auto gateValue =
                modulationOutputToNormalisedForTrigger (
                    link->source->type,
                    link->source->polarity.load (
                        std::memory_order_relaxed),
                    link->source->output.load (
                        std::memory_order_relaxed));
            link->triggerGateHigh = gateValue >= 0.55f;
            link->gateResyncPending = false;
        }

        targetCount = modulationRuntime->targets.size();
    }

    for (size_t targetIndex = 0;
         targetIndex < targetCount;
         ++targetIndex)
    {
        ModulationTarget* targetToApply = nullptr;
        auto valueToApply = 0.0f;
        {
            const juce::SpinLock::ScopedTryLockType lock (
                modulationRuntimeLock);
            if (! lock.isLocked()
                || modulationRuntime == nullptr
                || targetIndex >= modulationRuntime->targets.size())
                continue;

            const auto& entry =
                modulationRuntime->targets[targetIndex];
            const auto& target = entry.target;
            if (target->userEditing.load (std::memory_order_acquire))
                continue;

            const auto holdBlocks =
                target->holdBlocks.load (std::memory_order_relaxed);
            if (holdBlocks > 0)
            {
                target->holdBlocks.store (
                    holdBlocks - 1, std::memory_order_relaxed);
                continue;
            }

            auto modulation = 0.0f;
            for (const auto& link : entry.links)
                modulation +=
                    link->source->output.load (std::memory_order_relaxed)
                    * link->amount.load (std::memory_order_relaxed);

            const auto value = juce::jlimit (
                0.0f,
                1.0f,
                target->baseValue.load (std::memory_order_relaxed)
                    + modulation);
            const auto lastValue =
                target->lastAppliedValue.load (std::memory_order_relaxed);
            if (std::abs (value - lastValue) < 0.000001f)
                continue;

            modulationTargetApplyReaders.fetch_add (
                1, std::memory_order_acq_rel);
            targetToApply = target.get();
            valueToApply = value;
        }

        if (targetToApply != nullptr)
        {
            setModulationTargetValue (targetToApply, valueToApply);
            if (modulationTargetApplyReaders.fetch_sub (
                    1, std::memory_order_acq_rel)
                    == 1
                && retiredModulationRuntimeReapPending.load (
                    std::memory_order_acquire))
                triggerAsyncUpdate();
        }
    }
}

void BoardAudioEngine::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto* messageManager = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (messageManager->isThisTheMessageThread()
            && stateSnapshotPending.exchange (false, std::memory_order_acq_rel))
            rebuildStateCache();
    }

    const juce::ScopedReadLock lock (stateCacheLock);
    destination = cachedState;
}

void BoardAudioEngine::setStateInformation (const void* data, int sizeInBytes)
{
    if (data == nullptr
        || sizeInBytes <= 0
        || static_cast<size_t> (sizeInBytes) > maximumBoardStateBytes)
        return;

    externalStateRestorePending.store (true, std::memory_order_release);
    historyOperationSerial.fetch_add (1, std::memory_order_acq_rel);
    const auto generation =
        restoreGeneration.fetch_add (1, std::memory_order_relaxed) + 1;
    auto state = std::make_shared<juce::MemoryBlock> (
        data, static_cast<size_t> (sizeInBytes));
    const auto gate = callbackGate;

    const auto restore = [gate, state, generation]
    {
        const juce::ScopedLock gateLock (gate->lock);
        if (gate->owner != nullptr)
            gate->owner->restoreStateOnMessageThread (*state, generation);
    };

    if (auto* messageManager = juce::MessageManager::getInstanceWithoutCreating())
    {
        if (messageManager->isThisTheMessageThread())
        {
            restore();
            return;
        }
    }

    juce::MessageManager::callAsync (restore);
}

void BoardAudioEngine::rebuildStateCache()
{
    auto xml = std::make_unique<juce::XmlElement> (boardStateTag);
    xml->setAttribute ("schemaVersion", boardStateSchemaVersion);
    xml->setAttribute ("productVersion", ProjectInfo::versionString);
    xml->setAttribute ("outputInputGainDb", outputInputGainDb);

    const juce::ScopedLock engineCallbackLock (getCallbackLock());

    for (const auto& card : inputCards)
    {
        auto cardXml = std::make_unique<juce::XmlElement> (inputCardTag);
        cardXml->setAttribute ("sourceIndex", card.sourceIndex);
        cardXml->setAttribute ("x", card.position.x);
        cardXml->setAttribute ("y", card.position.y);
        cardXml->setAttribute ("outputGainDb", card.outputGainDb);
        xml->addChildElement (cardXml.release());
    }

    for (auto* node : graph.getNodes())
    {
        if (isIONode (node->nodeID))
            continue;

        auto* processor = node->getProcessor();
        juce::PluginDescription description;
        auto dualMono = false;

        if (! getPluginDescriptionForProcessor (
                *processor, description, dualMono))
            continue;

        auto nodeXml = std::make_unique<juce::XmlElement> (nodeTag);
        nodeXml->setAttribute ("id", juce::String (node->nodeID.uid));
        nodeXml->setAttribute ("x", static_cast<int> (node->properties[nodeXKey]));
        nodeXml->setAttribute ("y", static_cast<int> (node->properties[nodeYKey]));
        nodeXml->setAttribute ("bypassed", node->isBypassed());
        nodeXml->setAttribute (
            "inputGainDb",
            getNodeLevelDb (node->nodeID, NodeLevelStage::input));
        nodeXml->setAttribute (
            "outputGainDb",
            getNodeLevelDb (node->nodeID, NodeLevelStage::output));
        nodeXml->setAttribute (
            "mode", dualMono ? "linkedDualMono" : "nativeStereo");

        nodeXml->addChildElement (description.createXml().release());

        juce::MemoryBlock childState;
        {
            const juce::ScopedLock processorLock (processor->getCallbackLock());
            processor->getStateInformation (childState);
        }

        auto stateXml = std::make_unique<juce::XmlElement> (childStateTag);
        stateXml->addTextElement (childState.toBase64Encoding());
        nodeXml->addChildElement (stateXml.release());
        xml->addChildElement (nodeXml.release());
    }

    const auto writeConnection = [&xml] (const auto& connection)
    {
        auto connectionXml =
            std::make_unique<juce::XmlElement> (connectionTag);
        connectionXml->setAttribute (
            "sourceNode", juce::String (connection.source.nodeID.uid));
        connectionXml->setAttribute (
            "sourceChannel", connection.source.channelIndex);
        connectionXml->setAttribute (
            "destinationNode",
            juce::String (connection.destination.nodeID.uid));
        connectionXml->setAttribute (
            "destinationChannel", connection.destination.channelIndex);
        xml->addChildElement (connectionXml.release());
    };

    std::set<juce::uint32> writtenInputDestinations;
    for (const auto& connection : graph.getConnections())
    {
        if (connection.source.isMIDI() || connection.destination.isMIDI()
            || isInternalConnection (connection))
            continue;

        if (connection.source.nodeID != inputNodeID)
        {
            writeConnection (connection);
            continue;
        }

        if (! writtenInputDestinations.insert (
                 connection.destination.nodeID.uid).second)
            continue;

        const auto sourceIndex = getInputSourceForDestination (
            connection.destination.nodeID);
        if (! sourceIndex.has_value())
            continue;

        std::vector<juce::AudioProcessorGraph::Connection> logicalRoute;
        if (*sourceIndex == stereoInputSource)
            logicalRoute = {
                { { inputNodeID, 0 }, { connection.destination.nodeID, 0 } },
                { { inputNodeID, 1 }, { connection.destination.nodeID, 1 } }
            };
        else
        {
            const auto legacyChannel =
                *sourceIndex == firstMonoInputSource ? 0 : 1;
            logicalRoute = {
                { { inputNodeID, legacyChannel },
                  { connection.destination.nodeID, 0 } },
                { { inputNodeID, legacyChannel },
                  { connection.destination.nodeID, 1 } }
            };
        }

        for (const auto& logicalConnection : logicalRoute)
            writeConnection (logicalConnection);
    }

    if (! modulationSources.empty())
    {
        auto modulationXml =
            std::make_unique<juce::XmlElement> (modulationTag);

        for (const auto& source : modulationSources)
        {
            auto sourceXml =
                std::make_unique<juce::XmlElement> (modulationSourceTag);
            sourceXml->setAttribute ("id", juce::String (source->id));
            sourceXml->setAttribute (
                "type", modulationTypeToString (source->type));
            if (source->type == ModulationType::lfo)
                sourceXml->setAttribute (
                    "waveform",
                    lfoWaveformToString (
                        source->lfoWaveform.load (
                            std::memory_order_relaxed)));
            sourceXml->setAttribute (
                "polarity",
                modulationPolarityToString (
                    source->polarity.load (
                        std::memory_order_relaxed)));
            sourceXml->setAttribute (
                "primary",
                static_cast<double> (
                    source->primary.load (std::memory_order_relaxed)));
            sourceXml->setAttribute (
                "secondary",
                static_cast<double> (
                    source->secondary.load (std::memory_order_relaxed)));
            if (source->type == ModulationType::envelope)
            {
                sourceXml->setAttribute (
                    "envelopeDecay",
                    static_cast<double> (
                        source->envelopeDecay.load (
                            std::memory_order_relaxed)));
                sourceXml->setAttribute (
                    "envelopeSustain",
                    static_cast<double> (
                        source->envelopeSustain.load (
                            std::memory_order_relaxed)));
            }
            sourceXml->setAttribute (
                "jitter",
                static_cast<double> (
                    source->jitter.load (std::memory_order_relaxed)));
            sourceXml->setAttribute (
                "jitterRange",
                static_cast<double> (
                    source->jitterRange.load (
                        std::memory_order_relaxed)));
            sourceXml->setAttribute (
                "autoTrigger",
                source->autoTrigger.load (std::memory_order_relaxed));

            if (source->curveState != nullptr)
            {
                const auto& curve = *source->curveState;
                sourceXml->setAttribute (
                    "loopMode", curveLoopModeToString (curve.loopMode));
                sourceXml->setAttribute (
                    "loopStart", static_cast<double> (curve.loopStart));
                sourceXml->setAttribute (
                    "loopEnd", static_cast<double> (curve.loopEnd));

                for (const auto& point : curve.points)
                {
                    auto pointXml =
                        std::make_unique<juce::XmlElement> (
                            modulationCurvePointTag);
                    pointXml->setAttribute (
                        "x", static_cast<double> (point.x));
                    pointXml->setAttribute (
                        "y", static_cast<double> (point.y));
                    if (point.leftPadding > 0.0f)
                        pointXml->setAttribute (
                            "leftPadding",
                            static_cast<double> (point.leftPadding));
                    if (point.rightPadding > 0.0f)
                        pointXml->setAttribute (
                            "rightPadding",
                            static_cast<double> (point.rightPadding));
                    sourceXml->addChildElement (pointXml.release());
                }

                for (int index = 0; index < 8; ++index)
                {
                    const auto x = static_cast<double> (index) / 7.0;
                    sourceXml->setAttribute (
                        "point" + juce::String (index),
                        static_cast<double> (
                            sampleCurvePoints (curve.points, x)));
                }
            }

            modulationXml->addChildElement (sourceXml.release());
        }

        for (const auto& link : modulationLinks)
        {
            auto linkXml =
                std::make_unique<juce::XmlElement> (modulationLinkTag);
            linkXml->setAttribute ("id", juce::String (link->id));
            linkXml->setAttribute (
                "source", juce::String (link->source->id));
            linkXml->setAttribute (
                "node", juce::String (link->target->node->nodeID.uid));
            linkXml->setAttribute (
                "parameterID", link->target->parameterID);
            linkXml->setAttribute (
                "parameterIndex", link->target->parameterIndex);
            linkXml->setAttribute (
                "parameterName", link->target->parameterName);
            linkXml->setAttribute (
                "base",
                static_cast<double> (
                    link->target->baseValue.load (
                        std::memory_order_relaxed)));
            linkXml->setAttribute (
                "amount",
                static_cast<double> (
                    link->amount.load (std::memory_order_relaxed)));
            modulationXml->addChildElement (linkXml.release());
        }

        for (const auto& link : modulationControlLinks)
        {
            auto linkXml =
                std::make_unique<juce::XmlElement> (
                    modulationControlLinkTag);
            linkXml->setAttribute ("id", juce::String (link->id));
            linkXml->setAttribute (
                "source", juce::String (link->source->id));
            linkXml->setAttribute (
                "targetSource",
                juce::String (link->targetSource->id));
            linkXml->setAttribute (
                "control",
                modulationControlTargetToString (link->target));
            if (link->target != ModulationControlTarget::trigger)
                linkXml->setAttribute (
                    "amount",
                    static_cast<double> (
                        link->amount.load (
                            std::memory_order_relaxed)));
            modulationXml->addChildElement (linkXml.release());
        }

        xml->addChildElement (modulationXml.release());
    }

    juce::MemoryBlock newState;
    juce::AudioProcessor::copyXmlToBinary (*xml, newState);

    const juce::ScopedWriteLock lock (stateCacheLock);
    cachedState = std::move (newState);
}

void BoardAudioEngine::restoreStateOnMessageThread (
    const juce::MemoryBlock& state,
    juce::uint64 generation)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (generation != restoreGeneration.load (std::memory_order_relaxed))
        return;

    clearHistory();

    const auto xml = juce::AudioProcessor::getXmlFromBinary (
        state.getData(), static_cast<int> (state.getSize()));

    if (xml == nullptr || ! xml->hasTagName (boardStateTag))
    {
        failStateRestore (generation, "invalid state envelope");
        return;
    }

    const auto stateSchemaVersion =
        xml->getIntAttribute ("schemaVersion");
    if (stateSchemaVersion < oldestSupportedBoardStateSchemaVersion
        || stateSchemaVersion > boardStateSchemaVersion)
    {
        failStateRestore (generation, "unsupported schema version");
        return;
    }

    auto plan = std::make_shared<RestorePlan>();
    plan->generation = generation;
    if (stateSchemaVersion >= 4)
        plan->outputInputGainDb = sanitiseLevelDb (
            static_cast<float> (
                xml->getDoubleAttribute ("outputInputGainDb", 0.0)));
    std::set<juce::uint32> nodeIDs { inputNodeUID, outputNodeUID };
    std::set<std::tuple<juce::uint32, int, juce::uint32, int>> uniqueConnections;
    std::map<std::pair<juce::uint32, juce::uint32>,
             std::set<std::pair<int, int>>> audioRouteChannels;
    std::set<int> inputCardSourceIndices;
    std::set<ModulationID> modulationSourceIDs;
    std::map<ModulationID, ModulationType> modulationSourceTypes;
    std::set<ModulationID> modulationLinkIDs;
    std::set<std::tuple<ModulationID,
                        ModulationID,
                        ModulationControlTarget>>
        modulationControlRoutes;

    for (auto* child = xml->getFirstChildElement();
         child != nullptr;
         child = child->getNextElement())
    {
        if (child->hasTagName (inputCardTag))
        {
            const auto sourceIndex =
                child->getIntAttribute ("sourceIndex", -1);
            const auto card = makeInputCardInfo (
                sourceIndex,
                { child->getIntAttribute ("x"),
                  child->getIntAttribute ("y") });
            if (stateSchemaVersion < 3
                || ! card.has_value()
                || ! inputCardSourceIndices.insert (sourceIndex).second)
            {
                failStateRestore (
                    generation, "invalid or duplicate input card");
                return;
            }

            auto restoredCard = *card;
            if (stateSchemaVersion >= 4)
                restoredCard.outputGainDb = sanitiseLevelDb (
                    static_cast<float> (child->getDoubleAttribute (
                        "outputGainDb", 0.0)));
            plan->inputCards.push_back (std::move (restoredCard));
        }
        else if (child->hasTagName (nodeTag))
        {
            if (static_cast<int> (plan->nodes.size()) >= maximumRestoredNodes)
            {
                failStateRestore (
                    generation, "node count exceeds the safety limit");
                return;
            }

            RestorePlan::Node savedNode;
            const auto uid = readNodeUID (*child, "id");

            if (uid <= outputNodeUID || ! nodeIDs.insert (uid).second)
            {
                failStateRestore (
                    generation, "invalid or duplicate node id");
                return;
            }

            savedNode.nodeID = NodeID { uid };
            savedNode.position = {
                child->getIntAttribute ("x"),
                child->getIntAttribute ("y")
            };
            savedNode.bypassed = child->getBoolAttribute ("bypassed");
            if (stateSchemaVersion >= 4)
            {
                savedNode.inputGainDb = sanitiseLevelDb (
                    static_cast<float> (child->getDoubleAttribute (
                        "inputGainDb", 0.0)));
                savedNode.outputGainDb = sanitiseLevelDb (
                    static_cast<float> (child->getDoubleAttribute (
                        "outputGainDb", 0.0)));
            }

            const auto mode = child->getStringAttribute ("mode");
            if (mode == "nativeStereo")
                savedNode.mode = InstanceMode::nativeStereo;
            else if (mode == "linkedDualMono")
                savedNode.mode = InstanceMode::linkedDualMono;
            else
            {
                failStateRestore (generation, "unknown instance mode");
                return;
            }

            auto* descriptionXml = child->getChildByName ("PLUGIN");
            if (descriptionXml == nullptr
                || ! savedNode.description.loadFromXml (*descriptionXml)
                || isTBBoardDescription (savedNode.description))
            {
                failStateRestore (
                    generation,
                    "invalid, missing, or recursive plug-in description");
                return;
            }

            if (auto* childState = child->getChildByName (childStateTag))
            {
                if (! savedNode.childState.fromBase64Encoding (
                        childState->getAllSubText())
                    || savedNode.childState.getSize() > maximumChildStateBytes)
                {
                    failStateRestore (
                        generation, "invalid or oversized child state");
                    return;
                }
            }

            plan->nodes.push_back (std::move (savedNode));
        }
        else if (child->hasTagName (connectionTag))
        {
            if (static_cast<int> (plan->connections.size())
                >= maximumRestoredConnections)
            {
                failStateRestore (
                    generation,
                    "connection count exceeds the safety limit");
                return;
            }

            const auto sourceUID = readNodeUID (*child, "sourceNode");
            const auto destinationUID = readNodeUID (*child, "destinationNode");
            const auto sourceChannel =
                child->getIntAttribute ("sourceChannel", -1);
            const auto destinationChannel =
                child->getIntAttribute ("destinationChannel", -1);

            if (sourceUID == 0
                || destinationUID == 0
                || sourceUID == destinationUID
                || sourceUID == outputNodeUID
                || destinationUID == inputNodeUID
                || sourceChannel < 0
                || sourceChannel > 1
                || destinationChannel < 0
                || destinationChannel > 1
                || (sourceUID != inputNodeUID
                    && sourceChannel != destinationChannel))
            {
                failStateRestore (generation, "invalid stereo connection");
                return;
            }

            const auto key = std::make_tuple (
                sourceUID, sourceChannel, destinationUID, destinationChannel);
            if (! uniqueConnections.insert (key).second)
            {
                failStateRestore (generation, "duplicate connection");
                return;
            }

            audioRouteChannels[{ sourceUID, destinationUID }].insert (
                { sourceChannel, destinationChannel });
            plan->connections.push_back (
                { { NodeID { sourceUID }, sourceChannel },
                  { NodeID { destinationUID }, destinationChannel } });
        }
        else if (child->hasTagName (modulationTag))
        {
            for (auto* modulationChild = child->getFirstChildElement();
                 modulationChild != nullptr;
                 modulationChild = modulationChild->getNextElement())
            {
                if (modulationChild->hasTagName (modulationSourceTag))
                {
                    const auto sourceID =
                        readModulationID (*modulationChild, "id");
                    const auto type = modulationTypeFromString (
                        modulationChild->getStringAttribute ("type"));
                    if (sourceID == 0
                        || ! type.has_value()
                        || ! modulationSourceIDs.insert (sourceID).second)
                        continue;

                    RestorePlan::ModulationSourceState sourceState;
                    sourceState.id = sourceID;
                    sourceState.type = *type;
                    if (*type == ModulationType::lfo)
                        sourceState.lfoWaveform =
                            modulationChild->hasAttribute ("waveform")
                                ? lfoWaveformFromString (
                                      modulationChild
                                          ->getStringAttribute (
                                              "waveform"))
                                      .value_or (LfoWaveform::sine)
                                : LfoWaveform::legacyMorph;
                    sourceState.polarity =
                        modulationChild->hasAttribute ("polarity")
                            ? modulationPolarityFromString (
                                  modulationChild->getStringAttribute (
                                      "polarity"))
                                  .value_or (
                                      getDefaultModulationPolarity (
                                          *type))
                            : getDefaultModulationPolarity (*type);
                    const auto primary =
                        modulationChild->hasAttribute ("primary")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "primary"))
                            : getDefaultModulationPrimary (*type);
                    const auto secondary =
                        modulationChild->hasAttribute ("secondary")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "secondary"))
                            : getDefaultModulationSecondary (*type);
                    const auto jitter =
                        modulationChild->hasAttribute ("jitter")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "jitter"))
                            : 0.0f;
                    const auto jitterRange =
                        modulationChild->hasAttribute ("jitterRange")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "jitterRange"))
                            : 0.25f;
                    const auto envelopeDecay =
                        modulationChild->hasAttribute (
                            "envelopeDecay")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "envelopeDecay"))
                            : 200.0f;
                    const auto envelopeSustain =
                        modulationChild->hasAttribute (
                            "envelopeSustain")
                            ? static_cast<float> (
                                  modulationChild->getDoubleAttribute (
                                      "envelopeSustain"))
                            : 0.70f;
                    sourceState.primary = clampModulationPrimary (
                        *type,
                        std::isfinite (primary)
                            ? primary
                            : getDefaultModulationPrimary (*type));
                    sourceState.secondary = clampModulationSecondary (
                        *type,
                        std::isfinite (secondary)
                            ? secondary
                            : getDefaultModulationSecondary (*type));
                    sourceState.jitter = clampModulationJitter (
                        std::isfinite (jitter) ? jitter : 0.0f);
                    sourceState.jitterRange =
                        clampModulationJitterRange (
                            std::isfinite (jitterRange)
                                ? jitterRange
                                : 0.25f);
                    sourceState.envelopeDecay =
                        clampModulationEnvelopeDecay (
                            std::isfinite (envelopeDecay)
                                ? envelopeDecay
                                : 200.0f);
                    sourceState.envelopeSustain =
                        clampModulationEnvelopeSustain (
                            std::isfinite (envelopeSustain)
                                ? envelopeSustain
                                : 0.70f);
                    sourceState.autoTrigger =
                        modulationChild->getBoolAttribute (
                            "autoTrigger", true);

                    auto legacyPoints = makeDefaultCurvePoints();
                    for (size_t point = 0; point < legacyPoints.size(); ++point)
                    {
                        const auto defaultValue =
                            legacyPoints[point].y;
                        const auto attribute =
                            "point"
                            + juce::String (static_cast<int> (point));
                        const auto value =
                            modulationChild->hasAttribute (attribute)
                                ? static_cast<float> (
                                      modulationChild->getDoubleAttribute (
                                          attribute))
                                : defaultValue;
                        legacyPoints[point].y =
                            std::isfinite (value)
                                ? juce::jlimit (0.0f, 1.0f, value)
                                : defaultValue;
                    }

                    std::vector<CurvePoint> encodedPoints;
                    auto encodedPointsValid = true;
                    for (auto* pointXml =
                             modulationChild->getFirstChildElement();
                         pointXml != nullptr;
                         pointXml = pointXml->getNextElement())
                    {
                        if (! pointXml->hasTagName (
                                modulationCurvePointTag))
                            continue;

                        if (encodedPoints.size()
                            >= maximumModulationCurvePoints)
                        {
                            encodedPointsValid = false;
                            break;
                        }

                        const auto x = static_cast<float> (
                            pointXml->getDoubleAttribute ("x"));
                        const auto y = static_cast<float> (
                            pointXml->getDoubleAttribute ("y"));
                        const auto leftPadding = static_cast<float> (
                            pointXml->getDoubleAttribute (
                                "leftPadding", 0.0));
                        const auto rightPadding = static_cast<float> (
                            pointXml->getDoubleAttribute (
                                "rightPadding", 0.0));
                        if (! std::isfinite (x)
                            || ! std::isfinite (y)
                            || ! std::isfinite (leftPadding)
                            || ! std::isfinite (rightPadding))
                        {
                            encodedPointsValid = false;
                            break;
                        }
                        encodedPoints.push_back ({
                            x, y, leftPadding, rightPadding
                        });
                    }

                    sourceState.curvePoints = std::move (legacyPoints);
                    if (encodedPointsValid && ! encodedPoints.empty())
                        if (const auto normalised =
                                normaliseCurvePoints (encodedPoints))
                            sourceState.curvePoints = *normalised;

                    sourceState.loopMode =
                        curveLoopModeFromString (
                            modulationChild->getStringAttribute (
                                "loopMode"))
                            .value_or (CurveLoopMode::fullLoop);
                    const auto loopStart = static_cast<float> (
                        modulationChild->getDoubleAttribute (
                            "loopStart", 0.25));
                    const auto loopEnd = static_cast<float> (
                        modulationChild->getDoubleAttribute (
                            "loopEnd", 0.75));
                    if (const auto curve = createCurveState (
                            sourceState.curvePoints,
                            sourceState.loopMode,
                            loopStart,
                            loopEnd))
                    {
                        sourceState.curvePoints = curve->points;
                        sourceState.loopMode = curve->loopMode;
                        sourceState.loopStart = curve->loopStart;
                        sourceState.loopEnd = curve->loopEnd;
                    }

                    plan->modulationSources.push_back (
                        std::move (sourceState));
                    modulationSourceTypes[sourceID] = *type;
                }
                else if (modulationChild->hasTagName (modulationLinkTag))
                {
                    RestorePlan::ModulationLinkState linkState;
                    linkState.id =
                        readModulationID (*modulationChild, "id");
                    linkState.sourceID =
                        readModulationID (*modulationChild, "source");
                    linkState.nodeID = NodeID {
                        readNodeUID (*modulationChild, "node")
                    };
                    linkState.parameterID =
                        modulationChild->getStringAttribute ("parameterID");
                    linkState.parameterName =
                        modulationChild->getStringAttribute (
                            "parameterName");
                    linkState.parameterIndex =
                        modulationChild->getIntAttribute (
                            "parameterIndex", -1);
                    const auto base = static_cast<float> (
                        modulationChild->getDoubleAttribute ("base", 0.0));
                    const auto amount = static_cast<float> (
                        modulationChild->getDoubleAttribute (
                            "amount", 1.0));
                    linkState.baseValue =
                        std::isfinite (base)
                            ? juce::jlimit (0.0f, 1.0f, base)
                            : 0.0f;
                    linkState.amount =
                        std::isfinite (amount)
                            ? juce::jlimit (-1.0f, 1.0f, amount)
                            : 1.0f;

                    if (linkState.id == 0
                        || linkState.sourceID == 0
                        || linkState.nodeID.uid <= outputNodeUID
                        || linkState.parameterIndex < 0
                        || ! modulationLinkIDs.insert (
                                linkState.id).second)
                        continue;

                    plan->modulationLinks.push_back (
                        std::move (linkState));
                }
                else if (modulationChild->hasTagName (
                             modulationControlLinkTag))
                {
                    RestorePlan::ModulationControlLinkState linkState;
                    linkState.id =
                        readModulationID (*modulationChild, "id");
                    linkState.sourceID =
                        readModulationID (*modulationChild, "source");
                    linkState.targetSourceID =
                        readModulationID (
                            *modulationChild, "targetSource");
                    const auto target =
                        modulationControlTargetFromString (
                            modulationChild->getStringAttribute (
                                "control"));
                    const auto amount = static_cast<float> (
                        modulationChild->getDoubleAttribute (
                            "amount", 1.0));
                    linkState.amount =
                        std::isfinite (amount)
                            ? juce::jlimit (-1.0f, 1.0f, amount)
                            : 1.0f;

                    if (linkState.id == 0
                        || linkState.sourceID == 0
                        || linkState.targetSourceID == 0
                        || linkState.sourceID == linkState.targetSourceID
                        || ! target.has_value()
                        || ! modulationLinkIDs.insert (
                                linkState.id).second)
                        continue;

                    linkState.target = *target;
                    if (linkState.target == ModulationControlTarget::trigger)
                        linkState.amount = 1.0f;
                    if (! modulationControlRoutes.insert (
                             { linkState.sourceID,
                               linkState.targetSourceID,
                               linkState.target }).second)
                        continue;
                    plan->modulationControlLinks.push_back (
                        std::move (linkState));
                }
            }
        }
    }

    plan->modulationLinks.erase (
        std::remove_if (
            plan->modulationLinks.begin(),
            plan->modulationLinks.end(),
            [&] (const auto& link)
            {
                return modulationSourceIDs.count (link.sourceID) == 0
                    || nodeIDs.count (link.nodeID.uid) == 0;
            }),
        plan->modulationLinks.end());
    plan->modulationControlLinks.erase (
        std::remove_if (
            plan->modulationControlLinks.begin(),
            plan->modulationControlLinks.end(),
            [&] (const auto& link)
            {
                const auto targetType =
                    modulationSourceTypes.find (link.targetSourceID);
                return modulationSourceIDs.count (link.sourceID) == 0
                    || targetType == modulationSourceTypes.end()
                    || ! isModulationControlTargetSupported (
                        targetType->second, link.target);
            }),
        plan->modulationControlLinks.end());

    for (const auto& connection : plan->connections)
    {
        if (nodeIDs.count (connection.source.nodeID.uid) == 0
            || nodeIDs.count (connection.destination.nodeID.uid) == 0)
        {
            failStateRestore (
                generation, "connection endpoint does not exist");
            return;
        }
    }

    const std::set<std::pair<int, int>> stereoChannels {
        { 0, 0 }, { 1, 1 }
    };
    const std::set<std::pair<int, int>> firstMonoChannels {
        { 0, 0 }, { 0, 1 }
    };
    const std::set<std::pair<int, int>> secondMonoChannels {
        { 1, 0 }, { 1, 1 }
    };
    for (const auto& route : audioRouteChannels)
    {
        const auto fromInput = route.first.first == inputNodeUID;
        const auto valid = route.second == stereoChannels
                        || (stateSchemaVersion >= 2
                            && fromInput
                            && (route.second == firstMonoChannels
                                || route.second == secondMonoChannels));
        if (! valid)
        {
            failStateRestore (generation, "invalid complete audio route");
            return;
        }
    }

    for (const auto& route : audioRouteChannels)
    {
        if (route.first.first != inputNodeUID)
            continue;

        auto sourceIndex = -1;
        if (route.second == stereoChannels)
            sourceIndex = stereoInputSource;
        else if (route.second == firstMonoChannels)
            sourceIndex = firstMonoInputSource;
        else if (route.second == secondMonoChannels)
            sourceIndex = secondMonoInputSource;

        if (sourceIndex < 0
            || ! inputCardSourceIndices.insert (sourceIndex).second)
            continue;

        if (const auto card = makeInputCardInfo (
                sourceIndex,
                { 35,
                  130 + static_cast<int> (plan->inputCards.size()) * 188 }))
            plan->inputCards.push_back (*card);
    }

    if (stateSchemaVersion < 3 && plan->inputCards.empty())
        if (const auto card =
                makeInputCardInfo (stereoInputSource, { 35, 130 }))
            plan->inputCards.push_back (*card);

    for (auto& connection : plan->connections)
    {
        if (connection.source.nodeID.uid != inputNodeUID)
            continue;

        const auto route = audioRouteChannels.find (
            { inputNodeUID, connection.destination.nodeID.uid });
        if (route == audioRouteChannels.end())
            continue;

        if (route->second == firstMonoChannels)
            connection.source.channelIndex =
                connection.destination.channelIndex == 0 ? 2 : 3;
        else if (route->second == secondMonoChannels)
            connection.source.channelIndex =
                connection.destination.channelIndex == 0 ? 4 : 5;
    }

    std::map<juce::uint32, std::vector<juce::uint32>> adjacency;
    for (const auto& route : audioRouteChannels)
        adjacency[route.first.first].push_back (route.first.second);

    std::map<juce::uint32, int> visitState;
    std::function<bool (juce::uint32)> containsCycle =
        [&] (juce::uint32 node)
    {
        auto& stateForNode = visitState[node];
        if (stateForNode == 1)
            return true;
        if (stateForNode == 2)
            return false;

        stateForNode = 1;
        for (const auto destination : adjacency[node])
            if (containsCycle (destination))
                return true;
        stateForNode = 2;
        return false;
    };

    for (const auto uid : nodeIDs)
    {
        if (containsCycle (uid))
        {
            failStateRestore (generation, "feedback cycle");
            return;
        }
    }

    createNextRestoreNode (plan, 0);
}

void BoardAudioEngine::createNextRestoreNode (
    const std::shared_ptr<RestorePlan>& plan,
    size_t index)
{
    if (plan->generation
        != restoreGeneration.load (std::memory_order_relaxed))
        return;

    if (index >= plan->nodes.size())
    {
        commitRestorePlan (plan);
        return;
    }

    const auto gate = callbackGate;
    const auto description = plan->nodes[index].description;
    const auto requestedMode = plan->nodes[index].mode;

    createProcessorAsync (
        description,
        requestedMode,
        [gate, plan, index, requestedMode]
        (std::unique_ptr<juce::AudioProcessor> processor,
         InstanceMode actualMode,
         const juce::String& error)
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr
                || plan->generation
                       != gate->owner->restoreGeneration.load (
                           std::memory_order_relaxed))
                return;

            if (processor == nullptr || actualMode != requestedMode)
            {
                gate->owner->failStateRestore (
                    plan->generation,
                    error.isNotEmpty() ? error : "instance mode mismatch");
                return;
            }

            const auto& childState = plan->nodes[index].childState;
            if (! childState.isEmpty())
            {
                const juce::ScopedLock processorLock (
                    processor->getCallbackLock());
                processor->setStateInformation (
                    childState.getData(),
                    static_cast<int> (childState.getSize()));
            }

            if (processor->getMainBusNumInputChannels() != 2
                || processor->getMainBusNumOutputChannels() != 2)
            {
                gate->owner->failStateRestore (
                    plan->generation,
                    "child state changed the saved stereo layout");
                return;
            }

            plan->nodes[index].processor = std::move (processor);
            gate->owner->createNextRestoreNode (plan, index + 1);
        });
}

void BoardAudioEngine::commitRestorePlan (
    const std::shared_ptr<RestorePlan>& plan)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (plan->generation
        != restoreGeneration.load (std::memory_order_relaxed))
        return;

    for (const auto& node : plan->nodes)
    {
        if (node.processor == nullptr)
        {
            failStateRestore (
                plan->generation, "restore plan is incomplete");
            return;
        }
    }

    for (auto* node : graph.getNodes())
        if (! isIONode (node->nodeID))
            detachProcessorListeners (*node->getProcessor());

    clearModulation();
    graph.clear (juce::AudioProcessorGraph::UpdateKind::none);
    createDefaultGraph (juce::AudioProcessorGraph::UpdateKind::none);
    inputCards = plan->inputCards;
    for (const auto& card : inputCards)
        if (const auto inputNode = graph.getNodeForId (inputNodeID))
            if (auto* router = dynamic_cast<InputRouterProcessor*> (
                    inputNode->getProcessor()))
                router->setSourceLevelDb (
                    card.sourceIndex, card.outputGainDb);
    setNodeLevelDbRaw (
        outputNodeID,
        NodeLevelStage::input,
        plan->outputInputGainDb,
        false);

    for (auto& savedNode : plan->nodes)
    {
        auto node = graph.addNode (
            wrapNodeProcessor (
                std::move (savedNode.processor),
                savedNode.inputGainDb,
                savedNode.outputGainDb),
            savedNode.nodeID,
            juce::AudioProcessorGraph::UpdateKind::none);

        if (node == nullptr)
        {
            failStateRestore (plan->generation, "node commit failed");
            createDefaultGraph (juce::AudioProcessorGraph::UpdateKind::sync);
            return;
        }

        node->properties.set (nodeXKey, savedNode.position.x);
        node->properties.set (nodeYKey, savedNode.position.y);
        node->setBypassed (savedNode.bypassed);
        attachProcessorListeners (*node->getProcessor());
    }

    for (const auto& connection : plan->connections)
    {
        if (! graph.addConnection (
                connection, juce::AudioProcessorGraph::UpdateKind::none))
        {
            failStateRestore (
                plan->generation, "connection commit failed");
            return;
        }
    }

    for (const auto& savedSource : plan->modulationSources)
    {
        auto source = std::make_shared<ModulationSource> (
            savedSource.id, savedSource.type);
        source->lfoWaveform.store (
            savedSource.lfoWaveform,
            std::memory_order_relaxed);
        source->polarity.store (
            savedSource.polarity,
            std::memory_order_relaxed);
        source->primary.store (
            savedSource.primary, std::memory_order_relaxed);
        source->secondary.store (
            savedSource.secondary, std::memory_order_relaxed);
        source->effectiveSecondary.store (
            savedSource.secondary, std::memory_order_relaxed);
        source->envelopeDecay.store (
            savedSource.envelopeDecay, std::memory_order_relaxed);
        source->envelopeSustain.store (
            savedSource.envelopeSustain, std::memory_order_relaxed);
        source->jitter.store (
            savedSource.jitter, std::memory_order_relaxed);
        source->jitterRange.store (
            savedSource.jitterRange, std::memory_order_relaxed);
        source->autoTrigger.store (
            savedSource.autoTrigger, std::memory_order_relaxed);
        auto curveState = createCurveState (
            savedSource.curvePoints,
            savedSource.loopMode,
            savedSource.loopStart,
            savedSource.loopEnd);
        if (curveState == nullptr)
            curveState = createCurveState (
                makeDefaultCurvePoints(),
                CurveLoopMode::fullLoop,
                0.25f,
                0.75f);
        source->curveState = std::move (curveState);
        if (source->curveState->loopMode == CurveLoopMode::sectionLoop)
            source->phase = source->curveState->loopStart;
        source->displayPhase.store (
            static_cast<float> (source->phase),
            std::memory_order_relaxed);

        modulationSources.push_back (std::move (source));
        nextModulationSourceID =
            juce::jmax (
                nextModulationSourceID, savedSource.id + 1);
    }

    for (const auto& savedLink : plan->modulationLinks)
    {
        const auto source =
            findModulationSource (savedLink.sourceID);
        const auto node = graph.getNodeForId (savedLink.nodeID);
        if (source == nullptr || node == nullptr)
            continue;

        auto* processor = getParameterProcessor (node->getProcessor());
        if (processor == nullptr)
            continue;

        const auto& parameters = processor->getParameters();
        auto parameterIndex = -1;

        if (savedLink.parameterID.isNotEmpty())
        {
            for (int index = 0; index < parameters.size(); ++index)
                if (auto* hosted =
                        dynamic_cast<juce::HostedAudioProcessorParameter*> (
                            parameters.getUnchecked (index)))
                    if (hosted->getParameterID()
                        == savedLink.parameterID)
                    {
                        parameterIndex = index;
                        break;
                    }
        }
        else if (juce::isPositiveAndBelow (
                     savedLink.parameterIndex, parameters.size()))
        {
            parameterIndex = savedLink.parameterIndex;
        }

        if (! juce::isPositiveAndBelow (
                parameterIndex, parameters.size()))
            continue;

        auto* parameter = parameters.getUnchecked (parameterIndex);
        const auto category = static_cast<int> (parameter->getCategory());
        if (! parameter->isAutomatable()
            || (category & 0xffff0000) == (2 << 16))
            continue;

        std::shared_ptr<ModulationTarget> target;
        for (const auto& existingLink : modulationLinks)
            if (existingLink->target->processor == processor
                && existingLink->target->parameterIndex
                       == parameterIndex)
            {
                target = existingLink->target;
                break;
            }

        if (target == nullptr)
        {
            target = std::make_shared<ModulationTarget>();
            target->node = node;
            target->processor = processor;
            target->parameter = parameter;
            target->parameterIndex = parameterIndex;
            target->parameterID = savedLink.parameterID;
            target->parameterName =
                savedLink.parameterName.isNotEmpty()
                    ? savedLink.parameterName
                    : parameter->getName (96);
            target->targetName =
                node->getProcessor()->getName()
                + " #" + juce::String (node->nodeID.uid)
                + "  /  " + target->parameterName;
            target->baseValue.store (
                savedLink.baseValue, std::memory_order_relaxed);
            setModulationTargetValue (
                target, savedLink.baseValue);
        }

        auto link = std::make_shared<ModulationLink>();
        link->id = savedLink.id;
        link->source = source;
        link->target = target;
        link->amount.store (
            savedLink.amount, std::memory_order_relaxed);
        modulationLinks.push_back (std::move (link));
        nextModulationLinkID =
            juce::jmax (
                nextModulationLinkID, savedLink.id + 1);
    }

    for (const auto& savedLink : plan->modulationControlLinks)
    {
        const auto source =
            findModulationSource (savedLink.sourceID);
        const auto targetSource =
            findModulationSource (savedLink.targetSourceID);
        if (source == nullptr || targetSource == nullptr)
            continue;

        auto link = std::make_shared<ModulationControlLink>();
        link->id = savedLink.id;
        link->source = source;
        link->targetSource = targetSource;
        link->target = savedLink.target;
        link->amount.store (
            savedLink.target == ModulationControlTarget::trigger
                ? 1.0f
                : savedLink.amount,
            std::memory_order_relaxed);
        link->gateResyncPending =
            savedLink.target == ModulationControlTarget::trigger;
        modulationControlLinks.push_back (std::move (link));
        nextModulationLinkID =
            juce::jmax (
                nextModulationLinkID, savedLink.id + 1);
    }

    modulationStatus =
        modulationSources.empty()
            ? "Add a modulator to get started"
            : juce::String (modulationSources.size())
                  + (modulationSources.size() == 1
                         ? " modulator restored"
                         : " modulators restored");
    rebuildModulationRuntime();
    graph.rebuild();
    updateLatencyAndBypassCapacity();
    stateSnapshotPending.store (false, std::memory_order_relaxed);
    rebuildStateCache();
    sendChangeMessage();
    updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails {}
            .withNonParameterStateChanged (true));
    finishExternalStateRestore (plan->generation);
}

void BoardAudioEngine::initialiseProperties()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "TB_Board";
    options.filenameSuffix = "settings";
    options.folderName = "Team Impulse Impact/TB_Board";
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers = false;
    options.ignoreCaseOfKeyNames = true;
    applicationProperties.setStorageParameters (options);
}

void BoardAudioEngine::loadPluginList()
{
    if (auto* settings = getSettings())
        if (const auto xml = settings->getXmlValue (pluginListKey))
            knownPlugins.recreateFromXml (*xml);
}

void BoardAudioEngine::savePluginList()
{
    if (auto* settings = getSettings())
    {
        const auto xml = knownPlugins.createXml();
        settings->setValue (pluginListKey, xml.get());
    }
}

void BoardAudioEngine::scanDefaultPluginFolder()
{
    const auto folder = getDefaultPluginFolder();
    if (! folder.isDirectory())
        return;

    startCatalogScan (folder);
}

void BoardAudioEngine::bootstrapFactoryPresetCatalog (
    const juce::String& presetID,
    FactoryPresetCallback completion)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (completion == nullptr)
        return;

    const auto* preset = BoardPresetLibrary::findPreset (presetID);
    if (preset == nullptr)
    {
        completion (false, "The selected board preset is unavailable.");
        return;
    }

    const auto missing =
        findMissingFactoryPresetPlugins (knownPlugins, *preset);
    if (missing.isEmpty())
    {
        completion (true, {});
        return;
    }

    const auto folder = getDefaultPluginFolder();
    if (! folder.isDirectory())
    {
        completion (
            false,
            "The default TB_Plugins folder was not found: "
                + folder.getFullPathName());
        return;
    }

    if (catalogScanPool == nullptr)
        catalogScanPool = std::make_unique<juce::ThreadPool> (1);

    catalogScanJobsPending.fetch_add (1, std::memory_order_acq_rel);
    sendChangeMessage();

    const auto gate = callbackGate;
    catalogScanPool->addJob (
        new FactoryPresetCatalogScanJob (
            folder,
            missing,
            [gate,
             presetID,
             completionToCall = std::move (completion)] (
                CatalogScanResult result) mutable
            {
                const juce::ScopedLock gateLock (gate->lock);
                if (gate->owner != nullptr)
                    gate->owner->applyFactoryPresetCatalogScanResult (
                        presetID,
                        result,
                        std::move (completionToCall));
            }),
        true);
}

void BoardAudioEngine::applyFactoryPresetCatalogScanResult (
    const juce::String& presetID,
    const CatalogScanResult& result,
    FactoryPresetCallback completion)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (result.completed)
    {
        for (const auto& plugin : result.pluginTypes)
            knownPlugins.addType (plugin);

        savePluginList();
        applicationProperties.saveIfNeeded();
        sendChangeMessage();
    }

    const auto* preset = BoardPresetLibrary::findPreset (presetID);
    const auto missing =
        preset != nullptr
            ? findMissingFactoryPresetPlugins (
                  knownPlugins, *preset)
            : juce::StringArray {};

    finishCatalogScan();

    for (const auto& file : result.failedFiles)
        juce::Logger::writeToLog (
            "TB_Board factory preset VST3 bootstrap failed: " + file);

    if (preset == nullptr)
    {
        completion (false, "The selected board preset is unavailable.");
        return;
    }

    if (! result.completed || ! missing.isEmpty())
    {
        completion (
            false,
            "Missing required VST3 after checking "
                + getDefaultPluginFolder().getFullPathName()
                + ": "
                + missing.joinIntoString (", "));
        return;
    }

    completion (true, {});
}

void BoardAudioEngine::startCatalogScan (const juce::File& folder)
{
    if (! catalogWritable
        || catalogScanPool == nullptr
        || ! folder.isDirectory())
        return;

    catalogScanJobsPending.fetch_add (1, std::memory_order_acq_rel);
    sendChangeMessage();

    const auto gate = callbackGate;
    const juce::StringArray existingBlacklistedFiles (
        knownPlugins.getBlacklistedFiles());
    catalogScanPool->addJob (
        new CatalogScanJob (
            folder,
            getBackgroundScanDeadMansPedalFile(),
            existingBlacklistedFiles,
            [gate] (CatalogScanResult result)
            {
                const juce::ScopedLock gateLock (gate->lock);
                if (gate->owner != nullptr)
                {
                    gate->owner->applyCatalogScanResult (result);
                    gate->owner->finishCatalogScan();
                }
            }),
        true);
}

void BoardAudioEngine::finishCatalogScan()
{
    const auto previous =
        catalogScanJobsPending.fetch_sub (1, std::memory_order_acq_rel);
    jassert (previous > 0);
    if (previous <= 0)
        catalogScanJobsPending.store (0, std::memory_order_release);

    sendChangeMessage();
}

void BoardAudioEngine::applyCatalogScanResult (
    const CatalogScanResult& result)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (! catalogWritable || ! result.completed)
        return;

    juce::StringArray successfullyScannedFiles;
    for (const auto& plugin : result.pluginTypes)
        successfullyScannedFiles.addIfNotAlreadyThere (
            plugin.fileOrIdentifier);

    const auto existingTypes = knownPlugins.getTypes();
    for (const auto& plugin : existingTypes)
    {
        const juce::File pluginFile (plugin.fileOrIdentifier);
        const auto isInScannedFolder =
            pluginFile == result.scannedFolder
            || pluginFile.isAChildOf (result.scannedFolder);
        const auto isMissingVst3 =
            plugin.pluginFormatName.equalsIgnoreCase ("VST3")
            && isInScannedFolder
            && ! pluginFile.exists();

        if (isMissingVst3
            || successfullyScannedFiles.contains (
                   plugin.fileOrIdentifier,
                   true))
            knownPlugins.removeType (plugin);
    }

    for (const auto& plugin : result.pluginTypes)
        knownPlugins.addType (plugin);

    for (const auto& file : result.blacklistedFiles)
        knownPlugins.addToBlacklist (file);

    savePluginList();
    applicationProperties.saveIfNeeded();
    sendChangeMessage();

    for (const auto& file : result.failedFiles)
        juce::Logger::writeToLog (
            "TB_Board VST3 scan failed: " + file);
}

void BoardAudioEngine::createDefaultGraph (
    juce::AudioProcessorGraph::UpdateKind updateKind)
{
    auto physicalInput = graph.addNode (
        std::make_unique<
            juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode),
        NodeID { physicalInputNodeUID },
        juce::AudioProcessorGraph::UpdateKind::none);
    auto input = graph.addNode (
        std::make_unique<InputRouterProcessor>(),
        NodeID { inputNodeUID },
        juce::AudioProcessorGraph::UpdateKind::none);
    auto output = graph.addNode (
        std::make_unique<
            juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode),
        NodeID { outputNodeUID },
        juce::AudioProcessorGraph::UpdateKind::none);

    jassert (physicalInput != nullptr && input != nullptr && output != nullptr);

    if (physicalInput != nullptr)
        physicalInputNodeID = physicalInput->nodeID;
    if (input != nullptr)
        inputNodeID = input->nodeID;
    if (output != nullptr)
        outputNodeID = output->nodeID;

    if (physicalInput != nullptr && input != nullptr)
    {
        graph.addConnection (
            { { physicalInput->nodeID, 0 }, { input->nodeID, 0 } },
            juce::AudioProcessorGraph::UpdateKind::none);
        graph.addConnection (
            { { physicalInput->nodeID, 1 }, { input->nodeID, 1 } },
            updateKind);
    }
}

void BoardAudioEngine::attachProcessorListeners (juce::AudioProcessor& processor)
{
    processor.addListener (this);

    if (auto* parameterProcessor = getParameterProcessor (&processor);
        parameterProcessor != nullptr && parameterProcessor != &processor)
        parameterProcessor->addListener (this);
}

void BoardAudioEngine::detachProcessorListeners (juce::AudioProcessor& processor)
{
    if (auto* parameterProcessor = getParameterProcessor (&processor);
        parameterProcessor != nullptr && parameterProcessor != &processor)
        parameterProcessor->removeListener (this);

    processor.removeListener (this);
}

bool BoardAudioEngine::boardEditsBlocked() const noexcept
{
    return historyOperationBusy
        || externalStateRestorePending.load (std::memory_order_acquire);
}

juce::uint64 BoardAudioEngine::beginHistoryOperation()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto operationToken =
        historyOperationSerial.fetch_add (1, std::memory_order_acq_rel) + 1;
    historyOperationBusy = true;
    sendChangeMessage();
    return operationToken;
}

bool BoardAudioEngine::isCurrentHistoryOperation (
    juce::uint64 operationToken) const noexcept
{
    return historyOperationBusy
        && operationToken
               == historyOperationSerial.load (std::memory_order_acquire);
}

bool BoardAudioEngine::finishHistoryOperation (
    juce::uint64 operationToken)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (! isCurrentHistoryOperation (operationToken))
        return false;

    historyOperationBusy = false;
    sendChangeMessage();
    return true;
}

bool BoardAudioEngine::captureSavedNode (NodeID nodeID,
                                         SavedNode& savedNode,
                                         juce::String& error)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (isIONode (nodeID))
    {
        error = "Board input and output nodes cannot be removed.";
        return false;
    }

    const juce::ScopedLock engineCallbackLock (getCallbackLock());
    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr)
    {
        error = "The selected node no longer exists.";
        return false;
    }

    SavedNode candidate;
    candidate.nodeID = nodeID;
    candidate.position = getNodePosition (nodeID, {});
    candidate.bypassed = node->isBypassed();
    candidate.inputGainDb = getNodeLevelDb (
        nodeID, NodeLevelStage::input);
    candidate.outputGainDb = getNodeLevelDb (
        nodeID, NodeLevelStage::output);

    auto* processor = node->getProcessor();
    auto dualMono = false;
    if (! getPluginDescriptionForProcessor (
            *processor, candidate.description, dualMono))
    {
        error = "The selected node is not a restorable VST3 instance.";
        return false;
    }

    candidate.mode = dualMono ? InstanceMode::linkedDualMono
                              : InstanceMode::nativeStereo;

    {
        const juce::ScopedLock processorLock (processor->getCallbackLock());
        candidate.currentProgram = processor->getCurrentProgram();
        auto* parameterProcessor = getParameterProcessor (processor);
        for (auto* parameter : parameterProcessor->getParameters())
            candidate.parameterValues.push_back (
                parameter != nullptr ? parameter->getValue() : 0.0f);
        processor->getStateInformation (candidate.childState);
    }

    if (candidate.childState.getSize()
            + candidate.parameterValues.size()
                  * sizeof (float)
        > maximumChildStateBytes)
    {
        error = "The VST3 state is too large to store in undo history.";
        return false;
    }

    for (const auto& connection : graph.getConnections())
    {
        if ((connection.source.nodeID == nodeID
             || connection.destination.nodeID == nodeID)
            && ! connection.source.isMIDI()
            && ! connection.destination.isMIDI())
            candidate.incidentConnections.push_back (connection);
    }

    for (const auto& link : modulationLinks)
        if (link->target != nullptr
            && link->target->node != nullptr
            && link->target->node->nodeID == nodeID)
            candidate.modulationLinks.push_back (
                captureModulationLink (link));

    savedNode = std::move (candidate);
    return true;
}

bool BoardAudioEngine::removeNodeRaw (NodeID nodeID)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (isIONode (nodeID))
        return false;

    auto* node = graph.getNodeForId (nodeID);
    if (node == nullptr)
        return false;

    detachProcessorListeners (*node->getProcessor());
    const auto removed = graph.removeNode (
        nodeID, juce::AudioProcessorGraph::UpdateKind::none);
    if (removed == nullptr)
    {
        attachProcessorListeners (*node->getProcessor());
        return false;
    }

    removeModulationForNode (nodeID);
    scheduleBoardUpdate (true);
    return true;
}

void BoardAudioEngine::setNodePositionRaw (NodeID nodeID,
                                           juce::Point<int> position)
{
    if (auto* node = graph.getNodeForId (nodeID))
    {
        node->properties.set (nodeXKey, position.x);
        node->properties.set (nodeYKey, position.y);
        scheduleBoardUpdate (false);
    }
}

bool BoardAudioEngine::setNodeLevelDbRaw (
    NodeID nodeID,
    NodeLevelStage stage,
    float decibels,
    bool scheduleUpdate)
{
    const auto level = sanitiseLevelDb (decibels);
    if (nodeID == outputNodeID)
    {
        if (stage != NodeLevelStage::input)
            return false;

        outputInputGainDb = level;
        outputInputGainTarget.store (
            levelDbToGain (level), std::memory_order_relaxed);
        if (scheduleUpdate)
            scheduleBoardUpdate (false);
        return true;
    }

    if (isIONode (nodeID))
        return false;

    const auto node = graph.getNodeForId (nodeID);
    if (node == nullptr)
        return false;
    auto* gain = getNodeGainProcessor (node->getProcessor());
    if (gain == nullptr)
        return false;

    gain->setLevelDb (stage, level);
    if (scheduleUpdate)
        scheduleBoardUpdate (false);
    return true;
}

bool BoardAudioEngine::setInputCardOutputLevelDbRaw (
    int sourceIndex,
    float decibels,
    bool scheduleUpdate)
{
    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    if (card == inputCards.end())
        return false;

    card->outputGainDb = sanitiseLevelDb (decibels);
    const auto inputNode = graph.getNodeForId (inputNodeID);
    auto* router = inputNode != nullptr
                       ? dynamic_cast<InputRouterProcessor*> (
                             inputNode->getProcessor())
                       : nullptr;
    if (router == nullptr)
        return false;

    router->setSourceLevelDb (sourceIndex, card->outputGainDb);
    if (scheduleUpdate)
        scheduleBoardUpdate (false);
    return true;
}

std::optional<BoardAudioEngine::InputCardInfo>
BoardAudioEngine::makeInputCardInfo (
    int sourceIndex,
    juce::Point<int> position) const
{
    for (const auto& source : getInputSources())
        if (source.index == sourceIndex && source.routable)
            return InputCardInfo {
                source.index, source.name, source.detail, position, 0.0f
            };

    return std::nullopt;
}

bool BoardAudioEngine::addInputCardRaw (
    const InputCardInfo& card,
    bool scheduleUpdate)
{
    if (hasInputCard (card.sourceIndex)
        || ! makeInputCardInfo (card.sourceIndex, card.position).has_value())
        return false;

    inputCards.push_back (card);
    if (const auto inputNode = graph.getNodeForId (inputNodeID))
        if (auto* router = dynamic_cast<InputRouterProcessor*> (
                inputNode->getProcessor()))
            router->setSourceLevelDb (
                card.sourceIndex, card.outputGainDb);
    if (scheduleUpdate)
        scheduleBoardUpdate (false);
    return true;
}

bool BoardAudioEngine::removeInputCardRaw (
    int sourceIndex,
    bool scheduleUpdate)
{
    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    if (card == inputCards.end())
        return false;

    if (const auto inputNode = graph.getNodeForId (inputNodeID))
        if (auto* router = dynamic_cast<InputRouterProcessor*> (
                inputNode->getProcessor()))
            router->setSourceLevelDb (sourceIndex, 0.0f);
    inputCards.erase (card);
    if (scheduleUpdate)
        scheduleBoardUpdate (false);
    return true;
}

void BoardAudioEngine::setInputCardPositionRaw (
    int sourceIndex,
    juce::Point<int> position,
    bool scheduleUpdate)
{
    const auto card = std::find_if (
        inputCards.begin(), inputCards.end(),
        [sourceIndex] (const auto& candidate)
        {
            return candidate.sourceIndex == sourceIndex;
        });
    if (card == inputCards.end() || card->position == position)
        return;

    card->position = position;
    if (scheduleUpdate)
        scheduleBoardUpdate (false);
}

bool BoardAudioEngine::addConnectionsRaw (
    const std::vector<juce::AudioProcessorGraph::Connection>& connections,
    juce::String& error,
    bool scheduleUpdate)
{
    std::vector<juce::AudioProcessorGraph::Connection> connectionsToAdd;
    for (const auto& connection : connections)
    {
        if (isInternalConnection (connection))
        {
            error = "The physical input bridge is protected.";
            return false;
        }

        if (graph.isConnected (connection))
            continue;

        if (graph.getNodeForId (connection.source.nodeID) == nullptr
            || graph.getNodeForId (connection.destination.nodeID) == nullptr)
        {
            error = "A connection endpoint no longer exists.";
            return false;
        }

        if (! graph.canConnect (connection))
        {
            error = "The saved audio pins are no longer compatible.";
            return false;
        }

        if (graph.isAnInputTo (
                connection.destination.nodeID,
                connection.source.nodeID))
        {
            error = "The edit would create a feedback cycle.";
            return false;
        }

        connectionsToAdd.push_back (connection);
    }

    std::vector<juce::AudioProcessorGraph::Connection> addedConnections;
    for (const auto& connection : connectionsToAdd)
    {
        if (! graph.addConnection (
                connection, juce::AudioProcessorGraph::UpdateKind::none))
        {
            for (const auto& added : addedConnections)
                graph.removeConnection (
                    added, juce::AudioProcessorGraph::UpdateKind::none);

            error = "The saved audio route could not be restored.";
            return false;
        }

        addedConnections.push_back (connection);
    }

    if (scheduleUpdate && ! addedConnections.empty())
        scheduleBoardUpdate (true);

    return true;
}

bool BoardAudioEngine::removeConnectionsRaw (
    const std::vector<juce::AudioProcessorGraph::Connection>& connections,
    bool requireAll,
    bool scheduleUpdate)
{
    if (std::any_of (
            connections.begin(), connections.end(),
            [this] (const auto& connection)
            {
                return isInternalConnection (connection);
            }))
        return false;

    if (requireAll)
        for (const auto& connection : connections)
            if (! graph.isConnected (connection))
                return false;

    std::vector<juce::AudioProcessorGraph::Connection> removedConnections;
    for (const auto& connection : connections)
    {
        if (! graph.isConnected (connection))
            continue;

        if (! graph.removeConnection (
                connection, juce::AudioProcessorGraph::UpdateKind::none))
        {
            juce::String ignoredError;
            addConnectionsRaw (removedConnections, ignoredError, false);
            return false;
        }

        removedConnections.push_back (connection);
    }

    if (scheduleUpdate && ! removedConnections.empty())
        scheduleBoardUpdate (true);

    return true;
}

void BoardAudioEngine::restoreSavedNodeAsync (
    const SavedNode& savedNode,
    juce::uint64 generation,
    HistoryCompletion completion)
{
    auto saved = std::make_shared<SavedNode> (savedNode);
    const auto gate = callbackGate;

    createProcessorAsync (
        saved->description,
        saved->mode,
        [gate, saved, generation, completion = std::move (completion)]
        (std::unique_ptr<juce::AudioProcessor> processor,
         InstanceMode actualMode,
         const juce::String& error) mutable
        {
            const juce::ScopedLock gateLock (gate->lock);
            if (gate->owner == nullptr)
                return;

            auto& owner = *gate->owner;
            if (generation
                    != owner.restoreGeneration.load (
                        std::memory_order_relaxed)
                || owner.externalStateRestorePending.load (
                    std::memory_order_acquire))
            {
                completion (false);
                return;
            }

            if (processor == nullptr || actualMode != saved->mode)
            {
                juce::Logger::writeToLog (
                    "TB_Board history restore failed: "
                    + (error.isNotEmpty()
                           ? error
                           : juce::String ("VST3 instance mode mismatch.")));
                completion (false);
                return;
            }

            {
                const juce::ScopedLock processorLock (
                    processor->getCallbackLock());
                if (juce::isPositiveAndBelow (
                        saved->currentProgram,
                        processor->getNumPrograms()))
                    processor->setCurrentProgram (
                        saved->currentProgram);

                if (! saved->childState.isEmpty())
                    processor->setStateInformation (
                        saved->childState.getData(),
                        static_cast<int> (
                            saved->childState.getSize()));

                if (juce::isPositiveAndBelow (
                        saved->currentProgram,
                        processor->getNumPrograms())
                    && processor->getCurrentProgram()
                           != saved->currentProgram)
                    processor->setCurrentProgram (
                        saved->currentProgram);

                auto* parameterProcessor =
                    getParameterProcessor (processor.get());
                const auto& parameters =
                    parameterProcessor->getParameters();
                const auto valuesToRestore =
                    juce::jmin (
                        parameters.size(),
                        static_cast<int> (
                            saved->parameterValues.size()));
                for (auto index = 0;
                     index < valuesToRestore;
                     ++index)
                    if (parameters[index] != nullptr)
                    {
                        const auto value = saved->parameterValues[
                            static_cast<size_t> (index)];
                        if (std::abs (
                                parameters[index]->getValue() - value)
                            <= 1.0e-6f)
                            continue;

                        if (auto* dualMono =
                                getDualMonoProcessor (processor.get()))
                            dualMono->setLinkedParameterValue (
                                index, value);
                        else
                            parameters[index]->setValue (value);
                    }
            }

            if (processor->getMainBusNumInputChannels() != 2
                || processor->getMainBusNumOutputChannels() != 2)
            {
                juce::Logger::writeToLog (
                    "TB_Board history restore failed: "
                    "the saved VST3 no longer has a stereo main bus.");
                completion (false);
                return;
            }

            auto node = owner.graph.addNode (
                wrapNodeProcessor (
                    std::move (processor),
                    saved->inputGainDb,
                    saved->outputGainDb),
                saved->nodeID,
                juce::AudioProcessorGraph::UpdateKind::none);
            if (node == nullptr)
            {
                juce::Logger::writeToLog (
                    "TB_Board history restore failed: "
                    "the saved node ID is no longer available.");
                completion (false);
                return;
            }

            node->properties.set (nodeXKey, saved->position.x);
            node->properties.set (nodeYKey, saved->position.y);
            node->setBypassed (saved->bypassed);
            owner.attachProcessorListeners (*node->getProcessor());

            juce::String routeError;
            if (! owner.addConnectionsRaw (
                    saved->incidentConnections, routeError, false))
            {
                owner.detachProcessorListeners (*node->getProcessor());
                owner.graph.removeNode (
                    node->nodeID,
                    juce::AudioProcessorGraph::UpdateKind::none);
                juce::Logger::writeToLog (
                    "TB_Board history restore failed: " + routeError);
                completion (false);
                return;
            }

            for (const auto& savedLink : saved->modulationLinks)
                if (! owner.addModulationLinkRaw (savedLink))
                {
                    juce::Logger::writeToLog (
                        "TB_Board history restore failed: "
                        "a modulation link could not be restored.");
                    owner.removeNodeRaw (node->nodeID);
                    completion (false);
                    return;
                }

            if (! saved->modulationLinks.empty())
                owner.modulationStatus =
                    "Plug-in and modulation links restored";

            owner.scheduleBoardUpdate (true);
            completion (true);
        });
}

void BoardAudioEngine::applyHistoryEdit (BoardEdit& edit,
                                         bool forward,
                                         HistoryCompletion completion)
{
    switch (edit.kind)
    {
        case BoardEdit::Kind::addNode:
        {
            if (forward)
            {
                restoreSavedNodeAsync (
                    edit.savedNode,
                    restoreGeneration.load (std::memory_order_relaxed),
                    std::move (completion));
                return;
            }

            SavedNode currentState;
            juce::String error;
            if (! captureSavedNode (
                    edit.savedNode.nodeID, currentState, error)
                || ! removeNodeRaw (edit.savedNode.nodeID))
            {
                if (error.isNotEmpty())
                    juce::Logger::writeToLog (
                        "TB_Board undo failed: " + error);
                completion (false);
                return;
            }

            edit.savedNode = std::move (currentState);
            completion (true);
            return;
        }

        case BoardEdit::Kind::deleteNode:
        {
            if (! forward)
            {
                restoreSavedNodeAsync (
                    edit.savedNode,
                    restoreGeneration.load (std::memory_order_relaxed),
                    std::move (completion));
                return;
            }

            SavedNode currentState;
            juce::String error;
            if (! captureSavedNode (
                    edit.savedNode.nodeID, currentState, error)
                || ! removeNodeRaw (edit.savedNode.nodeID))
            {
                if (error.isNotEmpty())
                    juce::Logger::writeToLog (
                        "TB_Board redo failed: " + error);
                completion (false);
                return;
            }

            edit.savedNode = std::move (currentState);
            completion (true);
            return;
        }

        case BoardEdit::Kind::moveNode:
        {
            if (graph.getNodeForId (edit.nodeID) == nullptr)
            {
                completion (false);
                return;
            }

            setNodePositionRaw (
                edit.nodeID,
                forward ? edit.newPosition : edit.oldPosition);
            completion (true);
            return;
        }

        case BoardEdit::Kind::addInputCard:
        {
            auto succeeded = false;
            if (forward)
                succeeded = addInputCardRaw (edit.inputCard, true);
            else
                succeeded = removeInputCardRaw (
                    edit.inputCard.sourceIndex, true);

            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::removeInputCard:
        {
            if (forward)
            {
                if (! hasInputCard (edit.inputCard.sourceIndex)
                    || ! removeConnectionsRaw (
                        edit.connections, true, false))
                {
                    completion (false);
                    return;
                }

                if (! removeInputCardRaw (
                        edit.inputCard.sourceIndex, false))
                {
                    juce::String ignoredError;
                    addConnectionsRaw (
                        edit.connections, ignoredError, false);
                    completion (false);
                    return;
                }

                scheduleBoardUpdate (! edit.connections.empty());
                completion (true);
                return;
            }

            if (! addInputCardRaw (edit.inputCard, false))
            {
                completion (false);
                return;
            }

            juce::String error;
            if (! addConnectionsRaw (edit.connections, error, false))
            {
                removeInputCardRaw (edit.inputCard.sourceIndex, false);
                if (error.isNotEmpty())
                    juce::Logger::writeToLog (
                        "TB_Board input restore failed: " + error);
                completion (false);
                return;
            }

            scheduleBoardUpdate (! edit.connections.empty());
            completion (true);
            return;
        }

        case BoardEdit::Kind::moveInputCard:
        {
            if (! hasInputCard (edit.inputCard.sourceIndex))
            {
                completion (false);
                return;
            }

            setInputCardPositionRaw (
                edit.inputCard.sourceIndex,
                forward ? edit.newPosition : edit.oldPosition,
                true);
            completion (true);
            return;
        }

        case BoardEdit::Kind::changeNodeLevel:
        {
            const auto succeeded = setNodeLevelDbRaw (
                edit.nodeID,
                edit.nodeLevelStage,
                forward ? edit.newLevelDb : edit.oldLevelDb,
                true);
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::changeInputCardOutputLevel:
        {
            const auto succeeded = setInputCardOutputLevelDbRaw (
                edit.inputCard.sourceIndex,
                forward ? edit.newLevelDb : edit.oldLevelDb,
                true);
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::addConnections:
        {
            juce::String error;
            const auto succeeded =
                forward
                    ? addConnectionsRaw (
                          edit.connections, error, true)
                    : removeConnectionsRaw (
                          edit.connections, true, true);
            if (! succeeded && error.isNotEmpty())
                juce::Logger::writeToLog (
                    "TB_Board history route edit failed: " + error);
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::removeConnections:
        {
            juce::String error;
            const auto succeeded =
                forward
                    ? removeConnectionsRaw (
                          edit.connections, true, true)
                    : addConnectionsRaw (
                          edit.connections, error, true);
            if (! succeeded && error.isNotEmpty())
                juce::Logger::writeToLog (
                    "TB_Board history route edit failed: " + error);
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::addModulationSource:
        {
            auto succeeded = false;
            if (forward)
            {
                succeeded = addModulationSourceRaw (
                    edit.modulationSource,
                    edit.modulationLinks,
                    edit.modulationControlLinks);
            }
            else
            {
                const auto source =
                    findModulationSource (edit.modulationSource.id);
                if (source != nullptr)
                {
                    auto currentSource = captureModulationSource (source);
                    std::vector<SavedModulationLink> currentLinks;
                    std::vector<SavedModulationControlLink>
                        currentControlLinks;
                    for (const auto& link : modulationLinks)
                        if (link->source == source)
                            currentLinks.push_back (
                                captureModulationLink (link));
                    for (const auto& link : modulationControlLinks)
                        if (link->source == source
                            || link->targetSource == source)
                            currentControlLinks.push_back (
                                captureModulationControlLink (link));

                    succeeded =
                        removeModulationSourceRaw (edit.modulationSource.id);
                    if (succeeded)
                    {
                        edit.modulationSource = std::move (currentSource);
                        edit.modulationLinks = std::move (currentLinks);
                        edit.modulationControlLinks =
                            std::move (currentControlLinks);
                    }
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? getModulationTypeName (edit.modulationSource.type)
                          + " restored"
                    : getModulationTypeName (edit.modulationSource.type)
                          + " removed";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::removeModulationSource:
        {
            auto succeeded = false;
            if (! forward)
            {
                succeeded = addModulationSourceRaw (
                    edit.modulationSource,
                    edit.modulationLinks,
                    edit.modulationControlLinks);
            }
            else
            {
                const auto source =
                    findModulationSource (edit.modulationSource.id);
                if (source != nullptr)
                {
                    auto currentSource = captureModulationSource (source);
                    std::vector<SavedModulationLink> currentLinks;
                    std::vector<SavedModulationControlLink>
                        currentControlLinks;
                    for (const auto& link : modulationLinks)
                        if (link->source == source)
                            currentLinks.push_back (
                                captureModulationLink (link));
                    for (const auto& link : modulationControlLinks)
                        if (link->source == source
                            || link->targetSource == source)
                            currentControlLinks.push_back (
                                captureModulationControlLink (link));

                    succeeded =
                        removeModulationSourceRaw (edit.modulationSource.id);
                    if (succeeded)
                    {
                        edit.modulationSource = std::move (currentSource);
                        edit.modulationLinks = std::move (currentLinks);
                        edit.modulationControlLinks =
                            std::move (currentControlLinks);
                    }
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? getModulationTypeName (edit.modulationSource.type)
                          + " removed"
                    : getModulationTypeName (edit.modulationSource.type)
                          + " restored";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::changeModulationSource:
        {
            const auto source =
                findModulationSource (edit.modulationSource.id);
            if (source == nullptr)
            {
                completion (false);
                return;
            }

            auto current = captureModulationSource (source);
            const auto succeeded =
                restoreModulationSourceSettingsRaw (
                    edit.modulationSource);
            if (succeeded)
            {
                edit.modulationSource = std::move (current);
                modulationStatus =
                    getModulationTypeName (source->type)
                    + (forward ? " edit restored"
                               : " edit undone");
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::addModulationLink:
        {
            auto succeeded = false;
            if (forward)
            {
                succeeded = addModulationLinkRaw (edit.modulationLink);
            }
            else
            {
                const auto iterator = std::find_if (
                    modulationLinks.begin(),
                    modulationLinks.end(),
                    [&edit] (const auto& link)
                    {
                        return link->id == edit.modulationLink.id;
                    });
                if (iterator != modulationLinks.end())
                {
                    const auto current = captureModulationLink (*iterator);
                    succeeded =
                        removeModulationLinkRaw (edit.modulationLink.id);
                    if (succeeded)
                        edit.modulationLink = current;
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? "Modulation link restored"
                    : "Modulation link removed";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::removeModulationLink:
        {
            auto succeeded = false;
            if (! forward)
            {
                succeeded = addModulationLinkRaw (edit.modulationLink);
            }
            else
            {
                const auto iterator = std::find_if (
                    modulationLinks.begin(),
                    modulationLinks.end(),
                    [&edit] (const auto& link)
                    {
                        return link->id == edit.modulationLink.id;
                    });
                if (iterator != modulationLinks.end())
                {
                    const auto current = captureModulationLink (*iterator);
                    succeeded =
                        removeModulationLinkRaw (edit.modulationLink.id);
                    if (succeeded)
                        edit.modulationLink = current;
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? "Modulation link removed"
                    : "Modulation link restored";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::addModulationControlLink:
        {
            auto succeeded = false;
            if (forward)
            {
                succeeded = addModulationControlLinkRaw (
                    edit.modulationControlLink);
            }
            else
            {
                const auto iterator = std::find_if (
                    modulationControlLinks.begin(),
                    modulationControlLinks.end(),
                    [&edit] (const auto& link)
                    {
                        return link->id
                            == edit.modulationControlLink.id;
                    });
                if (iterator != modulationControlLinks.end())
                {
                    const auto current =
                        captureModulationControlLink (*iterator);
                    succeeded = removeModulationLinkRaw (
                        edit.modulationControlLink.id);
                    if (succeeded)
                        edit.modulationControlLink = current;
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? "Modulation control link restored"
                    : "Modulation control link removed";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::removeModulationControlLink:
        {
            auto succeeded = false;
            if (! forward)
            {
                succeeded = addModulationControlLinkRaw (
                    edit.modulationControlLink);
            }
            else
            {
                const auto iterator = std::find_if (
                    modulationControlLinks.begin(),
                    modulationControlLinks.end(),
                    [&edit] (const auto& link)
                    {
                        return link->id
                            == edit.modulationControlLink.id;
                    });
                if (iterator != modulationControlLinks.end())
                {
                    const auto current =
                        captureModulationControlLink (*iterator);
                    succeeded = removeModulationLinkRaw (
                        edit.modulationControlLink.id);
                    if (succeeded)
                        edit.modulationControlLink = current;
                }
            }

            if (succeeded)
            {
                modulationStatus = forward
                    ? "Modulation control link removed"
                    : "Modulation control link restored";
                scheduleBoardUpdate (false);
            }
            completion (succeeded);
            return;
        }

        case BoardEdit::Kind::changeModulationLinkAmount:
        {
            const auto iterator = std::find_if (
                modulationLinks.begin(),
                modulationLinks.end(),
                [&edit] (const auto& link)
                {
                    return link->id == edit.modulationLink.id;
                });
            if (iterator != modulationLinks.end())
            {
                (*iterator)->amount.store (
                    forward ? edit.newModulationLinkAmount
                            : edit.oldModulationLinkAmount,
                    std::memory_order_relaxed);
                modulationStatus = forward
                    ? "Modulation amount restored"
                    : "Modulation amount undone";
                scheduleBoardUpdate (false);
                completion (true);
                return;
            }

            const auto controlIterator = std::find_if (
                modulationControlLinks.begin(),
                modulationControlLinks.end(),
                [&edit] (const auto& link)
                {
                    return link->id == edit.modulationLink.id;
                });
            if (controlIterator == modulationControlLinks.end()
                || (*controlIterator)->target
                       == ModulationControlTarget::trigger)
            {
                completion (false);
                return;
            }

            (*controlIterator)->amount.store (
                forward ? edit.newModulationLinkAmount
                        : edit.oldModulationLinkAmount,
                std::memory_order_relaxed);
            modulationStatus = forward
                ? "Modulation amount restored"
                : "Modulation amount undone";
            scheduleBoardUpdate (false);
            completion (true);
            return;
        }

        case BoardEdit::Kind::compound:
        {
            beginBoardUpdateBatch();
            applyCompoundHistoryEdit (
                edit,
                forward,
                0,
                [this, completion = std::move (completion)]
                (bool success) mutable
                {
                    endBoardUpdateBatch();
                    completion (success);
                });
            return;
        }
    }
}

void BoardAudioEngine::applyCompoundHistoryEdit (
    BoardEdit& edit,
    bool forward,
    size_t childOffset,
    HistoryCompletion completion)
{
    if (childOffset >= edit.compoundEdits.size())
    {
        completion (true);
        return;
    }

    const auto childIndex =
        forward
            ? childOffset
            : edit.compoundEdits.size() - 1 - childOffset;
    applyHistoryEdit (
        edit.compoundEdits[childIndex],
        forward,
        [this, &edit, forward, childOffset,
         completion = std::move (completion)] (bool success) mutable
        {
            if (! success)
            {
                rollbackCompoundHistoryEdit (
                    edit,
                    forward,
                    childOffset,
                    [completion = std::move (completion)]
                    (bool rollbackSucceeded) mutable
                    {
                        if (! rollbackSucceeded)
                            juce::Logger::writeToLog (
                                "TB_Board compound history rollback failed.");
                        completion (false);
                    });
                return;
            }

            applyCompoundHistoryEdit (
                edit,
                forward,
                childOffset + 1,
                std::move (completion));
        });
}

void BoardAudioEngine::rollbackCompoundHistoryEdit (
    BoardEdit& edit,
    bool forward,
    size_t completedChildCount,
    HistoryCompletion completion)
{
    if (completedChildCount == 0)
    {
        completion (true);
        return;
    }

    const auto completedOffset = completedChildCount - 1;
    const auto childIndex =
        forward
            ? completedOffset
            : edit.compoundEdits.size() - 1 - completedOffset;
    applyHistoryEdit (
        edit.compoundEdits[childIndex],
        ! forward,
        [this, &edit, forward, completedOffset,
         completion = std::move (completion)] (bool success) mutable
        {
            if (! success)
            {
                completion (false);
                return;
            }

            rollbackCompoundHistoryEdit (
                edit,
                forward,
                completedOffset,
                std::move (completion));
        });
}

void BoardAudioEngine::recordPerformedEdit (BoardEdit edit)
{
    if (historyCursor < editHistory.size())
        editHistory.erase (
            editHistory.begin()
                + static_cast<std::ptrdiff_t> (historyCursor),
            editHistory.end());

    editHistory.push_back (std::move (edit));
    historyCursor = editHistory.size();

    const std::function<size_t (const BoardEdit&)>
        getStoredStateBytes =
            [&getStoredStateBytes] (const BoardEdit& storedEdit)
            {
                auto bytes =
                    storedEdit.savedNode.childState.getSize()
                    + storedEdit.savedNode.parameterValues.size()
                          * sizeof (float);
                for (const auto& child : storedEdit.compoundEdits)
                    bytes += getStoredStateBytes (child);
                return bytes;
            };

    size_t storedStateBytes = 0;
    for (const auto& storedEdit : editHistory)
        storedStateBytes += getStoredStateBytes (storedEdit);

    while (editHistory.size() > 1
           && (editHistory.size() > maximumHistoryEntries
               || storedStateBytes > maximumHistoryStateBytes))
    {
        storedStateBytes -= getStoredStateBytes (editHistory.front());
        editHistory.erase (editHistory.begin());
    }

    historyCursor = editHistory.size();

    sendChangeMessage();
}

void BoardAudioEngine::clearHistory()
{
    historyOperationSerial.fetch_add (1, std::memory_order_acq_rel);
    pendingModulationChange.reset();
    activeModulationLinkAmountEdit.reset();
    editHistory.clear();
    historyCursor = 0;
    historyOperationBusy = false;
    sendChangeMessage();
}

void BoardAudioEngine::finishExternalStateRestore (juce::uint64 generation)
{
    if (generation != restoreGeneration.load (std::memory_order_relaxed))
        return;

    externalStateRestorePending.store (false, std::memory_order_release);
    sendChangeMessage();
}

void BoardAudioEngine::failStateRestore (juce::uint64 generation,
                                         const juce::String& reason)
{
    logRestoreFailure (reason);
    finishExternalStateRestore (generation);
}

std::vector<juce::AudioProcessorGraph::Connection>
BoardAudioEngine::makeStereoConnections (NodeID source,
                                          NodeID destination) const
{
    const auto sourceNode = graph.getNodeForId (source);
    const auto destinationNode = graph.getNodeForId (destination);

    if (sourceNode == nullptr || destinationNode == nullptr)
        return {};

    const auto sourceChannels =
        sourceNode->getProcessor()->getMainBusNumOutputChannels();
    const auto destinationChannels =
        destinationNode->getProcessor()->getMainBusNumInputChannels();

    if (sourceChannels < 2 || destinationChannels < 2)
        return {};

    return {
        { { source, 0 }, { destination, 0 } },
        { { source, 1 }, { destination, 1 } }
    };
}

bool BoardAudioEngine::isInternalConnection (
    const juce::AudioProcessorGraph::Connection& connection) const noexcept
{
    return connection.source.nodeID == physicalInputNodeID
        && connection.destination.nodeID == inputNodeID;
}

std::vector<juce::AudioProcessorGraph::Connection>
BoardAudioEngine::makeInputSourceConnections (int sourceIndex,
                                               NodeID destination) const
{
    const auto destinationNode = graph.getNodeForId (destination);
    if (destinationNode == nullptr
        || destinationNode->getProcessor()->getMainBusNumInputChannels() < 2)
        return {};

    if (sourceIndex == stereoInputSource)
        return makeStereoConnections (inputNodeID, destination);

    const auto firstSourceChannel = sourceIndex == firstMonoInputSource
                                        ? 2
                                        : sourceIndex == secondMonoInputSource
                                              ? 4
                                              : -1;
    if (firstSourceChannel < 0)
        return {};

    return {
        { { inputNodeID, firstSourceChannel }, { destination, 0 } },
        { { inputNodeID, firstSourceChannel + 1 }, { destination, 1 } }
    };
}

void BoardAudioEngine::scheduleBoardUpdate (bool graphNeedsRebuild)
{
    if (boardUpdateBatchDepth > 0)
    {
        batchedBoardUpdatePending = true;
        batchedGraphRebuildPending =
            batchedGraphRebuildPending || graphNeedsRebuild;
        return;
    }

    boardRevision.fetch_add (1, std::memory_order_relaxed);

    if (graphNeedsRebuild)
        graphRebuildPending.store (true, std::memory_order_relaxed);

    stateSnapshotPending.store (true, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void BoardAudioEngine::beginBoardUpdateBatch()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    ++boardUpdateBatchDepth;
}

void BoardAudioEngine::endBoardUpdateBatch()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    jassert (boardUpdateBatchDepth > 0);
    if (boardUpdateBatchDepth <= 0)
        return;

    --boardUpdateBatchDepth;
    if (boardUpdateBatchDepth > 0 || ! batchedBoardUpdatePending)
        return;

    const auto graphNeedsRebuild = batchedGraphRebuildPending;
    batchedBoardUpdatePending = false;
    batchedGraphRebuildPending = false;
    scheduleBoardUpdate (graphNeedsRebuild);
}

void BoardAudioEngine::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (catalogWritable)
        savePluginList();

    sendChangeMessage();
}

void BoardAudioEngine::audioProcessorParameterChanged (
    juce::AudioProcessor* processor,
    int parameterIndex,
    float value)
{
    if (activeModulationWriter == this
        || updateModulationTargetFromUser (
            processor, parameterIndex, value, false, false))
        return;

    queueModulationLearnCandidate (
        processor, parameterIndex, false);
    boardRevision.fetch_add (1, std::memory_order_relaxed);
    stateSnapshotPending.store (true, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void BoardAudioEngine::audioProcessorChanged (
    juce::AudioProcessor* processor,
    const juce::AudioProcessorListener::ChangeDetails& details)
{
    if (activeModulationWriter == this)
        return;

    if (details.programChanged)
        refreshModulationTargetBasesForProcessor (processor);

    if (details.latencyChanged)
        graphRebuildPending.store (true, std::memory_order_relaxed);

    if (details.latencyChanged
        || details.programChanged
        || details.nonParameterStateChanged)
        stateSnapshotPending.store (true, std::memory_order_relaxed);

    triggerAsyncUpdate();
}

void BoardAudioEngine::audioProcessorParameterChangeGestureBegin (
    juce::AudioProcessor* processor,
    int parameterIndex)
{
    if (activeModulationWriter == this)
        return;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (
            parameterIndex, parameters.size()))
        return;

    updateModulationTargetFromUser (
        processor,
        parameterIndex,
        parameters.getUnchecked (parameterIndex)->getValue(),
        true,
        false);
    queueModulationLearnCandidate (
        processor, parameterIndex, true);
}

void BoardAudioEngine::audioProcessorParameterChangeGestureEnd (
    juce::AudioProcessor* processor,
    int parameterIndex)
{
    if (activeModulationWriter == this)
        return;

    const auto& parameters = processor->getParameters();
    if (! juce::isPositiveAndBelow (
            parameterIndex, parameters.size()))
        return;

    updateModulationTargetFromUser (
        processor,
        parameterIndex,
        parameters.getUnchecked (parameterIndex)->getValue(),
        false,
        true);
}

void BoardAudioEngine::handleAsyncUpdate()
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    reapRetiredModulationRuntimes();
    const auto modulationChanged =
        completePendingModulationLearn();
    const auto graphChanged =
        graphRebuildPending.exchange (false, std::memory_order_acq_rel);
    const auto stateChanged =
        stateSnapshotPending.exchange (false, std::memory_order_acq_rel);

    if (graphChanged)
    {
        graph.rebuild();
        updateLatencyAndBypassCapacity();
    }

    if (stateChanged)
        rebuildStateCache();

    if (graphChanged || stateChanged || modulationChanged)
    {
        sendChangeMessage();

        if (graphChanged || stateChanged)
            updateHostDisplay (
                juce::AudioProcessorListener::ChangeDetails {}
                    .withNonParameterStateChanged (true));
    }
}

void BoardAudioEngine::setAgentStatusText (const juce::String& status)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    {
        const juce::ScopedLock lock (agentStatusLock);
        if (agentStatusText == status)
            return;

        agentStatusText = status;
    }

    sendChangeMessage();
}
