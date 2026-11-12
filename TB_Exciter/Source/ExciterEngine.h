#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

/*  TB_Exciter의 상단 강조·배음 생성 엔진.

    신호 경로:

      입력 ─┬─ 드라이 지연 ────────────────────────────────────────────────────────┐
            │                                                                      │
            ├─ 지연 드라이 → 하이셸프 delta @ min(3×FOCUS, 9k) ───────┐            │
            │                                                         │            │
            └─ 여기 대역 FOCUS ~ 2×FOCUS → 4x 오버샘플링              │            │
               → adaptive 정규화 → 해석신호 Re(z²)/Re(z³)              │            │
               → CLARITY 모프 → 다운샘플링 → 대역 정리 ───────────────+→ WIDTH ────+→ LEVEL

    해석신호 배가는 선형 기음 항과 차음을 만들지 않으므로, 지연이 다른 현재 입력을
    상관시켜 빼는 별도 기음 투영은 사용하지 않는다. BRIGHT = 0 %에서는 지연 정렬된
    드라이와 샘플 단위로 동일하다.
*/
class ExciterEngine final
{
public:
    struct Settings
    {
        float tuneHz    = 3000.0f;   // FOCUS  500 .. 6000
        float amount    = 0.40f;     // BRIGHT 0 .. 1
        float character = 0.45f;     // CLARITY 0 = 따뜻함, 1 = 맑음
        float width     = 1.30f;     // WIDTH  0 = 모노, 1 = 원본 폭, 2 = 최대
    };

    /** 오버샘플링 배수는 2^2 = 4x. 앨리어싱 목표는 -100 dBFS 이하다. */
    static constexpr int oversamplingFactorLog2 = 2;
    static constexpr int oversamplingFactor     = 1 << oversamplingFactorLog2;

    void prepare (double newSampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /** 스무딩을 거쳐 목표값으로 이동한다. 오디오 스레드에서 블록마다 호출한다. */
    void setSettings (const Settings& newSettings) noexcept;

    /** 스무딩 없이 즉시 반영한다. prepare 직후와 상태 복원에만 사용한다. */
    void snapSettings (const Settings& newSettings) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }

    /** bypass 전환 크로스페이드 길이. Processor의 LEVEL 전환도 같은 길이를 쓴다. */
    static constexpr double bypassFadeSeconds = 0.01;

    /** 제자리 처리: 지연 드라이와 BRIGHT 상단 성분을 합친 출력으로 대체한다.

        bypassed가 바뀌면 added 경로를 bypassFadeSeconds 동안 선형으로 넣고 뺀다.
        prepare/reset 뒤 첫 호출은 페이드 없이 그 상태에서 시작한다. 전환이 끝난 bypass는
        지연 드라이와 샘플 단위로 같고, 내부 상태는 bypass 중에도 계속 진행한다. */
    void process (juce::AudioBuffer<float>& buffer, bool bypassed = false) noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using ArrayCoefficients = juce::dsp::IIR::ArrayCoefficients<float>;

    /*  2차 올패스 한 단.  H(z) = (c - z^-2) / (1 - c·z^-2)
        위상만 바꾸고 진폭은 그대로 통과시킨다. */
    struct AllpassSection
    {
        double coefficient = 0.0;
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        inline float process (float input) noexcept
        {
            const auto x = (double) input;
            const auto y = coefficient * (x + y2) - x2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return (float) y;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
    };

    /*  힐베르트 변환기: 두 올패스 체인의 출력이 넓은 대역에서 서로 90° 차이를 갖는다.
        결과 (real, imaginary)는 해석신호 z = real + j·imaginary 이며, 여기서
        Re(z²), Re(z³)를 취하면 차음과 DC가 원리적으로 존재하지 않는 순수 배가 신호가 된다. */
    struct AnalyticPair
    {
        AllpassSection branchA[4];
        AllpassSection branchB[4];
        float previousInput = 0.0f;

        void prepare() noexcept;
        void reset() noexcept;

