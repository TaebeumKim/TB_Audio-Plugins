#include "GranularShimmerEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float minimumToneHz = 1200.0f;
constexpr float maximumToneHz = 18000.0f;
constexpr float historyHighPassHz = 120.0f;
constexpr double safetyDistance = 12.0;
constexpr float maximumDiffuseMotionMs = 1.6f;
constexpr float inverseSqrtEight = 0.3535533905932738f;
constexpr float maximumHybridNormalisationLift = 1.41421356237f;

// Sparkle layer: short percussive "shards" of the input, transposed onto the
// harmonic series and scattered after each attack, then bounced through a
// ping-pong echo that follows Time.
constexpr float maximumTimeMs = 1200.0f;
constexpr float sparkleHighPassHz = 700.0f;
constexpr float sparkleMaximumGain = 3.2f;
constexpr float sparkleEchoLevel = 0.6f;
constexpr float sparkleEchoFeedbackScale = 0.85f;
constexpr float sparkleTailSend = 0.45f;
constexpr float sparkleEchoHeadroom = 2.0f;
constexpr float sparkleOverlapPowerAllowance = 4.0f;
constexpr float sparkleDrizzlePerSecond = 48.0f;
constexpr float sparkleBurstPerSecond = 380.0f;
constexpr float shardEnvelopeFloor = 0.002f;
constexpr float shardEnvelopeScale = 1.0f / (1.0f - shardEnvelopeFloor);
constexpr float onsetRatioThreshold = 2.2f;
constexpr float onsetAbsoluteThreshold = 0.002f;
constexpr float maximumShatterCharge = 1.5f;
constexpr float sparkleQuietThreshold = 1.0e-9f;

constexpr std::array<float, 5> shardIntervals
{{ 12.0f, 19.0f, 24.0f, 31.0f, 36.0f }};
constexpr std::array<float, 5> shardIntervalWeights
{{ 0.8f, 0.75f, 1.0f, 0.65f, 0.55f }};

float sparkleGain (float amount) noexcept
{
    return amount <= 0.0f ? 0.0f
                          : sparkleMaximumGain * std::pow (amount, 1.25f);
}

float shatterChargeSeconds (float shatter) noexcept
{
    return 0.12f + 0.38f * shatter;
}

constexpr std::array<std::array<float, 4>, 2> earlyDiffusionDelayMs
{{
    {{ 3.1f, 5.3f, 9.1f, 15.7f }},
    {{ 3.7f, 6.2f, 10.7f, 16.9f }}
}};

constexpr std::array<std::array<float, 4>, 2> earlyDiffusionCoefficients
{{
    {{ 0.57f, 0.61f, 0.66f, 0.70f }},
    {{ 0.59f, 0.63f, 0.68f, 0.72f }}
}};

constexpr std::array<float, 8> diffuseDelayRatios
{{
    0.593f, 0.727f, 0.887f, 1.031f,
    1.193f, 1.369f, 1.571f, 1.787f
}};

constexpr std::array<float, 8> diffuseWanderPeriods
{{
    0.37f, 0.43f, 0.53f, 0.61f, 0.71f, 0.83f, 0.97f, 1.13f
}};

constexpr std::array<float, 8> hadamardRowOne
{{ 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f }};
constexpr std::array<float, 8> hadamardRowTwo
{{ 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f }};
constexpr std::array<float, 8> hadamardRowFour
{{ 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, -1.0f }};

float diffuseBaseDelayMs (float sizeMs) noexcept
{
    return 12.0f + 0.20f * sizeMs;
}

float diffuseMotionMs (float scatterPercent) noexcept
{
    const auto normalised = scatterPercent * 0.01f;
    return maximumDiffuseMotionMs * std::pow (normalised, 1.5f);
}

float diffuseBlend (float density) noexcept
{
    const auto normalised = juce::jlimit (0.0f, 1.0f, (density - 2.0f) / 22.0f);
    const auto smooth = normalised * normalised * (3.0f - 2.0f * normalised);
    return 0.30f + 0.50f * smooth;
}

float earlyDiffusionBlend (float sizeMs, float density) noexcept
{
    const auto sizeNormalised = juce::jlimit (0.0f, 1.0f, (sizeMs - 1.0f) / 499.0f);
    const auto densityNormalised = juce::jlimit (0.0f, 1.0f, (density - 2.0f) / 22.0f);
    const auto smoothSize = sizeNormalised * sizeNormalised
                          * (3.0f - 2.0f * sizeNormalised);
    const auto smoothDensity = densityNormalised * densityNormalised
                             * (3.0f - 2.0f * densityNormalised);
    return 0.24f + 0.34f * (0.55f * smoothSize + 0.45f * smoothDensity);
}

bool isPrime (int value) noexcept
{
    if (value < 2)
        return false;
    if ((value & 1) == 0)
        return value == 2;

    for (int divisor = 3; divisor <= value / divisor; divisor += 2)
        if (value % divisor == 0)
            return false;

    return true;
}

int nearestPrime (int target) noexcept
{
    target = juce::jmax (2, target);
    for (int distance = 0;; ++distance)
    {
        const auto lower = target - distance;
        if (lower >= 2 && isPrime (lower))
            return lower;

        const auto upper = target + distance;
        if (isPrime (upper))
            return upper;
    }
}

float diffuseLoopGain (float feedback, float delayMs) noexcept
{
    const auto normalised = juce::jlimit (0.0f, 1.0f, feedback / 0.88f);
    if (normalised <= 1.0e-4f)
        return 0.0f;

    const auto t60Seconds = 45.0f * normalised * normalised * normalised;
    const auto gain = std::pow (10.0f, -3.0f * delayMs * 0.001f / t60Seconds);
    return juce::jlimit (0.0f, 0.9985f, gain);
}

float sinc (float value) noexcept
{
    if (std::abs (value) < 1.0e-6f)
        return 1.0f;

    const auto angle = juce::MathConstants<float>::pi * value;
    return std::sin (angle) / angle;
}
}

