/*
  ==============================================================================

    HrtfRenderer.cpp

  ==============================================================================
*/

#include "HrtfRenderer.h"
#include "HrtfResources.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
constexpr float firstElevationDegrees = -40.0f;
constexpr float lastElevationDegrees = 90.0f;
constexpr float elevationStepDegrees = 10.0f;
constexpr int horizontalRow = 4;
// Measured directions around the full circle on each elevation row, from
// -40 to +90 degrees. Rows hold the right-hand half; the left is its mirror.
constexpr std::array<int, 14> fullCircleCounts { 56, 60, 72, 72, 72, 72, 72, 60, 56, 45, 36, 24, 12, 1 };
constexpr float stereoHalfWidthDegrees = 30.0f;
constexpr int filterUpdateInterval = 64;

float wrapAzimuth (float angle)
{
    auto wrapped = std::fmod (angle + 180.0f, 360.0f);
    if (wrapped < 0.0f)
        wrapped += 360.0f;
    return wrapped - 180.0f;
}

float getRowStepDegrees (int row)
{
    return 360.0f / static_cast<float> (fullCircleCounts[static_cast<size_t> (row)]);
}

// Measured azimuths from 0 up to at most 180 degrees on one row.
int getRowCount (int row)
{
    return static_cast<int> (std::floor (180.0f / getRowStepDegrees (row) + 1.0e-3f)) + 1;
}

constexpr int kernelResolution = 512;
constexpr double kernelZeroCrossings = 24.0;
constexpr double kernelKaiserBeta = 9.0;
constexpr float onsetThreshold = 0.15f;
constexpr int fractionalDelayMargin = 3;

double besselI0 (double value)
{
    double sum = 1.0, term = 1.0;
    for (int index = 1; index < 40; ++index)
    {
        const auto factor = value / (2.0 * static_cast<double> (index));
        term *= factor * factor;
        sum += term;
    }
    return sum;
}

// Kaiser-windowed sinc sampled at kernelResolution points per measured
// sample. Empty when the host runs at the measured rate.
std::vector<float> makeResamplingKernel (double sourcePerTargetSample)
{
    if (std::abs (sourcePerTargetSample - 1.0) < 1.0e-9)
        return {};

    const auto cutoff = juce::jmin (1.0, 1.0 / sourcePerTargetSample);
    const auto halfWidth = kernelZeroCrossings / cutoff;
    const auto entries = static_cast<int> (std::ceil (halfWidth * kernelResolution));
    std::vector<float> kernel (static_cast<size_t> (entries + 2), 0.0f);
    const auto windowScale = 1.0 / besselI0 (kernelKaiserBeta);

    for (int index = 0; index < entries; ++index)
    {
        const auto offset = static_cast<double> (index) / kernelResolution;
        const auto phase = juce::MathConstants<double>::pi * cutoff * offset;
        const auto sinc = phase < 1.0e-9 ? 1.0 : std::sin (phase) / phase;
        const auto windowPosition = offset / halfWidth;
        const auto window = besselI0 (kernelKaiserBeta * std::sqrt (juce::jmax (0.0, 1.0 - windowPosition * windowPosition)))
                          * windowScale;
        kernel[static_cast<size_t> (index)] = static_cast<float> (cutoff * sinc * window);
    }

    return kernel;
}

// Linear interpolation dulled the measured cues above 8 kHz and let images
// through at every host rate other than the measured one.
void resampleImpulse (const float* source, int sourceSamples, double sourcePerTargetSample,
                      const std::vector<float>& kernel, float* target, int targetSamples)
{
    if (kernel.empty())
    {
        for (int sample = 0; sample < targetSamples; ++sample)
            target[sample] = sample < sourceSamples ? source[sample] : 0.0f;
        return;
    }

    const auto halfWidth = static_cast<double> (kernel.size() - 2) / kernelResolution;

    for (int sample = 0; sample < targetSamples; ++sample)
    {
        const auto position = static_cast<double> (sample) * sourcePerTargetSample;
        const auto first = juce::jmax (0, static_cast<int> (std::ceil (position - halfWidth)));
        const auto last = juce::jmin (sourceSamples - 1, static_cast<int> (std::floor (position + halfWidth)));
        double sum = 0.0;

        for (int index = first; index <= last; ++index)
        {
            const auto tablePosition = std::abs (position - static_cast<double> (index)) * kernelResolution;
            const auto tableIndex = static_cast<size_t> (tablePosition);
            if (tableIndex + 1 >= kernel.size())
                continue;
            const auto fraction = static_cast<float> (tablePosition - static_cast<double> (tableIndex));
            sum += static_cast<double> (source[index])
                 * static_cast<double> (kernel[tableIndex] + fraction * (kernel[tableIndex + 1] - kernel[tableIndex]));
        }

        target[sample] = static_cast<float> (sum);
    }
}

