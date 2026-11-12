#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto designWidth = 1000.0f;
constexpr auto designHeight = 620.0f;

constexpr juce::uint32 canvasTop = 0xff25262d;
constexpr juce::uint32 canvasBottom = 0xff111218;
constexpr juce::uint32 surfaceTop = 0xff2b2c34;
constexpr juce::uint32 surfaceBottom = 0xff18191f;
constexpr juce::uint32 displayTop = 0xff12151c;
constexpr juce::uint32 displayBottom = 0xff080a0e;
constexpr juce::uint32 analyzerGraphTop = 0xff20242c;
constexpr juce::uint32 analyzerGraphBottom = 0xff141820;
constexpr juce::uint32 textPrimary = 0xffd9dae1;
constexpr juce::uint32 textSecondary = 0xffa6a7b0;
constexpr juce::uint32 valueText = 0xffe6e4db;
constexpr juce::uint32 accentPrimary = 0xffc65350;
constexpr juce::uint32 accentSecondary = 0xff7088b2;
constexpr juce::uint32 controlActive = 0xff4f805f;
constexpr juce::uint32 calibrationIvory = 0xffc7c1ad;
constexpr juce::uint32 agedBrass = 0xffaa985d;
constexpr juce::uint32 outline = 0xff4a4c56;
constexpr juce::uint32 deepShadow = 0xff06070a;
constexpr juce::uint32 hardwareHighlight = 0xff7a7b86;
constexpr juce::uint32 zeroDatum = 0xff817c66;
constexpr std::array<juce::uint32, tbmbc::params::numBands> bandColours {{
    0xffa94b49, 0xff3f7558, 0xff4d6fa8, 0xffc7c1ad
}};
constexpr std::array<const char*, 8> mainControlLabels {{
    "FREQUENCY", "WIDTH", "THRESHOLD", "RANGE", "RATIO", "KNEE",
    "ATTACK", "RELEASE"
}};

float frequencyToX (float frequency, float width) noexcept
{
    const auto normalised = std::log (juce::jlimit (20.0f, 20000.0f, frequency) / 20.0f)
                            / std::log (1000.0f);
    return normalised * width;
}

float xToFrequency (float x, float width) noexcept
{
    if (width <= 0.0f)
        return 1000.0f;
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, x / width));
}

void configureRotary (juce::Slider& slider, const juce::String& tooltip)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    slider.setScrollWheelEnabled (false);
    slider.setTooltip (tooltip);
}

void configureLinear (juce::Slider& slider, const juce::String& tooltip)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    slider.setScrollWheelEnabled (false);
    slider.setTooltip (tooltip);
    slider.setColour (juce::Slider::backgroundColourId,
                      juce::Colour (displayBottom));
    slider.setColour (juce::Slider::trackColourId,
                      juce::Colour (accentSecondary));
    slider.setColour (juce::Slider::thumbColourId,
                      juce::Colour (valueText));
}

juce::Rectangle<int> scaledBounds (juce::Component& component,
                                   juce::Rectangle<float> designBounds)
{
    const auto sx = (float) component.getWidth() / designWidth;
    const auto sy = (float) component.getHeight() / designHeight;
    return { juce::roundToInt (designBounds.getX() * sx),
             juce::roundToInt (designBounds.getY() * sy),
             juce::roundToInt (designBounds.getWidth() * sx),
             juce::roundToInt (designBounds.getHeight() * sy) };
}
}

