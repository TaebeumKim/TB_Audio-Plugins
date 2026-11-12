#include "AmbientEngine.h"

#include <algorithm>
#include <cmath>

namespace tbambient
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
    constexpr float kAirShelf2kDbPer100m = 4.0f;
    constexpr float kAirShelf8kDbPer100m = 10.0f;
    constexpr float kListenerHeightM = 1.5f;
    constexpr float kSpeedOfSoundMps = 343.0f;
    constexpr float kGroundToneQ = 0.55f;

    inline float dbToGain (float db) noexcept
    {
        return std::pow (10.0f, db * 0.05f);
    }

    inline float logarithmicInterpolate (float a, float b, float amount) noexcept
    {
        const float safeA = std::max (a, 1.0e-3f);
        const float safeB = std::max (b, 1.0e-3f);
        return std::exp (std::log (safeA)
                         + std::clamp (amount, 0.0f, 1.0f)
                           * (std::log (safeB) - std::log (safeA)));
    }

    inline float materialHardnessForSigma (float sigmaKPa) noexcept
    {
        constexpr float softReferenceKPa = 10.0f;
        constexpr float hardReferenceKPa = 300000.0f;
        return std::clamp (
            std::log (std::max (sigmaKPa, softReferenceKPa) / softReferenceKPa)
            / std::log (hardReferenceKPa / softReferenceKPa),
            0.0f, 1.0f);
    }
}

//==============================================================================
// Biquad
//==============================================================================

void Biquad::setBypass() noexcept
{
    b0 = 1.0f;
    b1 = b2 = a1 = a2 = 0.0f;
}

void Biquad::setPeak (double fs, double freq, double gainDb, double q) noexcept
{
    if (std::abs (gainDb) < 1.0e-4)
    {
        setBypass();
        return;
    }

    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
    const double c = std::cos (w);
    const double alpha = std::sin (w) / (2.0 * std::max (q, 0.05));
    const double a0 = 1.0 + alpha / A;

    b0 = (float) ((1.0 + alpha * A) / a0);
    b1 = (float) ((-2.0 * c) / a0);
    b2 = (float) ((1.0 - alpha * A) / a0);
    a1 = b1;
    a2 = (float) ((1.0 - alpha / A) / a0);
}

void Biquad::setHighShelf (double fs, double freq, double gainDb, double q) noexcept
{
    if (std::abs (gainDb) < 1.0e-4)
    {
        setBypass();
        return;
    }

    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
    const double c = std::cos (w), s = std::sin (w);
    const double alpha = s / (2.0 * q);
    const double t = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1.0) - (A - 1.0) * c + t;

    b0 = (float) (A * ((A + 1.0) + (A - 1.0) * c + t) / a0);
    b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0);
    b2 = (float) (A * ((A + 1.0) + (A - 1.0) * c - t) / a0);
    a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * c) / a0);
    a2 = (float) (((A + 1.0) - (A - 1.0) * c - t) / a0);
}

void Biquad::setLowShelf (double fs, double freq, double gainDb, double q) noexcept
{
    if (std::abs (gainDb) < 1.0e-4)
    {
        setBypass();
        return;
    }

    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * std::clamp (freq, 10.0, fs * 0.49) / fs;
    const double c = std::cos (w), s = std::sin (w);
    const double alpha = s / (2.0 * q);
    const double t = 2.0 * std::sqrt (A) * alpha;
    const double a0 = (A + 1.0) + (A - 1.0) * c + t;

    b0 = (float) (A * ((A + 1.0) - (A - 1.0) * c + t) / a0);
    b1 = (float) (2.0 * A * ((A - 1.0) - (A + 1.0) * c) / a0);
    b2 = (float) (A * ((A + 1.0) - (A - 1.0) * c - t) / a0);
    a1 = (float) (-2.0 * ((A - 1.0) + (A + 1.0) * c) / a0);
    a2 = (float) (((A + 1.0) + (A - 1.0) * c - t) / a0);
}

float Biquad::process (float x) noexcept
{
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

//==============================================================================
// AmbientEngine
//==============================================================================

void AmbientEngine::resetBank (FilterBank& bank) noexcept
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        bank.lowTone[ch].reset();
        bank.highTone[ch].reset();
        bank.veryHighAir[ch].reset();
        bank.groundTone[ch].reset();
    }
}

void AmbientEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs = std::max (sampleRate, 1.0);
    channels = std::clamp (numChannels, 1, kMaxChannels);
    transitionLengthSamples = std::max (1, (int) std::lround (fs * 0.020));
    activeBank = 0;
    targetBank = 1;
    transitionPosition = 0;
    transitionActive = false;
    prepared = true;
    coefficientsDirty = true;
    continuousCoefficientsDirty = true;
    reset();
}

void AmbientEngine::reset() noexcept
{
    resetBank (filterBanks[0]);
    resetBank (filterBanks[1]);

    targetBank = 1 - activeBank;
    transitionPosition = 0;
    transitionActive = false;
    hasProcessedAudio = false;

    // A reset is an audio discontinuity, so there is no preceding path to
    // preserve. Apply the latest host-restored/offline-render settings now;
    // the first 20 ms must not be coloured by the previous profile.
    if (prepared)
    {
        configureBank (filterBanks[activeBank], spaceIndex);
        currentSigmaKPa = filterBanks[activeBank].sigmaKPa;
        currentCosTheta = filterBanks[activeBank].cosTheta;
        coefficientsDirty = false;
        continuousCoefficientsDirty = false;
    }
}

void AmbientEngine::setSpace (int index) noexcept
{
    const int value = std::clamp (index, 0, kNumSpaces - 1);
    if (value != spaceIndex)
    {
        spaceIndex = value;
        coefficientsDirty = true;
    }
}

void AmbientEngine::setDistance (float metres) noexcept
{
    const float value = std::clamp (metres, 1.0f, 100.0f);
    if (std::abs (value - distanceM) > 1.0e-4f)
    {
        distanceM = value;
        continuousCoefficientsDirty = true;
    }
}

void AmbientEngine::setGround (float norm) noexcept
{
    const float value = std::clamp (norm, 0.0f, 1.0f);
    if (std::abs (value - groundNorm) > 1.0e-5f)
    {
        groundNorm = value;
        continuousCoefficientsDirty = true;
    }
}

void AmbientEngine::setOpenness (float norm) noexcept
{
    const float value = std::clamp (norm, 0.0f, 1.0f);
    if (std::abs (value - openness) > 1.0e-5f)
    {
        openness = value;
        continuousCoefficientsDirty = true;
    }
}

void AmbientEngine::setHeight (float metres) noexcept
{
    const float value = std::clamp (metres, 0.2f, 5.0f);
    if (std::abs (value - heightM) > 1.0e-4f)
    {
        heightM = value;
        continuousCoefficientsDirty = true;
    }
}

void AmbientEngine::setMix (float norm) noexcept
{
    mix = std::clamp (norm, 0.0f, 1.0f);
}

