#include "ExciterEngine.h"

namespace
{
    float envelopeCoefficient (double sampleRate, double milliseconds) noexcept
    {
        if (milliseconds <= 0.0)
            return 0.0f;

        return (float) std::exp (-1.0 / (sampleRate * milliseconds * 0.001));
    }

    inline float followEnvelope (float current, float target, float attack, float release) noexcept
    {
        const auto coeff = target > current ? attack : release;
        return target + coeff * (current - target);
    }

    inline float smoothStep (float lower, float upper, float value) noexcept
    {
        const auto normalised = juce::jlimit (0.0f, 1.0f, (value - lower) / (upper - lower));
        return normalised * normalised * (3.0f - 2.0f * normalised);
    }

    /*  4차 니를 가진 소프트 클램프: x / (1 + (x/limit)^4)^(1/4).

        2차(= x / sqrt(1 + (x/limit)^2))는 니가 너무 완만해 limit 훨씬 아래의 신호까지
        일률적으로 눌러 효과 자체를 줄인다. 4차는 limit의 절반에서 -0.13 dB로 사실상
        투명하고 limit를 넘는 구간만 확실히 묶는다. pow 대신 제곱근 두 번으로 계산한다. */
    inline float softClamp (float x, float limit) noexcept
    {
        const auto normalised = x / limit;
        const auto squared = normalised * normalised;
        return x / std::sqrt (std::sqrt (1.0f + squared * squared));
    }

    /*  손상된 상태나 호스트가 NaN/Inf를 넘겨도 필터 계수까지 번지지 않게 막는다.
        한 번 NaN이 된 IIR 상태는 스스로 회복하지 못한다. */
    inline float finiteOr (float value, float fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

}

/*  4+4단 올패스 힐베르트 네트워크.

    아래 값은 극 반경 a이고, 2차 올패스의 계수로는 **a²**를 써야 한다.
    지연 z^-1은 **branchA 쪽 입력**에 건다.

    이 두 가지(계수 제곱, 지연 위치)를 Tests/DSPHarness --diagnose 로 8가지 조합을
    모두 실측해 확정했다. 잘못 조합하면 |z|² 리플이 20 dB까지 벌어져 해석신호가
    성립하지 않고, 결과적으로 깨끗한 주파수 배가가 아니라 평범한 x² 웨이브셰이핑이 된다.
    실제로 그 상태에서 청취 실패가 났다.

    확정 조합의 |z|² 리플: 1–30 kHz에서 최악 0.106 dB (이미지 억압 약 -50 dB). */
void ExciterEngine::AnalyticPair::prepare() noexcept
{
    static constexpr double poleRadiusA[4]
        { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    static constexpr double poleRadiusB[4]
        { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };

    for (int i = 0; i < 4; ++i)
    {
        branchA[i].coefficient = poleRadiusA[i] * poleRadiusA[i];
        branchB[i].coefficient = poleRadiusB[i] * poleRadiusB[i];
    }

    reset();
}

void ExciterEngine::AnalyticPair::reset() noexcept
{
    for (auto& section : branchA) section.reset();
    for (auto& section : branchB) section.reset();
    previousInput = 0.0f;
}

void ExciterEngine::prepare (double newSampleRate, int maximumBlockSize, int numChannels)
{
    jassert (newSampleRate > 0.0);
    jassert (maximumBlockSize > 0);
    jassert (numChannels > 0);

    sampleRate       = newSampleRate;
    maxBlockSize     = juce::jmax (1, maximumBlockSize);
    preparedChannels = juce::jmax (1, numChannels);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) preparedChannels,
        (size_t) oversamplingFactorLog2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true,     // maximum quality
        true);    // integer latency — 드라이를 정수 샘플로 정렬하기 위해 필요하다

    oversampling->initProcessing ((size_t) maxBlockSize);
    latencySamples = juce::roundToInt (oversampling->getLatencyInSamples());