class TB_MultiBandCompressorAudioProcessorEditor::LookAndFeel final
    : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        knobImage = juce::ImageFileFormat::loadFrom (
            BinaryData::tb_knob_bakelite_png,
            BinaryData::tb_knob_bakelite_pngSize);
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (valueText));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (displayBottom));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (displayBottom));
        setColour (juce::ComboBox::textColourId, juce::Colour (textPrimary));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (outline));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (surfaceBottom));
        setColour (juce::PopupMenu::textColourId, juce::Colour (textPrimary));
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   juce::Colour (accentSecondary).withAlpha (0.82f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosition, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto compact = (bool) slider.getProperties().getWithDefault (
            "compact", false);
        const auto uiScale = static_cast<float> ((double)
            slider.getProperties().getWithDefault ("uiScale", 1.0));
        // JUCE has already removed the TextBoxBelow area from this rectangle.
        const auto available = (float) juce::jmin (width, height)
            - 4.0f * uiScale;
        const auto diameter = juce::jmin (
            (compact ? 32.0f : 50.0f) * uiScale, available);
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width,
                                                     (float) height)
                                .withSizeKeepingCentre (diameter, diameter);
        const auto radius = bounds.getWidth() * 0.5f;
        const auto angle = startAngle + sliderPosition * (endAngle - startAngle);

        g.setColour (juce::Colour (deepShadow).withAlpha (0.82f));
        g.fillEllipse (bounds.expanded (1.8f * uiScale)
                            .translated (0.0f, 2.8f * uiScale));

        const auto imageBounds = bounds.expanded (1.2f * uiScale);
        if (knobImage.isValid())
        {
            juce::Graphics::ScopedSaveState imageState (g);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (knobImage, imageBounds,
                         juce::RectanglePlacement::stretchToFit);

            auto capColour = juce::Colour (calibrationIvory);
            const auto title = slider.getTitle();
            if (title == "THRESHOLD" || title == "RANGE"
                || title == "RATIO" || title == "KNEE")
                capColour = juce::Colour (bandColours[1]);
            else if (title == "FREQUENCY" || title == "WIDTH")
                capColour = juce::Colour (bandColours[2]);
            else if (title == "ATTACK" || title == "RELEASE")
                capColour = juce::Colour (bandColours[0]);
            g.setColour (capColour.withAlpha (slider.isEnabled() ? 0.62f : 0.12f));
            g.fillEllipse (bounds.reduced (radius * 0.31f));
        }
        else
        {
            juce::ColourGradient face (juce::Colour (hardwareHighlight),
                                       imageBounds.getX(), imageBounds.getY(),
                                       juce::Colour (deepShadow),
                                       imageBounds.getRight(), imageBounds.getBottom(), false);
            g.setGradientFill (face);
            g.fillEllipse (imageBounds);
        }

        constexpr int tickCount = 11;
        for (int tick = 0; tick < tickCount; ++tick)
        {
            const auto proportion = (float) tick / (float) (tickCount - 1);
            const auto tickAngle = startAngle + proportion * (endAngle - startAngle);
            const auto centre = bounds.getCentre();
            const auto innerRadius = radius - 1.8f * uiScale;
            const auto outerRadius = radius + 0.8f * uiScale;
            const auto inner = juce::Point<float> (
                centre.x + std::sin (tickAngle) * innerRadius,
                centre.y - std::cos (tickAngle) * innerRadius);
            const auto outer = juce::Point<float> (
                centre.x + std::sin (tickAngle) * outerRadius,
                centre.y - std::cos (tickAngle) * outerRadius);
            const auto lit = proportion <= sliderPosition + 0.001f;
            g.setColour (lit ? juce::Colour (agedBrass).withAlpha (0.92f)
                             : juce::Colour (calibrationIvory).withAlpha (0.40f));
            g.drawLine ({ inner, outer }, (lit ? 1.35f : 0.9f) * uiScale);
        }

        const auto pointerStart = radius * 0.60f;
        const auto pointerLength = radius * 0.88f;
        const auto centre = bounds.getCentre();
        const auto pointerBase = juce::Point<float> (
            centre.x + std::sin (angle) * pointerStart,
            centre.y - std::cos (angle) * pointerStart);
        g.setColour (juce::Colour (deepShadow).withAlpha (0.82f));
        g.drawLine (pointerBase.x + uiScale, pointerBase.y + uiScale,
                    centre.x + std::sin (angle) * pointerLength + uiScale,
                    centre.y - std::cos (angle) * pointerLength + uiScale,
                     3.2f * uiScale);
        g.setColour (juce::Colour (agedBrass));
        g.drawLine (pointerBase.x, pointerBase.y,
                    centre.x + std::sin (angle) * pointerLength,
                    centre.y - std::cos (angle) * pointerLength,
                    2.0f * uiScale);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        auto scale = 1.0f;
        if (const auto* parent = label.getParentComponent())
            scale = static_cast<float> ((double)
                parent->getProperties().getWithDefault ("uiScale", 1.0));
        return juce::Font (juce::FontOptions (13.0f * scale));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        const auto scale = juce::jmax (0.7f, (float) box.getHeight() / 26.0f);
        return juce::Font (juce::FontOptions (10.0f * scale,
                                              juce::Font::bold));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted,
                               bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.6f);
        const auto uiScale = juce::jmax (0.7f, bounds.getHeight() / 25.0f);
        auto activeColour = juce::Colour (controlActive);
        const auto role = button.getProperties().getWithDefault (
            "accentRole", juce::var()).toString();
        if (role == "pre")
            activeColour = juce::Colour (accentSecondary);
        else if (role == "delta" || role == "bypass")
            activeColour = juce::Colour (accentPrimary);
        else if (role == "analyzer")
            activeColour = juce::Colour (accentSecondary).interpolatedWith (
                juce::Colour (calibrationIvory), 0.16f);
        auto fill = button.getToggleState() ? activeColour
                                             : juce::Colour (displayBottom);
        if (highlighted)
            fill = fill.brighter (0.10f);
        if (down)
            fill = fill.darker (0.10f);
        g.setColour (juce::Colour (deepShadow).withAlpha (0.6f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.5f * uiScale),
                                5.0f * uiScale);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f * uiScale);
        g.setColour (button.getToggleState() ? activeColour.brighter (0.35f)
                                              : juce::Colour (outline));
        g.drawRoundedRectangle (bounds, 5.0f * uiScale, 1.0f * uiScale);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                          bool, bool) override
    {
        g.setColour (button.isEnabled() ? juce::Colour (textPrimary)
                                         : juce::Colour (textSecondary).withAlpha (0.45f));
        const auto label = button.getButtonText();
        const auto bounds = button.getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto uiScale = juce::jmax (0.7f, bounds.getHeight() / 26.0f);

        if (label == "<" || label == ">")
        {
            const auto direction = label == "<" ? -1.0f : 1.0f;
            juce::Path chevron;
            chevron.startNewSubPath (centre.x - direction * 3.0f * uiScale,
                                     centre.y - 5.0f * uiScale);
            chevron.lineTo (centre.x + direction * 2.0f * uiScale, centre.y);
            chevron.lineTo (centre.x - direction * 3.0f * uiScale,
                            centre.y + 5.0f * uiScale);
            g.strokePath (chevron, juce::PathStrokeType (
                1.8f * uiScale, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
            return;
        }

        if (label == "SAVE")
        {
            const auto icon = bounds.withSizeKeepingCentre (15.0f * uiScale,
                                                            15.0f * uiScale);
            g.drawRoundedRectangle (icon, 1.8f * uiScale, 1.4f * uiScale);
            g.drawRect (icon.getX() + 3.0f * uiScale,
                        icon.getY() + 1.5f * uiScale,
                        icon.getWidth() - 6.0f * uiScale,
                        4.5f * uiScale, 1.2f * uiScale);
            g.drawRoundedRectangle (icon.reduced (3.0f * uiScale,
                                                  2.0f * uiScale)
                                         .withTrimmedTop (6.0f * uiScale),
                                    1.0f * uiScale, 1.2f * uiScale);
            return;
        }

        if (label == "UNDO" || label == "REDO")
        {
            const auto mirror = label == "UNDO" ? 1.0f : -1.0f;
            const auto px = [&] (float offset)
            {
                return centre.x + mirror * offset * uiScale;
            };
            juce::Path arrow;
            arrow.startNewSubPath (px (-5.0f), centre.y - 2.0f * uiScale);
            arrow.cubicTo (px (-1.0f), centre.y - 6.0f * uiScale,
                           px (6.0f), centre.y - 4.0f * uiScale,
                           px (6.0f), centre.y + 4.0f * uiScale);
            arrow.startNewSubPath (px (-6.0f), centre.y - 2.0f * uiScale);
            arrow.lineTo (px (-1.0f), centre.y - 7.0f * uiScale);
            arrow.startNewSubPath (px (-6.0f), centre.y - 2.0f * uiScale);
            arrow.lineTo (px (-1.0f), centre.y + 3.0f * uiScale);
            g.strokePath (arrow, juce::PathStrokeType (
                1.7f * uiScale, juce::PathStrokeType::curved,
                juce::PathStrokeType::rounded));
            return;
        }

        g.setFont (juce::Font (juce::FontOptions (10.0f * uiScale,
                                                  juce::Font::bold)));
        g.drawText (label, button.getLocalBounds().reduced (4),
                    juce::Justification::centred, true);
    }

private:
    juce::Image knobImage;
};

class TB_MultiBandCompressorAudioProcessorEditor::VisibilityWatcher final
    : public juce::ComponentMovementWatcher
{
public:
    explicit VisibilityWatcher (TB_MultiBandCompressorAudioProcessorEditor& editorToWatch)
        : juce::ComponentMovementWatcher (&editorToWatch), editor (editorToWatch)
    {
    }

    void componentMovedOrResized (bool, bool) override {}
    void componentPeerChanged() override { editor.updateVisibleActivity(); }
    void componentVisibilityChanged() override { editor.updateVisibleActivity(); }

private:
    TB_MultiBandCompressorAudioProcessorEditor& editor;
};

class TB_MultiBandCompressorAudioProcessorEditor::BandGraph final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    BandGraph (TB_MultiBandCompressorAudioProcessor& p,
               std::function<void (int)> selectionChanged,
               std::function<void()> deltaChanged)
        : processor (p), onSelectionChanged (std::move (selectionChanged)),
          onDeltaChanged (std::move (deltaChanged))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setComponentID ("bandGraph");
        setTitle ("Interactive multiband dynamics display");
        setDescription ("Smoothed main input spectrum with direct band editing. Click a band to select it. Double-click empty space to enable a band. Drag a node to set frequency and range; drag either coloured band edge or use the wheel over a node to set width. Hold the right mouse button on a band node for momentary Delta Listen and drag horizontally to move that band's frequency.");
        setTooltip ("Drag nodes and coloured edges to edit bands. Hold right-click on an enabled band node for Delta Listen; drag horizontally to move its frequency, then release to stop listening.");
        displayedDetectorMeters.fill (-120.0f);
        displayedInputSpectrum.fill (-120.0f);

        analyzerButton.setClickingTogglesState (true);
        analyzerButton.setComponentID ("analyzerToggle");
        analyzerButton.setToggleState (true, juce::dontSendNotification);
        analyzerButton.getProperties().set ("accentRole", "analyzer");
        analyzerButton.setTooltip ("Show or hide the smoothed main input spectrum.");
        analyzerButton.setWantsKeyboardFocus (false);
        addAndMakeVisible (analyzerButton);
        inputSourceLabel.setText ("INPUT", juce::dontSendNotification);
        inputSourceLabel.setComponentID ("analyzerInputSource");
        inputSourceLabel.setTitle ("Analyzer input source");
        inputSourceLabel.setDescription ("The analyzer displays the main input spectrum.");
        inputSourceLabel.setTooltip ("The spectrum shown is the main input signal.");
        inputSourceLabel.setJustificationType (juce::Justification::centredLeft);
        inputSourceLabel.setColour (juce::Label::textColourId,
                                    juce::Colour (accentSecondary).withAlpha (0.90f));
        inputSourceLabel.setFont (juce::Font (juce::FontOptions (8.2f,
                                                                 juce::Font::bold)));
        inputSourceLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (inputSourceLabel);
        analyzerButton.onClick = [this]
        {
            inputSourceLabel.setVisible (analyzerButton.getToggleState());
            updateAnalyzerDemand();
            repaint();
        };
    }

    ~BandGraph() override
    {
        endDragGesture();
        cancelMomentaryDelta (false);
        processor.setDeltaListenBand (-1);
        setAnalyzerDemand (false);
    }

    void setHostShowing (bool shouldBeShowing)
    {
        hostShowing = shouldBeShowing;
        if (! hostShowing)
        {
            endDragGesture();
            cancelMomentaryDelta (true);
            processor.setDeltaListenBand (-1);
        }
        updateAnalyzerDemand();
    }

    bool updateSpectrum (bool audioRunning)
    {
        if (! analyzerDemandActive)
            return false;

        if (! audioRunning)
        {
            processor.getInputSpectrumAnalyzer().discardPending();
            return clearSpectrumDisplay();
        }

        auto& analyzer = processor.getInputSpectrumAnalyzer();
        const auto changed = analyzer.pullSpectrum()
            && smoothSpectrum (displayedInputSpectrum, analyzer.getMagnitudesDb());
        if (changed)
            rebuildSpectrumPaths();
        return changed;
    }

    void closeAbandonedGesture()
    {
        if (momentaryDeltaBand >= 0
            && ! juce::ModifierKeys::getCurrentModifiersRealtime()
                    .isRightButtonDown())
            releaseMomentaryDelta (true);
        if (dragging
            && (! isEnabled()
                || ! juce::ModifierKeys::getCurrentModifiersRealtime()
                        .isAnyMouseButtonDown()))
            endDragGesture();
    }

    bool isEditing() const noexcept { return dragging; }

    void setSelectedBand (int index)
    {
        const auto clamped = juce::jlimit (0, tbmbc::params::numBands - 1, index);
        if (dragging && clamped != gestureBand)
            endDragGesture();
        selected = clamped;
        repaint();
    }

    void setMeterValues (
        const std::array<float, tbmbc::params::numBands>& gains,
        const std::array<float, tbmbc::params::numBands>& detectors) noexcept
    {
        displayedGainMeters = gains;
        displayedDetectorMeters = detectors;
    }

    void resized() override
    {
        const auto scaleX = (float) getWidth() / designWidth;
        const auto scaleY = (float) getHeight() / designHeight;
        const auto scaled = [scaleX, scaleY] (juce::Rectangle<float> bounds)
        {
            return juce::Rectangle<int> (
                juce::roundToInt (bounds.getX() * scaleX),
                juce::roundToInt (bounds.getY() * scaleY),
                juce::roundToInt (bounds.getWidth() * scaleX),
                juce::roundToInt (bounds.getHeight() * scaleY));
        };
        analyzerButton.setBounds (scaled ({ 884.0f, 6.0f, 82.0f, 24.0f }));
        inputSourceLabel.setBounds (scaled ({ 42.0f, 6.0f, 64.0f, 24.0f }));
        rebuildSpectrumPaths();
    }

    void paint (juce::Graphics& g) override
    {
        const auto scaleX = (float) getWidth() / designWidth;
        const auto scaleY = (float) getHeight() / designHeight;
        const juce::Graphics::ScopedSaveState designSpace (g);
        g.addTransform (juce::AffineTransform::scale (scaleX, scaleY));

        const juce::Rectangle<float> bounds { 0.0f, 0.0f,
                                              designWidth, designHeight };
        juce::ColourGradient glass (juce::Colour (displayTop), 0.0f, 0.0f,
                                    juce::Colour (displayBottom), 0.0f,
                                    bounds.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (bounds, 8.0f);

        const auto plot = getPlotBounds();
        juce::ColourGradient graphBackground (
            juce::Colour (analyzerGraphTop), plot.getX(), plot.getY(),
            juce::Colour (analyzerGraphBottom), plot.getX(), plot.getBottom(), false);
        g.setGradientFill (graphBackground);
        g.fillRect (plot);

        for (int band = 0; band < tbmbc::params::numBands; ++band)
            drawBandZone (g, plot, band, false);

        g.setColour (juce::Colour (outline).withAlpha (0.38f));
        const std::array<float, 10> frequencies { 20.0f, 50.0f, 100.0f, 200.0f,
                                                  500.0f, 1000.0f, 2000.0f,
                                                  5000.0f, 10000.0f, 20000.0f };
        for (auto frequency : frequencies)
        {
            const auto x = plot.getX() + frequencyToX (frequency, plot.getWidth());
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            if (frequency == 20.0f || frequency == 100.0f || frequency == 1000.0f
                || frequency == 10000.0f || frequency == 20000.0f)
            {
                g.setColour (juce::Colour (textSecondary).withAlpha (0.68f));
                const auto label = frequency >= 1000.0f
                    ? juce::String (frequency / 1000.0f, frequency == 1000.0f ? 0 : 0) + "k"
                    : juce::String ((int) frequency);
                g.setFont (juce::Font (juce::FontOptions (8.5f)));
                g.drawText (label, juce::roundToInt (x - 18.0f),
                            juce::roundToInt (plot.getBottom() - 13.0f), 36, 12,
                            juce::Justification::centred, false);
                g.setColour (juce::Colour (outline).withAlpha (0.38f));
            }
        }

        for (const auto responseDb : { 12.0f, 6.0f, 0.0f, -24.0f, -48.0f })
        {
            const auto y = responseDbToY (responseDb);
            if (responseDb != 0.0f)
            {
                g.setColour (juce::Colour (outline).withAlpha (0.25f));
                g.drawHorizontalLine (juce::roundToInt (y), plot.getX(),
                                      plot.getRight());
            }

            g.setColour (juce::Colour (textSecondary).withAlpha (0.60f));
            g.setFont (juce::Font (juce::FontOptions (7.8f, juce::Font::bold)));
            const auto label = responseDb > 0.0f
                ? "+" + juce::String ((int) responseDb)
                : juce::String ((int) responseDb)
                      + (responseDb == 0.0f ? " dB" : "");
            g.drawText (label, juce::Rectangle<float> (4.0f, y - 6.0f,
                                                       34.0f, 12.0f),
                        juce::Justification::centredRight, false);
        }

        if (analyzerButton.getToggleState())
        {
            drawSpectrum (g, inputSpectrumPaths,
                          juce::Colour (accentSecondary), 0.095f, 1.3f);
            g.setColour (juce::Colour (textSecondary).withAlpha (0.58f));
            g.setFont (juce::Font (juce::FontOptions (7.5f, juce::Font::bold)));
            g.drawText ("0 dBFS", juce::Rectangle<float> (
                            plot.getRight() - 52.0f, plot.getY() + 3.0f,
                            48.0f, 14.0f),
                        juce::Justification::centredRight, false);
        }

        for (int band = 0; band < tbmbc::params::numBands; ++band)
            drawBandZone (g, plot, band, true);

        drawGainContour (g, plot);
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (band != selected)
                drawBand (g, plot, band, BandDrawLayer::node);
        drawBand (g, plot, selected, BandDrawLayer::node);

        const auto selectedWantsExternal = isBandEnabled (selected)
            && getValue (selected, tbmbc::params::sourceSuffix) >= 0.5f;
        const auto externalMissing = selectedWantsExternal
            && ! processor.isExternalSidechainBusActiveForBand (selected);
        if (externalMissing)
        {
            g.setColour (juce::Colour (displayBottom).withAlpha (0.90f));
            g.fillRoundedRectangle (bounds.getRight() - 314.0f, 7.0f, 106.0f, 24.0f, 4.0f);
            g.setColour (juce::Colour (textSecondary));
            g.setFont (juce::Font (juce::FontOptions (8.8f, juce::Font::bold)));
            g.drawText ("NO SC INPUT",
                        juce::Rectangle<float> (bounds.getRight() - 310.0f, 9.0f,
                                                98.0f, 20.0f),
                        juce::Justification::centred, false);
        }

        g.setColour (juce::Colour (outline));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto plot = getPlotBounds();
        const auto position = toDesignPoint (event.position);
        if (! isInsidePlot (plot, position))
            return;

        if (event.mods.isRightButtonDown())
        {
            auto closestBand = -1;
            auto closestDistance = 28.0f;
            for (int band = 0; band < tbmbc::params::numBands; ++band)
            {
                if (! isBandEnabled (band))
                    continue;
                const auto distance = position.getDistanceFrom (
                    getBandGeometry (plot, band).node);
                if (distance <= closestDistance)
                {
                    closestBand = band;
                    closestDistance = distance;
                }
            }
            if (closestBand < 0)
                return;

            endDragGesture();
            selected = closestBand;
            if (momentaryDeltaBand < 0)
                preHoldDeltaBand = processor.getDeltaListenBand();
            momentaryDeltaBand = selected;
            processor.setDeltaListenBand (selected);
            if (onSelectionChanged)
                onSelectionChanged (selected);
            if (onDeltaChanged)
                onDeltaChanged();
            dragMode = DragMode::frequencyOnly;
            beginUndoTransaction (
                "Move Band " + juce::String (selected + 1) + " Frequency");
            beginDragGesture();
            return;
        }

        if (! event.mods.isLeftButtonDown())
            return;

        auto closestBand = -1;
        auto closestDistance = 1.0e6f;
        auto hitMode = DragMode::bandCentre;

        const auto evaluateBand = [&] (int band)
        {
            if (! isBandEnabled (band))
                return;
            const auto geometry = getBandGeometry (plot, band);
            const auto centreDistance = position.getDistanceFrom (geometry.node);
            if (centreDistance < closestDistance)
            {
                closestDistance = centreDistance;
                closestBand = band;
                hitMode = DragMode::bandCentre;
            }
            const auto leftDistance = std::abs (position.x - geometry.leftX);
            const auto rightDistance = std::abs (position.x - geometry.rightX);
            if (centreDistance > 12.0f
                && position.y >= plot.getY() && position.y <= plot.getBottom()
                && juce::jmin (leftDistance, rightDistance) < 9.0f
                && juce::jmin (leftDistance, rightDistance) < closestDistance)
            {
                closestDistance = juce::jmin (leftDistance, rightDistance);
                closestBand = band;
                hitMode = DragMode::bandWidth;
            }
        };

        evaluateBand (selected);
        if (closestDistance > 28.0f)
        {
            closestBand = -1;
            closestDistance = 1.0e6f;
            for (int band = 0; band < tbmbc::params::numBands; ++band)
                if (band != selected)
                    evaluateBand (band);
        }

        if (closestBand < 0 || closestDistance > 28.0f)
            return;

        endDragGesture();
        selected = closestBand;
        dragMode = hitMode;
        if (onSelectionChanged)
            onSelectionChanged (selected);
        beginUndoTransaction ("Edit Band " + juce::String (selected + 1));
        beginDragGesture();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (momentaryDeltaBand >= 0)
        {
            if (! event.mods.isRightButtonDown())
            {
                releaseMomentaryDelta (true);
                endDragGesture();
                return;
            }

            const auto plot = getPlotBounds();
            const auto position = toDesignPoint (event.position);
            setDraggedParameter (
                tbmbc::params::frequencySuffix,
                xToFrequency (position.x - plot.getX(), plot.getWidth()));
            repaint();
            return;
        }

        if (! dragging || gestureBand < 0)
            return;
        const auto plot = getPlotBounds();
        const auto position = toDesignPoint (event.position);
        if (dragMode == DragMode::bandCentre)
        {
            const auto frequency = xToFrequency (position.x - plot.getX(), plot.getWidth());
            setDraggedParameter (tbmbc::params::frequencySuffix, frequency);
            setDraggedParameter (tbmbc::params::rangeSuffix,
                                 responseYToDb (position.y));
        }
        else if (dragMode == DragMode::bandWidth)
        {
            const auto centreFrequency = getValue (
                gestureBand, tbmbc::params::frequencySuffix);
            const auto edgeFrequency = xToFrequency (position.x - plot.getX(), plot.getWidth());
            const auto octaves = 2.0f * std::abs (std::log2 (
                juce::jmax (20.0f, edgeFrequency) / juce::jmax (20.0f, centreFrequency)));
            setDraggedParameter (tbmbc::params::bandwidthSuffix,
                                 juce::jlimit (0.25f, 4.0f, octaves));
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        releaseMomentaryDelta (true);
        endDragGesture();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const auto plot = getPlotBounds();
        const auto position = toDesignPoint (event.position);
        if (! event.mods.isLeftButtonDown() || ! isInsidePlot (plot, position))
            return;

        for (int existing = 0; existing < tbmbc::params::numBands; ++existing)
        {
            if (! isBandEnabled (existing))
                continue;
            const auto geometry = getBandGeometry (plot, existing);
            const auto occupied = juce::Rectangle<float> (
                geometry.leftX - 9.0f,
                juce::jmin (geometry.node.y, gainZeroY) - 9.0f,
                geometry.rightX - geometry.leftX + 18.0f,
                std::abs (geometry.node.y - gainZeroY) + 18.0f);
            if (occupied.contains (position)
                || position.getDistanceFrom (geometry.node) <= 28.0f)
                return;
        }

        auto band = firstDisabledBand();
        if (band < 0)
            return;
        beginUndoTransaction ("Create Band " + juce::String (band + 1));
        processor.setParameterActual (
            tbmbc::params::bandId (band, tbmbc::params::frequencySuffix),
            xToFrequency (position.x - plot.getX(), plot.getWidth()));
        processor.setParameterActual (
            tbmbc::params::bandId (band, tbmbc::params::rangeSuffix),
            responseYToDb (position.y));
        processor.setParameterActual (
            tbmbc::params::bandId (band, tbmbc::params::enabledSuffix), 1.0f);
        commitUndoTransaction();
        selected = band;
        if (onSelectionChanged)
            onSelectionChanged (selected);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override
    {
        const auto plot = getPlotBounds();
        const auto position = toDesignPoint (event.position);
        if (! isInsidePlot (plot, position)
            || std::abs (wheel.deltaY) <= 1.0e-6f)
            return;

        auto hoveredBand = -1;
        auto closestDistance = 28.0f;
        const auto selectedGeometry = getBandGeometry (plot, selected);
        if (isBandEnabled (selected)
            && position.getDistanceFrom (selectedGeometry.node) <= closestDistance)
        {
            hoveredBand = selected;
            closestDistance = position.getDistanceFrom (selectedGeometry.node);
        }
        if (hoveredBand < 0)
            for (int band = 0; band < tbmbc::params::numBands; ++band)
            {
                if (! isBandEnabled (band))
                    continue;
                const auto distance = position.getDistanceFrom (
                    getBandGeometry (plot, band).node);
                if (distance <= closestDistance)
                {
                    hoveredBand = band;
                    closestDistance = distance;
                }
            }
        if (hoveredBand < 0)
            return;
        if (hoveredBand != selected)
        {
            selected = hoveredBand;
            if (onSelectionChanged)
                onSelectionChanged (selected);
        }
        beginUndoTransaction (
            "Band " + juce::String (selected + 1) + " Width");
        const auto current = getValue (selected, tbmbc::params::bandwidthSuffix);
        processor.setParameterActual (
            tbmbc::params::bandId (selected, tbmbc::params::bandwidthSuffix),
            juce::jlimit (0.25f, 4.0f, current + wheel.deltaY * 0.35f));
        commitUndoTransaction();
        repaint();
    }

private:
    static constexpr float designWidth = 976.0f;
    static constexpr float designHeight = 499.0f;
    static constexpr float gainAxisTopY = 38.0f;
    static constexpr float gainAxisBottomY = 338.0f;
    static constexpr float gainZeroY = (gainAxisTopY + gainAxisBottomY) * 0.5f;
    static constexpr float gainHalfRangePixels =
        (gainAxisBottomY - gainAxisTopY) * 0.5f;

    enum class DragMode { bandCentre, bandWidth, frequencyOnly };
    enum class BandDrawLayer { node };
    struct Geometry
    {
        juce::Point<float> node;
        float leftX = 0.0f;
        float rightX = 0.0f;
    };

    struct SpectrumPaths
    {
        juce::Path line;
        juce::Path fill;
    };

    float getValue (int band, const char* suffix) const
    {
        if (const auto* raw = processor.getValueTreeState().getRawParameterValue (
                tbmbc::params::bandId (band, suffix)))
            return raw->load();
        return 0.0f;
    }

    bool isBandEnabled (int band) const
    {
        return getValue (band, tbmbc::params::enabledSuffix) >= 0.5f;
    }

    int firstDisabledBand() const
    {
        for (int band = 0; band < tbmbc::params::numBands; ++band)
            if (! isBandEnabled (band))
                return band;
        return -1;
    }

    juce::Rectangle<float> getPlotBounds() const
    {
        return { 42.0f, gainAxisTopY, 892.0f,
                 gainAxisBottomY - gainAxisTopY };
    }

    static bool isInsidePlot (juce::Rectangle<float> plot,
                              juce::Point<float> point) noexcept
    {
        return point.x >= plot.getX() && point.x <= plot.getRight()
            && point.y >= plot.getY() && point.y <= plot.getBottom();
    }

    static float responseDbToY (float db) noexcept
    {
        const auto clamped = juce::jlimit (-48.0f, 12.0f, db);
        return clamped >= 0.0f
            ? gainZeroY - (clamped / 12.0f) * gainHalfRangePixels
            : gainZeroY - (clamped / 48.0f) * gainHalfRangePixels;
    }

    static float responseYToDb (float y) noexcept
    {
        const auto clamped = juce::jlimit (gainAxisTopY, gainAxisBottomY, y);
        return clamped <= gainZeroY
            ? ((gainZeroY - clamped) / gainHalfRangePixels) * 12.0f
            : -((clamped - gainZeroY) / gainHalfRangePixels) * 48.0f;
    }

    static float spectrumDbToY (float db) noexcept
    {
        const auto clamped = juce::jlimit (-96.0f, 0.0f, db);
        return gainAxisTopY + (-clamped / 96.0f)
            * (gainAxisBottomY - gainAxisTopY);
    }

    juce::Point<float> toDesignPoint (juce::Point<float> point) const noexcept
    {
        if (getWidth() <= 0 || getHeight() <= 0)
            return point;
        return { point.x * designWidth / (float) getWidth(),
                 point.y * designHeight / (float) getHeight() };
    }

    static bool smoothSpectrum (
        std::array<float, TBMBCSpectrumAnalyzer::numBins>& displayed,
        const std::array<float, TBMBCSpectrumAnalyzer::numBins>& measured) noexcept
    {
        auto changed = false;
        for (std::size_t bin = 0; bin < displayed.size(); ++bin)
        {
            const auto target = std::isfinite (measured[bin])
                ? juce::jlimit (-120.0f, 0.0f, measured[bin]) : -120.0f;
            const auto coefficient = displayed[bin] <= -119.0f
                ? 1.0f : (target > displayed[bin] ? 0.78f : 0.24f);
            const auto next = displayed[bin] + coefficient * (target - displayed[bin]);
            changed = changed || std::abs (next - displayed[bin]) > 0.04f;
            displayed[bin] = next;
        }
        return changed;
    }

    bool clearSpectrumDisplay()
    {
        const auto hadVisibleData = std::any_of (
            displayedInputSpectrum.begin(), displayedInputSpectrum.end(),
            [] (float value) { return value > -119.9f; });
        if (! hadVisibleData)
            return false;
        displayedInputSpectrum.fill (-120.0f);
        rebuildSpectrumPaths();
        return true;
    }

    void buildSpectrumPaths (
        const std::array<float, TBMBCSpectrumAnalyzer::numBins>& levels,
        SpectrumPaths& paths) const
    {
        const auto plot = getPlotBounds();
        const auto sampleRate = processor.getSampleRate();
        paths.line.clear();
        paths.fill.clear();
        if (sampleRate <= 0.0 || plot.getWidth() <= 1.0f)
            return;

        const auto pointCount = juce::jlimit (96, 512,
                                              juce::roundToInt (plot.getWidth() * 0.55f));
        std::array<float, 512> smoothedLevels {};
        paths.line.preallocateSpace (pointCount * 3);
        for (int point = 0; point < pointCount; ++point)
        {
            const auto proportion = (float) point / (float) (pointCount - 1);
            const auto frequency = 20.0f * std::pow (1000.0f, proportion);
            const auto exactBin = frequency * (float) TBMBCSpectrumAnalyzer::fftSize
                                / (float) sampleRate;
            const auto lower = juce::jlimit (
                0, TBMBCSpectrumAnalyzer::numBins - 1,
                (int) std::floor (exactBin));
            const auto upper = juce::jmin (TBMBCSpectrumAnalyzer::numBins - 1,
                                           lower + 1);
            const auto fraction = juce::jlimit (0.0f, 1.0f,
                                                exactBin - (float) lower);
            smoothedLevels[(std::size_t) point] = juce::jlimit (
                -96.0f, 0.0f,
                levels[(std::size_t) lower]
                    + fraction * (levels[(std::size_t) upper]
                                  - levels[(std::size_t) lower]));
        }

        // Symmetric log-frequency smoothing keeps the display readable without
        // moving spectral peaks left or right.
        constexpr float spatialBlend = 0.42f;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int point = 1; point < pointCount; ++point)
                smoothedLevels[(std::size_t) point] =
                    spatialBlend * smoothedLevels[(std::size_t) point]
                    + (1.0f - spatialBlend)
                          * smoothedLevels[(std::size_t) (point - 1)];
            for (int point = pointCount - 2; point >= 0; --point)
                smoothedLevels[(std::size_t) point] =
                    spatialBlend * smoothedLevels[(std::size_t) point]
                    + (1.0f - spatialBlend)
                          * smoothedLevels[(std::size_t) (point + 1)];
        }

        for (int point = 0; point < pointCount; ++point)
        {
            const auto proportion = (float) point / (float) (pointCount - 1);
            const auto x = plot.getX() + proportion * plot.getWidth();
            const auto y = spectrumDbToY (smoothedLevels[(std::size_t) point]);
            if (point == 0)
                paths.line.startNewSubPath (x, y);
            else
                paths.line.lineTo (x, y);
        }

        paths.fill = paths.line;
        paths.fill.lineTo (plot.getRight(), spectrumDbToY (-96.0f));
        paths.fill.lineTo (plot.getX(), spectrumDbToY (-96.0f));
        paths.fill.closeSubPath();
    }

    void rebuildSpectrumPaths()
    {
        buildSpectrumPaths (displayedInputSpectrum, inputSpectrumPaths);
    }

    static void drawSpectrum (
        juce::Graphics& g, const SpectrumPaths& paths,
        juce::Colour colour, float fillAlpha, float lineWidth)
    {
        if (paths.line.isEmpty())
            return;
        if (fillAlpha > 0.0f)
        {
            g.setColour (colour.withAlpha (fillAlpha));
            g.fillPath (paths.fill);
        }
        g.setColour (colour.withAlpha (0.64f));
        g.strokePath (paths.line, juce::PathStrokeType (lineWidth));
    }

    void updateAnalyzerDemand()
    {
        setAnalyzerDemand (hostShowing && analyzerButton.getToggleState());
    }

    void setAnalyzerDemand (bool shouldAnalyze)
    {
        if (shouldAnalyze == analyzerDemandActive)
            return;
        analyzerDemandActive = shouldAnalyze;
        if (shouldAnalyze)
        {
            // Consume any deferred prepare/reset request on the GUI thread before
            // the audio callback starts filling the display FIFO again.
            processor.getInputSpectrumAnalyzer().pullSpectrum();
            processor.setSpectrumAnalyzerDemand (true);
        }
        else
        {
            processor.setSpectrumAnalyzerDemand (false);
            processor.getInputSpectrumAnalyzer().discardPending();
            clearSpectrumDisplay();
        }
    }

    Geometry getBandGeometry (juce::Rectangle<float> plot, int band) const
    {
        const auto frequency = getValue (band, tbmbc::params::frequencySuffix);
        const auto bandwidth = getValue (band, tbmbc::params::bandwidthSuffix);
        const auto range = getValue (band, tbmbc::params::rangeSuffix);
        const auto ratio = std::pow (2.0f, bandwidth * 0.5f);
        const auto centreX = plot.getX() + frequencyToX (frequency, plot.getWidth());
        const auto nodeY = responseDbToY (range);
        return { { centreX, nodeY },
                 plot.getX() + frequencyToX (frequency / ratio, plot.getWidth()),
                 plot.getX() + frequencyToX (frequency * ratio, plot.getWidth()) };
    }

    void drawBandZone (juce::Graphics& g, juce::Rectangle<float> plot,
                       int band, bool edgesOnly)
    {
        if (! isBandEnabled (band))
            return;
        const auto geometry = getBandGeometry (plot, band);
        const auto colour = juce::Colour (bandColours[(std::size_t) band]);
        const auto selectedZone = band == selected;
        if (! edgesOnly)
        {
            const auto left = juce::jlimit (plot.getX(), plot.getRight(),
                                            geometry.leftX);
            const auto right = juce::jlimit (plot.getX(), plot.getRight(),
                                             geometry.rightX);
            if (right > left)
            {
                juce::ColourGradient zone (
                    colour.withAlpha (0.015f), left, plot.getCentreY(),
                    colour.withAlpha (0.015f), right, plot.getCentreY(), false);
                zone.addColour (0.5, colour.withAlpha (
                    selectedZone ? 0.11f : 0.075f));
                g.setGradientFill (zone);
                g.fillRect (juce::Rectangle<float> (
                    left, plot.getY(), right - left, plot.getHeight()));
            }
            return;
        }

        g.setColour (colour.withAlpha (selectedZone ? 0.62f : 0.34f));
        const auto thickness = selectedZone ? 1.3f : 0.9f;
        g.drawLine (geometry.leftX, plot.getY(), geometry.leftX,
                    plot.getBottom(), thickness);
        g.drawLine (geometry.rightX, plot.getY(), geometry.rightX,
                    plot.getBottom(), thickness);
    }

    void drawBand (juce::Graphics& g, juce::Rectangle<float> plot, int band,
                   BandDrawLayer layer)
    {
        const auto enabled = isBandEnabled (band);
        const auto geometry = getBandGeometry (plot, band);
        const auto isSelected = band == selected;
        const auto colour = juce::Colour (bandColours[(std::size_t) band]);

        if (! enabled)
        {
            if (layer == BandDrawLayer::node)
            {
                g.setColour (juce::Colour (outline).withAlpha (0.52f));
                g.drawEllipse (juce::Rectangle<float> (geometry.node.x - 5.0f,
                                                       geometry.node.y - 5.0f,
                                                       10.0f, 10.0f), 1.0f);
            }
            return;
        }

        g.setColour (juce::Colour (deepShadow));
        g.fillEllipse (geometry.node.x - 8.0f, geometry.node.y - 8.0f, 16.0f, 16.0f);
        g.setColour (colour);
        g.fillEllipse (geometry.node.x - 6.0f, geometry.node.y - 6.0f, 12.0f, 12.0f);
        if (isSelected)
        {
            g.setColour (juce::Colour (valueText).withAlpha (0.94f));
            g.drawEllipse (geometry.node.x - 8.0f, geometry.node.y - 8.0f,
                           16.0f, 16.0f, 1.25f);
        }
        if (processor.getDeltaListenBand() == band)
        {
            g.setColour (juce::Colour (accentPrimary).withAlpha (0.98f));
            g.drawEllipse (geometry.node.x - 10.5f, geometry.node.y - 10.5f,
                           21.0f, 21.0f, 2.0f);
        }
        g.setColour (juce::Colour (valueText));
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        g.drawText (juce::String (band + 1),
                    juce::Rectangle<float> (geometry.node.x - 8.0f,
                                            geometry.node.y - 8.0f, 16.0f, 16.0f),
                    juce::Justification::centred, false);
    }

    // Range remains visible at each band's node. The single graph contour is the
    // measured DSP gain: at idle it coincides with the 0 dB datum, then moves as
    // compression or expansion is applied.
    void drawGainContour (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        constexpr int points = 256;
        juce::Path liveLine;

        for (int point = 0; point < points; ++point)
        {
            const auto proportion = (float) point / (float) (points - 1);
            const auto x = plot.getX() + proportion * plot.getWidth();
            auto actualGain = 0.0f;
            for (int band = 0; band < tbmbc::params::numBands; ++band)
            {
                if (! isBandEnabled (band))
                    continue;
                const auto geometry = getBandGeometry (plot, band);
                if (x < geometry.leftX || x > geometry.rightX)
                    continue;
                const auto leftSide = x <= geometry.node.x;
                const auto denominator = leftSide
                    ? geometry.node.x - geometry.leftX
                    : geometry.rightX - geometry.node.x;
                if (denominator <= 1.0e-4f)
                    continue;
                const auto phase = leftSide
                    ? (x - geometry.leftX) / denominator
                    : (x - geometry.node.x) / denominator;
                const auto weight = leftSide
                    ? 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * phase)
                    : 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * phase);
                actualGain += displayedGainMeters[(std::size_t) band] * weight;
            }

            const auto y = responseDbToY (actualGain);
            if (point == 0)
                liveLine.startNewSubPath (x, y);
            else
                liveLine.lineTo (x, y);
        }

        g.setColour (juce::Colour (zeroDatum));
        g.strokePath (liveLine, juce::PathStrokeType (
            3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::RangedAudioParameter* getParameter (int band, const char* suffix)
    {
        return processor.getValueTreeState().getParameter (
            tbmbc::params::bandId (band, suffix));
    }

    void beginDragGesture()
    {
        gestureBand = selected;
        dragging = true;
        if (dragMode == DragMode::bandCentre)
        {
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::frequencySuffix))
                parameter->beginChangeGesture();
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::rangeSuffix))
                parameter->beginChangeGesture();
        }
        else if (dragMode == DragMode::bandWidth)
        {
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::bandwidthSuffix))
                parameter->beginChangeGesture();
        }
        else if (auto* parameter = getParameter (
                     gestureBand, tbmbc::params::frequencySuffix))
            parameter->beginChangeGesture();
    }

    void endDragGesture()
    {
        if (! dragging)
            return;
        if (dragMode == DragMode::bandCentre)
        {
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::frequencySuffix))
                parameter->endChangeGesture();
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::rangeSuffix))
                parameter->endChangeGesture();
        }
        else if (dragMode == DragMode::bandWidth)
        {
            if (auto* parameter = getParameter (
                    gestureBand, tbmbc::params::bandwidthSuffix))
                parameter->endChangeGesture();
        }
        else if (auto* parameter = getParameter (
                     gestureBand, tbmbc::params::frequencySuffix))
            parameter->endChangeGesture();
        commitUndoTransaction();
        dragging = false;
        gestureBand = -1;
    }

    void beginUndoTransaction (const juce::String& name)
    {
        jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
        (void) processor.getValueTreeState().copyState();
        processor.getUndoManager().beginNewTransaction (name);
    }

    void commitUndoTransaction()
    {
        jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
        (void) processor.getValueTreeState().copyState();
    }

    void setDraggedParameter (const char* suffix, float actualValue)
    {
        if (auto* parameter = getParameter (gestureBand, suffix))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (actualValue));
    }

    void releaseMomentaryDelta (bool notify)
    {
        if (momentaryDeltaBand < 0)
            return;
        const auto heldBand = momentaryDeltaBand;
        momentaryDeltaBand = -1;
        if (processor.getDeltaListenBand() == heldBand)
            processor.setDeltaListenBand (preHoldDeltaBand);
        preHoldDeltaBand = -1;
        if (notify && onDeltaChanged)
            onDeltaChanged();
    }

    void cancelMomentaryDelta (bool notify)
    {
        if (momentaryDeltaBand < 0)
            return;
        momentaryDeltaBand = -1;
        preHoldDeltaBand = -1;
        processor.setDeltaListenBand (-1);
        if (notify && onDeltaChanged)
            onDeltaChanged();
    }

    TB_MultiBandCompressorAudioProcessor& processor;
    std::function<void (int)> onSelectionChanged;
    std::function<void()> onDeltaChanged;
    int selected = 0;
    int gestureBand = -1;
    int momentaryDeltaBand = -1;
    int preHoldDeltaBand = -1;
    bool dragging = false;
    bool hostShowing = false;
    bool analyzerDemandActive = false;
    DragMode dragMode = DragMode::bandCentre;
    juce::TextButton analyzerButton { "ANALYZER" };
    juce::Label inputSourceLabel;
    std::array<float, tbmbc::params::numBands> displayedGainMeters {};
    std::array<float, tbmbc::params::numBands> displayedDetectorMeters {};
    std::array<float, TBMBCSpectrumAnalyzer::numBins> displayedInputSpectrum {};
    SpectrumPaths inputSpectrumPaths;
};

