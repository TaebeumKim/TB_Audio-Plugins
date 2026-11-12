#pragma once

#include <JuceHeader.h>

#include "FactoryPresets.h"
#include "MultibandEngine.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

class TBMBCSpectrumAnalyzer final
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBins = fftSize / 2 + 1;

    TBMBCSpectrumAnalyzer();

    void reset() noexcept;
    void discardPending() noexcept;
    void push (const juce::AudioBuffer<float>&) noexcept;
    bool pullSpectrum();

    const std::array<float, numBins>& getMagnitudesDb() const noexcept
    {
        return magnitudesDb;
    }

    std::uint64_t getPushedSampleCount() const noexcept
    {
        return pushedSampleCount.load (std::memory_order_relaxed);
    }

    std::uint64_t getFrameGeneration() const noexcept
    {
        return frameGeneration.load (std::memory_order_relaxed);
    }

private:
    static constexpr int fifoSize = fftSize * 8;

    struct Storage
    {
        std::array<std::array<float, fifoSize>, 2> audioFifo {};
        std::array<std::array<float, fftSize * 2>, 2> fftData {};
    };

    void copyToFifo (int startIndex, int numSamples,
                     const juce::AudioBuffer<float>&, int sourceOffset) noexcept;

    juce::AbstractFifo fifo { fifoSize };
    std::unique_ptr<Storage> storage;
    std::array<float, numBins> magnitudesDb {};
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        fftSize, juce::dsp::WindowingFunction<float>::hann, false
    };
    std::atomic<std::uint64_t> pushedSampleCount { 0 };
    std::atomic<std::uint64_t> frameGeneration { 0 };
    std::atomic<bool> resetRequested { false };
};

class TB_MultiBandCompressorAudioProcessor final
    : public juce::AudioProcessor,
      private juce::AudioProcessorValueTreeState::Listener,
      private juce::AsyncUpdater
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    inline static constexpr int stateSchemaVersion = 4;
    inline static constexpr int presetFormatVersion = 4;

    TB_MultiBandCompressorAudioProcessor();
    ~TB_MultiBandCompressorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    APVTS& getValueTreeState() noexcept { return apvts; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    bool applyFactoryPreset (int presetId, bool makeUndoable = true);
    int getFactoryPresetCount() const noexcept;
    const tbmbc::presets::FactoryPreset& getFactoryPreset (int index) const;
    int getActiveFactoryPresetId() const noexcept { return activeFactoryPresetId.load(); }
    juce::String getCurrentPresetName() const;
    juce::File getCurrentUserPresetFile() const;

    static juce::String getUserPresetExtension();
    static juce::File getUserPresetDirectory();
    juce::Result saveUserPreset (const juce::File&);
    juce::Result loadUserPreset (const juce::File&, bool makeUndoable = true);

    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }
    void undo();
    void redo();

    float getGainReductionDb (int band) const noexcept;
    float getDetectorLevelDb (int band) const noexcept;
    int getDeltaListenBand() const noexcept
    {
        return deltaListenBand.load (std::memory_order_acquire);
    }
    void setDeltaListenBand (int band) noexcept;
    bool isExternalSidechainBusActive() const noexcept { return externalSidechainActive.load(); }
    bool isExternalSidechainBusActiveForBand (int band) const noexcept
    {
        return juce::isPositiveAndBelow (band, MultibandEngine::bandCount)
            && (externalSidechainBusMask.load (std::memory_order_relaxed)
                & (std::uint32_t (1) << static_cast<unsigned> (band))) != 0;
    }
    std::uint32_t getExternalSidechainBusMask() const noexcept
    {
        return externalSidechainBusMask.load (std::memory_order_relaxed);
    }
    int getExternalSidechainChannelCount() const noexcept
    {
        return externalSidechainChannels.load (std::memory_order_relaxed);
    }
    std::uint64_t getParameterRevision() const noexcept { return parameterRevision.load(); }
    std::uint64_t getAudioBlockCounter() const noexcept { return audioBlockCounter.load(); }
    TBMBCSpectrumAnalyzer& getInputSpectrumAnalyzer() noexcept { return inputSpectrumAnalyzer; }
    void setSpectrumAnalyzerDemand (bool shouldAnalyze) noexcept;
    int getSpectrumAnalyzerDemandCount() const noexcept
    {
        return spectrumAnalyzerDemandCount.load (std::memory_order_relaxed);
    }

    void setParameterActual (const juce::String& id, float actualValue,
                             bool notifyHost = true);