        inline void process (float input, float& real, float& imaginary) noexcept
        {
            // 지연은 branchA 입력에 건다. 반대로 걸면 위상차가 90°에서 크게 벗어난다.
            auto a = previousInput;

            for (auto& section : branchA)
                a = section.process (a);

            auto b = input;

            for (auto& section : branchB)
                b = section.process (b);

            previousInput = input;
            real = a;
            imaginary = b;
        }
    };

    static constexpr int resonanceBands = 8;

    struct ChannelState
    {
        AnalyticPair analytic;
        Filter bandHighPass[2];      // LR4 = Butterworth 2차 × 2
        Filter bandLowPass[2];       // LR4
        Filter residualHighPass[2];  // LR4
        Filter postLowPass;
        Filter brightShelf;          // BRIGHT의 선형 셸프. 지연 정렬된 드라이에 적용한다.
        Filter resonanceDetector[resonanceBands];
        Filter resonanceProtector[resonanceBands];
        std::vector<float> dryDelay;
        int dryWriteIndex = 0;
    };

    void processChunk (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples) noexcept;
    void updateFilters (float tuneHz);

    /*  --- 배음 생성기: 해석신호 주파수 배가 -----------------------------------

        메모리리스 비선형(포화 함수든 다항식이든)에 넓은 대역을 통과시키면, 단일 사인에서만
        깨끗한 배음이 나오고 실제 음악처럼 성분이 많으면 모든 쌍이 합·차음을 만들어
        **이산 배음이 아니라 잡음 구름**이 된다. 2차 청취에서 지적된 "초고역 지글거림"의
        정체가 이것이다. 특히 실수 세제곱은

            x^3 = Re(z^3)/4 + 3|z|^2·x/4

        의 뒷항, 즉 "대역 신호가 자기 엔벌로프로 진폭변조된 성분"을 만든다. 거칠고 조밀하다.

        해석신호 z = a + j·b (b = 힐베르트 변환) 를 쓰면 이 문제가 원리적으로 사라진다.

            Re(z^2) = a^2 - b^2          → 정확히 2배 주파수
            Re(z^3) = a^3 - 3·a·b^2      → 정확히 3배 주파수

        두 톤 z = e^{jw1t} + e^{jw2t} 에 대해 z^2 = e^{2jw1t} + 2e^{j(w1+w2)t} + e^{2jw2t}
        이므로 **합 성분만 생기고 차음과 DC는 존재하지 않는다.** 또 단일 성분에 대해
        출력 진폭이 |z|^2 로 일정해서 피크가 뾰족해지지 않는다. 이것이 "매끄러움"의 정체다.

        CLARITY는 두 배가 차수의 배합만 바꾼다. 별도의 포화·레벨 기울기·트랜지언트
        드라이브는 더하지 않는다.
          WARM  : 2배 우세(6–12 kHz)
          CLEAR : 3배 우세(9–18 kHz) */
    static constexpr float vintageEvenWeight = 1.00f;
    static constexpr float vintageOddWeight  = 0.20f;
    static constexpr float modernEvenWeight  = 0.30f;
    static constexpr float modernOddWeight   = 1.00f;

    /*  Re(z^2)와 Re(z^3)은 단위 입력에 대해 둘 다 진폭 1이므로 같은 makeup을 쓴다. */
    static constexpr float vintageMakeup = 0.55f;
    static constexpr float modernMakeup  = 0.55f;

