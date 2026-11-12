#include "DelayMapModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>

namespace
{
constexpr double minimumDelayMs = 10.0;
constexpr double maximumDelayMs = 8000.0;
constexpr int maximumGeneration = 8;
constexpr int maximumVisibleEvents = 72;
constexpr float minimumVisibleLevel = 0.0125f;

constexpr std::array<double, 18> divisionBeats {
    0.125, 1.0 / 12.0, 0.1875,
    0.25, 1.0 / 6.0, 0.375,
    0.5, 1.0 / 3.0, 0.75,
    1.0, 2.0 / 3.0, 1.5,
    2.0, 4.0 / 3.0, 3.0,
    4.0, 8.0 / 3.0, 6.0
};

template <typename Value>
Value clampValue (Value minimum, Value maximum, Value value)
{
    return std::max (minimum, std::min (maximum, value));
}

double finiteOr (double value, double fallback)
{
    return std::isfinite (value) ? value : fallback;
}

float finiteOr (float value, float fallback)
{
    return std::isfinite (value) ? value : fallback;
}

double calculateBaseDelayMs (const TBDelayMapParameters& parameters)
{
    const auto timeMode = clampValue (0, 2, parameters.timeMode);

    if (timeMode == 1)
    {
        const auto bpm = clampValue (
            20.0, 400.0, finiteOr (parameters.bpm, 120.0));
        const auto division = clampValue (
            0, static_cast<int> (divisionBeats.size()) - 1,
            parameters.division);
        return clampValue (
            minimumDelayMs, maximumDelayMs,
            (60000.0 / bpm)
                * divisionBeats[static_cast<std::size_t> (division)]);
    }

    auto delayMs = clampValue (
        minimumDelayMs, maximumDelayMs,
        finiteOr (parameters.timeMs, 375.0));

    if (timeMode == 2)
    {
        const auto frequency = 1000.0 / delayMs;
        const auto midiNote =
            69.0 + 12.0 * std::log2 (frequency / 440.0);
        const auto tunedFrequency =
            440.0
            * std::pow (
                2.0, (std::round (midiNote) - 69.0) / 12.0);
        delayMs = 1000.0 / tunedFrequency;
    }

    return clampValue (minimumDelayMs, maximumDelayMs, delayMs);
}

float calculateFeedbackGain (const TBDelayMapParameters& parameters)
{
    if (parameters.freeze)
        return 1.0f;

    const auto percent = clampValue (
        0.0f, 110.0f,
        finiteOr (parameters.feedbackPercent, 48.0f));
    return percent <= 100.0f
               ? 0.985f * percent / 100.0f
               : 0.985f + 0.0035f * (percent - 100.0f);
}

double calculateNominalTailMs (
    double effectiveDelayMs,
    float feedbackGain,
    float feedbackPercent,
    bool freeze)
{
    if (feedbackGain <= 0.001f)
        return effectiveDelayMs;

    if (freeze || feedbackPercent >= 100.0f)
        return 120000.0;

    const auto repeatsToMinus60Db =
        std::log (0.001)
        / std::log (
            std::max (0.0001, static_cast<double> (feedbackGain)));
    return clampValue (
        effectiveDelayMs, 120000.0,
        effectiveDelayMs * (1.0 + repeatsToMinus60Db));
}

struct FeedbackState
{
    double timeMs = 0.0;
    float amplitude = 1.0f;
    double pitchRatio = 1.0;
    float shiftOffsetHz = 0.0f;
    int lane = 0;
    int sourceLane = 0;
    int generation = 0;
};

struct EarlierState
{
    bool operator() (
        const FeedbackState& first,
        const FeedbackState& second) const noexcept
    {
        return first.timeMs > second.timeMs;
    }
};

struct GrainSupport
{
    double minimumMs = 0.0;
    double maximumMs = 0.0;
};

GrainSupport calculateGrainSupport (
    double baseDelayMs,
    double grainSizeMs,
    double pitchRatio,
    double spray,
    float reverseProbability,
    double sampleRate)
{
    const auto safeSampleRate = clampValue (
        8000.0, 384000.0, finiteOr (sampleRate, 48000.0));
    const auto grainLengthSamples = std::max (
        8.0, std::round (grainSizeMs * safeSampleRate / 1000.0));
    const auto grainLengthMs =
        grainLengthSamples * 1000.0 / safeSampleRate;
    const auto safetySamples = 4.0;
    const auto maximumReadSamples =
        maximumDelayMs * safeSampleRate / 1000.0;
    const auto spreadRangeMs =
        std::min (baseDelayMs * 0.45, grainLengthMs * 2.0)
        * spray;

    const auto supportForIncrement =
        [=] (double increment)
    {
        const auto forwardSafetySamples =
            increment > 0.0
                ? grainLengthSamples * increment + safetySamples
                : safetySamples;
        const auto relativeDriftSamples =
            (1.0 - increment)
            * std::max (0.0, grainLengthSamples - 1.0);
        const auto minimumStartSamples = std::min (
            forwardSafetySamples, maximumReadSamples);
        const auto maximumStartSamples = std::max (
            minimumStartSamples,
            maximumReadSamples
                - std::max (0.0, relativeDriftSamples));
        const auto minimumStartMs =
            minimumStartSamples * 1000.0 / safeSampleRate;
        const auto maximumStartMs =
            maximumStartSamples * 1000.0 / safeSampleRate;
        const auto startMinimum = clampValue (
            minimumStartMs, maximumStartMs,
            baseDelayMs - spreadRangeMs);
        const auto startMaximum = clampValue (
            minimumStartMs, maximumStartMs,
            baseDelayMs + spreadRangeMs);
        const auto relativeDriftMs =
            relativeDriftSamples * 1000.0 / safeSampleRate;
        return GrainSupport {
            startMinimum + std::min (0.0, relativeDriftMs),
            startMaximum + std::max (0.0, relativeDriftMs)
        };
    };

    const auto forward = supportForIncrement (pitchRatio);
    const auto reverse = supportForIncrement (-pitchRatio);

    if (reverseProbability <= 0.0001f)
        return forward;
    if (reverseProbability >= 0.9999f)
        return reverse;

    return {
        std::min (forward.minimumMs, reverse.minimumMs),
        std::max (forward.maximumMs, reverse.maximumMs)
    };
}

float ratioToSemitones (double ratio)
{
    if (! std::isfinite (ratio) || ratio <= 0.0)
        return 0.0f;

    return static_cast<float> (
        12.0 * std::log2 (ratio));
}
}