int findOnset (const float* samples, int count)
{
    float peak = 0.0f;
    for (int sample = 0; sample < count; ++sample)
        peak = juce::jmax (peak, std::abs (samples[sample]));

    const auto threshold = onsetThreshold * peak;
    for (int sample = 0; sample < count; ++sample)
        if (std::abs (samples[sample]) >= threshold)
            return sample;

    return 0;
}

// Delay, in possibly fractional samples, of one response relative to another
// that lines the two up best. A level threshold alone misjudges the far ear
// beside the head, where the sound arrives round both sides at once.
float findRelativeDelay (const float* later, const float* reference, int count, int maximumLag)
{
    constexpr int largestSupportedLag = 64;
    maximumLag = juce::jlimit (1, largestSupportedLag, maximumLag);
    std::array<double, 2 * largestSupportedLag + 1> correlation {};
    int best = 0;

    for (int lag = -maximumLag; lag <= maximumLag; ++lag)
    {
        double sum = 0.0;
        const auto end = juce::jmin (count, count - lag);
        for (int sample = juce::jmax (0, -lag); sample < end; ++sample)
            sum += static_cast<double> (later[sample + lag]) * static_cast<double> (reference[sample]);

        correlation[static_cast<size_t> (lag + maximumLag)] = sum;
        if (lag == -maximumLag || sum > correlation[static_cast<size_t> (best + maximumLag)])
            best = lag;
    }

    if (best > -maximumLag && best < maximumLag)
    {
        const auto before = correlation[static_cast<size_t> (best + maximumLag - 1)];
        const auto peak = correlation[static_cast<size_t> (best + maximumLag)];
        const auto after = correlation[static_cast<size_t> (best + maximumLag + 1)];
        const auto curvature = before - 2.0 * peak + after;
        if (curvature < 0.0)
            return static_cast<float> (best + 0.5 * (before - after) / curvature);
    }

    return static_cast<float> (best);
}

// Adds gain * source, moved later (or earlier) by a possibly fractional number
// of samples. Whole shifts are exact copies, so a measured direction
// reproduces its measurement; in-between shifts use fifth-order Lagrange.
void addDelayed (const float* source, int count, float delay, float gain, float* destination, int capacity)
{
    const auto whole = static_cast<int> (std::floor (delay));
    const auto fraction = delay - static_cast<float> (whole);

    if (fraction < 1.0e-4f)
    {
        const auto end = juce::jmin (count, capacity - whole);
        for (int sample = juce::jmax (0, -whole); sample < end; ++sample)
            destination[sample + whole] += gain * source[sample];
        return;
    }

    for (int node = -2; node <= 3; ++node)
    {
        float coefficient = gain;
        for (int other = -2; other <= 3; ++other)
            if (other != node)
                coefficient *= (fraction - static_cast<float> (other)) / static_cast<float> (node - other);

        const auto offset = whole + node;
        const auto end = juce::jmin (count, capacity - offset);
        for (int sample = juce::jmax (0, -offset); sample < end; ++sample)
            destination[sample + offset] += coefficient * source[sample];
    }
}
}

void HrtfRenderer::SourceHistory::reset()
{
    samples.fill (0.0f);
    writeIndex = 0;
}

void HrtfRenderer::SourceHistory::push (float sample)
{
    writeIndex = (writeIndex + maximumTaps - 1) % maximumTaps;
    samples[static_cast<size_t> (writeIndex)] = sample;
    samples[static_cast<size_t> (writeIndex + maximumTaps)] = sample;
}

void HrtfRenderer::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    filterCrossfade.reset (sampleRate, 0.020);
    loadImpulseResponses();
    reset();
}

void HrtfRenderer::reset()
{
    for (auto& history : histories)
        history.reset();

    hasInitialFilters = false;
    activeSourceCount = 0;
    samplesUntilFilterUpdate = 0;
    filterCrossfade.setCurrentAndTargetValue (1.0f);
}

