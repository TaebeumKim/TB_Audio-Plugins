#include "../Source/PluginProcessor.h"
#include "../Source/HrtfResources.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int passed = 0;
int failed = 0;
using Signal = std::vector<std::vector<float>>;
using Layout = juce::AudioChannelSet;

void check (bool result, const std::string& name, double metric = 0.0)
{
    (result ? ++passed : ++failed);
    std::cout << (result ? "PASS " : "FAIL ") << name << " metric=" << metric << std::endl;
}

void set (NewProjectAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    if (parameter == nullptr)
        throw std::runtime_error (std::string ("Missing parameter: ") + id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

std::unique_ptr<NewProjectAudioProcessor> makeProcessor (Layout input, Layout output, double rate,
                                                       float pan = 0.0f, float front = 1.0f,
                                                       float height = 0.0f, int mode = 1)
{
    auto processor = std::make_unique<NewProjectAudioProcessor>();
    auto buses = processor->getBusesLayout();
    buses.inputBuses.set (0, input);
    buses.outputBuses.set (0, output);
    if (! processor->setBusesLayout (buses))
        throw std::runtime_error ("Required bus layout rejected");
    set (*processor, "pan", pan);
    set (*processor, "frontBack", front);
    set (*processor, "height", height);
    set (*processor, "distance", 0.0f);
    set (*processor, "room", 0.0f);
    set (*processor, "renderMode", static_cast<float> (mode));
    set (*processor, "outputTrim", 0.0f);
    processor->setRateAndBufferSizeDetails (rate, 256);
    processor->prepareToPlay (rate, 256);
    return processor;
}

Signal render (NewProjectAudioProcessor& processor, const Signal& input)
{
    const auto samples = static_cast<int> (input.front().size());
    const auto outputs = processor.getTotalNumOutputChannels();
    const auto inputs = processor.getTotalNumInputChannels();
    Signal result (static_cast<size_t> (outputs), std::vector<float> (static_cast<size_t> (samples)));
    juce::MidiBuffer midi;
    for (int start = 0; start < samples; start += 256)
    {
        const auto length = std::min (256, samples - start);
        juce::AudioBuffer<float> block (std::max (inputs, outputs), length);
        block.clear();
        for (int channel = 0; channel < inputs; ++channel)
            block.copyFrom (channel, 0, input[static_cast<size_t> (channel)].data() + start, length);
        processor.processBlock (block, midi);
        for (int channel = 0; channel < outputs; ++channel)
            std::copy_n (block.getReadPointer (channel), length,
                         result[static_cast<size_t> (channel)].begin() + start);
    }
    return result;
}

Signal impulse (Layout input, float pan, float front, float height = 0.0f,
                int activeChannel = 0, double rate = 48000.0, int mode = 1,
                Layout output = Layout::stereo())
{
    auto processor = makeProcessor (input, output, rate, pan, front, height, mode);
    Signal samples (static_cast<size_t> (input.size()), std::vector<float> (2048));
    samples[static_cast<size_t> (activeChannel)][0] = 0.25f;
    return render (*processor, samples);
}

double energy (const std::vector<float>& samples)
{
    double result = 0.0;
    for (auto sample : samples)
        result += static_cast<double> (sample) * sample;
    return result;
}

double energy (const Signal& samples)
{
    double result = 0.0;
    for (const auto& channel : samples)
        result += energy (channel);
    return result;
}

double difference (const Signal& first, const Signal& second, bool mirror = false)
{
    double result = 0.0;
    for (size_t channel = 0; channel < first.size(); ++channel)
        for (size_t sample = 0; sample < first[channel].size(); ++sample)
        {
            const auto delta = first[channel][sample] - second[mirror ? 1 - channel : channel][sample];
            result += static_cast<double> (delta) * delta;
        }
    return result;
}

size_t peakIndex (const std::vector<float>& samples)
{
    return static_cast<size_t> (std::max_element (samples.begin(), samples.end(),
        [] (float a, float b) { return std::abs (a) < std::abs (b); }) - samples.begin());
}

bool finiteAndBounded (const Signal& samples, float bound = 4.0f)
{
    for (const auto& channel : samples)
        for (auto sample : channel)
            if (! std::isfinite (sample) || std::abs (sample) > bound)
                return false;
    return true;
}

void testBusesAndState()
{
    auto processor = std::make_unique<NewProjectAudioProcessor>();
    const std::array<Layout, 4> inputs { Layout::mono(), Layout::stereo(), Layout::create5point1(), Layout::create7point1() };
    const std::array<Layout, 3> outputs { Layout::stereo(), Layout::create5point1(), Layout::create7point1() };
    for (const auto& input : inputs)
        for (const auto& output : outputs)
        {
            auto buses = processor->getBusesLayout();
            buses.inputBuses.set (0, input);
            buses.outputBuses.set (0, output);
            const auto expected = output == Layout::stereo() || input.size() <= 2 || input == output;
            check (processor->isBusesLayoutSupported (buses) == expected,
                   "bus " + std::to_string (input.size()) + "->" + std::to_string (output.size()));
        }

    const std::array<const char*, 8> ids { "pan", "frontBack", "height", "distance", "room", "distanceModel", "renderMode", "outputTrim" };
    const std::array<float, 8> values { -0.627f, -0.853f, 0.731f, 0.319f, 0.627f, 2.0f, 1.0f, -7.3f };
    check (processor->getParameters().size() == static_cast<int> (ids.size()), "public parameter count preserved");
    for (size_t index = 0; index < ids.size(); ++index)
        set (*processor, ids[index], values[index]);
    juce::MemoryBlock saved;
    processor->getStateInformation (saved);
    auto restored = std::make_unique<NewProjectAudioProcessor>();
    restored->setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    for (size_t index = 0; index < ids.size(); ++index)
    {
        const auto delta = std::abs (restored->parameters.getRawParameterValue (ids[index])->load() - values[index]);
        check (delta < 0.00001f, std::string ("state roundtrip ") + ids[index], delta);
    }
}

void testDirections()
{
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const auto suffix = " rate=" + std::to_string (static_cast<int> (rate));
        const auto left = impulse (Layout::mono(), -1.0f, 0.0f, 0.0f, 0, rate);
        const auto right = impulse (Layout::mono(), 1.0f, 0.0f, 0.0f, 0, rate);
        const auto dominance = energy (left[0]) / std::max (1.0e-15, energy (left[1]));
        check (dominance > 1.5, "mono left ear energy dominance" + suffix, dominance);
        check (energy (right[1]) > 1.5 * energy (right[0]), "mono right ear energy dominance" + suffix);
        check (peakIndex (left[0]) < peakIndex (left[1]), "mono ITD left near ear arrives first" + suffix,
               static_cast<double> (peakIndex (left[1])) - static_cast<double> (peakIndex (left[0])));
        const auto mirrorError = difference (left, right, true) / std::max (1.0e-15, energy (left));
        check (mirrorError < 1.0e-8, "mono left-right mirror null" + suffix, mirrorError);
        check (finiteAndBounded (left) && finiteAndBounded (right), "mono finite supported sample rate" + suffix);
    }
    const auto front = impulse (Layout::mono(), 0.0f, 1.0f);
    const auto rear = impulse (Layout::mono(), 0.0f, -1.0f);
    const auto high = impulse (Layout::mono(), 0.0f, 1.0f, 1.0f);
    const auto low = impulse (Layout::mono(), 0.0f, 1.0f, -1.0f);
    check (difference (front, rear) > energy (front) * 0.01, "front-rear impulse responses differ",
           difference (front, rear) / energy (front));
    check (difference (high, low) > energy (high) * 0.01, "above-below impulse responses differ",
           difference (high, low) / energy (high));
    const auto rearLeft = impulse (Layout::mono(), -1.0f, -1.0f);
    const auto rearRight = impulse (Layout::mono(), 1.0f, -1.0f);
    check (energy (rearLeft[0]) > energy (rearLeft[1]), "rear-left retains correct lateral sign");
    check (difference (rearLeft, rearRight, true) / energy (rearLeft) < 1.0e-8, "rear-left/right mirror null");
}

void testMeasuredRearResponse()
{
    juce::MemoryInputStream data (kemar_hrir_zip, kemar_hrir_zipSize, false);
    juce::ZipFile archive (data);
    juce::WavAudioFormat format;
    for (auto angle : { 0, 180 })
    {
        const auto name = "elev0/H0e" + juce::String (angle).paddedLeft ('0', 3) + "a.wav";
        const auto* entry = archive.getEntry (name);
        if (entry == nullptr)
            throw std::runtime_error ("Missing measured HRIR " + name.toStdString());
        std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (archive.createStreamForEntry (*entry), true));
        if (! reader)
            throw std::runtime_error ("Cannot decode measured HRIR");
        juce::AudioBuffer<float> measured (2, static_cast<int> (reader->lengthInSamples));
        reader->read (&measured, 0, measured.getNumSamples(), 0, true, true);
        auto hrtf = std::make_unique<HrtfRenderer>();
        hrtf->prepare (reader->sampleRate);
        check (hrtf->isReady(), "complete KEMAR grid loaded");
        Signal rendered (2, std::vector<float> (2048));
        for (size_t sample = 0; sample < rendered[0].size(); ++sample)
        {
            const float value = sample == 0 ? 0.25f : 0.0f;
            hrtf->process (&value, 1, static_cast<float> (angle), 0.0f, rendered[0][sample], rendered[1][sample]);
        }
        double dot = 0.0, measuredEnergy = 0.0;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < measured.getNumSamples(); ++sample)
            {
                const auto original = measured.getSample (channel, sample);
                dot += original * rendered[static_cast<size_t> (channel)][static_cast<size_t> (sample)];
                measuredEnergy += original * original;
            }
        // A single fitted gain permits the documented normalisation, but no filtering,
        // ear swap, time shift, discarded tail or synthetic rear response.
        const auto gain = dot / measuredEnergy;
        double residual = 0.0;
        for (int channel = 0; channel < 2; ++channel)
            for (size_t sample = 0; sample < rendered[0].size(); ++sample)
            {
                const auto expected = sample < static_cast<size_t> (measured.getNumSamples())
                                    ? gain * measured.getSample (channel, static_cast<int> (sample)) : 0.0;
                residual += std::pow (rendered[static_cast<size_t> (channel)][sample] - expected, 2.0);
            }
        check (gain > 0.0 && residual / energy (rendered) < 1.0e-12,
               "raw HRTF matches measured waveform angle=" + std::to_string (angle), residual / energy (rendered));
        const auto processed = impulse (Layout::mono(), 0.0f, angle == 0 ? 1.0f : -1.0f, 0.0f, 0, reader->sampleRate);
        check (difference (rendered, processed) / energy (rendered) < 1.0e-12,
               "processor adds no duplicate rear colouring at zero distance angle=" + std::to_string (angle),
               difference (rendered, processed) / energy (rendered));
    }
    const auto highRate = impulse (Layout::mono(), -1.0f, 0.0f, 0.0f, 0, 192000.0);
    double tailEnergy = 0.0;
    for (const auto& channel : highRate)
        for (size_t sample = 160; sample < channel.size(); ++sample)
            tailEnergy += channel[sample] * channel[sample];
    check (tailEnergy > energy (highRate) * 0.01, "192k resampling retains HRIR beyond old 160-tap limit", tailEnergy / energy (highRate));
}

