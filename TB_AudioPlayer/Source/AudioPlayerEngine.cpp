#include "AudioPlayerEngine.h"

#include <cmath>

void TBAudioPlayerEngine::prepare (double newOutputSampleRate) noexcept
{
    outputSampleRate = newOutputSampleRate > 0.0 ? newOutputSampleRate : 44100.0;
    fadeLengthSamples = juce::jmax (1, static_cast<int> (
        std::llround (outputSampleRate * retriggerFadeSeconds)));
    resetVoices();
}

void TBAudioPlayerEngine::resetVoices() noexcept
{
    for (auto& voice : voices)
        voice = {};
}

void TBAudioPlayerEngine::setCurrentSample (TBAudioSampleData* sample) noexcept
{
    currentSample = sample;
}

void TBAudioPlayerEngine::fadeActiveVoices() noexcept
{
    for (auto& voice : voices)
        if (voice.active && voice.fadeSamplesRemaining == 0)
        {
            voice.fadeSamplesRemaining = fadeLengthSamples;
            voice.fadeStep = voice.gain / static_cast<float> (fadeLengthSamples);
        }
}

TBAudioPlayerEngine::Voice* TBAudioPlayerEngine::findVoiceForTrigger() noexcept
{
    for (auto& voice : voices)
        if (! voice.active)
            return &voice;

    auto* quietest = &voices.front();
    for (auto& voice : voices)
        if (voice.gain < quietest->gain
            || (voice.gain == quietest->gain && voice.serial < quietest->serial))
            quietest = &voice;

    return quietest;
}

bool TBAudioPlayerEngine::trigger() noexcept
{
    if (currentSample == nullptr
        || ! std::isfinite (currentSample->sampleRate)
        || currentSample->sampleRate <= 0.0
        || currentSample->audio.getNumChannels() <= 0
        || currentSample->audio.getNumSamples() <= 0)
        return false;

    fadeActiveVoices();

    auto* voice = findVoiceForTrigger();
    *voice = {};
    voice->sample = currentSample;
    voice->increment = currentSample->sampleRate / outputSampleRate;
    if (! std::isfinite (voice->increment) || voice->increment <= 0.0)
    {
        *voice = {};
        return false;
    }
    voice->gain = 1.0f;
    voice->serial = nextVoiceSerial++;
    voice->active = true;
    return true;
}

void TBAudioPlayerEngine::stop() noexcept
{
    fadeActiveVoices();
}

float TBAudioPlayerEngine::readCubic (const TBAudioSampleData& sample,
                                      int sourceChannel,
                                      double position) noexcept
{
    const auto sampleCount = sample.audio.getNumSamples();
    if (sampleCount <= 0)
        return 0.0f;

    const auto index = static_cast<int> (position);
    const auto fraction = static_cast<float> (position - static_cast<double> (index));
    const auto* source = sample.audio.getReadPointer (
        juce::jlimit (0, sample.audio.getNumChannels() - 1, sourceChannel));

    const auto at = [source, sampleCount] (int i) noexcept
    {
        return source[juce::jlimit (0, sampleCount - 1, i)];
    };

    const auto y0 = at (index - 1);
    const auto y1 = at (index);
    const auto y2 = at (index + 1);
    const auto y3 = at (index + 2);
    const auto a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const auto a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const auto a2 = -0.5f * y0 + 0.5f * y2;
    return ((a0 * fraction + a1) * fraction + a2) * fraction + y1;
}

void TBAudioPlayerEngine::render (juce::AudioBuffer<float>& output) noexcept
{
    output.clear();

    const auto outputChannels = output.getNumChannels();
    const auto outputSamples = output.getNumSamples();

    for (auto& voice : voices)
    {
        if (! voice.active || voice.sample == nullptr)
            continue;

        const auto sourceChannels = voice.sample->audio.getNumChannels();
        const auto sourceSamples = voice.sample->audio.getNumSamples();

        for (auto frame = 0; frame < outputSamples; ++frame)
        {
            if (voice.position >= static_cast<double> (sourceSamples))
            {
                voice = {};
                break;
            }

            if (outputChannels == 1 && sourceChannels >= 2)
            {
                const auto mono = 0.5f * (readCubic (*voice.sample, 0, voice.position)
                                        + readCubic (*voice.sample, 1, voice.position));
                output.addSample (0, frame, mono * voice.gain);
            }
            else
            {
                for (auto channel = 0; channel < outputChannels; ++channel)
                    output.addSample (channel, frame,
                                      readCubic (*voice.sample,
                                                 juce::jmin (channel, sourceChannels - 1),
                                                 voice.position)
                                      * voice.gain);
            }

            voice.position += voice.increment;

            if (voice.fadeSamplesRemaining > 0)
            {
                --voice.fadeSamplesRemaining;
                voice.gain = juce::jmax (0.0f, voice.gain - voice.fadeStep);
                if (voice.fadeSamplesRemaining == 0)
                {
                    voice = {};
                    break;
                }
            }

            if (voice.position >= static_cast<double> (sourceSamples))
            {
                voice = {};
                break;
            }
        }
    }
}

bool TBAudioPlayerEngine::referencesSample (
    const TBAudioSampleData* sample) const noexcept
{
    if (sample == nullptr)
        return false;

    for (const auto& voice : voices)
        if (voice.active && voice.sample == sample)
            return true;

    return currentSample == sample;
}

int TBAudioPlayerEngine::getActiveVoiceCount() const noexcept
{
    auto count = 0;
    for (const auto& voice : voices)
        if (voice.active)
            ++count;
    return count;
}