    bandBuffer.setSize (preparedChannels, maxBlockSize, false, false, true);
    bandCopy.setSize (preparedChannels, maxBlockSize, false, false, true);
    resonanceControl.setSize (resonanceBands, maxBlockSize, false, false, true);

    envelopeControl.assign ((size_t) maxBlockSize, envelopeFloor);
    characterControl.assign ((size_t) maxBlockSize, 0.0f);
    amountControl.assign ((size_t) maxBlockSize, 0.0f);
    brightControl.assign ((size_t) maxBlockSize, 0.0f);
    addedMixControl.assign ((size_t) maxBlockSize, 1.0f);

    channels.clear();
    channels.resize ((size_t) preparedChannels);

    for (auto& channel : channels)
    {
        channel.dryDelay.assign ((size_t) juce::jmax (1, latencySamples), 0.0f);
        channel.analytic.prepare();
    }

    const auto nyquist = (float) (sampleRate * 0.5);

    for (int band = 0; band < resonanceBands; ++band)
    {
        const auto centre = juce::jlimit (20.0f, nyquist * 0.9f,
                                          resonanceCentreHz[(size_t) band]);
        const auto coefficients = ArrayCoefficients::makeBandPass (
            sampleRate, centre, resonanceQ);

        for (auto& channel : channels)
        {
            *channel.resonanceDetector[band].coefficients = coefficients;
            *channel.resonanceProtector[band].coefficients = coefficients;
        }
    }

    /*  정규화 엔벌로프는 입력 크기를 빠르게 따라야 한다.

        느린 어택은 시작 구간에서 x/env를 클램프에 오래 붙여 배음 엔벌로프를 크게
        변조한다. --diagnose의 동적 두 톤 검사에서 20 ms는 치맛자락이 -2.5 dB까지
        올라갔고, 1 ms는 약 -23 dB로 낮았다. */
    normAttackCoeff  = envelopeCoefficient (sampleRate, 1.0);
    normReleaseCoeff = envelopeCoefficient (sampleRate, 150.0);
    resonanceEnvelopeAttackCoeff  = envelopeCoefficient (sampleRate, 8.0);
    resonanceEnvelopeReleaseCoeff = envelopeCoefficient (sampleRate, 120.0);
    resonanceScoreAttackCoeff     = envelopeCoefficient (sampleRate, 15.0);
    resonanceScoreReleaseCoeff    = envelopeCoefficient (sampleRate, 180.0);

    constexpr auto smoothingSeconds = 0.02;
    tuneSmoothed.reset (sampleRate, smoothingSeconds);
    amountSmoothed.reset (sampleRate, smoothingSeconds);
    characterSmoothed.reset (sampleRate, smoothingSeconds);
    widthSmoothed.reset (sampleRate, smoothingSeconds);
    addedMixSmoothed.reset (sampleRate, bypassFadeSeconds);

    currentTuneHz = 0.0f;
    prepared = true;

    const auto initialTune = tuneSmoothed.getTargetValue() > 0.0f ? tuneSmoothed.getTargetValue()
                                                                  : Settings{}.tuneHz;
    updateFilters (initialTune);
    reset();
}

void ExciterEngine::reset()
{
    normEnvelope = 0.0f;
    filterUpdateSamplesRemaining = 0;
    addedMixPrimed = false;   // 다음 process()가 페이드 없이 그 블록의 bypass 상태에서 시작한다.
    resonanceEnvelope.fill (0.0f);
    resonanceScore.fill (0.0f);
    resonanceControl.clear();

    if (oversampling != nullptr)
        oversampling->reset();

    for (auto& channel : channels)
    {
        for (auto& filter : channel.bandHighPass)     filter.reset();
        for (auto& filter : channel.bandLowPass)      filter.reset();
        for (auto& filter : channel.residualHighPass) filter.reset();

        channel.postLowPass.reset();
        channel.brightShelf.reset();
        for (auto& filter : channel.resonanceDetector)  filter.reset();
        for (auto& filter : channel.resonanceProtector) filter.reset();
        channel.analytic.reset();

        std::fill (channel.dryDelay.begin(), channel.dryDelay.end(), 0.0f);
        channel.dryWriteIndex = 0;
    }
}

