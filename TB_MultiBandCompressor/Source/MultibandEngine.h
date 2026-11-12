#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

namespace tb::multiband
{
class MultibandEngine final
{
public:
    static constexpr int bandCount = 4;
    static_assert (std::atomic<float>::is_always_lock_free,
                   "Audio-thread meters require lock-free float atomics.");

    enum class DynamicsMode
    {
        compress,
        expand
    };

    enum class DetectorSource
    {
        internal,
        external
    };

    enum class DetectorMode
    {
        band,
        free
    };

    enum class PhaseMode
    {
        dynamic,
        linear
    };

    enum class OversamplingMode
    {
        off,
        x2,
        x4,
        x8
    };

    enum class ChannelMode
    {
        stereo,
        mid,
        side
    };

    struct BandSettings
    {
        bool enabled = true;
        float frequencyHz = 120.0f;
        float bandwidthOctaves = 1.5f;
        float thresholdDb = -18.0f;
        float rangeDb = -3.0f;
        float ratio = 2.0f;
        float kneeDb = 6.0f;
        float attackMs = 15.0f;
        float releaseMs = 180.0f;
        float outputDb = 0.0f;
        DynamicsMode dynamicsMode = DynamicsMode::compress;
        DetectorSource detectorSource = DetectorSource::internal;
        DetectorMode detectorMode = DetectorMode::band;
        float detectorFrequencyHz = 120.0f;
        float detectorBandwidthOctaves = 1.5f;
        float stereoLink = 1.0f; // Normalised 0..1.
        ChannelMode channelMode = ChannelMode::stereo;
        // If state/automation enables more than one band, the lowest index wins.
        bool audition = false;
    };

    struct Settings
    {
        Settings() noexcept;

        std::array<BandSettings, bandCount> bands {};
        float mix = 1.0f; // Normalised 0..1.
        float outputDb = 0.0f;
        PhaseMode phaseMode = PhaseMode::dynamic;
        OversamplingMode oversamplingMode = OversamplingMode::off;
        float lookaheadMs = 0.0f;
        bool bypass = false;
        // Transient monitor override. Not part of host/state/preset data.
        int deltaListenBand = -1;
    };

    MultibandEngine() noexcept;
    ~MultibandEngine();

    using ExternalSidechainBuses =
        std::array<const juce::AudioBuffer<float>*, bandCount>;

    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset() noexcept;

    /** Processes mainBuffer in place. A null externalBuffer means that the host
        sidechain bus is unavailable, so every EXT band deterministically falls
        back to the internal input. Four optional mono-default external buses
        are a fixed key matrix (Sidechain 1 -> Band 1 through Sidechain 4 ->
        Band 4); stereo layouts are downmixed to one detector key.
        The legacy single-buffer overload remains available for shared
        mono/stereo/quad callers. A valid but silent external input remains
        silent and is never treated as disconnected. */
    void process (juce::AudioBuffer<float>& mainBuffer,
                  const juce::AudioBuffer<float>* externalBuffer,
                  const Settings& settings) noexcept;

    /** Processes a set of optional mono-default sidechain buses. Bus N is the
        fixed external key for Band N; stereo bus data is downmixed and a null
        or disabled bus falls back to the internal detector for that band. */
    void process (juce::AudioBuffer<float>& mainBuffer,
                  const ExternalSidechainBuses& externalBuses,
                  const Settings& settings) noexcept;

    /** Returns the signed dynamic gain for the band. Compression/reduction is
        negative and upward processing is positive. Band Output is excluded. */
    float getGainReductionDb (int band) const noexcept;
    float getDetectorLevelDb (int band) const noexcept;

    /** Returns the complete base-rate latency for the supplied quality settings.
        This is valid after prepare(). It includes lookahead, the Linear FIR path,
        and the selected JUCE oversampling round trip. */
    int getLatencySamples (const Settings& settings) const noexcept;

private:
    static constexpr int maximumChannels = 2;

    struct NormalisedBandPass
    {
        void prepare (double sampleRate, int maximumBlockSize);
        void reset() noexcept;
        void setParameters (float frequencyHz, float bandwidthOctaves,
                            double sampleRate) noexcept;
        float processSample (int channel, float input) noexcept;
        void snapToZero() noexcept;

        void applyParameters (float frequencyHz,
                              float bandwidthOctaves) noexcept;

        juce::dsp::StateVariableTPTFilter<float> filter;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>
            frequencySmoother;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
            bandwidthSmoother;
        float normalisation = 1.0f;
        float appliedFrequencyHz = -1.0f;
        float appliedBandwidthOctaves = -1.0f;
        int coefficientSamplesUntilUpdate = 0;
        bool smoothingInitialised = false;
    };

    struct BandState
    {
        NormalisedBandPass processingFilter;
        NormalisedBandPass detectorFilter;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
            detectorSourceMix;
        std::array<float, maximumChannels> envelope {};
        bool wasEnabled = false;
        ChannelMode channelMode = ChannelMode::stereo;
        bool channelModeInitialised = false;
    };

    struct ProcessingPath;

    static float bandwidthToQ (float bandwidthOctaves) noexcept;
    static float computeActivation (float distanceDb, float kneeDb) noexcept;
    static float computeDynamicGainDb (float detectorLevelDb,
                                       const BandSettings&) noexcept;
    static BandSettings sanitise (const BandSettings&, double sampleRate) noexcept;

    static int oversamplingIndex (OversamplingMode) noexcept;
    static int lookaheadSamples (const Settings&, double sampleRate) noexcept;

    std::array<std::unique_ptr<ProcessingPath>, 4> processingPaths;
    std::array<std::atomic<float>, bandCount> gainMetersDb {};
    std::array<std::atomic<float>, bandCount> detectorMetersDb {};
    std::array<float, maximumChannels> lastOutputSamples {};
    std::array<float, maximumChannels> transitionStartSamples {};
    double currentSampleRate = 48000.0;
    int preparedMaximumBlockSize = 1;
    int preparedChannels = maximumChannels;
    PhaseMode activePhaseMode = PhaseMode::dynamic;
    OversamplingMode activeOversamplingMode = OversamplingMode::off;
    int activeLookaheadSamples = 0;
    int transitionSamplesRemaining = 0;
    int transitionTotalSamples = 0;
    int transitionGuardSamplesRemaining = 0;
    bool qualityConfigurationInitialised = false;
    bool prepared = false;
    const ExternalSidechainBuses* activeExternalBuses = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultibandEngine)
};
} // namespace tb::multiband

using MultibandEngine = tb::multiband::MultibandEngine;
