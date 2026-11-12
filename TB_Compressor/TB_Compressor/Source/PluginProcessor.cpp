//==============================================================================
// PluginProcessor.cpp
//==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <array>

namespace
{
constexpr const char* presetRootTag = "TB_COMPRESSOR_PRESET";
constexpr const char* currentProgramProperty = "currentProgram";
constexpr const char* saturationAnalogModelId = "SAT_ANALOG_MODEL";

enum ParameterIndex : size_t
{
    inputIndex,
    thresholdIndex,
    ratioIndex,
    attackIndex,
    releaseIndex,
    makeupIndex,
    lookaheadIndex,
    outputIndex,
    kneeIndex,
    rmsIndex,
    stereoLinkIndex,
    saturationIndex,
    saturationEnabledIndex,
    saturationDriveIndex,
    saturationMixIndex,
    sidechainLowCutIndex,
    sidechainHighCutIndex,
    oversamplingIndex
};

constexpr std::array<const char*, 18> presetParameterIds {
    "INPUT", "THRESHOLD", "RATIO", "ATTACK", "RELEASE", "MAKEUP",
    "LOOKAHEAD", "OUTPUT", "KNEE", "RMS", "STEREO_LINK", "SATURATION",
    "SAT_ENABLED", "SAT_DRIVE", "SAT_MIX", "SC_LOW_CUT", "SC_HIGH_CUT",
    "OVERSAMPLING"
};

struct FactoryPreset
{
    const char* name;
    std::array<float, presetParameterIds.size()> values;
};

constexpr std::array<FactoryPreset, 5> factoryPresets {{
    { "Clean Control",  { 0.0f, -18.0f, 2.0f, 10.0f, 100.0f, 0.0f, 0.0f, 0.0f, 6.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.00f,  20.0f, 20000.0f, 0.0f } },
    { "Vocal Leveler",  { 0.0f, -22.0f, 3.0f, 12.0f, 110.0f, 3.0f, 1.0f, 0.0f, 6.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.3f, 0.18f,  90.0f, 12000.0f, 1.0f } },
    { "Drum Punch",     { 0.0f, -14.0f, 4.0f, 25.0f,  70.0f, 2.0f, 0.0f, 0.0f, 2.0f, 0.0f, 1.0f, 3.0f, 1.0f, 1.8f, 0.25f,  50.0f, 16000.0f, 1.0f } },
    { "Bus Glue",       { 0.0f, -10.0f, 2.0f, 30.0f, 150.0f, 1.0f, 0.0f, 0.0f, 4.0f, 1.0f, 1.0f, 4.0f, 1.0f, 1.2f, 0.12f,  40.0f, 18000.0f, 1.0f } },
    { "Sidechain Duck", { 0.0f, -28.0f, 6.0f,  2.0f, 180.0f, 0.0f, 3.0f, 0.0f, 4.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.00f, 120.0f,  8000.0f, 1.0f } }
}};

bool containsEveryPresetParameter(const juce::ValueTree& state)
{
    const juce::Identifier idProperty("id");

    for (const auto* parameterId : presetParameterIds)
        if (! state.getChildWithProperty(idProperty, parameterId).isValid())
            return false;

    return true;
}

void addMissingAnalogModelParameter(juce::ValueTree& state,
                                    const juce::ValueTree& currentState)
{
    const juce::Identifier idProperty("id");
    const juce::Identifier valueProperty("value");

    if (state.getChildWithProperty(idProperty, saturationAnalogModelId).isValid())
        return;

    const auto templateNode = currentState.getChildWithProperty(idProperty,
                                                                 saturationAnalogModelId);
    if (! templateNode.isValid())
        return;

    auto node = templateNode.createCopy();
    node.setProperty(valueProperty, 0.0f, nullptr);
    state.appendChild(node, nullptr);
}
}

// kOsFactors 정의 (헤더에서 선언한 constexpr 배열)
constexpr int TB_CompressorAudioProcessor::kOsFactors[];

//==============================================================================
TB_CompressorAudioProcessor::TB_CompressorAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
              .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    // 각 오버샘플러에 factor 설정 (prepare 전에 반드시 호출)
    for (int i = 0; i < kNumOversamplers; ++i)
        oversamplers[i].setFactor(kOsFactors[i]);

    for (size_t index = 0; index < presetParameterIds.size(); ++index)
    {
        parameterValues[index] = apvts.getRawParameterValue(presetParameterIds[index]);
        jassert(parameterValues[index] != nullptr);
    }

    saturationAnalogModelValue = apvts.getRawParameterValue(saturationAnalogModelId);
    jassert(saturationAnalogModelValue != nullptr);
}