void GranularShimmerEngine::prepare (double newSampleRate, int maximumBlockSize,
                                     int numberOfChannels)
{
    juce::ignoreUnused (maximumBlockSize);
    sampleRate = juce::jmax (1.0, newSampleRate);
    preparedChannels = juce::jlimit (1, 2, numberOfChannels);
    historySize = static_cast<int> (std::ceil (sampleRate * historySeconds)) + sincTapCount * 2;

    for (auto& channel : history)
        channel.assign (static_cast<size_t> (historySize), 0.0f);

    for (int channel = 0; channel < 2; ++channel)
        for (int stage = 0; stage < earlyDiffusionStageCount; ++stage)
        {
            const auto delaySamples = nearestPrime (static_cast<int> (std::round (
                sampleRate * earlyDiffusionDelayMs[static_cast<size_t> (channel)]
                                                   [static_cast<size_t> (stage)]
                * 0.001)));
            earlyDiffusionBuffers[static_cast<size_t> (channel)]
                                 [static_cast<size_t> (stage)].assign (
                static_cast<size_t> (delaySamples), 0.0f);
        }

    const auto maximumBaseDelayMs = diffuseBaseDelayMs (500.0f);
    for (int line = 0; line < diffuseLineCount; ++line)
    {
        const auto maximumDelayMs = maximumBaseDelayMs
                                  * diffuseDelayRatios[static_cast<size_t> (line)]
                                  + maximumDiffuseMotionMs;
        const auto capacity = static_cast<int> (std::ceil (
            sampleRate * maximumDelayMs * 0.001)) + 4;
        diffuseBuffers[static_cast<size_t> (line)].assign (
            static_cast<size_t> (juce::jmax (8, capacity)), 0.0f);
    }

    sparkleEchoSize = static_cast<int> (std::ceil (
        sampleRate * maximumTimeMs * 0.001)) + 8;
    for (auto& channel : sparkleEchoBuffers)
        channel.assign (static_cast<size_t> (sparkleEchoSize), 0.0f);

    const auto onePoleAmount = [this] (double seconds)
    {
        return static_cast<float> (1.0 - std::exp (-1.0 / (seconds * sampleRate)));
    };
    onsetAttackAmount = onePoleAmount (0.0005);
    onsetReleaseAmount = onePoleAmount (0.015);
    onsetSlowAmount = onePoleAmount (0.060);
    onsetHoldoffSamples = juce::jmax (1, static_cast<int> (std::round (sampleRate * 0.070)));
    onsetMeasureSamples = juce::jmax (1, static_cast<int> (std::round (sampleRate * 0.005)));
    maximumOnsetAge = juce::jmax (1, static_cast<int> (std::round (sampleRate * 2.0)));
    sparkleOverlapAttackAmount = onePoleAmount (0.001);
    sparkleOverlapReleaseAmount = onePoleAmount (0.080);
    sparkleHighPassAmount = 1.0f - std::exp (
        -juce::MathConstants<float>::twoPi * sparkleHighPassHz
        / static_cast<float> (sampleRate));

    for (int index = 0; index < windowTableSize; ++index)
    {
        const auto phase = static_cast<float> (index) / static_cast<float> (windowTableSize - 1);
        windowTable[static_cast<size_t> (index)] =
            0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
    }

    sincTable.resize (static_cast<size_t> (pitchBinCount * fractionalPhaseCount * sincTapCount));
    for (int pitchBin = 0; pitchBin < pitchBinCount; ++pitchBin)
    {
        const auto semitones = static_cast<float> (pitchBin - 12);
        const auto ratio = std::pow (2.0f, semitones / 12.0f);
        const auto cutoff = 0.46f / juce::jmax (1.0f, ratio);

        for (int phaseIndex = 0; phaseIndex < fractionalPhaseCount; ++phaseIndex)
        {
            const auto fraction = static_cast<float> (phaseIndex)
                                / static_cast<float> (fractionalPhaseCount);
            auto coefficientSum = 0.0f;

            for (int tap = 0; tap < sincTapCount; ++tap)
            {
                const auto distance = static_cast<float> (tap - 3) - fraction;
                const auto window = std::abs (distance) < 4.0f
                    ? 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * distance / 4.0f)
                    : 0.0f;
                const auto coefficient = 2.0f * cutoff * sinc (2.0f * cutoff * distance) * window;
                const auto tableIndex = ((pitchBin * fractionalPhaseCount + phaseIndex)
                                          * sincTapCount) + tap;
                sincTable[static_cast<size_t> (tableIndex)] = coefficient;
                coefficientSum += coefficient;
            }

            if (std::abs (coefficientSum) > 1.0e-8f)
                for (int tap = 0; tap < sincTapCount; ++tap)
                {
                    const auto tableIndex = ((pitchBin * fractionalPhaseCount + phaseIndex)
                                              * sincTapCount) + tap;
                    sincTable[static_cast<size_t> (tableIndex)] /= coefficientSum;
                }
        }
    }

    feedbackSmoother.reset (sampleRate, 0.05);
    toneSmoother.reset (sampleRate, 0.04);
    earlyDiffusionBlendSmoother.reset (sampleRate, 0.08);
    diffuseDelaySmoother.reset (sampleRate, 4.0);
    diffuseMotionSmoother.reset (sampleRate, 0.12);
    diffuseBlendSmoother.reset (sampleRate, 0.08);
    for (auto& smoother : diffuseGainSmoothers)
        smoother.reset (sampleRate, 0.05);
    widthSmoother.reset (sampleRate, 0.03);
    mixSmoother.reset (sampleRate, 0.02);
    bypassSmoother.reset (sampleRate, 0.01);
    sparkleGainSmoother.reset (sampleRate, 0.03);
    sparkleEchoDelaySmoother.reset (sampleRate, 0.30);
    diffuseWanderAmount = 1.0f - std::exp (
        -1.0f / (0.12f * static_cast<float> (sampleRate)));
    reset();
}

void GranularShimmerEngine::reset() noexcept
{
    for (auto& channel : history)
        std::fill (channel.begin(), channel.end(), 0.0f);

    for (auto& grain : grains)
        grain = {};

    highPassLowState.fill (0.0f);
    toneState.fill (0.0f);
    for (auto& channel : earlyDiffusionBuffers)
        for (auto& stage : channel)
            std::fill (stage.begin(), stage.end(), 0.0f);
    for (auto& channel : earlyDiffusionWriteIndices)
        channel.fill (0);
    for (auto& line : diffuseBuffers)
        std::fill (line.begin(), line.end(), 0.0f);
    diffuseWriteIndices.fill (0);
    diffuseInterpolationInput.fill (0.0f);
    diffuseInterpolationOutput.fill (0.0f);
    diffuseDampingState.fill (0.0f);
    diffuseWander.fill (0.0f);

    for (int line = 0; line < diffuseLineCount; ++line)
    {
        auto& state = diffuseRandomStates[static_cast<size_t> (line)];
        state = 0x9e3779b9u ^ (0x85ebca6bu * static_cast<std::uint32_t> (line + 1));
        diffuseWanderTargets[static_cast<size_t> (line)] = randomBipolar (state);
        diffuseWanderCountdowns[static_cast<size_t> (line)] = juce::jmax (
            1, static_cast<int> (std::round (
                sampleRate * diffuseWanderPeriods[static_cast<size_t> (line)])));
    }

    writeIndex = 0;
    nextGrainIndex = 0;
    samplesUntilNextGrain = 0.0;
    diffuseTargetSize = -1.0f;
    diffuseTargetDensity = -1.0f;
    diffuseTargetScatter = -1.0f;
    diffuseTargetFeedback = -1.0f;
    randomState = 0x6d2b79f5u;
    levelRandomState = 0x243f6a88u;
    smoothersPrimed = false;

    for (auto& channel : sparkleEchoBuffers)
        std::fill (channel.begin(), channel.end(), 0.0f);
    sparkleEchoWriteIndex = 0;
    shardRandomState = 0x1b873593u;
    clearSparkleState();
}

void GranularShimmerEngine::clearSparkleState() noexcept
{
    for (auto& shard : shards)
        shard = {};

    sparkleHighPassState.fill (0.0f);
    sparkleEchoDampState.fill (0.0f);
    sparkleOverlapEnvelope = 0.0f;
    sparkleQuietSamples = 0;
    nextShardIndex = 0;
    onsetHoldoff = 0;
    onsetMeasureCountdown = 0;
    samplesSinceOnset = maximumOnsetAge;
    onsetFastEnvelope = 0.0f;
    onsetSlowEnvelope = 0.0f;
    onsetPeak = 0.0f;
    shatterCharge = 0.0f;
    shardSpawnPhase = 0.0f;
    shardSpawnThreshold = 1.0f;
    sparkleEngaged = false;
}

