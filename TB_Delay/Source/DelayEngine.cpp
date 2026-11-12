#include "DelayEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double maximumDelaySeconds = 8.0;
constexpr double minimumDelayMs = 10.0;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

float finiteOr (float value, float fallback)
{
    return std::isfinite (value) ? value : fallback;
}

double finiteOr (double value, double fallback)
{
    return std::isfinite (value) ? value : fallback;
}

// Choice order: straight, triplet, dotted for each note value.
constexpr std::array<double, 18> divisionBeats {
    0.125, 1.0 / 12.0, 0.1875, // 1/32
    0.25, 1.0 / 6.0, 0.375,   // 1/16
    0.5, 1.0 / 3.0, 0.75,     // 1/8
    1.0, 2.0 / 3.0, 1.5,      // 1/4
    2.0, 4.0 / 3.0, 3.0,      // 1/2
    4.0, 8.0 / 3.0, 6.0       // 1/1
};

constexpr std::array<double, 4> diffuserRatios {
    0.193, 0.337, 0.547, 0.797
};
}

//==============================================================================
void TBDelayEngine::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    juce::ignoreUnused (maximumBlockSize);

    currentSampleRate = juce::jlimit (
        8000.0, 384000.0, finiteOr (sampleRate, 44100.0));
    preparedChannels = juce::jlimit (1, maxChannels, numChannels);
    delayBufferSize = static_cast<int> (
        std::ceil (currentSampleRate * maximumDelaySeconds)) + 8;

    for (auto& channel : delayMemory)
        channel.assign (static_cast<std::size_t> (delayBufferSize), 0.0f);
    delayGenerationTags.assign (
        static_cast<std::size_t> (delayBufferSize), 0u);

    maximumDiffuserSamples = static_cast<int> (
        std::ceil (currentSampleRate * 0.121)) + 8;

    for (auto& channel : diffuserMemory)
        for (auto& stage : channel)
            stage.assign (static_cast<std::size_t> (maximumDiffuserSamples), 0.0f);

    hilbertLength = juce::jlimit (
        31, 511,
        static_cast<int> (std::round (63.0 * currentSampleRate / 48000.0)));
    if ((hilbertLength & 1) == 0)
        ++hilbertLength;
    hilbertCentre = hilbertLength / 2;
    hilbertCoefficients.assign (
        static_cast<std::size_t> (hilbertLength), 0.0f);
    for (auto& history : hilbertHistory)
        history.assign (static_cast<std::size_t> (hilbertLength), 0.0f);

    for (int i = 0; i < hilbertLength; ++i)
    {
        const int distance = i - hilbertCentre;
        const double window = 0.54
                              - 0.46 * std::cos (twoPi * static_cast<double> (i)
                                                 / static_cast<double> (hilbertLength - 1));

        hilbertCoefficients[static_cast<std::size_t> (i)] =
            (distance != 0 && (std::abs (distance) & 1) != 0)
                ? static_cast<float> (window * 2.0
                                      / (juce::MathConstants<double>::pi
                                         * static_cast<double> (distance)))
                : 0.0f;
    }

    parameterSmoothingCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / (0.015 * currentSampleRate)));
    freezeSmoothingCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / (0.010 * currentSampleRate)));
    detectorAttackCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / (0.005 * currentSampleRate)));
    detectorReleaseCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / (0.140 * currentSampleRate)));
    timeSmoothingCoefficient =
        1.0 - std::exp (-1.0 / (0.045 * currentSampleRate));

    reset();
}

void TBDelayEngine::reset()
{
    for (auto& channel : delayMemory)
        std::fill (channel.begin(), channel.end(), 0.0f);
    std::fill (delayGenerationTags.begin(), delayGenerationTags.end(), 0u);
    delayGeneration = 1;

    for (auto& channel : diffuserMemory)
        for (auto& stage : channel)
            std::fill (stage.begin(), stage.end(), 0.0f);

    for (auto& channel : hilbertHistory)
        std::fill (channel.begin(), channel.end(), 0.0f);

    writeIndex = 0;
    timeStates = {};
    diffuserWriteIndex = {};
    diffuserDelaySamples = {};
    diffuserClearChannel = 0;
    diffuserClearStage = 0;
    diffuserClearIndex = 0;
    diffuserClearing = false;
    hilbertWriteIndex = {};
    shiftPhase = {};
    smoothedShiftHz = {};

    grainVoices = {};
    nextGrainVoice = 0;
    grainSpawnCountdown = 0;
    randomState = 0x6d2b79f5u;

    characterLowpassState = {};
    feedbackLowpassState = {};
    feedbackHighpassState = {};
    digitalHeldSample = {};
    digitalHoldCounter = {};

    modulationPhase = 0.0;
    wowPhase = 0.0;
    flutterPhase = 0.0;
    detectorEnvelope = 0.0f;
    freezeInputGain = 1.0f;
    loopActivityEnvelope = 0.0f;
    freezeCaptureArmed = false;
    freezeWasActive = false;

    feedbackFilterLowpassCoefficient = 1.0f;
    feedbackFilterHighpassCoefficient = 0.0f;
    characterLowpassCoefficient = 1.0f;
    smoothedFeedbackGain = 0.0f;
    smoothedFeedbackPolarity = 1.0f;
    smoothedSpacingPercent = 100.0f;
    smoothedSwingPercent = 0.0f;
    smoothedModDepthSamples = 0.0;
    smoothedMix = 0.0f;
    smoothedWidth = 1.0f;
    smoothedDuck = 0.0f;
    smoothedOutputGain = 1.0f;
    smoothedDrive = 0.0f;
    smoothedAge = 0.0f;
    smoothedGrainAmount = 0.0f;
    smoothedSmearAmount = 0.0f;
    smoothedStereoActive = 1.0f;
    smoothedColorSpaceActive = 1.0f;
    parametersInitialised = false;

    silentInputSeconds = 0.0;
    panicRequested.store (false, std::memory_order_release);
    inputLevel.store (0.0f, std::memory_order_relaxed);
    wetLevel.store (0.0f, std::memory_order_relaxed);
}

void TBDelayEngine::requestPanic()
{
    panicRequested.store (true, std::memory_order_release);
}

//==============================================================================
double TBDelayEngine::calculateBaseDelayMs (const Parameters& parameters) const
{
    if (parameters.timeMode == 1)
    {
        const double bpm = juce::jlimit (20.0, 400.0, parameters.bpm);
        const int index = juce::jlimit (
            0, static_cast<int> (divisionBeats.size()) - 1, parameters.division);
        return juce::jlimit (
            minimumDelayMs, maximumDelaySeconds * 1000.0,
            (60000.0 / bpm) * divisionBeats[static_cast<std::size_t> (index)]);
    }

    double delayMs = juce::jlimit (
        minimumDelayMs, maximumDelaySeconds * 1000.0, parameters.timeMs);

    if (parameters.timeMode == 2)
    {
        const double frequency = 1000.0 / delayMs;
        const double midiNote = 69.0 + 12.0 * std::log2 (frequency / 440.0);
        const double tunedFrequency =
            440.0 * std::pow (2.0, (std::round (midiNote) - 69.0) / 12.0);
        delayMs = 1000.0 / tunedFrequency;
    }

    return juce::jlimit (
        minimumDelayMs, maximumDelaySeconds * 1000.0, delayMs);
}

