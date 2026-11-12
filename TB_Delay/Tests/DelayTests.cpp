#include <JuceHeader.h>

#include "../Source/DelayMapModel.h"
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Processor = TB_DelayAudioProcessor;
using Snapshot = std::array<float, TBDelayParameterIDs::count>;

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 257;
constexpr double pi = juce::MathConstants<double>::pi;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

std::string toStdString (const juce::String& text)
{
    return text.toStdString();
}

std::string number (double value, int precision = 6)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision (precision) << value;
    return stream.str();
}

int findFactoryPresetIndex (const juce::String& name)
{
    const auto& presets = getTBDelayFactoryPresets();
    for (std::size_t index = 0; index < presets.size(); ++index)
        if (juce::String (presets[index].name) == name)
            return static_cast<int> (index);

    return -1;
}

double gainToDecibels (double gain)
{
    return 20.0 * std::log10 (std::max (gain, 1.0e-12));
}

struct TestCase
{
    bool passed = true;
    std::vector<std::string> details;

    void expect (bool condition, const std::string& failure)
    {
        if (! condition)
        {
            passed = false;
            details.push_back ("FAIL: " + failure);
        }
    }

    void report (const std::string& measurement)
    {
        details.push_back (measurement);
    }
};

class TestRunner
{
public:
    template <typename Function>
    void run (const std::string& name, Function&& function)
    {
        std::cout << "[RUN ] " << name << std::endl;
        TestCase test;

        try
        {
            function (test);
        }
        catch (const std::exception& exception)
        {
            test.expect (false, std::string ("exception: ") + exception.what());
        }
        catch (...)
        {
            test.expect (false, "unknown exception");
        }

        std::cout << (test.passed ? "[PASS] " : "[FAIL] ") << name << '\n';
        for (const auto& detail : test.details)
            std::cout << "       " << detail << '\n';
        std::cout << std::flush;

        if (test.passed)
            ++passes;
        else
            ++failures;
    }

    void skip (const std::string& name, const std::string& reason)
    {
        std::cout << "[SKIP] " << name << '\n'
                  << "       " << reason << '\n';
        ++skips;
    }

    int finish() const
    {
        std::cout << "\nSUMMARY pass=" << passes
                  << " fail=" << failures
                  << " skip=" << skips << '\n';
        return failures == 0 ? 0 : 1;
    }

private:
    int passes = 0;
    int failures = 0;
    int skips = 0;
};

juce::File findProjectRoot()
{
    const auto walkParents = [] (juce::File start)
    {
        for (int depth = 0; depth < 12; ++depth)
        {
            if (start.getChildFile ("TB_Delay.jucer").existsAsFile())
                return start;

            const auto parent = start.getParentDirectory();
            if (parent == start)
                break;
            start = parent;
        }

        return juce::File();
    };

    if (auto root = walkParents (juce::File::getCurrentWorkingDirectory());
        root != juce::File())
        return root;

    return walkParents (
        juce::File::getSpecialLocation (juce::File::currentExecutableFile));
}

juce::File getOutputDirectory (const juce::File& projectRoot)
{
    const auto directory = projectRoot.getChildFile ("Tests").getChildFile ("output");
    directory.createDirectory();
    return directory;
}

juce::RangedAudioParameter* getParameter (Processor& processor, const char* id)
{
    return processor.getValueTreeState().getParameter (id);
}

const juce::RangedAudioParameter* getParameter (const Processor& processor,
                                                const char* id)
{
    return processor.getValueTreeState().getParameter (id);
}

float getPlainValue (const Processor& processor, const char* id)
{
    if (const auto* value = processor.getValueTreeState().getRawParameterValue (id))
        return value->load();

    return std::numeric_limits<float>::quiet_NaN();
}

bool setPlainValue (Processor& processor, const char* id, float plainValue)
{
    auto* parameter = getParameter (processor, id);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    return true;
}

template <size_t Size>
bool applyValues (
    Processor& processor,
    const std::array<std::pair<const char*, float>, Size>& values)
{
    bool allFound = true;
    for (const auto& [id, value] : values)
        allFound = setPlainValue (processor, id, value) && allFound;
    return allFound;
}

bool configureNeutral (Processor& processor)
{
    const std::array<std::pair<const char*, float>, TBDelayParameterIDs::count> values {{
        { TBDelayParameterIDs::time, 100.0f },
        { TBDelayParameterIDs::offset, 0.0f },
        { TBDelayParameterIDs::feedback, 0.0f },
        { TBDelayParameterIDs::spacing, 100.0f },
        { TBDelayParameterIDs::swing, 0.0f },
        { TBDelayParameterIDs::width, 100.0f },
        { TBDelayParameterIDs::mix, 100.0f },
        { TBDelayParameterIDs::duck, 0.0f },
        { TBDelayParameterIDs::output, 0.0f },
        { TBDelayParameterIDs::lowCut, 20.0f },
        { TBDelayParameterIDs::highCut, 20000.0f },
        { TBDelayParameterIDs::drive, 0.0f },
        { TBDelayParameterIDs::age, 0.0f },
        { TBDelayParameterIDs::modRate, 0.25f },
        { TBDelayParameterIDs::modDepth, 0.0f },
        { TBDelayParameterIDs::smear, 0.0f },
        { TBDelayParameterIDs::size, 32.0f },
        { TBDelayParameterIDs::shift, 0.0f },
        { TBDelayParameterIDs::diverge, 0.0f },
        { TBDelayParameterIDs::grain, 0.0f },
        { TBDelayParameterIDs::grainSize, 90.0f },
        { TBDelayParameterIDs::density, 4.0f },
        { TBDelayParameterIDs::spray, 0.0f },
        { TBDelayParameterIDs::pitch, 0.0f },
        { TBDelayParameterIDs::reverse, 0.0f },
        { TBDelayParameterIDs::timeMode, 0.0f },
        { TBDelayParameterIDs::division, 8.0f },
        { TBDelayParameterIDs::style, 0.0f },
        { TBDelayParameterIDs::taps, 0.0f },
        { TBDelayParameterIDs::character, 0.0f },
        { TBDelayParameterIDs::timeMotion, 0.0f },
        { TBDelayParameterIDs::transformRoute, 0.0f },
        { TBDelayParameterIDs::freeze, 0.0f },
        { TBDelayParameterIDs::stereoBypass, 0.0f },
        { TBDelayParameterIDs::colorSpaceBypass, 0.0f },
        { TBDelayParameterIDs::grainTransformBypass, 0.0f },
        { TBDelayParameterIDs::colorMacro, 0.0f },
        { TBDelayParameterIDs::spaceMacro, 0.0f }
    }};

    return applyValues (processor, values);
}

bool configureChannels (Processor& processor, int channels)
{
    const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono()
                                          : juce::AudioChannelSet::stereo();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (channelSet);
    layout.outputBuses.add (channelSet);
    return processor.setBusesLayout (layout);
}

bool configureMonoToStereo (Processor& processor)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    return processor.setBusesLayout (layout);
}

Snapshot takeSnapshot (const Processor& processor)
{
    Snapshot result {};
    for (size_t index = 0; index < result.size(); ++index)
        result[index] = getPlainValue (processor, TBDelayParameterIDs::all[index]);
    return result;
}

double valueTolerance (const juce::RangedAudioParameter& parameter,
                       float expected)
{
    const auto interval = static_cast<double> (
        parameter.getNormalisableRange().interval);
    if (interval > 0.0)
        return interval * 0.51 + 1.0e-6;

    return std::max (1.0e-5, std::abs (static_cast<double> (expected)) * 1.0e-5);
}

bool snapshotsMatch (const Processor& processor,
                     const Snapshot& first,
                     const Snapshot& second,
                     double& maximumError)
{
    maximumError = 0.0;

    for (size_t index = 0; index < first.size(); ++index)
    {
        if (! std::isfinite (first[index]) || ! std::isfinite (second[index]))
            return false;

        const auto error = std::abs (
            static_cast<double> (first[index] - second[index]));
        maximumError = std::max (maximumError, error);

        const auto* parameter = getParameter (
            processor, TBDelayParameterIDs::all[index]);
        if (parameter == nullptr
            || error > valueTolerance (*parameter, first[index]))
            return false;
    }

    return true;
}

void moveEveryParameterAwayFrom (Processor& processor,
                                 const Snapshot& reference)
{
    for (size_t index = 0; index < reference.size(); ++index)
    {
        auto* parameter = getParameter (
            processor, TBDelayParameterIDs::all[index]);
        if (parameter == nullptr)
            continue;

        const auto normalised = parameter->convertTo0to1 (reference[index]);
        const auto replacement = parameter->convertFrom0to1 (
            normalised < 0.5f ? 0.82f : 0.18f);
        setPlainValue (
            processor, TBDelayParameterIDs::all[index], replacement);
    }
}

juce::AudioBuffer<float> processBuffer (
    Processor& processor,
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    int blockSize)
{
    juce::AudioBuffer<float> output (
        input.getNumChannels(), input.getNumSamples());

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
        output.copyFrom (
            channel, 0, input, channel, 0, input.getNumSamples());

    processor.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;

    for (int offset = 0; offset < output.getNumSamples(); offset += blockSize)
    {
        const auto count = std::min (blockSize, output.getNumSamples() - offset);
        juce::AudioBuffer<float> block (output.getNumChannels(), count);

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            block.copyFrom (channel, 0, output, channel, offset, count);

        midi.clear();
        processor.processBlock (block, midi);

        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            output.copyFrom (channel, offset, block, channel, 0, count);
    }

    processor.releaseResources();
    return output;
}

juce::AudioBuffer<float> processMonoInputToStereo (
    Processor& processor,
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    int blockSize)
{
    juce::AudioBuffer<float> output (2, input.getNumSamples());
    output.clear();
    output.copyFrom (
        0, 0, input, 0, 0, input.getNumSamples());

    processor.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;

    for (int offset = 0; offset < output.getNumSamples(); offset += blockSize)
    {
        const auto count = std::min (
            blockSize, output.getNumSamples() - offset);
        juce::AudioBuffer<float> block (2, count);
        block.clear();
        block.copyFrom (0, 0, output, 0, offset, count);

        midi.clear();
        processor.processBlock (block, midi);

        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom (
                channel, offset, block, channel, 0, count);
    }

    processor.releaseResources();
    return output;
}

struct SignalMetrics
{
    bool finite = true;
    double peak = 0.0;
    double rms = 0.0;
    double mean = 0.0;
};

SignalMetrics measureSignal (const juce::AudioBuffer<float>& buffer,
                             int startSample = 0,
                             int requestedSamples = -1)
{
    SignalMetrics result;
    startSample = juce::jlimit (0, buffer.getNumSamples(), startSample);
    const auto available = buffer.getNumSamples() - startSample;
    const auto samples = requestedSamples < 0
                             ? available
                             : juce::jlimit (0, available, requestedSamples);
    double sum = 0.0;
    double sumSquares = 0.0;
    std::uint64_t count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer (channel, startSample);
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto value = static_cast<double> (data[sample]);
            if (! std::isfinite (value))
            {
                result.finite = false;
                continue;
            }

            result.peak = std::max (result.peak, std::abs (value));
            sum += value;
            sumSquares += value * value;
            ++count;
        }
    }

    if (count > 0)
    {
        result.mean = sum / static_cast<double> (count);
        result.rms = std::sqrt (sumSquares / static_cast<double> (count));
    }

    return result;
}

struct Peak
{
    int index = -1;
    float value = 0.0f;
};

Peak findPeak (const juce::AudioBuffer<float>& buffer,
               int channel,
               int startSample,
               int endSample)
{
    Peak result;
    if (! juce::isPositiveAndBelow (channel, buffer.getNumChannels()))
        return result;

    startSample = juce::jlimit (0, buffer.getNumSamples(), startSample);
    endSample = juce::jlimit (startSample, buffer.getNumSamples(), endSample);
    const auto* data = buffer.getReadPointer (channel);

    for (int sample = startSample; sample < endSample; ++sample)
    {
        if (result.index < 0
            || std::abs (data[sample]) > std::abs (result.value))
        {
            result.index = sample;
            result.value = data[sample];
        }
    }

    return result;
}

double toneMagnitude (const juce::AudioBuffer<float>& buffer,
                      int channel,
                      int startSample,
                      int sampleCount,
                      double frequency,
                      double sampleRate,
                      bool useHannWindow = true)
{
    if (! juce::isPositiveAndBelow (channel, buffer.getNumChannels())
        || frequency <= 0.0 || frequency >= sampleRate * 0.5
        || sampleCount < 2)
        return 0.0;

    startSample = juce::jlimit (0, buffer.getNumSamples(), startSample);
    sampleCount = juce::jlimit (
        0, buffer.getNumSamples() - startSample, sampleCount);
    if (sampleCount < 2)
        return 0.0;

    const auto omega = twoPi * frequency / sampleRate;
    const auto coefficient = 2.0 * std::cos (omega);
    double state1 = 0.0;
    double state2 = 0.0;
    double windowSum = 0.0;
    const auto* data = buffer.getReadPointer (channel, startSample);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto window = useHannWindow
                                ? 0.5
                                      - 0.5 * std::cos (
                                                  twoPi
                                                  * static_cast<double> (sample)
                                                  / static_cast<double> (
                                                      sampleCount - 1))
                                : 1.0;
        const auto state0 =
            static_cast<double> (data[sample]) * window
            + coefficient * state1 - state2;
        state2 = state1;
        state1 = state0;
        windowSum += window;
    }

    const auto real = state1 - state2 * std::cos (omega);
    const auto imaginary = state2 * std::sin (omega);
    return 2.0 * std::sqrt (real * real + imaginary * imaginary)
           / std::max (windowSum, 1.0);
}

juce::AudioBuffer<float> makeImpulse (
    int channels,
    int samples,
    float amplitude,
    bool leftOnly = false)
{
    juce::AudioBuffer<float> result (channels, samples);
    result.clear();
    result.setSample (0, 0, amplitude);
    if (channels > 1 && ! leftOnly)
        result.setSample (1, 0, amplitude);
    return result;
}

juce::AudioBuffer<float> makeSine (
    int channels,
    int totalSamples,
    int activeSamples,
    double sampleRate,
    double frequency,
    float amplitude)
{
    juce::AudioBuffer<float> result (channels, totalSamples);
    result.clear();
    const auto fadeSamples = std::max (
        1, static_cast<int> (std::round (sampleRate * 0.01)));

    for (int sample = 0; sample < std::min (activeSamples, totalSamples); ++sample)
    {
        auto gain = 1.0;
        if (sample < fadeSamples)
            gain *= static_cast<double> (sample) / fadeSamples;
        if (sample >= activeSamples - fadeSamples)
            gain *= static_cast<double> (activeSamples - 1 - sample)
                    / fadeSamples;

        const auto value = static_cast<float> (
            amplitude * gain
            * std::sin (twoPi * frequency * sample / sampleRate));
        for (int channel = 0; channel < channels; ++channel)
            result.setSample (channel, sample, value);
    }

    return result;
}

double differenceRms (const juce::AudioBuffer<float>& first,
                      const juce::AudioBuffer<float>& second,
                      double& maximumDifference)
{
    maximumDifference = 0.0;
    double sumSquares = 0.0;
    std::uint64_t count = 0;
    const auto channels = std::min (
        first.getNumChannels(), second.getNumChannels());
    const auto samples = std::min (
        first.getNumSamples(), second.getNumSamples());

    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto difference = static_cast<double> (
                first.getSample (channel, sample)
                - second.getSample (channel, sample));
            maximumDifference =
                std::max (maximumDifference, std::abs (difference));
            sumSquares += difference * difference;
            ++count;
        }

    return count > 0
               ? std::sqrt (sumSquares / static_cast<double> (count))
               : 0.0;
}

void testParameters (TestRunner& runner)
{
    runner.run ("parameter count, IDs, and defaults", [] (TestCase& test)
    {
        Processor processor;
        const auto actualCount = processor.getParameters().size();
        test.expect (
            actualCount == static_cast<int> (TBDelayParameterIDs::count),
            "expected 38 parameters, got " + std::to_string (actualCount));

        std::set<std::string> ids;
        auto missing = 0;
        auto wrongDefaults = 0;
        double maximumDefaultError = 0.0;

        for (size_t index = 0; index < TBDelayParameterIDs::all.size(); ++index)
        {
            const auto* id = TBDelayParameterIDs::all[index];
            ids.insert (id);
            const auto* parameter = getParameter (processor, id);

            if (parameter == nullptr)
            {
                ++missing;
                continue;
            }

            const auto expected = TBDelayPresetDetail::initValues[index];
            const auto declaredDefault =
                parameter->convertFrom0to1 (parameter->getDefaultValue());
            const auto current = getPlainValue (processor, id);
            const auto error = std::max (
                std::abs (static_cast<double> (declaredDefault - expected)),
                std::abs (static_cast<double> (current - expected)));
            maximumDefaultError = std::max (maximumDefaultError, error);

            if (error > valueTolerance (*parameter, expected))
                ++wrongDefaults;
        }

        test.expect (ids.size() == TBDelayParameterIDs::count,
                     "parameter ID contract contains duplicates");
        test.expect (missing == 0,
                     std::to_string (missing) + " expected IDs are missing");
        test.expect (wrongDefaults == 0,
                     std::to_string (wrongDefaults)
                         + " defaults differ from Init");
        const auto* timeParameter =
            getParameter (processor, TBDelayParameterIDs::time);
        const auto timeRangeIsProductRange =
            timeParameter != nullptr
            && std::abs (
                   timeParameter->getNormalisableRange().start
                   - 10.0f) <= 0.000001f
            && std::abs (
                   timeParameter->getNormalisableRange().end
                   - 8000.0f) <= 0.000001f
            && std::abs (
                   static_cast<double> (
                       timeParameter->convertFrom0to1 (0.5f))
                   - std::sqrt (10.0 * 8000.0)) <= 0.01;
        test.expect (
            timeRangeIsProductRange,
            "TIME range is not 10-8000 ms with the expected log centre");
        test.report (
            "count=" + std::to_string (actualCount)
            + " uniqueIDs=" + std::to_string (ids.size())
            + " maxDefaultError=" + number (maximumDefaultError, 8)
            + " timeCentre="
            + number (
                timeParameter != nullptr
                    ? timeParameter->convertFrom0to1 (0.5f)
                    : 0.0f,
                3) + "ms");
    });
}

void testMusicalMacros (TestRunner& runner)
{
    runner.run (
        "COLOR/SPACE macros preserve BASE and derive technical coefficients",
        [] (TestCase& test)
    {
        Processor processor;
        const auto base =
            processor.getEffectiveEngineParametersForDisplay();
        const std::array<const char*, 8> hiddenIDs {
            TBDelayParameterIDs::lowCut,
            TBDelayParameterIDs::highCut,
            TBDelayParameterIDs::drive,
            TBDelayParameterIDs::age,
            TBDelayParameterIDs::modRate,
            TBDelayParameterIDs::modDepth,
            TBDelayParameterIDs::smear,
            TBDelayParameterIDs::size
        };
        std::array<float, hiddenIDs.size()> hiddenBase {};
        for (std::size_t index = 0; index < hiddenIDs.size(); ++index)
            hiddenBase[index] = getPlainValue (processor, hiddenIDs[index]);

        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::colorMacro, -100.0f)
                && setPlainValue (
                    processor, TBDelayParameterIDs::spaceMacro, -100.0f),
            "failed to set macro minimums");
        const auto minimum =
            processor.getEffectiveEngineParametersForDisplay();
        test.expect (
            std::abs (minimum.drivePercent) < 0.0001f
                && std::abs (minimum.agePercent) < 0.0001f
                && std::abs (minimum.lowCutHz - 20.0f) < 0.001f
                && std::abs (minimum.highCutHz - 20000.0f) < 0.01f,
            "COLOR -100 is not OPEN/CLEAN");
        test.expect (
            std::abs (minimum.modDepthMs) < 0.0001f
                && std::abs (minimum.smearPercent) < 0.0001f
                && std::abs (minimum.diffusionSizeMs - 2.0f) < 0.001f,
            "SPACE -100 is not TIGHT/STILL");

        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::colorMacro, 100.0f)
                && setPlainValue (
                    processor, TBDelayParameterIDs::spaceMacro, 100.0f),
            "failed to set macro maximums");
        const auto maximum =
            processor.getEffectiveEngineParametersForDisplay();
        test.expect (
            maximum.drivePercent >= 71.99f
                && maximum.agePercent >= 81.99f
                && maximum.lowCutHz >= 319.99f
                && maximum.highCutHz <= 4200.01f,
            "COLOR +100 is not WORN/DARK");
        test.expect (
            maximum.modRateHz >= 1.499f
                && maximum.modDepthMs >= 4.399f
                && maximum.smearPercent >= 99.99f
                && maximum.diffusionSizeMs >= 119.99f,
            "SPACE +100 is not WASH/MOVING");

        auto hiddenValuesPreserved = true;
        for (std::size_t index = 0; index < hiddenIDs.size(); ++index)
            hiddenValuesPreserved = hiddenValuesPreserved
                && getPlainValue (processor, hiddenIDs[index])
                       == hiddenBase[index];
        test.expect (
            hiddenValuesPreserved,
            "a macro mutated legacy APVTS parameter values");

        setPlainValue (
            processor, TBDelayParameterIDs::colorMacro, 0.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::spaceMacro, 0.0f);
        const auto restored =
            processor.getEffectiveEngineParametersForDisplay();
        const auto baseRestored =
            restored.lowCutHz == base.lowCutHz
            && restored.highCutHz == base.highCutHz
            && restored.drivePercent == base.drivePercent
            && restored.agePercent == base.agePercent
            && restored.modRateHz == base.modRateHz
            && restored.modDepthMs == base.modDepthMs
            && restored.smearPercent == base.smearPercent
            && restored.diffusionSizeMs == base.diffusionSizeMs
            && restored.timeMs == base.timeMs
            && restored.style == base.style
            && restored.mixPercent == base.mixPercent
            && restored.outputDb == base.outputDb;
        test.expect (
            baseRestored,
            "macro BASE does not exactly restore the preset-derived engine state");
        test.report (
            "COLOR=" + number (minimum.highCutHz, 0)
            + "->" + number (base.highCutHz, 0)
            + "->" + number (maximum.highCutHz, 0)
            + "Hz SPACE=" + number (minimum.smearPercent, 0)
            + "->" + number (base.smearPercent, 0)
            + "->" + number (maximum.smearPercent, 0)
            + "% rawTechnicalValuesPreserved="
            + std::string (hiddenValuesPreserved ? "yes" : "no"));
    });
}

void testFactoryPresets (TestRunner& runner)
{
    runner.run ("factory bank: 120 unique and in-range states", [] (TestCase& test)
    {
        Processor processor;
        const auto& presets = getTBDelayFactoryPresets();
        test.expect (presets.size() == 120,
                     "expected 120 presets, got "
                         + std::to_string (presets.size()));
        test.expect (
            processor.getNumPrograms() == static_cast<int> (presets.size()),
            "host program count does not match the factory bank");

        std::set<std::string> names;
        auto emptyNames = 0;
        auto invalidValues = 0;
        auto duplicateStates = 0;
        auto nearDuplicateStates = 0;
        std::string nearDuplicateExample;
        auto applyMismatches = 0;
        auto programApiMismatches = 0;
        auto coldStartFreezePresets = 0;
        auto legacyNonNeutralMacroPresets = 0;
        auto appendedMacroPresets = 0;

        for (size_t presetIndex = 0; presetIndex < presets.size(); ++presetIndex)
        {
            const auto& preset = presets[presetIndex];
            names.insert (preset.name != nullptr ? preset.name : "");
            if (preset.name == nullptr || preset.name[0] == '\0')
                ++emptyNames;
            if (preset.values[static_cast<std::size_t> (
                    TBDelayPresetDetail::Parameter::freeze)] >= 0.5f)
                ++coldStartFreezePresets;
            const auto colorMacro = preset.values[static_cast<std::size_t> (
                TBDelayPresetDetail::Parameter::colorMacro)];
            const auto spaceMacro = preset.values[static_cast<std::size_t> (
                TBDelayPresetDetail::Parameter::spaceMacro)];
            if (presetIndex < 100
                && (colorMacro != 0.0f || spaceMacro != 0.0f))
                ++legacyNonNeutralMacroPresets;
            if (presetIndex >= 100
                && (colorMacro != 0.0f || spaceMacro != 0.0f))
                ++appendedMacroPresets;

            for (size_t parameterIndex = 0;
                 parameterIndex < preset.values.size();
                 ++parameterIndex)
            {
                const auto value = preset.values[parameterIndex];
                const auto* parameter = getParameter (
                    processor, TBDelayParameterIDs::all[parameterIndex]);
                if (parameter == nullptr || ! std::isfinite (value))
                {
                    ++invalidValues;
                    continue;
                }

                const auto range = parameter->getNormalisableRange();
                const auto roundTrip = parameter->convertFrom0to1 (
                    parameter->convertTo0to1 (value));
                if (value < range.start || value > range.end
                    || std::abs (roundTrip - value)
                           > valueTolerance (*parameter, value))
                    ++invalidValues;
            }

            for (size_t other = presetIndex + 1;
                 other < presets.size();
                 ++other)
            {
                if (TBDelayPresetDetail::sameSettings (
                        preset, presets[other]))
                    ++duplicateStates;

                auto nonLevelDifferences = 0;
                for (std::size_t parameterIndex = 0;
                     parameterIndex < preset.values.size();
                     ++parameterIndex)
                {
                    if (parameterIndex
                            == static_cast<std::size_t> (
                                TBDelayPresetDetail::Parameter::mix)
                        || parameterIndex
                               == static_cast<std::size_t> (
                                   TBDelayPresetDetail::Parameter::output))
                        continue;
                    if (preset.values[parameterIndex]
                        != presets[other].values[parameterIndex])
                        ++nonLevelDifferences;
                }

                if (nonLevelDifferences < 2)
                {
                    ++nearDuplicateStates;
                    if (nearDuplicateExample.empty())
                        nearDuplicateExample =
                            std::string (preset.name) + " / "
                            + presets[other].name;
                }
            }

            processor.setCurrentProgram (
                static_cast<int> (presetIndex));
            const auto applied = takeSnapshot (processor);
            if (processor.getCurrentProgram()
                    != static_cast<int> (presetIndex)
                || processor.getProgramName (
                       static_cast<int> (presetIndex))
                       != preset.name
                || processor.getDisplayedPresetName() != preset.name)
                ++programApiMismatches;
            for (size_t parameterIndex = 0;
                 parameterIndex < applied.size();
                 ++parameterIndex)
            {
                const auto* parameter = getParameter (
                    processor, TBDelayParameterIDs::all[parameterIndex]);
                if (parameter == nullptr
                    || std::abs (
                           applied[parameterIndex]
                           - preset.values[parameterIndex])
                           > valueTolerance (
                               *parameter,
                               preset.values[parameterIndex]))
                    ++applyMismatches;
            }
        }

        test.expect (names.size() == presets.size(),
                     "factory names are empty or duplicated");
        test.expect (emptyNames == 0,
                     std::to_string (emptyNames)
                         + " factory names are empty");
        test.expect (invalidValues == 0,
                     std::to_string (invalidValues)
                         + " values are invalid or out of range");
        test.expect (duplicateStates == 0,
                     std::to_string (duplicateStates)
                         + " duplicate full states found");
        test.expect (
            nearDuplicateStates == 0,
            std::to_string (nearDuplicateStates)
                + " preset pairs differ only by Mix/Output or one "
                  "non-level parameter"
                + (nearDuplicateExample.empty()
                       ? std::string()
                       : ": " + nearDuplicateExample));
        test.expect (
            coldStartFreezePresets == 0,
            std::to_string (coldStartFreezePresets)
                + " factory presets cold-start with Freeze enabled");
        test.expect (
            legacyNonNeutralMacroPresets == 0,
            "legacy 0-99 factory programs must keep both new macros at BASE");
        test.expect (
            appendedMacroPresets >= 8,
            "appended factory programs do not exercise the new macro directions");
        test.expect (applyMismatches == 0,
                     std::to_string (applyMismatches)
                         + " values failed actual APVTS application");
        test.expect (programApiMismatches == 0,
                     std::to_string (programApiMismatches)
                         + " host program API mismatches found");
        test.report (
            "presets=" + std::to_string (presets.size())
            + " uniqueNames=" + std::to_string (names.size())
            + " invalidValues=" + std::to_string (invalidValues)
            + " duplicateStates=" + std::to_string (duplicateStates)
            + " nearDuplicateStates="
            + std::to_string (nearDuplicateStates)
            + " programApiMismatches="
            + std::to_string (programApiMismatches)
            + " coldStartFreeze="
            + std::to_string (coldStartFreezePresets)
            + " legacyNonNeutralMacros="
            + std::to_string (legacyNonNeutralMacroPresets)
            + " appendedMacroPresets="
            + std::to_string (appendedMacroPresets));
    });
}