class TB_MultiBandCompressorAudioProcessorEditor::BandPanel final
    : public juce::Component
{
public:
    BandPanel (TB_MultiBandCompressorAudioProcessor& p,
               std::function<void (int)> selectionChanged,
               std::function<void()> deltaChanged)
        : processor (p), onBandSelected (std::move (selectionChanged)),
          onDeltaChanged (std::move (deltaChanged))
    {
        setComponentID ("bandPanel");
        for (int index = 0; index < tbmbc::params::numBands; ++index)
        {
            auto& selector = bandSelectors[static_cast<std::size_t> (index)];
            selector.setButtonText (juce::String (index + 1));
            selector.setTooltip ("Select band " + juce::String (index + 1));
            selector.onClick = [this, index]
            {
                if (onBandSelected)
                    onBandSelected (index);
            };
            addAndMakeVisible (selector);
        }

        for (std::size_t index = 0; index < mainSliders.size(); ++index)
        {
            auto* slider = mainSliders[index];
            configureRotary (*slider, "Double-click the value to type an exact setting.");
            slider->setTitle (mainControlLabels[index]);
            addAndMakeVisible (*slider);
        }
        // Stereo Link remains attached for host automation and state compatibility,
        // but is intentionally not added as a visible or hit-testable child.
        stereoLink.setComponentID ("bandStereoLink");

        modeBox.addItemList ({ "COMP", "EXPAND" }, 1);
        modeBox.setComponentID ("bandDynamicsMode");
        modeBox.setTooltip ("Choose compression or expansion. The sign of Range selects downward or upward direction.");
        addAndMakeVisible (modeBox);
        channelModeBox.addItemList (tbmbc::params::channelModeChoices(), 1);
        channelModeBox.setComponentID ("bandChannelMode");
        channelModeBox.setTooltip (
            "Process stereo, Mid, or Side content. Mono hosts use an effective Stereo fallback.");
        channelModeBox.onChange = [this] { syncButtonText(); };
        addAndMakeVisible (channelModeBox);

        for (auto* button : parameterButtons)
        {
            button->setClickingTogglesState (true);
            addAndMakeVisible (*button);
        }
        auditionButton.setClickingTogglesState (true);
        addAndMakeVisible (auditionButton);
        enabledButton.setComponentID ("bandEnabled");
        sourceButton.setComponentID ("bandSidechainSource");
        auditionButton.setComponentID ("bandDeltaListen");
        sourceButton.getProperties().set ("accentRole", "pre");
        auditionButton.getProperties().set ("accentRole", "delta");
        enabledButton.setTooltip ("Enable this dynamic band without changing the other bands.");
        sourceButton.setTooltip ("SIDECHAIN Off uses the main input as this band's internal detector. On uses the matching auxiliary key: Sidechain 1 to Band 1 through Sidechain 4 to Band 4.");
        auditionButton.setTooltip (
            "Click to latch this band's wet-minus-dry monitor on or off. Hold right-click on a band node for momentary Delta Listen. This state is not saved or automated.");
        enabledButton.onStateChange = [this] { syncButtonText(); };
        sourceButton.onStateChange = [this] { syncButtonText(); };
        auditionButton.onClick = [this]
        {
            processor.setDeltaListenBand (
                processor.getDeltaListenBand() == band ? -1 : band);
            syncButtonText();
            if (onDeltaChanged)
                onDeltaChanged();
        };

        bindToBand (0);
    }

    void bindToBand (int newBand)
    {
        const auto clampedBand = juce::jlimit (0, tbmbc::params::numBands - 1,
                                               newBand);
        for (int index = 0; index < tbmbc::params::numBands; ++index)
            bandSelectors[static_cast<std::size_t> (index)].setToggleState (
                index == clampedBand, juce::dontSendNotification);
        if (clampedBand == band && ! sliderAttachments.empty())
            return;
        band = clampedBand;
        sliderAttachments.clear();
        buttonAttachments.clear();
        modeAttachment.reset();
        channelModeAttachment.reset();

        const std::array<const char*, 10> suffixes {
            tbmbc::params::frequencySuffix, tbmbc::params::bandwidthSuffix,
            tbmbc::params::thresholdSuffix, tbmbc::params::rangeSuffix,
            tbmbc::params::ratioSuffix, tbmbc::params::attackSuffix,
            tbmbc::params::releaseSuffix, tbmbc::params::kneeSuffix,
            tbmbc::params::outputSuffix, tbmbc::params::stereoLinkSuffix
        };
        auto& state = processor.getValueTreeState();
        for (std::size_t index = 0; index < attachmentSliders.size(); ++index)
            sliderAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    state, tbmbc::params::bandId (band, suffixes[index]),
                    *attachmentSliders[index]));

        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, tbmbc::params::bandId (band, tbmbc::params::modeSuffix), modeBox);
        channelModeAttachment =
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                state, tbmbc::params::bandId (band, tbmbc::params::channelModeSuffix),
                channelModeBox);

        const std::array<const char*, 2> buttonSuffixes {
            tbmbc::params::enabledSuffix, tbmbc::params::sourceSuffix
        };
        for (std::size_t index = 0; index < parameterButtons.size(); ++index)
            buttonAttachments.push_back (
                std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    state, tbmbc::params::bandId (band, buttonSuffixes[index]),
                    *parameterButtons[index]));
        syncFromParameters();
        repaint();
    }

    void syncFromParameters()
    {
        if (! isEnabled())
            return;

        auto& state = processor.getValueTreeState();
        const auto readToggle = [&] (const char* suffix)
        {
            if (const auto* value = state.getRawParameterValue (
                    tbmbc::params::bandId (band, suffix)))
                return value->load() >= 0.5f;
            return false;
        };

        enabledButton.setEnabled (true);
        sourceButton.setEnabled (true);
        auditionButton.setEnabled (true);
        enabledButton.setToggleState (
            readToggle (tbmbc::params::enabledSuffix), juce::dontSendNotification);
        sourceButton.setToggleState (
            readToggle (tbmbc::params::sourceSuffix), juce::dontSendNotification);
        syncButtonText();
    }

    void syncButtonText()
    {
        enabledButton.setButtonText (enabledButton.getToggleState() ? "BAND ON" : "BAND OFF");
        sourceButton.setButtonText ("SIDECHAIN");
        auditionButton.setToggleState (processor.getDeltaListenBand() == band,
                                       juce::dontSendNotification);
        auditionButton.setButtonText (auditionButton.getToggleState()
                                          ? "DELTA ON"
                                          : "DELTA");
        const auto bandEnabled = enabledButton.getToggleState();
        for (auto* slider : mainSliders)
            slider->setEnabled (bandEnabled);
        modeBox.setEnabled (bandEnabled);
        channelModeBox.setEnabled (bandEnabled);
        sourceButton.setEnabled (bandEnabled);
        auditionButton.setEnabled (bandEnabled);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto uiScale = juce::jmin ((float) getWidth() / 824.0f,
                                         (float) getHeight() / 120.0f);
        const auto plate = getLocalBounds().toFloat();
        g.setColour (juce::Colour (deepShadow).withAlpha (0.72f));
        g.fillRoundedRectangle (plate.translated (0.0f, 3.0f * uiScale),
                                8.0f * uiScale);
        juce::ColourGradient panel (juce::Colour (surfaceTop), 0.0f, 0.0f,
                                    juce::Colour (surfaceBottom), 0.0f,
                                    plate.getBottom(), false);
        g.setGradientFill (panel);
        g.fillRoundedRectangle (plate, 8.0f * uiScale);
        const auto bandColour = juce::Colour (bandColours[(std::size_t) band]);
        g.setColour (bandColour.withAlpha (0.86f));
        g.drawRoundedRectangle (plate.reduced (0.5f * uiScale),
                                8.0f * uiScale, 1.1f * uiScale);

        g.setColour (juce::Colour (textPrimary));
        g.setFont (juce::Font (juce::FontOptions (10.0f * uiScale,
                                                  juce::Font::bold)));
        g.drawText ("BAND", juce::roundToInt (10.0f * uiScale),
                    juce::roundToInt (8.0f * uiScale),
                    juce::roundToInt (34.0f * uiScale),
                    juce::roundToInt (18.0f * uiScale),
                    juce::Justification::centredLeft, false);

        if (processor.getTotalNumInputChannels() < 2
            && channelModeBox.getSelectedId() > 1)
        {
            g.setColour (juce::Colour (bandColours[(std::size_t) band]));
            g.setFont (juce::Font (juce::FontOptions (7.4f * uiScale,
                                                      juce::Font::bold)));
            g.drawText ("MONO > STEREO", juce::roundToInt (628.0f * uiScale),
                        juce::roundToInt (29.0f * uiScale),
                        juce::roundToInt (184.0f * uiScale),
                        juce::roundToInt (8.0f * uiScale),
                        juce::Justification::centred, false);
        }

        for (std::size_t index = 0; index < mainControlLabels.size(); ++index)
        {
            const auto r = mainSliders[index]->getBounds();
            g.setColour (juce::Colour (textSecondary));
            const auto labelHeight = juce::roundToInt (11.0f * uiScale);
            g.setFont (juce::Font (juce::FontOptions (8.7f * uiScale,
                                                      juce::Font::bold)));
            g.drawText (mainControlLabels[index], r.getX(), r.getY() - labelHeight,
                        r.getWidth(), labelHeight,
                        juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        const auto uiScale = juce::jmin ((float) getWidth() / 824.0f,
                                         (float) getHeight() / 120.0f);
        const auto scaled = [uiScale] (float value)
        {
            return juce::roundToInt (value * uiScale);
        };
        for (auto* slider : mainSliders)
        {
            slider->getProperties().set ("uiScale", (double) uiScale);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                     scaled (70.0f), scaled (18.0f));
        }

        for (int index = 0; index < (int) mainSliders.size(); ++index)
        {
            mainSliders[(std::size_t) index]->setBounds (
                scaled (20.0f + index * 100.0f), scaled (40.0f),
                scaled (84.0f), scaled (76.0f));
        }
        for (int index = 0; index < tbmbc::params::numBands; ++index)
            bandSelectors[static_cast<std::size_t> (index)].setBounds (
                scaled (44.0f + index * 24.0f), scaled (5.0f),
                scaled (22.0f), scaled (22.0f));
        enabledButton.setBounds (scaled (158.0f), scaled (5.0f),
                                 scaled (72.0f), scaled (22.0f));
        modeBox.setBounds (scaled (252.0f), scaled (5.0f),
                           scaled (96.0f), scaled (22.0f));
        sourceButton.setBounds (scaled (370.0f), scaled (5.0f),
                                scaled (132.0f), scaled (22.0f));
        auditionButton.setBounds (scaled (524.0f), scaled (5.0f),
                                  scaled (82.0f), scaled (22.0f));
        channelModeBox.setBounds (scaled (628.0f), scaled (5.0f),
                                  scaled (184.0f), scaled (22.0f));
    }

private:
    TB_MultiBandCompressorAudioProcessor& processor;
    std::function<void (int)> onBandSelected;
    std::function<void()> onDeltaChanged;
    int band = 0;
    std::array<juce::TextButton, tbmbc::params::numBands> bandSelectors;

    juce::Slider frequency, bandwidth, threshold, range, ratio, attack, release,
        knee, output, stereoLink;
    std::array<juce::Slider*, 8> mainSliders {
        &frequency, &bandwidth, &threshold, &range, &ratio, &knee,
        &attack, &release
    };
    std::array<juce::Slider*, 10> attachmentSliders {
        &frequency, &bandwidth, &threshold, &range, &ratio, &attack,
        &release, &knee, &output, &stereoLink
    };
    juce::ComboBox modeBox, channelModeBox;
    juce::TextButton enabledButton, sourceButton, auditionButton;
    std::array<juce::TextButton*, 2> parameterButtons {
        &enabledButton, &sourceButton
    };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>
        sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>
        buttonAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        channelModeAttachment;
};

