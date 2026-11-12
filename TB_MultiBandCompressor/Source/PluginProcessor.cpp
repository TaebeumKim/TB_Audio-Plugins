#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
static_assert (std::atomic<std::uint32_t>::is_always_lock_free,
               "Quality state publication must remain lock-free on the audio thread.");

bool parseStrictNumber (const juce::var& rawValue, double& result) noexcept
{
    if (rawValue.isInt() || rawValue.isInt64() || rawValue.isDouble())
    {
        result = (double) rawValue;
        return std::isfinite (result);
    }
    if (! rawValue.isString())
        return false;
    const auto source = rawValue.toString().trim();
    if (source.isEmpty())
        return false;
    auto cursor = source.getCharPointer();
    const auto begin = cursor;
    result = juce::CharacterFunctions::readDoubleValue (cursor);
    return cursor != begin && cursor.isEmpty() && std::isfinite (result);
}

bool parseStrictInteger (const juce::var& rawValue, int& result) noexcept
{
    double number = 0.0;
    if (! parseStrictNumber (rawValue, number) || number != std::floor (number)
        || number < (double) std::numeric_limits<int>::min()
        || number > (double) std::numeric_limits<int>::max())
        return false;
    result = (int) number;
    return true;
}

bool isBandRangeParameter (std::size_t index) noexcept
{
    for (int band = 0; band < tbmbc::params::numBands; ++band)
        if (index == tbmbc::params::bandParameterIndex (
                         band, tbmbc::params::rangeOffset))
            return true;

    return false;
}

bool acceptsLegacyRangeValue (double value) noexcept
{
    constexpr auto legacyMinimum = -24.0;
    constexpr auto legacyMaximum = 24.0;
    constexpr auto interval = 0.1;
    if (! std::isfinite (value) || value < legacyMinimum - 1.0e-4
        || value > legacyMaximum + 1.0e-4)
        return false;

    const auto snapped = legacyMinimum
                         + std::round ((value - legacyMinimum) / interval) * interval;
    return std::abs (value - snapped) <= interval * 1.0e-3;
}

void clearAuditionValues (tbmbc::presets::Values& values) noexcept
{
    for (int band = 0; band < tbmbc::params::numBands; ++band)
        values.values[tbmbc::params::bandParameterIndex (
            band, tbmbc::params::auditionOffset)] = 0.0f;
}

void clearAuditionState (juce::ValueTree& state)
{
    for (auto child : state)
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (child.getProperty ("id").toString()
                == tbmbc::params::bandId (band, tbmbc::params::auditionSuffix))
                child.setProperty ("value", 0.0f, nullptr);
}

std::uint32_t packQualityState (int phaseMode, int oversampling,
                                float lookaheadMs) noexcept
{
    const auto phase = (std::uint32_t) juce::jlimit (0, 1, phaseMode);
    const auto oversamplingValue = (std::uint32_t) juce::jlimit (
        0, 3, oversampling);
    const auto lookaheadTenths = (std::uint32_t) juce::jlimit (
        0, 200, juce::roundToInt (lookaheadMs * 10.0f));
    return phase | (oversamplingValue << 1u) | (lookaheadTenths << 3u);
}

void unpackQualityState (std::uint32_t packed, int& phaseMode,
                         int& oversampling, float& lookaheadMs) noexcept
{
    phaseMode = (int) (packed & 1u);
    oversampling = (int) ((packed >> 1u) & 3u);
    lookaheadMs = (float) ((packed >> 3u) & 0xffu) * 0.1f;
}
}

TBMBCSpectrumAnalyzer::TBMBCSpectrumAnalyzer()
    : storage (std::make_unique<Storage>())
{
    magnitudesDb.fill (-120.0f);
}

void TBMBCSpectrumAnalyzer::reset() noexcept
{
    resetRequested.store (true, std::memory_order_release);
    pushedSampleCount.store (0, std::memory_order_relaxed);
    frameGeneration.store (0, std::memory_order_relaxed);
}

void TBMBCSpectrumAnalyzer::discardPending() noexcept
{
    auto samplesToDiscard = fifo.getNumReady();
    while (samplesToDiscard > 0)
    {
        int firstBlockStart = 0;
        int firstBlockSize = 0;
        int secondBlockStart = 0;
        int secondBlockSize = 0;
        fifo.prepareToRead (samplesToDiscard, firstBlockStart, firstBlockSize,
                            secondBlockStart, secondBlockSize);
        const auto discarded = firstBlockSize + secondBlockSize;
        fifo.finishedRead (discarded);
        if (discarded <= 0)
            break;
        samplesToDiscard -= discarded;
    }
}