    /*  --- BRIGHT: 선형 셸프 위주 + 배음 소량 -----------------------------------

        밝기를 전부 배음으로 만들면, 조밀한 음악 신호에서 3차 항이 N³개의 합 성분을
        만들어 결국 잡음 구름이 된다. 해석신호로도 이건 못 없앤다. 3차 청취까지
        "지글거림"이 남은 근본 이유다.

        선형 셸프는 배음을 **0개** 만든다. 그래서 밝기의 대부분을 셸프로 만들고,
        배음은 "정보가 더해진 느낌"을 주는 보조로만 쓴다. 대신 셸프는 드라이가 아니라
        **더해지는 성분(delta)** 으로 계산해 WIDTH와 함께 넓힌다. 원음 정위는 보존된다.

        2026-11-11 청취 선택(4_air_sparkle): 코너를 여기 대역 상단(2×FOCUS)에서 3×FOCUS로
        올리고 최대 게인을 +5 → +6.5 dB로 키웠다. 12 kHz 위 상승량은 그대로이면서
        치찰·거친 느낌이 모이는 4–12 kHz 상승이 절반 이하로 줄어든다. 코너는 여기 대역
        폭(bandWidthRatio)과 별개의 값이므로 상수를 분리한다.

        같은 날 두 번째 청취 선택(3_cap9k): 코너는 9 kHz에서 멈춘다. 상한이 없으면
        FOCUS 4.8 kHz에서 코너가 14.4 kHz, 6 kHz에서 18 kHz가 되어 셸프가 가청 대역
        밖으로 올라가고, FOCUS가 높은 프리셋 10개에서 6–16 kHz가 오히려 내려갔다.
        9 kHz는 FOCUS 3 kHz(기본값)에서 청취로 고른 바로 그 코너다. */
    static constexpr float brightShelfMaxDb          = 6.5f;
    static constexpr float brightShelfCornerRatio    = 3.0f;      // 셸프 코너 = 3×FOCUS,
    static constexpr float brightShelfCornerMaxHz    = 9000.0f;   // 단 9 kHz를 넘지 않는다

    static float brightShelfCornerFor (float focusHz) noexcept
    {
        return juce::jmin (brightShelfCornerMaxHz, focusHz * brightShelfCornerRatio);
    }

    /*  BRIGHT의 상승 방향은 유지하되, 주변보다 좁게 솟은 성분은 셸프 delta를
        최대 25 % 덜 받게 한다. 검출기는 고정 주파수라 FOCUS 자동화 중에도
        계수와 상태가 움직이지 않고, FOCUS는 보호 밴드의 가중치만 정한다. */
    static constexpr std::array<float, resonanceBands> resonanceCentreHz
        { 1500.0f, 2121.32f, 3000.0f, 4242.64f,
          6000.0f, 8485.28f, 12000.0f, 16970.56f };
    static constexpr float resonanceQ = 3.0f;
    static constexpr float resonanceProminenceStartDb = 3.0f;
    static constexpr float resonanceProminenceFullDb = 8.0f;
    static constexpr float resonanceLevelGateStartDb = -90.0f;
    static constexpr float resonanceLevelGateFullDb = -70.0f;
    static constexpr float resonanceProtectionBudget = 0.25f;

    // adaptive 정규화 기준.
    static constexpr float envelopeFloor     = 1.0e-4f;
    /*  이전 설계의 정상상태 생성기 동작점(0.8425)을 고정값으로 유지한다.
        1.0으로 올리면 2·3차가 각각 약 3/4.5 dB 증가해 wetScale -20 dB 보정이 무너진다. */
    static constexpr float generatorDrive    = 0.84f;
    /*  adaptive 정규화 후 다항식 생성기에 들어가는 신호의 상한. u^3의 발산을 막는다. */
    static constexpr float generatorClamp    = 1.3f;

    /*  배음 경로의 안전 상한(정규화 단위).

        정상 상태의 잔차는 0.3~0.5 부근이라 이 값에 닿지 않는다. 어택 직후에만
        정규화 엔벌로프가 1 ms 뒤처지며 x/env가 튀어 여기에 걸린다. 즉 이 상수는
        **정상 상태를 건드리지 않고 onset 배음만 묶는** 지점이다.
        0.9로 낮춰봤더니 정상 상태까지 물려 IMD가 -58에서 -44 dB로 악화됐다. 1.5가 맞다. */
    static constexpr float residualCeiling   = 1.5f;
    /*  배음은 셸프보다 훨씬 낮은 보조 레이어다. 이전 3.6은 기본값에서도 배음이 원 여기
        신호보다 몇 dB만 낮아 디스토션으로 들렸으므로 20 dB 낮춘다. */
    static constexpr float wetScale = 0.36f;