void testStereoAndSurround()
{
    auto processor = makeProcessor (Layout::stereo(), Layout::stereo(), 48000.0, 0.0f, 1.0f, 0.0f, 0);
    Signal dry (2, std::vector<float> (4096));
    for (size_t index = 0; index < dry[0].size(); ++index)
    {
        dry[0][index] = 0.2f * std::sin (static_cast<float> (index) * 0.137f);
        dry[1][index] = 0.15f * std::cos (static_cast<float> (index) * 0.073f);
    }
    const auto centre = render (*processor, dry);
    const auto identityError = difference (dry, centre) / energy (dry);
    check (identityError < 1.0e-12, "ordinary stereo centre dry identity preserved", identityError);

    for (auto pan : { -1.0f, 0.0f, 1.0f })
    {
        const auto suffix = " pan=" + std::to_string (pan);
        const auto left = impulse (Layout::stereo(), pan, 1.0f, 0.0f, 0);
        const auto right = impulse (Layout::stereo(), pan, 1.0f, 0.0f, 1);
        check (energy (left) > 1.0e-6 && energy (right) > 1.0e-6, "headphone stereo both inputs retained" + suffix);
        auto antiProcessor = makeProcessor (Layout::stereo(), Layout::stereo(), 48000.0, pan);
        Signal anti (2, std::vector<float> (2048));
        anti[0][0] = 0.25f;
        anti[1][0] = -0.25f;
        const auto antiOutput = render (*antiProcessor, anti);
        check (energy (antiOutput) > 0.01 * (energy (left) + energy (right)),
               "headphone antiphase stereo not collapsed" + suffix,
               energy (antiOutput) / (energy (left) + energy (right)));
    }
    for (const auto& layout : { Layout::create5point1(), Layout::create7point1() })
    {
        for (int channel = 0; channel < layout.size(); ++channel)
        {
            const auto suffix = " " + std::to_string (layout.size()) + "ch input=" + std::to_string (channel);
            const auto result = impulse (layout, 0.0f, 1.0f, 0.0f, channel);
            check (energy (result) > 1.0e-6 && finiteAndBounded (result), "binaural channel survives" + suffix, energy (result));
            const auto automatic = impulse (layout, 0.0f, 1.0f, 0.0f, channel, 48000.0, 0);
            check (difference (result, automatic) / std::max (1.0e-15, energy (result)) < 1.0e-10,
                   "surround-to-stereo automatically binaural" + suffix);
            const auto passthrough = impulse (layout, 0.0f, 1.0f, 0.0f, channel, 48000.0, 0, layout);
            double otherEnergy = 0.0;
            for (int other = 0; other < layout.size(); ++other)
                if (other != channel)
                    otherEnergy += energy (passthrough[static_cast<size_t> (other)]);
            check (std::abs (energy (passthrough[static_cast<size_t> (channel)]) - 0.0625) < 1.0e-6 && otherEnergy < 1.0e-10,
                   "native surround identity mapping" + suffix, otherEnergy);
        }
    }
}