void GranularShimmerEngine::primeSmoothers (const Parameters& parameters,
                                           bool bypassed) noexcept
{
    const auto feedback = sanitise (parameters.feedbackPercent, 42.0f, 0.0f, 88.0f) * 0.01f;
    const auto tone = sanitise (parameters.toneHz, 9000.0f, minimumToneHz, maximumToneHz);
    const auto size = sanitise (parameters.sizeMs, 160.0f, 1.0f, 500.0f);
    const auto density = sanitise (parameters.density, 9.0f, 2.0f, 2000.0f);
    const auto scatter = sanitise (parameters.scatterPercent, 32.0f, 0.0f, 100.0f);
    const auto baseDelayMs = diffuseBaseDelayMs (size);
    const auto width = sanitise (parameters.widthPercent, 135.0f, 0.0f, 200.0f) * 0.01f;
    const auto mix = sanitise (parameters.mixPercent, 48.0f, 0.0f, 100.0f) * 0.01f;

    feedbackSmoother.setCurrentAndTargetValue (feedback);
    toneSmoother.setCurrentAndTargetValue (tone);
    earlyDiffusionBlendSmoother.setCurrentAndTargetValue (
        earlyDiffusionBlend (size, density));
    diffuseDelaySmoother.setCurrentAndTargetValue (baseDelayMs);
    diffuseMotionSmoother.setCurrentAndTargetValue (
        diffuseMotionMs (scatter) * 0.001f * static_cast<float> (sampleRate));
    diffuseBlendSmoother.setCurrentAndTargetValue (diffuseBlend (density));
    for (int line = 0; line < diffuseLineCount; ++line)
        diffuseGainSmoothers[static_cast<size_t> (line)].setCurrentAndTargetValue (
            diffuseLoopGain (feedback,
                             baseDelayMs * diffuseDelayRatios[static_cast<size_t> (line)]));
    diffuseTargetSize = size;
    diffuseTargetDensity = density;
    diffuseTargetScatter = scatter;
    diffuseTargetFeedback = feedback;
    widthSmoother.setCurrentAndTargetValue (width);
    mixSmoother.setCurrentAndTargetValue (mix);
    bypassSmoother.setCurrentAndTargetValue (bypassed ? 0.0f : 1.0f);
    sparkleGainSmoother.setCurrentAndTargetValue (sparkleGain (
        sanitise (parameters.sparklePercent, 0.0f, 0.0f, 100.0f) * 0.01f));
    sparkleEchoDelaySmoother.setCurrentAndTargetValue (
        sanitise (parameters.timeMs, 240.0f, 60.0f, maximumTimeMs)
        * 0.001f * static_cast<float> (sampleRate));
    smoothersPrimed = true;
}

