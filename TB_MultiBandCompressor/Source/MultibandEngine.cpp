#include "MultibandEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace tb::multiband
{
namespace
{
constexpr float minimumFrequencyHz = 30.0f;
constexpr float maximumFrequencyHz = 18000.0f;
constexpr float minimumBandwidthOctaves = 0.25f;
constexpr float maximumBandwidthOctaves = 4.0f;
constexpr float detectorFloorDb = -120.0f;
constexpr double parameterSmoothingSeconds = 0.02;
constexpr double linearKernelSmoothingSeconds = 0.05;
constexpr double linearHalfImpulseSeconds = 0.02;
constexpr double maximumLookaheadSeconds = 0.02;
constexpr int coefficientUpdateIntervalSamples = 8;
constexpr int linearFftOrder = 10;
constexpr int linearFftSize = 1 << linearFftOrder;
constexpr int linearPartitionSize = linearFftSize / 2;
constexpr int detectorExternalBandFirstLane = 14;
constexpr int oversamplingLaneCount = detectorExternalBandFirstLane
                                      + MultibandEngine::bandCount;
constexpr int dryLeftLane = 0;
constexpr int dryRightLane = 1;
constexpr int firstBandLane = 2;
constexpr int detectorMainLeftLane = 10;
constexpr int detectorMainRightLane = 11;
constexpr int detectorExternalLeftLane = 12;
constexpr int detectorExternalRightLane = 13;

float finiteOr (float value, float fallback) noexcept
{
    return std::isfinite (value) ? value : fallback;
}

float clampFinite (float value, float minimum, float maximum,
                   float fallback) noexcept
{
    return std::clamp (finiteOr (value, fallback), minimum, maximum);
}

template <int ChannelCount>
class FrameDelay final
{
public:
    void prepare (int maximumDelaySamples)
    {
        size = juce::jmax (1, maximumDelaySamples + 1);
        storage.resize (static_cast<std::size_t> (size * ChannelCount));
        reset();
    }

    void reset() noexcept
    {
        std::fill (storage.begin(), storage.end(), 0.0f);
        writePosition = 0;
    }

    void process (const std::array<float, ChannelCount>& input,
                  int delaySamples,
                  std::array<float, ChannelCount>& output) noexcept
    {
        const auto delay = juce::jlimit (0, size - 1, delaySamples);
        const auto readPosition = (writePosition - delay + size) % size;

        for (int channel = 0; channel < ChannelCount; ++channel)
        {
            const auto offset = channel * size;
            storage[static_cast<std::size_t> (offset + writePosition)] =
                input[static_cast<std::size_t> (channel)];
            output[static_cast<std::size_t> (channel)] = delay == 0
                ? input[static_cast<std::size_t> (channel)]
                : storage[static_cast<std::size_t> (offset + readPosition)];
        }

        writePosition = (writePosition + 1) % size;
    }

private:
    std::vector<float> storage;
    int size = 1;
    int writePosition = 0;
};

class RealtimeFFT final
{
public:
    void prepare (int orderToUse)
    {
        size = 1 << orderToUse;
        bitReversed.resize (static_cast<std::size_t> (size));
        roots.resize (static_cast<std::size_t> (size / 2));

        for (int index = 0; index < size; ++index)
        {
            auto source = index;
            auto reversed = 0;
            for (int bit = 0; bit < orderToUse; ++bit)
            {
                reversed = (reversed << 1) | (source & 1);
                source >>= 1;
            }
            bitReversed[static_cast<std::size_t> (index)] = reversed;
        }

        for (int index = 0; index < size / 2; ++index)
        {
            const auto phase = -juce::MathConstants<float>::twoPi
                               * static_cast<float> (index)
                               / static_cast<float> (size);
            roots[static_cast<std::size_t> (index)] =
                { std::cos (phase), std::sin (phase) };
        }
    }

    void transform (std::complex<float>* data, bool inverse) const noexcept
    {
        for (int index = 0; index < size; ++index)
        {
            const auto reversed = bitReversed[static_cast<std::size_t> (index)];
            if (reversed > index)
                std::swap (data[index], data[reversed]);
        }

        for (int length = 2; length <= size; length <<= 1)
        {
            const auto halfLength = length / 2;
            const auto rootStep = size / length;
            for (int start = 0; start < size; start += length)
                for (int offset = 0; offset < halfLength; ++offset)
                {
                    auto root = roots[static_cast<std::size_t> (offset * rootStep)];
                    if (inverse)
                        root = std::conj (root);
                    const auto even = data[start + offset];
                    const auto odd = data[start + offset + halfLength] * root;
                    data[start + offset] = even + odd;
                    data[start + offset + halfLength] = even - odd;
                }
        }

        if (inverse)
        {
            const auto scale = 1.0f / static_cast<float> (size);
            for (int index = 0; index < size; ++index)
                data[index] *= scale;
        }
    }

private:
    std::vector<int> bitReversed;
    std::vector<std::complex<float>> roots;
    int size = 0;
};

class LinearPhaseBandBank final
{
public:
    using BandSettings = MultibandEngine::BandSettings;
    using ChannelMode = MultibandEngine::ChannelMode;

    void prepare (double sampleRateToUse)
    {
        sampleRate = sampleRateToUse;
        groupDelay = juce::jmax (1, juce::roundToInt (
            sampleRate * linearHalfImpulseSeconds));
        impulseLength = groupDelay * 2 + 1;
        partitionCount = juce::jmax (
            1, (impulseLength + linearPartitionSize - 1) / linearPartitionSize);

        fft.prepare (linearFftOrder);
        for (auto& history : inputSpectra)
            history.resize (static_cast<std::size_t> (
                partitionCount * linearFftSize));
        for (auto& input : inputTime)
            input.resize (linearFftSize);
        frequencyWork.resize (linearFftSize);
        impulseWork.resize (linearFftSize);
        impulse.resize (static_cast<std::size_t> (impulseLength));

        for (auto& kernel : kernels)
        {
            const auto spectrumSize = static_cast<std::size_t> (
                partitionCount * linearFftSize);
            kernel.current.resize (spectrumSize);
            kernel.target.resize (spectrumSize);
            kernel.initialised = false;
            kernel.requestedFrequencyHz = 1000.0f;
            kernel.requestedBandwidthOctaves = 1.5f;
            kernel.designedFrequencyHz = -1.0f;
            kernel.designedBandwidthOctaves = -1.0f;
        }

        for (auto& band : outputBlocks)
            for (auto& channel : band)
                channel.resize (linearPartitionSize);
        for (auto& band : overlap)
            for (auto& channel : band)
                channel.resize (linearPartitionSize);

        reset();
    }

    void reset() noexcept
    {
        for (auto& history : inputSpectra)
            std::fill (history.begin(), history.end(), std::complex<float> {});
        for (auto& input : inputTime)
            std::fill (input.begin(), input.end(), std::complex<float> {});
        for (auto& band : outputBlocks)
            for (auto& channel : band)
                std::fill (channel.begin(), channel.end(), 0.0f);
        for (auto& band : overlap)
            for (auto& channel : band)
                std::fill (channel.begin(), channel.end(), 0.0f);

        inputFillPosition = 0;
        outputReadPosition = 0;
        spectrumWritePosition = 0;
        lastModes.fill (ChannelMode::stereo);
        modeInitialised.fill (false);
    }

    int getLatencySamples() const noexcept
    {
        return groupDelay + linearPartitionSize;
    }

    void setParameters (int band, float frequencyHz,
                        float bandwidthOctaves) noexcept
    {
        auto& kernel = kernels[static_cast<std::size_t> (band)];
        kernel.requestedFrequencyHz = frequencyHz;
        kernel.requestedBandwidthOctaves = bandwidthOctaves;
    }

    void processFrame (
        const std::array<float, 2>& input,
        const std::array<BandSettings, MultibandEngine::bandCount>& settings,
        std::array<std::array<float, 2>, MultibandEngine::bandCount>& output) noexcept
    {
        for (int band = 0; band < MultibandEngine::bandCount; ++band)
            for (int channel = 0; channel < 2; ++channel)
                output[static_cast<std::size_t> (band)]
                      [static_cast<std::size_t> (channel)] =
                    outputBlocks[static_cast<std::size_t> (band)]
                                [static_cast<std::size_t> (channel)]
                                [static_cast<std::size_t> (outputReadPosition)];

        for (int channel = 0; channel < 2; ++channel)
            inputTime[static_cast<std::size_t> (channel)]
                     [static_cast<std::size_t> (inputFillPosition)] =
                { input[static_cast<std::size_t> (channel)], 0.0f };

        ++inputFillPosition;
        ++outputReadPosition;
        if (inputFillPosition >= linearPartitionSize)
        {
            renderNextPartition (settings);
            inputFillPosition = 0;
            outputReadPosition = 0;
        }
    }

private:
    struct Kernel
    {
        std::vector<std::complex<float>> current;
        std::vector<std::complex<float>> target;
        float requestedFrequencyHz = 1000.0f;
        float requestedBandwidthOctaves = 1.5f;
        float designedFrequencyHz = -1.0f;
        float designedBandwidthOctaves = -1.0f;
        float mix = 1.0f;
        int updateCooldownSamples = 0;
        bool initialised = false;
    };

    void renderNextPartition (
        const std::array<BandSettings, MultibandEngine::bandCount>& settings) noexcept
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            auto& time = inputTime[static_cast<std::size_t> (channel)];
            std::fill (time.begin() + linearPartitionSize, time.end(),
                       std::complex<float> {});
            fft.transform (time.data(), false);
            auto* destination = inputSpectra[static_cast<std::size_t> (channel)].data()
                + spectrumWritePosition * linearFftSize;
            std::copy_n (time.data(), linearFftSize, destination);
            std::fill (time.begin(), time.begin() + linearPartitionSize,
                       std::complex<float> {});
        }

        for (int band = 0; band < MultibandEngine::bandCount; ++band)
        {
            const auto index = static_cast<std::size_t> (band);
            updateKernel (kernels[index]);

            if (! settings[index].enabled)
            {
                clearBandOutput (band);
                advanceKernel (kernels[index]);
                continue;
            }

            const auto mode = settings[index].channelMode;
            if (! modeInitialised[index] || lastModes[index] != mode)
            {
                for (auto& channel : overlap[index])
                    std::fill (channel.begin(), channel.end(), 0.0f);
                modeInitialised[index] = true;
                lastModes[index] = mode;
            }

            if (mode == ChannelMode::stereo)
            {
                renderSpectrum (band, 0, ChannelMode::stereo);
                writeStereoChannel (band, 0);
                renderSpectrum (band, 1, ChannelMode::stereo);
                writeStereoChannel (band, 1);
            }
            else
            {
                renderSpectrum (band, 0, mode);
                writeMidSide (band, mode == ChannelMode::side ? -1.0f : 1.0f);
            }

            advanceKernel (kernels[index]);
        }

        spectrumWritePosition = (spectrumWritePosition + 1) % partitionCount;
    }

    void clearBandOutput (int band) noexcept
    {
        const auto index = static_cast<std::size_t> (band);
        for (auto& channel : outputBlocks[index])
            std::fill (channel.begin(), channel.end(), 0.0f);
        for (auto& channel : overlap[index])
            std::fill (channel.begin(), channel.end(), 0.0f);
    }

    void updateKernel (Kernel& kernel) noexcept
    {
        const auto changed = kernel.requestedFrequencyHz
                                 != kernel.designedFrequencyHz
                             || kernel.requestedBandwidthOctaves
                                    != kernel.designedBandwidthOctaves;
        if (! kernel.initialised)
        {
            designKernel (kernel.requestedFrequencyHz,
                          kernel.requestedBandwidthOctaves, kernel.target);
            std::copy (kernel.target.begin(), kernel.target.end(),
                       kernel.current.begin());
            kernel.designedFrequencyHz = kernel.requestedFrequencyHz;
            kernel.designedBandwidthOctaves = kernel.requestedBandwidthOctaves;
            kernel.mix = 1.0f;
            kernel.initialised = true;
            return;
        }

        if (changed && kernel.updateCooldownSamples <= 0)
        {
            if (kernel.mix < 1.0f)
                for (std::size_t index = 0; index < kernel.current.size(); ++index)
                    kernel.current[index] += kernel.mix
                        * (kernel.target[index] - kernel.current[index]);
            else
                std::copy (kernel.target.begin(), kernel.target.end(),
                           kernel.current.begin());

            designKernel (kernel.requestedFrequencyHz,
                          kernel.requestedBandwidthOctaves, kernel.target);
            kernel.designedFrequencyHz = kernel.requestedFrequencyHz;
            kernel.designedBandwidthOctaves = kernel.requestedBandwidthOctaves;
            kernel.mix = 0.0f;
            kernel.updateCooldownSamples = juce::jmax (
                1, juce::roundToInt (sampleRate * parameterSmoothingSeconds));
        }
    }

    void advanceKernel (Kernel& kernel) noexcept
    {
        kernel.updateCooldownSamples = juce::jmax (
            0, kernel.updateCooldownSamples - linearPartitionSize);
        if (kernel.mix >= 1.0f)
            return;

        kernel.mix = juce::jmin (
            1.0f, kernel.mix
                + static_cast<float> (linearPartitionSize
                    / (sampleRate * linearKernelSmoothingSeconds)));
        if (kernel.mix >= 1.0f)
            std::copy (kernel.target.begin(), kernel.target.end(),
                       kernel.current.begin());
    }

    void designKernel (float frequencyHz, float bandwidthOctaves,
                       std::vector<std::complex<float>>& destination) noexcept
    {
        const auto octaveRatio = std::pow (
            2.0f, 0.5f * bandwidthOctaves);
        const auto nyquistLimit = static_cast<float> (sampleRate * 0.45);
        const auto low = juce::jlimit (
            5.0f, nyquistLimit, frequencyHz / octaveRatio);
        const auto high = juce::jlimit (
            low + 1.0f, nyquistLimit, frequencyHz * octaveRatio);

        for (int tap = 0; tap < impulseLength; ++tap)
        {
            const auto centred = tap - groupDelay;
            const auto idealLowPass = [centred, this] (float cutoff)
            {
                if (centred == 0)
                    return 2.0f * cutoff / static_cast<float> (sampleRate);
                return std::sin (2.0f * juce::MathConstants<float>::pi
                                 * cutoff * static_cast<float> (centred)
                                 / static_cast<float> (sampleRate))
                       / (juce::MathConstants<float>::pi
                          * static_cast<float> (centred));
            };
            const auto phase = juce::MathConstants<float>::twoPi
                               * static_cast<float> (tap)
                               / static_cast<float> (impulseLength - 1);
            const auto window = 0.42f - 0.5f * std::cos (phase)
                                + 0.08f * std::cos (2.0f * phase);
            impulse[static_cast<std::size_t> (tap)] =
                (idealLowPass (high) - idealLowPass (low)) * window;
        }

        auto centreResponse = 0.0;
        const auto centreOmega = juce::MathConstants<double>::twoPi
                                 * static_cast<double> (frequencyHz)
                                 / sampleRate;
        for (int tap = 0; tap < impulseLength; ++tap)
            centreResponse += static_cast<double> (
                impulse[static_cast<std::size_t> (tap)])
                * std::cos (centreOmega * static_cast<double> (tap - groupDelay));
        const auto normalisation = std::abs (centreResponse) > 1.0e-9
                                       ? static_cast<float> (1.0 / centreResponse)
                                       : 1.0f;
        for (auto& coefficient : impulse)
            coefficient *= normalisation;

        for (int partition = 0; partition < partitionCount; ++partition)
        {
            std::fill (impulseWork.begin(), impulseWork.end(),
                       std::complex<float> {});
            for (int sample = 0; sample < linearPartitionSize; ++sample)
            {
                const auto tap = partition * linearPartitionSize + sample;
                if (tap >= impulseLength)
                    break;
                impulseWork[static_cast<std::size_t> (sample)] =
                    { impulse[static_cast<std::size_t> (tap)], 0.0f };
            }
            fft.transform (impulseWork.data(), false);
            std::copy_n (
                impulseWork.data(), linearFftSize,
                destination.data() + partition * linearFftSize);
        }
    }

    void renderSpectrum (int band, int stereoChannel,
                         ChannelMode mode) noexcept
    {
        const auto& kernel = kernels[static_cast<std::size_t> (band)];
        for (int bin = 0; bin < linearFftSize; ++bin)
        {
            auto sum = std::complex<float> {};
            for (int partition = 0; partition < partitionCount; ++partition)
            {
                const auto historyIndex = (spectrumWritePosition - partition
                                           + partitionCount) % partitionCount;
                const auto spectrumOffset = historyIndex * linearFftSize + bin;
                auto input = inputSpectra[static_cast<std::size_t> (
                    stereoChannel)][static_cast<std::size_t> (spectrumOffset)];
                if (mode == ChannelMode::mid)
                    input = 0.5f * (
                        inputSpectra[0][static_cast<std::size_t> (spectrumOffset)]
                        + inputSpectra[1][static_cast<std::size_t> (spectrumOffset)]);
                else if (mode == ChannelMode::side)
                    input = 0.5f * (
                        inputSpectra[0][static_cast<std::size_t> (spectrumOffset)]
                        - inputSpectra[1][static_cast<std::size_t> (spectrumOffset)]);

                const auto kernelOffset = partition * linearFftSize + bin;
                const auto current = kernel.current[
                    static_cast<std::size_t> (kernelOffset)];
                const auto response = kernel.mix >= 1.0f
                    ? current
                    : current + kernel.mix * (
                        kernel.target[static_cast<std::size_t> (kernelOffset)]
                        - current);
                sum += input * response;
            }
            frequencyWork[static_cast<std::size_t> (bin)] = sum;
        }

        fft.transform (frequencyWork.data(), true);
    }

    void writeStereoChannel (int band, int channel) noexcept
    {
        auto& destination = outputBlocks[static_cast<std::size_t> (band)]
                                        [static_cast<std::size_t> (channel)];
        auto& previousOverlap = overlap[static_cast<std::size_t> (band)]
                                       [static_cast<std::size_t> (channel)];
        for (int sample = 0; sample < linearPartitionSize; ++sample)
        {
            destination[static_cast<std::size_t> (sample)] =
                frequencyWork[static_cast<std::size_t> (sample)].real()
                + previousOverlap[static_cast<std::size_t> (sample)];
            previousOverlap[static_cast<std::size_t> (sample)] =
                frequencyWork[static_cast<std::size_t> (
                    sample + linearPartitionSize)].real();
        }
    }

    void writeMidSide (int band, float rightSign) noexcept
    {
        const auto index = static_cast<std::size_t> (band);
        for (int sample = 0; sample < linearPartitionSize; ++sample)
        {
            const auto first = frequencyWork[static_cast<std::size_t> (sample)].real();
            const auto second = frequencyWork[static_cast<std::size_t> (
                sample + linearPartitionSize)].real();
            outputBlocks[index][0][static_cast<std::size_t> (sample)] =
                first + overlap[index][0][static_cast<std::size_t> (sample)];
            outputBlocks[index][1][static_cast<std::size_t> (sample)] =
                rightSign * first
                + overlap[index][1][static_cast<std::size_t> (sample)];
            overlap[index][0][static_cast<std::size_t> (sample)] = second;
            overlap[index][1][static_cast<std::size_t> (sample)] =
                rightSign * second;
        }
    }

    RealtimeFFT fft;
    std::array<std::vector<std::complex<float>>, 2> inputSpectra;
    std::array<std::vector<std::complex<float>>, 2> inputTime;
    std::array<Kernel, MultibandEngine::bandCount> kernels;
    std::array<std::array<std::vector<float>, 2>,
               MultibandEngine::bandCount> outputBlocks;
    std::array<std::array<std::vector<float>, 2>,
               MultibandEngine::bandCount> overlap;
    std::array<ChannelMode, MultibandEngine::bandCount> lastModes {};
    std::array<bool, MultibandEngine::bandCount> modeInitialised {};
    std::vector<std::complex<float>> frequencyWork;
    std::vector<std::complex<float>> impulseWork;
    std::vector<float> impulse;
    double sampleRate = 48000.0;
    int groupDelay = 1;
    int impulseLength = 3;
    int partitionCount = 1;
    int inputFillPosition = 0;
    int outputReadPosition = 0;
    int spectrumWritePosition = 0;
};
} // namespace