std::array<double, TBDelayEngine::maxChannels>
TBDelayEngine::calculateChannelDelaySamples (const Parameters& parameters) const
{
    const double base = calculateBaseDelayMs (parameters)
                        * currentSampleRate / 1000.0;
    const double offset = juce::jlimit (
        -1.0, 1.0, static_cast<double> (parameters.offsetPercent) / 100.0);

    double rightMultiplier = 1.0;

    switch (parameters.style)
    {
        case 1:  rightMultiplier = 1.0 + 0.5 * offset; break; // Dual
        case 2:  rightMultiplier = 1.0 + 0.25 * offset; break; // Ping-Pong
        case 3:  rightMultiplier = std::pow (2.0, offset); break; // Ratio
        default: break; // Single
    }

    const double minimumSamples = std::max (
        4.0, currentSampleRate * minimumDelayMs / 1000.0);
    const double maximumSamples = std::max (
        minimumSamples, static_cast<double> (delayBufferSize - 4));

    return {
        juce::jlimit (minimumSamples, maximumSamples, base),
        juce::jlimit (minimumSamples, maximumSamples, base * rightMultiplier)
    };
}

TBDelayEngine::ReadHeads TBDelayEngine::updateTimeState (
    int channel, double targetDelay, int timeMotion)
{
    auto& state = timeStates[static_cast<std::size_t> (channel)];

    if (! state.initialised)
    {
        state.currentDelay = targetDelay;
        state.oldDelay = targetDelay;
        state.newDelay = targetDelay;
        state.crossfadeLength = std::max (
            1, static_cast<int> (std::round (0.030 * currentSampleRate)));
        state.initialised = true;
    }

    if (timeMotion != 0)
    {
        if (state.crossfadeRemaining > 0)
        {
            const double progress = 1.0
                                    - static_cast<double> (state.crossfadeRemaining)
                                          / static_cast<double> (state.crossfadeLength);
            state.currentDelay = state.oldDelay
                                 + (state.newDelay - state.oldDelay) * progress;
            state.crossfadeRemaining = 0;
        }

        state.currentDelay +=
            (targetDelay - state.currentDelay) * timeSmoothingCoefficient;
        state.oldDelay = state.currentDelay;
        state.newDelay = state.currentDelay;
        return { state.currentDelay, state.currentDelay, 1.0f, 0.0f, false };
    }

    if (state.crossfadeRemaining == 0
        && std::abs (targetDelay - state.currentDelay) > 0.25)
    {
        state.oldDelay = state.currentDelay;
        state.newDelay = targetDelay;
        state.crossfadeRemaining = state.crossfadeLength;
    }

    if (state.crossfadeRemaining > 0)
    {
        const double progress = 1.0
                                - static_cast<double> (state.crossfadeRemaining)
                                      / static_cast<double> (state.crossfadeLength);
        const float firstGain = static_cast<float> (
            std::cos (0.5 * juce::MathConstants<double>::pi * progress));
        const float secondGain = static_cast<float> (
            std::sin (0.5 * juce::MathConstants<double>::pi * progress));

        --state.crossfadeRemaining;

        if (state.crossfadeRemaining == 0)
            state.currentDelay = state.newDelay;

        return {
            state.oldDelay, state.newDelay, firstGain, secondGain, true
        };
    }

    state.currentDelay = targetDelay;
    state.oldDelay = targetDelay;
    state.newDelay = targetDelay;
    return { targetDelay, targetDelay, 1.0f, 0.0f, false };
}

int TBDelayEngine::getTapCount (int tapsValue) const
{
    if (tapsValue >= 0 && tapsValue <= 3)
        return 1 << tapsValue;

    if (tapsValue <= 1) return 1;
    if (tapsValue <= 2) return 2;
    if (tapsValue <= 4) return 4;
    return 8;
}

//==============================================================================
int TBDelayEngine::wrapIndex (int index, int size) const
{
    index %= size;
    return index < 0 ? index + size : index;
}

double TBDelayEngine::wrapPosition (double position, int size) const
{
    position = std::fmod (position, static_cast<double> (size));
    return position < 0.0 ? position + static_cast<double> (size) : position;
}

float TBDelayEngine::readAtPosition (int channel, double position) const
{
    if (delayBufferSize <= 0)
        return 0.0f;

    position = wrapPosition (position, delayBufferSize);
    const int centre = static_cast<int> (std::floor (position));
    const double fraction = position - static_cast<double> (centre);
    const auto& memory = delayMemory[static_cast<std::size_t> (channel)];

    const auto readValidSample = [this, &memory] (int index)
    {
        const auto wrapped = static_cast<std::size_t> (
            wrapIndex (index, delayBufferSize));
        return delayGenerationTags[wrapped] == delayGeneration
            ? memory[wrapped]
            : 0.0f;
    };

    const float yMinus1 = readValidSample (centre - 1);
    const float y0 = readValidSample (centre);
    const float y1 = readValidSample (centre + 1);
    const float y2 = readValidSample (centre + 2);

    const double cMinus1 =
        -fraction * (fraction - 1.0) * (fraction - 2.0) / 6.0;
    const double c0 =
        (fraction + 1.0) * (fraction - 1.0) * (fraction - 2.0) / 2.0;
    const double c1 =
        -(fraction + 1.0) * fraction * (fraction - 2.0) / 2.0;
    const double c2 =
        (fraction + 1.0) * fraction * (fraction - 1.0) / 6.0;

    return static_cast<float> (cMinus1 * yMinus1 + c0 * y0
                               + c1 * y1 + c2 * y2);
}

float TBDelayEngine::readDelay (int channel, double delaySamples) const
{
    const double minimumSamples = std::max (
        4.0, currentSampleRate * minimumDelayMs / 1000.0);
    delaySamples = juce::jlimit (
        minimumSamples, static_cast<double> (delayBufferSize - 4), delaySamples);
    return readAtPosition (
        channel, static_cast<double> (writeIndex) - delaySamples);
}