TB_CompressorAudioProcessor::~TB_CompressorAudioProcessor()
{
    stopTimer();
    const juce::ScopedLock lock(latencyCoordinatorLock);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
TB_CompressorAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto ratioRange = juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f);
    ratioRange.setSkewForCentre(4.0f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "INPUT",     "Input",      -24.0f,   24.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "THRESHOLD", "Threshold",  -60.0f,    0.0f, -18.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RATIO",     "Ratio",        ratioRange,       2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ATTACK",    "Attack",       0.1f,  200.0f,  10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RELEASE",   "Release",      5.0f, 1000.0f, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MAKEUP",    "Makeup",       0.0f,   24.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LOOKAHEAD", "Lookahead",    0.0f,   20.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OUTPUT",    "Output",     -24.0f,   24.0f,   0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "KNEE",      "Knee",         0.0f,    6.0f,   6.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "RMS",        "RMS",         false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "STEREO_LINK","Stereo Link", true));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SATURATION", "Saturation",
        juce::StringArray{ "Clean", "Soft", "Hard", "Punch", "Warm", "Tape" },
        0));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "SAT_ENABLED", "Saturation Enabled", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SAT_DRIVE", "Saturation Drive", 0.0f, 4.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SAT_MIX", "Saturation Mix", 0.0f, 1.0f, 1.0f));

    auto sidechainLowCutRange = juce::NormalisableRange<float>(20.0f, 10000.0f, 1.0f);
    sidechainLowCutRange.setSkewForCentre(200.0f);
    auto sidechainHighCutRange = juce::NormalisableRange<float>(100.0f, 20000.0f, 1.0f);
    sidechainHighCutRange.setSkewForCentre(3000.0f);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SC_LOW_CUT", "Sidechain Low Cut", sidechainLowCutRange, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SC_HIGH_CUT", "Sidechain High Cut", sidechainHighCutRange, 20000.0f));

    // OVERSAMPLING: 0=1x(Off), 1=2x, 2=4x, 3=8x
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "OVERSAMPLING", "Oversampling",
        juce::StringArray{ "1x (Off)", "2x", "4x", "8x" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        saturationAnalogModelId, "Saturator Circuit Model",
        juce::StringArray{ "Legacy", "XXL", "LX-2X", "FET-X76", "FX-67X" },
        0));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String TB_CompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}
bool   TB_CompressorAudioProcessor::acceptsMidi()  const { return false; }
bool   TB_CompressorAudioProcessor::producesMidi() const { return false; }
bool   TB_CompressorAudioProcessor::isMidiEffect() const { return false; }
double TB_CompressorAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int TB_CompressorAudioProcessor::getNumPrograms()
{
    return static_cast<int>(factoryPresets.size());
}

int TB_CompressorAudioProcessor::getCurrentProgram()
{
    return juce::jlimit(0, getNumPrograms() - 1, currentProgram.load());
}

void TB_CompressorAudioProcessor::setCurrentProgram(int index)
{
    loadFactoryPreset(index);
}

const juce::String TB_CompressorAudioProcessor::getProgramName(int index)
{
    if (juce::isPositiveAndBelow(index, getNumPrograms()))
        return factoryPresets[static_cast<size_t>(index)].name;

    return {};
}

