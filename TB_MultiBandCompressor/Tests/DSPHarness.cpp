#include <JuceHeader.h>

#include "../Source/MultibandEngine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
constexpr double testSampleRate = 48000.0;

struct TestRunner
{
    void expect (bool condition, const juce::String& name,
                 const juce::String& detail = {})
    {
        ++checks;
        if (! condition)
            ++failures;

        std::cout << (condition ? "  [PASS] " : "  [FAIL] ") << name;
        if (detail.isNotEmpty())
            std::cout << "   " << detail;
        std::cout << std::endl;
    }

    void section (const juce::String& name)
    {
        std::cout << std::endl << "== " << name << " ==" << std::endl;
    }

    int checks = 0;
    int failures = 0;
};

void disableAllBands (MultibandEngine::Settings& settings)
{
    for (auto& band : settings.bands)
    {
        band.enabled = false;
        band.audition = false;
    }
}

void configureTestBand (MultibandEngine::BandSettings& band,
                        float frequencyHz = 1000.0f)
{
    band.enabled = true;
    band.frequencyHz = frequencyHz;
    band.bandwidthOctaves = 1.0f;
    band.thresholdDb = -24.0f;
    band.rangeDb = -12.0f;
    band.ratio = 4.0f;
    band.kneeDb = 0.0f;
    band.attackMs = 0.5f;
    band.releaseMs = 20.0f;
    band.outputDb = 0.0f;
    band.dynamicsMode = MultibandEngine::DynamicsMode::compress;
    band.detectorSource = MultibandEngine::DetectorSource::internal;
    band.detectorMode = MultibandEngine::DetectorMode::band;
    band.detectorFrequencyHz = frequencyHz;
    band.detectorBandwidthOctaves = 1.0f;
    band.stereoLink = 1.0f;
    band.audition = false;
}

void fillSine (juce::AudioBuffer<float>& buffer, double frequencyHz,
               const std::array<float, 2>& amplitudes, double sampleRate)
{
    const auto phaseIncrement = juce::MathConstants<double>::twoPi
                                * frequencyHz / sampleRate;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto amplitude = amplitudes[static_cast<std::size_t> (
            juce::jmin (channel, 1))];
        auto* output = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            output[sample] = amplitude * static_cast<float> (
                std::sin (phaseIncrement * static_cast<double> (sample)));
    }
}

void fillSine (juce::AudioBuffer<float>& buffer, double frequencyHz,
               float amplitude, double sampleRate)
{
    fillSine (buffer, frequencyHz, { amplitude, amplitude }, sampleRate);
}

void addSine (juce::AudioBuffer<float>& buffer, double frequencyHz,
              float amplitude, double sampleRate)
{
    const auto phaseIncrement = juce::MathConstants<double>::twoPi
                                * frequencyHz / sampleRate;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* output = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            output[sample] += amplitude * static_cast<float> (
                std::sin (phaseIncrement * static_cast<double> (sample)));
    }
}

void fillNoise (juce::AudioBuffer<float>& buffer, float amplitude, int seed)
{
    juce::Random random (seed);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* output = buffer.getWritePointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            output[sample] = amplitude * (2.0f * random.nextFloat() - 1.0f);
    }
}

double maximumDifference (const juce::AudioBuffer<float>& first,
                           const juce::AudioBuffer<float>& second)
{
    auto result = 0.0;
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = 0; sample < first.getNumSamples(); ++sample)
            result = std::max (result, std::abs (
                static_cast<double> (first.getSample (channel, sample))
                - static_cast<double> (second.getSample (channel, sample))));
    return result;
}

double maximumDifference (const juce::AudioBuffer<float>& first,
                          const juce::AudioBuffer<float>& second,
                          int startSample, int numSamples)
{
    auto result = 0.0;
    const auto endSample = juce::jmin (
        first.getNumSamples(), startSample + numSamples);
    for (int channel = 0; channel < first.getNumChannels(); ++channel)
        for (int sample = startSample; sample < endSample; ++sample)
            result = std::max (result, std::abs (
                static_cast<double> (first.getSample (channel, sample))
                - static_cast<double> (second.getSample (channel, sample))));
    return result;
}

double maximumDeltaDifference (const juce::AudioBuffer<float>& wet,
                               const juce::AudioBuffer<float>& dry,
                               const juce::AudioBuffer<float>& delta,
                               int startSample, int numSamples)
{
    auto result = 0.0;
    const auto endSample = juce::jmin (
        wet.getNumSamples(), startSample + numSamples);
    for (int channel = 0; channel < wet.getNumChannels(); ++channel)
        for (int sample = startSample; sample < endSample; ++sample)
            result = std::max (result, std::abs (
                static_cast<double> (wet.getSample (channel, sample))
                - static_cast<double> (dry.getSample (channel, sample))
                - static_cast<double> (delta.getSample (channel, sample))));
    return result;
}

double maximumMagnitude (const juce::AudioBuffer<float>& buffer)
{
    auto result = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            result = std::max (result, std::abs (
                static_cast<double> (buffer.getSample (channel, sample))));
    return result;
}

double maximumStep (const juce::AudioBuffer<float>& buffer, int channel,
                    int startSample, int numSamples)
{
    auto result = 0.0;
    const auto first = juce::jmax (1, startSample);
    const auto end = juce::jmin (buffer.getNumSamples(), startSample + numSamples);
    for (int sample = first; sample < end; ++sample)
        result = std::max (result, std::abs (
            static_cast<double> (buffer.getSample (channel, sample))
            - static_cast<double> (buffer.getSample (channel, sample - 1))));
    return result;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;
    return true;
}

double toneAmplitude (const juce::AudioBuffer<float>& buffer, int channel,
                      int startSample, int numSamples, double frequencyHz,
                      double sampleRate)
{
    auto real = 0.0;
    auto imaginary = 0.0;
    const auto phaseIncrement = juce::MathConstants<double>::twoPi
                                * frequencyHz / sampleRate;
    for (int offset = 0; offset < numSamples; ++offset)
    {
        const auto sample = startSample + offset;
        const auto phase = phaseIncrement * static_cast<double> (sample);
        const auto value = static_cast<double> (buffer.getSample (channel, sample));
        real += value * std::cos (phase);
        imaginary -= value * std::sin (phase);
    }

    return 2.0 * std::sqrt (real * real + imaginary * imaginary)
           / static_cast<double> (juce::jmax (1, numSamples));
}

juce::AudioBuffer<float> renderAuditionTone (double frequencyHz,
                                              double bandFrequencyHz)
{
    constexpr int sampleCount = 48000;
    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 1);

    MultibandEngine::Settings settings;
    disableAllBands (settings);
    auto& band = settings.bands[0];
    configureTestBand (band, static_cast<float> (bandFrequencyHz));
    band.rangeDb = 0.0f;
    band.audition = true;

    juce::AudioBuffer<float> buffer (1, sampleCount);
    fillSine (buffer, frequencyHz, 0.25f, testSampleRate);
    engine.process (buffer, nullptr, settings);
    return buffer;
}

struct SignedRangeRender
{
    float meterDb = 0.0f;
    float outputGainDb = 0.0f;
};

SignedRangeRender renderSignedRange (MultibandEngine::DynamicsMode mode,
                                     float rangeDb, float inputAmplitude)
{
    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 2);

    MultibandEngine::Settings settings;
    disableAllBands (settings);
    auto& band = settings.bands[0];
    configureTestBand (band);
    band.dynamicsMode = mode;
    band.rangeDb = rangeDb;
    band.thresholdDb = -24.0f;
    band.ratio = 4.0f;

    juce::AudioBuffer<float> buffer (2, 48000);
    fillSine (buffer, 1000.0, inputAmplitude, testSampleRate);
    engine.process (buffer, nullptr, settings);

    constexpr int analysisStart = 24000;
    constexpr int analysisCount = 24000;
    const auto outputAmplitude = toneAmplitude (
        buffer, 0, analysisStart, analysisCount, 1000.0, testSampleRate);

    return {
        engine.getGainReductionDb (0),
        juce::Decibels::gainToDecibels (
            static_cast<float> (outputAmplitude)
                / std::max (inputAmplitude, 1.0e-12f),
            -120.0f)
    };
}

struct SidechainRender
{
    float externalBandGainDb = 0.0f;
    float internalBandGainDb = 0.0f;
    juce::AudioBuffer<float> output { 2, 1 };
};

SidechainRender renderIndependentSidechains (float externalAmplitude)
{
    constexpr int sampleCount = 48000;
    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 2);

    MultibandEngine::Settings settings;
    disableAllBands (settings);
    configureTestBand (settings.bands[0], 500.0f);
    configureTestBand (settings.bands[1], 4000.0f);
    settings.bands[0].bandwidthOctaves = 0.5f;
    settings.bands[0].detectorSource = MultibandEngine::DetectorSource::external;
    settings.bands[1].bandwidthOctaves = 0.5f;
    settings.bands[1].detectorSource = MultibandEngine::DetectorSource::internal;

    juce::AudioBuffer<float> main (2, sampleCount);
    juce::AudioBuffer<float> external (2, sampleCount);
    fillSine (main, 4000.0, 0.5f, testSampleRate);
    fillSine (external, 500.0, externalAmplitude, testSampleRate);
    engine.process (main, &external, settings);

    SidechainRender result;
    result.externalBandGainDb = engine.getGainReductionDb (0);
    result.internalBandGainDb = engine.getGainReductionDb (1);
    result.output.makeCopyOf (main);
    return result;
}