void HrtfRenderer::setSourceLayout (const float* azimuths, const bool* lfeChannels, int sourceCount)
{
    sourceAzimuths.fill (0.0f);
    sourceIsLfe.fill (false);
    for (int source = 0; source < juce::jlimit (0, maximumSources, sourceCount); ++source)
    {
        sourceAzimuths[static_cast<size_t> (source)] = azimuths[source];
        sourceIsLfe[static_cast<size_t> (source)] = lfeChannels[source];
    }
    reset();
}

void HrtfRenderer::loadImpulseResponses()
{
    juce::MemoryInputStream zipData (kemar_hrir_zip, kemar_hrir_zipSize, false);
    juce::ZipFile archive (zipData);
    const auto resamplingKernel = makeResamplingKernel (measurementSampleRate / currentSampleRate);
    ready = true;
    inHeadDelaySamples = 0;
    measurementTaps.clear();

    for (int elevation = 0; elevation < numberOfElevations; ++elevation)
    {
        auto& row = impulseResponses[static_cast<size_t> (elevation)];
        const auto count = getRowCount (elevation);
        row.resize (static_cast<size_t> (count));
        const auto elevationName = juce::String (juce::roundToInt (firstElevationDegrees
                                                                   + elevationStepDegrees * static_cast<float> (elevation)));

        for (int azimuth = 0; azimuth < count; ++azimuth)
        {
            // Most rows have fractional spacing (6.42857 degrees at +/-40);
            // only the filenames round the measured angle to an integer.
            const auto angle = getRowStepDegrees (elevation) * static_cast<float> (azimuth);
            const auto fileName = "elev" + elevationName + "/H" + elevationName + "e"
                                + juce::String (juce::roundToInt (angle)).paddedLeft ('0', 3) + "a.wav";
            auto& response = row[static_cast<size_t> (azimuth)];
            response = loadImpulseResponse (archive, fileName, resamplingKernel);

            if (response.count <= 1)
                ready = false;
        }
    }

    if (ready)
    {
        alignMeasurements();

        // The frontal arrival is where an in-head signal must sit to blend
        // with the measured responses without combing against them.
        const auto& front = impulseResponses[static_cast<size_t> (horizontalRow)].front();
        const auto first = measurementTaps.begin() + front.offset;
        inHeadDelaySamples = juce::jlimit (0, maximumTaps - 1,
                                           static_cast<int> (std::max_element (first, first + front.count) - first));
    }
}

