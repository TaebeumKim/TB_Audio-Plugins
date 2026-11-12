#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int sampleCount = 96000;
constexpr int blockSize = 257;

struct StereoSignal
{
    std::vector<float> left;
    std::vector<float> right;
};

struct TestLog
{
    void check (bool passed, const char* name, double value)
    {
        std::cout << (passed ? "PASS  " : "FAIL  ") << name << "  value=" << value << '\n';
        failures += passed ? 0 : 1;
    }

    int failures = 0;
};

void setParameter (TB_CenterAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getValueTreeState().getParameter (id);
    if (parameter == nullptr)
        throw std::runtime_error (std::string ("Missing parameter: ") + id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float getParameter (TB_CenterAudioProcessor& processor, const char* id)
{
    const auto* value = processor.getValueTreeState().getRawParameterValue (id);
    return value != nullptr ? value->load() : std::numeric_limits<float>::quiet_NaN();
}

StereoSignal render (float intensity, float response, float lookaheadMs, float width,
                     const std::function<std::array<float, 2> (int)>& generator,
                     int totalSamples = sampleCount,
                     float timeAlignPercent = 100.0f)
{
    TB_CenterAudioProcessor processor;
    setParameter (processor, TB_CenterAudioProcessor::intensityParamId, intensity);
    setParameter (processor, TB_CenterAudioProcessor::responseParamId, response);
    setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, lookaheadMs);
    setParameter (processor, TB_CenterAudioProcessor::widthParamId, width);
    setParameter (processor, TB_CenterAudioProcessor::timeAlignParamId, timeAlignPercent);
    processor.prepareToPlay (testSampleRate, blockSize);

    StereoSignal output;
    output.left.resize (static_cast<size_t> (totalSamples));
    output.right.resize (static_cast<size_t> (totalSamples));
    juce::MidiBuffer midi;

    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        const auto count = juce::jmin (blockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer (2, count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto input = generator (offset + sample);
            buffer.setSample (0, sample, input[0]);
            buffer.setSample (1, sample, input[1]);
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < count; ++sample)
        {
            output.left[static_cast<size_t> (offset + sample)] = buffer.getSample (0, sample);
            output.right[static_cast<size_t> (offset + sample)] = buffer.getSample (1, sample);
        }
    }
    return output;
}

StereoSignal renderMovingPanWithBlockSize (int configuredBlockSize, int totalSamples = 24000)
{
    TB_CenterAudioProcessor processor;
    setParameter (processor, TB_CenterAudioProcessor::intensityParamId, 115.0f);
    setParameter (processor, TB_CenterAudioProcessor::responseParamId, 72.0f);
    setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 17.0f);
    setParameter (processor, TB_CenterAudioProcessor::widthParamId, 121.0f);
    processor.prepareToPlay (testSampleRate, configuredBlockSize);

    StereoSignal output;
    output.left.resize (static_cast<size_t> (totalSamples));
    output.right.resize (static_cast<size_t> (totalSamples));
    juce::MidiBuffer midi;

    for (int offset = 0; offset < totalSamples; offset += configuredBlockSize)
    {
        const auto count = juce::jmin (configuredBlockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer (2, count);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto absoluteSample = offset + sample;
            const auto time = static_cast<double> (absoluteSample) / testSampleRate;
            const auto spatialAngle = 70.0 * std::sin (
                juce::MathConstants<double>::twoPi * 0.73 * time);
            const auto channelAngle = juce::degreesToRadians (45.0 + spatialAngle * 0.5);
            const auto value = 0.3 * std::sin (
                juce::MathConstants<double>::twoPi * 997.0 * time);
            buffer.setSample (0, sample, static_cast<float> (value * std::cos (channelAngle)));
            buffer.setSample (1, sample, static_cast<float> (value * std::sin (channelAngle)));
        }
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < count; ++sample)
        {
            output.left[static_cast<size_t> (offset + sample)] = buffer.getSample (0, sample);
            output.right[static_cast<size_t> (offset + sample)] = buffer.getSample (1, sample);
        }
    }
    return output;
}

