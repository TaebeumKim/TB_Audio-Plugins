#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "DelayMapModel.h"

namespace
{
// juce::String reads a plain const char* as Latin-1, so a UTF-8 literal handed
// over that way arrives as mojibake. Every non-ASCII literal in this file goes
// through here instead.
juce::String utf8 (const char* text)
{
    return juce::String::fromUTF8 (text);
}

const auto canvasTop = juce::Colour::fromString ("ff2ba0ff");
const auto canvasBottom = juce::Colour::fromString ("ffd2eeff");
const auto surfaceTop = juce::Colour::fromString ("fff2fbff");
const auto surfaceBottom = juce::Colour::fromString ("ffd8f0ff");
const auto displayTop = juce::Colour::fromString ("ffd6efff");
const auto displayBottom = juce::Colour::fromString ("ffeaf7ff");
const auto textPrimary = juce::Colour::fromString ("ff073457");
const auto textSecondary = juce::Colour::fromString ("ff2d6e96");
const auto valueText = juce::Colour::fromString ("ff0d4a78");
const auto accentPrimary = juce::Colour::fromString ("ff1478fa");
const auto accentSecondary = juce::Colour::fromString ("ff58bdff");
const auto sectionAccent = juce::Colour::fromString ("ff1e86fd");
const auto outline = juce::Colour::fromString ("ff8cc7ef");
const auto deepShadow = juce::Colour::fromString ("ff2e6f9e");
const auto highlight = juce::Colour::fromString ("fffcffff");
const auto gridColour = juce::Colour::fromString ("ff9fd4f5");
const auto sandNear = juce::Colour::fromString ("fff0dfb4");
const auto sandFar = juce::Colour::fromString ("ffe3cb98");
constexpr double successStatusDurationMs = 3200.0;
constexpr double delayMapHorizonMs = 5000.0;
constexpr double minimumTapIntervalMs = 100.0;
constexpr double maximumTapIntervalMs = 3000.0;
constexpr double maximumTapDeviation = 0.35;

// Choice order: straight, triplet, dotted for each note value. TAP uses the
// same order as the existing Sync parameter and never creates hidden tempo
// state that could disagree with the host.
constexpr std::array<double, 18> tapDivisionBeats {
    0.125, 1.0 / 12.0, 0.1875,
    0.25, 1.0 / 6.0, 0.375,
    0.5, 1.0 / 3.0, 0.75,
    1.0, 2.0 / 3.0, 1.5,
    2.0, 4.0 / 3.0, 3.0,
    4.0, 8.0 / 3.0, 6.0
};

// Every distance on the face is written in the units of the base editor size
// and passed through this helper, so one scale factor resizes the whole UI
// instead of stretching one region against another.
struct UiScale
{
    explicit UiScale (float scaleFactor) noexcept : scale (scaleFactor) {}

    int operator() (float base) const noexcept
    {
        return juce::roundToInt (base * scale);
    }

    float f (float base) const noexcept { return base * scale; }

    float scale = 1.0f;
};

constexpr int outerMargin = 18;
constexpr int headerHeight = 54;
constexpr int headerGap = 13;
constexpr int footerHeight = 33;
constexpr int footerGap = 4;
constexpr int analyserGap = 10;
// The board keeps clear of its own left border so no control sits against the
// panel edge, and the four sections repeat that inset inside themselves.
constexpr int boardPadLeft = 20;
constexpr int boardPadRight = 14;
constexpr int boardPadY = 8;
constexpr int quadrantGap = 8;
constexpr int sectionTitleHeight = 36;
constexpr int sectionInsetLeft = 12;
constexpr int sectionInsetRight = 6;
constexpr int controlRowHeight = 82;

// Toon banding: colour steps in discrete bands instead of a smooth ramp.
void fillToonBands (juce::Graphics& g,
                    juce::Rectangle<float> bounds,
                    juce::Colour top,
                    juce::Colour bottom,
                    int steps,
                    float corner = 0.0f)
{
    juce::Graphics::ScopedSaveState state (g);
    if (corner > 0.0f)
    {
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);
        g.reduceClipRegion (clip);
    }

    for (int band = 0; band < steps; ++band)
    {
        const auto amount =
            steps <= 1 ? 0.0f
                       : static_cast<float> (band)
                             / static_cast<float> (steps - 1);
        const auto y = bounds.getY()
                       + bounds.getHeight() * static_cast<float> (band)
                             / static_cast<float> (steps);
        const auto next = bounds.getY()
                          + bounds.getHeight()
                                * static_cast<float> (band + 1)
                                / static_cast<float> (steps);
        g.setColour (top.interpolatedWith (bottom, amount));
        g.fillRect (
            juce::Rectangle<float> (
                bounds.getX(), y, bounds.getWidth(), next - y + 0.5f));
    }
}

// The control board takes a fixed share of the working area and the Delay Map
// keeps everything else, so lowering this ratio is what makes the analyser
// taller without changing the window height.
int controlBoardHeightFor (int availableHeight)
{
    return juce::roundToInt (
        static_cast<float> (availableHeight) * 0.50f);
}

