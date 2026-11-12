#include <JuceHeader.h>

#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using Processor = TB_MultiBandCompressorAudioProcessor;

constexpr double testSampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int editorWidth = 1000;
constexpr int editorHeight = 620;
constexpr int editorMinimumWidth = 850;
constexpr int editorMinimumHeight = 527;
constexpr int editorMaximumWidth = 1500;
constexpr int editorMaximumHeight = 930;
constexpr float valueTolerance = 1.0e-4f;

static_assert (static_cast<int> (MultibandEngine::PhaseMode::dynamic)
                   == tbmbc::params::dynamicPhaseMode
               && static_cast<int> (MultibandEngine::PhaseMode::linear)
                      == tbmbc::params::linearPhaseMode);
static_assert (static_cast<int> (MultibandEngine::OversamplingMode::off)
                   == tbmbc::params::oversamplingOff
               && static_cast<int> (MultibandEngine::OversamplingMode::x2)
                      == tbmbc::params::oversampling2x
               && static_cast<int> (MultibandEngine::OversamplingMode::x4)
                      == tbmbc::params::oversampling4x
               && static_cast<int> (MultibandEngine::OversamplingMode::x8)
                      == tbmbc::params::oversampling8x);
static_assert (static_cast<int> (MultibandEngine::ChannelMode::stereo)
                   == tbmbc::params::stereoChannelMode
               && static_cast<int> (MultibandEngine::ChannelMode::mid)
                      == tbmbc::params::midChannelMode
               && static_cast<int> (MultibandEngine::ChannelMode::side)
                      == tbmbc::params::sideChannelMode);

class TestRunner
{
public:
    void run (const char* name, const std::function<void()>& test)
    {
        try
        {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        }
    }

    int finish() const
    {
        std::cout << "Processor integration: " << passed << " passed, "
                  << failed << " failed\n";
        return failed == 0 ? 0 : 1;
    }

private:
    int passed = 0;
    int failed = 0;
};

void require (bool condition, const std::string& message)
{
    if (! condition)
        throw std::runtime_error (message);
}

float getPlainValue (Processor& processor, const juce::String& id)
{
    const auto* value = processor.getValueTreeState().getRawParameterValue (id);
    require (value != nullptr, "missing parameter " + id.toStdString());
    return value->load();
}

void setPlainValue (Processor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getValueTreeState().getParameter (id);
    require (parameter != nullptr, "missing parameter " + id.toStdString());
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    parameter->endChangeGesture();
}

tbmbc::presets::Values captureValues (Processor& processor)
{
    tbmbc::presets::Values result {};
    const auto& ids = tbmbc::params::allParameterIds();
    for (std::size_t index = 0; index < ids.size(); ++index)
        result.values[index] = getPlainValue (processor, ids[index]);
    return result;
}

void requireValuesEqual (const tbmbc::presets::Values& actual,
                         const tbmbc::presets::Values& expected,
                         const std::string& context)
{
    const auto& ids = tbmbc::params::allParameterIds();
    for (std::size_t index = 0; index < ids.size(); ++index)
        require (std::abs (actual.values[index] - expected.values[index])
                     <= valueTolerance,
                 context + " changed " + ids[index].toStdString());
}

juce::MemoryBlock serialiseTree (const juce::ValueTree& tree)
{
    const auto xml = tree.createXml();
    require (xml != nullptr, "could not encode state fixture");
    juce::MemoryBlock result;
    juce::AudioProcessor::copyXmlToBinary (*xml, result);
    return result;
}

juce::ValueTree stateTreeFrom (Processor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    require (! state.isEmpty(), "processor returned empty state");
    const auto xml = juce::AudioProcessor::getXmlFromBinary (
        state.getData(), static_cast<int> (state.getSize()));
    require (xml != nullptr, "processor state is not binary XML");
    return juce::ValueTree::fromXml (*xml);
}

juce::ValueTree makeLegacyStateFixture (const juce::ValueTree& currentState)
{
    auto result = currentState.createCopy();
    result.setProperty ("stateSchemaVersion", 1, nullptr);
    const auto& ids = tbmbc::params::allParameterIds();
    const auto legacyEnd = ids.begin() + tbmbc::params::legacyParameterCount;

    for (int childIndex = result.getNumChildren() - 1; childIndex >= 0; --childIndex)
    {
        const auto id = result.getChild (childIndex).getProperty ("id").toString();
        if (std::find (ids.begin(), legacyEnd, id) == legacyEnd)
            result.removeChild (childIndex, nullptr);
    }

    require (result.getNumChildren() == tbmbc::params::legacyParameterCount,
             "legacy state fixture is not exactly 71 parameters");
    for (auto id = ids.begin(); id != legacyEnd; ++id)
        require (result.getChildWithProperty ("id", *id).isValid(),
                 "legacy state fixture is missing " + id->toStdString());
    return result;
}

juce::AudioChannelSet channelSet (int channels)
{
    if (channels == 1)
        return juce::AudioChannelSet::mono();
    if (channels == 2)
        return juce::AudioChannelSet::stereo();
    if (channels == 4)
        return juce::AudioChannelSet::discreteChannels (4);
    return juce::AudioChannelSet::disabled();
}

juce::AudioProcessor::BusesLayout makeLayout (int mainChannels,
                                               int sidechainBuses)
{
    juce::AudioProcessor::BusesLayout result;
    result.inputBuses.add (channelSet (mainChannels));
    // The plugin exposes four independent optional mono auxiliary buses.
    // Keep the first N active so the test process buffer remains laid out as
    // main channels followed by Sidechain 1..N channels.
    for (int bus = 0; bus < tbmbc::params::numBands; ++bus)
        result.inputBuses.add (bus < sidechainBuses
                                   ? juce::AudioChannelSet::mono()
                                   : juce::AudioChannelSet::disabled());
    result.outputBuses.add (channelSet (mainChannels));
    return result;
}

void setLayout (Processor& processor, int mainChannels, int sidechainBuses)
{
    const auto layout = makeLayout (mainChannels, sidechainBuses);
    require (processor.isBusesLayoutSupported (layout), "supported bus layout was rejected");
    require (processor.setBusesLayout (layout), "could not apply supported bus layout");
    require (processor.getMainBusNumInputChannels() == mainChannels
                 && processor.getMainBusNumOutputChannels() == mainChannels,
             "main bus channel count changed");
    require (processor.getBusCount (true) == 5,
             "four optional sidechain input buses are missing");
    for (int bus = 1; bus <= tbmbc::params::numBands; ++bus)
        require (processor.getBus (true, bus) != nullptr
                     && processor.getBus (true, bus)->getName()
                            == juce::String ("Sidechain ") + juce::String (bus),
                 "sidechain bus name is not visible as Sidechain N");
}

juce::AudioProcessor::BusesLayout makeSparseLayout (
    int mainChannels, const std::array<bool, tbmbc::params::numBands>& active)
{
    juce::AudioProcessor::BusesLayout result;
    result.inputBuses.add (channelSet (mainChannels));
    for (const auto enabled : active)
        result.inputBuses.add (enabled ? juce::AudioChannelSet::mono()
                                       : juce::AudioChannelSet::disabled());
    result.outputBuses.add (channelSet (mainChannels));
    return result;
}

void setSparseLayout (
    Processor& processor, int mainChannels,
    const std::array<bool, tbmbc::params::numBands>& active)
{
    const auto layout = makeSparseLayout (mainChannels, active);
    require (processor.isBusesLayoutSupported (layout),
             "supported sparse sidechain layout was rejected");
    require (processor.setBusesLayout (layout),
             "could not apply supported sparse sidechain layout");
    require (processor.getBusCount (true) == 5,
             "sparse layout lost one of the four optional sidechain buses");
    for (int bus = 1; bus <= tbmbc::params::numBands; ++bus)
        require (processor.getBus (true, bus) != nullptr
                     && processor.getBus (true, bus)->getName()
                            == juce::String ("Sidechain ") + juce::String (bus),
                 "sparse layout changed a Sidechain N bus name");
}

void setStereoSidechainLayout (Processor& processor, int mainChannels,
                               int activeBus = 1)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channelSet (mainChannels));
    for (int bus = 1; bus <= tbmbc::params::numBands; ++bus)
        layout.inputBuses.add (bus == activeBus
                                   ? juce::AudioChannelSet::stereo()
                                   : juce::AudioChannelSet::disabled());
    layout.outputBuses.add (channelSet (mainChannels));
    require (processor.isBusesLayoutSupported (layout),
             "supported stereo sidechain layout was rejected");
    require (processor.setBusesLayout (layout),
             "could not apply supported stereo sidechain layout");
}

juce::AudioBuffer<float> makeProcessBuffer (Processor& processor, int samples)
{
    return { juce::jmax (processor.getTotalNumInputChannels(),
                         processor.getTotalNumOutputChannels()),
             samples };
}

void fillProcessBuffer (juce::AudioBuffer<float>& buffer,
                        int mainChannels, int sidechainChannels,
                        std::int64_t firstSample, float mainAmplitude = 0.35f,
                        float sidechainAmplitude = 0.7f,
                        double mainFrequency = 997.0,
                        double sidechainFrequency = 503.0)
{
    buffer.clear();
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto absoluteSample = static_cast<double> (firstSample + sample);
        for (int channel = 0; channel < mainChannels; ++channel)
        {
            const auto phase = juce::MathConstants<double>::twoPi
                               * mainFrequency * absoluteSample / testSampleRate
                               + 0.31 * channel;
            buffer.setSample (channel, sample,
                              mainAmplitude * static_cast<float> (std::sin (phase)));
        }

        for (int channel = 0; channel < sidechainChannels; ++channel)
        {
            const auto phase = juce::MathConstants<double>::twoPi
                               * sidechainFrequency * absoluteSample / testSampleRate
                               + 0.19 * channel;
            buffer.setSample (mainChannels + channel, sample,
                              sidechainAmplitude * static_cast<float> (std::sin (phase)));
        }
    }
}

juce::AudioBuffer<float> copyMain (const juce::AudioBuffer<float>& source,
                                   int mainChannels)
{
    juce::AudioBuffer<float> result (mainChannels, source.getNumSamples());
    for (int channel = 0; channel < mainChannels; ++channel)
        result.copyFrom (channel, 0, source, channel, 0, source.getNumSamples());
    return result;
}

double maximumMainDifference (const juce::AudioBuffer<float>& buffer,
                              const juce::AudioBuffer<float>& reference)
{
    auto result = 0.0;
    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
        for (int sample = 0; sample < reference.getNumSamples(); ++sample)
            result = std::max (result, std::abs (
                static_cast<double> (buffer.getSample (channel, sample))
                - static_cast<double> (reference.getSample (channel, sample))));
    return result;
}

float maximumMainMagnitude (const juce::AudioBuffer<float>& buffer,
                            int mainChannels)
{
    auto result = 0.0f;
    for (int channel = 0; channel < mainChannels; ++channel)
        result = std::max (result,
                           buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return result;
}

bool mainIsFinite (const juce::AudioBuffer<float>& buffer, int mainChannels)
{
    for (int channel = 0; channel < mainChannels; ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;
    return true;
}

void disableAllBands (Processor& processor)
{
    for (int band = 0; band < tbmbc::params::numBands; ++band)
    {
        setPlainValue (processor, tbmbc::params::bandId (
                           band, tbmbc::params::enabledSuffix), 0.0f);
        setPlainValue (processor, tbmbc::params::bandId (
                           band, tbmbc::params::auditionSuffix), 0.0f);
    }
}

void configureDetectorBand (Processor& processor, int band, float frequency,
                            int source, float rangeDb = -12.0f)
{
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::enabledSuffix), 1.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::frequencySuffix), frequency);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::bandwidthSuffix), 0.5f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::thresholdSuffix), -36.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::rangeSuffix), rangeDb);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::ratioSuffix), 8.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::attackSuffix), 0.5f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::releaseSuffix), 20.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::kneeSuffix), 0.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::sourceSuffix), static_cast<float> (source));
    setPlainValue (processor, tbmbc::params::bandId (
                       band, tbmbc::params::detectorModeSuffix),
                   static_cast<float> (tbmbc::params::bandDetector));
}

void testParameterAndHostContract()
{
    Processor processor;
    auto& apvts = processor.getValueTreeState();
    const auto& ids = tbmbc::params::allParameterIds();
    const auto& metadata = tbmbc::params::allParameterMetadata();

    require (tbmbc::params::legacyParameterCount == 71
                 && tbmbc::params::parameterCount == 78
                 && ids.size() == 78 && metadata.size() == 78,
             "public parameter count is not the stable 71 plus 7 additions");
    require (processor.getParameters().size() == 78,
             "processor does not expose exactly 78 parameters");

    const std::array<const char*, tbmbc::params::parametersPerBand> suffixes {{
        "enabled", "frequency", "bandwidth", "threshold", "range", "ratio",
        "attack", "release", "knee", "output", "mode", "source",
        "detector_mode", "detector_frequency", "detector_bandwidth",
        "stereo_link", "audition"
    }};
    std::array<juce::String, tbmbc::params::parameterCount> expected {};
    expected[0] = "mix";
    expected[1] = "output";
    expected[2] = "bypass";
    auto cursor = std::size_t { 3 };
    for (int band = 1; band <= tbmbc::params::numBands; ++band)
        for (const auto* suffix : suffixes)
            expected[cursor++] = "band" + juce::String (band) + "_" + suffix;
    require (cursor == tbmbc::params::legacyParameterCount,
             "legacy parameter prefix is not exactly 71 entries");
    expected[cursor++] = "phase_mode";
    expected[cursor++] = "oversampling";
    expected[cursor++] = "lookahead";
    for (int band = 1; band <= tbmbc::params::numBands; ++band)
        expected[cursor++] = "band" + juce::String (band) + "_channel_mode";
    require (cursor == expected.size(), "new parameter suffix is incomplete");

    juce::StringArray uniqueIds;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        require (ids[index] == expected[index],
                 "stable ID/order changed at index " + std::to_string (index));
        require (uniqueIds.addIfNotAlreadyThere (ids[index]),
                 "duplicate stable parameter ID " + ids[index].toStdString());
        auto* parameter = apvts.getParameter (ids[index]);
        require (parameter != nullptr,
                 "APVTS is missing " + ids[index].toStdString());
        require (processor.getParameters()[static_cast<int> (index)] == parameter,
                 "host parameter order changed for " + ids[index].toStdString());
        require (parameter->getName (128) == metadata[index].name,
                 "host name changed for " + ids[index].toStdString());
        require (parameter->getLabel() == metadata[index].unit,
                 "host unit changed for " + ids[index].toStdString());
        require (parameter->isAutomatable() == metadata[index].automatable,
                 "automation metadata differs for " + ids[index].toStdString());
        require (std::abs (getPlainValue (processor, ids[index])
                           - metadata[index].defaultValue) <= valueTolerance,
                 "default changed for " + ids[index].toStdString());

        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        require (ranged != nullptr, "parameter is not ranged");
        const auto& normalisable = ranged->getNormalisableRange();
        require (std::abs (normalisable.start - metadata[index].minimum) <= 1.0e-6f
                     && std::abs (normalisable.end - metadata[index].maximum) <= 1.0e-6f
                     && std::abs (normalisable.interval - metadata[index].interval)
                            <= 1.0e-6f,
                 "range metadata differs for " + ids[index].toStdString());
        if (metadata[index].kind != tbmbc::params::ParameterKind::continuous)
            require (parameter->getNumSteps() == metadata[index].choiceCount,
                     "choice step count changed for " + ids[index].toStdString());
    }

    require (processor.getBypassParameter() == apvts.getParameter (tbmbc::params::bypassId),
             "host-recognised bypass is not the stable bypass parameter");
    require (processor.getBypassParameter()->getName (128) == "Bypass",
             "host bypass name changed");
    require (processor.getLatencySamples() == 0 && processor.getTailLengthSeconds() == 0.0,
             "zero latency/tail contract changed");

    auto* rangeParameter = dynamic_cast<juce::RangedAudioParameter*> (
        apvts.getParameter (tbmbc::params::bandId (
            0, tbmbc::params::rangeSuffix)));
    require (rangeParameter != nullptr,
             "Band 1 Range is not a ranged host parameter");
    const auto& rangeContract = rangeParameter->getNormalisableRange();
    require (std::abs (rangeContract.start - (-48.0f)) <= 1.0e-6f
                 && std::abs (rangeContract.end - 12.0f) <= 1.0e-6f
                 && std::abs (rangeContract.interval - 0.1f) <= 1.0e-6f,
             "Range host metadata is not the asymmetric -48..+12 dB contract");

    auto* mode = apvts.getParameter (tbmbc::params::bandId (0, tbmbc::params::modeSuffix));
    auto* source = apvts.getParameter (tbmbc::params::bandId (0, tbmbc::params::sourceSuffix));
    auto* detector = apvts.getParameter (
        tbmbc::params::bandId (0, tbmbc::params::detectorModeSuffix));
    auto* phaseMode = apvts.getParameter (tbmbc::params::phaseModeId);
    auto* oversampling = apvts.getParameter (tbmbc::params::oversamplingId);
    auto* lookahead = apvts.getParameter (tbmbc::params::lookaheadId);
    auto* channelMode = apvts.getParameter (
        tbmbc::params::bandId (0, tbmbc::params::channelModeSuffix));
    require (mode->getText (mode->convertTo0to1 (0.0f), 32) == "Compress"
                 && mode->getText (mode->convertTo0to1 (1.0f), 32) == "Expand",
             "Dynamics choice ordering changed");
    require (source->getText (source->convertTo0to1 (0.0f), 32) == "INT"
                 && source->getText (source->convertTo0to1 (1.0f), 32) == "EXT",
             "Source choice ordering changed");
    require (detector->getText (detector->convertTo0to1 (0.0f), 32) == "BAND"
                 && detector->getText (detector->convertTo0to1 (1.0f), 32) == "FREE",
             "Detector choice ordering changed");
    require (phaseMode->getText (phaseMode->convertTo0to1 (0.0f), 32) == "Dynamic"
                 && phaseMode->getText (phaseMode->convertTo0to1 (1.0f), 32)
                        == "Linear",
             "Phase Mode choice ordering changed");
    require (oversampling->getText (oversampling->convertTo0to1 (0.0f), 32) == "OFF"
                 && oversampling->getText (oversampling->convertTo0to1 (1.0f), 32)
                        == "2x"
                 && oversampling->getText (oversampling->convertTo0to1 (2.0f), 32)
                        == "4x"
                 && oversampling->getText (oversampling->convertTo0to1 (3.0f), 32)
                        == "8x",
             "Oversampling choice ordering changed");
    require (channelMode->getText (channelMode->convertTo0to1 (0.0f), 32)
                        == "Stereo"
                 && channelMode->getText (channelMode->convertTo0to1 (1.0f), 32)
                        == "Mid"
                 && channelMode->getText (channelMode->convertTo0to1 (2.0f), 32)
                        == "Side",
             "Channel Mode choice ordering changed");
    require (! phaseMode->isAutomatable() && ! oversampling->isAutomatable()
                 && ! lookahead->isAutomatable() && channelMode->isAutomatable(),
             "latency controls or Channel Mode have the wrong automation contract");
    require (lookahead->getText (lookahead->convertTo0to1 (7.5f), 32) == "7.5 ms",
             "Lookahead host text format changed");
}

