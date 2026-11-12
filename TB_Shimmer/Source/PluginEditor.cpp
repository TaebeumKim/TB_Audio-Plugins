#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace palette
{
const auto canvasTop = juce::Colour (0xff30343b);
const auto canvasBottom = juce::Colour (0xff101217);
const auto surfaceTop = juce::Colour (0xff282d35);
const auto surfaceBottom = juce::Colour (0xff171b22);
const auto displayTop = juce::Colour (0xff18202a);
const auto displayBottom = juce::Colour (0xff090c12);
const auto textPrimary = juce::Colour (0xffeef3f7);
const auto textSecondary = juce::Colour (0xffaeb8c4);
const auto valueText = juce::Colour (0xffdceaf2);
const auto accentPrimary = juce::Colour (0xffbcebff);
const auto accentSecondary = juce::Colour (0xfff7c5ff);
const auto sectionAccent = juce::Colour (0xff7e769e);
const auto outline = juce::Colour (0xff4a5260);
const auto deepShadow = juce::Colour (0xff05070a);
const auto highlight = juce::Colour (0xff697381);
}

namespace
{
constexpr auto headerHeight = 52;
constexpr auto userMenuBase = 1000;
constexpr auto controlColumns = 8;
constexpr auto controlWidth = 88;
constexpr auto controlPitch = 91;
constexpr auto firstControlX = 18;
constexpr auto maximumShardGlints = 160;
constexpr std::array<std::array<float, 3>, 9> pitchTints
{{
    {{ 0.66f, 0.79f, 1.00f }}, {{ 0.76f, 0.86f, 1.00f }},
    {{ 0.86f, 0.93f, 1.00f }}, {{ 0.94f, 0.97f, 1.00f }},
    {{ 1.00f, 1.00f, 1.00f }}, {{ 1.00f, 0.96f, 0.93f }},
    {{ 1.00f, 0.89f, 0.83f }}, {{ 1.00f, 0.81f, 0.72f }},
    {{ 1.00f, 0.74f, 0.63f }}
}};

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour (palette::deepShadow.withAlpha (0.78f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), radius);

    juce::ColourGradient face (palette::surfaceTop, bounds.getX(), bounds.getY(),
                               palette::surfaceBottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (bounds, radius, 1.0f);
    g.setColour (palette::highlight.withAlpha (0.18f));
    g.drawHorizontalLine (juce::roundToInt (bounds.getY() + 1.0f),
                          bounds.getX() + radius, bounds.getRight() - radius);
}

void drawRackScrew (juce::Graphics& g, juce::Point<float> centre)
{
    const auto screw = juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre);
    g.setColour (palette::deepShadow.withAlpha (0.85f));
    g.fillEllipse (screw.translated (0.0f, 1.0f));
    juce::ColourGradient metal (palette::highlight.withAlpha (0.58f),
                                screw.getX(), screw.getY(),
                                palette::surfaceBottom, screw.getRight(), screw.getBottom(), false);
    g.setGradientFill (metal);
    g.fillEllipse (screw);
    g.setColour (palette::deepShadow.withAlpha (0.88f));
    g.drawLine (centre.x - 1.8f, centre.y + 1.3f,
                centre.x + 1.8f, centre.y - 1.3f, 0.8f);
}

float hashUnit (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float> (value & 0x00ffffffu) / 16777215.0f;
}

// A four-point glint: long vertical/horizontal spikes with a pinched waist.
juce::Path makeGlintPath (juce::Point<float> centre, float radius, float rotation)
{
    juce::Path path;
    for (auto corner = 0; corner < 8; ++corner)
    {
        const auto angle = rotation + static_cast<float> (corner)
                                      * juce::MathConstants<float>::pi * 0.25f;
        const auto reach = (corner % 2) != 0 ? radius * 0.15f
                                             : ((corner % 4) == 0 ? radius
                                                                  : radius * 0.72f);
        const auto point = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                    * reach;
        if (corner == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }
    path.closeSubPath();
    return path;
}

juce::String parseableNumber (juce::String text)
{
    return text.trim().replaceCharacter (',', '.').retainCharacters ("+-0123456789.");
}
}

void TBShimmerAudioProcessorEditor::ShimmerLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
    float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                 static_cast<float> (width), static_cast<float> (height));
    constexpr auto radius = 22.0f;
    const auto centre = bounds.getCentre().translated (0.0f, -1.0f);
    const auto knob = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    const auto angle = juce::jmap (sliderPos, rotaryStartAngle, rotaryEndAngle);
    const auto opalControl = slider.getName() == "REVERSE" || slider.getName() == "FEEDBACK"
                          || slider.getName() == "SPARKLE" || slider.getName() == "SHATTER";
    const auto accent = opalControl ? palette::accentSecondary : palette::accentPrimary;

    g.setColour (palette::deepShadow.withAlpha (0.55f));
    g.fillEllipse (knob.expanded (1.5f).translated (0.0f, 2.5f));

    juce::ColourGradient face (juce::Colour (0xff1a1f25), centre.x, centre.y - radius,
                               juce::Colour (0xff0a0c10), centre.x, centre.y + radius, false);
    face.addColour (0.55, juce::Colour (0xff12161b));
    g.setGradientFill (face);
    g.fillEllipse (knob);

    g.setColour (palette::highlight.withAlpha (0.32f));
    g.drawEllipse (knob.reduced (0.5f), 1.0f);

    juce::Path lowerSheen;
    lowerSheen.addCentredArc (centre.x, centre.y, radius - 2.6f, radius - 2.6f,
                              0.0f, juce::MathConstants<float>::pi * 1.12f,
                              juce::MathConstants<float>::pi * 1.88f, true);
    g.setColour (palette::highlight.withAlpha (0.16f));
    g.strokePath (lowerSheen, juce::PathStrokeType (1.0f));

    const auto indicatorStart = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                         * (radius * 0.30f);
    const auto indicatorEnd = centre + juce::Point<float> (std::sin (angle), -std::cos (angle))
                                       * (radius - 4.5f);

    juce::Path indicatorShadow;
    indicatorShadow.startNewSubPath (indicatorStart.translated (0.6f, 0.9f));
    indicatorShadow.lineTo (indicatorEnd.translated (0.6f, 0.9f));
    g.setColour (palette::deepShadow.withAlpha (0.85f));
    g.strokePath (indicatorShadow, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    juce::Path indicator;
    indicator.startNewSubPath (indicatorStart);
    indicator.lineTo (indicatorEnd);
    g.setColour (palette::valueText.withAlpha (0.96f));
    g.strokePath (indicator, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    if (slider.isMouseOverOrDragging() || slider.hasKeyboardFocus (true))
    {
        g.setColour (accent.withAlpha (0.42f));
        g.drawEllipse (knob.expanded (2.5f), 1.0f);
    }
}

juce::Label* TBShimmerAudioProcessorEditor::ShimmerLookAndFeel::createSliderTextBox (
    juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::Font (juce::FontOptions (10.6f)));
    label->setMinimumHorizontalScale (0.90f);
    return label;
}