void processSilence (TB_CenterAudioProcessor& processor, int totalSamples,
                     int configuredBlockSize)
{
    juce::MidiBuffer midi;
    for (int offset = 0; offset < totalSamples; offset += configuredBlockSize)
    {
        const auto count = juce::jmin (configuredBlockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer (2, count);
        buffer.clear();
        processor.processBlock (buffer, midi);
    }
}

int renderImpulseLatency (TB_CenterAudioProcessor& processor, int totalSamples,
                          int configuredBlockSize)
{
    juce::MidiBuffer midi;
    auto firstNonZero = -1;
    for (int offset = 0; offset < totalSamples; offset += configuredBlockSize)
    {
        const auto count = juce::jmin (configuredBlockSize, totalSamples - offset);
        juce::AudioBuffer<float> buffer (2, count);
        buffer.clear();
        if (offset == 0)
            buffer.setSample (0, 0, 0.5f);
        processor.processBlock (buffer, midi);
        for (int sample = 0; sample < count; ++sample)
            if (firstNonZero < 0
                && (std::abs (buffer.getSample (0, sample)) > 1.0e-7f
                    || std::abs (buffer.getSample (1, sample)) > 1.0e-7f))
                firstNonZero = offset + sample;
    }
    return firstNonZero;
}

std::function<std::array<float, 2> (int)> pannedSine (float spatialAngleDegrees,
                                                      float frequency = 997.0f,
                                                      float amplitude = 0.4f)
{
    const auto channelAngle = juce::degreesToRadians (45.0f + spatialAngleDegrees * 0.5f);
    const auto leftGain = std::cos (channelAngle);
    const auto rightGain = std::sin (channelAngle);
    return [=] (int sample)
    {
        const auto value = amplitude * static_cast<float> (
            std::sin (juce::MathConstants<double>::twoPi * frequency * sample / testSampleRate));
        return std::array<float, 2> { value * leftGain, value * rightGain };
    };
}

double rms (const std::vector<float>& signal, size_t start = 24000)
{
    double sum = 0.0;
    auto count = 0;
    for (size_t index = juce::jmin (start, signal.size()); index < signal.size(); ++index)
    {
        sum += static_cast<double> (signal[index]) * signal[index];
        ++count;
    }
    return std::sqrt (sum / juce::jmax (1, count));
}

double stereoEnergy (const StereoSignal& signal, size_t start = 24000)
{
    double sum = 0.0;
    auto count = 0;
    for (size_t index = juce::jmin (start, signal.left.size()); index < signal.left.size(); ++index)
    {
        sum += static_cast<double> (signal.left[index]) * signal.left[index]
               + static_cast<double> (signal.right[index]) * signal.right[index];
        ++count;
    }
    return sum / juce::jmax (1, count);
}

double rmsDifference (const std::vector<float>& a, const std::vector<float>& b,
                      size_t start = 24000)
{
    double sum = 0.0;
    auto count = 0;
    for (size_t index = juce::jmin (start, juce::jmin (a.size(), b.size()));
         index < juce::jmin (a.size(), b.size()); ++index)
    {
        const auto difference = static_cast<double> (a[index]) - b[index];
        sum += difference * difference;
        ++count;
    }
    return std::sqrt (sum / juce::jmax (1, count));
}

double outputAngleDegrees (const StereoSignal& signal)
{
    const auto leftRms = rms (signal.left);
    const auto rightRms = rms (signal.right);
    return 2.0 * (juce::radiansToDegrees (std::atan2 (rightRms, leftRms)) - 45.0);
}

int bestCrossCorrelationLag (const StereoSignal& signal, size_t start,
                             size_t count, int maximumLag)
{
    const auto begin = juce::jmin (start + static_cast<size_t> (maximumLag),
                                   signal.left.size());
    const auto end = juce::jmin (begin + count,
                                 signal.left.size() - static_cast<size_t> (maximumLag));
    auto bestLag = 0;
    auto bestCorrelation = -std::numeric_limits<double>::infinity();

    for (int lag = -maximumLag; lag <= maximumLag; ++lag)
    {
        double cross = 0.0;
        double leftEnergy = 1.0e-12;
        double rightEnergy = 1.0e-12;
        for (auto index = begin; index < end; ++index)
        {
            const auto rightIndex = static_cast<int> (index) + lag;
            const auto leftSample = static_cast<double> (signal.left[index]);
            const auto rightSample = static_cast<double> (signal.right[
                static_cast<size_t> (rightIndex)]);
            cross += leftSample * rightSample;
            leftEnergy += leftSample * leftSample;
            rightEnergy += rightSample * rightSample;
        }
        const auto correlation = cross / std::sqrt (leftEnergy * rightEnergy);
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }
    return bestLag;
}

double maximumAbsoluteSample (const StereoSignal& signal)
{
    double maximum = 0.0;
    for (size_t index = 0; index < signal.left.size(); ++index)
    {
        maximum = juce::jmax (maximum, std::abs (static_cast<double> (signal.left[index])));
        maximum = juce::jmax (maximum, std::abs (static_cast<double> (signal.right[index])));
    }
    return maximum;
}

double monoWindowRms (const StereoSignal& signal, size_t start, size_t count)
{
    double sum = 0.0;
    const auto end = juce::jmin (signal.left.size(), start + count);
    for (auto index = start; index < end; ++index)
    {
        const auto mid = (static_cast<double> (signal.left[index]) + signal.right[index])
                         * 0.7071067811865475244;
        sum += mid * mid;
    }
    return std::sqrt (sum / juce::jmax<size_t> (1, end - start));
}

double monoToneAmplitude (const StereoSignal& signal, double frequency,
                          size_t start, size_t count)
{
    double sineProjection = 0.0;
    double cosineProjection = 0.0;
    const auto end = juce::jmin (signal.left.size(), start + count);
    for (auto index = start; index < end; ++index)
    {
        const auto mid = (static_cast<double> (signal.left[index]) + signal.right[index])
                         * 0.7071067811865475244;
        const auto phase = juce::MathConstants<double>::twoPi * frequency
                           * static_cast<double> (index) / testSampleRate;
        sineProjection += mid * std::sin (phase);
        cosineProjection += mid * std::cos (phase);
    }
    return 2.0 * std::hypot (sineProjection, cosineProjection)
           / static_cast<double> (juce::jmax<size_t> (1, end - start));
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    TestLog tests;

    const auto leftInput = pannedSine (-90.0f);
    const auto centredFromLeft = render (100.0f, 100.0f, 0.0f, 100.0f, leftInput);
    const auto leftAngle = outputAngleDegrees (centredFromLeft);
    tests.check (std::abs (leftAngle) < 2.0, "hard-left source moves to centre", leftAngle);

    const auto rightInput = pannedSine (90.0f);
    const auto centredFromRight = render (100.0f, 100.0f, 0.0f, 100.0f, rightInput);
    const auto rightAngle = outputAngleDegrees (centredFromRight);
    tests.check (std::abs (rightAngle) < 2.0, "hard-right source moves to centre", rightAngle);

    {
        auto maximumCrossing = 0.0;
        auto intensityDoesNotCrossCentre = true;
        for (const auto intensity : { 100.0f, 120.0f, 200.0f })
        {
            const auto leftResult = render (intensity, 100.0f, 0.0f, 100.0f, leftInput);
            const auto rightResult = render (intensity, 100.0f, 0.0f, 100.0f, rightInput);
            const auto fromLeft = outputAngleDegrees (leftResult);
            const auto fromRight = outputAngleDegrees (rightResult);
            maximumCrossing = juce::jmax (maximumCrossing,
                                          juce::jmax (fromLeft, -fromRight));
            intensityDoesNotCrossCentre = intensityDoesNotCrossCentre
                && fromLeft <= 1.0 && fromRight >= -1.0
                && std::abs (fromLeft) < 2.0 && std::abs (fromRight) < 2.0;
        }
        tests.check (intensityDoesNotCrossCentre,
                     "intensity above 100 percent strengthens lock without crossing centre",
                     maximumCrossing);
    }

    const auto centreInput = pannedSine (0.0f);
    const auto centredInputOutput = render (100.0f, 100.0f, 0.0f, 100.0f, centreInput);
    const auto referenceCentre = render (0.0f, 100.0f, 0.0f, 100.0f, centreInput);
    const auto centreError = rmsDifference (centredInputOutput.left, referenceCentre.left);
    tests.check (centreError < 1.0e-6, "already-centred input remains transparent", centreError);

    const auto independentStereo = [] (int sample)
    {
        return std::array<float, 2> {
            0.31f * static_cast<float> (std::sin (
                juce::MathConstants<double>::twoPi * 431.0 * sample / testSampleRate)),
            0.23f * static_cast<float> (std::sin (
                juce::MathConstants<double>::twoPi * 719.0 * sample / testSampleRate))
        };
    };
    const auto intensityZero = render (0.0f, 50.0f, 0.0f, 100.0f, independentStereo);
    std::vector<float> inputLeft (sampleCount), inputRight (sampleCount);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto input = independentStereo (sample);
        inputLeft[static_cast<size_t> (sample)] = input[0];
        inputRight[static_cast<size_t> (sample)] = input[1];
    }
    const auto transparentError = juce::jmax (
        rmsDifference (intensityZero.left, inputLeft),
        rmsDifference (intensityZero.right, inputRight));
    tests.check (transparentError < 1.0e-6,
                 "intensity zero is delayed-wet unity, not a mix", transparentError);

    const auto protectedIndependentStereo = render (
        100.0f, 100.0f, 0.0f, 100.0f, independentStereo);
    const auto leftToneRatio = monoToneAmplitude (protectedIndependentStereo, 431.0, 48000, 48000)
                               / monoToneAmplitude (intensityZero, 431.0, 48000, 48000);
    const auto rightToneRatio = monoToneAmplitude (protectedIndependentStereo, 719.0, 48000, 48000)
                                / monoToneAmplitude (intensityZero, 719.0, 48000, 48000);
    const auto minimumIndependentToneRatio = juce::jmin (leftToneRatio, rightToneRatio);
    tests.check (minimumIndependentToneRatio > 0.7,
                 "incoherent stereo components survive mono summing",
                 minimumIndependentToneRatio);

    const auto monoWidth = render (0.0f, 50.0f, 0.0f, 0.0f, independentStereo);
    const auto sideError = rmsDifference (monoWidth.left, monoWidth.right);
    tests.check (sideError < 1.0e-7, "width zero produces mono output", sideError);

    const auto leftEnergyInput = render (0.0f, 100.0f, 0.0f, 100.0f, leftInput);
    const auto energyRatio = stereoEnergy (centredFromLeft) / stereoEnergy (leftEnergyInput);
    tests.check (std::abs (energyRatio - 1.0) < 1.0e-5,
                 "orthogonal centring preserves stereo energy at width 100", energyRatio);

    const auto antiPhase = [] (int sample)
    {
        const auto value = 0.3f * static_cast<float> (std::sin (
            juce::MathConstants<double>::twoPi * 997.0 * sample / testSampleRate));
        return std::array<float, 2> { value, -value };
    };
    const auto antiPhaseOutput = render (200.0f, 100.0f, 0.0f, 100.0f, antiPhase);
    const auto antiPhaseEnergy = stereoEnergy (antiPhaseOutput);
    tests.check (std::isfinite (antiPhaseEnergy) && antiPhaseEnergy > 0.01
                     && maximumAbsoluteSample (antiPhaseOutput) < 1.0,
                 "negative correlation is finite and not forced into cancellation",
                 antiPhaseEnergy);

    const auto itdPannedSignal = [] (int sample)
    {
        const auto source = [] (int index)
        {
            if (index < 0)
                return 0.0f;
            const auto time = static_cast<double> (index) / testSampleRate;
            return 0.22f * static_cast<float> (std::sin (
                       juce::MathConstants<double>::twoPi * 211.0 * time))
                   + 0.18f * static_cast<float> (std::sin (
                       juce::MathConstants<double>::twoPi * 1231.0 * time))
                   + 0.13f * static_cast<float> (std::sin (
                       juce::MathConstants<double>::twoPi * 3719.0 * time))
                   + 0.09f * static_cast<float> (std::sin (
                       juce::MathConstants<double>::twoPi * 8123.0 * time));
        };
        constexpr int rightDelaySamples = 12;
        return std::array<float, 2> {
            source (sample) * 0.86f,
            source (sample - rightDelaySamples) * 0.55f
        };
    };
    const auto itdDry = render (0.0f, 100.0f, 0.0f, 100.0f, itdPannedSignal,
                                sampleCount, 0.0f);
    const auto itdOutput = render (100.0f, 100.0f, 0.0f, 100.0f, itdPannedSignal);
    const auto dryItdLag = bestCrossCorrelationLag (itdDry, 48000, 24000, 24);
    const auto outputItdLag = bestCrossCorrelationLag (itdOutput, 48000, 24000, 24);
    tests.check (std::abs (dryItdLag - 12) <= 1 && std::abs (outputItdLag) <= 2,
                 "confidence-gated ITD alignment recentres a delayed stereo image",
                 outputItdLag);

    const auto nearQuadratureLeftDominant = [] (int sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * 997.0 * sample / testSampleRate;
        return std::array<float, 2> {
            0.4f * static_cast<float> (std::sin (phase)),
            0.2f * static_cast<float> (std::sin (phase + juce::MathConstants<double>::halfPi
                                                 + juce::degreesToRadians (0.001)))
        };
    };
    const auto nearQuadratureInput = render (
        0.0f, 100.0f, 0.0f, 100.0f, nearQuadratureLeftDominant);
    const auto nearQuadratureOutput = render (
        100.0f, 100.0f, 0.0f, 100.0f, nearQuadratureLeftDominant);
    const auto nearQuadratureMonoRatio = monoWindowRms (nearQuadratureOutput, 48000, 24000)
                                         / monoWindowRms (nearQuadratureInput, 48000, 24000);
    tests.check (nearQuadratureMonoRatio >= 0.99,
                 "tiny negative cross-energy does not flip the correction direction",
                 nearQuadratureMonoRatio);

    const auto jumpingPosition = [] (int sample)
    {
        const auto value = 0.4f * static_cast<float> (std::sin (
            juce::MathConstants<double>::twoPi * 997.0 * sample / testSampleRate));
        if (sample < 48000 || sample >= 96000)
            return std::array<float, 2> { value, 0.0f };
        return std::array<float, 2> { 0.0f, value };
    };
    const auto jumpingInput = render (
        0.0f, 50.0f, 0.0f, 100.0f, jumpingPosition, 144000);
    auto minimumMonoRatio = std::numeric_limits<double>::max();
    for (const auto response : { 0.0f, 50.0f, 100.0f })
    {
        const auto jumpingOutput = render (
            100.0f, response, 0.0f, 100.0f, jumpingPosition, 144000);
        for (const auto transition : { 48000, 96000 })
            for (size_t start = static_cast<size_t> (transition);
                 start < static_cast<size_t> (transition + 2400); start += 128)
            {
                const auto inputMono = monoWindowRms (jumpingInput, start, 256);
                const auto outputMono = monoWindowRms (jumpingOutput, start, 256);
                minimumMonoRatio = juce::jmin (
                    minimumMonoRatio, outputMono / juce::jmax (1.0e-9, inputMono));
            }
    }
    const auto minusThreeDb = juce::Decibels::decibelsToGain (-3.0);
    tests.check (minimumMonoRatio >= minusThreeDb,
                 "mono guard stays within 3 dB across abrupt left-right changes",
                 minimumMonoRatio);

    {
        const auto oneSampleBlocks = renderMovingPanWithBlockSize (1);
        const auto largeBlocks = renderMovingPanWithBlockSize (1024);
        const auto blockDifference = juce::jmax (
            rmsDifference (oneSampleBlocks.left, largeBlocks.left, 0),
            rmsDifference (oneSampleBlocks.right, largeBlocks.right, 0));
        tests.check (blockDifference < 1.0e-6,
                     "processing is independent of host block size",
                     blockDifference);
    }

    {
        const std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };
        const std::array<int, 5> blockSizes { 1, 16, 64, 257, 1024 };
        auto matrixPassed = true;
        auto matrixCases = 0;
        auto worstAngle = 0.0;

        for (const auto sampleRate : sampleRates)
        {
            for (const auto configuredBlockSize : blockSizes)
            {
                TB_CenterAudioProcessor processor;
                setParameter (processor, TB_CenterAudioProcessor::intensityParamId, 100.0f);
                setParameter (processor, TB_CenterAudioProcessor::responseParamId, 100.0f);
                setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
                setParameter (processor, TB_CenterAudioProcessor::widthParamId, 100.0f);
                processor.prepareToPlay (sampleRate, configuredBlockSize);

                const auto totalSamples = juce::jmax (
                    configuredBlockSize, juce::roundToInt (sampleRate * 0.18));
                double leftEnergy = 0.0;
                double rightEnergy = 0.0;
                int measuredSamples = 0;
                juce::MidiBuffer midi;

                for (int offset = 0; offset < totalSamples; offset += configuredBlockSize)
                {
                    const auto count = juce::jmin (configuredBlockSize, totalSamples - offset);
                    juce::AudioBuffer<float> buffer (2, count);
                    for (int sample = 0; sample < count; ++sample)
                    {
                        const auto absoluteSample = offset + sample;
                        const auto value = 0.3f * static_cast<float> (std::sin (
                            juce::MathConstants<double>::twoPi * 997.0
                            * absoluteSample / sampleRate));
                        buffer.setSample (0, sample, value);
                        buffer.setSample (1, sample, 0.0f);
                    }
                    processor.processBlock (buffer, midi);
                    for (int sample = 0; sample < count; ++sample)
                    {
                        const auto leftValue = buffer.getSample (0, sample);
                        const auto rightValue = buffer.getSample (1, sample);
                        matrixPassed = matrixPassed && std::isfinite (leftValue)
                                       && std::isfinite (rightValue);
                        if (offset + sample >= totalSamples / 2)
                        {
                            leftEnergy += static_cast<double> (leftValue) * leftValue;
                            rightEnergy += static_cast<double> (rightValue) * rightValue;
                            ++measuredSamples;
                        }
                    }
                }

                const auto leftRms = std::sqrt (leftEnergy / juce::jmax (1, measuredSamples));
                const auto rightRms = std::sqrt (rightEnergy / juce::jmax (1, measuredSamples));
                const auto angle = 2.0 * (juce::radiansToDegrees (
                    std::atan2 (rightRms, leftRms)) - 45.0);
                worstAngle = juce::jmax (worstAngle, std::abs (angle));
                matrixPassed = matrixPassed && std::abs (angle) < 2.0;
                ++matrixCases;
            }
        }

        tests.check (matrixPassed,
                     "44.1-192 kHz and 1-1024 sample block matrix is stable",
                     matrixPassed ? matrixCases : worstAngle);
    }

    {
        TB_CenterAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, 64);
        juce::MidiBuffer midi;
        auto finiteDuringChanges = true;
        auto maximum = 0.0f;

        for (int block = 0; block < 300; ++block)
        {
            setParameter (processor, TB_CenterAudioProcessor::intensityParamId,
                          block % 2 == 0 ? 0.0f : 200.0f);
            setParameter (processor, TB_CenterAudioProcessor::responseParamId,
                          block % 3 == 0 ? 0.0f : 100.0f);
            setParameter (processor, TB_CenterAudioProcessor::widthParamId,
                          block % 5 == 0 ? 0.0f : 200.0f);
            juce::AudioBuffer<float> buffer (2, 64);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto absoluteSample = block * 64 + sample;
                buffer.setSample (0, sample, 0.24f * static_cast<float> (std::sin (
                    juce::MathConstants<double>::twoPi * 431.0
                    * absoluteSample / testSampleRate)));
                buffer.setSample (1, sample, 0.19f * static_cast<float> (std::sin (
                    juce::MathConstants<double>::twoPi * 719.0
                    * absoluteSample / testSampleRate)));
            }
            processor.processBlock (buffer, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    const auto value = buffer.getSample (channel, sample);
                    finiteDuringChanges = finiteDuringChanges && std::isfinite (value);
                    maximum = juce::jmax (maximum, std::abs (value));
                }
        }

        tests.check (finiteDuringChanges && maximum < 2.0f,
                     "rapid automatable parameter changes remain finite",
                     maximum);
    }

    const auto silence = render (200.0f, 100.0f, 100.0f, 200.0f,
                                 [] (int) { return std::array<float, 2> { 0.0f, 0.0f }; },
                                 480000);
    tests.check (maximumAbsoluteSample (silence) == 0.0,
                 "silence remains exactly silent", maximumAbsoluteSample (silence));

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::responseParamId, 0.0f);
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 20.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        juce::MidiBuffer midi;
        const auto left = pannedSine (-90.0f);
        constexpr int activeSamples = 48000;
        constexpr int trailingSilenceSamples = 1920;
        for (int offset = 0; offset < activeSamples + trailingSilenceSamples; offset += blockSize)
        {
            const auto count = juce::jmin (
                blockSize, activeSamples + trailingSilenceSamples - offset);
            juce::AudioBuffer<float> buffer (2, count);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto absoluteSample = offset + sample;
                const auto value = absoluteSample < activeSamples
                    ? left (absoluteSample)
                    : std::array<float, 2> { 0.0f, 0.0f };
                buffer.setSample (0, sample, value[0]);
                buffer.setSample (1, sample, value[1]);
            }
            processor.processBlock (buffer, midi);
        }
        const auto snapshot = processor.getAnalysisSnapshot();
        const auto returnedToNeutral = snapshot.confidence < 0.01f
                                       && snapshot.inputRms < 1.0e-6f
                                       && snapshot.outputRms < 1.0e-6f;
        tests.check (returnedToNeutral,
                     "analysis lock returns to neutral after lookahead drains",
                     snapshot.confidence);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 20.0f);
        setParameter (processor, TB_CenterAudioProcessor::responseParamId, 100.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        const auto expectedLatency = 960;
        tests.check (processor.getLatencySamples() == expectedLatency,
                     "lookahead reports exact latency", processor.getLatencySamples());

        juce::MidiBuffer midi;
        auto firstNonZero = -1;
        for (int offset = 0; offset < 2048; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, 2048 - offset);
            juce::AudioBuffer<float> buffer (2, count);
            buffer.clear();
            if (offset == 0)
                buffer.setSample (0, 0, 0.5f);
            processor.processBlock (buffer, midi);
            for (int sample = 0; sample < count; ++sample)
                if (firstNonZero < 0
                    && (std::abs (buffer.getSample (0, sample)) > 1.0e-7f
                        || std::abs (buffer.getSample (1, sample)) > 1.0e-7f))
                    firstNonZero = offset + sample;
        }
        tests.check (firstNonZero == expectedLatency,
                     "lookahead audio delay matches reported latency", firstNonZero);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 37.0f);
        processSilence (processor, 512, blockSize);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto upLatency = renderImpulseLatency (processor, 2304, blockSize);
        const auto upMatches = processor.getLatencySamples() == 1776 && upLatency == 1776;
        tests.check (upMatches,
                     "runtime lookahead increase matches reported PDC",
                     upLatency);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 37.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
        processSilence (processor, 512, blockSize);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto downLatency = renderImpulseLatency (processor, 512, blockSize);
        const auto downMatches = processor.getLatencySamples() == 0 && downLatency == 0;
        tests.check (downMatches,
                     "runtime lookahead decrease matches reported PDC",
                     downLatency);
    }

    {
        RecenterEngine engine;
        RecenterEngine::Parameters parameters;
        parameters.lookaheadMs = 0.0f;
        engine.prepare (testSampleRate, blockSize, parameters);

        juce::AudioBuffer<float> silenceBlock (2, blockSize);
        silenceBlock.clear();
        parameters.lookaheadMs = 37.0f;
        engine.setParameters (parameters);
        engine.process (silenceBlock);
        const auto firstPending = engine.getPendingLatencySamples();

        parameters.lookaheadMs = 0.0f;
        engine.setParameters (parameters);
        const auto latestPending = engine.getPendingLatencySamples();

        engine.confirmLatencyChange (0);
        parameters.lookaheadMs = 37.0f;
        engine.setParameters (parameters);
        parameters.lookaheadMs = 0.0f;
        engine.setParameters (parameters);
        silenceBlock.clear();
        engine.process (silenceBlock);
        const auto oldMatchingConfirmationIgnored = engine.getLookaheadSamples() == 0
                                                    && engine.getPendingLatencySamples() == 0;

        engine.confirmLatencyChange (1776);
        silenceBlock.clear();
        engine.process (silenceBlock);
        const auto staleConfirmationIgnored = engine.getLookaheadSamples() == 0
                                              && engine.getPendingLatencySamples() == 0;

        engine.confirmLatencyChange (0);
        silenceBlock.clear();
        engine.process (silenceBlock);
        const auto latestConfirmationAccepted = engine.getLookaheadSamples() == 0
                                                && engine.getPendingLatencySamples() == -1;

        const auto rapidReturnSafe = firstPending == 1776 && latestPending == 0
                                     && oldMatchingConfirmationIgnored
                                     && staleConfirmationIgnored
                                     && latestConfirmationAccepted;
        tests.check (rapidReturnSafe,
                     "rapid lookahead return ignores a stale PDC confirmation",
                     latestPending);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        processor.loadFactoryPreset (1);
        processSilence (processor, 512, blockSize);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto presetLatency = renderImpulseLatency (processor, 2432, blockSize);
        const auto presetMatches = processor.getLatencySamples() == 1920
                                   && presetLatency == 1920;
        tests.check (presetMatches,
                     "preset lookahead change matches reported PDC",
                     presetLatency);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 10.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        juce::MidiBuffer midi;
        auto firstNonZero = -1;
        auto bypassStayedDry = false;
        for (int offset = 0; offset < 1024; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, 1024 - offset);
            juce::AudioBuffer<float> buffer (2, count);
            buffer.clear();
            if (offset == 0)
                buffer.setSample (0, 0, 0.5f);
            processor.processBlockBypassed (buffer, midi);
            for (int sample = 0; sample < count; ++sample)
            {
                if (firstNonZero < 0
                    && (std::abs (buffer.getSample (0, sample)) > 1.0e-7f
                        || std::abs (buffer.getSample (1, sample)) > 1.0e-7f))
                {
                    firstNonZero = offset + sample;
                    bypassStayedDry = std::abs (buffer.getSample (0, sample) - 0.5f) < 1.0e-7f
                                      && std::abs (buffer.getSample (1, sample)) < 1.0e-7f;
                }
            }
        }
        tests.check (firstNonZero == 480 && bypassStayedDry,
                     "host bypass preserves dry signal and plugin latency", firstNonZero);
        tests.check (processor.getAnalysisSnapshot().bypassed,
                     "polar state identifies host bypass",
                     processor.getAnalysisSnapshot().bypassed ? 1.0 : 0.0);
    }

    {
        TB_CenterAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, blockSize);
        const auto initialAnalysis = processor.getAnalysisSnapshot();
        const auto neutralAnalysis = initialAnalysis.inputRms == 0.0f
                                     && initialAnalysis.outputRms == 0.0f
                                     && initialAnalysis.inputAngleDegrees == 0.0f
                                     && initialAnalysis.outputAngleDegrees == 0.0f;
        tests.check (neutralAnalysis, "silence publishes a neutral polar state",
                     neutralAnalysis ? 1.0 : 0.0);
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::intensityParamId, 137.0f);
        setParameter (processor, TB_CenterAudioProcessor::responseParamId, 73.0f);
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 41.0f);
        setParameter (processor, TB_CenterAudioProcessor::widthParamId, 128.0f);
        juce::MemoryBlock savedState;
        processor.getStateInformation (savedState);
        processor.loadFactoryPreset (0);
        processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));
        const auto restored = std::abs (getParameter (processor, TB_CenterAudioProcessor::intensityParamId)
                                         - 137.0f) < 0.01f
                              && std::abs (getParameter (processor, TB_CenterAudioProcessor::responseParamId)
                                           - 73.0f) < 0.01f
                              && std::abs (getParameter (processor, TB_CenterAudioProcessor::lookaheadParamId)
                                           - 41.0f) < 0.01f
                              && std::abs (getParameter (processor, TB_CenterAudioProcessor::widthParamId)
                                           - 128.0f) < 0.01f;
        tests.check (restored, "DAW state restores every parameter", restored ? 1.0 : 0.0);

        auto malformedState = processor.getValueTreeState().copyState();
        malformedState.appendChild (
            malformedState.getChild (0).createCopy(), nullptr);
        juce::MemoryBlock malformedData;
        if (auto xml = malformedState.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, malformedData);
        processor.setStateInformation (
            malformedData.getData(), static_cast<int> (malformedData.getSize()));
        const auto duplicateRejected = std::abs (getParameter (
            processor, TB_CenterAudioProcessor::intensityParamId) - 137.0f) < 0.01f;
        tests.check (duplicateRejected, "duplicate state parameter preserves live values",
                     duplicateRejected ? 1.0 : 0.0);

        auto legacyState = processor.getValueTreeState().copyState();
        legacyState.removeProperty (TB_CenterAudioProcessor::stateSchemaVersionProperty,
                                    nullptr);
        for (auto index = legacyState.getNumChildren(); --index >= 0;)
        {
            const auto id = legacyState.getChild (index).getProperty ("id").toString();
            if (id != TB_CenterAudioProcessor::intensityParamId
                && id != TB_CenterAudioProcessor::responseParamId
                && id != TB_CenterAudioProcessor::lookaheadParamId
                && id != TB_CenterAudioProcessor::widthParamId)
                legacyState.removeChild (index, nullptr);
        }
        juce::MemoryBlock legacyData;
        if (auto xml = legacyState.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, legacyData);
        processor.loadFactoryPreset (0);
        processor.setStateInformation (
            legacyData.getData(), static_cast<int> (legacyData.getSize()));
        const auto legacyRestored = std::abs (getParameter (
            processor, TB_CenterAudioProcessor::intensityParamId) - 137.0f) < 0.01f
            && std::abs (getParameter (
                processor, TB_CenterAudioProcessor::widthParamId) - 128.0f) < 0.01f;
        tests.check (legacyRestored, "legacy four-control state remains compatible",
                     legacyRestored ? 1.0 : 0.0);

        const auto& presets = TB_CenterAudioProcessor::getFactoryPresets();
        auto allPresetsValid = presets.size() == 6;
        for (int index = 0; index < static_cast<int> (presets.size()); ++index)
        {
            processor.loadFactoryPreset (index);
            const auto& preset = presets[static_cast<size_t> (index)];
            allPresetsValid = allPresetsValid
                && std::abs (getParameter (processor, TB_CenterAudioProcessor::intensityParamId)
                             - preset.intensityPercent) < 0.01f
                && std::abs (getParameter (processor, TB_CenterAudioProcessor::responseParamId)
                             - preset.responsePercent) < 0.01f
                && std::abs (getParameter (processor, TB_CenterAudioProcessor::lookaheadParamId)
                             - preset.lookaheadMs) < 0.01f
                && std::abs (getParameter (processor, TB_CenterAudioProcessor::widthParamId)
                             - preset.widthPercent) < 0.01f;
        }
        tests.check (allPresetsValid, "all factory presets recall complete valid states",
                     static_cast<double> (presets.size()));

        processor.loadFactoryPreset (0);
        processor.getUndoManager().clearUndoHistory();
        processor.loadFactoryPreset (2);
        const auto undoPassed = processor.getUndoManager().undo()
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::intensityParamId)
                         - presets[0].intensityPercent) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::responseParamId)
                         - presets[0].responsePercent) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::lookaheadParamId)
                         - presets[0].lookaheadMs) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::widthParamId)
                         - presets[0].widthPercent) < 0.01f;
        const auto redoPassed = processor.getUndoManager().redo()
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::intensityParamId)
                         - presets[2].intensityPercent) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::responseParamId)
                         - presets[2].responsePercent) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::lookaheadParamId)
                         - presets[2].lookaheadMs) < 0.01f
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::widthParamId)
                         - presets[2].widthPercent) < 0.01f;
        tests.check (undoPassed && redoPassed, "preset load is one undo-redo transaction",
                     undoPassed && redoPassed ? 1.0 : 0.0);

        const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getNonexistentChildFile ("TB_Center_RoundTrip", ".tbcenter");
        processor.loadFactoryPreset (4);
        const auto saved = processor.savePresetToFile (presetFile);
        processor.loadFactoryPreset (0);
        const auto loaded = saved && processor.loadPresetFromFile (presetFile);
        const auto roundTrip = loaded
            && std::abs (getParameter (processor, TB_CenterAudioProcessor::widthParamId)
                         - presets[4].widthPercent) < 0.01f;
        tests.check (roundTrip, "user preset save-load round trip", roundTrip ? 1.0 : 0.0);
        presetFile.deleteFile();

        const auto invalidFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getNonexistentChildFile ("TB_Center_Invalid", ".tbcenter");
        invalidFile.replaceWithText ("<NOT_TB_CENTER/>");
        const auto valueBefore = getParameter (processor, TB_CenterAudioProcessor::widthParamId);
        const auto rejected = ! processor.loadPresetFromFile (invalidFile)
                              && std::abs (getParameter (processor,
                                                        TB_CenterAudioProcessor::widthParamId)
                                           - valueBefore) < 0.01f;
        tests.check (rejected, "invalid preset preserves the current state", rejected ? 1.0 : 0.0);
        invalidFile.deleteFile();
    }

    {
        TB_CenterAudioProcessor processor;
        setParameter (processor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
        setParameter (processor, TB_CenterAudioProcessor::responseParamId, 100.0f);
        processor.prepareToPlay (testSampleRate, blockSize);
        juce::MidiBuffer midi;
        const auto input = pannedSine (-55.0f);
        for (int offset = 0; offset < 24000; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, 24000 - offset);
            juce::AudioBuffer<float> buffer (2, count);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = input (offset + sample);
                buffer.setSample (0, sample, value[0]);
                buffer.setSample (1, sample, value[1]);
            }
            processor.processBlock (buffer, midi);
        }

        const auto polarSnapshot = processor.getAnalysisSnapshot();
        const auto polarMatchesWetOutput = polarSnapshot.inputRms > 0.01f
                                           && polarSnapshot.outputRms > 0.01f
                                           && std::abs (polarSnapshot.inputAngleDegrees + 55.0f) < 5.0f
                                           && std::abs (polarSnapshot.outputAngleDegrees) < 2.0f;
        tests.check (polarMatchesWetOutput,
                     "polar snapshot compares delayed input with final wet output",
                     polarSnapshot.outputAngleDegrees);

        const auto renderEditor = [] (TB_CenterAudioProcessor& sourceProcessor,
                                      const juce::String& fileName)
        {
            TB_CenterAudioProcessorEditor editor (sourceProcessor);
            juce::Thread::sleep (50);
            juce::Timer::callPendingTimersSynchronously();
            juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true,
                               juce::SoftwareImageType());
            juce::Graphics graphics (image);
            editor.paintEntireComponent (graphics, true);
            const auto screenshotFile = juce::File::getCurrentWorkingDirectory()
                                            .getChildFile ("output")
                                            .getChildFile ("ui-qa")
                                            .getChildFile (fileName);
            screenshotFile.getParentDirectory().createDirectory();
            screenshotFile.deleteFile();
            juce::FileOutputStream stream (screenshotFile);
            juce::PNGImageFormat png;
            return editor.getWidth() == 900 && editor.getHeight() == 610
                   && stream.openedOk() && png.writeImageToStream (image, stream);
        };

        const auto wroteActive = renderEditor (processor, "TB_Center_Debug_100pct.png");
        tests.check (wroteActive, "active 100 percent UI snapshot rendered",
                     wroteActive ? 1.0 : 0.0);

        TB_CenterAudioProcessor silentProcessor;
        silentProcessor.prepareToPlay (testSampleRate, blockSize);
        tests.check (std::abs (silentProcessor.getProcessingSampleRate() - testSampleRate) < 0.01,
                     "UI receives the prepared sample rate for latency display",
                     silentProcessor.getProcessingSampleRate());
        const auto wroteSilent = renderEditor (silentProcessor, "TB_Center_Debug_NoSignal.png");
        tests.check (wroteSilent, "no-signal UI snapshot rendered",
                     wroteSilent ? 1.0 : 0.0);

        TB_CenterAudioProcessor bypassedProcessor;
        setParameter (bypassedProcessor, TB_CenterAudioProcessor::lookaheadParamId, 0.0f);
        bypassedProcessor.prepareToPlay (testSampleRate, blockSize);
        for (int offset = 0; offset < 24000; offset += blockSize)
        {
            const auto count = juce::jmin (blockSize, 24000 - offset);
            juce::AudioBuffer<float> buffer (2, count);
            for (int sample = 0; sample < count; ++sample)
            {
                const auto value = input (offset + sample);
                buffer.setSample (0, sample, value[0]);
                buffer.setSample (1, sample, value[1]);
            }
            bypassedProcessor.processBlockBypassed (buffer, midi);
        }
        const auto wroteBypassed = renderEditor (bypassedProcessor,
                                                  "TB_Center_Debug_Bypassed.png");
        tests.check (wroteBypassed, "bypassed UI snapshot rendered",
                     wroteBypassed ? 1.0 : 0.0);
    }

    std::cout << "SUMMARY  " << (tests.failures == 0 ? "PASS" : "FAIL")
              << "  failures=" << tests.failures << '\n';
    return tests.failures == 0 ? 0 : 1;
}