void testFactoryPresetAudioSafety (TestRunner& runner)
{
    runner.run (
        "all 120 factory presets render finite and bounded audio",
        [] (TestCase& test)
    {
        constexpr int renderSamples =
            static_cast<int> (testSampleRate * 9.25);
        juce::AudioBuffer<float> input (2, renderSamples);
        input.clear();
        input.setSample (0, 0, 0.35f);
        input.setSample (1, 0, -0.22f);
        input.setSample (0, 12000, -0.14f);
        input.setSample (1, 12000, 0.18f);

        const auto& presets = getTBDelayFactoryPresets();
        auto nonFiniteRenders = 0;
        auto overBoundRenders = 0;
        double maximumPeak = 0.0;
        juce::String maximumPeakPreset;

        for (std::size_t index = 0; index < presets.size(); ++index)
        {
            Processor processor;
            processor.applyFactoryPreset (
                static_cast<int> (index), false);
            const auto output = processBuffer (
                processor, input, testSampleRate, 512);
            const auto metrics = measureSignal (output);

            if (! metrics.finite)
                ++nonFiniteRenders;
            if (metrics.peak > 8.0)
                ++overBoundRenders;
            if (metrics.peak > maximumPeak)
            {
                maximumPeak = metrics.peak;
                maximumPeakPreset = presets[index].name;
            }
        }

        test.expect (
            nonFiniteRenders == 0,
            std::to_string (nonFiniteRenders)
                + " factory preset renders produced NaN/Inf");
        test.expect (
            overBoundRenders == 0,
            std::to_string (overBoundRenders)
                + " factory preset renders exceeded peak 8.0");
        test.report (
            "renders=" + std::to_string (presets.size())
            + " maxPeak=" + number (maximumPeak, 6)
            + " maxPeakPreset=\""
            + toStdString (maximumPeakPreset) + "\"");
    });
}

void testImpulseTiming (TestRunner& runner)
{
    runner.run ("clean and Dual impulse timing within two samples",
                [] (TestCase& test)
    {
        Processor processor;
        test.expect (configureNeutral (processor),
                     "neutral parameters could not be set");
        test.expect (configureChannels (processor, 2),
                     "stereo layout was rejected");
        setPlainValue (processor, TBDelayParameterIDs::time, 100.0f);

        const auto input = makeImpulse (2, 7200, 0.25f);
        const auto output = processBuffer (
            processor, input, testSampleRate, testBlockSize);
        const auto expected = 4800;
        const auto left = findPeak (output, 0, 0, output.getNumSamples());
        const auto right = findPeak (output, 1, 0, output.getNumSamples());
        const auto metrics = measureSignal (output);

        test.expect (metrics.finite, "output contains NaN or Inf");
        test.expect (std::abs (left.index - expected) <= 2,
                     "left peak at " + std::to_string (left.index)
                         + ", expected " + std::to_string (expected));
        test.expect (std::abs (right.index - expected) <= 2,
                     "right peak at " + std::to_string (right.index)
                         + ", expected " + std::to_string (expected));

        Processor dualProcessor;
        configureNeutral (dualProcessor);
        configureChannels (dualProcessor, 2);
        setPlainValue (dualProcessor, TBDelayParameterIDs::time, 100.0f);
        setPlainValue (dualProcessor, TBDelayParameterIDs::style, 1.0f);
        setPlainValue (dualProcessor, TBDelayParameterIDs::offset, 40.0f);
        const auto dual = processBuffer (
            dualProcessor, makeImpulse (2, 7200, 0.25f),
            testSampleRate, testBlockSize);
        const auto dualLeft = findPeak (
            dual, 0, 0, dual.getNumSamples());
        const auto dualRight = findPeak (
            dual, 1, 0, dual.getNumSamples());
        TBDelayMapParameters mapParameters;
        mapParameters.timeMode = 0;
        mapParameters.timeMs = 100.0;
        mapParameters.style = 1;
        mapParameters.offsetPercent = 40.0f;
        mapParameters.feedbackPercent = 0.0f;
        const auto mapPrediction =
            buildTBDelayMapPrediction (mapParameters);
        const auto mapLeft = juce::roundToInt (
            mapPrediction.channelDelayMs[0]
            * testSampleRate / 1000.0);
        const auto mapRight = juce::roundToInt (
            mapPrediction.channelDelayMs[1]
            * testSampleRate / 1000.0);
        test.expect (std::abs (dualLeft.index - 4800) <= 2,
                     "Dual left peak at "
                         + std::to_string (dualLeft.index)
                         + ", expected 4800");
        test.expect (std::abs (dualRight.index - 5760) <= 2,
                     "Dual right peak at "
                         + std::to_string (dualRight.index)
                         + ", expected 5760");
        test.expect (
            std::abs (dualLeft.index - mapLeft) <= 2
                && std::abs (dualRight.index - mapRight) <= 2,
            "Delay Map channel times drifted from processBlock impulse peaks");
        test.report (
            "expected=4800 left=" + std::to_string (left.index)
            + " right=" + std::to_string (right.index)
            + " dualL=" + std::to_string (dualLeft.index)
            + " dualR=" + std::to_string (dualRight.index)
            + " map=" + std::to_string (mapLeft)
            + "/" + std::to_string (mapRight)
            + " peak=" + number (metrics.peak));
    });

    runner.run ("TIME minimum is an audible-safe 10 ms", [] (TestCase& test)
    {
        Processor processor;
        configureNeutral (processor);
        configureChannels (processor, 1);
        setPlainValue (processor, TBDelayParameterIDs::time, 0.0f);

        const auto output = processBuffer (
            processor, makeImpulse (1, 1200, 0.25f),
            testSampleRate, 64);
        const auto beforeMinimum = findPeak (output, 0, 0, 477);
        const auto firstEcho = findPeak (output, 0, 477, 486);
        const auto effectiveTime =
            getPlainValue (processor, TBDelayParameterIDs::time);

        test.expect (
            std::abs (effectiveTime - 10.0f) <= 0.001f,
            "TIME did not clamp to 10 ms");
        test.expect (
            std::abs (beforeMinimum.value) <= 1.0e-6f,
            "wet signal leaked before the 10 ms minimum");
        test.expect (
            std::abs (firstEcho.index - 480) <= 2
                && std::abs (firstEcho.value) > 1.0e-5f,
            "10 ms first echo was not rendered at 480 samples");

        Processor tape;
        configureNeutral (tape);
        configureChannels (tape, 1);
        setPlainValue (tape, TBDelayParameterIDs::time, 10.0f);
        setPlainValue (tape, TBDelayParameterIDs::character, 1.0f);
        setPlainValue (tape, TBDelayParameterIDs::age, 100.0f);
        setPlainValue (tape, TBDelayParameterIDs::modDepth, 20.0f);
        const auto tapeOutput = processBuffer (
            tape, makeImpulse (1, 1200, 0.25f),
            testSampleRate, 64);
        const auto tapeBefore = findPeak (
            tapeOutput, 0, 0, 477);
        const auto tapeEcho = findPeak (
            tapeOutput, 0, 477, 488);
        test.expect (
            std::abs (tapeBefore.value) <= 1.0e-6f
                && std::abs (tapeEcho.index - 480) <= 2
                && std::abs (tapeEcho.value) > 1.0e-5f,
            "Tape Age/modulation escaped the 10 ms read-head floor");
        test.report (
            "effectiveTime=" + number (effectiveTime, 3)
            + "ms firstEcho=" + std::to_string (firstEcho.index)
            + " prePeak=" + number (beforeMinimum.value, 8)
            + " tapeFirstEcho=" + std::to_string (tapeEcho.index));
    });
}

void testTimeChangeAutomation (TestRunner& runner)
{
    runner.run (
        "TIME CHANGE Crossfade and Glide survive 100-to-600 ms automation",
        [] (TestCase& test)
    {
        constexpr int blockSize = 64;
        constexpr int totalSamples = static_cast<int> (1.6 * testSampleRate);
        constexpr int changeSample = static_cast<int> (0.8 * testSampleRate);
        constexpr double sourceFrequency = 223.0;
        constexpr float sourceAmplitude = 0.12f;

        const auto render = [=] (int timeMotion)
        {
            Processor processor;
            configureNeutral (processor);
            configureChannels (processor, 1);
            setPlainValue (processor, TBDelayParameterIDs::time, 100.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::timeMotion,
                static_cast<float> (timeMotion));
            processor.prepareToPlay (testSampleRate, blockSize);

            juce::AudioBuffer<float> output (1, totalSamples);
            output.clear();
            juce::MidiBuffer midi;

            for (int offset = 0; offset < totalSamples; offset += blockSize)
            {
                if (offset == changeSample)
                    setPlainValue (
                        processor, TBDelayParameterIDs::time, 600.0f);

                const auto samples = std::min (
                    blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, samples);
                for (int sample = 0; sample < samples; ++sample)
                {
                    const auto absoluteSample = offset + sample;
                    block.setSample (
                        0, sample,
                        static_cast<float> (
                            sourceAmplitude
                            * std::sin (
                                twoPi * sourceFrequency
                                * absoluteSample / testSampleRate)));
                }

                midi.clear();
                processor.processBlock (block, midi);
                output.copyFrom (0, offset, block, 0, 0, samples);
            }

            processor.releaseResources();
            return output;
        };

        const auto maximumAdjacentStep = [] (
            const juce::AudioBuffer<float>& buffer)
        {
            double result = 0.0;
            const auto startSample = static_cast<int> (
                0.2 * testSampleRate);
            const auto* data = buffer.getReadPointer (0);
            for (int sample = startSample + 1;
                 sample < buffer.getNumSamples();
                 ++sample)
                result = std::max (
                    result,
                    std::abs (
                        static_cast<double> (data[sample])
                        - static_cast<double> (data[sample - 1])));
            return result;
        };

        const auto crossfade = render (0);
        const auto glide = render (1);
        const auto crossfadeMetrics = measureSignal (crossfade);
        const auto glideMetrics = measureSignal (glide);
        const auto crossfadeStep = maximumAdjacentStep (crossfade);
        const auto glideStep = maximumAdjacentStep (glide);

        double pathDifferenceMaximum = 0.0;
        double pathDifferenceEnergy = 0.0;
        std::uint64_t pathDifferenceCount = 0;
        const auto compareEnd = std::min (
            totalSamples,
            changeSample + static_cast<int> (0.45 * testSampleRate));
        for (int sample = changeSample; sample < compareEnd; ++sample)
        {
            const auto difference = static_cast<double> (
                crossfade.getSample (0, sample)
                - glide.getSample (0, sample));
            pathDifferenceMaximum = std::max (
                pathDifferenceMaximum, std::abs (difference));
            pathDifferenceEnergy += difference * difference;
            ++pathDifferenceCount;
        }
        const auto pathDifferenceRms =
            pathDifferenceCount > 0
                ? std::sqrt (
                      pathDifferenceEnergy
                      / static_cast<double> (pathDifferenceCount))
                : 0.0;

        test.expect (
            crossfadeMetrics.finite && glideMetrics.finite,
            "TIME CHANGE automation produced NaN or Inf");
        test.expect (
            crossfadeMetrics.peak < 1.0 && glideMetrics.peak < 1.0,
            "TIME CHANGE automation exceeded the bounded peak limit");
        test.expect (
            crossfadeStep < 0.08 && glideStep < 0.08,
            "TIME CHANGE automation produced an excessive adjacent-sample step");
        test.expect (
            pathDifferenceRms > 1.0e-3
                && pathDifferenceMaximum > 1.0e-2,
            "Crossfade and Glide audio paths are indistinguishable");
        test.report (
            "crossfadePeak=" + number (crossfadeMetrics.peak, 6)
            + " glidePeak=" + number (glideMetrics.peak, 6)
            + " crossfadeStep=" + number (crossfadeStep, 6)
            + " glideStep=" + number (glideStep, 6)
            + " pathDiffRMS=" + number (pathDifferenceRms, 6)
            + " pathDiffMax=" + number (pathDifferenceMaximum, 6));
    });
}

void testPingPong (TestRunner& runner)
{
    runner.run ("Ping-Pong alternates L-R-L", [] (TestCase& test)
    {
        Processor processor;
        configureNeutral (processor);
        configureChannels (processor, 2);
        setPlainValue (processor, TBDelayParameterIDs::style, 2.0f);
        setPlainValue (processor, TBDelayParameterIDs::feedback, 70.0f);

        const auto input = makeImpulse (2, 16600, 0.25f);
        const auto output = processBuffer (
            processor, input, testSampleRate, testBlockSize);
        const std::array<int, 3> centres { 4800, 9600, 14400 };
        const std::array<int, 3> intendedChannels { 0, 1, 0 };
        double minimumDominanceDb = 999.0;

        for (size_t echo = 0; echo < centres.size(); ++echo)
        {
            const auto intended = findPeak (
                output, intendedChannels[echo],
                centres[echo] - 16, centres[echo] + 17);
            const auto other = findPeak (
                output, 1 - intendedChannels[echo],
                centres[echo] - 16, centres[echo] + 17);
            const auto dominanceDb = gainToDecibels (
                (std::abs (intended.value) + 1.0e-12)
                / (std::abs (other.value) + 1.0e-12));
            minimumDominanceDb = std::min (
                minimumDominanceDb, dominanceDb);

            test.expect (
                std::abs (intended.index - centres[echo]) <= 3,
                "echo " + std::to_string (echo + 1)
                    + " peak index=" + std::to_string (intended.index));
            test.expect (
                std::abs (intended.value) > 1.0e-5f,
                "echo " + std::to_string (echo + 1) + " is silent");
            test.expect (
                dominanceDb >= 6.0,
                "echo " + std::to_string (echo + 1)
                    + " channel dominance=" + number (dominanceDb, 2)
                    + " dB");
        }

        test.expect (measureSignal (output).finite,
                     "output contains NaN or Inf");
        test.report (
            "echoSamples=4800/9600/14400 minimumChannelDominance="
            + number (minimumDominanceDb, 2) + " dB");
    });

    runner.run ("mono input expands to stereo Ping-Pong", [] (TestCase& test)
    {
        Processor processor;
        configureNeutral (processor);
        test.expect (
            configureMonoToStereo (processor),
            "mono-input/stereo-output layout was rejected");
        setPlainValue (processor, TBDelayParameterIDs::style, 2.0f);
        setPlainValue (processor, TBDelayParameterIDs::feedback, 70.0f);

        const auto output = processMonoInputToStereo (
            processor, makeImpulse (1, 16600, 0.25f),
            testSampleRate, testBlockSize);
        const std::array<int, 3> centres { 4800, 9600, 14400 };
        const std::array<int, 3> intendedChannels { 0, 1, 0 };
        double minimumDominanceDb = 999.0;

        for (std::size_t echo = 0; echo < centres.size(); ++echo)
        {
            const auto intended = findPeak (
                output, intendedChannels[echo],
                centres[echo] - 16, centres[echo] + 17);
            const auto other = findPeak (
                output, 1 - intendedChannels[echo],
                centres[echo] - 16, centres[echo] + 17);
            const auto dominanceDb = gainToDecibels (
                (std::abs (intended.value) + 1.0e-12)
                / (std::abs (other.value) + 1.0e-12));
            minimumDominanceDb =
                std::min (minimumDominanceDb, dominanceDb);

            test.expect (
                std::abs (intended.index - centres[echo]) <= 3
                    && std::abs (intended.value) > 1.0e-5f,
                "mono-to-stereo Ping-Pong echo "
                    + std::to_string (echo + 1)
                    + " is missing");
            test.expect (
                dominanceDb >= 20.0,
                "mono-to-stereo Ping-Pong echo "
                    + std::to_string (echo + 1)
                    + " dominance=" + number (dominanceDb, 2)
                    + " dB");
        }

        test.expect (
            measureSignal (output).finite,
            "mono-to-stereo Ping-Pong produced NaN or Inf");
        test.report (
            "layout=1->2 minimumChannelDominance="
            + number (minimumDominanceDb, 2) + " dB");
    });

    runner.run ("Classic Ping Pong preset survives three centred repeats",
                [] (TestCase& test)
    {
        Processor processor;
        configureChannels (processor, 2);
        const auto presetIndex =
            findFactoryPresetIndex ("Classic Ping Pong");
        test.expect (
            presetIndex == 4,
            "Classic Ping Pong is missing or no longer program 4");
        if (presetIndex < 0)
            return;
        processor.applyFactoryPreset (presetIndex, false);

        const auto output = processBuffer (
            processor, makeImpulse (2, 53000, 0.25f),
            testSampleRate, testBlockSize);
        const std::array<int, 3> centres { 16000, 32000, 48000 };
        const std::array<int, 3> intendedChannels { 0, 1, 0 };
        double minimumDominanceDb = 999.0;

        for (std::size_t echo = 0; echo < centres.size(); ++echo)
        {
            const auto intended = findPeak (
                output, intendedChannels[echo],
                centres[echo] - 24, centres[echo] + 25);
            const auto other = findPeak (
                output, 1 - intendedChannels[echo],
                centres[echo] - 24, centres[echo] + 25);
            const auto dominanceDb = gainToDecibels (
                (std::abs (intended.value) + 1.0e-12)
                / (std::abs (other.value) + 1.0e-12));
            minimumDominanceDb =
                std::min (minimumDominanceDb, dominanceDb);

            test.expect (
                std::abs (intended.index - centres[echo]) <= 24
                    && std::abs (intended.value) > 1.0e-5f,
                "Classic Ping Pong repeat "
                    + std::to_string (echo + 1)
                    + " is missing");
            test.expect (
                dominanceDb >= 8.0,
                "Classic Ping Pong repeat "
                    + std::to_string (echo + 1)
                    + " dominance=" + number (dominanceDb, 2)
                    + " dB");
        }

        test.expect (
            measureSignal (output).finite,
            "Classic Ping Pong produced NaN or Inf");
        test.report (
            "echoSamples=16000/32000/48000 minimumChannelDominance="
            + number (minimumDominanceDb, 2) + " dB");
    });
}

void testCombTune (TestRunner& runner)
{
    runner.run ("comb Tune quantisation and feedback polarity", [] (TestCase& test)
    {
        const auto requestedFrequency = 100.0;
        const auto midi = std::round (
            69.0 + 12.0 * std::log2 (requestedFrequency / 440.0));
        const auto tunedFrequency =
            440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
        const auto period = testSampleRate / tunedFrequency;

        const auto renderComb = [&] (float offset)
        {
            Processor processor;
            configureNeutral (processor);
            configureChannels (processor, 1);
            setPlainValue (processor, TBDelayParameterIDs::time, 10.0f);
            setPlainValue (processor, TBDelayParameterIDs::timeMode, 2.0f);
            setPlainValue (processor, TBDelayParameterIDs::feedback, 75.0f);
            setPlainValue (processor, TBDelayParameterIDs::offset, offset);
            return processBuffer (
                processor, makeImpulse (1, 4096, 0.20f),
                testSampleRate, 64);
        };

        const auto positive = renderComb (0.0f);
        const auto negative = renderComb (-1.0f);
        std::array<Peak, 3> positivePeaks;
        std::array<Peak, 3> negativePeaks;

        for (int echo = 1; echo <= 3; ++echo)
        {
            const auto centre = static_cast<int> (
                std::round (period * echo));
            positivePeaks[static_cast<size_t> (echo - 1)] =
                findPeak (positive, 0, centre - 5, centre + 6);
            negativePeaks[static_cast<size_t> (echo - 1)] =
                findPeak (negative, 0, centre - 5, centre + 6);

            const auto tolerance = echo == 1 ? 2 : 3;
            test.expect (
                std::abs (
                    positivePeaks[static_cast<size_t> (echo - 1)].index
                    - centre)
                    <= tolerance,
                "positive comb echo " + std::to_string (echo)
                    + " timing error");
        }

        const auto negativeAlternates =
            negativePeaks[0].value * negativePeaks[1].value < 0.0f
            && negativePeaks[1].value * negativePeaks[2].value < 0.0f;
        test.expect (negativeAlternates,
                     "negative feedback comb does not alternate polarity");

        const auto resonant = toneMagnitude (
            positive, 0, 0, positive.getNumSamples(),
            tunedFrequency, testSampleRate, false);
        const auto betweenModes = toneMagnitude (
            positive, 0, 0, positive.getNumSamples(),
            tunedFrequency * 1.5, testSampleRate, false);
        const auto resonanceDb = gainToDecibels (
            (resonant + 1.0e-12) / (betweenModes + 1.0e-12));
        test.expect (resonanceDb >= 3.0,
                     "tuned comb resonance contrast is only "
                         + number (resonanceDb, 2) + " dB");
        test.expect (measureSignal (positive).finite
                         && measureSignal (negative).finite,
                     "comb output contains NaN or Inf");
        test.report (
            "tunedHz=" + number (tunedFrequency, 3)
            + " periodSamples=" + number (period, 3)
            + " measuredFirst="
            + std::to_string (positivePeaks[0].index)
            + " resonanceContrast=" + number (resonanceDb, 2) + " dB"
            + " negativeSigns=" + number (negativePeaks[0].value, 4)
            + "/" + number (negativePeaks[1].value, 4)
            + "/" + number (negativePeaks[2].value, 4));
    });
}

void testFrequencyShift (TestRunner& runner)
{
    runner.run ("frequency shift +/-100 Hz direction and 30 dB mirror rejection",
                [] (TestCase& test)
    {
        constexpr double sourceFrequency = 3000.0;
        constexpr double shift = 100.0;
        const std::array<double, 3> sampleRates {
            48000.0, 96000.0, 192000.0
        };

        for (const auto sampleRate : sampleRates)
        {
            const auto totalSamples = static_cast<int> (
                1.20 * sampleRate);
            const auto activeSamples = static_cast<int> (
                1.00 * sampleRate);
            const auto input = makeSine (
                1, totalSamples, activeSamples, sampleRate,
                sourceFrequency, 0.10f);

            const auto renderShift = [&] (float signedShift)
            {
                Processor processor;
                configureNeutral (processor);
                configureChannels (processor, 1);
                setPlainValue (
                    processor, TBDelayParameterIDs::time, 50.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::transformRoute, 1.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::shift, signedShift);
                return processBuffer (
                    processor, input, sampleRate, 257);
            };

            const auto up = renderShift (static_cast<float> (shift));
            const auto down = renderShift (static_cast<float> (-shift));
            const auto analysisStart = static_cast<int> (
                0.25 * sampleRate);
            const auto analysisSamples = static_cast<int> (
                0.50 * sampleRate);

            const auto upDesired = toneMagnitude (
                up, 0, analysisStart, analysisSamples,
                sourceFrequency + shift, sampleRate);
            const auto upMirror = toneMagnitude (
                up, 0, analysisStart, analysisSamples,
                sourceFrequency - shift, sampleRate);
            const auto downDesired = toneMagnitude (
                down, 0, analysisStart, analysisSamples,
                sourceFrequency - shift, sampleRate);
            const auto downMirror = toneMagnitude (
                down, 0, analysisStart, analysisSamples,
                sourceFrequency + shift, sampleRate);
            const auto upRejection = gainToDecibels (
                (upDesired + 1.0e-12) / (upMirror + 1.0e-12));
            const auto downRejection = gainToDecibels (
                (downDesired + 1.0e-12) / (downMirror + 1.0e-12));

            test.expect (
                upDesired > upMirror,
                number (sampleRate / 1000.0, 0)
                    + " kHz positive Shift favours the downward mirror");
            test.expect (
                downDesired > downMirror,
                number (sampleRate / 1000.0, 0)
                    + " kHz negative Shift favours the upward mirror");
            test.expect (
                upDesired >= 0.01 && downDesired >= 0.01,
                number (sampleRate / 1000.0, 0)
                    + " kHz shifted target energy is unexpectedly low"
                    + " (up=" + number (upDesired, 6)
                    + ", down=" + number (downDesired, 6) + ")");
            test.expect (
                upRejection >= 30.0,
                number (sampleRate / 1000.0, 0)
                    + " kHz up mirror rejection="
                    + number (upRejection, 2)
                    + " dB, expected at least 30 dB");
            test.expect (
                downRejection >= 30.0,
                number (sampleRate / 1000.0, 0)
                    + " kHz down mirror rejection="
                    + number (downRejection, 2)
                    + " dB, expected at least 30 dB");
            test.expect (
                measureSignal (up).finite
                    && measureSignal (down).finite,
                number (sampleRate / 1000.0, 0)
                    + " kHz shift output contains NaN or Inf");
            test.report (
                number (sampleRate / 1000.0, 0)
                + " kHz source=3000 targetUp=3100 rejectionUp="
                + number (upRejection, 2)
                + " dB upMag=" + number (upDesired, 6)
                + " targetDown=2900 rejectionDown="
                + number (downRejection, 2)
                + " dB downMag=" + number (downDesired, 6));
        }
    });
}

