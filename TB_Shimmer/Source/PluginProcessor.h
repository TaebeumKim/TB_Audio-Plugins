#pragma once

#include <JuceHeader.h>

#include "GranularShimmerEngine.h"

#include <array>
#include <atomic>
#include <cstdint>

class TBShimmerAudioProcessor final
    : public juce::AudioProcessor,
      private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct FactoryPreset
    {
        const char* stableId;
        const char* group;
        const char* name;
        float time;
        float size;
        float density;
        float scatter;
        float pitch;
        float reverse;
        float feedback;
        float tone;
        float width;
        float mix;
        float bypass;
        float levelVar;
        float positionRand;
        float pitchSpray;
        float freeze;
        float sparkle;
        float shatter;
    };

    inline static constexpr auto timeId = "time";
    inline static constexpr auto sizeId = "size";
    inline static constexpr auto densityId = "density";
    inline static constexpr auto scatterId = "scatter";
    inline static constexpr auto pitchId = "pitch";
    inline static constexpr auto reverseId = "reverse";
    inline static constexpr auto feedbackId = "feedback";
    inline static constexpr auto toneId = "tone";
    inline static constexpr auto widthId = "width";
    inline static constexpr auto mixId = "mix";
    inline static constexpr auto bypassId = "bypass";
    inline static constexpr auto levelVarId = "level_var";
    inline static constexpr auto positionRandId = "position_rand";
    inline static constexpr auto pitchSprayId = "pitch_spray";
    inline static constexpr auto freezeId = "freeze";
    inline static constexpr auto sparkleId = "sparkle";
    inline static constexpr auto shatterId = "shatter";

    inline static constexpr std::array<const char*, 17> parameterIds
    {{
        timeId,
        sizeId,
        densityId,
        scatterId,
        pitchId,
        reverseId,
        feedbackId,
        toneId,
        widthId,
        mixId,
        bypassId,
        levelVarId,
        positionRandId,
        pitchSprayId,
        freezeId,
        sparkleId,
        shatterId
    }};

    inline static constexpr auto pluginStateId = "TB_Shimmer";
    inline static constexpr auto userPresetExtension = ".tbshimmer";
    inline static constexpr int stateSchemaVersion = 4;
    inline static constexpr int presetFormatVersion = 4;

    TBShimmerAudioProcessor();
    ~TBShimmerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 180.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return hostBypassParameter;
    }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return parameters; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    int getFactoryPresetCount() const noexcept;
    const FactoryPreset& getFactoryPreset (int index) const;
    bool applyFactoryPreset (int index);

    juce::File getUserPresetDirectory() const;
    juce::Result saveUserPreset (const juce::File& requestedFile);
    juce::Result loadUserPreset (const juce::File& file);

    int getSelectedFactoryPreset() const noexcept { return selectedFactoryPreset.load(); }
    juce::String getSelectedUserPresetName() const;
    std::uint64_t getParameterRevision() const noexcept { return parameterRevision.load(); }
    std::uint32_t getShardSpawnCount() const noexcept { return engine.getShardSpawnCount(); }
    bool isCurrentPresetDirty() const noexcept
    {
        return parameterRevision.load() != selectedPresetRevision.load();
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float sanitise (float value, float fallback,
                           float minimum, float maximum) noexcept;
    void parameterChanged (const juce::String&, float) override;
    bool isValidState (const juce::ValueTree&) const;
    juce::ValueTree migrateStateToCurrent (const juce::ValueTree&) const;
    bool applyPresetValues (const std::array<float, parameterIds.size()>& values,
                            const juce::String& transactionName,
                            int factoryIndex,
                            const juce::String& userName);
    void setPresetSelection (int factoryIndex,
                             const juce::String& userName,
                             std::uint64_t revision);
    void processAudio (juce::AudioBuffer<float>&, bool hostBypassed);

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState parameters;
    GranularShimmerEngine engine;

    std::atomic<float>* timeParameter = nullptr;
    std::atomic<float>* sizeParameter = nullptr;
    std::atomic<float>* densityParameter = nullptr;
    std::atomic<float>* scatterParameter = nullptr;
    std::atomic<float>* pitchParameter = nullptr;
    std::atomic<float>* reverseParameter = nullptr;
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* toneParameter = nullptr;
    std::atomic<float>* widthParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* bypassParameter = nullptr;
    std::atomic<float>* levelVarParameter = nullptr;
    std::atomic<float>* positionRandParameter = nullptr;
    std::atomic<float>* pitchSprayParameter = nullptr;
    std::atomic<float>* freezeParameter = nullptr;
    std::atomic<float>* sparkleParameter = nullptr;
    std::atomic<float>* shatterParameter = nullptr;
    juce::AudioProcessorParameter* hostBypassParameter = nullptr;

    std::atomic<int> selectedFactoryPreset { 0 };
    std::atomic<bool> applyingStateOrPreset { false };
    std::atomic<std::uint64_t> parameterRevision { 0 };
    std::atomic<std::uint64_t> selectedPresetRevision { 0 };
    mutable juce::CriticalSection presetMetadataLock;
    juce::String selectedUserPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TBShimmerAudioProcessor)
};