void testAutomationAndReset()
{
    for (auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    for (const auto moveDistance : { false, true })
    {
        auto processor = makeProcessor (Layout::create7point1(), Layout::stereo(), rate);
        juce::AudioBuffer<float> block (8, 256);
        juce::MidiBuffer midi;
        bool finite = true;
        double maximumJump = 0.0;
        int maximumJumpSample = 0;
        float beforeJump = 0.0f, afterJump = 0.0f;
        std::array<float, 2> previous {};
        // Low-frequency inputs make a large discontinuity visible while parameters cross the rear seam.
        for (int step = 0; step < 64; ++step)
        {
            const auto phase = static_cast<float> (step) / 63.0f;
            set (*processor, "pan", 2.0f * phase - 1.0f);
            set (*processor, "frontBack", -1.0f);
            set (*processor, "height", 2.0f * phase - 1.0f);
            set (*processor, "distance", moveDistance ? phase : 0.0f);
            for (int channel = 0; channel < 8; ++channel)
                for (int sample = 0; sample < 256; ++sample)
                    block.setSample (channel, sample, 0.01f * std::sin (static_cast<float> (step * 256 + sample)
                        * static_cast<float> (juce::MathConstants<double>::twoPi * (180.0 + channel * 20.0) / rate)));
            processor->processBlock (block, midi);
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < 256; ++sample)
                {
                    const auto value = block.getSample (channel, sample);
                    finite = finite && std::isfinite (value) && std::abs (value) < 0.25f;
                    const auto jump = static_cast<double> (std::abs (value - previous[static_cast<size_t> (channel)]));
                    if (jump > maximumJump)
                    {
                        maximumJump = jump;
                        maximumJumpSample = step * 256 + sample;
                        beforeJump = previous[static_cast<size_t> (channel)];
                        afterJump = value;
                    }
                    previous[static_cast<size_t> (channel)] = value;
                }
        }
        const auto suffix = " rate=" + std::to_string (static_cast<int> (rate)) + (moveDistance ? " distance-moving" : " distance-zero");
        std::cout << "MEASURE automation jump" << suffix << " sample=" << maximumJumpSample
                  << " before=" << beforeJump << " after=" << afterJump << std::endl;
        check (finite, "moving eight-source binaural finite" + suffix);
        check (maximumJump < 0.01, "moving eight-source binaural no large sample discontinuity" + suffix, maximumJump);
        processor->releaseResources();
        block.clear();
        processor->processBlock (block, midi);
        check (block.getMagnitude (0, 256) < 1.0e-9f, "reset clears all binaural histories" + suffix, block.getMagnitude (0, 256));
    }
}

