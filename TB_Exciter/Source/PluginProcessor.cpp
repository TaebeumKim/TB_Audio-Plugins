#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr auto stateRootType    = "TB_ExciterState";
    constexpr auto stateVersionKey  = "stateSchemaVersion";
    constexpr auto parameterTreeKey = "PARAMETERS";
    constexpr auto presetVersionKey = "presetFormatVersion";
    constexpr auto factoryPresetKey = "factoryPresetId";
}

TB_ExciterAudioProcessor::TB_ExciterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMETERS", tbx::params::createLayout())
{
    tuneParam      = apvts.getRawParameterValue (tbx::params::tuneId);
    amountParam    = apvts.getRawParameterValue (tbx::params::amountId);
    characterParam = apvts.getRawParameterValue (tbx::params::characterId);
    widthParam     = apvts.getRawParameterValue (tbx::params::widthId);
    outputParam    = apvts.getRawParameterValue (tbx::params::outputId);

    jassert (tuneParam != nullptr && amountParam != nullptr && characterParam != nullptr
             && widthParam != nullptr && outputParam != nullptr);
}

ExciterEngine::Settings TB_ExciterAudioProcessor::readSettings() const noexcept
{
    ExciterEngine::Settings settings;
    settings.tuneHz    = tuneParam->load();
    settings.amount    = amountParam->load()    * 0.01f;
    settings.character = characterParam->load() * 0.01f;
    settings.width     = widthParam->load()     * 0.01f;
    return settings;
}

float TB_ExciterAudioProcessor::readOutputGain() const noexcept
{
    const auto levelDb = outputParam->load();

    // NaN/Inf가 출력 게인으로 번지면 버퍼 전체가 오염된다. 엔진의 방어와 같은 이유다.
    return juce::Decibels::decibelsToGain (std::isfinite (levelDb)
                                               ? juce::jlimit (-12.0f, 12.0f, levelDb)
                                               : tbx::params::outputDefaultDb);
}

namespace
{
    /*  상태 XML의 속성은 문자열로 들어온다. "nan", "inf", 빈 값과 숫자가 아닌 텍스트는
        파라미터에 도달하기 전에 걸러 상태 전체를 거부한다. */
    bool isFiniteNumber (const juce::var& value)
    {
        if (value.isString())
        {
            const auto text = value.toString().trim();

            if (text.isEmpty() || ! text.containsOnly ("+-.0123456789eE"))
                return false;
        }
        else if (! (value.isInt() || value.isInt64() || value.isDouble()))
        {
            return false;
        }

        return std::isfinite ((double) value);
    }

    bool hasOnlyFiniteParameterValues (const juce::ValueTree& parameters)
    {
        for (const auto& child : parameters)
            if (child.hasProperty ("value") && ! isFiniteNumber (child.getProperty ("value")))
                return false;

        return true;
    }
}

void TB_ExciterAudioProcessor::setParameterFromPreset (const char* parameterId,
                                                        float actualValue)
{
    auto* parameter = apvts.getParameter (parameterId);
    jassert (parameter != nullptr);

    if (parameter == nullptr)
        return;

    const auto normalisedValue = parameter->convertTo0to1 (actualValue);

    if (juce::approximatelyEqual (parameter->getValue(), normalisedValue))
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (normalisedValue);
    parameter->endChangeGesture();
}

bool TB_ExciterAudioProcessor::applyFactoryPreset (int presetId)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    const auto* preset = tbx::presets::findById (presetId);

    if (preset == nullptr)
        return false;

    undoManager.beginNewTransaction (juce::String ("Preset: ") + preset->name);
    setParameterFromPreset (tbx::params::amountId, preset->bright);
    setParameterFromPreset (tbx::params::characterId, preset->clarity);
    setParameterFromPreset (tbx::params::tuneId, preset->focusHz);
    setParameterFromPreset (tbx::params::widthId, preset->width);
    setParameterFromPreset (tbx::params::outputId, preset->levelDb);
    activeFactoryPresetId.store (preset->id);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails {}
                           .withNonParameterStateChanged (true));
    return true;
}

bool TB_ExciterAudioProcessor::matchesFactoryPreset (int presetId) const noexcept
{
    const auto* preset = tbx::presets::findById (presetId);

    return preset != nullptr
           && tbx::presets::findMatch (amountParam->load(), characterParam->load(),
                                       tuneParam->load(), widthParam->load(),
                                       outputParam->load()) == preset->id;
}

void TB_ExciterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = juce::jmax (1, getTotalNumOutputChannels());

    engine.prepare (sampleRate, samplesPerBlock, numChannels);
    engine.snapSettings (readSettings());

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (readOutputGain());

    levelMix.reset (sampleRate, ExciterEngine::bypassFadeSeconds);
    levelMixPrimed = false;

    setLatencySamples (engine.getLatencySamples());
}

void TB_ExciterAudioProcessor::releaseResources()
{
    engine.reset();
    levelMixPrimed = false;
}