void HrtfRenderer::alignMeasurements()
{
    // Each ear of each measured direction is a node; node = 2 * direction + ear.
    std::vector<Measurement*> directions;
    std::array<int, numberOfElevations> rowStart {};

    for (int row = 0; row < numberOfElevations; ++row)
    {
        rowStart[static_cast<size_t> (row)] = static_cast<int> (directions.size());
        for (auto& measurement : impulseResponses[static_cast<size_t> (row)])
            directions.push_back (&measurement);
    }

    const auto nodeCount = static_cast<int> (directions.size()) * 2;
    const auto maximumLag = static_cast<int> (std::ceil (8.0 * currentSampleRate / measurementSampleRate));
    const auto tapsOf = [&] (int node)
    {
        const auto& measurement = *directions[static_cast<size_t> (node / 2)];
        return measurementTaps.data() + measurement.offset + (node & 1) * measurement.count;
    };

    // Measure how far apart every pair of neighbouring responses arrives.
    struct Link { int other; float delay; };
    std::vector<std::vector<Link>> links (static_cast<size_t> (nodeCount));
    const auto connect = [&] (int later, int reference)
    {
        const auto count = juce::jmin (directions[static_cast<size_t> (later / 2)]->count,
                                       directions[static_cast<size_t> (reference / 2)]->count);
        const auto delay = findRelativeDelay (tapsOf (later), tapsOf (reference), count, maximumLag);
        links[static_cast<size_t> (later)].push_back ({ reference, delay });
        links[static_cast<size_t> (reference)].push_back ({ later, -delay });
    };

    for (int row = 0; row < numberOfElevations; ++row)
    {
        const auto count = static_cast<int> (impulseResponses[static_cast<size_t> (row)].size());
        const auto first = 2 * rowStart[static_cast<size_t> (row)];

        for (int azimuth = 0; azimuth < count; ++azimuth)
        {
            const auto node = first + 2 * azimuth;

            for (int ear = 0; ear < 2; ++ear)
            {
                if (azimuth + 1 < count)
                    connect (node + 2 + ear, node + ear);

                if (row + 1 < numberOfElevations)
                {
                    const auto countAbove = static_cast<int> (impulseResponses[static_cast<size_t> (row + 1)].size());
                    const auto above = juce::jlimit (0, countAbove - 1,
                                                     juce::roundToInt (static_cast<float> (azimuth) * getRowStepDegrees (row)
                                                                       / getRowStepDegrees (row + 1)));
                    connect (2 * (rowStart[static_cast<size_t> (row + 1)] + above) + ear, node + ear);
                }
            }
        }

        // The two ears are only tied together in front of and behind the
        // head, where a row meets its own mirror image.
        connect (first + 1, first);
        if (count > 1)
            connect (first + 2 * (count - 1) + 1, first + 2 * (count - 1));
    }

    // Start from one threshold arrival and walk the measured delays outwards.
    std::vector<float> arrival (static_cast<size_t> (nodeCount), 0.0f);
    std::vector<bool> known (static_cast<size_t> (nodeCount), false);
    std::vector<int> pending { 0 };
    arrival[0] = directions[0]->leftOnset;
    known[0] = true;

    while (! pending.empty())
    {
        const auto node = pending.back();
        pending.pop_back();

        for (const auto& link : links[static_cast<size_t> (node)])
            if (! known[static_cast<size_t> (link.other)])
            {
                arrival[static_cast<size_t> (link.other)] = arrival[static_cast<size_t> (node)] - link.delay;
                known[static_cast<size_t> (link.other)] = true;
                pending.push_back (link.other);
            }
    }

    // The measured delays disagree slightly around any loop of neighbours.
    // Settle every arrival on the average its neighbours ask for.
    for (int pass = 0; pass < 100; ++pass)
        for (int node = 1; node < nodeCount; ++node)
        {
            float sum = 0.0f;
            for (const auto& link : links[static_cast<size_t> (node)])
                sum += arrival[static_cast<size_t> (link.other)] + link.delay;
            arrival[static_cast<size_t> (node)] = sum / static_cast<float> (links[static_cast<size_t> (node)].size());
        }

    for (int node = 0; node < nodeCount; ++node)
    {
        auto& measurement = *directions[static_cast<size_t> (node / 2)];
        ((node & 1) == 0 ? measurement.leftOnset : measurement.rightOnset) = arrival[static_cast<size_t> (node)];
    }
}

HrtfRenderer::Measurement HrtfRenderer::loadImpulseResponse (juce::ZipFile& archive, const juce::String& fileName,
                                                           const std::vector<float>& resamplingKernel)
{
    Measurement result;
    const auto* entry = archive.getEntry (fileName);

    if (entry == nullptr)
        return result;

    std::unique_ptr<juce::InputStream> stream (archive.createStreamForEntry (*entry));

    if (stream == nullptr)
        return result;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (wavFormat.createReaderFor (stream.release(), true));

    if (reader == nullptr || reader->numChannels < 2 || reader->lengthInSamples <= 1
        || std::abs (reader->sampleRate - measurementSampleRate) > 0.5)
        return result;

    const auto sourceSamples = static_cast<int> (reader->lengthInSamples);
    juce::AudioBuffer<float> source (2, sourceSamples);
    reader->read (&source, 0, sourceSamples, 0, true, true);

    const auto targetPerSourceSample = currentSampleRate / measurementSampleRate;
    // Leave room for the few samples a fractional arrival time can add.
    const auto targetSamples = juce::jlimit (2, maximumTaps - 8,
                                              juce::roundToInt (static_cast<double> (sourceSamples) * targetPerSourceSample));
    result.offset = static_cast<int> (measurementTaps.size());
    measurementTaps.resize (measurementTaps.size() + static_cast<size_t> (2 * targetSamples));

    for (int channel = 0; channel < 2; ++channel)
    {
        auto* target = measurementTaps.data() + result.offset + channel * targetSamples;
        resampleImpulse (source.getReadPointer (channel), sourceSamples, 1.0 / targetPerSourceSample,
                         resamplingKernel, target, targetSamples);
        (channel == 0 ? result.leftOnset : result.rightOnset) = static_cast<float> (findOnset (target, targetSamples));
    }

    result.count = targetSamples;
    return result;
}