void testBusLayoutsFiniteLatencyAndTail()
{
    for (const auto mainChannels : { 1, 2 })
    {
        for (const auto sidechainBuses : { 0, 1, 2, 4 })
        {
            Processor processor;
            setLayout (processor, mainChannels, sidechainBuses);
            require (processor.applyFactoryPreset (3, false),
                     "could not load finite-output fixture preset");
            setPlainValue (processor, tbmbc::params::bandId (
                               0, tbmbc::params::sourceSuffix),
                           static_cast<float> (tbmbc::params::externalSource));
            processor.prepareToPlay (testSampleRate, 257);
            require (processor.getLatencySamples() == 0
                         && processor.getTailLengthSeconds() == 0.0,
                     "prepare changed latency/tail");

            std::int64_t firstSample = 0;
            for (const auto samples : { 0, 1, 63, 257, 65, 513 })
            {
                auto buffer = makeProcessBuffer (processor, samples);
                fillProcessBuffer (buffer, mainChannels, sidechainBuses, firstSample);
                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);
                require (mainIsFinite (buffer, mainChannels),
                         "processor emitted NaN/Inf for a supported layout");
                require (processor.isExternalSidechainBusActive()
                             == (sidechainBuses > 0),
                         "sidechain active state disagrees with bus layout");
                require (processor.getExternalSidechainChannelCount()
                             == sidechainBuses,
                         "sidechain channel count disagrees with bus layout");
                firstSample += samples;
            }
            processor.releaseResources();
        }
    }

    Processor invalid;
    auto mismatched = makeLayout (2, 0);
    mismatched.outputBuses.set (0, juce::AudioChannelSet::mono());
    require (! invalid.isBusesLayoutSupported (mismatched),
             "mismatched main input/output layout was accepted");

    auto invalidAuxShape = makeLayout (2, 0);
    invalidAuxShape.inputBuses.set (1, juce::AudioChannelSet::discreteChannels (4));
    require (! invalid.isBusesLayoutSupported (invalidAuxShape),
             "four-channel auxiliary bus was accepted in the four-bus contract");
}

void testBypassAndDryAreExact()
{
    const auto render = [] (int mode)
    {
        Processor processor;
        setLayout (processor, 2, 0);
        processor.prepareToPlay (testSampleRate, blockSize);
        if (mode == 0)
            setPlainValue (processor, tbmbc::params::mixId, 0.0f);
        else if (mode == 1)
            setPlainValue (processor, tbmbc::params::bypassId, 1.0f);
        else if (mode == 3)
            for (int band = 0; band < tbmbc::params::numBands; ++band)
                setPlainValue (processor, tbmbc::params::bandId (
                                   band, tbmbc::params::rangeSuffix), 0.0f);

        auto buffer = makeProcessBuffer (processor, blockSize);
        fillProcessBuffer (buffer, 2, 0, 0);
        const auto reference = copyMain (buffer, 2);
        juce::MidiBuffer midi;
        if (mode == 2)
            processor.processBlockBypassed (buffer, midi);
        else
            processor.processBlock (buffer, midi);
        return maximumMainDifference (buffer, reference);
    };

    require (render (0) == 0.0, "Mix 0 is not bit-exact dry");
    require (render (1) == 0.0, "host bypass parameter is not bit-exact dry");
    require (render (2) == 0.0, "processBlockBypassed is not bit-exact dry");
    require (render (3) <= 1.0e-6, "zero Range does not null to dry");
}

struct SidechainMeters
{
    float externalBand = 0.0f;
    float internalBand = 0.0f;
};

SidechainMeters renderIndependentSidechains (float sidechainAmplitude)
{
    Processor processor;
    setLayout (processor, 2, 1);
    disableAllBands (processor);
    configureDetectorBand (processor, 0, 500.0f, tbmbc::params::externalSource);
    configureDetectorBand (processor, 1, 4000.0f, tbmbc::params::internalSource, -8.0f);
    processor.prepareToPlay (testSampleRate, blockSize);

    for (int block = 0; block < 128; ++block)
    {
        auto buffer = makeProcessBuffer (processor, blockSize);
        fillProcessBuffer (buffer, 2, 1,
                           static_cast<std::int64_t> (block) * blockSize,
                           0.5f, sidechainAmplitude, 4000.0, 500.0);
        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);
        require (mainIsFinite (buffer, 2), "sidechain render became non-finite");
    }

    require (processor.isExternalSidechainBusActive(),
             "connected sidechain bus was not reported active");
    require (processor.getExternalSidechainChannelCount() == 1,
             "connected mono sidechain did not publish its channel count");
    return { processor.getGainReductionDb (0), processor.getGainReductionDb (1) };
}

void testSidechainSelectionFallbackAndNonLeak()
{
    const auto silent = renderIndependentSidechains (0.0f);
    const auto hot = renderIndependentSidechains (0.8f);
    require (std::abs (silent.externalBand) < 0.02f && hot.externalBand < -6.0f,
             "EXT did not independently drive the opted-in band");
    require (silent.internalBand < -3.0f && hot.internalBand < -3.0f
                 && std::abs (silent.internalBand - hot.internalBand) < 0.05f,
             "INT band changed with the external key level");

    // Four independent mono auxiliary buses form a per-band key matrix:
    // Sidechain N must drive only the corresponding EXT band, even when the
    // other key buses are silent. All bands use the same key tone so a
    // shared/mixed channel implementation cannot pass by relying on
    // detector-frequency separation.
    constexpr auto keyFrequency = 500.0;
    for (int targetChannel = 0;
         targetChannel < tbmbc::params::numBands; ++targetChannel)
    {
        Processor processor;
        setLayout (processor, 2, 4);
        disableAllBands (processor);
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            configureDetectorBand (processor, band,
                                   static_cast<float> (keyFrequency),
                                   tbmbc::params::externalSource);
        processor.prepareToPlay (testSampleRate, blockSize);

        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 4,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.5f, 0.7f,
                               keyFrequency, keyFrequency);
            for (int channel = 0; channel < 4; ++channel)
                if (channel != targetChannel)
                    buffer.clear (2 + channel, 0, buffer.getNumSamples());

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                 "four-bus sidechain render became non-finite");
        }

        require (processor.isExternalSidechainBusActive()
                     && processor.getExternalSidechainChannelCount() == 4,
                 "four-bus sidechain layout did not publish its active buses");
        const auto targetGainReduction = processor.getGainReductionDb (targetChannel);
        require (targetGainReduction < -6.0f,
                 "Sidechain channel " + std::to_string (targetChannel + 1)
                     + " did not drive matching Band "
                     + std::to_string (targetChannel + 1));

        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (band != targetChannel)
                require (std::abs (processor.getGainReductionDb (band)) < 0.05f,
                         "Sidechain channel " + std::to_string (targetChannel + 1)
                             + " leaked into Band " + std::to_string (band + 1));
    }

    // The four keys must also remain independent when all are present at once.
    {
        Processor processor;
        setLayout (processor, 2, 4);
        disableAllBands (processor);
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            configureDetectorBand (processor, band,
                                   static_cast<float> (keyFrequency),
                                   tbmbc::params::externalSource);
        processor.prepareToPlay (testSampleRate, blockSize);
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 4,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.5f, 0.7f, keyFrequency, keyFrequency);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     "simultaneous four-bus sidechain became non-finite");
        }
        require (processor.getExternalSidechainChannelCount() == 4,
                 "simultaneous four-bus sidechain lost its channel count");
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            require (processor.getGainReductionDb (band) < -6.0f,
                     "simultaneous Sidechain channel did not duck Band "
                         + std::to_string (band + 1));
    }

    // Hosts are allowed to expose only one of the optional sends at a time.
    // Verify that a sparse layout still preserves the fixed bus-index to band
    // mapping instead of collapsing the active send onto Band 1.
    for (int targetBand = 0; targetBand < tbmbc::params::numBands; ++targetBand)
    {
        std::array<bool, tbmbc::params::numBands> active {};
        active[static_cast<std::size_t> (targetBand)] = true;
        Processor processor;
        setSparseLayout (processor, 2, active);
        disableAllBands (processor);
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            configureDetectorBand (processor, band,
                                   static_cast<float> (keyFrequency),
                                   tbmbc::params::externalSource);
        processor.prepareToPlay (testSampleRate, blockSize);
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 1,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.0f, 0.7f, keyFrequency, keyFrequency);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     "sparse sidechain layout became non-finite");
        }
        require (processor.isExternalSidechainBusActive()
                     && processor.getExternalSidechainChannelCount() == 1,
                 "sparse sidechain bus did not publish one active key");
        require (processor.getGainReductionDb (targetBand) < -6.0f,
                 "sparse Sidechain bus did not drive matching Band "
                     + std::to_string (targetBand + 1));
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (band != targetBand)
                require (std::abs (processor.getGainReductionDb (band)) < 0.05f,
                         "sparse Sidechain bus leaked into Band "
                             + std::to_string (band + 1));

        // An unavailable key must fall back to that band's internal detector,
        // while the connected (but silent) key remains external and silent.
        processor.reset();
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 1,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.7f, 0.0f, keyFrequency, keyFrequency);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     "sparse internal fallback became non-finite");
        }
        require (std::abs (processor.getGainReductionDb (targetBand)) < 0.05f,
                 "connected silent Sidechain bus did not remain external");
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (band != targetBand)
                require (processor.getGainReductionDb (band) < -3.0f,
                         "missing Sidechain bus did not fall back to internal detection");
    }

    {
        Processor processor;
        setLayout (processor, 2, 1);
        disableAllBands (processor);
        configureDetectorBand (processor, 0, 500.0f, tbmbc::params::externalSource);
        processor.prepareToPlay (testSampleRate, blockSize);
        auto peak = 0.0f;
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 1,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.0f, 0.9f, 500.0, 500.0);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            peak = std::max (peak, maximumMainMagnitude (buffer, 2));
        }
        require (processor.getGainReductionDb (0) < -6.0f,
                 "external key did not reach the detector");
        require (peak == 0.0f, "external sidechain leaked into the main output");
    }

    // A stereo source routed to any sidechain bus must be downmixed inside the
    // plugin, so a right-only source is not lost as a first-channel-only key
    // and cannot trigger a different band's detector.
    for (int targetBand = 0; targetBand < tbmbc::params::numBands; ++targetBand)
    {
        Processor processor;
        setStereoSidechainLayout (processor, 2, targetBand + 1);
        disableAllBands (processor);
        configureDetectorBand (processor, targetBand, 500.0f,
                               tbmbc::params::externalSource);
        processor.prepareToPlay (testSampleRate, blockSize);
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            buffer.clear();
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto phase = juce::MathConstants<double>::twoPi
                                   * 500.0 * (block * blockSize + sample)
                                   / testSampleRate;
                buffer.setSample (3, sample, 0.8f * std::sin ((float) phase));
            }
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     "stereo sidechain downmix render became non-finite");
        }
        require (processor.getGainReductionDb (targetBand) < -6.0f,
                 "right-only stereo sidechain was reduced to channel 1 only");
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (band != targetBand)
                require (std::abs (processor.getGainReductionDb (band)) < 0.05f,
                         "stereo Sidechain bus triggered a non-matching band");
    }

    {
        Processor externalChoice;
        Processor internalChoice;
        setLayout (externalChoice, 2, 0);
        setLayout (internalChoice, 2, 0);
        disableAllBands (externalChoice);
        disableAllBands (internalChoice);
        configureDetectorBand (externalChoice, 0, 1000.0f,
                               tbmbc::params::externalSource);
        configureDetectorBand (internalChoice, 0, 1000.0f,
                               tbmbc::params::internalSource);
        externalChoice.prepareToPlay (testSampleRate, blockSize);
        internalChoice.prepareToPlay (testSampleRate, blockSize);

        auto maximumDifference = 0.0;
        for (int block = 0; block < 96; ++block)
        {
            auto externalBuffer = makeProcessBuffer (externalChoice, blockSize);
            auto internalBuffer = makeProcessBuffer (internalChoice, blockSize);
            fillProcessBuffer (externalBuffer, 2, 0,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.5f, 0.0f, 1000.0, 500.0);
            internalBuffer.makeCopyOf (externalBuffer);
            juce::MidiBuffer firstMidi, secondMidi;
            externalChoice.processBlock (externalBuffer, firstMidi);
            internalChoice.processBlock (internalBuffer, secondMidi);
            maximumDifference = std::max (
                maximumDifference,
                maximumMainDifference (externalBuffer, copyMain (internalBuffer, 2)));
        }

        require (! externalChoice.isExternalSidechainBusActive(),
                 "disabled sidechain bus was reported active");
        require (maximumDifference <= 1.0e-7
                     && std::abs (externalChoice.getGainReductionDb (0)
                                  - internalChoice.getGainReductionDb (0)) <= 1.0e-5f,
                 "no-bus EXT did not deterministically fall back to INT");
    }

    {
        Processor follow;
        Processor compatibilityFree;
        setLayout (follow, 2, 0);
        setLayout (compatibilityFree, 2, 0);
        disableAllBands (follow);
        disableAllBands (compatibilityFree);
        configureDetectorBand (follow, 0, 1000.0f,
                               tbmbc::params::internalSource);
        configureDetectorBand (compatibilityFree, 0, 1000.0f,
                               tbmbc::params::internalSource);
        setPlainValue (compatibilityFree, tbmbc::params::bandId (
                           0, tbmbc::params::detectorModeSuffix),
                       static_cast<float> (tbmbc::params::freeDetector));
        setPlainValue (compatibilityFree, tbmbc::params::bandId (
                           0, tbmbc::params::detectorFrequencySuffix), 80.0f);
        setPlainValue (compatibilityFree, tbmbc::params::bandId (
                           0, tbmbc::params::detectorBandwidthSuffix), 0.25f);
        follow.prepareToPlay (testSampleRate, blockSize);
        compatibilityFree.prepareToPlay (testSampleRate, blockSize);

        auto maximumDifference = 0.0;
        for (int block = 0; block < 128; ++block)
        {
            auto followBuffer = makeProcessBuffer (follow, blockSize);
            auto compatibilityBuffer = makeProcessBuffer (
                compatibilityFree, blockSize);
            fillProcessBuffer (followBuffer, 2, 0,
                               static_cast<std::int64_t> (block) * blockSize,
                               0.5f, 0.0f, 1000.0);
            compatibilityBuffer.makeCopyOf (followBuffer);
            juce::MidiBuffer firstMidi, secondMidi;
            follow.processBlock (followBuffer, firstMidi);
            compatibilityFree.processBlock (compatibilityBuffer, secondMidi);
            maximumDifference = std::max (
                maximumDifference,
                maximumMainDifference (compatibilityBuffer,
                                       copyMain (followBuffer, 2)));
        }

        require (getPlainValue (compatibilityFree, tbmbc::params::bandId (
                     0, tbmbc::params::detectorModeSuffix))
                     == static_cast<float> (tbmbc::params::freeDetector),
                 "compatibility FREE value was not preserved in public state");
        require (maximumDifference <= 1.0e-7
                     && std::abs (follow.getGainReductionDb (0)
                                  - compatibilityFree.getGainReductionDb (0))
                            <= 1.0e-5f,
                 "compatibility FREE parameters changed effective FOLLOW audio");
    }
}