void TBShimmerAudioProcessorEditor::ShimmerLookAndFeel::drawComboBox (
    juce::Graphics& g, int width, int height, bool isButtonDown,
    int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                          static_cast<float> (width) - 1.0f,
                                          static_cast<float> (height) - 1.0f);
    const auto hovered = box.isMouseOverOrDragging();
    const auto face = isButtonDown ? palette::displayBottom
                                   : (hovered ? palette::surfaceTop.brighter (0.08f)
                                              : palette::surfaceBottom);

    g.setColour (palette::deepShadow.withAlpha (0.72f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 4.0f);
    g.setColour (face);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (hovered ? palette::accentSecondary.withAlpha (0.72f) : palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto arrowX = bounds.getRight() - 10.0f;
    const auto arrowY = bounds.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - 3.0f, arrowY - 1.5f);
    arrow.lineTo (arrowX, arrowY + 2.0f);
    arrow.lineTo (arrowX + 3.0f, arrowY - 1.5f);
    g.setColour (box.isEnabled() ? palette::textSecondary
                                 : palette::textSecondary.withAlpha (0.3f));
    g.strokePath (arrow, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

juce::Font TBShimmerAudioProcessorEditor::ShimmerLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (9.2f, juce::Font::bold));
}

void TBShimmerAudioProcessorEditor::ShimmerLookAndFeel::positionComboBoxText (
    juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

TBShimmerAudioProcessorEditor::HeaderButton::HeaderButton (const juce::String& name,
                                                            Kind buttonKind)
    : Button (name), kind (buttonKind)
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void TBShimmerAudioProcessorEditor::HeaderButton::paintButton (
    juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto base = down ? palette::displayBottom
                           : (highlighted ? palette::surfaceTop.brighter (0.08f)
                                          : palette::surfaceBottom);
    g.setColour (palette::deepShadow.withAlpha (0.68f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f), 4.0f);
    g.setColour (base);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (highlighted ? palette::accentPrimary.withAlpha (0.72f) : palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    g.setColour (isEnabled() ? palette::textSecondary
                             : palette::textSecondary.withAlpha (0.28f));
    const auto centre = bounds.getCentre();

    if (kind == Kind::previous || kind == Kind::next)
    {
        const auto direction = kind == Kind::previous ? 1.0f : -1.0f;
        juce::Path chevron;
        chevron.startNewSubPath (centre.x + direction * 3.5f, centre.y - 5.0f);
        chevron.lineTo (centre.x - direction * 2.5f, centre.y);
        chevron.lineTo (centre.x + direction * 3.5f, centre.y + 5.0f);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        return;
    }

    if (kind == Kind::save)
    {
        const auto disk = juce::Rectangle<float> (13.0f, 14.0f).withCentre (centre);
        juce::Path body;
        body.startNewSubPath (disk.getX(), disk.getY());
        body.lineTo (disk.getRight() - 3.0f, disk.getY());
        body.lineTo (disk.getRight(), disk.getY() + 3.0f);
        body.lineTo (disk.getRight(), disk.getBottom());
        body.lineTo (disk.getX(), disk.getBottom());
        body.closeSubPath();
        g.strokePath (body, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.drawRect (disk.getX() + 2.4f, disk.getY() + 1.5f, 6.1f, 4.0f, 1.0f);
        g.drawRoundedRectangle ({ disk.getX() + 2.5f, disk.getY() + 8.0f, 8.0f, 4.5f },
                                0.7f, 1.0f);
        return;
    }

    const auto pointsLeft = kind == Kind::undo;
    const auto direction = pointsLeft ? -1.0f : 1.0f;
    juce::Path arrow;
    arrow.startNewSubPath (centre.x + direction * 6.0f, centre.y + 5.0f);
    arrow.cubicTo (centre.x - direction * 5.0f, centre.y + 6.0f,
                   centre.x - direction * 7.0f, centre.y - 5.0f,
                   centre.x + direction * 2.0f, centre.y - 5.0f);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    juce::Path head;
    head.startNewSubPath (centre.x + direction * 2.0f, centre.y - 9.0f);
    head.lineTo (centre.x + direction * 8.0f, centre.y - 5.0f);
    head.lineTo (centre.x + direction * 2.0f, centre.y - 1.0f);
    g.strokePath (head, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

TBShimmerAudioProcessorEditor::TBShimmerAudioProcessorEditor (TBShimmerAudioProcessor& processor)
    : AudioProcessorEditor (&processor),
      audioProcessor (processor),
      effectLogo (juce::ImageCache::getFromMemory (BinaryData::tb_effect_logo_png,
                                                   BinaryData::tb_effect_logo_pngSize)),
      prismGlass (juce::ImageCache::getFromMemory (BinaryData::shimmer_prism_glass_png,
                                                   BinaryData::shimmer_prism_glass_pngSize))
{
    setOpaque (true);
    setWantsKeyboardFocus (false);
    setLookAndFeel (&lookAndFeel);

    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, palette::surfaceBottom);
    lookAndFeel.setColour (juce::PopupMenu::textColourId, palette::textPrimary);
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId,
                           palette::sectionAccent.withAlpha (0.76f));
    lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, palette::textPrimary);
    lookAndFeel.setColour (juce::ComboBox::textColourId, palette::textSecondary);
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, palette::surfaceBottom);
    lookAndFeel.setColour (juce::ComboBox::outlineColourId, palette::outline);
    buildGrainHaloSprites();

    configureKnob (timeSlider, timeLabel, "TIME",
                   "Sets how far behind the input each grain begins.");
    configureKnob (sizeSlider, sizeLabel, "SIZE",
                   "Sets grain duration and the scale of the diffuse space.");
    configureKnob (densitySlider, densityLabel, "DENSITY",
                   "Moves from sparse grain echoes to a smooth, dense wash.");
    configureKnob (scatterSlider, scatterLabel, "SCATTER",
                   "Spreads grains and adds slow motion inside the diffuse tail.");
    configureKnob (pitchSlider, pitchLabel, "PITCH",
                   "Transposes the grain cloud in semitones.");
    configureKnob (reverseSlider, reverseLabel, "REVERSE",
                   "Sets the chance that a grain reads backward.");
    configureKnob (feedbackSlider, feedbackLabel, "FEEDBACK",
                   "Extends both rising grain generations and the diffuse decay.");
    configureKnob (toneSlider, toneLabel, "TONE",
                   "Damps each tail circulation and sets the wet brightness.");
    configureKnob (widthSlider, widthLabel, "WIDTH",
                   "Sets the stereo spread of the combined grain and tail field.");
    configureKnob (mixSlider, mixLabel, "MIX",
                   "Blends the dry input with the granular shimmer and diffuse tail.");
    configureKnob (levelVariationSlider, levelVariationLabel, "LEVEL VAR",
                   "Varies each grain level downward by up to the selected amount.");
    configureKnob (positionRandomSlider, positionRandomLabel, "POSITION RAND",
                   "Randomises each grain's read-start position around Time.");
    configureKnob (pitchSpraySlider, pitchSprayLabel, "PITCH SPRAY",
                   "Spreads each grain continuously around the main Pitch setting.");
    configureKnob (freezeSlider, freezeLabel, "FREEZE",
                   "Holds the captured history so grains keep replaying it.");
    configureKnob (sparkleSlider, sparkleLabel, "SPARKLE",
                   "Adds glittering shards of the input that echo at Time.");
    configureKnob (shatterSlider, shatterLabel, "SHATTER",
                   "Turns the even glitter into bursts that break off each attack.");
    sparkleSlider.setComponentID ("sparkleSlider");
    shatterSlider.setComponentID ("shatterSlider");
    timeSlider.setComponentID ("timeSlider");
    sizeSlider.setComponentID ("sizeSlider");
    densitySlider.setComponentID ("densitySlider");
    scatterSlider.setComponentID ("scatterSlider");
    pitchSlider.setComponentID ("pitchSlider");
    reverseSlider.setComponentID ("reverseSlider");
    feedbackSlider.setComponentID ("feedbackSlider");
    toneSlider.setComponentID ("toneSlider");
    widthSlider.setComponentID ("widthSlider");
    mixSlider.setComponentID ("mixSlider");
    levelVariationSlider.setComponentID ("levelVariationSlider");
    positionRandomSlider.setComponentID ("positionRandomSlider");
    pitchSpraySlider.setComponentID ("pitchSpraySlider");
    freezeSlider.setComponentID ("freezeSlider");

    auto& state = audioProcessor.getValueTreeState();
    timeAttachment = std::make_unique<SliderAttachment> (state, "time", timeSlider);
    sizeAttachment = std::make_unique<SliderAttachment> (state, "size", sizeSlider);
    densityAttachment = std::make_unique<SliderAttachment> (state, "density", densitySlider);
    scatterAttachment = std::make_unique<SliderAttachment> (state, "scatter", scatterSlider);
    pitchAttachment = std::make_unique<SliderAttachment> (state, "pitch", pitchSlider);
    reverseAttachment = std::make_unique<SliderAttachment> (state, "reverse", reverseSlider);
    feedbackAttachment = std::make_unique<SliderAttachment> (state, "feedback", feedbackSlider);
    toneAttachment = std::make_unique<SliderAttachment> (state, "tone", toneSlider);
    widthAttachment = std::make_unique<SliderAttachment> (state, "width", widthSlider);
    mixAttachment = std::make_unique<SliderAttachment> (state, "mix", mixSlider);
    levelVariationAttachment = std::make_unique<SliderAttachment> (
        state, "level_var", levelVariationSlider);
    positionRandomAttachment = std::make_unique<SliderAttachment> (
        state, "position_rand", positionRandomSlider);
    pitchSprayAttachment = std::make_unique<SliderAttachment> (
        state, "pitch_spray", pitchSpraySlider);
    freezeAttachment = std::make_unique<SliderAttachment> (
        state, "freeze", freezeSlider);
    sparkleAttachment = std::make_unique<SliderAttachment> (
        state, "sparkle", sparkleSlider);
    shatterAttachment = std::make_unique<SliderAttachment> (
        state, "shatter", shatterSlider);

    freezeSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value >= 0.5 ? "ON" : "OFF");
    };
    freezeSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.trim().equalsIgnoreCase ("ON") ? 1.0 : 0.0;
    };
    freezeSlider.setMouseDragSensitivity (40);

    timeSlider.textFromValueFunction = [] (double value) { return formatMilliseconds (value); };
    sizeSlider.textFromValueFunction = timeSlider.textFromValueFunction;
    densitySlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " gr/s";
    };
    scatterSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 0) + " %";
    };
    pitchSlider.textFromValueFunction = [] (double value) { return formatSignedSemitones (value); };
    reverseSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    feedbackSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    toneSlider.textFromValueFunction = [] (double value) { return formatFrequency (value); };
    widthSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    mixSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    sparkleSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    shatterSlider.textFromValueFunction = scatterSlider.textFromValueFunction;
    levelVariationSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " dB";
    };
    positionRandomSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " %";
    };
    pitchSpraySlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " st";
    };

    timeSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return parseableNumber (text).getDoubleValue();
    };
    sizeSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    densitySlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    scatterSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    pitchSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    reverseSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    feedbackSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    toneSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        auto value = text.trim().toLowerCase().replaceCharacter (',', '.');
        const auto multiplier = value.containsChar ('k') ? 1000.0 : 1.0;
        return parseableNumber (value).getDoubleValue() * multiplier;
    };
    widthSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    mixSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    sparkleSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    shatterSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    levelVariationSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    positionRandomSlider.valueFromTextFunction = timeSlider.valueFromTextFunction;
    pitchSpraySlider.valueFromTextFunction = timeSlider.valueFromTextFunction;

    const std::array<juce::Slider*, 15> visualFocusOrder
    {{
        &timeSlider, &sizeSlider, &densitySlider, &scatterSlider,
        &levelVariationSlider, &positionRandomSlider, &pitchSpraySlider,
        &reverseSlider,
        &pitchSlider, &feedbackSlider, &toneSlider,
        &sparkleSlider, &shatterSlider, &widthSlider, &mixSlider
    }};
    for (size_t index = 0; index < visualFocusOrder.size(); ++index)
        visualFocusOrder[index]->setExplicitFocusOrder (static_cast<int> (index + 1));

    for (auto* slider : getSliders())
    {
        slider->updateText();
        slider->onValueChange = [this] { handleParameterVisualChange(); };
    }
    previousButton.setTooltip ("Load the previous Factory or User preset.");
    presetBox.setTooltip ("Browse Factory groups and User presets.");
    nextButton.setTooltip ("Load the next Factory or User preset.");
    saveButton.setTooltip ("Save the current settings as a User preset.");
    undoButton.setTooltip ("Undo the last parameter change.");
    redoButton.setTooltip ("Redo the last undone parameter change.");

    previousButton.setRepeatSpeed (520, 160);
    nextButton.setRepeatSpeed (520, 160);
    previousButton.onClick = [this] { cyclePreset (-1); };
    nextButton.onClick = [this] { cyclePreset (1); };
    saveButton.onClick = [this] { saveCurrentPreset(); };
    undoButton.onClick = [this]
    {
        audioProcessor.getUndoManager().undo();
        refreshPresetDisplay();
        refreshCommandState();
    };
    redoButton.onClick = [this]
    {
        audioProcessor.getUndoManager().redo();
        refreshPresetDisplay();
        refreshCommandState();
    };
    presetBox.setWantsKeyboardFocus (false);
    presetBox.setMouseClickGrabsKeyboardFocus (false);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this] { handlePresetSelection(); };

    addAndMakeVisible (previousButton);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    const auto directoryResult = audioProcessor.getUserPresetDirectory().createDirectory();
    refreshUserPresetFiles();
    rebuildPresetMenu();
    refreshPresetDisplay();
    refreshCommandState();

    if (directoryResult.failed())
    {
        auto safeThis = juce::Component::SafePointer<TBShimmerAudioProcessorEditor> (this);
        juce::MessageManager::callAsync ([safeThis, message = directoryResult.getErrorMessage()]
        {
            if (safeThis != nullptr)
                safeThis->showPresetError (message);
        });
    }

    setResizable (false, false);
    setSize (760, 430);
    seedGrainField();
    seedShardGlints();
    lastShardSpawnCount = audioProcessor.getShardSpawnCount();
    lastVisualUpdateMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

