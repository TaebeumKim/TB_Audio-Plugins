/*
  ==============================================================================

    SpatialRenderer.cpp

  ==============================================================================
*/

#include "SpatialRenderer.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float maxDistanceDelaySeconds = 0.040f;
constexpr float ceilingReflectionSeconds = 0.011f;
constexpr float floorReflectionSeconds = 0.023f;
constexpr float maxReflectionSeconds = 0.060f;
constexpr float inverseSquareRootOfTwo = 0.70710678118f;
constexpr float rearNotchFrequency = 7000.0f;
constexpr float maximumRearNotchMix = 0.70f;
constexpr float stereoSourceHalfWidthDegrees = 30.0f;
// Plane radius, in parameter units, inside which the image opens up around the listener.
constexpr float nearFieldRadius = 0.25f;
constexpr int filterUpdateInterval = 16;

float smoothUnit (float value)
{
    const auto limited = juce::jlimit (0.0f, 1.0f, value);
    return limited * limited * (3.0f - 2.0f * limited);
}

float getObjectAzimuth (float pan, float frontBack)
{
    if (std::abs (pan) < 0.0001f && std::abs (frontBack) < 0.0001f)
        return 0.0f;

    return juce::radiansToDegrees (std::atan2 (pan, frontBack));
}

// Where the object is heard from, taking Pan / Front Back / Height as a
// position around the listener rather than as three independent faders.
struct ObjectPlacement
{
    float azimuth = 0.0f;        // Degrees: 0 front, +90 right, +/-180 rear.
    float elevation = 0.0f;      // Degrees above the horizontal plane, -90 to +90.
    float focus = 0.0f;          // Speakers: 0 above/below/at the listener, 1 outside the near field.
    float binauralFocus = 0.0f;  // Headphones: 0 only at the listener itself.
    float lateral = 0.0f;        // Stereo pan position for speakers at +/-45 degrees.
    float rear = 0.0f;           // Amount of rear colouration for speaker output.
};

ObjectPlacement getObjectPlacement (float pan, float frontBack, float height)
{
    ObjectPlacement placement;
    const auto radius = std::hypot (pan, frontBack);
    const auto distanceFromListener = std::hypot (radius, height);

    if (distanceFromListener >= 1.0e-6f)
    {
        // The angle the object is seen at: straight overhead when it is
        // above the listener, lower the further out it sits on the plane.
        placement.elevation = juce::radiansToDegrees (std::atan2 (height, radius));
        placement.binauralFocus = smoothUnit (distanceFromListener / nearFieldRadius);
    }

    if (radius < 1.0e-6f)
        return placement;

    placement.azimuth = getObjectAzimuth (pan, frontBack);
    // Direction is undefined at the listener. Like a game engine's spread
    // curve, open the image there instead of flipping it across the head.
    placement.focus = smoothUnit (radius / nearFieldRadius);

    // Tangent law: Pan is unchanged along the front edge, anything further
    // round than a speaker stays in that speaker, and the rear arc mirrors
    // the front arc back to the centre.
    const auto depth = std::abs (frontBack);
    const auto direction = std::abs (pan) >= depth ? (pan < 0.0f ? -1.0f : 1.0f) : pan / depth;
    placement.lateral = direction * placement.focus;
    placement.rear = smoothUnit (juce::jmax (0.0f, -frontBack / radius)) * placement.focus;
    return placement;
}

float getDistanceAttenuationDb (float distance, int model)
{
    const auto clampedDistance = juce::jlimit (0.0f, 1.0f, distance);
    if (model == 1)
        return -6.0f * std::pow (clampedDistance, 0.70f);
    if (model == 2)
        return -18.0f * clampedDistance;
    return -12.0f * std::pow (clampedDistance, 1.35f);
}
}

void SpatialRenderer::FractionalDelay::prepare (int maximumDelaySamples)
{
    buffer.assign (static_cast<size_t> (maximumDelaySamples + 2), 0.0f);
    writeIndex = 0;
}

void SpatialRenderer::FractionalDelay::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