float TBDelayEngine::readTapSum (
    int channel, const ReadHeads& heads, double modulationSamples,
    int tapCount, float spacingPercent, float swingPercent) const
{
    const double spacing = juce::jlimit (
        0.25, 2.0, static_cast<double> (spacingPercent) / 100.0);
    const double swing = juce::jlimit (
        -0.5, 0.5, static_cast<double> (swingPercent) / 100.0);

    double output = 0.0;
    double weightSum = 0.0;

    for (int tap = 0; tap < tapCount; ++tap)
    {
        double units = 1.0 + static_cast<double> (tap) * spacing;

        if ((tap & 1) != 0)
            units += 0.5 * swing * spacing;

        units = std::max (0.125, units);
        const double firstDelay = heads.first * units + modulationSamples;
        double value = static_cast<double> (readDelay (channel, firstDelay))
                       * heads.firstGain;

        if (heads.crossfading)
        {
            const double secondDelay =
                heads.second * units + modulationSamples;
            value += static_cast<double> (
                         readDelay (channel, secondDelay))
                     * heads.secondGain;
        }

        const double weight = 1.0 / (1.0 + 0.15 * static_cast<double> (tap));
        output += value * weight;
        weightSum += weight;
    }

    return static_cast<float> (output / std::max (1.0, weightSum));
}

//==============================================================================
float TBDelayEngine::randomUnit()
{
    std::uint32_t value = randomState;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    randomState = value != 0 ? value : 0x6d2b79f5u;
    return static_cast<float> ((randomState >> 8) * (1.0 / 16777216.0));
}

float TBDelayEngine::randomSigned()
{
    return 2.0f * randomUnit() - 1.0f;
}

void TBDelayEngine::spawnGrain (
    const std::array<double, maxChannels>& baseDelaySamples,
    int grainLength, double pitchRatio, float sprayPercent,
    float reversePercent)
{
    auto& voice = grainVoices[static_cast<std::size_t> (nextGrainVoice)];
    nextGrainVoice = (nextGrainVoice + 1) % maxGrains;

    const bool reverse =
        randomUnit() < juce::jlimit (0.0f, 1.0f, reversePercent / 100.0f);
    voice.increment = (reverse ? -1.0 : 1.0) * pitchRatio;
    voice.age = 0;
    voice.length = grainLength;
    voice.active = true;

    const double maximumReadDistance =
        static_cast<double> (delayBufferSize - 4);
    const double forwardSafety =
        voice.increment > 0.0
            ? static_cast<double> (grainLength) * voice.increment + 4.0
            : 4.0;
    const double relativeDrift =
        (1.0 - voice.increment)
        * static_cast<double> (std::max (0, grainLength - 1));
    const double minimumStartDistance = std::min (
        forwardSafety, maximumReadDistance);
    const double maximumStartDistance = std::max (
        minimumStartDistance,
        maximumReadDistance - std::max (0.0, relativeDrift));
    const double spray = juce::jlimit (
        0.0, 1.0, static_cast<double> (sprayPercent) / 100.0);

    for (int channel = 0; channel < maxChannels; ++channel)
    {
        const double spreadRange = std::min (
            baseDelaySamples[static_cast<std::size_t> (channel)] * 0.45,
            static_cast<double> (grainLength) * 2.0);
        const double delay = juce::jlimit (
            minimumStartDistance,
            maximumStartDistance,
            baseDelaySamples[static_cast<std::size_t> (channel)]
                + static_cast<double> (randomSigned()) * spreadRange * spray);

        voice.readPosition[static_cast<std::size_t> (channel)] =
            wrapPosition (static_cast<double> (writeIndex) - delay,
                          delayBufferSize);
    }
}

std::array<float, TBDelayEngine::maxChannels> TBDelayEngine::processGrains (
    int channels, const std::array<double, maxChannels>& baseDelaySamples,
    const GrainSettings& settings)
{
    std::array<float, maxChannels> output {};

    if (settings.enabled)
    {
        if (grainSpawnCountdown <= 0)
        {
            spawnGrain (baseDelaySamples, settings.lengthSamples,
                        settings.pitchRatio, settings.sprayPercent,
                        settings.reversePercent);
            grainSpawnCountdown = std::max (
                1, settings.lengthSamples / settings.density);
        }

        --grainSpawnCountdown;
    }
    else
    {
        grainSpawnCountdown = 0;
    }

    for (auto& voice : grainVoices)
    {
        if (! voice.active)
            continue;

        const double phase = voice.length > 1
                                 ? static_cast<double> (voice.age)
                                       / static_cast<double> (voice.length - 1)
                                 : 1.0;
        const float window = static_cast<float> (
            0.5 - 0.5 * std::cos (twoPi * phase));

        for (int channel = 0; channel < channels; ++channel)
        {
            output[static_cast<std::size_t> (channel)] +=
                readAtPosition (
                    channel,
                    voice.readPosition[static_cast<std::size_t> (channel)])
                * window;
            voice.readPosition[static_cast<std::size_t> (channel)] =
                wrapPosition (
                    voice.readPosition[static_cast<std::size_t> (channel)]
                        + voice.increment,
                    delayBufferSize);
        }

        if (++voice.age >= voice.length)
            voice.active = false;
    }

    const float normalisation =
        1.0f / std::sqrt (static_cast<float> (std::max (1, settings.density)));

    for (int channel = 0; channel < channels; ++channel)
        output[static_cast<std::size_t> (channel)] *= normalisation;

    return output;
}

//==============================================================================
float TBDelayEngine::sanitise (float sample)
{
    return std::isfinite (sample) ? sample : 0.0f;
}

float TBDelayEngine::boundedSoftClip (float sample)
{
    sample = sanitise (sample);
    const float magnitude = std::abs (sample);

    if (magnitude <= 1.0f)
        return sample;

    return std::copysign (
        1.0f + std::tanh (magnitude - 1.0f), sample);
}

float TBDelayEngine::applyRecordCharacter (
    int channel, float sample, int character,
    float driveAmount, float ageAmount)
{
    sample = sanitise (sample);
    driveAmount = juce::jlimit (0.0f, 1.0f, driveAmount);
    ageAmount = juce::jlimit (0.0f, 1.0f, ageAmount);
    const float driveGain = 1.0f + 5.0f * driveAmount;

    if (character == 1)
    {
        const float bias = 0.12f * ageAmount;
        const float saturated =
            (std::tanh (driveGain * sample + bias)
             - std::tanh (bias))
            / driveGain;
        const float saturationAmount =
            juce::jlimit (0.0f, 1.0f, 0.15f * ageAmount
                                           + 0.85f * driveAmount);
        sample += saturationAmount * (saturated - sample);
    }
    else if (driveAmount > 0.0f)
    {
        const float saturated =
            std::tanh (driveGain * sample) / driveGain;
        sample += driveAmount * (saturated - sample);
    }

    if (character == 2)
    {
        const int bits = juce::jlimit (
            7, 12, 12 - static_cast<int> (std::round (5.0f * ageAmount)));
        const float levels = static_cast<float> (1 << (bits - 1));
        sample = std::round (sample * levels) / levels;
    }
    else if (character == 3)
    {
        const int bits = juce::jlimit (
            6, 16, 16 - static_cast<int> (std::round (10.0f * ageAmount)));
        const float levels = static_cast<float> (1 << (bits - 1));
        const int holdPeriod = 1 + static_cast<int> (
            std::round (15.0f * ageAmount));
        auto& counter = digitalHoldCounter[static_cast<std::size_t> (channel)];

        if (counter <= 0)
        {
            digitalHeldSample[static_cast<std::size_t> (channel)] =
                std::round (sample * levels) / levels;
            counter = holdPeriod;
        }

        --counter;
        sample = digitalHeldSample[static_cast<std::size_t> (channel)];
    }

    return boundedSoftClip (sample);
}