void testDeterministicGrain (TestRunner& runner)
{
    runner.run ("grain reset is deterministic", [] (TestCase& test)
    {
        const auto totalSamples = static_cast<int> (3.0 * testSampleRate);
        juce::AudioBuffer<float> input (1, totalSamples);
        input.clear();
        std::uint32_t random = 0x1f123bb5u;

        for (int sample = 0;
             sample < static_cast<int> (0.75 * testSampleRate);
             ++sample)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            const auto noise =
                2.0f * static_cast<float> ((random >> 8)
                                          * (1.0 / 16777216.0))
                - 1.0f;
            const auto tone = static_cast<float> (
                std::sin (twoPi * 523.25 * sample / testSampleRate));
            input.setSample (0, sample, 0.055f * noise + 0.035f * tone);
        }

        const auto render = [&] (float grainAmount)
        {
            Processor processor;
            configureNeutral (processor);
            configureChannels (processor, 1);
            setPlainValue (processor, TBDelayParameterIDs::time, 80.0f);
            setPlainValue (processor, TBDelayParameterIDs::feedback, 38.0f);
            setPlainValue (processor, TBDelayParameterIDs::grain, grainAmount);
            setPlainValue (processor, TBDelayParameterIDs::grainSize, 60.0f);
            setPlainValue (processor, TBDelayParameterIDs::density, 5.0f);
            setPlainValue (processor, TBDelayParameterIDs::spray, 70.0f);
            setPlainValue (processor, TBDelayParameterIDs::pitch, 7.0f);
            setPlainValue (processor, TBDelayParameterIDs::reverse, 35.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::transformRoute, 1.0f);
            return processBuffer (
                processor, input, testSampleRate, testBlockSize);
        };

        const auto first = render (100.0f);
        const auto second = render (100.0f);
        const auto noGrain = render (0.0f);
        double deterministicMaximum = 0.0;
        const auto deterministicRms = differenceRms (
            first, second, deterministicMaximum);
        double effectMaximum = 0.0;
        const auto effectRms = differenceRms (
            first, noGrain, effectMaximum);

        test.expect (deterministicMaximum <= 1.0e-6,
                     "identical reset renders differ by "
                         + number (deterministicMaximum, 9));
        test.expect (effectRms >= 1.0e-4,
                     "grain render is indistinguishable from grain-off; "
                         "difference RMS=" + number (effectRms, 9));
        test.expect (measureSignal (first).finite
                         && measureSignal (second).finite,
                     "grain output contains NaN or Inf");
        test.report (
            "repeatMaxDiff=" + number (deterministicMaximum, 9)
            + " repeatDiffRMS=" + number (deterministicRms, 9)
            + " grainVsOffRMS=" + number (effectRms, 7)
            + " grainVsOffMax=" + number (effectMaximum, 7));
    });
}

void testLongGrainHistoryBoundary (TestRunner& runner)
{
    runner.run (
        "long reverse grain stays inside the 8-second history",
        [] (TestCase& test)
    {
        constexpr int blockSize = 512;
        const auto fillSamples =
            static_cast<int> (8.0 * testSampleRate);
        const auto markerStart =
            fillSamples - static_cast<int> (0.5 * testSampleRate);
        const auto observeSamples =
            static_cast<int> (0.55 * testSampleRate);

        Processor processor;
        configureNeutral (processor);
        configureChannels (processor, 1);
        setPlainValue (processor, TBDelayParameterIDs::time, 8000.0f);
        setPlainValue (processor, TBDelayParameterIDs::feedback, 0.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        juce::MidiBuffer midi;

        for (int offset = 0; offset < fillSamples; offset += blockSize)
        {
            const auto count =
                std::min (blockSize, fillSamples - offset);
            juce::AudioBuffer<float> block (1, count);
            block.clear();
            for (int sample = 0; sample < count; ++sample)
                if (offset + sample >= markerStart)
                    block.setSample (0, sample, 0.20f);
            midi.clear();
            processor.processBlock (block, midi);
        }

        setPlainValue (processor, TBDelayParameterIDs::grain, 100.0f);
        setPlainValue (processor, TBDelayParameterIDs::grainSize, 500.0f);
        setPlainValue (processor, TBDelayParameterIDs::density, 1.0f);
        setPlainValue (processor, TBDelayParameterIDs::spray, 0.0f);
        setPlainValue (processor, TBDelayParameterIDs::pitch, 0.0f);
        setPlainValue (processor, TBDelayParameterIDs::reverse, 100.0f);

        double peak = 0.0;
        auto finite = true;
        for (int offset = 0; offset < observeSamples; offset += blockSize)
        {
            const auto count =
                std::min (blockSize, observeSamples - offset);
            juce::AudioBuffer<float> block (1, count);
            block.clear();
            midi.clear();
            processor.processBlock (block, midi);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = static_cast<double> (
                    block.getSample (0, sample));
                finite = finite && std::isfinite (value);
                peak = std::max (peak, std::abs (value));
            }
        }

        processor.releaseResources();
        test.expect (finite, "long reverse grain produced NaN/Inf");
        test.expect (
            peak <= 1.0e-5,
            "long reverse grain wrapped into recent marker; peak="
                + number (peak, 8));
        test.report (
            "recentMarker=0.20 observedPeak=" + number (peak, 9));
    });
}

struct StabilityResult
{
    bool finite = true;
    double peak = 0.0;
    double previousRms = 0.0;
    double lastRms = 0.0;
    double growthDb = 0.0;
    double lastMean = 0.0;
};

StabilityResult runStabilityCorner (
    const std::vector<std::pair<const char*, float>>& overrides)
{
    constexpr int seconds = 20;
    constexpr int blockSize = 512;
    const auto totalSamples = static_cast<int> (seconds * testSampleRate);
    Processor processor;
    configureNeutral (processor);
    configureChannels (processor, 2);

    for (const auto& [id, value] : overrides)
        setPlainValue (processor, id, value);

    processor.prepareToPlay (testSampleRate, blockSize);
    juce::MidiBuffer midi;
    std::array<double, seconds> energy {};
    std::array<double, seconds> sums {};
    std::array<std::uint64_t, seconds> counts {};
    StabilityResult result;

    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const auto samples = std::min (blockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer (2, samples);
        buffer.clear();
        if (offset == 0)
        {
            buffer.setSample (0, 0, 0.125f);
            buffer.setSample (1, 0, 0.125f);
        }

        midi.clear();
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < samples; ++sample)
            {
                const auto value = static_cast<double> (
                    buffer.getSample (channel, sample));
                if (! std::isfinite (value))
                {
                    result.finite = false;
                    continue;
                }

                result.peak = std::max (result.peak, std::abs (value));
                const auto second = juce::jlimit (
                    0, seconds - 1,
                    static_cast<int> (
                        (offset + sample) / testSampleRate));
                energy[static_cast<size_t> (second)] += value * value;
                sums[static_cast<size_t> (second)] += value;
                ++counts[static_cast<size_t> (second)];
            }
    }

    processor.releaseResources();
    const auto rmsAt = [&] (int second)
    {
        return counts[static_cast<size_t> (second)] > 0
                   ? std::sqrt (
                         energy[static_cast<size_t> (second)]
                         / static_cast<double> (
                             counts[static_cast<size_t> (second)]))
                   : 0.0;
    };

    result.previousRms = rmsAt (seconds - 2);
    result.lastRms = rmsAt (seconds - 1);
    result.growthDb =
        std::max (result.previousRms, result.lastRms) < 1.0e-7
            ? 0.0
            : gainToDecibels (
                  (result.lastRms + 1.0e-12)
                  / (result.previousRms + 1.0e-12));
    if (counts.back() > 0)
        result.lastMean =
            sums.back() / static_cast<double> (counts.back());
    return result;
}