struct MultibandEngine::ProcessingPath
{
    void prepare (double baseSampleRateToUse, int maximumBlockSize,
                  int factorExponent)
    {
        baseSampleRate = baseSampleRateToUse;
        factor = 1 << factorExponent;
        internalSampleRate = baseSampleRate * static_cast<double> (factor);
        maximumBaseBlockSize = juce::jmax (1, maximumBlockSize);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            oversamplingLaneCount, static_cast<std::size_t> (factorExponent),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true, true);
        oversampling->initProcessing (
            static_cast<std::size_t> (maximumBaseBlockSize));
        baseScratch.setSize (oversamplingLaneCount, maximumBaseBlockSize,
                             false, false, true);
        downsampleScratch.setSize (oversamplingLaneCount, maximumBaseBlockSize,
                                   false, false, true);

        linearBands.prepare (baseSampleRate);
        const auto maximumLookahead = juce::roundToInt (
            baseSampleRate * maximumLookaheadSeconds);
        const auto linearLatency = linearBands.getLatencySamples();
        processingDelay.prepare (maximumLookahead);
        dryDelay.prepare (linearLatency + maximumLookahead);
        detectorMainDelay.prepare (linearLatency);
        detectorExternalDelay.prepare (linearLatency);
        detectorExternalBandDelay.prepare (linearLatency);
        auditionDelay.prepare (maximumLookahead * factor);