HrtfRenderer::Filter HrtfRenderer::selectFilter (float azimuthDegrees, float elevationDegrees) const
{
    azimuthDegrees = wrapAzimuth (azimuthDegrees);
    elevationDegrees = juce::jlimit (firstElevationDegrees, lastElevationDegrees, elevationDegrees);

    struct Corner
    {
        const float* left = nullptr;
        const float* right = nullptr;
        float leftOnset = 0.0f;
        float rightOnset = 0.0f;
        int count = 0;
        float weight = 0.0f;
    };

    std::array<Corner, 4> corners;
    int cornerCount = 0;

    const auto addCorner = [&] (const Measurement& measurement, bool mirrored, float weight)
    {
        if (weight <= 0.0f)
            return;

        const auto* taps = measurementTaps.data() + measurement.offset;
        auto& corner = corners[static_cast<size_t> (cornerCount++)];
        corner.left = mirrored ? taps + measurement.count : taps;
        corner.right = mirrored ? taps : taps + measurement.count;
        corner.leftOnset = mirrored ? measurement.rightOnset : measurement.leftOnset;
        corner.rightOnset = mirrored ? measurement.leftOnset : measurement.rightOnset;
        corner.count = measurement.count;
        corner.weight = weight;
    };

    const auto addRow = [&] (int row, float rowWeight)
    {
        if (rowWeight <= 0.0f)
            return;

        const auto& measurements = impulseResponses[static_cast<size_t> (row)];
        const auto count = static_cast<int> (measurements.size());
        const auto step = getRowStepDegrees (row);
        const auto position = count > 1 ? std::abs (azimuthDegrees) / step : 0.0f;
        const auto lower = juce::jlimit (0, count - 1, static_cast<int> (position));
        const auto amount = count > 1 ? juce::jlimit (0.0f, 1.0f, position - static_cast<float> (lower)) : 0.0f;
        addCorner (measurements[static_cast<size_t> (lower)], false, rowWeight * (1.0f - amount));

        if (amount > 0.0f)
        {
            // Past the last measured angle on this side (176 degrees at +50)
            // the next measurement is the mirror image on the other side.
            const auto beyondRear = lower + 1 >= count;
            const auto upper = beyondRear
                             ? juce::jlimit (0, count - 1, juce::roundToInt (360.0f / step) - (lower + 1))
                             : lower + 1;
            addCorner (measurements[static_cast<size_t> (upper)], beyondRear, rowWeight * amount);
        }
    };

    const auto rowPosition = (elevationDegrees - firstElevationDegrees) / elevationStepDegrees;
    const auto lowerRow = juce::jlimit (0, numberOfElevations - 1, static_cast<int> (rowPosition));
    const auto rowAmount = lowerRow + 1 < numberOfElevations
                         ? juce::jlimit (0.0f, 1.0f, rowPosition - static_cast<float> (lowerRow)) : 0.0f;
    addRow (lowerRow, 1.0f - rowAmount);
    if (rowAmount > 0.0f)
        addRow (lowerRow + 1, rowAmount);

    Filter filter;
    filter.elevationDegrees = elevationDegrees;
    filter.azimuthDegrees = azimuthDegrees;

    // Blend the arrival times, then move every neighbour onto that arrival
    // before blending the responses. Blending them where they stood summed
    // copies a few samples apart, which notched the high frequencies between
    // measured directions. A measured direction is a single neighbour moved
    // by zero samples: the measurement itself.
    float leftArrival = 0.0f, rightArrival = 0.0f;
    for (int index = 0; index < cornerCount; ++index)
    {
        leftArrival += corners[static_cast<size_t> (index)].weight * corners[static_cast<size_t> (index)].leftOnset;
        rightArrival += corners[static_cast<size_t> (index)].weight * corners[static_cast<size_t> (index)].rightOnset;
    }

    int count = 1;
    float largestShift = 0.0f;
    for (int index = 0; index < cornerCount; ++index)
    {
        const auto& corner = corners[static_cast<size_t> (index)];
        const auto leftShift = leftArrival - corner.leftOnset;
        const auto rightShift = rightArrival - corner.rightOnset;
        addDelayed (corner.left, corner.count, leftShift, corner.weight, filter.left.data(), maximumTaps);
        addDelayed (corner.right, corner.count, rightShift, corner.weight, filter.right.data(), maximumTaps);
        count = juce::jmax (count, corner.count);
        largestShift = juce::jmax (largestShift, leftShift, rightShift);
    }

    filter.tapCount = juce::jmin (maximumTaps,
                                  count + static_cast<int> (std::ceil (largestShift)) + fractionalDelayMargin);

    if (azimuthDegrees < 0.0f)
        std::swap (filter.left, filter.right);

    double energy = 0.0;

    for (int tap = 0; tap < filter.tapCount; ++tap)
    {
        const auto left = filter.left[static_cast<size_t> (tap)];
        const auto right = filter.right[static_cast<size_t> (tap)];
        energy += static_cast<double> (left * left + right * right);
    }

    if (energy > 1.0e-12)
    {
        // Unit energy at the measured rate. A resampled response holds
        // proportionally more samples of the same response, so scaling by the
        // rate ratio keeps the audible level identical at every host rate.
        const auto normalisationGain = static_cast<float> (
            1.0 / std::sqrt (energy * currentSampleRate / measurementSampleRate));

        for (int tap = 0; tap < filter.tapCount; ++tap)
        {
            filter.left[static_cast<size_t> (tap)] *= normalisationGain;
            filter.right[static_cast<size_t> (tap)] *= normalisationGain;
        }
    }

    return filter;
}