void testFeedbackStability (TestRunner& runner)
{
    runner.run ("20-second finite and bounded feedback corners", [] (TestCase& test)
    {
        struct Corner
        {
            const char* name;
            std::vector<std::pair<const char*, float>> values;
        };

        const std::array<Corner, 4> corners {{
            {
                "minimum negative comb",
                {
                    { TBDelayParameterIDs::time, 10.0f },
                    { TBDelayParameterIDs::timeMode, 2.0f },
                    { TBDelayParameterIDs::feedback, 110.0f },
                    { TBDelayParameterIDs::offset, -100.0f }
                }
            },
            {
                "maximum tape ping-pong",
                {
                    { TBDelayParameterIDs::time, 45.0f },
                    { TBDelayParameterIDs::style, 2.0f },
                    { TBDelayParameterIDs::feedback, 110.0f },
                    { TBDelayParameterIDs::character, 1.0f },
                    { TBDelayParameterIDs::drive, 100.0f },
                    { TBDelayParameterIDs::age, 100.0f },
                    { TBDelayParameterIDs::modDepth, 20.0f }
                }
            },
            {
                "opposed stereo shift",
                {
                    { TBDelayParameterIDs::time, 70.0f },
                    { TBDelayParameterIDs::style, 1.0f },
                    { TBDelayParameterIDs::feedback, 107.0f },
                    { TBDelayParameterIDs::shift, 1800.0f },
                    { TBDelayParameterIDs::diverge, 100.0f },
                    { TBDelayParameterIDs::smear, 70.0f },
                    { TBDelayParameterIDs::size, 80.0f }
                }
            },
            {
                "grain diffusion composite",
                {
                    { TBDelayParameterIDs::time, 80.0f },
                    { TBDelayParameterIDs::style, 1.0f },
                    { TBDelayParameterIDs::feedback, 105.0f },
                    { TBDelayParameterIDs::grain, 100.0f },
                    { TBDelayParameterIDs::grainSize, 500.0f },
                    { TBDelayParameterIDs::density, 8.0f },
                    { TBDelayParameterIDs::spray, 100.0f },
                    { TBDelayParameterIDs::pitch, 24.0f },
                    { TBDelayParameterIDs::reverse, 100.0f },
                    { TBDelayParameterIDs::smear, 100.0f },
                    { TBDelayParameterIDs::size, 120.0f },
                    { TBDelayParameterIDs::shift, -800.0f }
                }
            }
        }};

        for (const auto& corner : corners)
        {
            const auto result = runStabilityCorner (corner.values);
            test.expect (result.finite,
                         std::string (corner.name) + " produced NaN/Inf");
            test.expect (
                result.peak <= 8.0,
                std::string (corner.name) + " peak="
                    + number (result.peak, 4)
                    + " exceeds conservative 8.0 safety bound");
            test.expect (
                result.growthDb <= 6.0,
                std::string (corner.name)
                    + " final one-second RMS growth="
                    + number (result.growthDb, 2) + " dB");
            test.expect (
                std::abs (result.lastMean) <= 0.1,
                std::string (corner.name) + " final DC mean="
                    + number (result.lastMean, 6));
            test.report (
                std::string (corner.name)
                + ": peak=" + number (result.peak, 5)
                + " prevRMS=" + number (result.previousRms, 7)
                + " lastRMS=" + number (result.lastRms, 7)
                + " growth=" + number (result.growthDb, 2)
                + " dB mean=" + number (result.lastMean, 7));
        }
    });

    runner.run (
        "runaway feedback decays once the input stops",
        [] (TestCase& test)
    {
        constexpr int blockSize = 64;
        constexpr double driveSeconds = 2.0;
        constexpr double silenceSeconds = 13.0;

        Processor processor;
        configureNeutral (processor);
        configureChannels (processor, 1);
        setPlainValue (processor, TBDelayParameterIDs::time, 200.0f);
        setPlainValue (processor, TBDelayParameterIDs::timeMode, 0.0f);
        setPlainValue (processor, TBDelayParameterIDs::feedback, 110.0f);
        setPlainValue (processor, TBDelayParameterIDs::mix, 100.0f);
        processor.prepareToPlay (testSampleRate, blockSize);

        const auto driveSamples = static_cast<int> (
            std::round (driveSeconds * testSampleRate));
        const auto totalSamples = static_cast<int> (
            std::round ((driveSeconds + silenceSeconds) * testSampleRate));
        const auto windowSamples = static_cast<int> (
            std::round (testSampleRate));
        const auto earlyStart = driveSamples + windowSamples;
        const auto lateStart = totalSamples - windowSamples;

        juce::MidiBuffer midi;
        auto finite = true;
        double earlyEnergy = 0.0;
        double lateEnergy = 0.0;
        std::uint64_t earlyCount = 0;
        std::uint64_t lateCount = 0;
        double peak = 0.0;

        for (int offset = 0; offset < totalSamples; offset += blockSize)
        {
            const auto count = std::min (blockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (1, count);
            block.clear();
            if (offset < driveSamples)
                for (int sample = 0; sample < count; ++sample)
                {
                    const auto absoluteSample = offset + sample;
                    block.setSample (
                        0, sample,
                        static_cast<float> (
                            0.25 * std::sin (
                                       twoPi * 180.0 * absoluteSample
                                       / testSampleRate)));
                }

            midi.clear();
            processor.processBlock (block, midi);

            for (int sample = 0; sample < count; ++sample)
            {
                const auto absoluteSample = offset + sample;
                const auto value =
                    static_cast<double> (block.getSample (0, sample));
                if (! std::isfinite (value))
                    finite = false;

                peak = std::max (peak, std::abs (value));

                if (absoluteSample >= earlyStart
                    && absoluteSample < earlyStart + windowSamples)
                {
                    earlyEnergy += value * value;
                    ++earlyCount;
                }
                else if (absoluteSample >= lateStart)
                {
                    lateEnergy += value * value;
                    ++lateCount;
                }
            }
        }

        const auto earlyRms =
            earlyCount > 0
                ? std::sqrt (earlyEnergy / static_cast<double> (earlyCount))
                : 0.0;
        const auto lateRms =
            lateCount > 0
                ? std::sqrt (lateEnergy / static_cast<double> (lateCount))
                : 0.0;
        const auto decayDb =
            earlyRms > 1.0e-9
                ? 20.0 * std::log10 (
                             std::max (lateRms, 1.0e-12) / earlyRms)
                : 0.0;

        test.expect (finite, "silent-input runaway produced NaN/Inf");
        test.expect (
            earlyRms > 1.0e-5,
            "no tail was present to observe after the input stopped");
        test.expect (
            decayDb <= -3.0,
            std::string ("110% feedback did not decay with no input: ")
                + number (decayDb, 2) + " dB over "
                + number (silenceSeconds - 2.0, 1) + " s");
        test.report (
            std::string ("earlyRMS=") + number (earlyRms, 7)
            + " lateRMS=" + number (lateRms, 7)
            + " decay=" + number (decayDb, 2)
            + " dB peak=" + number (peak, 5));
    });
}

void testFreezeHold (TestRunner& runner)
{
    runner.run ("Freeze holds an existing tail at 10/100/500 ms",
                [] (TestCase& test)
    {
        constexpr int blockSize = 64;
        constexpr double holdSeconds = 4.0;
        const std::array<double, 3> delayTimesMs {
            10.0, 100.0, 500.0
        };

        for (const auto delayMs : delayTimesMs)
        {
            Processor processor;
            configureNeutral (processor);
            configureChannels (processor, 1);
            setPlainValue (
                processor, TBDelayParameterIDs::time,
                static_cast<float> (delayMs));
            setPlainValue (
                processor, TBDelayParameterIDs::feedback, 0.0f);
            processor.prepareToPlay (testSampleRate, blockSize);

            const auto prefillSamples = static_cast<int> (
                std::round (
                    std::max (0.75, 3.0 * delayMs / 1000.0)
                    * testSampleRate));
            const auto holdSamples = static_cast<int> (
                std::round (holdSeconds * testSampleRate));
            const auto totalSamples = prefillSamples + holdSamples;
            const auto earlyStart = static_cast<int> (
                std::round (
                    std::max (0.25, 2.0 * delayMs / 1000.0)
                    * testSampleRate));
            const auto windowSamples = static_cast<int> (
                std::round (0.5 * testSampleRate));
            const auto prefillWindowStart =
                std::max (0, prefillSamples - windowSamples);
            const auto lateStart = static_cast<int> (
                std::round (3.25 * testSampleRate));

            juce::MidiBuffer midi;
            auto freezeEnabled = false;
            auto freezeSample = -1;
            auto finite = true;
            double prefillEnergy = 0.0;
            double earlyEnergy = 0.0;
            double lateEnergy = 0.0;
            std::uint64_t prefillCount = 0;
            std::uint64_t earlyCount = 0;
            std::uint64_t lateCount = 0;

            for (int offset = 0; offset < totalSamples; offset += blockSize)
            {
                if (! freezeEnabled && offset >= prefillSamples)
                {
                    setPlainValue (
                        processor, TBDelayParameterIDs::freeze, 1.0f);
                    freezeEnabled = true;
                    freezeSample = offset;
                }

                const auto count =
                    std::min (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, count);
                block.clear();
                if (! freezeEnabled)
                    for (int sample = 0; sample < count; ++sample)
                    {
                        const auto absoluteSample = offset + sample;
                        block.setSample (
                            0, sample,
                            static_cast<float> (
                                0.10 * std::sin (
                                           twoPi * 223.0
                                           * absoluteSample
                                           / testSampleRate)));
                    }

                midi.clear();
                processor.processBlock (block, midi);

                if (freezeSample < 0)
                {
                    for (int sample = 0; sample < count; ++sample)
                    {
                        const auto absoluteSample = offset + sample;
                        if (absoluteSample >= prefillWindowStart
                            && absoluteSample < prefillSamples)
                        {
                            const auto value = static_cast<double> (
                                block.getSample (0, sample));
                            prefillEnergy += value * value;
                            ++prefillCount;
                        }
                    }
                    continue;
                }

                for (int sample = 0; sample < count; ++sample)
                {
                    const auto value = static_cast<double> (
                        block.getSample (0, sample));
                    finite = finite && std::isfinite (value);
                    const auto heldSample =
                        offset + sample - freezeSample;
                    if (heldSample >= earlyStart
                        && heldSample < earlyStart + windowSamples)
                    {
                        earlyEnergy += value * value;
                        ++earlyCount;
                    }
                    if (heldSample >= lateStart
                        && heldSample < lateStart + windowSamples)
                    {
                        lateEnergy += value * value;
                        ++lateCount;
                    }
                }
            }

            processor.releaseResources();
            const auto prefillRms =
                prefillCount > 0
                    ? std::sqrt (
                          prefillEnergy
                          / static_cast<double> (prefillCount))
                    : 0.0;
            const auto earlyRms =
                earlyCount > 0
                    ? std::sqrt (
                          earlyEnergy
                          / static_cast<double> (earlyCount))
                    : 0.0;
            const auto lateRms =
                lateCount > 0
                    ? std::sqrt (
                          lateEnergy
                          / static_cast<double> (lateCount))
                    : 0.0;
            const auto driftDb = gainToDecibels (
                (lateRms + 1.0e-12) / (earlyRms + 1.0e-12));
            const auto captureDb = gainToDecibels (
                (earlyRms + 1.0e-12) / (prefillRms + 1.0e-12));

            test.expect (
                finite,
                "Freeze produced NaN/Inf at "
                    + number (delayMs, 0) + " ms");
            test.expect (
                earlyRms >= 1.0e-4,
                "Freeze tail is silent at "
                    + number (delayMs, 0) + " ms");
            test.expect (
                std::abs (captureDb) <= 3.0,
                "Freeze capture changed the existing tail by "
                    + number (captureDb, 2) + " dB at "
                    + number (delayMs, 0) + " ms");
            test.expect (
                std::abs (driftDb) <= 1.5,
                "Freeze level drift at "
                    + number (delayMs, 0) + " ms is "
                    + number (driftDb, 2) + " dB");
            test.report (
                "time=" + number (delayMs, 0)
                + "ms preRMS=" + number (prefillRms, 7)
                + " earlyRMS=" + number (earlyRms, 7)
                + " lateRMS=" + number (lateRms, 7)
                + " capture=" + number (captureDb, 2)
                + " dB drift=" + number (driftDb, 2) + " dB");
        }

        for (const auto delayMs : delayTimesMs)
        {
            Processor coldStart;
            configureNeutral (coldStart);
            configureChannels (coldStart, 1);
            setPlainValue (
                coldStart, TBDelayParameterIDs::time,
                static_cast<float> (delayMs));
            setPlainValue (
                coldStart, TBDelayParameterIDs::freeze, 1.0f);
            coldStart.prepareToPlay (testSampleRate, blockSize);

            const auto silenceSamples =
                static_cast<int> (0.10 * testSampleRate);
            const auto sourceSamples =
                static_cast<int> (0.60 * testSampleRate);
            const auto observeSamples =
                static_cast<int> (1.50 * testSampleRate);
            const auto coldTotal =
                silenceSamples + sourceSamples + observeSamples;
            const auto coldMeasureStart =
                coldTotal - static_cast<int> (0.25 * testSampleRate);
            double coldEnergy = 0.0;
            std::uint64_t coldCount = 0;
            juce::MidiBuffer coldMidi;

            for (int offset = 0; offset < coldTotal; offset += blockSize)
            {
                const auto count =
                    std::min (blockSize, coldTotal - offset);
                juce::AudioBuffer<float> block (1, count);
                block.clear();
                for (int sample = 0; sample < count; ++sample)
                {
                    const auto absoluteSample = offset + sample;
                    if (absoluteSample >= silenceSamples
                        && absoluteSample
                               < silenceSamples + sourceSamples)
                    {
                        block.setSample (
                            0, sample,
                            static_cast<float> (
                                0.10 * std::sin (
                                           twoPi * 223.0
                                           * absoluteSample
                                           / testSampleRate)));
                    }
                }

                coldMidi.clear();
                coldStart.processBlock (block, coldMidi);
                for (int sample = 0; sample < count; ++sample)
                    if (offset + sample >= coldMeasureStart)
                    {
                        const auto value = static_cast<double> (
                            block.getSample (0, sample));
                        coldEnergy += value * value;
                        ++coldCount;
                    }
            }

            coldStart.releaseResources();
            const auto coldRms =
                coldCount > 0
                    ? std::sqrt (
                          coldEnergy
                          / static_cast<double> (coldCount))
                    : 0.0;
            const auto coldCaptureDb = gainToDecibels (
                (coldRms + 1.0e-12) / (0.10 / std::sqrt (2.0)));
            test.expect (
                coldRms >= 1.0e-4
                    && std::abs (coldCaptureDb) <= 3.0,
                "cold-start Freeze capture at "
                    + number (delayMs, 0) + " ms is "
                    + number (coldCaptureDb, 2) + " dB");
            test.report (
                "coldStart=" + number (delayMs, 0)
                + "ms RMS=" + number (coldRms, 7)
                + " capture=" + number (coldCaptureDb, 2) + " dB");
        }

        for (const auto delayMs : delayTimesMs)
        {
            Processor transient;
            configureNeutral (transient);
            configureChannels (transient, 1);
            setPlainValue (
                transient, TBDelayParameterIDs::time,
                static_cast<float> (delayMs));
            setPlainValue (
                transient, TBDelayParameterIDs::freeze, 1.0f);
            transient.prepareToPlay (testSampleRate, blockSize);

            const auto totalSamples =
                static_cast<int> (4.0 * testSampleRate);
            const auto earlyStart =
                static_cast<int> (0.75 * testSampleRate);
            const auto earlyEnd =
                static_cast<int> (1.75 * testSampleRate);
            const auto lateStart =
                static_cast<int> (2.75 * testSampleRate);
            const auto lateEnd =
                static_cast<int> (3.75 * testSampleRate);
            double earlyEnergy = 0.0;
            double lateEnergy = 0.0;
            std::uint64_t earlyCount = 0;
            std::uint64_t lateCount = 0;
            auto finite = true;
            juce::MidiBuffer midi;

            for (int offset = 0; offset < totalSamples;
                 offset += blockSize)
            {
                const auto count =
                    std::min (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, count);
                block.clear();
                if (offset == 0)
                    block.setSample (0, 0, 0.25f);
                transient.processBlock (block, midi);
                for (int sample = 0; sample < count; ++sample)
                {
                    const auto absolute = offset + sample;
                    const auto value = static_cast<double> (
                        block.getSample (0, sample));
                    finite = finite && std::isfinite (value);
                    if (absolute >= earlyStart && absolute < earlyEnd)
                    {
                        earlyEnergy += value * value;
                        ++earlyCount;
                    }
                    if (absolute >= lateStart && absolute < lateEnd)
                    {
                        lateEnergy += value * value;
                        ++lateCount;
                    }
                }
            }
            transient.releaseResources();
            const auto earlyRms = std::sqrt (
                earlyEnergy
                / static_cast<double> (std::max<std::uint64_t> (
                    1, earlyCount)));
            const auto lateRms = std::sqrt (
                lateEnergy
                / static_cast<double> (std::max<std::uint64_t> (
                    1, lateCount)));
            const auto driftDb = gainToDecibels (
                (lateRms + 1.0e-12) / (earlyRms + 1.0e-12));
            test.expect (
                finite && earlyRms > 1.0e-5 && lateRms > 1.0e-5,
                "cold-start Freeze lost an isolated transient at "
                    + number (delayMs, 0) + " ms");
            test.expect (
                std::abs (driftDb) <= 1.5,
                "isolated Freeze transient drifted by "
                    + number (driftDb, 2) + " dB at "
                    + number (delayMs, 0) + " ms");
            test.report (
                "impulseColdStart=" + number (delayMs, 0)
                + "ms earlyRMS=" + number (earlyRms, 8)
                + " lateRMS=" + number (lateRms, 8)
                + " drift=" + number (driftDb, 2) + " dB");
        }

        for (const auto delayMs : delayTimesMs)
        {
            Processor diffused;
            configureNeutral (diffused);
            configureChannels (diffused, 1);
            setPlainValue (
                diffused, TBDelayParameterIDs::time,
                static_cast<float> (delayMs));
            setPlainValue (
                diffused, TBDelayParameterIDs::smear, 100.0f);
            setPlainValue (
                diffused, TBDelayParameterIDs::size, 120.0f);
            diffused.prepareToPlay (testSampleRate, blockSize);

            const auto prefillSamples =
                static_cast<int> (1.0 * testSampleRate);
            const auto holdSamples =
                static_cast<int> (4.0 * testSampleRate);
            const auto totalSamples = prefillSamples + holdSamples;
            const auto earlyStart = prefillSamples
                + static_cast<int> (0.75 * testSampleRate);
            const auto earlyEnd = prefillSamples
                + static_cast<int> (1.25 * testSampleRate);
            const auto lateStart = prefillSamples
                + static_cast<int> (3.0 * testSampleRate);
            const auto lateEnd = prefillSamples
                + static_cast<int> (3.5 * testSampleRate);
            double earlyEnergy = 0.0;
            double lateEnergy = 0.0;
            std::uint64_t earlyCount = 0;
            std::uint64_t lateCount = 0;
            juce::MidiBuffer midi;

            for (int offset = 0; offset < totalSamples;
                 offset += blockSize)
            {
                if (offset == prefillSamples)
                    setPlainValue (
                        diffused, TBDelayParameterIDs::freeze, 1.0f);
                const auto count =
                    std::min (blockSize, totalSamples - offset);
                juce::AudioBuffer<float> block (1, count);
                block.clear();
                if (offset < prefillSamples)
                    for (int sample = 0; sample < count; ++sample)
                        block.setSample (
                            0, sample,
                            static_cast<float> (
                                0.10 * std::sin (
                                    twoPi * 223.0 * (offset + sample)
                                    / testSampleRate)));
                diffused.processBlock (block, midi);
                for (int sample = 0; sample < count; ++sample)
                {
                    const auto absolute = offset + sample;
                    const auto value = static_cast<double> (
                        block.getSample (0, sample));
                    if (absolute >= earlyStart && absolute < earlyEnd)
                    {
                        earlyEnergy += value * value;
                        ++earlyCount;
                    }
                    if (absolute >= lateStart && absolute < lateEnd)
                    {
                        lateEnergy += value * value;
                        ++lateCount;
                    }
                }
            }
            diffused.releaseResources();
            const auto earlyRms = std::sqrt (
                earlyEnergy
                / static_cast<double> (std::max<std::uint64_t> (
                    1, earlyCount)));
            const auto lateRms = std::sqrt (
                lateEnergy
                / static_cast<double> (std::max<std::uint64_t> (
                    1, lateCount)));
            const auto driftDb = gainToDecibels (
                (lateRms + 1.0e-12) / (earlyRms + 1.0e-12));
            test.expect (
                earlyRms > 1.0e-5 && lateRms > 1.0e-5
                    && std::abs (driftDb) <= 1.5,
                "Freeze repeatedly re-diffused or lost a SMEAR=100 tail at "
                    + number (delayMs, 0) + " ms ("
                    + number (driftDb, 2) + " dB)");
            test.report (
                "diffusedHold=" + number (delayMs, 0)
                + "ms earlyRMS=" + number (earlyRms, 7)
                + " lateRMS=" + number (lateRms, 7)
                + " drift=" + number (driftDb, 2) + " dB");
        }
    });
}

void testModuleBypasses (TestRunner& runner)
{
    runner.run (
        "Stereo, Color/Space, and Grain/Transform bypass neutral equivalence",
        [] (TestCase& test)
    {
        const auto totalSamples =
            juce::roundToInt (1.5 * testSampleRate);
        const auto activeSamples =
            juce::roundToInt (0.45 * testSampleRate);

        const auto renderStereo =
            [totalSamples] (
                bool bypassed,
                int style,
                float offset,
                float width)
        {
            Processor processor;
            configureNeutral (processor);
            setPlainValue (
                processor, TBDelayParameterIDs::time, 65.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::feedback, 68.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::style,
                static_cast<float> (style));
            setPlainValue (
                processor, TBDelayParameterIDs::offset, offset);
            setPlainValue (
                processor, TBDelayParameterIDs::width, width);
            setPlainValue (
                processor, TBDelayParameterIDs::stereoBypass,
                bypassed ? 1.0f : 0.0f);
            return processBuffer (
                processor,
                makeImpulse (
                    2, totalSamples, 0.25f, true),
                testSampleRate, testBlockSize);
        };

        const auto stereoReference =
            renderStereo (false, 0, 0.0f, 100.0f);
        const auto stereoBypassed =
            renderStereo (true, 2, 83.0f, 18.0f);
        const auto stereoActive =
            renderStereo (false, 2, 83.0f, 18.0f);
        double stereoBypassMaximum = 0.0;
        double stereoActiveMaximum = 0.0;
        const auto stereoBypassDifference = differenceRms (
            stereoReference, stereoBypassed,
            stereoBypassMaximum);
        const auto stereoActiveDifference = differenceRms (
            stereoReference, stereoActive,
            stereoActiveMaximum);

        const auto renderColor =
            [totalSamples, activeSamples] (
                bool bypassed,
                bool aggressive)
        {
            Processor processor;
            configureNeutral (processor);
            setPlainValue (
                processor, TBDelayParameterIDs::time, 70.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::feedback, 62.0f);
            if (aggressive)
            {
                setPlainValue (
                    processor, TBDelayParameterIDs::character, 3.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::lowCut, 1100.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::highCut, 1800.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::drive, 100.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::age, 100.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::modRate, 6.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::modDepth, 15.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::smear, 85.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::size, 90.0f);
            }
            setPlainValue (
                processor, TBDelayParameterIDs::colorSpaceBypass,
                bypassed ? 1.0f : 0.0f);
            return processBuffer (
                processor,
                makeSine (
                    2, totalSamples, activeSamples,
                    testSampleRate, 997.0, 0.18f),
                testSampleRate, testBlockSize);
        };

        const auto colorReference =
            renderColor (true, false);
        const auto colorBypassed =
            renderColor (true, true);
        const auto colorActive =
            renderColor (false, true);
        double colorBypassMaximum = 0.0;
        double colorActiveMaximum = 0.0;
        const auto colorBypassDifference = differenceRms (
            colorReference, colorBypassed,
            colorBypassMaximum);
        const auto colorActiveDifference = differenceRms (
            colorReference, colorActive,
            colorActiveMaximum);

        const auto renderTransform =
            [totalSamples, activeSamples] (
                bool bypassed,
                bool aggressive)
        {
            Processor processor;
            configureNeutral (processor);
            setPlainValue (
                processor, TBDelayParameterIDs::time, 80.0f);
            setPlainValue (
                processor, TBDelayParameterIDs::feedback, 58.0f);
            if (aggressive)
            {
                setPlainValue (
                    processor, TBDelayParameterIDs::shift, 420.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::diverge, 100.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::grain, 82.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::grainSize, 180.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::density, 7.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::spray, 75.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::pitch, 12.0f);
                setPlainValue (
                    processor, TBDelayParameterIDs::reverse, 60.0f);
            }
            setPlainValue (
                processor,
                TBDelayParameterIDs::grainTransformBypass,
                bypassed ? 1.0f : 0.0f);
            return processBuffer (
                processor,
                makeSine (
                    2, totalSamples, activeSamples,
                    testSampleRate, 733.0, 0.16f),
                testSampleRate, testBlockSize);
        };

        const auto transformReference =
            renderTransform (true, false);
        const auto transformBypassed =
            renderTransform (true, true);
        const auto transformActive =
            renderTransform (false, true);
        double transformBypassMaximum = 0.0;
        double transformActiveMaximum = 0.0;
        const auto transformBypassDifference = differenceRms (
            transformReference, transformBypassed,
            transformBypassMaximum);
        const auto transformActiveDifference = differenceRms (
            transformReference, transformActive,
            transformActiveMaximum);

        Processor freezeProcessor;
        configureNeutral (freezeProcessor);
        setPlainValue (
            freezeProcessor, TBDelayParameterIDs::time, 50.0f);
        setPlainValue (
            freezeProcessor, TBDelayParameterIDs::feedback, 55.0f);
        setPlainValue (
            freezeProcessor,
            TBDelayParameterIDs::grainTransformBypass, 1.0f);
        setPlainValue (
            freezeProcessor, TBDelayParameterIDs::freeze, 1.0f);
        const auto freezeOutput = processBuffer (
            freezeProcessor,
            makeSine (
                2, juce::roundToInt (1.2 * testSampleRate),
                juce::roundToInt (0.3 * testSampleRate),
                testSampleRate, 440.0, 0.12f),
            testSampleRate, testBlockSize);
        const auto heldSignal = measureSignal (
            freezeOutput,
            juce::roundToInt (0.9 * testSampleRate),
            juce::roundToInt (0.2 * testSampleRate));

        test.expect (
            stereoBypassDifference <= 1.0e-7
                && stereoBypassMaximum <= 1.0e-6,
            "Stereo bypass differs from Single/0%/100% neutral path");
        test.expect (
            colorBypassDifference <= 1.0e-7
                && colorBypassMaximum <= 1.0e-6,
            "Color/Space bypass still depends on section controls");
        test.expect (
            transformBypassDifference <= 1.0e-7
                && transformBypassMaximum <= 1.0e-6,
            "Grain/Transform bypass still depends on section controls");
        test.expect (
            stereoActiveDifference > 1.0e-4
                && colorActiveDifference > 1.0e-4
                && transformActiveDifference > 1.0e-4,
            "an active section is indistinguishable from its bypass");
        test.expect (
            heldSignal.finite && heldSignal.rms > 0.005,
            "Grain/Transform bypass disabled independent Freeze hold");
        test.report (
            "bypassDiffRMS stereo="
            + number (stereoBypassDifference, 9)
            + " color=" + number (colorBypassDifference, 9)
            + " transform="
            + number (transformBypassDifference, 9)
            + " activeDiffRMS="
            + number (stereoActiveDifference, 6) + "/"
            + number (colorActiveDifference, 6) + "/"
            + number (transformActiveDifference, 6)
            + " freezeHeldRMS="
            + number (heldSignal.rms, 7));
    });

    runner.run (
        "module bypass automation is finite and click-bounded",
        [] (TestCase& test)
    {
        Processor processor;
        configureNeutral (processor);
        setPlainValue (
            processor, TBDelayParameterIDs::time, 55.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::feedback, 72.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::style, 2.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::offset, 75.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::width, 180.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::character, 2.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::drive, 80.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::age, 75.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::modDepth, 8.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::smear, 65.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::shift, 240.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::grain, 55.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::pitch, 7.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::spray, 45.0f);

        processor.prepareToPlay (
            testSampleRate, testBlockSize);
        const auto totalSamples =
            juce::roundToInt (1.8 * testSampleRate);
        const auto bypassOnSample =
            juce::roundToInt (0.55 * testSampleRate);
        const auto bypassOffSample =
            juce::roundToInt (1.10 * testSampleRate);
        juce::MidiBuffer midi;
        auto bypassed = false;
        auto restored = false;
        auto finite = true;
        auto peak = 0.0;
        auto maximumStep = 0.0;
        std::array<float, 2> previous {};
        auto hasPrevious = false;

        for (int offset = 0;
             offset < totalSamples;
             offset += testBlockSize)
        {
            if (! bypassed && offset >= bypassOnSample)
            {
                for (const auto* id : {
                         TBDelayParameterIDs::stereoBypass,
                         TBDelayParameterIDs::colorSpaceBypass,
                         TBDelayParameterIDs::grainTransformBypass })
                    setPlainValue (processor, id, 1.0f);
                bypassed = true;
            }
            if (! restored && offset >= bypassOffSample)
            {
                for (const auto* id : {
                         TBDelayParameterIDs::stereoBypass,
                         TBDelayParameterIDs::colorSpaceBypass,
                         TBDelayParameterIDs::grainTransformBypass })
                    setPlainValue (processor, id, 0.0f);
                restored = true;
            }

            const auto blockSamples =
                std::min (
                    testBlockSize, totalSamples - offset);
            juce::AudioBuffer<float> block (2, blockSamples);
            for (int sample = 0;
                 sample < blockSamples;
                 ++sample)
            {
                const auto absoluteSample = offset + sample;
                const auto input = static_cast<float> (
                    0.09
                    * std::sin (
                        twoPi * 523.25
                        * absoluteSample / testSampleRate));
                block.setSample (0, sample, input);
                block.setSample (1, sample, input * 0.73f);
            }

            processor.processBlock (block, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0;
                     sample < blockSamples;
                     ++sample)
                {
                    const auto value =
                        block.getSample (channel, sample);
                    finite = finite && std::isfinite (value);
                    peak = std::max (
                        peak,
                        std::abs (static_cast<double> (value)));
                    if (hasPrevious
                        && offset + sample
                               > juce::roundToInt (
                                     0.2 * testSampleRate))
                        maximumStep = std::max (
                            maximumStep,
                            std::abs (
                                static_cast<double> (value)
                                - previous[static_cast<std::size_t> (
                                    channel)]));
                    previous[static_cast<std::size_t> (channel)] =
                        value;
                }
            hasPrevious = true;
        }

        test.expect (
            bypassed && restored && finite,
            "module bypass automation did not complete or produced NaN/Inf");
        test.expect (
            peak < 1.5 && maximumStep < 0.35,
            "module bypass automation produced an excessive transient");
        test.report (
            "peak=" + number (peak, 6)
            + " maxAdjacentStep="
            + number (maximumStep, 6));
    });
}

void testPanicTailInvalidation (TestRunner& runner)
{
    runner.run ("PANIC invalidates all pre-existing delay generations",
                [] (TestCase& test)
    {
        constexpr int blockSize = 512;
        constexpr double fillSeconds = 7.95;
        constexpr double observeSeconds = 1.0;
        const auto fillSamples = static_cast<int> (
            std::round (fillSeconds * testSampleRate));
        const auto observeSamples = static_cast<int> (
            std::round (observeSeconds * testSampleRate));
        Processor processor;
        configureNeutral (processor);
        configureChannels (processor, 1);
        setPlainValue (processor, TBDelayParameterIDs::time, 100.0f);
        setPlainValue (processor, TBDelayParameterIDs::feedback, 0.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        juce::MidiBuffer midi;
        double prePanicSumSquares = 0.0;
        std::uint64_t prePanicCount = 0;

        for (int offset = 0; offset < fillSamples; offset += blockSize)
        {
            const auto samples = std::min (
                blockSize, fillSamples - offset);
            juce::AudioBuffer<float> buffer (1, samples);
            for (int sample = 0; sample < samples; ++sample)
            {
                const auto absoluteSample = offset + sample;
                buffer.setSample (
                    0, sample,
                    static_cast<float> (
                        0.10 * std::sin (
                                   twoPi * 440.0 * absoluteSample
                                   / testSampleRate)));
            }

            midi.clear();
            processor.processBlock (buffer, midi);
            if (offset + samples
                > fillSamples - static_cast<int> (testSampleRate * 0.25))
            {
                for (int sample = 0; sample < samples; ++sample)
                {
                    const auto value = static_cast<double> (
                        buffer.getSample (0, sample));
                    prePanicSumSquares += value * value;
                    ++prePanicCount;
                }
            }
        }

        const auto prePanicRms =
            prePanicCount > 0
                ? std::sqrt (
                      prePanicSumSquares
                      / static_cast<double> (prePanicCount))
                : 0.0;
        processor.panic();

        bool finite = true;
        double postPanicPeak = 0.0;
        double postPanicSumSquares = 0.0;
        std::uint64_t postPanicCount = 0;
        for (int offset = 0; offset < observeSamples; offset += blockSize)
        {
            const auto samples = std::min (
                blockSize, observeSamples - offset);
            juce::AudioBuffer<float> buffer (1, samples);
            buffer.clear();
            midi.clear();
            processor.processBlock (buffer, midi);

            for (int sample = 0; sample < samples; ++sample)
            {
                const auto value = static_cast<double> (
                    buffer.getSample (0, sample));
                finite = finite && std::isfinite (value);
                if (std::isfinite (value))
                {
                    postPanicPeak =
                        std::max (postPanicPeak, std::abs (value));
                    postPanicSumSquares += value * value;
                    ++postPanicCount;
                }
            }
        }

        processor.releaseResources();
        const auto postPanicRms =
            postPanicCount > 0
                ? std::sqrt (
                      postPanicSumSquares
                      / static_cast<double> (postPanicCount))
                : 0.0;

        test.expect (prePanicRms >= 0.01,
                     "pre-PANIC delay memory was not audibly populated; RMS="
                         + number (prePanicRms, 7));
        test.expect (finite, "post-PANIC output contains NaN or Inf");
        test.expect (
            postPanicPeak <= 1.0e-6,
            "old delay data resurfaced after PANIC; peak="
                + number (postPanicPeak, 9));
        test.report (
            "prePanicRMS=" + number (prePanicRms, 7)
            + " postPanicPeak=" + number (postPanicPeak, 9)
            + " postPanicRMS=" + number (postPanicRms, 9)
            + " observeSeconds=" + number (observeSeconds, 2));
    });
}

void testMonoSampleRateBlockMatrix (TestRunner& runner)
{
    runner.run ("mono/stereo sample-rate and block matrix", [] (TestCase& test)
    {
        const std::array<int, 2> channelCounts { 1, 2 };
        const std::array<double, 3> sampleRates { 44100.0, 48000.0, 96000.0 };
        const std::array<int, 5> blockSizes { 1, 17, 64, 257, 1024 };
        auto combinations = 0;
        auto layoutFailures = 0;
        auto finiteFailures = 0;
        auto timingFailures = 0;
        auto maximumTimingError = 0;

        for (const auto channels : channelCounts)
            for (const auto sampleRate : sampleRates)
                for (const auto blockSize : blockSizes)
                {
                    ++combinations;
                    Processor processor;
                    configureNeutral (processor);
                    const auto layoutOk =
                        configureChannels (processor, channels);
                    if (! layoutOk)
                        ++layoutFailures;

                    setPlainValue (
                        processor, TBDelayParameterIDs::time, 20.0f);
                    const auto expected = static_cast<int> (
                        std::round (sampleRate * 0.020));
                    const auto input = makeImpulse (
                        channels, expected + 192, 0.20f);
                    const auto output = processBuffer (
                        processor, input, sampleRate, blockSize);
                    const auto metrics = measureSignal (output);
                    if (! metrics.finite)
                        ++finiteFailures;

                    const auto peak = findPeak (
                        output, 0, std::max (0, expected - 8),
                        std::min (output.getNumSamples(), expected + 9));
                    const auto error = std::abs (peak.index - expected);
                    maximumTimingError =
                        std::max (maximumTimingError, error);
                    if (error > 2)
                        ++timingFailures;
                }

        test.expect (layoutFailures == 0,
                     std::to_string (layoutFailures)
                         + " supported layouts were rejected");
        test.expect (finiteFailures == 0,
                     std::to_string (finiteFailures)
                         + " matrix renders were non-finite");
        test.expect (timingFailures == 0,
                     std::to_string (timingFailures)
                         + " matrix renders exceeded +/-2 samples");
        test.report (
            "combinations=" + std::to_string (combinations)
            + " maxTimingError=" + std::to_string (maximumTimingError)
            + " layoutFailures=" + std::to_string (layoutFailures)
            + " finiteFailures=" + std::to_string (finiteFailures));
    });
}

void testStateRoundTrip (TestRunner& runner)
{
    runner.run ("DAW state round-trip and corrupt-state preservation",
                [] (TestCase& test)
    {
        Processor processor;
        const auto presetIndex =
            findFactoryPresetIndex ("Freeze Ready Reverse Swell");
        test.expect (presetIndex >= 0,
                     "state round-trip source preset is missing");
        if (presetIndex < 0)
            return;
        processor.applyFactoryPreset (presetIndex, false);
        const auto expected = takeSnapshot (processor);

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        test.expect (! saved.isEmpty(), "saved state is empty");

        moveEveryParameterAwayFrom (processor, expected);
        processor.setStateInformation (
            saved.getData(), static_cast<int> (saved.getSize()));
        const auto restored = takeSnapshot (processor);
        double restoreError = 0.0;
        test.expect (
            snapshotsMatch (processor, expected, restored, restoreError),
            "restored state differs; max error="
                + number (restoreError, 8));

        const auto beforeCorrupt = takeSnapshot (processor);
        const auto truncatedSize = std::max<size_t> (1, saved.getSize() / 2);
        juce::MemoryBlock truncated (
            saved.getData(), truncatedSize);
        processor.setStateInformation (
            truncated.getData(), static_cast<int> (truncated.getSize()));
        const auto afterCorrupt = takeSnapshot (processor);
        double corruptError = 0.0;
        test.expect (
            snapshotsMatch (
                processor, beforeCorrupt, afterCorrupt, corruptError),
            "truncated state changed current parameters; max error="
                + number (corruptError, 8));
        test.report (
            "stateBytes=" + std::to_string (saved.getSize())
            + " restoreMaxError=" + number (restoreError, 8)
            + " corruptPreserveMaxError=" + number (corruptError, 8));
    });
}

void testUserPresetRoundTrip (TestRunner& runner,
                              const juce::File& projectRoot)
{
    runner.run ("user preset round-trip and corrupt-file preservation",
                [&projectRoot] (TestCase& test)
    {
        const auto outputDirectory = getOutputDirectory (projectRoot);
        const auto validFile =
            outputDirectory.getChildFile ("roundtrip_test.tbdelay");
        const auto corruptFile =
            outputDirectory.getChildFile ("corrupt_test.tbdelay");
        validFile.deleteFile();
        corruptFile.deleteFile();

        Processor processor;
        const auto presetIndex =
            findFactoryPresetIndex ("Spiral Ascender");
        test.expect (presetIndex >= 0,
                     "user round-trip source preset is missing");
        if (presetIndex < 0)
            return;
        processor.applyFactoryPreset (presetIndex, false);
        const auto expected = takeSnapshot (processor);
        const auto saveResult = processor.saveUserPreset (validFile);
        test.expect (saveResult.wasOk(),
                     "save failed: " + toStdString (
                         saveResult.getErrorMessage()));
        const auto savedPresetXml =
            juce::XmlDocument::parse (validFile);
        test.expect (
            savedPresetXml != nullptr
                && savedPresetXml->getIntAttribute (
                       "presetFormatVersion") == 3
                && savedPresetXml->getIntAttribute (
                       "stateSchemaVersion") == 2
                && ! savedPresetXml->hasAttribute ("formatVersion")
                && savedPresetXml->getNumChildElements() == 38,
            "saved user preset does not expose preset v3/state schema 2");

        moveEveryParameterAwayFrom (processor, expected);
        const auto loadResult = processor.loadUserPreset (validFile);
        const auto restored = takeSnapshot (processor);
        double restoreError = 0.0;
        test.expect (loadResult.wasOk(),
                     "load failed: " + toStdString (
                         loadResult.getErrorMessage()));
        test.expect (
            snapshotsMatch (processor, expected, restored, restoreError),
            "user preset restored different values; max error="
                + number (restoreError, 8));
        test.expect (
            processor.getSelectedFactoryPreset()
                == Processor::userPresetState,
            "loaded User preset was not retained as the selected UI state");

        const auto editedTime =
            expected[0] < 3999.0f ? expected[0] + 1.0f
                                  : expected[0] - 1.0f;
        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::time, editedTime),
            "could not edit the loaded User preset");
        test.expect (
            processor.getSelectedFactoryPreset()
                == Processor::customPresetState,
            "editing a User preset did not change its state to Custom");
        test.expect (
            processor.loadUserPreset (validFile).wasOk(),
            "could not restore the User preset after the Custom-state check");

        const auto beforeCorrupt = takeSnapshot (processor);
        const auto wroteCorrupt = corruptFile.replaceWithText (
            "<TBDELAY_PRESET product=\"TB_Delay\"><broken>");
        test.expect (wroteCorrupt, "could not create corrupt test file");
        const auto corruptResult = processor.loadUserPreset (corruptFile);
        const auto afterCorrupt = takeSnapshot (processor);
        double corruptError = 0.0;
        test.expect (corruptResult.failed(),
                     "corrupt preset was accepted");
        test.expect (
            snapshotsMatch (
                processor, beforeCorrupt, afterCorrupt, corruptError),
            "corrupt preset changed current state; max error="
                + number (corruptError, 8));
        test.report (
            "presetBytes=" + std::to_string (validFile.getSize())
            + " restoreMaxError=" + number (restoreError, 8)
            + " corruptResult=\""
            + toStdString (corruptResult.getErrorMessage()) + "\"");

        validFile.deleteFile();
        corruptFile.deleteFile();
    });
}