void GranularShimmerEngine::process (juce::AudioBuffer<float>& buffer,
                                     const Parameters& parameters,
                                     bool bypassed) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto numberOfSamples = buffer.getNumSamples();
    const auto numberOfChannels = juce::jmin (preparedChannels, juce::jmin (2, buffer.getNumChannels()));
    if (numberOfSamples <= 0 || numberOfChannels <= 0 || historySize <= 0)
        return;

    if (! smoothersPrimed)
        primeSmoothers (parameters, bypassed);

    feedbackSmoother.setTargetValue (
        sanitise (parameters.feedbackPercent, 42.0f, 0.0f, 88.0f) * 0.01f);
    toneSmoother.setTargetValue (
        sanitise (parameters.toneHz, 9000.0f, minimumToneHz, maximumToneHz));
    const auto diffuseSize = sanitise (parameters.sizeMs, 160.0f, 1.0f, 500.0f);
    const auto diffuseDensity = sanitise (parameters.density, 9.0f, 2.0f, 2000.0f);
    const auto diffuseScatter = sanitise (
        parameters.scatterPercent, 32.0f, 0.0f, 100.0f);
    const auto diffuseFeedback = sanitise (
        parameters.feedbackPercent, 42.0f, 0.0f, 88.0f) * 0.01f;
    const auto sizeChanged = diffuseSize != diffuseTargetSize;
    const auto densityChanged = diffuseDensity != diffuseTargetDensity;
    const auto feedbackChanged = diffuseFeedback != diffuseTargetFeedback;
    if (sizeChanged || densityChanged)
        earlyDiffusionBlendSmoother.setTargetValue (
            earlyDiffusionBlend (diffuseSize, diffuseDensity));
    if (sizeChanged)
        diffuseDelaySmoother.setTargetValue (diffuseBaseDelayMs (diffuseSize));
    if (diffuseScatter != diffuseTargetScatter)
        diffuseMotionSmoother.setTargetValue (
            diffuseMotionMs (diffuseScatter) * 0.001f * static_cast<float> (sampleRate));
    if (densityChanged)
        diffuseBlendSmoother.setTargetValue (diffuseBlend (diffuseDensity));
    if (sizeChanged || feedbackChanged || diffuseDelaySmoother.isSmoothing())
    {
        const auto diffuseBaseMs = diffuseDelaySmoother.getCurrentValue();
        for (int line = 0; line < diffuseLineCount; ++line)
            diffuseGainSmoothers[static_cast<size_t> (line)].setTargetValue (
                diffuseLoopGain (diffuseFeedback,
                                 diffuseBaseMs * diffuseDelayRatios[static_cast<size_t> (line)]));
    }
    diffuseTargetSize = diffuseSize;
    diffuseTargetDensity = diffuseDensity;
    diffuseTargetScatter = diffuseScatter;
    diffuseTargetFeedback = diffuseFeedback;
    widthSmoother.setTargetValue (
        sanitise (parameters.widthPercent, 135.0f, 0.0f, 200.0f) * 0.01f);
    mixSmoother.setTargetValue (
        sanitise (parameters.mixPercent, 48.0f, 0.0f, 100.0f) * 0.01f);
    bypassSmoother.setTargetValue (bypassed ? 0.0f : 1.0f);
    const auto frozen = parameters.frozen;

    SparkleSettings sparkle;
    sparkle.amount = sanitise (parameters.sparklePercent, 0.0f, 0.0f, 100.0f) * 0.01f;
    sparkle.shatter = sanitise (parameters.shatterPercent, 0.0f, 0.0f, 100.0f) * 0.01f;
    const auto sparkleEchoDelay = sanitise (parameters.timeMs, 240.0f, 60.0f, maximumTimeMs)
                                * 0.001f * static_cast<float> (sampleRate);
    sparkleGainSmoother.setTargetValue (sparkleGain (sparkle.amount));
    if (sparkle.amount > 0.0f && ! sparkleEngaged)
    {
        sparkleEngaged = true;
        sparkleQuietSamples = 0;
        sparkleEchoDelaySmoother.setCurrentAndTargetValue (sparkleEchoDelay);
    }

    if (sparkleEngaged)
    {
        const auto inverseSampleRate = 1.0f / static_cast<float> (sampleRate);
        const auto densityScale = 0.2f + 0.8f * sparkle.amount;
        sparkle.chargeDecay = std::exp (
            -inverseSampleRate / shatterChargeSeconds (sparkle.shatter));
        sparkle.drizzlePerSample = sparkleDrizzlePerSecond * densityScale
                                 * (1.0f - 0.8f * sparkle.shatter) * inverseSampleRate;
        sparkle.burstPerSample = sparkleBurstPerSecond * densityScale
                               * sparkle.shatter * inverseSampleRate;
        sparkleEchoDelaySmoother.setTargetValue (sparkleEchoDelay);
    }

    std::array<float*, 2> outputPointers
    {{
        buffer.getWritePointer (0),
        numberOfChannels > 1 ? buffer.getWritePointer (1) : nullptr
    }};

    const auto highPassAmount = 1.0f - std::exp (
        -juce::MathConstants<float>::twoPi * historyHighPassHz / static_cast<float> (sampleRate));

    for (int sample = 0; sample < numberOfSamples; ++sample)
    {
        if (samplesUntilNextGrain <= 0.0)
        {
            spawnGrain (parameters);
            const auto density = sanitise (parameters.density, 9.0f, 2.0f, 2000.0f);
            samplesUntilNextGrain += sampleRate / static_cast<double> (density);
        }
        samplesUntilNextGrain -= 1.0;

        std::array<float, 2> dry
        {{
            outputPointers[0][sample],
            numberOfChannels > 1 ? outputPointers[1][sample] : outputPointers[0][sample]
        }};
        for (int channel = 0; channel < numberOfChannels; ++channel)
            if (! std::isfinite (dry[static_cast<size_t> (channel)]))
                dry[static_cast<size_t> (channel)] = 0.0f;

        std::array<float, 2> wet {};
        auto accumulatedWeight = 0.0f;
        auto accumulatedWeightSquared = 0.0f;

        for (auto& grain : grains)
        {
            if (! grain.active)
                continue;

            const auto envelope = windowAt (grain.age, grain.length);
            const auto weight = envelope * grain.levelGain;
            accumulatedWeight += weight;
            accumulatedWeightSquared += weight * weight;
            wet[0] += readHistory (0, grain.readPosition, grain.pitchBin)
                    * weight * grain.leftGain;
            if (numberOfChannels > 1)
                wet[1] += readHistory (1, grain.readPosition, grain.pitchBin)
                        * weight * grain.rightGain;

            grain.readPosition = wrapPosition (grain.readPosition + grain.increment);
            if (++grain.age >= grain.length)
                grain.active = false;
        }

        const auto coherentNormalisation = 1.0f
            / juce::jmax (1.0f, accumulatedWeight);
        const auto rmsNormalisation = 1.0f
            / std::sqrt (juce::jmax (1.0f, accumulatedWeightSquared));
        const auto hybridNormalisation = std::sqrt (
            coherentNormalisation * rmsNormalisation);
        const auto overlapNormalisation = juce::jmin (
            hybridNormalisation,
            coherentNormalisation * maximumHybridNormalisationLift);
        wet[0] *= overlapNormalisation;
        wet[1] *= overlapNormalisation;

        const auto cutoff = juce::jmin (toneSmoother.getNextValue(),
                                        static_cast<float> (sampleRate * 0.45));
        const auto lowPassAmount = 1.0f - std::exp (
            -juce::MathConstants<float>::twoPi * cutoff / static_cast<float> (sampleRate));

        for (int channel = 0; channel < numberOfChannels; ++channel)
        {
            highPassLowState[static_cast<size_t> (channel)] += highPassAmount
                * (wet[static_cast<size_t> (channel)]
                   - highPassLowState[static_cast<size_t> (channel)]);
            const auto highPassed = wet[static_cast<size_t> (channel)]
                                  - highPassLowState[static_cast<size_t> (channel)];
            toneState[static_cast<size_t> (channel)] += lowPassAmount
                * (highPassed - toneState[static_cast<size_t> (channel)]);
            wet[static_cast<size_t> (channel)] = toneState[static_cast<size_t> (channel)];
        }

        const auto feedback = feedbackSmoother.getNextValue();
        if (! frozen)
        {
            for (int channel = 0; channel < numberOfChannels; ++channel)
            {
                const auto filteredWet = wet[static_cast<size_t> (channel)];
                auto historyInput = dry[static_cast<size_t> (channel)]
                                  + feedback * softBound (filteredWet);
                if (! std::isfinite (historyInput))
                    historyInput = dry[static_cast<size_t> (channel)];
                history[static_cast<size_t> (channel)][static_cast<size_t> (writeIndex)] =
                    juce::jlimit (-8.0f, 8.0f, historyInput);
            }

            if (numberOfChannels == 1)
                history[1][static_cast<size_t> (writeIndex)] =
                    history[0][static_cast<size_t> (writeIndex)];
        }

        std::array<float, 2> sparkleOutput {};
        const auto sparkleActive = sparkleEngaged;
        if (sparkleActive)
            sparkleOutput = processSparkle (dry, parameters, sparkle, feedback,
                                            lowPassAmount, frozen, numberOfChannels);

        const auto earlyDiffused = processEarlyDiffusion (wet, numberOfChannels);
        const auto earlyBlend = earlyDiffusionBlendSmoother.getNextValue();
        const auto earlyDry = 1.0f - earlyBlend;
        const auto earlyNormalisation = 1.0f / std::sqrt (
            earlyDry * earlyDry + earlyBlend * earlyBlend);
        for (int channel = 0; channel < numberOfChannels; ++channel)
            wet[static_cast<size_t> (channel)] = earlyNormalisation
                * (earlyDry * wet[static_cast<size_t> (channel)]
                   + earlyBlend * earlyDiffused[static_cast<size_t> (channel)]);

        auto tailInput = wet;
        if (sparkleActive)
            for (int channel = 0; channel < numberOfChannels; ++channel)
                tailInput[static_cast<size_t> (channel)] +=
                    sparkleTailSend * sparkleOutput[static_cast<size_t> (channel)];

        const auto diffuseTail = processDiffuseTail (
            tailInput,
            diffuseDelaySmoother.getNextValue(),
            diffuseMotionSmoother.getNextValue(),
            lowPassAmount,
            numberOfChannels);
        const auto tailBlend = diffuseBlendSmoother.getNextValue();
        const auto tailNormalisation = 1.0f / std::sqrt (
            1.0f + tailBlend * tailBlend);
        for (int channel = 0; channel < numberOfChannels; ++channel)
            wet[static_cast<size_t> (channel)] = tailNormalisation
                * (wet[static_cast<size_t> (channel)]
                   + tailBlend * diffuseTail[static_cast<size_t> (channel)]);

        if (sparkleActive)
            for (int channel = 0; channel < numberOfChannels; ++channel)
                wet[static_cast<size_t> (channel)] +=
                    sparkleOutput[static_cast<size_t> (channel)];

        const auto width = widthSmoother.getNextValue();
        if (numberOfChannels > 1)
        {
            const auto mid = 0.5f * (wet[0] + wet[1]);
            const auto side = 0.5f * (wet[0] - wet[1]) * width;
            wet[0] = mid + side;
            wet[1] = mid - side;
        }

        const auto mix = mixSmoother.getNextValue();
        const auto effectAmount = bypassSmoother.getNextValue();
        for (int channel = 0; channel < numberOfChannels; ++channel)
        {
            const auto filteredWet = wet[static_cast<size_t> (channel)];

            const auto processed = dry[static_cast<size_t> (channel)]
                                 + mix * (filteredWet - dry[static_cast<size_t> (channel)]);
            auto output = dry[static_cast<size_t> (channel)]
                        + effectAmount * (processed - dry[static_cast<size_t> (channel)]);
            if (! std::isfinite (output))
                output = dry[static_cast<size_t> (channel)];
            outputPointers[static_cast<size_t> (channel)][sample] =
                juce::jlimit (-8.0f, 8.0f, output);
        }

        if (! frozen && ++writeIndex >= historySize)
            writeIndex = 0;
    }
}