void testQualityLatencyAndMonoChannelFallback()
{
    Processor quality;
    setLayout (quality, 2, 0);
    quality.setRateAndBufferSizeDetails (testSampleRate, 257);
    quality.prepareToPlay (testSampleRate, 257);
    require (quality.getLatencySamples() == 0,
             "Dynamic/OFF/0ms default is not zero latency");

    std::int64_t firstSample = 0;
    const auto processFinite = [&]
    {
        for (int block = 0; block < 24; ++block)
        {
            auto buffer = makeProcessBuffer (quality, 257);
            fillProcessBuffer (buffer, 2, 0, firstSample);
            firstSample += buffer.getNumSamples();
            juce::MidiBuffer midi;
            quality.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     "quality or latency mode produced non-finite audio");
        }
    };
    const auto commitLatencySettings = [&] (int phaseMode, int oversampling,
                                              float lookaheadMs)
    {
        setPlainValue (quality, tbmbc::params::phaseModeId,
                       static_cast<float> (phaseMode));
        setPlainValue (quality, tbmbc::params::oversamplingId,
                       static_cast<float> (oversampling));
        setPlainValue (quality, tbmbc::params::lookaheadId, lookaheadMs);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    };

    commitLatencySettings (tbmbc::params::linearPhaseMode,
                           tbmbc::params::oversamplingOff, 0.0f);
    require (quality.getLatencySamples() > 0,
             "Linear mode did not publish positive latency");
    processFinite();

    commitLatencySettings (tbmbc::params::dynamicPhaseMode,
                           tbmbc::params::oversamplingOff, 0.0f);
    require (quality.getLatencySamples() == 0,
             "returning to Dynamic/OFF/0ms did not restore zero latency");

    commitLatencySettings (tbmbc::params::dynamicPhaseMode,
                           tbmbc::params::oversampling2x, 0.0f);
    require (quality.getLatencySamples() > 0,
             "2x oversampling did not publish positive latency");
    processFinite();

    commitLatencySettings (tbmbc::params::dynamicPhaseMode,
                           tbmbc::params::oversampling4x, 0.0f);
    const auto fourTimesLatency = quality.getLatencySamples();
    require (fourTimesLatency > 0,
             "4x oversampling did not publish positive latency");
    processFinite();

    commitLatencySettings (tbmbc::params::dynamicPhaseMode,
                           tbmbc::params::oversampling8x, 0.0f);
    require (quality.getLatencySamples() >= fourTimesLatency,
             "8x oversampling did not publish its full latency");
    processFinite();

    commitLatencySettings (tbmbc::params::dynamicPhaseMode,
                           tbmbc::params::oversamplingOff, 10.0f);
    require (quality.getLatencySamples()
                 == juce::roundToInt (testSampleRate * 0.010),
             "10ms Lookahead did not publish the exact sample latency");
    processFinite();
    require (quality.getTailLengthSeconds() == 0.0,
             "quality controls changed the zero-tail contract");

    const auto renderMono = [] (int channelMode)
    {
        Processor processor;
        setLayout (processor, 1, 0);
        disableAllBands (processor);
        configureDetectorBand (processor, 0, 997.0f,
                               tbmbc::params::internalSource, -12.0f);
        setPlainValue (processor, tbmbc::params::bandId (
                           0, tbmbc::params::channelModeSuffix),
                       static_cast<float> (channelMode));
        processor.setRateAndBufferSizeDetails (testSampleRate, blockSize);
        processor.prepareToPlay (testSampleRate, blockSize);

        auto maximumDifference = 0.0;
        std::int64_t firstSample = 0;
        for (int block = 0; block < 128; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 1, 0, firstSample, 0.5f, 0.0f, 997.0);
            firstSample += buffer.getNumSamples();
            const auto reference = copyMain (buffer, 1);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 1),
                     "mono Channel Mode produced non-finite audio");
            if (block >= 32)
                maximumDifference = std::max (
                    maximumDifference, maximumMainDifference (buffer, reference));
        }
        return std::pair<double, float> { maximumDifference,
                                          processor.getGainReductionDb (0) };
    };

    const auto stereo = renderMono (tbmbc::params::stereoChannelMode);
    const auto mid = renderMono (tbmbc::params::midChannelMode);
    const auto side = renderMono (tbmbc::params::sideChannelMode);
    require (stereo.first > 0.01 && stereo.second < -3.0f
                 && mid.first > 0.01 && mid.second < -3.0f
                 && side.first > 0.01 && side.second < -3.0f,
             "mono Channel Mode did not fall back to normal Stereo processing");
    require (std::abs (mid.first - stereo.first) <= 1.0e-7
                 && std::abs (side.first - stereo.first) <= 1.0e-7
                 && std::abs (mid.second - stereo.second) <= 1.0e-5f
                 && std::abs (side.second - stereo.second) <= 1.0e-5f,
             "mono Mid/Side fallback differs from Stereo");
}

void testSpectrumAnalyzerDemandAndTransport()
{
    {
        TBMBCSpectrumAnalyzer analyzer;
        juce::AudioBuffer<float> antiPhase (
            2, TBMBCSpectrumAnalyzer::fftSize * 9);
        for (int sample = 0; sample < antiPhase.getNumSamples(); ++sample)
        {
            const auto value = 0.5f * (float) std::sin (
                juce::MathConstants<double>::twoPi * 1000.0
                * (double) sample / testSampleRate);
            antiPhase.setSample (0, sample, value);
            antiPhase.setSample (1, sample, -value);
        }
        antiPhase.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
        analyzer.push (antiPhase);
        require (analyzer.getPushedSampleCount()
                     <= (std::uint64_t) TBMBCSpectrumAnalyzer::fftSize * 8,
                 "analyzer FIFO exceeded its fixed capacity");
        require (analyzer.pullSpectrum(),
                 "bounded analyzer FIFO did not produce a frame");
        const auto& levels = analyzer.getMagnitudesDb();
        const auto expectedBin = juce::roundToInt (
            1000.0 * TBMBCSpectrumAnalyzer::fftSize / testSampleRate);
        require (std::isfinite (levels[(std::size_t) expectedBin])
                     && levels[(std::size_t) expectedBin] > -12.0f,
                 "anti-phase stereo or non-finite input broke the analyzer frame");
    }

    {
        Processor withoutAnalyzer;
        Processor withAnalyzer;
        setLayout (withoutAnalyzer, 2, 0);
        setLayout (withAnalyzer, 2, 0);
        withoutAnalyzer.setRateAndBufferSizeDetails (testSampleRate, 257);
        withAnalyzer.setRateAndBufferSizeDetails (testSampleRate, 257);
        withoutAnalyzer.prepareToPlay (testSampleRate, 257);
        withAnalyzer.prepareToPlay (testSampleRate, 257);
        auto reference = makeProcessBuffer (withoutAnalyzer, 257);
        auto analyzed = makeProcessBuffer (withAnalyzer, 257);
        fillProcessBuffer (reference, 2, 0, 0);
        fillProcessBuffer (analyzed, 2, 0, 0);
        juce::MidiBuffer referenceMidi;
        juce::MidiBuffer analyzedMidi;
        withAnalyzer.setSpectrumAnalyzerDemand (true);
        withoutAnalyzer.processBlock (reference, referenceMidi);
        withAnalyzer.processBlock (analyzed, analyzedMidi);
        withAnalyzer.setSpectrumAnalyzerDemand (false);
        require (maximumMainDifference (analyzed, copyMain (reference, 2)) == 0.0,
                 "analyzer demand changed the audio output");
    }

    Processor processor;
    setLayout (processor, 2, 0);
    processor.setRateAndBufferSizeDetails (testSampleRate, 257);
    processor.prepareToPlay (testSampleRate, 257);
    juce::MidiBuffer midi;
    std::int64_t firstSample = 0;

    const auto processTone = [&] (int blocks, int samples)
    {
        for (int block = 0; block < blocks; ++block)
        {
            auto buffer = makeProcessBuffer (processor, samples);
            fillProcessBuffer (buffer, 2, 0, firstSample, 0.35f, 0.0f, 997.0);
            firstSample += samples;
            processor.processBlock (buffer, midi);
            require (std::isfinite (maximumMainMagnitude (buffer, 2)),
                     "analyzer transport produced non-finite audio");
        }
    };

    processTone (10, 257);
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount() == 0,
             "analyzer producer ran without display demand");

    processor.setSpectrumAnalyzerDemand (true);
    require (processor.getSpectrumAnalyzerDemandCount() == 1,
             "analyzer demand did not become active");
    processTone (12, 257);
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount()
                 >= (std::uint64_t) TBMBCSpectrumAnalyzer::fftSize,
             "active analyzer did not receive a complete FFT window");
    require (processor.getInputSpectrumAnalyzer().pullSpectrum(),
             "input analyzer did not produce a frame");

    const auto& inputSpectrum = processor.getInputSpectrumAnalyzer().getMagnitudesDb();
    auto peakBin = 1;
    for (int bin = 2; bin < TBMBCSpectrumAnalyzer::numBins; ++bin)
    {
        require (std::isfinite (inputSpectrum[(std::size_t) bin]),
                 "input spectrum contains a non-finite bin");
        if (inputSpectrum[(std::size_t) bin]
            > inputSpectrum[(std::size_t) peakBin])
            peakBin = bin;
    }
    const auto expectedBin = juce::roundToInt (
        997.0 * TBMBCSpectrumAnalyzer::fftSize / testSampleRate);
    require (std::abs (peakBin - expectedBin) <= 2,
             "input analyzer peak is not at the source tone");

    const auto inputCount = processor.getInputSpectrumAnalyzer().getPushedSampleCount();
    processor.setSpectrumAnalyzerDemand (false);
    require (processor.getSpectrumAnalyzerDemandCount() == 0,
             "analyzer demand did not return to zero");
    processTone (10, 513);
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount() == inputCount,
             "analyzer producer kept running after demand was released");

    processor.setNonRealtime (true);
    processor.setSpectrumAnalyzerDemand (true);
    processTone (10, 257);
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount() == inputCount,
             "analyzer producer ran during offline processing");
    processor.setSpectrumAnalyzerDemand (false);
    processor.setNonRealtime (false);
}