HrtfRenderer::Filter HrtfRenderer::blendFilters (const Filter& first, const Filter& second, float amount) noexcept
{
    const auto blend = juce::jlimit (0.0f, 1.0f, amount);
    Filter result = second;
    result.tapCount = std::max (first.tapCount, second.tapCount);

    for (int tap = 0; tap < result.tapCount; ++tap)
    {
        result.left[static_cast<size_t> (tap)] = first.left[static_cast<size_t> (tap)]
                                              + blend * (second.left[static_cast<size_t> (tap)]
                                                       - first.left[static_cast<size_t> (tap)]);
        result.right[static_cast<size_t> (tap)] = first.right[static_cast<size_t> (tap)]
                                               + blend * (second.right[static_cast<size_t> (tap)]
                                                        - first.right[static_cast<size_t> (tap)]);
    }

    return result;
}

float HrtfRenderer::getSourceAzimuth (int source, int sourceCount, float azimuthDegrees) const
{
    const auto offset = sourceCount == 1 ? 0.0f
                      : sourceCount == 2 ? (source == 0 ? -stereoHalfWidthDegrees : stereoHalfWidthDegrees)
                                         : sourceAzimuths[static_cast<size_t> (source)];
    return wrapAzimuth (azimuthDegrees + offset);
}

void HrtfRenderer::updateFilters (float azimuthDegrees, float elevationDegrees, int sourceCount)
{
    bool changed = ! hasInitialFilters;
    for (int source = 0; source < sourceCount && ! changed; ++source)
    {
        if (sourceCount > 2 && sourceIsLfe[static_cast<size_t> (source)])
            continue;
        const auto& previous = pendingFilters[static_cast<size_t> (source)];
        changed = std::abs (wrapAzimuth (previous.azimuthDegrees - getSourceAzimuth (source, sourceCount, azimuthDegrees))) > 0.001f
               || std::abs (previous.elevationDegrees - elevationDegrees) > 0.001f;
    }
    if (! changed)
        return;

    const auto blend = filterCrossfade.getCurrentValue();
    for (int source = 0; source < sourceCount; ++source)
    {
        const auto index = static_cast<size_t> (source);
        if (sourceIsLfe[index] && sourceCount > 2)
            continue;
        const auto requested = selectFilter (getSourceAzimuth (source, sourceCount, azimuthDegrees), elevationDegrees);
        if (! hasInitialFilters)
            activeFilters[index] = requested;
        else if (filterCrossfade.isSmoothing())
            activeFilters[index] = blendFilters (activeFilters[index], pendingFilters[index], blend);
        else
            activeFilters[index] = pendingFilters[index];
        pendingFilters[index] = requested;
    }

    if (hasInitialFilters)
    {
        filterCrossfade.setCurrentAndTargetValue (0.0f);
        filterCrossfade.setTargetValue (1.0f);
    }
    hasInitialFilters = true;
}