TB_MultiBandCompressorAudioProcessorEditor::TB_MultiBandCompressorAudioProcessorEditor (
    TB_MultiBandCompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), tooltipWindow (this, 650)
{
    lookAndFeel = std::make_unique<LookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    const auto deltaChanged = [this]
    {
        if (bandPanel != nullptr)
            bandPanel->syncFromParameters();
        if (graph != nullptr)
            graph->repaint();
    };
    graph = std::make_unique<BandGraph> (
        processor, [this] (int band) { selectBand (band); }, deltaChanged);
    bandPanel = std::make_unique<BandPanel> (
        processor, [this] (int band) { selectBand (band); }, deltaChanged);
    addAndMakeVisible (*graph);
    addAndMakeVisible (*bandPanel);

    configureLinear (mixSlider,
                     "Blend the processed signal with the exact dry input.");
    mixSlider.setComponentID ("globalMix");
    configureLinear (lookaheadSlider,
                     "Delay gain action by 0 to 20 ms. This changes reported latency.");
    const std::array<const char*, 2> phaseLabels { "DYNAMIC", "LINEAR" };
    for (int index = 0; index < (int) phaseModeButtons.size(); ++index)
    {
        auto& button = phaseModeButtons[(std::size_t) index];
        button.setButtonText (phaseLabels[(std::size_t) index]);
        button.setComponentID (index == 0 ? "phaseDynamic" : "phaseLinear");
        button.setRadioGroupId (701);
        button.setClickingTogglesState (true);
        button.setTooltip (index == 0
            ? "Zero-latency band topology. Oversampling or lookahead can still add total latency."
            : "Linear-phase mastering topology. Adds FIR group delay.");
        button.onClick = [this, index]
        {
            if (phaseModeSegmentAttachment != nullptr)
                phaseModeSegmentAttachment->setValueAsCompleteGesture (
                    (float) index);
            syncQualityButtonStates();
        };
        addAndMakeVisible (button);
    }
    const std::array<const char*, 4> oversamplingLabels {
        "OFF", "2X", "4X", "8X"
    };
    for (int index = 0; index < (int) oversamplingButtons.size(); ++index)
    {
        auto& button = oversamplingButtons[(std::size_t) index];
        button.setButtonText (oversamplingLabels[(std::size_t) index]);
        button.setComponentID ("oversampling" + juce::String (index));
        button.setRadioGroupId (702);
        button.setClickingTogglesState (true);
        button.setTooltip (
            "Oversample the dynamics path. Higher settings add latency and CPU use.");
        button.onClick = [this, index]
        {
            if (oversamplingSegmentAttachment != nullptr)
                oversamplingSegmentAttachment->setValueAsCompleteGesture (
                    (float) index);
            syncQualityButtonStates();
        };
        addAndMakeVisible (button);
    }
    lookaheadSlider.setComponentID ("lookahead");
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (lookaheadSlider);
    bypassButton.setClickingTogglesState (true);
    bypassButton.getProperties().set ("accentRole", "bypass");
    addAndMakeVisible (bypassButton);

    auto& state = processor.getValueTreeState();
    phaseModeSegmentAttachment = std::make_unique<juce::ParameterAttachment> (
        *state.getParameter (tbmbc::params::phaseModeId),
        [this] (float) { syncQualityButtonStates(); },
        &processor.getUndoManager());
    oversamplingSegmentAttachment = std::make_unique<juce::ParameterAttachment> (
        *state.getParameter (tbmbc::params::oversamplingId),
        [this] (float) { syncQualityButtonStates(); },
        &processor.getUndoManager());
    phaseModeSegmentAttachment->sendInitialUpdate();
    oversamplingSegmentAttachment->sendInitialUpdate();
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, tbmbc::params::mixId, mixSlider);
    lookaheadAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, tbmbc::params::lookaheadId, lookaheadSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, tbmbc::params::bypassId, bypassButton);
    syncQualityButtonStates();

    previousButton = std::make_unique<juce::TextButton> ("<");
    nextButton = std::make_unique<juce::TextButton> (">");
    saveButton = std::make_unique<juce::TextButton> ("SAVE");
    undoButton = std::make_unique<juce::TextButton> ("UNDO");
    redoButton = std::make_unique<juce::TextButton> ("REDO");
    for (auto* button : { previousButton.get(), nextButton.get(), saveButton.get(),
                          undoButton.get(), redoButton.get() })
        addAndMakeVisible (*button);
    addAndMakeVisible (presetButton);

    previousButton->onClick = [this] { cyclePreset (-1); };
    nextButton->onClick = [this] { cyclePreset (1); };
    presetButton.onClick = [this] { showPresetMenu(); };
    saveButton->onClick = [this] { choosePresetToSave(); };
    undoButton->onClick = [this]
    {
        processor.undo();
        refreshPresetDisplay();
    };
    redoButton->onClick = [this]
    {
        processor.redo();
        refreshPresetDisplay();
    };
    previousButton->setTooltip ("Previous preset. Hold to browse continuously.");
    nextButton->setTooltip ("Next preset. Hold to browse continuously.");
    previousButton->setRepeatSpeed (420, 140, 70);
    nextButton->setRepeatSpeed (420, 140, 70);
    saveButton->setTooltip ("Save the current settings as a user preset.");
    undoButton->setTooltip ("Undo the last edit.");
    redoButton->setTooltip ("Redo the last undone edit.");

    effectLogo = juce::ImageFileFormat::loadFrom (BinaryData::tb_effect_logo_png,
                                                   BinaryData::tb_effect_logo_pngSize);
    const auto scanResult = scanUserPresets();
    refreshPresetDisplay();
    lastParameterRevision = processor.getParameterRevision();
    lastAudioBlockCounter = processor.getAudioBlockCounter();
    lastAudioBlockTimeMs = juce::Time::getMillisecondCounterHiRes();
    lastSidechainActive = processor.isExternalSidechainBusActive();
    lastSidechainBusMask = processor.getExternalSidechainBusMask();
    if (const auto* bypass = processor.getValueTreeState().getRawParameterValue (
            tbmbc::params::bypassId))
        lastBypassState = bypass->load() >= 0.5f;
    graph->setEnabled (! lastBypassState);
    bandPanel->setEnabled (! lastBypassState);
    mixSlider.setEnabled (! lastBypassState);
    for (auto& button : phaseModeButtons)
        button.setEnabled (! lastBypassState);
    for (auto& button : oversamplingButtons)
        button.setEnabled (! lastBypassState);
    lookaheadSlider.setEnabled (! lastBypassState);
    graph->setAlpha (lastBypassState ? 0.55f : 1.0f);
    bandPanel->setAlpha (lastBypassState ? 0.55f : 1.0f);
    mixSlider.setAlpha (lastBypassState ? 0.55f : 1.0f);
    for (auto& button : phaseModeButtons)
        button.setAlpha (lastBypassState ? 0.55f : 1.0f);
    for (auto& button : oversamplingButtons)
        button.setAlpha (lastBypassState ? 0.55f : 1.0f);
    lookaheadSlider.setAlpha (lastBypassState ? 0.55f : 1.0f);
    lastDetectorMeters.fill (-120.0f);

    setResizable (true, true);
    setResizeLimits (850, 527, 1500, 930);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize ((int) designWidth, (int) designHeight);
    visibilityWatcher = std::make_unique<VisibilityWatcher> (*this);
    updateVisibleActivity();

    if (scanResult.failed())
        showError (scanResult.getErrorMessage());
}

