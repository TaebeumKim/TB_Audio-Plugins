/*
  ==============================================================================

    HrtfRenderer.h
    Compact MIT KEMAR binaural renderer for headphone output.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

class HrtfRenderer
{
public:
    void prepare (double sampleRate);
    void reset();

    bool isReady() const noexcept { return ready; }

    void setSourceLayout (const float* azimuths, const bool* lfeChannels, int sourceCount);

    // Elevation spans the measured -40 to +90 degrees. focus is 1 for a fully
    // directional source and falls to 0 as a mono or stereo source reaches the
    // listener position, where it is heard in-head.
    void process (const float* input, int numInputChannels, float azimuthDegrees,
                  float elevationDegrees, float& outputLeft, float& outputRight, float focus = 1.0f);

private:
    // Every elevation row of the MIT compact set: -40 to +90 degrees in 10 degree steps.
    static constexpr int numberOfElevations = 14;
    static constexpr int maximumSources = 8;
    // Retain the complete 128-sample HRIR when resampling up to 192 kHz.
    static constexpr int maximumTaps = 1024;
    // Tools/GenerateKemarResources.ps1 validates every bundled HRIR at this rate.
    static constexpr double measurementSampleRate = 44100.0;

    struct Filter
    {
        std::array<float, maximumTaps> left {};
        std::array<float, maximumTaps> right {};
        int tapCount = 1;
        float elevationDegrees = 0.0f;
        float azimuthDegrees = 0.0f;
    };

    // One measured direction at the host rate: count left-ear samples at
    // offset in measurementTaps, followed by count right-ear samples. The
    // arrival sample of each ear lets neighbouring directions be lined up
    // before they are blended.
    struct Measurement
    {
        int offset = 0;
        int count = 0;
        float leftOnset = 0.0f;
        float rightOnset = 0.0f;
    };

    struct SourceHistory
    {
        // Mirrored ring storage gives the FIR a contiguous newest-to-oldest span.
        std::array<float, maximumTaps * 2> samples {};
        int writeIndex = 0;

        void reset();
        void push (float sample);
    };

    void loadImpulseResponses();
    Measurement loadImpulseResponse (juce::ZipFile& archive, const juce::String& fileName,
                                     const std::vector<float>& resamplingKernel);
    void alignMeasurements();
    Filter selectFilter (float azimuthDegrees, float elevationDegrees) const;
    void updateFilters (float azimuthDegrees, float elevationDegrees, int sourceCount);
    float getSourceAzimuth (int source, int sourceCount, float azimuthDegrees) const;
    void renderFilter (const SourceHistory& history, const Filter& filter, float& outputLeft, float& outputRight) const;
    static Filter blendFilters (const Filter& first, const Filter& second, float amount) noexcept;

    std::array<std::vector<Measurement>, numberOfElevations> impulseResponses;
    std::vector<float> measurementTaps;
    std::array<Filter, maximumSources> activeFilters;
    std::array<Filter, maximumSources> pendingFilters;
    std::array<SourceHistory, maximumSources> histories;
    std::array<float, maximumSources> sourceAzimuths {};
    std::array<bool, maximumSources> sourceIsLfe {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> filterCrossfade;

    double currentSampleRate = 44100.0;
    bool ready = false;
    bool hasInitialFilters = false;
    int activeSourceCount = 0;
    int samplesUntilFilterUpdate = 0;
    // Arrival of the frontal response; aligns the in-head signal with it.
    int inHeadDelaySamples = 0;
};