float parameterValue (const juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (auto* value = state.getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

juce::String parameterText (
    const juce::AudioProcessorValueTreeState& state,
    const char* id)
{
    if (auto* parameter = state.getParameter (id))
    {
        const auto plain = parameterValue (state, id);
        return parameter->getText (
            parameter->convertTo0to1 (plain), 32);
    }

    return {};
}

juce::String delayTimeText (double milliseconds)
{
    return juce::String (
               milliseconds,
               milliseconds < 10.0 ? 1 : 0)
           + "ms";
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float corner = 9.0f)
{
    g.setColour (deepShadow.withAlpha (0.88f));
    g.fillRoundedRectangle (bounds.translated (1.2f, 3.2f), corner);

    juce::ColourGradient fill (surfaceTop, bounds.getX(), bounds.getY(),
                               surfaceBottom, bounds.getX(), bounds.getBottom(), false);
    g.setColour (fill.getColour (0));
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (outline.withAlpha (0.92f));
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    const auto inner = bounds.reduced (1.25f);
    g.setColour (highlight.withAlpha (0.24f));
    g.drawRoundedRectangle (
        inner, std::max (1.0f, corner - 1.25f), 0.9f);
    g.setColour (textPrimary.withAlpha (0.10f));
    g.drawLine (bounds.getX() + corner,
                bounds.getY() + 1.0f,
                bounds.getRight() - corner,
                bounds.getY() + 1.0f, 1.0f);
    g.setColour (deepShadow.withAlpha (0.52f));
    g.drawLine (
        bounds.getX() + corner,
        bounds.getBottom() - 1.0f,
        bounds.getRight() - corner,
        bounds.getBottom() - 1.0f, 1.0f);
}

void drawRaisedSection (
    juce::Graphics& g,
    juce::Rectangle<float> bounds,
    bool primary)
{
    constexpr auto corner = 6.0f;
    g.setColour (deepShadow.withAlpha (0.30f));
    g.fillRoundedRectangle (
        bounds.translated (0.0f, 7.0f).expanded (2.0f, 1.0f), corner);
    g.setColour (deepShadow.withAlpha (0.40f));
    g.fillRoundedRectangle (
        bounds.translated (0.0f, 3.0f), corner);

    const auto top =
        (primary ? surfaceTop.brighter (0.035f) : surfaceTop)
            .withAlpha (0.86f);
    const auto bottom =
        surfaceBottom.withAlpha (primary ? 0.94f : 0.90f);
    juce::ColourGradient fill (
        top, bounds.getX(), bounds.getY(),
        bottom, bounds.getX(), bounds.getBottom(), false);
    g.setColour (fill.getColour (0));
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (highlight.withAlpha (primary ? 0.25f : 0.18f));
    g.drawLine (
        bounds.getX() + corner, bounds.getY() + 0.8f,
        bounds.getRight() - corner, bounds.getY() + 0.8f,
        0.9f);
    g.setColour (outline.withAlpha (0.72f));
    g.drawRoundedRectangle (bounds, corner, 0.9f);
    g.setColour (deepShadow.withAlpha (0.44f));
    g.drawLine (
        bounds.getX() + corner,
        bounds.getBottom() - 0.8f,
        bounds.getRight() - corner,
        bounds.getBottom() - 0.8f, 0.8f);
}

// paint() and resized() have to agree on the board and its four section panels
// to the pixel, so both read the geometry from here.
struct BoardGeometry
{
    juce::Rectangle<int> analyser;
    juce::Rectangle<int> board;
    std::array<juce::Rectangle<int>, 4> quadrants {};
};

BoardGeometry computeBoardGeometry (
    juce::Rectangle<int> editorBounds, float scale)
{
    const UiScale s { scale };
    auto content = editorBounds.reduced (s (outerMargin));
    content.removeFromTop (s (headerHeight + headerGap));
    content.removeFromBottom (s (footerHeight + footerGap));

    BoardGeometry geometry;
    geometry.board = content.removeFromBottom (
        controlBoardHeightFor (content.getHeight()));
    content.removeFromBottom (s (analyserGap));
    geometry.analyser = content;

    const auto gap = s (quadrantGap);
    const auto boardContent =
        geometry.board
            .withTrimmedLeft (s (boardPadLeft))
            .withTrimmedRight (s (boardPadRight))
            .reduced (0, s (boardPadY));

    // Each panel holds a single control row, so a quadrant is as wide as the
    // number of cells it carries. That keeps one cell width across the whole
    // board instead of leaving one panel stretched and another crowded.
    constexpr std::array<int, 4> sectionCells { 5, 2, 3, 5 };
    const auto rowSplit =
        [&boardContent, gap] (int leftCells, int rightCells)
    {
        const auto usable = boardContent.getWidth() - gap;
        return usable * leftCells / (leftCells + rightCells);
    };
    const auto topLeftWidth = rowSplit (sectionCells[0], sectionCells[1]);
    const auto bottomLeftWidth = rowSplit (sectionCells[2], sectionCells[3]);
    const auto topHeight = (boardContent.getHeight() - gap) / 2;
    const auto bottomHeight = boardContent.getHeight() - topHeight - gap;

    geometry.quadrants = {{
        { boardContent.getX(), boardContent.getY(),
          topLeftWidth, topHeight },
        { boardContent.getX() + topLeftWidth + gap, boardContent.getY(),
          boardContent.getWidth() - topLeftWidth - gap, topHeight },
        { boardContent.getX(), boardContent.getY() + topHeight + gap,
          bottomLeftWidth, bottomHeight },
        { boardContent.getX() + bottomLeftWidth + gap,
          boardContent.getY() + topHeight + gap,
          boardContent.getWidth() - bottomLeftWidth - gap, bottomHeight }
    }};

    return geometry;
}
}

class TB_DelayAudioProcessorEditor::DelayLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    DelayLookAndFeel()
    {
        const char* const koreanTypefaceCandidates[] {
           #if JUCE_MAC
            "Apple SD Gothic Neo", "AppleGothic",
           #endif
            "Malgun Gothic", "Noto Sans KR", "Gulim"
        };

        for (const auto* candidate
             : koreanTypefaceCandidates)
        {
            if (juce::Font::findAllTypefaceNames().contains (candidate))
            {
                setDefaultSansSerifTypefaceName (candidate);
                break;
            }
        }

        setColour (juce::Label::textColourId, textSecondary);
        setColour (juce::ComboBox::backgroundColourId, displayBottom);
        setColour (juce::ComboBox::textColourId, textPrimary);
        setColour (juce::ComboBox::outlineColourId, outline);
        setColour (juce::ComboBox::arrowColourId, accentPrimary);
        setColour (juce::PopupMenu::backgroundColourId, surfaceBottom);
        setColour (juce::PopupMenu::textColourId, textPrimary);
        setColour (juce::PopupMenu::highlightedBackgroundColourId,
                   accentPrimary.darker (0.54f));
        setColour (juce::TextButton::textColourOffId, textSecondary);
        setColour (juce::TextButton::textColourOnId, textPrimary);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto opacity = slider.isEnabled() ? 1.0f : 0.35f;
        const auto diameter = static_cast<float> (juce::jmin (width, height));
        auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                               diameter, diameter).reduced (2.0f);
        bounds.setCentre (static_cast<float> (x) + width * 0.5f,
                          static_cast<float> (y) + height * 0.5f);

        const auto socket = bounds.expanded (1.0f);
        g.setColour (deepShadow.withMultipliedAlpha (0.94f * opacity));
        g.fillEllipse (socket.translated (0.0f, 1.7f));
        g.setColour (displayBottom.withMultipliedAlpha (opacity));
        g.fillEllipse (socket);
        g.setColour (outline.withMultipliedAlpha (0.90f * opacity));
        g.drawEllipse (socket, 1.0f);

        const auto capBounds = bounds.reduced (4.2f);
        juce::ColourGradient cap (
            highlight.withMultipliedAlpha (0.66f * opacity),
            capBounds.getX(), capBounds.getY(),
            surfaceBottom.withMultipliedAlpha (opacity),
            capBounds.getRight(), capBounds.getBottom(), false);
        g.setColour (cap.getColour (0));
        g.fillEllipse (capBounds);
        g.setColour (highlight.withMultipliedAlpha (0.24f * opacity));
        g.drawEllipse (capBounds.reduced (0.6f), 0.9f);

        auto arcBounds = socket.reduced (0.4f);
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (arcBounds.getCentreX(), arcBounds.getCentreY(),
                                     arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                                     0.0f, startAngle, endAngle, true);
        g.setColour (outline.brighter (0.05f)
                         .withMultipliedAlpha (0.72f * opacity));
        g.strokePath (backgroundArc, juce::PathStrokeType (1.15f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

        const auto angle = startAngle + sliderPos * (endAngle - startAngle);
        if (sliderPos > 0.001f)
        {
            juce::Path energyArc;
            energyArc.addCentredArc (
                arcBounds.getCentreX(), arcBounds.getCentreY(),
                arcBounds.getWidth() * 0.5f,
                arcBounds.getHeight() * 0.5f,
                0.0f, startAngle, angle, true);
            const auto energyThickness =
                diameter <= 32.0f ? 3.4f : 2.6f;
            g.setColour (
                accentPrimary.withMultipliedAlpha (
                    0.16f * opacity));
            g.strokePath (
                energyArc,
                juce::PathStrokeType (
                    energyThickness + 2.0f,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
            g.setColour (
                accentPrimary.withMultipliedAlpha (
                    0.88f * opacity));
            g.strokePath (
                energyArc,
                juce::PathStrokeType (
                    energyThickness,
                    juce::PathStrokeType::curved,
                    juce::PathStrokeType::rounded));
        }

        const auto radius = capBounds.getWidth() * 0.37f;
        const auto centre = capBounds.getCentre();
        const auto point = centre + juce::Point<float> (std::sin (angle) * radius,
                                                        -std::cos (angle) * radius);
        const auto pointerStart =
            centre + (point - centre) * 0.22f;
        g.setColour (
            accentPrimary.withMultipliedAlpha (0.92f * opacity));
        g.drawLine ({ pointerStart, point }, 1.7f);
        g.fillEllipse (
            point.x - 1.55f, point.y - 1.55f, 3.1f, 3.1f);
        g.setColour (deepShadow.withMultipliedAlpha (0.88f * opacity));
        g.fillEllipse (
            centre.x - 2.2f, centre.y - 2.2f, 4.4f, 4.4f);
        g.setColour (textPrimary.withMultipliedAlpha (0.15f * opacity));
        g.fillEllipse (
            capBounds.getX() + capBounds.getWidth() * 0.24f,
            capBounds.getY() + capBounds.getHeight() * 0.19f,
            3.2f, 1.8f);
    }

    void drawLinearSlider (
        juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos, float, float,
        juce::Slider::SliderStyle style,
        juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            LookAndFeel_V4::drawLinearSlider (
                g, x, y, width, height, sliderPos,
                0.0f, 0.0f, style, slider);
            return;
        }

        const auto opacity = slider.isEnabled() ? 1.0f : 0.35f;
        auto track = juce::Rectangle<float> (
            static_cast<float> (x + 7),
            static_cast<float> (y) + height * 0.5f - 3.5f,
            static_cast<float> (width - 14), 7.0f);

        g.setColour (deepShadow.withMultipliedAlpha (0.90f * opacity));
        g.fillRoundedRectangle (
            track.translated (0.0f, 1.5f), 3.5f);
        g.setColour (displayBottom.withMultipliedAlpha (opacity));
        g.fillRoundedRectangle (track, 3.5f);
        g.setColour (outline.withMultipliedAlpha (0.90f * opacity));
        g.drawRoundedRectangle (track, 3.5f, 1.0f);

        auto fillStart = track.getX();
        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
            fillStart = track.getX()
                        + static_cast<float> (
                              slider.valueToProportionOfLength (0.0))
                              * track.getWidth();

        const auto fillLeft = std::min (fillStart, sliderPos);
        const auto fillRight = std::max (fillStart, sliderPos);
        if (fillRight - fillLeft > 0.5f)
        {
            auto energy = juce::Rectangle<float> (
                fillLeft, track.getCentreY() - 4.5f,
                fillRight - fillLeft, 9.0f);
            g.setColour (
                accentPrimary.withMultipliedAlpha (0.18f * opacity));
            g.fillRoundedRectangle (energy.expanded (1.0f), 4.5f);
            g.setColour (
                accentPrimary.withMultipliedAlpha (0.90f * opacity));
            g.fillRoundedRectangle (energy, 4.5f);
        }

        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        {
            g.setColour (valueText.withMultipliedAlpha (0.66f * opacity));
            g.drawVerticalLine (
                juce::roundToInt (fillStart),
                track.getY() - 2.0f, track.getBottom() + 2.0f);
        }

        const auto thumb = juce::Rectangle<float> (
            sliderPos - 5.5f, track.getCentreY() - 7.0f,
            11.0f, 14.0f);
        g.setColour (deepShadow.withMultipliedAlpha (0.92f * opacity));
        g.fillRoundedRectangle (
            thumb.translated (0.0f, 1.2f), 4.0f);
        juce::ColourGradient cap (
            highlight.withMultipliedAlpha (0.72f * opacity),
            thumb.getX(), thumb.getY(),
            surfaceBottom.withMultipliedAlpha (opacity),
            thumb.getRight(), thumb.getBottom(), false);
        g.setColour (cap.getColour (0));
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (
            accentPrimary.withMultipliedAlpha (0.92f * opacity));
        g.drawRoundedRectangle (thumb, 4.0f, 1.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto active = button.getToggleState();
        const auto opacity = button.isEnabled() ? 1.0f : 0.35f;
        auto top = active ? accentPrimary.darker (0.48f) : surfaceTop;
        auto bottom = active ? accentPrimary.darker (0.68f) : surfaceBottom;
        if (highlighted)
            top = top.brighter (0.08f);
        if (down)
            std::swap (top, bottom);

        juce::ColourGradient fill (top.withMultipliedAlpha (opacity),
                                   0.0f, bounds.getY(),
                                   bottom.withMultipliedAlpha (opacity),
                                   0.0f, bounds.getBottom(), false);
        g.setColour (fill.getColour (0));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour ((active ? accentPrimary : outline)
                         .withMultipliedAlpha (
                             (active ? 0.82f : 0.74f) * opacity));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        g.setColour (highlight.withMultipliedAlpha (0.10f * opacity));
        g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 1.0f,
                    bounds.getRight() - 4.0f, bounds.getY() + 1.0f,
                    0.8f);

        if (button.hasKeyboardFocus (true))
        {
            g.setColour (accentSecondary.withAlpha (0.95f));
            g.drawRoundedRectangle (bounds.reduced (2.0f), 2.5f, 1.2f);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int,
                       juce::ComboBox& comboBox) override
    {
        const auto supporting =
            comboBox.getComponentID() == "preset-box";
        auto bounds = juce::Rectangle<float> (0.5f, 0.5f,
                                               static_cast<float> (width - 1),
                                               static_cast<float> (height - 1));
        const auto top =
            supporting
                ? displayTop.interpolatedWith (surfaceBottom, 0.42f)
                : displayTop;
        const auto bottom =
            supporting
                ? displayBottom.interpolatedWith (surfaceBottom, 0.28f)
                : displayBottom;
        juce::ColourGradient fill (
            top, 0.0f, 0.0f,
            bottom, 0.0f, bounds.getBottom(), false);
        g.setColour (fill.getColour (0));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (outline.withAlpha (supporting ? 0.68f : 0.9f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        g.setColour (highlight.withAlpha (supporting ? 0.06f : 0.10f));
        g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 1.0f,
                    bounds.getRight() - 18.0f, bounds.getY() + 1.0f,
                    0.8f);

        juce::Path arrow;
        const auto cx = static_cast<float> (width - 13);
        const auto cy = static_cast<float> (height) * 0.5f;
        arrow.addTriangle (cx - 3.0f, cy - 2.0f, cx + 3.0f, cy - 2.0f, cx, cy + 2.0f);
        g.setColour (
            comboBox.findColour (juce::ComboBox::arrowColourId)
                .withMultipliedAlpha (supporting ? 0.74f : 0.92f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (
            juce::FontOptions (12.0f * uiScale, juce::Font::plain));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int height) override
    {
        return juce::Font (
            juce::FontOptions (
                juce::jmin (11.0f * uiScale, height * 0.5f),
                juce::Font::bold));
    }

    // The corner grip is the only affordance for resizing the window, so it is
    // drawn in the chassis palette instead of JUCE's default grey, which
    // disappears against this face.
    void drawCornerResizer (juce::Graphics& g, int width, int height,
                            bool isMouseOver, bool isMouseDragging) override
    {
        const auto alpha =
            isMouseDragging ? 0.95f : (isMouseOver ? 0.82f : 0.58f);
        const auto w = static_cast<float> (width);
        const auto h = static_cast<float> (height);
        const auto thickness =
            juce::jmax (1.2f, juce::jmin (w, h) * 0.085f);

        for (const auto offset : { 0.34f, 0.60f, 0.86f })
        {
            g.setColour (highlight.withAlpha (alpha * 0.75f));
            g.drawLine (
                w * offset, h - 1.5f, w - 1.5f, h * offset,
                thickness);
            g.setColour (deepShadow.withAlpha (alpha));
            g.drawLine (
                w * offset, h - 3.0f, w - 3.0f, h * offset,
                thickness);
        }
    }

    void setUiScale (float newScale) noexcept
    {
        uiScale = juce::jmax (0.5f, newScale);
    }

private:
    float uiScale = 1.0f;
};

class TB_DelayAudioProcessorEditor::ContinuousControl final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    enum class Presentation
    {
        rotary,
        horizontalSlider
    };

    ContinuousControl (juce::AudioProcessorValueTreeState& state,
                       const char* parameterID,
                       const juce::String& shortName,
                       const juce::String& help,
                       Presentation controlPresentation)
        : parameter (state.getParameter (parameterID)),
          parameterIDString (parameterID),
          presentation (controlPresentation)
    {
        setComponentID ("knob-" + juce::String (parameterID));
        setTooltip (help);
        slider.setComponentID (parameterID);
        if (parameter != nullptr)
        {
            const auto range = parameter->getNormalisableRange();
            slider.zeroDetent = range.start < 0.0f && range.end > 0.0f;
        }
        slider.setSliderStyle (
            presentation == Presentation::horizontalSlider
                ? juce::Slider::LinearHorizontal
                : juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        if (presentation == Presentation::rotary)
        {
            slider.setRotaryParameters (
                juce::MathConstants<float>::pi * 1.25f,
                juce::MathConstants<float>::pi * 2.75f, true);
            slider.setMouseDragSensitivity (180);
        }
        slider.setDoubleClickReturnValue (true,
            parameter != nullptr ? parameter->convertFrom0to1 (parameter->getDefaultValue()) : 0.0f);
        slider.setTooltip (help);
        addAndMakeVisible (slider);

        name.setText (shortName, juce::dontSendNotification);
        name.setComponentID (
            "label-" + juce::String (parameterID));
        name.setJustificationType (juce::Justification::centred);
        name.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        name.setColour (juce::Label::textColourId, textSecondary);
        name.setInterceptsMouseClicks (false, false);
        name.setTooltip (help);
        addAndMakeVisible (name);

        value.setJustificationType (juce::Justification::centred);
        value.setComponentID (
            "value-" + juce::String (parameterID));
        value.setFont (juce::FontOptions (10.0f));
        value.setColour (juce::Label::textColourId, valueText);
        value.setInterceptsMouseClicks (false, false);
        value.setTooltip (help);
        addAndMakeVisible (value);

        slider.onValueChange = [this] { updateValue(); };
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, parameterID, slider);
        updateValue();
    }

    void resized() override
    {
        const UiScale s { uiScale };
        auto bounds = getLocalBounds();
        name.setFont (
            juce::FontOptions (s.f (12.0f), juce::Font::bold));
        value.setFont (juce::FontOptions (s.f (10.0f)));

        if (presentation == Presentation::horizontalSlider)
        {
            auto valueBounds = bounds.removeFromRight (s (52));
            auto nameBounds = bounds.removeFromLeft (s (58));
            name.setBounds (nameBounds);
            value.setBounds (valueBounds);
            slider.setBounds (bounds.reduced (s (2), s (5)));
        }
        else
        {
            name.setBounds (bounds.removeFromTop (s (16)));
            auto knobRow = bounds.removeFromTop (s (51));
            slider.setBounds (
                knobRow.withSizeKeepingCentre (s (50), s (50)));
            value.setBounds (bounds.removeFromTop (s (15)));
        }
    }

    void setUiScale (float newScale)
    {
        if (std::abs (newScale - uiScale) < 0.001f)
            return;

        uiScale = newScale;
        resized();
    }

    void setContextActive (
        bool shouldBeActive,
        const juce::String& inactiveValue = "BYP")
    {
        if (contextActive == shouldBeActive
            && inactiveValueText == inactiveValue)
            return;

        inactiveValueText = inactiveValue;
        contextActive = shouldBeActive;
        setEnabled (shouldBeActive);
        slider.setEnabled (shouldBeActive);
        refreshContextAlpha();
        updateValue();
    }

    // A dependency-inert control still accepts edits so a value can be dialled
    // in before its parent is raised, but reads as not currently audible.
    void setDependencySatisfied (bool isSatisfied)
    {
        if (dependencySatisfied == isSatisfied)
            return;

        dependencySatisfied = isSatisfied;
        refreshContextAlpha();
    }

    void setDelayTimeMode (int mode)
    {
        if (parameterIDString != TBDelayParameterIDs::time)
            return;

        delayTimeMode = juce::jlimit (0, 2, mode);
        updateValue();
    }

private:
    class DetentSlider final : public juce::Slider
    {
    public:
        double snapValue (double attemptedValue, DragMode dragMode) override
        {
            if (zeroDetent)
            {
                const auto threshold = (getMaximum() - getMinimum()) * 0.0125;
                if (std::abs (attemptedValue) <= threshold)
                    return 0.0;
            }

            return juce::Slider::snapValue (attemptedValue, dragMode);
        }

        bool zeroDetent = false;
    };

    float uiScale = 1.0f;

    void updateValue()
    {
        if (! contextActive)
        {
            value.setText (
                inactiveValueText, juce::dontSendNotification);
            return;
        }

        if (parameter == nullptr)
            return;

        const auto plain = slider.getValue();
        const auto magnitude = std::abs (plain);
        const auto unit = parameter->getLabel().trim();

        if (parameterIDString == TBDelayParameterIDs::time
            && delayTimeMode == 2)
        {
            const auto safeMilliseconds = std::max (10.0, plain);
            const auto frequency = 1000.0 / safeMilliseconds;
            const auto midiNote = std::round (
                69.0 + 12.0 * std::log2 (frequency / 440.0));
            const auto tunedFrequency =
                440.0 * std::pow (
                            2.0, (midiNote - 69.0) / 12.0);
            const auto decimals =
                tunedFrequency >= 100.0
                    ? 0
                    : tunedFrequency >= 10.0 ? 1 : 2;
            value.setText (
                juce::String (tunedFrequency, decimals) + " Hz",
                juce::dontSendNotification);
            return;
        }

        if (parameterIDString == TBDelayParameterIDs::colorMacro
            || parameterIDString == TBDelayParameterIDs::spaceMacro)
        {
            if (std::abs (plain) < 0.5)
                value.setText ("BASE", juce::dontSendNotification);
            else
            {
                const auto direction =
                    parameterIDString == TBDelayParameterIDs::colorMacro
                        ? (plain < 0.0 ? "CLEAN " : "WORN ")
                        : (plain < 0.0 ? "TIGHT " : "WASH ");
                value.setText (
                    direction + juce::String (juce::roundToInt (magnitude)),
                    juce::dontSendNotification);
            }
            return;
        }

        auto decimals = 2;

        if (dynamic_cast<juce::AudioParameterInt*> (parameter) != nullptr)
            decimals = 0;
        else if (unit == "%")
            decimals = 0;
        else if (unit == "dB" || unit == "st")
            decimals = 1;
        else if (unit == "Hz")
            decimals = magnitude >= 100.0 ? 0 : 2;
        else if (unit == "ms")
            decimals =
                magnitude >= 100.0 ? 0
                                   : magnitude >= 10.0 ? 1 : 2;
        else
            decimals =
                magnitude >= 100.0 ? 0
                                   : magnitude >= 10.0 ? 1 : 2;

        auto display = juce::String (plain, decimals);
        if (unit.isNotEmpty())
            display += " " + unit;
        value.setText (display, juce::dontSendNotification);
    }

    void refreshContextAlpha()
    {
        setAlpha (contextActive ? (dependencySatisfied ? 1.0f : 0.55f)
                                : 0.34f);
    }

    juce::RangedAudioParameter* parameter = nullptr;
    juce::String parameterIDString;
    DetentSlider slider;
    juce::Label name;
    juce::Label value;
    Presentation presentation;
    int delayTimeMode = 0;
    bool contextActive = true;
    bool dependencySatisfied = true;
    juce::String inactiveValueText { "BYP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class TB_DelayAudioProcessorEditor::ChoiceControl final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    enum class Presentation
    {
        steppedRotary,
        segmentedToggle,
        binaryToggle,
        dropdown
    };

    ChoiceControl (juce::AudioProcessorValueTreeState& state,
                   const char* parameterID,
                   const juce::String& nameText,
                   const juce::String& help,
                   Presentation controlPresentation)
        : presentation (controlPresentation)
    {
        setComponentID ("choice-" + juce::String (parameterID));
        setTooltip (help);
        name.setText (nameText, juce::dontSendNotification);
        name.setComponentID (
            "choice-label-" + juce::String (parameterID));
        name.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        name.setColour (juce::Label::textColourId, textSecondary);
        name.setJustificationType (juce::Justification::centred);
        name.setInterceptsMouseClicks (false, false);
        name.setTooltip (help);
        addAndMakeVisible (name);

        if (auto* parameter = state.getParameter (parameterID))
            valueStrings = parameter->getAllValueStrings();

        if (juce::String (parameterID) == TBDelayParameterIDs::taps)
            valueStrings = { "1 echo", "2 echoes", "4 echoes", "8 echoes" };

        if (juce::String (parameterID)
            == TBDelayParameterIDs::transformRoute)
            valueStrings = { "LOOP", "OUTPUT" };

        if (presentation == Presentation::steppedRotary)
        {
            slider.setComponentID (parameterID);
            slider.setSliderStyle (
                juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (
                juce::MathConstants<float>::pi * 1.25f,
                juce::MathConstants<float>::pi * 2.75f, true);
            slider.setTextBoxStyle (
                juce::Slider::NoTextBox, false, 0, 0);
            slider.setMouseDragSensitivity (180);
            slider.setTooltip (help);
            slider.setWantsKeyboardFocus (false);
            slider.setMouseClickGrabsKeyboardFocus (false);
            slider.onValueChange = [this] { updateRotaryValue(); };
            addAndMakeVisible (slider);

            value.setJustificationType (
                juce::Justification::centred);
            value.setComponentID (
                "choice-value-" + juce::String (parameterID));
            value.setFont (juce::FontOptions (10.0f));
            value.setColour (
                juce::Label::textColourId, valueText);
            value.setInterceptsMouseClicks (false, false);
            value.setTooltip (help);
            addAndMakeVisible (value);

            sliderAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (
                    state, parameterID, slider);
            updateRotaryValue();
        }
        else if (presentation == Presentation::segmentedToggle)
        {
            slider.setComponentID (parameterID);
            slider.onValueChange = [this] { updateSegmentButtons(); };
            sliderAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (
                    state, parameterID, slider);

            for (int index = 0;
                 index < valueStrings.size();
                 ++index)
            {
                auto segment = std::make_unique<juce::TextButton> (
                    valueStrings[index]);
                segment->setComponentID (
                    juce::String (parameterID)
                    + "-segment-" + juce::String (index));
                segment->setClickingTogglesState (false);
                segment->setTooltip (help);
                segment->setWantsKeyboardFocus (false);
                segment->setMouseClickGrabsKeyboardFocus (false);
                segment->onClick = [this, index]
                {
                    slider.setValue (
                        static_cast<double> (index),
                        juce::sendNotificationSync);
                };
                addAndMakeVisible (*segment);
                segmentButtons.push_back (std::move (segment));
            }

            updateSegmentButtons();
        }
        else if (presentation == Presentation::binaryToggle)
        {
            button.setComponentID (parameterID);
            button.setClickingTogglesState (true);
            button.setTooltip (help);
            button.setWantsKeyboardFocus (false);
            button.setMouseClickGrabsKeyboardFocus (false);
            button.onStateChange = [this] { updateButtonText(); };
            addAndMakeVisible (button);
            buttonAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    state, parameterID, button);
            updateButtonText();
        }
        else
        {
            box.setComponentID (parameterID);
            for (int index = 0;
                 index < valueStrings.size();
                 ++index)
                box.addItem (valueStrings[index], index + 1);
            box.setTooltip (help);
            box.setWantsKeyboardFocus (false);
            box.setMouseClickGrabsKeyboardFocus (false);
            addAndMakeVisible (box);
            boxAttachment = std::make_unique<
                juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    state, parameterID, box);
        }
    }

    void resized() override
    {
        const UiScale s { uiScale };
        auto bounds = getLocalBounds();
        name.setFont (
            juce::FontOptions (s.f (11.5f), juce::Font::bold));

        if (presentation == Presentation::steppedRotary)
        {
            value.setFont (juce::FontOptions (s.f (10.0f)));
            name.setBounds (bounds.removeFromTop (s (16)));
            auto knobRow = bounds.removeFromTop (s (51));
            slider.setBounds (
                knobRow.withSizeKeepingCentre (s (50), s (50)));
            value.setBounds (bounds.removeFromTop (s (15)));
        }
        else if (presentation == Presentation::segmentedToggle)
        {
            name.setBounds (bounds.removeFromTop (s (16)));
            auto controlBounds = bounds.removeFromTop (s (26));
            const auto count =
                static_cast<int> (segmentButtons.size());
            const auto gap = s (2);
            for (int index = 0; index < count; ++index)
            {
                const auto left =
                    controlBounds.getX()
                    + controlBounds.getWidth() * index / count;
                const auto right =
                    controlBounds.getX()
                    + controlBounds.getWidth() * (index + 1) / count;
                segmentButtons[static_cast<std::size_t> (index)]
                    ->setBounds (
                        juce::Rectangle<int> (
                            left, controlBounds.getY(),
                            right - left, controlBounds.getHeight())
                            .reduced (
                                index == 0 ? 0 : gap / 2,
                                0)
                            .withTrimmedRight (
                                index == count - 1 ? 0 : gap / 2));
            }
        }
        else
        {
            name.setBounds (bounds.removeFromTop (s (16)));
            auto controlBounds = bounds.removeFromTop (s (26));
            box.setBounds (controlBounds);
            button.setBounds (controlBounds);
        }
    }

    void setUiScale (float newScale)
    {
        if (std::abs (newScale - uiScale) < 0.001f)
            return;

        uiScale = newScale;
        resized();
    }

    void setContextActive (bool shouldBeActive)
    {
        setEnabled (shouldBeActive);
        box.setEnabled (shouldBeActive);
        slider.setEnabled (shouldBeActive);
        button.setEnabled (shouldBeActive);
        for (auto& segment : segmentButtons)
            segment->setEnabled (shouldBeActive);
        setAlpha (shouldBeActive ? 1.0f : 0.34f);
    }

private:
    void updateRotaryValue()
    {
        if (valueStrings.isEmpty())
            return;

        const auto index = juce::jlimit (
            0, valueStrings.size() - 1,
            juce::roundToInt (slider.getValue()));
        value.setText (
            valueStrings[index], juce::dontSendNotification);
    }

    void updateButtonText()
    {
        if (valueStrings.isEmpty())
            return;

        const auto index =
            button.getToggleState() && valueStrings.size() > 1
                ? 1
                : 0;
        button.setButtonText (valueStrings[index]);
    }

    void updateSegmentButtons()
    {
        const auto selected = juce::jlimit (
            0, static_cast<int> (segmentButtons.size()) - 1,
            juce::roundToInt (slider.getValue()));
        for (int index = 0;
             index < static_cast<int> (segmentButtons.size());
             ++index)
            segmentButtons[static_cast<std::size_t> (index)]
                ->setToggleState (
                    index == selected,
                    juce::dontSendNotification);
    }

    float uiScale = 1.0f;
    Presentation presentation;
    juce::StringArray valueStrings;
    juce::Label name;
    juce::Label value;
    juce::Slider slider;
    juce::ComboBox box;
    juce::TextButton button;
    std::vector<std::unique_ptr<juce::TextButton>> segmentButtons;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::SliderAttachment>
        sliderAttachment;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        boxAttachment;
    std::unique_ptr<
        juce::AudioProcessorValueTreeState::ButtonAttachment>
        buttonAttachment;
};

class TB_DelayAudioProcessorEditor::ToggleControl final : public juce::Component
{
public:
    ToggleControl (juce::AudioProcessorValueTreeState& state,
                   const char* parameterID,
                   const juce::String& nameText,
                   const juce::String& help)
    {
        setComponentID ("toggle-" + juce::String (parameterID));
        button.setComponentID (parameterID);
        button.setButtonText (nameText);
        button.setClickingTogglesState (true);
        button.setTooltip (help);
        button.setWantsKeyboardFocus (false);
        button.setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (button);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, parameterID, button);
    }

    void resized() override { button.setBounds (getLocalBounds()); }

private:
    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class TB_DelayAudioProcessorEditor::DelayLogoDisplay final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    explicit DelayLogoDisplay (juce::Image sourceImage)
        : image (std::move (sourceImage))
    {
        setComponentID ("delay-logo");
        setTooltip (
            utf8 ("Team Impulse Impact — 반복될수록 흐려지는 Delay echo mark"));
        getProperties().set (
            juce::Identifier ("echo-copy-count"), 3);
    }

    void paint (juce::Graphics& g) override
    {
        const auto size = std::min (
            44.0f, static_cast<float> (getHeight() - 2));
        const auto base = juce::Rectangle<float> (
            static_cast<float> (getWidth()) - size,
            (static_cast<float> (getHeight()) - size) * 0.5f,
            size, size);
        const std::array<float, 3> echoAlpha {
            0.065f, 0.12f, 0.24f
        };

        for (int echo = 0;
             echo < static_cast<int> (echoAlpha.size());
             ++echo)
        {
            const auto distance =
                static_cast<float> (
                    echoAlpha.size() - static_cast<std::size_t> (echo))
                * 12.0f;
            const auto echoBounds =
                base.translated (-distance, 0.0f)
                    .reduced (1.5f * (echo + 1));
            g.setOpacity (
                echoAlpha[static_cast<std::size_t> (echo)]);
            g.drawImageWithin (
                image,
                juce::roundToInt (echoBounds.getX()),
                juce::roundToInt (echoBounds.getY()),
                juce::roundToInt (echoBounds.getWidth()),
                juce::roundToInt (echoBounds.getHeight()),
                juce::RectanglePlacement::centred,
                false);
        }

        g.setOpacity (0.96f);
        g.drawImageWithin (
            image,
            juce::roundToInt (base.getX()),
            juce::roundToInt (base.getY()),
            juce::roundToInt (base.getWidth()),
            juce::roundToInt (base.getHeight()),
            juce::RectanglePlacement::centred,
            false);
        g.setOpacity (1.0f);
    }

private:
    juce::Image image;
};

class TB_DelayAudioProcessorEditor::DelayPatternDisplay final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    explicit DelayPatternDisplay (TB_DelayAudioProcessor& owner)
        : processor (owner)
    {
        setComponentID ("delay-map");
        setTooltip (
            utf8 ("현재 파라미터로 예상되는 L/R echo 시간, grain pitch 범위와 feedback 감쇄"));
    }

    void setUiScale (float newScale)
    {
        if (std::abs (newScale - uiScale) < 0.001f)
            return;

        uiScale = newScale;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const UiScale s { uiScale };
        auto bounds =
            getLocalBounds().toFloat().reduced (0.0f, s.f (11.0f));
        getProperties().set (
            juce::Identifier ("bezel-outer-x"),
            juce::roundToInt (bounds.getX()));
        getProperties().set (
            juce::Identifier ("bezel-outer-width"),
            juce::roundToInt (bounds.getWidth()));
        g.setColour (deepShadow.withAlpha (0.34f));
        g.fillRoundedRectangle (
            bounds.translated (0.0f, s.f (11.0f))
                .expanded (s.f (3.0f), s.f (1.0f)),
            s.f (15.0f));
        g.setColour (deepShadow.withAlpha (0.46f));
        g.fillRoundedRectangle (
            bounds.translated (0.0f, s.f (5.0f)), s.f (15.0f));

        juce::ColourGradient bezel (
            surfaceTop, bounds.getX(), bounds.getY(),
            surfaceBottom, bounds.getX(), bounds.getBottom(), false);
        g.setColour (bezel.getColour (0));
        g.fillRoundedRectangle (bounds, s.f (15.0f));
        g.setColour (outline.withAlpha (0.94f));
        g.drawRoundedRectangle (bounds, s.f (15.0f), 1.0f);

        bounds = bounds.reduced (s.f (3.0f));
        juce::ColourGradient glass (
            displayTop, bounds.getX(), bounds.getY(),
            displayBottom, bounds.getRight(), bounds.getBottom(), true);
        juce::ignoreUnused (glass);
        fillToonBands (
            g, bounds, displayTop, displayBottom, 5, s.f (12.0f));
        g.setColour (outline.withAlpha (0.82f));
        g.drawRoundedRectangle (bounds, s.f (12.0f), 1.0f);
        g.setColour (textPrimary.withAlpha (0.035f));
        g.fillRoundedRectangle (
            bounds.withHeight (s.f (38.0f)), s.f (12.0f));
        g.setColour (highlight.withAlpha (0.11f));
        g.drawLine (
            bounds.getX() + s.f (12.0f), bounds.getY() + 1.0f,
            bounds.getRight() - s.f (12.0f), bounds.getY() + 1.0f,
            0.8f);

        const auto& state = processor.getValueTreeState();
        const auto effective =
            processor.getEffectiveEngineParametersForDisplay();
        TBDelayMapParameters parameters;
        parameters.timeMs = effective.timeMs;
        parameters.bpm = effective.bpm;
        parameters.sampleRate =
            processor.getSampleRate() > 0.0
                ? processor.getSampleRate()
                : 48000.0;
        parameters.offsetPercent = effective.offsetPercent;
        parameters.feedbackPercent = effective.feedbackPercent;
        parameters.spacingPercent = effective.spacingPercent;
        parameters.swingPercent = effective.swingPercent;
        parameters.modDepthMs = effective.modDepthMs;
        parameters.shiftHz = effective.shiftHz;
        parameters.divergePercent = effective.divergePercent;
        parameters.grainPercent = effective.grainPercent;
        parameters.grainSizeMs = effective.grainSizeMs;
        parameters.grainDensity = effective.grainDensity;
        parameters.sprayPercent = effective.sprayPercent;
        parameters.pitchSemitones = effective.pitchSemitones;
        parameters.reversePercent = effective.reversePercent;
        parameters.smearPercent = effective.smearPercent;
        parameters.timeMode = effective.timeMode;
        parameters.division = effective.division;
        parameters.style = effective.style;
        parameters.taps = effective.taps;
        parameters.transformRoute = effective.transformRoute;
        parameters.channelCount = juce::jlimit (
            1, 2, processor.getTotalNumOutputChannels());
        parameters.freeze = effective.freeze;
        parameters.stereoBypass = effective.stereoBypass;
        parameters.colorSpaceBypass = effective.colorSpaceBypass;
        parameters.grainTransformBypass =
            effective.grainTransformBypass;

        const auto prediction = buildTBDelayMapPrediction (parameters);
        auto header = bounds.removeFromTop (s.f (30.0f))
                          .reduced (s.f (15.0f), 0.0f);
        g.setFont (juce::FontOptions (s.f (11.5f), juce::Font::bold));
        g.setColour (textPrimary.withAlpha (0.88f));
        g.drawText ("DELAY MAP", header, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (s.f (9.5f)));
        g.setColour (textPrimary.withAlpha (0.70f));
        const auto modeText =
            parameters.timeMode == 1
                ? "SYNC  " + parameterText (
                      state, TBDelayParameterIDs::division)
                : parameters.timeMode == 2
                      ? juce::String ("TUNE")
                      : juce::String ("FREE");
        g.drawText (
            modeText, header,
            juce::Justification::centredRight);

        auto footer = bounds.removeFromBottom (s.f (28.0f))
                          .reduced (s.f (15.0f), 0.0f);
        auto plot = bounds.reduced (s.f (14.0f), s.f (4.0f));

        // The map is a prediction timeline: input enters at the left, L/R
        // returns occupy separate lanes, and every x position is an actual
        // delay time. This keeps short first echoes visible and makes long
        // feedback paths comparable without decoding a radial metaphor.
        const auto timelineLeft = plot.getX() + s.f (28.0f);
        const auto timelineRight = plot.getRight() - s.f (12.0f);
        const auto timelineWidth = juce::jmax (1.0f,
            timelineRight - timelineLeft);
        const std::array<float, 2> laneY {
            plot.getY() + plot.getHeight() * 0.34f,
            plot.getY() + plot.getHeight() * 0.66f
        };
        const std::array<juce::Colour, 2> laneColours {
            accentPrimary, accentSecondary
        };
        const std::array<const char*, 2> laneNames { "L", "R" };

        const auto timeToX = [timelineLeft, timelineWidth] (double milliseconds)
        {
            return timelineLeft
                + static_cast<float> (
                      tbDelayMapTimeNormalised (milliseconds))
                      * timelineWidth;
        };

        const auto formatTick = [] (double milliseconds)
        {
            if (milliseconds >= 1000.0)
                return juce::String (milliseconds / 1000.0, 1) + " s";
            return juce::String (juce::roundToInt (milliseconds)) + " ms";
        };
        const auto diffusionAmount = juce::jlimit (
            0.0f, 1.0f,
            effective.smearPercent / 100.0f
                * juce::jlimit (0.25f, 1.0f, effective.diffusionSizeMs / 60.0f));

        {
            juce::Graphics::ScopedSaveState clip (g);
            juce::Path window;
            window.addRoundedRectangle (plot, s.f (10.0f));
            g.reduceClipRegion (window);

            const auto sourceX = timeToX (10.0);
            const auto axisLabelY = plot.getBottom() - s.f (17.0f);
            const std::array<double, 6> ticks {
                500.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0
            };

            g.setColour (gridColour.withAlpha (0.30f));
            for (const auto y : laneY)
                g.drawLine (sourceX, y, timelineRight, y, 1.0f);

            for (const auto tick : ticks)
            {
                const auto x = timeToX (tick);
                g.setColour (gridColour.withAlpha (0.16f));
                g.drawLine (
                    x, plot.getY() + s.f (17.0f),
                    x, axisLabelY - s.f (5.0f), 0.8f);
                g.setColour (textSecondary.withAlpha (0.62f));
                g.setFont (juce::FontOptions (s.f (8.5f)));
                g.drawText (
                    formatTick (tick),
                    juce::Rectangle<float> (s.f (42.0f), s.f (13.0f))
                        .withCentre ({ x, axisLabelY }),
                    juce::Justification::centred);
            }

            g.setColour (textPrimary.withAlpha (0.70f));
            g.setFont (juce::FontOptions (s.f (8.5f), juce::Font::bold));
            g.drawText (
                "IN",
                juce::Rectangle<float> (sourceX - s.f (16.0f), laneY[0] - s.f (28.0f),
                                        s.f (32.0f), s.f (16.0f)),
                juce::Justification::centred);
            g.setColour (accentPrimary.withAlpha (0.80f));
            g.drawText (
                "L",
                juce::Rectangle<float> (plot.getX(), laneY[0] - s.f (9.0f),
                                        s.f (18.0f), s.f (18.0f)),
                juce::Justification::centredLeft);
            g.setColour (accentSecondary.withAlpha (0.80f));
            g.drawText (
                "R",
                juce::Rectangle<float> (plot.getX(), laneY[1] - s.f (9.0f),
                                        s.f (18.0f), s.f (18.0f)),
                juce::Justification::centredLeft);

            g.setColour (textPrimary.withAlpha (0.70f));
            g.drawLine (
                sourceX, laneY[0] - s.f (19.0f),
                sourceX, laneY[1] + s.f (19.0f), 1.8f);

            for (int lane = 0; lane < 2; ++lane)
            {
                const auto laneColour =
                    laneColours[static_cast<std::size_t> (lane)];

                const auto y = laneY[static_cast<std::size_t> (lane)];
                auto overflowed = false;
                for (const auto& event : prediction.events)
                {
                    if (event.lane != lane)
                        continue;

                    const auto eventSpread = event.kind == TBDelayMapEvent::Kind::grainCloud
                                                 ? std::max (0.0, event.timeSpreadMs)
                                                 : 0.0;
                    if (event.timeMs > delayMapHorizonMs)
                    {
                        overflowed = true;
                        continue;
                    }
                    if (event.timeMs + eventSpread > delayMapHorizonMs)
                        overflowed = true;

                    const auto level = juce::jlimit (0.0f, 1.0f, event.level);
                    const auto eventX = timeToX (event.timeMs);

                    if (event.kind == TBDelayMapEvent::Kind::grainCloud)
                    {
                        const auto spread = juce::jmax (
                            s.f (3.0f),
                            std::abs (timeToX (event.timeMs + event.timeSpreadMs)
                                      - timeToX (event.timeMs - event.timeSpreadMs))
                                * 0.5f);
                        g.setColour (laneColour.withAlpha (0.10f * level + 0.04f));
                        g.fillRoundedRectangle (
                            juce::Rectangle<float> (
                                eventX - spread, y - s.f (7.0f),
                                spread * 2.0f, s.f (14.0f)),
                            s.f (7.0f));
                        g.setColour (laneColour.withAlpha (0.30f * level + 0.08f));
                        g.drawLine (eventX, y - s.f (10.0f), eventX, y + s.f (10.0f), 1.2f);
                        const auto pitch = tbDelayMapPitchNormalised (event);
                        const auto pitchY = y - pitch * s.f (12.0f);
                        if (std::abs (pitch) > 0.025f)
                        {
                            g.setColour (laneColour.withAlpha (0.68f));
                            g.drawLine (eventX, y, eventX, pitchY, 1.0f);
                            juce::Path pitchMark;
                            pitchMark.addTriangle (
                                eventX,
                                pitchY - (pitch > 0.0f ? s.f (3.0f) : -s.f (3.0f)),
                                eventX - s.f (3.0f), pitchY,
                                eventX + s.f (3.0f), pitchY);
                            g.fillPath (pitchMark);
                        }
                        continue;
                    }

                    const auto markerWidth =
                        1.0f + 3.4f * level + (event.repeat == 0 ? 3.2f : 0.0f);
                    const auto markerHeight =
                        event.repeat == 0 ? s.f (13.0f) : s.f (5.0f + 7.0f * level);
                    const auto tailX = timeToX (
                        event.timeMs + std::max (0.0, event.decayMs));
                    if (event.decayMs > 0.0)
                    {
                        const auto visibleTail = juce::jmin (timelineRight, tailX);
                        juce::Path decay;
                        decay.startNewSubPath (eventX, y - markerHeight);
                        decay.lineTo (visibleTail, y);
                        decay.lineTo (eventX, y + markerHeight);
                        decay.closeSubPath();
                        g.setColour (
                            laneColour.withAlpha (0.035f + 0.08f * level));
                        g.fillPath (decay);
                    }

                    const auto pitch = tbDelayMapPitchNormalised (event);
                    const auto pitchY = y - pitch * s.f (14.0f);
                    if (diffusionAmount > 0.01f)
                    {
                        g.setColour (laneColour.withAlpha (
                            0.05f + 0.12f * diffusionAmount * level));
                        g.drawLine (
                            eventX, y - s.f (4.0f + 8.0f * diffusionAmount),
                            eventX, y + s.f (4.0f + 8.0f * diffusionAmount),
                            s.f (3.0f + 9.0f * diffusionAmount));
                    }
                    g.setColour (laneColour.withAlpha (0.30f + 0.62f * level));
                    g.drawLine (eventX, y, eventX, pitchY, 1.1f);
                    g.drawLine (
                        eventX, y - markerHeight,
                        eventX, y + markerHeight, markerWidth);
                    if (std::abs (pitch) > 0.025f)
                    {
                        juce::Path pitchMark;
                        pitchMark.addTriangle (
                            eventX, pitchY - (pitch > 0.0f ? s.f (3.0f) : -s.f (3.0f)),
                            eventX - s.f (3.0f), pitchY,
                            eventX + s.f (3.0f), pitchY);
                        g.setColour (laneColour.withAlpha (0.75f));
                        g.fillPath (pitchMark);
                    }

                    if (tailX > timelineRight)
                        overflowed = true;
                }

                if (overflowed)
                {
                    g.setColour (laneColour.withAlpha (0.65f));
                    g.drawLine (
                        timelineRight, y - s.f (13.0f),
                        timelineRight, y + s.f (13.0f), 1.4f);
                    g.setFont (juce::FontOptions (s.f (8.0f), juce::Font::bold));
                    g.drawText (
                        ">5 s",
                        juce::Rectangle<float> (s.f (30.0f), s.f (12.0f))
                            .withCentre ({ timelineRight - s.f (16.0f), y - s.f (16.0f) }),
                        juce::Justification::centred);
                }
            }
        }

        // The first event is always a wide, visible bar, including at 10 ms.
        for (int lane = 0; lane < 2; ++lane)
        {
            const auto laneColour =
                laneColours[static_cast<std::size_t> (lane)];
            const auto y = laneY[static_cast<std::size_t> (lane)];
            const auto firstMs =
                prediction.primaryEventTimeMs[static_cast<std::size_t> (lane)];
            const auto firstX = timeToX (firstMs);
            const auto markerWidth = 1.0f + 3.4f
                * juce::jlimit (0.0f, 1.0f, prediction.feedbackGain)
                + 3.2f + 2.0f;
            g.setColour (deepShadow.withAlpha (0.58f));
            g.drawLine (
                firstX, y - s.f (17.0f),
                firstX, y + s.f (17.0f), markerWidth + s.f (2.0f));
            g.setColour (laneColour.withAlpha (0.96f));
            g.drawLine (
                firstX, y - s.f (15.0f),
                firstX, y + s.f (15.0f), markerWidth);

            const auto anchorTag = lane == 0 ? "l" : "r";
            getProperties().set (
                juce::Identifier (
                    juce::String ("primary-anchor-") + anchorTag + "-x"),
                juce::roundToInt (firstX));
            getProperties().set (
                juce::Identifier (
                    juce::String ("primary-anchor-") + anchorTag + "-y"),
                juce::roundToInt (y));
            getProperties().set (
                juce::Identifier (
                    juce::String ("primary-anchor-") + anchorTag + "-width"),
                juce::roundToInt (markerWidth));

            g.setColour (laneColour.withAlpha (0.92f));
            g.setFont (juce::FontOptions (s.f (9.0f), juce::Font::bold));
            g.drawText (
                juce::String (juce::roundToInt (firstMs)) + "ms",
                juce::Rectangle<float> (s.f (48.0f), s.f (12.0f))
                    .withCentre ({ firstX, y - s.f (22.0f) }),
                juce::Justification::centred);
        }

        g.setFont (juce::FontOptions (s.f (9.5f)));
        g.setColour (textPrimary.withAlpha (0.82f));
        g.drawText (
            "FIRST  L "
                + juce::String (
                    juce::roundToInt (prediction.primaryEventTimeMs[0]))
                + "ms  /  R "
                + juce::String (
                    juce::roundToInt (prediction.primaryEventTimeMs[1]))
                + utf8 ("ms  ·  ")
                + juce::String (
                    juce::roundToInt (prediction.feedbackGain * 100.0f))
                + "% DECAY",
            footer, juce::Justification::centred);

    }

private:
    TB_DelayAudioProcessor& processor;
    float uiScale = 1.0f;
};

TB_DelayAudioProcessorEditor::TB_DelayAudioProcessorEditor (TB_DelayAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner)
{
    lookAndFeel = std::make_unique<DelayLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);

    logo = std::make_unique<DelayLogoDisplay> (
        juce::ImageCache::getFromMemory (
            BinaryData::tb_mouse_icon_png,
            BinaryData::tb_mouse_icon_pngSize));
    addAndMakeVisible (*logo);

    productLabel.setText ("TB_DELAY", juce::dontSendNotification);
    productLabel.setComponentID ("product-title");
    productLabel.setFont (
        juce::FontOptions (22.0f, juce::Font::bold));
    productLabel.setColour (juce::Label::textColourId, textPrimary);
    productLabel.setJustificationType (
        juce::Justification::centredLeft);
    addAndMakeVisible (productLabel);

    enum class HeaderIconKind { previous, next, save, undo, redo };
    const auto makeHeaderIcon = [] (HeaderIconKind kind, juce::Colour colour)
    {
        auto icon = std::make_unique<juce::DrawablePath>();
        juce::Path path;
        if (kind == HeaderIconKind::previous)
        {
            path.startNewSubPath (15.5f, 4.0f);
            path.lineTo (7.0f, 12.0f);
            path.lineTo (15.5f, 20.0f);
        }
        else if (kind == HeaderIconKind::next)
        {
            path.startNewSubPath (8.5f, 4.0f);
            path.lineTo (17.0f, 12.0f);
            path.lineTo (8.5f, 20.0f);
        }
        else if (kind == HeaderIconKind::save)
        {
            path.startNewSubPath (3.0f, 2.0f);
            path.lineTo (17.0f, 2.0f);
            path.lineTo (22.0f, 7.0f);
            path.lineTo (22.0f, 22.0f);
            path.lineTo (3.0f, 22.0f);
            path.closeSubPath();
            path.addRectangle (7.0f, 2.0f, 9.0f, 7.0f);
            path.addRectangle (7.0f, 14.0f, 11.0f, 6.0f);
        }
        else if (kind == HeaderIconKind::undo)
        {
            path.startNewSubPath (10.0f, 5.5f);
            path.lineTo (4.5f, 10.0f);
            path.lineTo (10.0f, 14.5f);
            path.startNewSubPath (5.0f, 10.0f);
            path.cubicTo (9.0f, 4.8f, 20.0f, 5.8f, 20.0f, 14.0f);
            path.cubicTo (20.0f, 18.0f, 17.5f, 20.0f, 14.0f, 20.0f);
        }
        else
        {
            path.startNewSubPath (14.0f, 5.5f);
            path.lineTo (19.5f, 10.0f);
            path.lineTo (14.0f, 14.5f);
            path.startNewSubPath (19.0f, 10.0f);
            path.cubicTo (15.0f, 4.8f, 4.0f, 5.8f, 4.0f, 14.0f);
            path.cubicTo (4.0f, 18.0f, 6.5f, 20.0f, 10.0f, 20.0f);
        }
        icon->setPath (path);
        icon->setFill (juce::Colours::transparentBlack);
        icon->setStrokeFill (colour);
        icon->setStrokeThickness (kind == HeaderIconKind::save ? 1.7f : 2.0f);
        return icon;
    };
    const auto configureHeaderIcon = [&makeHeaderIcon] (
        juce::DrawableButton& button, HeaderIconKind kind, int edgeIndent)
    {
        const auto normalIcon = makeHeaderIcon (kind, textSecondary);
        const auto hoverIcon = makeHeaderIcon (kind, accentPrimary);
        const auto downIcon = makeHeaderIcon (kind, accentSecondary);
        button.setImages (normalIcon.get(), hoverIcon.get(), downIcon.get());
        button.setEdgeIndent (edgeIndent);
    };

    configureHeaderIcon (previousPresetButton, HeaderIconKind::previous, 8);
    configureHeaderIcon (nextPresetButton, HeaderIconKind::next, 8);
    configureHeaderIcon (saveButton, HeaderIconKind::save, 7);
    configureHeaderIcon (undoButton, HeaderIconKind::undo, 7);
    configureHeaderIcon (redoButton, HeaderIconKind::redo, 7);

    previousPresetButton.setComponentID ("preset-previous");
    presetBox.setComponentID ("preset-box");
    nextPresetButton.setComponentID ("preset-next");
    saveButton.setComponentID ("preset-save");
    undoButton.setComponentID ("preset-undo");
    redoButton.setComponentID ("preset-redo");

    previousPresetButton.setRepeatSpeed (500, 240, 160);
    nextPresetButton.setRepeatSpeed (500, 240, 160);

    for (auto* button : std::array<juce::Button*, 5> {
             &previousPresetButton, &nextPresetButton, &saveButton,
             &undoButton, &redoButton })
    {
        button->setColour (juce::TextButton::buttonColourId, surfaceBottom);
        button->setColour (juce::TextButton::buttonOnColourId, surfaceTop);
        button->setWantsKeyboardFocus (false);
        button->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*button);
    }

    statusLabel.setFont (juce::FontOptions (10.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    presetBox.setTextWhenNothingSelected ("PRESETS");
    presetBox.setColour (
        juce::ComboBox::textColourId, textSecondary);
    presetBox.setColour (
        juce::ComboBox::arrowColourId,
        textSecondary.withAlpha (0.82f));
    presetBox.setWantsKeyboardFocus (false);
    presetBox.setMouseClickGrabsKeyboardFocus (false);
    presetBox.onChange = [this]
    {
        const auto presetIndex = presetBox.getSelectedId() - 1;
        if (presetIndex >= 0)
            loadPresetAt (presetIndex);
    };
    addAndMakeVisible (presetBox);
    rebuildPresetMenu();

    previousPresetButton.onClick = [this] { stepPreset (-1); };
    nextPresetButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { showSaveDialog(); };
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };

    delayMapDisplay = std::make_unique<DelayPatternDisplay> (processor);
    addAndMakeVisible (*delayMapDisplay);

    const std::array<const char*, 27> knobIDs {
        TBDelayParameterIDs::time, TBDelayParameterIDs::offset,
        TBDelayParameterIDs::feedback, TBDelayParameterIDs::spacing,
        TBDelayParameterIDs::swing, TBDelayParameterIDs::width,
        TBDelayParameterIDs::mix, TBDelayParameterIDs::duck,
        TBDelayParameterIDs::output, TBDelayParameterIDs::lowCut,
        TBDelayParameterIDs::highCut, TBDelayParameterIDs::drive,
        TBDelayParameterIDs::age, TBDelayParameterIDs::modRate,
        TBDelayParameterIDs::modDepth, TBDelayParameterIDs::smear,
        TBDelayParameterIDs::size, TBDelayParameterIDs::shift,
        TBDelayParameterIDs::diverge, TBDelayParameterIDs::grain,
        TBDelayParameterIDs::grainSize, TBDelayParameterIDs::density,
        TBDelayParameterIDs::spray, TBDelayParameterIDs::pitch,
        TBDelayParameterIDs::reverse,
        TBDelayParameterIDs::colorMacro,
        TBDelayParameterIDs::spaceMacro
    };
    const std::array<const char*, 27> knobNames {
        "DELAY TIME", "R OFFSET", "FEEDBACK", "SPACING", "SWING", "WIDTH", "MIX", "DUCK",
        "OUTPUT", "LOW CUT", "HIGH CUT", "DISTORT", "WEAR", "SPEED", "WOBBLE",
        "BLUR", "SPACE", "SHIFT", "L/R SHIFT", "GRAIN", "GRAIN SIZE", "DENSITY",
        "SPRAY", "PITCH", "REVERSE", "TONE", "DIFFUSION"
    };
    const std::array<juce::String, 27> knobHelp {
        utf8 ("Free에서는 ms, Sync에서는 음표, Tune에서는 공진 주파수로 조절하는 기본 딜레이 시간"),
        utf8 ("오른쪽 시간 비율과 comb 방향"), utf8 ("반복 감쇠와 발진 접근량"),
        utf8 ("멀티탭 간격"), utf8 ("짝수 탭의 groove 이동"), utf8 ("wet 스테레오 폭"), "constant-power dry/wet",
        utf8 ("원음이 들어올 때 반복음을 낮춰 소리를 또렷하게 유지합니다."),
        utf8 ("최종 출력 보정"),
        utf8 ("이 주파수 아래의 저음을 반복마다 줄입니다."),
        utf8 ("이 주파수 위의 고음을 반복마다 줄입니다."),
        utf8 ("올릴수록 반복음이 더 눌리고 거칠어집니다."),
        utf8 ("올릴수록 오래된 장비처럼 어둡고 불안정해집니다."),
        utf8 ("반복음이 흔들리는 속도를 조절합니다."),
        utf8 ("반복음의 시간 흔들림 폭을 조절합니다."),
        utf8 ("올릴수록 또렷한 반복이 퍼진 잔향처럼 흐려집니다."),
        utf8 ("퍼지는 잔향의 길이와 공간감을 조절합니다."),
        utf8 ("반복마다 음높이를 올리거나 내리는 주파수 이동"),
        utf8 ("좌우 shift의 반대 방향 비율"), utf8 ("일반 echo와 grain layer 비율"),
        utf8 ("grain window 길이"), utf8 ("동시 grain overlap 수"), utf8 ("grain 시작 위치와 시간 분산"),
        utf8 ("grain 재생 피치"), utf8 ("grain별 reverse 확률"),
        utf8 ("왼쪽은 더 깨끗하고 열린 반복, 가운데는 프리셋 원형, 오른쪽은 더 낡고 어두운 반복입니다."),
        utf8 ("왼쪽은 단단하고 정적인 반복, 가운데는 프리셋 원형, 오른쪽은 더 움직이고 넓게 퍼지는 반복입니다.")
    };

    std::array<ContinuousControl::Presentation, 27> presentations;
    presentations.fill (
        ContinuousControl::Presentation::rotary);
    presentations[6] =
        ContinuousControl::Presentation::horizontalSlider;
    presentations[8] =
        ContinuousControl::Presentation::horizontalSlider;
    constexpr std::array<std::size_t, 12> visibleContinuousIndices {
        0, 2, 5, 6, 7, 8, 17, 19, 23, 24, 25, 26
    };

    for (size_t i = 0; i < continuousControls.size(); ++i)
    {
        continuousControls[i] = std::make_unique<ContinuousControl> (
            processor.getValueTreeState(), knobIDs[i], knobNames[i], knobHelp[i],
            presentations[i]);
        if (std::find (visibleContinuousIndices.begin(),
                       visibleContinuousIndices.end(), i)
            != visibleContinuousIndices.end())
            addAndMakeVisible (*continuousControls[i]);
    }

    const std::array<const char*, 7> choiceIDs {
        TBDelayParameterIDs::timeMode, TBDelayParameterIDs::division,
        TBDelayParameterIDs::style, TBDelayParameterIDs::taps,
        TBDelayParameterIDs::character, TBDelayParameterIDs::timeMotion,
        TBDelayParameterIDs::transformRoute
    };
    const std::array<const char*, 7> choiceNames {
        "TIME MODE", "DIVISION", "STYLE", "ECHOES", "CHARACTER", "TIME CHANGE", "SHIFT ROUTE"
    };
    const std::array<juce::String, 7> choiceHelp {
        utf8 ("Free ms, host Sync, tuned comb를 직접 선택합니다."),
        utf8 ("Sync에서 반복 간격으로 사용하는 host tempo 음표입니다."),
        "Single, Dual, Ping-Pong, Ratio echo pattern", utf8 ("한 번의 입력에서 만들어지는 echo 개수"),
        utf8 ("반복음의 재질을 Clean, Tape, BBD, Digital 중에서 선택합니다."),
        utf8 ("Delay Time 변경 시 CROSSFADE는 깨끗하게 전환하고 GLIDE는 테이프처럼 피치를 미끄러뜨립니다."),
        utf8 ("LOOP은 반복마다 shift를 누적하고 OUTPUT은 전체 반복 뒤 한 번 적용합니다.")
    };
    const std::array<ChoiceControl::Presentation, 7>
        choicePresentations {
            ChoiceControl::Presentation::dropdown,
            ChoiceControl::Presentation::steppedRotary,
            ChoiceControl::Presentation::steppedRotary,
            ChoiceControl::Presentation::steppedRotary,
            ChoiceControl::Presentation::steppedRotary,
            ChoiceControl::Presentation::binaryToggle,
            ChoiceControl::Presentation::dropdown
        };

    for (size_t i = 0; i < choices.size(); ++i)
    {
        choices[i] = std::make_unique<ChoiceControl> (
            processor.getValueTreeState(), choiceIDs[i],
            choiceNames[i], choiceHelp[i],
            choicePresentations[i]);
        if (i != 5)
            addAndMakeVisible (*choices[i]);
    }

    tapTempoButton.setComponentID ("tap-tempo");
    tapTempoButton.setClickingTogglesState (false);
    tapTempoButton.setTriggeredOnMouseDown (true);
    tapTempoButton.setWantsKeyboardFocus (false);
    tapTempoButton.setMouseClickGrabsKeyboardFocus (false);
    tapTempoButton.setTooltip (
        utf8 ("두 번 이상 탭해 반복 간격을 입력합니다. Free/Tune은 Delay Time, Sync는 호스트 BPM에 가장 가까운 음표를 선택합니다."));
    tapTempoButton.onClick = [this] { handleTapTempo(); };
    addAndMakeVisible (tapTempoButton);

    freezeToggle = std::make_unique<ToggleControl> (
        processor.getValueTreeState(), TBDelayParameterIDs::freeze, "FREEZE",
        utf8 ("입력을 부드럽게 차단하고 현재 tail을 안전하게 유지"));
    addAndMakeVisible (*freezeToggle);

    const std::array<const char*, 3> bypassIDs {
        TBDelayParameterIDs::stereoBypass,
        TBDelayParameterIDs::colorSpaceBypass,
        TBDelayParameterIDs::grainTransformBypass
    };
    const std::array<juce::String, 3> bypassHelp {
        utf8 ("Stereo topology, offset, width를 중립 경로로 바이패스"),
        utf8 ("Character, filter, modulation, diffusion을 바이패스"),
        utf8 ("Grain과 frequency transform을 바이패스")
    };
    for (size_t index = 0;
         index < sectionBypassToggles.size();
         ++index)
    {
        sectionBypassToggles[index] =
            std::make_unique<ToggleControl> (
                processor.getValueTreeState(),
                bypassIDs[index], "BYP", bypassHelp[index]);
        addAndMakeVisible (*sectionBypassToggles[index]);
    }

    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 2000);

    // The window scales as one piece: the constrainer locks the aspect ratio so
    // dragging the corner only ever changes the scale factor, and the size the
    // user settled on comes back with the session. The stored scale is read
    // first because setResizeLimits already constrains the empty bounds, and
    // that resize would otherwise overwrite it with the minimum.
    const auto storedScale = juce::jlimit (
        minimumUiScale, maximumUiScale, processor.getEditorScale());

    setResizable (true, true);
    setResizeLimits (
        juce::roundToInt (baseWidth * minimumUiScale),
        juce::roundToInt (baseHeight * minimumUiScale),
        juce::roundToInt (baseWidth * maximumUiScale),
        juce::roundToInt (baseHeight * maximumUiScale));
    if (auto* sizeConstrainer = getConstrainer())
        sizeConstrainer->setFixedAspectRatio (
            static_cast<double> (baseWidth)
                / static_cast<double> (baseHeight));

    setSize (
        juce::roundToInt (baseWidth * storedScale),
        juce::roundToInt (baseHeight * storedScale));
    processor.getValueTreeState().addParameterListener (
        TBDelayParameterIDs::timeMode, this);
    processor.getValueTreeState().addParameterListener (
        TBDelayParameterIDs::stereoBypass, this);
    processor.getValueTreeState().addParameterListener (
        TBDelayParameterIDs::colorSpaceBypass, this);
    processor.getValueTreeState().addParameterListener (
        TBDelayParameterIDs::grainTransformBypass, this);
    processor.getValueTreeState().addParameterListener (
        TBDelayParameterIDs::grain, this);
    updateControlAvailability();
    updatePresetDisplay();
    startTimerHz (30);
}

TB_DelayAudioProcessorEditor::~TB_DelayAudioProcessorEditor()
{
    processor.getValueTreeState().removeParameterListener (
        TBDelayParameterIDs::timeMode, this);
    processor.getValueTreeState().removeParameterListener (
        TBDelayParameterIDs::stereoBypass, this);
    processor.getValueTreeState().removeParameterListener (
        TBDelayParameterIDs::colorSpaceBypass, this);
    processor.getValueTreeState().removeParameterListener (
        TBDelayParameterIDs::grainTransformBypass, this);
    processor.getValueTreeState().removeParameterListener (
        TBDelayParameterIDs::grain, this);
    stopTimer();
    tooltipWindow.reset();
    setLookAndFeel (nullptr);
}

float TB_DelayAudioProcessorEditor::uiScale() const noexcept
{
    // A host that ignores the constrainer can hand over any size, so the scale
    // follows whichever axis is tightest and the layout simply spreads into the
    // extra room on the other one.
    return juce::jlimit (
        minimumUiScale, maximumUiScale,
        juce::jmin (
            static_cast<float> (getWidth())
                / static_cast<float> (baseWidth),
            static_cast<float> (getHeight())
                / static_cast<float> (baseHeight)));
}

void TB_DelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (canvasTop, 0.0f, 0.0f,
                                     canvasBottom, 0.0f, static_cast<float> (getHeight()), false);
    juce::ignoreUnused (background);
    fillToonBands (
        g, getLocalBounds().toFloat(), canvasTop, canvasBottom, 6);

    const auto width = static_cast<float> (getWidth());
    const auto height = static_cast<float> (getHeight());

    // Shoreline: the bottom-left corner is dry sand, met by a foam edge, so the
    // chassis reads as water seen from above rather than a flat blue panel.
    {
        const auto shoreLeftY = height * 0.475f;
        const auto shoreRightX = width * 0.545f;

        juce::Path sand;
        sand.startNewSubPath (0.0f, shoreLeftY);
        sand.cubicTo (
            width * 0.115f, shoreLeftY + height * 0.195f,
            width * 0.250f, height * 0.955f,
            shoreRightX, height);
        sand.lineTo (0.0f, height);
        sand.closeSubPath();

        juce::ColourGradient beach (
            sandFar.withAlpha (0.92f), width * 0.30f, height * 0.70f,
            sandNear, 0.0f, height, false);
        g.setColour (beach.getColour (0));
        g.fillPath (sand);

        juce::Path foam;
        foam.startNewSubPath (0.0f, shoreLeftY);
        foam.cubicTo (
            width * 0.115f, shoreLeftY + height * 0.195f,
            width * 0.250f, height * 0.955f,
            shoreRightX, height);

        g.setColour (highlight.withAlpha (0.92f));
        g.strokePath (foam, juce::PathStrokeType (5.0f));
        g.setColour (highlight.withAlpha (0.34f));
        g.strokePath (
            foam.createPathWithRoundedCorners (0.0f),
            juce::PathStrokeType (11.0f));

        juce::Path spill;
        spill.startNewSubPath (0.0f, shoreLeftY - height * 0.052f);
        spill.cubicTo (
            width * 0.125f, shoreLeftY + height * 0.150f,
            width * 0.275f, height * 0.890f,
            width * 0.455f, height);
        g.setColour (highlight.withAlpha (0.24f));
        g.strokePath (spill, juce::PathStrokeType (2.4f));
    }

    juce::ColourGradient warmGlow (
        accentPrimary.withAlpha (0.075f), width * 0.12f, 0.0f,
        accentPrimary.withAlpha (0.0f), width * 0.64f, height * 0.54f,
        true);
    g.setColour (warmGlow.getColour (0));
    g.fillRect (getLocalBounds());

    juce::ColourGradient sageGlow (
        accentSecondary.withAlpha (0.055f), width * 0.88f, 0.0f,
        accentSecondary.withAlpha (0.0f), width * 0.42f, height * 0.46f,
        true);
    g.setColour (sageGlow.getColour (0));
    g.fillRect (getLocalBounds());

    const UiScale s { uiScale() };
    auto headerBounds =
        getLocalBounds().reduced (s (outerMargin))
            .removeFromTop (s (headerHeight)).toFloat();
    drawPanel (g, headerBounds, s.f (9.0f));

    for (const auto screwX
         : { headerBounds.getX() + s.f (10.0f),
             headerBounds.getRight() - s.f (10.0f) })
    {
        const auto screwRadius = s.f (3.2f);
        const auto screw = juce::Rectangle<float> (
            screwX - screwRadius,
            headerBounds.getCentreY() - screwRadius,
            screwRadius * 2.0f, screwRadius * 2.0f);
        g.setColour (deepShadow.withAlpha (0.90f));
        g.fillEllipse (screw.translated (0.0f, s.f (0.8f)));
        juce::ColourGradient metal (
            highlight.withAlpha (0.78f),
            screw.getX(), screw.getY(),
            surfaceBottom, screw.getRight(), screw.getBottom(), false);
        g.setColour (metal.getColour (0));
        g.fillEllipse (screw);
        g.setColour (outline.brighter (0.10f));
        g.drawEllipse (screw, 0.8f);
        g.setColour (deepShadow.withAlpha (0.78f));
        g.drawLine (
            screw.getCentreX() - s.f (1.7f),
            screw.getCentreY() + s.f (1.7f),
            screw.getCentreX() + s.f (1.7f),
            screw.getCentreY() - s.f (1.7f),
            0.8f);
    }

    const auto geometry = computeBoardGeometry (getLocalBounds(), s.scale);
    drawPanel (g, geometry.board.toFloat(), s.f (10.0f));

    const std::array<juce::String, 4> sectionNames {{
        "REPEAT", "STEREO",
        "CHARACTER / SPACE", "TRANSFORM"
    }};

    for (size_t index = 0; index < geometry.quadrants.size(); ++index)
        drawRaisedSection (
            g,
            geometry.quadrants[index].toFloat().reduced (1.0f),
            index == 0);

    for (size_t section = 0; section < geometry.quadrants.size(); ++section)
    {
        const auto title =
            geometry.quadrants[section].withHeight (s (sectionTitleHeight));
        g.setColour (sectionAccent.withAlpha (
            section == 0 ? 0.90f : 0.58f));
        g.fillRoundedRectangle (
            static_cast<float> (title.getX() + s (sectionInsetLeft - 5)),
            static_cast<float> (title.getY() + s (10)),
            s.f (3.0f), s.f (16.0f), s.f (1.5f));
        g.setFont (juce::FontOptions (s.f (17.0f), juce::Font::bold));
        g.setColour (textPrimary.withAlpha (0.92f));
        auto titleText = title.withTrimmedLeft (s (sectionInsetLeft + 6));
        if (section > 0)
            titleText = titleText.withTrimmedRight (s (52));
        g.drawFittedText (
            sectionNames[section],
            titleText,
            juce::Justification::centredLeft,
            1, 0.72f);
    }

    const auto brandLine =
        productLabel.getBounds().toFloat()
            .withY (headerBounds.getBottom() - s.f (2.0f))
            .withHeight (s.f (2.0f))
            .withWidth (s.f (76.0f));
    g.setColour (sectionAccent.withAlpha (0.78f));
    g.fillRoundedRectangle (brandLine, s.f (1.0f));
}