void testRealtimeCost()
{
    // Loading resamples and aligns every measured direction; it must stay a
    // brief one-off when the host prepares the plug-in.
    for (auto rate : { 44100.0, 48000.0, 192000.0 })
    {
        auto processor = makeProcessor (Layout::mono(), Layout::stereo(), rate);
        const auto start = std::chrono::steady_clock::now();
        processor->prepareToPlay (rate, 256);
        const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        const auto label = "prepare seconds rate=" + std::to_string (static_cast<int> (rate));
        if (rate == 48000.0)
            check (seconds < 0.5, label, seconds);
        else
            std::cout << "MEASURE " << label << " metric=" << seconds << std::endl;
    }

    for (auto rate : { 48000.0, 192000.0 })
    for (const auto moving : { false, true })
    {
    auto processor = makeProcessor (Layout::create7point1(), Layout::stereo(), rate);
    juce::AudioBuffer<float> block (8, 256);
    juce::MidiBuffer midi;
    const auto run = [&]
    {
        for (int channel = 0; channel < 8; ++channel)
            for (int sample = 0; sample < 256; ++sample)
                block.setSample (channel, sample, 0.01f * std::sin (static_cast<float> (sample) * (0.01f + 0.004f * channel)));
        processor->processBlock (block, midi);
    };
    for (int warmup = 0; warmup < 8; ++warmup)
        run();
    constexpr int blocks = 188;
    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < blocks; ++index)
    {
        if (moving)
            set (*processor, "pan", static_cast<float> (index) / static_cast<float> (blocks) * 2.0f - 1.0f);
        run();
    }
    const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto realTimeRatio = seconds / (blocks * 256.0 / rate);
    const auto label = "eight-source CPU real-time ratio rate=" + std::to_string (static_cast<int> (rate))
                     + (moving ? " moving" : " steady");
    if (rate == 48000.0)
        check (realTimeRatio < 1.0, label, realTimeRatio);
    else
        std::cout << "MEASURE " << label << " metric=" << realTimeRatio << std::endl;
    }
}

void testDistanceBypassTransition()
{
    auto processor = makeProcessor (Layout::mono(), Layout::stereo(), 48000.0, 0.0f, 1.0f, 0.0f, 0);
    juce::AudioBuffer<float> block (2, 256);
    juce::MidiBuffer midi;
    float previousSample = 0.0f;
    double previousEnvelope = 0.0, maximumEnvelopeStep = 0.0;
    for (int step = 0; step < 64; ++step)
    {
        if (step == 8) set (*processor, "distance", 0.002f);
        if (step == 32) set (*processor, "distance", 0.0f);
        block.clear();
        for (int sample = 0; sample < 256; ++sample)
            block.setSample (0, sample, 0.05f * static_cast<float> (std::sin (
                (step * 256 + sample) * juce::MathConstants<double>::halfPi + 0.2)));
        processor->processBlock (block, midi);
        for (int sample = 0; sample < 256; ++sample)
        {
            const auto value = block.getSample (0, sample);
            // At 48 kHz / 4 the adjacent samples are quadrature. Their norm is
            // the exact steady sine envelope, exposing a bypass switch click
            // without mistaking the normal 12 kHz sample slope for a defect.
            const auto envelope = std::hypot (static_cast<double> (value), static_cast<double> (previousSample));
            if (step >= 4)
                maximumEnvelopeStep = std::max (maximumEnvelopeStep, std::abs (envelope - previousEnvelope));
            previousSample = value;
            previousEnvelope = envelope;
        }
    }
    check (maximumEnvelopeStep < 0.002, "12k sine distance on/off has no abrupt envelope step", maximumEnvelopeStep);
}

void testLowRateStability()
{
    for (auto rate : { 22050.0, 24000.0, 32000.0 })
        for (const auto& output : { Layout::stereo(), Layout::create5point1() })
            for (auto mode : { 0, 1 })
            for (const auto distantRear : { false, true })
            {
                auto processor = makeProcessor (Layout::mono(), output, rate, 0.0f,
                                                distantRear ? -1.0f : 1.0f, distantRear ? 1.0f : 0.0f, mode);
                if (distantRear)
                {
                    set (*processor, "distance", 0.8f);
                    processor->prepareToPlay (rate, 256);
                }
                Signal input (1, std::vector<float> (8192));
                input[0][0] = 0.1f;
                const auto result = render (*processor, input);
                check (finiteAndBounded (result, 1.0f),
                       "low-rate output finite rate=" + std::to_string (static_cast<int> (rate))
                           + " outputs=" + std::to_string (output.size()) + " mode=" + std::to_string (mode)
                           + (distantRear ? " distant-rear-high" : " near-front-neutral"));
            }
}

void testModeRoundtripDoesNotReplayHistory()
{
    auto processor = makeProcessor (Layout::mono(), Layout::stereo(), 48000.0);
    // Stop the binaural path immediately after an impulse enters its history.
    // The next switch occurs long after the plugin's advertised 100 ms tail.
    render (*processor, Signal (1, std::vector<float> (1, 0.1f)));
    set (*processor, "renderMode", 0.0f);
    render (*processor, Signal (1, std::vector<float> (9600)));
    set (*processor, "renderMode", 1.0f);
    const auto resumed = render (*processor, Signal (1, std::vector<float> (2048)));
    check (energy (resumed) < 1.0e-15, "headphone re-entry does not replay expired input history", energy (resumed));
}

void testHeightAutomationContinuity()
{
    for (const auto& output : { Layout::stereo(), Layout::create5point1() })
    {
        auto processor = makeProcessor (Layout::mono(), output, 48000.0, 0.0f, 1.0f, 0.0f, 0);
        juce::AudioBuffer<float> block (output.size(), 256);
        juce::MidiBuffer midi;
        std::vector<float> previous (static_cast<size_t> (output.size()));
        double maximumJump = 0.0;
        for (int step = 0; step < 64; ++step)
        {
            if (step == 8) set (*processor, "height", 1.0f);
            if (step == 24) set (*processor, "height", -1.0f);
            if (step == 40) set (*processor, "height", 0.0f);
            block.clear();
            for (int sample = 0; sample < block.getNumSamples(); ++sample)
                block.setSample (0, sample, 0.05f);
            processor->processBlock (block, midi);
            for (int channel = 0; channel < output.size(); ++channel)
                for (int sample = 0; sample < block.getNumSamples(); ++sample)
                {
                    const auto value = block.getSample (channel, sample);
                    if (step >= 4)
                        maximumJump = std::max (maximumJump, static_cast<double> (std::abs (value - previous[static_cast<size_t> (channel)])));
                    previous[static_cast<size_t> (channel)] = value;
                }
        }
        // A high shelf has unity DC gain: moving Height should not create a
        // click on constant input. 0.0005 is 1% of the 0.05 test input amplitude.
        check (maximumJump < 0.0005, "height automation has no DC click outputs=" + std::to_string (output.size()), maximumJump);
    }
}