void testNeutralNull (TestRunner& tests)
{
    tests.section ("Neutral delta null");

    juce::AudioBuffer<float> source (2, 8192);
    fillNoise (source, 0.5f, 0x51a7);

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        for (auto& band : settings.bands)
        {
            band.enabled = true;
            band.rangeDb = 0.0f;
            band.outputDb = 0.0f;
        }
        juce::AudioBuffer<float> output;
        output.makeCopyOf (source);
        engine.process (output, nullptr, settings);
        const auto error = maximumDifference (source, output);
        tests.expect (error <= 1.0e-6, "all active bands at unity null",
                      "max error " + juce::String (error, 9));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        settings.mix = 0.0f;
        juce::AudioBuffer<float> output;
        output.makeCopyOf (source);
        engine.process (output, nullptr, settings);
        const auto error = maximumDifference (source, output);
        tests.expect (error <= 1.0e-6, "Mix 0 is exact dry",
                      "max error " + juce::String (error, 9));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        settings.bypass = true;
        juce::AudioBuffer<float> output;
        output.makeCopyOf (source);
        engine.process (output, nullptr, settings);
        const auto error = maximumDifference (source, output);
        tests.expect (error == 0.0, "engine bypass is bit-exact dry",
                      "max error " + juce::String (error, 9));
    }
}

void testBandPassNormalisationAndSelectivity (TestRunner& tests)
{
    tests.section ("TPT band-pass normalisation and selectivity");

    const auto centre = renderAuditionTone (1000.0, 1000.0);
    const auto far = renderAuditionTone (100.0, 1000.0);
    constexpr int analysisStart = 24000;
    constexpr int analysisCount = 24000;
    const auto centreAmplitude = toneAmplitude (
        centre, 0, analysisStart, analysisCount, 1000.0, testSampleRate);
    const auto farAmplitude = toneAmplitude (
        far, 0, analysisStart, analysisCount, 100.0, testSampleRate);
    const auto centreGain = centreAmplitude / 0.25;

    tests.expect (std::abs (centreGain - 1.0) <= 0.015,
                  "1/Q normalisation gives 0 dB centre peak",
                  "gain " + juce::String (centreGain, 5));
    tests.expect (centreAmplitude > farAmplitude * 5.0,
                  "free band is selective around its centre",
                  "centre/far " + juce::String (centreAmplitude
                                                / std::max (farAmplitude, 1.0e-12), 2));

    MultibandEngine::Settings bandDetect;
    disableAllBands (bandDetect);
    configureTestBand (bandDetect.bands[0], 4000.0f);
    bandDetect.bands[0].rangeDb = 0.0f;

    MultibandEngine::Settings freeDetect = bandDetect;
    freeDetect.bands[0].detectorMode = MultibandEngine::DetectorMode::free;
    freeDetect.bands[0].detectorFrequencyHz = 500.0f;
    freeDetect.bands[0].detectorBandwidthOctaves = 0.5f;

    juce::AudioBuffer<float> bandInput (2, 48000);
    fillSine (bandInput, 500.0, 0.4f, testSampleRate);
    juce::AudioBuffer<float> freeInput;
    freeInput.makeCopyOf (bandInput);

    MultibandEngine bandEngine;
    MultibandEngine freeEngine;
    bandEngine.prepare (testSampleRate, 64, 2);
    freeEngine.prepare (testSampleRate, 64, 2);
    bandEngine.process (bandInput, nullptr, bandDetect);
    freeEngine.process (freeInput, nullptr, freeDetect);
    const auto detectorDifference = freeEngine.getDetectorLevelDb (0)
                                    - bandEngine.getDetectorLevelDb (0);
    tests.expect (detectorDifference > 12.0f,
                  "FREE detector is independent of processing band",
                  "level difference " + juce::String (detectorDifference, 2) + " dB");

    MultibandEngine::Settings multipleAuditions;
    disableAllBands (multipleAuditions);
    configureTestBand (multipleAuditions.bands[0], 500.0f);
    configureTestBand (multipleAuditions.bands[1], 4000.0f);
    multipleAuditions.bands[0].rangeDb = 0.0f;
    multipleAuditions.bands[1].rangeDb = 0.0f;
    multipleAuditions.bands[0].audition = true;
    multipleAuditions.bands[1].audition = true;
    auto firstAuditionOnly = multipleAuditions;
    firstAuditionOnly.bands[1].audition = false;

    juce::AudioBuffer<float> multipleOutput (2, 48000);
    fillSine (multipleOutput, 500.0, 0.2f, testSampleRate);
    addSine (multipleOutput, 4000.0, 0.2f, testSampleRate);
    juce::AudioBuffer<float> firstOnlyOutput;
    firstOnlyOutput.makeCopyOf (multipleOutput);
    MultibandEngine multipleEngine;
    MultibandEngine firstOnlyEngine;
    multipleEngine.prepare (testSampleRate, 64, 2);
    firstOnlyEngine.prepare (testSampleRate, 64, 2);
    multipleEngine.process (multipleOutput, nullptr, multipleAuditions);
    firstOnlyEngine.process (firstOnlyOutput, nullptr, firstAuditionOnly);
    const auto auditionError = maximumDifference (multipleOutput, firstOnlyOutput);
    tests.expect (auditionError == 0.0,
                  "lowest enabled Audition band deterministically wins",
                  "max error " + juce::String (auditionError, 9));
}

void testSignedRangeModes (TestRunner& tests)
{
    tests.section ("Signed Range and dynamics modes");

    const auto negativeCompression = renderSignedRange (
        MultibandEngine::DynamicsMode::compress, -4.0f, 0.5f);
    const auto positiveCompression = renderSignedRange (
        MultibandEngine::DynamicsMode::compress, 4.0f, 0.5f);
    const auto negativeExpansion = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, -4.0f, 0.01f);
    const auto positiveExpansion = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, 4.0f, 0.01f);

    const auto meterMatchesRange = [] (const SignedRangeRender& result,
                                       float rangeDb)
    {
        return std::abs (result.meterDb - rangeDb) <= 0.001f;
    };

    tests.expect (meterMatchesRange (negativeCompression, -4.0f),
                  "Compress above threshold: negative Range sets negative meter",
                  juce::String (negativeCompression.meterDb, 3) + " dB");
    tests.expect (negativeCompression.outputGainDb < -3.5f,
                  "Compress above threshold: negative Range attenuates output",
                  juce::String (negativeCompression.outputGainDb, 3) + " dB");
    tests.expect (meterMatchesRange (positiveCompression, 4.0f),
                  "Compress above threshold: positive Range sets positive meter",
                  juce::String (positiveCompression.meterDb, 3) + " dB");
    tests.expect (positiveCompression.outputGainDb > 3.5f,
                  "Compress above threshold: positive Range boosts output",
                  juce::String (positiveCompression.outputGainDb, 3) + " dB");
    tests.expect (meterMatchesRange (negativeExpansion, -4.0f),
                  "Expand below threshold: negative Range sets negative meter",
                  juce::String (negativeExpansion.meterDb, 3) + " dB");
    tests.expect (negativeExpansion.outputGainDb < -3.5f,
                  "Expand below threshold: negative Range attenuates output",
                  juce::String (negativeExpansion.outputGainDb, 3) + " dB");
    tests.expect (meterMatchesRange (positiveExpansion, 4.0f),
                  "Expand below threshold: positive Range sets positive meter",
                  juce::String (positiveExpansion.meterDb, 3) + " dB");
    tests.expect (positiveExpansion.outputGainDb > 3.5f,
                  "Expand below threshold: positive Range boosts output",
                  juce::String (positiveExpansion.outputGainDb, 3) + " dB");

    const auto negativeEndpoint = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, -48.0f, 0.0001f);
    const auto positiveEndpoint = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, 12.0f, 0.0001f);
    tests.expect (meterMatchesRange (negativeEndpoint, -48.0f),
                  "Range reaches the -48 dB attenuation endpoint",
                  juce::String (negativeEndpoint.meterDb, 3) + " dB");
    tests.expect (meterMatchesRange (positiveEndpoint, 12.0f),
                  "Range reaches the +12 dB boost endpoint",
                  juce::String (positiveEndpoint.meterDb, 3) + " dB");

    const auto belowMinimum = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, -100.0f, 0.0001f);
    const auto aboveMaximum = renderSignedRange (
        MultibandEngine::DynamicsMode::expand, 100.0f, 0.0001f);
    tests.expect (meterMatchesRange (belowMinimum, -48.0f),
                  "DSP sanitises Range below -48 dB",
                  juce::String (belowMinimum.meterDb, 3) + " dB");
    tests.expect (meterMatchesRange (aboveMaximum, 12.0f),
                  "DSP sanitises Range above +12 dB",
                  juce::String (aboveMaximum.meterDb, 3) + " dB");
}