float SpatialRenderer::FractionalDelay::process (float input, float delaySamples)
{
    if (buffer.empty())
        return input;

    buffer[static_cast<size_t> (writeIndex)] = input;

    auto readPosition = static_cast<float> (writeIndex) - juce::jlimit (0.0f,
                                                                          static_cast<float> (buffer.size() - 2),
                                                                          delaySamples);

    while (readPosition < 0.0f)
        readPosition += static_cast<float> (buffer.size());

    const auto indexA = static_cast<int> (readPosition) % static_cast<int> (buffer.size());
    const auto indexB = (indexA + 1) % static_cast<int> (buffer.size());
    const auto fraction = readPosition - static_cast<float> (static_cast<int> (readPosition));
    const auto result = buffer[static_cast<size_t> (indexA)]
                      + fraction * (buffer[static_cast<size_t> (indexB)] - buffer[static_cast<size_t> (indexA)]);

    writeIndex = (writeIndex + 1) % static_cast<int> (buffer.size());
    return result;
}

void SpatialRenderer::prepare (double sampleRate, int samplesPerBlock, int outputChannels)
{
    currentSampleRate = sampleRate;
    surroundChannelCount = juce::jlimit (0, maximumOutputChannels, outputChannels);
    maximumDistanceDelaySamples = juce::roundToInt (maxDistanceDelaySeconds * static_cast<float> (sampleRate));
    const auto maximumDelaySamples = juce::roundToInt ((maxDistanceDelaySeconds + maxReflectionSeconds)
                                                        * static_cast<float> (sampleRate));

    for (auto& delay : ceilingReflectionDelays)
        delay.prepare (maximumDelaySamples);

    for (auto& delay : floorReflectionDelays)
        delay.prepare (maximumDelaySamples);

    const juce::dsp::ProcessSpec monoSpec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    const auto maximumFilterFrequency = static_cast<float> (sampleRate * 0.45);

    for (int channel = 0; channel < maximumOutputChannels; ++channel)
    {
        directLowPasses[static_cast<size_t> (channel)].prepare (monoSpec);
        directLowPasses[static_cast<size_t> (channel)].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        reflectionLowPasses[static_cast<size_t> (channel)].prepare (monoSpec);
        reflectionLowPasses[static_cast<size_t> (channel)].setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        // Establish second-order storage before processing; later coefficient
        // updates reuse it rather than allocating on the audio thread.
        *heightShelves[static_cast<size_t> (channel)].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
            sampleRate, juce::jmin (4000.0f, maximumFilterFrequency), 0.7071f, 1.0f);
        heightShelves[static_cast<size_t> (channel)].reset();
        rearNotches[static_cast<size_t> (channel)].coefficients = juce::dsp::IIR::Coefficients<float>::makeNotch (
            sampleRate, juce::jmin (rearNotchFrequency, maximumFilterFrequency), 1.4f);
        rearNotches[static_cast<size_t> (channel)].reset();
    }

    pan.reset (sampleRate, 0.02);
    frontBack.reset (sampleRate, 0.02);
    height.reset (sampleRate, 0.02);
    distance.reset (sampleRate, 0.02);
    room.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    directFilterMix.reset (sampleRate, 0.02);
    lastFilterDistance = -1.0f;
    samplesUntilFilterUpdate = 0;
    hrtfRenderer.prepare (sampleRate);

   #if JUCE_DEBUG
    static const auto regressionChecksPassed = runRegressionChecks();
    jassert (regressionChecksPassed);
   #endif

    reset();
}

void SpatialRenderer::reset()
{
    for (auto& delay : ceilingReflectionDelays)
        delay.reset();

    for (auto& delay : floorReflectionDelays)
        delay.reset();

    for (int channel = 0; channel < maximumOutputChannels; ++channel)
    {
        directLowPasses[static_cast<size_t> (channel)].reset();
        reflectionLowPasses[static_cast<size_t> (channel)].reset();
        heightShelves[static_cast<size_t> (channel)].reset();
        rearNotches[static_cast<size_t> (channel)].reset();
    }

    hrtfRenderer.reset();
}