void testStaticHeightResponsePreserved()
{
    for (const auto& output : { Layout::stereo(), Layout::create5point1() })
        for (auto height : { -1.0f, -0.5f, 0.5f, 1.0f })
        {
            auto processor = makeProcessor (Layout::mono(), output, 48000.0, 0.0f, 1.0f, height, 0);
            Signal input (1, std::vector<float> (2048));
            input[0][0] = 0.1f;
            const auto result = render (*processor, input);
            juce::dsp::IIR::Filter<float> reference (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                48000.0, 4000.0f, 0.7071f, juce::Decibels::decibelsToGain (height * 2.0f)));
            const auto gain = output == Layout::stereo() ? std::sqrt (0.5f) : 1.0f;
            const auto channel = output == Layout::stereo() ? 0 : output.getChannelIndexForType (Layout::centre);
            double error = 0.0;
            for (size_t sample = 0; sample < input[0].size(); ++sample)
            {
                const auto expected = reference.processSample (gain * input[0][sample]);
                error += std::pow (result[static_cast<size_t> (channel)][sample] - expected, 2.0);
            }
            check (error < 1.0e-12, "static height shelf unchanged outputs=" + std::to_string (output.size())
                   + " height=" + std::to_string (height), error);
        }
}

void testFilterAutomationBlockIndependence()
{
    const auto run = [] (int blockSize)
    {
        auto processor = makeProcessor (Layout::mono(), Layout::stereo(), 48000.0, 0.0f, 1.0f, 0.0f, 0);
        Signal result (2, std::vector<float> (8192));
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;
        for (int start = 0; start < 8192; start += blockSize)
        {
            if (start == 1024)
            {
                set (*processor, "height", 1.0f);
                set (*processor, "distance", 0.8f);
                set (*processor, "frontBack", -1.0f);
            }
            if (start == 4096)
            {
                set (*processor, "height", -1.0f);
                set (*processor, "distance", 0.1f);
                set (*processor, "frontBack", 1.0f);
            }
            block.clear();
            for (int sample = 0; sample < blockSize; ++sample)
                block.setSample (0, sample, 0.05f * std::sin (static_cast<float> (start + sample) * 0.371f));
            processor->processBlock (block, midi);
            for (int channel = 0; channel < 2; ++channel)
                std::copy_n (block.getReadPointer (channel), blockSize, result[static_cast<size_t> (channel)].begin() + start);
        }
        return result;
    };
    const auto small = run (64);
    const auto large = run (256);
    const auto error = difference (small, large) / energy (large);
    check (error < 1.0e-12, "height distance and rear automation independent of block size", error);
}

// ---- Listener-relative 3D placement ---------------------------------------

Signal rawHrtf (double rate, float azimuth, float elevation)
{
    auto hrtf = std::make_unique<HrtfRenderer>();
    hrtf->prepare (rate);
    Signal rendered (2, std::vector<float> (2048));
    for (size_t sample = 0; sample < rendered[0].size(); ++sample)
    {
        const float value = sample == 0 ? 0.25f : 0.0f;
        hrtf->process (&value, 1, azimuth, elevation, rendered[0][sample], rendered[1][sample]);
    }
    return rendered;
}

std::vector<float> magnitudeSpectrum (const std::vector<float>& samples)
{
    juce::dsp::FFT fft (11);
    std::vector<float> data (4096, 0.0f);
    std::copy_n (samples.begin(), std::min<size_t> (2048, samples.size()), data.begin());
    fft.performFrequencyOnlyForwardTransform (data.data());
    data.resize (1025);
    return data;
}