TB_MultiBandCompressorAudioProcessorEditor::~TB_MultiBandCompressorAudioProcessorEditor()
{
    processor.setDeltaListenBand (-1);
    if (graph != nullptr)
        graph->setHostShowing (false);
    stopTimer();
    visibilityWatcher.reset();
    setLookAndFeel (nullptr);
}

void TB_MultiBandCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (juce::Colour (canvasTop), 0.0f, 0.0f,
                                     juce::Colour (canvasBottom), 0.0f,
                                     (float) getHeight(), false);
    g.setGradientFill (background);
    g.fillAll();

    const auto header = scaledBounds (*this, { 0.0f, 0.0f, designWidth, 42.0f }).toFloat();
    juce::ColourGradient headerGradient (juce::Colour (surfaceTop), 0.0f, header.getY(),
                                         juce::Colour (surfaceBottom), 0.0f,
                                         header.getBottom(), false);
    g.setGradientFill (headerGradient);
    g.fillRect (header);
    g.setColour (juce::Colour (outline));
    g.drawLine (0.0f, header.getBottom() - 0.5f, (float) getWidth(),
                header.getBottom() - 0.5f, 1.0f);

    const auto sx = (float) getWidth() / designWidth;
    const auto sy = (float) getHeight() / designHeight;
    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale (sx, sy));

    for (const auto centreX : { 8.0f, 995.0f })
    {
        const auto screw = juce::Rectangle<float> (centreX - 2.5f, 18.5f, 5.0f, 5.0f);
        g.setColour (juce::Colour (deepShadow).withAlpha (0.80f));
        g.fillEllipse (screw);
        g.setColour (juce::Colour (outline).brighter (0.18f));
        g.drawEllipse (screw, 0.7f);
        g.drawLine (centreX - 1.2f, 21.0f, centreX + 1.2f, 21.0f, 0.7f);
    }

    const auto qualityStrip = juce::Rectangle<float> (12.0f, 565.0f, 976.0f, 43.0f);
    g.setColour (juce::Colour (deepShadow).withAlpha (0.68f));
    g.fillRoundedRectangle (qualityStrip.translated (0.0f, 2.0f), 8.0f);
    juce::ColourGradient qualityGradient (juce::Colour (surfaceTop), 0.0f,
                                          qualityStrip.getY(),
                                          juce::Colour (surfaceBottom), 0.0f,
                                          qualityStrip.getBottom(), false);
    g.setGradientFill (qualityGradient);
    g.fillRoundedRectangle (qualityStrip, 8.0f);
    g.setColour (juce::Colour (outline));
    g.drawRoundedRectangle (qualityStrip.reduced (0.5f), 8.0f, 1.0f);

    const auto titleFont = juce::Font (juce::FontOptions (14.0f,
                                                          juce::Font::bold));
    juce::AttributedString title;
    title.setJustification (juce::Justification::centredLeft);
    title.setWordWrap (juce::AttributedString::none);
    title.append ("M", titleFont, juce::Colour (accentSecondary));
    title.append ("ulti ", titleFont, juce::Colour (textPrimary));
    title.append ("B", titleFont, juce::Colour (controlActive));
    title.append ("and ", titleFont, juce::Colour (textPrimary));
    title.append ("C", titleFont, juce::Colour (accentPrimary));
    title.append ("ompressor", titleFont, juce::Colour (textPrimary));
    title.draw (g, juce::Rectangle<float> (20.0f, 11.0f, 250.0f, 20.0f));

    if (effectLogo.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageWithin (effectLogo, 940, 0, 48, 48,
                           juce::RectanglePlacement::centred
                               | juce::RectanglePlacement::onlyReduceInSize,
                           false);
    }

    g.setColour (juce::Colour (textSecondary).withAlpha (
        lastBypassState ? 0.45f : 1.0f));
    g.setFont (juce::Font (juce::FontOptions (7.8f, juce::Font::bold)));
    g.drawText ("PROCESSING", 108, 567, 158, 10,
                juce::Justification::centred, false);
    g.drawText ("OVERSAMPLING", 280, 567, 186, 10,
                juce::Justification::centred, false);
    g.drawText ("LOOKAHEAD / LAT "
                    + juce::String (processor.getLatencySamples()) + " smp",
                480, 567, 200, 10,
                juce::Justification::centred, false);
    g.drawText ("MIX", 700, 567, 268, 10,
                juce::Justification::centred, false);

}

