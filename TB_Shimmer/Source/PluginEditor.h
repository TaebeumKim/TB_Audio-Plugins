#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class TBShimmerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit TBShimmerAudioProcessorEditor (TBShimmerAudioProcessor&);
    ~TBShimmerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    class ShimmerLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                               juce::Slider&) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    };

    class HeaderButton final : public juce::Button
    {
    public:
        enum class Kind { previous, next, save, undo, redo };

        HeaderButton (const juce::String& name, Kind);
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    private:
        Kind kind;
    };

    struct GrainParticle
    {
        float age = 0.0f;
        float life = 1.0f;
        float angle = 0.0f;
        float radialRandom = 0.0f;
        float sizeScale = 1.0f;
        float tone = 0.0f;
        float spin = 0.0f;
        bool reverse = false;
    };

    // One visible fragment per sparkle shard the engine spawns.
    struct ShardGlint
    {
        float age = 0.0f;
        float life = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float velocityX = 0.0f;
        float velocityY = 0.0f;
        float rotation = 0.0f;
        float spin = 0.0f;
        float size = 3.0f;
        float tone = 0.0f;
    };

    void configureKnob (juce::Slider&, juce::Label&, const juce::String& name,
                        const juce::String& tooltip);
    void drawGrainFieldDisplay (juce::Graphics&, juce::Rectangle<float>);
    void buildGrainHaloSprites();
    void timerCallback() override;
    void seedGrainField();
    void advanceGrainField (float elapsedSeconds);
    GrainParticle makeGrainParticle (float lifeSeconds, float reverseChance,
                                     bool randomiseAge);
    ShardGlint makeShardGlint (float shatterNormal);
    void advanceShardGlints (float elapsedSeconds);
    void seedShardGlints();
    float getGrainLifeSeconds() const noexcept;
    int getTargetGrainCount() const noexcept;
    void refreshUserPresetFiles();
    void rebuildPresetMenu();
    void handlePresetSelection();
    void cyclePreset (int direction);
    void saveCurrentPreset();
    void saveNamedUserPreset (const juce::String&, bool allowOverwrite);
    void loadUserPreset (const juce::File&);
    void refreshPresetDisplay();
    void refreshCommandState();
    void showPresetError (const juce::String&);
    void showToast (const juce::String&);
    void handleParameterVisualChange();

    static juce::String formatMilliseconds (double);
    static juce::String formatFrequency (double);
    static juce::String formatSignedSemitones (double);

    std::array<juce::Slider*, 16> getSliders() noexcept;
    std::array<juce::Label*, 16> getLabels() noexcept;

    TBShimmerAudioProcessor& audioProcessor;
    ShimmerLookAndFeel lookAndFeel;
    juce::Image effectLogo;
    juce::Image prismGlass;
    std::array<juce::Image, 54> grainHaloSprites;

    juce::Slider timeSlider;
    juce::Slider sizeSlider;
    juce::Slider densitySlider;
    juce::Slider scatterSlider;
    juce::Slider pitchSlider;
    juce::Slider reverseSlider;
    juce::Slider feedbackSlider;
    juce::Slider toneSlider;
    juce::Slider widthSlider;
    juce::Slider mixSlider;
    juce::Slider levelVariationSlider;
    juce::Slider positionRandomSlider;
    juce::Slider pitchSpraySlider;
    juce::Slider freezeSlider;
    juce::Slider sparkleSlider;
    juce::Slider shatterSlider;

    juce::Label timeLabel;
    juce::Label sizeLabel;
    juce::Label densityLabel;
    juce::Label scatterLabel;
    juce::Label pitchLabel;
    juce::Label reverseLabel;
    juce::Label feedbackLabel;
    juce::Label toneLabel;
    juce::Label widthLabel;
    juce::Label mixLabel;
    juce::Label levelVariationLabel;
    juce::Label positionRandomLabel;
    juce::Label pitchSprayLabel;
    juce::Label freezeLabel;
    juce::Label sparkleLabel;
    juce::Label shatterLabel;

    HeaderButton previousButton { "Previous", HeaderButton::Kind::previous };
    juce::ComboBox presetBox { "Preset" };
    HeaderButton nextButton { "Next", HeaderButton::Kind::next };
    HeaderButton saveButton { "Save", HeaderButton::Kind::save };
    HeaderButton undoButton { "Undo", HeaderButton::Kind::undo };
    HeaderButton redoButton { "Redo", HeaderButton::Kind::redo };
    std::unique_ptr<SliderAttachment> timeAttachment;
    std::unique_ptr<SliderAttachment> sizeAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> scatterAttachment;
    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> reverseAttachment;
    std::unique_ptr<SliderAttachment> feedbackAttachment;
    std::unique_ptr<SliderAttachment> toneAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> levelVariationAttachment;
    std::unique_ptr<SliderAttachment> positionRandomAttachment;
    std::unique_ptr<SliderAttachment> pitchSprayAttachment;
    std::unique_ptr<SliderAttachment> freezeAttachment;
    std::unique_ptr<SliderAttachment> sparkleAttachment;
    std::unique_ptr<SliderAttachment> shatterAttachment;

    std::vector<juce::File> userPresetFiles;
    juce::String displayedPresetName;
    juce::String toastMessage;
    int toastGeneration = 0;
    bool rebuildingPresetMenu = false;

    std::vector<GrainParticle> grainParticles;
    float grainSpawnAccumulator = 0.0f;
    float grainPhase = 0.0f;
    std::uint32_t nextGrainSeed = 1;
    std::vector<ShardGlint> shardGlints;
    std::uint32_t lastShardSpawnCount = 0;
    float glintPreviewAccumulator = 0.0f;
    double lastVisualUpdateMs = 0.0;

    juce::TooltipWindow tooltipWindow { nullptr, 2000 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TBShimmerAudioProcessorEditor)
};