void testStateRoundTripAndAtomicRejection()
{
    Processor source;
    require (source.applyFactoryPreset (10, false), "state fixture preset failed");
    setPlainValue (source, tbmbc::params::mixId, 73.2f);
    setPlainValue (source, tbmbc::params::outputId, -2.3f);
    setPlainValue (source, tbmbc::params::bandId (
                       2, tbmbc::params::detectorModeSuffix),
                   static_cast<float> (tbmbc::params::freeDetector));
    setPlainValue (source, tbmbc::params::bandId (
                       2, tbmbc::params::detectorFrequencySuffix), 3456.0f);
    setPlainValue (source, tbmbc::params::phaseModeId,
                   static_cast<float> (tbmbc::params::linearPhaseMode));
    setPlainValue (source, tbmbc::params::oversamplingId,
                   static_cast<float> (tbmbc::params::oversampling4x));
    setPlainValue (source, tbmbc::params::lookaheadId, 7.3f);
    setPlainValue (source, tbmbc::params::bandId (
                       2, tbmbc::params::channelModeSuffix),
                   static_cast<float> (tbmbc::params::sideChannelMode));
    setPlainValue (source, tbmbc::params::bandId (
                       0, tbmbc::params::rangeSuffix), 12.0f);
    setPlainValue (source, tbmbc::params::bandId (
                       1, tbmbc::params::rangeSuffix), -48.0f);
    const auto expected = captureValues (source);

    juce::MemoryBlock saved;
    source.getStateInformation (saved);
    require (! saved.isEmpty(), "DAW state is empty");
    const auto validTree = stateTreeFrom (source);
    require (validTree.hasType ("PARAMETERS")
                 && validTree.getProperty ("product").toString()
                        == "com.TeamImpulseImpact.TB_MultiBandCompressor"
                 && static_cast<int> (validTree.getProperty ("stateSchemaVersion")) == 4
                 && validTree.getNumChildren() == tbmbc::params::parameterCount,
             "state schema/header/parameter count changed");

    Processor restored;
    restored.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    requireValuesEqual (captureValues (restored), expected, "state round-trip");

    const auto positiveLegacyRangeId = tbmbc::params::bandId (
        0, tbmbc::params::rangeSuffix);
    const auto negativeLegacyRangeId = tbmbc::params::bandId (
        1, tbmbc::params::rangeSuffix);
    const auto makePreviousState = [&] (int schema)
    {
        auto result = validTree.createCopy();
        result.setProperty ("stateSchemaVersion", schema, nullptr);
        result.getChildWithProperty ("id", positiveLegacyRangeId)
            .setProperty ("value", 24.0, nullptr);
        result.getChildWithProperty ("id", negativeLegacyRangeId)
            .setProperty ("value", -24.0, nullptr);
        return result;
    };
    auto expectedPrevious = expected;
    expectedPrevious.values[tbmbc::params::bandParameterIndex (
        0, tbmbc::params::rangeOffset)] = 12.0f;
    expectedPrevious.values[tbmbc::params::bandParameterIndex (
        1, tbmbc::params::rangeOffset)] = -24.0f;

    const auto versionThreeTree = makePreviousState (3);
    Processor versionThreeRestored;
    const auto versionThreeBytes = serialiseTree (versionThreeTree);
    versionThreeRestored.setStateInformation (
        versionThreeBytes.getData(), static_cast<int> (versionThreeBytes.getSize()));
    requireValuesEqual (captureValues (versionThreeRestored), expectedPrevious,
                        "legacy v3 state migration and +24 to +12 clamp");

    auto versionTwoTree = makePreviousState (2);
    versionTwoTree.setProperty ("stateSchemaVersion", 2, nullptr);
    Processor versionTwoRestored;
    const auto versionTwoBytes = serialiseTree (versionTwoTree);
    versionTwoRestored.setStateInformation (
        versionTwoBytes.getData(), static_cast<int> (versionTwoBytes.getSize()));
    requireValuesEqual (captureValues (versionTwoRestored), expectedPrevious,
                        "legacy v2 state migration and +24 to +12 clamp");

    auto legacyTree = makeLegacyStateFixture (validTree);
    legacyTree.getChildWithProperty ("id", positiveLegacyRangeId)
        .setProperty ("value", 24.0, nullptr);
    legacyTree.getChildWithProperty ("id", negativeLegacyRangeId)
        .setProperty ("value", -24.0, nullptr);

    tbmbc::presets::LegacyValues legacyValues {};
    for (std::size_t index = 0; index < legacyValues.size(); ++index)
        legacyValues[index] = expected.values[index];
    legacyValues[tbmbc::params::bandParameterIndex (
        0, tbmbc::params::rangeOffset)] = 12.0f;
    legacyValues[tbmbc::params::bandParameterIndex (
        1, tbmbc::params::rangeOffset)] = -24.0f;
    const auto upgradedLegacyValues = tbmbc::presets::upgradeLegacyValues (
        legacyValues);
    Processor legacyRestored;
    const auto legacyBytes = serialiseTree (legacyTree);
    legacyRestored.setStateInformation (legacyBytes.getData(),
                                        static_cast<int> (legacyBytes.getSize()));
    requireValuesEqual (captureValues (legacyRestored), upgradedLegacyValues,
                        "legacy v1 state migration and +24 to +12 clamp");

    Processor target;
    require (target.applyFactoryPreset (2, false), "atomic baseline preset failed");
    setPlainValue (target, tbmbc::params::mixId, 61.0f);
    const auto baseline = captureValues (target);
    const auto assertTreeRejected = [&] (const juce::ValueTree& invalid,
                                         const std::string& description)
    {
        const auto bytes = serialiseTree (invalid);
        target.setStateInformation (bytes.getData(), static_cast<int> (bytes.getSize()));
        requireValuesEqual (captureValues (target), baseline,
                            description + " atomic rejection");
    };

    auto future = validTree.createCopy();
    future.setProperty ("stateSchemaVersion", 5, nullptr);
    assertTreeRejected (future, "future state");

    auto forgedVersionTwo = versionTwoTree.createCopy();
    forgedVersionTwo.getChildWithProperty ("id", tbmbc::params::oversamplingId)
        .setProperty ("value", tbmbc::params::oversampling8x, nullptr);
    assertTreeRejected (forgedVersionTwo, "v2 state with 8x choice");

    auto partial = validTree.createCopy();
    partial.removeChild (partial.getNumChildren() - 1, nullptr);
    assertTreeRejected (partial, "partial state");

    auto corrupt = validTree.createCopy();
    corrupt.getChild (0).removeProperty ("value", nullptr);
    assertTreeRejected (corrupt, "corrupt state");

    auto wrongProduct = validTree.createCopy();
    wrongProduct.setProperty ("product", "wrong.product", nullptr);
    assertTreeRejected (wrongProduct, "foreign state");

    auto duplicate = validTree.createCopy();
    duplicate.getChild (duplicate.getNumChildren() - 1)
        .setProperty ("id", duplicate.getChild (0).getProperty ("id"), nullptr);
    assertTreeRejected (duplicate, "duplicate-ID state");

    auto fractionalChoice = validTree.createCopy();
    fractionalChoice.getChildWithProperty (
        "id", tbmbc::params::bandId (0, tbmbc::params::sourceSuffix))
        .setProperty ("value", 0.5, nullptr);
    assertTreeRejected (fractionalChoice, "fractional choice state");

    auto fractionalPhaseMode = validTree.createCopy();
    fractionalPhaseMode.getChildWithProperty ("id", tbmbc::params::phaseModeId)
        .setProperty ("value", 0.5, nullptr);
    assertTreeRejected (fractionalPhaseMode, "fractional Phase Mode state");

    auto outOfRangeLookahead = validTree.createCopy();
    outOfRangeLookahead.getChildWithProperty ("id", tbmbc::params::lookaheadId)
        .setProperty ("value", 20.1, nullptr);
    assertTreeRejected (outOfRangeLookahead, "out-of-range Lookahead state");

    auto rangeAboveCurrentMaximum = validTree.createCopy();
    rangeAboveCurrentMaximum.getChildWithProperty (
        "id", positiveLegacyRangeId).setProperty ("value", 12.1, nullptr);
    assertTreeRejected (rangeAboveCurrentMaximum,
                        "v4 Range above +12 dB");

    auto rangeBelowCurrentMinimum = validTree.createCopy();
    rangeBelowCurrentMinimum.getChildWithProperty (
        "id", positiveLegacyRangeId).setProperty ("value", -48.1, nullptr);
    assertTreeRejected (rangeBelowCurrentMinimum,
                        "v4 Range below -48 dB");

    auto rangeAboveLegacyMaximum = versionThreeTree.createCopy();
    rangeAboveLegacyMaximum.getChildWithProperty (
        "id", positiveLegacyRangeId).setProperty ("value", 24.1, nullptr);
    assertTreeRejected (rangeAboveLegacyMaximum,
                        "v3 Range above legacy +24 dB");

    auto rangeBelowLegacyMinimum = versionThreeTree.createCopy();
    rangeBelowLegacyMinimum.getChildWithProperty (
        "id", positiveLegacyRangeId).setProperty ("value", -24.1, nullptr);
    assertTreeRejected (rangeBelowLegacyMinimum,
                        "v3 Range below legacy -24 dB");

    auto partialLegacy = legacyTree.createCopy();
    partialLegacy.removeChild (partialLegacy.getNumChildren() - 1, nullptr);
    assertTreeRejected (partialLegacy, "partial legacy state");

    auto legacyWithNewFields = validTree.createCopy();
    legacyWithNewFields.setProperty ("stateSchemaVersion", 1, nullptr);
    assertTreeRejected (legacyWithNewFields, "oversized legacy state");

    const char garbage[] = "not a JUCE state";
    target.setStateInformation (garbage, static_cast<int> (sizeof (garbage) - 1));
    requireValuesEqual (captureValues (target), baseline, "garbage state rejection");

    target.setStateInformation (saved.getData(), static_cast<int> (saved.getSize() / 3));
    requireValuesEqual (captureValues (target), baseline, "truncated state rejection");
}

void dirtyAgainstPreset (Processor& processor, const tbmbc::presets::Values& preset)
{
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (std::size_t index = 0; index < metadata.size(); ++index)
    {
        const auto& item = metadata[index];
        float alternate = 0.0f;
        if (item.kind == tbmbc::params::ParameterKind::continuous)
            alternate = std::abs (preset.values[index] - item.minimum) > valueTolerance
                            ? item.minimum
                            : item.maximum;
        else
            alternate = preset.values[index] < 0.5f ? 1.0f : 0.0f;
        setPlainValue (processor, item.id, alternate);
    }
}

void testFactoryPresetsAndUndoRedo()
{
    Processor processor;
    require (processor.getFactoryPresetCount() == 20
                 && tbmbc::presets::factory.size() == 20,
             "Factory preset count is not 20");
    constexpr std::array<const char*, 20> categories {{
        "START", "START", "START", "SOURCE", "SOURCE", "SOURCE",
        "BUS", "BUS", "BUS", "MOTION", "MOTION", "MOTION",
        "MASTER", "MASTER", "MASTER", "MASTER", "MASTER",
        "SOURCE", "SOURCE", "BUS"
    }};
    constexpr std::array<const char*, 20> expectedNames {{
        "Init", "Gentle Balance", "Broad Control", "Vocal Focus",
        "Bass Foundation", "Drum Tamer", "Mix Bus Balance", "Drum Bus Glue",
        "Mastering Touch", "Kick Duck", "Rhythmic Pump", "Transient Lift",
        "Transparent Master", "Loudness Guard", "De-Harsh Master",
        "Low End Anchor", "Stereo Focus", "Guitar Smooth", "Keys Space",
        "Parallel Drum Punch"
    }};
    constexpr std::array<int, 20> phaseModes {{
        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 1, 1, 1, 0, 0, 0
    }};
    constexpr std::array<int, 20> oversamplingModes {{
        0, 0, 0, 1, 0, 2, 1, 0, 1, 0, 0, 2,
        2, 3, 3, 2, 2, 1, 2, 3
    }};
    constexpr std::array<float, 20> lookaheadValues {{
        0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 3.0f,
        1.0f, 0.0f, 5.0f, 3.0f, 0.0f, 1.0f,
        5.0f, 10.0f, 8.0f, 6.0f, 5.0f, 2.0f, 1.0f, 1.5f
    }};
    constexpr int stereo = tbmbc::params::stereoChannelMode;
    constexpr int mid = tbmbc::params::midChannelMode;
    constexpr int side = tbmbc::params::sideChannelMode;
    constexpr std::array<std::array<int, 4>, 20> channelModes {{
        {{ stereo, stereo, stereo, stereo }},
        {{ stereo, stereo, stereo, stereo }},
        {{ stereo, stereo, stereo, stereo }},
        {{ mid,    mid,    mid,    stereo }},
        {{ mid,    mid,    stereo, stereo }},
        {{ stereo, stereo, stereo, stereo }},
        {{ mid,    mid,    stereo, side   }},
        {{ stereo, stereo, stereo, stereo }},
        {{ mid,    stereo, stereo, side   }},
        {{ stereo, stereo, stereo, stereo }},
        {{ stereo, stereo, stereo, stereo }},
        {{ mid,    mid,    side,   stereo }},
        {{ mid,    mid,    stereo, side   }},
        {{ mid,    mid,    stereo, stereo }},
        {{ mid,    mid,    side,   side   }},
        {{ mid,    mid,    stereo, stereo }},
        {{ side,   mid,    stereo, side   }},
        {{ mid,    mid,    stereo, side   }},
        {{ mid,    stereo, mid,    side   }},
        {{ mid,    stereo, stereo, side   }}
    }};
    juce::StringArray names;
    bool sawDynamic = false;
    bool sawLinear = false;
    bool sawOversamplingOff = false;
    bool sawOversampling2x = false;
    bool sawOversampling4x = false;
    bool sawOversampling8x = false;
    bool sawLookahead = false;
    bool sawStereo = false;
    bool sawMid = false;
    bool sawSide = false;
    bool sawExternal = false;
    bool sawPositiveRange = false;
    bool sawNegativeRange = false;
    bool sawExpand = false;

    for (std::size_t index = 0; index < tbmbc::presets::factory.size(); ++index)
    {
        const auto& preset = tbmbc::presets::factory[index];
        require (preset.id == static_cast<int> (index + 1)
                     && juce::String (preset.category) == categories[index],
                 "Factory ID/category order changed");
        require (juce::String (preset.name) == expectedNames[index],
                 "Factory preset name/order changed");
        require (names.addIfNotAlreadyThere (preset.name),
                 "Factory preset names are not unique");
        require (tbmbc::presets::isValid (preset.values),
                 std::string (preset.name) + " contains an invalid value");
        require (preset.values.values[tbmbc::params::phaseModeParameterIndex]
                         == static_cast<float> (phaseModes[index])
                     && preset.values.values[tbmbc::params::oversamplingParameterIndex]
                            == static_cast<float> (oversamplingModes[index])
                     && preset.values.values[tbmbc::params::lookaheadParameterIndex]
                            == lookaheadValues[index],
                 std::string (preset.name)
                     + " quality snapshot changed");

        sawDynamic = sawDynamic || phaseModes[index] == tbmbc::params::dynamicPhaseMode;
        sawLinear = sawLinear || phaseModes[index] == tbmbc::params::linearPhaseMode;
        sawOversamplingOff = sawOversamplingOff
                              || oversamplingModes[index] == tbmbc::params::oversamplingOff;
        sawOversampling2x = sawOversampling2x
                             || oversamplingModes[index] == tbmbc::params::oversampling2x;
        sawOversampling4x = sawOversampling4x
                             || oversamplingModes[index] == tbmbc::params::oversampling4x;
        sawOversampling8x = sawOversampling8x
                             || oversamplingModes[index] == tbmbc::params::oversampling8x;
        sawLookahead = sawLookahead || lookaheadValues[index] > 0.0f;

        for (int band = 0; band < tbmbc::params::numBands; ++band)
        {
            require (preset.values.values[tbmbc::params::bandParameterIndex (
                         band, tbmbc::params::auditionOffset)] == 0.0f,
                     std::string (preset.name) + " persisted Audition on");
            require (preset.values.values[tbmbc::params::channelModeParameterIndex (
                         band)] == static_cast<float> (channelModes[index][band]),
                     std::string (preset.name) + " Channel Mode snapshot changed");
            require (preset.values.values[tbmbc::params::bandParameterIndex (
                         band, tbmbc::params::detectorModeOffset)]
                         == static_cast<float> (tbmbc::params::bandDetector),
                     std::string (preset.name) + " did not use FOLLOW/BAND detection");
            require (std::abs (preset.values.values[tbmbc::params::bandParameterIndex (
                                  band, tbmbc::params::detectorFrequencyOffset)]
                               - preset.values.values[tbmbc::params::bandParameterIndex (
                                  band, tbmbc::params::frequencyOffset)])
                         <= valueTolerance
                         && std::abs (preset.values.values[tbmbc::params::bandParameterIndex (
                                         band, tbmbc::params::detectorBandwidthOffset)]
                                      - preset.values.values[tbmbc::params::bandParameterIndex (
                                         band, tbmbc::params::bandwidthOffset)])
                                <= valueTolerance,
                     std::string (preset.name)
                         + " detector did not follow processing frequency/Q");

            if (preset.values.values[tbmbc::params::bandParameterIndex (
                    band, tbmbc::params::enabledOffset)] < 0.5f)
                continue;

            const auto channel = channelModes[index][band];
            sawStereo = sawStereo || channel == stereo;
            sawMid = sawMid || channel == mid;
            sawSide = sawSide || channel == side;
            sawExternal = sawExternal
                           || preset.values.values[tbmbc::params::bandParameterIndex (
                                  band, tbmbc::params::sourceOffset)]
                                  == static_cast<float> (tbmbc::params::externalSource);
            const auto range = preset.values.values[tbmbc::params::bandParameterIndex (
                band, tbmbc::params::rangeOffset)];
            sawPositiveRange = sawPositiveRange || range > 0.0f;
            sawNegativeRange = sawNegativeRange || range < 0.0f;
            sawExpand = sawExpand
                        || preset.values.values[tbmbc::params::bandParameterIndex (
                               band, tbmbc::params::modeOffset)]
                               == static_cast<float> (tbmbc::params::expandMode);
        }

        require (preset.values.values[tbmbc::params::outputParameterIndex] == 0.0f,
                 std::string (preset.name)
                     + " uses hidden global Output instead of unity");

        dirtyAgainstPreset (processor, preset.values);
        require (processor.applyFactoryPreset (preset.id, false),
                 std::string ("could not apply ") + preset.name);
        requireValuesEqual (captureValues (processor), preset.values,
                            std::string (preset.name) + " full recall");
        require (processor.getActiveFactoryPresetId() == preset.id
                     && tbmbc::presets::findMatch (captureValues (processor)) == preset.id,
                 std::string (preset.name) + " selection/match failed");
    }

    for (std::size_t left = 0; left < tbmbc::presets::factory.size(); ++left)
        for (std::size_t right = left + 1; right < tbmbc::presets::factory.size(); ++right)
            require (! tbmbc::presets::matches (
                         tbmbc::presets::factory[left].values,
                         tbmbc::presets::factory[right].values),
                     "Factory presets contain duplicate states");

    require (sawDynamic && sawLinear && sawOversamplingOff
                 && sawOversampling2x && sawOversampling4x
                 && sawOversampling8x && sawLookahead,
             "Factory bank does not cover all quality and timing modes");
    require (sawStereo && sawMid && sawSide && sawExternal,
             "Factory bank does not cover channel and sidechain modes");
    require (sawPositiveRange && sawNegativeRange && sawExpand,
             "Factory bank does not cover signed Range and expansion");

    const auto requirePositiveBandModes = [] (int presetId, int expectedMode)
    {
        const auto* preset = tbmbc::presets::findById (presetId);
        require (preset != nullptr, "missing signed-Range Factory preset");
        auto positiveBandCount = 0;
        for (int band = 0; band < tbmbc::params::numBands; ++band)
        {
            const auto enabled = preset->values.values[
                tbmbc::params::bandParameterIndex (
                    band, tbmbc::params::enabledOffset)] >= 0.5f;
            const auto range = preset->values.values[
                tbmbc::params::bandParameterIndex (
                    band, tbmbc::params::rangeOffset)];
            if (! enabled || range <= 0.0f)
                continue;
            ++positiveBandCount;
            require (preset->values.values[
                         tbmbc::params::bandParameterIndex (
                             band, tbmbc::params::modeOffset)]
                         == static_cast<float> (expectedMode),
                     std::string (preset->name)
                         + " positive Range/dynamics mode pairing changed");
        }
        require (positiveBandCount > 0,
                 std::string (preset->name) + " lost all positive Range bands");
    };
    requirePositiveBandModes (12, tbmbc::params::compressMode);
    requirePositiveBandModes (19, tbmbc::params::expandMode);
    requirePositiveBandModes (20, tbmbc::params::compressMode);

    const auto& init = tbmbc::presets::factory.front().values;
    const auto& metadata = tbmbc::params::allParameterMetadata();
    for (std::size_t index = 0; index < metadata.size(); ++index)
        require (std::abs (init.values[index] - metadata[index].defaultValue)
                     <= valueTolerance,
                 "Init differs from parameter defaults");

    require (processor.applyFactoryPreset (1, false), "could not establish Undo baseline");
    const auto undoBaseline = captureValues (processor);
    processor.getUndoManager().clearUndoHistory();
    require (processor.applyFactoryPreset (11, true), "Undo target preset failed");
    const auto redoTarget = tbmbc::presets::factory[10].values;
    require (processor.canUndo(), "Factory recall created no Undo transaction");
    processor.undo();
    requireValuesEqual (captureValues (processor), undoBaseline,
                        "Factory preset Undo");
    require (processor.canRedo(), "Factory preset Undo created no Redo transaction");
    processor.redo();
    requireValuesEqual (captureValues (processor), redoTarget,
                        "Factory preset Redo");
}