void TB_MultiBandCompressorAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    if (toastMessage.isEmpty())
        return;
    const auto toast = scaledBounds (*this, { 380.0f, 346.0f, 240.0f, 28.0f }).toFloat();
    g.setColour (juce::Colour (displayBottom).withAlpha (0.96f));
    g.fillRoundedRectangle (toast, 5.0f);
    g.setColour (juce::Colour (accentSecondary));
    g.drawRoundedRectangle (toast, 5.0f, 1.0f);
    g.setColour (juce::Colour (textPrimary));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (toastMessage, toast, juce::Justification::centred, false);
}

void TB_MultiBandCompressorAudioProcessorEditor::resized()
{
    const auto uiScale = juce::jmin ((float) getWidth() / designWidth,
                                     (float) getHeight() / designHeight);
    mixSlider.getProperties().set ("uiScale", (double) uiScale);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false,
                               juce::roundToInt (58.0f * uiScale),
                               juce::roundToInt (18.0f * uiScale));
    lookaheadSlider.getProperties().set ("uiScale", (double) uiScale);
    lookaheadSlider.setTextBoxStyle (
        juce::Slider::TextBoxRight, false,
        juce::roundToInt (64.0f * uiScale),
        juce::roundToInt (20.0f * uiScale));
    previousButton->setBounds (scaledBounds (*this, { 380, 8, 24, 26 }));
    presetButton.setBounds (scaledBounds (*this, { 410, 7, 180, 28 }));
    nextButton->setBounds (scaledBounds (*this, { 596, 8, 24, 26 }));
    saveButton->setBounds (scaledBounds (*this, { 630, 8, 54, 26 }));
    undoButton->setBounds (scaledBounds (*this, { 694, 8, 56, 26 }));
    redoButton->setBounds (scaledBounds (*this, { 760, 8, 56, 26 }));
    graph->setBounds (scaledBounds (*this, { 12, 54, 976, 499 }));
    bandPanel->setBounds (scaledBounds (*this, { 88, 400, 824, 120 }));
    bypassButton.setBounds (scaledBounds (*this, { 24, 578, 72, 22 }));
    phaseModeButtons[0].setBounds (scaledBounds (*this, { 108, 578, 78, 22 }));
    phaseModeButtons[1].setBounds (scaledBounds (*this, { 188, 578, 78, 22 }));
    for (int index = 0; index < (int) oversamplingButtons.size(); ++index)
        oversamplingButtons[(std::size_t) index].setBounds (
            scaledBounds (*this, { 280.0f + index * 47.0f, 578.0f, 45.0f, 22.0f }));
    lookaheadSlider.setBounds (scaledBounds (*this, { 480, 578, 200, 22 }));
    mixSlider.setBounds (scaledBounds (*this, { 700, 578, 268, 22 }));
}

