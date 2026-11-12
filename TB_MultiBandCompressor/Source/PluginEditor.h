#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

class TB_MultiBandCompressorAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      private juce::Timer
{
public:
    explicit TB_MultiBandCompressorAudioProcessorEditor (
        TB_MultiBandCompressorAudioProcessor&);
    ~TB_MultiBandCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    class LookAndFeel;
    class BandGraph;
    class BandPanel;
    class VisibilityWatcher;

    void timerCallback() override;
    void updateVisibleActivity();
    void selectBand (int index);
    void syncQualityButtonStates();
    void refreshPresetDisplay();
    juce::Result scanUserPresets();
    void showPresetMenu();
    void cyclePreset (int direction);
    void choosePresetToSave();
    void showError (const juce::String&);
    void showToast (const juce::String&);
    int getCurrentPresetPosition() const;

    TB_MultiBandCompressorAudioProcessor& processor;
    std::unique_ptr<LookAndFeel> lookAndFeel;
    std::unique_ptr<VisibilityWatcher> visibilityWatcher;
    juce::TooltipWindow tooltipWindow;

    std::unique_ptr<BandGraph> graph;
    std::unique_ptr<BandPanel> bandPanel;
    int selectedBand = 0;

    juce::Slider mixSlider;
    std::array<juce::TextButton, 2> phaseModeButtons;
    std::array<juce::TextButton, 4> oversamplingButtons;
    juce::Slider lookaheadSlider;
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        lookaheadAttachment;
    std::unique_ptr<juce::ParameterAttachment> phaseModeSegmentAttachment;
    std::unique_ptr<juce::ParameterAttachment> oversamplingSegmentAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::unique_ptr<juce::Button> previousButton;
    std::unique_ptr<juce::Button> nextButton;
    std::unique_ptr<juce::Button> saveButton;
    std::unique_ptr<juce::Button> undoButton;
    std::unique_ptr<juce::Button> redoButton;
    juce::TextButton presetButton { "Init" };

    juce::Image effectLogo;
    std::vector<juce::File> userPresetFiles;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String toastMessage;
    double toastUntilMs = 0.0;
    std::uint64_t lastParameterRevision = 0;
    std::uint64_t lastAudioBlockCounter = 0;
    double lastAudioBlockTimeMs = 0.0;
    bool timerAtIdleRate = false;
    bool lastSidechainActive = false;
    std::uint32_t lastSidechainBusMask = 0;
    bool lastBypassState = false;
    std::array<float, tbmbc::params::numBands> lastGainMeters {};
    std::array<float, tbmbc::params::numBands> lastDetectorMeters {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        TB_MultiBandCompressorAudioProcessorEditor)
};