void testLegacyStateAndPresetMigration (
    TestRunner& runner,
    const juce::File& projectRoot)
{
    runner.run (
        "legacy 33 and previous 36 parameter states/presets migrate to 38",
        [&projectRoot] (TestCase& test)
    {
        constexpr std::array<const char*, 33> legacyIDs {
            TBDelayParameterIDs::time,
            TBDelayParameterIDs::offset,
            TBDelayParameterIDs::feedback,
            TBDelayParameterIDs::spacing,
            TBDelayParameterIDs::swing,
            TBDelayParameterIDs::width,
            TBDelayParameterIDs::mix,
            TBDelayParameterIDs::duck,
            TBDelayParameterIDs::output,
            TBDelayParameterIDs::lowCut,
            TBDelayParameterIDs::highCut,
            TBDelayParameterIDs::drive,
            TBDelayParameterIDs::age,
            TBDelayParameterIDs::modRate,
            TBDelayParameterIDs::modDepth,
            TBDelayParameterIDs::smear,
            TBDelayParameterIDs::size,
            TBDelayParameterIDs::shift,
            TBDelayParameterIDs::diverge,
            TBDelayParameterIDs::grain,
            TBDelayParameterIDs::grainSize,
            TBDelayParameterIDs::density,
            TBDelayParameterIDs::spray,
            TBDelayParameterIDs::pitch,
            TBDelayParameterIDs::reverse,
            TBDelayParameterIDs::timeMode,
            TBDelayParameterIDs::division,
            TBDelayParameterIDs::style,
            TBDelayParameterIDs::taps,
            TBDelayParameterIDs::character,
            TBDelayParameterIDs::timeMotion,
            TBDelayParameterIDs::transformRoute,
            TBDelayParameterIDs::freeze
        };

        Processor source;
        const auto presetIndex =
            findFactoryPresetIndex ("Lo-Fi Stutter");
        test.expect (presetIndex >= 0,
                     "legacy migration source preset is missing");
        if (presetIndex < 0)
            return;
        source.applyFactoryPreset (presetIndex, false);
        const auto expected = takeSnapshot (source);

        auto legacyState =
            source.getValueTreeState().copyState();
        for (int childIndex = legacyState.getNumChildren() - 1;
             childIndex >= 0;
             --childIndex)
        {
            const auto id =
                legacyState.getChild (childIndex)
                    .getProperty ("id").toString();
            if (id == TBDelayParameterIDs::stereoBypass
                || id == TBDelayParameterIDs::colorSpaceBypass
                || id
                       == TBDelayParameterIDs::grainTransformBypass
                || id == TBDelayParameterIDs::colorMacro
                || id == TBDelayParameterIDs::spaceMacro)
                legacyState.removeChild (childIndex, nullptr);
        }
        legacyState.removeProperty (
            "stateSchemaVersion", nullptr);
        legacyState.setProperty (
            "formatVersion", 1, nullptr);

        juce::MemoryBlock legacyStateData;
        if (auto xml = legacyState.createXml())
            juce::AudioProcessor::copyXmlToBinary (
                *xml, legacyStateData);

        Processor stateTarget;
        setPlainValue (
            stateTarget,
            TBDelayParameterIDs::stereoBypass, 1.0f);
        setPlainValue (
            stateTarget,
            TBDelayParameterIDs::colorSpaceBypass, 1.0f);
        setPlainValue (
            stateTarget,
            TBDelayParameterIDs::grainTransformBypass, 1.0f);
        stateTarget.setStateInformation (
            legacyStateData.getData(),
            static_cast<int> (legacyStateData.getSize()));

        const auto migratedState =
            takeSnapshot (stateTarget);
        auto legacyStateValuesMatch = true;
        for (std::size_t index = 0;
             index < legacyIDs.size();
             ++index)
            legacyStateValuesMatch =
                legacyStateValuesMatch
                && std::abs (
                       migratedState[index] - expected[index])
                       <= valueTolerance (
                           *getParameter (
                               stateTarget, legacyIDs[index]),
                           expected[index]);
        const auto stateBypassesOff =
            migratedState[33] < 0.5f
            && migratedState[34] < 0.5f
            && migratedState[35] < 0.5f
            && std::abs (migratedState[36])
                   <= valueTolerance (
                       *getParameter (stateTarget,
                                      TBDelayParameterIDs::colorMacro),
                       0.0f)
            && std::abs (migratedState[37])
                   <= valueTolerance (
                       *getParameter (stateTarget,
                                      TBDelayParameterIDs::spaceMacro),
                       0.0f);

        juce::MemoryBlock currentStateData;
        stateTarget.getStateInformation (currentStateData);
        const auto currentStateXml =
            juce::AudioProcessor::getXmlFromBinary (
                currentStateData.getData(),
                static_cast<int> (currentStateData.getSize()));
        const auto currentStateContract =
            currentStateXml != nullptr
            && currentStateXml->getIntAttribute (
                   "stateSchemaVersion") == 2
            && ! currentStateXml->hasAttribute ("formatVersion")
            && currentStateXml->getNumChildElements() == 38;

        const auto beforeFuture =
            takeSnapshot (stateTarget);
        auto futureState = stateTarget.getValueTreeState().copyState();
        futureState.setProperty (
            "stateSchemaVersion", 99, nullptr);
        juce::MemoryBlock futureStateData;
        if (auto xml = futureState.createXml())
            juce::AudioProcessor::copyXmlToBinary (
                *xml, futureStateData);
        stateTarget.setStateInformation (
            futureStateData.getData(),
            static_cast<int> (futureStateData.getSize()));
        const auto afterFuture =
            takeSnapshot (stateTarget);
        double futurePreservationError = 0.0;
        const auto futureStateRejected =
            snapshotsMatch (
                stateTarget, beforeFuture, afterFuture,
                futurePreservationError);

        const auto legacyPresetFile =
            getOutputDirectory (projectRoot)
                .getChildFile ("legacy_v1_33.tbdelay");
        legacyPresetFile.deleteFile();
        juce::XmlElement legacyPreset ("TBDELAY_PRESET");
        legacyPreset.setAttribute ("product", "TB_Delay");
        legacyPreset.setAttribute ("formatVersion", 1);
        legacyPreset.setAttribute ("name", "Legacy v1");
        for (std::size_t index = 0;
             index < legacyIDs.size();
             ++index)
        {
            auto* parameter =
                legacyPreset.createNewChildElement ("PARAM");
            parameter->setAttribute ("id", legacyIDs[index]);
            parameter->setAttribute (
                "value",
                static_cast<double> (expected[index]));
        }
        const auto wroteLegacyPreset =
            legacyPreset.writeTo (legacyPresetFile);

        Processor presetTarget;
        setPlainValue (
            presetTarget,
            TBDelayParameterIDs::stereoBypass, 1.0f);
        setPlainValue (
            presetTarget,
            TBDelayParameterIDs::colorSpaceBypass, 1.0f);
        setPlainValue (
            presetTarget,
            TBDelayParameterIDs::grainTransformBypass, 1.0f);
        const auto presetResult =
            presetTarget.loadUserPreset (legacyPresetFile);
        const auto migratedPreset =
            takeSnapshot (presetTarget);
        auto legacyPresetValuesMatch = true;
        for (std::size_t index = 0;
             index < legacyIDs.size();
             ++index)
            legacyPresetValuesMatch =
                legacyPresetValuesMatch
                && std::abs (
                       migratedPreset[index] - expected[index])
                       <= valueTolerance (
                           *getParameter (
                               presetTarget, legacyIDs[index]),
                           expected[index]);
        const auto presetBypassesOff =
            migratedPreset[33] < 0.5f
            && migratedPreset[34] < 0.5f
            && migratedPreset[35] < 0.5f
            && std::abs (migratedPreset[36])
                   <= valueTolerance (
                       *getParameter (presetTarget,
                                      TBDelayParameterIDs::colorMacro),
                       0.0f)
            && std::abs (migratedPreset[37])
                   <= valueTolerance (
                       *getParameter (presetTarget,
                                      TBDelayParameterIDs::spaceMacro),
                       0.0f);

        test.expect (
            legacyStateData.getSize() > 0
                && legacyState.getNumChildren() == 33,
            "legacy state fixture is not the known 33-parameter schema");
        test.expect (
            legacyStateValuesMatch && stateBypassesOff,
            "legacy DAW state did not migrate atomically");
        test.expect (
            currentStateContract,
            "migrated state did not re-save as schema 2 with 38 parameters");
        test.expect (
            futureStateRejected,
            "future state schema changed the current state");
        test.expect (
            wroteLegacyPreset
                && presetResult.wasOk()
                && legacyPresetValuesMatch
                && presetBypassesOff,
            "legacy v1 user preset did not migrate with bypasses off");

        auto previousState =
            source.getValueTreeState().copyState();
        for (int childIndex = previousState.getNumChildren() - 1;
             childIndex >= 0; --childIndex)
        {
            const auto id = previousState.getChild (childIndex)
                                .getProperty ("id").toString();
            if (id == TBDelayParameterIDs::colorMacro
                || id == TBDelayParameterIDs::spaceMacro)
                previousState.removeChild (childIndex, nullptr);
        }
        previousState.setProperty (
            "stateSchemaVersion", 1, nullptr);
        previousState.removeProperty ("formatVersion", nullptr);
        juce::MemoryBlock previousStateData;
        if (auto xml = previousState.createXml())
            juce::AudioProcessor::copyXmlToBinary (
                *xml, previousStateData);
        Processor previousStateTarget;
        setPlainValue (
            previousStateTarget,
            TBDelayParameterIDs::colorMacro, 100.0f);
        setPlainValue (
            previousStateTarget,
            TBDelayParameterIDs::spaceMacro, -100.0f);
        previousStateTarget.setStateInformation (
            previousStateData.getData(),
            static_cast<int> (previousStateData.getSize()));
        const auto migratedPreviousState =
            takeSnapshot (previousStateTarget);
        auto previousStateValuesMatch =
            previousState.getNumChildren() == 36
            && std::abs (migratedPreviousState[36])
                   <= valueTolerance (
                       *getParameter (previousStateTarget,
                                      TBDelayParameterIDs::colorMacro),
                       0.0f)
            && std::abs (migratedPreviousState[37])
                   <= valueTolerance (
                       *getParameter (previousStateTarget,
                                      TBDelayParameterIDs::spaceMacro),
                       0.0f);
        for (std::size_t index = 0; index < 36; ++index)
            previousStateValuesMatch = previousStateValuesMatch
                && std::abs (
                       migratedPreviousState[index] - expected[index])
                       <= valueTolerance (
                           *getParameter (
                               previousStateTarget,
                               TBDelayParameterIDs::all[index]),
                           expected[index]);

        const auto previousPresetFile =
            getOutputDirectory (projectRoot)
                .getChildFile ("previous_v2_36.tbdelay");
        previousPresetFile.deleteFile();
        juce::XmlElement previousPreset ("TBDELAY_PRESET");
        previousPreset.setAttribute ("product", "TB_Delay");
        previousPreset.setAttribute ("presetFormatVersion", 2);
        previousPreset.setAttribute ("stateSchemaVersion", 1);
        previousPreset.setAttribute ("name", "Previous v2");
        for (std::size_t index = 0; index < 36; ++index)
        {
            auto* parameter =
                previousPreset.createNewChildElement ("PARAM");
            parameter->setAttribute (
                "id", TBDelayParameterIDs::all[index]);
            parameter->setAttribute (
                "value", static_cast<double> (expected[index]));
        }
        const auto wrotePreviousPreset =
            previousPreset.writeTo (previousPresetFile);
        Processor previousPresetTarget;
        setPlainValue (
            previousPresetTarget,
            TBDelayParameterIDs::colorMacro, 100.0f);
        setPlainValue (
            previousPresetTarget,
            TBDelayParameterIDs::spaceMacro, -100.0f);
        const auto previousPresetResult =
            previousPresetTarget.loadUserPreset (previousPresetFile);
        const auto migratedPreviousPreset =
            takeSnapshot (previousPresetTarget);
        auto previousPresetValuesMatch =
            std::abs (migratedPreviousPreset[36])
                <= valueTolerance (
                    *getParameter (previousPresetTarget,
                                   TBDelayParameterIDs::colorMacro),
                    0.0f)
            && std::abs (migratedPreviousPreset[37])
                   <= valueTolerance (
                       *getParameter (previousPresetTarget,
                                      TBDelayParameterIDs::spaceMacro),
                       0.0f);
        for (std::size_t index = 0; index < 36; ++index)
            previousPresetValuesMatch = previousPresetValuesMatch
                && std::abs (
                       migratedPreviousPreset[index] - expected[index])
                       <= valueTolerance (
                           *getParameter (
                               previousPresetTarget,
                               TBDelayParameterIDs::all[index]),
                           expected[index]);
        test.expect (
            previousStateValuesMatch,
            "schema 1 / 36-parameter DAW state did not migrate to BASE macros");
        test.expect (
            wrotePreviousPreset
                && previousPresetResult.wasOk()
                && previousPresetValuesMatch,
            "preset v2/schema 1 did not migrate to preset v3/schema 2");
        test.report (
            "legacyStateChildren="
            + std::to_string (legacyState.getNumChildren())
            + " currentStateChildren="
            + std::to_string (
                currentStateXml != nullptr
                    ? currentStateXml->getNumChildElements()
                    : 0)
            + " futurePreserveMaxError="
            + number (futurePreservationError, 8)
            + " presetResult=\""
            + toStdString (presetResult.getErrorMessage())
            + "\" previousStateChildren="
            + std::to_string (previousState.getNumChildren())
            + " previousPresetResult=\""
            + toStdString (
                previousPresetResult.getErrorMessage())
            + "\"");

        legacyPresetFile.deleteFile();
        previousPresetFile.deleteFile();
    });
}

void testUndo (TestRunner& runner)
{
    runner.run ("factory preset load is one undo transaction", [] (TestCase& test)
    {
        Processor processor;
        processor.getUndoManager().clearUndoHistory();
        const auto initial = takeSnapshot (processor);
        const auto presetIndex =
            findFactoryPresetIndex ("Classic Ping Pong");
        test.expect (presetIndex >= 0,
                     "undo source preset is missing");
        if (presetIndex < 0)
            return;
        processor.applyFactoryPreset (presetIndex, true);
        const auto preset = takeSnapshot (processor);
        test.expect (processor.canUndo(),
                     "preset load created no undo operation");

        processor.undo();
        const auto undone = takeSnapshot (processor);
        double undoError = 0.0;
        test.expect (
            snapshotsMatch (processor, initial, undone, undoError),
            "one Undo did not restore the entire state; max error="
                + number (undoError, 8));
        test.expect (processor.canRedo(),
                     "Undo created no redo operation");

        processor.redo();
        const auto redone = takeSnapshot (processor);
        double redoError = 0.0;
        test.expect (
            snapshotsMatch (processor, preset, redone, redoError),
            "one Redo did not restore the preset; max error="
                + number (redoError, 8));
        test.report (
            "undoMaxError=" + number (undoError, 8)
            + " redoMaxError=" + number (redoError, 8));
    });
}