void testSidechainContracts (TestRunner& tests)
{
    tests.section ("External sidechain contracts");

    const auto silentExternal = renderIndependentSidechains (0.0f);
    const auto hotExternal = renderIndependentSidechains (0.5f);
    tests.expect (hotExternal.externalBandGainDb < -6.0f
                      && std::abs (silentExternal.externalBandGainDb) < 0.01f,
                  "EXT drives only the opted-in band and silence is honoured",
                  "silent/hot " + juce::String (silentExternal.externalBandGainDb, 2)
                      + "/" + juce::String (hotExternal.externalBandGainDb, 2) + " dB");
    tests.expect (std::abs (hotExternal.internalBandGainDb
                            - silentExternal.internalBandGainDb) < 0.05f
                      && hotExternal.internalBandGainDb < -6.0f,
                  "INT band is independent of external key level",
                  "difference " + juce::String (
                      hotExternal.internalBandGainDb
                          - silentExternal.internalBandGainDb, 4) + " dB");

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;

        juce::AudioBuffer<float> main (2, 48000);
        juce::AudioBuffer<float> external (1, 48000);
        main.clear();
        fillSine (external, 1000.0, 0.8f, testSampleRate);
        engine.process (main, &external, settings);
        const auto leakage = maximumMagnitude (main);
        tests.expect (leakage <= 1.0e-7,
                      "external key never leaks to normal output",
                      "peak " + juce::String (leakage, 9));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;
        settings.bands[0].channelMode = MultibandEngine::ChannelMode::side;

        juce::AudioBuffer<float> main (2, 48000);
        juce::AudioBuffer<float> external (4, 48000);
        main.clear();
        external.clear();
        fillSine (external, 1000.0, 0.8f, testSampleRate);
        engine.process (main, &external, settings);
        tests.expect (engine.getGainReductionDb (0) < -6.0f,
                      "per-band EXT key drives a Side detector as a mono key",
                      juce::String (engine.getGainReductionDb (0), 2) + " dB");
    }

    {
        MultibandEngine::Settings externalSettings;
        disableAllBands (externalSettings);
        configureTestBand (externalSettings.bands[0], 1000.0f);
        externalSettings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;
        auto internalSettings = externalSettings;
        internalSettings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::internal;

        juce::AudioBuffer<float> externalFallback (2, 48000);
        fillSine (externalFallback, 1000.0, 0.5f, testSampleRate);
        juce::AudioBuffer<float> internal;
        internal.makeCopyOf (externalFallback);

        MultibandEngine externalEngine;
        MultibandEngine internalEngine;
        externalEngine.prepare (testSampleRate, 64, 2);
        internalEngine.prepare (testSampleRate, 64, 2);
        externalEngine.process (externalFallback, nullptr, externalSettings);
        internalEngine.process (internal, nullptr, internalSettings);
        const auto error = maximumDifference (externalFallback, internal);
        const auto meterError = std::abs (externalEngine.getGainReductionDb (0)
                                          - internalEngine.getGainReductionDb (0));
        tests.expect (error <= 1.0e-6 && meterError <= 1.0e-5f,
                      "nullptr EXT deterministically falls back to INT",
                      "audio/meter error " + juce::String (error, 9) + "/"
                          + juce::String (meterError, 7));
    }
}

double renderQuietChannelAmplitude (float stereoLink)
{
    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 2);
    MultibandEngine::Settings settings;
    disableAllBands (settings);
    configureTestBand (settings.bands[0]);
    settings.bands[0].stereoLink = stereoLink;

    juce::AudioBuffer<float> buffer (2, 48000);
    fillSine (buffer, 1000.0, { 0.5f, 0.01f }, testSampleRate);
    engine.process (buffer, nullptr, settings);
    return toneAmplitude (buffer, 1, 24000, 24000, 1000.0, testSampleRate);
}

void testStereoLink (TestRunner& tests)
{
    tests.section ("Stereo link");
    const auto unlinkedRight = renderQuietChannelAmplitude (0.0f);
    const auto linkedRight = renderQuietChannelAmplitude (1.0f);
    tests.expect (linkedRight < unlinkedRight * 0.4,
                  "100% link applies the hot-channel detector to both channels",
                  "linked/unlinked " + juce::String (linkedRight, 6) + "/"
                      + juce::String (unlinkedRight, 6));
}

void testSoftBypass (TestRunner& tests)
{
    tests.section ("Soft bypass");

    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 2);
    MultibandEngine::Settings settings;
    disableAllBands (settings);
    settings.outputDb = 12.0f;

    juce::AudioBuffer<float> warmup (2, 128);
    warmup.clear();
    for (int channel = 0; channel < warmup.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill (warmup.getWritePointer (channel),
                                           0.1f, warmup.getNumSamples());
    engine.process (warmup, nullptr, settings);
    const auto wetBeforeBypass = warmup.getSample (0, warmup.getNumSamples() - 1);

    settings.bypass = true;
    juce::AudioBuffer<float> bypassed (2, 1200);
    for (int channel = 0; channel < bypassed.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill (bypassed.getWritePointer (channel),
                                           0.1f, bypassed.getNumSamples());
    engine.process (bypassed, nullptr, settings);

    const auto bypassBoundary = std::abs (
        bypassed.getSample (0, 0) - wetBeforeBypass);
    const auto bypassStep = maximumStep (bypassed, 0, 0,
                                          bypassed.getNumSamples());
    const auto settledDry = bypassed.getSample (
        0, bypassed.getNumSamples() - 1);
    tests.expect (bypassBoundary < 0.005f && bypassStep < 0.005
                      && std::abs (settledDry - 0.1f) < 1.0e-6f,
                  "bypass automation crossfades to exact dry",
                  "boundary/max/final " + juce::String (bypassBoundary, 6)
                      + "/" + juce::String (bypassStep, 6) + "/"
                      + juce::String (settledDry, 6));

    settings.bypass = false;
    juce::AudioBuffer<float> active (2, 1200);
    for (int channel = 0; channel < active.getNumChannels(); ++channel)
        juce::FloatVectorOperations::fill (active.getWritePointer (channel),
                                           0.1f, active.getNumSamples());
    engine.process (active, nullptr, settings);

    const auto activeBoundary = std::abs (
        active.getSample (0, 0) - settledDry);
    const auto activeStep = maximumStep (active, 0, 0,
                                          active.getNumSamples());
    const auto settledWet = active.getSample (0, active.getNumSamples() - 1);
    tests.expect (activeBoundary < 0.005f && activeStep < 0.005
                      && std::abs (settledWet - wetBeforeBypass) < 1.0e-6f,
                  "unbypass automation crossfades back to wet",
                  "boundary/max/final " + juce::String (activeBoundary, 6)
                      + "/" + juce::String (activeStep, 6) + "/"
                      + juce::String (settledWet, 6));
}

void testAutomationSmoothing (TestRunner& tests)
{
    tests.section ("Automation and source smoothing");
    constexpr int halfSamples = 4096;
    constexpr int totalSamples = 2 * halfSamples;

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 250.0f);
        settings.bands[0].rangeDb = 0.0f;
        settings.bands[0].outputDb = -12.0f;
        settings.bands[0].bandwidthOctaves = 0.5f;

        juce::AudioBuffer<float> main (2, totalSamples);
        fillSine (main, 1000.0, 0.25f, testSampleRate);
        juce::AudioBuffer<float> first (
            main.getArrayOfWritePointers(), 2, 0, halfSamples);
        engine.process (first, nullptr, settings);

        settings.bands[0].frequencyHz = 1000.0f;
        settings.bands[0].bandwidthOctaves = 4.0f;
        juce::AudioBuffer<float> second (
            main.getArrayOfWritePointers(), 2, halfSamples, halfSamples);
        engine.process (second, nullptr, settings);

        const auto boundaryStep = std::abs (
            static_cast<double> (main.getSample (0, halfSamples))
            - static_cast<double> (main.getSample (0, halfSamples - 1)));
        const auto largestStep = maximumStep (main, 0, halfSamples,
                                              halfSamples);
        const auto settledAmplitude = toneAmplitude (
            main, 0, totalSamples - 1536, 1536, 1000.0, testSampleRate);
        tests.expect (allFinite (main) && boundaryStep < 0.08
                          && largestStep < 0.12 && settledAmplitude < 0.10,
                      "processing frequency/width automation is bounded and settles",
                      "boundary/max/settled " + juce::String (boundaryStep, 5)
                          + "/" + juce::String (largestStep, 5) + "/"
                          + juce::String (settledAmplitude, 5));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 4000.0f);
        settings.bands[0].rangeDb = 0.0f;
        settings.bands[0].audition = true;
        settings.bands[0].detectorMode = MultibandEngine::DetectorMode::free;
        settings.bands[0].detectorFrequencyHz = 250.0f;
        settings.bands[0].detectorBandwidthOctaves = 0.5f;

        juce::AudioBuffer<float> main (2, totalSamples);
        fillSine (main, 1000.0, 0.25f, testSampleRate);
        juce::AudioBuffer<float> first (
            main.getArrayOfWritePointers(), 2, 0, halfSamples);
        engine.process (first, nullptr, settings);

        settings.bands[0].detectorFrequencyHz = 1000.0f;
        settings.bands[0].detectorBandwidthOctaves = 4.0f;
        juce::AudioBuffer<float> second (
            main.getArrayOfWritePointers(), 2, halfSamples, halfSamples);
        engine.process (second, nullptr, settings);

        const auto boundaryStep = std::abs (
            static_cast<double> (main.getSample (0, halfSamples))
            - static_cast<double> (main.getSample (0, halfSamples - 1)));
        const auto largestStep = maximumStep (main, 0, halfSamples,
                                              halfSamples);
        const auto settledAmplitude = toneAmplitude (
            main, 0, totalSamples - 1536, 1536, 1000.0, testSampleRate);
        tests.expect (allFinite (main) && boundaryStep < 0.08
                          && largestStep < 0.12 && settledAmplitude > 0.22,
                      "FREE detector frequency/width automation is bounded in Audition",
                      "boundary/max/settled " + juce::String (boundaryStep, 5)
                          + "/" + juce::String (largestStep, 5) + "/"
                          + juce::String (settledAmplitude, 5));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].rangeDb = 0.0f;
        settings.bands[0].audition = true;
        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::internal;

        juce::AudioBuffer<float> main (2, totalSamples);
        juce::AudioBuffer<float> external (2, totalSamples);
        fillSine (main, 1000.0, 0.25f, testSampleRate);
        fillSine (external, 1000.0, -0.75f, testSampleRate);
        juce::AudioBuffer<float> firstMain (
            main.getArrayOfWritePointers(), 2, 0, halfSamples);
        juce::AudioBuffer<float> firstExternal (
            external.getArrayOfWritePointers(), 2, 0, halfSamples);
        engine.process (firstMain, &firstExternal, settings);

        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;
        juce::AudioBuffer<float> secondMain (
            main.getArrayOfWritePointers(), 2, halfSamples, halfSamples);
        juce::AudioBuffer<float> secondExternal (
            external.getArrayOfWritePointers(), 2, halfSamples, halfSamples);
        engine.process (secondMain, &secondExternal, settings);

        const auto boundaryStep = std::abs (
            static_cast<double> (main.getSample (0, halfSamples))
            - static_cast<double> (main.getSample (0, halfSamples - 1)));
        const auto largestStep = maximumStep (main, 0, halfSamples,
                                              halfSamples);
        const auto settledAmplitude = toneAmplitude (
            main, 0, totalSamples - 1536, 1536, 1000.0, testSampleRate);
        tests.expect (allFinite (main) && boundaryStep < 0.08
                          && largestStep < 0.20 && settledAmplitude > 0.68,
                      "INT/EXT switch is smoothed and finite in Audition",
                      "boundary/max/settled " + juce::String (boundaryStep, 5)
                          + "/" + juce::String (largestStep, 5) + "/"
                          + juce::String (settledAmplitude, 5));
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::internal;

        juce::AudioBuffer<float> main (2, totalSamples);
        juce::AudioBuffer<float> external (1, totalSamples);
        fillSine (main, 1000.0, 0.01f, testSampleRate);
        fillSine (external, 1000.0, 0.8f, testSampleRate);
        juce::AudioBuffer<float> firstMain (
            main.getArrayOfWritePointers(), 2, 0, halfSamples);
        juce::AudioBuffer<float> firstExternal (
            external.getArrayOfWritePointers(), 1, 0, halfSamples);
        engine.process (firstMain, &firstExternal, settings);

        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;
        juce::AudioBuffer<float> secondMain (
            main.getArrayOfWritePointers(), 2, halfSamples, halfSamples);
        juce::AudioBuffer<float> secondExternal (
            external.getArrayOfWritePointers(), 1, halfSamples, halfSamples);
        engine.process (secondMain, &secondExternal, settings);

        const auto largestStep = maximumStep (main, 0, halfSamples,
                                              halfSamples);
        tests.expect (allFinite (main) && largestStep < 0.01
                          && engine.getGainReductionDb (0) < -6.0f,
                      "source smoothing plus envelope bounds normal detector switch",
                      "max step/gain " + juce::String (largestStep, 6) + "/"
                          + juce::String (engine.getGainReductionDb (0), 3) + " dB");
    }
}