void TB_MultiBandCompressorAudioProcessorEditor::updateVisibleActivity()
{
    if (graph == nullptr)
        return;
    const auto showing = isShowing();
    graph->setHostShowing (showing);
    if (! showing)
    {
        processor.setDeltaListenBand (-1);
        stopTimer();
        return;
    }

    if (! isTimerRunning())
    {
        timerAtIdleRate = false;
        startTimerHz (30);
    }
}

void TB_MultiBandCompressorAudioProcessorEditor::selectBand (int index)
{
    selectedBand = juce::jlimit (0, tbmbc::params::numBands - 1, index);
    graph->setSelectedBand (selectedBand);
    bandPanel->bindToBand (selectedBand);
}

void TB_MultiBandCompressorAudioProcessorEditor::syncQualityButtonStates()
{
    const auto* phase = processor.getValueTreeState().getRawParameterValue (
        tbmbc::params::phaseModeId);
    const auto* oversampling = processor.getValueTreeState().getRawParameterValue (
        tbmbc::params::oversamplingId);
    const auto phaseIndex = juce::jlimit (
        0, 1, phase != nullptr ? juce::roundToInt (phase->load()) : 0);
    const auto oversamplingIndex = juce::jlimit (
        0, 3, oversampling != nullptr
            ? juce::roundToInt (oversampling->load()) : 0);
    for (int index = 0; index < (int) phaseModeButtons.size(); ++index)
        phaseModeButtons[(std::size_t) index].setToggleState (
            index == phaseIndex, juce::dontSendNotification);
    for (int index = 0; index < (int) oversamplingButtons.size(); ++index)
        oversamplingButtons[(std::size_t) index].setToggleState (
            index == oversamplingIndex, juce::dontSendNotification);
}