TBShimmerAudioProcessorEditor::~TBShimmerAudioProcessorEditor()
{
    stopTimer();
    ++toastGeneration;
    presetBox.hidePopup();
    setLookAndFeel (nullptr);
}

void TBShimmerAudioProcessorEditor::configureKnob (
    juce::Slider& slider, juce::Label& label, const juce::String& name,
    const juce::String& tooltip)
{
    slider.setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, controlWidth - 2, 16);
    slider.setColour (juce::Slider::textBoxTextColourId, palette::valueText.withAlpha (0.94f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxHighlightColourId,
                      palette::sectionAccent.withAlpha (0.58f));
    slider.setMouseDragSensitivity (180);
    slider.setWantsKeyboardFocus (true);
    slider.setMouseClickGrabsKeyboardFocus (true);
    slider.setTitle (name);
    slider.setDescription (tooltip);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, palette::textSecondary.withAlpha (0.82f));
    label.setFont (juce::Font (juce::FontOptions (8.1f, juce::Font::bold)));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

std::array<juce::Slider*, 16> TBShimmerAudioProcessorEditor::getSliders() noexcept
{
    return { &timeSlider, &sizeSlider, &densitySlider, &scatterSlider,
             &levelVariationSlider, &positionRandomSlider, &pitchSpraySlider,
             &reverseSlider,
             &pitchSlider, &feedbackSlider, &toneSlider, &sparkleSlider,
             &shatterSlider, &widthSlider, &mixSlider, &freezeSlider };
}