void SpatialRenderer::setParameters (SpatialRendererParameters newParameters, bool forceImmediately)
{
    pan.setTargetValue (newParameters.pan);
    frontBack.setTargetValue (newParameters.frontBack);
    height.setTargetValue (newParameters.height);
    distance.setTargetValue (newParameters.distance);
    room.setTargetValue (newParameters.room);
    outputGain.setTargetValue (newParameters.outputGain);
    distanceModel = juce::jlimit (0, 2, newParameters.distanceModel);
    const auto wasUsingHeadphones = surroundChannelCount == 2
                                && (renderMode == 1 || multichannelBinauralInput) && hrtfRenderer.isReady();
    renderMode = newParameters.renderMode;
    const auto usingHeadphones = surroundChannelCount == 2
                             && (renderMode == 1 || multichannelBinauralInput) && hrtfRenderer.isReady();
    if (usingHeadphones != wasUsingHeadphones)
        hrtfRenderer.reset(); // An inactive FIR must not replay old input on re-entry.
    const auto targetRear = getObjectPlacement (newParameters.pan, newParameters.frontBack, newParameters.height).rear;
    const auto needsDirectFilter = newParameters.distance > 0.0001f
                               || (! usingHeadphones && targetRear > 0.0001f);
    directFilterMix.setTargetValue (needsDirectFilter ? 1.0f : 0.0f);

    if (forceImmediately)
    {
        pan.setCurrentAndTargetValue (newParameters.pan);
        frontBack.setCurrentAndTargetValue (newParameters.frontBack);
        height.setCurrentAndTargetValue (newParameters.height);
        distance.setCurrentAndTargetValue (newParameters.distance);
        room.setCurrentAndTargetValue (newParameters.room);
        outputGain.setCurrentAndTargetValue (newParameters.outputGain);
        directFilterMix.setCurrentAndTargetValue (needsDirectFilter ? 1.0f : 0.0f);
        updateFilterSettings (newParameters.distance, newParameters.height, targetRear);
    }
}

void SpatialRenderer::updateFilterSettings (float currentDistance, float currentHeight, float currentRear)
{
    const auto usingHeadphones = surroundChannelCount == 2
                             && (renderMode == 1 || multichannelBinauralInput) && hrtfRenderer.isReady();
    if (currentDistance == lastFilterDistance && currentHeight == lastFilterHeight
        && currentRear == lastFilterRear && usingHeadphones == lastFilterHeadphones)
        return;

    lastFilterDistance = currentDistance;
    lastFilterHeight = currentHeight;
    lastFilterRear = currentRear;
    lastFilterHeadphones = usingHeadphones;
    const auto maximumFilterFrequency = static_cast<float> (currentSampleRate * 0.45);
    // The measured rear HRIR already contains the head/pinna coloration.
    const auto rearAmount = usingHeadphones ? 0.0f : currentRear;
    const auto directCutoff = juce::jlimit (1800.0f, 20000.0f,
                                            juce::jmap (currentDistance, 19000.0f, 4200.0f)
                                            * (1.0f - 0.30f * rearAmount));
    const auto reflectionCutoff = juce::jlimit (1200.0f, 16000.0f,
                                                juce::jmap (currentDistance, 12000.0f, 2300.0f)
                                                * (1.0f - 0.18f * rearAmount));
    const auto shelfGainDb = currentHeight * 2.0f;
    const auto shelfGain = juce::Decibels::decibelsToGain (shelfGainDb);
    const auto shelfCoefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighShelf (
        currentSampleRate, juce::jmin (4000.0f, maximumFilterFrequency), 0.7071f, shelfGain);

    for (int channel = 0; channel < maximumOutputChannels; ++channel)
    {
        directLowPasses[static_cast<size_t> (channel)].setCutoffFrequency (juce::jmin (directCutoff, maximumFilterFrequency));
        reflectionLowPasses[static_cast<size_t> (channel)].setCutoffFrequency (juce::jmin (reflectionCutoff, maximumFilterFrequency));
        *heightShelves[static_cast<size_t> (channel)].coefficients = shelfCoefficients;
    }
}