void testStereoDirectionalPanning()
{
    const auto levels = [] (float pan, float front)
    {
        const auto result = impulse (Layout::mono(), pan, front, 0.0f, 0, 48000.0, 0);
        return std::array<double, 2> { energy (result[0]), energy (result[1]) };
    };

    // The default front line keeps the released pan law, so existing sessions
    // that only automate Pan are not re-balanced.
    for (auto pan : { -1.0f, -0.5f, 0.0f, 0.25f, 0.5f, 1.0f })
    {
        const auto result = impulse (Layout::mono(), pan, 1.0f, 0.0f, 0, 48000.0, 0);
        const auto expectedLeft = 0.25 * std::sqrt (0.5 * (1.0 - pan));
        const auto expectedRight = 0.25 * std::sqrt (0.5 * (1.0 + pan));
        const auto error = std::max (std::abs (result[0][0] - expectedLeft), std::abs (result[1][0] - expectedRight));
        check (error < 1.0e-6, "stereo front-line pan law preserved pan=" + std::to_string (pan), error);
    }

    // A source beside the listener is beside the listener at any radius
    // outside the near field, as in a game-engine 3D panner.
    for (const auto position : { std::array<float, 2> { 0.3f, 0.0f }, std::array<float, 2> { 0.6f, 0.1f },
                                 std::array<float, 2> { 1.0f, -0.4f }, std::array<float, 2> { 0.45f, 0.45f } })
    {
        const auto suffix = " pan=" + std::to_string (position[0]) + " front=" + std::to_string (position[1]);
        const auto right = levels (position[0], position[1]);
        const auto left = levels (-position[0], position[1]);
        check (right[0] < 1.0e-6 * right[1], "stereo source right of listener is fully right" + suffix, right[0] / right[1]);
        check (left[1] < 1.0e-6 * left[0], "stereo source left of listener is fully left" + suffix, left[1] / left[0]);
    }

    const auto rearCentre = levels (0.0f, -0.6f);
    check (std::abs (rearCentre[0] / rearCentre[1] - 1.0) < 1.0e-6, "stereo source behind listener is centred",
           rearCentre[0] / rearCentre[1]);
    const auto frontRight = levels (0.5f, 1.0f);
    const auto rearRight = levels (0.5f, -1.0f);
    check (std::abs (rearRight[1] / rearRight[0] - frontRight[1] / frontRight[0]) < 0.01,
           "stereo rear arc folds onto the front arc", rearRight[1] / rearRight[0]);

    double worstPowerError = 0.0;
    for (const auto position : { std::array<float, 2> { 0.0f, 0.0f }, std::array<float, 2> { 0.05f, 0.05f },
                                 std::array<float, 2> { 0.1f, 0.0f }, std::array<float, 2> { 0.3f, 0.0f },
                                 std::array<float, 2> { 0.2f, 0.9f }, std::array<float, 2> { -0.6f, 0.1f },
                                 std::array<float, 2> { 1.0f, 1.0f } })
    {
        const auto level = levels (position[0], position[1]);
        worstPowerError = std::max (worstPowerError, std::abs (level[0] + level[1] - 0.0625));
    }
    check (worstPowerError < 1.0e-6, "stereo placement is constant power in the front half-plane", worstPowerError);

    const auto centre = levels (0.0f, 0.0f);
    check (std::abs (centre[0] / centre[1] - 1.0) < 1.0e-6, "stereo source at the listener is centred", centre[0] / centre[1]);
    const auto nearCentre = levels (0.02f, 0.0f);
    check (nearCentre[1] / nearCentre[0] < 1.2, "stereo near field has no jump beside the listener", nearCentre[1] / nearCentre[0]);

    bool monotonic = true;
    double previousRight = 0.0;
    for (int degrees = 0; degrees <= 90; degrees += 10)
    {
        const auto radians = juce::degreesToRadians (static_cast<float> (degrees));
        const auto level = levels (0.5f * std::sin (radians), 0.5f * std::cos (radians));
        monotonic = monotonic && level[1] >= previousRight - 1.0e-9;
        previousRight = level[1];
    }
    check (monotonic && previousRight > 0.0624, "stereo orbit front to right rises to a full right image", previousRight);
}

void testSurroundPlacement()
{
    const auto layout = Layout::create5point1();
    const auto lfe = static_cast<size_t> (layout.getChannelIndexForType (Layout::LFE));
    const auto centre = impulse (Layout::mono(), 0.0f, 0.0f, 0.0f, 0, 48000.0, 0, layout);
    double minimum = 1.0, maximum = 0.0;
    for (size_t channel = 0; channel < centre.size(); ++channel)
        if (channel != lfe)
        {
            minimum = std::min (minimum, energy (centre[channel]));
            maximum = std::max (maximum, energy (centre[channel]));
        }
    check (minimum > 0.0 && maximum / std::max (1.0e-15, minimum) < 1.0001 && energy (centre[lfe]) < 1.0e-12,
           "surround source at the listener spreads to every speaker", maximum / std::max (1.0e-15, minimum));
    check (std::abs (energy (centre) - 0.0625) < 1.0e-6, "surround spread is constant power", energy (centre));

    const auto beside = impulse (Layout::mono(), 0.5f, 0.0f, 0.0f, 0, 48000.0, 0, layout);
    const auto right = static_cast<size_t> (layout.getChannelIndexForType (Layout::right));
    const auto rightSurround = static_cast<size_t> (layout.getChannelIndexForType (Layout::rightSurround));
    double elsewhere = 0.0;
    for (size_t channel = 0; channel < beside.size(); ++channel)
        if (channel != right && channel != rightSurround)
            elsewhere += energy (beside[channel]);
    check (elsewhere < 1.0e-10 && energy (beside[right]) > 1.0e-4 && energy (beside[rightSurround]) > 1.0e-4,
           "surround source outside the near field stays on its speaker pair", elsewhere);
    const auto nearField = impulse (Layout::mono(), 0.1f, 0.05f, 0.0f, 0, 48000.0, 0, layout);
    check (std::abs (energy (nearField) - 0.0625) < 1.0e-6, "surround near field is constant power", energy (nearField));
}

void testHeadphoneNearField()
{
    const auto centre = impulse (Layout::mono(), 0.0f, 0.0f);
    double interaural = 0.0, peak = 0.0;
    for (size_t sample = 0; sample < centre[0].size(); ++sample)
    {
        interaural += std::pow (centre[0][sample] - centre[1][sample], 2.0);
        peak = std::max (peak, static_cast<double> (centre[0][sample]) * centre[0][sample]);
    }
    check (interaural / energy (centre) < 1.0e-10, "headphone source at the listener is identical in both ears",
           interaural / energy (centre));
    check (peak / energy (centre[0]) > 0.99, "headphone source at the listener is the uncoloured input", peak / energy (centre[0]));
    check (std::abs (energy (centre) - 0.0625) < 1.0e-6, "headphone source at the listener keeps input power", energy (centre));

    // Crossing the listener must not flip the image between opposite directions.
    const auto before = impulse (Layout::mono(), 0.001f, 0.001f);
    const auto after = impulse (Layout::mono(), -0.001f, -0.001f);
    check (difference (before, after) / energy (before) < 1.0e-3, "headphone image is continuous through the listener",
           difference (before, after) / energy (before));

    const auto nearSide = impulse (Layout::mono(), 0.3f, 0.0f);
    const auto farSide = impulse (Layout::mono(), 1.0f, 0.0f);
    check (difference (nearSide, farSide) / energy (farSide) < 1.0e-12,
           "headphone direction is independent of radius outside the near field", difference (nearSide, farSide) / energy (farSide));

    // The in-head blend must stay aligned with the measured response rather than comb against it.
    const auto blended = impulse (Layout::mono(), 0.0f, 0.125f);
    check (energy (blended) > 0.5 * 0.0625 && energy (blended) < 1.5 * 0.0625 && finiteAndBounded (blended),
           "headphone near-field blend keeps level", energy (blended) / 0.0625);

    for (auto pan : { -1.0f, 1.0f })
    {
        const auto stereoCentre = impulse (Layout::stereo(), 0.0f, 0.0f, 0.0f, pan < 0.0f ? 0 : 1);
        const auto own = energy (stereoCentre[pan < 0.0f ? 0 : 1]);
        const auto other = energy (stereoCentre[pan < 0.0f ? 1 : 0]);
        check (other < 1.0e-10 && std::abs (own - 0.0625) < 1.0e-6,
               std::string ("headphone stereo source at the listener keeps its channel input=") + (pan < 0.0f ? "L" : "R"), other);
    }
}