void         TB_CompressorAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void TB_CompressorAudioProcessor::prepareToPlay(double sampleRate,
                                                 int samplesPerBlock)
{
    stopTimer();
    const juce::ScopedLock coordinatorLock(latencyCoordinatorLock);

    const int numChannels = getTotalNumOutputChannels();
    constexpr int sidechainScratchSamples = 32768;
    mSampleRate = sampleRate;
    sidechainScratch.setSize(numChannels, sidechainScratchSamples, false, false, true);
    sidechainScratch.clear();

    const juce::dsp::ProcessSpec sidechainSpec {
        sampleRate,
        static_cast<juce::uint32>(sidechainScratchSamples),
        static_cast<juce::uint32>(numChannels)
    };
    sidechainHighPass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    sidechainLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    sidechainHighPass.prepare(sidechainSpec);
    sidechainLowPass.prepare(sidechainSpec);
    sidechainHighPass.reset();
    sidechainLowPass.reset();
    sidechainWasEnabled = false;
    lastSidechainLowCut = -1.0f;
    lastSidechainHighCut = -1.0f;

    for (int i = 0; i < kNumOversamplers; ++i)
    {
        oversamplers[i].prepare(sampleRate, samplesPerBlock, numChannels);
        sidechainOversamplers[i].prepare(sampleRate, samplesPerBlock, numChannels);

        const int factor = kOsFactors[i];
        compressors[i].prepare(sampleRate * factor,
                               samplesPerBlock * factor,
                               numChannels);
        saturations[i].prepare(sampleRate * factor,
                               samplesPerBlock * factor,
                               numChannels);
    }

    mCurrentOsIndex = juce::jlimit(
        0, kNumOversamplers - 1,
        static_cast<int>(parameterValues[oversamplingIndex]->load(std::memory_order_relaxed)));
    requestedOsIndex.store(mCurrentOsIndex, std::memory_order_relaxed);
    latencyApprovedOsIndex.store(mCurrentOsIndex, std::memory_order_relaxed);

    for (int index = 0; index < kNumOversamplers; ++index)
        latencySamplesByOsIndex[static_cast<size_t>(index)].store(
            getLatencySamplesForIndex(index), std::memory_order_relaxed);

    updateDSPParameters(true);
    setLatencySamples(getLatencySamplesForIndex(mCurrentOsIndex));
    startTimerHz(60);
}

void TB_CompressorAudioProcessor::releaseResources()
{
    stopTimer();
    const juce::ScopedLock lock(latencyCoordinatorLock);
}

//==============================================================================
void TB_CompressorAudioProcessor::applyOversamplingIndex(int newIndex)
{
    if (newIndex == mCurrentOsIndex)
        return;

    mCurrentOsIndex = newIndex;
    compressors[newIndex].reset();
    saturations[newIndex].reset();
    oversamplers[newIndex].reset();
    sidechainOversamplers[newIndex].reset();
}

int TB_CompressorAudioProcessor::getLatencySamplesForIndex(int index) const
{
    return oversamplers[index].getLatencySamples()
        + compressors[index].getMaxDelaySamples() / kOsFactors[index];
}

void TB_CompressorAudioProcessor::timerCallback()
{
    const juce::ScopedLock lock(latencyCoordinatorLock);
    constexpr int maxCoalescedAttempts = 2;

    for (int attempt = 0; attempt < maxCoalescedAttempts; ++attempt)
    {
        const auto requested = juce::jlimit(
            0, kNumOversamplers - 1,
            requestedOsIndex.load(std::memory_order_acquire));

        if (requested == latencyApprovedOsIndex.load(std::memory_order_relaxed))
            return;

        setLatencySamples(
            latencySamplesByOsIndex[static_cast<size_t>(requested)].load(
                std::memory_order_relaxed));

        if (requested == requestedOsIndex.load(std::memory_order_acquire))
        {
            latencyApprovedOsIndex.store(requested, std::memory_order_release);
            return;
        }
    }
}

//==============================================================================
bool TB_CompressorAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    const auto sidechain = layouts.getChannelSet(true, 1);

    if (mainInput != mainOutput
        || (mainOutput != juce::AudioChannelSet::mono()
            && mainOutput != juce::AudioChannelSet::stereo()))
    {
        return false;
    }

    return sidechain.isDisabled()
        || sidechain == juce::AudioChannelSet::mono()
        || sidechain == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TB_CompressorAudioProcessor::updateDSPParameters(bool snapToTarget)
{
    const auto loadParameter = [this](ParameterIndex index)
    {
        return parameterValues[static_cast<size_t>(index)]->load(std::memory_order_relaxed);
    };

    const int newOsIndex = juce::jlimit(
        0, kNumOversamplers - 1,
        static_cast<int>(loadParameter(oversamplingIndex)));

    requestedOsIndex.store(newOsIndex, std::memory_order_release);

    if (! snapToTarget
        && latencyApprovedOsIndex.load(std::memory_order_acquire) == newOsIndex)
    {
        applyOversamplingIndex(newOsIndex);
    }

    // ── 컴프레서 파라미터 ────────────────────────────────────────────────
    CompressorParameters cp;
    cp.input      = loadParameter(inputIndex);
    cp.threshold  = loadParameter(thresholdIndex);
    cp.ratio      = loadParameter(ratioIndex);
    cp.attack     = loadParameter(attackIndex);
    cp.release    = loadParameter(releaseIndex);
    cp.makeup     = loadParameter(makeupIndex);
    cp.lookahead  = loadParameter(lookaheadIndex);
    cp.output     = loadParameter(outputIndex);
    cp.knee       = loadParameter(kneeIndex);
    cp.rmsMode    = loadParameter(rmsIndex) > 0.5f;
    cp.stereoLink = loadParameter(stereoLinkIndex) > 0.5f;

    if (snapToTarget)
    {
        for (auto& compressor : compressors)
            compressor.updateParameters(cp, true);
    }
    else
    {
        compressors[mCurrentOsIndex].updateParameters(cp, false);
    }

    // ── 새츄레이션 파라미터 ──────────────────────────────────────────────
    SaturationParameters sp;
    const bool saturationEnabled = loadParameter(saturationEnabledIndex) > 0.5f;
    sp.mode  = saturationEnabled
        ? static_cast<int>(loadParameter(saturationIndex))
        : 0;
    sp.analogModel = saturationEnabled && saturationAnalogModelValue != nullptr
        ? juce::roundToInt(saturationAnalogModelValue->load(std::memory_order_relaxed))
        : 0;
    sp.drive = loadParameter(saturationDriveIndex);
    sp.mix   = loadParameter(saturationMixIndex);

    if (snapToTarget)
    {
        for (auto& saturation : saturations)
            saturation.updateParameters(sp);
    }
    else
    {
        saturations[mCurrentOsIndex].updateParameters(sp);
    }
}