void ExciterEngine::setSettings (const Settings& s) noexcept
{
    const auto currentTune = tuneSmoothed.getTargetValue() > 0.0f ? tuneSmoothed.getTargetValue()
                                                                  : Settings{}.tuneHz;

    tuneSmoothed.setTargetValue (juce::jlimit (20.0f, (float) (sampleRate * 0.4),
                                               finiteOr (s.tuneHz, currentTune)));
    amountSmoothed.setTargetValue (juce::jlimit (
        0.0f, 1.0f, finiteOr (s.amount, amountSmoothed.getTargetValue())));
    characterSmoothed.setTargetValue (juce::jlimit (
        0.0f, 1.0f, finiteOr (s.character, characterSmoothed.getTargetValue())));
    widthSmoothed.setTargetValue (juce::jlimit (
        0.0f, 2.0f, finiteOr (s.width, widthSmoothed.getTargetValue())));
}

void ExciterEngine::snapSettings (const Settings& s) noexcept
{
    setSettings (s);
    tuneSmoothed.setCurrentAndTargetValue (tuneSmoothed.getTargetValue());
    amountSmoothed.setCurrentAndTargetValue (amountSmoothed.getTargetValue());
    characterSmoothed.setCurrentAndTargetValue (characterSmoothed.getTargetValue());
    widthSmoothed.setCurrentAndTargetValue (widthSmoothed.getTargetValue());
}

void ExciterEngine::updateFilters (float tuneHz)
{
    const auto nyquist = (float) (sampleRate * 0.5);
    const auto bandHz  = juce::jlimit (20.0f, nyquist * 0.8f, tuneHz);

    if (juce::approximatelyEqual (bandHz, currentTuneHz))
        return;

    currentTuneHz = bandHz;
    constexpr auto butterworthQ = juce::MathConstants<float>::sqrt2 * 0.5f;

    /*  필터 객체는 prepare()에서 한 번만 생성된다. ArrayCoefficients로 계산한 숫자만
        기존 객체에 복사해 오디오 스레드에서 ref-counted 객체를 할당·해제하지 않는다. */
    const auto brightShelfHz = juce::jlimit (60.0f, nyquist * 0.8f, brightShelfCornerFor (bandHz));
    const auto brightShelfCoeffs = ArrayCoefficients::makeHighShelf (
        sampleRate, brightShelfHz, butterworthQ,
        juce::Decibels::decibelsToGain (brightShelfMaxDb));

    const auto bandHighCoeffs = ArrayCoefficients::makeHighPass (sampleRate, bandHz, butterworthQ);

    const auto bandTopHz = juce::jlimit (30.0f, nyquist * 0.95f, bandHz * bandWidthRatio);
    const auto bandLowCoeffs = ArrayCoefficients::makeLowPass (sampleRate, bandTopHz, butterworthQ);

    const auto residualHz = juce::jlimit (20.0f, nyquist * 0.9f, bandHz * residualHighPassRatio);
    const auto residualCoeffs = ArrayCoefficients::makeHighPass (sampleRate, residualHz, butterworthQ);

    const auto lowPassHz = juce::jmin (postLowPassHz, nyquist * 0.9f);
    const auto lowPassCoeffs = ArrayCoefficients::makeLowPass (sampleRate, lowPassHz, butterworthQ);

    for (auto& channel : channels)
    {
        for (auto& filter : channel.bandHighPass)     *filter.coefficients = bandHighCoeffs;
        for (auto& filter : channel.bandLowPass)      *filter.coefficients = bandLowCoeffs;
        for (auto& filter : channel.residualHighPass) *filter.coefficients = residualCoeffs;

        *channel.postLowPass.coefficients = lowPassCoeffs;
        *channel.brightShelf.coefficients = brightShelfCoeffs;
    }
}