std::array<juce::Label*, 16> TBShimmerAudioProcessorEditor::getLabels() noexcept
{
    return { &timeLabel, &sizeLabel, &densityLabel, &scatterLabel,
             &levelVariationLabel, &positionRandomLabel, &pitchSprayLabel,
             &reverseLabel,
             &pitchLabel, &feedbackLabel, &toneLabel, &sparkleLabel,
             &shatterLabel, &widthLabel, &mixLabel, &freezeLabel };
}

juce::String TBShimmerAudioProcessorEditor::formatMilliseconds (double value)
{
    return juce::String (value, 0) + " ms";
}

juce::String TBShimmerAudioProcessorEditor::formatFrequency (double value)
{
    if (value >= 1000.0)
        return juce::String (value / 1000.0, value < 10000.0 ? 1 : 0) + " kHz";
    return juce::String (value, 0) + " Hz";
}

juce::String TBShimmerAudioProcessorEditor::formatSignedSemitones (double value)
{
    return juce::String (value > 0.0 ? "+" : "") + juce::String (value, 2) + " st";
}

void TBShimmerAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient chassis (palette::canvasTop, 0.0f, 0.0f,
                                  palette::canvasBottom, 0.0f,
                                  static_cast<float> (getHeight()), false);
    g.setGradientFill (chassis);
    g.fillAll();

    g.setColour (palette::deepShadow.withAlpha (0.64f));
    g.fillRect (0, 0, getWidth(), headerHeight);
    g.setColour (palette::highlight.withAlpha (0.19f));
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));
    g.setColour (palette::outline.withAlpha (0.92f));
    g.drawHorizontalLine (headerHeight - 1, 0.0f, static_cast<float> (getWidth()));

    drawRackScrew (g, { 8.0f, 26.0f });
    drawRackScrew (g, { 752.0f, 26.0f });

    g.setColour (palette::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawFittedText ("TB SHIMMER", 23, 10, 178, 32,
                      juce::Justification::centredLeft, 1, 0.88f);

    if (effectLogo.isValid())
    {
        juce::Graphics::ScopedSaveState imageState (g);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        constexpr std::array<float, 8> directionX
        {{ 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f, -1.0f, -0.7071f }};
        constexpr std::array<float, 8> directionY
        {{ -1.0f, -0.7071f, 0.0f, 0.7071f, 1.0f, 0.7071f, 0.0f, -0.7071f }};
        constexpr auto logoCentreX = 707.0f;
        constexpr auto logoCentreY = 26.0f;

        const auto drawCloneRing = [&] (float horizontalRadius, float verticalRadius,
                                        float imageSize, float opacity)
        {
            g.setOpacity (opacity);
            for (size_t direction = 0; direction < directionX.size(); ++direction)
                g.drawImageWithin (
                    effectLogo,
                    juce::roundToInt (logoCentreX + directionX[direction] * horizontalRadius
                                      - imageSize * 0.5f),
                    juce::roundToInt (logoCentreY + directionY[direction] * verticalRadius
                                      - imageSize * 0.5f),
                    juce::roundToInt (imageSize), juce::roundToInt (imageSize),
                    juce::RectanglePlacement::centred, false);
        };

        drawCloneRing (38.0f, 22.0f, 10.0f, 0.09f);
        drawCloneRing (27.0f, 16.0f, 15.0f, 0.22f);

        g.setOpacity (1.0f);
        g.drawImageWithin (effectLogo, 688, 7, 38, 38,
                           juce::RectanglePlacement::centred, false);
    }

    drawGrainFieldDisplay (g, { 14.0f, 60.0f, 732.0f, 132.0f });

    const auto controls = juce::Rectangle<float> (14.0f, 202.0f, 732.0f, 214.0f);
    drawPanel (g, controls, 7.0f);

    g.setColour (palette::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
    g.drawText ("GRAIN, MOTION & RANDOM", 27, 205, 166, 15,
                juce::Justification::centredLeft);
    g.drawText ("SHIMMER, SPARKLE & OUTPUT", 27, 311, 190, 15,
                juce::Justification::centredLeft);

    g.setColour (palette::sectionAccent.withAlpha (0.45f));
    g.drawHorizontalLine (218, 196.0f, 732.0f);
    g.drawHorizontalLine (324, 196.0f, 732.0f);
    g.setColour (palette::outline.withAlpha (0.60f));
    g.drawHorizontalLine (308, 25.0f, 735.0f);

    if (toastMessage.isNotEmpty())
    {
        const auto toast = juce::Rectangle<float> (532.0f, 68.0f, 195.0f, 22.0f);
        g.setColour (palette::displayBottom.withAlpha (0.95f));
        g.fillRoundedRectangle (toast, 4.0f);
        g.setColour (palette::accentSecondary.withAlpha (0.86f));
        g.drawRoundedRectangle (toast, 4.0f, 1.0f);
        g.setColour (palette::textPrimary);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawFittedText (toastMessage, toast.toNearestInt().reduced (8, 1),
                          juce::Justification::centred, 1, 0.84f);
    }
}

void TBShimmerAudioProcessorEditor::drawGrainFieldDisplay (
    juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (palette::deepShadow.withAlpha (0.92f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 8.0f);

    juce::ColourGradient bezel (juce::Colour (0xff454c55), bounds.getX(), bounds.getY(),
                                palette::deepShadow, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (palette::outline);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    const auto screen = bounds.reduced (7.0f);
    juce::ColourGradient glass (palette::displayTop, screen.getX(), screen.getY(),
                                palette::displayBottom, screen.getX(), screen.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (screen, 4.0f);

    if (prismGlass.isValid())
    {
        juce::Graphics::ScopedSaveState imageState (g);
        g.reduceClipRegion (screen.toNearestInt());
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (0.30f);
        const auto imageScale = juce::jmax (
            screen.getWidth() / static_cast<float> (prismGlass.getWidth()),
            screen.getHeight() / static_cast<float> (prismGlass.getHeight()));
        const auto imageWidth = static_cast<float> (prismGlass.getWidth()) * imageScale;
        const auto imageHeight = static_cast<float> (prismGlass.getHeight()) * imageScale;
        g.drawImage (prismGlass,
                     juce::roundToInt (screen.getCentreX() - imageWidth * 0.5f),
                     juce::roundToInt (screen.getCentreY() - imageHeight * 0.5f),
                     juce::roundToInt (imageWidth), juce::roundToInt (imageHeight),
                     0, 0, prismGlass.getWidth(), prismGlass.getHeight());
    }

    g.setColour (palette::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (8.6f, juce::Font::bold)));
    g.drawText ("GRAIN HALO", juce::roundToInt (screen.getX() + 10.0f),
                juce::roundToInt (screen.getY() + 4.0f), 112, 15,
                juce::Justification::centredLeft);

    const auto summary = formatSignedSemitones (pitchSlider.getValue()) + "  ·  "
                       + juce::String (densitySlider.getValue(), 1) + " gr/s  ·  "
                       + formatMilliseconds (timeSlider.getValue());
    g.setColour (palette::valueText.withAlpha (0.88f));
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    g.drawFittedText (summary, juce::roundToInt (screen.getRight() - 245.0f),
                      juce::roundToInt (screen.getY() + 4.0f), 235, 15,
                      juce::Justification::centredRight, 1, 0.86f);

    const auto plot = juce::Rectangle<float> (screen.getX() + 12.0f, screen.getY() + 24.0f,
                                               screen.getWidth() - 24.0f,
                                               screen.getHeight() - 33.0f);
    const auto sizeNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> ((sizeSlider.getValue() - 40.0) / 460.0));
    const auto scatterNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (scatterSlider.getValue() * 0.01));
    const auto pitchNormal = juce::jlimit (-1.0f, 1.0f,
        static_cast<float> (pitchSlider.getValue() / (pitchSlider.getValue() >= 0.0 ? 24.0 : 12.0)));
    const auto feedbackNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (feedbackSlider.getValue() / 88.0));
    const auto toneNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> ((toneSlider.getValue() - 1200.0) / 16800.0));
    const auto widthNormal = juce::jlimit (0.0f, 2.0f,
        static_cast<float> (widthSlider.getValue() * 0.01));
    const auto mixNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (mixSlider.getValue() * 0.01));

    const auto tintIndex = juce::jlimit (0, 8,
        juce::roundToInt ((pitchNormal + 1.0f) * 4.0f));
    const auto tint = pitchTints[static_cast<size_t> (tintIndex)];
    const auto tinted = [&tint] (juce::Colour colour, float alpha)
    {
        return juce::Colour::fromFloatRGBA (
            juce::jlimit (0.0f, 1.0f, colour.getFloatRed() * tint[0]),
            juce::jlimit (0.0f, 1.0f, colour.getFloatGreen() * tint[1]),
            juce::jlimit (0.0f, 1.0f, colour.getFloatBlue() * tint[2]),
            juce::jlimit (0.0f, 1.0f, alpha));
    };

    juce::Graphics::ScopedSaveState fieldState (g);
    g.reduceClipRegion (screen.toNearestInt());

    const auto centre = plot.getCentre();
    const auto radiusX = plot.getWidth() * 0.44f * (0.34f + 0.66f * scatterNormal)
                       * (0.62f + 0.38f * widthNormal);
    const auto radiusY = plot.getHeight() * 0.62f * (0.42f + 0.58f * scatterNormal);

    for (auto layer = 0; layer < 3; ++layer)
    {
        juce::Path streak;
        for (auto localX = 0.0f; localX <= plot.getWidth(); localX += 8.0f)
        {
            const auto wobble = std::sin (localX * 0.018f + grainPhase * 0.6f
                                          + static_cast<float> (layer) * 1.7f)
                                      * (2.4f + static_cast<float> (layer) * 1.6f)
                                + std::sin (localX * 0.006f - grainPhase * 0.35f) * 3.0f;
            const auto point = juce::Point<float> (plot.getX() + localX, centre.y + wobble);
            if (localX == 0.0f)
                streak.startNewSubPath (point);
            else
                streak.lineTo (point);
        }

        juce::ColourGradient streakColour (palette::accentPrimary.withAlpha (0.0f),
                                            plot.getX(), centre.y,
                                            palette::accentPrimary.withAlpha (0.0f),
                                            plot.getRight(), centre.y, false);
        streakColour.addColour (0.5,
            palette::valueText.withAlpha ((0.16f + toneNormal * 0.20f)
                                          / static_cast<float> (layer + 1)));
        g.setGradientFill (streakColour);
        g.strokePath (streak, juce::PathStrokeType (0.8f + static_cast<float> (layer) * 0.7f,
                                                    juce::PathStrokeType::curved));
    }

    constexpr auto coreGlow = 2.0f;
    const auto coreRadius = (12.0f + 22.0f * mixNormal + 10.0f * feedbackNormal) * coreGlow;
    const auto coreColour = juce::Colour::fromFloatRGBA (
        0.55f + 0.45f * tint[0], 0.55f + 0.45f * tint[1],
        0.55f + 0.45f * tint[2], 0.38f * coreGlow);
    juce::ColourGradient core (coreColour, centre.x, centre.y,
                               palette::accentSecondary.withAlpha (0.0f),
                               centre.x + coreRadius, centre.y, true);
    core.addColour (0.35, tinted (palette::accentPrimary, 0.14f * coreGlow));
    g.setGradientFill (core);
    g.fillEllipse (juce::Rectangle<float> (coreRadius * 2.0f, coreRadius * 2.0f)
                       .withCentre (centre));

    for (const auto& particle : grainParticles)
    {
        const auto life = juce::jmax (0.001f, particle.life);
        const auto ageNormal = juce::jlimit (0.0f, 1.0f, particle.age / life);
        const auto progression = particle.reverse ? 1.0f - ageNormal : ageNormal;
        const auto expansion = 0.18f + 0.82f * std::pow (progression, 0.55f);
        const auto angle = particle.angle + particle.spin * progression;
        const auto reach = 0.42f + 0.58f * std::sqrt (particle.radialRandom);
        const auto x = centre.x + std::cos (angle) * radiusX * expansion * reach;
        const auto y = centre.y + std::sin (angle) * radiusY * expansion * reach;
        const auto grainRadius = (1.7f + sizeNormal * 5.0f) * particle.sizeScale
                               * (0.58f + 0.42f * std::sin (
                                      juce::MathConstants<float>::pi * ageNormal));
        const auto fade = juce::jmin (1.0f, ageNormal * 9.0f)
                        * juce::jmin (1.0f, (1.0f - ageNormal) * 3.2f);
        const auto alpha = (0.30f + 0.70f * mixNormal) * fade;
        if (alpha <= 0.01f || x < plot.getX() - 20.0f || x > plot.getRight() + 20.0f)
            continue;

        const auto colourIndex = juce::jlimit (0, 5, juce::roundToInt (
            (toneNormal * 0.62f + particle.tone * 0.38f) * 5.0f));
        const auto haloRadius = grainRadius * 2.2f;
        const auto& sprite = grainHaloSprites[static_cast<size_t> (colourIndex * 9 + tintIndex)];
        g.setOpacity (alpha);
        g.drawImage (sprite, { x - haloRadius, y - haloRadius,
                              haloRadius * 2.0f, haloRadius * 2.0f });
    }
    g.setOpacity (1.0f);

    for (const auto& glint : shardGlints)
    {
        const auto ageNormal = juce::jlimit (0.0f, 1.0f,
            glint.age / juce::jmax (0.001f, glint.life));
        const auto x = centre.x + glint.x * plot.getWidth() * 0.5f;
        const auto y = centre.y + glint.y * plot.getHeight() * 0.5f;
        if (x < screen.getX() - 12.0f || x > screen.getRight() + 12.0f
            || y < screen.getY() - 12.0f || y > screen.getBottom() + 12.0f)
            continue;

        const auto twinkle = 0.62f + 0.38f * std::sin (glint.age * 34.0f
                                                       + glint.rotation * 5.0f);
        const auto fade = juce::jmin (1.0f, ageNormal * 14.0f)
                        * std::pow (1.0f - ageNormal, 1.4f);
        const auto alpha = juce::jlimit (0.0f, 1.0f,
            (0.45f + 0.55f * mixNormal) * fade * twinkle);
        if (alpha <= 0.01f)
            continue;

        const auto radius = glint.size * (0.55f + 0.45f * (1.0f - ageNormal));
        const auto colourIndex = juce::jlimit (0, 5,
            juce::roundToInt (glint.tone * 5.0f));
        const auto& sprite = grainHaloSprites[static_cast<size_t> (colourIndex * 9 + tintIndex)];
        const auto haloRadius = radius * 1.25f;
        g.setOpacity (alpha * 0.45f);
        g.drawImage (sprite, { x - haloRadius, y - haloRadius,
                              haloRadius * 2.0f, haloRadius * 2.0f });
        g.setOpacity (1.0f);
        g.setColour (juce::Colours::white.interpolatedWith (
                         tinted (palette::accentSecondary.interpolatedWith (
                                     palette::accentPrimary, glint.tone), 1.0f),
                         0.18f).withAlpha (alpha));
        g.fillPath (makeGlintPath ({ x, y }, radius, glint.rotation));
    }
    g.setOpacity (1.0f);

    if (const auto* bypass = audioProcessor.getValueTreeState().getRawParameterValue ("bypass");
        bypass != nullptr && bypass->load() >= 0.5f)
    {
        g.setColour (palette::displayBottom.withAlpha (0.78f));
        g.fillRoundedRectangle (screen, 4.0f);
        g.setColour (palette::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("BYPASSED", screen, juce::Justification::centred);
    }
}