float TBDelayEngine::applyPlaybackCharacter (
    int channel, float sample, int character)
{
    auto& state =
        characterLowpassState[static_cast<std::size_t> (channel)];
    state += characterLowpassCoefficient * (sample - state);

    if (character == 0)
        return sample;

    return state;
}

float TBDelayEngine::coefficientForCutoff (
    float cutoffHz, double sampleRate)
{
    cutoffHz = juce::jlimit (
        1.0f, static_cast<float> (sampleRate * 0.45), cutoffHz);
    return static_cast<float> (
        1.0 - std::exp (-twoPi * static_cast<double> (cutoffHz) / sampleRate));
}

float TBDelayEngine::processFeedbackFilters (
    int channel, float sample)
{
    auto& highpassState =
        feedbackHighpassState[static_cast<std::size_t> (channel)];
    auto& lowpassState =
        feedbackLowpassState[static_cast<std::size_t> (channel)];

    highpassState +=
        feedbackFilterHighpassCoefficient * (sample - highpassState);
    sample -= highpassState;
    lowpassState +=
        feedbackFilterLowpassCoefficient * (sample - lowpassState);
    return lowpassState;
}

float TBDelayEngine::processDiffusion (
    int channel, float sample, float amount, float sizeMs)
{
    if (diffuserClearing)
        return sample;

    amount = juce::jlimit (0.0f, 1.0f, amount);
    const float input = sample;
    const float coefficient = 0.48f + 0.18f * amount;
    const double sizeSamples = juce::jlimit (
        2.0, 120.0, static_cast<double> (sizeMs))
                               * currentSampleRate / 1000.0;

    for (int stage = 0; stage < diffuserStages; ++stage)
    {
        auto& memory = diffuserMemory[static_cast<std::size_t> (channel)]
                                      [static_cast<std::size_t> (stage)];

        if (memory.empty())
            continue;

        auto& delay =
            diffuserDelaySamples[static_cast<std::size_t> (channel)]
                                 [static_cast<std::size_t> (stage)];
        const double targetDelay = juce::jlimit (
            1.0, static_cast<double> (memory.size() - 2),
            sizeSamples * diffuserRatios[static_cast<std::size_t> (stage)]);
        delay += (targetDelay - delay) * timeSmoothingCoefficient;

        auto& stageWrite =
            diffuserWriteIndex[static_cast<std::size_t> (channel)]
                               [static_cast<std::size_t> (stage)];
        const double readPosition = wrapPosition (
            static_cast<double> (stageWrite) - delay,
            static_cast<int> (memory.size()));
        const int first = static_cast<int> (std::floor (readPosition));
        const int second = wrapIndex (
            first + 1, static_cast<int> (memory.size()));
        const float fraction = static_cast<float> (
            readPosition - static_cast<double> (first));
        const float delayed =
            memory[static_cast<std::size_t> (first)]
            + fraction
                  * (memory[static_cast<std::size_t> (second)]
                     - memory[static_cast<std::size_t> (first)]);

        const float output = delayed - coefficient * sample;
        memory[static_cast<std::size_t> (stageWrite)] =
            sanitise (sample + coefficient * output);
        stageWrite = (stageWrite + 1) % static_cast<int> (memory.size());
        sample = output;
    }

    return input + amount * (sample - input);
}

float TBDelayEngine::processFrequencyShift (
    int channel, float sample, float targetShiftHz)
{
    auto& history = hilbertHistory[static_cast<std::size_t> (channel)];
    auto& historyIndex =
        hilbertWriteIndex[static_cast<std::size_t> (channel)];
    history[static_cast<std::size_t> (historyIndex)] = sample;

    auto& shift = smoothedShiftHz[static_cast<std::size_t> (channel)];
    shift += parameterSmoothingCoefficient * (targetShiftHz - shift);

    if (std::abs (targetShiftHz) < 0.0001f && std::abs (shift) < 0.0001f)
    {
        historyIndex = (historyIndex + 1) % hilbertLength;
        return sample;
    }

    double quadrature = 0.0;

    const int firstActiveTap = (hilbertCentre & 1) == 0 ? 1 : 0;
    for (int tap = firstActiveTap; tap < hilbertLength; tap += 2)
    {
        const int index = wrapIndex (
            historyIndex - tap, hilbertLength);
        quadrature +=
            static_cast<double> (
                hilbertCoefficients[static_cast<std::size_t> (tap)])
            * history[static_cast<std::size_t> (index)];
    }

    const int realIndex = wrapIndex (
        historyIndex - hilbertCentre, hilbertLength);
    const float inPhase = history[static_cast<std::size_t> (realIndex)];
    historyIndex = (historyIndex + 1) % hilbertLength;

    auto& phase = shiftPhase[static_cast<std::size_t> (channel)];

    const float shifted = static_cast<float> (
        static_cast<double> (inPhase) * std::cos (phase)
        - quadrature * std::sin (phase));
    phase += twoPi * static_cast<double> (shift) / currentSampleRate;

    if (phase > juce::MathConstants<double>::pi)
        phase -= twoPi;
    else if (phase < -juce::MathConstants<double>::pi)
        phase += twoPi;

    const float transformMix =
        juce::jlimit (0.0f, 1.0f, std::abs (shift) * 0.5f);
    return sample + transformMix * (shifted - sample);
}

void TBDelayEngine::performRealtimePanic()
{
    if (++delayGeneration == 0)
    {
        std::fill (delayGenerationTags.begin(), delayGenerationTags.end(), 0u);
        delayGeneration = 1;
    }

    writeIndex = 0;
    timeStates = {};
    for (auto& history : hilbertHistory)
        std::fill (history.begin(), history.end(), 0.0f);
    hilbertWriteIndex = {};
    shiftPhase = {};
    smoothedShiftHz = {};
    grainVoices = {};
    nextGrainVoice = 0;
    grainSpawnCountdown = 0;
    randomState = 0x6d2b79f5u;
    characterLowpassState = {};
    feedbackLowpassState = {};
    feedbackHighpassState = {};
    digitalHeldSample = {};
    digitalHoldCounter = {};
    modulationPhase = 0.0;
    wowPhase = 0.0;
    flutterPhase = 0.0;
    detectorEnvelope = 0.0f;
    freezeInputGain = 1.0f;
    loopActivityEnvelope = 0.0f;
    freezeCaptureArmed = false;
    freezeWasActive = false;
    parametersInitialised = false;
    silentInputSeconds = 0.0;
    inputLevel.store (0.0f, std::memory_order_relaxed);
    wetLevel.store (0.0f, std::memory_order_relaxed);

    diffuserWriteIndex = {};
    diffuserDelaySamples = {};
    diffuserClearChannel = 0;
    diffuserClearStage = 0;
    diffuserClearIndex = 0;
    diffuserClearing = true;
}