void ExciterEngine::process (juce::AudioBuffer<float>& buffer, bool bypassed) noexcept
{
    if (! prepared || oversampling == nullptr)
        return;

    const auto numChannels  = buffer.getNumChannels();
    const auto totalSamples = buffer.getNumSamples();

    if (totalSamples <= 0)
        return;

    if (numChannels != preparedChannels)
    {
        // prepareToPlay가 실제 버스 구성으로 다시 호출되기 전의 과도 상태.
        jassertfalse;
        return;
    }

    const auto addedMixTarget = bypassed ? 0.0f : 1.0f;

    if (! addedMixPrimed)
    {
        addedMixSmoothed.setCurrentAndTargetValue (addedMixTarget);
        addedMixPrimed = true;
    }
    else
    {
        addedMixSmoothed.setTargetValue (addedMixTarget);
    }

    for (int position = 0; position < totalSamples;)
    {
        if (filterUpdateSamplesRemaining <= 0)
        {
            updateFilters (tuneSmoothed.getCurrentValue());
            filterUpdateSamplesRemaining = filterUpdateInterval;
        }

        const auto count = juce::jmin (filterUpdateSamplesRemaining,
                                       juce::jmin (maxBlockSize, totalSamples - position));
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), numChannels, position, count);
        processChunk (view, numChannels, count);
        tuneSmoothed.skip (count);
        position += count;
        filterUpdateSamplesRemaining -= count;
    }
}