float TBShimmerAudioProcessorEditor::getGrainLifeSeconds() const noexcept
{
    return juce::jmax (0.001f,
        static_cast<float> ((timeSlider.getValue() + sizeSlider.getValue()) * 0.001)
            * (1.0f + static_cast<float> (feedbackSlider.getValue() / 88.0) * 6.0f));
}

int TBShimmerAudioProcessorEditor::getTargetGrainCount() const noexcept
{
    constexpr auto grainCap = 420;
    return juce::jlimit (3, grainCap, juce::roundToInt (
        static_cast<float> (densitySlider.getValue()) * getGrainLifeSeconds() * 3.0f));
}

TBShimmerAudioProcessorEditor::GrainParticle
TBShimmerAudioProcessorEditor::makeGrainParticle (
    float lifeSeconds, float reverseChance, bool randomiseAge)
{
    const auto seed = nextGrainSeed++;
    const auto random = [seed] (std::uint32_t salt)
    {
        return hashUnit (seed * 0x9e3779b9u + salt);
    };

    GrainParticle particle;
    particle.life = lifeSeconds * (0.6f + random (0x11u) * 0.8f);
    particle.angle = random (0x23u) * juce::MathConstants<float>::twoPi;
    particle.radialRandom = random (0x35u);
    particle.sizeScale = 0.55f + random (0x47u) * 0.85f;
    particle.reverse = random (0x59u) < reverseChance;
    particle.tone = random (0x6bu);
    particle.spin = (random (0x7du) - 0.5f) * 0.9f;
    if (randomiseAge)
        particle.age = random (0x8fu) * particle.life;
    return particle;
}