void TB_DelayAudioProcessorEditor::resized()
{
    const auto scale = uiScale();
    const UiScale s { scale };
    processor.setEditorScale (scale);
    lookAndFeel->setUiScale (scale);
    for (auto& control : continuousControls)
        if (control != nullptr)
            control->setUiScale (scale);
    for (auto& choice : choices)
        if (choice != nullptr)
            choice->setUiScale (scale);
    delayMapDisplay->setUiScale (scale);

    auto bounds = getLocalBounds().reduced (s (outerMargin));
    const auto header = bounds.removeFromTop (s (headerHeight));
    productLabel.setFont (
        juce::FontOptions (s.f (22.0f), juce::Font::bold));
    productLabel.setBounds (
        header.withTrimmedLeft (s (22))
            .withWidth (s (164))
            .reduced (0, s (4)));
    logo->setBounds (
        juce::Rectangle<int> (
            header.getRight() - s (108),
            header.getY() + s (4),
            s (88), header.getHeight() - s (8)));

    const auto presetBounds =
        header.withSizeKeepingCentre (s (176), s (30));
    presetBox.setBounds (presetBounds);
    previousPresetButton.setBounds (
        presetBounds.withX (presetBounds.getX() - s (34))
            .withWidth (s (30)));
    nextPresetButton.setBounds (
        presetBounds.withX (presetBounds.getRight() + s (4))
            .withWidth (s (30)));
    saveButton.setBounds (
        presetBounds.withX (presetBounds.getRight() + s (42))
            .withWidth (s (30)));
    undoButton.setBounds (
        presetBounds.withX (presetBounds.getRight() + s (84))
            .withWidth (s (30)));
    redoButton.setBounds (
        presetBounds.withX (presetBounds.getRight() + s (120))
            .withWidth (s (30)));

    bounds.removeFromTop (s (headerGap));
    auto footer = bounds.removeFromBottom (s (footerHeight));
    auto finalStrip = footer.removeFromRight (
        juce::jmin (s (520), footer.getWidth() - s (180)));
    const std::array<int, 2> finalIndices { 6, 8 };
    for (int column = 0;
         column < static_cast<int> (finalIndices.size());
         ++column)
    {
        const auto left =
            finalStrip.getX()
            + finalStrip.getWidth() * column
                  / static_cast<int> (finalIndices.size());
        const auto right =
            finalStrip.getX()
            + finalStrip.getWidth() * (column + 1)
                  / static_cast<int> (finalIndices.size());
        continuousControls[static_cast<std::size_t> (
            finalIndices[static_cast<std::size_t> (column)])]
            ->setBounds (
                juce::Rectangle<int> (
                    left, finalStrip.getY(),
                    right - left, finalStrip.getHeight())
                    .reduced (s (3), 0));
    }
    statusLabel.setFont (juce::FontOptions (s.f (10.0f)));
    statusLabel.setBounds (footer);

    const auto geometry = computeBoardGeometry (getLocalBounds(), scale);
    delayMapDisplay->setBounds (geometry.analyser);

    // Section content keeps clear of its own panel border so no control sits
    // flush against the left edge.
    std::array<juce::Rectangle<int>, 4> sectionContent;
    for (size_t quadrant = 0;
         quadrant < geometry.quadrants.size();
         ++quadrant)
    {
        auto content = geometry.quadrants[quadrant];
        content.removeFromTop (s (sectionTitleHeight));
        sectionContent[quadrant] =
            content.withTrimmedLeft (s (sectionInsetLeft))
                .withTrimmedRight (s (sectionInsetRight));
    }

    const auto setComponentRow = [&s] (
        const juce::Rectangle<int>& row,
        const auto& controls)
    {
        const auto count = static_cast<int> (controls.size());
        for (int column = 0; column < count; ++column)
        {
            const auto left =
                row.getX() + row.getWidth() * column / count;
            const auto right =
                row.getX() + row.getWidth() * (column + 1) / count;
            controls[static_cast<std::size_t> (column)]
                ->setBounds (
                    juce::Rectangle<int> (
                        left, row.getY(), right - left, row.getHeight())
                        .reduced (s (3), 0));
        }
    };

    const auto centredRow = [&s] (juce::Rectangle<int> content)
    {
        return content.withHeight (s (controlRowHeight))
            .withCentre (content.getCentre());
    };

    const std::array<juce::Component*, 5> delayControls {
        choices[0].get(), continuousControls[0].get(),
        continuousControls[2].get(), choices[3].get(),
        continuousControls[7].get()
    };
    setComponentRow (centredRow (sectionContent[0]), delayControls);
    choices[1]->setBounds (continuousControls[0]->getBounds());

    // TAP shares the TIME MODE cell as a short momentary button underneath it.
    const auto timeModeCell = choices[0]->getBounds();
    choices[0]->setBounds (timeModeCell.withHeight (s (44)));
    tapTempoButton.setBounds (
        timeModeCell.withTop (timeModeCell.getY() + s (50))
            .withHeight (s (26)));

    const std::array<juce::Component*, 2> stereoControls {
        choices[2].get(), continuousControls[5].get()
    };
    setComponentRow (centredRow (sectionContent[1]), stereoControls);

    const std::array<juce::Component*, 3> colorControls {
        choices[4].get(), continuousControls[25].get(),
        continuousControls[26].get()
    };
    setComponentRow (centredRow (sectionContent[2]), colorControls);

    const std::array<juce::Component*, 5> grainControls {
        choices[6].get(),
        continuousControls[17].get(),
        continuousControls[19].get(), continuousControls[23].get(),
        continuousControls[24].get()
    };
    setComponentRow (centredRow (sectionContent[3]), grainControls);

    // FREEZE shares the BUILD / ONCE cell as a short bar underneath it.
    const auto routeCell = choices[6]->getBounds();
    choices[6]->setBounds (routeCell.withHeight (s (44)));
    freezeToggle->setBounds (
        routeCell.withTop (routeCell.getY() + s (50))
            .withHeight (s (26)));

    const std::array<int, 3> bypassQuadrants { 1, 2, 3 };
    for (size_t index = 0;
         index < sectionBypassToggles.size();
         ++index)
    {
        auto headerStrip =
            geometry.quadrants[static_cast<std::size_t> (
                bypassQuadrants[index])]
                .withHeight (s (sectionTitleHeight))
                .reduced (s (14), s (4));
        sectionBypassToggles[index]->setBounds (
            headerStrip.removeFromRight (s (34))
                .withSizeKeepingCentre (s (34), s (28)));
    }
}