void testDeltaListen (TestRunner& tests)
{
    tests.section ("Transient per-band Delta Listen");

    {
        constexpr int sampleCount = 16384;
        constexpr int compareSamples = 4096;
        constexpr int compareStart = sampleCount - compareSamples;

        juce::AudioBuffer<float> source (2, sampleCount);
        fillNoise (source, 0.25f, 0x44544c54);

        MultibandEngine::Settings wetSettings;
        disableAllBands (wetSettings);
        configureTestBand (wetSettings.bands[0], 1200.0f);
        wetSettings.bands[0].bandwidthOctaves = 0.75f;
        wetSettings.bands[0].outputDb = -3.0f;
        wetSettings.outputDb = 6.0f;

        auto drySettings = wetSettings;
        disableAllBands (drySettings);
        auto deltaFullMixSettings = wetSettings;
        deltaFullMixSettings.deltaListenBand = 0;
        auto deltaZeroMixSettings = deltaFullMixSettings;
        deltaZeroMixSettings.mix = 0.0f;
        auto deltaUnityOutputSettings = deltaFullMixSettings;
        deltaUnityOutputSettings.outputDb = 0.0f;

        juce::AudioBuffer<float> wet, dry, deltaFullMix, deltaZeroMix,
            deltaUnityOutput;
        wet.makeCopyOf (source);
        dry.makeCopyOf (source);
        deltaFullMix.makeCopyOf (source);
        deltaZeroMix.makeCopyOf (source);
        deltaUnityOutput.makeCopyOf (source);
        MultibandEngine wetEngine, dryEngine, deltaFullMixEngine,
            deltaZeroMixEngine, deltaUnityOutputEngine;
        for (auto* engine : { &wetEngine, &dryEngine, &deltaFullMixEngine,
                              &deltaZeroMixEngine, &deltaUnityOutputEngine })
            engine->prepare (testSampleRate, 64, 2);
        wetEngine.process (wet, nullptr, wetSettings);
        dryEngine.process (dry, nullptr, drySettings);
        deltaFullMixEngine.process (
            deltaFullMix, nullptr, deltaFullMixSettings);
        deltaZeroMixEngine.process (
            deltaZeroMix, nullptr, deltaZeroMixSettings);
        deltaUnityOutputEngine.process (
            deltaUnityOutput, nullptr, deltaUnityOutputSettings);

        const auto deltaError = maximumDeltaDifference (
            wet, dry, deltaFullMix, compareStart, compareSamples);
        tests.expect (deltaError <= 2.0e-6,
                      "settled Dynamic Delta equals band-only wet minus aligned dry",
                      "max error " + juce::String (deltaError, 9));

        const auto mixError = maximumDifference (
            deltaFullMix, deltaZeroMix, compareStart, compareSamples);
        tests.expect (mixError <= 2.0e-6,
                      "Delta Listen is independent of global Mix after settling",
                      "max error " + juce::String (mixError, 9));

        const auto outputGain = juce::Decibels::decibelsToGain (
            wetSettings.outputDb);
        auto outputGainError = 0.0;
        for (int channel = 0; channel < deltaFullMix.getNumChannels(); ++channel)
            for (int sample = compareStart; sample < sampleCount; ++sample)
                outputGainError = std::max (
                    outputGainError,
                    std::abs (static_cast<double> (
                                  deltaFullMix.getSample (channel, sample))
                              - outputGain * static_cast<double> (
                                  deltaUnityOutput.getSample (channel, sample))));
        tests.expect (outputGainError <= 2.0e-6,
                      "Delta Listen includes non-unity hidden global Output gain",
                      "max error " + juce::String (outputGainError, 9));
    }

    {
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].thresholdDb = -36.0f;
        settings.bands[0].rangeDb = -18.0f;
        settings.bands[0].detectorSource =
            MultibandEngine::DetectorSource::external;
        settings.deltaListenBand = 0;

        juce::AudioBuffer<float> main (2, 8192);
        juce::AudioBuffer<float> external (2, 8192);
        main.clear();
        fillSine (external, 1000.0, 0.8f, testSampleRate);
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        engine.process (main, &external, settings);

        tests.expect (allFinite (main) && maximumMagnitude (main) <= 1.0e-7
                          && engine.getGainReductionDb (0) < -3.0f,
                      "Delta uses EXT only as key and never leaks it into the carrier",
                      "peak/gain " + juce::String (maximumMagnitude (main), 9)
                          + "/" + juce::String (
                              engine.getGainReductionDb (0), 3) + " dB");
    }

    {
        const auto renderDeltaDomain = [] (MultibandEngine::ChannelMode mode,
                                           bool sideInput, int channels)
        {
            MultibandEngine::Settings settings;
            disableAllBands (settings);
            configureTestBand (settings.bands[0], 1000.0f);
            settings.bands[0].rangeDb = 0.0f;
            settings.bands[0].outputDb = -12.0f;
            settings.bands[0].channelMode = mode;
            settings.deltaListenBand = 0;

            juce::AudioBuffer<float> buffer (channels, 12000);
            if (channels == 1)
                fillSine (buffer, 1000.0, 0.1f, testSampleRate);
            else
                fillSine (buffer, 1000.0,
                          sideInput ? std::array<float, 2> { 0.1f, -0.1f }
                                    : std::array<float, 2> { 0.1f, 0.1f },
                          testSampleRate);
            MultibandEngine engine;
            engine.prepare (testSampleRate, 64, channels);
            engine.process (buffer, nullptr, settings);
            return buffer;
        };

        const auto midOnSide = renderDeltaDomain (
            MultibandEngine::ChannelMode::mid, true, 2);
        const auto sideOnSide = renderDeltaDomain (
            MultibandEngine::ChannelMode::side, true, 2);
        const auto sideOnMid = renderDeltaDomain (
            MultibandEngine::ChannelMode::side, false, 2);
        const auto midOnMid = renderDeltaDomain (
            MultibandEngine::ChannelMode::mid, false, 2);
        constexpr int analysisStart = 8000;
        constexpr int analysisSamples = 4000;
        const auto midOnSideAmplitude = toneAmplitude (
            midOnSide, 0, analysisStart, analysisSamples, 1000.0,
            testSampleRate);
        const auto sideOnSideAmplitude = toneAmplitude (
            sideOnSide, 0, analysisStart, analysisSamples, 1000.0,
            testSampleRate);
        const auto sideOnMidAmplitude = toneAmplitude (
            sideOnMid, 0, analysisStart, analysisSamples, 1000.0,
            testSampleRate);
        const auto midOnMidAmplitude = toneAmplitude (
            midOnMid, 0, analysisStart, analysisSamples, 1000.0,
            testSampleRate);
        tests.expect (midOnSideAmplitude < 0.001
                          && sideOnSideAmplitude > 0.04
                          && sideOnMidAmplitude < 0.001
                          && midOnMidAmplitude > 0.04,
                      "Delta respects per-band Mid and Side carrier domains",
                      "M<-S/S<-S/S<-M/M<-M "
                          + juce::String (midOnSideAmplitude, 4) + "/"
                          + juce::String (sideOnSideAmplitude, 4) + "/"
                          + juce::String (sideOnMidAmplitude, 4) + "/"
                          + juce::String (midOnMidAmplitude, 4));

        const auto monoStereo = renderDeltaDomain (
            MultibandEngine::ChannelMode::stereo, false, 1);
        const auto monoMid = renderDeltaDomain (
            MultibandEngine::ChannelMode::mid, false, 1);
        const auto monoSide = renderDeltaDomain (
            MultibandEngine::ChannelMode::side, false, 1);
        const auto monoMidError = maximumDifference (monoStereo, monoMid);
        const auto monoSideError = maximumDifference (monoStereo, monoSide);
        tests.expect (monoMidError == 0.0 && monoSideError == 0.0,
                      "mono Delta Mid and Side selections fall back to Stereo",
                      "mid/side error " + juce::String (monoMidError, 9)
                          + "/" + juce::String (monoSideError, 9));
    }

    {
        juce::AudioBuffer<float> dry (2, 8192);
        fillNoise (dry, 0.2f, 0x4259444c);
        juce::AudioBuffer<float> bypassed;
        bypassed.makeCopyOf (dry);

        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.bands[0].outputDb = -12.0f;
        settings.outputDb = 6.0f;
        settings.deltaListenBand = 0;
        settings.bypass = true;
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        engine.process (bypassed, nullptr, settings);

        const auto error = maximumDifference (bypassed, dry);
        tests.expect (error == 0.0,
                      "bypass wins over Delta Listen and global Output",
                      "max error " + juce::String (error, 9));
    }

    {
        constexpr int segmentSamples = 4092;
        constexpr int sampleCount = 2 * segmentSamples;
        juce::AudioBuffer<float> buffer (2, sampleCount);
        fillSine (buffer, 1000.0, 0.2f, testSampleRate);

        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        configureTestBand (settings.bands[1], 8000.0f);
        for (int band = 0; band < 2; ++band)
        {
            settings.bands[static_cast<std::size_t> (band)].rangeDb = 0.0f;
            settings.bands[static_cast<std::size_t> (band)].outputDb = -12.0f;
            settings.bands[static_cast<std::size_t> (band)].bandwidthOctaves =
                0.5f;
        }

        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        settings.deltaListenBand = 0;
        juce::AudioBuffer<float> first (
            buffer.getArrayOfWritePointers(), 2, 0, segmentSamples);
        engine.process (first, nullptr, settings);
        settings.deltaListenBand = 1;
        juce::AudioBuffer<float> second (
            buffer.getArrayOfWritePointers(), 2, segmentSamples,
            segmentSamples);
        engine.process (second, nullptr, settings);

        const auto boundaryStep = std::abs (
            static_cast<double> (buffer.getSample (0, segmentSamples))
            - static_cast<double> (buffer.getSample (0, segmentSamples - 1)));
        const auto largestStep = maximumStep (buffer, 0, 1, sampleCount - 1);
        const auto firstAmplitude = toneAmplitude (
            buffer, 0, segmentSamples - 2048, 2048, 1000.0,
            testSampleRate);
        const auto secondAmplitude = toneAmplitude (
            buffer, 0, sampleCount - 2048, 2048, 1000.0,
            testSampleRate);
        tests.expect (allFinite (buffer) && boundaryStep < 0.03
                          && largestStep < 0.08
                          && firstAmplitude > secondAmplitude * 5.0,
                      "band-to-band Delta transfer is finite, smoothed, and exclusive",
                      "boundary/max/amplitude ratio "
                          + juce::String (boundaryStep, 6) + "/"
                          + juce::String (largestStep, 6) + "/"
                          + juce::String (firstAmplitude
                                             / juce::jmax (1.0e-12,
                                                           secondAmplitude),
                                         2));
    }

    {
        constexpr int segmentSamples = 8192;
        constexpr int sampleCount = 3 * segmentSamples;
        juce::AudioBuffer<float> buffer (2, sampleCount);
        fillSine (buffer, 250.0, 0.2f, testSampleRate);
        addSine (buffer, 4000.0, 0.2f, testSampleRate);

        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 4000.0f);
        auto& band = settings.bands[0];
        band.bandwidthOctaves = 0.5f;
        band.rangeDb = 0.0f;
        band.outputDb = -12.0f;
        band.audition = true;
        band.detectorMode = MultibandEngine::DetectorMode::free;
        band.detectorFrequencyHz = 250.0f;
        band.detectorBandwidthOctaves = 0.5f;

        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        settings.deltaListenBand = -1;
        juce::AudioBuffer<float> before (
            buffer.getArrayOfWritePointers(), 2, 0, segmentSamples);
        engine.process (before, nullptr, settings);
        settings.deltaListenBand = 0;
        juce::AudioBuffer<float> during (
            buffer.getArrayOfWritePointers(), 2, segmentSamples,
            segmentSamples);
        engine.process (during, nullptr, settings);
        settings.deltaListenBand = -1;
        juce::AudioBuffer<float> after (
            buffer.getArrayOfWritePointers(), 2, 2 * segmentSamples,
            segmentSamples);
        engine.process (after, nullptr, settings);

        constexpr int analysisSamples = 2048;
        const auto detectorBefore = toneAmplitude (
            buffer, 0, segmentSamples - analysisSamples, analysisSamples,
            250.0, testSampleRate);
        const auto deltaBefore = toneAmplitude (
            buffer, 0, segmentSamples - analysisSamples, analysisSamples,
            4000.0, testSampleRate);
        const auto detectorDuring = toneAmplitude (
            buffer, 0, 2 * segmentSamples - analysisSamples,
            analysisSamples, 250.0, testSampleRate);
        const auto deltaDuring = toneAmplitude (
            buffer, 0, 2 * segmentSamples - analysisSamples,
            analysisSamples, 4000.0, testSampleRate);
        const auto detectorAfter = toneAmplitude (
            buffer, 0, sampleCount - analysisSamples, analysisSamples,
            250.0, testSampleRate);
        const auto deltaAfter = toneAmplitude (
            buffer, 0, sampleCount - analysisSamples, analysisSamples,
            4000.0, testSampleRate);
        tests.expect (allFinite (buffer)
                          && detectorBefore > deltaBefore * 5.0
                          && deltaDuring > detectorDuring * 5.0
                          && detectorAfter > deltaAfter * 5.0,
                      "Delta overrides legacy detector Listen and releases back to it",
                      "DET/DELTA before "
                          + juce::String (detectorBefore, 4) + "/"
                          + juce::String (deltaBefore, 4) + ", during "
                          + juce::String (detectorDuring, 4) + "/"
                          + juce::String (deltaDuring, 4) + ", after "
                          + juce::String (detectorAfter, 4) + "/"
                          + juce::String (deltaAfter, 4));
    }

    {
        constexpr int sampleCount = 16384;
        constexpr int compareSamples = 4096;
        constexpr int compareStart = sampleCount - compareSamples;
        juce::AudioBuffer<float> source (2, sampleCount);
        fillNoise (source, 0.2f, 0x4c38444c);

        MultibandEngine::Settings wetSettings;
        disableAllBands (wetSettings);
        configureTestBand (wetSettings.bands[0], 3000.0f);
        wetSettings.bands[0].outputDb = -6.0f;
        wetSettings.phaseMode = MultibandEngine::PhaseMode::linear;
        wetSettings.oversamplingMode =
            MultibandEngine::OversamplingMode::x8;
        wetSettings.lookaheadMs = 10.0f;
        auto drySettings = wetSettings;
        disableAllBands (drySettings);
        auto deltaSettings = wetSettings;
        deltaSettings.deltaListenBand = 0;

        juce::AudioBuffer<float> wet, dry, delta;
        wet.makeCopyOf (source);
        dry.makeCopyOf (source);
        delta.makeCopyOf (source);
        MultibandEngine wetEngine, dryEngine, deltaEngine;
        for (auto* engine : { &wetEngine, &dryEngine, &deltaEngine })
            engine->prepare (testSampleRate, 64, 2);
        wetEngine.process (wet, nullptr, wetSettings);
        dryEngine.process (dry, nullptr, drySettings);
        deltaEngine.process (delta, nullptr, deltaSettings);

        const auto error = maximumDeltaDifference (
            wet, dry, delta, compareStart, compareSamples);
        tests.expect (allFinite (wet) && allFinite (dry) && allFinite (delta)
                          && error <= 2.0e-5,
                      "Linear+8x+lookahead Delta keeps aligned wet-dry identity",
                      "max error " + juce::String (error, 9));
    }
}