void TBMBCSpectrumAnalyzer::push (const juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return;

    const auto samplesToWrite = juce::jmin (buffer.getNumSamples(), fifo.getFreeSpace());
    if (samplesToWrite <= 0)
        return;

    int firstBlockStart = 0;
    int firstBlockSize = 0;
    int secondBlockStart = 0;
    int secondBlockSize = 0;
    fifo.prepareToWrite (samplesToWrite, firstBlockStart, firstBlockSize,
                         secondBlockStart, secondBlockSize);
    copyToFifo (firstBlockStart, firstBlockSize, buffer, 0);
    copyToFifo (secondBlockStart, secondBlockSize, buffer, firstBlockSize);
    const auto written = firstBlockSize + secondBlockSize;
    fifo.finishedWrite (written);
    pushedSampleCount.fetch_add ((std::uint64_t) written, std::memory_order_relaxed);
}

bool TBMBCSpectrumAnalyzer::pullSpectrum()
{
    if (resetRequested.exchange (false, std::memory_order_acq_rel))
    {
        discardPending();
        for (auto& channel : storage->fftData)
            channel.fill (0.0f);
        magnitudesDb.fill (-120.0f);
        return false;
    }

    const auto readySamples = fifo.getNumReady();
    if (readySamples < fftSize)
        return false;

    const auto samplesToDiscard = readySamples - fftSize;
    if (samplesToDiscard > 0)
    {
        int firstBlockStart = 0;
        int firstBlockSize = 0;
        int secondBlockStart = 0;
        int secondBlockSize = 0;
        fifo.prepareToRead (samplesToDiscard, firstBlockStart, firstBlockSize,
                            secondBlockStart, secondBlockSize);
        fifo.finishedRead (firstBlockSize + secondBlockSize);
    }

    int firstBlockStart = 0;
    int firstBlockSize = 0;
    int secondBlockStart = 0;
    int secondBlockSize = 0;
    fifo.prepareToRead (fftSize, firstBlockStart, firstBlockSize,
                        secondBlockStart, secondBlockSize);
    for (int channel = 0; channel < 2; ++channel)
    {
        auto& destination = storage->fftData[(std::size_t) channel];
        const auto& source = storage->audioFifo[(std::size_t) channel];
        std::copy_n (source.begin() + firstBlockStart, firstBlockSize,
                     destination.begin());
        std::copy_n (source.begin() + secondBlockStart, secondBlockSize,
                     destination.begin() + firstBlockSize);
    }
    fifo.finishedRead (firstBlockSize + secondBlockSize);

    for (auto& channelData : storage->fftData)
    {
        std::fill (channelData.begin() + fftSize, channelData.end(), 0.0f);
        window.multiplyWithWindowingTable (channelData.data(), fftSize);
        fft.performRealOnlyForwardTransform (channelData.data());
    }

    constexpr auto hannWindowSum = 0.5f * (float) (fftSize - 1);
    for (int bin = 0; bin < numBins; ++bin)
    {
        auto squaredMagnitude = 0.0f;
        for (const auto& channelData : storage->fftData)
        {
            const auto real = channelData[(std::size_t) (2 * bin)];
            const auto imaginary = channelData[(std::size_t) (2 * bin + 1)];
            squaredMagnitude += real * real + imaginary * imaginary;
        }
        const auto oneSidedScale = bin == 0 || bin == numBins - 1 ? 1.0f : 2.0f;
        const auto magnitude = std::sqrt (0.5f * squaredMagnitude)
                             * oneSidedScale / hannWindowSum;
        magnitudesDb[(std::size_t) bin] = juce::jlimit (
            -120.0f, 24.0f, juce::Decibels::gainToDecibels (magnitude, -120.0f));
    }

    frameGeneration.fetch_add (1, std::memory_order_relaxed);
    return true;
}

void TBMBCSpectrumAnalyzer::copyToFifo (
    int startIndex, int numSamples, const juce::AudioBuffer<float>& buffer,
    int sourceOffset) noexcept
{
    if (numSamples <= 0)
        return;
    const auto* left = buffer.getReadPointer (0) + sourceOffset;
    const auto* right = buffer.getNumChannels() > 1
        ? buffer.getReadPointer (1) + sourceOffset : left;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto leftSample = std::isfinite (left[sample]) ? left[sample] : 0.0f;
        const auto rightSample = std::isfinite (right[sample]) ? right[sample] : 0.0f;
        storage->audioFifo[0][(std::size_t) (startIndex + sample)] = leftSample;
        storage->audioFifo[1][(std::size_t) (startIndex + sample)] = rightSample;
    }
}