void AmbientEngine::configureBank (FilterBank& bank, int profileIndex) noexcept
{
    if (! prepared)
        return;

    const SpaceDef& sp = kSpaces[profileIndex];

    if (groundNorm <= 0.5f)
        bank.sigmaKPa = logarithmicInterpolate (
            sp.ground.rangeMinKPa, sp.ground.flowResistivityKPa, groundNorm * 2.0f);
    else
        bank.sigmaKPa = logarithmicInterpolate (
            sp.ground.flowResistivityKPa, sp.ground.rangeMaxKPa,
            (groundNorm - 0.5f) * 2.0f);

    const float heightSum = heightM + kListenerHeightM;
    const float reflectedDistance = std::sqrt (distanceM * distanceM
                                                + heightSum * heightSum);
    const float directHeight = heightM - kListenerHeightM;
    const float directDistance = std::sqrt (distanceM * distanceM
                                             + directHeight * directHeight);
    const float pathDifference = std::max (reflectedDistance - directDistance, 0.001f);

    bank.cosTheta = heightSum / std::max (reflectedDistance, 1.0e-3f);
    const float materialHardness = materialHardnessForSigma (bank.sigmaKPa);

    // Retain the useful geometry cue of the first ground interaction, but use
    // one broad minimum-phase bell rather than summing a delayed copy. The Q
    // and depth bounds make repeated/narrow comb notches impossible.
    const float materialCentreScale = 0.8f + 0.4f * materialHardness;
    const float groundCentreHz = std::clamp (
        kSpeedOfSoundMps / (2.0f * pathDifference) * materialCentreScale,
        160.0f, 2800.0f);
    const float geometryStrength = 0.65f + 0.35f * (1.0f - bank.cosTheta);
    const float hardnessStrength = 0.65f + 0.35f * materialHardness;
    const float groundDepthDb = std::clamp (
        sp.ground.levelTrimDb * openness * geometryStrength * hardnessStrength,
        -9.0f, 0.0f);

    const float per10m = distanceM / 10.0f;
    const float per100m = distanceM / 100.0f;
    const float distanceGain = std::min (kReferenceDistanceM / distanceM, 1.0f);
    const float excessMidDb = -per10m * sp.direct.excessAttenDbPer10m.mid;
    bank.perspectiveGain = distanceGain * dbToGain (excessMidDb);

    const float proximityDb = sp.direct.proximityLfTrimDb
        * std::clamp (1.0f - distanceM / 40.0f, 0.0f, 1.0f);
    const float excessLowRelativeDb = per10m
        * (sp.direct.excessAttenDbPer10m.mid - sp.direct.excessAttenDbPer10m.low);
    const float lowShelfDb = excessLowRelativeDb + proximityDb
                             + openness * sp.tone.lowShelfDb;

    const float excessHighRelativeDb = -per10m
        * (sp.direct.excessAttenDbPer10m.high - sp.direct.excessAttenDbPer10m.mid);
    const float groundHighScale = 1.35f - 0.7f * materialHardness;
    const float highShelfDb = excessHighRelativeDb
                              - kAirShelf2kDbPer100m * per100m
                              + openness * (sp.tone.highShelfDb
                                            + sp.ground.hfShelfDb * groundHighScale);
    const float veryHighShelfDb = -kAirShelf8kDbPer100m * per100m;

    bank.monoFocus = sp.direct.monoFocusWithDistance
                     * std::clamp ((distanceM - 5.0f) / 60.0f, 0.0f, 1.0f);

    for (int ch = 0; ch < channels; ++ch)
    {
        bank.lowTone[ch].setLowShelf (fs, kBandCrossoverLowMidHz, lowShelfDb);
        bank.highTone[ch].setHighShelf (fs, kBandCrossoverMidHighHz, highShelfDb);
        bank.veryHighAir[ch].setHighShelf (fs, 8000.0, veryHighShelfDb);
        bank.groundTone[ch].setPeak (fs, groundCentreHz, groundDepthDb, kGroundToneQ);
    }

    bank.configuredSpace = profileIndex;
    bank.configuredDistance = distanceM;
    bank.configuredGround = groundNorm;
    bank.configuredOpenness = openness;
    bank.configuredHeight = heightM;
}

bool AmbientEngine::bankMatchesCurrentParameters (const FilterBank& bank) const noexcept
{
    return bank.configuredSpace == spaceIndex
           && bank.configuredDistance == distanceM
           && bank.configuredGround == groundNorm
           && bank.configuredOpenness == openness
           && bank.configuredHeight == heightM;
}

void AmbientEngine::applyPendingCoefficients() noexcept
{
    if (! prepared || (! coefficientsDirty && ! continuousCoefficientsDirty))
        return;

    // Before any audio (including immediately after reset), there is no old
    // audible state to preserve. Configure directly so host state restore and
    // independent offline renders start wholly in the requested profile.
    if (! hasProcessedAudio)
    {
        transitionActive = false;
        transitionPosition = 0;
        targetBank = 1 - activeBank;
        configureBank (filterBanks[activeBank], spaceIndex);
        resetBank (filterBanks[activeBank]);
        currentSigmaKPa = filterBanks[activeBank].sigmaKPa;
        currentCosTheta = filterBanks[activeBank].cosTheta;
        coefficientsDirty = false;
        continuousCoefficientsDirty = false;
        return;
    }

    // Continuous controls can arrive at very small block intervals. Updating
    // them in place preserves that timing instead of stretching every gesture
    // into another 20 ms transition. During a profile fade, each live bank
    // keeps its own profile identity while receiving the shared controls.
    if (continuousCoefficientsDirty)
    {
        configureBank (filterBanks[activeBank],
                       filterBanks[activeBank].configuredSpace);

        if (transitionActive)
            configureBank (filterBanks[targetBank],
                           filterBanks[targetBank].configuredSpace);

        const int audibleTarget = transitionActive ? targetBank : activeBank;
        currentSigmaKPa = filterBanks[audibleTarget].sigmaKPa;
        currentCosTheta = filterBanks[audibleTarget].cosTheta;
        continuousCoefficientsDirty = false;
    }

    if (! coefficientsDirty)
        return;

    if (transitionActive)
    {
        if (bankMatchesCurrentParameters (filterBanks[targetBank]))
        {
            coefficientsDirty = false;
            return;
        }

        if (bankMatchesCurrentParameters (filterBanks[activeBank]))
        {
            // Both paths have consumed every raw sample, so returning to the
            // source configuration is a state-safe fade reversal. Preserve
            // the current blend exactly by mirroring its progress.
            const int oldActive = activeBank;
            activeBank = targetBank;
            targetBank = oldActive;
            transitionPosition = std::clamp (
                transitionLengthSamples - transitionPosition,
                0, transitionLengthSamples);
            currentSigmaKPa = filterBanks[targetBank].sigmaKPa;
            currentCosTheta = filterBanks[targetBank].cosTheta;
            coefficientsDirty = false;
        }

        // A third profile needs a third state to begin immediately. Keep only
        // the latest selection and start it after this 20 ms fade; changing
        // either live bank would recreate the DF-II transient.
        return;
    }

    if (bankMatchesCurrentParameters (filterBanks[activeBank]))
    {
        coefficientsDirty = false;
        return;
    }

    targetBank = 1 - activeBank;
    configureBank (filterBanks[targetBank], spaceIndex);
    resetBank (filterBanks[targetBank]);
    currentSigmaKPa = filterBanks[targetBank].sigmaKPa;
    currentCosTheta = filterBanks[targetBank].cosTheta;
    transitionPosition = 0;
    transitionActive = true;
    coefficientsDirty = false;
}