void testQualityModesLatencyAndChannels (TestRunner& tests)
{
    tests.section ("Quality modes, latency, and channel domains");

    MultibandEngine latencyEngine;
    latencyEngine.prepare (testSampleRate, 64, 2);
    MultibandEngine::Settings defaults;
    const auto defaultLatency = latencyEngine.getLatencySamples (defaults);

    auto lookahead = defaults;
    lookahead.lookaheadMs = 10.0f;
    const auto lookaheadLatency = latencyEngine.getLatencySamples (lookahead);

    auto linear = defaults;
    linear.phaseMode = MultibandEngine::PhaseMode::linear;
    const auto linearLatency = latencyEngine.getLatencySamples (linear);

    auto twoTimes = defaults;
    twoTimes.oversamplingMode = MultibandEngine::OversamplingMode::x2;
    const auto twoTimesLatency = latencyEngine.getLatencySamples (twoTimes);

    auto fourTimes = defaults;
    fourTimes.oversamplingMode = MultibandEngine::OversamplingMode::x4;
    const auto fourTimesLatency = latencyEngine.getLatencySamples (fourTimes);

    auto eightTimes = defaults;
    eightTimes.oversamplingMode = MultibandEngine::OversamplingMode::x8;
    const auto eightTimesLatency = latencyEngine.getLatencySamples (eightTimes);

    tests.expect (defaultLatency == 0 && lookaheadLatency == 480,
                  "default is zero latency and lookahead rounds in base samples",
                  "default/lookahead " + juce::String (defaultLatency) + "/"
                      + juce::String (lookaheadLatency));
    tests.expect (linearLatency > lookaheadLatency
                      && twoTimesLatency > 0 && fourTimesLatency > 0
                      && eightTimesLatency > 0,
                  "Linear FIR and real 2x/4x/8x paths report non-zero latency",
                  "linear/2x/4x/8x " + juce::String (linearLatency) + "/"
                      + juce::String (twoTimesLatency) + "/"
                      + juce::String (fourTimesLatency) + "/"
                      + juce::String (eightTimesLatency));

    const auto findImpulsePeak = [] (MultibandEngine::Settings settings)
    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        disableAllBands (settings);
        const auto latency = engine.getLatencySamples (settings);
        juce::AudioBuffer<float> impulse (2, latency + 1024);
        impulse.clear();
        impulse.setSample (0, 0, 1.0f);
        impulse.setSample (1, 0, 1.0f);
        engine.process (impulse, nullptr, settings);
        auto peakSample = 0;
        auto peak = 0.0f;
        for (int sample = 0; sample < impulse.getNumSamples(); ++sample)
            if (const auto magnitude = std::abs (impulse.getSample (0, sample));
                magnitude > peak)
            {
                peak = magnitude;
                peakSample = sample;
            }
        return std::array<int, 2> { latency, peakSample };
    };
    const auto twoTimesImpulse = findImpulsePeak (twoTimes);
    const auto fourTimesImpulse = findImpulsePeak (fourTimes);
    const auto eightTimesImpulse = findImpulsePeak (eightTimes);
    tests.expect (std::abs (twoTimesImpulse[0] - twoTimesImpulse[1]) <= 1
                      && std::abs (fourTimesImpulse[0]
                                   - fourTimesImpulse[1]) <= 1
                      && std::abs (eightTimesImpulse[0]
                                   - eightTimesImpulse[1]) <= 1,
                  "2x/4x/8x reported latency matches the neutral impulse peak",
                  "2x " + juce::String (twoTimesImpulse[0]) + "/"
                      + juce::String (twoTimesImpulse[1]) + ", 4x "
                      + juce::String (fourTimesImpulse[0]) + "/"
                      + juce::String (fourTimesImpulse[1]) + ", 8x "
                      + juce::String (eightTimesImpulse[0]) + "/"
                      + juce::String (eightTimesImpulse[1]));

    const auto verifyNeutralDelay = [&] (MultibandEngine::Settings settings,
                                         const juce::String& name)
    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        for (auto& band : settings.bands)
        {
            band.enabled = true;
            band.rangeDb = 0.0f;
            band.outputDb = 0.0f;
        }
        const auto latency = engine.getLatencySamples (settings);
        const auto sampleCount = latency + 4096;
        juce::AudioBuffer<float> source (2, sampleCount);
        fillNoise (source, 0.25f, 0x7a10 + latency);
        juce::AudioBuffer<float> output;
        output.makeCopyOf (source);
        engine.process (output, nullptr, settings);

        auto maximumError = 0.0;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                const auto reference = sample >= latency
                    ? source.getSample (channel, sample - latency)
                    : 0.0f;
                maximumError = std::max (maximumError, std::abs (
                    static_cast<double> (output.getSample (channel, sample))
                    - static_cast<double> (reference)));
            }
        tests.expect (maximumError <= 1.0e-6 && allFinite (output), name,
                      "latency/error " + juce::String (latency) + "/"
                          + juce::String (maximumError, 9));
    };

    verifyNeutralDelay (lookahead,
                        "lookahead neutral path is an exact aligned delay");
    verifyNeutralDelay (linear,
                        "Linear neutral reconstruction nulls to delayed dry");

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        configureTestBand (settings.bands[0], 1000.0f);
        settings.phaseMode = MultibandEngine::PhaseMode::linear;
        settings.bands[0].rangeDb = 0.0f;
        settings.bands[0].outputDb = 6.0205999f;
        const auto latency = engine.getLatencySamples (settings);
        const auto sampleCount = latency * 2 + 2048;
        juce::AudioBuffer<float> impulse (2, sampleCount);
        impulse.clear();
        impulse.setSample (0, 0, 1.0f);
        impulse.setSample (1, 0, 1.0f);
        engine.process (impulse, nullptr, settings);
        impulse.addSample (0, latency, -1.0f);
        impulse.addSample (1, latency, -1.0f);

        auto symmetryError = 0.0;
        const auto radius = juce::jmin (latency - 512,
                                       sampleCount - latency - 1);
        for (int offset = 0; offset <= radius; ++offset)
            symmetryError = std::max (symmetryError, std::abs (
                static_cast<double> (impulse.getSample (0, latency - offset))
                - static_cast<double> (impulse.getSample (0, latency + offset))));
        tests.expect (allFinite (impulse) && symmetryError < 2.0e-5
                          && maximumMagnitude (impulse) > 1.0e-3,
                      "Linear delta impulse is symmetric around reported delay",
                      "symmetry " + juce::String (symmetryError, 8));
    }

    {
        const auto renderDomain = [] (MultibandEngine::ChannelMode mode,
                                      bool sideInput, int channels)
        {
            MultibandEngine engine;
            engine.prepare (testSampleRate, 64, channels);
            MultibandEngine::Settings settings;
            disableAllBands (settings);
            configureTestBand (settings.bands[0], 1000.0f);
            settings.bands[0].rangeDb = 0.0f;
            settings.bands[0].outputDb = 6.0f;
            settings.bands[0].channelMode = mode;
            juce::AudioBuffer<float> buffer (channels, 12000);
            if (channels == 1)
                fillSine (buffer, 1000.0, 0.1f, testSampleRate);
            else
                fillSine (buffer, 1000.0,
                          sideInput ? std::array<float, 2> { 0.1f, -0.1f }
                                    : std::array<float, 2> { 0.1f, 0.1f },
                          testSampleRate);
            engine.process (buffer, nullptr, settings);
            return buffer;
        };

        const auto midOnSide = renderDomain (
            MultibandEngine::ChannelMode::mid, true, 2);
        const auto sideOnSide = renderDomain (
            MultibandEngine::ChannelMode::side, true, 2);
        const auto sideOnMid = renderDomain (
            MultibandEngine::ChannelMode::side, false, 2);
        const auto midOnMid = renderDomain (
            MultibandEngine::ChannelMode::mid, false, 2);
        const auto midOnSideAmplitude = toneAmplitude (
            midOnSide, 0, 8000, 4000, 1000.0, testSampleRate);
        const auto sideOnSideAmplitude = toneAmplitude (
            sideOnSide, 0, 8000, 4000, 1000.0, testSampleRate);
        const auto sideOnMidAmplitude = toneAmplitude (
            sideOnMid, 0, 8000, 4000, 1000.0, testSampleRate);
        const auto midOnMidAmplitude = toneAmplitude (
            midOnMid, 0, 8000, 4000, 1000.0, testSampleRate);
        tests.expect (std::abs (midOnSideAmplitude - 0.1) < 0.003
                          && sideOnSideAmplitude > 0.16,
                      "Mid ignores pure Side while Side processes it",
                      "mid/side " + juce::String (midOnSideAmplitude, 4) + "/"
                          + juce::String (sideOnSideAmplitude, 4));
        tests.expect (std::abs (sideOnMidAmplitude - 0.1) < 0.003
                          && midOnMidAmplitude > 0.16,
                      "Side ignores pure Mid while Mid processes it",
                      "side/mid " + juce::String (sideOnMidAmplitude, 4) + "/"
                          + juce::String (midOnMidAmplitude, 4));

        const auto monoStereo = renderDomain (
            MultibandEngine::ChannelMode::stereo, false, 1);
        const auto monoMid = renderDomain (
            MultibandEngine::ChannelMode::mid, false, 1);
        const auto monoSide = renderDomain (
            MultibandEngine::ChannelMode::side, false, 1);
        tests.expect (maximumDifference (monoStereo, monoMid) == 0.0
                          && maximumDifference (monoStereo, monoSide) == 0.0,
                      "mono Mid and Side selections both fall back to Stereo");
    }

    {
        MultibandEngine engine;
        engine.prepare (testSampleRate, 64, 2);
        MultibandEngine::Settings settings;
        disableAllBands (settings);
        juce::AudioBuffer<float> buffer (2, 8192);
        fillSine (buffer, 1000.0, 0.2f, testSampleRate);
        juce::AudioBuffer<float> first (
            buffer.getArrayOfWritePointers(), 2, 0, 4096);
        engine.process (first, nullptr, settings);
        settings.phaseMode = MultibandEngine::PhaseMode::linear;
        settings.oversamplingMode = MultibandEngine::OversamplingMode::x8;
        settings.lookaheadMs = 20.0f;
        juce::AudioBuffer<float> second (
            buffer.getArrayOfWritePointers(), 2, 4096, 4096);
        engine.process (second, nullptr, settings);
        const auto boundaryStep = std::abs (
            static_cast<double> (buffer.getSample (0, 4096))
            - static_cast<double> (buffer.getSample (0, 4095)));
        tests.expect (allFinite (buffer) && boundaryStep < 0.01
                          && maximumStep (buffer, 0, 4096, 4096) < 0.08,
                      "mode/OS/lookahead switch resets and bounds the click",
                      "boundary/max " + juce::String (boundaryStep, 6) + "/"
                          + juce::String (maximumStep (buffer, 0, 4096, 4096), 6));
    }
}

