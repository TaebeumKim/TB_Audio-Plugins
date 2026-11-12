#include "../Source/PluginEditor.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
int passed = 0, failed = 0;

void check (bool ok, const juce::String& name, double metric = 0.0)
{
    (ok ? ++passed : ++failed);
    std::cout << (ok ? "PASS " : "FAIL ") << name << " metric=" << metric << std::endl;
}

void set (NewProjectAudioProcessor& p, const char* id, float value)
{
    auto* parameter = p.parameters.getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float value (NewProjectAudioProcessor& p, const char* id)
{
    return p.parameters.getRawParameterValue (id)->load();
}

juce::MouseEvent event (juce::Component& component, juce::Point<float> position, bool dragged = false,
                        int modifiers = juce::ModifierKeys::leftButtonModifier)
{
    const auto time = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position, juce::ModifierKeys (modifiers),
             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component, time, position, time, 1, dragged };
}

// Observe the actually painted solid orange marker, independent of the implementation's mapping formula.
juce::Point<float> markerCentre (SpatialPlane& plane)
{
    const auto image = plane.createComponentSnapshot (plane.getLocalBounds());
    const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);
    double xSum = 0.0, ySum = 0.0;
    int count = 0;
    for (int y = 2; y < image.getHeight() - 2; ++y)
        for (int x = 2; x < image.getWidth() - 2; ++x)
        {
            // A solid 5x5 neighbourhood identifies the filled dot, not similarly
            // coloured thin axis glyphs in older skins.
            bool solid = true;
            for (int dy = -2; dy <= 2 && solid; ++dy)
                for (int dx = -2; dx <= 2 && solid; ++dx)
                {
                    const auto colour = pixels.getPixelColour (x + dx, y + dy);
                    solid = colour.getRed() == 237 && colour.getGreen() == 146 && colour.getBlue() == 81;
                }
            if (solid)
            {
                xSum += x + 0.5;
                ySum += y + 0.5;
                ++count;
            }
        }
    if (count == 0)
    {
        auto stream = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("marker-detection-failure.png").createOutputStream();
        if (stream != nullptr) juce::PNGImageFormat().writeImageToStream (image, *stream);
        throw std::runtime_error ("Cannot locate painted object marker; inspect marker-detection-failure.png");
    }
    return { static_cast<float> (xSum / count), static_cast<float> (ySum / count) };
}

struct Gestures : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool start) override { start ? ++begins : ++ends; }
};

void testPlanes()
{
    auto p = std::make_unique<NewProjectAudioProcessor>();
    for (auto view : { SpatialPlane::View::top, SpatialPlane::View::side })
    {
        SpatialPlane plane (p->parameters, view);
        const auto top = view == SpatialPlane::View::top;
        const char* xID = top ? "pan" : "distance";
        const char* yID = top ? "frontBack" : "height";
        for (int size : { 300, 420 })
        {
            plane.setSize (size, size);
            double maxValueDrift = 0.0, maxPixelDrift = 0.0;
            for (float x : { 0.0f, 0.35f, 1.0f })
                for (float y : { -1.0f, 0.0f, 1.0f })
                {
                    set (*p, xID, x);
                    set (*p, yID, y);
                    const auto before = markerCentre (plane);
                    plane.mouseDown (event (plane, before));
                    plane.mouseUp (event (plane, before));
                    maxValueDrift = std::max (maxValueDrift, static_cast<double> (std::max (
                        std::abs (value (*p, xID) - x), std::abs (value (*p, yID) - y))));
                    maxPixelDrift = std::max (maxPixelDrift, static_cast<double> (markerCentre (plane).getDistanceFrom (before)));
                }
            const auto name = juce::String (top ? "top" : "side") + " size=" + juce::String (size);
            check (maxValueDrift < 0.004, name + " clicking painted marker retains parameters", maxValueDrift);
            check (maxPixelDrift < 0.8, name + " marker does not jump on click", maxPixelDrift);
        }

        Gestures xGestures, yGestures;
        auto* xp = p->parameters.getParameter (xID);
        auto* yp = p->parameters.getParameter (yID);
        xp->addListener (&xGestures);
        yp->addListener (&yGestures);
        plane.mouseDown (event (plane, { 180.0f, 180.0f }));
        for (int i = 0; i < 8; ++i)
        {
            plane.mouseDrag (event (plane, { 181.0f + i, 180.0f + i }, true));
            p->parameters.copyState(); // emulate APVTS timer flushes during a real drag
        }
        check (xGestures.begins == 1 && yGestures.begins == 1 && xGestures.ends == 0 && yGestures.ends == 0,
               "plane holds one host gesture throughout drag");
        plane.mouseUp (event (plane, { 188.0f, 187.0f }, true));
        check (xGestures.ends == 1 && yGestures.ends == 1, "plane balances gesture on mouse up");
        xp->removeListener (&xGestures);
        yp->removeListener (&yGestures);

        p->parameters.copyState();
        p->undoManager.clearUndoHistory();
        const auto initialX = value (*p, xID), initialY = value (*p, yID);
        auto click = [&] (juce::Point<float> pos)
        {
            plane.mouseDown (event (plane, pos));
            plane.mouseUp (event (plane, pos));
            p->parameters.copyState();
        };
        click ({ 120.0f, 100.0f });
        const auto firstX = value (*p, xID), firstY = value (*p, yID);
        click ({ 290.0f, 300.0f });
        p->undoManager.undo();
        check (std::abs (value (*p, xID) - firstX) < 0.001f && std::abs (value (*p, yID) - firstY) < 0.001f,
               "undo reverses exactly one plane gesture, both axes");
        p->undoManager.undo();
        check (std::abs (value (*p, xID) - initialX) < 0.001f && std::abs (value (*p, yID) - initialY) < 0.001f,
               "second undo restores original plane position");
        p->undoManager.redo();
        check (std::abs (value (*p, xID) - firstX) < 0.001f && std::abs (value (*p, yID) - firstY) < 0.001f,
               "redo restores both plane axes together");
        const auto prior = value (*p, xID);
        plane.mouseDown (event (plane, { 10.0f, 10.0f }, false, juce::ModifierKeys::rightButtonModifier));
        plane.mouseUp (event (plane, { 10.0f, 10.0f }, false, juce::ModifierKeys::rightButtonModifier));
        check (value (*p, xID) == prior, "right-click does not reposition object");
    }
}