void TB_DelayAudioProcessorEditor::updateControlAvailability()
{
    const auto& state = processor.getValueTreeState();
    const auto timeMode = juce::jlimit (
        0, 2,
        juce::roundToInt (
            parameterValue (
                state, TBDelayParameterIDs::timeMode)));
    const auto syncMode = timeMode == 1;
    const auto stereoBypassed =
        parameterValue (
            state, TBDelayParameterIDs::stereoBypass) >= 0.5f;
    const auto colorSpaceBypassed =
        parameterValue (
            state, TBDelayParameterIDs::colorSpaceBypass) >= 0.5f;
    const auto grainTransformBypassed =
        parameterValue (
            state,
            TBDelayParameterIDs::grainTransformBypass) >= 0.5f;

    continuousControls[0]->setDelayTimeMode (timeMode);
    continuousControls[0]->setContextActive (true);
    continuousControls[0]->setVisible (! syncMode);
    choices[1]->setContextActive (true);
    choices[1]->setVisible (syncMode);

    for (const auto index : { 5 })
        continuousControls[static_cast<std::size_t> (index)]
            ->setContextActive (! stereoBypassed);
    choices[2]->setContextActive (! stereoBypassed);

    for (const auto index : { 25, 26 })
        continuousControls[static_cast<std::size_t> (index)]
            ->setContextActive (! colorSpaceBypassed);
    choices[4]->setContextActive (! colorSpaceBypassed);

    for (const auto index
         : { 17, 19, 23, 24 })
        continuousControls[static_cast<std::size_t> (index)]
            ->setContextActive (! grainTransformBypassed);
    choices[6]->setContextActive (! grainTransformBypassed);

    // PITCH and REVERSE only shape grain voices, so they stay editable but read
    // as inaudible until GRAIN MIX opens the grain layer.
    const auto grainAudible =
        parameterValue (state, TBDelayParameterIDs::grain) > 0.0f;
    for (const auto index : { 23, 24 })
        continuousControls[static_cast<std::size_t> (index)]
            ->setDependencySatisfied (grainAudible);
}