void testBlockAndFiniteMatrix (TestRunner& tests)
{
    tests.section ("Block sizes, rates, channels, and finite output");

    constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0,
                                                   96000.0, 192000.0 };
    constexpr std::array<int, 7> blockSizes { 0, 1, 3, 63, 65, 257, 513 };
    auto matrixPasses = true;
    auto cases = 0;

    for (const auto sampleRate : sampleRates)
    {
        for (const auto channels : { 1, 2 })
        {
            for (const auto blockSize : blockSizes)
            {
                MultibandEngine engine;
                engine.prepare (sampleRate, 64, channels);
                MultibandEngine::Settings settings;
                settings.outputDb = 18.0f;
                constexpr std::array<float, 4> frequencies {
                    30.0f, 300.0f, 3000.0f, 18000.0f
                };

                for (int band = 0; band < MultibandEngine::bandCount; ++band)
                {
                    auto& item = settings.bands[static_cast<std::size_t> (band)];
                    configureTestBand (item, frequencies[static_cast<std::size_t> (band)]);
                    item.bandwidthOctaves = band % 2 == 0 ? 0.25f : 4.0f;
                    item.thresholdDb = -60.0f;
                    item.rangeDb = band % 2 == 0 ? 24.0f : -24.0f;
                    item.ratio = 20.0f;
                    item.outputDb = 18.0f;
                    item.dynamicsMode = band % 2 == 0
                                            ? MultibandEngine::DynamicsMode::expand
                                            : MultibandEngine::DynamicsMode::compress;
                    item.detectorSource = band % 2 == 0
                                              ? MultibandEngine::DetectorSource::external
                                              : MultibandEngine::DetectorSource::internal;
                    item.detectorMode = band % 2 == 0
                                            ? MultibandEngine::DetectorMode::free
                                            : MultibandEngine::DetectorMode::band;
                    item.detectorFrequencyHz = frequencies[
                        static_cast<std::size_t> ((band + 1) % 4)];
                }

                juce::AudioBuffer<float> main (channels, blockSize);
                juce::AudioBuffer<float> external (1, blockSize);
                fillNoise (main, 0.1f, 1000 + cases);
                fillNoise (external, 0.1f, 2000 + cases);
                engine.process (main, &external, settings);
                matrixPasses = matrixPasses && allFinite (main);
                ++cases;
            }
        }
    }

    tests.expect (matrixPasses,
                  "finite for 44.1/48/96/192 kHz, mono/stereo, variable/oversize blocks",
                  juce::String (cases) + " cases");

    MultibandEngine engine;
    engine.prepare (testSampleRate, 64, 2);
    MultibandEngine::Settings invalidSettings;
    invalidSettings.bands[0].frequencyHz =
        std::numeric_limits<float>::quiet_NaN();
    invalidSettings.bands[0].bandwidthOctaves =
        std::numeric_limits<float>::infinity();
    invalidSettings.bands[0].rangeDb =
        -std::numeric_limits<float>::infinity();
    invalidSettings.mix = std::numeric_limits<float>::quiet_NaN();
    juce::AudioBuffer<float> invalidBuffer (2, 257);
    fillNoise (invalidBuffer, 0.1f, 0x71bd);
    engine.process (invalidBuffer, nullptr, invalidSettings);
    tests.expect (allFinite (invalidBuffer),
                  "non-finite settings are sanitised before DSP");

    auto advancedMatrixPasses = true;
    for (const auto advancedSampleRate : { 44100.0, 192000.0 })
        for (const auto channels : { 1, 2 })
            for (const auto blockSize : { 1, 63, 65, 513 })
            {
                MultibandEngine advancedEngine;
                advancedEngine.prepare (advancedSampleRate, 64, channels);
                MultibandEngine::Settings advanced;
                advanced.phaseMode = MultibandEngine::PhaseMode::linear;
                advanced.oversamplingMode = MultibandEngine::OversamplingMode::x8;
                advanced.lookaheadMs = 20.0f;
                for (int band = 0; band < MultibandEngine::bandCount; ++band)
                    advanced.bands[static_cast<std::size_t> (band)].channelMode =
                        static_cast<MultibandEngine::ChannelMode> (band % 3);
                juce::AudioBuffer<float> buffer (channels, blockSize);
                juce::AudioBuffer<float> external (1, blockSize);
                fillNoise (buffer, 0.1f, 0x7b00 + blockSize + channels);
                fillNoise (external, 0.2f, 0x7c00 + blockSize + channels);
                advancedEngine.process (buffer, &external, advanced);
                advancedMatrixPasses = advancedMatrixPasses && allFinite (buffer);
            }
    tests.expect (advancedMatrixPasses,
                  "Linear+8x+20 ms handles 44.1/192 kHz mono/stereo variable/oversize blocks");
}