void testFactoryPresetAudioSmoke()
{
    constexpr std::array<int, 7> sampleCounts {{ 1, 63, 257, 65, 513, 3, 256 }};

    for (const auto& preset : tbmbc::presets::factory)
    {
        Processor processor;
        setLayout (processor, 2, 2);
        processor.setRateAndBufferSizeDetails (testSampleRate, 257);
        processor.prepareToPlay (testSampleRate, 257);
        require (processor.applyFactoryPreset (preset.id, false),
                 std::string ("could not load audio-smoke preset ") + preset.name);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);

        std::int64_t firstSample = 0;
        auto maximumPeak = 0.0f;
        for (const auto samples : sampleCounts)
        {
            auto buffer = makeProcessBuffer (processor, samples);
            fillProcessBuffer (buffer, 2, 2, firstSample);
            firstSample += samples;
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            require (mainIsFinite (buffer, 2),
                     std::string (preset.name) + " produced NaN/Inf");
            maximumPeak = std::max (maximumPeak, maximumMainMagnitude (buffer, 2));
        }

        require (std::isfinite (maximumPeak) && maximumPeak < 4.0f,
                 std::string (preset.name) + " produced unsafe smoke-test output");
        processor.releaseResources();
    }
}

class ScopedTestDirectory
{
public:
    explicit ScopedTestDirectory (juce::File directoryToUse)
        : directory (std::move (directoryToUse))
    {
    }

    ~ScopedTestDirectory()
    {
        if (active && directory.isDirectory())
            directory.deleteRecursively();
    }

    void dismiss() noexcept { active = false; }

    juce::File directory;

private:
    bool active = true;
};

void testUserPresetRoundTripInTempOnly()
{
    const auto systemTemp = juce::File::getSpecialLocation (
        juce::File::tempDirectory);
    auto testDirectory = systemTemp.getNonexistentChildFile (
        "TB_MultiBandCompressor_ProcessorIntegration", {}, false);
    require (testDirectory.isAChildOf (systemTemp),
             "refusing to create a test directory outside the system temp folder");
    require (testDirectory.createDirectory().wasOk(),
             "could not create isolated preset test directory");
    ScopedTestDirectory cleanup (testDirectory);

    Processor processor;
    require (processor.applyFactoryPreset (4, false), "user preset fixture failed");
    setPlainValue (processor, tbmbc::params::mixId, 88.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       1, tbmbc::params::rangeSuffix), 4.2f);
    setPlainValue (processor, tbmbc::params::bandId (
                       0, tbmbc::params::auditionSuffix), 1.0f);
    setPlainValue (processor, tbmbc::params::bandId (
                       2, tbmbc::params::auditionSuffix), 1.0f);
    setPlainValue (processor, tbmbc::params::phaseModeId,
                   static_cast<float> (tbmbc::params::linearPhaseMode));
    setPlainValue (processor, tbmbc::params::oversamplingId,
                   static_cast<float> (tbmbc::params::oversampling2x));
    setPlainValue (processor, tbmbc::params::lookaheadId, 5.7f);
    setPlainValue (processor, tbmbc::params::bandId (
                       1, tbmbc::params::channelModeSuffix),
                   static_cast<float> (tbmbc::params::midChannelMode));
    auto expected = captureValues (processor);
    for (int band = 0; band < tbmbc::params::numBands; ++band)
        expected.values[tbmbc::params::bandParameterIndex (
            band, tbmbc::params::auditionOffset)] = 0.0f;

    const auto presetFile = testDirectory.getChildFile ("Roundtrip.tbmbc");
    require (! presetFile.isAChildOf (Processor::getUserPresetDirectory()),
             "test preset unexpectedly targets the user Documents preset directory");
    const auto saveResult = processor.saveUserPreset (presetFile);
    require (saveResult.wasOk() && presetFile.existsAsFile()
                 && presetFile.getSize() > 0,
             "user preset save failed: " + saveResult.getErrorMessage().toStdString());

    require (processor.applyFactoryPreset (12, false), "could not dirty user state");
    const auto loadResult = processor.loadUserPreset (presetFile, true);
    require (loadResult.wasOk(),
             "user preset load failed: " + loadResult.getErrorMessage().toStdString());
    requireValuesEqual (captureValues (processor), expected, "user preset round-trip");
    require (processor.getCurrentPresetName() == "Roundtrip"
                  && processor.getCurrentUserPresetFile() == presetFile,
              "loaded user preset selection was not retained");

    processor.undo();
    requireValuesEqual (captureValues (processor),
                        tbmbc::presets::factory[11].values,
                        "user preset load Undo");
    require (processor.getCurrentPresetName() == "Transient Lift",
             "user preset Undo did not restore the previous selection");
    processor.redo();
    requireValuesEqual (captureValues (processor), expected,
                        "user preset load Redo");
    require (processor.getCurrentPresetName() == "Roundtrip"
                  && processor.getCurrentUserPresetFile() == presetFile,
             "user preset Redo did not restore its selection");

    const auto savedXml = juce::parseXML (presetFile);
    require (savedXml != nullptr, "could not parse current user preset fixture");
    const auto currentPreset = juce::ValueTree::fromXml (*savedXml);
    require (currentPreset.isValid() && currentPreset.getNumChildren() == 1,
             "current user preset fixture has an invalid shape");
    require (static_cast<int> (currentPreset.getProperty (
                 "presetFormatVersion")) == 4
                 && static_cast<int> (currentPreset.getProperty (
                        "stateSchemaVersion")) == 4,
             "current user preset did not save the v4 format/schema");

    const auto positiveLegacyRangeId = tbmbc::params::bandId (
        0, tbmbc::params::rangeSuffix);
    const auto negativeLegacyRangeId = tbmbc::params::bandId (
        1, tbmbc::params::rangeSuffix);
    const auto makePreviousPreset = [&] (int version)
    {
        auto result = currentPreset.createCopy();
        result.setProperty ("presetFormatVersion", version, nullptr);
        result.setProperty ("stateSchemaVersion", version, nullptr);
        auto state = result.getChild (0);
        state.setProperty ("stateSchemaVersion", version, nullptr);
        state.getChildWithProperty ("id", positiveLegacyRangeId)
            .setProperty ("value", 24.0, nullptr);
        state.getChildWithProperty ("id", negativeLegacyRangeId)
            .setProperty ("value", -24.0, nullptr);
        return result;
    };
    auto expectedPrevious = expected;
    expectedPrevious.values[tbmbc::params::bandParameterIndex (
        0, tbmbc::params::rangeOffset)] = 12.0f;
    expectedPrevious.values[tbmbc::params::bandParameterIndex (
        1, tbmbc::params::rangeOffset)] = -24.0f;

    const auto loadPreviousPreset = [&] (int version)
    {
        const auto previousPreset = makePreviousPreset (version);
        const auto previousPresetFile = testDirectory.getChildFile (
            "Version" + juce::String (version) + ".tbmbc");
        const auto previousXml = previousPreset.createXml();
        require (previousXml != nullptr
                     && previousXml->writeTo (previousPresetFile, {}),
                 "could not write v" + std::to_string (version)
                     + " user preset fixture");
        require (processor.applyFactoryPreset (12, false),
                 "could not dirty previous user preset state");
        const auto result = processor.loadUserPreset (previousPresetFile, false);
        require (result.wasOk(),
                 "v" + std::to_string (version)
                     + " user preset migration failed: "
                     + result.getErrorMessage().toStdString());
        requireValuesEqual (captureValues (processor), expectedPrevious,
                            "v" + std::to_string (version)
                                + " user preset migration and +24 to +12 clamp");
    };
    loadPreviousPreset (3);
    loadPreviousPreset (2);

    auto legacyPreset = currentPreset.createCopy();
    legacyPreset.setProperty ("presetFormatVersion", 1, nullptr);
    legacyPreset.setProperty ("stateSchemaVersion", 1, nullptr);
    auto legacyState = makeLegacyStateFixture (legacyPreset.getChild (0));
    legacyState.getChildWithProperty ("id", positiveLegacyRangeId)
        .setProperty ("value", 24.0, nullptr);
    legacyState.getChildWithProperty ("id", negativeLegacyRangeId)
        .setProperty ("value", -24.0, nullptr);
    legacyPreset.removeChild (0, nullptr);
    legacyPreset.addChild (legacyState, 0, nullptr);

    const auto legacyPresetFile = testDirectory.getChildFile ("Legacy.tbmbc");
    const auto legacyXml = legacyPreset.createXml();
    require (legacyXml != nullptr && legacyXml->writeTo (legacyPresetFile, {}),
             "could not write legacy user preset fixture");
    tbmbc::presets::LegacyValues legacyValues {};
    for (std::size_t index = 0; index < legacyValues.size(); ++index)
        legacyValues[index] = expected.values[index];
    legacyValues[tbmbc::params::bandParameterIndex (
        0, tbmbc::params::rangeOffset)] = 12.0f;
    legacyValues[tbmbc::params::bandParameterIndex (
        1, tbmbc::params::rangeOffset)] = -24.0f;
    const auto upgradedLegacyValues = tbmbc::presets::upgradeLegacyValues (
        legacyValues);

    require (processor.applyFactoryPreset (12, false),
             "could not dirty legacy user preset state");
    const auto legacyLoadResult = processor.loadUserPreset (legacyPresetFile, false);
    require (legacyLoadResult.wasOk(),
             "legacy user preset migration failed: "
                 + legacyLoadResult.getErrorMessage().toStdString());
    requireValuesEqual (captureValues (processor), upgradedLegacyValues,
                        "legacy user preset migration and +24 to +12 clamp");

    const auto legacyBaseline = captureValues (processor);
    auto malformedLegacyPreset = legacyPreset.createCopy();
    auto malformedLegacyState = malformedLegacyPreset.getChild (0);
    malformedLegacyState.removeChild (malformedLegacyState.getNumChildren() - 1,
                                      nullptr);
    const auto malformedPresetFile = testDirectory.getChildFile (
        "MalformedLegacy.tbmbc");
    const auto malformedXml = malformedLegacyPreset.createXml();
    require (malformedXml != nullptr
                 && malformedXml->writeTo (malformedPresetFile, {}),
             "could not write malformed legacy preset fixture");
    const auto malformedResult = processor.loadUserPreset (malformedPresetFile, false);
    require (malformedResult.failed(),
             "partial legacy user preset was unexpectedly accepted");
    requireValuesEqual (captureValues (processor), legacyBaseline,
                        "partial legacy user preset atomic rejection");

    require (testDirectory.deleteRecursively(), "could not remove preset test directory");
    require (! testDirectory.exists(), "preset test directory was left behind");
    cleanup.dismiss();
}

void writeSnapshot (const juce::Image& image, const juce::File& destination)
{
    require (destination.getParentDirectory().createDirectory().wasOk(),
             "could not create UI QA output directory");
    juce::MemoryOutputStream encoded;
    juce::PNGImageFormat png;
    require (png.writeImageToStream (image, encoded), "could not encode editor PNG");
    require (destination.replaceWithData (encoded.getData(), encoded.getDataSize()),
             "could not write editor PNG");
    require (destination.existsAsFile() && destination.getSize() > 10000,
             "editor PNG is missing or implausibly small");
}

void destroyEditor (Processor& processor, juce::AudioProcessorEditor* editor)
{
    if (editor == nullptr)
        return;
    editor->setVisible (false);
    if (editor->isOnDesktop())
        editor->removeFromDesktop();
    processor.editorBeingDeleted (editor);
    delete editor;
}