    /*  --- 배음이 떨어지는 위치에 따른 양 조절 -------------------------------

        배음량을 주파수와 무관한 하나의 상수로 두면, 낮은 FOCUS에서 디스토션으로 들리지
        않을 만큼 낮춰야 하고 그러면 높은 FOCUS에서 대역 확장이 불가능해진다.
        실제로 그 상태였다: 대역제한 소재의 컷오프 위 생성량이 어깨 대비 -20 dB로
        들리지 않는 수준이었고, 동시에 added 성분과 하이셸프의 상관이 0.986이었다.

        해법은 양이 아니라 **위치에 따른 양**이다.
          FOCUS 500 Hz  → 배음이 1~3 kHz. 귀가 가장 예민한 대역이라 조금만 넘쳐도
                          디스토션으로 인식된다. 지금 수준을 유지한다.
          FOCUS 3 kHz+  → 배음이 6~18 kHz. 같은 양이라도 공기감으로 인식되고,
                          여기서만 EQ가 못 하는 대역 확장이 가능하다.

        등청감 곡선과 마스킹이 뒷받침하는 구분이며, 검사 신호를 피하려는 보정이 아니다.

        2026-11-11 청취 선택(4_air_sparkle): 높은 FOCUS의 배음량을 2.0 → 2.8배(+2.9 dB)로
        올렸다. 낮은 FOCUS(800 Hz 이하)의 배음량은 그대로다. */
    static constexpr float harmonicTiltLowHz   = 800.0f;
    static constexpr float harmonicTiltHighHz  = 3000.0f;
    static constexpr float harmonicTiltMaxGain = 2.8f;

    static constexpr float bandWidthRatio        = 2.0f;   // 여기 대역 = TUNE ~ 2×TUNE
    static constexpr float residualHighPassRatio = 2.0f;
    static constexpr float postLowPassHz         = 18000.0f;
    static constexpr int filterUpdateInterval    = 32;

    double sampleRate       = 44100.0;
    int    maxBlockSize     = 512;
    int    preparedChannels = 0;
    int    latencySamples   = 0;
    bool   prepared         = false;

    // 입력 레벨과 무관한 낮은 배음량을 유지하는 adaptive 정규화 엔벌로프.
    float normAttackCoeff = 0.0f, normReleaseCoeff = 0.0f;
    float normEnvelope = 0.0f;

    float resonanceEnvelopeAttackCoeff = 0.0f, resonanceEnvelopeReleaseCoeff = 0.0f;
    float resonanceScoreAttackCoeff = 0.0f, resonanceScoreReleaseCoeff = 0.0f;
    std::array<float, resonanceBands> resonanceEnvelope {};
    std::array<float, resonanceBands> resonanceScore {};

    float currentTuneHz = 0.0f;
    int filterUpdateSamplesRemaining = 0;

    juce::SmoothedValue<float> tuneSmoothed;
    juce::SmoothedValue<float> amountSmoothed;
    juce::SmoothedValue<float> characterSmoothed;
    juce::SmoothedValue<float> widthSmoothed;

    // added 경로의 bypass 크로스페이드. 1 = 처리, 0 = bypass.
    juce::SmoothedValue<float> addedMixSmoothed;
    bool addedMixPrimed = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> bandBuffer;
    juce::AudioBuffer<float> bandCopy;   // 드라이 셸프 delta와 배음을 합치는 작업 버퍼.
    juce::AudioBuffer<float> resonanceControl;

    std::vector<float> envelopeControl;
    std::vector<float> characterControl;
    std::vector<float> amountControl;
    std::vector<float> brightControl;
    std::vector<float> addedMixControl;

    std::vector<ChannelState> channels;

    JUCE_LEAK_DETECTOR (ExciterEngine)
};
