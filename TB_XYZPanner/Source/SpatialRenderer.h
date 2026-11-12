/*
  ==============================================================================

    SpatialRenderer.h
    Owns the real-time stereo compatibility spatial DSP.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "HrtfRenderer.h"

#include <array>
#include <vector>

struct SpatialRendererParameters
{
    float pan = 0.0f;
    float frontBack = 1.0f;
    float height = 0.0f;
    float distance = 0.0f;
    float room = 0.35f;
    int distanceModel = 0;
    int renderMode = 0;
    float outputGain = 1.0f;
};

class SpatialRenderer
{
public:
    static constexpr double maximumTailLengthSeconds = 0.100;
    static constexpr int maximumOutputChannels = 8;

    void prepare (double sampleRate, int samplesPerBlock, int outputChannels);
    void reset();
    void setParameters (SpatialRendererParameters newParameters, bool forceImmediately = false);
    void setSurroundLayout (const juce::AudioChannelSet& outputLayout);
    void setInputLayout (const juce::AudioChannelSet& inputLayout);

    void process (float inputLeft, float inputRight, bool hasStereoInput,
                  float& outputLeft, float& outputRight);
    void processStereoOutput (const float* input, int numInputChannels, float& outputLeft, float& outputRight);
    void processSurround (const float* input, float* output, int numInputChannels, int numOutputChannels);

private:
    struct FractionalDelay
    {
        void prepare (int maximumDelaySamples);
        void reset();
        float process (float input, float delaySamples);

    private:
        std::vector<float> buffer;
        int writeIndex = 0;
    };

    void updateFilterSettings (float distance, float height, float rearAmount);
    // focus below 1 spreads the object over every speaker as it nears the listener.
    void calculateSurroundGains (float azimuth, float focus, std::array<float, maximumOutputChannels>& gains) const;
    static float getChannelAzimuth (juce::AudioChannelSet::ChannelType type);
    static float wrapAzimuth (float azimuth);

   #if JUCE_DEBUG
    static bool runRegressionChecks();
   #endif

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> pan;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> frontBack;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> height;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> distance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> room;
    int distanceModel = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> directFilterMix;

    std::array<FractionalDelay, maximumOutputChannels> ceilingReflectionDelays;
    std::array<FractionalDelay, maximumOutputChannels> floorReflectionDelays;

    std::array<juce::dsp::StateVariableTPTFilter<float>, maximumOutputChannels> directLowPasses;
    std::array<juce::dsp::StateVariableTPTFilter<float>, maximumOutputChannels> reflectionLowPasses;
    std::array<juce::dsp::IIR::Filter<float>, maximumOutputChannels> heightShelves;
    std::array<juce::dsp::IIR::Filter<float>, maximumOutputChannels> rearNotches;
    HrtfRenderer hrtfRenderer;

    std::array<float, maximumOutputChannels> channelAzimuths {};
    std::array<bool, maximumOutputChannels> isLfeChannel {};
    std::array<int, maximumOutputChannels> sortedSpeakerIndices {};
    int surroundChannelCount = 0;
    int surroundSpeakerCount = 0;

    double currentSampleRate = 44100.0;
    int maximumDistanceDelaySamples = 1;
    int samplesUntilFilterUpdate = 0;
    float lastFilterDistance = -1.0f;
    float lastFilterHeight = 0.0f;
    float lastFilterRear = 0.0f;
    bool lastFilterHeadphones = false;
    int renderMode = 0;
    bool multichannelBinauralInput = false;
};