//==============================================================================
void TB_CompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() == 0)
        return;

    updateDSPParameters();
    const bool meteringActive = meteringDemandCount.load(std::memory_order_relaxed) > 0;

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);

    // ── 입력 레벨 미터 ─────────────────────────────────────────────────
    if (meteringActive)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
            peak = std::max(peak, mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples()));

        currentInputLevel.store(
            juce::Decibels::gainToDecibels(
                peak * juce::Decibels::decibelsToGain(
                    parameterValues[inputIndex]->load(std::memory_order_relaxed)),
                -60.0f));
    }

    // ── 오버샘플링 업 ──────────────────────────────────────────────────
    auto& os = oversamplers[mCurrentOsIndex];
    auto& sidechainOs = sidechainOversamplers[mCurrentOsIndex];
    auto& compressor = compressors[mCurrentOsIndex];
    auto& saturation = saturations[mCurrentOsIndex];

    juce::dsp::AudioBlock<float> outputBlock(mainBuffer);
    auto osBlock = os.processUp(mainBuffer);

    const bool sidechainEnabled =
        sidechainBuffer.getNumChannels() > 0
        && sidechainBuffer.getNumSamples() == mainBuffer.getNumSamples()
        && mainBuffer.getNumSamples() <= sidechainScratch.getNumSamples();

    if (sidechainEnabled != sidechainWasEnabled)
    {
        sidechainHighPass.reset();
        sidechainLowPass.reset();
        sidechainWasEnabled = sidechainEnabled;
    }

    if (sidechainEnabled)
    {
        const int numSamples = mainBuffer.getNumSamples();
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            const int sourceChannel = juce::jmin(ch, sidechainBuffer.getNumChannels() - 1);
            sidechainScratch.copyFrom(ch, 0, sidechainBuffer, sourceChannel, 0, numSamples);
        }

        // Keep the filter range valid even when a host prepares an unusually
        // low sample rate, and keep the high cut above the low cut so an
        // inverted automation gesture cannot mute the detector completely.
        const float maxCutoff = juce::jmax (20.0f,
                                            static_cast<float> (mSampleRate * 0.45));
        const float lowCut = juce::jlimit(
            20.0f, maxCutoff,
            parameterValues[sidechainLowCutIndex]->load(std::memory_order_relaxed));
        const float highCut = juce::jmax (
            lowCut,
            juce::jlimit (
                20.0f, maxCutoff,
                parameterValues[sidechainHighCutIndex]->load(std::memory_order_relaxed)));

        if (lowCut != lastSidechainLowCut)
        {
            sidechainHighPass.setCutoffFrequency(lowCut);
            lastSidechainLowCut = lowCut;
        }

        if (highCut != lastSidechainHighCut)
        {
            sidechainLowPass.setCutoffFrequency(highCut);
            lastSidechainHighCut = highCut;
        }

        juce::AudioBuffer<float> sidechainProcessBuffer(
            sidechainScratch.getArrayOfWritePointers(),
            mainBuffer.getNumChannels(),
            numSamples);
        juce::dsp::AudioBlock<float> sidechainBlock(sidechainProcessBuffer);
        juce::dsp::ProcessContextReplacing<float> sidechainContext(sidechainBlock);
        sidechainHighPass.process(sidechainContext);
        sidechainLowPass.process(sidechainContext);

        auto sidechainOsBlock = sidechainOs.processUp(sidechainProcessBuffer);
        compressor.process(osBlock, &sidechainOsBlock, meteringActive);
    }
    else
    {
        compressor.process(osBlock, nullptr, meteringActive);
    }

    saturation.process(osBlock);   // 2. 새츄레이션

    // ── 오버샘플링 다운 ────────────────────────────────────────────────
    os.processDown(outputBlock);

    // ── GR / 출력 레벨 미터 업데이트 ───────────────────────────────────
    if (meteringActive)
    {
        currentGR.store(compressor.getCurrentGR(), std::memory_order_relaxed);

        float outPeak = 0.0f;
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
            outPeak = std::max(outPeak, mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples()));

        currentOutputLevel.store(juce::Decibels::gainToDecibels(outPeak, -60.0f),
                                 std::memory_order_relaxed);
    }
}