void AmbientEngine::processBank (FilterBank& bank, const float* raw,
                                 float* processed, int numChannels) noexcept
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float value = raw[ch] * bank.perspectiveGain;
        value = bank.lowTone[ch].process (value);
        value = bank.groundTone[ch].process (value);
        value = bank.highTone[ch].process (value);
        value = bank.veryHighAir[ch].process (value);
        processed[ch] = value;
    }

    if (numChannels == 2 && bank.monoFocus > 0.0f)
    {
        const float mid = 0.5f * (processed[0] + processed[1]);
        const float side = 0.5f * (processed[0] - processed[1])
                           * (1.0f - bank.monoFocus);
        processed[0] = mid + side;
        processed[1] = mid - side;
    }
}

void AmbientEngine::process (float* const* data, int numChannels, int numSamples) noexcept
{
    if (! prepared || data == nullptr || numSamples <= 0)
        return;

    if (coefficientsDirty || continuousCoefficientsDirty)
        applyPendingCoefficients();

    const int nch = std::min (numChannels, channels);
    if (nch <= 0)
        return;

    // The processor always supplies a complete pointer array, but keep the
    // engine safe for direct callers and test harnesses that pass an incomplete
    // channel array. Processing with a missing channel would otherwise
    // dereference it inside the per-sample loop.
    for (int ch = 0; ch < nch; ++ch)
        if (data[ch] == nullptr)
            return;

    hasProcessedAudio = true;

    for (int i = 0; i < numSamples; ++i)
    {
        float raw[kMaxChannels] = {};
        float processed[kMaxChannels] = {};
        float targetProcessed[kMaxChannels] = {};

        for (int ch = 0; ch < nch; ++ch)
            raw[ch] = data[ch][i];

        processBank (filterBanks[activeBank], raw, processed, nch);

        if (transitionActive)
        {
            processBank (filterBanks[targetBank], raw, targetProcessed, nch);
            const float amount = (float) transitionPosition
                                 / (float) transitionLengthSamples;

            for (int ch = 0; ch < nch; ++ch)
                processed[ch] += amount * (targetProcessed[ch] - processed[ch]);

            if (++transitionPosition >= transitionLengthSamples)
            {
                activeBank = targetBank;
                targetBank = 1 - activeBank;
                transitionPosition = 0;
                transitionActive = false;
                currentSigmaKPa = filterBanks[activeBank].sigmaKPa;
                currentCosTheta = filterBanks[activeBank].cosTheta;

                // A third/latest profile may have been queued while both
                // banks were live. Start it now so its timing is independent
                // of the host block boundary; its reset target begins
                // consuming raw audio on the next sample.
                if (coefficientsDirty || continuousCoefficientsDirty)
                    applyPendingCoefficients();
            }
        }

        for (int ch = 0; ch < nch; ++ch)
            data[ch][i] = raw[ch] + mix * (processed[ch] - raw[ch]);
    }
}

} // namespace tbambient
