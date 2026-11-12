#include "PluginEditor.h"

#include <cmath>
#include <utility>

namespace
{
    const auto panelColour = juce::Colour (0xff100e0b);
    const auto insetColour = juce::Colour (0xff15120e);
    const auto creamColour = juce::Colour (0xfff6efe2);
    const auto accentColour = juce::Colour (0xffb68235);

    constexpr auto panelWidth = 590;
    constexpr auto panelHeight = 450;
    constexpr auto fieldSize = 540.0f;
    constexpr auto fieldRadius = fieldSize * 0.5f;

    juce::Font makeFont (const juce::Typeface::Ptr& typeface, float height,
                         float extraKerning = 0.0f)
    {
        if (typeface != nullptr)
            return juce::Font (juce::FontOptions (typeface).withHeight (height))
                .withExtraKerningFactor (extraKerning);

        return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(),
                                              height, juce::Font::plain))
            .withExtraKerningFactor (extraKerning);
    }

    juce::Typeface::Ptr loadTypeface (const char* data, int dataSize)
    {
        if (data == nullptr || dataSize <= 0)
            return {};

        return juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (dataSize));
    }

    juce::Point<float> polarPoint (juce::Point<float> centre, float radius, float angle)
    {
        return centre + juce::Point<float> (std::cos (angle - juce::MathConstants<float>::halfPi) * radius,
                                             std::sin (angle - juce::MathConstants<float>::halfPi) * radius);
    }
}

TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::SparkleLookAndFeel (
    juce::Typeface::Ptr headingTypefaceIn, juce::Typeface::Ptr bodyTypefaceIn)
    : headingTypeface (std::move (headingTypefaceIn)),
      bodyTypeface (std::move (bodyTypefaceIn))
{
    setColour (juce::PopupMenu::backgroundColourId, panelColour);
    setColour (juce::PopupMenu::textColourId, creamColour);
    setColour (juce::PopupMenu::headerTextColourId, accentColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.28f));
    setColour (juce::PopupMenu::highlightedTextColourId, creamColour);
}

void TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                 static_cast<float> (width), static_cast<float> (height));
    const auto centre = bounds.getCentre();
    const auto scale = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 150.0f;
    const auto outerRadius = 62.0f * scale;
    const auto knobRadius = 46.0f * scale;
    const auto trackRadius = 52.0f * scale;
    const auto position = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
    const auto valueAngle = rotaryStartAngle + position * (rotaryEndAngle - rotaryStartAngle);
    const auto thinStroke = juce::jmax (0.75f, 1.0f * scale);
    const auto trackStroke = juce::jmax (0.8f, 1.5f * scale);
    const auto activeStroke = juce::jmax (1.0f, 2.0f * scale);
    const auto pointerStroke = juce::jmax (1.0f, 2.0f * scale);

    if (slider.hasKeyboardFocus (true))
    {
        g.setColour (accentColour.withAlpha (0.55f));
        g.drawEllipse (juce::Rectangle<float> (outerRadius * 2.0f + 8.0f * scale,
                                                outerRadius * 2.0f + 8.0f * scale)
                           .withCentre (centre),
                       juce::jmax (1.0f, 1.5f * scale));
    }

    g.setColour (creamColour.withAlpha (0.09f));
    g.drawEllipse (juce::Rectangle<float> (outerRadius * 2.0f, outerRadius * 2.0f).withCentre (centre),
                   thinStroke);

    g.setColour (insetColour);
    g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));
    g.setColour (creamColour.withAlpha (0.12f));
    g.drawEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre),
                   thinStroke);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (creamColour.withAlpha (0.13f));
    g.strokePath (track, juce::PathStrokeType (trackStroke,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (position > 0.0f)
    {
        juce::Path valueTrack;
        valueTrack.addCentredArc (centre.x, centre.y, trackRadius, trackRadius, 0.0f,
                                  rotaryStartAngle, valueAngle, true);
        g.setColour (accentColour);
        g.strokePath (valueTrack, juce::PathStrokeType (activeStroke,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }

    g.setColour (creamColour);
    g.drawLine ({ polarPoint (centre, 44.0f * scale, valueAngle),
                  polarPoint (centre, 24.0f * scale, valueAngle) },
                pointerStroke);
}

juce::Slider::SliderLayout
TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    juce::Slider::SliderLayout layout;
    layout.sliderBounds = slider.getLocalBounds();
    return layout;
}

void TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::drawComboBox (
    juce::Graphics& g, int width, int height, bool, int, int, int, int,
    juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                                 static_cast<float> (width),
                                                 static_cast<float> (height));
    g.setColour (insetColour);
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accentColour.withAlpha (0.8f)
                                              : creamColour.withAlpha (0.16f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

    const auto centreY = static_cast<float> (height) * 0.5f;
    const auto arrowX = static_cast<float> (width - 13);
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - 3.0f, centreY - 1.5f);
    arrow.lineTo (arrowX, centreY + 1.5f);
    arrow.lineTo (arrowX + 3.0f, centreY - 1.5f);
    g.setColour (accentColour);
    g.strokePath (arrow, juce::PathStrokeType (1.0f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

juce::Font TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::getComboBoxFont (
    juce::ComboBox&)
{
    return makeFont (headingTypeface, 11.0f);
}

void TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::positionComboBoxText (
    juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setBorderSize (juce::BorderSize<int> (0));
}

juce::Font TB_ExciterAudioProcessorEditor::SparkleLookAndFeel::getPopupMenuFont()
{
    return makeFont (bodyTypeface, 11.0f);
}

TB_ExciterAudioProcessorEditor::SparkleField::SparkleField (
    juce::Slider& brightIn, juce::Slider& clarityIn, juce::Slider& focusIn,
    juce::Slider& widthIn, juce::Slider& levelIn)
    : bright (brightIn), clarity (clarityIn), focus (focusIn), width (widthIn), level (levelIn)
{
    setOpaque (false);
    setAccessible (false);
    setInterceptsMouseClicks (false, false);

    for (auto& particle : particles)
    {
        particle.velocity = 0.03f + random.nextFloat() * 0.07f;
        particle.size = 0.9f + random.nextFloat() * 1.9f;
        particle.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
        resetParticle (particle, 0.25f);
    }

    for (auto& point : corePoints)
    {
        point.angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        point.distance = 0.15f + random.nextFloat() * 0.85f;
        point.size = 1.0f + random.nextFloat() * 2.2f;
        point.phase = random.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    lastFrameMs = juce::Time::getMillisecondCounterHiRes();
    startTimer (activeIntervalMs);
}

TB_ExciterAudioProcessorEditor::SparkleField::~SparkleField()
{
    stopTimer();
}

float TB_ExciterAudioProcessorEditor::SparkleField::normalisedValue (
    juce::Slider& slider) const
{
    return juce::jlimit (0.0f, 1.0f,
                         static_cast<float> (slider.valueToProportionOfLength (slider.getValue())));
}

TB_ExciterAudioProcessorEditor::SparkleField::Values
TB_ExciterAudioProcessorEditor::SparkleField::readValues() const
{
    const auto levelIntensity = juce::jlimit (
        0.06f, 1.15f, std::pow (10.0f, static_cast<float> (level.getValue()) / 40.0f));

    return { normalisedValue (bright), normalisedValue (clarity), normalisedValue (focus),
             normalisedValue (width), levelIntensity };
}

void TB_ExciterAudioProcessorEditor::SparkleField::resetParticle (
    Particle& particle, float coreRadius)
{
    particle.angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
    particle.radius = coreRadius * (0.4f + random.nextFloat() * 0.9f);
    particle.angularVelocity = (random.nextFloat() - 0.5f) * 1.1f;
    particle.trailCount = 0;
}

void TB_ExciterAudioProcessorEditor::SparkleField::recordParticlePosition (
    Particle& particle)
{
    particle.trailHead = (particle.trailHead + 1) % Particle::trailCapacity;
    particle.trail[static_cast<size_t> (particle.trailHead)] = {
        std::cos (particle.angle) * particle.radius,
        std::sin (particle.angle) * particle.radius
    };
    particle.trailCount = juce::jmin (particle.trailCount + 1, Particle::trailCapacity);
}

void TB_ExciterAudioProcessorEditor::SparkleField::separateParticles (
    int particleCount, float coreRadius)
{
    std::array<juce::Point<float>, 196> positions;

    for (int i = 0; i < particleCount; ++i)
    {
        const auto& particle = particles[static_cast<size_t> (i)];
        positions[static_cast<size_t> (i)] = {
            std::cos (particle.angle) * particle.radius,
            std::sin (particle.angle) * particle.radius
        };
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < particleCount; ++i)
        {
            for (int j = i + 1; j < particleCount; ++j)
            {
                auto delta = positions[static_cast<size_t> (j)]
                             - positions[static_cast<size_t> (i)];
                auto distanceSquared = delta.getDistanceSquaredFromOrigin();
                const auto minimumDistance = 0.016f
                                             + 0.004f
                                                   * (particles[static_cast<size_t> (i)].size
                                                      + particles[static_cast<size_t> (j)].size);

                if (distanceSquared >= minimumDistance * minimumDistance)
                    continue;

                if (distanceSquared < 1.0e-8f)
                {
                    const auto angle = static_cast<float> ((i * 73 + j * 151) % 360)
                                       * juce::MathConstants<float>::pi / 180.0f;
                    delta = { std::cos (angle) * 0.001f, std::sin (angle) * 0.001f };
                    distanceSquared = delta.getDistanceSquaredFromOrigin();
                }

                const auto distance = std::sqrt (distanceSquared);
                const auto direction = delta / distance;
                const auto correction = direction * ((minimumDistance - distance) * 0.5f);
                positions[static_cast<size_t> (i)] = positions[static_cast<size_t> (i)]
                                                     - correction;
                positions[static_cast<size_t> (j)] = positions[static_cast<size_t> (j)]
                                                     + correction;
            }
        }
    }

    const auto minimumRadius = coreRadius * 0.35f;

    for (int i = 0; i < particleCount; ++i)
    {
        auto& particle = particles[static_cast<size_t> (i)];
        const auto position = positions[static_cast<size_t> (i)];
        const auto radius = position.getDistanceFromOrigin();

        if (radius > 1.0e-5f)
            particle.angle = std::atan2 (position.y, position.x);

        particle.radius = juce::jlimit (minimumRadius, 1.0f, radius);
    }
}

void TB_ExciterAudioProcessorEditor::SparkleField::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    /*  최소화되었거나 화면에 없는 동안에는 시뮬레이션과 repaint를 멈춘다. 호스트의
        가시성 신호를 항상 믿을 수는 없으므로 타이머를 끄지 않고 낮은 속도로 확인만 해,
        다시 보일 때 반드시 재개되게 한다 (성능 가이드 7.1, 7.2). */
    if (! isShowing())
    {
        if (getTimerInterval() != hiddenIntervalMs)
            startTimer (hiddenIntervalMs);

        lastFrameMs = now;
        return;
    }

    if (getTimerInterval() != activeIntervalMs)
        startTimer (activeIntervalMs);

    const auto deltaSeconds = juce::jlimit (0.0, 0.05, (now - lastFrameMs) * 0.001);
    lastFrameMs = now;
    elapsedSeconds += static_cast<float> (deltaSeconds);

    const auto values = readValues();
    const auto coreRadius = 0.05f + values.focus * 0.40f;
    const auto flow = 0.35f + values.bright * 1.4f;
    const auto particleCount = juce::jlimit (0, static_cast<int> (particles.size()),
                                             juce::roundToInt (6.0f + values.bright * 190.0f));

    for (int i = 0; i < particleCount; ++i)
    {
        auto& particle = particles[static_cast<size_t> (i)];
        particle.radius += particle.velocity * flow * static_cast<float> (deltaSeconds);
        particle.angle += particle.angularVelocity * values.width * 1.7f
                          * static_cast<float> (deltaSeconds);

        if (particle.radius > 1.02f)
            resetParticle (particle, coreRadius);
    }

    separateParticles (particleCount, coreRadius);

    for (int i = 0; i < particleCount; ++i)
        recordParticlePosition (particles[static_cast<size_t> (i)]);

    for (int i = particleCount; i < static_cast<int> (particles.size()); ++i)
        particles[static_cast<size_t> (i)].trailCount = 0;

    repaint();
}

void TB_ExciterAudioProcessorEditor::SparkleField::paint (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState savedState (g);
    const auto renderScale = juce::jmin (static_cast<float> (getWidth()) / fieldSize,
                                         static_cast<float> (getHeight()) / fieldSize);
    g.addTransform (juce::AffineTransform::scale (renderScale));

    const auto values = readValues();
    const juce::Point<float> centre { fieldRadius, fieldRadius };
    const auto coreRadius = 0.05f + values.focus * 0.40f;
    const auto brightness = values.level;
    constexpr auto twinkleRate = 4.0f;

    const auto particleCount = juce::jlimit (0, static_cast<int> (particles.size()),
                                             juce::roundToInt (6.0f + values.bright * 190.0f));

    for (int i = 0; i < particleCount; ++i)
    {
        const auto& particle = particles[static_cast<size_t> (i)];
        const auto point = centre + juce::Point<float> (std::cos (particle.angle) * particle.radius * fieldRadius,
                                                         std::sin (particle.angle) * particle.radius * fieldRadius);
        const auto edge = particle.radius > 0.9f
                              ? juce::jmax (0.0f, (1.02f - particle.radius) / 0.12f)
                              : 1.0f;
        const auto born = juce::jlimit (0.0f, 1.0f,
                                        (particle.radius - coreRadius * 0.35f) / 0.06f);
        const auto glint = 0.22f + 0.78f
                           * (0.5f + 0.5f * std::sin (elapsedSeconds * twinkleRate
                                                     + particle.phase));
        const auto baseAlpha = juce::jmin (1.0f, brightness * edge * born);
        const auto alpha = baseAlpha * glint;

        if (alpha <= 0.01f)
            continue;

        const auto size = particle.size * (0.65f + 0.6f * (1.0f - particle.radius)) * 1.45f;

        const auto trailSegments = juce::jmin (
            particle.trailCount - 1,
            juce::roundToInt (values.clarity * static_cast<float> (Particle::trailCapacity - 1)));

        if (particle.size > 1.8f && trailSegments > 0)
        {
            for (int age = trailSegments; age > 0; --age)
            {
                const auto olderIndex = (particle.trailHead - age + Particle::trailCapacity)
                                        % Particle::trailCapacity;
                const auto newerIndex = (olderIndex + 1) % Particle::trailCapacity;
                const auto older = centre + particle.trail[static_cast<size_t> (olderIndex)]
                                              * fieldRadius;
                const auto newer = centre + particle.trail[static_cast<size_t> (newerIndex)]
                                              * fieldRadius;
                const auto freshness = 1.0f - static_cast<float> (age - 1)
                                                  / static_cast<float> (trailSegments + 1);
                const auto trailAlpha = baseAlpha * values.clarity * 0.30f * freshness;

                g.setColour (accentColour.interpolatedWith (creamColour, freshness * 0.45f)
                                 .withAlpha (trailAlpha));
                g.drawLine ({ older, newer }, 1.0f + particle.size * 0.12f);
            }
        }

        const auto particleColour = particle.size > 2.2f
                                        ? accentColour.interpolatedWith (creamColour, 0.58f)
                                        : creamColour;
        g.setColour (particleColour.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (size * 2.0f, size * 2.0f).withCentre (point));
    }

    const auto clusterRadius = juce::jmax (14.0f, coreRadius * fieldRadius * 0.9f);
    juce::ColourGradient cluster (creamColour.withAlpha (juce::jmin (0.36f, 0.24f * brightness)),
                                  centre, accentColour.withAlpha (0.0f),
                                  centre + juce::Point<float> (clusterRadius, 0.0f), true);
    cluster.addColour (0.35, accentColour.withAlpha (juce::jmin (0.22f, 0.12f * brightness)));
    g.setGradientFill (cluster);
    g.fillEllipse (juce::Rectangle<float> (clusterRadius * 2.0f, clusterRadius * 2.0f)
                       .withCentre (centre));

    for (const auto& point : corePoints)
    {
        const auto distance = point.distance * coreRadius * fieldRadius * 0.55f;
        const auto angle = point.angle + elapsedSeconds * 0.12f;
        const auto alpha = juce::jmin (1.0f, brightness
                                               * (0.35f + 0.65f
                                                  * (0.5f + 0.5f
                                                     * std::sin (elapsedSeconds * twinkleRate * 0.6f
                                                                 + point.phase))));
        const auto position = centre + juce::Point<float> (std::cos (angle) * distance,
                                                            std::sin (angle) * distance);
        g.setColour (creamColour.withAlpha (alpha));
        g.fillEllipse (juce::Rectangle<float> (point.size * 2.8f, point.size * 2.8f)
                           .withCentre (position));
    }

    juce::Path focusRing;
    const auto focusRadius = coreRadius * fieldRadius;
    focusRing.addEllipse (juce::Rectangle<float> (focusRadius * 2.0f, focusRadius * 2.0f)
                              .withCentre (centre));
    juce::Path dashedFocusRing;
    const float dashLengths[] { 2.0f, 7.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashedFocusRing, focusRing,
                                                    dashLengths, 2);
    g.setColour (accentColour.withAlpha (0.42f));
    g.fillPath (dashedFocusRing);

    juce::ColourGradient outerRing (creamColour.withAlpha (0.30f), 0.0f, 0.0f,
                                    creamColour.withAlpha (0.18f), fieldSize, fieldSize, false);
    outerRing.addColour (0.5, accentColour.withAlpha (0.85f));
    g.setGradientFill (outerRing);
    g.drawEllipse (juce::Rectangle<float> ((fieldRadius - 3.0f) * 2.0f,
                                            (fieldRadius - 3.0f) * 2.0f)
                       .withCentre (centre),
                   1.2f);

    for (int i = 0; i < 72; ++i)
    {
        const auto angle = static_cast<float> (i) / 72.0f
                           * juce::MathConstants<float>::twoPi;
        const auto major = (i % 6) == 0;
        const auto outer = fieldRadius - 3.0f;
        const auto inner = outer - (major ? 11.0f : 5.0f);
        const auto start = centre + juce::Point<float> (std::cos (angle) * outer,
                                                         std::sin (angle) * outer);
        const auto end = centre + juce::Point<float> (std::cos (angle) * inner,
                                                       std::sin (angle) * inner);
        g.setColour (creamColour.withAlpha (major ? 0.30f : 0.13f));
        g.drawLine ({ start, end }, 1.0f);
    }
}

TB_ExciterAudioProcessorEditor::ActiveReadout::ActiveReadout (
    juce::Typeface::Ptr headingTypefaceIn)
    : headingTypeface (std::move (headingTypefaceIn))
{
    setOpaque (false);
    setAccessible (false);
    setInterceptsMouseClicks (false, false);
}

TB_ExciterAudioProcessorEditor::ActiveReadout::~ActiveReadout()
{
    if (activeSlider != nullptr)
        activeSlider->removeListener (this);
}

void TB_ExciterAudioProcessorEditor::ActiveReadout::setActive (
    juce::String name, juce::Slider& slider)
{
    if (activeSlider != &slider)
    {
        if (activeSlider != nullptr)
            activeSlider->removeListener (this);

        activeSlider = &slider;
        activeSlider->addListener (this);
    }

    activeName = std::move (name);
    valueText = slider.getTextFromValue (slider.getValue());
    repaint();
}

void TB_ExciterAudioProcessorEditor::ActiveReadout::sliderValueChanged (juce::Slider*)
{
    refreshValueText();
}

void TB_ExciterAudioProcessorEditor::ActiveReadout::refreshValueText()
{
    if (activeSlider == nullptr)
        return;

    auto text = activeSlider->getTextFromValue (activeSlider->getValue());

    // 표시 문자열이 같으면 다시 그리지 않는다.
    if (text != valueText)
    {
        valueText = std::move (text);
        repaint();
    }
}

void TB_ExciterAudioProcessorEditor::ActiveReadout::paint (juce::Graphics& g)
{
    juce::ColourGradient divider (creamColour.withAlpha (0.0f), 0.0f, 0.5f,
                                  creamColour.withAlpha (0.0f),
                                  static_cast<float> (getWidth()), 0.5f, false);
    divider.addColour (0.5, accentColour.withAlpha (0.55f));
    g.setGradientFill (divider);
    g.fillRect (0.0f, 0.0f, static_cast<float> (getWidth()), 1.0f);

    g.setColour (creamColour);
    g.setFont (makeFont (headingTypeface, 10.5f, 0.14f));
    g.drawFittedText (activeName, 0, 5, getWidth(), 13,
                      juce::Justification::centred, 1, 1.0f);

    g.setColour (accentColour);
    g.setFont (makeFont (headingTypeface, 20.0f));
    g.drawFittedText (valueText, 0, 20, getWidth(), 25,
                      juce::Justification::centred, 1, 1.0f);
}

TB_ExciterAudioProcessorEditor::TB_ExciterAudioProcessorEditor (
    TB_ExciterAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      headingTypeface (loadTypeface (BinaryData::CormorantGaramond_ttf,
                                     BinaryData::CormorantGaramond_ttfSize)),
      bodyTypeface (loadTypeface (BinaryData::Lora_ttf, BinaryData::Lora_ttfSize)),
      lookAndFeel (headingTypeface, bodyTypeface),
      sparkleField (amount.slider, character.slider, tune.slider, width.slider, output.slider),
      activeReadout (headingTypeface)
{
    setOpaque (true);
    setFocusContainerType (juce::Component::FocusContainerType::keyboardFocusContainer);

    addAndMakeVisible (sparkleField);
    addAndMakeVisible (activeReadout);

    addControl (amount, tbx::params::amountId, "BRIGHT", "Bright",
                "Controls the amount of high-frequency enhancement.",
                tbx::params::amountDefault);
    addControl (character, tbx::params::characterId, "CLARITY", "Clarity",
                "Moves the added high-frequency character from warm to clear.",
                tbx::params::characterDefault);
    addControl (tune, tbx::params::tuneId, "FOCUS", "Focus",
                "Selects where the high-frequency enhancement begins.",
                tbx::params::tuneDefaultHz);
    addControl (width, tbx::params::widthId, "WIDTH", "Width",
                "Controls the stereo width of the added high-frequency layer.",
                tbx::params::widthDefault);
    addControl (output, tbx::params::outputId, "LEVEL", "Level",
                "Adjusts the plugin output level.",
                tbx::params::outputDefaultDb);

    title.setText ("TB_EXCITER", juce::dontSendNotification);
    title.setFont (makeFont (headingTypeface, 20.0f, 0.05f));
    title.setColour (juce::Label::textColourId, creamColour);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setBorderSize (juce::BorderSize<int> (0));
    title.setInterceptsMouseClicks (false, false);
    title.setAccessible (false);
    addAndMakeVisible (title);

    provisionalNotice.setText ("DSP PROTOTYPE UI", juce::dontSendNotification);
    provisionalNotice.setFont (makeFont (bodyTypeface, 7.5f, 0.22f));
    provisionalNotice.setColour (juce::Label::textColourId, creamColour.withAlpha (0.42f));
    provisionalNotice.setJustificationType (juce::Justification::centredLeft);
    provisionalNotice.setBorderSize (juce::BorderSize<int> (0));
    provisionalNotice.setInterceptsMouseClicks (false, false);
    provisionalNotice.setAccessible (false);
    addAndMakeVisible (provisionalNotice);

    presetBox.setLookAndFeel (&lookAndFeel);
    presetBox.setName ("Preset");
    presetBox.setTitle ("Factory preset");
    presetBox.setDescription ("Selects one of the built-in TB Exciter settings.");
    presetBox.setTooltip ("Factory preset");
    presetBox.setWantsKeyboardFocus (true);
    presetBox.setEditableText (false);
    presetBox.setTextWhenNothingSelected ("Custom");
    populatePresetBox();
    presetBox.onChange = [this]
    {
        if (const auto presetId = presetBox.getSelectedId(); presetId > 0)
            processor.applyFactoryPreset (presetId);

        syncPresetDisplay();
    };
    addAndMakeVisible (presetBox);

    setActiveControl (amount);
    syncPresetDisplay();
    startTimerHz (10);
    setResizable (false, false);
    setSize (panelWidth, panelHeight);
}

TB_ExciterAudioProcessorEditor::~TB_ExciterAudioProcessorEditor()
{
    /*  멤버 슬라이더가 포커스를 가진 채 파괴되면 focusOfChildComponentChanged가 이미 일부
        파괴된 에디터에서 호출된다. 먼저 포커스를 내려놓고 콜백도 막는다. */
    tearingDown = true;

    if (hasKeyboardFocus (true))
        giveAwayKeyboardFocus();

    stopTimer();
    presetBox.onChange = nullptr;
    presetBox.setLookAndFeel (nullptr);

    Control* controls[] { &amount, &character, &tune, &width, &output };

    for (auto* control : controls)
    {
        control->slider.removeMouseListener (this);
        control->slider.setLookAndFeel (nullptr);
    }
}

void TB_ExciterAudioProcessorEditor::addControl (
    Control& control, const juce::String& parameterId, const juce::String& titleText,
    const juce::String& readoutText, const juce::String& description, double defaultValue)
{
    control.readoutName = readoutText;
    control.slider.setLookAndFeel (&lookAndFeel);
    control.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                        juce::MathConstants<float>::pi * 2.75f, true);
    control.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    control.slider.setMouseDragSensitivity (120);
    control.slider.setDoubleClickReturnValue (true, defaultValue);
    control.slider.setName (titleText);
    control.slider.setTitle (titleText);
    control.slider.setDescription (description);
    control.slider.setTooltip (description);
    control.slider.setWantsKeyboardFocus (true);
    control.slider.setAccessible (true);
    control.slider.addMouseListener (this, true);
    addAndMakeVisible (control.slider);

    control.label.setText (titleText, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centred);
    control.label.setFont (makeFont (bodyTypeface, 9.0f, 0.18f));
    control.label.setColour (juce::Label::textColourId, creamColour.withAlpha (0.55f));
    control.label.setInterceptsMouseClicks (false, false);
    control.label.setAccessible (false);
    addAndMakeVisible (control.label);

    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getValueTreeState(), parameterId, control.slider);
}

void TB_ExciterAudioProcessorEditor::setActiveControl (Control& control)
{
    activeReadout.setActive (control.readoutName, control.slider);
}

void TB_ExciterAudioProcessorEditor::populatePresetBox()
{
    juce::String currentCategory;

    for (const auto& preset : tbx::presets::factory)
    {
        if (currentCategory != preset.category)
        {
            currentCategory = preset.category;
            presetBox.addSectionHeading (currentCategory);
        }

        presetBox.addItem (preset.name, preset.id);
    }
}

void TB_ExciterAudioProcessorEditor::syncPresetDisplay()
{
    const auto presetId = processor.getActiveFactoryPresetId();
    const auto* preset = tbx::presets::findById (presetId);

    if (preset == nullptr)
    {
        if (presetBox.getText() != "Custom")
            presetBox.setText ("Custom", juce::dontSendNotification);

        return;
    }

    if (processor.matchesFactoryPreset (presetId))
    {
        if (presetBox.getSelectedId() != presetId)
            presetBox.setSelectedId (presetId, juce::dontSendNotification);
    }
    else
    {
        const auto dirtyName = juce::String (preset->name) + " *";

        if (presetBox.getText() != dirtyName)
            presetBox.setText (dirtyName, juce::dontSendNotification);
    }
}

void TB_ExciterAudioProcessorEditor::timerCallback()
{
    syncPresetDisplay();
}

TB_ExciterAudioProcessorEditor::Control*
TB_ExciterAudioProcessorEditor::findControlFor (const juce::Component* component)
{
    if (component == nullptr)
        return nullptr;

    Control* controls[] { &amount, &character, &tune, &width, &output };

    for (auto* control : controls)
        if (component == &control->slider || control->slider.isParentOf (component))
            return control;

    return nullptr;
}

/*  중앙 리드아웃은 "지금 조작 중인 노브"를 보여야 한다. 클릭뿐 아니라 휠과 키보드로
    조작할 때도 따라가게 한다. 그러지 않으면 FOCUS를 돌리는데 BRIGHT 값이 표시된다. */
void TB_ExciterAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    if (auto* control = findControlFor (event.eventComponent))
        setActiveControl (*control);
}

void TB_ExciterAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& event,
                                                     const juce::MouseWheelDetails& wheel)
{
    if (auto* control = findControlFor (event.eventComponent))
    {
        // 슬라이더가 이미 처리한 휠 이벤트의 listener 통지다. 위로 다시 전달하지 않는다.
        setActiveControl (*control);
        return;
    }

    AudioProcessorEditor::mouseWheelMove (event, wheel);
}