struct PerformanceStats
{
    double p50Microseconds = 0.0;
    double p95Microseconds = 0.0;
    double p99Microseconds = 0.0;
    double maximumMicroseconds = 0.0;
};

PerformanceStats benchmarkEngine (const MultibandEngine::Settings& settings,
                                  int instanceCount, bool useExternal)
{
    constexpr int blockSize = 64;
    constexpr int maximumInstances = 8;
    constexpr int warmupBlocks = 200;
    constexpr int measuredBlocks = 2000;

    std::array<MultibandEngine, maximumInstances> engines;
    std::array<juce::AudioBuffer<float>, maximumInstances> buffers;
    std::array<juce::AudioBuffer<float>, maximumInstances> inputs;
    std::array<juce::AudioBuffer<float>, maximumInstances> sidechains;

    for (int instance = 0; instance < instanceCount; ++instance)
    {
        engines[static_cast<std::size_t> (instance)].prepare (
            testSampleRate, blockSize, 2);
        buffers[static_cast<std::size_t> (instance)].setSize (2, blockSize);
        inputs[static_cast<std::size_t> (instance)].setSize (2, blockSize);
        sidechains[static_cast<std::size_t> (instance)].setSize (2, blockSize);
        fillNoise (inputs[static_cast<std::size_t> (instance)], 0.2f,
                   0x7100 + instance);
        fillNoise (sidechains[static_cast<std::size_t> (instance)], 0.3f,
                   0x7200 + instance);
    }

    const auto restoreInputs = [&]
    {
        for (int instance = 0; instance < instanceCount; ++instance)
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::copy (
                    buffers[static_cast<std::size_t> (instance)].getWritePointer (channel),
                    inputs[static_cast<std::size_t> (instance)].getReadPointer (channel),
                    blockSize);
    };

    const auto processInstances = [&]
    {
        for (int instance = 0; instance < instanceCount; ++instance)
        {
            const auto index = static_cast<std::size_t> (instance);
            engines[index].process (buffers[index],
                                    useExternal ? &sidechains[index] : nullptr,
                                    settings);
        }
    };

    for (int block = 0; block < warmupBlocks; ++block)
    {
        restoreInputs();
        processInstances();
    }

    std::vector<double> timings (measuredBlocks);
    for (auto& timing : timings)
    {
        restoreInputs();
        const auto start = std::chrono::steady_clock::now();
        processInstances();
        const auto end = std::chrono::steady_clock::now();
        timing = std::chrono::duration<double, std::micro> (end - start).count();
    }

    std::sort (timings.begin(), timings.end());
    const auto percentile = [&] (double fraction)
    {
        const auto index = static_cast<std::size_t> (std::ceil (
            fraction * static_cast<double> (timings.size() - 1)));
        return timings[index];
    };

    return { percentile (0.50), percentile (0.95), percentile (0.99),
             timings.back() };
}