private:
    struct BandParameterPointers
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* bandwidth = nullptr;
        std::atomic<float>* threshold = nullptr;
        std::atomic<float>* range = nullptr;
        std::atomic<float>* ratio = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* knee = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* source = nullptr;
        std::atomic<float>* detectorMode = nullptr;
        std::atomic<float>* detectorFrequency = nullptr;
        std::atomic<float>* detectorBandwidth = nullptr;
        std::atomic<float>* stereoLink = nullptr;
        std::atomic<float>* audition = nullptr;
        std::atomic<float>* channelMode = nullptr;
    };

    inline static constexpr auto stateRootType = "PARAMETERS";
    inline static constexpr auto presetRootType = "TB_MultiBandCompressorPreset";
    inline static constexpr auto productIdentifier =
        "com.TeamImpulseImpact.TB_MultiBandCompressor";
    inline static constexpr auto presetExtension = ".tbmbc";
    inline static constexpr int maximumStateBytes = 1024 * 1024;
    inline static constexpr int maximumPresetBytes = 1024 * 1024;

    MultibandEngine::Settings readSettings (bool forceBypass = false) const noexcept;
    void processAudio (juce::AudioBuffer<float>&, bool forceBypass) noexcept;
    juce::ValueTree createSerializableState();
    juce::ValueTree createStateFromValues (const tbmbc::presets::Values&) const;
    bool isValidState (const juce::ValueTree&) const;
    bool extractStateValues (const juce::ValueTree&, tbmbc::presets::Values&,
                             bool allowLegacy) const;
    bool extractPresetValues (const juce::ValueTree&, tbmbc::presets::Values&,
                              bool allowLegacy = false) const;
    bool applyPresetValues (const tbmbc::presets::Values&, const juce::String& transaction,
                            bool makeUndoable, int factoryPresetId,
                            const juce::File& userPresetFile = {});
    void updateSelectionFromCurrentValues();
    void setUserPresetSelection (const juce::File&,
                                 const tbmbc::presets::Values* values = nullptr);
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;

    juce::UndoManager undoManager;
    APVTS apvts;

    std::array<BandParameterPointers, tbmbc::params::numBands> bandParams {};
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* phaseModeParam = nullptr;
    std::atomic<float>* oversamplingParam = nullptr;
    std::atomic<float>* lookaheadParam = nullptr;
    juce::AudioParameterBool* hostBypassParameter = nullptr;

    std::atomic<std::uint32_t> approvedQualityState { 0 };

    std::atomic<bool> applyingStateOrPreset { false };
    std::atomic<bool> externalSidechainActive { false };
    std::atomic<int> externalSidechainChannels { 0 };
    std::atomic<std::uint32_t> externalSidechainBusMask { 0 };
    std::atomic<int> activeFactoryPresetId { 1 };
    std::atomic<bool> currentSelectionIsUser { false };
    std::atomic<std::uint64_t> parameterRevision { 0 };
    std::atomic<std::uint64_t> audioBlockCounter { 0 };
    std::atomic<int> spectrumAnalyzerDemandCount { 0 };
    static_assert (std::atomic<int>::is_always_lock_free,
                   "Delta Listen selection must be realtime lock-free.");
    std::atomic<int> deltaListenBand { -1 };
    mutable juce::CriticalSection presetSelectionLock;
    juce::File selectedUserPresetFile;
    tbmbc::presets::Values selectedUserPresetValues {};
    bool selectedUserPresetValuesValid = false;

    MultibandEngine engine;
    TBMBCSpectrumAnalyzer inputSpectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TB_MultiBandCompressorAudioProcessor)
};