void TB_DelayAudioProcessorEditor::resetTapSequence (double tapTimeMs)
{
    lastTapTimeMs = tapTimeMs;
    tapIntervalsMs.fill (0.0);
    tapIntervalCount = 0;
    tapIntervalWriteIndex = 0;
    tapTempoButton.setButtonText ("TAP...");
    tapFeedbackClearAtMs = tapTimeMs + maximumTapIntervalMs;
}

void TB_DelayAudioProcessorEditor::setTapParameter (
    const char* parameterID, float plainValue)
{
    auto* parameter =
        processor.getValueTreeState().getParameter (parameterID);
    if (parameter == nullptr)
        return;

    const auto normalised = parameter->convertTo0to1 (plainValue);
    if (std::abs (parameter->getValue() - normalised) <= 1.0e-7f)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (normalised);
    parameter->endChangeGesture();
}

void TB_DelayAudioProcessorEditor::applyTappedInterval (double intervalMs)
{
    const auto& state = processor.getValueTreeState();
    const auto timeMode = juce::jlimit (
        0, 2,
        juce::roundToInt (
            parameterValue (state, TBDelayParameterIDs::timeMode)));

    if (timeMode != 1)
    {
        setTapParameter (
            TBDelayParameterIDs::time,
            static_cast<float> (
                juce::jlimit (10.0, 8000.0, intervalMs)));
        return;
    }

    const auto bpm = juce::jlimit (20.0, 400.0, processor.getHostBpm());
    const auto currentDivision = juce::jlimit (
        0, static_cast<int> (tapDivisionBeats.size()) - 1,
        juce::roundToInt (
            parameterValue (state, TBDelayParameterIDs::division)));
    auto bestDivision = currentDivision;
    auto bestError = std::numeric_limits<double>::max();

    for (int division = 0;
         division < static_cast<int> (tapDivisionBeats.size());
         ++division)
    {
        const auto candidateMs =
            60000.0 / bpm
            * tapDivisionBeats[static_cast<std::size_t> (division)];
        const auto error = std::abs (std::log (candidateMs / intervalMs));
        if (error + 1.0e-12 < bestError)
        {
            bestError = error;
            bestDivision = division;
        }
        else if (std::abs (error - bestError) <= 1.0e-12
                 && division == currentDivision)
        {
            bestDivision = division;
        }
    }

    setTapParameter (
        TBDelayParameterIDs::division,
        static_cast<float> (bestDivision));
}