void HrtfRenderer::renderFilter (const SourceHistory& history, const Filter& filter,
                                 float& outputLeft, float& outputRight) const
{
    const auto* samples = history.samples.data() + history.writeIndex;
    // Independent lanes avoid a long serial accumulation chain for high-rate,
    // multi-source rendering, without changing the HRIR or reducing its length.
    float left0 = 0.0f, left1 = 0.0f, left2 = 0.0f, left3 = 0.0f;
    float right0 = 0.0f, right1 = 0.0f, right2 = 0.0f, right3 = 0.0f;
    int tap = 0;
    for (; tap + 3 < filter.tapCount; tap += 4)
    {
        left0 += samples[tap]     * filter.left[static_cast<size_t> (tap)];
        left1 += samples[tap + 1] * filter.left[static_cast<size_t> (tap + 1)];
        left2 += samples[tap + 2] * filter.left[static_cast<size_t> (tap + 2)];
        left3 += samples[tap + 3] * filter.left[static_cast<size_t> (tap + 3)];
        right0 += samples[tap]     * filter.right[static_cast<size_t> (tap)];
        right1 += samples[tap + 1] * filter.right[static_cast<size_t> (tap + 1)];
        right2 += samples[tap + 2] * filter.right[static_cast<size_t> (tap + 2)];
        right3 += samples[tap + 3] * filter.right[static_cast<size_t> (tap + 3)];
    }
    outputLeft = (left0 + left1) + (left2 + left3);
    outputRight = (right0 + right1) + (right2 + right3);
    for (; tap < filter.tapCount; ++tap)
    {
        outputLeft += samples[tap] * filter.left[static_cast<size_t> (tap)];
        outputRight += samples[tap] * filter.right[static_cast<size_t> (tap)];
    }
}

void HrtfRenderer::process (const float* input, int numInputChannels, float azimuthDegrees,
                            float elevationDegrees, float& outputLeft, float& outputRight, float focus)
{
    const auto sourceCount = juce::jlimit (0, maximumSources, numInputChannels);
    // A surround bed is rotated as a whole and keeps its own image.
    const auto directional = sourceCount > 2 ? 1.0f : juce::jlimit (0.0f, 1.0f, focus);
    outputLeft = outputRight = 0.0f;
    if (! ready)
    {
        // Keep every channel audible if the bundled resource cannot be loaded.
        for (int source = 0; source < sourceCount; ++source)
        {
            const auto lateral = directional
                               * std::sin (juce::degreesToRadians (getSourceAzimuth (source, sourceCount, azimuthDegrees)));
            outputLeft += input[source] * std::sqrt (0.5f * (1.0f - lateral));
            outputRight += input[source] * std::sqrt (0.5f * (1.0f + lateral));
        }
        return;
    }

    if (activeSourceCount != sourceCount)
    {
        reset();
        activeSourceCount = sourceCount;
    }
    if (samplesUntilFilterUpdate-- <= 0)
    {
        updateFilters (azimuthDegrees, elevationDegrees, sourceCount);
        samplesUntilFilterUpdate = filterUpdateInterval - 1;
    }

    const auto fading = filterCrossfade.isSmoothing();
    const auto blend = filterCrossfade.getNextValue();
    for (int source = 0; source < sourceCount; ++source)
    {
        const auto index = static_cast<size_t> (source);
        if (sourceCount > 2 && sourceIsLfe[index])
        {
            outputLeft += input[source] * 0.70710678118f;
            outputRight += input[source] * 0.70710678118f;
            continue;
        }
        histories[index].push (input[source]);
        float left = 0.0f, right = 0.0f;
        renderFilter (histories[index], activeFilters[index], left, right);
        if (fading)
        {
            float pendingLeft = 0.0f, pendingRight = 0.0f;
            renderFilter (histories[index], pendingFilters[index], pendingLeft, pendingRight);
            left += blend * (pendingLeft - left);
            right += blend * (pendingRight - right);
        }
        if (directional < 1.0f)
        {
            // At the listener position a source has no direction. Open the
            // image into the head instead of snapping between opposite HRIRs.
            const auto inHead = histories[index].samples[static_cast<size_t> (histories[index].writeIndex + inHeadDelaySamples)];
            const auto inHeadLeft = sourceCount == 2 ? (source == 0 ? inHead : 0.0f) : inHead * 0.70710678118f;
            const auto inHeadRight = sourceCount == 2 ? (source == 1 ? inHead : 0.0f) : inHead * 0.70710678118f;
            left = inHeadLeft + directional * (left - inHeadLeft);
            right = inHeadRight + directional * (right - inHeadRight);
        }
        outputLeft += left;
        outputRight += right;
    }

    // Copy once at the end of a transition, not once per audio sample.
    if (fading && ! filterCrossfade.isSmoothing())
        activeFilters = pendingFilters;
}
