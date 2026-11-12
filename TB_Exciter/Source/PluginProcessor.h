#pragma once

#include <JuceHeader.h>
#include "ExciterEngine.h"
#include "FactoryPresets.h"
#include "Parameters.h"

class TB_ExciterAudioProcessor final : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    inline static constexpr int stateSchemaVersion  = 1;
    inline static constexpr int presetFormatVersion = 1;

    TB_ExciterAudioProcessor();
    ~TB_ExciterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    APVTS& getValueTreeState() noexcept { return apvts; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    bool applyFactoryPreset (int presetId);
    int getActiveFactoryPresetId() const noexcept { return activeFactoryPresetId.load(); }
    bool matchesFactoryPreset (int presetId) const noexcept;

private:
    ExciterEngine::Settings readSettings() const noexcept;
    float readOutputGain() const noexcept;
    void processAudio (juce::AudioBuffer<float>&, bool bypassed) noexcept;
    void setParameterFromPreset (const char* parameterId, float actualValue);

    juce::UndoManager undoManager;
    APVTS apvts;

    std::atomic<float>* tuneParam      = nullptr;
    std::atomic<float>* amountParam    = nullptr;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* widthParam     = nullptr;
    std::atomic<float>* outputParam    = nullptr;
    std::atomic<int> activeFactoryPresetId { 1 };

    ExciterEngine engine;
    juce::SmoothedValue<float> outputGain;

    // LEVEL의 bypass 크로스페이드. 1 = LEVEL 적용, 0 = bypass(단위 게인).
    juce::SmoothedValue<float> levelMix;
    bool levelMixPrimed = false;

    /*  상태 복원은 오디오 스레드와 직렬화되지 않는다. setStateInformation()은 이 플래그만
        세우고, 스무딩 상태를 건너뛰는 실제 snap은 다음 오디오 블록이 수행한다. */
    std::atomic<bool> settingsSnapPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TB_ExciterAudioProcessor)
};