void TB_ExciterAudioProcessor::reset()
{
    engine.reset();
    engine.snapSettings (readSettings());
    outputGain.setCurrentAndTargetValue (readOutputGain());
    levelMixPrimed = false;
}

bool TB_ExciterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainInput == mainOutput;
}

void TB_ExciterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio (buffer, false);
}

void TB_ExciterAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                     juce::MidiBuffer&)
{
    processAudio (buffer, true);
}

void TB_ExciterAudioProcessor::processAudio (juce::AudioBuffer<float>& buffer, bool bypassed) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputs  = getTotalNumInputChannels();
    const auto totalOutputs = getTotalNumOutputChannels();

    for (int channel = totalInputs; channel < totalOutputs; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (settingsSnapPending.exchange (false, std::memory_order_acq_rel))
    {
        // 상태 복원 직후: 이전 세션 값에서 미끄러져 오지 않고 복원된 값에서 바로 시작한다.
        engine.snapSettings (readSettings());
        outputGain.setCurrentAndTargetValue (readOutputGain());
    }
    else
    {
        engine.setSettings (readSettings());
        outputGain.setTargetValue (readOutputGain());
    }

    engine.process (buffer, bypassed);

    const auto numSamples = buffer.getNumSamples();
    const auto levelMixTarget = bypassed ? 0.0f : 1.0f;

    if (! levelMixPrimed)
    {
        levelMix.setCurrentAndTargetValue (levelMixTarget);
        levelMixPrimed = true;
    }
    else
    {
        levelMix.setTargetValue (levelMixTarget);
    }

    if (levelMix.isSmoothing())
    {
        /*  bypass 전환 중: LEVEL을 단위 게인과 선형으로 섞는다. 엔진의 added 페이드와
            같은 길이·같은 블록에서 시작하므로 전체가 처리음 ↔ 지연 dry의 크로스페이드가 된다. */
        for (int i = 0; i < numSamples; ++i)
        {
            const auto mix = levelMix.getNextValue();
            const auto gain = 1.0f + mix * (outputGain.getNextValue() - 1.0f);

            for (int channel = 0; channel < totalOutputs; ++channel)
                buffer.getWritePointer (channel)[i] *= gain;
        }

        return;
    }

    if (levelMix.getCurrentValue() < 0.5f)
    {
        outputGain.skip (numSamples);
        return;
    }

    if (outputGain.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto gain = outputGain.getNextValue();

            for (int channel = 0; channel < totalOutputs; ++channel)
                buffer.getWritePointer (channel)[i] *= gain;
        }
    }
    else
    {
        const auto gain = outputGain.getCurrentValue();

        if (! juce::approximatelyEqual (gain, 1.0f))
            buffer.applyGain (gain);
    }
}

juce::AudioProcessorEditor* TB_ExciterAudioProcessor::createEditor()
{
    return new TB_ExciterAudioProcessorEditor (*this);
}

void TB_ExciterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root { stateRootType };
    root.setProperty (stateVersionKey, stateSchemaVersion, nullptr);
    root.setProperty (presetVersionKey, presetFormatVersion, nullptr);
    root.setProperty (factoryPresetKey, activeFactoryPresetId.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void TB_ExciterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    juce::ValueTree incoming;

    auto restoredPresetId = 0;

    if (xml->hasTagName (stateRootType))
    {
        const auto root = juce::ValueTree::fromXml (*xml);
        const auto version = (int) root.getProperty (stateVersionKey, 0);

        // 알 수 없는 미래 스키마는 현재 상태를 건드리지 않고 거부한다.
        if (version <= 0 || version > stateSchemaVersion)
            return;

        const auto restoredPresetVersion = (int) root.getProperty (presetVersionKey, 0);
        const auto candidatePresetId = (int) root.getProperty (factoryPresetKey, 0);

        if (restoredPresetVersion == presetFormatVersion
            && tbx::presets::findById (candidatePresetId) != nullptr)
            restoredPresetId = candidatePresetId;

        incoming = root.getChildWithName (parameterTreeKey);
    }
    else if (xml->hasTagName (parameterTreeKey))
    {
        // 스키마 루트가 없던 초기 빌드의 상태.
        incoming = juce::ValueTree::fromXml (*xml);
    }

    if (! incoming.isValid() || ! incoming.hasType (parameterTreeKey))
        return;

    // 검증에 실패하면 현재 상태를 일부라도 바꾸지 않는다.
    if (! hasOnlyFiniteParameterValues (incoming))
        return;

    apvts.replaceState (incoming);
    const auto inferredPresetId = tbx::presets::findMatch (
        amountParam->load(), characterParam->load(), tuneParam->load(),
        widthParam->load(), outputParam->load());
    activeFactoryPresetId.store (restoredPresetId != 0 ? restoredPresetId
                                                       : juce::jmax (0, inferredPresetId));

    /*  엔진과 outputGain의 스무딩 상태는 오디오 스레드가 쓰고 있을 수 있다. 여기서 직접
        건드리면 data race이므로 요청만 남긴다 (성능 가이드 11장). */
    settingsSnapPending.store (true, std::memory_order_release);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB_ExciterAudioProcessor();
}