void GranularShimmerEngine::spawnGrain (const Parameters& parameters) noexcept
{
    Grain* target = nullptr;
    for (int offset = 0; offset < maximumGrains; ++offset)
    {
        const auto index = (nextGrainIndex + offset) % maximumGrains;
        if (! grains[static_cast<size_t> (index)].active)
        {
            target = &grains[static_cast<size_t> (index)];
            nextGrainIndex = (index + 1) % maximumGrains;
            break;
        }
    }

    if (target == nullptr)
    {
        auto quietestProgress = -1.0f;
        for (int index = 0; index < maximumGrains; ++index)
        {
            auto& candidate = grains[static_cast<size_t> (index)];
            const auto progress = static_cast<float> (candidate.age)
                                / static_cast<float> (juce::jmax (1, candidate.length));
            if (progress > quietestProgress)
            {
                quietestProgress = progress;
                target = &candidate;
                nextGrainIndex = (index + 1) % maximumGrains;
            }
        }
    }

    if (target == nullptr)
        return;

    const auto levelVariationDb = sanitise (
        parameters.levelVariationDb, 0.0f, 0.0f, 12.0f);
    const auto levelRandom = static_cast<float> (advanceRandom (levelRandomState) >> 8)
                           * (1.0f / 16777216.0f);
    const auto levelGain = std::pow (10.0f,
        -levelVariationDb * levelRandom * 0.05f);

    const auto scatter = sanitise (parameters.scatterPercent, 32.0f, 0.0f, 100.0f) * 0.01f;
    const auto nominalSizeMs = sanitise (parameters.sizeMs, 160.0f, 1.0f, 500.0f);
    const auto sizeMs = nominalSizeMs
                      * (1.0f + randomBipolar() * scatter * 0.15f);
    const auto grainLength = juce::jmax (16, static_cast<int> (std::round (
        sizeMs * 0.001 * sampleRate)));
    const auto pitchRandom = randomBipolar();
    const auto pitch = juce::jlimit (-12.0f, 24.0f,
        sanitise (parameters.pitchSemitones, 12.0f, -12.0f, 24.0f)
        + pitchRandom * sanitise (
            parameters.pitchSpraySemitones, 0.112f, 0.0f, 12.0f));
    const auto ratio = std::pow (2.0, static_cast<double> (pitch) / 12.0);
    const auto reverse = randomUnit()
        < sanitise (parameters.reversePercent, 18.0f, 0.0f, 100.0f) * 0.01f;
    const auto increment = reverse ? -ratio : ratio;

    const auto baseDelay = sanitise (parameters.timeMs, 240.0f, 60.0f, 1200.0f)
                         * 0.001 * sampleRate;
    const auto positionRandom = sanitise (
        parameters.positionRandomPercent, 32.0f, 0.0f, 100.0f) * 0.01f;
    const auto nominalGrainLength = nominalSizeMs * 0.001 * sampleRate;
    const auto positionRange = juce::jmin (baseDelay * 0.8,
                                           nominalGrainLength * 2.0)
                             * positionRandom;
    auto delay = baseDelay + static_cast<double> (randomBipolar()) * positionRange;
    const auto motion = static_cast<double> (grainLength + sincTapCount);
    const auto minimumDelay = safetyDistance + juce::jmax (0.0, increment - 1.0) * motion;
    const auto maximumDelay = static_cast<double> (historySize) - safetyDistance
                            - juce::jmax (0.0, 1.0 - increment) * motion;
    delay = juce::jlimit (minimumDelay, juce::jmax (minimumDelay, maximumDelay), delay);

    const auto pan = randomBipolar() * scatter;
    const auto panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;

    target->active = true;
    target->age = 0;
    target->length = grainLength;
    target->readPosition = wrapPosition (static_cast<double> (writeIndex) - delay);
    target->increment = increment;
    target->pitchBin = juce::jlimit (0, pitchBinCount - 1,
                                     static_cast<int> (std::ceil (pitch)) + 12);
    target->levelGain = levelGain;
    target->leftGain = preparedChannels > 1
        ? std::cos (panAngle) * 1.41421356237f : 1.0f;
    target->rightGain = preparedChannels > 1
        ? std::sin (panAngle) * 1.41421356237f : 1.0f;
}