#if JUCE_ENABLE_ALLOCATION_HOOKS
class ProcessAllocationTest final : public juce::UnitTest
{
public:
    ProcessAllocationTest (MultibandEngine& targetEngine,
                           juce::AudioBuffer<float>& targetMain,
                           const juce::AudioBuffer<float>& targetExternal,
                           const MultibandEngine::Settings& targetSettings)
        : juce::UnitTest ("Audio callback allocation probe"),
          engine (targetEngine), main (targetMain), external (targetExternal),
          settings (targetSettings)
    {
    }

    void runTest() override
    {
        beginTest ("MultibandEngine::process");
        juce::UnitTestAllocationChecker checker (*this);
        engine.process (main, &external, settings);
    }

private:
    MultibandEngine& engine;
    juce::AudioBuffer<float>& main;
    const juce::AudioBuffer<float>& external;
    const MultibandEngine::Settings& settings;
};
#endif

void testRealtimePerformance (TestRunner& tests)
{
    tests.section ("Realtime performance");

    MultibandEngine::Settings worst;
    constexpr std::array<float, 4> frequencies {
        80.0f, 500.0f, 3000.0f, 12000.0f
    };
    for (int band = 0; band < MultibandEngine::bandCount; ++band)
    {
        auto& item = worst.bands[static_cast<std::size_t> (band)];
        configureTestBand (item, frequencies[static_cast<std::size_t> (band)]);
        item.thresholdDb = -42.0f;
        item.rangeDb = band % 2 == 0 ? -24.0f : 24.0f;
        item.ratio = 20.0f;
        item.attackMs = 0.5f;
        item.releaseMs = 20.0f;
        item.detectorSource = MultibandEngine::DetectorSource::external;
        item.detectorMode = MultibandEngine::DetectorMode::free;
        item.detectorFrequencyHz = frequencies[
            static_cast<std::size_t> ((band + 1) % 4)];
        item.detectorBandwidthOctaves = band % 2 == 0 ? 0.25f : 4.0f;
    }

#if JUCE_ENABLE_ALLOCATION_HOOKS
    MultibandEngine allocationEngine;
    allocationEngine.prepare (testSampleRate, 64, 2);
    juce::AudioBuffer<float> allocationMain (2, 64);
    juce::AudioBuffer<float> allocationExternal (2, 64);
    fillNoise (allocationMain, 0.2f, 0x7300);
    fillNoise (allocationExternal, 0.3f, 0x7400);
    allocationEngine.process (allocationMain, &allocationExternal, worst);

    ProcessAllocationTest allocationTest (allocationEngine, allocationMain,
                                          allocationExternal, worst);

    MultibandEngine advancedAllocationEngine;
    advancedAllocationEngine.prepare (testSampleRate, 64, 2);
    MultibandEngine::Settings advancedAllocationSettings = worst;
    advancedAllocationSettings.phaseMode = MultibandEngine::PhaseMode::linear;
    advancedAllocationSettings.oversamplingMode =
        MultibandEngine::OversamplingMode::x8;
    advancedAllocationSettings.lookaheadMs = 20.0f;
    for (int band = 0; band < MultibandEngine::bandCount; ++band)
    {
        advancedAllocationSettings.bands[static_cast<std::size_t> (band)]
            .channelMode = static_cast<MultibandEngine::ChannelMode> (band % 3);
    }
    juce::AudioBuffer<float> advancedAllocationMain (2, 513);
    juce::AudioBuffer<float> advancedAllocationExternal (2, 513);
    fillNoise (advancedAllocationMain, 0.2f, 0x7500);
    fillNoise (advancedAllocationExternal, 0.3f, 0x7600);
    ProcessAllocationTest advancedAllocationTest (
        advancedAllocationEngine, advancedAllocationMain,
        advancedAllocationExternal, advancedAllocationSettings);

    juce::Array<juce::UnitTest*> allocationTests;
    allocationTests.add (&allocationTest);
    allocationTests.add (&advancedAllocationTest);
    juce::UnitTestRunner allocationRunner;
    allocationRunner.setAssertOnFailure (false);
    allocationRunner.runTests (allocationTests, 0x75424d42);
    auto allocationFailures = 0;
    for (int result = 0; result < allocationRunner.getNumResults(); ++result)
        allocationFailures += allocationRunner.getResult (result)->failures;
    tests.expect (allocationFailures == 0,
                  "default and Linear+8x callbacks perform no new/delete calls");
#endif

    const MultibandEngine::Settings defaults;
    auto advanced = worst;
    advanced.phaseMode = MultibandEngine::PhaseMode::linear;
    advanced.oversamplingMode = MultibandEngine::OversamplingMode::x8;
    advanced.lookaheadMs = 20.0f;
    for (int band = 0; band < MultibandEngine::bandCount; ++band)
        advanced.bands[static_cast<std::size_t> (band)].channelMode =
            static_cast<MultibandEngine::ChannelMode> (band % 3);
    const auto defaultStats = benchmarkEngine (defaults, 1, false);
    const auto worstStats = benchmarkEngine (worst, 1, true);
    const auto advancedStats = benchmarkEngine (advanced, 1, true);
    const auto eightInstanceStats = benchmarkEngine (worst, 8, true);

    const auto detail = [&] (const PerformanceStats& stats)
    {
        return "p50/p95/p99/max " + juce::String (stats.p50Microseconds, 2)
            + "/" + juce::String (stats.p95Microseconds, 2)
            + "/" + juce::String (stats.p99Microseconds, 2)
            + "/" + juce::String (stats.maximumMicroseconds, 2) + " us";
    };

#if defined (NDEBUG)
    constexpr auto blockDeadlineMicroseconds = 64.0 / testSampleRate * 1.0e6;
    tests.expect (defaultStats.p99Microseconds
                          < blockDeadlineMicroseconds * 0.10,
                  "Release default p99 is below 10% of a 48 kHz/64 deadline",
                  detail (defaultStats));
    tests.expect (worstStats.p99Microseconds
                          < blockDeadlineMicroseconds * 0.20,
                  "Release worst-case p99 is below 20% of a 48 kHz/64 deadline",
                  detail (worstStats));
    tests.expect (advancedStats.p99Microseconds < blockDeadlineMicroseconds,
                  "Release Linear+8x p99 fits one 48 kHz/64 deadline",
                  detail (advancedStats));
    tests.expect (eightInstanceStats.p99Microseconds
                          < blockDeadlineMicroseconds,
                  "eight worst-case instances fit one 48 kHz/64 deadline",
                  detail (eightInstanceStats));
#else
    tests.expect (defaultStats.p99Microseconds > 0.0,
                  "Debug default benchmark recorded; Release owns the gate",
                  detail (defaultStats));
    tests.expect (worstStats.p99Microseconds > 0.0,
                  "Debug worst-case benchmark recorded; Release owns the gate",
                  detail (worstStats));
    tests.expect (advancedStats.p99Microseconds > 0.0,
                  "Debug Linear+8x benchmark recorded; Release owns the gate",
                  detail (advancedStats));
    tests.expect (eightInstanceStats.p99Microseconds > 0.0,
                  "Debug eight-instance benchmark recorded; Release owns the gate",
                  detail (eightInstanceStats));
#endif
}
} // namespace

int main()
{
    std::cout << "TB_MultiBandCompressor DSP Harness" << std::endl;
    TestRunner tests;

    testNeutralNull (tests);
    testBandPassNormalisationAndSelectivity (tests);
    testSignedRangeModes (tests);
    testSidechainContracts (tests);
    testStereoLink (tests);
    testSoftBypass (tests);
    testAutomationSmoothing (tests);
    testDeltaListen (tests);
    testQualityModesLatencyAndChannels (tests);
    testBlockAndFiniteMatrix (tests);
    testRealtimePerformance (tests);

    std::cout << std::endl << tests.checks << " checks, "
              << tests.failures << " failures" << std::endl;
    return tests.failures == 0 ? 0 : 1;
}