        for (auto& state : bandStates)
        {
            state.processingFilter.prepare (
                internalSampleRate, maximumBaseBlockSize * factor);
            state.detectorFilter.prepare (
                internalSampleRate, maximumBaseBlockSize * factor);
            state.detectorSourceMix.reset (
                internalSampleRate, parameterSmoothingSeconds);
            state.detectorSourceMix.setCurrentAndTargetValue (0.0f);
        }

        for (auto& weight : deltaListenWeights)
        {
            weight.reset (internalSampleRate, parameterSmoothingSeconds);
            weight.setCurrentAndTargetValue (0.0f);
        }

        bypassWetMix.reset (internalSampleRate, parameterSmoothingSeconds);
        reset();
    }

    void reset() noexcept
    {
        if (oversampling != nullptr)
            oversampling->reset();
        linearBands.reset();
        processingDelay.reset();
        dryDelay.reset();
        detectorMainDelay.reset();
        detectorExternalDelay.reset();
        detectorExternalBandDelay.reset();
        auditionDelay.reset();

        for (auto& state : bandStates)
        {
            state.processingFilter.reset();
            state.detectorFilter.reset();
            state.detectorSourceMix.setCurrentAndTargetValue (0.0f);
            state.envelope.fill (0.0f);
            state.wasEnabled = false;
            state.channelMode = ChannelMode::stereo;
            state.channelModeInitialised = false;
        }

        for (auto& weight : deltaListenWeights)
            weight.setCurrentAndTargetValue (0.0f);

        bypassWetMix.setCurrentAndTargetValue (1.0f);
        bypassSmoothingInitialised = false;
        lastExternalSamples.fill (0.0f);
        lastExternalBandSamples.fill (0.0f);
    }

    int getOversamplingLatencySamples() const noexcept
    {
        return oversampling != nullptr
                   ? juce::roundToInt (oversampling->getLatencyInSamples())
                   : 0;
    }

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> baseScratch;
    juce::AudioBuffer<float> downsampleScratch;
    LinearPhaseBandBank linearBands;
    FrameDelay<2> processingDelay;
    FrameDelay<2> dryDelay;
    FrameDelay<2> detectorMainDelay;
    FrameDelay<2> detectorExternalDelay;
    FrameDelay<MultibandEngine::bandCount> detectorExternalBandDelay;
    FrameDelay<2> auditionDelay;
    std::array<BandState, bandCount> bandStates {};
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>,
               bandCount> deltaListenWeights {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassWetMix;
    std::array<float, maximumChannels> lastExternalSamples {};
    std::array<float, MultibandEngine::bandCount> lastExternalBandSamples {};
    double baseSampleRate = 48000.0;
    double internalSampleRate = 48000.0;
    int maximumBaseBlockSize = 1;
    int factor = 1;
    bool bypassSmoothingInitialised = false;
};