void SpatialRenderer::setInputLayout (const juce::AudioChannelSet& inputLayout)
{
    std::array<float, maximumOutputChannels> azimuths {};
    std::array<bool, maximumOutputChannels> lfeChannels {};
    const auto channels = juce::jmin (maximumOutputChannels, inputLayout.size());
    for (int channel = 0; channel < channels; ++channel)
    {
        const auto type = inputLayout.getTypeOfChannel (channel);
        azimuths[static_cast<size_t> (channel)] = getChannelAzimuth (type);
        lfeChannels[static_cast<size_t> (channel)] = type == juce::AudioChannelSet::LFE;
    }
    multichannelBinauralInput = channels > 2 && surroundChannelCount == 2;
    hrtfRenderer.setSourceLayout (azimuths.data(), lfeChannels.data(), channels);
}

float SpatialRenderer::getChannelAzimuth (juce::AudioChannelSet::ChannelType type)
{
    using ChannelType = juce::AudioChannelSet::ChannelType;

    switch (type)
    {
        case ChannelType::left:              return -30.0f;
        case ChannelType::right:             return 30.0f;
        case ChannelType::centre:            return 0.0f;
        case ChannelType::leftCentre:        return -60.0f;
        case ChannelType::rightCentre:       return 60.0f;
        case ChannelType::leftSurround:      return -110.0f;
        case ChannelType::rightSurround:     return 110.0f;
        case ChannelType::leftSurroundSide:  return -90.0f;
        case ChannelType::rightSurroundSide: return 90.0f;
        case ChannelType::leftSurroundRear:  return -150.0f;
        case ChannelType::rightSurroundRear: return 150.0f;
        case ChannelType::centreSurround:    return 180.0f;
        case ChannelType::wideLeft:          return -60.0f;
        case ChannelType::wideRight:         return 60.0f;
        default:                             return 0.0f;
    }
}

float SpatialRenderer::wrapAzimuth (float azimuth)
{
    while (azimuth <= -180.0f)
        azimuth += 360.0f;

    while (azimuth > 180.0f)
        azimuth -= 360.0f;

    return azimuth;
}

void SpatialRenderer::setSurroundLayout (const juce::AudioChannelSet& outputLayout)
{
    surroundChannelCount = juce::jlimit (0, maximumOutputChannels, outputLayout.size());
    surroundSpeakerCount = 0;
    isLfeChannel.fill (false);
    channelAzimuths.fill (0.0f);

    for (int channel = 0; channel < surroundChannelCount; ++channel)
    {
        const auto type = outputLayout.getTypeOfChannel (channel);
        isLfeChannel[static_cast<size_t> (channel)] = type == juce::AudioChannelSet::LFE;
        channelAzimuths[static_cast<size_t> (channel)] = getChannelAzimuth (type);

        if (! isLfeChannel[static_cast<size_t> (channel)])
            sortedSpeakerIndices[static_cast<size_t> (surroundSpeakerCount++)] = channel;
    }

    std::sort (sortedSpeakerIndices.begin(), sortedSpeakerIndices.begin() + surroundSpeakerCount,
               [this] (int first, int second)
               {
                   return channelAzimuths[static_cast<size_t> (first)] < channelAzimuths[static_cast<size_t> (second)];
               });
}