std::array<float, 2> GranularShimmerEngine::processSparkle (
    const std::array<float, 2>& dry,
    const Parameters& parameters,
    const SparkleSettings& settings,
    float feedback,
    float dampingAmount,
    bool frozen,
    int numberOfChannels) noexcept
{
    // Attack detector on the dry input: a fast follower racing a slow one.
    const auto level = numberOfChannels > 1
        ? 0.5f * (std::abs (dry[0]) + std::abs (dry[1]))
        : std::abs (dry[0]);
    onsetFastEnvelope += (level > onsetFastEnvelope ? onsetAttackAmount
                                                    : onsetReleaseAmount)
                       * (level - onsetFastEnvelope);
    onsetSlowEnvelope += onsetSlowAmount * (level - onsetSlowEnvelope);
    if (! frozen && samplesSinceOnset < maximumOnsetAge)
        ++samplesSinceOnset;

    if (onsetMeasureCountdown > 0)
    {
        onsetPeak = juce::jmax (onsetPeak, onsetFastEnvelope);
        if (--onsetMeasureCountdown == 0)
            shatterCharge = juce::jmin (
                maximumShatterCharge,
                shatterCharge + juce::jlimit (0.3f, 1.0f,
                                              1.6f * std::sqrt (onsetPeak)));
    }

    if (onsetHoldoff > 0)
    {
        --onsetHoldoff;
    }
    else if (onsetFastEnvelope > onsetRatioThreshold * onsetSlowEnvelope
                                     + onsetAbsoluteThreshold)
    {
        onsetHoldoff = onsetHoldoffSamples;
        onsetMeasureCountdown = onsetMeasureSamples;
        onsetPeak = onsetFastEnvelope;
        if (! frozen)
            samplesSinceOnset = 0;
    }

    shatterCharge *= settings.chargeDecay;
    if (shatterCharge < 1.0e-5f)
        shatterCharge = 0.0f;

    // Stochastic shard scheduler: an even drizzle plus a burst that follows
    // the charge left behind by each attack.
    if (settings.amount > 0.0f)
    {
        const auto burstRate = settings.burstPerSample * shatterCharge;
        const auto rate = settings.drizzlePerSample + burstRate;
        shardSpawnPhase = juce::jmin (shardSpawnThreshold + 1.0f,
                                      shardSpawnPhase + rate);
        if (shardSpawnPhase >= shardSpawnThreshold)
        {
            shardSpawnPhase -= shardSpawnThreshold;
            shardSpawnThreshold = juce::jlimit (
                0.08f, 4.0f, -std::log (juce::jmax (1.0e-6f, shardRandomUnit())));
            const auto fromBurst = shardRandomUnit() * rate < burstRate;
            spawnShard (parameters, settings.shatter, fromBurst, frozen);
        }
    }

    std::array<float, 2> bus {};
    auto weightSquared = 0.0f;
    auto anyShardActive = false;
    for (auto& shard : shards)
    {
        if (! shard.active)
            continue;

        anyShardActive = true;
        // Smoothstep rise: no corner at either end, so a fragment starts as a
        // soft "tink" instead of a click.
        const auto rise = juce::jmin (
            1.0f, static_cast<float> (shard.age + 1) * shard.attackStep);
        const auto attack = rise * rise * (3.0f - 2.0f * rise);
        const auto weight = attack * shard.level * shardEnvelopeScale
                          * (shard.envelope - shardEnvelopeFloor);
        const auto value = readHistory (shard.sourceChannel, shard.readPosition,
                                        shard.pitchBin) * weight;
        bus[0] += value * shard.leftGain;
        bus[1] += value * shard.rightGain;
        weightSquared += weight * weight;

        shard.envelope *= shard.decay;
        shard.readPosition = wrapPosition (shard.readPosition + shard.increment);
        if (++shard.age >= shard.length)
            shard.active = false;
    }

    // Overlap control follows a smoothed power estimate, never the raw
    // per-sample sum, so dense bursts are levelled without gain chatter.
    sparkleOverlapEnvelope += (weightSquared > sparkleOverlapEnvelope
                                   ? sparkleOverlapAttackAmount
                                   : sparkleOverlapReleaseAmount)
                            * (weightSquared - sparkleOverlapEnvelope);
    const auto normalisation = sparkleOverlapEnvelope > sparkleOverlapPowerAllowance
        ? std::sqrt (sparkleOverlapPowerAllowance / sparkleOverlapEnvelope)
        : 1.0f;
    const auto gain = sparkleGainSmoother.getNextValue() * normalisation;
    const auto echoDelay = sparkleEchoDelaySmoother.getNextValue();
    const auto echoFeedback = feedback * sparkleEchoFeedbackScale;

    std::array<float, 2> direct {};
    std::array<float, 2> echo {};
    for (int channel = 0; channel < numberOfChannels; ++channel)
    {
        const auto index = static_cast<size_t> (channel);
        sparkleHighPassState[index] += sparkleHighPassAmount
                                     * (bus[index] - sparkleHighPassState[index]);
        direct[index] = (bus[index] - sparkleHighPassState[index]) * gain;

        sparkleEchoDampState[index] += dampingAmount
            * (readSparkleEcho (channel, echoDelay) - sparkleEchoDampState[index]);
        echo[index] = sparkleEchoDampState[index];
    }

    // Ping-pong: each line is fed by the opposite line's echo, and each
    // output hears the opposite line, so a shard bounces L, R, L, R.
    std::array<float, 2> output {};
    auto writeMagnitude = 0.0f;
    for (int channel = 0; channel < numberOfChannels; ++channel)
    {
        const auto index = static_cast<size_t> (channel);
        const auto opposite = numberOfChannels > 1 ? static_cast<size_t> (1 - channel)
                                                   : index;
        auto lineInput = direct[index] + echoFeedback * echo[opposite];
        if (! std::isfinite (lineInput))
            lineInput = 0.0f;
        lineInput = sparkleEchoHeadroom
                  * diffuseSoftBound (lineInput / sparkleEchoHeadroom);
        sparkleEchoBuffers[index][static_cast<size_t> (sparkleEchoWriteIndex)] = lineInput;
        writeMagnitude = juce::jmax (writeMagnitude, std::abs (lineInput));
        output[index] = direct[index] + sparkleEchoLevel * echo[opposite];
    }

    if (++sparkleEchoWriteIndex >= sparkleEchoSize)
        sparkleEchoWriteIndex = 0;

    if (numberOfChannels == 1)
        output[1] = output[0];

    // Once Sparkle is back at zero and every echo has rung out, step aside so
    // the layer costs nothing and the dry engine path is untouched.
    if (settings.amount <= 0.0f && ! anyShardActive
        && writeMagnitude < sparkleQuietThreshold
        && std::abs (output[0]) < sparkleQuietThreshold
        && std::abs (output[1]) < sparkleQuietThreshold)
    {
        if (++sparkleQuietSamples > sparkleEchoSize)
        {
            clearSparkleState();
            return {};
        }
    }
    else
    {
        sparkleQuietSamples = 0;
    }

    return output;
}