void testDelayMapPrediction (TestRunner& runner)
{
    runner.run (
        "Delay Map L/R time, pitch, grain, and nominal decay preview",
        [] (TestCase& test)
    {
        const auto findEvent = [] (
            const TBDelayMapPrediction& prediction,
            TBDelayMapEvent::Kind kind,
            int lane, int sourceLane, int repeat, int tap)
            -> const TBDelayMapEvent*
        {
            for (const auto& event : prediction.events)
                if (event.kind == kind
                    && event.lane == lane
                    && event.sourceLane == sourceLane
                    && event.repeat == repeat
                    && event.tap == tap)
                    return &event;
            return nullptr;
        };
        const auto close = [] (double first, double second)
        {
            return std::abs (first - second) <= 1.0e-5;
        };
        const auto closeWithin = [] (
            double first, double second, double tolerance)
        {
            return std::abs (first - second) <= tolerance;
        };

        TBDelayMapParameters dual;
        dual.timeMode = 0;
        dual.timeMs = 100.0;
        dual.style = 1;
        dual.offsetPercent = 40.0f;
        dual.feedbackPercent = 0.0f;
        const auto dualMap = buildTBDelayMapPrediction (dual);
        test.expect (
            close (dualMap.channelDelayMs[0], 100.0)
                && close (dualMap.channelDelayMs[1], 120.0),
            "Dual +40% offset did not predict 100/120 ms");

        auto laterFree = dual;
        laterFree.timeMs = 820.0;
        const auto laterFreeMap =
            buildTBDelayMapPrediction (laterFree);
        test.expect (
            close (laterFreeMap.channelDelayMs[0], 820.0)
                && laterFreeMap.channelDelayMs[0]
                       > dualMap.channelDelayMs[0],
            "Free TIME did not move the primary delay anchor");

        TBDelayMapParameters sync;
        sync.timeMode = 1;
        sync.bpm = 120.0;
        sync.division = 6;
        sync.timeMs = 160.0;
        const auto syncMap =
            buildTBDelayMapPrediction (sync);
        sync.timeMs = 1400.0;
        const auto syncRawTimeChanged =
            buildTBDelayMapPrediction (sync);
        sync.division = 12;
        const auto syncDivisionChanged =
            buildTBDelayMapPrediction (sync);
        test.expect (
            close (syncMap.channelDelayMs[0], 250.0)
                && close (
                    syncRawTimeChanged.channelDelayMs[0],
                    syncMap.channelDelayMs[0])
                && close (
                    syncDivisionChanged.channelDelayMs[0],
                    1000.0),
            "Sync preview did not ignore raw TIME or follow DIVISION");

        TBDelayMapParameters ping = dual;
        ping.style = 2;
        ping.offsetPercent = 0.0f;
        ping.feedbackPercent = 70.0f;
        const auto pingMap = buildTBDelayMapPrediction (ping);
        const auto* ping0 = findEvent (
            pingMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 0);
        const auto* ping1 = findEvent (
            pingMap, TBDelayMapEvent::Kind::echo, 1, 0, 1, 0);
        const auto* ping2 = findEvent (
            pingMap, TBDelayMapEvent::Kind::echo, 0, 0, 2, 0);
        std::array<int, 3> pingPathCounts {};
        for (const auto& event : pingMap.events)
            if (event.kind == TBDelayMapEvent::Kind::echo
                && event.sourceLane == 0
                && event.tap == 0
                && event.repeat >= 0 && event.repeat < 3)
                ++pingPathCounts[
                    static_cast<std::size_t> (event.repeat)];
        test.expect (
            ping0 != nullptr && ping1 != nullptr && ping2 != nullptr
                && close (ping0->timeMs, 100.0)
                && close (ping1->timeMs, 200.0)
                && close (ping2->timeMs, 300.0)
                && close (pingMap.primaryEventTimeMs[0], 100.0)
                && close (pingMap.primaryEventTimeMs[1], 200.0)
                && pingPathCounts
                       == std::array<int, 3> { 1, 1, 1 },
            "Ping-Pong L seed did not predict L-R-L at 100/200/300 ms");

        TBDelayMapParameters taps = dual;
        taps.style = 0;
        taps.offsetPercent = 0.0f;
        taps.taps = 3;
        taps.spacingPercent = 150.0f;
        taps.swingPercent = 50.0f;
        const auto tapMap = buildTBDelayMapPrediction (taps);
        const auto* tap0 = findEvent (
            tapMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 0);
        const auto* tap1 = findEvent (
            tapMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 1);
        const auto* tap2 = findEvent (
            tapMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 2);
        test.expect (
            tapMap.tapCount == 8
                && tap0 != nullptr && tap1 != nullptr && tap2 != nullptr
                && close (tap0->timeMs, 100.0)
                && close (tap1->timeMs, 287.5)
                && close (tap2->timeMs, 400.0),
            "TAPS/SPACING/SWING prediction does not match DSP tap units");

        TBDelayMapParameters shift = dual;
        shift.style = 0;
        shift.offsetPercent = 0.0f;
        shift.feedbackPercent = 80.0f;
        shift.shiftHz = 100.0f;
        shift.divergePercent = 100.0f;
        shift.transformRoute = 0;
        const auto loopShiftMap = buildTBDelayMapPrediction (shift);
        const auto* loopLeft1 = findEvent (
            loopShiftMap, TBDelayMapEvent::Kind::echo, 0, 0, 1, 0);
        const auto* loopRight1 = findEvent (
            loopShiftMap, TBDelayMapEvent::Kind::echo, 1, 1, 1, 0);
        test.expect (
            loopLeft1 != nullptr && loopRight1 != nullptr
                && close (loopLeft1->cumulativeShiftHz, 200.0)
                && close (loopRight1->cumulativeShiftHz, -200.0),
            "Loop Shift/Diverge did not accumulate +/-200 Hz by generation 2");

        shift.transformRoute = 1;
        const auto postShiftMap = buildTBDelayMapPrediction (shift);
        const auto* postLeft1 = findEvent (
            postShiftMap, TBDelayMapEvent::Kind::echo, 0, 0, 1, 0);
        const auto* postRight1 = findEvent (
            postShiftMap, TBDelayMapEvent::Kind::echo, 1, 1, 1, 0);
        test.expect (
            postLeft1 != nullptr && postRight1 != nullptr
                && close (postLeft1->cumulativeShiftHz, 100.0)
                && close (postRight1->cumulativeShiftHz, -100.0),
            "Post Shift should remain one non-cumulative pass");

        shift.style = 2;
        shift.transformRoute = 0;
        const auto pingLoopShiftMap =
            buildTBDelayMapPrediction (shift);
        const auto* pingLoop0 = findEvent (
            pingLoopShiftMap, TBDelayMapEvent::Kind::echo,
            0, 0, 0, 0);
        const auto* pingLoop1 = findEvent (
            pingLoopShiftMap, TBDelayMapEvent::Kind::echo,
            1, 0, 1, 0);
        const auto* pingLoop2 = findEvent (
            pingLoopShiftMap, TBDelayMapEvent::Kind::echo,
            0, 0, 2, 0);
        test.expect (
            pingLoop0 != nullptr
                && pingLoop1 != nullptr
                && pingLoop2 != nullptr
                && close (pingLoop0->cumulativeShiftHz, 100.0)
                && close (pingLoop1->cumulativeShiftHz, 0.0)
                && close (pingLoop2->cumulativeShiftHz, 100.0),
            "Ping Loop shift did not carry the crossed +100/0/+100 Hz path");

        shift.transformRoute = 1;
        const auto pingPostShiftMap =
            buildTBDelayMapPrediction (shift);
        const auto* pingPost0 = findEvent (
            pingPostShiftMap, TBDelayMapEvent::Kind::echo,
            0, 0, 0, 0);
        const auto* pingPost1 = findEvent (
            pingPostShiftMap, TBDelayMapEvent::Kind::echo,
            1, 0, 1, 0);
        const auto* pingPost2 = findEvent (
            pingPostShiftMap, TBDelayMapEvent::Kind::echo,
            0, 0, 2, 0);
        test.expect (
            pingPost0 != nullptr
                && pingPost1 != nullptr
                && pingPost2 != nullptr
                && close (pingPost0->cumulativeShiftHz, 100.0)
                && close (pingPost1->cumulativeShiftHz, -100.0)
                && close (pingPost2->cumulativeShiftHz, 100.0),
            "Ping Post shift should follow the current lane only");

        TBDelayMapParameters lowFeedback = dual;
        lowFeedback.style = 0;
        lowFeedback.feedbackPercent = 25.0f;
        const auto lowFeedbackMap =
            buildTBDelayMapPrediction (lowFeedback);
        lowFeedback.feedbackPercent = 90.0f;
        const auto highFeedbackMap =
            buildTBDelayMapPrediction (lowFeedback);
        const auto* lowDecay = findEvent (
            lowFeedbackMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 0);
        const auto* highDecay = findEvent (
            highFeedbackMap, TBDelayMapEvent::Kind::echo, 0, 0, 0, 0);
        test.expect (
            lowDecay != nullptr && highDecay != nullptr
                && close (lowFeedbackMap.feedbackGain, 0.24625)
                && close (highFeedbackMap.feedbackGain, 0.8865)
                && highDecay->decayMs > lowDecay->decayMs
                && tbDelayMapTimeNormalised (
                       highDecay->timeMs + highDecay->decayMs)
                       > tbDelayMapTimeNormalised (
                           lowDecay->timeMs + lowDecay->decayMs),
            "feedback decay triangle is not monotonic");

        TBDelayMapParameters longTail = lowFeedback;
        longTail.taps = 3;
        longTail.spacingPercent = 200.0f;
        longTail.swingPercent = 0.0f;
        const auto longTailMap =
            buildTBDelayMapPrediction (longTail);
        const auto* longTailLastTap = findEvent (
            longTailMap, TBDelayMapEvent::Kind::echo,
            0, 0, 0, 7);
        const auto expectedLongDecay =
            1500.0
            * (1.0 + std::log (0.001) / std::log (0.8865));
        longTail.feedbackPercent = 100.0f;
        const auto unityTailMap =
            buildTBDelayMapPrediction (longTail);
        test.expect (
            closeWithin (
                longTailMap.nominalTailMs,
                expectedLongDecay, 1.0)
                && longTailMap.nominalTailMs > 12000.0
                && longTailLastTap != nullptr
                && closeWithin (
                    longTailLastTap->timeMs
                        + longTailLastTap->decayMs,
                    longTailMap.nominalTailMs, 0.001)
                && close (unityTailMap.nominalTailMs, 120000.0),
            "nominal decay does not use last-tap -60 dB tail semantics");

        TBDelayMapEvent upwardPitch;
        upwardPitch.pitchSemitones = 12.0f;
        upwardPitch.cumulativeShiftHz = -2000.0f;
        TBDelayMapEvent downwardPitch;
        downwardPitch.pitchSemitones = -12.0f;
        downwardPitch.cumulativeShiftHz = 2000.0f;
        TBDelayMapEvent upwardShift;
        upwardShift.cumulativeShiftHz = 100.0f;
        TBDelayMapEvent downwardShift;
        downwardShift.cumulativeShiftHz = -100.0f;
        test.expect (
            closeWithin (
                tbDelayMapTimeNormalised (10.0),
                0.0, 0.000001)
                && closeWithin (
                    tbDelayMapTimeNormalised (2505.0),
                    0.5, 0.000001)
                && tbDelayMapTimeNormalised (1800.0) > 0.0
                && tbDelayMapTimeNormalised (1800.0) < 1.0
                && closeWithin (
                    tbDelayMapTimeNormalised (5000.0),
                    1.0, 0.000001)
                && closeWithin (
                    tbDelayMapTimeNormalised (120000.0),
                    1.0, 0.000001)
                && tbDelayMapPitchNormalised (upwardPitch) > 0.0f
                && tbDelayMapPitchNormalised (downwardPitch) < 0.0f
                && tbDelayMapPitchNormalised (upwardShift) > 0.0f
                && tbDelayMapPitchNormalised (downwardShift) < 0.0f,
            "linear 10 ms-5 s mapping or pitch/shift direction is wrong");

        TBDelayMapParameters minimumTime;
        minimumTime.timeMode = 0;
        minimumTime.timeMs = 10.0;
        minimumTime.style = 0;
        minimumTime.feedbackPercent = 0.0f;
        minimumTime.modDepthMs = 20.0f;
        const auto minimumTimeMap =
            buildTBDelayMapPrediction (minimumTime);
        const auto* minimumTimeEcho = findEvent (
            minimumTimeMap, TBDelayMapEvent::Kind::echo,
            0, 0, 0, 0);
        test.expect (
            minimumTimeEcho != nullptr
                && close (minimumTimeMap.channelDelayMs[0], 10.0)
                && close (minimumTimeEcho->timeSpreadMs, 0.0),
            "minimum TIME did not cap modulation to safe read headroom");

        TBDelayMapParameters grain = dual;
        grain.style = 0;
        grain.feedbackPercent = 0.0f;
        grain.grainPercent = 100.0f;
        grain.grainSizeMs = 200.0f;
        grain.grainDensity = 8.0f;
        grain.sprayPercent = 100.0f;
        grain.pitchSemitones = 12.0f;
        grain.reversePercent = 60.0f;
        const auto grainMap = buildTBDelayMapPrediction (grain);
        const auto* cloud = findEvent (
            grainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        const auto* ghostEcho = findEvent (
            grainMap, TBDelayMapEvent::Kind::echo,
            0, 0, 0, 0);
        test.expect (
            cloud != nullptr
                && ghostEcho == nullptr
                && close (grainMap.channelDelayMs[0], 100.0)
                && closeWithin (cloud->timeMs, 400.0, 0.1)
                && closeWithin (cloud->timeSpreadMs, 345.0, 0.1)
                && close (cloud->durationMs, 200.0)
                && close (cloud->pitchSemitones, 12.0)
                && cloud->grainCount == 8
                && close (cloud->reverseProbability, 0.6),
            "grain cloud does not expose its safe forward/reverse output support");

        grain.reversePercent = 0.0f;
        const auto forwardGrainMap =
            buildTBDelayMapPrediction (grain);
        const auto* forwardCloud = findEvent (
            forwardGrainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        const auto grainSafetyMs = 4000.0 / grain.sampleRate;
        test.expect (
            forwardCloud != nullptr
                && closeWithin (
                    forwardCloud->timeMs
                        - forwardCloud->timeSpreadMs,
                    200.0 + grainSafetyMs, 0.1)
                && closeWithin (
                    forwardCloud->timeMs
                        + forwardCloud->timeSpreadMs,
                    400.0 + grainSafetyMs, 0.1),
            "forward +12 st cloud ignored grain-length read safety");

        TBDelayMapParameters boundaryGrain = grain;
        boundaryGrain.timeMs = 8000.0;
        boundaryGrain.grainSizeMs = 500.0f;
        boundaryGrain.sprayPercent = 0.0f;
        boundaryGrain.pitchSemitones = -24.0f;
        boundaryGrain.reversePercent = 0.0f;
        const auto slowBoundaryMap =
            buildTBDelayMapPrediction (boundaryGrain);
        const auto* slowBoundaryCloud = findEvent (
            slowBoundaryMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        boundaryGrain.pitchSemitones = 0.0f;
        boundaryGrain.reversePercent = 100.0f;
        const auto reverseBoundaryMap =
            buildTBDelayMapPrediction (boundaryGrain);
        const auto* reverseBoundaryCloud = findEvent (
            reverseBoundaryMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        test.expect (
            slowBoundaryCloud != nullptr
                && reverseBoundaryCloud != nullptr
                && slowBoundaryCloud->timeMs
                           + slowBoundaryCloud->timeSpreadMs
                       <= 8000.1
                && reverseBoundaryCloud->timeMs
                           + reverseBoundaryCloud->timeSpreadMs
                       <= 8000.1,
            "long grain trajectory crossed the 8 s history boundary");

        TBDelayMapParameters routeGrain = grain;
        routeGrain.feedbackPercent = 80.0f;
        routeGrain.grainSizeMs = 50.0f;
        routeGrain.sprayPercent = 0.0f;
        routeGrain.reversePercent = 0.0f;
        routeGrain.shiftHz = 100.0f;
        routeGrain.transformRoute = 0;
        const auto loopGrainMap =
            buildTBDelayMapPrediction (routeGrain);
        routeGrain.transformRoute = 1;
        const auto postGrainMap =
            buildTBDelayMapPrediction (routeGrain);
        const auto* loopCloud0 = findEvent (
            loopGrainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        const auto* loopCloud1 = findEvent (
            loopGrainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 1, 0);
        const auto* postCloud0 = findEvent (
            postGrainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 0, 0);
        const auto* postCloud1 = findEvent (
            postGrainMap, TBDelayMapEvent::Kind::grainCloud,
            0, 0, 1, 0);
        test.expect (
            loopCloud0 != nullptr && loopCloud1 != nullptr
                && postCloud0 != nullptr && postCloud1 != nullptr
                && close (loopCloud0->pitchSemitones, 12.0)
                && close (loopCloud0->cumulativeShiftHz, 100.0)
                && close (loopCloud1->pitchSemitones, 24.0)
                && close (loopCloud1->cumulativeShiftHz, 300.0)
                && close (postCloud0->pitchSemitones, 12.0)
                && close (postCloud0->cumulativeShiftHz, 100.0)
                && close (postCloud1->pitchSemitones, 12.0)
                && close (postCloud1->cumulativeShiftHz, 100.0),
            "Loop/Post grain route did not preserve affine pitch/shift feedback");

        TBDelayMapParameters mono = ping;
        mono.channelCount = 1;
        mono.feedbackPercent = 0.0f;
        mono.grainPercent = 50.0f;
        const auto monoMap = buildTBDelayMapPrediction (mono);
        auto monoHasRightEvent = false;
        auto monoEchoCount = 0;
        auto monoCloudCount = 0;
        for (const auto& event : monoMap.events)
        {
            monoHasRightEvent =
                monoHasRightEvent || event.lane == 1;
            monoEchoCount +=
                event.kind == TBDelayMapEvent::Kind::echo ? 1 : 0;
            monoCloudCount +=
                event.kind == TBDelayMapEvent::Kind::grainCloud ? 1 : 0;
        }
        test.expect (
            monoMap.channelCount == 1 && ! monoHasRightEvent
                && monoEchoCount == 1 && monoCloudCount == 1,
            "mono Delay Map predicted a false R/Ping feedback lane");

        TBDelayMapParameters negativeFeedback = dual;
        negativeFeedback.style = 0;
        negativeFeedback.offsetPercent = -1.0f;
        negativeFeedback.feedbackPercent = 60.0f;
        const auto negativeMap =
            buildTBDelayMapPrediction (negativeFeedback);
        const auto* invertedRepeat = findEvent (
            negativeMap, TBDelayMapEvent::Kind::echo,
            0, 0, 1, 0);
        test.expect (
            negativeMap.feedbackPolarity < 0.0f
                && invertedRepeat != nullptr
                && invertedRepeat->inverted,
            "negative OFFSET feedback polarity is absent from the map");

        lowFeedback.feedbackPercent = 110.0f;
        const auto saturationMap =
            buildTBDelayMapPrediction (lowFeedback);
        auto finiteSaturationEvents = true;
        for (const auto& event : saturationMap.events)
            finiteSaturationEvents =
                finiteSaturationEvents
                && (event.lane == 0 || event.lane == 1)
                && event.timeMs >= 0.0
                && std::isfinite (event.timeMs)
                && std::isfinite (event.timeSpreadMs)
                && std::isfinite (event.durationMs)
                && std::isfinite (event.decayMs)
                && std::isfinite (event.pitchSemitones)
                && std::isfinite (event.cumulativeShiftHz)
                && std::isfinite (event.level);
        test.expect (
            std::isfinite (saturationMap.feedbackGain)
                && saturationMap.feedbackGain > 1.0f
                && saturationMap.events.size() <= 72
                && finiteSaturationEvents,
            "110% feedback prediction is unbounded or non-finite");

        TBDelayMapParameters bypassedMapParameters;
        bypassedMapParameters.timeMs = 140.0;
        bypassedMapParameters.timeMode = 0;
        bypassedMapParameters.style = 2;
        bypassedMapParameters.offsetPercent = 80.0f;
        bypassedMapParameters.modDepthMs = 12.0f;
        bypassedMapParameters.shiftHz = 500.0f;
        bypassedMapParameters.divergePercent = 100.0f;
        bypassedMapParameters.grainPercent = 100.0f;
        bypassedMapParameters.pitchSemitones = 12.0f;
        bypassedMapParameters.freeze = true;
        bypassedMapParameters.stereoBypass = true;
        bypassedMapParameters.colorSpaceBypass = true;
        bypassedMapParameters.grainTransformBypass = true;
        const auto bypassedMap =
            buildTBDelayMapPrediction (bypassedMapParameters);
        auto bypassedMapIsNeutral = true;
        auto bypassedCloudCount = 0;
        for (const auto& event : bypassedMap.events)
        {
            bypassedCloudCount +=
                event.kind == TBDelayMapEvent::Kind::grainCloud
                    ? 1
                    : 0;
            bypassedMapIsNeutral =
                bypassedMapIsNeutral
                && std::abs (event.timeSpreadMs) <= 0.000001
                && std::abs (event.cumulativeShiftHz) <= 0.000001f;
        }
        test.expect (
            close (bypassedMap.channelDelayMs[0], 140.0)
                && close (bypassedMap.channelDelayMs[1], 140.0)
                && close (bypassedMap.primaryEventTimeMs[0], 140.0)
                && close (bypassedMap.primaryEventTimeMs[1], 140.0)
                && bypassedCloudCount == 0
                && bypassedMapIsNeutral
                && close (bypassedMap.nominalTailMs, 120000.0),
            "Delay Map does not reflect module bypasses or independent Freeze");

        test.report (
            "dual=" + number (dualMap.channelDelayMs[0], 1)
            + "/" + number (dualMap.channelDelayMs[1], 1) + "ms"
            + " pingPath=100/200/300ms"
            + " taps=" + std::to_string (tapMap.tapCount)
            + " pingLoopShift=+100/0/+100Hz"
            + " decay=" + number (lowDecay != nullptr
                                      ? lowDecay->decayMs : 0.0, 1)
            + "->" + number (highDecay != nullptr
                                 ? highDecay->decayMs : 0.0, 1) + "ms"
            + " grainSupport="
            + number (
                cloud != nullptr
                    ? cloud->timeMs - cloud->timeSpreadMs
                    : 0.0, 1)
            + ".."
            + number (
                cloud != nullptr
                    ? cloud->timeMs + cloud->timeSpreadMs
                    : 0.0, 1)
            + "ms");
    });
}

void testTapTempo (TestRunner& runner)
{
    runner.run (
        "TAP tempo timing, mode ownership, and DAW focus safety",
        [] (TestCase& test)
    {
        constexpr auto freeTapWaitMs = 220;
        constexpr auto syncTapWaitMs = 250;
        constexpr auto timingToleranceMs = 45.0;

        Processor freeProcessor;
        test.expect (
            applyValues (
                freeProcessor,
                std::array<std::pair<const char*, float>, 3> {{
                    { TBDelayParameterIDs::timeMode, 0.0f },
                    { TBDelayParameterIDs::time, 731.0f },
                    { TBDelayParameterIDs::division, 12.0f }
                }}),
            "failed to prepare the Free TAP state");
        TB_DelayAudioProcessorEditor freeEditor (freeProcessor);
        freeEditor.setSize (
            TB_DelayAudioProcessorEditor::baseWidth,
            TB_DelayAudioProcessorEditor::baseHeight);
        freeEditor.resized();

        auto* freeTap = dynamic_cast<juce::Button*> (
            freeEditor.findChildWithID ("tap-tempo"));
        auto* timeModeControl =
            freeEditor.findChildWithID ("choice-time_mode");
        test.expect (
            freeTap != nullptr
                && freeTap->isVisible()
                && ! freeTap->getBounds().isEmpty(),
            "TAP button is missing below TIME MODE");
        test.expect (
            freeTap != nullptr
                && timeModeControl != nullptr
                && freeTap->getX() == timeModeControl->getX()
                && freeTap->getWidth() == timeModeControl->getWidth()
                && freeTap->getY() - timeModeControl->getBottom() == 6,
            "TAP is not aligned 6 px below TIME MODE at 100% scale");
        if (freeTap == nullptr)
            return;

        test.expect (
            ! freeTap->getClickingTogglesState()
                && ! freeTap->getWantsKeyboardFocus()
                && ! freeTap->getMouseClickGrabsKeyboardFocus(),
            "TAP is not a focus-safe momentary button");
        const auto beforeSpace = takeSnapshot (freeProcessor);
        const auto spaceWasConsumed = freeEditor.keyPressed (
            juce::KeyPress (juce::KeyPress::spaceKey));
        const auto afterSpace = takeSnapshot (freeProcessor);
        double spaceParameterError = 0.0;
        test.expect (
            ! spaceWasConsumed
                && snapshotsMatch (
                    freeProcessor, beforeSpace,
                    afterSpace, spaceParameterError),
            "editor/TAP consumed Space or changed a parameter");

        const auto beforeFirstFreeTap = takeSnapshot (freeProcessor);
        const auto freeFirstTapAt =
            juce::Time::getMillisecondCounterHiRes();
        freeTap->onClick();
        const auto afterFirstFreeTap = takeSnapshot (freeProcessor);
        double firstFreeTapError = 0.0;
        test.expect (
            snapshotsMatch (
                freeProcessor, beforeFirstFreeTap,
                afterFirstFreeTap, firstFreeTapError),
            "first Free TAP changed a parameter; max error="
                + number (firstFreeTapError, 6));

        juce::Thread::sleep (freeTapWaitMs);
        const auto freeSecondTapAt =
            juce::Time::getMillisecondCounterHiRes();
        freeTap->onClick();
        const auto measuredFreeInterval =
            freeSecondTapAt - freeFirstTapAt;
        const auto tappedFreeTime = getPlainValue (
            freeProcessor, TBDelayParameterIDs::time);
        const auto afterSecondFreeTap = takeSnapshot (freeProcessor);
        auto expectedFreeState = beforeFirstFreeTap;
        expectedFreeState[static_cast<std::size_t> (
            TBDelayPresetDetail::Parameter::time)] = tappedFreeTime;
        double secondFreeTapError = 0.0;
        test.expect (
            std::abs (tappedFreeTime - measuredFreeInterval)
                    <= timingToleranceMs
                && tappedFreeTime >= 100.0f
                && tappedFreeTime <= 3000.0f,
            "second Free TAP did not set Delay Time from its measured interval");
        test.expect (
            snapshotsMatch (
                freeProcessor, expectedFreeState,
                afterSecondFreeTap, secondFreeTapError)
                && std::abs (getPlainValue (
                       freeProcessor, TBDelayParameterIDs::timeMode))
                       < 0.001f
                && freeProcessor.getParameters().size()
                       == static_cast<int> (TBDelayParameterIDs::count),
            "Free TAP changed mode, another parameter, or the 38-parameter contract");

        freeEditor.setSize (
            juce::roundToInt (
                TB_DelayAudioProcessorEditor::baseWidth * 1.5f),
            juce::roundToInt (
                TB_DelayAudioProcessorEditor::baseHeight * 1.5f));
        freeEditor.resized();
        test.expect (
            timeModeControl != nullptr
                && freeTap->getX() == timeModeControl->getX()
                && freeTap->getWidth() == timeModeControl->getWidth()
                && freeTap->getY() - timeModeControl->getBottom() == 9,
            "TAP alignment does not scale with TIME MODE at 150%");

        Processor syncProcessor;
        test.expect (
            applyValues (
                syncProcessor,
                std::array<std::pair<const char*, float>, 3> {{
                    { TBDelayParameterIDs::timeMode, 1.0f },
                    { TBDelayParameterIDs::time, 731.0f },
                    { TBDelayParameterIDs::division, 12.0f }
                }}),
            "failed to prepare the Sync TAP state");
        TB_DelayAudioProcessorEditor syncEditor (syncProcessor);
        auto* syncTap = dynamic_cast<juce::Button*> (
            syncEditor.findChildWithID ("tap-tempo"));
        test.expect (syncTap != nullptr, "Sync TAP button is missing");
        if (syncTap == nullptr)
            return;

        const auto beforeFirstSyncTap = takeSnapshot (syncProcessor);
        syncTap->onClick();
        const auto afterFirstSyncTap = takeSnapshot (syncProcessor);
        double firstSyncTapError = 0.0;
        test.expect (
            snapshotsMatch (
                syncProcessor, beforeFirstSyncTap,
                afterFirstSyncTap, firstSyncTapError),
            "first Sync TAP changed a parameter; max error="
                + number (firstSyncTapError, 6));

        juce::Thread::sleep (syncTapWaitMs);
        syncTap->onClick();
        const auto tappedDivision = getPlainValue (
            syncProcessor, TBDelayParameterIDs::division);
        const auto afterSecondSyncTap = takeSnapshot (syncProcessor);
        auto expectedSyncState = beforeFirstSyncTap;
        expectedSyncState[static_cast<std::size_t> (
            TBDelayPresetDetail::Parameter::division)] = 6.0f;
        double secondSyncTapError = 0.0;
        test.expect (
            std::abs (tappedDivision - 6.0f) < 0.001f
                && snapshotsMatch (
                    syncProcessor, expectedSyncState,
                    afterSecondSyncTap, secondSyncTapError)
                && std::abs (getPlainValue (
                       syncProcessor, TBDelayParameterIDs::timeMode)
                       - 1.0f) < 0.001f
                && std::abs (getPlainValue (
                       syncProcessor, TBDelayParameterIDs::time)
                       - 731.0f) < 0.01f
                && syncProcessor.getParameters().size()
                       == static_cast<int> (TBDelayParameterIDs::count),
            "Sync TAP did not select 1/8 at default 120 BPM without changing mode/time/contract");
        test.report (
            "freeInterval=" + number (measuredFreeInterval, 1)
            + "ms freeTime=" + number (tappedFreeTime, 1)
            + "ms syncDivision=" + number (tappedDivision, 0));
    });
}

void testUiSnapshot (TestRunner& runner,
                     const juce::File& projectRoot)
{
    runner.run (
        "800x700 UI and parameter-driven Delay Map snapshots",
        [&projectRoot] (TestCase& test)
    {
        Processor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);
        TB_DelayAudioProcessorEditor editor (processor);
        editor.setSize (
            TB_DelayAudioProcessorEditor::baseWidth,
            TB_DelayAudioProcessorEditor::baseHeight);
        editor.resized();
        juce::Thread::sleep (50);
        juce::Timer::callPendingTimersSynchronously();

        juce::Image image (
            juce::Image::ARGB,
            TB_DelayAudioProcessorEditor::baseWidth,
            TB_DelayAudioProcessorEditor::baseHeight, true,
            juce::SoftwareImageType());
        juce::Graphics graphics (image);
        editor.paintEntireComponent (graphics, true);

        std::set<juce::uint32> sampledColours;
        for (int y = 0; y < image.getHeight(); y += 31)
            for (int x = 0; x < image.getWidth(); x += 31)
                sampledColours.insert (
                    image.getPixelAt (x, y).getARGB());

        std::size_t vividPurplePixelCount = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto colour = image.getPixelAt (x, y);
                const auto hue = colour.getHue();
                if (colour.getSaturation() > 0.45f
                    && hue >= 0.72f
                    && hue <= 0.90f
                    && colour.getBrightness() > 0.20f)
                    ++vividPurplePixelCount;
            }
        const auto vividPurplePixelLimit =
            static_cast<std::size_t> (
                image.getWidth() * image.getHeight())
            / 500;

        auto* delayMap = editor.findChildWithID ("delay-map");
        test.expect (delayMap != nullptr, "delay-map component ID was not found");

        // Tooltips and status text are Korean; JUCE's Windows default face has
        // no Hangul glyphs, so a Korean-capable face must be available and must
        // actually measure a real advance for Hangul.
        const juce::StringArray koreanFaces {
           #if JUCE_MAC
            "Apple SD Gothic Neo", "AppleGothic",
           #endif
            "Malgun Gothic", "Noto Sans KR", "Gulim"
        };
        juce::String resolvedFace;
        for (const auto& candidate : koreanFaces)
            if (juce::Font::findAllTypefaceNames().contains (candidate))
            {
                resolvedFace = candidate;
                break;
            }

        const auto koreanText = juce::String::fromUTF8 (
            "\xed\x94\x84\xeb\xa6\xac\xec\x85\x8b");
        juce::Font hangulFont (juce::FontOptions (14.0f));
        hangulFont = hangulFont.withTypefaceStyle (hangulFont.getTypefaceStyle());
        if (resolvedFace.isNotEmpty())
            hangulFont.setTypefaceName (resolvedFace);

        test.expect (
            resolvedFace.isNotEmpty(),
            std::string ("no Korean-capable typeface is installed; "
                         "editor tooltips would render as boxes"));
        test.expect (
            juce::GlyphArrangement::getStringWidth (hangulFont, koreanText)
                > 1.0f,
            std::string ("Hangul measured zero width in \"")
                + resolvedFace.toStdString() + "\"");
        test.expect (
            delayMap != nullptr
                && delayMap->getWidth() >= 760
                && delayMap->getHeight() >= 250,
            "Delay Map bounds are too small");
        if (delayMap != nullptr)
        {
            const auto bezelX = static_cast<int> (
                delayMap->getProperties()[
                    juce::Identifier ("bezel-outer-x")]);
            const auto bezelWidth = static_cast<int> (
                delayMap->getProperties()[
                    juce::Identifier ("bezel-outer-width")]);
            test.expect (
                bezelX == 0
                    && bezelWidth == delayMap->getWidth(),
                "painted Delay Map bezel is not the same width as the control board");
        }

        const auto findBoardKnob = [&editor] (const char* parameterID)
        {
            return editor.findChildWithID (
                "knob-" + juce::String (parameterID));
        };
        constexpr std::array<std::size_t, 12> visibleContinuous {
            0, 2, 5, 6, 7, 8, 17, 19, 23, 24, 36, 37
        };
        constexpr std::array<std::size_t, 15> hiddenContinuous {
            1, 3, 4, 9, 10, 11, 12, 13, 14, 15, 16,
            18, 20, 21, 22
        };
        auto rotaryControlCount = 0;
        auto linearControlCount = 0;
        for (const auto index : visibleContinuous)
        {
            auto* control = findBoardKnob (
                TBDelayParameterIDs::all[index]);
            const auto modeAdaptiveDelayTime = index == 0;
            test.expect (
                control != nullptr
                    && (modeAdaptiveDelayTime || control->isVisible())
                    && ! control->getBounds().isEmpty()
                    && editor.getLocalBounds().contains (
                        control->getBounds()),
                "public musical control is missing: "
                    + std::string (TBDelayParameterIDs::all[index]));
            if (control == nullptr)
                continue;

            for (int child = 0;
                 child < control->getNumChildComponents(); ++child)
                if (auto* slider = dynamic_cast<juce::Slider*> (
                        control->getChildComponent (child)))
                {
                    if (slider->getSliderStyle()
                        == juce::Slider::RotaryHorizontalVerticalDrag)
                        ++rotaryControlCount;
                    else if (slider->getSliderStyle()
                             == juce::Slider::LinearHorizontal)
                        ++linearControlCount;
                }
        }
        for (const auto index : hiddenContinuous)
            test.expect (
                findBoardKnob (TBDelayParameterIDs::all[index]) == nullptr,
                "legacy technical control remains on the main board: "
                    + std::string (TBDelayParameterIDs::all[index]));
        test.expect (
            rotaryControlCount == 10 && linearControlCount == 2,
            "main UI is not reduced to 10 continuous rotaries and 2 output sliders");

        constexpr std::array<std::size_t, 6> visibleChoices {
            25, 26, 27, 28, 29, 31
        };
        for (const auto index : visibleChoices)
        {
            auto* control = editor.findChildWithID (
                "choice-" + juce::String (
                    TBDelayParameterIDs::all[index]));
            test.expect (
                control != nullptr
                    && control->isVisible()
                    && ! control->getBounds().isEmpty(),
                "public selector is missing: "
                    + std::string (TBDelayParameterIDs::all[index]));
        }
        test.expect (
            editor.findChildWithID (
                "choice-" + juce::String (
                    TBDelayParameterIDs::timeMotion)) == nullptr,
            "legacy TIME CHANGE selector remains on the main board");

        auto steppedChoiceCount = 0;
        auto binaryChoiceCount = 0;
        auto dropdownChoiceCount = 0;
        auto unsafeChoiceFocusCount = 0;
        for (const auto index : visibleChoices)
        {
            auto* control = editor.findChildWithID (
                "choice-" + juce::String (
                    TBDelayParameterIDs::all[index]));
            if (control == nullptr)
                continue;
            for (int child = 0;
                 child < control->getNumChildComponents(); ++child)
            {
                auto* component = control->getChildComponent (child);
                if (auto* slider = dynamic_cast<juce::Slider*> (component))
                {
                    if (slider->getSliderStyle()
                            == juce::Slider::RotaryHorizontalVerticalDrag
                        && std::abs (slider->getInterval() - 1.0)
                               <= 0.000001)
                        ++steppedChoiceCount;
                    unsafeChoiceFocusCount +=
                        slider->getWantsKeyboardFocus()
                            || slider->getMouseClickGrabsKeyboardFocus()
                            ? 1 : 0;
                }
                else if (auto* button =
                             dynamic_cast<juce::Button*> (component))
                {
                    ++binaryChoiceCount;
                    unsafeChoiceFocusCount +=
                        button->getWantsKeyboardFocus()
                            || button->getMouseClickGrabsKeyboardFocus()
                            ? 1 : 0;
                }
                else if (dynamic_cast<juce::ComboBox*> (component) != nullptr)
                    ++dropdownChoiceCount;
            }
        }
        test.expect (
            steppedChoiceCount == 4
                && binaryChoiceCount == 0
                && dropdownChoiceCount == 2,
            "selector presentation is not 4 stepped knobs / "
            "2 dropdowns (TIME MODE, BUILD-ONCE) / 0 toggles");
        test.expect (
            unsafeChoiceFocusCount == 0,
            "a board selector can steal DAW keyboard focus");

        // A UTF-8 literal handed to juce::String as a plain const char* is
        // decoded as Latin-1, and the hover text then reaches the user as
        // mojibake. Real Hangul is what tells a correct tooltip from a broken
        // one, and any codepoint in the Latin-1 supplement is the fingerprint
        // of the mis-decode.
        const auto containsRange = [] (const juce::String& text,
                                       juce::juce_wchar low,
                                       juce::juce_wchar high)
        {
            for (auto character = text.getCharPointer();
                 ! character.isEmpty();
                 ++character)
            {
                const auto code = *character;
                if (code >= low && code <= high)
                    return true;
            }

            return false;
        };
        auto hangulTooltipCount = 0;
        auto mojibakeTooltipCount = 0;
        const auto inspectTooltip = [&] (juce::Component* component)
        {
            if (component == nullptr)
                return;

            if (auto* client =
                    dynamic_cast<juce::SettableTooltipClient*> (component))
            {
                const auto tip = client->getTooltip();
                hangulTooltipCount +=
                    containsRange (tip, 0xac00, 0xd7a3) ? 1 : 0;
                mojibakeTooltipCount +=
                    containsRange (tip, 0x0080, 0x00ff) ? 1 : 0;
            }
        };
        for (const auto index : visibleContinuous)
            inspectTooltip (
                findBoardKnob (TBDelayParameterIDs::all[index]));
        for (const auto index : visibleChoices)
            inspectTooltip (
                editor.findChildWithID (
                    "choice-" + juce::String (
                        TBDelayParameterIDs::all[index])));
        test.expect (
            hangulTooltipCount >= 10 && mojibakeTooltipCount == 0,
            "control tooltips are not decoded as UTF-8: hangul="
                + std::to_string (hangulTooltipCount)
                + " mojibake=" + std::to_string (mojibakeTooltipCount));

        auto* freezeControl =
            editor.findChildWithID ("toggle-freeze");
        test.expect (
            freezeControl != nullptr
                && freezeControl->isVisible()
                && ! freezeControl->getBounds().isEmpty(),
            "FREEZE is missing from TRANSFORM");

        auto* quadrantTimeControl =
            findBoardKnob (TBDelayParameterIDs::time);
        auto* quadrantWidthControl =
            findBoardKnob (TBDelayParameterIDs::width);
        auto* quadrantColorControl =
            findBoardKnob (TBDelayParameterIDs::colorMacro);
        auto* quadrantGrainControl =
            findBoardKnob (TBDelayParameterIDs::grain);
        const auto quadrantControlsFound =
            quadrantTimeControl != nullptr
            && quadrantWidthControl != nullptr
            && quadrantColorControl != nullptr
            && quadrantGrainControl != nullptr;
        test.expect (
            quadrantControlsFound,
            "2x2 musical quadrant anchors were not all found");
        if (quadrantControlsFound)
        {
            const auto timeCentre =
                quadrantTimeControl->getBounds().getCentre();
            const auto widthCentre =
                quadrantWidthControl->getBounds().getCentre();
            const auto colorCentre =
                quadrantColorControl->getBounds().getCentre();
            const auto grainCentre =
                quadrantGrainControl->getBounds().getCentre();
            test.expect (
                std::max (timeCentre.x, colorCentre.x)
                        < std::min (widthCentre.x, grainCentre.x)
                    && std::max (timeCentre.y, widthCentre.y)
                           < std::min (colorCentre.y, grainCentre.y),
                "board is not REPEAT/STEREO over CHARACTER/TRANSFORM");
        }

        const std::array<const char*, 3> bypassIDs {
            TBDelayParameterIDs::stereoBypass,
            TBDelayParameterIDs::colorSpaceBypass,
            TBDelayParameterIDs::grainTransformBypass
        };
        for (const auto* id : bypassIDs)
        {
            auto* control = editor.findChildWithID (
                "toggle-" + juce::String (id));
            test.expect (
                control != nullptr && control->isVisible()
                    && control->getWidth() <= 34
                    && control->getHeight() <= 34,
                "supporting module bypass is missing from a section header");
        }

        auto* finalMix = findBoardKnob (TBDelayParameterIDs::mix);
        auto* finalDuck = findBoardKnob (TBDelayParameterIDs::duck);
        auto* finalOutput = findBoardKnob (TBDelayParameterIDs::output);
        test.expect (
            editor.findChildWithID ("final-mix-label") == nullptr
                && editor.findChildWithID ("panic") == nullptr,
            "obsolete FINAL/PANIC utility remains exposed");
        test.expect (
            finalMix != nullptr && finalDuck != nullptr
                && finalOutput != nullptr
                && finalMix->getBounds().getCentreY()
                       == finalOutput->getBounds().getCentreY()
                && finalMix->getWidth() == finalOutput->getWidth()
                && finalMix->getY() > quadrantGrainControl->getBottom()
                && finalDuck->getY() < quadrantColorControl->getY(),
            "DUCK is not in REPEAT or MIX/OUTPUT footer geometry regressed");

        auto legacyPageButtonCount = 0;
        const auto countLegacyPageButtons = [&legacyPageButtonCount] (
            auto&& self, juce::Component& parent) -> void
        {
            for (int child = 0;
                 child < parent.getNumChildComponents();
                 ++child)
            {
                auto& component = *parent.getChildComponent (child);
                if (auto* button =
                        dynamic_cast<juce::Button*> (&component))
                    for (const auto& legacyName
                         : { "ECHO", "COLOR", "SPACE", "GRAIN" })
                        if (button->getButtonText()
                                .equalsIgnoreCase (legacyName))
                            ++legacyPageButtonCount;
                self (self, component);
            }
        };
        countLegacyPageButtons (
            countLegacyPageButtons, editor);
        test.expect (
            legacyPageButtonCount == 0,
            "legacy ECHO/COLOR/SPACE/GRAIN page buttons still exist");

        auto* presetPrevious =
            editor.findChildWithID ("preset-previous");
        auto* presetBoxComponent =
            editor.findChildWithID ("preset-box");
        auto* presetNext =
            editor.findChildWithID ("preset-next");
        auto* presetSave =
            editor.findChildWithID ("preset-save");
        auto* presetUndo =
            editor.findChildWithID ("preset-undo");
        auto* presetRedo =
            editor.findChildWithID ("preset-redo");
        auto* productTitle =
            editor.findChildWithID ("product-title");
        auto* brandLogo =
            editor.findChildWithID ("delay-logo");
        const auto headerComponentsFound =
            presetPrevious != nullptr
            && presetBoxComponent != nullptr
            && presetNext != nullptr
            && presetSave != nullptr
            && presetUndo != nullptr
            && presetRedo != nullptr
            && productTitle != nullptr
            && brandLogo != nullptr;
        test.expect (
            headerComponentsFound,
            "guide header controls were not all found");
        if (headerComponentsFound)
        {
            test.expect (
                presetPrevious->getRight()
                        < presetBoxComponent->getRight()
                    && presetBoxComponent->getRight()
                        < presetNext->getRight()
                    && presetNext->getRight()
                        < presetSave->getRight()
                    && presetSave->getRight()
                        < presetUndo->getRight()
                    && presetUndo->getRight()
                        < presetRedo->getRight(),
                "preset header order is not previous/browser/next/save/undo/redo");
            test.expect (
                presetBoxComponent->getWidth() <= 190,
                "preset browser is wider than the compact guide layout");
            auto* productTitleLabel =
                dynamic_cast<juce::Label*> (productTitle);
            test.expect (
                std::abs (
                    presetBoxComponent->getBounds().getCentreX()
                    - editor.getLocalBounds().getCentreX()) <= 1
                    && productTitle->getRight()
                           < presetPrevious->getX()
                    && brandLogo->getX()
                           > presetRedo->getRight()
                    && brandLogo->getHeight()
                           > presetBoxComponent->getHeight()
                    && productTitleLabel != nullptr
                    && productTitleLabel->getFont().getHeight()
                           >= 21.0f
                    && static_cast<int> (
                           brandLogo->getProperties()[
                               juce::Identifier (
                                   "echo-copy-count")])
                           == 3,
                "header is not left-title / centred preset / "
                "right delayed-logo hierarchy");
            test.expect (
                dynamic_cast<juce::DrawableButton*> (presetSave) != nullptr
                    && dynamic_cast<juce::DrawableButton*> (presetUndo)
                           != nullptr
                    && dynamic_cast<juce::DrawableButton*> (presetRedo)
                           != nullptr,
                "save/undo/redo are not vector icon buttons");
        }

        auto* presetCombo =
            dynamic_cast<juce::ComboBox*> (presetBoxComponent);
        test.expect (
            presetCombo != nullptr,
            "preset browser is not a ComboBox");
        if (presetCombo != nullptr)
        {
            presetCombo->hidePopup();
            const auto presetBeforeSpace =
                presetCombo->getSelectedId();
            const auto spaceWasConsumed = presetCombo->keyPressed (
                juce::KeyPress (juce::KeyPress::spaceKey));
            test.expect (
                ! spaceWasConsumed
                    && ! presetCombo->isPopupActive()
                    && presetCombo->getSelectedId()
                           == presetBeforeSpace,
                "preset browser consumed Space or opened its popup");
            test.expect (
                ! presetCombo->getWantsKeyboardFocus()
                    && ! presetCombo
                            ->getMouseClickGrabsKeyboardFocus(),
                "preset browser can steal DAW keyboard focus");

            const std::array<juce::String, 12> expectedFolders {
                "ESSENTIAL ECHO", "SLAP & DOUBLE",
                "RHYTHM & MULTITAP", "PING PONG & STEREO",
                "TAPE & ANALOG", "DIGITAL & LO-FI",
                "COMB & RESONANCE", "SPIRAL & SHIFT",
                "SPACE & DIFFUSION", "GRAIN & FREEZE",
                "PERFORMANCE & UTILITY", "HYBRID & EXTREME"
            };
            auto submenuCount = 0;
            auto rootLeafCount = 0;
            std::set<int> factoryLeafIds;
            juce::PopupMenu::MenuItemIterator rootIterator (
                *presetCombo->getRootMenu(), false);
            while (rootIterator.next())
            {
                auto& item = rootIterator.getItem();
                if (item.subMenu == nullptr)
                {
                    if (item.itemID != 0)
                        ++rootLeafCount;
                    continue;
                }

                if (submenuCount
                    < static_cast<int> (expectedFolders.size()))
                {
                    test.expect (
                        item.text
                            == expectedFolders[static_cast<size_t> (
                                submenuCount)],
                        "Factory logical folder order changed");
                    auto leafCount = 0;
                    juce::PopupMenu::MenuItemIterator leafIterator (
                        *item.subMenu, true);
                    while (leafIterator.next())
                    {
                        const auto& leaf = leafIterator.getItem();
                        if (leaf.itemID > 0 && leaf.isEnabled)
                        {
                            ++leafCount;
                            factoryLeafIds.insert (leaf.itemID);
                        }
                    }
                    test.expect (
                        leafCount == 10,
                        "Factory logical folder does not contain 10 presets");
                }
                else
                {
                    test.expect (
                        item.text == "USER",
                        "thirteenth preset logical folder is not USER");
                }

                ++submenuCount;
            }

            test.expect (
                submenuCount == 13 && rootLeafCount == 0,
                "preset root is not thirteen folder-only submenus");
            test.expect (
                factoryLeafIds.size() == 120,
                "Factory submenu leaf IDs are missing or duplicated");
        }

        auto hasTextLoadButton = false;
        for (int child = 0; child < editor.getNumChildComponents();
             ++child)
            if (auto* button = dynamic_cast<juce::Button*> (
                    editor.getChildComponent (child)))
                hasTextLoadButton =
                    hasTextLoadButton
                    || button->getButtonText().equalsIgnoreCase ("LOAD");
        test.expect (
            ! hasTextLoadButton,
            "a separate LOAD button is still present");

        setPlainValue (
            processor, TBDelayParameterIDs::stereoBypass, 1.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::colorSpaceBypass, 1.0f);
        setPlainValue (
            processor,
            TBDelayParameterIDs::grainTransformBypass, 1.0f);
        juce::Timer::callPendingTimersSynchronously();
        test.expect (
            quadrantControlsFound
                && freezeControl != nullptr
                && ! quadrantWidthControl->isEnabled()
                && ! quadrantColorControl->isEnabled()
                && ! quadrantGrainControl->isEnabled()
                && freezeControl->isEnabled(),
            "module bypass does not dim its controls while preserving independent FREEZE");
        for (const auto* id : bypassIDs)
            if (auto* control = editor.findChildWithID (
                    "toggle-" + juce::String (id)))
                test.expect (
                    control->isEnabled(),
                    "a module bypass disabled its own header switch");
        setPlainValue (
            processor, TBDelayParameterIDs::stereoBypass, 0.0f);
        setPlainValue (
            processor, TBDelayParameterIDs::colorSpaceBypass, 0.0f);
        setPlainValue (
            processor,
            TBDelayParameterIDs::grainTransformBypass, 0.0f);
        juce::Timer::callPendingTimersSynchronously();

        juce::Image firstMap;
        juce::Image repeatedMap;
        juce::Image changedMap;
        if (delayMap != nullptr)
        {
            firstMap = delayMap->createComponentSnapshot (
                delayMap->getLocalBounds(), true, 1.0f);
            repeatedMap = delayMap->createComponentSnapshot (
                delayMap->getLocalBounds(), true, 1.0f);
        }

        const auto changedPixelCount = [] (
            const juce::Image& first,
            const juce::Image& second)
        {
            if (! first.isValid() || ! second.isValid()
                || first.getBounds() != second.getBounds())
                return std::numeric_limits<std::size_t>::max();

            std::size_t changed = 0;
            for (int y = 0; y < first.getHeight(); ++y)
                for (int x = 0; x < first.getWidth(); ++x)
                    if (first.getPixelAt (x, y).getARGB()
                        != second.getPixelAt (x, y).getARGB())
                        ++changed;
            return changed;
        };
        const auto staticDifference =
            changedPixelCount (firstMap, repeatedMap);
        test.expect (
            staticDifference == 0,
            "Delay Map changes without a parameter change");

        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::spaceMacro, 100.0f),
            "failed to automate SPACE macro for the Delay Map");
        juce::Timer::callPendingTimersSynchronously();
        const auto macroMap =
            delayMap != nullptr
                ? delayMap->createComponentSnapshot (
                      delayMap->getLocalBounds(), true, 1.0f)
                : juce::Image {};
        const auto macroDifference =
            changedPixelCount (firstMap, macroMap);
        setPlainValue (
            processor, TBDelayParameterIDs::spaceMacro, 0.0f);
        juce::Timer::callPendingTimersSynchronously();
        const auto restoredMacroMap =
            delayMap != nullptr
                ? delayMap->createComponentSnapshot (
                      delayMap->getLocalBounds(), true, 1.0f)
                : juce::Image {};
        test.expect (
            macroDifference
                    != std::numeric_limits<std::size_t>::max()
                && macroDifference > 64
                && changedPixelCount (firstMap, restoredMacroMap) == 0,
            "Delay Map does not share the effective SPACE derivation or BASE restore");

        const auto findKnobSlider = [&editor] (const juce::String& id)
            -> juce::Slider*
        {
            if (auto* knob = editor.findChildWithID ("knob-" + id))
                for (int child = 0;
                     child < knob->getNumChildComponents();
                     ++child)
                    if (auto* candidate = dynamic_cast<juce::Slider*> (
                            knob->getChildComponent (child)))
                        return candidate;
            return nullptr;
        };
        const auto findChoiceSlider = [&editor] (
            const juce::String& id) -> juce::Slider*
        {
            if (auto* choice =
                    editor.findChildWithID ("choice-" + id))
                for (int child = 0;
                     child < choice->getNumChildComponents();
                     ++child)
                    if (auto* candidate =
                            dynamic_cast<juce::Slider*> (
                                choice->getChildComponent (child)))
                        return candidate;
            return nullptr;
        };
        const auto findDescendantWithID = [] (
            auto&& self,
            juce::Component& parent,
            const juce::String& id) -> juce::Component*
        {
            for (int child = 0;
                 child < parent.getNumChildComponents();
                 ++child)
            {
                auto* component =
                    parent.getChildComponent (child);
                if (component->getComponentID() == id)
                    return component;
                if (auto* found =
                        self (self, *component, id))
                    return found;
            }
            return nullptr;
        };

        auto* timeSlider = findKnobSlider (TBDelayParameterIDs::time);
        auto* divisionSlider =
            findChoiceSlider (TBDelayParameterIDs::division);
        auto* shiftSlider = findKnobSlider (TBDelayParameterIDs::shift);
        auto* timeControl =
            editor.findChildWithID ("knob-time");
        auto* divisionControl =
            editor.findChildWithID ("choice-division");
        auto* timeModeBox =
            dynamic_cast<juce::ComboBox*> (
                findDescendantWithID (
                    findDescendantWithID, editor, "time_mode"));
        auto* syncDelayTimeValue =
            dynamic_cast<juce::Label*> (
                findDescendantWithID (
                    findDescendantWithID, editor,
                    "choice-value-division"));
        test.expect (
            timeSlider != nullptr,
            "DELAY TIME Free/Tune slider was not found");
        test.expect (
            divisionSlider != nullptr,
            "DELAY TIME Sync division slider was not found");
        test.expect (
            shiftSlider != nullptr,
            "SHIFT knob slider was not found through its component ID");
        test.expect (
            timeControl != nullptr
                && ! timeControl->isVisible()
                && divisionControl != nullptr
                && divisionControl->isVisible()
                && timeControl->getBounds()
                       == divisionControl->getBounds()
                && timeModeBox != nullptr
                && timeModeBox->getNumItems() == 3
                && syncDelayTimeValue != nullptr
                && syncDelayTimeValue->getText()
                       .equalsIgnoreCase ("1/8 D"),
            "Sync mode is not one mode dropdown plus "
            "one integrated DELAY TIME tile");
        test.expect (
            timeModeBox != nullptr
                && timeModeBox->getSelectedItemIndex() == 1
                && timeModeBox->getText().equalsIgnoreCase ("Sync"),
            "TIME MODE dropdown does not show the recalled Sync state");
        if (timeModeBox != nullptr)
        {
            timeModeBox->setSelectedItemIndex (
                2, juce::sendNotificationSync);
            juce::Timer::callPendingTimersSynchronously();
            auto* tuneDelayTimeValue =
                dynamic_cast<juce::Label*> (
                    findDescendantWithID (
                        findDescendantWithID, editor,
                        "value-time"));
            test.expect (
                std::abs (
                    getPlainValue (
                        processor,
                        TBDelayParameterIDs::timeMode)
                    - 2.0f) < 0.001f
                    && timeControl != nullptr
                    && timeControl->isVisible()
                    && divisionControl != nullptr
                    && ! divisionControl->isVisible()
                    && tuneDelayTimeValue != nullptr
                    && tuneDelayTimeValue->getText()
                           .containsIgnoreCase ("Hz"),
                "Tune recall/edit is not represented by the integrated DELAY TIME tile");
        }
        if (timeModeBox != nullptr)
        {
            timeModeBox->setSelectedItemIndex (
                1, juce::sendNotificationSync);
            juce::Timer::callPendingTimersSynchronously();
            test.expect (
                std::abs (
                    getPlainValue (
                        processor,
                        TBDelayParameterIDs::timeMode)
                    - 1.0f) < 0.001f
                    && timeControl != nullptr
                    && ! timeControl->isVisible()
                    && divisionControl != nullptr
                    && divisionControl->isVisible(),
                "TIME MODE dropdown did not return to Sync");
        }

        const auto snapshotMap = [delayMap]
        {
            return delayMap != nullptr
                       ? delayMap->createComponentSnapshot (
                             delayMap->getLocalBounds(), true, 1.0f)
                       : juce::Image {};
        };
        const auto anchorPoint = [delayMap] (
            const char* xProperty,
            const char* yProperty)
        {
            if (delayMap == nullptr)
                return juce::Point<int> (-1, -1);
            return juce::Point<int> (
                static_cast<int> (
                    delayMap->getProperties()[
                        juce::Identifier (xProperty)]),
                static_cast<int> (
                    delayMap->getProperties()[
                        juce::Identifier (yProperty)]));
        };
        const auto changedNearAnchors = [] (
            const juce::Image& first,
            const juce::Image& second,
            const std::array<juce::Point<int>, 4>& points)
        {
            if (! first.isValid() || ! second.isValid()
                || first.getBounds() != second.getBounds())
                return std::numeric_limits<std::size_t>::max();

            std::size_t changed = 0;
            for (const auto point : points)
            {
                const auto window = juce::Rectangle<int> (
                    point.x - 6, point.y - 13, 13, 27)
                                        .getIntersection (
                                            first.getBounds());
                for (auto y = window.getY();
                     y < window.getBottom(); ++y)
                    for (auto x = window.getX();
                         x < window.getRight(); ++x)
                        if (first.getPixelAt (x, y).getARGB()
                            != second.getPixelAt (x, y).getARGB())
                            ++changed;
            }
            return changed;
        };

        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::time, 820.0f),
            "failed to change raw TIME in Sync mode");
        juce::Timer::callPendingTimersSynchronously();
        const auto syncTimeMap = snapshotMap();
        const auto syncTimeDifference =
            changedPixelCount (firstMap, syncTimeMap);
        test.expect (
            syncTimeDifference == 0,
            "raw TIME changed the Delay Map while Sync mode owns timing");

        if (divisionSlider != nullptr)
            divisionSlider->setValue (
                11.0, juce::sendNotificationSync);
        test.expect (
            std::abs (
                getPlainValue (
                    processor, TBDelayParameterIDs::division)
                - 11.0f) < 0.001f,
            "integrated DELAY TIME gesture did not update Sync division");
        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::division, 12.0f),
            "failed to change Sync DIVISION");
        juce::Timer::callPendingTimersSynchronously();
        const auto syncDivisionMap = snapshotMap();
        const auto syncDivisionDifference =
            changedPixelCount (syncTimeMap, syncDivisionMap);
        test.expect (
            syncDivisionDifference
                    != std::numeric_limits<std::size_t>::max()
                && syncDivisionDifference > 64,
            "DIVISION did not move the Delay Map in Sync mode");

        const std::array<std::pair<const char*, float>, 9>
            freeAnchorState {{
                { TBDelayParameterIDs::timeMode, 0.0f },
                { TBDelayParameterIDs::time, 160.0f },
                { TBDelayParameterIDs::style, 0.0f },
                { TBDelayParameterIDs::taps, 0.0f },
                { TBDelayParameterIDs::offset, 0.0f },
                { TBDelayParameterIDs::feedback, 0.0f },
                { TBDelayParameterIDs::modDepth, 0.0f },
                { TBDelayParameterIDs::shift, 0.0f },
                { TBDelayParameterIDs::grain, 0.0f }
            }};
        test.expect (
            applyValues (processor, freeAnchorState),
            "failed to set deterministic Free anchor state");
        juce::Timer::callPendingTimersSynchronously();
        auto* freeDelayTimeValue =
            dynamic_cast<juce::Label*> (
                findDescendantWithID (
                    findDescendantWithID, editor,
                    "value-time"));
        test.expect (
            timeControl != nullptr
                && timeControl->isVisible()
                && divisionControl != nullptr
                && ! divisionControl->isVisible()
                && freeDelayTimeValue != nullptr
                && freeDelayTimeValue->getText().contains ("ms"),
            "Free mode does not expose the integrated DELAY TIME tile in ms");
        const auto freeTimeStartMap = snapshotMap();
        const auto freeStartLeft = anchorPoint (
            "primary-anchor-l-x", "primary-anchor-l-y");
        const auto freeStartRight = anchorPoint (
            "primary-anchor-r-x", "primary-anchor-r-y");
        const auto leftAnchorWidth = delayMap != nullptr
            ? static_cast<int> (delayMap->getProperties()[
                  juce::Identifier ("primary-anchor-l-width")])
            : 0;
        const auto rightAnchorWidth = delayMap != nullptr
            ? static_cast<int> (delayMap->getProperties()[
                  juce::Identifier ("primary-anchor-r-width")])
            : 0;
        test.expect (
            leftAnchorWidth >= 5
                && rightAnchorWidth >= 5,
            "first L/R delay anchors do not begin with wide bars");

        if (timeSlider != nullptr)
            timeSlider->setValue (
                920.0, juce::sendNotificationSync);
        test.expect (
            std::abs (
                getPlainValue (processor, TBDelayParameterIDs::time)
                - 920.0f) < 0.01f,
            "Free DELAY TIME gesture did not update APVTS");
        juce::Timer::callPendingTimersSynchronously();
        const auto freeTimeEndMap = snapshotMap();
        const auto freeEndLeft = anchorPoint (
            "primary-anchor-l-x", "primary-anchor-l-y");
        const auto freeEndRight = anchorPoint (
            "primary-anchor-r-x", "primary-anchor-r-y");
        const auto freeTimeDifference =
            changedPixelCount (
                freeTimeStartMap, freeTimeEndMap);
        const auto freeAnchorDifference =
            changedNearAnchors (
                freeTimeStartMap, freeTimeEndMap,
                { freeStartLeft, freeStartRight,
                  freeEndLeft, freeEndRight });
        test.expect (
            freeTimeDifference
                    != std::numeric_limits<std::size_t>::max()
                && freeTimeDifference > 64
                && freeAnchorDifference > 16
                && freeEndLeft.x > freeStartLeft.x,
            "Free TIME did not visibly move the first L/R delay anchors");

        if (shiftSlider != nullptr)
            shiftSlider->setValue (
                420.0, juce::sendNotificationSync);
        test.expect (
            std::abs (
                getPlainValue (processor, TBDelayParameterIDs::shift)
                - 420.0f) < 0.01f,
            "SHIFT knob gesture did not update APVTS");

        const std::array<std::pair<const char*, float>, 12>
            grainAnchorState {{
                { TBDelayParameterIDs::timeMode, 0.0f },
                { TBDelayParameterIDs::time, 160.0f },
                { TBDelayParameterIDs::style, 0.0f },
                { TBDelayParameterIDs::taps, 0.0f },
                { TBDelayParameterIDs::offset, 0.0f },
                { TBDelayParameterIDs::feedback, 0.0f },
                { TBDelayParameterIDs::modDepth, 0.0f },
                { TBDelayParameterIDs::shift, 0.0f },
                { TBDelayParameterIDs::grain, 100.0f },
                { TBDelayParameterIDs::grainSize, 500.0f },
                { TBDelayParameterIDs::spray, 0.0f },
                { TBDelayParameterIDs::pitch, 12.0f }
            }};
        test.expect (
            applyValues (processor, grainAnchorState)
                && setPlainValue (
                    processor,
                    TBDelayParameterIDs::reverse, 0.0f),
            "failed to set pure-grain anchor state");
        juce::Timer::callPendingTimersSynchronously();
        const auto grainAnchorStartMap = snapshotMap();
        const auto grainStartLeft = anchorPoint (
            "primary-anchor-l-x", "primary-anchor-l-y");
        const auto grainStartRight = anchorPoint (
            "primary-anchor-r-x", "primary-anchor-r-y");
        test.expect (
            setPlainValue (
                processor, TBDelayParameterIDs::time, 400.0f),
            "failed to move pure-grain primary TIME");
        juce::Timer::callPendingTimersSynchronously();
        const auto grainAnchorEndMap = snapshotMap();
        const auto grainEndLeft = anchorPoint (
            "primary-anchor-l-x", "primary-anchor-l-y");
        const auto grainEndRight = anchorPoint (
            "primary-anchor-r-x", "primary-anchor-r-y");
        const auto pureGrainAnchorDifference =
            changedNearAnchors (
                grainAnchorStartMap, grainAnchorEndMap,
                { grainStartLeft, grainStartRight,
                  grainEndLeft, grainEndRight });
        test.expect (
            pureGrainAnchorDifference > 16
                && grainEndLeft.x > grainStartLeft.x,
            "primary delay anchor disappeared at 100% Grain");

        const std::array<std::pair<const char*, float>, 13> mapState {{
            { TBDelayParameterIDs::timeMode, 0.0f },
            { TBDelayParameterIDs::time, 820.0f },
            { TBDelayParameterIDs::style, 1.0f },
            { TBDelayParameterIDs::taps, 2.0f },
            { TBDelayParameterIDs::spacing, 145.0f },
            { TBDelayParameterIDs::swing, 32.0f },
            { TBDelayParameterIDs::feedback, 88.0f },
            { TBDelayParameterIDs::shift, 420.0f },
            { TBDelayParameterIDs::diverge, 100.0f },
            { TBDelayParameterIDs::grain, 78.0f },
            { TBDelayParameterIDs::grainSize, 210.0f },
            { TBDelayParameterIDs::spray, 82.0f },
            { TBDelayParameterIDs::pitch, 12.0f }
        }};
        test.expect (
            applyValues (processor, mapState),
            "failed to apply Delay Map visual state");
        juce::Timer::callPendingTimersSynchronously();
        if (delayMap != nullptr)
            changedMap = delayMap->createComponentSnapshot (
                delayMap->getLocalBounds(), true, 1.0f);

        const auto parameterDifference =
            changedPixelCount (firstMap, changedMap);
        test.expect (
            parameterDifference != std::numeric_limits<std::size_t>::max()
                && parameterDifference > 128,
            "active Delay Map state did not update visible pixels");

        juce::Image activeImage (
            juce::Image::ARGB,
            TB_DelayAudioProcessorEditor::baseWidth,
            TB_DelayAudioProcessorEditor::baseHeight, true,
            juce::SoftwareImageType());
        juce::Graphics activeGraphics (activeImage);
        editor.paintEntireComponent (activeGraphics, true);

        const auto outputDirectory =
            getOutputDirectory (projectRoot).getChildFile ("ui-qa");
        const auto file =
            outputDirectory.getChildFile ("TB_Delay_UI.png");
        const auto activeFile =
            outputDirectory.getChildFile (
                "TB_Delay_UI_DelayMap_Active.png");
        outputDirectory.createDirectory();
        const auto writePng = [] (
            const juce::Image& source,
            const juce::File& destination)
        {
            destination.deleteFile();
            juce::FileOutputStream stream (destination);
            juce::PNGImageFormat png;
            const auto wrote = stream.openedOk()
                               && png.writeImageToStream (
                                   source, stream);
            stream.flush();
            return wrote;
        };
        const auto wrote = writePng (image, file);
        const auto wroteActive = writePng (activeImage, activeFile);

        auto* previousButton =
            dynamic_cast<juce::Button*> (presetPrevious);
        auto* nextButton =
            dynamic_cast<juce::Button*> (presetNext);
        if (presetCombo != nullptr
            && previousButton != nullptr
            && nextButton != nullptr)
        {
            auto allFolderBoundariesWork = true;
            for (int boundary = 10; boundary < 120; boundary += 10)
            {
                presetCombo->setSelectedId (
                    boundary, juce::sendNotificationSync);
                nextButton->onClick();
                allFolderBoundariesWork =
                    allFolderBoundariesWork
                    && presetCombo->getSelectedId() == boundary + 1;
                previousButton->onClick();
                allFolderBoundariesWork =
                    allFolderBoundariesWork
                    && presetCombo->getSelectedId() == boundary;
            }
            test.expect (
                allFolderBoundariesWork,
                "preset navigation failed across a logical folder boundary");
        }

        // Resizing has to scale the whole face rather than stretch one part
        // of it, so at 1.5x both the Delay Map and a board knob grow by the
        // same factor and everything stays inside the window.
        auto* scaleKnob = findBoardKnob (TBDelayParameterIDs::feedback);
        const auto baseMapWidth =
            delayMap != nullptr ? delayMap->getWidth() : 0;
        const auto baseKnobWidth =
            scaleKnob != nullptr ? scaleKnob->getWidth() : 0;
        editor.setSize (
            juce::roundToInt (
                TB_DelayAudioProcessorEditor::baseWidth * 1.5f),
            juce::roundToInt (
                TB_DelayAudioProcessorEditor::baseHeight * 1.5f));
        editor.resized();
        juce::Image scaledImage (
            juce::Image::ARGB, editor.getWidth(), editor.getHeight(),
            true, juce::SoftwareImageType());
        juce::Graphics scaledGraphics (scaledImage);
        editor.paintEntireComponent (scaledGraphics, true);
        const auto scaledFile =
            outputDirectory.getChildFile ("TB_Delay_UI_Scaled_150.png");
        test.expect (
            writePng (scaledImage, scaledFile)
                && scaledFile.existsAsFile(),
            "150% scaled UI PNG write failed");
        const auto scaledMapWidth =
            delayMap != nullptr ? delayMap->getWidth() : 0;
        const auto scaledKnobWidth =
            scaleKnob != nullptr ? scaleKnob->getWidth() : 0;
        test.expect (
            baseMapWidth > 0 && baseKnobWidth > 0
                && std::abs (
                       scaledMapWidth
                       - juce::roundToInt (baseMapWidth * 1.5f)) <= 6
                && std::abs (
                       scaledKnobWidth
                       - juce::roundToInt (baseKnobWidth * 1.5f)) <= 6
                && delayMap != nullptr
                && editor.getLocalBounds().contains (
                       delayMap->getBounds()),
            "editor did not scale as a whole when resized");

        // The size the user drags to belongs to the session, so an editor
        // opened on restored state comes back at that scale instead of at the
        // minimum the constrainer would otherwise impose.
        juce::MemoryBlock scaledState;
        processor.getStateInformation (scaledState);
        Processor restoredProcessor;
        restoredProcessor.setStateInformation (
            scaledState.getData(),
            static_cast<int> (scaledState.getSize()));
        TB_DelayAudioProcessorEditor restoredEditor (restoredProcessor);
        test.expect (
            std::abs (restoredEditor.getWidth() - editor.getWidth()) <= 2
                && std::abs (
                       restoredEditor.getHeight() - editor.getHeight()) <= 2,
            "a restored session did not reopen the editor at the saved scale");

        editor.setSize (
            TB_DelayAudioProcessorEditor::baseWidth,
            TB_DelayAudioProcessorEditor::baseHeight);
        editor.resized();

        processor.releaseResources();
        test.expect (
            editor.getWidth() == TB_DelayAudioProcessorEditor::baseWidth
                && editor.getHeight()
                       == TB_DelayAudioProcessorEditor::baseHeight,
            "editor did not retain its base size");
        test.expect (wrote && file.existsAsFile(),
                     "PNG write failed");
        test.expect (
            wroteActive && activeFile.existsAsFile(),
            "active Delay Map PNG write failed");
        test.expect (file.getSize() > 10000,
                     "PNG is unexpectedly small: "
                         + std::to_string (file.getSize()) + " bytes");
        test.expect (sampledColours.size() >= 8,
                     "snapshot appears blank or nearly uniform");
        test.expect (
            vividPurplePixelCount <= vividPurplePixelLimit,
            "vivid purple returned to the neutral-dark design: "
                + std::to_string (vividPurplePixelCount)
                + " pixels, limit "
                + std::to_string (vividPurplePixelLimit));
        test.report (
            "path=" + toStdString (file.getFullPathName())
            + " bytes=" + std::to_string (file.getSize())
            + " sampledColours="
            + std::to_string (sampledColours.size())
            + " vividPurplePixels="
            + std::to_string (vividPurplePixelCount)
            + " mapStaticDiff="
            + std::to_string (staticDifference)
            + " macroMapDiff="
            + std::to_string (macroDifference)
            + " syncTimeDiff="
            + std::to_string (syncTimeDifference)
            + " syncDivisionDiff="
            + std::to_string (syncDivisionDifference)
            + " freeTimeDiff="
            + std::to_string (freeTimeDifference)
            + " freeAnchorDiff="
            + std::to_string (freeAnchorDifference)
            + " pureGrainAnchorDiff="
            + std::to_string (pureGrainAnchorDifference)
            + " mapParameterDiff="
            + std::to_string (parameterDifference)
            + " active="
            + toStdString (activeFile.getFullPathName()));
    });
}