void TBShimmerAudioProcessorEditor::seedGrainField()
{
    grainParticles.clear();
    grainParticles.reserve (525);
    grainSpawnAccumulator = 0.0f;
    grainPhase = 0.0f;
    nextGrainSeed = 1;

    const auto lifeSeconds = getGrainLifeSeconds();
    const auto reverseChance = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (reverseSlider.getValue() * 0.01));
    const auto target = getTargetGrainCount();
    for (auto index = 0; index < target; ++index)
        grainParticles.push_back (makeGrainParticle (lifeSeconds, reverseChance, true));
}

void TBShimmerAudioProcessorEditor::buildGrainHaloSprites()
{
    constexpr auto spriteSize = 64;
    constexpr auto spriteRadius = static_cast<float> (spriteSize) * 0.5f;

    for (auto colourIndex = 0; colourIndex < 6; ++colourIndex)
    {
        const auto grainColour = palette::accentSecondary.interpolatedWith (
            palette::accentPrimary, static_cast<float> (colourIndex) / 5.0f);

        for (auto tintIndex = 0; tintIndex < 9; ++tintIndex)
        {
            const auto& tint = pitchTints[static_cast<size_t> (tintIndex)];
            const auto tinted = [&tint] (juce::Colour colour, float alpha)
            {
                return juce::Colour::fromFloatRGBA (
                    juce::jlimit (0.0f, 1.0f, colour.getFloatRed() * tint[0]),
                    juce::jlimit (0.0f, 1.0f, colour.getFloatGreen() * tint[1]),
                    juce::jlimit (0.0f, 1.0f, colour.getFloatBlue() * tint[2]), alpha);
            };
            const auto centreColour = juce::Colour::fromFloatRGBA (
                0.55f + 0.45f * tint[0], 0.55f + 0.45f * tint[1],
                0.55f + 0.45f * tint[2], 0.98f);

            juce::Image sprite (juce::Image::ARGB, spriteSize, spriteSize, true);
            juce::Graphics spriteGraphics (sprite);
            juce::ColourGradient glow (centreColour, spriteRadius, spriteRadius,
                                       tinted (grainColour, 0.0f),
                                       static_cast<float> (spriteSize), spriteRadius, true);
            glow.addColour (0.16, tinted (grainColour, 0.92f));
            glow.addColour (0.42, tinted (grainColour, 0.34f));
            spriteGraphics.setGradientFill (glow);
            spriteGraphics.fillEllipse (0.0f, 0.0f,
                                        static_cast<float> (spriteSize),
                                        static_cast<float> (spriteSize));
            grainHaloSprites[static_cast<size_t> (colourIndex * 9 + tintIndex)]
                = std::move (sprite);
        }
    }
}

void TBShimmerAudioProcessorEditor::advanceGrainField (float elapsedSeconds)
{
    constexpr auto grainCap = 420;
    constexpr auto overflowCap = grainCap * 5 / 4;
    const auto lifeSeconds = getGrainLifeSeconds();
    const auto target = getTargetGrainCount();
    const auto reverseChance = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (reverseSlider.getValue() * 0.01));

    grainSpawnAccumulator += static_cast<float> (target) / lifeSeconds * elapsedSeconds;
    auto spawnGuard = 0;
    while (grainSpawnAccumulator >= 1.0f
           && static_cast<int> (grainParticles.size()) < overflowCap
           && spawnGuard++ < 40)
    {
        grainSpawnAccumulator -= 1.0f;
        grainParticles.push_back (makeGrainParticle (lifeSeconds, reverseChance, false));
    }
    if (grainSpawnAccumulator > 40.0f)
        grainSpawnAccumulator = 0.0f;

    for (auto& particle : grainParticles)
        particle.age += elapsedSeconds;
    grainParticles.erase (std::remove_if (grainParticles.begin(), grainParticles.end(),
        [] (const GrainParticle& particle) { return particle.age >= particle.life; }),
        grainParticles.end());
    if (static_cast<int> (grainParticles.size()) > overflowCap)
        grainParticles.erase (grainParticles.begin(),
                              grainParticles.begin()
                                  + (static_cast<int> (grainParticles.size()) - overflowCap));

    grainPhase += elapsedSeconds;
}