void testEditor (const juce::String& prefix)
{
    auto p = std::make_unique<NewProjectAudioProcessor>();
    std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
    std::vector<juce::Slider*> sliders;
    std::vector<SpatialPlane*> planes;
    juce::ComboBox* selector = nullptr;
    bool frontBackControl = false;
    bool allInside = true, noOverlap = true;
    for (auto* child : editor->getChildren())
    {
        if (! child->isVisible()) continue;
        allInside &= editor->getLocalBounds().contains (child->getBounds());
        if (auto* slider = dynamic_cast<juce::Slider*> (child)) sliders.push_back (slider);
        if (auto* plane = dynamic_cast<SpatialPlane*> (child)) planes.push_back (plane);
        if (auto* combo = dynamic_cast<juce::ComboBox*> (child)) selector = combo;
        if (auto* label = dynamic_cast<juce::Label*> (child))
            frontBackControl |= label->getText().containsIgnoreCase ("front");
    }
    for (auto* child : editor->getChildren())
        for (auto* other : editor->getChildren())
            if (child != other && child->isVisible() && other->isVisible())
                noOverlap &= ! child->getBounds().intersects (other->getBounds());
    check (allInside, "all editor controls inside editor bounds");
    check (noOverlap, "no visible sibling controls overlap");
    check (frontBackControl && sliders.size() == 6, "front/back has a dedicated equal-size knob");
    bool sameSize = ! sliders.empty();
    for (auto* slider : sliders)
        sameSize &= slider->getLocalBounds() == sliders.front()->getLocalBounds();
    check (sameSize, "all rotary controls have identical size");
    check (planes.size() == 2 && planes[0]->getWidth() == planes[0]->getHeight()
           && planes[1]->getLocalBounds() == planes[0]->getLocalBounds(), "two equal square planes retained");
    check (selector != nullptr && std::all_of (sliders.begin(), sliders.end(), [selector] (const auto* slider)
        { return ! selector->getBounds().intersects (slider->getBounds()); }), "distance curve does not obstruct any knob or value");

    const auto out = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto save = [&] (const juce::String& suffix, float scale)
    {
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
        const auto path = out.getChildFile (prefix + suffix + ".png");
        auto stream = path.createOutputStream();
        check (stream != nullptr && stream->setPosition (0) && stream->truncate().wasOk()
               && juce::PNGImageFormat().writeImageToStream (image, *stream), "render snapshot " + path.getFileName());
    };
    save ("-default", 1.0f);
    save ("-150pct", 1.5f);
    set (*p, "pan", -1.0f);
    set (*p, "frontBack", -1.0f);
    set (*p, "height", 1.0f);
    set (*p, "distance", 1.0f);
    save ("-extreme", 1.0f);

    if (sliders.size() == 6)
    {
        const std::array<const char*, 6> ids { "pan", "frontBack", "height", "distance", "room", "outputTrim" };
        for (size_t i = 0; i < ids.size(); ++i)
        {
            auto* parameter = p->parameters.getParameter (ids[i]);
            bool attachmentOK = true, pointerOK = true;
            for (float normalised : { 0.0f, 0.5f, 1.0f })
            {
                const auto expected = parameter->convertFrom0to1 (normalised);
                set (*p, ids[i], expected);
                attachmentOK &= std::abs (sliders[i]->getValue() - expected) < 0.001;
                juce::Image knob (juce::Image::ARGB, 100, 100, true);
                const auto rotary = sliders[i]->getRotaryParameters();
                const auto proportion = static_cast<float> (sliders[i]->valueToProportionOfLength (sliders[i]->getValue()));
                {
                    juce::Graphics g (knob);
                    sliders[i]->getLookAndFeel().drawRotarySlider (g, 0, 0, 100, 100, proportion,
                        rotary.startAngleRadians, rotary.endAngleRadians, *sliders[i]);
                }
                const juce::Image::BitmapData pixels (knob, juce::Image::BitmapData::readOnly);
                juce::Point<float> direction;
                int dots = 0;
                for (int y = 0; y < 100; ++y)
                    for (int x = 0; x < 100; ++x)
                    {
                        const auto c = pixels.getPixelColour (x, y);
                        if (c.getRed() >= 200 && c.getGreen() >= 110 && c.getGreen() <= 170 && c.getBlue() < 110)
                        {
                            direction += juce::Point<float> (x + 0.5f - 50.0f, y + 0.5f - 50.0f);
                            ++dots;
                        }
                    }
                const auto angle = rotary.startAngleRadians + normalised * (rotary.endAngleRadians - rotary.startAngleRadians);
                const juce::Point<float> expectedDirection { std::sin (angle), -std::cos (angle) };
                const auto length = direction.getDistanceFromOrigin();
                const auto cosine = length > 0.0f
                    ? (direction.x * expectedDirection.x + direction.y * expectedDirection.y) / length : 0.0f;
                pointerOK &= dots > 0 && cosine > 0.99f;
                if (dots == 0 || cosine <= 0.99f)
                    std::cout << "MEASURE pointer " << ids[i] << " normalized=" << normalised
                              << " orangePixels=" << dots << " cosine=" << cosine << std::endl;
            }
            check (attachmentOK, juce::String (ids[i]) + " host value equals knob value at min/mid/max");
            check (pointerOK, juce::String (ids[i]) + " painted pointer agrees with value angle");
        }

        set (*p, "pan", 0.0f);
        set (*p, "height", 0.0f);
        p->parameters.copyState();
        p->undoManager.clearUndoHistory();
        {
            juce::Slider::ScopedDragNotification drag (*sliders[0]);
            sliders[0]->setValue (-0.4, juce::sendNotificationSync);
        }
        {
            juce::Slider::ScopedDragNotification drag (*sliders[2]);
            sliders[2]->setValue (0.6, juce::sendNotificationSync);
        }
        p->parameters.copyState();
        p->undoManager.undo();
        check (std::abs (value (*p, "pan") + 0.4f) < 0.001f && std::abs (value (*p, "height")) < 0.001f,
               "rapid successive knob gestures remain separate undo steps");
    }

    if (selector != nullptr)
    {
        selector->setSelectedId (1, juce::sendNotificationSync);
        p->parameters.copyState();
        p->undoManager.clearUndoHistory();
        selector->setSelectedId (2, juce::sendNotificationSync);
        selector->setSelectedId (3, juce::sendNotificationSync);
        p->parameters.copyState();
        p->undoManager.undo();
        check (value (*p, "distanceModel") == 1.0f, "rapid distance curve selections remain separate undo steps");
    }

    juce::TextButton *stereo = nullptr, *headphones = nullptr, *undo = nullptr, *redo = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child))
        {
            if (button->getButtonText() == "STEREO") stereo = button;
            if (button->getButtonText() == "HEADPHONES") headphones = button;
            if (button->getComponentID() == "historyUndo") undo = button;
            if (button->getComponentID() == "historyRedo") redo = button;
        }
    check (stereo && headphones && undo && redo, "mode and history controls present");
    if (stereo && headphones && undo && redo)
    {
        set (*p, "renderMode", 0.0f);
        p->parameters.copyState();
        p->undoManager.clearUndoHistory();
        headphones->onClick();
        stereo->onClick();
        undo->onClick();
        check (value (*p, "renderMode") == 1.0f && headphones->getToggleState(), "mode undo updates audio and active button immediately");
        undo->onClick();
        check (value (*p, "renderMode") == 0.0f && stereo->getToggleState(), "second mode undo restores original mode");
        redo->onClick();
        check (value (*p, "renderMode") == 1.0f && headphones->getToggleState(), "mode redo restores audio and active button");
        p->undoManager.clearUndoHistory();
        headphones->onClick();
        check (! p->undoManager.canUndo(), "clicking active mode creates no undo entry");
    }
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        testPlanes();
        testEditor (argc > 1 ? argv[1] : "editor");
    }
    catch (const std::exception& error) { check (false, error.what()); }
    std::cout << "RESULT " << (failed == 0 ? "PASS" : "FAIL") << " passed=" << passed << " failed=" << failed << std::endl;
    return failed == 0 ? 0 : 1;
}