void SpatialRenderer::calculateSurroundGains (float azimuth, float focus,
                                               std::array<float, maximumOutputChannels>& gains) const
{
    gains.fill (0.0f);

    if (surroundSpeakerCount == 0)
        return;

    const auto target = wrapAzimuth (azimuth);

    for (int position = 0; position < surroundSpeakerCount; ++position)
    {
        const auto firstChannel = sortedSpeakerIndices[static_cast<size_t> (position)];
        const auto secondChannel = sortedSpeakerIndices[static_cast<size_t> ((position + 1) % surroundSpeakerCount)];
        const auto firstAzimuth = channelAzimuths[static_cast<size_t> (firstChannel)];
        auto secondAzimuth = channelAzimuths[static_cast<size_t> (secondChannel)];
        auto adjustedTarget = target;

        if (position == surroundSpeakerCount - 1)
        {
            secondAzimuth += 360.0f;

            if (adjustedTarget < firstAzimuth)
                adjustedTarget += 360.0f;
        }

        if (adjustedTarget >= firstAzimuth && adjustedTarget <= secondAzimuth)
        {
            const auto amount = juce::jlimit (0.0f, 1.0f,
                                               (adjustedTarget - firstAzimuth) / (secondAzimuth - firstAzimuth));
            gains[static_cast<size_t> (firstChannel)] = std::cos (juce::MathConstants<float>::halfPi * amount);
            gains[static_cast<size_t> (secondChannel)] = std::sin (juce::MathConstants<float>::halfPi * amount);
            break;
        }
    }

    if (focus >= 1.0f)
        return;

    // Near the listener, blend at constant power from the speaker pair to
    // every speaker, so the object surrounds the listener at the origin.
    const auto directional = juce::jmax (0.0f, focus);
    const auto shared = (1.0f - directional) / static_cast<float> (surroundSpeakerCount);

    for (int position = 0; position < surroundSpeakerCount; ++position)
    {
        auto& gain = gains[static_cast<size_t> (sortedSpeakerIndices[static_cast<size_t> (position)])];
        gain = std::sqrt (directional * gain * gain + shared);
    }
}

#if JUCE_DEBUG
bool SpatialRenderer::runRegressionChecks()
{
    const auto hasIdentityMapping = [] (const juce::AudioChannelSet& layout)
    {
        SpatialRenderer renderer;
        renderer.setSurroundLayout (layout);
        std::array<float, maximumOutputChannels> gains {};

        for (int channel = 0; channel < layout.size(); ++channel)
        {
            if (renderer.isLfeChannel[static_cast<size_t> (channel)])
                continue;

            renderer.calculateSurroundGains (renderer.channelAzimuths[static_cast<size_t> (channel)], 1.0f, gains);

            for (int output = 0; output < layout.size(); ++output)
            {
                const auto expected = output == channel ? 1.0f : 0.0f;

                if (std::abs (gains[static_cast<size_t> (output)] - expected) > 0.0001f)
                    return false;
            }
        }

        return true;
    };

    const auto hasEqualPowerPan = []
    {
        for (int step = 0; step <= 20; ++step)
        {
            const auto testPan = -1.0f + 0.1f * static_cast<float> (step);
            const auto leftGain = std::sqrt (0.5f * (1.0f - testPan));
            const auto rightGain = std::sqrt (0.5f * (1.0f + testPan));

            if (std::abs (leftGain * leftGain + rightGain * rightGain - 1.0f) > 0.0001f)
                return false;
        }

        return true;
    };

    const auto hasCorrectPlaneAngles = std::abs (getObjectAzimuth (0.0f, 1.0f)) < 0.0001f
                                    && std::abs (getObjectAzimuth (1.0f, 1.0f) - 45.0f) < 0.0001f
                                    && std::abs (getObjectAzimuth (1.0f, 0.0f) - 90.0f) < 0.0001f
                                    && std::abs (std::abs (getObjectAzimuth (0.0f, -1.0f)) - 180.0f) < 0.0001f;

    const auto beside = getObjectPlacement (0.3f, 0.0f, 0.0f);
    const auto behind = getObjectPlacement (0.0f, -1.0f, 0.0f);
    const auto origin = getObjectPlacement (0.0f, 0.0f, 0.0f);
    const auto overhead = getObjectPlacement (0.0f, 0.0f, 1.0f);
    const auto hasListenerRelativePlacement = std::abs (getObjectPlacement (0.5f, 1.0f, 0.0f).lateral - 0.5f) < 0.0001f
                                           && std::abs (beside.lateral - 1.0f) < 0.0001f
                                           && std::abs (behind.lateral) < 0.0001f
                                           && std::abs (behind.rear - 1.0f) < 0.0001f
                                           && origin.focus == 0.0f && origin.lateral == 0.0f && origin.binauralFocus == 0.0f
                                           && std::abs (overhead.elevation - 90.0f) < 0.0001f
                                           && overhead.focus == 0.0f && overhead.binauralFocus == 1.0f;

    return hasEqualPowerPan()
        && hasCorrectPlaneAngles
        && hasListenerRelativePlacement
        && hasIdentityMapping (juce::AudioChannelSet::create5point1())
        && hasIdentityMapping (juce::AudioChannelSet::create7point1());
}
#endif