void TB_ExciterAudioProcessorEditor::focusOfChildComponentChanged (FocusChangeType)
{
    if (tearingDown)
        return;

    Control* controls[] { &amount, &character, &tune, &width, &output };

    for (auto* control : controls)
        if (control->slider.hasKeyboardFocus (true))
        {
            setActiveControl (*control);
            break;
        }
}

void TB_ExciterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (panelColour);
    g.setColour (creamColour.withAlpha (0.09f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 2.0f, 1.0f);

    g.setColour (creamColour.withAlpha (0.22f));
    g.drawLine (11.0f, 11.0f, 29.0f, 11.0f, 1.0f);
    g.drawLine (11.0f, 11.0f, 11.0f, 29.0f, 1.0f);
    g.drawLine (561.0f, 11.0f, 579.0f, 11.0f, 1.0f);
    g.drawLine (579.0f, 11.0f, 579.0f, 29.0f, 1.0f);
    g.drawLine (11.0f, 421.0f, 11.0f, 439.0f, 1.0f);
    g.drawLine (11.0f, 439.0f, 29.0f, 439.0f, 1.0f);
    g.drawLine (579.0f, 421.0f, 579.0f, 439.0f, 1.0f);
    g.drawLine (561.0f, 439.0f, 579.0f, 439.0f, 1.0f);

    {
        juce::Graphics::ScopedSaveState savedState (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi,
                                                          15.0f, 225.0f));
        g.setColour (accentColour.withAlpha (0.75f));
        g.setFont (makeFont (bodyTypeface, 7.0f, 0.20f));
        g.drawFittedText ("SPARKLE FIELD", -40, 221, 110, 8,
                          juce::Justification::centred, 1, 1.0f);
    }

    g.setColour (creamColour.withAlpha (0.35f));
    g.setFont (makeFont (bodyTypeface, 7.0f, 0.16f));
    const auto dragHint = "DRAG " + juce::String::charToString (0x2195) + " TO SET";
    g.drawFittedText (dragHint, 430, 418, 124, 12,
                      juce::Justification::centredRight, 1, 1.0f);
    g.setColour (accentColour);
    g.fillRect (560, 421, 4, 4);
}

void TB_ExciterAudioProcessorEditor::layoutControl (Control& control, int x, int y)
{
    control.slider.setBounds (x + 9, y + 15, 72, 72);
    control.label.setBounds (x + 9, y + 91, 72, 12);
}

void TB_ExciterAudioProcessorEditor::resized()
{
    sparkleField.setBounds (160, 100, 270, 270);
    activeReadout.setBounds (200, 307, 190, 48);

    title.setBounds (26, 17, 190, 26);
    provisionalNotice.setBounds (26, 41, 170, 11);
    presetBox.setBounds (26, 57, 150, 22);

    layoutControl (amount, 250, -5);
    layoutControl (character, 421, 129);
    layoutControl (tune, 356, 331);
    layoutControl (width, 144, 331);
    layoutControl (output, 79, 129);
}