void TBDelayEngine::clearDiffuserIncrementally (int blockSamples)
{
    if (! diffuserClearing)
        return;

    int remaining = std::max (8, blockSamples * 8);

    while (remaining > 0 && diffuserClearing)
    {
        auto& memory =
            diffuserMemory[static_cast<std::size_t> (diffuserClearChannel)]
                           [static_cast<std::size_t> (diffuserClearStage)];
        const int available =
            static_cast<int> (memory.size()) - diffuserClearIndex;
        const int toClear = std::min (remaining, available);

        std::fill_n (memory.begin() + diffuserClearIndex, toClear, 0.0f);
        diffuserClearIndex += toClear;
        remaining -= toClear;

        if (diffuserClearIndex >= static_cast<int> (memory.size()))
        {
            diffuserClearIndex = 0;
            if (++diffuserClearStage >= diffuserStages)
            {
                diffuserClearStage = 0;
                if (++diffuserClearChannel >= maxChannels)
                {
                    diffuserClearChannel = 0;
                    diffuserClearing = false;
                }
            }
        }
    }
}

//==============================================================================
void TBDelayEngine::process (
    juce::AudioBuffer<float>& buffer, const Parameters& inputParameters)
{
    juce::ScopedNoDenormals noDenormals;

    Parameters parameters = inputParameters;
    const Parameters defaults;
    parameters.timeMs = finiteOr (parameters.timeMs, defaults.timeMs);
    parameters.offsetPercent = finiteOr (parameters.offsetPercent, defaults.offsetPercent);
    parameters.feedbackPercent = finiteOr (parameters.feedbackPercent, defaults.feedbackPercent);
    parameters.spacingPercent = finiteOr (parameters.spacingPercent, defaults.spacingPercent);
    parameters.swingPercent = finiteOr (parameters.swingPercent, defaults.swingPercent);
    parameters.widthPercent = finiteOr (parameters.widthPercent, defaults.widthPercent);
    parameters.mixPercent = finiteOr (parameters.mixPercent, defaults.mixPercent);
    parameters.duckPercent = finiteOr (parameters.duckPercent, defaults.duckPercent);
    parameters.outputDb = finiteOr (parameters.outputDb, defaults.outputDb);
    parameters.lowCutHz = finiteOr (parameters.lowCutHz, defaults.lowCutHz);
    parameters.highCutHz = finiteOr (parameters.highCutHz, defaults.highCutHz);
    parameters.drivePercent = finiteOr (parameters.drivePercent, defaults.drivePercent);
    parameters.agePercent = finiteOr (parameters.agePercent, defaults.agePercent);
    parameters.modRateHz = finiteOr (parameters.modRateHz, defaults.modRateHz);
    parameters.modDepthMs = finiteOr (parameters.modDepthMs, defaults.modDepthMs);
    parameters.smearPercent = finiteOr (parameters.smearPercent, defaults.smearPercent);
    parameters.diffusionSizeMs = finiteOr (
        parameters.diffusionSizeMs, defaults.diffusionSizeMs);
    parameters.shiftHz = finiteOr (parameters.shiftHz, defaults.shiftHz);
    parameters.divergePercent = finiteOr (parameters.divergePercent, defaults.divergePercent);
    parameters.grainPercent = finiteOr (parameters.grainPercent, defaults.grainPercent);
    parameters.grainSizeMs = finiteOr (parameters.grainSizeMs, defaults.grainSizeMs);
    parameters.grainDensity = finiteOr (parameters.grainDensity, defaults.grainDensity);
    parameters.sprayPercent = finiteOr (parameters.sprayPercent, defaults.sprayPercent);
    parameters.pitchSemitones = finiteOr (
        parameters.pitchSemitones, defaults.pitchSemitones);
    parameters.reversePercent = finiteOr (parameters.reversePercent, defaults.reversePercent);
    parameters.bpm = finiteOr (parameters.bpm, defaults.bpm);
    parameters.timeMode = juce::jlimit (0, 2, parameters.timeMode);
    parameters.division = juce::jlimit (0, 17, parameters.division);
    parameters.style = juce::jlimit (0, 3, parameters.style);
    parameters.taps = juce::jlimit (0, 3, parameters.taps);
    parameters.character = juce::jlimit (0, 3, parameters.character);
    parameters.timeMotion = juce::jlimit (0, 1, parameters.timeMotion);
    parameters.transformRoute = juce::jlimit (0, 1, parameters.transformRoute);

    const float targetStereoActive =
        parameters.stereoBypass ? 0.0f : 1.0f;
    const float targetColorSpaceActive =
        parameters.colorSpaceBypass ? 0.0f : 1.0f;
    const float targetGrainTransformActive =
        parameters.grainTransformBypass ? 0.0f : 1.0f;
    parameters.offsetPercent *= targetStereoActive;
    parameters.widthPercent =
        100.0f
        + targetStereoActive * (parameters.widthPercent - 100.0f);

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
        performRealtimePanic();

    const int channels = juce::jlimit (
        0, maxChannels, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();

    if (channels == 0 || samples == 0 || delayBufferSize <= 0)
        return;

    clearDiffuserIncrementally (samples);

    std::array<float*, maxChannels> data {};

    for (int channel = 0; channel < channels; ++channel)
        data[static_cast<std::size_t> (channel)] =
            buffer.getWritePointer (channel);

    const auto targetDelaySamples =
        calculateChannelDelaySamples (parameters);
    const int tapCount = getTapCount (parameters.taps);
    const float targetAge =
        targetColorSpaceActive
        * juce::jlimit (0.0f, 1.0f, parameters.agePercent / 100.0f);
    const float targetDrive =
        targetColorSpaceActive
        * juce::jlimit (0.0f, 1.0f, parameters.drivePercent / 100.0f);
    const float targetSpacingPercent =
        juce::jlimit (25.0f, 200.0f, parameters.spacingPercent);
    const float targetSwingPercent =
        juce::jlimit (-50.0f, 50.0f, parameters.swingPercent);
    const double requestedModDepthSamples =
        static_cast<double> (targetColorSpaceActive)
        * juce::jlimit (0.0, 20.0,
                       static_cast<double> (parameters.modDepthMs))
        * currentSampleRate / 1000.0;
    const double minimumReadSamples = std::max (
        4.0, currentSampleRate * minimumDelayMs / 1000.0);
    const double modulationHeadroom = std::max (
        0.0,
        std::min (targetDelaySamples[0], targetDelaySamples[1])
            - minimumReadSamples);
    const double targetModDepthSamples = std::min (
        requestedModDepthSamples, modulationHeadroom);
    const float targetMix =
        juce::jlimit (0.0f, 1.0f, parameters.mixPercent / 100.0f);
    const float targetWidth =
        juce::jlimit (0.0f, 2.0f, parameters.widthPercent / 100.0f);
    const float targetDuck =
        juce::jlimit (0.0f, 1.0f, parameters.duckPercent / 100.0f);
    const float targetGrain =
        targetGrainTransformActive
        * juce::jlimit (0.0f, 1.0f, parameters.grainPercent / 100.0f);
    const float targetSmear =
        targetColorSpaceActive
        * juce::jlimit (0.0f, 1.0f, parameters.smearPercent / 100.0f);
    const float targetOutputGain =
        juce::Decibels::decibelsToGain (
            juce::jlimit (-18.0f, 6.0f, parameters.outputDb));

    const float feedbackPercent =
        juce::jlimit (0.0f, 110.0f, parameters.feedbackPercent);
    float targetFeedbackGain =
        feedbackPercent <= 100.0f
            ? 0.985f * feedbackPercent / 100.0f
            : 0.985f + 0.0035f * (feedbackPercent - 100.0f);

    // The >100% region is only musical while something is being played into it.
    // With no input the loop must not hold or grow, so its gain is pulled back
    // under unity and the existing smoother ramps it there without a click.
    constexpr double silenceGraceSeconds = 2.0;
    if (silentInputSeconds >= silenceGraceSeconds)
        targetFeedbackGain = juce::jmin (targetFeedbackGain, 0.985f);
    const float targetFeedbackPolarity =
        parameters.offsetPercent < -0.0001f ? -1.0f : 1.0f;

    // Treat LOW CUT/HIGH CUT as an ordered band.  During automation the two
    // parameters can briefly cross; preserving the order avoids collapsing
    // the feedback path to silence while retaining both host values.
    const float effectiveLowCutHz = parameters.lowCutHz;
    const float effectiveHighCutHz = juce::jmax (
        parameters.highCutHz, effectiveLowCutHz);
    const float targetFeedbackLowpass = coefficientForCutoff (
        effectiveHighCutHz, currentSampleRate);
    const float targetFeedbackHighpass = coefficientForCutoff (
        effectiveLowCutHz, currentSampleRate);

    float characterCutoff = static_cast<float> (currentSampleRate * 0.45);

    switch (parameters.character)
    {
        case 1:
            characterCutoff = 18000.0f
                              + targetAge * (3500.0f - 18000.0f);
            break;
        case 2:
            characterCutoff = 12000.0f
                              + targetAge * (2200.0f - 12000.0f);
            break;
        case 3:
            characterCutoff = 20000.0f
                              + targetAge * (5000.0f - 20000.0f);
            break;
        default: break;
    }

    const float targetCharacterLowpass =
        coefficientForCutoff (characterCutoff, currentSampleRate);

    const float shiftLimit =
        std::min (2000.0f, static_cast<float> (currentSampleRate * 0.4));
    const float leftShift =
        targetGrainTransformActive
        * juce::jlimit (-shiftLimit, shiftLimit, parameters.shiftHz);
    const float divergence =
        juce::jlimit (0.0f, 1.0f, parameters.divergePercent / 100.0f);
    const std::array<float, maxChannels> targetShift {
        leftShift, leftShift * (1.0f - 2.0f * divergence)
    };

    GrainSettings grainSettings;
    grainSettings.lengthSamples = juce::jlimit (
        8, std::max (8, static_cast<int> (0.5 * currentSampleRate)),
        static_cast<int> (
            std::round (
                juce::jlimit (10.0f, 500.0f, parameters.grainSizeMs)
                * static_cast<float> (currentSampleRate) / 1000.0f)));
    grainSettings.density = juce::jlimit (
        1, maxGrains, static_cast<int> (std::round (parameters.grainDensity)));
    grainSettings.pitchRatio = std::pow (
        2.0, juce::jlimit (-24.0, 24.0,
                           static_cast<double> (parameters.pitchSemitones))
                 / 12.0);
    grainSettings.sprayPercent =
        juce::jlimit (0.0f, 100.0f, parameters.sprayPercent);
    grainSettings.reversePercent =
        juce::jlimit (0.0f, 100.0f, parameters.reversePercent);
    grainSettings.enabled = targetGrain > 0.0001f;

    if (! parametersInitialised)
    {
        smoothedFeedbackGain = targetFeedbackGain;
        smoothedFeedbackPolarity = targetFeedbackPolarity;
        smoothedSpacingPercent = targetSpacingPercent;
        smoothedSwingPercent = targetSwingPercent;
        smoothedModDepthSamples = targetModDepthSamples;
        smoothedMix = targetMix;
        smoothedWidth = targetWidth;
        smoothedDuck = targetDuck;
        smoothedOutputGain = targetOutputGain;
        smoothedDrive = targetDrive;
        smoothedAge = targetAge;
        smoothedGrainAmount = targetGrain;
        smoothedSmearAmount = targetSmear;
        smoothedStereoActive = targetStereoActive;
        smoothedColorSpaceActive = targetColorSpaceActive;
        feedbackFilterLowpassCoefficient = targetFeedbackLowpass;
        feedbackFilterHighpassCoefficient = targetFeedbackHighpass;
        characterLowpassCoefficient = targetCharacterLowpass;
        freezeInputGain = 1.0f;
        smoothedShiftHz = targetShift;
        parametersInitialised = true;
    }

    const float modulationRate =
        juce::jlimit (0.02f, 20.0f, parameters.modRateHz);
    const bool transformInLoop = parameters.transformRoute == 0;

    const double spacing = juce::jlimit (
        0.25, 2.0, static_cast<double> (parameters.spacingPercent) / 100.0);
    const double swing = juce::jlimit (
        -0.5, 0.5, static_cast<double> (parameters.swingPercent) / 100.0);
    double lastTapUnits =
        1.0 + static_cast<double> (tapCount - 1) * spacing;

    if (((tapCount - 1) & 1) != 0)
        lastTapUnits += 0.5 * swing * spacing;

    const double effectiveDelay = std::min (
        maximumDelaySeconds,
        std::max (targetDelaySamples[0], targetDelaySamples[1])
            * std::max (1.0, lastTapUnits) / currentSampleRate);
    lastEffectiveDelaySeconds.store (
        effectiveDelay, std::memory_order_relaxed);

    float blockInputPeak = 0.0f;
    float blockWetPeak = 0.0f;

    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex)
    {
        smoothedFeedbackGain += parameterSmoothingCoefficient
                                * (targetFeedbackGain
                                   - smoothedFeedbackGain);
        smoothedFeedbackPolarity +=
            parameterSmoothingCoefficient
            * (targetFeedbackPolarity - smoothedFeedbackPolarity);
        smoothedSpacingPercent +=
            parameterSmoothingCoefficient
            * (targetSpacingPercent - smoothedSpacingPercent);
        smoothedSwingPercent +=
            parameterSmoothingCoefficient
            * (targetSwingPercent - smoothedSwingPercent);
        smoothedModDepthSamples +=
            static_cast<double> (parameterSmoothingCoefficient)
            * (targetModDepthSamples - smoothedModDepthSamples);
        smoothedMix +=
            parameterSmoothingCoefficient * (targetMix - smoothedMix);
        smoothedWidth +=
            parameterSmoothingCoefficient * (targetWidth - smoothedWidth);
        smoothedDuck +=
            parameterSmoothingCoefficient * (targetDuck - smoothedDuck);
        smoothedOutputGain +=
            parameterSmoothingCoefficient
            * (targetOutputGain - smoothedOutputGain);
        smoothedDrive +=
            parameterSmoothingCoefficient * (targetDrive - smoothedDrive);
        smoothedAge +=
            parameterSmoothingCoefficient * (targetAge - smoothedAge);
        smoothedGrainAmount +=
            parameterSmoothingCoefficient
            * (targetGrain - smoothedGrainAmount);
        smoothedSmearAmount +=
            parameterSmoothingCoefficient
            * (targetSmear - smoothedSmearAmount);
        smoothedStereoActive +=
            parameterSmoothingCoefficient
            * (targetStereoActive - smoothedStereoActive);
        smoothedColorSpaceActive +=
            parameterSmoothingCoefficient
            * (targetColorSpaceActive - smoothedColorSpaceActive);
        feedbackFilterLowpassCoefficient +=
            parameterSmoothingCoefficient
            * (targetFeedbackLowpass
               - feedbackFilterLowpassCoefficient);
        feedbackFilterHighpassCoefficient +=
            parameterSmoothingCoefficient
            * (targetFeedbackHighpass
               - feedbackFilterHighpassCoefficient);
        characterLowpassCoefficient +=
            parameterSmoothingCoefficient
            * (targetCharacterLowpass - characterLowpassCoefficient);
        if (parameters.freeze != freezeWasActive)
        {
            freezeCaptureArmed =
                parameters.freeze && loopActivityEnvelope < 1.0e-5f;
            freezeWasActive = parameters.freeze;
        }
        const float freezeInputTarget =
            parameters.freeze && ! freezeCaptureArmed ? 0.0f : 1.0f;
        freezeInputGain +=
            freezeSmoothingCoefficient
            * (freezeInputTarget - freezeInputGain);
        const float freezeHoldAmount =
            juce::jlimit (0.0f, 1.0f, 1.0f - freezeInputGain);
        const float freezeFeedbackAmount = std::sqrt (
            std::max (
                0.0f, 1.0f - freezeInputGain * freezeInputGain));

        std::array<float, maxChannels> dry {};
        float detectorInput = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            dry[static_cast<std::size_t> (channel)] = sanitise (
                data[static_cast<std::size_t> (channel)][sampleIndex]);
            detectorInput = std::max (
                detectorInput,
                std::abs (dry[static_cast<std::size_t> (channel)]));
        }

        blockInputPeak = std::max (blockInputPeak, detectorInput);
        const float detectorCoefficient =
            detectorInput > detectorEnvelope
                ? detectorAttackCoefficient
                : detectorReleaseCoefficient;
        detectorEnvelope +=
            detectorCoefficient * (detectorInput - detectorEnvelope);

        std::array<ReadHeads, maxChannels> readHeads;
        std::array<double, maxChannels> grainBaseDelay {};
        std::array<float, maxChannels> normalWet {};

        for (int channel = 0; channel < channels; ++channel)
        {
            readHeads[static_cast<std::size_t> (channel)] =
                updateTimeState (
                    channel,
                    targetDelaySamples[static_cast<std::size_t> (channel)],
                    parameters.timeMotion);
            const auto& heads =
                readHeads[static_cast<std::size_t> (channel)];
            grainBaseDelay[static_cast<std::size_t> (channel)] =
                heads.crossfading ? heads.second : heads.first;

            const double channelPhase =
                modulationPhase
                + (channel == 0 ? 0.0
                                : 0.5 * juce::MathConstants<double>::pi);
            double modulation =
                smoothedModDepthSamples * std::sin (channelPhase);

            if (parameters.character == 1)
            {
                modulation +=
                    smoothedAge * currentSampleRate
                    * (0.00035 * std::sin (wowPhase)
                       + 0.00008
                             * std::sin (
                                 flutterPhase
                                 + static_cast<double> (channel) * 1.73));
            }
            else if (parameters.character == 2)
            {
                modulation +=
                    smoothedAge * currentSampleRate * 0.000025
                    * std::sin (
                        flutterPhase
                        + static_cast<double> (channel) * 1.13);
            }

            const double activeReadDelay =
                heads.crossfading
                    ? std::min (heads.first, heads.second)
                    : heads.first;
            const double finalModulationHeadroom = std::max (
                0.0, activeReadDelay - minimumReadSamples);
            modulation = juce::jlimit (
                -finalModulationHeadroom,
                finalModulationHeadroom,
                modulation);

            normalWet[static_cast<std::size_t> (channel)] =
                readTapSum (
                    channel, heads, modulation, tapCount,
                    smoothedSpacingPercent, smoothedSwingPercent);
        }

        if (channels == 1)
            grainBaseDelay[1] = grainBaseDelay[0];

        const auto grainWet =
            processGrains (channels, grainBaseDelay, grainSettings);
        std::array<float, maxChannels> loopSignal {};

        for (int channel = 0; channel < channels; ++channel)
        {
            float source = normalWet[static_cast<std::size_t> (channel)];

            if (transformInLoop)
            {
                source += smoothedGrainAmount
                          * (grainWet[static_cast<std::size_t> (channel)]
                             - source);
            }

            const float characterisedSource = applyPlaybackCharacter (
                channel, source, parameters.character);
            const float playbackColourAmount =
                smoothedColorSpaceActive * (1.0f - freezeHoldAmount);
            source +=
                playbackColourAmount * (characterisedSource - source);

            if (transformInLoop)
            {
                source = processFrequencyShift (
                    channel, source,
                    targetShift[static_cast<std::size_t> (channel)]);
            }

            source = processDiffusion (
                channel, source,
                smoothedSmearAmount * (1.0f - freezeHoldAmount),
                parameters.diffusionSizeMs);
            const float filteredSource =
                processFeedbackFilters (channel, source);
            const float filterColourAmount =
                smoothedColorSpaceActive * (1.0f - freezeHoldAmount);
            loopSignal[static_cast<std::size_t> (channel)] =
                source + filterColourAmount * (filteredSource - source);
        }

        float loopPeak = 0.0f;
        for (int channel = 0; channel < channels; ++channel)
            loopPeak = std::max (
                loopPeak,
                std::abs (loopSignal[static_cast<std::size_t> (channel)]));
        const float loopEnvelopeCoefficient =
            loopPeak > loopActivityEnvelope
                ? detectorAttackCoefficient
                : detectorReleaseCoefficient;
        loopActivityEnvelope +=
            loopEnvelopeCoefficient
            * (loopPeak - loopActivityEnvelope);
        const bool coldStartCaptureTriggered =
            freezeCaptureArmed && loopActivityEnvelope >= 1.0e-5f;
        if (coldStartCaptureTriggered)
        {
            freezeCaptureArmed = false;
            freezeInputGain = 0.0f;
        }

        std::array<float, maxChannels> feedback {};
        const float feedbackGainForWrite =
            coldStartCaptureTriggered
                ? 1.0f
                : smoothedFeedbackGain
                      + freezeFeedbackAmount
                            * (1.0f - smoothedFeedbackGain);

        const float pingPongAmount =
            channels == 2 && parameters.style == 2
                ? smoothedStereoActive
                : 0.0f;
        if (channels == 2 && parameters.style == 2)
        {
            feedback[0] =
                (loopSignal[0]
                     + pingPongAmount * (loopSignal[1] - loopSignal[0]))
                * feedbackGainForWrite
                          * smoothedFeedbackPolarity;
            feedback[1] =
                (loopSignal[1]
                     + pingPongAmount * (loopSignal[0] - loopSignal[1]))
                * feedbackGainForWrite
                          * smoothedFeedbackPolarity;
        }
        else
        {
            for (int channel = 0; channel < channels; ++channel)
                feedback[static_cast<std::size_t> (channel)] =
                    loopSignal[static_cast<std::size_t> (channel)]
                    * feedbackGainForWrite * smoothedFeedbackPolarity;
        }

        std::array<float, maxChannels> wet = loopSignal;

        if (! transformInLoop)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                float post =
                    loopSignal[static_cast<std::size_t> (channel)];
                post += smoothedGrainAmount
                        * (grainWet[static_cast<std::size_t> (channel)]
                           - post);
                wet[static_cast<std::size_t> (channel)] =
                    processFrequencyShift (
                        channel, post,
                        targetShift[static_cast<std::size_t> (channel)]);
            }
        }

        auto delayInput = dry;
        if (channels == 2 && parameters.style == 2)
        {
            const float monoSeed = 0.5f * (dry[0] + dry[1]);
            delayInput[0] =
                dry[0] + pingPongAmount * (monoSeed - dry[0]);
            delayInput[1] =
                dry[1] * (1.0f - pingPongAmount);
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            const float uncolouredWrite =
                freezeInputGain
                    * delayInput[static_cast<std::size_t> (channel)]
                + feedback[static_cast<std::size_t> (channel)];
            const float characterisedWrite = applyRecordCharacter (
                channel, uncolouredWrite, parameters.character,
                smoothedDrive, smoothedAge);
            const float recordColourAmount =
                smoothedColorSpaceActive * (1.0f - freezeHoldAmount);
            const float writeValue = boundedSoftClip (
                uncolouredWrite
                + recordColourAmount
                      * (characterisedWrite - uncolouredWrite));
            delayMemory[static_cast<std::size_t> (channel)]
                       [static_cast<std::size_t> (writeIndex)] =
                writeValue;
        }
        delayGenerationTags[static_cast<std::size_t> (writeIndex)] =
            delayGeneration;

        if (channels == 2)
        {
            const float middle = 0.5f * (wet[0] + wet[1]);
            const float side =
                0.5f * (wet[0] - wet[1]) * smoothedWidth;
            wet[0] = middle + side;
            wet[1] = middle - side;
        }

        const float duckEnvelope =
            detectorEnvelope / (detectorEnvelope + 0.1f);
        const float duckGain =
            1.0f - smoothedDuck * juce::jlimit (
                                       0.0f, 1.0f, duckEnvelope);
        const float dryGain = std::cos (
            0.5f * juce::MathConstants<float>::pi * smoothedMix);
        const float wetGain = std::sin (
            0.5f * juce::MathConstants<float>::pi * smoothedMix);

        for (int channel = 0; channel < channels; ++channel)
        {
            const float audibleWet =
                wet[static_cast<std::size_t> (channel)] * duckGain;
            blockWetPeak =
                std::max (blockWetPeak, std::abs (audibleWet));
            float output =
                (dry[static_cast<std::size_t> (channel)] * dryGain
                 + audibleWet * wetGain)
                * smoothedOutputGain;

            data[static_cast<std::size_t> (channel)][sampleIndex] =
                sanitise (output);
        }

        writeIndex = (writeIndex + 1) % delayBufferSize;
        modulationPhase +=
            twoPi * static_cast<double> (modulationRate)
            / currentSampleRate;
        wowPhase +=
            twoPi * 0.37 * static_cast<double> (modulationRate)
            / currentSampleRate;
        flutterPhase +=
            twoPi * (5.0 + 1.7 * static_cast<double> (modulationRate))
            / currentSampleRate;

        if (modulationPhase > twoPi)
            modulationPhase -= twoPi;

        if (wowPhase > twoPi)
            wowPhase -= twoPi;

        if (flutterPhase > twoPi)
            flutterPhase -= twoPi;
    }

    constexpr float inputSilenceThreshold = 1.0e-4f;
    if (blockInputPeak < inputSilenceThreshold)
        silentInputSeconds +=
            static_cast<double> (samples) / currentSampleRate;
    else
        silentInputSeconds = 0.0;

    const float previousInput =
        inputLevel.load (std::memory_order_relaxed);
    const float previousWet =
        wetLevel.load (std::memory_order_relaxed);
    inputLevel.store (
        0.75f * previousInput + 0.25f * blockInputPeak,
        std::memory_order_relaxed);
    wetLevel.store (
        0.75f * previousWet + 0.25f * blockWetPeak,
        std::memory_order_relaxed);
}

//==============================================================================
double TBDelayEngine::getTailSeconds (float feedbackPercent) const
{
    const double delay = juce::jlimit (
        minimumDelayMs / 1000.0, maximumDelaySeconds,
        lastEffectiveDelaySeconds.load (std::memory_order_relaxed));
    const double percent = juce::jlimit (
        0.0, 110.0,
        static_cast<double> (finiteOr (feedbackPercent, 0.0f)));

    if (percent >= 100.0)
        return 120.0;

    const double gain = 0.985 * percent / 100.0;

    if (gain <= 0.001)
        return delay;

    const double repeatsToMinus60dB =
        std::log (0.001) / std::log (gain);
    return juce::jlimit (
        delay, 120.0, delay * (1.0 + repeatsToMinus60dB));
}

float TBDelayEngine::getInputLevel() const
{
    return inputLevel.load (std::memory_order_relaxed);
}

float TBDelayEngine::getWetLevel() const
{
    return wetLevel.load (std::memory_order_relaxed);
}
