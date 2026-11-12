#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <array>

class TB_ExciterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit TB_ExciterAudioProcessorEditor (TB_ExciterAudioProcessor&);
    ~TB_ExciterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label  label;
        juce::String readoutName;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    class SparkleLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        SparkleLookAndFeel (juce::Typeface::Ptr headingTypefaceIn,
                            juce::Typeface::Ptr bodyTypefaceIn);

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;

    private:
        juce::Typeface::Ptr headingTypeface;
        juce::Typeface::Ptr bodyTypeface;
    };

    class SparkleField final : public juce::Component,
                               private juce::Timer
    {
    public:
        SparkleField (juce::Slider& brightIn, juce::Slider& clarityIn,
                      juce::Slider& focusIn, juce::Slider& widthIn,
                      juce::Slider& levelIn);
        ~SparkleField() override;

        void paint (juce::Graphics&) override;

    private:
        struct Particle
        {
            static constexpr auto trailCapacity = 16;

            float angle = 0.0f;
            float radius = 0.0f;
            float velocity = 0.0f;
            float angularVelocity = 0.0f;
            float size = 1.0f;
            float phase = 0.0f;
            std::array<juce::Point<float>, trailCapacity> trail;
            int trailHead = 0;
            int trailCount = 0;
        };

        struct CorePoint
        {
            float angle = 0.0f;
            float distance = 0.0f;
            float size = 1.0f;
            float phase = 0.0f;
        };

        struct Values
        {
            float bright = 0.0f;
            float clarity = 0.0f;
            float focus = 0.0f;
            float width = 0.0f;
            float level = 0.0f;
        };

        void timerCallback() override;
        Values readValues() const;
        float normalisedValue (juce::Slider&) const;
        void resetParticle (Particle&, float coreRadius);
        void recordParticlePosition (Particle&);
        void separateParticles (int particleCount, float coreRadius);

        juce::Slider& bright;
        juce::Slider& clarity;
        juce::Slider& focus;
        juce::Slider& width;
        juce::Slider& level;

        static constexpr int activeIntervalMs = 33;    // 30 Hz
        static constexpr int hiddenIntervalMs = 250;   // 보이지 않을 때는 가시성만 확인

        std::array<Particle, 196> particles;
        std::array<CorePoint, 7> corePoints;
        juce::Random random { 0x54425863 };
        double lastFrameMs = 0.0;
        float elapsedSeconds = 0.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SparkleField)
    };

    /*  선택된 노브의 이름과 값. 값이 바뀔 때만 다시 그린다(이벤트 기반). 이전에는 자체
        30 Hz 타이머로 내용이 같아도 계속 repaint했다 (성능 가이드 7.1, 7.2). */
    class ActiveReadout final : public juce::Component,
                                private juce::Slider::Listener
    {
    public:
        explicit ActiveReadout (juce::Typeface::Ptr headingTypefaceIn);
        ~ActiveReadout() override;

        void setActive (juce::String name, juce::Slider& slider);
        void paint (juce::Graphics&) override;

    private:
        void sliderValueChanged (juce::Slider*) override;
        void refreshValueText();

        juce::Typeface::Ptr headingTypeface;
        juce::String activeName { "Bright" };
        juce::String valueText;
        juce::Slider* activeSlider = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActiveReadout)
    };

    void addControl (Control&, const juce::String& parameterId,
                     const juce::String& titleText, const juce::String& readoutText,
                     const juce::String& description, double defaultValue);
    void layoutControl (Control&, int x, int y);
    void setActiveControl (Control&);
    Control* findControlFor (const juce::Component*);
    void populatePresetBox();
    void syncPresetDisplay();
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void focusOfChildComponentChanged (FocusChangeType) override;

    TB_ExciterAudioProcessor& processor;

    juce::Typeface::Ptr headingTypeface;
    juce::Typeface::Ptr bodyTypeface;
    SparkleLookAndFeel lookAndFeel;

    Control tune, amount, character, width, output;
    SparkleField sparkleField;
    ActiveReadout activeReadout;
    juce::Label title;
    juce::Label provisionalNotice;
    juce::ComboBox presetBox;
    bool tearingDown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TB_ExciterAudioProcessorEditor)
};
