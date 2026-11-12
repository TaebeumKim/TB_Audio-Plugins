#include "../Source/GranularShimmerEngine.h"
#include "../Source/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
int testsRun = 0;
int testsFailed = 0;

constexpr std::array<const char*, 17> parameterIds
{{
    "time", "size", "density", "scatter", "pitch", "reverse",
    "feedback", "tone", "width", "mix", "bypass",
    "level_var", "position_rand", "pitch_spray", "freeze",
    "sparkle", "shatter"
}};

struct GoldenFactoryPreset
{
    const char* stableId;
    const char* group;
    const char* name;
    std::array<float, parameterIds.size()> values;
};

constexpr std::array<GoldenFactoryPreset, 12> originalFactoryPresetGolden
{{
    { "init", "START", "Init",
      {{ 240.0f, 160.0f, 9.0f, 32.0f, 12.0f, 18.0f, 42.0f,
         9000.0f, 135.0f, 48.0f, 0.0f, 0.0f, 32.0f, 32.0f * 0.0035f }} },
    { "soft_halo", "START", "Soft Halo",
      {{ 180.0f, 220.0f, 12.0f, 15.0f, 7.0f, 5.0f, 28.0f,
         12000.0f, 115.0f, 35.0f, 0.0f, 0.0f, 15.0f, 15.0f * 0.0035f }} },
    { "open_air", "START", "Open Air",
      {{ 120.0f, 80.0f, 16.0f, 22.0f, 12.0f, 0.0f, 18.0f,
         16000.0f, 150.0f, 30.0f, 0.0f, 0.0f, 22.0f, 22.0f * 0.0035f }} },
    { "glass_halo", "SHIMMER", "Glass Halo",
      {{ 320.0f, 200.0f, 13.0f, 24.0f, 12.0f, 10.0f, 50.0f,
         12000.0f, 145.0f, 50.0f, 0.0f, 0.0f, 24.0f, 24.0f * 0.0035f }} },
    { "guitar_ascension", "SHIMMER", "Guitar Ascension",
      {{ 260.0f, 140.0f, 10.0f, 18.0f, 12.0f, 8.0f, 38.0f,
         10500.0f, 125.0f, 40.0f, 0.0f, 0.0f, 18.0f, 18.0f * 0.0035f }} },
    { "vocal_starlight", "SHIMMER", "Vocal Starlight",
      {{ 180.0f, 120.0f, 16.0f, 14.0f, 7.0f, 6.0f, 26.0f,
         12000.0f, 120.0f, 34.0f, 0.0f, 0.0f, 14.0f, 14.0f * 0.0035f }} },
    { "pad_aurora", "CLOUD", "Pad Aurora",
      {{ 480.0f, 360.0f, 20.0f, 45.0f, 12.0f, 22.0f, 52.0f,
         10000.0f, 160.0f, 54.0f, 0.0f, 0.0f, 45.0f, 45.0f * 0.0035f }} },
    { "slow_nebula", "CLOUD", "Slow Nebula",
      {{ 800.0f, 500.0f, 15.0f, 60.0f, 7.0f, 30.0f, 58.0f,
         6000.0f, 175.0f, 60.0f, 0.0f, 0.0f, 60.0f, 60.0f * 0.0035f }} },
    { "wide_harmonic_fog", "CLOUD", "Wide Harmonic Fog",
      {{ 600.0f, 280.0f, 22.0f, 62.0f, 19.0f, 18.0f, 44.0f,
         11000.0f, 185.0f, 54.0f, 0.0f, 0.0f, 62.0f, 62.0f * 0.0035f }} },
    { "reverse_comet", "REVERSE", "Reverse Comet",
      {{ 420.0f, 220.0f, 9.0f, 38.0f, 12.0f, 100.0f, 40.0f,
         10500.0f, 155.0f, 54.0f, 0.0f, 0.0f, 38.0f, 38.0f * 0.0035f }} },
    { "falling_glass", "REVERSE", "Falling Glass",
      {{ 360.0f, 180.0f, 12.0f, 52.0f, -7.0f, 80.0f, 35.0f,
         7500.0f, 150.0f, 50.0f, 0.0f, 0.0f, 52.0f, 52.0f * 0.0035f }} },
    { "dark_orbit", "REVERSE", "Dark Orbit",
      {{ 900.0f, 420.0f, 10.0f, 58.0f, -12.0f, 60.0f, 54.0f,
         3200.0f, 175.0f, 58.0f, 0.0f, 0.0f, 58.0f, 58.0f * 0.0035f }} }
}};

void check (bool condition, const juce::String& name)
{
    ++testsRun;
    if (! condition)
        ++testsFailed;

    std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", name.toRawUTF8());
}

void fillSine (juce::AudioBuffer<float>& buffer, double sampleRate,
               float frequency, float amplitude)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample,
                              amplitude * std::sin (
                                  juce::MathConstants<float>::twoPi * frequency
                                  * static_cast<float> (sample)
                                  / static_cast<float> (sampleRate)));
}

bool isFiniteAndBounded (const juce::AudioBuffer<float>& buffer, float bound)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            if (! std::isfinite (value) || std::abs (value) > bound)
                return false;
        }

    return true;
}

float maximumDifference (const juce::AudioBuffer<float>& first,
                         const juce::AudioBuffer<float>& second)
{
    auto maximum = 0.0f;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            maximum = juce::jmax (
                maximum,
                std::abs (first.getSample (channel, sample)
                          - second.getSample (channel, sample)));
    return maximum;
}

float combinedRms (const juce::AudioBuffer<float>& buffer, int startSample,
                   int numberOfSamples)
{
    double sum = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = startSample; sample < startSample + numberOfSamples; ++sample)
        {
            const auto value = static_cast<double> (buffer.getSample (channel, sample));
            sum += value * value;
        }

    const auto count = static_cast<double> (buffer.getNumChannels() * numberOfSamples);
    return static_cast<float> (std::sqrt (sum / juce::jmax (1.0, count)));
}

float maximumMagnitude (const juce::AudioBuffer<float>& buffer)
{
    auto maximum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        maximum = juce::jmax (maximum, buffer.getMagnitude (
            channel, 0, buffer.getNumSamples()));
    return maximum;
}

float stereoSideRms (const juce::AudioBuffer<float>& buffer, int startSample,
                     int numberOfSamples)
{
    if (buffer.getNumChannels() < 2)
        return 0.0f;

    double sum = 0.0;
    for (int sample = startSample; sample < startSample + numberOfSamples; ++sample)
    {
        const auto side = 0.5 * static_cast<double> (
            buffer.getSample (0, sample) - buffer.getSample (1, sample));
        sum += side * side;
    }
    return static_cast<float> (std::sqrt (
        sum / static_cast<double> (juce::jmax (1, numberOfSamples))));
}

float maximumStereoDifference (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return 0.0f;

    auto maximum = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        maximum = juce::jmax (
            maximum,
            std::abs (buffer.getSample (0, sample) - buffer.getSample (1, sample)));
    return maximum;
}

juce::AudioBuffer<float> renderWithPartitions (
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    const GranularShimmerEngine::Parameters& parameters,
    const std::vector<int>& partitions)
{
    juce::AudioBuffer<float> output (input.getNumChannels(), input.getNumSamples());
    GranularShimmerEngine engine;
    engine.prepare (sampleRate,
                    *std::max_element (partitions.begin(), partitions.end()),
                    input.getNumChannels());

    int position = 0;
    size_t partitionIndex = 0;
    while (position < input.getNumSamples())
    {
        const auto blockSize = juce::jmin (
            partitions[partitionIndex++ % partitions.size()],
            input.getNumSamples() - position);
        juce::AudioBuffer<float> block (input.getNumChannels(), blockSize);

        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            block.copyFrom (channel, 0, input, channel, position, blockSize);

        engine.process (block, parameters, false);

        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            output.copyFrom (channel, position, block, channel, 0, blockSize);

        position += blockSize;
    }

    return output;
}

double spectralPower (const float* samples, int sampleCount,
                      double sampleRate, double frequency)
{
    double real = 0.0;
    double imaginary = 0.0;
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto phase = juce::MathConstants<double>::twoPi * frequency
                         * static_cast<double> (sample) / sampleRate;
        const auto window = 0.5 - 0.5 * std::cos (
            juce::MathConstants<double>::twoPi * static_cast<double> (sample)
            / static_cast<double> (sampleCount - 1));
        const auto value = static_cast<double> (samples[sample]) * window;
        real += value * std::cos (phase);
        imaginary -= value * std::sin (phase);
    }

    return real * real + imaginary * imaginary;
}

double estimateDominantFrequency (const float* samples, int sampleCount,
                                  double sampleRate, int minimumHz, int maximumHz)
{
    auto bestFrequency = static_cast<double> (minimumHz);
    auto bestPower = -1.0;
    for (int frequency = minimumHz; frequency <= maximumHz; ++frequency)
    {
        const auto power = spectralPower (samples, sampleCount, sampleRate,
                                          static_cast<double> (frequency));
        if (power > bestPower)
        {
            bestPower = power;
            bestFrequency = static_cast<double> (frequency);
        }
    }
    return bestFrequency;
}

bool setParameter (TBShimmerAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.getValueTreeState().getParameter (id);
    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    return true;
}

float readParameter (TBShimmerAudioProcessor& processor, const char* id)
{
    if (auto* value = processor.getValueTreeState().getRawParameterValue (id))
        return value->load();
    return 0.0f;
}

std::array<float, parameterIds.size()> readAllParameters (
    TBShimmerAudioProcessor& processor)
{
    std::array<float, parameterIds.size()> values {};
    for (size_t index = 0; index < parameterIds.size(); ++index)
        values[index] = readParameter (processor, parameterIds[index]);
    return values;
}

std::array<float, parameterIds.size()> factoryPresetValues (
    const TBShimmerAudioProcessor::FactoryPreset& preset)
{
    return {{
        preset.time, preset.size, preset.density, preset.scatter,
        preset.pitch, preset.reverse, preset.feedback, preset.tone,
        preset.width, preset.mix, preset.bypass, preset.levelVar,
        preset.positionRand, preset.pitchSpray, preset.freeze,
        preset.sparkle, preset.shatter
    }};
}

bool approximatelyEqual (const std::array<float, parameterIds.size()>& first,
                         const std::array<float, parameterIds.size()>& second)
{
    for (size_t index = 0; index < first.size(); ++index)
        if (std::abs (first[index] - second[index]) > 1.0e-3f)
            return false;
    return true;
}

juce::File projectRootFromSource()
{
    const auto sourcePath = juce::String (__FILE__);
    auto sourceFile = juce::File::isAbsolutePath (sourcePath)
        ? juce::File (sourcePath)
        : juce::File::getCurrentWorkingDirectory().getChildFile (sourcePath);
    auto candidate = sourceFile.getParentDirectory().getParentDirectory();
    if (candidate.getChildFile ("TB_Shimmer.jucer").existsAsFile())
        return candidate;

    candidate = juce::File::getCurrentWorkingDirectory();
    for (int level = 0; level < 8; ++level)
    {
        if (candidate.getChildFile ("TB_Shimmer.jucer").existsAsFile())
            return candidate;
        candidate = candidate.getParentDirectory();
    }

    return sourceFile.getParentDirectory().getParentDirectory();
}

bool imageIsNontrivial (const juce::Image& image)
{
    auto minimumBrightness = 1.0f;
    auto maximumBrightness = 0.0f;
    int visibleSamples = 0;
    for (int y = 0; y < image.getHeight(); y += 3)
        for (int x = 0; x < image.getWidth(); x += 3)
        {
            const auto colour = image.getPixelAt (x, y);
            if (colour.getAlpha() > 0)
                ++visibleSamples;
            minimumBrightness = juce::jmin (minimumBrightness, colour.getBrightness());
            maximumBrightness = juce::jmax (maximumBrightness, colour.getBrightness());
        }

    return visibleSamples > 1000 && maximumBrightness - minimumBrightness > 0.15f;
}

