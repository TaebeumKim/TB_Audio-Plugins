#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class GranularShimmerEngine
{
public:
    struct Parameters
    {
        float timeMs = 240.0f;
        float sizeMs = 160.0f;
        float density = 9.0f;
        float scatterPercent = 32.0f;
        float levelVariationDb = 0.0f;
        float positionRandomPercent = 32.0f;
        float pitchSpraySemitones = 0.112f;
        float pitchSemitones = 12.0f;
        float reversePercent = 18.0f;
        float feedbackPercent = 42.0f;
        float toneHz = 9000.0f;
        float widthPercent = 135.0f;
        float mixPercent = 48.0f;
        bool frozen = false;
        float sparklePercent = 0.0f;
        float shatterPercent = 0.0f;
    };

    void prepare (double newSampleRate, int maximumBlockSize, int numberOfChannels);
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer,
                  const Parameters& parameters,
                  bool bypassed) noexcept;

    static constexpr int maximumGrains = 64;
    static constexpr int maximumShards = 32;
    int getActiveGrainCount() const noexcept;
    int getActiveShardCount() const noexcept;

    // Monotonic count of sparkle shards spawned so far. Safe to poll from the
    // message thread; the editor turns the delta into visible fragments.
    std::uint32_t getShardSpawnCount() const noexcept
    {
        return shardSpawnCounter.load (std::memory_order_relaxed);
    }