TB_MultiBandCompressorAudioProcessor::TB_MultiBandCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain 1", juce::AudioChannelSet::mono(), false)
                          .withInput ("Sidechain 2", juce::AudioChannelSet::mono(), false)
                          .withInput ("Sidechain 3", juce::AudioChannelSet::mono(), false)
                          .withInput ("Sidechain 4", juce::AudioChannelSet::mono(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, stateRootType, tbmbc::params::createLayout())
{
    mixParam = apvts.getRawParameterValue (tbmbc::params::mixId);
    outputParam = apvts.getRawParameterValue (tbmbc::params::outputId);
    bypassParam = apvts.getRawParameterValue (tbmbc::params::bypassId);
    phaseModeParam = apvts.getRawParameterValue (tbmbc::params::phaseModeId);
    oversamplingParam = apvts.getRawParameterValue (tbmbc::params::oversamplingId);
    lookaheadParam = apvts.getRawParameterValue (tbmbc::params::lookaheadId);
    hostBypassParameter = dynamic_cast<juce::AudioParameterBool*> (
        apvts.getParameter (tbmbc::params::bypassId));

    for (int band = 0; band < tbmbc::params::numBands; ++band)
    {
        auto& pointers = bandParams[(std::size_t) band];
        const auto raw = [this, band] (const char* suffix)
        {
            return apvts.getRawParameterValue (tbmbc::params::bandId (band, suffix));
        };
        pointers.enabled = raw (tbmbc::params::enabledSuffix);
        pointers.frequency = raw (tbmbc::params::frequencySuffix);
        pointers.bandwidth = raw (tbmbc::params::bandwidthSuffix);
        pointers.threshold = raw (tbmbc::params::thresholdSuffix);
        pointers.range = raw (tbmbc::params::rangeSuffix);
        pointers.ratio = raw (tbmbc::params::ratioSuffix);
        pointers.attack = raw (tbmbc::params::attackSuffix);
        pointers.release = raw (tbmbc::params::releaseSuffix);
        pointers.knee = raw (tbmbc::params::kneeSuffix);
        pointers.output = raw (tbmbc::params::outputSuffix);
        pointers.mode = raw (tbmbc::params::modeSuffix);
        pointers.source = raw (tbmbc::params::sourceSuffix);
        pointers.detectorMode = raw (tbmbc::params::detectorModeSuffix);
        pointers.detectorFrequency = raw (tbmbc::params::detectorFrequencySuffix);
        pointers.detectorBandwidth = raw (tbmbc::params::detectorBandwidthSuffix);
        pointers.stereoLink = raw (tbmbc::params::stereoLinkSuffix);
        pointers.audition = raw (tbmbc::params::auditionSuffix);
        pointers.channelMode = raw (tbmbc::params::channelModeSuffix);
    }

    jassert (mixParam != nullptr && outputParam != nullptr && bypassParam != nullptr
             && phaseModeParam != nullptr && oversamplingParam != nullptr
             && lookaheadParam != nullptr
             && hostBypassParameter != nullptr);
    for (const auto& id : tbmbc::params::allParameterIds())
        apvts.addParameterListener (id, this);
    setLatencySamples (0);
}

TB_MultiBandCompressorAudioProcessor::~TB_MultiBandCompressorAudioProcessor()
{
    cancelPendingUpdate();
    for (const auto& id : tbmbc::params::allParameterIds())
        apvts.removeParameterListener (id, this);
}

MultibandEngine::Settings TB_MultiBandCompressorAudioProcessor::readSettings (
    bool forceBypass) const noexcept
{
    MultibandEngine::Settings settings;
    for (int band = 0; band < tbmbc::params::numBands; ++band)
    {
        const auto& source = bandParams[(std::size_t) band];
        auto& destination = settings.bands[(std::size_t) band];
        destination.enabled = source.enabled->load() >= 0.5f;
        destination.frequencyHz = source.frequency->load();
        destination.bandwidthOctaves = source.bandwidth->load();
        destination.thresholdDb = source.threshold->load();
        destination.rangeDb = source.range->load();
        destination.ratio = source.ratio->load();
        destination.attackMs = source.attack->load();
        destination.releaseMs = source.release->load();
        destination.kneeDb = source.knee->load();
        destination.outputDb = source.output->load();
        destination.dynamicsMode = source.mode->load() >= 0.5f
            ? MultibandEngine::DynamicsMode::expand
            : MultibandEngine::DynamicsMode::compress;
        destination.detectorSource = source.source->load() >= 0.5f
            ? MultibandEngine::DetectorSource::external
            : MultibandEngine::DetectorSource::internal;
        // Detector-mode parameters remain serialised for compatibility, but the
        // simplified product always keys from the processing band.
        destination.detectorMode = MultibandEngine::DetectorMode::band;
        destination.detectorFrequencyHz = source.detectorFrequency->load();
        destination.detectorBandwidthOctaves = source.detectorBandwidth->load();
        destination.stereoLink = source.stereoLink->load() * 0.01f;
        destination.audition = source.audition->load() >= 0.5f;
        destination.channelMode = static_cast<MultibandEngine::ChannelMode> (
            juce::jlimit (tbmbc::params::stereoChannelMode,
                          tbmbc::params::sideChannelMode,
                          juce::roundToInt (source.channelMode->load())));
    }
    int phaseMode = 0, oversampling = 0;
    float lookaheadMs = 0.0f;
    unpackQualityState (approvedQualityState.load (std::memory_order_acquire),
                        phaseMode, oversampling, lookaheadMs);
    settings.phaseMode = static_cast<MultibandEngine::PhaseMode> (phaseMode);
    settings.oversamplingMode =
        static_cast<MultibandEngine::OversamplingMode> (oversampling);
    settings.lookaheadMs = lookaheadMs;
    settings.mix = mixParam->load() * 0.01f;
    settings.outputDb = outputParam->load();
    settings.bypass = forceBypass || bypassParam->load() >= 0.5f;
    settings.deltaListenBand = deltaListenBand.load (std::memory_order_acquire);
    return settings;
}

void TB_MultiBandCompressorAudioProcessor::setDeltaListenBand (int band) noexcept
{
    deltaListenBand.store (
        band >= 0 && band < tbmbc::params::numBands ? band : -1,
        std::memory_order_release);
}

void TB_MultiBandCompressorAudioProcessor::prepareToPlay (
    double sampleRate, int maximumExpectedSamplesPerBlock)
{
    setDeltaListenBand (-1);
    engine.prepare (sampleRate, maximumExpectedSamplesPerBlock,
                    juce::jmax (1, getTotalNumOutputChannels()));
    approvedQualityState.store (packQualityState (
        juce::roundToInt (phaseModeParam->load()),
        juce::roundToInt (oversamplingParam->load()), lookaheadParam->load()),
        std::memory_order_release);
    if (spectrumAnalyzerDemandCount.load (std::memory_order_relaxed) > 0)
        inputSpectrumAnalyzer.reset();
    setLatencySamples (engine.getLatencySamples (readSettings()));
    externalSidechainActive.store (false, std::memory_order_relaxed);
    externalSidechainChannels.store (0, std::memory_order_relaxed);
    externalSidechainBusMask.store (0, std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::releaseResources()
{
    setDeltaListenBand (-1);
    engine.reset();
    if (spectrumAnalyzerDemandCount.load (std::memory_order_relaxed) > 0)
        inputSpectrumAnalyzer.reset();
    externalSidechainActive.store (false, std::memory_order_relaxed);
    externalSidechainChannels.store (0, std::memory_order_relaxed);
    externalSidechainBusMask.store (0, std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::reset()
{
    setDeltaListenBand (-1);
    engine.reset();
    if (spectrumAnalyzerDemandCount.load (std::memory_order_relaxed) > 0)
        inputSpectrumAnalyzer.reset();
    externalSidechainActive.store (false, std::memory_order_relaxed);
    externalSidechainChannels.store (0, std::memory_order_relaxed);
    externalSidechainBusMask.store (0, std::memory_order_relaxed);
}

bool TB_MultiBandCompressorAudioProcessor::isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (input != output
        || (output != juce::AudioChannelSet::mono()
            && output != juce::AudioChannelSet::stereo()))
        return false;

    // The plug-in exposes four independently selectable optional auxiliary
    // inputs. Keep the bus count fixed so hosts show four named Send/Aux keys.
    if (layouts.inputBuses.size() != 1 + MultibandEngine::bandCount)
        return false;

    for (int bus = 1; bus < 5; ++bus)
    {
        const auto sidechain = layouts.getChannelSet (true, bus);
        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void TB_MultiBandCompressorAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio (buffer, false);
}

void TB_MultiBandCompressorAudioProcessor::processBlockBypassed (
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio (buffer, true);
}

void TB_MultiBandCompressorAudioProcessor::processAudio (
    juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    auto main = getBusBuffer (buffer, false, 0);
    const auto analyzerActive = spectrumAnalyzerDemandCount.load (
                                    std::memory_order_relaxed) > 0
                                && ! isNonRealtime();
    if (analyzerActive)
        inputSpectrumAnalyzer.push (main);

    std::array<juce::AudioBuffer<float>, MultibandEngine::bandCount> sidechainViews {
        getBusBuffer (buffer, true, 1), getBusBuffer (buffer, true, 2),
        getBusBuffer (buffer, true, 3), getBusBuffer (buffer, true, 4) };

    MultibandEngine::ExternalSidechainBuses sidechainBuses {};
    auto activeSidechainCount = 0;
    std::uint32_t activeSidechainMask = 0;
    for (int band = 0; band < MultibandEngine::bandCount; ++band)
    {
        const auto* bus = getBus (true, band + 1);
        const auto& sidechain = sidechainViews[static_cast<std::size_t> (band)];
        const auto valid = bus != nullptr && bus->isEnabled()
                           && sidechain.getNumChannels() > 0
                           && sidechain.getNumSamples() == main.getNumSamples();
        sidechainBuses[static_cast<std::size_t> (band)] = valid ? &sidechain : nullptr;
        if (valid)
        {
            ++activeSidechainCount;
            activeSidechainMask |= std::uint32_t (1) << static_cast<unsigned> (band);
        }
    }

    externalSidechainActive.store (activeSidechainCount > 0,
                                   std::memory_order_relaxed);
    externalSidechainChannels.store (activeSidechainCount,
                                     std::memory_order_relaxed);
    externalSidechainBusMask.store (activeSidechainMask,
                                    std::memory_order_relaxed);
    engine.process (main, sidechainBuses, readSettings (forceBypass));
    audioBlockCounter.fetch_add (1, std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::setSpectrumAnalyzerDemand (
    bool shouldAnalyze) noexcept
{
    if (shouldAnalyze)
    {
        spectrumAnalyzerDemandCount.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const auto previous = spectrumAnalyzerDemandCount.fetch_sub (
        1, std::memory_order_relaxed);
    jassert (previous > 0);
    if (previous <= 0)
        spectrumAnalyzerDemandCount.store (0, std::memory_order_relaxed);
}

juce::AudioProcessorParameter*
TB_MultiBandCompressorAudioProcessor::getBypassParameter() const
{
    return hostBypassParameter;
}

juce::AudioProcessorEditor*
TB_MultiBandCompressorAudioProcessor::createEditor()
{
    return new TB_MultiBandCompressorAudioProcessorEditor (*this);
}

juce::ValueTree TB_MultiBandCompressorAudioProcessor::createSerializableState()
{
    auto state = apvts.copyState();
    state.setProperty ("product", productIdentifier, nullptr);
    state.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    return state;
}

juce::ValueTree TB_MultiBandCompressorAudioProcessor::createStateFromValues (
    const tbmbc::presets::Values& values) const
{
    juce::ValueTree state (stateRootType);
    state.setProperty ("product", productIdentifier, nullptr);
    state.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (std::size_t index = 0; index < metadata.size(); ++index)
    {
        juce::ValueTree parameter ("PARAM");
        parameter.setProperty ("id", metadata[index].id, nullptr);
        parameter.setProperty ("value", values.values[index], nullptr);
        state.appendChild (parameter, nullptr);
    }
    return state;
}

bool TB_MultiBandCompressorAudioProcessor::isValidState (
    const juce::ValueTree& candidate) const
{
    tbmbc::presets::Values ignored {};
    return extractStateValues (candidate, ignored, false);
}

bool TB_MultiBandCompressorAudioProcessor::extractStateValues (
    const juce::ValueTree& candidate, tbmbc::presets::Values& values,
    bool allowLegacy) const
{
    int schema = 0;
    if (! candidate.isValid() || ! candidate.hasType (stateRootType)
        || candidate.getProperty ("product").toString() != productIdentifier
        || ! parseStrictInteger (candidate.getProperty ("stateSchemaVersion"), schema))
        return false;
    const auto legacyV1 = schema == 1;
    const auto previousV2 = schema == 2;
    const auto previousV3 = schema == 3;
    if (schema != stateSchemaVersion
        && (! allowLegacy || (! legacyV1 && ! previousV2 && ! previousV3)))
        return false;
    const auto requiredCount = legacyV1 ? tbmbc::params::legacyParameterCount
                                        : tbmbc::params::parameterCount;
    if (candidate.getNumChildren() != (int) requiredCount)
        return false;

    values = tbmbc::presets::makeDefaultValues();
    std::array<bool, tbmbc::params::parameterCount> found {};
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (const auto& child : candidate)
    {
        if (! child.hasType ("PARAM") || child.getNumChildren() != 0
            || ! child.hasProperty ("id") || ! child.hasProperty ("value"))
            return false;
        const auto id = child.getProperty ("id").toString();
        std::size_t index = metadata.size();
        for (std::size_t cursor = 0; cursor < metadata.size(); ++cursor)
            if (metadata[cursor].id == id)
            {
                index = cursor;
                break;
            }
        if (index >= requiredCount || found[index])
            return false;
        found[index] = true;
        double value = 0.0;
        if (! parseStrictNumber (child.getProperty ("value"), value))
            return false;
        const auto legacyRange = schema < stateSchemaVersion
                                 && isBandRangeParameter (index);
        if ((legacyRange && ! acceptsLegacyRangeValue (value))
            || (! legacyRange && ! metadata[index].acceptsRawValue ((float) value)))
            return false;
        if (previousV2
            && index == tbmbc::params::oversamplingParameterIndex
            && value > tbmbc::params::oversampling4x)
            return false;
        values.values[index] = legacyRange
                                   ? juce::jlimit (tbmbc::params::rangeMinDb,
                                                   tbmbc::params::rangeMaxDb,
                                                   (float) value)
                                   : (float) value;
    }
    return std::all_of (found.begin(), found.begin() + (std::ptrdiff_t) requiredCount,
                        [] (bool value) { return value; });
}

bool TB_MultiBandCompressorAudioProcessor::extractPresetValues (
    const juce::ValueTree& state, tbmbc::presets::Values& values,
    bool allowLegacy) const
{
    return extractStateValues (state, values, allowLegacy);
}

void TB_MultiBandCompressorAudioProcessor::getStateInformation (
    juce::MemoryBlock& destination)
{
    destination.reset();
    if (const auto xml = createSerializableState().createXml())
        copyXmlToBinary (*xml, destination);
}

void TB_MultiBandCompressorAudioProcessor::setStateInformation (
    const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > maximumStateBytes)
        return;
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    const auto candidate = juce::ValueTree::fromXml (*xml);
    tbmbc::presets::Values values {};
    if (! extractStateValues (candidate, values, true))
        return;

    setDeltaListenBand (-1);
    applyingStateOrPreset.store (true, std::memory_order_relaxed);
    apvts.replaceState (createStateFromValues (values));
    undoManager.clearUndoHistory();
    applyingStateOrPreset.store (false, std::memory_order_relaxed);
    updateSelectionFromCurrentValues();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void TB_MultiBandCompressorAudioProcessor::setParameterActual (
    const juce::String& id, float actualValue, bool notifyHost)
{
    auto* parameter = apvts.getParameter (id);
    if (parameter == nullptr || ! std::isfinite (actualValue))
        return;
    const auto normalised = parameter->convertTo0to1 (actualValue);
    if (juce::approximatelyEqual (normalised, parameter->getValue()))
        return;
    if (notifyHost)
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        parameter->endChangeGesture();
    }
    else
        parameter->setValue (normalised);
}

bool TB_MultiBandCompressorAudioProcessor::applyPresetValues (
    const tbmbc::presets::Values& values, const juce::String& transaction,
    bool makeUndoable, int factoryPresetId, const juce::File& userPresetFile)
{
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (std::size_t index = 0; index < metadata.size(); ++index)
        if (! metadata[index].acceptsRawValue (values.values[index]))
            return false;

    setDeltaListenBand (-1);
    if (makeUndoable)
        undoManager.beginNewTransaction (transaction);
    applyingStateOrPreset.store (true, std::memory_order_relaxed);
    for (std::size_t index = 0; index < metadata.size(); ++index)
        setParameterActual (metadata[index].id, values.values[index]);
    juce::ignoreUnused (apvts.copyState());
    activeFactoryPresetId.store (factoryPresetId, std::memory_order_relaxed);
    if (userPresetFile != juce::File())
        setUserPresetSelection (userPresetFile, &values);
    else
        currentSelectionIsUser.store (false, std::memory_order_relaxed);
    applyingStateOrPreset.store (false, std::memory_order_relaxed);
    if (! makeUndoable)
        undoManager.clearUndoHistory();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    updateHostDisplay (juce::AudioProcessorListener::ChangeDetails {}
                           .withNonParameterStateChanged (true));
    return true;
}

bool TB_MultiBandCompressorAudioProcessor::applyFactoryPreset (
    int presetId, bool makeUndoable)
{
    const auto* preset = tbmbc::presets::findById (presetId);
    return preset != nullptr
           && applyPresetValues (preset->values, "Preset: " + juce::String (preset->name),
                                 makeUndoable, preset->id);
}

int TB_MultiBandCompressorAudioProcessor::getFactoryPresetCount() const noexcept
{
    return (int) tbmbc::presets::factory.size();
}

const tbmbc::presets::FactoryPreset&
TB_MultiBandCompressorAudioProcessor::getFactoryPreset (int index) const
{
    return tbmbc::presets::factory[(std::size_t) juce::jlimit (
        0, getFactoryPresetCount() - 1, index)];
}

juce::String TB_MultiBandCompressorAudioProcessor::getCurrentPresetName() const
{
    if (currentSelectionIsUser.load (std::memory_order_relaxed))
    {
        const juce::ScopedLock lock (presetSelectionLock);
        if (selectedUserPresetFile != juce::File())
            return selectedUserPresetFile.getFileNameWithoutExtension();
    }
    if (const auto* preset = tbmbc::presets::findById (
            activeFactoryPresetId.load (std::memory_order_relaxed)))
        return preset->name;
    return "Custom";
}

juce::File TB_MultiBandCompressorAudioProcessor::getCurrentUserPresetFile() const
{
    if (! currentSelectionIsUser.load (std::memory_order_relaxed))
        return {};
    const juce::ScopedLock lock (presetSelectionLock);
    return selectedUserPresetFile;
}

juce::String TB_MultiBandCompressorAudioProcessor::getUserPresetExtension()
{
    return presetExtension;
}

juce::File TB_MultiBandCompressorAudioProcessor::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("TB_MultiBandCompressor")
        .getChildFile ("Presets")
        .getChildFile ("User");
}

juce::Result TB_MultiBandCompressorAudioProcessor::saveUserPreset (
    const juce::File& requestedFile)
{
    auto file = requestedFile;
    if (file.getFileExtension().isEmpty())
        file = file.withFileExtension (presetExtension);
    else if (! file.hasFileExtension (presetExtension))
        return juce::Result::fail ("Preset files must use " + juce::String (presetExtension));
    if (file.getFileNameWithoutExtension().trim().isEmpty())
        return juce::Result::fail ("Preset name cannot be empty.");
    if (const auto directory = file.getParentDirectory().createDirectory(); directory.failed())
        return directory;

    juce::ValueTree preset (presetRootType);
    preset.setProperty ("product", productIdentifier, nullptr);
    preset.setProperty ("presetFormatVersion", presetFormatVersion, nullptr);
    preset.setProperty ("stateSchemaVersion", stateSchemaVersion, nullptr);
    preset.setProperty ("name", file.getFileNameWithoutExtension(), nullptr);
    auto serializableState = createSerializableState();
    clearAuditionState (serializableState);
    tbmbc::presets::Values savedValues {};
    if (! extractPresetValues (serializableState, savedValues))
        return juce::Result::fail ("Could not encode preset parameter data.");
    preset.appendChild (serializableState, nullptr);
    const auto xml = preset.createXml();
    if (xml == nullptr)
        return juce::Result::fail ("Could not encode preset.");
    juce::TemporaryFile temporary (file);
    if (! xml->writeTo (temporary.getFile(), {}))
        return juce::Result::fail ("Could not write preset.");
    if (! temporary.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("Could not replace preset.");
    activeFactoryPresetId.store (0, std::memory_order_relaxed);
    setUserPresetSelection (file, &savedValues);
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    return juce::Result::ok();
}

juce::Result TB_MultiBandCompressorAudioProcessor::loadUserPreset (
    const juce::File& file, bool makeUndoable)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("Preset file does not exist.");
    if (! file.hasFileExtension (presetExtension))
        return juce::Result::fail ("Unsupported preset extension.");
    if (file.getSize() <= 0 || file.getSize() > maximumPresetBytes)
        return juce::Result::fail ("Preset file size is invalid.");
    const auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName (presetRootType))
        return juce::Result::fail ("Preset is not valid XML.");
    const auto preset = juce::ValueTree::fromXml (*xml);
    int format = 0, schema = 0;
    if (! preset.isValid() || preset.getNumChildren() != 1
        || preset.getProperty ("product").toString() != productIdentifier
        || ! parseStrictInteger (preset.getProperty ("presetFormatVersion"), format)
        || ! parseStrictInteger (preset.getProperty ("stateSchemaVersion"), schema)
        || ! ((format == presetFormatVersion && schema == stateSchemaVersion)
              || (format == 3 && schema == 3)
              || (format == 2 && schema == 2)
              || (format == 1 && schema == 1)))
        return juce::Result::fail ("Preset parameter data is invalid.");

    const auto presetState = preset.getChild (0);
    int nestedSchema = 0;
    tbmbc::presets::Values values {};
    if (! parseStrictInteger (
            presetState.getProperty ("stateSchemaVersion"), nestedSchema)
        || nestedSchema != schema
        || ! extractPresetValues (presetState, values, true))
        return juce::Result::fail ("Preset parameter data is invalid.");
    clearAuditionValues (values);
    return applyPresetValues (values, "Preset: " + file.getFileNameWithoutExtension(),
                              makeUndoable, 0, file)
        ? juce::Result::ok()
        : juce::Result::fail ("Preset parameter data is invalid.");
}

void TB_MultiBandCompressorAudioProcessor::setUserPresetSelection (
    const juce::File& file, const tbmbc::presets::Values* values)
{
    const juce::ScopedLock lock (presetSelectionLock);
    selectedUserPresetFile = file;
    if (file != juce::File() && values != nullptr)
    {
        selectedUserPresetValues = *values;
        clearAuditionValues (selectedUserPresetValues);
        selectedUserPresetValuesValid = true;
    }
    else if (file == juce::File())
    {
        selectedUserPresetValuesValid = false;
    }
    currentSelectionIsUser.store (file != juce::File(), std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::updateSelectionFromCurrentValues()
{
    tbmbc::presets::Values values {};
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (std::size_t index = 0; index < metadata.size(); ++index)
        if (const auto* raw = apvts.getRawParameterValue (metadata[index].id))
            values.values[index] = raw->load();
    clearAuditionValues (values);
    const auto factoryMatch = tbmbc::presets::findMatch (values);
    activeFactoryPresetId.store (factoryMatch, std::memory_order_relaxed);
    if (factoryMatch != 0)
    {
        currentSelectionIsUser.store (false, std::memory_order_relaxed);
        return;
    }

    auto matchesUserPreset = false;
    {
        const juce::ScopedLock lock (presetSelectionLock);
        matchesUserPreset = selectedUserPresetValuesValid
                            && selectedUserPresetFile != juce::File()
                            && tbmbc::presets::matches (
                                values, selectedUserPresetValues);
    }
    currentSelectionIsUser.store (matchesUserPreset,
                                  std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::parameterChanged (
    const juce::String& parameterId, float)
{
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
    if (! applyingStateOrPreset.load (std::memory_order_relaxed))
    {
        activeFactoryPresetId.store (0, std::memory_order_relaxed);
        currentSelectionIsUser.store (false, std::memory_order_relaxed);
    }
    if (parameterId == tbmbc::params::phaseModeId
        || parameterId == tbmbc::params::oversamplingId
        || parameterId == tbmbc::params::lookaheadId)
        triggerAsyncUpdate();
}

void TB_MultiBandCompressorAudioProcessor::handleAsyncUpdate()
{
    const auto phaseMode = juce::jlimit (
        tbmbc::params::dynamicPhaseMode, tbmbc::params::linearPhaseMode,
        juce::roundToInt (phaseModeParam->load()));
    const auto oversampling = juce::jlimit (
        tbmbc::params::oversamplingOff, tbmbc::params::oversampling8x,
        juce::roundToInt (oversamplingParam->load()));
    const auto lookaheadMs = juce::jlimit (
        tbmbc::params::lookaheadMinMs, tbmbc::params::lookaheadMaxMs,
        lookaheadParam->load());

    auto pendingSettings = readSettings();
    pendingSettings.phaseMode = static_cast<MultibandEngine::PhaseMode> (phaseMode);
    pendingSettings.oversamplingMode =
        static_cast<MultibandEngine::OversamplingMode> (oversampling);
    pendingSettings.lookaheadMs = lookaheadMs;
    setLatencySamples (engine.getLatencySamples (pendingSettings));
    approvedQualityState.store (packQualityState (
        phaseMode, oversampling, lookaheadMs), std::memory_order_release);
}

void TB_MultiBandCompressorAudioProcessor::undo()
{
    if (! undoManager.canUndo())
        return;
    applyingStateOrPreset.store (true, std::memory_order_relaxed);
    undoManager.undo();
    applyingStateOrPreset.store (false, std::memory_order_relaxed);
    updateSelectionFromCurrentValues();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

void TB_MultiBandCompressorAudioProcessor::redo()
{
    if (! undoManager.canRedo())
        return;
    applyingStateOrPreset.store (true, std::memory_order_relaxed);
    undoManager.redo();
    applyingStateOrPreset.store (false, std::memory_order_relaxed);
    updateSelectionFromCurrentValues();
    parameterRevision.fetch_add (1, std::memory_order_relaxed);
}

float TB_MultiBandCompressorAudioProcessor::getGainReductionDb (int band) const noexcept
{
    return engine.getGainReductionDb (band);
}

float TB_MultiBandCompressorAudioProcessor::getDetectorLevelDb (int band) const noexcept
{
    return engine.getDetectorLevelDb (band);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TB_MultiBandCompressorAudioProcessor();
}