bool writeUiSnapshot (const juce::File& output)
{
    TBShimmerAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr || editor->getWidth() != 760 || editor->getHeight() != 430)
        return false;

    constexpr std::array<const char*, 8> firstRow
    {{
        "timeSlider", "sizeSlider", "densitySlider", "scatterSlider",
        "levelVariationSlider", "positionRandomSlider", "pitchSpraySlider",
        "reverseSlider"
    }};
    constexpr std::array<const char*, 7> secondRow
    {{
        "pitchSlider", "feedbackSlider", "toneSlider",
        "sparkleSlider", "shatterSlider", "widthSlider", "mixSlider"
    }};

    const auto rowIsOrderedAndVisible = [&editor] (const auto& ids,
                                                    int firstFocusOrder)
    {
        auto previousRight = std::numeric_limits<int>::min();
        auto rowCentreY = std::numeric_limits<int>::min();
        auto index = 0;
        for (const auto* id : ids)
        {
            const auto* component = editor->findChildWithID (id);
            if (component == nullptr || ! component->isVisible()
                || component->getWidth() <= 0 || component->getHeight() <= 0
                || component->getX() < previousRight
                || component->getExplicitFocusOrder() != firstFocusOrder + index
                || (rowCentreY != std::numeric_limits<int>::min()
                    && component->getBounds().getCentreY() != rowCentreY))
                return false;
            previousRight = component->getRight();
            rowCentreY = component->getBounds().getCentreY();
            ++index;
        }
        return true;
    };

    const auto* firstControl = editor->findChildWithID (firstRow.front());
    const auto* secondControl = editor->findChildWithID (secondRow.front());
    const auto* lastUpperControl = editor->findChildWithID (firstRow.back());
    const auto* freezeControl = editor->findChildWithID ("freezeSlider");
    const auto* mixControl = editor->findChildWithID (secondRow.back());
    if (lastUpperControl == nullptr || freezeControl == nullptr || mixControl == nullptr
        || ! freezeControl->isVisible()
        || freezeControl->getX() < mixControl->getRight()
        || freezeControl->getBounds().getCentreY() != mixControl->getBounds().getCentreY()
        || lastUpperControl->getRight() > 746 || freezeControl->getRight() > 746)
        return false;

    if (editor->findChildWithID ("controlPageButton") != nullptr
        || ! rowIsOrderedAndVisible (firstRow, 1)
        || ! rowIsOrderedAndVisible (secondRow, 9)
        || firstControl == nullptr || secondControl == nullptr
        || firstControl->getBounds().getCentreY()
              >= secondControl->getBounds().getCentreY())
        return false;

    const auto image = editor->createComponentSnapshot (
        editor->getLocalBounds(), true, 1.0f);
    if (! image.isValid() || ! imageIsNontrivial (image))
        return false;

    juce::MemoryOutputStream encoded;
    const auto encodedOk = juce::PNGImageFormat().writeImageToStream (image, encoded);
    return output.getParentDirectory().createDirectory().wasOk()
        && encodedOk
        && output.replaceWithData (encoded.getData(), encoded.getDataSize());
}

bool writeWav (const juce::File& output, const juce::AudioBuffer<float>& rendered,
               double sampleRate)
{
    if (output.getParentDirectory().createDirectory().failed())
        return false;
    output.deleteFile();

    std::unique_ptr<juce::OutputStream> stream = output.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriter::Options {}
        .withSampleRate (sampleRate)
        .withChannelLayout (juce::AudioChannelSet::stereo())
        .withBitsPerSample (24);
    auto writer = wav.createWriterFor (stream, options);
    return writer != nullptr
        && writer->writeFromAudioSampleBuffer (
            rendered, 0, rendered.getNumSamples());
}

// Plucked A-major arpeggio with six harmonics per note, so the shards have
// real attacks and overtones to break apart.
bool writeSparkleDemo (const juce::File& output, float sparklePercent,
                       float shatterPercent)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto durationSeconds = 12.0;
    constexpr std::array<double, 8> notes
    {{ 220.0, 277.18, 329.63, 440.0, 329.63, 277.18, 220.0, 440.0 }};
    const auto sampleCount = static_cast<int> (sampleRate * durationSeconds);
    juce::AudioBuffer<float> input (2, sampleCount);
    input.clear();

    for (size_t note = 0; note < notes.size(); ++note)
    {
        const auto start = static_cast<int> (sampleRate * (0.05 + 0.6 * static_cast<double> (note)));
        const auto length = static_cast<int> (sampleRate * 1.2);
        for (int sample = 0; sample < length && start + sample < sampleCount; ++sample)
        {
            const auto time = static_cast<double> (sample) / sampleRate;
            auto value = 0.0;
            for (int harmonic = 1; harmonic <= 6; ++harmonic)
                value += std::exp (-time * (3.0 + 1.5 * harmonic))
                       * std::sin (juce::MathConstants<double>::twoPi
                                   * notes[note] * harmonic * time)
                       / static_cast<double> (harmonic);
            value *= 0.22 * juce::jmin (1.0, time / 0.0015);
            input.addSample (0, start + sample, static_cast<float> (value));
            input.addSample (1, start + sample, static_cast<float> (value));
        }
    }

    GranularShimmerEngine::Parameters parameters;
    parameters.timeMs = 375.0f;
    parameters.sizeMs = 90.0f;
    parameters.density = 14.0f;
    parameters.scatterPercent = 45.0f;
    parameters.pitchSemitones = 12.0f;
    parameters.reversePercent = 10.0f;
    parameters.feedbackPercent = 48.0f;
    parameters.toneHz = 15000.0f;
    parameters.widthPercent = 170.0f;
    parameters.mixPercent = 50.0f;
    parameters.sparklePercent = sparklePercent;
    parameters.shatterPercent = shatterPercent;

    const auto rendered = renderWithPartitions (
        input, sampleRate, parameters, { 127, 256, 511 });
    return isFiniteAndBounded (rendered, 1.0f)
        && rendered.getRMSLevel (0, 0, rendered.getNumSamples()) > 0.005f
        && writeWav (output, rendered, sampleRate);
}

bool writeSparkleUiSnapshot (const juce::File& output)
{
    TBShimmerAudioProcessor processor;
    auto starShower = -1;
    for (int index = 0; index < processor.getFactoryPresetCount(); ++index)
        if (juce::String (processor.getFactoryPreset (index).stableId) == "star_shower")
            starShower = index;
    if (starShower < 0 || ! processor.applyFactoryPreset (starShower))
        return false;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return false;

    const auto image = editor->createComponentSnapshot (
        editor->getLocalBounds(), true, 1.0f);
    if (! image.isValid() || ! imageIsNontrivial (image))
        return false;

    juce::MemoryOutputStream encoded;
    const auto encodedOk = juce::PNGImageFormat().writeImageToStream (image, encoded);
    return output.getParentDirectory().createDirectory().wasOk()
        && encodedOk
        && output.replaceWithData (encoded.getData(), encoded.getDataSize());
}

