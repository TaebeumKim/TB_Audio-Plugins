/*
  ==============================================================================

    Editor for the XYZ panner.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpatialPlane final : public juce::Component
{
public:
    enum class View { top, side };

    SpatialPlane (juce::AudioProcessorValueTreeState& parameters, View view);
    ~SpatialPlane() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getPlotBounds() const;
    juce::Rectangle<float> getMovementBounds() const;
    void beginGesture();
    void endGesture();
    float getParameterValue (const juce::String& parameterID) const;
    void setParameterValue (const juce::String& parameterID, float value);
    void updatePosition (juce::Point<float> position);
    void drawObjectMarker (juce::Graphics&, juce::Point<float>) const;
    void drawTopListener (juce::Graphics&, juce::Point<float>) const;
    void drawSideListener (juce::Graphics&, juce::Point<float>) const;

    juce::AudioProcessorValueTreeState& parameters;
    const View view;
    bool gestureActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpatialPlane)
};

class MetalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

class NewProjectAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit NewProjectAudioProcessorEditor (NewProjectAudioProcessor&);
    ~NewProjectAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text);
    void setRenderMode (int mode);
    void syncRenderModeButtons();
    void performUndo();
    void performRedo();
    void syncHistoryButtons();

    NewProjectAudioProcessor& audioProcessor;

    SpatialPlane topPlane;
    SpatialPlane sidePlane;
    MetalLookAndFeel metalLookAndFeel;
    std::unique_ptr<juce::Drawable> headerIcon;
    juce::Rectangle<int> controlPanelBounds;

    juce::Slider panSlider;
    juce::Slider frontBackSlider;
    juce::Slider heightSlider;
    juce::Slider distanceSlider;
    juce::Slider roomSlider;
    juce::Slider outputTrimSlider;
    juce::ComboBox distanceModelSelector;
    juce::Label distanceModelLabel;
    juce::TextButton stereoButton { "STEREO" };
    juce::TextButton headphonesButton { "HEADPHONES" };
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    juce::Label panLabel;
    juce::Label frontBackLabel;
    juce::Label heightLabel;
    juce::Label distanceLabel;
    juce::Label roomLabel;
    juce::Label outputTrimLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> panAttachment;
    std::unique_ptr<SliderAttachment> frontBackAttachment;
    std::unique_ptr<SliderAttachment> heightAttachment;
    std::unique_ptr<SliderAttachment> distanceAttachment;
    std::unique_ptr<SliderAttachment> roomAttachment;
    std::unique_ptr<SliderAttachment> outputTrimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> distanceModelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NewProjectAudioProcessorEditor)
};