void testEditorLifecycleAndSnapshot (const juce::String& snapshotPath)
{
    require (BinaryData::tb_knob_bakelite_pngSize == 271104,
             "embedded raster knob asset is stale or has the wrong byte size");
    const auto knobAsset = juce::ImageFileFormat::loadFrom (
        BinaryData::tb_knob_bakelite_png,
        BinaryData::tb_knob_bakelite_pngSize);
    require (knobAsset.isValid()
                 && knobAsset.getWidth() == 512
                 && knobAsset.getHeight() == 512,
             "embedded raster knob asset is missing or has the wrong dimensions");
    require (knobAsset.getPixelAt (0, 0).getAlpha() == 0
                 && knobAsset.getPixelAt (511, 0).getAlpha() == 0
                 && knobAsset.getPixelAt (0, 511).getAlpha() == 0
                 && knobAsset.getPixelAt (511, 511).getAlpha() == 0
                 && knobAsset.getPixelAt (256, 256).getAlpha() == 255,
             "embedded raster knob alpha matte is invalid");
    auto chromaFringePixels = 0;
    for (int y = 0; y < knobAsset.getHeight(); ++y)
        for (int x = 0; x < knobAsset.getWidth(); ++x)
        {
            const auto pixel = knobAsset.getPixelAt (x, y);
            if (pixel.getAlpha() > 16
                && ((pixel.getRed() > 180 && pixel.getBlue() > 180
                     && pixel.getGreen() < 100)
                    || (pixel.getGreen() > 170 && pixel.getBlue() > 170
                        && pixel.getRed() < 100)))
                ++chromaFringePixels;
        }
    require (chromaFringePixels == 0,
             "embedded raster knob retained a chroma-key fringe");

    Processor processor;
    setLayout (processor, 2, 0);
    processor.setRateAndBufferSizeDetails (testSampleRate, blockSize);
    processor.prepareToPlay (testSampleRate, blockSize);
    require (processor.applyFactoryPreset (3, false), "UI fixture preset failed");
    auto* editor = processor.createEditorIfNeeded();
    require (editor != nullptr, "custom editor could not be created");
    require (editor->getWidth() == editorWidth && editor->getHeight() == editorHeight,
             "editor default size is not 1000x620");
    auto* constrainer = editor->getConstrainer();
    require (constrainer != nullptr
                 && constrainer->getMinimumWidth() == editorMinimumWidth
                 && constrainer->getMinimumHeight() == editorMinimumHeight
                 && constrainer->getMaximumWidth() == editorMaximumWidth
                 && constrainer->getMaximumHeight() == editorMaximumHeight,
             "editor constrainer limits are not 0.85x-1.5x");

    editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    editor->setTopLeftPosition (32, 32);
    editor->setVisible (true);
    editor->toFront (false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    require (processor.getSpectrumAnalyzerDemandCount() == 1,
             "visible editor did not activate analyzer demand");

    auto* bandGraph = editor->findChildWithID ("bandGraph");
    require (bandGraph != nullptr, "analyzer graph component is missing");
    auto* bandPanel = editor->findChildWithID ("bandPanel");
    auto* mixControl = dynamic_cast<juce::Slider*> (
        editor->findChildWithID ("globalMix"));
    require (bandPanel != nullptr
                 && bandPanel->getBounds() == juce::Rectangle<int> (88, 400, 824, 120),
             "band controls are not fixed at the centred design bounds");
    require (bandPanel->findChildWithID ("bandDetails") == nullptr,
             "removed DETAILS control is still present");
    require (bandPanel->findChildWithID ("bandStereoLink") == nullptr,
             "removed Stereo Link control is still visible or hit-testable");
    const auto stereoLinkId = tbmbc::params::bandId (
        0, tbmbc::params::stereoLinkSuffix);
    const auto channelModeId = tbmbc::params::bandId (
        0, tbmbc::params::channelModeSuffix);
    require (processor.getValueTreeState().getParameter (stereoLinkId) != nullptr,
             "hidden Stereo Link host/state parameter was removed");
    const auto originalStereoLink = getPlainValue (processor, stereoLinkId);
    const auto originalChannelMode = getPlainValue (processor, channelModeId);
    setPlainValue (processor, stereoLinkId, 37.0f);
    setPlainValue (processor, channelModeId,
                   static_cast<float> (tbmbc::params::midChannelMode));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (std::abs (getPlainValue (processor, stereoLinkId) - 37.0f)
                 <= valueTolerance,
             "hidden Stereo Link value changed with Channel Mode");
    setPlainValue (processor, channelModeId, originalChannelMode);
    juce::MemoryBlock hiddenLinkState;
    processor.getStateInformation (hiddenLinkState);
    setPlainValue (processor, stereoLinkId, 83.0f);
    processor.setStateInformation (hiddenLinkState.getData(),
                                   static_cast<int> (hiddenLinkState.getSize()));
    require (std::abs (getPlainValue (processor, stereoLinkId) - 37.0f)
                 <= valueTolerance
                 && std::abs (getPlainValue (processor, channelModeId)
                              - originalChannelMode) <= valueTolerance,
             "hidden Stereo Link value did not survive state round-trip");
    setPlainValue (processor, stereoLinkId, originalStereoLink);
    require (mixControl != nullptr
                 && mixControl->getSliderStyle() == juce::Slider::LinearHorizontal,
             "Mix is not the requested compact horizontal slider");
    std::array<const juce::Slider*, 8> rotaryControls {};
    auto titledRotaryCount = 0;
    for (int child = 0; child < bandPanel->getNumChildComponents(); ++child)
        if (const auto* slider = dynamic_cast<const juce::Slider*> (
                bandPanel->getChildComponent (child));
            slider != nullptr
                && slider->getSliderStyle()
                       == juce::Slider::RotaryHorizontalVerticalDrag)
        {
            require (slider->getTitle().isNotEmpty(),
                     "raster rotary control lost its accessible title");
            if (titledRotaryCount < static_cast<int> (rotaryControls.size()))
                rotaryControls[(std::size_t) titledRotaryCount] = slider;
            ++titledRotaryCount;
        }
    require (titledRotaryCount == 8,
             "band panel does not expose exactly eight visible rotary controls");
    std::sort (rotaryControls.begin(), rotaryControls.end(),
               [] (const auto* left, const auto* right)
               {
                   return left->getX() < right->getX();
               });
    constexpr std::array<const char*, 8> expectedRotaryTitles {{
        "FREQUENCY", "WIDTH", "THRESHOLD", "RANGE", "RATIO", "KNEE",
        "ATTACK", "RELEASE"
    }};
    const auto firstRotaryBounds = rotaryControls.front()->getBounds();
    require (firstRotaryBounds.getWidth() == 84
                 && firstRotaryBounds.getHeight() == 76,
             "band rotary controls do not use the final common size");
    for (std::size_t index = 0; index < rotaryControls.size(); ++index)
    {
        const auto bounds = rotaryControls[index]->getBounds();
        require (rotaryControls[index]->getTitle() == expectedRotaryTitles[index],
                 "band rotary title/order contract changed at index "
                     + std::to_string (index));
        require (bounds == juce::Rectangle<int> (
                              20 + static_cast<int> (index) * 100,
                              40, 84, 76),
                 "band rotary control left the uniform design grid");
        require (bounds.getWidth() == firstRotaryBounds.getWidth()
                     && bounds.getHeight() == firstRotaryBounds.getHeight(),
                 "band rotary controls do not share one width and height");
        if (index > 0)
        {
            const auto previous = rotaryControls[index - 1]->getBounds();
            require (bounds.getCentreX() - previous.getCentreX() == 100,
                     "band rotary centre pitch is not 100 px");
            require (bounds.getX() - previous.getRight() == 16,
                     "band rotary bounds gap is not 16 px");
        }
    }
    const auto leftRotaryMargin = rotaryControls.front()->getX();
    const auto rightRotaryMargin = bandPanel->getWidth()
        - rotaryControls.back()->getRight();
    require (leftRotaryMargin == 20 && rightRotaryMargin == 20
                 && leftRotaryMargin == rightRotaryMargin,
             "band rotary row does not have symmetric 20 px side margins");
    require (processor.getValueTreeState().getParameter (
                 tbmbc::params::bandId (0, tbmbc::params::outputSuffix)) != nullptr,
             "hidden Band Output host/state parameter was removed");
    for (const auto* slider : rotaryControls)
        require (! slider->getTitle().containsIgnoreCase ("BAND OUT")
                     && ! slider->getTitle().containsIgnoreCase ("OUTPUT"),
                 "Band Output remains visible in the compact band panel");
    auto* sidechainButton = dynamic_cast<juce::Button*> (
        bandPanel->findChildWithID ("bandSidechainSource"));
    require (sidechainButton != nullptr
                 && sidechainButton->getButtonText() == "SIDECHAIN",
             "SIDECHAIN button is missing or has state-dependent text");
    require (bandPanel->findChildWithID ("bandDetectorMode") == nullptr,
             "removed detector mode UI component is still present");
    auto* deltaButton = dynamic_cast<juce::Button*> (
        bandPanel->findChildWithID ("bandDeltaListen"));
    require (deltaButton != nullptr
                 && bandPanel->findChildWithID ("bandListen") == nullptr,
             "transient Delta control does not use its canonical component ID");
    const auto sidechainWasEnabled = sidechainButton->getToggleState();
    sidechainButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (sidechainButton->getToggleState() != sidechainWasEnabled
                 && sidechainButton->getButtonText() == "SIDECHAIN",
             "SIDECHAIN text changed with its toggle state");
    sidechainButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (sidechainButton->getToggleState() == sidechainWasEnabled
                 && sidechainButton->getButtonText() == "SIDECHAIN",
             "SIDECHAIN did not restore its state with stable text");
    auto* analyzerToggle = dynamic_cast<juce::Button*> (
        bandGraph->findChildWithID ("analyzerToggle"));
    auto* analyzerInputSource = dynamic_cast<juce::Label*> (
        bandGraph->findChildWithID ("analyzerInputSource"));
    require (analyzerToggle != nullptr, "analyzer display toggle is missing");
    require (analyzerInputSource != nullptr
                 && analyzerInputSource->getText() == "INPUT"
                 && analyzerInputSource->isVisible(),
             "analyzer INPUT source label is missing, renamed, or hidden");
    for (int child = 0; child < bandGraph->getNumChildComponents(); ++child)
        if (const auto* label = dynamic_cast<const juce::Label*> (
                bandGraph->getChildComponent (child)))
            require (! label->getText().containsIgnoreCase ("dBFS")
                         && ! label->getText().startsWithIgnoreCase ("BAND ")
                         && ! label->getText().startsWithIgnoreCase ("ACTUAL "),
                     "removed analyzer debug numeric readout remains visible");
    require (bandGraph->findChildWithID ("analyzerPre") == nullptr
                 && bandGraph->findChildWithID ("analyzerPost") == nullptr,
             "removed analyzer source selectors are still present");
    analyzerToggle->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getSpectrumAnalyzerDemandCount() == 0,
             "Analyzer Off did not release producer demand");
    require (! analyzerInputSource->isVisible(),
             "Analyzer Off did not hide the INPUT source label");
    const auto samplesWhileOff = processor.getInputSpectrumAnalyzer()
                                     .getPushedSampleCount();
    {
        juce::MidiBuffer offMidi;
        for (int block = 0; block < 10; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            fillProcessBuffer (buffer, 2, 0, block * blockSize);
            processor.processBlock (buffer, offMidi);
        }
    }
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount()
                 == samplesWhileOff,
             "Analyzer Off continued the audio-thread sample tap");
    analyzerToggle->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getSpectrumAnalyzerDemandCount() == 1,
             "Analyzer On did not restore producer demand");
    require (analyzerInputSource->isVisible()
                 && analyzerInputSource->getText() == "INPUT",
             "Analyzer On did not restore the exact INPUT source label");

    std::int64_t analyzerSample = 0;
    std::uint32_t analyzerNoiseState = 0x42d00d5u;
    juce::MidiBuffer midi;
    const auto renderAnalyzer = [&]
    {
        for (int block = 0; block < 14; ++block)
        {
            auto buffer = makeProcessBuffer (processor, blockSize);
            buffer.clear();
            for (int sample = 0; sample < blockSize; ++sample)
            {
                analyzerNoiseState ^= analyzerNoiseState << 13;
                analyzerNoiseState ^= analyzerNoiseState >> 17;
                analyzerNoiseState ^= analyzerNoiseState << 5;
                const auto noise = ((float) (analyzerNoiseState & 0x00ffffffu)
                                    / 8388607.5f) - 1.0f;
                const auto absoluteSample = (double) (analyzerSample + sample);
                const auto tone = 0.30f * (float) std::sin (
                    juce::MathConstants<double>::twoPi * 120.0
                    * absoluteSample / testSampleRate)
                    + 0.22f * (float) std::sin (
                    juce::MathConstants<double>::twoPi * 997.0
                    * absoluteSample / testSampleRate)
                    + 0.14f * (float) std::sin (
                        juce::MathConstants<double>::twoPi * 3200.0
                        * absoluteSample / testSampleRate)
                    + 0.10f * (float) std::sin (
                        juce::MathConstants<double>::twoPi * 9000.0
                        * absoluteSample / testSampleRate);
                for (int channel = 0; channel < 2; ++channel)
                    buffer.setSample (channel, sample,
                                      tone + noise * (channel == 0 ? 0.07f : 0.06f));
            }
            analyzerSample += blockSize;
            processor.processBlock (buffer, midi);
        }
        // The editor intentionally idles at 5 Hz while transport is stopped.
        // Wait through one idle tick so a fresh audio burst is visible in QA captures.
        juce::MessageManager::getInstance()->runDispatchLoopUntil (260);
    };
    renderAnalyzer();
    require (processor.getInputSpectrumAnalyzer().getFrameGeneration() > 0,
             "visible editor did not pull the input analyzer frame");
    require (processor.getGainReductionDb (0) < -0.05f,
             "default negative-Range UI fixture did not report downward gain");

    const auto frequencyId = tbmbc::params::bandId (
        0, tbmbc::params::frequencySuffix);
    const auto rangeId = tbmbc::params::bandId (0, tbmbc::params::rangeSuffix);
    const auto baselineFrequency = getPlainValue (processor, frequencyId);
    const auto baselineRange = getPlainValue (processor, rangeId);
    const auto graphScaleX = (float) bandGraph->getWidth() / 976.0f;
    const auto graphScaleY = (float) bandGraph->getHeight() / 499.0f;
    constexpr float graphPlotTopY = 38.0f;
    constexpr float graphPlotHeight = 300.0f;
    constexpr float graphRangeHalfHeight = graphPlotHeight * 0.5f;
    constexpr float graphRangeZeroY = graphPlotTopY + graphRangeHalfHeight;
    const auto rangeToDesignY = [=] (float rangeDb)
    {
        const auto clamped = juce::jlimit (-48.0f, 12.0f, rangeDb);
        return clamped >= 0.0f
            ? graphRangeZeroY - clamped / 12.0f * graphRangeHalfHeight
            : graphRangeZeroY - clamped / 48.0f * graphRangeHalfHeight;
    };
    require (std::abs (rangeToDesignY (12.0f) - graphPlotTopY) <= 1.0e-6f
                 && std::abs (rangeToDesignY (0.0f) - graphRangeZeroY)
                        <= 1.0e-6f
                 && std::abs (rangeToDesignY (-48.0f)
                              - (graphPlotTopY + graphPlotHeight)) <= 1.0e-6f,
             "asymmetric Range graph anchors are not +12/0/-48 at 38/188/338");
    const auto node = juce::Point<float> (
        (42.0f + std::log (baselineFrequency / 20.0f) / std::log (1000.0f)
                       * 892.0f) * graphScaleX,
        rangeToDesignY (baselineRange) * graphScaleY);
    const auto eventTime = juce::Time::getCurrentTime();
    const auto left = juce::ModifierKeys (
        juce::ModifierKeys::leftButtonModifier);
    const auto source = juce::Desktop::getInstance().getMainMouseSource();
    bandGraph->mouseDown (juce::MouseEvent (
        source, node, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, false));
    const auto draggedNode = node + juce::Point<float> (52.0f, -24.0f);
    bandGraph->mouseDrag (juce::MouseEvent (
        source, draggedNode, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, true));
    bandGraph->mouseUp (juce::MouseEvent (
        source, draggedNode, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, true));
    require (std::abs (getPlainValue (processor, frequencyId) - baselineFrequency)
                 > 1.0f
                 && std::abs (getPlainValue (processor, rangeId) - baselineRange)
                        > 0.1f,
             "analyzer node drag did not edit Frequency and Range");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (140);
    processor.undo();
    const auto undoneFrequency = getPlainValue (processor, frequencyId);
    const auto undoneRange = getPlainValue (processor, rangeId);
    require (std::abs (undoneFrequency - baselineFrequency)
                 <= valueTolerance
                 && std::abs (undoneRange - baselineRange) <= valueTolerance,
             "analyzer node drag was not one undoable interaction: frequency "
                 + std::to_string (undoneFrequency) + " vs "
                 + std::to_string (baselineFrequency) + ", range "
                 + std::to_string (undoneRange) + " vs "
                 + std::to_string (baselineRange));

    const auto negativeRangeTarget = juce::Point<float> (
        node.x, (graphRangeZeroY + graphRangeHalfHeight) * graphScaleY);
    bandGraph->mouseDown (juce::MouseEvent (
        source, node, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, false));
    bandGraph->mouseDrag (juce::MouseEvent (
        source, negativeRangeTarget, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, true));
    bandGraph->mouseUp (juce::MouseEvent (
        source, negativeRangeTarget, juce::ModifierKeys(), 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, bandGraph, bandGraph, eventTime, node,
        eventTime, 1, true));
    require (getPlainValue (processor, rangeId) <= -47.5f
                 && std::abs (getPlainValue (processor, frequencyId)
                              - baselineFrequency) <= 1.0f,
             "dragging below 0 dB did not reach the full negative Range axis");
    require (processor.canUndo(),
             "full negative Range drag created no undoable gesture");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (140);
    processor.undo();
    require (std::abs (getPlainValue (processor, rangeId) - baselineRange)
                     <= valueTolerance
                 && std::abs (getPlainValue (processor, frequencyId)
                              - baselineFrequency) <= valueTolerance,
             "full negative Range drag was not restored by one Undo gesture");

    const auto positiveRangeTarget = juce::Point<float> (
        node.x, (graphRangeZeroY - graphRangeHalfHeight) * graphScaleY);
    bandGraph->mouseDown (juce::MouseEvent (
        source, node, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, false));
    bandGraph->mouseDrag (juce::MouseEvent (
        source, positiveRangeTarget, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        bandGraph, bandGraph, eventTime, node, eventTime, 1, true));
    bandGraph->mouseUp (juce::MouseEvent (
        source, positiveRangeTarget, juce::ModifierKeys(), 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, bandGraph, bandGraph, eventTime, node,
        eventTime, 1, true));
    require (getPlainValue (processor, rangeId) >= 11.9f
                 && std::abs (getPlainValue (processor, frequencyId)
                              - baselineFrequency) <= 1.0f,
             "dragging above 0 dB did not reach the full positive Range axis");
    require (processor.canUndo(),
             "full positive Range drag created no undoable gesture");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (140);
    processor.undo();
    require (std::abs (getPlainValue (processor, rangeId) - baselineRange)
                     <= valueTolerance
                 && std::abs (getPlainValue (processor, frequencyId)
                              - baselineFrequency) <= valueTolerance,
             "full positive Range drag was not restored by one Undo gesture");

    require (bandPanel->getBounds() == juce::Rectangle<int> (88, 400, 824, 120),
             "band controls moved after a graph edit");

    const auto nodeForBand = [&] (int band)
    {
        const auto frequency = getPlainValue (processor, tbmbc::params::bandId (
            band, tbmbc::params::frequencySuffix));
        const auto range = getPlainValue (processor, tbmbc::params::bandId (
            band, tbmbc::params::rangeSuffix));
        return juce::Point<float> (
            (42.0f + std::log (frequency / 20.0f) / std::log (1000.0f)
                           * 892.0f) * graphScaleX,
            rangeToDesignY (range) * graphScaleY);
    };
    const auto rightMouseDownAt = [&] (juce::Point<float> position)
    {
        const auto right = juce::ModifierKeys (
            juce::ModifierKeys::rightButtonModifier);
        bandGraph->mouseDown (juce::MouseEvent (
            source, position, right, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            bandGraph, bandGraph, eventTime, position, eventTime, 1, false));
    };
    const auto rightMouseUpAt = [&] (juce::Point<float> position)
    {
        bandGraph->mouseUp (juce::MouseEvent (
            source, position, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, bandGraph, bandGraph, eventTime, position,
            eventTime, 1, true));
    };
    const auto rightMouseDragAt = [&] (juce::Point<float> position,
                                       juce::Point<float> downPosition)
    {
        const auto right = juce::ModifierKeys (
            juce::ModifierKeys::rightButtonModifier);
        bandGraph->mouseDrag (juce::MouseEvent (
            source, position, right, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            bandGraph, bandGraph, eventTime, downPosition, eventTime, 1, true));
    };

    const auto requireRangeNodeHit = [&] (float rangeDb, float expectedDesignY,
                                          const char* description)
    {
        setPlainValue (processor, rangeId, rangeDb);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        const auto rangeNode = nodeForBand (0);
        require (std::abs (rangeNode.y - expectedDesignY * graphScaleY) <= 0.5f,
                 std::string (description) + " node left its graph anchor");
        rightMouseDownAt (rangeNode);
        require (processor.getDeltaListenBand() == 0,
                 std::string (description)
                     + " node did not win analyzer hit-testing");
        rightMouseUpAt (rangeNode);
        require (processor.getDeltaListenBand() == -1,
                 std::string (description)
                     + " node hit did not release momentary Delta Listen");
    };
    requireRangeNodeHit (12.0f, graphPlotTopY, "+12 dB");
    requireRangeNodeHit (0.0f, graphRangeZeroY, "0 dB");
    requireRangeNodeHit (-48.0f, graphPlotTopY + graphPlotHeight, "-48 dB");
    setPlainValue (processor, rangeId, baselineRange);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);

    std::array<float, tbmbc::params::numBands> auditionBeforeDelta {};
    for (int band = 0; band < tbmbc::params::numBands; ++band)
        auditionBeforeDelta[(std::size_t) band] = getPlainValue (
            processor, tbmbc::params::bandId (band, tbmbc::params::auditionSuffix));

    const auto band0Node = nodeForBand (0);
    const auto band1Node = nodeForBand (1);
    require (processor.getDeltaListenBand() == -1,
             "Delta Listen was not transiently off at editor start");
    rightMouseDownAt (band0Node);
    require (processor.getDeltaListenBand() == 0
                 && deltaButton->getToggleState()
                 && deltaButton->getButtonText() == "DELTA ON",
             "right-button down on an enabled node did not enable Delta Listen");
    rightMouseUpAt (band0Node);
    require (processor.getDeltaListenBand() == -1
                 && ! deltaButton->getToggleState()
                 && deltaButton->getButtonText() == "DELTA",
             "right-button up did not release momentary Delta Listen");
    const auto rightDragFrequencyBefore = getPlainValue (processor, frequencyId);
    const auto rightDragRangeBefore = getPlainValue (processor, rangeId);
    const auto band1FrequencyId = tbmbc::params::bandId (
        1, tbmbc::params::frequencySuffix);
    const auto band1FrequencyBefore = getPlainValue (processor, band1FrequencyId);
    const auto rightDragTarget = band0Node + juce::Point<float> (
        64.0f * graphScaleX, 0.0f);
    rightMouseDownAt (band0Node);
    rightMouseDragAt (rightDragTarget, band0Node);
    require (processor.getDeltaListenBand() == 0
                 && deltaButton->getToggleState()
                 && deltaButton->getButtonText() == "DELTA ON",
             "right-drag transferred Delta away from the held band");
    require (std::abs (getPlainValue (processor, frequencyId)
                      - rightDragFrequencyBefore) > 1.0f,
             "right-drag did not move the held band's frequency");
    require (std::abs (getPlainValue (processor, rangeId)
                      - rightDragRangeBefore) <= valueTolerance,
             "right-drag changed Range instead of moving frequency only");
    require (std::abs (getPlainValue (processor, band1FrequencyId)
                      - band1FrequencyBefore) <= valueTolerance,
             "right-drag changed a different band's frequency");
    rightMouseUpAt (rightDragTarget);
    require (processor.getDeltaListenBand() == -1
                 && ! deltaButton->getToggleState()
                 && deltaButton->getButtonText() == "DELTA",
             "momentary Delta remained on after right-button release");
    require (processor.canUndo(),
             "right-drag frequency edit created no undoable gesture");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (140);
    processor.undo();
    require (std::abs (getPlainValue (processor, frequencyId)
                      - rightDragFrequencyBefore) <= valueTolerance
                 && std::abs (getPlainValue (processor, rangeId)
                              - rightDragRangeBefore) <= valueTolerance
                 && std::abs (getPlainValue (processor, band1FrequencyId)
                              - band1FrequencyBefore) <= valueTolerance,
             "one Undo did not restore the right-drag frequency-only edit");
    const auto emptyGraphPoint = juce::Point<float> (
        50.0f * graphScaleX, 55.0f * graphScaleY);
    rightMouseDownAt (emptyGraphPoint);
    require (processor.getDeltaListenBand() == -1,
             "right-button down on empty analyzer space enabled Delta Listen");
    rightMouseUpAt (emptyGraphPoint);
    rightMouseDownAt (band0Node);
    require (processor.getDeltaListenBand() == 0,
             "Delta capture-loss fixture could not be enabled");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (220);
    require (processor.getDeltaListenBand() == -1,
             "abandoned right-button gesture retained momentary Delta Listen");

    deltaButton->triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getDeltaListenBand() == 0,
             "panel DELTA did not latch Band 1 before momentary override");
    rightMouseDownAt (band1Node);
    require (processor.getDeltaListenBand() == 1,
             "momentary Band 2 hold did not override latched Band 1 Delta");
    rightMouseUpAt (band1Node);
    require (processor.getDeltaListenBand() == 0,
             "releasing momentary Band 2 did not restore latched Band 1 Delta");

    rightMouseDownAt (band1Node);
    require (processor.getDeltaListenBand() == 1,
             "external-reset guard fixture could not start momentary Band 2");
    processor.setDeltaListenBand (-1);
    rightMouseUpAt (band1Node);
    require (processor.getDeltaListenBand() == -1,
             "momentary release resurrected a latch after external reset");
    for (int band = 0; band < tbmbc::params::numBands; ++band)
        require (std::abs (getPlainValue (processor, tbmbc::params::bandId (
                              band, tbmbc::params::auditionSuffix))
                           - auditionBeforeDelta[(std::size_t) band])
                     <= valueTolerance,
                 "Delta right-click changed a legacy Audition parameter");

    require (processor.applyFactoryPreset (3, false),
             "Delta preset-clear fixture failed");
    require (processor.getDeltaListenBand() == -1,
             "Factory preset load retained transient Delta Listen");
    rightMouseDownAt (band0Node);
    require (processor.getDeltaListenBand() == 0,
             "Delta state-load fixture could not be enabled");
    juce::MemoryBlock deltaState;
    processor.getStateInformation (deltaState);
    processor.setStateInformation (deltaState.getData(),
                                   static_cast<int> (deltaState.getSize()));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getDeltaListenBand() == -1,
             "state load retained transient Delta Listen");

    const auto constrainTo = [&] (int width, int height)
    {
        constrainer->setBoundsForComponent (
            editor, { editor->getX(), editor->getY(), width, height },
            false, false, true, true);
        editor->resized();
        editor->repaint();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    };

    constrainTo (800, 496);
    require (editor->getWidth() == editorMinimumWidth
                 && editor->getHeight() == editorMinimumHeight,
             "editor constrainer did not enforce the minimum size");
    constrainTo (1600, 992);
    require (editor->getWidth() == editorMaximumWidth
                 && editor->getHeight() == editorMaximumHeight,
             "editor constrainer did not enforce the maximum size");

    const auto captureAt = [&] (int width, int height,
                                 const juce::File& destination,
                                 bool verifySingleGainContour)
    {
        constrainTo (width, height);
        require (editor->getWidth() == width && editor->getHeight() == height,
                 "editor did not reach a requested QA size");
        const auto expectedPanel = juce::Rectangle<int> (
            juce::roundToInt (88.0f * width / editorWidth),
            juce::roundToInt (400.0f * height / editorHeight),
            juce::roundToInt (824.0f * width / editorWidth),
            juce::roundToInt (120.0f * height / editorHeight));
        require (bandPanel->getBounds() == expectedPanel,
                 "fixed band panel did not scale from its centred design bounds");
        renderAnalyzer();
        const auto image = editor->createComponentSnapshot (
            editor->getLocalBounds(), true, 1.0f);
        require (image.isValid() && image.getWidth() == width
                     && image.getHeight() == height,
                 "actual editor snapshot has the wrong dimensions");
        const auto reference = image.getPixelAt (0, 0);
        auto differingSamples = 0;
        for (int y = 0; y < image.getHeight(); y += 20)
            for (int x = 0; x < image.getWidth(); x += 20)
                differingSamples += image.getPixelAt (x, y) != reference ? 1 : 0;
        require (differingSamples > 40,
                 "editor snapshot appears blank or uniform");
        auto legacyOrangePixels = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto pixel = image.getPixelAt (x, y);
                if (pixel.getRed() >= 220
                    && pixel.getGreen() >= 55 && pixel.getGreen() <= 145
                    && pixel.getBlue() <= 105
                    && pixel.getRed() - pixel.getGreen() >= 70)
                    ++legacyOrangePixels;
            }
        require (legacyOrangePixels
                     <= juce::jmax (4, image.getWidth() * image.getHeight() / 10000),
                 "legacy high-saturation orange remains in the editor theme");

        if (width == editorWidth && height == editorHeight)
        {
            juce::Image expectedTitle (juce::Image::RGB, 250, 20, true);
            {
                juce::Graphics titleGraphics (expectedTitle);
                const auto headerTop = juce::Colour (0xff2b2c34);
                const auto headerBottom = juce::Colour (0xff18191f);
                for (int y = 0; y < expectedTitle.getHeight(); ++y)
                {
                    titleGraphics.setColour (headerTop.interpolatedWith (
                        headerBottom, (float) (y + 11) / 42.0f));
                    titleGraphics.fillRect (0, y, expectedTitle.getWidth(), 1);
                }
                const auto titleFont = juce::Font (juce::FontOptions (
                    14.0f, juce::Font::bold));
                juce::AttributedString title;
                title.setJustification (juce::Justification::centredLeft);
                title.setWordWrap (juce::AttributedString::none);
                title.append ("M", titleFont, juce::Colour (0xff7088b2));
                title.append ("ulti ", titleFont, juce::Colour (0xffd9dae1));
                title.append ("B", titleFont, juce::Colour (0xff4f805f));
                title.append ("and ", titleFont, juce::Colour (0xffd9dae1));
                title.append ("C", titleFont, juce::Colour (0xffc65350));
                title.append ("ompressor", titleFont, juce::Colour (0xffd9dae1));
                title.draw (titleGraphics, expectedTitle.getBounds().toFloat());
            }

            auto matchingTitlePixels = 0;
            for (int y = 0; y < expectedTitle.getHeight(); ++y)
                for (int x = 0; x < expectedTitle.getWidth(); ++x)
                {
                    const auto expected = expectedTitle.getPixelAt (x, y);
                    const auto actual = image.getPixelAt (20 + x, 11 + y);
                    if (std::abs ((int) actual.getRed() - (int) expected.getRed()) <= 4
                        && std::abs ((int) actual.getGreen()
                                    - (int) expected.getGreen()) <= 4
                        && std::abs ((int) actual.getBlue()
                                    - (int) expected.getBlue()) <= 4)
                        ++matchingTitlePixels;
                }
            constexpr auto titleCropPixels = 250 * 20;
            const auto expectedProbe = expectedTitle.getPixelAt (0, 0);
            const auto actualProbe = image.getPixelAt (20, 11);
            require (matchingTitlePixels >= titleCropPixels * 9 / 10,
                     "visible title is not exact coloured 'Multi Band Compressor' ("
                         + std::to_string (matchingTitlePixels) + "/"
                         + std::to_string (titleCropPixels) + " pixels matched; probe "
                         + expectedProbe.toDisplayString (false).toStdString() + "/"
                         + actualProbe.toDisplayString (false).toStdString() + ")");
        }

        if (width == editorWidth && height == editorHeight
            && verifySingleGainContour)
        {
            constexpr int plotLeft = 54;
            constexpr int plotRight = 946;
            constexpr int plotTop = 92;
            constexpr int plotBottom = 392;
            constexpr int zeroY = 242;
            std::array<float, tbmbc::params::numBands> centreXs {};
            std::array<float, tbmbc::params::numBands> edgeLeftXs {};
            std::array<float, tbmbc::params::numBands> edgeRightXs {};
            std::array<float, tbmbc::params::numBands> nodeYs {};
            for (int band = 0; band < tbmbc::params::numBands; ++band)
            {
                const auto frequency = getPlainValue (processor,
                    tbmbc::params::bandId (
                        band, tbmbc::params::frequencySuffix));
                const auto widthOctaves = getPlainValue (processor,
                    tbmbc::params::bandId (
                        band, tbmbc::params::bandwidthSuffix));
                const auto ratio = std::pow (2.0f, widthOctaves * 0.5f);
                const auto toEditorX = [] (float frequencyHz)
                {
                    return 54.0f
                        + std::log (juce::jlimit (20.0f, 20000.0f,
                                                 frequencyHz) / 20.0f)
                              / std::log (1000.0f) * 892.0f;
                };
                centreXs[(std::size_t) band] = toEditorX (frequency);
                edgeLeftXs[(std::size_t) band] = toEditorX (frequency / ratio);
                edgeRightXs[(std::size_t) band] = toEditorX (frequency * ratio);
                nodeYs[(std::size_t) band] = 54.0f + rangeToDesignY (
                    getPlainValue (processor, tbmbc::params::bandId (
                        band, tbmbc::params::rangeSuffix)));
            }
            const auto isStructuralBandPixel = [&] (int x, int y)
            {
                for (int band = 0; band < tbmbc::params::numBands; ++band)
                {
                    if (std::abs ((float) x - edgeLeftXs[(std::size_t) band]) <= 4.0f
                        || std::abs ((float) x
                                    - edgeRightXs[(std::size_t) band]) <= 4.0f)
                        return true;
                    const auto dx = (float) x - centreXs[(std::size_t) band];
                    const auto dy = (float) y - nodeYs[(std::size_t) band];
                    if (dx * dx + dy * dy <= 17.0f * 17.0f)
                        return true;
                }
                return false;
            };
            const auto isLiveContourPixel = [] (juce::Colour pixel)
            {
                return std::abs ((int) pixel.getRed() - 129) <= 24
                    && std::abs ((int) pixel.getGreen() - 124) <= 24
                    && std::abs ((int) pixel.getBlue() - 102) <= 24
                    && std::abs ((int) pixel.getRed()
                                - (int) pixel.getGreen()) <= 22
                    && pixel.getGreen() >= pixel.getBlue();
            };

            std::array<bool, tbmbc::params::numBands> visiblyMovingBands {};
            std::array<int, tbmbc::params::numBands> offDatumShoulderCounts {};
            auto visiblyMovingBandCount = 0;
            for (int band = 0; band < tbmbc::params::numBands; ++band)
            {
                const auto enabled = getPlainValue (processor,
                    tbmbc::params::bandId (
                        band, tbmbc::params::enabledSuffix)) >= 0.5f;
                // A node that sits on the datum would cover the exact pixels this
                // regression check needs, so only judge unobstructed band centres.
                if (! enabled
                    || std::abs (nodeYs[(std::size_t) band] - zeroY) <= 9.0f)
                    continue;

                const auto centreX = juce::roundToInt (
                    centreXs[(std::size_t) band]);
                auto offDatumShoulderPixels = 0;
                for (int x = juce::jmax (plotLeft, centreX - 28);
                     x <= juce::jmin (plotRight - 1, centreX + 28); ++x)
                    for (int y = plotTop; y < plotBottom; ++y)
                        if (std::abs (y - zeroY) >= 6
                            && isLiveContourPixel (image.getPixelAt (x, y)))
                            ++offDatumShoulderPixels;
                offDatumShoulderCounts[(std::size_t) band]
                    = offDatumShoulderPixels;

                if (offDatumShoulderPixels < 8)
                    continue;
                visiblyMovingBands[(std::size_t) band] = true;
                ++visiblyMovingBandCount;

                auto datumPixelsAtMovingCentre = 0;
                for (int x = juce::jmax (plotLeft, centreX - 8);
                     x <= juce::jmin (plotRight - 1, centreX + 8); ++x)
                    for (int y = zeroY - 3; y <= zeroY + 3; ++y)
                        if (isLiveContourPixel (image.getPixelAt (x, y)))
                            ++datumPixelsAtMovingCentre;
                require (datumPixelsAtMovingCentre <= 2,
                         "same-colour fixed 0 dB datum remains behind moving Band "
                             + std::to_string (band + 1) + " ("
                             + std::to_string (datumPixelsAtMovingCentre)
                             + " near-datum pixels)");
            }
            require (visiblyMovingBandCount >= 1,
                     "gain-contour fixture has no unobstructed visibly moving band ("
                         + std::to_string (offDatumShoulderCounts[0]) + ","
                         + std::to_string (offDatumShoulderCounts[1]) + ","
                         + std::to_string (offDatumShoulderCounts[2]) + ","
                         + std::to_string (offDatumShoulderCounts[3]) + ")");

            const auto isInsideMovingBand = [&] (int x)
            {
                for (int band = 0; band < tbmbc::params::numBands; ++band)
                    if (visiblyMovingBands[(std::size_t) band]
                        && x >= juce::roundToInt (
                            edgeLeftXs[(std::size_t) band]) - 2
                        && x <= juce::roundToInt (
                            edgeRightXs[(std::size_t) band]) + 2)
                        return true;
                return false;
            };

            auto liveContourPixels = 0;
            auto liveColumns = 0;
            auto baselineColumnsOutsideMovingBands = 0;
            auto offDatumAbovePixels = 0;
            auto offDatumBelowPixels = 0;
            auto overlyThickColumns = 0;
            auto legacyConfiguredPixels = 0;
            auto legacyRedPixels = 0;
            double graphLuminance = 0.0;
            auto graphSamples = 0;
            double frameLuminance = 0.0;
            auto frameSamples = 0;
            for (int x = plotLeft; x < plotRight; ++x)
            {
                auto liveInColumn = 0;
                auto liveAtBaseline = false;
                for (int y = plotTop; y < plotBottom; ++y)
                {
                    const auto pixel = image.getPixelAt (x, y);
                    const auto configured = pixel.getRed() >= 145
                        && pixel.getRed() <= 225
                        && pixel.getGreen() >= 135
                        && pixel.getGreen() <= 215
                        && pixel.getBlue() >= 90
                        && pixel.getBlue() <= 180
                        && std::abs ((int) pixel.getRed()
                                    - (int) pixel.getGreen()) <= 35
                        && pixel.getGreen() > pixel.getBlue();
                    const auto legacyRed = pixel.getRed() >= 130
                        && pixel.getGreen() >= 30 && pixel.getGreen() <= 125
                        && pixel.getBlue() >= 40 && pixel.getBlue() <= 125
                        && pixel.getRed() - pixel.getGreen() >= 40;
                    if (isLiveContourPixel (pixel))
                    {
                        ++liveContourPixels;
                        ++liveInColumn;
                        liveAtBaseline = liveAtBaseline
                            || std::abs (y - zeroY) <= 2;
                        if (! isStructuralBandPixel (x, y)
                            && y < zeroY - 4)
                            ++offDatumAbovePixels;
                        if (! isStructuralBandPixel (x, y)
                            && y > zeroY + 4)
                            ++offDatumBelowPixels;
                    }
                    if (! isStructuralBandPixel (x, y)
                        && std::abs (y - zeroY) >= 5)
                    {
                        if (configured)
                            ++legacyConfiguredPixels;
                        if (legacyRed)
                            ++legacyRedPixels;
                    }
                    if ((x - 54) % 17 == 8 && (y - 92) % 17 == 8)
                    {
                        graphLuminance += 0.2126 * pixel.getRed()
                            + 0.7152 * pixel.getGreen()
                            + 0.0722 * pixel.getBlue();
                        ++graphSamples;
                    }
                }
                if (liveInColumn > 0)
                    ++liveColumns;
                if (liveAtBaseline && ! isInsideMovingBand (x))
                    ++baselineColumnsOutsideMovingBands;
                if (liveInColumn > 7)
                    ++overlyThickColumns;
            }
            for (int y = 62; y <= 82; y += 5)
                for (int x = 150; x <= 850; x += 17)
                {
                    const auto pixel = image.getPixelAt (x, y);
                    frameLuminance += 0.2126 * pixel.getRed()
                        + 0.7152 * pixel.getGreen()
                        + 0.0722 * pixel.getBlue();
                    ++frameSamples;
                }
            require (liveContourPixels >= 1400
                         && liveColumns >= 820
                         && baselineColumnsOutsideMovingBands >= 140,
                     "single continuous pale-yellow live-gain contour is absent");
            require (overlyThickColumns <= 16,
                     "live-gain contour has an opaque fill or extra thick layer");
            require (legacyConfiguredPixels <= 24,
                     "separate configured-Range guide/fill remains visible");
            require (legacyRedPixels <= 24,
                     "separate red actual-gain overlay remains visible");
            const auto currentGain = processor.getGainReductionDb (0);
            require (currentGain < -0.05f
                         ? offDatumBelowPixels >= 80
                         : offDatumAbovePixels >= 80,
                     "single live-gain contour moved in the wrong direction or stayed at 0 dB");
            require (graphSamples > 0 && frameSamples > 0
                         && graphLuminance / graphSamples
                                >= frameLuminance / frameSamples + 8.0,
                     "analyzer graph background is not visibly lighter than the frame");
        }
        if (destination != juce::File())
            writeSnapshot (image, destination);
    };

    juce::File defaultSnapshot;
    juce::File minimumSnapshot;
    juce::File maximumSnapshot;
    juce::File internalSnapshot;
    juce::File sidechainSnapshot;
    juce::File deltaSnapshot;
    juce::File bypassSnapshot;
    juce::File positive8xMidSnapshot;
    juce::File masterPresetSnapshot;
    juce::File fourBandsSnapshot;
    if (snapshotPath.isNotEmpty())
    {
        defaultSnapshot = juce::File (snapshotPath);
        auto stem = defaultSnapshot.getFileNameWithoutExtension();
        if (stem.endsWith ("_100pct"))
            stem = stem.dropLastCharacters (7);
        minimumSnapshot = defaultSnapshot.getSiblingFile (stem + "_85pct.png");
        maximumSnapshot = defaultSnapshot.getSiblingFile (stem + "_150pct.png");
        internalSnapshot = defaultSnapshot.getSiblingFile (stem + "_internal_band.png");
        sidechainSnapshot = defaultSnapshot.getSiblingFile (stem + "_sidechain_on.png");
        deltaSnapshot = defaultSnapshot.getSiblingFile (stem + "_delta.png");
        bypassSnapshot = defaultSnapshot.getSiblingFile (stem + "_bypass.png");
        positive8xMidSnapshot = defaultSnapshot.getSiblingFile (
            stem + "_positive_8x_mid.png");
        masterPresetSnapshot = defaultSnapshot.getSiblingFile (
            stem + "_master_preset.png");
        fourBandsSnapshot = defaultSnapshot.getSiblingFile (
            stem + "_four_bands.png");
    }

    captureAt (editorMinimumWidth, editorMinimumHeight, minimumSnapshot, false);
    captureAt (editorMaximumWidth, editorMaximumHeight, maximumSnapshot, false);
    captureAt (editorWidth, editorHeight, defaultSnapshot, true);
    if (snapshotPath.isNotEmpty())
    {
        require (processor.applyFactoryPreset (1, false),
                 "internal/BAND UI fixture preset failed");
        captureAt (editorWidth, editorHeight, internalSnapshot, false);
        require (processor.applyFactoryPreset (10, false),
                 "SIDECHAIN On UI fixture preset failed");
        captureAt (editorWidth, editorHeight, sidechainSnapshot, false);
        require (processor.applyFactoryPreset (3, false),
                 "Delta UI capture fixture failed");
        bandGraph->mouseDown (juce::MouseEvent (
            source, band0Node, left, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            bandGraph, bandGraph, eventTime, band0Node, eventTime, 1, false));
        bandGraph->mouseUp (juce::MouseEvent (
            source, band0Node, juce::ModifierKeys(), 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, bandGraph, bandGraph, eventTime, band0Node,
            eventTime, 1, true));
        deltaButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        require (processor.getDeltaListenBand() == 0
                     && deltaButton->getToggleState()
                     && deltaButton->getButtonText() == "DELTA ON",
                 "Delta UI capture did not latch visible Band 1 state");
        captureAt (editorWidth, editorHeight, deltaSnapshot, false);
        require (processor.getDeltaListenBand() == 0
                     && deltaButton->getToggleState()
                     && deltaButton->getButtonText() == "DELTA ON",
                 "Delta UI capture lost its visible latch during rendering");
        deltaButton->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        require (processor.getDeltaListenBand() == -1
                     && ! deltaButton->getToggleState()
                     && deltaButton->getButtonText() == "DELTA",
                 "Delta UI capture did not clean up its latch");
        setPlainValue (processor, tbmbc::params::bypassId, 1.0f);
        captureAt (editorWidth, editorHeight, bypassSnapshot, false);
        setPlainValue (processor, tbmbc::params::bypassId, 0.0f);

        require (processor.applyFactoryPreset (13, false),
                 "MASTER UI fixture preset failed");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (160);
        auto* masterSource = dynamic_cast<juce::Button*> (
            bandPanel->findChildWithID ("bandSidechainSource"));
        require (masterSource != nullptr && ! masterSource->getToggleState()
                     && masterSource->getButtonText() == "SIDECHAIN"
                     && bandPanel->findChildWithID ("bandDetectorMode") == nullptr,
                 "MASTER preset UI did not return to internal SIDECHAIN state");
        captureAt (editorWidth, editorHeight, masterPresetSnapshot, false);

        require (processor.applyFactoryPreset (12, false),
                 "positive/8X/Mid UI fixture preset failed");
        setPlainValue (processor, tbmbc::params::phaseModeId,
                       static_cast<float> (tbmbc::params::linearPhaseMode));
        setPlainValue (processor, tbmbc::params::oversamplingId,
                       static_cast<float> (tbmbc::params::oversampling8x));
        setPlainValue (processor, tbmbc::params::lookaheadId, 5.0f);
        const auto hiddenLinkBeforeMid = getPlainValue (processor, stereoLinkId);
        setPlainValue (processor, tbmbc::params::bandId (
                           0, tbmbc::params::channelModeSuffix),
                       static_cast<float> (tbmbc::params::midChannelMode));
        setPlainValue (processor, tbmbc::params::bandId (
                           0, tbmbc::params::modeSuffix),
                       static_cast<float> (tbmbc::params::compressMode));
        setPlainValue (processor, tbmbc::params::bandId (
                           0, tbmbc::params::thresholdSuffix), -50.0f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (160);
        auto* phaseLinear = dynamic_cast<juce::Button*> (
            editor->findChildWithID ("phaseLinear"));
        auto* oversampling8x = dynamic_cast<juce::Button*> (
            editor->findChildWithID ("oversampling3"));
        require (phaseLinear != nullptr && phaseLinear->getToggleState()
                     && oversampling8x != nullptr
                     && oversampling8x->getToggleState(),
                 "quality segmented controls did not follow host parameter state");
        require (bandPanel->findChildWithID ("bandStereoLink") == nullptr
                     && std::abs (getPlainValue (processor, stereoLinkId)
                                  - hiddenLinkBeforeMid) <= valueTolerance,
                 "hidden Stereo Link UI/value contract changed in Mid mode");
        captureAt (editorWidth, editorHeight, positive8xMidSnapshot, true);
        require (processor.getGainReductionDb (0) > 0.05f,
                 "positive/8X/Mid UI fixture did not produce upward gain");

        require (processor.applyFactoryPreset (3, false),
                 "four-band colour UI fixture preset failed");
        captureAt (editorWidth, editorHeight, fourBandsSnapshot, false);
    }

    const auto samplesBeforeHide = processor.getInputSpectrumAnalyzer()
                                       .getPushedSampleCount();
    const auto framesBeforeHide = processor.getInputSpectrumAnalyzer()
                                      .getFrameGeneration();
    processor.setDeltaListenBand (0);
    rightMouseDownAt (band1Node);
    require (processor.getDeltaListenBand() == 1,
             "latched-plus-momentary Delta hide-clear fixture could not be enabled");
    editor->setVisible (false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getSpectrumAnalyzerDemandCount() == 0,
             "hidden editor retained analyzer demand");
    require (processor.getDeltaListenBand() == -1,
             "hidden editor retained transient Delta Listen");
    for (int block = 0; block < 10; ++block)
    {
        auto buffer = makeProcessBuffer (processor, blockSize);
        fillProcessBuffer (buffer, 2, 0, analyzerSample);
        analyzerSample += blockSize;
        processor.processBlock (buffer, midi);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    require (processor.getInputSpectrumAnalyzer().getPushedSampleCount()
                 == samplesBeforeHide
                 && processor.getInputSpectrumAnalyzer().getFrameGeneration()
                        == framesBeforeHide,
             "hidden editor continued analyzer producer or FFT work");

    processor.setDeltaListenBand (1);
    destroyEditor (processor, editor);
    require (processor.getDeltaListenBand() == -1,
             "destroyed editor retained transient Delta Listen");
    auto* secondEditor = processor.createEditorIfNeeded();
    require (secondEditor != nullptr, "editor could not be recreated after destruction");
    secondEditor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    secondEditor->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    require (processor.getSpectrumAnalyzerDemandCount() == 1,
             "recreated visible editor did not restore analyzer demand");
    processor.setDeltaListenBand (2);
    destroyEditor (processor, secondEditor);
    require (processor.getSpectrumAnalyzerDemandCount() == 0,
             "destroyed editor retained analyzer demand");
    require (processor.getDeltaListenBand() == -1,
             "recreated editor destruction retained transient Delta Listen");
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    const auto snapshotPath = argc >= 2 ? juce::String::fromUTF8 (argv[1])
                                        : juce::String();

    TestRunner runner;
    runner.run ("stable 71-prefix plus 7 parameters and host bypass",
                testParameterAndHostContract);
    runner.run ("mono/stereo main and 0-4 optional mono sidechain buses",
                testBusLayoutsFiniteLatencyAndTail);
    runner.run ("quality latency and mono Channel Mode fallback",
                testQualityLatencyAndMonoChannelFallback);
    runner.run ("bit-exact bypass, dry, and neutral delta", testBypassAndDryAreExact);
    runner.run ("per-band sidechain, fallback, and non-leak",
                testSidechainSelectionFallbackAndNonLeak);
    runner.run ("analyzer demand, FFT transport, and offline gate",
                testSpectrumAnalyzerDemandAndTransport);
    runner.run ("state round-trip and atomic rejection",
                testStateRoundTripAndAtomicRejection);
    runner.run ("20 curated Factory presets and Undo/Redo",
                testFactoryPresetsAndUndoRedo);
    runner.run ("Factory preset audio smoke", testFactoryPresetAudioSmoke);
    runner.run ("isolated user preset round-trip", testUserPresetRoundTripInTempOnly);
    runner.run ("editor lifecycle, analyzer visibility, resize, and PNG",
                [&] { testEditorLifecycleAndSnapshot (snapshotPath); });
    return runner.finish();
}