MultibandEngine::Settings::Settings() noexcept
{
    constexpr std::array<float, bandCount> defaultFrequencies {
        120.0f, 1200.0f, 7000.0f, 12000.0f
    };

    for (int band = 0; band < bandCount; ++band)
    {
        bands[static_cast<std::size_t> (band)].frequencyHz =
            defaultFrequencies[static_cast<std::size_t> (band)];
        bands[static_cast<std::size_t> (band)].detectorFrequencyHz =
            defaultFrequencies[static_cast<std::size_t> (band)];
    }

    bands.back().enabled = false;
}

MultibandEngine::MultibandEngine() noexcept
{
    for (auto& meter : gainMetersDb)
        meter.store (0.0f, std::memory_order_relaxed);
    for (auto& meter : detectorMetersDb)
        meter.store (detectorFloorDb, std::memory_order_relaxed);
}

MultibandEngine::~MultibandEngine() = default;

void MultibandEngine::NormalisedBandPass::prepare (double sampleRate,
                                                    int maximumBlockSize)
{
    filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    filter.prepare ({ sampleRate,
                      static_cast<juce::uint32> (juce::jmax (1, maximumBlockSize)),
                      static_cast<juce::uint32> (maximumChannels) });
    frequencySmoother.reset (sampleRate, parameterSmoothingSeconds);
    bandwidthSmoother.reset (sampleRate, parameterSmoothingSeconds);
    appliedFrequencyHz = -1.0f;
    appliedBandwidthOctaves = -1.0f;
    normalisation = 1.0f;
    coefficientSamplesUntilUpdate = 0;
    smoothingInitialised = false;
}

void MultibandEngine::NormalisedBandPass::reset() noexcept
{
    filter.reset();
    coefficientSamplesUntilUpdate = 0;
}

void MultibandEngine::NormalisedBandPass::setParameters (
    float frequencyHz, float bandwidthOctaves, double sampleRate) noexcept
{
    const auto maximumForRate = static_cast<float> (
        std::min (static_cast<double> (maximumFrequencyHz), sampleRate * 0.45));
    const auto safeFrequency = std::clamp (
        frequencyHz, minimumFrequencyHz, maximumForRate);
    const auto safeBandwidth = std::clamp (
        bandwidthOctaves, minimumBandwidthOctaves, maximumBandwidthOctaves);

    if (! smoothingInitialised)
    {
        frequencySmoother.setCurrentAndTargetValue (safeFrequency);
        bandwidthSmoother.setCurrentAndTargetValue (safeBandwidth);
        smoothingInitialised = true;
        applyParameters (safeFrequency, safeBandwidth);
        coefficientSamplesUntilUpdate = coefficientUpdateIntervalSamples;
        return;
    }

    if (safeFrequency != frequencySmoother.getTargetValue())
    {
        frequencySmoother.setTargetValue (safeFrequency);
        coefficientSamplesUntilUpdate = 0;
    }
    if (safeBandwidth != bandwidthSmoother.getTargetValue())
    {
        bandwidthSmoother.setTargetValue (safeBandwidth);
        coefficientSamplesUntilUpdate = 0;
    }
}

void MultibandEngine::NormalisedBandPass::applyParameters (
    float frequencyHz, float bandwidthOctaves) noexcept
{
    const auto q = MultibandEngine::bandwidthToQ (bandwidthOctaves);

    if (std::abs (bandwidthOctaves - appliedBandwidthOctaves) > 1.0e-6f)
    {
        filter.setResonance (q);
        appliedBandwidthOctaves = bandwidthOctaves;
        normalisation = 1.0f / q;
    }

    if (std::abs (frequencyHz - appliedFrequencyHz) > 1.0e-4f)
    {
        filter.setCutoffFrequency (frequencyHz);
        appliedFrequencyHz = frequencyHz;
    }
}

float MultibandEngine::NormalisedBandPass::processSample (
    int channel, float input) noexcept
{
    if (channel == 0 && smoothingInitialised)
    {
        const auto frequency = frequencySmoother.getNextValue();
        const auto bandwidth = bandwidthSmoother.getNextValue();
        const auto isSmoothing = frequencySmoother.isSmoothing()
                                 || bandwidthSmoother.isSmoothing();

        if (isSmoothing)
        {
            if (coefficientSamplesUntilUpdate <= 0)
            {
                applyParameters (frequency, bandwidth);
                coefficientSamplesUntilUpdate =
                    coefficientUpdateIntervalSamples - 1;
            }
            else
                --coefficientSamplesUntilUpdate;
        }
        else if (frequency != appliedFrequencyHz
                 || bandwidth != appliedBandwidthOctaves)
        {
            applyParameters (frequency, bandwidth);
            coefficientSamplesUntilUpdate = coefficientUpdateIntervalSamples;
        }
    }

    return filter.processSample (channel, input) * normalisation;
}

void MultibandEngine::NormalisedBandPass::snapToZero() noexcept
{
    filter.snapToZero();
}

void MultibandEngine::prepare (double sampleRate, int maximumBlockSize,
                               int channels)
{
    currentSampleRate = std::isfinite (sampleRate) && sampleRate > 0.0
                            ? sampleRate
                            : 48000.0;
    preparedMaximumBlockSize = juce::jmax (1, maximumBlockSize);
    preparedChannels = juce::jlimit (1, maximumChannels, channels);

    for (int index = 0; index < 4; ++index)
    {
        processingPaths[static_cast<std::size_t> (index)] =
            std::make_unique<ProcessingPath>();
        processingPaths[static_cast<std::size_t> (index)]->prepare (
            currentSampleRate, preparedMaximumBlockSize, index);
    }

    prepared = true;
    reset();
}

void MultibandEngine::reset() noexcept
{
    for (auto& path : processingPaths)
        if (path != nullptr)
            path->reset();

    lastOutputSamples.fill (0.0f);
    transitionStartSamples.fill (0.0f);
    transitionSamplesRemaining = 0;
    transitionTotalSamples = 0;
    transitionGuardSamplesRemaining = 0;
    qualityConfigurationInitialised = false;

    for (auto& meter : gainMetersDb)
        meter.store (0.0f, std::memory_order_relaxed);
    for (auto& meter : detectorMetersDb)
        meter.store (detectorFloorDb, std::memory_order_relaxed);
}

float MultibandEngine::bandwidthToQ (float bandwidthOctaves) noexcept
{
    const auto safeBandwidth = std::clamp (
        bandwidthOctaves, minimumBandwidthOctaves, maximumBandwidthOctaves);
    const auto denominator = 2.0f * std::sinh (
        0.5f * std::log (2.0f) * safeBandwidth);
    return 1.0f / denominator;
}