double tbDelayMapTimeNormalised (double milliseconds) noexcept
{
    constexpr double minimumHorizonMs = 10.0;
    constexpr double horizonMs = 5000.0;
    const auto safeTime = std::isfinite (milliseconds)
                              ? milliseconds
                              : minimumHorizonMs;
    return (
               clampValue (
                   minimumHorizonMs, horizonMs, safeTime)
               - minimumHorizonMs)
           / (horizonMs - minimumHorizonMs);
}

float tbDelayMapPitchNormalised (
    const TBDelayMapEvent& event) noexcept
{
    const auto safePitch = std::isfinite (event.pitchSemitones)
                               ? event.pitchSemitones
                               : 0.0f;
    const auto safeShift = std::isfinite (event.cumulativeShiftHz)
                               ? event.cumulativeShiftHz
                               : 0.0f;
    if (std::abs (safePitch) >= 0.05f)
        return clampValue (
            -1.0f, 1.0f, safePitch / 24.0f);

    return clampValue (
        -1.0f, 1.0f,
        std::tanh (safeShift / 600.0f) * 0.55f);
}

TBDelayMapPrediction buildTBDelayMapPrediction (
    const TBDelayMapParameters& input)
{
    auto parameters = input;
    if (parameters.stereoBypass)
    {
        parameters.offsetPercent = 0.0f;
        parameters.style = 0;
    }
    if (parameters.colorSpaceBypass)
    {
        parameters.modDepthMs = 0.0f;
        parameters.smearPercent = 0.0f;
    }
    if (parameters.grainTransformBypass)
    {
        parameters.shiftHz = 0.0f;
        parameters.divergePercent = 0.0f;
        parameters.grainPercent = 0.0f;
    }

    TBDelayMapPrediction prediction;
    const auto baseDelayMs = calculateBaseDelayMs (parameters);
    const auto offset = clampValue (
        -1.0, 1.0,
        static_cast<double> (
            finiteOr (parameters.offsetPercent, 0.0f)) / 100.0);
    const auto style = clampValue (0, 3, parameters.style);

    auto rightMultiplier = 1.0;
    if (style == 1)
        rightMultiplier = 1.0 + 0.5 * offset;
    else if (style == 2)
        rightMultiplier = 1.0 + 0.25 * offset;
    else if (style == 3)
        rightMultiplier = std::pow (2.0, offset);

    prediction.channelDelayMs = {
        baseDelayMs,
        clampValue (
            minimumDelayMs, maximumDelayMs,
            baseDelayMs * rightMultiplier)
    };
    prediction.feedbackGain = calculateFeedbackGain (parameters);
    prediction.tapCount = 1 << clampValue (0, 3, parameters.taps);

    const auto spacing = clampValue (
        0.25, 2.0,
        static_cast<double> (
            finiteOr (parameters.spacingPercent, 100.0f)) / 100.0);
    const auto swing = clampValue (
        -0.5, 0.5,
        static_cast<double> (
            finiteOr (parameters.swingPercent, 0.0f)) / 100.0);
    std::array<double, 8> tapUnits {};
    std::array<float, 8> tapWeights {};
    auto weightSum = 0.0f;

    for (int tap = 0; tap < prediction.tapCount; ++tap)
    {
        auto units = 1.0 + static_cast<double> (tap) * spacing;
        if ((tap & 1) != 0)
            units += 0.5 * swing * spacing;
        tapUnits[static_cast<std::size_t> (tap)] =
            std::max (0.125, units);
        tapWeights[static_cast<std::size_t> (tap)] =
            1.0f / (1.0f + 0.15f * static_cast<float> (tap));
        weightSum += tapWeights[static_cast<std::size_t> (tap)];
    }

    for (int tap = 0; tap < prediction.tapCount; ++tap)
        tapWeights[static_cast<std::size_t> (tap)] /= weightSum;

    prediction.channelCount = clampValue (1, 2, parameters.channelCount);
    prediction.primaryEventTimeMs = prediction.channelDelayMs;
    if (style == 2 && prediction.channelCount == 2)
        prediction.primaryEventTimeMs[1] =
            prediction.channelDelayMs[0] + prediction.channelDelayMs[1];
    prediction.feedbackPolarity = offset < -0.000001 ? -1.0f : 1.0f;

    const auto divergence = clampValue (
        0.0f, 1.0f,
        finiteOr (parameters.divergePercent, 0.0f) / 100.0f);
    const std::array<float, 2> channelShiftHz {
        finiteOr (parameters.shiftHz, 0.0f),
        finiteOr (parameters.shiftHz, 0.0f)
            * (1.0f - 2.0f * divergence)
    };
    const auto requestedModDepthMs = clampValue (
        0.0, 20.0,
        static_cast<double> (
            finiteOr (parameters.modDepthMs, 0.8f)));
    const auto modulationHeadroomMs = std::max (
        0.0,
        std::min (
            prediction.channelDelayMs[0],
            prediction.channelDelayMs[
                static_cast<std::size_t> (
                    prediction.channelCount - 1)])
            - minimumDelayMs);
    const auto modDepthMs = std::min (
        requestedModDepthMs, modulationHeadroomMs);
    const auto feedbackPercent = clampValue (
        0.0f, 110.0f,
        finiteOr (parameters.feedbackPercent, 48.0f));
    const auto lastTapUnits =
        tapUnits[static_cast<std::size_t> (prediction.tapCount - 1)];
    const auto effectiveDelayMs = clampValue (
        minimumDelayMs, maximumDelayMs,
        std::max (
            prediction.channelDelayMs[0],
            prediction.channelDelayMs[
                static_cast<std::size_t> (
                    prediction.channelCount - 1)])
            * std::max (1.0, lastTapUnits));
    const auto nominalTailMs = calculateNominalTailMs (
        effectiveDelayMs,
        prediction.feedbackGain,
        feedbackPercent,
        parameters.freeze);
    prediction.nominalTailMs = nominalTailMs;

    const auto grainAmount = clampValue (
        0.0f, 1.0f,
        finiteOr (parameters.grainPercent, 0.0f) / 100.0f);
    const auto grainSizeMs = clampValue (
        10.0, 500.0,
        static_cast<double> (
            finiteOr (parameters.grainSizeMs, 90.0f)));
    const auto spray = clampValue (
        0.0, 1.0,
        static_cast<double> (
            finiteOr (parameters.sprayPercent, 15.0f)) / 100.0);
    const auto pitchSemitones = clampValue (
        -24.0f, 24.0f,
        finiteOr (parameters.pitchSemitones, 0.0f));
    const auto grainPitchRatio = std::pow (
        2.0, static_cast<double> (pitchSemitones) / 12.0);
    const auto reverseProbability = clampValue (
        0.0f, 1.0f,
        finiteOr (parameters.reversePercent, 0.0f) / 100.0f);
    const auto grainCount = clampValue (
        1, 8,
        static_cast<int> (
            std::round (
                finiteOr (parameters.grainDensity, 4.0f))));
    const auto transformInLoop =
        clampValue (0, 1, parameters.transformRoute) == 0;

    const auto nextLane = [style, &prediction] (int lane)
    {
        return style == 2 && prediction.channelCount == 2
                   ? 1 - lane
                   : lane;
    };

    std::priority_queue<
        FeedbackState,
        std::vector<FeedbackState>,
        EarlierState> pending;
    const auto seedCount =
        style == 2 && prediction.channelCount == 2
            ? 1
            : prediction.channelCount;
    for (int lane = 0; lane < seedCount; ++lane)
        pending.push ({
            0.0, 1.0f, 1.0, 0.0f,
            lane, lane, 0
        });
    prediction.events.reserve (maximumVisibleEvents);

    while (! pending.empty()
           && static_cast<int> (prediction.events.size())
                  < maximumVisibleEvents)
    {
        const auto state = pending.top();
        pending.pop();
        const auto laneIndex =
            static_cast<std::size_t> (state.lane);

        if (grainAmount > 0.0001f
            && static_cast<int> (prediction.events.size())
                   < maximumVisibleEvents)
        {
            const auto support = calculateGrainSupport (
                prediction.channelDelayMs[laneIndex],
                grainSizeMs,
                grainPitchRatio,
                spray,
                reverseProbability,
                parameters.sampleRate);
            const auto supportCentre =
                0.5 * (support.minimumMs + support.maximumMs);
            const auto grainShift =
                static_cast<float> (grainPitchRatio)
                    * state.shiftOffsetHz
                + channelShiftHz[laneIndex];
            const auto grainLevel =
                clampValue (
                    0.0f, 1.0f,
                    std::abs (state.amplitude) * grainAmount);

            if (grainLevel >= 0.001f)
            {
                TBDelayMapEvent cloud;
                cloud.kind = TBDelayMapEvent::Kind::grainCloud;
                cloud.lane = state.lane;
                cloud.sourceLane = state.sourceLane;
                cloud.repeat = state.generation;
                cloud.grainCount = grainCount;
                cloud.timeMs = state.timeMs + supportCentre;
                cloud.timeSpreadMs =
                    0.5 * (support.maximumMs - support.minimumMs);
                cloud.durationMs = grainSizeMs;
                cloud.decayMs =
                    prediction.feedbackGain <= 0.001f
                        ? 0.0
                        : std::max (
                              0.0, nominalTailMs - cloud.timeMs);
                cloud.pitchSemitones = ratioToSemitones (
                    state.pitchRatio * grainPitchRatio);
                cloud.cumulativeShiftHz = grainShift;
                cloud.level = grainLevel;
                cloud.reverseProbability = reverseProbability;
                cloud.inverted = state.amplitude < 0.0f;
                prediction.events.push_back (cloud);
            }

            const auto grainFeedbackAmplitude =
                state.amplitude * grainAmount
                * prediction.feedbackGain
                * prediction.feedbackPolarity;
            if (transformInLoop
                && state.generation + 1 < maximumGeneration
                && std::abs (grainFeedbackAmplitude)
                       >= minimumVisibleLevel)
            {
                pending.push ({
                    state.timeMs + supportCentre,
                    grainFeedbackAmplitude,
                    state.pitchRatio * grainPitchRatio,
                    grainShift,
                    nextLane (state.lane),
                    state.sourceLane,
                    state.generation + 1
                });
            }
        }

        for (int tap = 0;
             tap < prediction.tapCount
                 && static_cast<int> (prediction.events.size())
                        < maximumVisibleEvents;
             ++tap)
        {
            const auto laneDelay = clampValue (
                minimumDelayMs, maximumDelayMs,
                prediction.channelDelayMs[laneIndex]
                    * tapUnits[static_cast<std::size_t> (tap)]);
            const auto pathAmplitude =
                state.amplitude
                * tapWeights[static_cast<std::size_t> (tap)];
            const auto normalLevel =
                clampValue (
                    0.0f, 1.0f,
                    std::abs (pathAmplitude)
                        * (1.0f - grainAmount));
            const auto audibleShift =
                state.shiftOffsetHz + channelShiftHz[laneIndex];

            if (normalLevel >= 0.001f)
            {
                TBDelayMapEvent event;
                event.kind = TBDelayMapEvent::Kind::echo;
                event.lane = state.lane;
                event.sourceLane = state.sourceLane;
                event.repeat = state.generation;
                event.tap = tap;
                event.timeMs = state.timeMs + laneDelay;
                event.timeSpreadMs = modDepthMs;
                event.decayMs =
                    prediction.feedbackGain <= 0.001f
                        ? 0.0
                        : std::max (
                              0.0, nominalTailMs - event.timeMs);
                event.pitchSemitones =
                    ratioToSemitones (state.pitchRatio);
                event.cumulativeShiftHz = audibleShift;
                event.level = normalLevel;
                event.inverted = pathAmplitude < 0.0f;
                prediction.events.push_back (event);
            }

            const auto feedbackAmplitude =
                pathAmplitude
                * (transformInLoop ? 1.0f - grainAmount : 1.0f)
                * prediction.feedbackGain
                * prediction.feedbackPolarity;
            if (state.generation + 1 < maximumGeneration
                && std::abs (feedbackAmplitude)
                       >= minimumVisibleLevel)
            {
                pending.push ({
                    state.timeMs + laneDelay,
                    feedbackAmplitude,
                    state.pitchRatio,
                    transformInLoop
                        ? audibleShift
                        : state.shiftOffsetHz,
                    nextLane (state.lane),
                    state.sourceLane,
                    state.generation + 1
                });
            }
        }
    }

    return prediction;
}