bool writeDefaultDemo (const juce::File& output)
{
    constexpr auto sampleRate = 48000.0;
    constexpr auto durationSeconds = 12.0;
    const auto sampleCount = static_cast<int> (sampleRate * durationSeconds);
    juce::AudioBuffer<float> input (2, sampleCount);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        const auto time = static_cast<double> (sample) / sampleRate;
        const auto attack = juce::jlimit (0.0, 1.0, time / 0.03);
        const auto sourceRelease = juce::jlimit (0.0, 1.0, (3.7 - time) / 0.45);
        const auto envelope = static_cast<float> (attack * sourceRelease);
        const auto chord = static_cast<float> (0.12f * envelope * (
            std::sin (juce::MathConstants<double>::twoPi * 220.0 * time)
            + std::sin (juce::MathConstants<double>::twoPi * 277.18 * time)
            + std::sin (juce::MathConstants<double>::twoPi * 329.63 * time)));
        input.setSample (0, sample, chord);
        input.setSample (1, sample, chord);
    }

    const auto rendered = renderWithPartitions (
        input, sampleRate, {}, { 127, 256, 511 });
    if (! isFiniteAndBounded (rendered, 8.0001f)
        || rendered.getRMSLevel (0, 0, rendered.getNumSamples()) < 0.005f)
        return false;

    return writeWav (output, rendered, sampleRate);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    std::printf ("TB_Shimmer deterministic test harness\n"
                 "=====================================\n");

    {
        std::printf ("\n[DSP silence]\n");
        auto silenceOk = true;
        for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            juce::AudioBuffer<float> silence (2, static_cast<int> (sampleRate));
            silence.clear();
            const auto result = renderWithPartitions (
                silence, sampleRate, {}, { 1, 17, 64, 257, 1024 });
            silenceOk = silenceOk && isFiniteAndBounded (result, 1.0e-7f);
        }
        check (silenceOk, "Silence remains silent and finite at common sample rates");
    }

    {
        std::printf ("\n[DSP determinism]\n");
        constexpr auto sampleRate = 48000.0;
        juce::AudioBuffer<float> input (2, static_cast<int> (sampleRate * 3.0));
        fillSine (input, sampleRate, 233.0f, 0.35f);
        input.addSample (0, 127, 0.8f);
        input.addSample (1, 511, -0.6f);

        GranularShimmerEngine::Parameters parameters;
        parameters.scatterPercent = 67.0f;
        parameters.levelVariationDb = 12.0f;
        parameters.positionRandomPercent = 100.0f;
        parameters.pitchSpraySemitones = 12.0f;
        parameters.reversePercent = 43.0f;
        parameters.feedbackPercent = 61.0f;

        const auto fixed = renderWithPartitions (
            input, sampleRate, parameters, { 512 });
        const auto arbitrary = renderWithPartitions (
            input, sampleRate, parameters, { 1, 31, 127, 509, 64, 3, 256 });
        check (maximumDifference (fixed, arbitrary) <= 1.0e-6f,
               "Output is invariant to arbitrary host block partitioning");

        GranularShimmerEngine resettable;
        resettable.prepare (sampleRate, input.getNumSamples(), input.getNumChannels());
        juce::AudioBuffer<float> beforeReset;
        beforeReset.makeCopyOf (input);
        resettable.process (beforeReset, parameters, false);
        resettable.reset();
        juce::AudioBuffer<float> afterReset;
        afterReset.makeCopyOf (input);
        resettable.process (afterReset, parameters, false);
        check (maximumDifference (beforeReset, afterReset) <= 1.0e-6f,
               "Reset restores both grain random streams and all-random output exactly");
    }

    {
        std::printf ("\n[Independent grain randomisation]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto sampleCount = static_cast<int> (sampleRate * 2.5);
        juce::AudioBuffer<float> input (2, sampleCount);
        fillSine (input, sampleRate, 263.0f, 0.3f);
        input.addSample (0, 97, 0.7f);
        input.addSample (1, 211, -0.5f);

        GranularShimmerEngine::Parameters base;
        base.timeMs = 140.0f;
        base.sizeMs = 120.0f;
        base.density = 14.0f;
        base.scatterPercent = 0.0f;
        base.levelVariationDb = 0.0f;
        base.positionRandomPercent = 0.0f;
        base.pitchSpraySemitones = 0.0f;
        base.pitchSemitones = 0.0f;
        base.reversePercent = 0.0f;
        base.feedbackPercent = 0.0f;
        base.toneHz = 18000.0f;
        base.widthPercent = 100.0f;
        base.mixPercent = 100.0f;

        const auto fixed = renderWithPartitions (
            input, sampleRate, base, { 37, 128, 509 });

        auto varied = base;
        varied.levelVariationDb = 12.0f;
        const auto levelVaried = renderWithPartitions (
            input, sampleRate, varied, { 37, 128, 509 });

        varied = base;
        varied.positionRandomPercent = 100.0f;
        const auto positionVaried = renderWithPartitions (
            input, sampleRate, varied, { 37, 128, 509 });

        varied = base;
        varied.pitchSpraySemitones = 12.0f;
        const auto pitchVaried = renderWithPartitions (
            input, sampleRate, varied, { 37, 128, 509 });

        check (maximumDifference (fixed, levelVaried) > 1.0e-5f
                   && isFiniteAndBounded (levelVaried, 8.0001f),
               "LEVEL VAR independently changes per-grain gain and remains bounded");
        check (maximumDifference (fixed, positionVaried) > 1.0e-5f
                   && isFiniteAndBounded (positionVaried, 8.0001f),
               "POSITION RAND independently changes source offsets and remains bounded");
        check (maximumDifference (fixed, pitchVaried) > 1.0e-5f
                   && isFiniteAndBounded (pitchVaried, 8.0001f),
               "PITCH SPRAY independently changes continuous grain pitch and remains bounded");

        auto overlapSafe = true;
        auto minimumRms = std::numeric_limits<float>::max();
        auto maximumRms = 0.0f;
        for (const auto size : { 40.0f, 160.0f, 500.0f })
            for (const auto density : { 2.0f, 24.0f })
            {
                auto grid = base;
                grid.sizeMs = size;
                grid.density = density;
                const auto rendered = renderWithPartitions (
                    input, sampleRate, grid, { 97, 256, 511 });
                const auto rms = combinedRms (
                    rendered, sampleCount - static_cast<int> (sampleRate * 0.5),
                    static_cast<int> (sampleRate * 0.5));
                minimumRms = juce::jmin (minimumRms, rms);
                maximumRms = juce::jmax (maximumRms, rms);
                overlapSafe = overlapSafe
                           && isFiniteAndBounded (rendered, 8.0001f)
                           && maximumMagnitude (rendered) <= 8.0001f;
            }

        std::printf ("  size/density RMS range: %.6f .. %.6f\n",
                     minimumRms, maximumRms);
        check (overlapSafe && minimumRms > 1.0e-3f
                   && maximumRms / minimumRms < 8.0f,
               "Hybrid OLA normalisation stays finite without severe density/size ducking");
    }

    {
        std::printf ("\n[Dry contract]\n");
        constexpr auto sampleRate = 48000.0;
        juce::AudioBuffer<float> input (2, 4096);
        fillSine (input, sampleRate, 431.0f, 0.7f);
        auto parameters = GranularShimmerEngine::Parameters {};
        parameters.mixPercent = 0.0f;
        const auto output = renderWithPartitions (
            input, sampleRate, parameters, { 1, 127, 512 });
        check (maximumDifference (input, output) == 0.0f,
               "Mix 0% is sample-identical to dry");
    }

    {
        std::printf ("\n[Pitch contract]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto durationSamples = static_cast<int> (sampleRate * 4.0);
        juce::AudioBuffer<float> input (2, durationSamples);
        fillSine (input, sampleRate, 220.0f, 0.4f);

        GranularShimmerEngine::Parameters parameters;
        parameters.timeMs = 220.0f;
        parameters.sizeMs = 180.0f;
        parameters.density = 24.0f;
        parameters.scatterPercent = 0.0f;
        parameters.pitchSemitones = 12.0f;
        parameters.reversePercent = 0.0f;
        parameters.feedbackPercent = 0.0f;
        parameters.toneHz = 18000.0f;
        parameters.widthPercent = 100.0f;
        parameters.mixPercent = 100.0f;

        const auto output = renderWithPartitions (
            input, sampleRate, parameters, { 97, 256, 511 });
        const auto analysisSamples = static_cast<int> (sampleRate);
        const auto* analysisStart = output.getReadPointer (
            0, output.getNumSamples() - analysisSamples);
        const auto peak = estimateDominantFrequency (
            analysisStart, analysisSamples, sampleRate, 330, 550);
        const auto sourcePower = spectralPower (
            analysisStart, analysisSamples, sampleRate, 220.0);
        const auto globalPeak = estimateDominantFrequency (
            analysisStart, analysisSamples, sampleRate, 100, 600);
        const auto upperPeakPower = spectralPower (
            analysisStart, analysisSamples, sampleRate, peak);
        std::printf ("  measured +12 st upper peak: %.1f Hz; global peak: %.1f Hz; "
                     "upper/source power ratio: %.3f\n",
                     peak, globalPeak,
                     upperPeakPower / juce::jmax (1.0e-18, sourcePower));
        check (std::abs (peak - 440.0) <= 20.0
                   && std::abs (globalPeak - 440.0) <= 20.0
                   && upperPeakPower > sourcePower * 1.5,
               "+12 st moves a 220 Hz source to a tolerant octave-up peak");

        auto fractional = parameters;
        fractional.pitchSemitones = 7.5f;
        const auto fractionalOutput = renderWithPartitions (
            input, sampleRate, fractional, { 97, 256, 511 });
        const auto* fractionalStart = fractionalOutput.getReadPointer (
            0, fractionalOutput.getNumSamples() - analysisSamples);
        const auto expected = 220.0 * std::pow (2.0, 7.5 / 12.0);
        const auto fractionalPeak = estimateDominantFrequency (
            fractionalStart, analysisSamples, sampleRate, 250, 450);
        std::printf ("  measured +7.5 st peak: %.1f Hz; expected %.1f Hz\n",
                     fractionalPeak, expected);
        check (std::abs (fractionalPeak - expected) <= expected * 0.03,
               "Fractional semitone Pitch shifts a 220 Hz source to a cent-accurate peak");
    }

    {
        std::printf ("\n[Feedback safety]\n");
        GranularShimmerEngine::Parameters parameters;
        parameters.timeMs = 60.0f;
        parameters.sizeMs = 500.0f;
        parameters.density = 24.0f;
        parameters.scatterPercent = 100.0f;
        parameters.levelVariationDb = 12.0f;
        parameters.positionRandomPercent = 100.0f;
        parameters.pitchSpraySemitones = 12.0f;
        parameters.pitchSemitones = 24.0f;
        parameters.reversePercent = 100.0f;
        parameters.feedbackPercent = 88.0f;
        parameters.toneHz = 18000.0f;
        parameters.widthPercent = 200.0f;
        parameters.mixPercent = 100.0f;
        parameters.sparklePercent = 100.0f;
        parameters.shatterPercent = 100.0f;

        auto safetyOk = true;
        for (const auto sampleRate : { 48000.0, 192000.0 })
        {
            const auto duration = sampleRate > 100000.0 ? 2.0 : 6.0;
            juce::AudioBuffer<float> input (
                2, static_cast<int> (sampleRate * duration));
            input.clear();
            input.setSample (0, 0, 4.0f);
            input.setSample (1, 0, -4.0f);
            input.setSample (0, 1, std::numeric_limits<float>::quiet_NaN());
            input.setSample (1, 1, std::numeric_limits<float>::infinity());

            const auto output = renderWithPartitions (
                input, sampleRate, parameters,
                sampleRate > 100000.0 ? std::vector<int> { 32 }
                                      : std::vector<int> { 1, 32, 128, 1024 });
            safetyOk = safetyOk && isFiniteAndBounded (output, 8.0001f);
        }

        check (safetyOk,
               "Maximum feedback/all-random grain and sparkle extremes stay safe at 48/192 kHz");
    }

    {
        std::printf ("\n[Dedicated early diffusion]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto sampleCount = static_cast<int> (sampleRate * 0.30);
        juce::AudioBuffer<float> input (2, sampleCount);
        input.clear();
        const auto burstStart = static_cast<int> (sampleRate * 0.005);
        const auto burstLength = static_cast<int> (sampleRate * 0.002);
        for (int sample = 0; sample < burstLength; ++sample)
        {
            const auto envelope = std::sin (
                juce::MathConstants<float>::pi * static_cast<float> (sample)
                / static_cast<float> (burstLength - 1));
            const auto value = 0.45f * envelope * std::sin (
                juce::MathConstants<float>::twoPi * 3000.0f
                * static_cast<float> (sample) / static_cast<float> (sampleRate));
            input.setSample (0, burstStart + sample, value);
            input.setSample (1, burstStart + sample, value);
        }

        GranularShimmerEngine::Parameters parameters;
        parameters.timeMs = 60.0f;
        parameters.sizeMs = 500.0f;
        parameters.density = 2.0f;
        parameters.scatterPercent = 0.0f;
        parameters.levelVariationDb = 0.0f;
        parameters.positionRandomPercent = 0.0f;
        parameters.pitchSpraySemitones = 0.0f;
        parameters.pitchSemitones = 0.0f;
        parameters.reversePercent = 0.0f;
        parameters.feedbackPercent = 0.0f;
        parameters.toneHz = 18000.0f;
        parameters.widthPercent = 160.0f;
        parameters.mixPercent = 100.0f;

        const auto fixed = renderWithPartitions (
            input, sampleRate, parameters, { 512 });
        const auto output = renderWithPartitions (
            input, sampleRate, parameters, { 1, 31, 127, 509, 64 });
        const auto directStart = static_cast<int> (sampleRate * 0.064);
        const auto directLength = static_cast<int> (sampleRate * 0.004);
        const auto earlyStart = static_cast<int> (sampleRate * 0.069);
        const auto earlyLength = static_cast<int> (sampleRate * 0.056);
        const auto directRms = combinedRms (output, directStart, directLength);
        const auto earlyRms = combinedRms (output, earlyStart, earlyLength);
        const auto earlySide = stereoSideRms (output, earlyStart, earlyLength);
        const auto binLength = static_cast<int> (sampleRate * 0.001);
        const auto binThreshold = juce::jmax (1.0e-7f, directRms * 0.005f);
        auto populatedBins = 0;
        for (auto binStart = earlyStart;
             binStart + binLength <= earlyStart + earlyLength;
             binStart += binLength)
            if (combinedRms (output, binStart, binLength) > binThreshold)
                ++populatedBins;

        juce::AudioBuffer<float> monoInput (1, sampleCount);
        monoInput.copyFrom (0, 0, input, 0, 0, sampleCount);
        const auto monoOutput = renderWithPartitions (
            monoInput, sampleRate, parameters, { 97, 256, 511 });
        const auto monoEarlyRms = combinedRms (
            monoOutput, earlyStart, earlyLength);

        std::printf ("  direct RMS: %.8f; early RMS: %.8f; side RMS: %.8f; "
                     "mono early RMS: %.8f; populated 1 ms bins: %d\n",
                     directRms, earlyRms, earlySide, monoEarlyRms, populatedBins);
        check (isFiniteAndBounded (output, 8.0001f)
                   && maximumMagnitude (output) < 2.0f
                   && maximumDifference (fixed, output) <= 1.0e-6f
                   && directRms > 1.0e-4f
                   && earlyRms > juce::jmax (1.0e-6f, directRms * 0.01f)
                   && earlySide > juce::jmax (1.0e-7f, earlyRms * 0.01f)
                   && isFiniteAndBounded (monoOutput, 8.0001f)
                   && monoEarlyRms > 1.0e-6f
                   && populatedBins >= 8,
               "Four-stage all-pass diffusion fills the pre-FDN window with stereo reflections");
    }

    {
        std::printf ("\n[Diffuse swarm tail]\n");
        constexpr auto sampleRate = 48000.0;
        juce::AudioBuffer<float> onePassInput (
            2, static_cast<int> (sampleRate * 1.5));
        onePassInput.clear();
        for (int sample = 0; sample < static_cast<int> (sampleRate * 0.48); ++sample)
        {
            const auto value = 0.2f * std::sin (
                juce::MathConstants<float>::twoPi * 220.0f
                * static_cast<float> (sample) / static_cast<float> (sampleRate));
            onePassInput.setSample (0, sample, value);
            onePassInput.setSample (1, sample, value);
        }

        GranularShimmerEngine::Parameters onePassParameters;
        onePassParameters.timeMs = 60.0f;
        onePassParameters.sizeMs = 40.0f;
        onePassParameters.density = 2.0f;
        onePassParameters.scatterPercent = 0.0f;
        onePassParameters.pitchSemitones = 0.0f;
        onePassParameters.reversePercent = 0.0f;
        onePassParameters.feedbackPercent = 0.0f;
        onePassParameters.toneHz = 18000.0f;
        onePassParameters.widthPercent = 160.0f;
        onePassParameters.mixPercent = 100.0f;
        const auto onePass = renderWithPartitions (
            onePassInput, sampleRate, onePassParameters, { 1, 31, 127, 509 });
        const auto onePassTailRms = combinedRms (
            onePass, static_cast<int> (sampleRate * 0.548),
            static_cast<int> (sampleRate * 0.024));
        const auto onePassSideRms = stereoSideRms (
            onePass, static_cast<int> (sampleRate * 0.548),
            static_cast<int> (sampleRate * 0.024));
        const auto earlyDiffusionDecayRms = combinedRms (
            onePass, static_cast<int> (sampleRate * 0.70),
            static_cast<int> (sampleRate * 0.20));
        const auto settledOnePassRms = combinedRms (
            onePass, static_cast<int> (sampleRate * 1.20),
            static_cast<int> (sampleRate * 0.20));
        std::printf ("  isolated one-pass tail RMS: %.8f; side RMS: %.8f; "
                     "early decay RMS: %.10f; settled RMS: %.10f\n",
                     onePassTailRms, onePassSideRms,
                     earlyDiffusionDecayRms, settledOnePassRms);
        check (isFiniteAndBounded (onePass, 8.0001f)
                   && onePassTailRms > 1.0e-3f
                   && onePassSideRms > 1.0e-4f
                   && earlyDiffusionDecayRms > 1.0e-5f
                   && earlyDiffusionDecayRms < 5.0e-4f
                   && settledOnePassRms < 1.0e-7f,
               "Feedback 0 produces a dense stereo early diffusion that settles safely");

        constexpr auto durationSeconds = 10.0;
        const auto sampleCount = static_cast<int> (sampleRate * durationSeconds);
        juce::AudioBuffer<float> burst (2, sampleCount);
        burst.clear();
        const auto burstSamples = static_cast<int> (sampleRate * 0.35);
        for (int sample = 0; sample < burstSamples; ++sample)
        {
            const auto time = static_cast<double> (sample) / sampleRate;
            const auto envelope = std::sin (
                juce::MathConstants<double>::pi * time / 0.35);
            const auto value = static_cast<float> (0.18 * envelope * (
                std::sin (juce::MathConstants<double>::twoPi * 196.0 * time)
                + std::sin (juce::MathConstants<double>::twoPi * 293.66 * time)));
            burst.setSample (0, sample, value);
            burst.setSample (1, sample, value);
        }

        GranularShimmerEngine::Parameters parameters;
        parameters.timeMs = 60.0f;
        parameters.sizeMs = 420.0f;
        parameters.density = 24.0f;
        parameters.scatterPercent = 60.0f;
        parameters.pitchSemitones = 0.0f;
        parameters.reversePercent = 0.0f;
        parameters.feedbackPercent = 36.0f;
        parameters.toneHz = 8000.0f;
        parameters.widthPercent = 160.0f;
        parameters.mixPercent = 100.0f;

        const auto wide = renderWithPartitions (
            burst, sampleRate, parameters, { 31, 127, 509, 64 });
        const auto bloomRms = combinedRms (
            wide, static_cast<int> (sampleRate * 1.5),
            static_cast<int> (sampleRate * 1.5));
        const auto lateRms = combinedRms (
            wide, static_cast<int> (sampleRate * 3.0),
            static_cast<int> (sampleRate * 2.0));
        const auto finalRms = combinedRms (
            wide, static_cast<int> (sampleRate * 8.0),
            static_cast<int> (sampleRate * 2.0));
        std::printf ("  bloom RMS: %.8f; late RMS: %.8f; final RMS: %.8f\n",
                     bloomRms, lateRms, finalRms);
        check (isFiniteAndBounded (wide, 8.0001f)
                   && bloomRms > 1.0e-4f
                   && lateRms > 1.0e-6f
                   && lateRms < bloomRms
                   && finalRms < lateRms,
               "A short burst blooms into a finite, smoothly decaying late wash");

        parameters.widthPercent = 0.0f;
        const auto monoWidth = renderWithPartitions (
            burst, sampleRate, parameters, { 257, 17, 64 });
        parameters.widthPercent = 160.0f;
        juce::AudioBuffer<float> monoBurst (1, sampleCount);
        monoBurst.copyFrom (0, 0, burst, 0, 0, sampleCount);
        const auto mono = renderWithPartitions (
            monoBurst, sampleRate, parameters, { 97, 256, 511 });
        const auto monoLateRms = combinedRms (
            mono, static_cast<int> (sampleRate * 3.0),
            static_cast<int> (sampleRate * 2.0));
        std::printf ("  width-0 max L/R diff: %.9f; wide side RMS: %.8f; "
                     "mono late RMS: %.8f\n",
                     maximumStereoDifference (monoWidth), onePassSideRms, monoLateRms);
        check (maximumStereoDifference (monoWidth) <= 1.0e-7f
                   && onePassSideRms > 1.0e-5f
                   && isFiniteAndBounded (mono, 8.0001f)
                   && monoLateRms > 1.0e-6f,
               "Diffuse tail obeys Width 0%, decorrelates stereo, and remains valid in mono");

        GranularShimmerEngine resetEngine;
        resetEngine.prepare (sampleRate, 512, 2);
        juce::AudioBuffer<float> excitation (2, static_cast<int> (sampleRate));
        excitation.copyFrom (0, 0, burst, 0, 0, excitation.getNumSamples());
        excitation.copyFrom (1, 0, burst, 1, 0, excitation.getNumSamples());
        resetEngine.process (excitation, parameters, false);
        resetEngine.reset();
        juce::AudioBuffer<float> resetSilence (2, 12000);
        resetSilence.clear();
        resetEngine.process (resetSilence, parameters, false);
        check (isFiniteAndBounded (resetSilence, 0.0f),
               "Reset clears every grain and diffuse-tail state sample exactly");
    }

    {
        std::printf ("\n[Tail decay physics]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto totalSamples = static_cast<int> (sampleRate * 4.0);
        const auto burstSamples = static_cast<int> (sampleRate * 0.5);

        const auto makeBurst = [&] (float amplitude)
        {
            juce::AudioBuffer<float> buffer (2, totalSamples);
            buffer.clear();
            for (int sample = 0; sample < burstSamples; ++sample)
            {
                const auto phase = juce::MathConstants<float>::twoPi
                                 * static_cast<float> (sample)
                                 / static_cast<float> (sampleRate);
                const auto value = amplitude
                    * (0.5f * std::sin (1000.0f * phase)
                       + 0.5f * std::sin (9000.0f * phase));
                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, value);
            }
            return buffer;
        };

        GranularShimmerEngine::Parameters tailParameters;
        tailParameters.timeMs = 100.0f;
        tailParameters.sizeMs = 120.0f;
        tailParameters.density = 14.0f;
        tailParameters.scatterPercent = 0.0f;
        tailParameters.levelVariationDb = 0.0f;
        tailParameters.positionRandomPercent = 0.0f;
        tailParameters.pitchSpraySemitones = 0.0f;
        tailParameters.pitchSemitones = 0.0f;
        tailParameters.reversePercent = 0.0f;
        tailParameters.feedbackPercent = 55.0f;
        tailParameters.toneHz = 18000.0f;
        tailParameters.widthPercent = 100.0f;
        tailParameters.mixPercent = 100.0f;

        const auto brightInput = makeBurst (0.6f);
        const auto brightTail = renderWithPartitions (
            brightInput, sampleRate, tailParameters, { 1, 31, 127, 509 });

        const auto windowSamples = static_cast<int> (sampleRate * 0.5);
        const auto earlyStart = static_cast<int> (sampleRate * 1.5);
        const auto lateStart = static_cast<int> (sampleRate * 2.5);
        const auto* tailLeft = brightTail.getReadPointer (0);
        const auto earlyLow = spectralPower (
            tailLeft + earlyStart, windowSamples, sampleRate, 1000.0);
        const auto earlyHigh = spectralPower (
            tailLeft + earlyStart, windowSamples, sampleRate, 9000.0);
        const auto lateLow = spectralPower (
            tailLeft + lateStart, windowSamples, sampleRate, 1000.0);
        const auto lateHigh = spectralPower (
            tailLeft + lateStart, windowSamples, sampleRate, 9000.0);
        const auto earlyRatio = earlyHigh / juce::jmax (1.0e-30, earlyLow);
        const auto lateRatio = lateHigh / juce::jmax (1.0e-30, lateLow);
        const auto highFrequencyRetention = lateRatio / juce::jmax (1.0e-30, earlyRatio);
        std::printf ("  tail HF/LF power at 1.5 s: %.6e; at 2.5 s: %.6e; retention: %.6e\n",
                     earlyRatio, lateRatio, highFrequencyRetention);
        check (isFiniteAndBounded (brightTail, 8.0001f)
                   && earlyHigh > 1.0e-6
                   && lateHigh > 1.0e-8
                   && highFrequencyRetention > 1.0e-3,
               "Loop interpolation preserves tail high frequencies for Tone to govern");

        const auto loudInput = makeBurst (4.0f);
        const auto loudTail = renderWithPartitions (
            loudInput, sampleRate, tailParameters, { 1, 31, 127, 509 });
        const auto quietInput = makeBurst (0.04f);
        const auto quietTail = renderWithPartitions (
            quietInput, sampleRate, tailParameters, { 1, 31, 127, 509 });
        const auto loudEarlyRms = combinedRms (loudTail, earlyStart, windowSamples);
        const auto loudLateRms = combinedRms (loudTail, lateStart, windowSamples);
        const auto quietEarlyRms = combinedRms (quietTail, earlyStart, windowSamples);
        const auto quietLateRms = combinedRms (quietTail, lateStart, windowSamples);
        const auto loudDecay = loudLateRms / juce::jmax (1.0e-30f, loudEarlyRms);
        const auto quietDecay = quietLateRms / juce::jmax (1.0e-30f, quietEarlyRms);
        const auto decayAgreement = loudDecay / juce::jmax (1.0e-30f, quietDecay);
        std::printf ("  decay ratio loud: %.8f; quiet: %.8f; agreement: %.6f\n",
                     loudDecay, quietDecay, decayAgreement);
        check (isFiniteAndBounded (quietTail, 8.0001f)
                   && isFiniteAndBounded (loudTail, 8.0001f)
                   && loudEarlyRms > 1.0e-6f
                   && quietEarlyRms > 1.0e-7f
                   && decayAgreement > 0.7f
                   && decayAgreement < 1.43f,
               "Tail decay rate stays level independent across a 40 dB input range");

        constexpr auto morphSamples = static_cast<int> (sampleRate * 9.0);
        juce::AudioBuffer<float> morphInput (2, morphSamples);
        morphInput.clear();
        for (int sample = 0; sample < burstSamples; ++sample)
            for (int channel = 0; channel < 2; ++channel)
                morphInput.setSample (channel, sample,
                                      brightInput.getSample (channel, sample));

        auto morphParameters = tailParameters;
        morphParameters.sizeMs = 500.0f;

        GranularShimmerEngine morphEngine;
        morphEngine.prepare (sampleRate, 256, 2);
        juce::AudioBuffer<float> morphOutput (2, morphSamples);
        morphOutput.clear();

        int morphPosition = 0;
        while (morphPosition < morphSamples)
        {
            const auto blockSize = juce::jmin (256, morphSamples - morphPosition);
            juce::AudioBuffer<float> block (2, blockSize);
            for (int channel = 0; channel < 2; ++channel)
                block.copyFrom (channel, 0, morphInput, channel, morphPosition, blockSize);

            if (morphPosition >= static_cast<int> (sampleRate * 0.6))
                morphParameters.sizeMs = 40.0f;

            morphEngine.process (block, morphParameters, false);

            for (int channel = 0; channel < 2; ++channel)
                morphOutput.copyFrom (channel, morphPosition, block, channel, 0, blockSize);

            morphPosition += blockSize;
        }

        const auto morphEarlyRms = combinedRms (
            morphOutput, static_cast<int> (sampleRate * 1.0),
            static_cast<int> (sampleRate * 1.0));
        const auto morphLateRms = combinedRms (
            morphOutput, static_cast<int> (sampleRate * 4.0),
            static_cast<int> (sampleRate * 1.0));
        const auto morphDecay = morphLateRms / juce::jmax (1.0e-30f, morphEarlyRms);
        std::printf ("  Size 500->40 morph RMS at 1.0 s: %.8f; at 4.0 s: %.8f; decay: %.8f\n",
                     morphEarlyRms, morphLateRms, morphDecay);
        check (isFiniteAndBounded (morphOutput, 8.0001f)
                   && morphEarlyRms > 1.0e-6f
                   && morphDecay < 0.25f,
               "Tail keeps decaying while a 4 s Size morph retunes every loop gain");
    }

    {
        std::printf ("\n[Audio-rate granulation]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto sampleCount = static_cast<int> (sampleRate * 2.0);
        juce::AudioBuffer<float> input (2, sampleCount);
        fillSine (input, sampleRate, 220.0f, 0.4f);

        GranularShimmerEngine::Parameters synthesis;
        synthesis.timeMs = 120.0f;
        synthesis.sizeMs = 6.0f;
        synthesis.density = 400.0f;
        synthesis.scatterPercent = 0.0f;
        synthesis.levelVariationDb = 0.0f;
        synthesis.positionRandomPercent = 0.0f;
        synthesis.pitchSpraySemitones = 0.0f;
        synthesis.pitchSemitones = 12.0f;
        synthesis.reversePercent = 0.0f;
        synthesis.feedbackPercent = 0.0f;
        synthesis.toneHz = 18000.0f;
        synthesis.widthPercent = 100.0f;
        synthesis.mixPercent = 100.0f;

        const auto analysisStart = static_cast<int> (sampleRate * 1.0);
        const auto analysisLength = static_cast<int> (sampleRate * 0.5);

        const auto probe = [&] (const char* label, float density, float sizeMs,
                                float positionRandom, float pitchSpray)
        {
            auto probeParameters = synthesis;
            probeParameters.density = density;
            probeParameters.sizeMs = sizeMs;
            probeParameters.positionRandomPercent = positionRandom;
            probeParameters.pitchSpraySemitones = pitchSpray;
            const auto rendered = renderWithPartitions (
                input, sampleRate, probeParameters, { 1, 31, 127, 509 });
            const auto* voice = rendered.getReadPointer (0) + analysisStart;
            const auto octave = spectralPower (voice, analysisLength, sampleRate, 440.0);
            const auto source = spectralPower (voice, analysisLength, sampleRate, 220.0);
            const auto rms = combinedRms (rendered, analysisStart, analysisLength);
            const auto ratio = octave / juce::jmax (1.0e-30, source);
            std::printf ("  %-26s rms %.6f  220Hz %.4e  440Hz %.4e  ratio %.4f  bounded %d\n",
                         label, rms, source, octave, ratio,
                         isFiniteAndBounded (rendered, 8.0001f) ? 1 : 0);
            return std::make_pair (rendered, ratio);
        };

        const auto legacyCoherent = probe ("9 gr/s 160 ms coherent", 9.0f, 160.0f, 0.0f, 0.0f);
        probe ("9 gr/s 160 ms randomised", 9.0f, 160.0f, 32.0f, 0.112f);
        const auto shortCoherent = probe ("24 gr/s 40 ms coherent", 24.0f, 40.0f, 0.0f, 0.0f);
        probe ("400 gr/s 6 ms coherent", 400.0f, 6.0f, 0.0f, 0.0f);
        const auto audioRate = probe ("400 gr/s 6 ms randomised", 400.0f, 6.0f, 32.0f, 0.112f);
        const auto fastRate = probe ("1200 gr/s 6 ms randomised", 1200.0f, 6.0f, 32.0f, 0.112f);
        const auto extreme = probe ("2000 gr/s 500 ms extreme", 2000.0f, 500.0f, 100.0f, 12.0f);

        check (isFiniteAndBounded (extreme.first, 8.0001f)
                   && isFiniteAndBounded (fastRate.first, 8.0001f)
                   && combinedRms (extreme.first, analysisStart, analysisLength) > 1.0e-4f,
               "Audio-rate density stays finite and bounded when voice stealing saturates");

        check (audioRate.second > 2.0 && fastRate.second > 2.0,
               "Randomised audio-rate granulation still tracks the shifted grain pitch");

        check (legacyCoherent.second > 1.0e3,
               "Cloud-range grain sizes keep the shifted pitch without any randomisation");

        check (shortCoherent.second < 1.0,
               "Short coherent grains phase-cancel the shift and need position randomisation");

        auto deterministic = synthesis;
        deterministic.density = 900.0f;
        deterministic.sizeMs = 4.0f;
        deterministic.positionRandomPercent = 45.0f;
        const auto fixedBlocks = renderWithPartitions (
            input, sampleRate, deterministic, { 512 });
        const auto arbitraryBlocks = renderWithPartitions (
            input, sampleRate, deterministic, { 1, 31, 127, 509, 64, 3, 256 });
        check (maximumDifference (fixedBlocks, arbitraryBlocks) <= 1.0e-6f,
               "Audio-rate granulation stays invariant to host block partitioning");
    }

    {
        std::printf ("\n[Freeze]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto totalSamples = static_cast<int> (sampleRate * 4.0);
        const auto burstSamples = static_cast<int> (sampleRate * 0.5);
        juce::AudioBuffer<float> input (2, totalSamples);
        input.clear();
        for (int sample = 0; sample < burstSamples; ++sample)
        {
            const auto value = 0.4f * std::sin (
                juce::MathConstants<float>::twoPi * 220.0f
                * static_cast<float> (sample) / static_cast<float> (sampleRate));
            input.setSample (0, sample, value);
            input.setSample (1, sample, value);
        }

        GranularShimmerEngine::Parameters holdParameters;
        holdParameters.timeMs = 200.0f;
        holdParameters.sizeMs = 160.0f;
        holdParameters.density = 12.0f;
        holdParameters.scatterPercent = 20.0f;
        holdParameters.levelVariationDb = 0.0f;
        holdParameters.positionRandomPercent = 25.0f;
        holdParameters.pitchSpraySemitones = 0.0f;
        holdParameters.pitchSemitones = 0.0f;
        holdParameters.reversePercent = 0.0f;
        holdParameters.feedbackPercent = 0.0f;
        holdParameters.toneHz = 18000.0f;
        holdParameters.widthPercent = 100.0f;
        holdParameters.mixPercent = 100.0f;

        const auto renderWithFreezeAt = [&] (double freezeSeconds)
        {
            GranularShimmerEngine engine;
            engine.prepare (sampleRate, 256, 2);
            juce::AudioBuffer<float> output (2, totalSamples);
            output.clear();
            auto blockParameters = holdParameters;

            int position = 0;
            while (position < totalSamples)
            {
                const auto blockSize = juce::jmin (256, totalSamples - position);
                juce::AudioBuffer<float> block (2, blockSize);
                for (int channel = 0; channel < 2; ++channel)
                    block.copyFrom (channel, 0, input, channel, position, blockSize);

                blockParameters.frozen = freezeSeconds >= 0.0
                    && position >= static_cast<int> (sampleRate * freezeSeconds);
                engine.process (block, blockParameters, false);

                for (int channel = 0; channel < 2; ++channel)
                    output.copyFrom (channel, position, block, channel, 0, blockSize);
                position += blockSize;
            }
            return output;
        };

        const auto running = renderWithFreezeAt (-1.0);
        const auto held = renderWithFreezeAt (0.6);
        const auto lateStart = static_cast<int> (sampleRate * 3.0);
        const auto lateLength = static_cast<int> (sampleRate * 0.9);
        const auto runningLate = combinedRms (running, lateStart, lateLength);
        const auto heldLate = combinedRms (held, lateStart, lateLength);
        std::printf ("  late RMS running %.8f; frozen %.8f; ratio %.2f\n",
                     runningLate, heldLate, heldLate / juce::jmax (1.0e-30f, runningLate));
        check (isFiniteAndBounded (held, 8.0001f)
                   && isFiniteAndBounded (running, 8.0001f)
                   && heldLate > runningLate * 100.0f
                   && heldLate > 1.0e-3f,
               "Freeze holds the captured history so grains keep sounding after input stops");

        auto dryParameters = holdParameters;
        dryParameters.mixPercent = 0.0f;
        GranularShimmerEngine dryEngine;
        dryEngine.prepare (sampleRate, 256, 2);
        juce::AudioBuffer<float> dryOutput;
        dryOutput.makeCopyOf (input);
        dryParameters.frozen = true;
        dryEngine.process (dryOutput, dryParameters, false);
        check (maximumDifference (dryOutput, input) <= 1.0e-7f,
               "Freeze leaves the dry path sample-identical at Mix 0%");
    }

    {
        std::printf ("\n[Sparkle shards]\n");
        constexpr auto sampleRate = 48000.0;
        constexpr auto noteSpacingSeconds = 2.0;
        constexpr auto noteCount = 3;
        constexpr auto sampleCount = static_cast<int> (
            sampleRate * noteSpacingSeconds * noteCount);
        const auto noteStart = [&] (int note)
        {
            return static_cast<int> (sampleRate * (0.1 + noteSpacingSeconds * note));
        };

        juce::AudioBuffer<float> plucks (2, sampleCount);
        plucks.clear();
        for (int note = 0; note < noteCount; ++note)
        {
            const auto length = static_cast<int> (sampleRate * 0.6);
            for (int sample = 0; sample < length; ++sample)
            {
                const auto time = static_cast<double> (sample) / sampleRate;
                const auto envelope = juce::jmin (1.0, time / 0.002) * std::exp (-time * 6.0);
                const auto value = static_cast<float> (0.5 * envelope * (
                    std::sin (juce::MathConstants<double>::twoPi * 220.0 * time)
                    + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * time)));
                plucks.setSample (0, noteStart (note) + sample, value);
                plucks.setSample (1, noteStart (note) + sample, value);
            }
        }

        GranularShimmerEngine::Parameters base;
        base.timeMs = 240.0f;
        base.sizeMs = 120.0f;
        base.density = 12.0f;
        base.scatterPercent = 20.0f;
        base.pitchSemitones = 12.0f;
        base.reversePercent = 0.0f;
        base.feedbackPercent = 30.0f;
        base.toneHz = 16000.0f;
        base.widthPercent = 100.0f;
        base.mixPercent = 100.0f;

        const std::vector<int> partitions { 97, 256, 511 };
        const auto clean = renderWithPartitions (plucks, sampleRate, base, partitions);

        auto shatterOnly = base;
        shatterOnly.shatterPercent = 100.0f;
        const auto shatterOnlyOutput = renderWithPartitions (
            plucks, sampleRate, shatterOnly, partitions);
        check (maximumDifference (clean, shatterOnlyOutput) == 0.0f,
               "Sparkle 0% leaves the engine sample-identical whatever Shatter is set to");

        const auto difference = [&] (const juce::AudioBuffer<float>& rendered)
        {
            juce::AudioBuffer<float> result;
            result.makeCopyOf (rendered);
            for (int channel = 0; channel < result.getNumChannels(); ++channel)
                result.addFrom (channel, 0, clean, channel, 0, result.getNumSamples(), -1.0f);
            return result;
        };

        // Energy of the first difference over energy: 4 sin^2 (pi f / fs) for a
        // sinusoid, so a larger value means a brighter signal.
        const auto brightness = [] (const juce::AudioBuffer<float>& buffer)
        {
            double slope = 0.0;
            double energy = 0.0;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 1; sample < buffer.getNumSamples(); ++sample)
                {
                    const auto current = static_cast<double> (buffer.getSample (channel, sample));
                    const auto previous = static_cast<double> (buffer.getSample (channel, sample - 1));
                    slope += (current - previous) * (current - previous);
                    energy += current * current;
                }
            return slope / juce::jmax (1.0e-30, energy);
        };

        auto glitter = base;
        glitter.sparklePercent = 70.0f;
        const auto glitterOutput = renderWithPartitions (
            plucks, sampleRate, glitter, partitions);
        const auto glitterLayer = difference (glitterOutput);
        const auto layerRms = combinedRms (glitterLayer, 0, sampleCount);
        const auto cleanRms = combinedRms (clean, 0, sampleCount);
        const auto layerBrightness = brightness (glitterLayer);
        const auto cleanBrightness = brightness (clean);
        std::printf ("  sparkle 70%% layer RMS: %.6f (cloud %.6f); brightness %.5f vs cloud %.5f\n",
                     layerRms, cleanRms, layerBrightness, cleanBrightness);
        check (isFiniteAndBounded (glitterOutput, 8.0001f)
                   && layerRms > cleanRms * 0.03f
                   && layerRms < cleanRms * 2.0f
                   && layerBrightness > cleanBrightness * 3.0,
               "Sparkle adds an audible shard layer that is far brighter than the grain cloud");

        auto broken = base;
        broken.sparklePercent = 80.0f;
        broken.shatterPercent = 60.0f;
        const auto brokenFixed = renderWithPartitions (
            plucks, sampleRate, broken, { 512 });
        const auto brokenArbitrary = renderWithPartitions (
            plucks, sampleRate, broken, { 1, 31, 127, 509, 64, 3, 256 });
        check (maximumDifference (brokenFixed, brokenArbitrary) <= 1.0e-6f
                   && isFiniteAndBounded (brokenFixed, 8.0001f),
               "Sparkle and Shatter stay invariant to host block partitioning");

        // Count shards per window straight from the engine's spawn counter.
        const auto countShards = [&] (float shatterPercent, double windowStartSeconds,
                                      double windowLengthSeconds)
        {
            auto counted = base;
            counted.sparklePercent = 100.0f;
            counted.shatterPercent = shatterPercent;
            GranularShimmerEngine engine;
            engine.prepare (sampleRate, 64, 2);

            std::uint32_t total = 0;
            int position = 0;
            while (position < sampleCount)
            {
                const auto blockSize = juce::jmin (64, sampleCount - position);
                juce::AudioBuffer<float> block (2, blockSize);
                for (int channel = 0; channel < 2; ++channel)
                    block.copyFrom (channel, 0, plucks, channel, position, blockSize);

                const auto before = engine.getShardSpawnCount();
                engine.process (block, counted, false);
                const auto spawned = engine.getShardSpawnCount() - before;

                for (int note = 0; note < noteCount; ++note)
                {
                    const auto start = noteStart (note)
                                     + static_cast<int> (sampleRate * windowStartSeconds);
                    const auto end = start + static_cast<int> (sampleRate * windowLengthSeconds);
                    if (position >= start && position < end)
                        total += spawned;
                }
                position += blockSize;
            }
            return static_cast<int> (total);
        };

        const auto burstEarly = countShards (100.0f, 0.0, 0.4);
        const auto burstLate = countShards (100.0f, 1.5, 0.4);
        const auto dustEarly = countShards (0.0f, 0.0, 0.4);
        const auto dustLate = countShards (0.0f, 1.5, 0.4);
        std::printf ("  shards in 0.4 s after attack / 1.5 s later: shatter 100%% %d / %d; "
                     "shatter 0%% %d / %d\n",
                     burstEarly, burstLate, dustEarly, dustLate);
        check (burstEarly > 60 && burstEarly > burstLate * 5,
               "Shatter breaks each attack into a dense burst of shards that thins out");
        check (dustEarly > 10 && dustLate > 10
                   && dustEarly < dustLate * 2 && dustLate < dustEarly * 2,
               "Shatter 0% keeps the sparkle an even drizzle regardless of attacks");

        {
            constexpr auto echoSamples = static_cast<int> (sampleRate * 3.0);
            juce::AudioBuffer<float> sustained (2, echoSamples);
            for (int sample = 0; sample < echoSamples; ++sample)
            {
                const auto time = static_cast<double> (sample) / sampleRate;
                const auto value = static_cast<float> (0.12 * (
                    std::sin (juce::MathConstants<double>::twoPi * 220.0 * time)
                    + std::sin (juce::MathConstants<double>::twoPi * 277.18 * time)
                    + std::sin (juce::MathConstants<double>::twoPi * 329.63 * time)));
                sustained.setSample (0, sample, value);
                sustained.setSample (1, sample, value);
            }

            auto echoParameters = base;
            echoParameters.timeMs = 500.0f;
            echoParameters.sizeMs = 20.0f;
            echoParameters.density = 2.0f;
            echoParameters.feedbackPercent = 0.0f;
            echoParameters.toneHz = 18000.0f;
            const auto echoClean = renderWithPartitions (
                sustained, sampleRate, echoParameters, partitions);
            echoParameters.sparklePercent = 100.0f;
            auto echoLayer = renderWithPartitions (
                sustained, sampleRate, echoParameters, partitions);
            for (int channel = 0; channel < 2; ++channel)
                echoLayer.addFrom (channel, 0, echoClean, channel, 0, echoSamples, -1.0f);

            // A shard heard on the left must come back on the right one Time later.
            const auto crossCorrelation = [&] (int lag)
            {
                double product = 0.0;
                double leftEnergy = 0.0;
                double rightEnergy = 0.0;
                for (int sample = 0; sample + lag < echoSamples; ++sample)
                {
                    const auto left = static_cast<double> (echoLayer.getSample (0, sample));
                    const auto right = static_cast<double> (echoLayer.getSample (1, sample + lag));
                    product += left * right;
                    leftEnergy += left * left;
                    rightEnergy += right * right;
                }
                return product / juce::jmax (1.0e-30, std::sqrt (leftEnergy * rightEnergy));
            };

            const auto atTime = crossCorrelation (static_cast<int> (sampleRate * 0.5));
            const auto offTime = crossCorrelation (static_cast<int> (sampleRate * 0.385));
            std::printf ("  L->R sparkle correlation at Time: %.4f; off Time: %.4f\n",
                         atTime, offTime);
            check (atTime > 0.2 && std::abs (offTime) < atTime * 0.4,
                   "Sparkle shards ping-pong to the opposite side one Time later");
        }

        {
            auto resetParameters = broken;
            resetParameters.feedbackPercent = 60.0f;
            GranularShimmerEngine engine;
            engine.prepare (sampleRate, 512, 2);
            juce::AudioBuffer<float> first;
            first.makeCopyOf (plucks);
            engine.process (first, resetParameters, false);
            engine.reset();
            juce::AudioBuffer<float> second;
            second.makeCopyOf (plucks);
            engine.process (second, resetParameters, false);
            engine.reset();
            juce::AudioBuffer<float> silence (2, 24000);
            silence.clear();
            engine.process (silence, resetParameters, false);
            check (maximumDifference (first, second) <= 1.0e-6f
                       && isFiniteAndBounded (silence, 0.0f)
                       && engine.getActiveShardCount() == 0,
                   "Reset restores the shard stream and clears every sparkle echo exactly");
        }

        {
            // Sparkle on for the first note, then back to 0%: once the echoes ring
            // out the engine must agree with one that never sparkled.
            auto ringParameters = base;
            ringParameters.feedbackPercent = 0.0f;
            GranularShimmerEngine sparkled;
            GranularShimmerEngine untouched;
            sparkled.prepare (sampleRate, 256, 2);
            untouched.prepare (sampleRate, 256, 2);
            juce::AudioBuffer<float> sparkledOutput (2, sampleCount);
            juce::AudioBuffer<float> untouchedOutput (2, sampleCount);

            int position = 0;
            while (position < sampleCount)
            {
                const auto blockSize = juce::jmin (256, sampleCount - position);
                juce::AudioBuffer<float> blockA (2, blockSize);
                juce::AudioBuffer<float> blockB (2, blockSize);
                for (int channel = 0; channel < 2; ++channel)
                {
                    blockA.copyFrom (channel, 0, plucks, channel, position, blockSize);
                    blockB.copyFrom (channel, 0, plucks, channel, position, blockSize);
                }

                auto active = ringParameters;
                if (position < static_cast<int> (sampleRate * 1.0))
                {
                    active.sparklePercent = 100.0f;
                    active.shatterPercent = 100.0f;
                }
                sparkled.process (blockA, active, false);
                untouched.process (blockB, ringParameters, false);
                for (int channel = 0; channel < 2; ++channel)
                {
                    sparkledOutput.copyFrom (channel, position, blockA, channel, 0, blockSize);
                    untouchedOutput.copyFrom (channel, position, blockB, channel, 0, blockSize);
                }
                position += blockSize;
            }

            const auto tailStart = static_cast<int> (sampleRate * 4.0);
            auto residual = 0.0f;
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = tailStart; sample < sampleCount; ++sample)
                    residual = juce::jmax (residual, std::abs (
                        sparkledOutput.getSample (channel, sample)
                        - untouchedOutput.getSample (channel, sample)));
            std::printf ("  residual 3 s after Sparkle returns to 0%%: %.3e\n", residual);
            check (residual <= 1.0e-7f && sparkled.getActiveShardCount() == 0,
                   "Sparkle rings out completely after it returns to 0%");
        }

        {
            juce::AudioBuffer<float> monoPlucks (1, sampleCount);
            monoPlucks.copyFrom (0, 0, plucks, 0, 0, sampleCount);
            const auto monoClean = renderWithPartitions (
                monoPlucks, sampleRate, base, partitions);
            const auto monoBroken = renderWithPartitions (
                monoPlucks, sampleRate, broken, partitions);
            check (isFiniteAndBounded (monoBroken, 8.0001f)
                       && maximumDifference (monoClean, monoBroken) > 1.0e-4f,
                   "Sparkle and Shatter remain valid and audible in mono");

            auto collapsed = broken;
            collapsed.widthPercent = 0.0f;
            const auto collapsedOutput = renderWithPartitions (
                plucks, sampleRate, collapsed, partitions);
            const auto wideLayer = difference (brokenFixed);
            const auto wideSide = stereoSideRms (wideLayer, 0, sampleCount);
            const auto wideTotal = combinedRms (wideLayer, 0, sampleCount);
            std::printf ("  shard layer side RMS %.6f of total %.6f at Width 100%%; "
                         "Width 0%% max L/R diff %.3e\n",
                         wideSide, wideTotal, maximumStereoDifference (collapsedOutput));
            check (maximumStereoDifference (collapsedOutput) <= 1.0e-7f
                       && wideSide > wideTotal * 0.5f,
                   "Shards spread to the edges of the image yet collapse to mono at Width 0%");
        }
    }

    {
        std::printf ("\n[Processor state]\n");
        TBShimmerAudioProcessor source;
        const auto& hostParameters = source.getParameters();
        auto hostOrderOk = hostParameters.size() == static_cast<int> (parameterIds.size());
        for (int index = 0; index < hostParameters.size() && hostOrderOk; ++index)
        {
            const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*> (
                hostParameters[index]);
            hostOrderOk = identified != nullptr
                       && identified->paramID == parameterIds[static_cast<size_t> (index)];
        }
        hostOrderOk = hostOrderOk
                   && source.getBypassParameter() == hostParameters[10];
        check (hostOrderOk,
               "Host enumeration preserves the old 11 IDs then appends Level/Position/Pitch/Freeze/Sparkle/Shatter");

        const std::array<float, parameterIds.size()> testValues
        {{
            777.0f, 311.0f, 17.0f, 63.0f, 19.0f, 71.0f,
            73.0f, 12345.0f, 177.0f, 64.0f, 1.0f,
            9.0f, 76.0f, 5.5f, 1.0f,
            37.0f, 81.0f
        }};
        auto parametersFound = true;
        for (size_t index = 0; index < parameterIds.size(); ++index)
            parametersFound = setParameter (source, parameterIds[index], testValues[index])
                           && parametersFound;

        juce::MemoryBlock state;
        source.getStateInformation (state);
        TBShimmerAudioProcessor restored;
        restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        check (parametersFound
                   && approximatelyEqual (readAllParameters (restored), testValues),
               "Versioned state round-trip restores all public parameters");

        auto legacyMigrated = false;
        if (auto legacyXml = TBShimmerAudioProcessor::getXmlFromBinary (
                state.getData(), static_cast<int> (state.getSize())))
        {
            auto legacyTree = juce::ValueTree::fromXml (*legacyXml);
            for (int childIndex = legacyTree.getNumChildren(); --childIndex >= 0;)
            {
                const auto id = legacyTree.getChild (childIndex)
                                             .getProperty ("id").toString();
                if (id == TBShimmerAudioProcessor::levelVarId
                    || id == TBShimmerAudioProcessor::positionRandId
                    || id == TBShimmerAudioProcessor::pitchSprayId
                    || id == TBShimmerAudioProcessor::freezeId
                    || id == TBShimmerAudioProcessor::sparkleId
                    || id == TBShimmerAudioProcessor::shatterId)
                    legacyTree.removeChild (childIndex, nullptr);
            }
            legacyTree.setProperty ("stateSchemaVersion", 1, nullptr);
            legacyTree.setProperty ("presetFormatVersion", 1, nullptr);

            if (auto encodedLegacy = legacyTree.createXml())
            {
                juce::MemoryBlock legacyState;
                TBShimmerAudioProcessor::copyXmlToBinary (
                    *encodedLegacy, legacyState);
                TBShimmerAudioProcessor migrated;
                migrated.setStateInformation (
                    legacyState.getData(), static_cast<int> (legacyState.getSize()));

                auto expectedLegacy = testValues;
                expectedLegacy[11] = 0.0f;
                expectedLegacy[12] = testValues[3];
                expectedLegacy[13] = testValues[3] * 0.0035f;
                expectedLegacy[14] = 0.0f;
                expectedLegacy[15] = 0.0f;
                expectedLegacy[16] = 0.0f;

                juce::MemoryBlock migratedState;
                migrated.getStateInformation (migratedState);
                auto migratedXml = TBShimmerAudioProcessor::getXmlFromBinary (
                    migratedState.getData(), static_cast<int> (migratedState.getSize()));
                legacyMigrated = approximatelyEqual (
                                     readAllParameters (migrated), expectedLegacy)
                              && migratedXml != nullptr
                              && migratedXml->getIntAttribute ("stateSchemaVersion") == 4
                              && migratedXml->getIntAttribute ("presetFormatVersion") == 4
                              && migratedXml->getNumChildElements()
                                     == static_cast<int> (parameterIds.size());
            }
        }
        check (legacyMigrated,
               "v1 state migrates atomically to v4 with the 3-1-2 mapping and Freeze/Sparkle off");

        auto freezeEraMigrated = false;
        if (auto freezeEraXml = TBShimmerAudioProcessor::getXmlFromBinary (
                state.getData(), static_cast<int> (state.getSize())))
        {
            auto freezeEraTree = juce::ValueTree::fromXml (*freezeEraXml);
            for (int childIndex = freezeEraTree.getNumChildren(); --childIndex >= 0;)
            {
                const auto id = freezeEraTree.getChild (childIndex)
                                                .getProperty ("id").toString();
                if (id == TBShimmerAudioProcessor::sparkleId
                    || id == TBShimmerAudioProcessor::shatterId)
                    freezeEraTree.removeChild (childIndex, nullptr);
            }
            freezeEraTree.setProperty ("stateSchemaVersion", 3, nullptr);
            freezeEraTree.setProperty ("presetFormatVersion", 3, nullptr);

            if (auto encodedFreezeEra = freezeEraTree.createXml())
            {
                juce::MemoryBlock freezeEraState;
                TBShimmerAudioProcessor::copyXmlToBinary (
                    *encodedFreezeEra, freezeEraState);
                TBShimmerAudioProcessor migrated;
                migrated.setStateInformation (
                    freezeEraState.getData(), static_cast<int> (freezeEraState.getSize()));

                auto expectedFreezeEra = testValues;
                expectedFreezeEra[15] = 0.0f;
                expectedFreezeEra[16] = 0.0f;
                freezeEraMigrated = approximatelyEqual (
                    readAllParameters (migrated), expectedFreezeEra);
            }
        }
        check (freezeEraMigrated,
               "v3 state keeps all 15 values and migrates to v4 with Sparkle/Shatter at 0");

        const auto beforeInvalid = readAllParameters (restored);
        restored.setStateInformation ("invalid", 7);
        check (approximatelyEqual (readAllParameters (restored), beforeInvalid),
               "Malformed state is rejected without partial mutation");

        auto wrongTagRejected = false;
        if (auto wrongTagXml = TBShimmerAudioProcessor::getXmlFromBinary (
                state.getData(), static_cast<int> (state.getSize())))
        {
            auto wrongTagTree = juce::ValueTree::fromXml (*wrongTagXml);
            const auto originalChild = wrongTagTree.getChild (0);
            juce::ValueTree wrongTagChild ("NOT_PARAM");
            for (int propertyIndex = 0;
                 propertyIndex < originalChild.getNumProperties(); ++propertyIndex)
            {
                const auto propertyName = originalChild.getPropertyName (propertyIndex);
                wrongTagChild.setProperty (
                    propertyName, originalChild.getProperty (propertyName), nullptr);
            }
            wrongTagTree.removeChild (0, nullptr);
            wrongTagTree.addChild (wrongTagChild, 0, nullptr);

            if (auto encodedWrongTag = wrongTagTree.createXml())
            {
                juce::MemoryBlock wrongTagState;
                TBShimmerAudioProcessor::copyXmlToBinary (
                    *encodedWrongTag, wrongTagState);
                restored.setStateInformation (
                    wrongTagState.getData(), static_cast<int> (wrongTagState.getSize()));
                wrongTagRejected = approximatelyEqual (
                    readAllParameters (restored), beforeInvalid);
            }
        }
        check (wrongTagRejected,
               "Non-canonical parameter child tags are rejected without partial mutation");

        auto xml = TBShimmerAudioProcessor::getXmlFromBinary (
            state.getData(), static_cast<int> (state.getSize()));
        auto futureRejected = false;
        if (xml != nullptr)
        {
            xml->setAttribute ("stateSchemaVersion", 99);
            juce::MemoryBlock futureState;
            TBShimmerAudioProcessor::copyXmlToBinary (*xml, futureState);
            restored.setStateInformation (futureState.getData(),
                                          static_cast<int> (futureState.getSize()));
            futureRejected = approximatelyEqual (
                readAllParameters (restored), beforeInvalid);
        }
        check (futureRejected,
               "Unknown future state schema is rejected without partial mutation");

        auto mixedVersionRejected = false;
        if (auto mixedXml = TBShimmerAudioProcessor::getXmlFromBinary (
                state.getData(), static_cast<int> (state.getSize())))
        {
            mixedXml->setAttribute ("stateSchemaVersion", 2);
            mixedXml->setAttribute ("presetFormatVersion", 1);
            juce::MemoryBlock mixedState;
            TBShimmerAudioProcessor::copyXmlToBinary (*mixedXml, mixedState);
            restored.setStateInformation (
                mixedState.getData(), static_cast<int> (mixedState.getSize()));
            mixedVersionRejected = approximatelyEqual (
                readAllParameters (restored), beforeInvalid);
        }
        check (mixedVersionRejected,
               "Mixed state schema/preset versions are rejected without partial mutation");
        check (restored.getLatencySamples() == 0
                   && restored.getTailLengthSeconds() >= 180.0,
               "Processor reports zero latency and a long feedback tail");
    }

    {
        std::printf ("\n[Factory and User presets]\n");
        TBShimmerAudioProcessor processor;

        const auto factoryCount = processor.getFactoryPresetCount();
        auto programApiOk = factoryCount == 100
                         && processor.getNumPrograms() == 100;
        for (int index = 0; index < factoryCount; ++index)
        {
            const auto& preset = processor.getFactoryPreset (index);
            const auto name = juce::String (preset.name != nullptr ? preset.name : "");
            programApiOk = processor.getProgramName (index) == name && programApiOk;
        }
        check (programApiOk,
               "Factory catalog and JUCE program API expose exactly 100 named programs");

        auto originalBankOk = factoryCount
                           >= static_cast<int> (originalFactoryPresetGolden.size());
        if (originalBankOk)
            for (size_t index = 0; index < originalFactoryPresetGolden.size(); ++index)
            {
                const auto& preset = processor.getFactoryPreset (static_cast<int> (index));
                const auto& golden = originalFactoryPresetGolden[index];
                originalBankOk = juce::String (preset.stableId) == golden.stableId
                              && juce::String (preset.group) == golden.group
                              && juce::String (preset.name) == golden.name
                              && factoryPresetValues (preset) == golden.values
                              && originalBankOk;
            }
        check (originalBankOk,
               "Original Factory 12 exactly preserve IDs, groups, names, order, and all 14 values");

        auto metadataOk = factoryCount == 100;
        auto valuesLegal = factoryCount == 100;
        std::vector<std::array<float, parameterIds.size()>> factoryStates;
        factoryStates.reserve (static_cast<size_t> (juce::jmax (0, factoryCount)));

        for (int index = 0; index < factoryCount; ++index)
        {
            const auto& preset = processor.getFactoryPreset (index);
            const auto stableId = juce::String (
                preset.stableId != nullptr ? preset.stableId : "").trim();
            const auto group = juce::String (
                preset.group != nullptr ? preset.group : "").trim();
            const auto name = juce::String (
                preset.name != nullptr ? preset.name : "").trim();
            const auto values = factoryPresetValues (preset);
            factoryStates.push_back (values);

            metadataOk = stableId.isNotEmpty() && group.isNotEmpty()
                      && name.isNotEmpty() && metadataOk;

            for (int prior = 0; prior < index; ++prior)
            {
                const auto& earlier = processor.getFactoryPreset (prior);
                metadataOk = ! stableId.equalsIgnoreCase (
                                 juce::String (earlier.stableId).trim())
                          && ! name.equalsIgnoreCase (
                                 juce::String (earlier.name).trim())
                          && metadataOk;
            }

            if (index > 0
                && ! group.equalsIgnoreCase (
                    juce::String (processor.getFactoryPreset (index - 1).group).trim()))
                for (int prior = 0; prior < index - 1; ++prior)
                    metadataOk = ! group.equalsIgnoreCase (
                                     juce::String (processor.getFactoryPreset (prior).group).trim())
                              && metadataOk;

            for (size_t parameterIndex = 0;
                 parameterIndex < parameterIds.size(); ++parameterIndex)
            {
                const auto* parameter = processor.getValueTreeState().getParameter (
                    parameterIds[parameterIndex]);
                if (parameter == nullptr || ! std::isfinite (values[parameterIndex]))
                {
                    valuesLegal = false;
                    continue;
                }

                const auto& range = parameter->getNormalisableRange();
                valuesLegal = values[parameterIndex] >= range.start
                           && values[parameterIndex] <= range.end
                           && valuesLegal;
            }

            valuesLegal = std::abs (values[4] - std::round (values[4])) <= 1.0e-6f
                       && values[9] > 0.0f
                       && values[10] == 0.0f
                       && valuesLegal;
        }
        check (metadataOk,
               "All 100 Factory presets have nonempty unique IDs/names and contiguous groups");
        check (valuesLegal,
               "All 100 Factory states are finite/in-range with integer Pitch, audible Mix, and Bypass off");

        auto distinctStates = factoryStates.size() == 100;
        for (size_t first = 0; first < factoryStates.size(); ++first)
            for (size_t second = first + 1; second < factoryStates.size(); ++second)
                distinctStates = ! approximatelyEqual (factoryStates[first],
                                                        factoryStates[second])
                              && distinctStates;
        auto sparkleBankOk = factoryCount == 100;
        auto sparklePresetCount = 0;
        for (int index = 0; index < factoryCount; ++index)
        {
            const auto& preset = processor.getFactoryPreset (index);
            const auto usesSparkle = preset.sparkle > 0.0f;
            const auto inSparkleGroup = juce::String (preset.group) == "SPARKLE";
            sparklePresetCount += usesSparkle ? 1 : 0;
            sparkleBankOk = usesSparkle == inSparkleGroup
                         && (usesSparkle || preset.shatter == 0.0f)
                         && preset.freeze == 0.0f
                         && sparkleBankOk;
        }
        check (sparkleBankOk && sparklePresetCount == 8,
               "Only the eight SPARKLE programs engage the sparkle layer; every other program keeps it off");

        check (distinctStates,
               "All 100 Factory presets have pairwise-distinct parameter states");

        auto allFactoryLoadsOk = factoryCount == 100;
        for (int index = 0; index < factoryCount; ++index)
        {
            const auto expected = factoryPresetValues (
                processor.getFactoryPreset (index));
            allFactoryLoadsOk = processor.applyFactoryPreset (index)
                             && approximatelyEqual (
                                    readAllParameters (processor), expected)
                             && processor.getSelectedFactoryPreset() == index
                             && processor.getSelectedUserPresetName().isEmpty()
                             && ! processor.isCurrentPresetDirty()
                             && allFactoryLoadsOk;
        }
        check (allFactoryLoadsOk,
               "Every Factory preset 0-99 loads all 17 values and selects a clean Factory state");

        check (! processor.applyFactoryPreset (-1)
                   && ! processor.applyFactoryPreset (100),
               "Factory preset indices -1 and 100 are rejected");

        auto highIndexStateOk = factoryCount == 100
                             && processor.applyFactoryPreset (99);
        if (highIndexStateOk)
        {
            const auto expected = factoryPresetValues (
                processor.getFactoryPreset (99));
            const auto expectedName = juce::String (
                processor.getFactoryPreset (99).name);
            juce::MemoryBlock highIndexState;
            processor.getStateInformation (highIndexState);

            TBShimmerAudioProcessor restoredHighIndex;
            restoredHighIndex.setStateInformation (
                highIndexState.getData(),
                static_cast<int> (highIndexState.getSize()));
            highIndexStateOk = approximatelyEqual (
                                   readAllParameters (restoredHighIndex), expected)
                            && restoredHighIndex.getSelectedFactoryPreset() == 99
                            && restoredHighIndex.getCurrentProgram() == 99
                            && restoredHighIndex.getProgramName (99) == expectedName
                            && restoredHighIndex.getSelectedUserPresetName().isEmpty()
                            && ! restoredHighIndex.isCurrentPresetDirty();
        }
        check (highIndexStateOk,
               "Factory preset 99 state round-trip restores values, index, name, and clean selection");

        const auto userDirectory = processor.getUserPresetDirectory();
        const auto directoryReady = userDirectory.createDirectory().wasOk();
        const auto validPreset = userDirectory.getNonexistentChildFile (
            "TB_Shimmer_Automated_Valid", ".tbshimmer", false);
        setParameter (processor, TBShimmerAudioProcessor::pitchId, 19.0f);
        const auto expectedPresetValues = readAllParameters (processor);
        const auto saveResult = directoryReady
            ? processor.saveUserPreset (validPreset)
            : juce::Result::fail ("User preset directory could not be created");
        setParameter (processor, TBShimmerAudioProcessor::pitchId, -12.0f);
        const auto loadResult = saveResult.wasOk()
            ? processor.loadUserPreset (validPreset)
            : saveResult;
        check (saveResult.wasOk() && loadResult.wasOk()
                   && approximatelyEqual (readAllParameters (processor),
                                            expectedPresetValues),
               "User preset atomically saves and restores all parameters");

        const auto legacyPreset = userDirectory.getNonexistentChildFile (
            "TB_Shimmer_Automated_Legacy", ".tbshimmer", false);
        auto legacyPresetWritten = false;
        if (saveResult.wasOk())
            if (auto savedXml = juce::parseXML (validPreset))
            {
                auto legacyPresetTree = juce::ValueTree::fromXml (*savedXml);
                legacyPresetTree.setProperty ("presetFormatVersion", 1, nullptr);
                legacyPresetTree.setProperty ("stateSchemaVersion", 1, nullptr);

                auto legacyStateTree = legacyPresetTree.getChild (0);
                for (int childIndex = legacyStateTree.getNumChildren(); --childIndex >= 0;)
                {
                    const auto id = legacyStateTree.getChild (childIndex)
                                                   .getProperty ("id").toString();
                    if (id == TBShimmerAudioProcessor::levelVarId
                        || id == TBShimmerAudioProcessor::positionRandId
                        || id == TBShimmerAudioProcessor::pitchSprayId
                        || id == TBShimmerAudioProcessor::freezeId
                        || id == TBShimmerAudioProcessor::sparkleId
                        || id == TBShimmerAudioProcessor::shatterId)
                        legacyStateTree.removeChild (childIndex, nullptr);
                }
                legacyStateTree.setProperty ("presetFormatVersion", 1, nullptr);
                legacyStateTree.setProperty ("stateSchemaVersion", 1, nullptr);

                if (auto encodedLegacyPreset = legacyPresetTree.createXml())
                    legacyPresetWritten = encodedLegacyPreset->writeTo (
                        legacyPreset, {});
            }

        setParameter (processor, TBShimmerAudioProcessor::levelVarId, 12.0f);
        setParameter (processor, TBShimmerAudioProcessor::positionRandId, 0.0f);
        setParameter (processor, TBShimmerAudioProcessor::pitchSprayId, 12.0f);
        setParameter (processor, TBShimmerAudioProcessor::sparkleId, 64.0f);
        setParameter (processor, TBShimmerAudioProcessor::shatterId, 48.0f);
        const auto legacyLoadResult = legacyPresetWritten
            ? processor.loadUserPreset (legacyPreset)
            : juce::Result::fail ("Legacy preset could not be written");
        auto expectedLegacyPresetValues = expectedPresetValues;
        expectedLegacyPresetValues[11] = 0.0f;
        expectedLegacyPresetValues[12] = expectedPresetValues[3];
        expectedLegacyPresetValues[13] = expectedPresetValues[3] * 0.0035f;
        expectedLegacyPresetValues[14] = 0.0f;
        expectedLegacyPresetValues[15] = 0.0f;
        expectedLegacyPresetValues[16] = 0.0f;
        check (legacyLoadResult.wasOk()
                   && approximatelyEqual (readAllParameters (processor),
                                            expectedLegacyPresetValues),
               "v1 User preset migrates atomically with the 3-1-2 mapping and Freeze/Sparkle off");

        const auto invalidPreset = userDirectory.getNonexistentChildFile (
            "TB_Shimmer_Automated_Wrong_Product", ".tbshimmer", false);
        if (directoryReady)
            invalidPreset.replaceWithText (
                "<TBShimmerPreset product=\"Not_TB_Shimmer\" "
                "presetFormatVersion=\"1\"/>");

        const auto beforeInvalid = readAllParameters (processor);
        const auto result = directoryReady
            ? processor.loadUserPreset (invalidPreset)
            : juce::Result::fail ("Temporary directory could not be created");
        check (result.failed()
                   && approximatelyEqual (readAllParameters (processor), beforeInvalid),
               "Wrong-product User preset is rejected without partial mutation");

        validPreset.deleteFile();
        legacyPreset.deleteFile();
        invalidPreset.deleteFile();
    }

    {
        std::printf ("\n[Worst-case callback timing]\n");
        constexpr auto sampleRate = 192000.0;
        constexpr auto blockSize = 32;
        constexpr auto blockCount = 4000;

        GranularShimmerEngine engine;
        engine.prepare (sampleRate, blockSize, 2);

        GranularShimmerEngine::Parameters worstCase;
        worstCase.timeMs = 1200.0f;
        worstCase.sizeMs = 500.0f;
        worstCase.density = 2000.0f;
        worstCase.scatterPercent = 100.0f;
        worstCase.levelVariationDb = 12.0f;
        worstCase.positionRandomPercent = 100.0f;
        worstCase.pitchSpraySemitones = 12.0f;
        worstCase.pitchSemitones = 24.0f;
        worstCase.reversePercent = 50.0f;
        worstCase.feedbackPercent = 88.0f;
        worstCase.toneHz = 18000.0f;
        worstCase.widthPercent = 200.0f;
        worstCase.mixPercent = 100.0f;
        worstCase.sparklePercent = 100.0f;
        worstCase.shatterPercent = 100.0f;

        juce::AudioBuffer<float> block (2, blockSize);
        std::vector<double> microseconds;
        microseconds.reserve (blockCount);

        for (int index = 0; index < blockCount; ++index)
        {
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                    block.setSample (channel, sample, 0.35f * std::sin (
                        juce::MathConstants<float>::twoPi * 220.0f
                        * static_cast<float> (index * blockSize + sample)
                        / static_cast<float> (sampleRate)));

            const auto start = std::chrono::steady_clock::now();
            engine.process (block, worstCase, false);
            const auto finish = std::chrono::steady_clock::now();
            microseconds.push_back (
                std::chrono::duration<double, std::micro> (finish - start).count());
        }

        std::sort (microseconds.begin(), microseconds.end());
        const auto deadline = 1.0e6 * blockSize / sampleRate;
        const auto fastest = microseconds.front();
        const auto p10 = microseconds[microseconds.size() / 10];
        const auto median = microseconds[microseconds.size() / 2];
        std::printf ("  192 kHz / 32 samples / density 2000 / size 500 ms / feedback 88%% / "
                     "sparkle 100%% / shatter 100%%\n");
        std::printf ("  deadline %.1f us; min %.1f us (%.0f%%); p10 %.1f us (%.0f%%); median %.1f us (%.0f%%)\n",
                     deadline, fastest, 100.0 * fastest / deadline,
                     p10, 100.0 * p10 / deadline,
                     median, 100.0 * median / deadline);
        std::printf ("  indicative only: this console harness is not a realtime audio thread, "
                     "so every statistic here moves with machine load. Repeat on an idle "
                     "machine and confirm in a host before trusting the numbers.\n");
        check (isFiniteAndBounded (block, 8.0001f),
               "Worst-case 192 kHz callback stays bounded while timing is recorded");
    }

    {
        std::printf ("\n[QA artifacts]\n");
        const auto projectRoot = projectRootFromSource();
        check (writeUiSnapshot (
                   projectRoot.getChildFile ("output")
                              .getChildFile ("ui-qa")
                              .getChildFile ("TB_Shimmer_UI.png")),
               "All sixteen controls share one ordered 8+8 760x430 panel snapshot");
        check (writeDefaultDemo (
                   projectRoot.getChildFile ("output")
                              .getChildFile ("audio-qa")
                              .getChildFile ("TB_Shimmer_Default_Demo.wav")),
               "Deterministic default wet demo WAV is written to output/audio-qa");

        const auto audioQa = projectRoot.getChildFile ("output").getChildFile ("audio-qa");
        check (writeSparkleDemo (audioQa.getChildFile ("TB_Shimmer_Sparkle_Off_Demo.wav"),
                                 0.0f, 0.0f)
                   && writeSparkleDemo (audioQa.getChildFile ("TB_Shimmer_Sparkle_Dust_Demo.wav"),
                                        80.0f, 0.0f)
                   && writeSparkleDemo (audioQa.getChildFile ("TB_Shimmer_Sparkle_Shatter_Demo.wav"),
                                        80.0f, 85.0f),
               "Sparkle off / dust / shatter comparison WAVs are written without clipping");
        check (writeSparkleUiSnapshot (
                   projectRoot.getChildFile ("output")
                              .getChildFile ("ui-qa")
                              .getChildFile ("TB_Shimmer_UI_Sparkle.png")),
               "Star Shower snapshot renders the sparkle fragments on the Grain Halo");
    }

    std::printf ("\nRESULT: %s (%d/%d passed)\n",
                 testsFailed == 0 ? "PASS" : "FAIL",
                 testsRun - testsFailed, testsRun);
    return testsFailed == 0 ? 0 : 1;
}