TBShimmerAudioProcessorEditor::ShardGlint
TBShimmerAudioProcessorEditor::makeShardGlint (float shatterNormal)
{
    const auto seed = nextGrainSeed++;
    const auto random = [seed] (std::uint32_t salt)
    {
        return hashUnit (seed * 0x85ebca6bu + salt);
    };

    // Fragments leave the core in every direction, then fall.
    const auto angle = random (0x13u) * juce::MathConstants<float>::twoPi;
    const auto speed = (0.22f + 0.70f * random (0x29u)) * (0.55f + 0.75f * shatterNormal);

    ShardGlint glint;
    glint.life = (0.55f + 0.85f * random (0x3bu)) * (1.15f - 0.35f * shatterNormal);
    glint.x = (random (0x4du) - 0.5f) * 0.10f;
    glint.y = (random (0x5fu) - 0.5f) * 0.30f;
    glint.velocityX = std::cos (angle) * speed;
    glint.velocityY = std::sin (angle) * speed * 2.2f - 0.55f * shatterNormal;
    glint.rotation = random (0x71u) * juce::MathConstants<float>::twoPi;
    glint.spin = (random (0x83u) - 0.5f) * (2.0f + 9.0f * shatterNormal);
    glint.size = 5.0f + 7.5f * random (0x95u) * random (0xa7u);
    glint.tone = random (0xb9u);
    return glint;
}

void TBShimmerAudioProcessorEditor::seedShardGlints()
{
    shardGlints.clear();
    shardGlints.reserve (maximumShardGlints);
    glintPreviewAccumulator = 0.0f;

    // Like the grain field, start mid-flight so the first frame already shows
    // how much Sparkle and Shatter are dialled in.
    const auto sparkleNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (sparkleSlider.getValue() * 0.01));
    const auto shatterNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (shatterSlider.getValue() * 0.01));
    const auto gravity = 0.9f + 2.6f * shatterNormal;
    const auto count = juce::roundToInt (sparkleNormal * (10.0f + 22.0f * shatterNormal));

    for (auto index = 0; index < count; ++index)
    {
        auto glint = makeShardGlint (shatterNormal);
        const auto elapsed = hashUnit (nextGrainSeed * 0xc2b2ae35u + 0xcbu)
                           * glint.life * 0.85f;
        glint.age = elapsed;
        glint.x += glint.velocityX * elapsed;
        glint.y += glint.velocityY * elapsed + 0.5f * gravity * elapsed * elapsed;
        glint.velocityY += gravity * elapsed;
        glint.rotation += glint.spin * elapsed;
        shardGlints.push_back (glint);
    }
}

void TBShimmerAudioProcessorEditor::advanceShardGlints (float elapsedSeconds)
{
    const auto sparkleNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (sparkleSlider.getValue() * 0.01));
    const auto shatterNormal = juce::jlimit (0.0f, 1.0f,
        static_cast<float> (shatterSlider.getValue() * 0.01));

    // The engine counts every shard it spawns, so the display breaks apart in
    // step with the audio. With no audio running a slow drizzle previews it.
    const auto spawnCount = audioProcessor.getShardSpawnCount();
    auto pending = static_cast<int> (juce::jmin<std::uint32_t> (
        spawnCount - lastShardSpawnCount, 14u));
    lastShardSpawnCount = spawnCount;

    if (pending == 0 && sparkleNormal > 0.0f)
    {
        glintPreviewAccumulator += sparkleNormal * 3.0f * elapsedSeconds;
        while (glintPreviewAccumulator >= 1.0f)
        {
            glintPreviewAccumulator -= 1.0f;
            ++pending;
        }
    }

    while (pending-- > 0 && static_cast<int> (shardGlints.size()) < maximumShardGlints)
        shardGlints.push_back (makeShardGlint (shatterNormal));

    const auto gravity = 0.9f + 2.6f * shatterNormal;
    for (auto& glint : shardGlints)
    {
        glint.age += elapsedSeconds;
        glint.velocityY += gravity * elapsedSeconds;
        glint.x += glint.velocityX * elapsedSeconds;
        glint.y += glint.velocityY * elapsedSeconds;
        glint.rotation += glint.spin * elapsedSeconds;
    }
    shardGlints.erase (std::remove_if (shardGlints.begin(), shardGlints.end(),
        [] (const ShardGlint& glint) { return glint.age >= glint.life; }),
        shardGlints.end());
}

void TBShimmerAudioProcessorEditor::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto elapsedSeconds = juce::jlimit (0.0f, 0.05f,
        static_cast<float> ((nowMs - lastVisualUpdateMs) * 0.001));
    lastVisualUpdateMs = nowMs;
    if (! isShowing())
        return;

    advanceGrainField (elapsedSeconds);
    advanceShardGlints (elapsedSeconds);
    repaint (14, 60, 732, 132);
}

void TBShimmerAudioProcessorEditor::resized()
{
    previousButton.setBounds (225, 14, 24, 24);
    presetBox.setBounds (253, 14, 164, 24);
    nextButton.setBounds (421, 14, 24, 24);
    saveButton.setBounds (451, 14, 24, 24);
    undoButton.setBounds (481, 14, 24, 24);
    redoButton.setBounds (511, 14, 24, 24);
    auto sliders = getSliders();
    auto labels = getLabels();

    for (auto index = 0; index < controlColumns; ++index)
    {
        const auto x = firstControlX + index * controlPitch;
        labels[static_cast<size_t> (index)]->setBounds (x, 220, controlWidth, 14);
        sliders[static_cast<size_t> (index)]->setBounds (x, 234, controlWidth, 70);

        const auto lowerIndex = static_cast<size_t> (index + controlColumns);
        labels[lowerIndex]->setBounds (x, 326, controlWidth, 14);
        sliders[lowerIndex]->setBounds (x, 340, controlWidth, 70);
    }
}

void TBShimmerAudioProcessorEditor::refreshUserPresetFiles()
{
    userPresetFiles.clear();
    const auto directory = audioProcessor.getUserPresetDirectory();
    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (
             directory, false,
             "*" + juce::String (TBShimmerAudioProcessor::userPresetExtension),
             juce::File::findFiles))
        userPresetFiles.push_back (entry.getFile());

    std::sort (userPresetFiles.begin(), userPresetFiles.end(),
               [] (const juce::File& first, const juce::File& second)
               {
                   return first.getFileNameWithoutExtension().compareIgnoreCase (
                              second.getFileNameWithoutExtension()) < 0;
               });
}

void TBShimmerAudioProcessorEditor::rebuildPresetMenu()
{
    const juce::ScopedValueSetter<bool> rebuilding (rebuildingPresetMenu, true);
    presetBox.clear (juce::dontSendNotification);
    auto* root = presetBox.getRootMenu();
    jassert (root != nullptr);
    if (root == nullptr)
        return;

    root->clear();
    juce::String currentGroup;
    juce::PopupMenu groupMenu;

    const auto flushGroup = [root, &currentGroup, &groupMenu]
    {
        if (currentGroup.isNotEmpty())
        {
            root->addSubMenu (currentGroup, groupMenu);
            groupMenu = {};
        }
    };

    for (auto index = 0; index < audioProcessor.getFactoryPresetCount(); ++index)
    {
        const auto& preset = audioProcessor.getFactoryPreset (index);
        const juce::String group (preset.group);
        if (group != currentGroup)
        {
            flushGroup();
            currentGroup = group;
        }
        groupMenu.addItem (index + 1, preset.name);
    }
    flushGroup();

    juce::PopupMenu userMenu;
    if (userPresetFiles.empty())
        userMenu.addItem (999, "NO USER PRESETS", false, false);
    else
        for (size_t index = 0; index < userPresetFiles.size(); ++index)
            userMenu.addItem (userMenuBase + static_cast<int> (index),
                              userPresetFiles[index].getFileNameWithoutExtension());
    root->addSubMenu ("USER", userMenu);
}