void TB_DelayAudioProcessorEditor::handleTapTempo()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (lastTapTimeMs <= 0.0)
    {
        resetTapSequence (now);
        return;
    }

    const auto interval = now - lastTapTimeMs;
    if (interval < minimumTapIntervalMs)
        return;
    if (interval > maximumTapIntervalMs)
    {
        resetTapSequence (now);
        return;
    }

    lastTapTimeMs = now;
    if (tapIntervalCount > 0)
    {
        auto average = 0.0;
        for (int index = 0; index < tapIntervalCount; ++index)
            average += tapIntervalsMs[static_cast<std::size_t> (index)];
        average /= static_cast<double> (tapIntervalCount);

        if (std::abs (interval - average) / average
            > maximumTapDeviation)
        {
            resetTapSequence (now);
            return;
        }
    }

    tapIntervalsMs[static_cast<std::size_t> (tapIntervalWriteIndex)] =
        interval;
    tapIntervalWriteIndex =
        (tapIntervalWriteIndex + 1)
        % static_cast<int> (tapIntervalsMs.size());
    tapIntervalCount = juce::jmin (
        tapIntervalCount + 1,
        static_cast<int> (tapIntervalsMs.size()));

    auto average = 0.0;
    for (int index = 0; index < tapIntervalCount; ++index)
        average += tapIntervalsMs[static_cast<std::size_t> (index)];
    average /= static_cast<double> (tapIntervalCount);
    processor.getUndoManager().beginNewTransaction ("Tap Tempo");
    applyTappedInterval (average);

    const auto syncMode =
        juce::roundToInt (
            parameterValue (
                processor.getValueTreeState(),
                TBDelayParameterIDs::timeMode)) == 1;
    tapTempoButton.setButtonText (
        "TAP "
        + (syncMode
               ? parameterText (
                     processor.getValueTreeState(),
                     TBDelayParameterIDs::division)
               : juce::String (
                     juce::roundToInt (60000.0 / average))));
    tapFeedbackClearAtMs = now + 1500.0;
}