void SpatialRenderer::process (float inputLeft, float inputRight, bool hasStereoInput,
                               float& outputLeft, float& outputRight)
{
    const float input[] { inputLeft, inputRight };
    processStereoOutput (input, hasStereoInput ? 2 : 1, outputLeft, outputRight);
}

void SpatialRenderer::processStereoOutput (const float* input, int numInputChannels,
                                           float& outputLeft, float& outputRight)
{
    const auto currentPan = pan.getNextValue();
    const auto currentFrontBack = frontBack.getNextValue();
    const auto currentHeight = height.getNextValue();
    const auto currentDistance = distance.getNextValue();
    const auto currentRoom = room.getNextValue();
    const auto currentOutputGain = outputGain.getNextValue();
    const auto placement = getObjectPlacement (currentPan, currentFrontBack, currentHeight);

    // Follow the same smoothed coordinates as gains and geometry, rather than
    // applying the next block's target filter response in one audible step.
    if (samplesUntilFilterUpdate-- <= 0)
    {
        updateFilterSettings (currentDistance, currentHeight, placement.rear);
        samplesUntilFilterUpdate = filterUpdateInterval - 1;
    }

    float pannedLeft = 0.0f;
    float pannedRight = 0.0f;
    const auto usingHeadphones = (renderMode == 1 || numInputChannels > 2) && hrtfRenderer.isReady();

    if (usingHeadphones || numInputChannels > 2)
    {
        hrtfRenderer.process (input, numInputChannels, placement.azimuth,
                              placement.elevation, pannedLeft, pannedRight, placement.binauralFocus);
    }
    else
    {
        const auto leftGain = std::sqrt (0.5f * (1.0f - placement.lateral));
        const auto rightGain = std::sqrt (0.5f * (1.0f + placement.lateral));

        if (numInputChannels > 1)
        {
            const auto mid = (input[0] + input[1]) * inverseSquareRootOfTwo;
            const auto side = (input[0] - input[1]) * inverseSquareRootOfTwo;
            const auto sideGain = juce::jmin (leftGain, rightGain);

            pannedLeft = leftGain * mid + sideGain * side;
            pannedRight = rightGain * mid - sideGain * side;
        }
        else
        {
            pannedLeft = leftGain * input[0];
            pannedRight = rightGain * input[0];
        }
    }

    const auto distanceDelay = currentDistance * static_cast<float> (maximumDistanceDelaySamples);
    auto directLeft = pannedLeft;
    auto directRight = pannedRight;

    const auto ceilingDelay = distanceDelay + ceilingReflectionSeconds * static_cast<float> (currentSampleRate);
    const auto floorDelay = distanceDelay + floorReflectionSeconds * static_cast<float> (currentSampleRate);
    const auto ceilingLeft = ceilingReflectionDelays[0].process (pannedLeft, ceilingDelay);
    const auto ceilingRight = ceilingReflectionDelays[1].process (pannedRight, ceilingDelay);
    const auto floorLeft = floorReflectionDelays[0].process (pannedLeft, floorDelay);
    const auto floorRight = floorReflectionDelays[1].process (pannedRight, floorDelay);

    const auto rearAmount = usingHeadphones ? 0.0f : placement.rear;

    // Keep the filter warm at distance zero, then crossfade its phase response
    // when enabling it. A cold filter caused large automation clicks at 96/192 kHz.
    const auto filteredLeft = directLowPasses[0].processSample (0, directLeft);
    const auto filteredRight = directLowPasses[1].processSample (0, directRight);
    const auto filterMix = directFilterMix.getNextValue();
    directLeft += filterMix * (filteredLeft - directLeft);
    directRight += filterMix * (filteredRight - directRight);

    const auto rearNotchedLeft = rearNotches[0].processSample (directLeft);
    const auto rearNotchedRight = rearNotches[1].processSample (directRight);
    const auto rearNotchMix = maximumRearNotchMix * rearAmount;
    directLeft += rearNotchMix * (rearNotchedLeft - directLeft);
    directRight += rearNotchMix * (rearNotchedRight - directRight);

    const auto heightFilteredLeft = heightShelves[0].processSample (directLeft);
    const auto heightFilteredRight = heightShelves[1].processSample (directRight);
    if (! usingHeadphones)
    {
        directLeft = heightFilteredLeft;
        directRight = heightFilteredRight;
    }

    const auto ceilingMixLeft = ceilingLeft;
    const auto ceilingMixRight = ceilingRight;
    const auto floorMixLeft = floorLeft;
    const auto floorMixRight = floorRight;
    const auto ceilingAmount = 0.5f * (currentHeight + 1.0f);
    const auto reflectionDrive = juce::jlimit (0.0f, 1.0f,
                                                currentDistance + (1.0f - currentDistance) * 0.18f * rearAmount);
    const auto earlyGain = reflectionDrive * (0.012f + 0.060f * currentRoom)
                         * (1.0f + 0.35f * rearAmount + 0.18f * std::abs (currentHeight));

    const auto reflectionLeft = reflectionLowPasses[0].processSample (0,
        ceilingAmount * ceilingMixLeft + (1.0f - ceilingAmount) * floorMixLeft);
    const auto reflectionRight = reflectionLowPasses[1].processSample (0,
        ceilingAmount * ceilingMixRight + (1.0f - ceilingAmount) * floorMixRight);
    const auto directAttenuationDb = getDistanceAttenuationDb (currentDistance, distanceModel);
    const auto directGain = juce::Decibels::decibelsToGain (directAttenuationDb);

    outputLeft = currentOutputGain * (directGain * directLeft + earlyGain * reflectionLeft);
    outputRight = currentOutputGain * (directGain * directRight + earlyGain * reflectionRight);
}