float MultibandEngine::computeActivation (float distanceDb,
                                           float kneeDb) noexcept
{
    if (kneeDb <= 0.0f)
        return std::max (0.0f, distanceDb);

    const auto halfKnee = 0.5f * kneeDb;
    if (distanceDb <= -halfKnee)
        return 0.0f;
    if (distanceDb >= halfKnee)
        return distanceDb;

    const auto position = distanceDb + halfKnee;
    return position * position / (2.0f * kneeDb);
}

float MultibandEngine::computeDynamicGainDb (
    float detectorLevelDb, const BandSettings& settings) noexcept
{
    if (std::abs (settings.rangeDb) <= 1.0e-7f || settings.ratio <= 1.0f)
        return 0.0f;

    const auto positiveRange = settings.rangeDb > 0.0f;
    const auto activeAboveThreshold =
        settings.dynamicsMode == DynamicsMode::compress;
    const auto distanceDb = activeAboveThreshold
                                ? detectorLevelDb - settings.thresholdDb
                                : settings.thresholdDb - detectorLevelDb;
    const auto activationDb = computeActivation (distanceDb, settings.kneeDb);
    const auto scale = settings.dynamicsMode == DynamicsMode::compress
                           ? 1.0f - 1.0f / settings.ratio
                           : settings.ratio - 1.0f;
    const auto signedGainDb = std::copysign (activationDb * scale,
                                             settings.rangeDb);

    return positiveRange ? std::min (signedGainDb, settings.rangeDb)
                         : std::max (signedGainDb, settings.rangeDb);
}

MultibandEngine::BandSettings MultibandEngine::sanitise (
    const BandSettings& source, double sampleRate) noexcept
{
    auto result = source;
    const auto maximumForRate = static_cast<float> (
        std::min (static_cast<double> (maximumFrequencyHz), sampleRate * 0.45));

    result.frequencyHz = clampFinite (source.frequencyHz, minimumFrequencyHz,
                                      maximumForRate, 1000.0f);
    result.bandwidthOctaves = clampFinite (
        source.bandwidthOctaves, minimumBandwidthOctaves,
        maximumBandwidthOctaves, 1.5f);
    result.thresholdDb = clampFinite (source.thresholdDb, -60.0f, 0.0f, -18.0f);
    result.rangeDb = clampFinite (source.rangeDb, -48.0f, 12.0f, 0.0f);
    result.ratio = clampFinite (source.ratio, 1.0f, 20.0f, 2.0f);
    result.kneeDb = clampFinite (source.kneeDb, 0.0f, 18.0f, 6.0f);
    result.attackMs = clampFinite (source.attackMs, 0.5f, 200.0f, 15.0f);
    result.releaseMs = clampFinite (source.releaseMs, 20.0f, 2000.0f, 180.0f);
    result.outputDb = clampFinite (source.outputDb, -18.0f, 18.0f, 0.0f);
    result.detectorFrequencyHz = clampFinite (
        source.detectorFrequencyHz, minimumFrequencyHz, maximumForRate,
        result.frequencyHz);
    result.detectorBandwidthOctaves = clampFinite (
        source.detectorBandwidthOctaves, minimumBandwidthOctaves,
        maximumBandwidthOctaves, result.bandwidthOctaves);
    result.stereoLink = clampFinite (source.stereoLink, 0.0f, 1.0f, 1.0f);

    if (result.dynamicsMode != DynamicsMode::expand)
        result.dynamicsMode = DynamicsMode::compress;
    if (result.detectorSource != DetectorSource::external)
        result.detectorSource = DetectorSource::internal;
    if (result.detectorMode != DetectorMode::free)
        result.detectorMode = DetectorMode::band;
    if (result.channelMode != ChannelMode::mid
        && result.channelMode != ChannelMode::side)
        result.channelMode = ChannelMode::stereo;

    return result;
}

int MultibandEngine::oversamplingIndex (OversamplingMode mode) noexcept
{
    if (mode == OversamplingMode::x2)
        return 1;
    if (mode == OversamplingMode::x4)
        return 2;
    if (mode == OversamplingMode::x8)
        return 3;
    return 0;
}

int MultibandEngine::lookaheadSamples (const Settings& settings,
                                       double sampleRate) noexcept
{
    const auto lookaheadMs = clampFinite (
        settings.lookaheadMs, 0.0f, 20.0f, 0.0f);
    return juce::roundToInt (sampleRate * 0.001
                             * static_cast<double> (lookaheadMs));
}

int MultibandEngine::getLatencySamples (const Settings& settings) const noexcept
{
    if (! prepared)
        return 0;

    const auto mode = settings.oversamplingMode == OversamplingMode::x2
                          ? OversamplingMode::x2
                      : settings.oversamplingMode == OversamplingMode::x4
                          ? OversamplingMode::x4
                      : settings.oversamplingMode == OversamplingMode::x8
                          ? OversamplingMode::x8
                          : OversamplingMode::off;
    const auto& path = *processingPaths[static_cast<std::size_t> (
        oversamplingIndex (mode))];
    const auto linearLatency = settings.phaseMode == PhaseMode::linear
                                   ? path.linearBands.getLatencySamples()
                                   : 0;
    return lookaheadSamples (settings, currentSampleRate)
           + linearLatency + path.getOversamplingLatencySamples();
}

void MultibandEngine::process (
    juce::AudioBuffer<float>& mainBuffer,
    const ExternalSidechainBuses& externalBuses,
    const Settings& settings) noexcept
{
    const auto* previousExternalBuses = activeExternalBuses;
    activeExternalBuses = &externalBuses;
    process (mainBuffer, static_cast<const juce::AudioBuffer<float>*> (nullptr),
             settings);
    activeExternalBuses = previousExternalBuses;
}

