/*
  ==============================================================================

    Editor for the XYZ panner.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
const auto canvasTop = juce::Colour (0xff30383b);
const auto canvasBottom = juce::Colour (0xff14191b);
const auto surfaceTop = juce::Colour (0xff2a3134);
const auto surfaceBottom = juce::Colour (0xff191e20);
const auto displayTop = juce::Colour (0xff102b2d);
const auto displayBottom = juce::Colour (0xff071719);
const auto textPrimary = juce::Colour (0xffd7dddc);
const auto textSecondary = juce::Colour (0xffaeb9b8);
const auto valueText = juce::Colour (0xffc7d7d4);
const auto accentPrimary = juce::Colour (0xffed9251);
const auto sectionAccent = juce::Colour (0xff9e4f2d);
const auto accentSecondary = juce::Colour (0xff78c4bd);
const auto outline = juce::Colour (0xff596265);
const auto deepShadow = juce::Colour (0xff080b0c);
const auto highlight = juce::Colour (0xffd9e1df);
const auto metalFaceTop = juce::Colour (0xff778185);
const auto gridColour = accentSecondary.withAlpha (0.16f);

void drawBezel (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius)
{
    g.setColour (deepShadow.withAlpha (0.82f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), cornerRadius);
    g.setColour (outline);
    g.fillRoundedRectangle (bounds, cornerRadius);
    g.setColour (deepShadow);
    g.fillRoundedRectangle (bounds.reduced (2.0f), cornerRadius - 1.0f);
    g.setColour (highlight.withAlpha (0.12f));
    g.drawRoundedRectangle (bounds.reduced (0.8f), cornerRadius, 1.0f);
}
}

SpatialPlane::SpatialPlane (juce::AudioProcessorValueTreeState& state, View planeView)
    : parameters (state), view (planeView)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

SpatialPlane::~SpatialPlane()
{
    endGesture();
}

juce::Rectangle<float> SpatialPlane::getPlotBounds() const
{
    return getLocalBounds().toFloat().reduced (22.0f);
}

juce::Rectangle<float> SpatialPlane::getMovementBounds() const
{
    const auto plot = getPlotBounds();

    if (view == View::top)
    {
        const auto diameter = juce::jmin (plot.getWidth(), plot.getHeight()) - 20.0f;
        return plot.withSizeKeepingCentre (diameter, diameter);
    }

    return { plot.getX() + 28.0f, plot.getY() + 8.0f,
             plot.getWidth() - 36.0f, plot.getHeight() - 16.0f };
}

void SpatialPlane::beginGesture()
{
    if (gestureActive)
        return;

    // Flush the preceding edit before opening a transaction for this whole XY drag.
    parameters.copyState();
    if (parameters.undoManager != nullptr)
        parameters.undoManager->beginNewTransaction();

    for (const auto* id : { view == View::top ? "pan" : "distance",
                            view == View::top ? "frontBack" : "height" })
        if (auto* parameter = parameters.getParameter (id))
            parameter->beginChangeGesture();

    gestureActive = true;
}

void SpatialPlane::endGesture()
{
    if (! gestureActive)
        return;

    parameters.copyState();
    for (const auto* id : { view == View::top ? "pan" : "distance",
                            view == View::top ? "frontBack" : "height" })
        if (auto* parameter = parameters.getParameter (id))
            parameter->endChangeGesture();

    gestureActive = false;
    if (parameters.undoManager != nullptr)
        parameters.undoManager->beginNewTransaction();
}

float SpatialPlane::getParameterValue (const juce::String& parameterID) const
{
    if (const auto* value = parameters.getRawParameterValue (parameterID))
        return value->load();

    return 0.0f;
}

void SpatialPlane::setParameterValue (const juce::String& parameterID, float value)
{
    if (auto* parameter = parameters.getParameter (parameterID))
    {
        const auto normalised = parameter->convertTo0to1 (value);
        if (! juce::approximatelyEqual (parameter->getValue(), normalised))
            parameter->setValueNotifyingHost (normalised);
    }
}

void SpatialPlane::drawObjectMarker (juce::Graphics& g, juce::Point<float> position) const
{
    g.setColour (accentPrimary.withAlpha (0.14f));
    g.fillEllipse (position.x - 15.0f, position.y - 15.0f, 30.0f, 30.0f);
    g.setColour (accentPrimary.withAlpha (0.45f));
    g.drawEllipse (position.x - 10.0f, position.y - 10.0f, 20.0f, 20.0f, 1.2f);
    g.setColour (accentPrimary);
    g.fillEllipse (position.x - 4.5f, position.y - 4.5f, 9.0f, 9.0f);
}

void SpatialPlane::drawTopListener (juce::Graphics& g, juce::Point<float> position) const
{
    g.setColour (textPrimary);
    g.fillEllipse (position.x - 10.0f, position.y - 10.0f, 20.0f, 20.0f);
    g.fillRoundedRectangle (position.x - 16.0f, position.y + 11.0f, 32.0f, 26.0f, 13.0f);
    g.setColour (textSecondary);
    g.drawLine (position.x, position.y - 14.0f, position.x, position.y - 29.0f, 2.0f);
    juce::Path arrow;
    arrow.addTriangle (position.x, position.y - 36.0f, position.x - 5.0f, position.y - 27.0f,
                       position.x + 5.0f, position.y - 27.0f);
    g.fillPath (arrow);
}

void SpatialPlane::drawSideListener (juce::Graphics& g, juce::Point<float> position) const
{
    g.setColour (textPrimary);
    g.fillEllipse (position.x - 10.0f, position.y - 10.0f, 20.0f, 20.0f);
    g.fillRoundedRectangle (position.x - 12.0f, position.y + 11.0f, 24.0f, 30.0f, 9.0f);
    g.setColour (textSecondary);
    g.drawLine (position.x + 16.0f, position.y, position.x + 31.0f, position.y, 2.0f);
    juce::Path arrow;
    arrow.addTriangle (position.x + 38.0f, position.y, position.x + 29.0f, position.y - 5.0f,
                       position.x + 29.0f, position.y + 5.0f);
    g.fillPath (arrow);
}

void SpatialPlane::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = getPlotBounds();
    const auto movement = getMovementBounds();
    const auto isTopView = view == View::top;

    drawBezel (g, bounds, 11.0f);
    const auto display = bounds.reduced (4.0f);
    juce::ColourGradient panelGradient (displayTop, display.getX(), display.getY(),
                                        displayBottom, display.getX(), display.getBottom(), false);
    g.setGradientFill (panelGradient);
    g.fillRoundedRectangle (display, 8.0f);

    g.saveState();
    g.reduceClipRegion (bounds.toNearestInt());
    for (int y = static_cast<int> (bounds.getY()); y < static_cast<int> (bounds.getBottom()); y += 3)
    {
        const auto texture = 0.008f + 0.008f * std::sin (static_cast<float> (y) * 0.31f);
        g.setColour (accentSecondary.withAlpha (texture));
        g.drawHorizontalLine (y, bounds.getX(), bounds.getRight());
    }
    g.restoreState();

    g.setColour (highlight.withAlpha (0.10f));
    g.drawRoundedRectangle (display.reduced (0.5f), 8.0f, 1.0f);

    g.setColour (gridColour);
    for (int division = 0; division <= 4; ++division)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float> (division) / 4.0f;
        const auto y = plot.getY() + plot.getHeight() * static_cast<float> (division) / 4.0f;
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
    }

    g.setColour (accentSecondary.withAlpha (0.24f));
    g.drawLine (plot.getCentreX(), plot.getY(), plot.getCentreX(), plot.getBottom(), 1.0f);
    g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 1.0f);

    const auto pan = getParameterValue ("pan");
    const auto frontBack = getParameterValue ("frontBack");
    const auto height = getParameterValue ("height");
    const auto distance = getParameterValue ("distance");

    g.setColour (textSecondary);
    g.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));

    if (isTopView)
    {
        const juce::Point<float> listener { plot.getCentreX(), plot.getCentreY() };
        const auto radius = 0.5f * movement.getWidth();

        g.setColour (gridColour.brighter (0.2f));
        g.drawEllipse (listener.x - radius, listener.y - radius, 2.0f * radius, 2.0f * radius, 1.0f);
        g.drawEllipse (listener.x - 0.5f * radius, listener.y - 0.5f * radius, radius, radius, 0.8f);

        const juce::Point<float> object {
            movement.getX() + 0.5f * (pan + 1.0f) * movement.getWidth(),
            movement.getY() + 0.5f * (1.0f - frontBack) * movement.getHeight()
        };

        drawTopListener (g, listener);
        g.setColour (textSecondary);
        g.drawText ("FRONT", static_cast<int> (plot.getCentreX() - 28.0f), static_cast<int> (plot.getY() - 16), 56, 16,
                    juce::Justification::centred);
        g.drawText ("BACK", static_cast<int> (plot.getCentreX() - 24.0f), static_cast<int> (plot.getBottom() + 2), 48, 16,
                    juce::Justification::centred);
        g.drawText ("LEFT", static_cast<int> (plot.getX()), static_cast<int> (plot.getCentreY() - 8.0f), 42, 16,
                    juce::Justification::centredLeft);
        g.drawText ("RIGHT", static_cast<int> (plot.getRight() - 48.0f), static_cast<int> (plot.getCentreY() - 8.0f), 48, 16,
                    juce::Justification::centredRight);
        drawObjectMarker (g, object);
    }
    else
    {
        const juce::Point<float> listener { movement.getX(), movement.getCentreY() };
        const juce::Point<float> object {
            movement.getX() + distance * movement.getWidth(),
            movement.getY() + 0.5f * (1.0f - height) * movement.getHeight()
        };

        drawSideListener (g, listener);
        g.setColour (textSecondary);
        g.drawText ("UP", static_cast<int> (plot.getX()), static_cast<int> (plot.getY() - 16), 30, 16,
                    juce::Justification::centredLeft);
        g.drawText ("DOWN", static_cast<int> (plot.getX()), static_cast<int> (plot.getBottom() + 2), 42, 16,
                    juce::Justification::centredLeft);
        g.drawText ("FAR", static_cast<int> (plot.getRight() - 34.0f), static_cast<int> (plot.getCentreY() - 8.0f), 34, 16,
                    juce::Justification::centredRight);
        drawObjectMarker (g, object);
    }
}

void SpatialPlane::updatePosition (juce::Point<float> position)
{
    const auto movement = getMovementBounds();
    const auto x = juce::jlimit (0.0f, 1.0f, (position.x - movement.getX()) / movement.getWidth());
    const auto y = juce::jlimit (0.0f, 1.0f, (position.y - movement.getY()) / movement.getHeight());

    if (view == View::top)
    {
        setParameterValue ("pan", 2.0f * x - 1.0f);
        setParameterValue ("frontBack", 1.0f - 2.0f * y);
    }
    else
    {
        setParameterValue ("height", 1.0f - 2.0f * y);
        setParameterValue ("distance", x);
    }

    repaint();
}

void SpatialPlane::mouseDown (const juce::MouseEvent& event)
{
    if (! event.mods.isLeftButtonDown() || event.mods.isRightButtonDown())
        return;

    beginGesture();
    updatePosition (event.position);
}

void SpatialPlane::mouseDrag (const juce::MouseEvent& event)
{
    if (gestureActive && event.mods.isLeftButtonDown())
        updatePosition (event.position);
}

void SpatialPlane::mouseUp (const juce::MouseEvent&)
{
    endGesture();
}

void MetalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider&)
{
    const auto diameter = juce::jmin (64.0f, static_cast<float> (juce::jmin (width, height)) - 8.0f);
    const juce::Rectangle<float> knobBounds (static_cast<float> (x) + 0.5f * (width - diameter),
                                             static_cast<float> (y) + 0.5f * (height - diameter),
                                             diameter, diameter);
    const auto centre = knobBounds.getCentre();
    const auto radius = 0.5f * diameter;

    for (int tick = 0; tick <= 10; ++tick)
    {
        const auto tickAmount = static_cast<float> (tick) / 10.0f;
        const auto tickAngle = rotaryStartAngle + tickAmount * (rotaryEndAngle - rotaryStartAngle);
        const juce::Point<float> direction { std::sin (tickAngle), -std::cos (tickAngle) };
        const auto inner = centre + direction * (radius + 3.5f);
        const auto outer = centre + direction * (radius + (tick % 5 == 0 ? 8.0f : 6.5f));
        g.setColour (tick % 5 == 0 ? textSecondary.withAlpha (0.62f) : outline.withAlpha (0.68f));
        g.drawLine ({ inner, outer }, tick % 5 == 0 ? 1.25f : 0.8f);
    }

    g.setColour (deepShadow.withAlpha (0.80f));
    g.fillEllipse (knobBounds.expanded (2.0f).translated (0.0f, 3.5f));
    g.setColour (deepShadow);
    g.fillEllipse (knobBounds.expanded (3.0f));

    const auto sideBounds = knobBounds.translated (0.0f, 2.0f);
    juce::ColourGradient sideGradient (outline, centre.x, sideBounds.getY(),
                                       deepShadow, centre.x, sideBounds.getBottom(), false);
    g.setGradientFill (sideGradient);
    g.fillEllipse (sideBounds);

    juce::ColourGradient knobGradient (metalFaceTop, knobBounds.getX() + radius * 0.35f, knobBounds.getY(),
                                       surfaceBottom, knobBounds.getRight(), knobBounds.getBottom(), true);
    g.setGradientFill (knobGradient);
    g.fillEllipse (knobBounds.translated (0.0f, -1.0f));
    g.setColour (deepShadow.withAlpha (0.85f));
    g.drawEllipse (knobBounds.reduced (0.8f).translated (0.0f, -1.0f), 1.2f);
    g.setColour (highlight.withAlpha (0.22f));
    const auto highlightBounds = knobBounds.reduced (2.4f).translated (0.0f, -1.0f);
    juce::Path rimHighlight;
    rimHighlight.addCentredArc (highlightBounds.getCentreX(), highlightBounds.getCentreY(),
                                0.5f * highlightBounds.getWidth(), 0.5f * highlightBounds.getHeight(), 0.0f,
                                juce::MathConstants<float>::pi * 1.08f,
                                juce::MathConstants<float>::pi * 1.88f, true);
    g.strokePath (rimHighlight, juce::PathStrokeType (1.0f));

    for (int ring = 0; ring < 4; ++ring)
    {
        g.setColour (highlight.withAlpha (0.025f));
        g.drawEllipse (knobBounds.reduced (5.0f + static_cast<float> (ring) * 2.1f).translated (0.0f, -1.0f), 0.7f);
    }

    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);
    const auto pointerEnd = centre + juce::Point<float> (std::cos (angle - juce::MathConstants<float>::halfPi),
                                                          std::sin (angle - juce::MathConstants<float>::halfPi))
                                     * (radius * 0.58f);
    g.setColour (deepShadow);
    g.drawLine (centre.x, centre.y, pointerEnd.x, pointerEnd.y, 4.0f);
    g.setColour (accentPrimary);
    g.drawLine (centre.x, centre.y, pointerEnd.x, pointerEnd.y, 2.0f);

    const auto capBounds = juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre);
    g.setColour (deepShadow);
    g.fillEllipse (capBounds.expanded (1.0f));
    juce::ColourGradient capGradient (outline.brighter (0.18f), capBounds.getX(), capBounds.getY(),
                                      surfaceBottom, capBounds.getRight(), capBounds.getBottom(), false);
    g.setGradientFill (capGradient);
    g.fillEllipse (capBounds);
}

void MetalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto active = button.getToggleState();
    auto top = active ? accentPrimary : surfaceTop;
    auto bottom = active ? sectionAccent : surfaceBottom;

    if (! button.isEnabled())
    {
        top = surfaceBottom;
        bottom = deepShadow;
    }

    if (highlighted && button.isEnabled())
        top = top.brighter (0.12f);

    if (down && button.isEnabled())
        bottom = bottom.darker (0.16f);

    juce::ColourGradient gradient (top, bounds.getCentreX(), bounds.getY(),
                                   bottom, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (active ? accentPrimary.brighter (0.32f) : outline.withAlpha (0.85f));
    g.drawRoundedRectangle (bounds, 6.0f, active ? 1.4f : 1.0f);
}

void MetalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool, bool)
{
    const auto historyButton = button.getComponentID() == "historyUndo"
                            || button.getComponentID() == "historyRedo";

    if (historyButton)
    {
        const auto centre = button.getLocalBounds().toFloat().getCentre();
        const auto isRedo = button.getComponentID() == "historyRedo";
        juce::Path arrow;
        arrow.startNewSubPath (centre.x + 7.0f, centre.y + 6.0f);
        arrow.cubicTo (centre.x + 8.0f, centre.y - 1.0f,
                       centre.x + 2.0f, centre.y - 7.0f,
                       centre.x - 6.0f, centre.y - 4.0f);

        juce::Path head;
        head.addTriangle (centre.x - 9.0f, centre.y - 4.0f,
                          centre.x - 2.0f, centre.y - 8.0f,
                          centre.x - 2.0f, centre.y + 0.2f);

        if (isRedo)
        {
            const auto mirror = juce::AffineTransform::horizontalFlip (2.0f * centre.x);
            arrow.applyTransform (mirror);
            head.applyTransform (mirror);
        }

        g.setColour (button.isEnabled() ? textPrimary : textSecondary.withAlpha (0.46f));
        g.strokePath (arrow, juce::PathStrokeType (2.1f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.fillPath (head);
        return;
    }

    g.setColour (button.isEnabled() ? (button.getToggleState() ? juce::Colours::white : textSecondary)
                                    : textSecondary.withAlpha (0.32f));
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}

juce::Label* MetalLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setName ("valueDisplay");
    label->setJustificationType (juce::Justification::centred);
    label->setFont (juce::FontOptions (10.5f).withStyle ("Bold"));
    return label;
}

void MetalLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.isBeingEdited())
        return;

    auto bounds = label.getLocalBounds().toFloat();
    if (label.getName() == "valueDisplay")
    {
        g.setColour (displayBottom);
        g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
        g.setColour (outline.withAlpha (0.86f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 3.0f, 1.0f);
    }

    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), label.getLocalBounds().reduced (3, 0),
                      label.getJustificationType(), 1, label.getMinimumHorizontalScale());
}

NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      topPlane (p.parameters, SpatialPlane::View::top),
      sidePlane (p.parameters, SpatialPlane::View::side)
{
    setLookAndFeel (&metalLookAndFeel);
    headerIcon = juce::Drawable::createFromImageData (BinaryData::tb_mouse_icon_svg,
                                                       BinaryData::tb_mouse_icon_svgSize);
    addAndMakeVisible (topPlane);
    addAndMakeVisible (sidePlane);

    configureSlider (panSlider, panLabel, "PAN");
    configureSlider (frontBackSlider, frontBackLabel, "FRONT / BACK");
    configureSlider (heightSlider, heightLabel, "HEIGHT");
    configureSlider (distanceSlider, distanceLabel, "DISTANCE");
    configureSlider (roomSlider, roomLabel, "ROOM");
    configureSlider (outputTrimSlider, outputTrimLabel, "OUTPUT");

    distanceModelLabel.setText ("DISTANCE CURVE", juce::dontSendNotification);
    distanceModelLabel.setJustificationType (juce::Justification::centred);
    distanceModelLabel.setColour (juce::Label::textColourId, textSecondary);
    distanceModelLabel.setFont (juce::FontOptions (10.0f).withStyle ("Bold"));
    addAndMakeVisible (distanceModelLabel);
    distanceModelSelector.addItem ("PHYSICAL", 1);
    distanceModelSelector.addItem ("NEAR FIELD", 2);
    distanceModelSelector.addItem ("LINEAR", 3);
    distanceModelSelector.setJustificationType (juce::Justification::centred);
    distanceModelSelector.setColour (juce::ComboBox::backgroundColourId, displayBottom);
    distanceModelSelector.setColour (juce::ComboBox::outlineColourId, outline);
    distanceModelSelector.setColour (juce::ComboBox::textColourId, valueText);
    addAndMakeVisible (distanceModelSelector);

    stereoButton.setClickingTogglesState (false);
    headphonesButton.setClickingTogglesState (false);
    undoButton.setClickingTogglesState (false);
    redoButton.setClickingTogglesState (false);
    undoButton.setComponentID ("historyUndo");
    redoButton.setComponentID ("historyRedo");
    undoButton.setTooltip ("Undo last change");
    redoButton.setTooltip ("Redo last change");
    stereoButton.onClick = [this] { setRenderMode (0); };
    headphonesButton.onClick = [this] { setRenderMode (1); };
    undoButton.onClick = [this] { performUndo(); };
    redoButton.onClick = [this] { performRedo(); };
    addAndMakeVisible (stereoButton);
    addAndMakeVisible (headphonesButton);
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);

    panAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "pan", panSlider);
    frontBackAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "frontBack", frontBackSlider);
    heightAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "height", heightSlider);
    distanceAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "distance", distanceSlider);
    roomAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "room", roomSlider);
    outputTrimAttachment = std::make_unique<SliderAttachment> (audioProcessor.parameters, "outputTrim", outputTrimSlider);
    distanceModelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.parameters, "distanceModel", distanceModelSelector);
    distanceModelSelector.onChange = [this]
    {
        // The attachment runs first; host-originated refreshes must not open a new transaction.
        audioProcessor.parameters.copyState();
        syncHistoryButtons();
    };

    syncRenderModeButtons();
    syncHistoryButtons();
    startTimerHz (30);
    setSize (1080, 720);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void NewProjectAudioProcessorEditor::configureSlider (juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 20);
    slider.setColour (juce::Slider::textBoxTextColourId, valueText);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, deepShadow);
    slider.setColour (juce::Slider::textBoxOutlineColourId, outline.withAlpha (0.65f));
    slider.onDragEnd = [this]
    {
        audioProcessor.parameters.copyState();
        audioProcessor.undoManager.beginNewTransaction();
        syncHistoryButtons();
    };

    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    label.setColour (juce::Label::textColourId, textSecondary);
    addAndMakeVisible (label);
}

void NewProjectAudioProcessorEditor::setRenderMode (int mode)
{
    if (audioProcessor.getTotalNumOutputChannels() != 2 || audioProcessor.getTotalNumInputChannels() > 2)
        return;

    if (auto* parameter = audioProcessor.parameters.getParameter ("renderMode"))
    {
        const auto normalised = parameter->convertTo0to1 (static_cast<float> (mode));
        if (juce::approximatelyEqual (parameter->getValue(), normalised))
            return;

        audioProcessor.parameters.copyState();
        audioProcessor.undoManager.beginNewTransaction();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalised);
        audioProcessor.parameters.copyState();
        parameter->endChangeGesture();
        audioProcessor.undoManager.beginNewTransaction();
        syncRenderModeButtons();
        syncHistoryButtons();
    }
}

void NewProjectAudioProcessorEditor::syncRenderModeButtons()
{
    const auto* parameter = audioProcessor.parameters.getRawParameterValue ("renderMode");
    const auto mode = parameter != nullptr ? juce::roundToInt (parameter->load()) : 0;
    const auto surroundOutput = audioProcessor.getTotalNumOutputChannels() > 2;
    const auto binauralDownmix = ! surroundOutput && audioProcessor.getTotalNumInputChannels() > 2;

    stereoButton.setButtonText (surroundOutput ? "SURROUND" : "STEREO");
    stereoButton.setEnabled (! surroundOutput && ! binauralDownmix);
    headphonesButton.setEnabled (! surroundOutput);
    stereoButton.setToggleState (surroundOutput || (! binauralDownmix && mode == 0), juce::dontSendNotification);
    headphonesButton.setToggleState (! surroundOutput && (binauralDownmix || mode == 1), juce::dontSendNotification);

    if (surroundOutput)
    {
        stereoButton.setTooltip ("Surround output uses speaker panning, not binaural HRTF rendering.");
        headphonesButton.setTooltip ("Headphone rendering requires a stereo output bus.");
    }
    else if (binauralDownmix)
    {
        stereoButton.setTooltip ("Surround input is automatically rendered to binaural stereo.");
        headphonesButton.setTooltip ("Each surround input channel is rendered to headphones from its speaker direction.");
    }
    else
    {
        stereoButton.setTooltip ("Stereo-compatible panning for loudspeakers.");
        headphonesButton.setTooltip ("Binaural HRTF rendering for headphones.");
    }
}

void NewProjectAudioProcessorEditor::performUndo()
{
    audioProcessor.parameters.copyState();
    if (audioProcessor.undoManager.canUndo())
        audioProcessor.undoManager.undo();

    syncRenderModeButtons();
    syncHistoryButtons();
    topPlane.repaint();
    sidePlane.repaint();
}

void NewProjectAudioProcessorEditor::performRedo()
{
    audioProcessor.parameters.copyState();
    if (audioProcessor.undoManager.canRedo())
        audioProcessor.undoManager.redo();

    syncRenderModeButtons();
    syncHistoryButtons();
    topPlane.repaint();
    sidePlane.repaint();
}

void NewProjectAudioProcessorEditor::syncHistoryButtons()
{
    undoButton.setEnabled (audioProcessor.undoManager.canUndo());
    redoButton.setEnabled (audioProcessor.undoManager.canRedo());
}

void NewProjectAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (canvasTop, 0.0f, 0.0f,
                                     canvasBottom, 0.0f, static_cast<float> (getHeight()), false);
    g.setGradientFill (background);
    g.fillAll();

    for (int y = 0; y < getHeight(); y += 3)
    {
        const auto wave = 0.010f + 0.009f * std::sin (static_cast<float> (y) * 0.37f)
                        + 0.004f * std::sin (static_cast<float> (y) * 1.81f);
        g.setColour (highlight.withAlpha (juce::jlimit (0.002f, 0.026f, wave)));
        g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));
    }

    auto titleBounds = getLocalBounds().reduced (54, 15).removeFromTop (48);
    const auto iconBounds = titleBounds.removeFromLeft (42).toFloat().reduced (1.0f, 3.0f);

    if (headerIcon != nullptr)
        headerIcon->drawWithin (g, iconBounds, juce::RectanglePlacement::centred, 0.96f);

    titleBounds.removeFromLeft (11);
    g.setColour (textPrimary);
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawText ("TB XYZ PANNER", titleBounds, juce::Justification::centredLeft);

    g.setColour (sectionAccent);
    g.fillRect (54.0f, 67.0f, 72.0f, 2.0f);

    const auto panel = controlPanelBounds.toFloat();
    g.setColour (deepShadow.withAlpha (0.72f));
    g.fillRoundedRectangle (panel.translated (0.0f, 3.0f), 9.0f);
    juce::ColourGradient panelGradient (surfaceTop, panel.getX(), panel.getY(),
                                        surfaceBottom, panel.getX(), panel.getBottom(), false);
    g.setGradientFill (panelGradient);
    g.fillRoundedRectangle (panel, 9.0f);
    g.setColour (outline.withAlpha (0.72f));
    g.drawRoundedRectangle (panel.reduced (0.5f), 9.0f, 1.0f);

    if (! panel.isEmpty())
    {
        const auto columns = panel.reduced (10.0f, 0.0f);
        const auto columnWidth = columns.getWidth() / 6.0f;
        const auto headingY = panel.getY() + 9.0f;
        g.setFont (juce::FontOptions (9.5f).withStyle ("Bold"));
        g.setColour (textSecondary.withAlpha (0.72f));
        g.drawText ("POSITION", juce::Rectangle<float> (columns.getX(), headingY, 3.0f * columnWidth, 15.0f),
                    juce::Justification::centred);
        g.drawText ("SPACE", juce::Rectangle<float> (columns.getX() + 3.0f * columnWidth, headingY,
                                                       2.0f * columnWidth, 15.0f),
                    juce::Justification::centred);
        g.drawText ("LEVEL", juce::Rectangle<float> (columns.getX() + 5.0f * columnWidth, headingY,
                                                       columnWidth, 15.0f),
                    juce::Justification::centred);

        g.setColour (sectionAccent.withAlpha (0.86f));
        g.fillRect (columns.getX() + 1.5f * columnWidth - 22.0f, headingY + 17.0f, 44.0f, 1.5f);
        g.fillRect (columns.getX() + 4.0f * columnWidth - 22.0f, headingY + 17.0f, 44.0f, 1.5f);
        g.fillRect (columns.getX() + 5.5f * columnWidth - 22.0f, headingY + 17.0f, 44.0f, 1.5f);

        g.setColour (outline.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (columns.getX() + 3.0f * columnWidth),
                            panel.getY() + 10.0f, panel.getBottom() - 10.0f);
        g.drawVerticalLine (juce::roundToInt (columns.getX() + 5.0f * columnWidth),
                            panel.getY() + 10.0f, panel.getBottom() - 10.0f);
    }
}

void NewProjectAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().reduced (54, 16).removeFromTop (46);
    redoButton.setBounds (header.removeFromRight (34).withSizeKeepingCentre (34, 32));
    header.removeFromRight (7);
    undoButton.setBounds (header.removeFromRight (34).withSizeKeepingCentre (34, 32));
    header.removeFromRight (18);
    headphonesButton.setBounds (header.removeFromRight (116).withSizeKeepingCentre (116, 32));
    header.removeFromRight (8);
    stereoButton.setBounds (header.removeFromRight (84).withSizeKeepingCentre (84, 32));

    auto bounds = getLocalBounds().reduced (54, 0);
    bounds.setY (82);
    bounds.setBottom (getHeight() - 22);
    auto planes = bounds.removeFromTop (420);
    const auto planeSize = juce::jmin (planes.getHeight(), (planes.getWidth() - 20) / 2);
    const auto planeGroupWidth = 2 * planeSize + 20;
    const auto planeGroupX = planes.getCentreX() - planeGroupWidth / 2;
    topPlane.setBounds (planeGroupX, planes.getY(), planeSize, planeSize);
    sidePlane.setBounds (planeGroupX + planeSize + 20, planes.getY(), planeSize, planeSize);
    bounds.removeFromTop (14);

    controlPanelBounds = juce::Rectangle<int> (planeGroupX, bounds.getY(), planeGroupWidth, 150);
    auto controls = controlPanelBounds.reduced (10, 9);
    controls.removeFromTop (27);
    const auto controlWidth = controls.getWidth() / 6;

    auto layoutControl = [&] (juce::Label& label, juce::Slider& slider)
    {
        auto control = controls.removeFromLeft (controlWidth);
        label.setBounds (control.removeFromTop (18));
        const auto sliderWidth = 104;
        slider.setBounds (control.withSizeKeepingCentre (sliderWidth, control.getHeight()));
    };

    layoutControl (panLabel, panSlider);
    layoutControl (frontBackLabel, frontBackSlider);
    layoutControl (heightLabel, heightSlider);
    layoutControl (distanceLabel, distanceSlider);
    layoutControl (roomLabel, roomSlider);
    layoutControl (outputTrimLabel, outputTrimSlider);

    auto modelArea = juce::Rectangle<int> (controlPanelBounds.getX() + 10 + 3 * controlWidth,
                                           controlPanelBounds.getBottom() + 4, 2 * controlWidth, 28)
                         .withSizeKeepingCentre (264, 28);
    distanceModelLabel.setBounds (modelArea.removeFromLeft (132));
    distanceModelSelector.setBounds (modelArea.withSizeKeepingCentre (132, 25));
}

void NewProjectAudioProcessorEditor::timerCallback()
{
    syncRenderModeButtons();
    syncHistoryButtons();
    topPlane.repaint();
    sidePlane.repaint();
}