void TB_CompressorAudioProcessor::setMeteringDemand(bool shouldMeter) noexcept
{
    if (shouldMeter)
    {
        meteringDemandCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const auto previous = meteringDemandCount.fetch_sub(1, std::memory_order_relaxed);
    jassert(previous > 0);
    if (previous <= 0)
        meteringDemandCount.store(0, std::memory_order_relaxed);
}

//==============================================================================
bool TB_CompressorAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* TB_CompressorAudioProcessor::createEditor()
{
    return new TB_CompressorAudioProcessorEditor(*this);
}

//==============================================================================
void TB_CompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty(currentProgramProperty, currentProgram.load(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TB_CompressorAudioProcessor::setStateInformation(const void* data,
                                                       int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(
        getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr
        && xmlState->hasTagName(apvts.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml(*xmlState);
        const auto restoredProgram = juce::jlimit(
            0,
            getNumPrograms() - 1,
            static_cast<int>(restoredState.getProperty(currentProgramProperty, 0)));
        addMissingAnalogModelParameter(restoredState, apvts.state);
        apvts.replaceState(restoredState);
        currentProgram.store(restoredProgram);
    }
}

juce::StringArray TB_CompressorAudioProcessor::getFactoryPresetNames()
{
    juce::StringArray names;

    for (const auto& preset : factoryPresets)
        names.add(preset.name);

    return names;
}

bool TB_CompressorAudioProcessor::loadFactoryPreset(int index)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(factoryPresets.size())))
        return false;

    const auto& preset = factoryPresets[static_cast<size_t>(index)];

    for (size_t i = 0; i < presetParameterIds.size(); ++i)
    {
        if (auto* parameter = apvts.getParameter(presetParameterIds[i]))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(preset.values[i]));
            parameter->endChangeGesture();
        }
    }

    if (auto* parameter = apvts.getParameter(saturationAnalogModelId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
        parameter->endChangeGesture();
    }

    currentProgram.store(index);
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}
                          .withNonParameterStateChanged(true));
    return true;
}

bool TB_CompressorAudioProcessor::savePresetToFile(const juce::File& file)
{
    auto stateXml = apvts.copyState().createXml();

    if (stateXml == nullptr)
        return false;

    juce::XmlElement presetXml(presetRootTag);
    presetXml.setAttribute("formatVersion", 1);
    presetXml.addChildElement(stateXml.release());
    return presetXml.writeTo(file);
}

bool TB_CompressorAudioProcessor::loadPresetFromFile(const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto presetXml = juce::XmlDocument::parse(file);

    if (presetXml == nullptr
        || ! presetXml->hasTagName(presetRootTag)
        || presetXml->getIntAttribute("formatVersion") != 1)
        return false;

    const auto* stateXml = presetXml->getChildByName(apvts.state.getType().toString());

    if (stateXml == nullptr)
        return false;

    auto newState = juce::ValueTree::fromXml(*stateXml);

    if (! newState.isValid()
        || newState.getType() != apvts.state.getType()
        || ! containsEveryPresetParameter(newState))
        return false;

    addMissingAnalogModelParameter(newState, apvts.state);
    apvts.replaceState(newState);
    undoManager.clearUndoHistory();
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails{}
                          .withNonParameterStateChanged(true));
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB_CompressorAudioProcessor();
}