void MultibandEngine::process (juce::AudioBuffer<float>& mainBuffer,
                               const juce::AudioBuffer<float>* externalBuffer,
                               const Settings& settings) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = mainBuffer.getNumSamples();
    const auto mainChannels = juce::jmin (maximumChannels,
                                          mainBuffer.getNumChannels());
    if (! prepared || mainChannels <= 0 || numSamples <= 0)
        return;

    jassert (mainChannels <= preparedChannels);

    const auto phaseMode = settings.phaseMode == PhaseMode::linear
                               ? PhaseMode::linear
                               : PhaseMode::dynamic;
    const auto oversamplingMode = settings.oversamplingMode == OversamplingMode::x2
                                      ? OversamplingMode::x2
                                  : settings.oversamplingMode == OversamplingMode::x4
                                      ? OversamplingMode::x4
                                  : settings.oversamplingMode == OversamplingMode::x8
                                      ? OversamplingMode::x8
                                      : OversamplingMode::off;
    const auto lookahead = lookaheadSamples (settings, currentSampleRate);
    auto& path = *processingPaths[static_cast<std::size_t> (
        oversamplingIndex (oversamplingMode))];

    const auto qualityChanged = qualityConfigurationInitialised
        && (phaseMode != activePhaseMode
            || oversamplingMode != activeOversamplingMode
            || lookahead != activeLookaheadSamples);
    if (qualityChanged)
    {
        path.reset();
        transitionStartSamples = lastOutputSamples;
        transitionTotalSamples = juce::jmax (
            1, juce::roundToInt (currentSampleRate
                                 * parameterSmoothingSeconds));
        transitionSamplesRemaining = transitionTotalSamples;
        const auto newPathLatency = lookahead
            + (phaseMode == PhaseMode::linear
                   ? path.linearBands.getLatencySamples()
                   : 0)
            + path.getOversamplingLatencySamples();
        transitionGuardSamplesRemaining = newPathLatency
                                          + transitionTotalSamples;
    }
    activePhaseMode = phaseMode;
    activeOversamplingMode = oversamplingMode;
    activeLookaheadSamples = lookahead;
    qualityConfigurationInitialised = true;

    const auto usingExternalBuses = activeExternalBuses != nullptr;
    std::array<bool, bandCount> externalBandAvailable {};
    auto hasAnyExternalBus = false;
    if (usingExternalBuses)
    {
        for (int band = 0; band < bandCount; ++band)
        {
            const auto* bus = (*activeExternalBuses)[static_cast<std::size_t> (band)];
            const auto valid = bus != nullptr && bus->getNumChannels() > 0
                               && bus->getNumSamples() >= numSamples;
            externalBandAvailable[static_cast<std::size_t> (band)] = valid;
            hasAnyExternalBus = hasAnyExternalBus || valid;
        }
    }

    const auto hasExternal = usingExternalBuses
                                 ? hasAnyExternalBus
                                 : externalBuffer != nullptr
                                       && externalBuffer->getNumChannels() > 0
                                       && externalBuffer->getNumSamples() >= numSamples;
    const auto externalChannels = ! usingExternalBuses && hasExternal
                                      ? externalBuffer->getNumChannels()
                                      : 0;
    // Four separate mono buses are the current fixed key matrix. Keep the
    // legacy shared mono/stereo/quad buffer path for direct engine callers.
    const auto hasPerBandExternal = usingExternalBuses
                                    || (hasExternal && externalChannels >= bandCount);
    if (! usingExternalBuses && hasExternal && externalChannels >= bandCount)
        externalBandAvailable.fill (true);

    std::array<BandSettings, bandCount> safeBands {};
    std::array<float, bandCount> attackCoefficients {};
    std::array<float, bandCount> releaseCoefficients {};
    std::array<float, bandCount> blockGainMeters {};
    std::array<float, bandCount> blockDetectorMeters {};
    blockDetectorMeters.fill (detectorFloorDb);

    auto auditionBand = -1;
    for (int band = 0; band < bandCount; ++band)
    {
        const auto index = static_cast<std::size_t> (band);
        safeBands[index] = sanitise (settings.bands[index], currentSampleRate);
        // The saved Mid/Side choice remains untouched, but a mono host has no
        // meaningful M/S domain. Both choices therefore process as Stereo.
        if (mainChannels == 1)
            safeBands[index].channelMode = ChannelMode::stereo;
        const auto& bandSettings = safeBands[index];
        const auto detectorFrequency = bandSettings.detectorMode == DetectorMode::band
                                           ? bandSettings.frequencyHz
                                           : bandSettings.detectorFrequencyHz;
        const auto detectorBandwidth = bandSettings.detectorMode == DetectorMode::band
                                           ? bandSettings.bandwidthOctaves
                                           : bandSettings.detectorBandwidthOctaves;

        path.linearBands.setParameters (
            band, bandSettings.frequencyHz, bandSettings.bandwidthOctaves);
        path.bandStates[index].processingFilter.setParameters (
            bandSettings.frequencyHz, bandSettings.bandwidthOctaves,
            path.internalSampleRate);
        path.bandStates[index].detectorFilter.setParameters (
            detectorFrequency, detectorBandwidth, path.internalSampleRate);
        path.bandStates[index].detectorSourceMix.setTargetValue (
            bandSettings.detectorSource == DetectorSource::external
                    && (usingExternalBuses
                            ? externalBandAvailable[index]
                            : hasExternal)
                ? 1.0f
                : 0.0f);

        auto& state = path.bandStates[index];
        const auto channelModeChanged = state.channelModeInitialised
                                        && state.channelMode
                                               != bandSettings.channelMode;
        if ((! bandSettings.enabled && state.wasEnabled) || channelModeChanged)
        {
            state.processingFilter.reset();
            state.detectorFilter.reset();
            state.envelope.fill (0.0f);
        }
        state.wasEnabled = bandSettings.enabled;
        state.channelMode = bandSettings.channelMode;
        state.channelModeInitialised = true;

        if (bandSettings.enabled)
        {
            attackCoefficients[index] = std::exp (
                -1.0f / (0.001f * bandSettings.attackMs
                         * static_cast<float> (path.internalSampleRate)));
            releaseCoefficients[index] = std::exp (
                -1.0f / (0.001f * bandSettings.releaseMs
                         * static_cast<float> (path.internalSampleRate)));
        }
        if (auditionBand < 0 && bandSettings.enabled && bandSettings.audition)
            auditionBand = band;
    }

    const auto deltaListenBand = juce::isPositiveAndBelow (
                                     settings.deltaListenBand, bandCount)
                                     && safeBands[static_cast<std::size_t> (
                                                      settings.deltaListenBand)]
                                            .enabled
                                 ? settings.deltaListenBand
                                 : -1;
    for (int band = 0; band < bandCount; ++band)
        path.deltaListenWeights[static_cast<std::size_t> (band)].setTargetValue (
            band == deltaListenBand ? 1.0f : 0.0f);

    const auto bypassTarget = settings.bypass ? 0.0f : 1.0f;
    if (! path.bypassSmoothingInitialised)
    {
        path.bypassWetMix.setCurrentAndTargetValue (bypassTarget);
        path.bypassSmoothingInitialised = true;
    }
    else
        path.bypassWetMix.setTargetValue (bypassTarget);

    const auto mix = clampFinite (settings.mix, 0.0f, 1.0f, 1.0f);
    const auto outputDb = clampFinite (settings.outputDb, -18.0f, 18.0f, 0.0f);
    const auto outputGain = juce::Decibels::decibelsToGain (outputDb);
    const auto linearLatency = path.linearBands.getLatencySamples();
    const auto internalLookahead = lookahead * path.factor;

    for (int sourceOffset = 0; sourceOffset < numSamples;
         sourceOffset += preparedMaximumBlockSize)
    {
        const auto chunkSamples = juce::jmin (
            preparedMaximumBlockSize, numSamples - sourceOffset);

        for (int sample = 0; sample < chunkSamples; ++sample)
        {
            const auto sourceSample = sourceOffset + sample;
            std::array<float, 2> rawMain {};
            std::array<float, 2> rawExternal {};
            std::array<float, bandCount> rawExternalBands {};
            for (int channel = 0; channel < mainChannels; ++channel)
            {
                const auto value = mainBuffer.getSample (channel, sourceSample);
                rawMain[static_cast<std::size_t> (channel)] =
                    std::isfinite (value) ? value : 0.0f;
            }
            if (mainChannels == 1)
                rawMain[1] = rawMain[0];

            for (int channel = 0; channel < maximumChannels; ++channel)
            {
                if (hasExternal && ! hasPerBandExternal)
                {
                    auto value = externalBuffer->getSample (
                        juce::jmin (channel, externalChannels - 1), sourceSample);
                    if (! std::isfinite (value))
                        value = 0.0f;
                    path.lastExternalSamples[static_cast<std::size_t> (channel)] = value;
                }
                rawExternal[static_cast<std::size_t> (channel)] =
                    path.lastExternalSamples[static_cast<std::size_t> (channel)];
            }

            for (int band = 0; band < bandCount; ++band)
            {
                if (hasPerBandExternal)
                {
                    auto value = 0.0f;
                    const auto index = static_cast<std::size_t> (band);
                    if (usingExternalBuses)
                    {
                        const auto* bus = (*activeExternalBuses)[index];
                        if (externalBandAvailable[index])
                        {
                            const auto channels = juce::jmin (
                                maximumChannels, bus->getNumChannels());
                            for (int channel = 0; channel < channels; ++channel)
                            {
                                const auto sampleValue = bus->getSample (
                                    channel, sourceSample);
                                if (std::isfinite (sampleValue))
                                    value += sampleValue;
                            }
                            if (channels > 1)
                                value /= static_cast<float> (channels);
                        }
                    }
                    else
                    {
                        value = externalBuffer->getSample (band, sourceSample);
                    }
                    if (! std::isfinite (value))
                        value = 0.0f;
                    path.lastExternalBandSamples[index] = value;
                }
                rawExternalBands[static_cast<std::size_t> (band)] =
                    path.lastExternalBandSamples[static_cast<std::size_t> (band)];
            }

            std::array<float, 2> delayedProcessing {};
            path.processingDelay.process (rawMain, lookahead, delayedProcessing);
            std::array<float, 2> dryFrame {};
            std::array<float, 2> detectorMainFrame {};
            std::array<float, 2> detectorExternalFrame {};
            std::array<float, bandCount> detectorExternalBandFrame {};
            std::array<std::array<float, 2>, bandCount> linearBandFrames {};

            if (phaseMode == PhaseMode::linear)
            {
                path.linearBands.processFrame (
                    delayedProcessing, safeBands, linearBandFrames);
                path.dryDelay.process (
                    rawMain, linearLatency + lookahead, dryFrame);
                path.detectorMainDelay.process (
                    rawMain, linearLatency, detectorMainFrame);
                path.detectorExternalDelay.process (
                    rawExternal, linearLatency, detectorExternalFrame);
                path.detectorExternalBandDelay.process (
                    rawExternalBands, linearLatency, detectorExternalBandFrame);
            }
            else
            {
                dryFrame = delayedProcessing;
                detectorMainFrame = rawMain;
                detectorExternalFrame = rawExternal;
                detectorExternalBandFrame = rawExternalBands;
            }

            path.baseScratch.setSample (dryLeftLane, sample, dryFrame[0]);
            path.baseScratch.setSample (dryRightLane, sample, dryFrame[1]);
            for (int band = 0; band < bandCount; ++band)
            {
                const auto lane = firstBandLane + band * 2;
                path.baseScratch.setSample (
                    lane, sample,
                    phaseMode == PhaseMode::linear
                        ? linearBandFrames[static_cast<std::size_t> (band)][0]
                        : 0.0f);
                path.baseScratch.setSample (
                    lane + 1, sample,
                    phaseMode == PhaseMode::linear
                        ? linearBandFrames[static_cast<std::size_t> (band)][1]
                        : 0.0f);
            }
            path.baseScratch.setSample (
                detectorMainLeftLane, sample, detectorMainFrame[0]);
            path.baseScratch.setSample (
                detectorMainRightLane, sample, detectorMainFrame[1]);
            path.baseScratch.setSample (
                detectorExternalLeftLane, sample, detectorExternalFrame[0]);
            path.baseScratch.setSample (
                detectorExternalRightLane, sample, detectorExternalFrame[1]);
            for (int band = 0; band < bandCount; ++band)
                path.baseScratch.setSample (
                    detectorExternalBandFirstLane + band, sample,
                    detectorExternalBandFrame[static_cast<std::size_t> (band)]);
        }

        const juce::dsp::AudioBlock<const float> scratchBlock (
            path.baseScratch);
        auto oversampled = path.oversampling->processSamplesUp (
            scratchBlock.getSubBlock (0, static_cast<std::size_t> (chunkSamples)));

        std::array<std::array<float, maximumChannels>, bandCount>
            processingSamples {};
        std::array<std::array<float, maximumChannels>, bandCount>
            detectorSamples {};
        std::array<std::array<float, maximumChannels>, bandCount>
            dynamicGainDb {};
        std::array<std::array<float, maximumChannels>, bandCount> bandDelta {};
        std::array<float, maximumChannels> delta {};
        std::array<float, maximumChannels> deltaListenInput {};
        std::array<float, maximumChannels> auditionInput {};
        std::array<float, maximumChannels> auditionAligned {};

        for (std::size_t sample = 0; sample < oversampled.getNumSamples(); ++sample)
        {
            const auto internalSample = static_cast<int> (sample);
            const std::array<float, 2> dry {
                oversampled.getSample (dryLeftLane, internalSample),
                oversampled.getSample (dryRightLane, internalSample)
            };
            delta.fill (0.0f);
            for (auto& contribution : bandDelta)
                contribution.fill (0.0f);
            deltaListenInput.fill (0.0f);
            auditionInput.fill (0.0f);

            for (int band = 0; band < bandCount; ++band)
            {
                const auto index = static_cast<std::size_t> (band);
                auto& state = path.bandStates[index];
                const auto& bandSettings = safeBands[index];
                if (! bandSettings.enabled)
                    continue;

                if (phaseMode == PhaseMode::linear)
                {
                    const auto lane = firstBandLane + band * 2;
                    processingSamples[index][0] =
                        oversampled.getSample (lane, internalSample);
                    processingSamples[index][1] =
                        oversampled.getSample (lane + 1, internalSample);
                }
                else if (bandSettings.channelMode == ChannelMode::stereo)
                {
                    processingSamples[index][0] =
                        state.processingFilter.processSample (0, dry[0]);
                    processingSamples[index][1] =
                        state.processingFilter.processSample (1, dry[1]);
                }
                else
                {
                    const auto domainInput = bandSettings.channelMode == ChannelMode::mid
                        ? 0.5f * (dry[0] + dry[1])
                        : 0.5f * (dry[0] - dry[1]);
                    const auto domainOutput =
                        state.processingFilter.processSample (0, domainInput);
                    processingSamples[index][0] = domainOutput;
                    processingSamples[index][1] =
                        bandSettings.channelMode == ChannelMode::side
                            ? -domainOutput
                            : domainOutput;
                }

                const auto detectorSourceMix =
                    state.detectorSourceMix.getNextValue();
                const auto useMappedExternalDomain = hasPerBandExternal
                    && bandSettings.detectorSource == DetectorSource::external
                    && bandSettings.channelMode != ChannelMode::stereo;
                std::array<float, 2> detectorInput {};
                for (int channel = 0; channel < maximumChannels; ++channel)
                {
                    const auto internalLane = channel == 0
                                                  ? detectorMainLeftLane
                                                  : detectorMainRightLane;
                    const auto externalLane = channel == 0
                                                  ? detectorExternalLeftLane
                                                  : detectorExternalRightLane;
                    const auto internal = oversampled.getSample (
                        internalLane, internalSample);
                    const auto external = hasPerBandExternal
                        ? oversampled.getSample (
                            detectorExternalBandFirstLane + band, internalSample)
                        : oversampled.getSample (externalLane, internalSample);
                    detectorInput[static_cast<std::size_t> (channel)] =
                        useMappedExternalDomain
                            ? internal
                            : internal + detectorSourceMix * (external - internal);
                }

                if (useMappedExternalDomain)
                {
                    // Per-band sidechain buses are mono control signals. Treat the selected
                    // key as the detector-domain input so Side does not cancel
                    // it merely because the key has no L/R difference.
                    const auto internalDomain = bandSettings.channelMode == ChannelMode::mid
                        ? 0.5f * (detectorInput[0] + detectorInput[1])
                        : 0.5f * (detectorInput[0] - detectorInput[1]);
                    const auto mappedExternal = oversampled.getSample (
                        detectorExternalBandFirstLane + band, internalSample);
                    const auto domainInput = internalDomain
                        + detectorSourceMix * (mappedExternal - internalDomain);
                    const auto domainOutput =
                        state.detectorFilter.processSample (0, domainInput);
                    detectorSamples[index][0] = domainOutput;
                    detectorSamples[index][1] = domainOutput;
                }
                else if (bandSettings.channelMode == ChannelMode::stereo)
                {
                    detectorSamples[index][0] =
                        state.detectorFilter.processSample (0, detectorInput[0]);
                    detectorSamples[index][1] =
                        state.detectorFilter.processSample (1, detectorInput[1]);
                }
                else
                {
                    const auto domainInput = bandSettings.channelMode == ChannelMode::mid
                        ? 0.5f * (detectorInput[0] + detectorInput[1])
                        : 0.5f * (detectorInput[0] - detectorInput[1]);
                    const auto domainOutput =
                        state.detectorFilter.processSample (0, domainInput);
                    detectorSamples[index][0] = domainOutput;
                    detectorSamples[index][1] = domainOutput;
                }

                const auto leftLevel = std::abs (detectorSamples[index][0]);
                const auto rightLevel = std::abs (detectorSamples[index][1]);
                const auto linkedLevel = std::max (leftLevel, rightLevel);
                const auto detectorChannels =
                    bandSettings.channelMode == ChannelMode::stereo ? 2 : 1;
                for (int channel = 0; channel < detectorChannels; ++channel)
                {
                    const auto localLevel = channel == 0 ? leftLevel : rightLevel;
                    const auto detectorTarget = localLevel
                        + bandSettings.stereoLink * (linkedLevel - localLevel);
                    auto& envelope = state.envelope[
                        static_cast<std::size_t> (channel)];
                    const auto coefficient = detectorTarget > envelope
                                                 ? attackCoefficients[index]
                                                 : releaseCoefficients[index];
                    envelope = detectorTarget
                               + coefficient * (envelope - detectorTarget);
                    if (std::abs (envelope) < 1.0e-20f)
                        envelope = 0.0f;
                    const auto detectorDb = juce::Decibels::gainToDecibels (
                        envelope, detectorFloorDb);
                    dynamicGainDb[index][static_cast<std::size_t> (channel)] =
                        computeDynamicGainDb (detectorDb, bandSettings);
                }
                if (detectorChannels == 1)
                {
                    state.envelope[1] = state.envelope[0];
                    dynamicGainDb[index][1] = dynamicGainDb[index][0];
                }

                const auto meterGain =
                    std::abs (dynamicGainDb[index][0])
                            >= std::abs (dynamicGainDb[index][1])
                        ? dynamicGainDb[index][0]
                        : dynamicGainDb[index][1];
                if (std::abs (meterGain) > std::abs (blockGainMeters[index]))
                    blockGainMeters[index] = meterGain;
                const auto detectorDb = juce::Decibels::gainToDecibels (
                    std::max (state.envelope[0], state.envelope[1]),
                    detectorFloorDb);
                blockDetectorMeters[index] = std::max (
                    blockDetectorMeters[index], detectorDb);

                if (band == auditionBand)
                {
                    if (bandSettings.channelMode == ChannelMode::stereo)
                    {
                        auditionInput[0] = detectorSamples[index][0];
                        auditionInput[1] = detectorSamples[index][1];
                    }
                    else
                    {
                        auditionInput[0] = detectorSamples[index][0];
                        auditionInput[1] =
                            bandSettings.channelMode == ChannelMode::side
                                ? -detectorSamples[index][0]
                                : detectorSamples[index][0];
                    }
                }

                for (int channel = 0; channel < maximumChannels; ++channel)
                {
                    const auto totalBandGain =
                        juce::Decibels::decibelsToGain (
                            dynamicGainDb[index][static_cast<std::size_t> (channel)]
                            + bandSettings.outputDb);
                    const auto contribution =
                        (totalBandGain - 1.0f)
                        * processingSamples[index][static_cast<std::size_t> (channel)];
                    bandDelta[index][static_cast<std::size_t> (channel)] =
                        contribution;
                    delta[static_cast<std::size_t> (channel)] += contribution;
                }
            }

            auto deltaListenAmount = 0.0f;
            for (int band = 0; band < bandCount; ++band)
            {
                const auto index = static_cast<std::size_t> (band);
                const auto weight = path.deltaListenWeights[index].getNextValue();
                deltaListenAmount += weight;
                for (int channel = 0; channel < maximumChannels; ++channel)
                    deltaListenInput[static_cast<std::size_t> (channel)] +=
                        weight * bandDelta[index][static_cast<std::size_t> (channel)];
            }
            deltaListenAmount = juce::jlimit (0.0f, 1.0f, deltaListenAmount);

            path.auditionDelay.process (
                auditionInput, internalLookahead, auditionAligned);
            const auto bypassMix = path.bypassWetMix.getNextValue();
            for (int channel = 0; channel < maximumChannels; ++channel)
            {
                const auto index = static_cast<std::size_t> (channel);
                const auto normalOrAudition = auditionBand >= 0
                                                  ? auditionAligned[index]
                                                  : dry[index] + mix * delta[index];
                const auto mixed = (1.0f - deltaListenAmount)
                                           * normalOrAudition
                                   + deltaListenInput[index];
                const auto processed = mixed * outputGain;
                const auto output = dry[index]
                                    + bypassMix * (processed - dry[index]);
                oversampled.setSample (
                    channel, internalSample,
                    std::isfinite (output) ? output : 0.0f);
            }
        }

        juce::dsp::AudioBlock<float> downsampleBlock (
            path.downsampleScratch);
        auto outputBlock = downsampleBlock.getSubBlock (
            0,
            static_cast<std::size_t> (chunkSamples));
        path.oversampling->processSamplesDown (outputBlock);

        for (int channel = 0; channel < mainChannels; ++channel)
            juce::FloatVectorOperations::copy (
                mainBuffer.getWritePointer (channel, sourceOffset),
                path.downsampleScratch.getReadPointer (channel),
                chunkSamples);

        for (int sample = 0; sample < chunkSamples; ++sample)
        {
            for (int channel = 0; channel < mainChannels; ++channel)
            {
                auto output = mainBuffer.getSample (
                    channel, sourceOffset + sample);
                if (! std::isfinite (output))
                    output = 0.0f;
                if (transitionSamplesRemaining > 0)
                {
                    const auto denominator = juce::jmax (
                        1, transitionTotalSamples - 1);
                    const auto elapsed = transitionTotalSamples
                                         - transitionSamplesRemaining;
                    const auto amount = static_cast<float> (elapsed)
                                        / static_cast<float> (denominator);
                    output = transitionStartSamples[
                                 static_cast<std::size_t> (channel)]
                             + amount * (output
                                         - transitionStartSamples[
                                             static_cast<std::size_t> (channel)]);
                }
                if (transitionGuardSamplesRemaining > 0)
                {
                    constexpr auto maximumTransitionStep = 0.02f;
                    const auto previous = lastOutputSamples[
                        static_cast<std::size_t> (channel)];
                    output = previous + juce::jlimit (
                        -maximumTransitionStep, maximumTransitionStep,
                        output - previous);
                }
                mainBuffer.setSample (channel, sourceOffset + sample, output);
                lastOutputSamples[static_cast<std::size_t> (channel)] = output;
            }
            if (mainChannels == 1)
                lastOutputSamples[1] = lastOutputSamples[0];
            if (transitionSamplesRemaining > 0)
                --transitionSamplesRemaining;
            if (transitionGuardSamplesRemaining > 0)
                --transitionGuardSamplesRemaining;
        }
    }

    const auto fullyBypassed = path.bypassWetMix.getTargetValue() <= 0.0f
                               && ! path.bypassWetMix.isSmoothing();
    for (int band = 0; band < bandCount; ++band)
    {
        const auto index = static_cast<std::size_t> (band);
        gainMetersDb[index].store (safeBands[index].enabled && ! fullyBypassed
                                       ? blockGainMeters[index]
                                       : 0.0f,
                                   std::memory_order_relaxed);
        detectorMetersDb[index].store (safeBands[index].enabled && ! fullyBypassed
                                           ? blockDetectorMeters[index]
                                           : detectorFloorDb,
                                       std::memory_order_relaxed);
        path.bandStates[index].processingFilter.snapToZero();
        path.bandStates[index].detectorFilter.snapToZero();
    }
}

float MultibandEngine::getGainReductionDb (int band) const noexcept
{
    if (! juce::isPositiveAndBelow (band, bandCount))
        return 0.0f;
    return gainMetersDb[static_cast<std::size_t> (band)].load (
        std::memory_order_relaxed);
}

float MultibandEngine::getDetectorLevelDb (int band) const noexcept
{
    if (! juce::isPositiveAndBelow (band, bandCount))
        return detectorFloorDb;
    return detectorMetersDb[static_cast<std::size_t> (band)].load (
        std::memory_order_relaxed);
}
} // namespace tb::multiband