void GranularShimmerEngine::spawnShard (const Parameters& parameters,
                                        float shatter, bool fromBurst,
                                        bool frozen) noexcept
{
    Shard* target = nullptr;
    for (int offset = 0; offset < maximumShards; ++offset)
    {
        const auto index = (nextShardIndex + offset) % maximumShards;
        if (! shards[static_cast<size_t> (index)].active)
        {
            target = &shards[static_cast<size_t> (index)];
            nextShardIndex = (index + 1) % maximumShards;
            break;
        }
    }

    if (target == nullptr)
    {
        auto furthestProgress = -1.0f;
        for (int index = 0; index < maximumShards; ++index)
        {
            auto& candidate = shards[static_cast<size_t> (index)];
            const auto progress = static_cast<float> (candidate.age)
                                / static_cast<float> (juce::jmax (1, candidate.length));
            if (progress > furthestProgress)
            {
                furthestProgress = progress;
                target = &candidate;
                nextShardIndex = (index + 1) % maximumShards;
            }
        }
    }

    if (target == nullptr)
        return;

    // Harder shattering leans a little toward the upper harmonics and detunes
    // each fragment by a few cents, like pieces of slightly different size.
    // Both stay small: wide detune and a top-heavy mix read as distortion.
    std::array<float, shardIntervals.size()> weights {};
    auto weightSum = 0.0f;
    for (size_t index = 0; index < weights.size(); ++index)
    {
        weights[index] = shardIntervalWeights[index]
                       * (1.0f + 0.3f * shatter * static_cast<float> (index));
        weightSum += weights[index];
    }

    auto choice = shardRandomUnit() * weightSum;
    auto intervalIndex = weights.size() - 1;
    for (size_t index = 0; index < weights.size(); ++index)
    {
        if (choice < weights[index])
        {
            intervalIndex = index;
            break;
        }
        choice -= weights[index];
    }

    const auto detune = (shardRandomUnit() * 2.0f - 1.0f) * (0.04f + 0.08f * shatter);
    const auto pitch = juce::jlimit (0.0f, 36.0f, shardIntervals[intervalIndex] + detune);
    const auto ratio = std::pow (2.0, static_cast<double> (pitch) / 12.0);
    const auto pitchShortening = static_cast<float> (std::pow (2.0 / ratio, 0.6));

    // Fragments get shorter as Shatter rises but never shrink into clicks:
    // 10 ms is the floor, and the rise below always spans at least 1.5 ms.
    const auto lengthMs = juce::jmax (
        10.0f, (48.0f - 22.0f * shatter)
                   * (0.45f + 1.1f * shardRandomUnit()) * pitchShortening);
    const auto length = juce::jmax (24, static_cast<int> (std::round (
        lengthMs * 0.001 * sampleRate)));
    const auto attackSamples = juce::jlimit (
        2, juce::jmax (2, length / 3),
        static_cast<int> (std::round ((3.0 - 1.5 * shatter) * 0.001 * sampleRate)));

    const auto motion = static_cast<double> (length + sincTapCount);
    const auto minimumDelay = safetyDistance
                            + (frozen ? ratio : ratio - 1.0) * motion;
    const auto maximumDelay = static_cast<double> (historySize) - safetyDistance
                            - static_cast<double> (sincTapCount);

    double delay = 0.0;
    if (fromBurst && ! frozen && samplesSinceOnset < maximumOnsetAge)
    {
        // Re-read the attack that triggered the burst so every fragment is a
        // transposed splinter of the same transient.
        delay = static_cast<double> (samplesSinceOnset) + 0.001 * sampleRate
              - static_cast<double> (shardRandomUnit()) * 0.035 * sampleRate;
    }
    else
    {
        const auto baseDelay = sanitise (parameters.timeMs, 240.0f, 60.0f, maximumTimeMs)
                             * 0.001 * sampleRate;
        delay = minimumDelay + static_cast<double> (shardRandomUnit()) * baseDelay;
    }
    delay = juce::jlimit (minimumDelay, juce::jmax (minimumDelay, maximumDelay), delay);

    // Cube-root bias pushes fragments toward the edges of the image so the
    // layer fills the whole width instead of crowding the centre: only one
    // fragment in eight lands inside the middle half.
    const auto panRandom = shardRandomUnit() * 2.0f - 1.0f;
    const auto pan = (panRandom < 0.0f ? -1.0f : 1.0f)
                   * std::cbrt (std::abs (panRandom)) * (0.92f + 0.08f * shatter);
    const auto panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const auto levelRandom = shardRandomUnit();

    target->active = true;
    target->age = 0;
    target->length = length;
    target->readPosition = wrapPosition (static_cast<double> (writeIndex) - delay);
    target->increment = ratio;
    target->pitchBin = juce::jlimit (0, pitchBinCount - 1,
                                     static_cast<int> (std::ceil (pitch)) + 12);
    target->sourceChannel = preparedChannels > 1 && pan > 0.0f ? 1 : 0;
    target->attackStep = 1.0f / static_cast<float> (attackSamples);
    target->envelope = 1.0f;
    target->decay = std::exp (std::log (shardEnvelopeFloor)
                              / static_cast<float> (length));
    // Longer, softer fragments carry more energy at high Shatter and a little
    // less at low Shatter; this trim keeps the layer level where it was.
    const auto levelTrim = std::pow (10.0f, (1.2f - 3.3f * shatter) * 0.05f);
    target->level = (0.5f + 0.5f * levelRandom) * levelTrim
                  * static_cast<float> (std::pow (2.0 / ratio, 0.3));
    target->leftGain = preparedChannels > 1
        ? std::cos (panAngle) * 1.41421356237f : 1.0f;
    target->rightGain = preparedChannels > 1
        ? std::sin (panAngle) * 1.41421356237f : 1.0f;

    shardSpawnCounter.fetch_add (1, std::memory_order_relaxed);
}

float GranularShimmerEngine::readSparkleEcho (int channel,
                                              float delaySamples) const noexcept
{
    const auto& buffer = sparkleEchoBuffers[static_cast<size_t> (channel)];
    const auto boundedDelay = juce::jlimit (
        1.0f, static_cast<float> (sparkleEchoSize - 2), delaySamples);
    const auto integerDelay = static_cast<int> (boundedDelay);
    const auto fraction = boundedDelay - static_cast<float> (integerDelay);

    auto newer = sparkleEchoWriteIndex - integerDelay;
    if (newer < 0)
        newer += sparkleEchoSize;
    auto older = newer - 1;
    if (older < 0)
        older += sparkleEchoSize;

    return buffer[static_cast<size_t> (newer)]
         + fraction * (buffer[static_cast<size_t> (older)]
                       - buffer[static_cast<size_t> (newer)]);
}

float GranularShimmerEngine::shardRandomUnit() noexcept
{
    return static_cast<float> (advanceRandom (shardRandomState) >> 8)
         * (1.0f / 16777216.0f);
}

float GranularShimmerEngine::readHistory (int channel, double position,
                                          int pitchBin) const noexcept
{
    const auto baseIndex = static_cast<int> (std::floor (position));
    const auto fraction = static_cast<float> (position - std::floor (position));
    const auto phaseIndex = juce::jlimit (0, fractionalPhaseCount - 1,
        static_cast<int> (fraction * static_cast<float> (fractionalPhaseCount)));
    const auto kernelOffset = (pitchBin * fractionalPhaseCount + phaseIndex) * sincTapCount;
    const auto* samples = history[static_cast<size_t> (channel)].data();
    const auto* kernel = sincTable.data() + static_cast<size_t> (kernelOffset);
    const auto start = wrapIndex (baseIndex - 3);
    auto result = 0.0f;

    if (start + sincTapCount <= historySize)
    {
        for (int tap = 0; tap < sincTapCount; ++tap)
            result += samples[static_cast<size_t> (start + tap)]
                    * kernel[static_cast<size_t> (tap)];
    }
    else
    {
        for (int tap = 0; tap < sincTapCount; ++tap)
        {
            auto index = start + tap;
            if (index >= historySize)
                index -= historySize;
            result += samples[static_cast<size_t> (index)]
                    * kernel[static_cast<size_t> (tap)];
        }
    }

    return result;
}

std::array<float, 2> GranularShimmerEngine::processEarlyDiffusion (
    const std::array<float, 2>& input,
    int numberOfChannels) noexcept
{
    std::array<float, 2> output {};
    for (int channel = 0; channel < numberOfChannels; ++channel)
    {
        auto stageInput = input[static_cast<size_t> (channel)];
        for (int stage = 0; stage < earlyDiffusionStageCount; ++stage)
        {
            const auto channelIndex = static_cast<size_t> (channel);
            const auto stageIndex = static_cast<size_t> (stage);
            auto& buffer = earlyDiffusionBuffers[channelIndex][stageIndex];
            auto& writePosition = earlyDiffusionWriteIndices[channelIndex][stageIndex];
            const auto coefficient = earlyDiffusionCoefficients[channelIndex][stageIndex];
            const auto delayed = buffer[static_cast<size_t> (writePosition)];
            const auto stageOutput = delayed - coefficient * stageInput;
            buffer[static_cast<size_t> (writePosition)] = stageInput
                                                       + coefficient * stageOutput;
            if (++writePosition >= static_cast<int> (buffer.size()))
                writePosition = 0;
            stageInput = stageOutput;
        }
        output[static_cast<size_t> (channel)] = stageInput;
    }

    if (numberOfChannels == 1)
        output[1] = output[0];

    return output;
}