juce::File findGuideAssets (const juce::File& projectRoot)
{
    std::array<juce::File, 3> candidates {
        projectRoot.getParentDirectory()
            .getChildFile ("Guide MD").getChildFile ("Assets"),
        projectRoot.getChildFile ("Guide MD").getChildFile ("Assets"),
        juce::File ("E:\\Plugin Projects\\Guide MD\\Assets")
    };

    for (const auto& candidate : candidates)
        if (candidate.getChildFile ("KEY_01_pad.wav").existsAsFile()
            && candidate.getChildFile ("GTR_01_acoustic.wav").existsAsFile()
            && candidate.getChildFile ("DRM_01_loop.wav").existsAsFile())
            return candidate;

    return {};
}

struct AssetRenderResult
{
    bool ok = false;
    std::string error;
    SignalMetrics input;
    SignalMetrics output;
    SignalMetrics outputActive;
    SignalMetrics matched;
    SignalMetrics matchedActive;
    double matchGain = 1.0;
    double comparisonGain = 1.0;
    juce::File dryFile;
    juce::File wetFile;
    juce::File matchedFile;
};

AssetRenderResult renderAsset (
    const juce::File& source,
    const juce::File& destination,
    int presetIndex)
{
    AssetRenderResult result;
    const auto directory = destination.getParentDirectory();
    const auto stem = destination.getFileNameWithoutExtension();
    result.dryFile = directory.getChildFile (stem + "__Dry.wav");
    result.wetFile = directory.getChildFile (stem + "__Wet.wav");
    result.matchedFile =
        directory.getChildFile (stem + "__Wet_RMS_Matched.wav");
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (source));

    if (reader == nullptr)
    {
        result.error = "reader creation failed";
        return result;
    }

    if (reader->lengthInSamples <= 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
    {
        result.error = "unsupported source length";
        return result;
    }

    const auto channels = juce::jlimit (
        1, 2, static_cast<int> (reader->numChannels));
    const auto inputSamples = static_cast<int> (reader->lengthInSamples);
    const auto tailSamples = static_cast<int> (
        std::round (reader->sampleRate * 4.0));
    juce::AudioBuffer<float> input (
        channels, inputSamples + tailSamples);
    input.clear();

    if (! reader->read (
            &input, 0, inputSamples, 0, true, channels > 1))
    {
        result.error = "WAV read failed";
        return result;
    }

    const auto sourceMetrics = measureSignal (input, 0, inputSamples);
    Processor processor;
    processor.applyFactoryPreset (presetIndex, false);
    if (! configureChannels (processor, channels))
    {
        result.error = "processor rejected source channel layout";
        return result;
    }

    auto output = processBuffer (
        processor, input, reader->sampleRate, 512);
    const auto rawOutputActive = measureSignal (output, 0, inputSamples);
    auto matched = output;
    if (sourceMetrics.rms > 1.0e-12 && rawOutputActive.rms > 1.0e-12)
        result.matchGain = sourceMetrics.rms / rawOutputActive.rms;
    matched.applyGain (static_cast<float> (result.matchGain));

    const auto matchedBeforeHeadroom = measureSignal (matched);
    const auto largestComparisonPeak =
        std::max (sourceMetrics.peak, matchedBeforeHeadroom.peak);
    if (largestComparisonPeak > 0.98)
        result.comparisonGain = 0.98 / largestComparisonPeak;

    input.applyGain (static_cast<float> (result.comparisonGain));
    output.applyGain (static_cast<float> (result.comparisonGain));
    matched.applyGain (static_cast<float> (result.comparisonGain));

    result.input = measureSignal (input, 0, inputSamples);
    result.output = measureSignal (output);
    result.outputActive = measureSignal (output, 0, inputSamples);
    result.matched = measureSignal (matched);
    result.matchedActive = measureSignal (matched, 0, inputSamples);

    directory.createDirectory();
    juce::WavAudioFormat wav;
    const auto channelSet = channels == 1
                                ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo();
    const auto options =
        juce::AudioFormatWriterOptions()
            .withSampleRate (reader->sampleRate)
            .withChannelLayout (channelSet)
            .withBitsPerSample (24);
    const auto writeWav = [&] (const juce::File& file,
                               const juce::AudioBuffer<float>& buffer)
    {
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;
        auto writer = wav.createWriterFor (stream, options);
        return writer != nullptr
               && writer->writeFromAudioSampleBuffer (
                   buffer, 0, buffer.getNumSamples());
    };

    result.ok = writeWav (result.dryFile, input)
                && writeWav (result.wetFile, output)
                && writeWav (result.matchedFile, matched);
    if (! result.ok)
        result.error = "Dry/Wet/RMS-matched WAV write failed";
    return result;
}