juce::File TB_DelayAudioProcessorEditor::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("TB Delay")
        .getChildFile ("Presets")
        .getChildFile ("User");
}

void TB_DelayAudioProcessorEditor::rebuildPresetMenu (
    const juce::File& preferredFile)
{
    PresetEntry previousEntry;
    const auto hadPreviousEntry = juce::isPositiveAndBelow (
        currentPresetIndex, static_cast<int> (presetEntries.size()));
    if (hadPreviousEntry)
        previousEntry = presetEntries[static_cast<size_t> (currentPresetIndex)];

    presetBox.clear (juce::dontSendNotification);
    presetEntries.clear();
    auto& rootMenu = *presetBox.getRootMenu();
    rootMenu.addSectionHeader ("FACTORY");

    const auto& presets = getTBDelayFactoryPresets();
    for (size_t first = 0; first < presets.size();)
    {
        const auto group = juce::String (presets[first].group);
        juce::PopupMenu folderMenu;
        auto index = first;
        while (index < presets.size()
               && juce::String (presets[index].group) == group)
        {
            presetEntries.push_back (
                { presets[index].name, static_cast<int> (index), {} });
            folderMenu.addItem (
                static_cast<int> (presetEntries.size()),
                presets[index].name);
            ++index;
        }

        rootMenu.addSubMenu (group, folderMenu);
        first = index;
    }

    rootMenu.addSeparator();
    rootMenu.addSectionHeader ("USER");

    const auto directory = getUserPresetDirectory();
    const auto directoryResult = directory.createDirectory();
    juce::Array<juce::File> candidateFiles;
    if (directoryResult.wasOk())
        directory.findChildFiles (
            candidateFiles, juce::File::findFiles, false, "*");
    else
    {
        showStatus (directoryResult.getErrorMessage(), true);
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Preset Folder Failed",
            utf8 ("User 프리셋 폴더를 만들 수 없습니다.\n\n")
                + directoryResult.getErrorMessage());
    }

    juce::Array<juce::File> userFiles;
    for (const auto& file : candidateFiles)
        if (file.getFileExtension().equalsIgnoreCase (".tbdelay"))
            userFiles.add (file);

    juce::File::NaturalFileComparator comparator (false);
    userFiles.sort (comparator);
    juce::PopupMenu userMenu;
    for (const auto& file : userFiles)
    {
        presetEntries.push_back (
            { file.getFileNameWithoutExtension(), -1, file });
        userMenu.addItem (
            static_cast<int> (presetEntries.size()),
            file.getFileNameWithoutExtension());
    }

    if (userFiles.isEmpty())
    {
        constexpr int emptyUserPresetItemId = 100000;
        userMenu.addItem (
            emptyUserPresetItemId,
            utf8 ("No user presets — click Save"),
            false, false);
    }
    rootMenu.addSubMenu ("USER", userMenu);

    auto selectedIndex = -1;
    for (int index = 0; index < static_cast<int> (presetEntries.size());
         ++index)
    {
        const auto& entry = presetEntries[static_cast<size_t> (index)];
        if (preferredFile != juce::File() && entry.file == preferredFile)
        {
            selectedIndex = index;
            break;
        }

        if (preferredFile == juce::File() && hadPreviousEntry
            && ((entry.factoryIndex >= 0
                 && entry.factoryIndex == previousEntry.factoryIndex)
                || (entry.file != juce::File()
                    && entry.file == previousEntry.file)))
        {
            selectedIndex = index;
            break;
        }
    }

    currentPresetIndex = selectedIndex;
    presetBox.setSelectedId (
        selectedIndex >= 0 ? selectedIndex + 1 : 0,
        juce::dontSendNotification);
    const auto hasPresets = ! presetEntries.empty();
    previousPresetButton.setEnabled (hasPresets);
    nextPresetButton.setEnabled (hasPresets);
    lastDisplayedPreset = -99;
}