void TB_MultiBandCompressorAudioProcessorEditor::timerCallback()
{
    if (! isShowing())
    {
        updateVisibleActivity();
        return;
    }
    graph->closeAbandonedGesture();

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto blocks = processor.getAudioBlockCounter();
    if (blocks != lastAudioBlockCounter)
    {
        lastAudioBlockCounter = blocks;
        lastAudioBlockTimeMs = now;
    }
    const auto audioRunning = now - lastAudioBlockTimeMs < 350.0;
    const auto shouldUseIdleRate = ! audioRunning;
    if (timerAtIdleRate != shouldUseIdleRate)
    {
        timerAtIdleRate = shouldUseIdleRate;
        startTimerHz (timerAtIdleRate ? 5 : 30);
    }

    const auto revision = processor.getParameterRevision();
    auto graphChanged = graph->updateSpectrum (audioRunning);
    if (revision != lastParameterRevision)
    {
        lastParameterRevision = revision;
        refreshPresetDisplay();
        bandPanel->syncFromParameters();
        syncQualityButtonStates();
        repaint();
        graphChanged = true;
    }
    const auto sidechainActive = processor.isExternalSidechainBusActive();
    if (sidechainActive != lastSidechainActive)
    {
        lastSidechainActive = sidechainActive;
        graphChanged = true;
    }
    const auto sidechainBusMask = processor.getExternalSidechainBusMask();
    if (sidechainBusMask != lastSidechainBusMask)
    {
        lastSidechainBusMask = sidechainBusMask;
        graphChanged = true;
    }
    if (const auto* bypass = processor.getValueTreeState().getRawParameterValue (
            tbmbc::params::bypassId))
    {
        const auto bypassed = bypass->load() >= 0.5f;
        if (bypassed != lastBypassState)
        {
            lastBypassState = bypassed;
            graph->setEnabled (! bypassed);
            bandPanel->setEnabled (! bypassed);
            mixSlider.setEnabled (! bypassed);
            for (auto& button : phaseModeButtons)
                button.setEnabled (! bypassed);
            for (auto& button : oversamplingButtons)
                button.setEnabled (! bypassed);
            lookaheadSlider.setEnabled (! bypassed);
            graph->setAlpha (bypassed ? 0.55f : 1.0f);
            bandPanel->setAlpha (bypassed ? 0.55f : 1.0f);
            mixSlider.setAlpha (bypassed ? 0.55f : 1.0f);
            for (auto& button : phaseModeButtons)
                button.setAlpha (bypassed ? 0.55f : 1.0f);
            for (auto& button : oversamplingButtons)
                button.setAlpha (bypassed ? 0.55f : 1.0f);
            lookaheadSlider.setAlpha (bypassed ? 0.55f : 1.0f);
            if (! bypassed)
            {
                syncQualityButtonStates();
                bandPanel->syncFromParameters();
            }
            repaint();
            graphChanged = true;
        }
    }
    for (int band = 0; band < tbmbc::params::numBands; ++band)
    {
        const auto index = (std::size_t) band;
        const auto gain = audioRunning ? processor.getGainReductionDb (band) : 0.0f;
        const auto detector = audioRunning ? processor.getDetectorLevelDb (band) : -120.0f;
        if (std::abs (gain - lastGainMeters[index]) > 0.01f
            || std::abs (detector - lastDetectorMeters[index]) > 0.05f)
            graphChanged = true;
        lastGainMeters[index] = gain;
        lastDetectorMeters[index] = detector;
    }
    graph->setMeterValues (lastGainMeters, lastDetectorMeters);
    undoButton->setEnabled (processor.canUndo());
    redoButton->setEnabled (processor.canRedo());
    if (graphChanged && isShowing())
        graph->repaint();

    if (toastMessage.isNotEmpty() && now >= toastUntilMs)
    {
        toastMessage.clear();
        repaint();
    }
}

juce::Result TB_MultiBandCompressorAudioProcessorEditor::scanUserPresets()
{
    const auto directory = TB_MultiBandCompressorAudioProcessor::getUserPresetDirectory();
    const auto result = directory.createDirectory();
    if (result.failed())
    {
        userPresetFiles.clear();
        return result;
    }
    const auto files = directory.findChildFiles (
        juce::File::findFiles, false,
        "*" + TB_MultiBandCompressorAudioProcessor::getUserPresetExtension());
    userPresetFiles.assign (files.begin(), files.end());
    std::sort (userPresetFiles.begin(), userPresetFiles.end(),
               [] (const juce::File& a, const juce::File& b)
               {
                   return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
               });
    return juce::Result::ok();
}

void TB_MultiBandCompressorAudioProcessorEditor::refreshPresetDisplay()
{
    presetButton.setButtonText (processor.getCurrentPresetName());
    undoButton->setEnabled (processor.canUndo());
    redoButton->setEnabled (processor.canRedo());
}

void TB_MultiBandCompressorAudioProcessorEditor::showPresetMenu()
{
    if (const auto result = scanUserPresets(); result.failed())
    {
        showError (result.getErrorMessage());
        return;
    }

    juce::PopupMenu start, source, bus, master, motion, user, menu;
    const auto active = processor.getActiveFactoryPresetId();
    constexpr int factoryBase = 100;
    constexpr int userBase = 1000;
    for (int index = 0; index < processor.getFactoryPresetCount(); ++index)
    {
        const auto& preset = processor.getFactoryPreset (index);
        auto* target = &start;
        const auto category = juce::String (preset.category);
        if (category == "SOURCE") target = &source;
        else if (category == "BUS") target = &bus;
        else if (category == "MASTER") target = &master;
        else if (category == "MOTION") target = &motion;
        target->addItem (factoryBase + index, preset.name, true, preset.id == active);
    }
    if (userPresetFiles.empty())
        user.addItem (9999, "No saved presets", false, false);
    else
        for (std::size_t index = 0; index < userPresetFiles.size(); ++index)
            user.addItem (userBase + (int) index,
                          userPresetFiles[index].getFileNameWithoutExtension());
    menu.addSubMenu ("START", start);
    menu.addSubMenu ("SOURCE", source);
    menu.addSubMenu ("BUS", bus);
    menu.addSubMenu ("MASTER", master);
    menu.addSubMenu ("MOTION", motion);
    menu.addSeparator();
    menu.addSubMenu ("USER", user);

    const auto filesAtOpen = userPresetFiles;
    juce::Component::SafePointer<TB_MultiBandCompressorAudioProcessorEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton),
                        [safeThis, filesAtOpen, factoryBase, userBase] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;
                            if (result >= factoryBase
                                && result < factoryBase + safeThis->processor.getFactoryPresetCount())
                            {
                                const auto& preset = safeThis->processor.getFactoryPreset (
                                    result - factoryBase);
                                safeThis->processor.applyFactoryPreset (preset.id);
                            }
                            else if (result >= userBase)
                            {
                                const auto index = result - userBase;
                                if (juce::isPositiveAndBelow (index, (int) filesAtOpen.size()))
                                {
                                    const auto load = safeThis->processor.loadUserPreset (
                                        filesAtOpen[(std::size_t) index]);
                                    if (load.failed())
                                        safeThis->showError (load.getErrorMessage());
                                }
                            }
                            safeThis->refreshPresetDisplay();
                        });
}

int TB_MultiBandCompressorAudioProcessorEditor::getCurrentPresetPosition() const
{
    const auto factoryId = processor.getActiveFactoryPresetId();
    for (int index = 0; index < processor.getFactoryPresetCount(); ++index)
        if (processor.getFactoryPreset (index).id == factoryId)
            return index;
    const auto selected = processor.getCurrentUserPresetFile();
    const auto found = std::find (userPresetFiles.begin(), userPresetFiles.end(), selected);
    return found == userPresetFiles.end()
        ? -1
        : processor.getFactoryPresetCount() + (int) std::distance (userPresetFiles.begin(), found);
}

void TB_MultiBandCompressorAudioProcessorEditor::cyclePreset (int direction)
{
    if (const auto result = scanUserPresets(); result.failed())
    {
        showError (result.getErrorMessage());
        return;
    }
    const auto factoryCount = processor.getFactoryPresetCount();
    const auto total = factoryCount + (int) userPresetFiles.size();
    if (total <= 0)
        return;
    auto current = getCurrentPresetPosition();
    if (current < 0)
        current = direction > 0 ? -1 : 0;
    const auto next = (current + direction + total) % total;
    if (next < factoryCount)
        processor.applyFactoryPreset (processor.getFactoryPreset (next).id);
    else
    {
        const auto result = processor.loadUserPreset (
            userPresetFiles[(std::size_t) (next - factoryCount)]);
        if (result.failed())
            showError (result.getErrorMessage());
    }
    refreshPresetDisplay();
}

void TB_MultiBandCompressorAudioProcessorEditor::choosePresetToSave()
{
    const auto directory = TB_MultiBandCompressorAudioProcessor::getUserPresetDirectory();
    if (const auto result = directory.createDirectory(); result.failed())
    {
        showError (result.getErrorMessage());
        return;
    }
    auto name = processor.getCurrentPresetName();
    if (name == "Custom" || name.isEmpty())
        name = "My Multiband";
    const auto suggested = directory.getChildFile (name).withFileExtension (
        TB_MultiBandCompressorAudioProcessor::getUserPresetExtension());
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save TB_MultiBandCompressor preset", suggested,
        "*" + TB_MultiBandCompressorAudioProcessor::getUserPresetExtension(), true);
    juce::Component::SafePointer<TB_MultiBandCompressorAudioProcessorEditor> safeThis (this);
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis, directory] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr || chooser.getResult() == juce::File())
                return;
            auto file = chooser.getResult();
            if (file.getParentDirectory() != directory)
            {
                safeThis->showError ("User presets must be saved in:\n"
                                     + directory.getFullPathName());
                return;
            }
            const auto result = safeThis->processor.saveUserPreset (file);
            if (result.failed())
                safeThis->showError (result.getErrorMessage());
            else
            {
                safeThis->scanUserPresets();
                safeThis->refreshPresetDisplay();
                safeThis->showToast ("PRESET SAVED");
            }
        });
}

void TB_MultiBandCompressorAudioProcessorEditor::showError (const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                            "TB_MultiBandCompressor", message, "OK", this);
}

void TB_MultiBandCompressorAudioProcessorEditor::showToast (const juce::String& message)
{
    toastMessage = message;
    toastUntilMs = juce::Time::getMillisecondCounterHiRes() + 3000.0;
    repaint();
}