private:
    static constexpr int historySeconds = 8;
    static constexpr int windowTableSize = 2048;
    static constexpr int pitchBinCount = 49;
    static constexpr int fractionalPhaseCount = 256;
    static constexpr int sincTapCount = 8;
    static constexpr int earlyDiffusionStageCount = 4;
    static constexpr int diffuseLineCount = 8;

    struct Grain
    {
        bool active = false;
        int age = 0;
        int length = 1;
        double readPosition = 0.0;
        double increment = 1.0;
        int pitchBin = 0;
        float levelGain = 1.0f;
        float leftGain = 1.0f;
        float rightGain = 1.0f;
    };

    struct Shard
    {
        bool active = false;
        int age = 0;
        int length = 1;
        double readPosition = 0.0;
        double increment = 2.0;
        int pitchBin = 24;
        int sourceChannel = 0;
        float attackStep = 1.0f;
        float envelope = 1.0f;
        float decay = 1.0f;
        float level = 1.0f;
        float leftGain = 1.0f;
        float rightGain = 1.0f;
    };

    struct SparkleSettings
    {
        float amount = 0.0f;
        float shatter = 0.0f;
        float chargeDecay = 0.0f;
        float drizzlePerSample = 0.0f;
        float burstPerSample = 0.0f;
    };

    void primeSmoothers (const Parameters&, bool bypassed) noexcept;
    void spawnGrain (const Parameters&) noexcept;
    void spawnShard (const Parameters&, float shatter, bool fromBurst,
                     bool frozen) noexcept;
    std::array<float, 2> processSparkle (const std::array<float, 2>& dry,
                                         const Parameters&,
                                         const SparkleSettings&,
                                         float feedback,
                                         float dampingAmount,
                                         bool frozen,
                                         int numberOfChannels) noexcept;
    float readSparkleEcho (int channel, float delaySamples) const noexcept;
    void clearSparkleState() noexcept;
    float shardRandomUnit() noexcept;
    float readHistory (int channel, double position, int pitchBin) const noexcept;
    std::array<float, 2> processEarlyDiffusion (const std::array<float, 2>& input,
                                                int numberOfChannels) noexcept;
    std::array<float, 2> processDiffuseTail (const std::array<float, 2>& input,
                                             float baseDelayMs,
                                             float motionDepthSamples,
                                             float dampingAmount,
                                             int numberOfChannels) noexcept;
    float readDiffuseLine (int line, float delaySamples) noexcept;
    float windowAt (int age, int length) const noexcept;
    float randomUnit() noexcept;
    float randomBipolar() noexcept;
    static std::uint32_t advanceRandom (std::uint32_t& state) noexcept;
    static float randomBipolar (std::uint32_t& state) noexcept;
    static float sanitise (float value, float fallback, float minimum, float maximum) noexcept;
    static float softBound (float value) noexcept;
    static float diffuseSoftBound (float value) noexcept;
    double wrapPosition (double position) const noexcept;
    int wrapIndex (int index) const noexcept;

    std::array<std::vector<float>, 2> history;
    std::array<Grain, maximumGrains> grains;
    std::array<float, windowTableSize> windowTable {};
    std::vector<float> sincTable;

    std::array<std::array<std::vector<float>, earlyDiffusionStageCount>, 2>
        earlyDiffusionBuffers;
    std::array<std::array<int, earlyDiffusionStageCount>, 2>
        earlyDiffusionWriteIndices {};
    std::array<std::vector<float>, diffuseLineCount> diffuseBuffers;
    std::array<int, diffuseLineCount> diffuseWriteIndices {};
    std::array<float, diffuseLineCount> diffuseInterpolationInput {};
    std::array<float, diffuseLineCount> diffuseInterpolationOutput {};
    std::array<float, diffuseLineCount> diffuseDampingState {};
    std::array<float, diffuseLineCount> diffuseWander {};
    std::array<float, diffuseLineCount> diffuseWanderTargets {};
    std::array<int, diffuseLineCount> diffuseWanderCountdowns {};
    std::array<std::uint32_t, diffuseLineCount> diffuseRandomStates {};

    std::array<Shard, maximumShards> shards;
    std::array<std::vector<float>, 2> sparkleEchoBuffers;
    std::array<float, 2> sparkleHighPassState {};
    std::array<float, 2> sparkleEchoDampState {};
    juce::SmoothedValue<float> sparkleGainSmoother;
    juce::SmoothedValue<float> sparkleEchoDelaySmoother;
    std::atomic<std::uint32_t> shardSpawnCounter { 0 };
    int sparkleEchoSize = 0;
    int sparkleEchoWriteIndex = 0;
    int sparkleQuietSamples = 0;
    int nextShardIndex = 0;
    int onsetHoldoff = 0;
    int onsetMeasureCountdown = 0;
    int samplesSinceOnset = 0;
    int onsetHoldoffSamples = 1;
    int onsetMeasureSamples = 1;
    int maximumOnsetAge = 1;
    float onsetFastEnvelope = 0.0f;
    float onsetSlowEnvelope = 0.0f;
    float onsetPeak = 0.0f;
    float onsetAttackAmount = 1.0f;
    float onsetReleaseAmount = 1.0f;
    float onsetSlowAmount = 1.0f;
    float sparkleHighPassAmount = 1.0f;
    float sparkleOverlapAttackAmount = 1.0f;
    float sparkleOverlapReleaseAmount = 1.0f;
    float sparkleOverlapEnvelope = 0.0f;
    float shatterCharge = 0.0f;
    float shardSpawnPhase = 0.0f;
    float shardSpawnThreshold = 1.0f;
    std::uint32_t shardRandomState = 0x1b873593u;
    bool sparkleEngaged = false;

    std::array<float, 2> highPassLowState {};
    std::array<float, 2> toneState {};
    juce::SmoothedValue<float> feedbackSmoother;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> toneSmoother;
    juce::SmoothedValue<float> earlyDiffusionBlendSmoother;
    juce::SmoothedValue<float> diffuseDelaySmoother;
    juce::SmoothedValue<float> diffuseMotionSmoother;
    juce::SmoothedValue<float> diffuseBlendSmoother;
    std::array<juce::SmoothedValue<float>, diffuseLineCount> diffuseGainSmoothers;
    juce::SmoothedValue<float> widthSmoother;
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> bypassSmoother;

    double sampleRate = 44100.0;
    int preparedChannels = 2;
    int historySize = 0;
    int writeIndex = 0;
    int nextGrainIndex = 0;
    double samplesUntilNextGrain = 0.0;
    float diffuseWanderAmount = 1.0f;
    float diffuseTargetSize = -1.0f;
    float diffuseTargetDensity = -1.0f;
    float diffuseTargetScatter = -1.0f;
    float diffuseTargetFeedback = -1.0f;
    std::uint32_t randomState = 0x6d2b79f5u;
    std::uint32_t levelRandomState = 0x243f6a88u;
    bool smoothersPrimed = false;
};