bool TB_DelayAudioProcessorEditor::loadPresetAt (int presetIndex)
{
    if (! juce::isPositiveAndBelow (
            presetIndex, static_cast<int> (presetEntries.size())))
        return false;

    const auto previousIndex = currentPresetIndex;
    const auto entry = presetEntries[static_cast<size_t> (presetIndex)];
    auto result = juce::Result::ok();
    if (entry.factoryIndex >= 0)
        processor.applyFactoryPreset (entry.factoryIndex);
    else
        result = processor.loadUserPreset (entry.file);

    if (result.failed())
    {
        presetBox.setSelectedId (
            previousIndex >= 0 ? previousIndex + 1 : 0,
            juce::dontSendNotification);
        showStatus (result.getErrorMessage(), true);
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Preset Load Failed",
            result.getErrorMessage());
        return false;
    }

    currentPresetIndex = presetIndex;
    presetBox.setSelectedId (presetIndex + 1, juce::dontSendNotification);
    lastDisplayedPreset = processor.getSelectedFactoryPreset();
    showStatus ("Loaded  /  " + entry.name, false);
    return true;
}

void TB_DelayAudioProcessorEditor::stepPreset (int direction)
{
    if (presetEntries.empty() || direction == 0)
        return;

    const auto presetCount = static_cast<int> (presetEntries.size());
    const auto nextIndex =
        currentPresetIndex < 0
            ? (direction < 0 ? presetCount - 1 : 0)
            : (currentPresetIndex + (direction < 0 ? -1 : 1)
               + presetCount)
                % presetCount;
    loadPresetAt (nextIndex);
}

void TB_DelayAudioProcessorEditor::updatePresetDisplay()
{
    const auto selected = processor.getSelectedFactoryPreset();
    if (selected == lastDisplayedPreset)
        return;

    lastDisplayedPreset = selected;
    if (selected >= 0)
    {
        currentPresetIndex = -1;
        for (int index = 0;
             index < static_cast<int> (presetEntries.size()); ++index)
        {
            if (presetEntries[static_cast<size_t> (index)].factoryIndex
                == selected)
            {
                currentPresetIndex = index;
                break;
            }
        }
        presetBox.setSelectedId (
            currentPresetIndex >= 0 ? currentPresetIndex + 1 : 0,
            juce::dontSendNotification);
    }
    else if (selected == TB_DelayAudioProcessor::userPresetState
             && juce::isPositiveAndBelow (
                 currentPresetIndex,
                 static_cast<int> (presetEntries.size()))
             && presetEntries[static_cast<size_t> (currentPresetIndex)].file
                    != juce::File())
    {
        presetBox.setSelectedId (
            currentPresetIndex + 1, juce::dontSendNotification);
    }
    else
    {
        currentPresetIndex = -1;
        presetBox.setSelectedId (0, juce::dontSendNotification);
        presetBox.setText ("Custom", juce::dontSendNotification);
    }
}

void TB_DelayAudioProcessorEditor::timerCallback()
{
    if (controlAvailabilityDirty.exchange (
            false, std::memory_order_acquire))
        updateControlAvailability();

    updatePresetDisplay();
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    const auto now = juce::Time::getMillisecondCounterHiRes();
    if (statusClearAtMs > 0.0 && now >= statusClearAtMs)
    {
        statusClearAtMs = 0.0;
        statusLabel.setText ({}, juce::dontSendNotification);
    }
    if (tapFeedbackClearAtMs > 0.0 && now >= tapFeedbackClearAtMs)
    {
        tapFeedbackClearAtMs = 0.0;
        tapTempoButton.setButtonText ("TAP");
    }

    const auto delayMapRevision = processor.getDisplayRevision();
    if (delayMapRevision != lastDelayMapRevision)
    {
        lastDelayMapRevision = delayMapRevision;
        delayMapDisplay->repaint();
    }
}

void TB_DelayAudioProcessorEditor::parameterChanged (
    const juce::String& parameterID,
    float)
{
    if (parameterID != TBDelayParameterIDs::timeMode
        && parameterID != TBDelayParameterIDs::stereoBypass
        && parameterID != TBDelayParameterIDs::colorSpaceBypass
        && parameterID
               != TBDelayParameterIDs::grainTransformBypass)
        return;

    if (auto* messageManager =
            juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr
            && messageManager->isThisTheMessageThread())
    {
        updateControlAvailability();
        return;
    }

    controlAvailabilityDirty.store (true, std::memory_order_release);
}

void TB_DelayAudioProcessorEditor::showSaveDialog()
{
    const auto directory = getUserPresetDirectory();
    const auto directoryResult = directory.createDirectory();
    if (directoryResult.failed())
    {
        showStatus (directoryResult.getErrorMessage(), true);
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Preset Folder Failed",
            utf8 ("User 프리셋 폴더를 만들 수 없습니다.\n\n")
                + directoryResult.getErrorMessage());
        return;
    }

    auto initialName = juce::String ("User Preset");
    if (juce::isPositiveAndBelow (
            currentPresetIndex, static_cast<int> (presetEntries.size())))
    {
        const auto& current =
            presetEntries[static_cast<size_t> (currentPresetIndex)];
        initialName =
            current.name + (current.factoryIndex >= 0 ? " User" : "");
    }
    initialName =
        juce::File::createLegalFileName (initialName.trim());

    fileChooser = std::make_unique<juce::FileChooser> (
        utf8 ("Save preset name — stored in the User folder"),
        directory.getChildFile (initialName)
            .withFileExtension (".tbdelay"),
        "*.tbdelay");
    const auto chooserFlags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;
    juce::Component::SafePointer<TB_DelayAudioProcessorEditor> safeThis (
        this);
    fileChooser->launchAsync (
        chooserFlags, [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto chosenFile = chooser.getResult();
        if (chosenFile == juce::File())
            return;

        if (! chosenFile.getFileExtension().equalsIgnoreCase (".tbdelay"))
            chosenFile = chosenFile.withFileExtension (".tbdelay");

        const auto legalName = juce::File::createLegalFileName (
            chosenFile.getFileNameWithoutExtension().trim());
        if (legalName.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Preset Save Failed",
                utf8 ("올바른 프리셋 이름을 입력해 주세요."));
            return;
        }

        const auto userDirectory =
            TB_DelayAudioProcessorEditor::getUserPresetDirectory();
        const auto result = userDirectory.createDirectory();
        if (result.failed())
        {
            safeThis->showStatus (result.getErrorMessage(), true);
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Preset Folder Failed",
                utf8 ("User 프리셋 폴더를 만들 수 없습니다.\n\n")
                    + result.getErrorMessage());
            return;
        }

        const auto presetFile =
            userDirectory.getChildFile (legalName)
                .withFileExtension (".tbdelay");
        if (presetFile.existsAsFile() && presetFile != chosenFile)
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Preset Already Exists",
                utf8 ("같은 이름의 User 프리셋이 있습니다. User 폴더의 해당 "
                      "파일을 선택해 교체를 확인해 주세요."));
            return;
        }

        const auto saveResult =
            safeThis->processor.saveUserPreset (presetFile);
        if (saveResult.failed())
        {
            safeThis->showStatus (
                saveResult.getErrorMessage(), true);
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Preset Save Failed",
                saveResult.getErrorMessage());
            return;
        }

        safeThis->processor.markUserPresetSelected();
        safeThis->rebuildPresetMenu (presetFile);
        safeThis->showStatus (
            "Saved to User  /  " + legalName, false);
    });
}

void TB_DelayAudioProcessorEditor::showStatus (const juce::String& message, bool isError)
{
    statusLabel.setColour (juce::Label::textColourId,
                           isError ? juce::Colour::fromRGB (255, 145, 170) : textSecondary);
    statusLabel.setText (message, juce::dontSendNotification);
    statusClearAtMs = isError
                          ? 0.0
                          : juce::Time::getMillisecondCounterHiRes()
                                + successStatusDurationMs;
}