void TBShimmerAudioProcessorEditor::handlePresetSelection()
{
    if (rebuildingPresetMenu)
        return;

    const auto selection = presetBox.getSelectedId();
    const auto factoryCount = audioProcessor.getFactoryPresetCount();
    if (selection > 0 && selection <= factoryCount)
    {
        audioProcessor.applyFactoryPreset (selection - 1);
        refreshPresetDisplay();
        refreshCommandState();
        repaint (14, 60, 732, 132);
        return;
    }

    const auto userIndex = selection - userMenuBase;
    if (juce::isPositiveAndBelow (userIndex, static_cast<int> (userPresetFiles.size())))
        loadUserPreset (userPresetFiles[static_cast<size_t> (userIndex)]);
}

void TBShimmerAudioProcessorEditor::cyclePreset (int direction)
{
    refreshUserPresetFiles();
    rebuildPresetMenu();

    const auto factoryCount = audioProcessor.getFactoryPresetCount();
    const auto totalCount = factoryCount + static_cast<int> (userPresetFiles.size());
    if (totalCount <= 0)
        return;

    auto current = audioProcessor.getSelectedFactoryPreset();
    if (current < 0)
    {
        const auto userName = audioProcessor.getSelectedUserPresetName();
        for (size_t index = 0; index < userPresetFiles.size(); ++index)
            if (userName.equalsIgnoreCase (userPresetFiles[index].getFileNameWithoutExtension()))
            {
                current = factoryCount + static_cast<int> (index);
                break;
            }
    }

    if (current < 0)
        current = direction > 0 ? -1 : 0;

    const auto next = (current + direction + totalCount) % totalCount;
    if (next < factoryCount)
        audioProcessor.applyFactoryPreset (next);
    else
        loadUserPreset (userPresetFiles[static_cast<size_t> (next - factoryCount)]);

    refreshPresetDisplay();
    refreshCommandState();
    repaint (14, 60, 732, 132);
}

void TBShimmerAudioProcessorEditor::saveCurrentPreset()
{
    const auto directoryResult = audioProcessor.getUserPresetDirectory().createDirectory();
    if (directoryResult.failed())
    {
        showPresetError (directoryResult.getErrorMessage());
        return;
    }

    const auto selectedUser = audioProcessor.getSelectedUserPresetName();
    const auto suggestedName = selectedUser.isNotEmpty() ? selectedUser : juce::String ("New Preset");
    auto* dialog = new juce::AlertWindow ("Save TB Shimmer Preset",
                                          "Name the preset. It will be saved in\n"
                                          "Documents/TB_Shimmer/Presets/User.",
                                          juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor ("presetName", suggestedName, "Preset name");
    dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<TBShimmerAudioProcessorEditor> (this);
    auto safeDialog = juce::Component::SafePointer<juce::AlertWindow> (dialog);
    dialog->enterModalState (
        true,
        juce::ModalCallbackFunction::create (
            [safeThis, safeDialog] (int result)
            {
                if (safeThis == nullptr || safeDialog == nullptr || result != 1)
                    return;
                safeThis->saveNamedUserPreset (
                    safeDialog->getTextEditorContents ("presetName"), false);
            }),
        true);
}

void TBShimmerAudioProcessorEditor::saveNamedUserPreset (
    const juce::String& requestedName, bool allowOverwrite)
{
    const auto trimmedName = requestedName.trim();
    const auto legalName = juce::File::createLegalFileName (trimmedName).trim();
    if (legalName.isEmpty())
    {
        showPresetError ("Preset name is empty.");
        return;
    }

    if (legalName != trimmedName)
    {
        showPresetError ("Use a filename-safe preset name without reserved characters.");
        return;
    }

    const auto target = audioProcessor.getUserPresetDirectory().getChildFile (
        legalName + TBShimmerAudioProcessor::userPresetExtension);
    if (target.existsAsFile() && ! allowOverwrite)
    {
        auto safeThis = juce::Component::SafePointer<TBShimmerAudioProcessorEditor> (this);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Replace User Preset?")
                .withMessage ("A preset named ‘" + legalName + "’ already exists.")
                .withButton ("Replace")
                .withButton ("Cancel")
                .withAssociatedComponent (this),
            [safeThis, legalName] (int result)
            {
                if (safeThis != nullptr && result == 1)
                    safeThis->saveNamedUserPreset (legalName, true);
            });
        return;
    }

    const auto result = audioProcessor.saveUserPreset (target);
    if (result.failed())
    {
        showPresetError (result.getErrorMessage());
        return;
    }

    refreshUserPresetFiles();
    rebuildPresetMenu();
    refreshPresetDisplay();
    refreshCommandState();
    showToast ("SAVED · " + legalName);
}

void TBShimmerAudioProcessorEditor::loadUserPreset (const juce::File& file)
{
    const auto result = audioProcessor.loadUserPreset (file);
    if (result.failed())
    {
        showPresetError (result.getErrorMessage());
        return;
    }

    refreshPresetDisplay();
    refreshCommandState();
    repaint (14, 60, 732, 132);
}

void TBShimmerAudioProcessorEditor::refreshPresetDisplay()
{
    juce::String name;
    const auto factoryIndex = audioProcessor.getSelectedFactoryPreset();
    if (juce::isPositiveAndBelow (factoryIndex, audioProcessor.getFactoryPresetCount()))
        name = audioProcessor.getFactoryPreset (factoryIndex).name;
    else
        name = audioProcessor.getSelectedUserPresetName();

    if (name.isEmpty())
        name = "Custom";

    const auto dirty = audioProcessor.isCurrentPresetDirty();
    const auto displayName = name + (dirty ? " *" : "");
    displayedPresetName = displayName;
    const juce::ScopedValueSetter<bool> rebuilding (rebuildingPresetMenu, true);
    presetBox.setSelectedId (0, juce::dontSendNotification);
    presetBox.setTextWhenNothingSelected (displayName);
    presetBox.repaint();
}

void TBShimmerAudioProcessorEditor::refreshCommandState()
{
    auto& manager = audioProcessor.getUndoManager();
    undoButton.setEnabled (manager.canUndo());
    redoButton.setEnabled (manager.canRedo());
}

void TBShimmerAudioProcessorEditor::showPresetError (const juce::String& message)
{
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("TB Shimmer Preset")
            .withMessage (message)
            .withButton ("OK")
            .withAssociatedComponent (this),
        {});
}

void TBShimmerAudioProcessorEditor::showToast (const juce::String& message)
{
    toastMessage = message;
    const auto generation = ++toastGeneration;
    repaint (526, 64, 205, 30);

    auto safeThis = juce::Component::SafePointer<TBShimmerAudioProcessorEditor> (this);
    juce::Timer::callAfterDelay (3200, [safeThis, generation]
    {
        if (safeThis == nullptr || safeThis->toastGeneration != generation)
            return;
        safeThis->toastMessage.clear();
        safeThis->repaint (526, 64, 205, 30);
    });
}

void TBShimmerAudioProcessorEditor::handleParameterVisualChange()
{
    refreshPresetDisplay();
    refreshCommandState();
    repaint (14, 60, 732, 132);
    repaint (14, 202, 732, 214);
}