bool writeShortsSnapshot (
    int presetIndex,
    const juce::File& destination)
{
    Processor processor;
    processor.applyFactoryPreset (presetIndex, false);
    processor.prepareToPlay (testSampleRate, testBlockSize);
    TB_DelayAudioProcessorEditor editor (processor);
    editor.setSize (
        TB_DelayAudioProcessorEditor::baseWidth,
        TB_DelayAudioProcessorEditor::baseHeight);
    editor.resized();
    juce::Thread::sleep (50);
    juce::Timer::callPendingTimersSynchronously();

    juce::Image image (
        juce::Image::ARGB,
        TB_DelayAudioProcessorEditor::baseWidth,
        TB_DelayAudioProcessorEditor::baseHeight, true,
        juce::SoftwareImageType());
    juce::Graphics graphics (image);
    editor.paintEntireComponent (graphics, true);

    destination.getParentDirectory().createDirectory();
    destination.deleteFile();
    juce::FileOutputStream stream (destination);
    juce::PNGImageFormat png;
    const auto wrote = stream.openedOk()
                       && png.writeImageToStream (image, stream);
    stream.flush();
    return wrote;
}

int renderShortsAssets (
    const juce::File& source,
    const juce::File& outputDirectory)
{
    if (! source.existsAsFile())
    {
        std::cerr << "[FAIL] shorts render source does not exist: "
                  << source.getFullPathName() << "\n";
        return 2;
    }

    struct Job
    {
        const char* factoryPresetName;
        const char* outputName;
    };

    const std::array<Job, 4> jobs {{
        { "Clean Dotted Eighth", "Clean_Dotted_Eighth" },
        { "Spiral Ascender", "Spiral_Ascender" },
        { "Grain Cloud", "Grain_Cloud" },
        { "Warm Reel Echo", "Warm_Reel_Echo" }
    }};
    const auto audioDirectory = outputDirectory.getChildFile ("audio");
    const auto uiDirectory = outputDirectory.getChildFile ("ui");
    audioDirectory.createDirectory();
    uiDirectory.createDirectory();

    for (const auto& job : jobs)
    {
        const auto presetIndex =
            findFactoryPresetIndex (job.factoryPresetName);
        if (presetIndex < 0)
        {
            std::cerr << "[FAIL] shorts preset is missing: "
                      << job.factoryPresetName << "\n";
            return 1;
        }

        const auto baseName =
            source.getFileNameWithoutExtension()
            + "__" + job.outputName + ".wav";
        const auto rendered = renderAsset (
            source,
            audioDirectory.getChildFile (baseName),
            presetIndex);
        if (! rendered.ok)
        {
            std::cerr << "[FAIL] shorts audio render failed: "
                      << job.factoryPresetName << " — "
                      << rendered.error << "\n";
            return 1;
        }

        const auto snapshot = uiDirectory.getChildFile (
            "TB_Delay_" + juce::String (job.outputName) + ".png");
        if (! writeShortsSnapshot (presetIndex, snapshot))
        {
            std::cerr << "[FAIL] shorts UI snapshot failed: "
                      << job.factoryPresetName << "\n";
            return 1;
        }

        std::cout << "[PASS] " << job.factoryPresetName
                  << "\n       audio="
                  << rendered.matchedFile.getFullPathName()
                  << "\n       ui="
                  << snapshot.getFullPathName() << "\n";
    }

    return 0;
}

void testAbRenders (TestRunner& runner,
                    const juce::File& projectRoot)
{
    const auto assets = findGuideAssets (projectRoot);
    if (assets == juce::File())
    {
        runner.skip (
            "real-material A/B renders",
            "Guide MD/Assets with KEY_01, GTR_01, and DRM_01 was not found");
        return;
    }

    runner.run ("real-material Init/shift/grain A/B renders",
                [&projectRoot, assets] (TestCase& test)
    {
        struct Job
        {
            const char* inputName;
            const char* factoryPresetName;
            const char* outputName;
        };

        const std::array<Job, 3> jobs {{
            { "KEY_01_pad.wav", "Init", "Init" },
            { "GTR_01_acoustic.wav",
              "Spiral Ascender", "Spiral_Ascender" },
            { "DRM_01_loop.wav", "Grain Cloud", "Grain_Cloud" }
        }};
        const auto outputDirectory =
            getOutputDirectory (projectRoot).getChildFile ("ab");

        for (const auto& job : jobs)
        {
            const auto source = assets.getChildFile (job.inputName);
            const auto presetIndex =
                findFactoryPresetIndex (job.factoryPresetName);
            test.expect (
                presetIndex >= 0,
                std::string ("A/B preset is missing: ")
                    + job.factoryPresetName);
            if (presetIndex < 0)
                continue;
            const auto baseName =
                source.getFileNameWithoutExtension()
                + "__" + job.outputName + ".wav";
            const auto rendered = renderAsset (
                source, outputDirectory.getChildFile (baseName),
                presetIndex);

            test.expect (
                rendered.ok,
                toStdString (source.getFileName())
                    + " render failed: " + rendered.error);
            test.expect (
                rendered.input.finite && rendered.output.finite,
                toStdString (source.getFileName())
                    + " contains non-finite input/output");
            test.expect (
                rendered.output.peak <= 8.0,
                toStdString (source.getFileName())
                    + " output peak exceeds 8.0: "
                    + number (rendered.output.peak, 5));
            test.expect (
                rendered.output.rms > 1.0e-8,
                toStdString (source.getFileName())
                    + " rendered silence");
            const auto matchErrorDb = gainToDecibels (
                (rendered.matchedActive.rms + 1.0e-12)
                / (rendered.input.rms + 1.0e-12));
            test.expect (
                std::isfinite (rendered.matchGain)
                    && std::abs (matchErrorDb) <= 1.0,
                toStdString (source.getFileName())
                    + " active RMS match error="
                    + number (matchErrorDb, 3) + " dB");
            test.expect (
                rendered.input.peak <= 0.981
                    && rendered.output.peak <= 0.981
                    && rendered.matched.peak <= 0.981,
                toStdString (source.getFileName())
                    + " comparison render exceeds peak headroom");
            test.report (
                toStdString (source.getFileName()) + " -> "
                + job.factoryPresetName
                + " inputPeak=" + number (rendered.input.peak, 6)
                + " inputRMS=" + number (rendered.input.rms, 6)
                + " outputPeak=" + number (rendered.output.peak, 6)
                + " outputRMS=" + number (rendered.output.rms, 6)
                + " activeWetRMS=" + number (rendered.outputActive.rms, 6)
                + " matchGain=" + number (rendered.matchGain, 6)
                + " comparisonGain=" + number (rendered.comparisonGain, 6)
                + " matchError=" + number (matchErrorDb, 3) + " dB"
                + " matchedPeak=" + number (rendered.matched.peak, 6)
                + " finite="
                + (rendered.output.finite ? "yes" : "no")
                + " dry=" + toStdString (rendered.dryFile.getFullPathName())
                + " wet=" + toStdString (rendered.wetFile.getFullPathName())
                + " matched="
                + toStdString (rendered.matchedFile.getFullPathName()));
        }
    });
}

void testOptionalVst3 (TestRunner& runner, const juce::String& path)
{
    if (path.isEmpty())
    {
        runner.skip (
            "optional VST3 scan and instantiate",
            "pass a built .vst3 bundle path as argv[1] to enable this test");
        return;
    }

#if JUCE_PLUGINHOST_VST3
    runner.run ("optional VST3 scan and instantiate", [&path] (TestCase& test)
    {
        const juce::File bundle (path);
        test.expect (bundle.isDirectory(),
                     "bundle does not exist: "
                         + toStdString (bundle.getFullPathName()));
        if (! bundle.isDirectory())
            return;

        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (
            descriptions, bundle.getFullPathName());
        test.expect (descriptions.size() == 1,
                     "expected one VST3 class, found "
                         + std::to_string (descriptions.size()));
        if (descriptions.size() != 1)
            return;

        const auto& description = *descriptions[0];
       #if JUCE_MAC
        constexpr auto expectedVst3Name = "TB-Delay";
       #else
        constexpr auto expectedVst3Name = "TB_Delay";
       #endif
        test.expect (description.name == expectedVst3Name,
                     "unexpected name: "
                         + toStdString (description.name));
        test.expect (
            description.category.containsIgnoreCase ("Delay"),
            "category does not identify Delay: "
                + toStdString (description.category));
        juce::String error;
        auto instance = format.createInstanceFromDescription (
            description, testSampleRate, 512, error);
        test.expect (instance != nullptr,
                     "instance creation failed: " + toStdString (error));
        if (instance == nullptr)
            return;

        const auto defaultInputs =
            instance->getMainBusNumInputChannels();
        const auto defaultOutputs =
            instance->getMainBusNumOutputChannels();
        test.expect (
            defaultInputs == 2 && defaultOutputs == 2,
            "instantiated main bus is not 2-in/2-out");
        juce::AudioProcessor::BusesLayout monoToStereo;
        monoToStereo.inputBuses.add (
            juce::AudioChannelSet::mono());
        monoToStereo.outputBuses.add (
            juce::AudioChannelSet::stereo());
        test.expect (
            instance->setBusesLayout (monoToStereo)
                && instance->getMainBusNumInputChannels() == 1
                && instance->getMainBusNumOutputChannels() == 2,
            "instantiated VST3 rejected mono-in/stereo-out");
        instance->prepareToPlay (testSampleRate, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        buffer.clear();
        juce::MidiBuffer midi;
        instance->processBlock (buffer, midi);
        const auto metrics = measureSignal (buffer);
        instance->releaseResources();
        test.expect (metrics.finite,
                     "instantiated VST3 produced NaN/Inf on silence");
        test.report (
            "name=" + toStdString (description.name)
            + " category=" + toStdString (description.category)
            + " descriptionIO=" + std::to_string (description.numInputChannels)
            + "->" + std::to_string (description.numOutputChannels)
            + " defaultIO="
            + std::to_string (defaultInputs)
            + "->"
            + std::to_string (defaultOutputs)
            + " monoStereoIO="
            + std::to_string (instance->getMainBusNumInputChannels())
            + "->"
            + std::to_string (instance->getMainBusNumOutputChannels())
            + " silencePeak=" + number (metrics.peak, 9));
    });
#else
    runner.skip (
        "optional VST3 scan and instantiate",
        "JUCE_PLUGINHOST_VST3 is disabled in this build");
#endif
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    TestRunner runner;
    const auto projectRoot = findProjectRoot();

    if (projectRoot == juce::File())
    {
        std::cerr << "[FAIL] setup\n"
                  << "       TB_Delay project root could not be found\n";
        return 1;
    }

    std::cout << "TB_Delay processor harness\n"
              << "projectRoot=" << projectRoot.getFullPathName() << "\n\n"
              << std::flush;

    if (argc == 4
        && juce::String::fromUTF8 (argv[1]) == "--render-shorts")
        return renderShortsAssets (
            juce::File (juce::String::fromUTF8 (argv[2])),
            juce::File (juce::String::fromUTF8 (argv[3])));

    testParameters (runner);
    testMusicalMacros (runner);
    testFactoryPresets (runner);
    testFactoryPresetAudioSafety (runner);
    testImpulseTiming (runner);
    testTimeChangeAutomation (runner);
    testPingPong (runner);
    testCombTune (runner);
    testFrequencyShift (runner);
    testDeterministicGrain (runner);
    testLongGrainHistoryBoundary (runner);
    testFeedbackStability (runner);
    testFreezeHold (runner);
    testModuleBypasses (runner);
    testPanicTailInvalidation (runner);
    testMonoSampleRateBlockMatrix (runner);
    testStateRoundTrip (runner);
    testUserPresetRoundTrip (runner, projectRoot);
    testLegacyStateAndPresetMigration (runner, projectRoot);
    testUndo (runner);
    testDelayMapPrediction (runner);
    testTapTempo (runner);
    testUiSnapshot (runner, projectRoot);
    testAbRenders (runner, projectRoot);
    testOptionalVst3 (
        runner,
        argc >= 2 ? juce::String::fromUTF8 (argv[1])
                  : juce::String());

    return runner.finish();
}