Signal measuredHrir (int elevation, int azimuth)
{
    juce::MemoryInputStream data (kemar_hrir_zip, kemar_hrir_zipSize, false);
    juce::ZipFile archive (data);
    const auto name = "elev" + juce::String (elevation) + "/H" + juce::String (elevation) + "e"
                    + juce::String (azimuth).paddedLeft ('0', 3) + "a.wav";
    const auto* entry = archive.getEntry (name);
    if (entry == nullptr)
        throw std::runtime_error ("Missing measured HRIR " + name.toStdString());
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader (format.createReaderFor (archive.createStreamForEntry (*entry), true));
    if (! reader)
        throw std::runtime_error ("Cannot decode measured HRIR");
    juce::AudioBuffer<float> measured (2, static_cast<int> (reader->lengthInSamples));
    reader->read (&measured, 0, measured.getNumSamples(), 0, true, true);
    Signal result (2, std::vector<float> (2048));
    for (size_t ear = 0; ear < 2; ++ear)
        std::copy_n (measured.getReadPointer (static_cast<int> (ear)), measured.getNumSamples(), result[ear].begin());
    return result;
}

// RMS level difference in dB between two responses over 1 kHz to 14 kHz
// (44.1 kHz, 2048 points), after removing any overall gain difference.
double spectralDistance (const Signal& first, const Signal& second)
{
    double sum = 0.0, offset = 0.0;
    int bins = 0;
    std::vector<double> differences;
    for (size_t ear = 0; ear < 2; ++ear)
    {
        const auto firstSpectrum = magnitudeSpectrum (first[ear]);
        const auto secondSpectrum = magnitudeSpectrum (second[ear]);
        for (size_t bin = 46; bin <= 650; ++bin)
        {
            differences.push_back (20.0 * std::log10 (std::max (1.0e-9f, firstSpectrum[bin])
                                                      / std::max (1.0e-9f, secondSpectrum[bin])));
            offset += differences.back();
            ++bins;
        }
    }
    offset /= bins;
    for (auto difference : differences)
        sum += (difference - offset) * (difference - offset);
    return std::sqrt (sum / bins);
}

void testMeasuredElevationRows()
{
    // Every elevation row in the bundled archive is used, so a direction that
    // was measured is rendered from its own measurement rather than
    // approximated from rows 20 degrees apart.
    struct Direction { int elevation, azimuth; };
    for (const auto direction : { Direction { -30, 60 }, Direction { -10, 90 }, Direction { 10, 30 }, Direction { 10, 120 },
                                  Direction { 30, 90 }, Direction { 50, 40 }, Direction { 60, 90 }, Direction { 70, 135 },
                                  Direction { 80, 30 }, Direction { 90, 0 } })
    {
        const auto rendered = rawHrtf (44100.0, static_cast<float> (direction.azimuth), static_cast<float> (direction.elevation));
        const auto measured = measuredHrir (direction.elevation, direction.azimuth);
        const auto distance = spectralDistance (rendered, measured);
        check (distance < 0.01, "measured direction rendered from its own measurement el=" + std::to_string (direction.elevation)
                                    + " az=" + std::to_string (direction.azimuth), distance);
    }

    // Left of the listener mirrors the measured right-hand response.
    const auto right = rawHrtf (44100.0, 40.0f, 50.0f);
    const auto left = rawHrtf (44100.0, -40.0f, 50.0f);
    check (difference (left, right, true) / energy (right) < 1.0e-12, "elevated left mirrors elevated right",
           difference (left, right, true) / energy (right));

    // The +50 degree row stops at 176 degrees; directly behind lies between
    // that measurement and its mirror image, so both ears must match.
    const auto rear = rawHrtf (44100.0, 180.0f, 50.0f);
    Signal swapped { rear[1], rear[0] };
    check (difference (rear, swapped) / energy (rear) < 1.0e-10, "elevated rear seam is symmetric",
           difference (rear, swapped) / energy (rear));
}

void testOverheadPlacement()
{
    // Above the listener is overhead, whichever way the plane was last facing.
    const auto zenith = rawHrtf (44100.0, 0.0f, 90.0f);
    const auto overhead = impulse (Layout::mono(), 0.0f, 0.0f, 1.0f, 0, 44100.0);
    check (difference (overhead, zenith) / energy (zenith) < 1.0e-12, "source above the listener uses the overhead measurement",
           difference (overhead, zenith) / energy (zenith));
    const auto justAhead = impulse (Layout::mono(), 0.0f, 0.02f, 1.0f, 0, 44100.0);
    const auto justBehind = impulse (Layout::mono(), 0.0f, -0.02f, 1.0f, 0, 44100.0);
    check (difference (justAhead, justBehind) / energy (justAhead) < 0.01, "passing over the listener is continuous",
           difference (justAhead, justBehind) / energy (justAhead));
    const auto horizon = impulse (Layout::mono(), 0.0f, 1.0f, 0.0f, 0, 44100.0);
    check (difference (overhead, horizon) / energy (horizon) > 0.05, "overhead differs from the horizon",
           difference (overhead, horizon) / energy (horizon));

    // Raising an object that sits out on the plane tilts it up by the angle it
    // is seen at, not by a fixed scale: Height 1 at radius 1 is 45 degrees.
    const auto raised = impulse (Layout::mono(), 1.0f, 0.0f, 1.0f, 0, 44100.0);
    const auto expected = rawHrtf (44100.0, 90.0f, 45.0f);
    check (difference (raised, expected) / energy (expected) < 1.0e-9, "height follows the viewing angle",
           difference (raised, expected) / energy (expected));

    // Speakers have no height channel: an overhead object is centred.
    const auto stereoOverhead = impulse (Layout::mono(), 0.0f, 0.0f, 1.0f, 0, 48000.0, 0);
    check (std::abs (energy (stereoOverhead[0]) / energy (stereoOverhead[1]) - 1.0) < 1.0e-6,
           "stereo object above the listener is centred", energy (stereoOverhead[0]) / energy (stereoOverhead[1]));
}