void ExciterEngine::processChunk (juce::AudioBuffer<float>& buffer, int numChannels,
                                  int numSamples) noexcept
{
    // 1) FOCUS 계수는 process()가 호스트 블록 경계를 넘어 유지하는 32-sample 주기로 갱신한다.

    // 2) 여기 대역 추출: TUNE ~ 2×TUNE 의 1옥타브.
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* source = buffer.getReadPointer (channel);
        auto* band   = bandBuffer.getWritePointer (channel);
        auto& state  = channels[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            auto value = source[i];
            value = state.bandHighPass[0].processSample (value);
            value = state.bandHighPass[1].processSample (value);
            value = state.bandLowPass[0].processSample (value);
            value = state.bandLowPass[1].processSample (value);
            band[i] = value;
        }

        state.bandHighPass[0].snapToZero();
        state.bandHighPass[1].snapToZero();
        state.bandLowPass[0].snapToZero();
        state.bandLowPass[1].snapToZero();
    }

    /*  배음이 떨어지는 위치에 따른 양. FOCUS는 청크 단위로만 바뀌므로 여기서 한 번 계산한다.
        로그 주파수 기준으로 보간해 노브 전 구간에서 고르게 변한다. */
    const auto focusHarmonicGain = 1.0f + (harmonicTiltMaxGain - 1.0f)
        * smoothStep (std::log (harmonicTiltLowHz), std::log (harmonicTiltHighHz),
                      std::log (juce::jmax (20.0f, currentTuneHz)));

    // 3) 베이스 레이트 제어 신호. 엔벌로프는 채널 링크해 스테레오 이미지를 유지한다.
    const auto shelfGainAtMaximum = juce::Decibels::decibelsToGain (brightShelfMaxDb);

    const auto brightFromAmount = [shelfGainAtMaximum] (float amount) noexcept
    {
        const auto shelfGain = juce::Decibels::decibelsToGain (amount * brightShelfMaxDb);
        return (shelfGain - 1.0f) / (shelfGainAtMaximum - 1.0f);
    };

    // BRIGHT가 멈춰 있으면 dB → gain 변환(pow)을 샘플마다 반복하지 않는다. 결과는 같다.
    const auto amountIsSmoothing = amountSmoothed.isSmoothing();
    const auto steadyBright = brightFromAmount (amountSmoothed.getCurrentValue());

    for (int i = 0; i < numSamples; ++i)
    {
        float linkedMagnitude = 0.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            linkedMagnitude = juce::jmax (linkedMagnitude, std::abs (bandBuffer.getSample (channel, i)));

        normEnvelope = followEnvelope (normEnvelope, linkedMagnitude, normAttackCoeff, normReleaseCoeff);

        envelopeControl[(size_t) i]  = juce::jmax (envelopeFloor, normEnvelope);
        characterControl[(size_t) i] = characterSmoothed.getNextValue();

        const auto bright = amountIsSmoothing ? brightFromAmount (amountSmoothed.getNextValue())
                                              : steadyBright;
        brightControl[(size_t) i] = bright;
        amountControl[(size_t) i] = bright * wetScale * focusHarmonicGain;
    }

    // 4) 오버샘플링 후 해석신호 배음 생성.
    auto bandBlock = juce::dsp::AudioBlock<float> (bandBuffer).getSubBlock (0, (size_t) numSamples);
    auto osBlock = oversampling->processSamplesUp (bandBlock);

    const auto osSamples = (int) osBlock.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* data  = osBlock.getChannelPointer ((size_t) channel);
        auto& state = channels[(size_t) channel];

        for (int j = 0; j < osSamples; ++j)
        {
            const auto controlIndex = (size_t) juce::jmin (numSamples - 1, j >> oversamplingFactorLog2);

            const auto envelope  = envelopeControl[controlIndex];
            const auto character = characterControl[controlIndex];

            // 해석신호 z = real + j·imaginary. 두 경로는 90° 위상차를 갖는다.
            float real = 0.0f, imaginary = 0.0f;
            state.analytic.process (data[j], real, imaginary);

            /*  엔벌로프로 정규화해 생성기의 동작점을 고정한다.

                상한은 **크기 |z|에만** 걸고 실수부·허수부를 같은 비율로 줄인다.
                두 성분을 따로 클램프하면 위상 φ가 왜곡되어(원이 사각형으로 눌림)
                해석신호 관계가 깨지고, cos2φ가 깨끗한 배가가 아니게 된다.
                즉 없애려던 IMD가 클램프에서 다시 생긴다. */
            const auto scale = generatorDrive / envelope;
            auto a = real * scale;
            auto b = imaginary * scale;

            const auto magnitude = std::sqrt (a * a + b * b);

            if (magnitude > generatorClamp)
            {
                const auto reduction = softClamp (magnitude, generatorClamp) / magnitude;
                a *= reduction;
                b *= reduction;
            }

            // Re(z^2), Re(z^3). 합 성분만 만들고 차음·DC·자기변조 항은 존재하지 않는다.
            const auto doubled = a * a - b * b;
            const auto tripled = a * (a * a - 3.0f * b * b);

            const auto vintageHarmonics = vintageEvenWeight * doubled + vintageOddWeight * tripled;
            const auto modernHarmonics  = modernEvenWeight  * doubled + modernOddWeight  * tripled;

            const auto blended = (1.0f - character) * (vintageHarmonics * vintageMakeup)
                               + character          * (modernHarmonics * modernMakeup);

            data[j] = softClamp (blended, residualCeiling) * envelope;
        }
    }

    oversampling->processSamplesDown (bandBlock);

    /*  5) 출력 하이패스로 여기 대역을 제거하고 최상단을 정리한다.

        이전의 zero-lag 기음 투영은 오버샘플링 레이턴시가 있는 wet과 지연되지 않은
        reference를 비교해 오히려 현재 입력을 wet에 주입했다. 현재 생성기는 Re(z²/z³)를
        직접 출력해 선형 기음 항이 없으므로 해당 투영을 제거한다. */
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* band  = bandBuffer.getWritePointer (channel);
        auto& state = channels[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            auto wet = band[i];
            wet = state.residualHighPass[0].processSample (wet);
            wet = state.residualHighPass[1].processSample (wet);
            band[i] = state.postLowPass.processSample (wet);
        }

        state.residualHighPass[0].snapToZero();
        state.residualHighPass[1].snapToZero();
        state.postLowPass.snapToZero();
    }

    /*  6) 드라이를 지연 정렬하고, 거기서 BRIGHT 셸프의 **더해지는 성분**을 뽑아
           배음과 합친다.

           셸프를 드라이에 직접 걸지 않고 delta = shelf(dry) - dry 로 분리하는 이유는
           두 가지다. 하나는 원음 정위를 보존한 채 더해진 성분만 WIDTH로 넓히기 위해서,
           다른 하나는 BRIGHT = 0에서 출력이 드라이와 샘플 단위로 같아야 하기 때문이다. */
    // 6-a) 모든 채널의 드라이를 먼저 지연 정렬하고 원래 셸프 delta를 만든다.
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* output = buffer.getWritePointer (channel);
        auto* shelfDelta = bandCopy.getWritePointer (channel);
        auto& state = channels[(size_t) channel];

        const auto useDelay = latencySamples > 0
                           && (int) state.dryDelay.size() >= latencySamples;

        for (int i = 0; i < numSamples; ++i)
        {
            auto dry = output[i];

            if (useDelay)
            {
                const auto delayed = state.dryDelay[(size_t) state.dryWriteIndex];
                state.dryDelay[(size_t) state.dryWriteIndex] = dry;
                state.dryWriteIndex = (state.dryWriteIndex + 1) % latencySamples;
                dry = delayed;
            }

            output[i] = dry;
            shelfDelta[i] = state.brightShelf.processSample (dry) - dry;
        }

        state.brightShelf.snapToZero();
    }

    // 6-b) 정렬된 원본에서 국소 공진을 검출한다. 채널별 에너지를 max-link하므로
    //      한쪽 또는 역상 Side에만 있는 피크도 양쪽에서 같은 보호량을 받는다.
    //      FOCUS 가중치는 청크 동안 변하지 않으므로 샘플 루프 밖에서 한 번만 계산한다.
    std::array<float, resonanceBands> focusMask {};
    {
        const auto shelfCorner = juce::jmax (40.0f, brightShelfCornerFor (currentTuneHz));

        for (int band = 1; band < resonanceBands; ++band)
            focusMask[(size_t) band] = smoothStep (0.75f, 1.0f,
                                                   resonanceCentreHz[(size_t) band] / shelfCorner);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        std::array<float, resonanceBands> bandDb {};

        for (int band = 0; band < resonanceBands; ++band)
        {
            float linkedPower = 0.0f;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto& detector = channels[(size_t) channel].resonanceDetector[band];
                const auto detected = detector.processSample (buffer.getSample (channel, i));
                linkedPower = juce::jmax (linkedPower, detected * detected);
            }

            resonanceEnvelope[(size_t) band] = followEnvelope (
                resonanceEnvelope[(size_t) band], linkedPower,
                resonanceEnvelopeAttackCoeff, resonanceEnvelopeReleaseCoeff);
            bandDb[(size_t) band] = 10.0f * std::log10 (
                juce::jmax (1.0e-12f, resonanceEnvelope[(size_t) band]));
        }

        std::array<float, resonanceBands> candidate {};
        float candidateSum = 0.0f;

        for (int band = 1; band < resonanceBands; ++band)
        {
            // The top band has no upper neighbour.  Extrapolating the local
            // log-frequency slope keeps a smooth air shelf neutral while still
            // allowing a narrow 17 kHz resonance to be protected.
            const auto neighbourDb = band == resonanceBands - 1
                                   ? 2.0f * bandDb[(size_t) (band - 1)]
                                       - bandDb[(size_t) (band - 2)]
                                   : 0.5f * (bandDb[(size_t) (band - 1)]
                                                 + bandDb[(size_t) (band + 1)]);
            const auto prominence = bandDb[(size_t) band] - neighbourDb;
            const auto prominenceMask = smoothStep (resonanceProminenceStartDb,
                                                     resonanceProminenceFullDb,
                                                     prominence);
            const auto levelMask = smoothStep (resonanceLevelGateStartDb,
                                                resonanceLevelGateFullDb,
                                                bandDb[(size_t) band]);
            const auto target = prominenceMask * levelMask;

            resonanceScore[(size_t) band] = followEnvelope (
                resonanceScore[(size_t) band], target,
                resonanceScoreAttackCoeff, resonanceScoreReleaseCoeff);
            candidate[(size_t) band] = resonanceScore[(size_t) band] * focusMask[(size_t) band];
            candidateSum += candidate[(size_t) band];
        }

        resonanceScore.front() = 0.0f;
        const auto normaliser = juce::jmax (1.0f, candidateSum);

        for (int band = 0; band < resonanceBands; ++band)
            resonanceControl.setSample (
                band, i, resonanceProtectionBudget * candidate[(size_t) band] / normaliser);
    }

    // 6-c) 보호 필터는 셸프 delta에만 직렬로 건다. 각 stage가 delta를 키우지
    //      않고 전체 감쇠 예산도 25 % 이하여서 BRIGHT의 상승 방향은 유지된다.
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* harmonics = bandBuffer.getWritePointer (channel);
        auto* added = bandCopy.getWritePointer (channel);
        auto& state = channels[(size_t) channel];

        for (int i = 0; i < numSamples; ++i)
        {
            auto safeShelfDelta = added[i];

            for (int band = 0; band < resonanceBands; ++band)
            {
                const auto local = state.resonanceProtector[band].processSample (safeShelfDelta);
                safeShelfDelta -= resonanceControl.getSample (band, i) * local;
            }

            added[i] = safeShelfDelta * brightControl[(size_t) i]
                     + harmonics[i] * amountControl[(size_t) i];
        }

        for (auto& filter : state.resonanceDetector)  filter.snapToZero();
        for (auto& filter : state.resonanceProtector) filter.snapToZero();
    }

    /*  7) 더해진 성분만 좌우로 넓힌다. 드라이 정위는 그대로이고 새로 만든 상단만 열린다.
           모노 입력은 Side가 0이라 변화가 없다. */
    if (numChannels == 2)
    {
        auto* left  = bandCopy.getWritePointer (0);
        auto* right = bandCopy.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto width = widthSmoothed.getNextValue();
            const auto mid  = (left[i] + right[i]) * 0.5f;
            const auto side = (left[i] - right[i]) * 0.5f * width;
            left[i]  = mid + side;
            right[i] = mid - side;
        }
    }
    else
    {
        widthSmoothed.skip (numSamples);
    }

    /*  8) 원본 드라이는 건드리지 않고 added 경로만 합친다.

        bypass 중에는 합산을 생략하되 내부 상태는 계속 진행한다. bypass가 바뀌는 동안에만
        added를 선형 페이드해 전환 클릭을 막는다. 페이드가 끝난 두 정상상태는 각각
        이전과 샘플 단위로 같다(처리 = dry + added, bypass = dry). */
    if (addedMixSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
            addedMixControl[(size_t) i] = addedMixSmoothed.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* output = buffer.getWritePointer (channel);
            const auto* added = bandCopy.getReadPointer (channel);

            for (int i = 0; i < numSamples; ++i)
                output[i] += added[i] * addedMixControl[(size_t) i];
        }
    }
    else if (addedMixSmoothed.getCurrentValue() > 0.5f)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* output = buffer.getWritePointer (channel);
            const auto* added = bandCopy.getReadPointer (channel);

            for (int i = 0; i < numSamples; ++i)
                output[i] = output[i] + added[i];
        }
    }
}