std::array<float, 2> GranularShimmerEngine::processDiffuseTail (
    const std::array<float, 2>& input,
    float baseDelayMs,
    float motionDepthSamples,
    float dampingAmount,
    int numberOfChannels) noexcept
{
    std::array<float, diffuseLineCount> delayed {};
    for (int line = 0; line < diffuseLineCount; ++line)
    {
        const auto index = static_cast<size_t> (line);
        if (--diffuseWanderCountdowns[index] <= 0)
        {
            diffuseWanderTargets[index] = randomBipolar (diffuseRandomStates[index]);
            diffuseWanderCountdowns[index] = juce::jmax (
                1, static_cast<int> (std::round (
                    sampleRate * diffuseWanderPeriods[index])));
        }

        diffuseWander[index] += diffuseWanderAmount
                              * (diffuseWanderTargets[index] - diffuseWander[index]);
        const auto delaySamples = baseDelayMs * diffuseDelayRatios[index]
                                * 0.001f * static_cast<float> (sampleRate)
                                + diffuseWander[index] * motionDepthSamples;
        delayed[index] = readDiffuseLine (line, delaySamples);
    }

    auto mixed = delayed;
    for (int stride = 1; stride < diffuseLineCount; stride *= 2)
        for (int group = 0; group < diffuseLineCount; group += stride * 2)
            for (int offset = 0; offset < stride; ++offset)
            {
                const auto firstIndex = static_cast<size_t> (group + offset);
                const auto secondIndex = static_cast<size_t> (group + offset + stride);
                const auto first = mixed[firstIndex];
                const auto second = mixed[secondIndex];
                mixed[firstIndex] = first + second;
                mixed[secondIndex] = first - second;
            }

    const auto inputLeft = input[0];
    const auto inputRight = numberOfChannels > 1 ? input[1] : input[0];
    const auto inputMid = 0.5f * (inputLeft + inputRight);
    const auto inputSide = 0.5f * (inputLeft - inputRight);
    for (int line = 0; line < diffuseLineCount; ++line)
    {
        const auto index = static_cast<size_t> (line);
        const auto orthogonalFeedback = mixed[index] * inverseSqrtEight;
        diffuseDampingState[index] += dampingAmount
            * (orthogonalFeedback - diffuseDampingState[index]);

        const auto injection = 0.7071067811865476f * inverseSqrtEight
            * (inputMid + hadamardRowOne[index] * inputSide);
        auto nextValue = injection
                       + diffuseGainSmoothers[index].getNextValue()
                       * diffuseDampingState[index];
        if (! std::isfinite (nextValue))
        {
            nextValue = 0.0f;
            diffuseDampingState[index] = 0.0f;
        }

        auto& buffer = diffuseBuffers[index];
        auto& writePosition = diffuseWriteIndices[index];
        buffer[static_cast<size_t> (writePosition)] = diffuseSoftBound (nextValue);
        if (++writePosition >= static_cast<int> (buffer.size()))
            writePosition = 0;
    }

    auto tailLeft = 0.0f;
    auto tailRight = 0.0f;
    for (int line = 0; line < diffuseLineCount; ++line)
    {
        const auto index = static_cast<size_t> (line);
        tailLeft += hadamardRowTwo[index] * delayed[index];
        tailRight += hadamardRowFour[index] * delayed[index];
    }
    tailLeft = diffuseSoftBound (tailLeft * inverseSqrtEight);
    tailRight = diffuseSoftBound (tailRight * inverseSqrtEight);

    if (numberOfChannels == 1)
    {
        auto mono = 0.0f;
        for (const auto value : delayed)
            mono += value;
        mono = diffuseSoftBound (mono * inverseSqrtEight);
        return {{ mono, mono }};
    }

    return {{ tailLeft, tailRight }};
}

float GranularShimmerEngine::readDiffuseLine (int line, float delaySamples) noexcept
{
    const auto index = static_cast<size_t> (line);
    const auto& buffer = diffuseBuffers[index];
    const auto size = static_cast<int> (buffer.size());
    const auto boundedDelay = juce::jlimit (
        3.0f, static_cast<float> (size - 3), delaySamples);

    auto integerDelay = static_cast<int> (std::floor (boundedDelay));
    auto fraction = boundedDelay - static_cast<float> (integerDelay);
    if (fraction < 0.5f)
    {
        fraction += 1.0f;
        --integerDelay;
    }

    auto readIndex = diffuseWriteIndices[index] - integerDelay;
    while (readIndex < 0)
        readIndex += size;
    while (readIndex >= size)
        readIndex -= size;

    const auto coefficient = (1.0f - fraction) / (1.0f + fraction);
    const auto current = buffer[static_cast<size_t> (readIndex)];
    auto output = coefficient * current
                + diffuseInterpolationInput[index]
                - coefficient * diffuseInterpolationOutput[index];
    if (! std::isfinite (output))
        output = 0.0f;

    diffuseInterpolationInput[index] = current;
    diffuseInterpolationOutput[index] = output;
    return output;
}

float GranularShimmerEngine::windowAt (int age, int length) const noexcept
{
    if (length <= 1)
        return 0.0f;

    const auto tablePosition = static_cast<float> (age)
                             * static_cast<float> (windowTableSize - 1)
                             / static_cast<float> (length - 1);
    const auto index = juce::jlimit (0, windowTableSize - 2,
                                     static_cast<int> (tablePosition));
    const auto fraction = tablePosition - static_cast<float> (index);
    return juce::jmap (fraction,
                       windowTable[static_cast<size_t> (index)],
                       windowTable[static_cast<size_t> (index + 1)]);
}

float GranularShimmerEngine::randomUnit() noexcept
{
    auto value = randomState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState = value;
    return static_cast<float> (value >> 8) * (1.0f / 16777216.0f);
}

float GranularShimmerEngine::randomBipolar() noexcept
{
    return randomUnit() * 2.0f - 1.0f;
}

std::uint32_t GranularShimmerEngine::advanceRandom (std::uint32_t& state) noexcept
{
    auto value = state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    state = value;
    return value;
}

float GranularShimmerEngine::randomBipolar (std::uint32_t& state) noexcept
{
    return static_cast<float> (advanceRandom (state) >> 8)
         * (2.0f / 16777216.0f) - 1.0f;
}

float GranularShimmerEngine::sanitise (float value, float fallback,
                                       float minimum, float maximum) noexcept
{
    return std::isfinite (value) ? juce::jlimit (minimum, maximum, value) : fallback;
}

float GranularShimmerEngine::softBound (float value) noexcept
{
    return value / (1.0f + std::abs (value));
}

float GranularShimmerEngine::diffuseSoftBound (float value) noexcept
{
    constexpr auto linearLimit = 0.9f;
    const auto magnitude = std::abs (value);
    if (magnitude <= linearLimit)
        return value;

    const auto excess = magnitude - linearLimit;
    const auto bounded = linearLimit + excess / (1.0f + 0.25f * excess);
    return value < 0.0f ? -bounded : bounded;
}

double GranularShimmerEngine::wrapPosition (double position) const noexcept
{
    while (position < 0.0)
        position += static_cast<double> (historySize);
    while (position >= static_cast<double> (historySize))
        position -= static_cast<double> (historySize);
    return position;
}

int GranularShimmerEngine::wrapIndex (int index) const noexcept
{
    while (index < 0)
        index += historySize;
    while (index >= historySize)
        index -= historySize;
    return index;
}

int GranularShimmerEngine::getActiveGrainCount() const noexcept
{
    return static_cast<int> (std::count_if (grains.begin(), grains.end(),
        [] (const Grain& grain) { return grain.active; }));
}

int GranularShimmerEngine::getActiveShardCount() const noexcept
{
    return static_cast<int> (std::count_if (shards.begin(), shards.end(),
        [] (const Shard& shard) { return shard.active; }));
}