void SpatialRenderer::processSurround (const float* input, float* output, int numInputChannels, int numOutputChannels)
{
    const auto channels = juce::jlimit (0, surroundChannelCount, numOutputChannels);
    const auto inputChannels = juce::jlimit (0, channels, numInputChannels);

    if (channels == 0 || inputChannels == 0)
        return;

    const auto currentPan = pan.getNextValue();
    const auto currentFrontBack = frontBack.getNextValue();
    const auto currentHeight = height.getNextValue();
    const auto currentDistance = distance.getNextValue();
    const auto currentRoom = room.getNextValue();
    const auto currentOutputGain = outputGain.getNextValue();
    const auto filterMix = directFilterMix.getNextValue();
    const auto placement = getObjectPlacement (currentPan, currentFrontBack, currentHeight);
    if (samplesUntilFilterUpdate-- <= 0)
    {
        updateFilterSettings (currentDistance, currentHeight, placement.rear);
        samplesUntilFilterUpdate = filterUpdateInterval - 1;
    }
    const auto rearAmount = placement.rear;
    const auto distanceDelay = currentDistance * static_cast<float> (maximumDistanceDelaySamples);
    const auto ceilingDelay = distanceDelay + ceilingReflectionSeconds * static_cast<float> (currentSampleRate);
    const auto floorDelay = distanceDelay + floorReflectionSeconds * static_cast<float> (currentSampleRate);
    const auto directAttenuationDb = getDistanceAttenuationDb (currentDistance, distanceModel);
    const auto directGain = juce::Decibels::decibelsToGain (directAttenuationDb);
    const auto reflectionDrive = juce::jlimit (0.0f, 1.0f,
                                                currentDistance + (1.0f - currentDistance) * 0.18f * rearAmount);
    const auto earlyGain = reflectionDrive * (0.012f + 0.060f * currentRoom)
                         * (1.0f + 0.35f * rearAmount + 0.18f * std::abs (currentHeight));
    const auto ceilingAmount = 0.5f * (currentHeight + 1.0f);

    std::array<float, maximumOutputChannels> directInput {};
    std::array<float, maximumOutputChannels> directOutput {};
    std::array<float, maximumOutputChannels> ceilingOutput {};
    std::array<float, maximumOutputChannels> floorOutput {};
    std::array<float, maximumOutputChannels> gains {};

    for (int channel = 0; channel < channels; ++channel)
        output[channel] = 0.0f;

    if (inputChannels <= 2)
    {
        for (int inputChannel = 0; inputChannel < inputChannels; ++inputChannel)
        {
            const auto stereoOffset = inputChannels == 2
                                    ? (inputChannel == 0 ? -stereoSourceHalfWidthDegrees : stereoSourceHalfWidthDegrees)
                                    : 0.0f;
            calculateSurroundGains (wrapAzimuth (placement.azimuth + stereoOffset), placement.focus, gains);

            for (int outputChannel = 0; outputChannel < channels; ++outputChannel)
                directInput[static_cast<size_t> (outputChannel)] += input[inputChannel]
                                                                    * gains[static_cast<size_t> (outputChannel)];
        }
    }
    else
    {
        // A surround bed is rotated as a whole and keeps its own image.
        for (int inputChannel = 0; inputChannel < inputChannels; ++inputChannel)
        {
            if (isLfeChannel[static_cast<size_t> (inputChannel)])
            {
                output[inputChannel] = input[inputChannel];
                continue;
            }

            const auto sourceAzimuth = wrapAzimuth (channelAzimuths[static_cast<size_t> (inputChannel)] + placement.azimuth);
            calculateSurroundGains (sourceAzimuth, 1.0f, gains);

            for (int outputChannel = 0; outputChannel < channels; ++outputChannel)
                directInput[static_cast<size_t> (outputChannel)] += input[inputChannel]
                                                                    * gains[static_cast<size_t> (outputChannel)];
        }
    }

    for (int channel = 0; channel < channels; ++channel)
    {
        if (isLfeChannel[static_cast<size_t> (channel)])
            continue;

        auto direct = directInput[static_cast<size_t> (channel)];
        ceilingOutput[static_cast<size_t> (channel)] = ceilingReflectionDelays[static_cast<size_t> (channel)].process (
            directInput[static_cast<size_t> (channel)], ceilingDelay);
        floorOutput[static_cast<size_t> (channel)] = floorReflectionDelays[static_cast<size_t> (channel)].process (
            directInput[static_cast<size_t> (channel)], floorDelay);

        const auto filtered = directLowPasses[static_cast<size_t> (channel)].processSample (0, direct);
        direct += filterMix * (filtered - direct);

        direct = heightShelves[static_cast<size_t> (channel)].processSample (direct);

        directOutput[static_cast<size_t> (channel)] = direct;
    }

    for (int channel = 0; channel < channels; ++channel)
    {
        if (isLfeChannel[static_cast<size_t> (channel)])
            continue;

        auto position = 0;

        while (position < surroundSpeakerCount
               && sortedSpeakerIndices[static_cast<size_t> (position)] != channel)
            ++position;

        const auto previous = sortedSpeakerIndices[static_cast<size_t> ((position + surroundSpeakerCount - 1) % surroundSpeakerCount)];
        const auto next = sortedSpeakerIndices[static_cast<size_t> ((position + 1) % surroundSpeakerCount)];
        const auto ceilingMix = 0.65f * ceilingOutput[static_cast<size_t> (channel)]
                              + 0.175f * (ceilingOutput[static_cast<size_t> (previous)]
                                        + ceilingOutput[static_cast<size_t> (next)]);
        const auto floorMix = 0.85f * floorOutput[static_cast<size_t> (channel)]
                            + 0.075f * (floorOutput[static_cast<size_t> (previous)]
                                      + floorOutput[static_cast<size_t> (next)]);
        const auto reflection = reflectionLowPasses[static_cast<size_t> (channel)].processSample (0,
            ceilingAmount * ceilingMix + (1.0f - ceilingAmount) * floorMix);

        output[channel] = directGain * directOutput[static_cast<size_t> (channel)] + earlyGain * reflection;
    }

    for (int channel = 0; channel < channels; ++channel)
        output[channel] *= currentOutputGain;
}