void testInterpolationQuality()
{
    // Halfway between two measured directions the response should lie between
    // its neighbours. Summing responses that arrive a few samples apart
    // instead notches the high frequencies, which shows up as distance from
    // the neighbours' mean level spectrum. Every gap on three rows is checked,
    // because single directions vary too much to stand for the whole circle.
    auto hrtf = std::make_unique<HrtfRenderer>();
    hrtf->prepare (44100.0);
    const auto spectra = [&hrtf] (float azimuth, float elevation)
    {
        hrtf->reset();
        Signal rendered (2, std::vector<float> (2048));
        for (size_t sample = 0; sample < rendered[0].size(); ++sample)
        {
            const float value = sample == 0 ? 0.25f : 0.0f;
            hrtf->process (&value, 1, azimuth, elevation, rendered[0][sample], rendered[1][sample]);
        }
        return Signal { magnitudeSpectrum (rendered[0]), magnitudeSpectrum (rendered[1]) };
    };

    double total = 0.0, worst = 0.0;
    int gaps = 0;
    for (auto elevation : { -20.0f, 0.0f, 20.0f })
    {
        auto first = spectra (0.0f, elevation);
        for (int azimuth = 0; azimuth < 180; azimuth += 5)
        {
            const auto second = spectra (static_cast<float> (azimuth + 5), elevation);
            const auto middle = spectra (static_cast<float> (azimuth) + 2.5f, elevation);
            double sum = 0.0;
            int bins = 0;
            for (size_t ear = 0; ear < 2; ++ear)
                for (size_t bin = 46; bin <= 650; ++bin) // 1 kHz to 14 kHz
                {
                    const auto reference = std::sqrt (std::max (1.0e-9f, first[ear][bin]) * std::max (1.0e-9f, second[ear][bin]));
                    const auto deviation = 20.0 * std::log10 (std::max (1.0e-9f, middle[ear][bin]) / reference);
                    sum += deviation * deviation;
                    ++bins;
                }
            const auto distance = std::sqrt (sum / bins);
            total += distance;
            worst = std::max (worst, distance);
            ++gaps;
            first = second;
        }
    }
    // The released renderer measured 1.72 dB mean and 4.15 dB worst here.
    check (total / gaps < 1.0, "interpolated HRTF mean distance from neighbours dB over "
                                   + std::to_string (gaps) + " gaps", total / gaps);
    check (worst < 4.0, "interpolated HRTF worst gap distance from neighbours dB", worst);
}

void testSampleRateConsistency()
{
    const auto sineGain = [] (double rate, double frequency)
    {
        auto processor = makeProcessor (Layout::mono(), Layout::stereo(), rate);
        const auto samples = static_cast<size_t> (rate * 0.2);
        Signal input (1, std::vector<float> (samples));
        for (size_t sample = 0; sample < samples; ++sample)
            input[0][sample] = 0.1f * static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * frequency
                                                                    * static_cast<double> (sample) / rate));
        const auto output = render (*processor, input);
        double in = 0.0, out = 0.0;
        for (size_t sample = samples / 2; sample < samples; ++sample)
        {
            in += static_cast<double> (input[0][sample]) * input[0][sample];
            out += static_cast<double> (output[0][sample]) * output[0][sample];
        }
        return std::sqrt (out / in);
    };

    const auto referenceLow = sineGain (44100.0, 1000.0);
    const auto referenceTilt = sineGain (44100.0, 12000.0) / referenceLow;
    for (auto rate : { 48000.0, 96000.0, 192000.0 })
    {
        const auto suffix = " rate=" + std::to_string (static_cast<int> (rate));
        const auto low = sineGain (rate, 1000.0);
        const auto levelError = 20.0 * std::log10 (low / referenceLow);
        check (std::abs (levelError) < 0.1, "headphone level matches 44.1k dB" + suffix, levelError);
        const auto tiltError = 20.0 * std::log10 (sineGain (rate, 12000.0) / low / referenceTilt);
        check (std::abs (tiltError) < 0.5, "headphone 12k cue level matches 44.1k dB" + suffix, tiltError);
    }
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    juce::ScopedNoDenormals noDenormals;
    try
    {
        testBusesAndState();
        testDirections();
        testMeasuredRearResponse();
        testStereoAndSurround();
        testAutomationAndReset();
        testDistanceBypassTransition();
        testLowRateStability();
        testModeRoundtripDoesNotReplayHistory();
        testHeightAutomationContinuity();
        testStaticHeightResponsePreserved();
        testFilterAutomationBlockIndependence();
        testStereoDirectionalPanning();
        testSurroundPlacement();
        testHeadphoneNearField();
        testMeasuredElevationRows();
        testOverheadPlacement();
        testInterpolationQuality();
        testSampleRateConsistency();
        testRealtimeCost();
    }
    catch (const std::exception& error)
    {
        check (false, std::string ("uncaught test setup failure: ") + error.what());
    }
    std::cout << "RESULT " << (failed == 0 ? "PASS" : "FAIL") << " passed=" << passed << " failed=" << failed << std::endl;
    return failed == 0 ? 0 : 1;
}
