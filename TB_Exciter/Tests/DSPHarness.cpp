/*  TB_Exciter DSP Harness

    실제 Processor 신호 경로를 통해 검증한다 (TB_PLUGIN_DEVELOPMENT_GUIDE.md 10장).

    사용법:
      TB_Exciter_DSPHarness              모든 테스트 실행
      TB_Exciter_DSPHarness --render     Guide MD/Assets로 레벨 매칭 A/B 파일 생성
      TB_Exciter_DSPHarness --render-ui  HTML 목업 기반 JUCE 에디터 PNG 생성
      TB_Exciter_DSPHarness --calibrate  실제 소재의 BRIGHT 캘리브레이션 실측
      TB_Exciter_DSPHarness --diagnose   힐베르트·스펙트럼·제어 변조 진단
      TB_Exciter_DSPHarness --bench      실제 Processor 경로의 CPU 비용 측정
      TB_Exciter_DSPHarness --preset-levels  Factory 프리셋별 실소재 변화 측정
      TB_Exciter_DSPHarness --render-voicing <label> [focusHz]
                                         현재 빌드의 기본 설정을 output/ab_voicing 에 청취용으로 렌더
*/

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
//==============================================================================
int failureCount = 0;
int checkCount   = 0;

void report (bool passed, const juce::String& name, const juce::String& detail = {})
{
    ++checkCount;

    if (! passed)
        ++failureCount;

    juce::String line;
    line << (passed ? "  [PASS] " : "  [FAIL] ") << name;

    if (detail.isNotEmpty())
        line << "   " << detail;

    std::cout << line << std::endl;
}

void section (const juce::String& title)
{
    std::cout << std::endl << "== " << title << " ==" << std::endl;
}

juce::String db (double linear)
{
    if (linear <= 0.0)
        return "-inf dB";

    return juce::String (20.0 * std::log10 (linear), 2) + " dB";
}

//==============================================================================
struct Rig
{
    TB_ExciterAudioProcessor processor;
    double sampleRate;
    int    blockSize;
    int    numChannels;

    Rig (double sr, int bs, int channels)
        : sampleRate (sr), blockSize (bs), numChannels (channels)
    {
        const auto set = channels == 1 ? juce::AudioChannelSet::mono()
                                       : juce::AudioChannelSet::stereo();

        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);

        layoutAccepted = processor.setBusesLayout (layout);

        processor.setRateAndBufferSizeDetails (sr, bs);
        processor.prepareToPlay (sr, bs);
    }

    bool layoutAccepted = false;

    void set (const char* parameterId, float actualValue)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (parameterId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (actualValue));
        else
            jassertfalse;
    }

    void setAll (float tune, float amount, float character, float width, float output)
    {
        set (tbx::params::tuneId, tune);
        set (tbx::params::amountId, amount);
        set (tbx::params::characterId, character);
        set (tbx::params::widthId, width);
        set (tbx::params::outputId, output);
    }

    /** 파라미터 스무딩이 목표값에 도달하도록 무음을 미리 흘린다. */
    void settle (double seconds = 0.3)
    {
        juce::AudioBuffer<float> warmUp (numChannels, (int) (sampleRate * seconds));
        warmUp.clear();
        render (warmUp);
    }

    void render (juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        const auto total = buffer.getNumSamples();

        for (int position = 0; position < total; position += blockSize)
        {
            const auto count = juce::jmin (blockSize, total - position);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), position, count);
            processor.processBlock (view, midi);
        }
    }

    void renderBypassed (juce::AudioBuffer<float>& buffer)
    {
        juce::MidiBuffer midi;
        const auto total = buffer.getNumSamples();

        for (int position = 0; position < total; position += blockSize)
        {
            const auto count = juce::jmin (blockSize, total - position);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(),
                                           buffer.getNumChannels(), position, count);
            processor.processBlockBypassed (view, midi);
        }
    }

    int latency() const { return processor.getLatencySamples(); }
};

juce::AudioBuffer<float> renderLatencyAligned (Rig& rig,
                                                const juce::AudioBuffer<float>& input)
{
    juce::AudioBuffer<float> padded (input.getNumChannels(),
                                     input.getNumSamples() + rig.latency());
    padded.clear();

    for (int channel = 0; channel < input.getNumChannels(); ++channel)
        padded.copyFrom (channel, 0, input, channel, 0, input.getNumSamples());

    rig.render (padded);

    juce::AudioBuffer<float> aligned (input.getNumChannels(), input.getNumSamples());
    for (int channel = 0; channel < input.getNumChannels(); ++channel)
        aligned.copyFrom (channel, 0, padded, channel, rig.latency(), input.getNumSamples());

    return aligned;
}

//==============================================================================
void fillSine (juce::AudioBuffer<float>& buffer, double frequency, double amplitude,
               double sampleRate, int startSample = 0)
{
    const auto increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = startSample; i < buffer.getNumSamples(); ++i)
            data[i] = (float) (amplitude * std::sin (increment * (i - startSample)));
    }
}

void fillNoise (juce::AudioBuffer<float>& buffer, double amplitude, juce::int64 seed)
{
    juce::Random random (seed);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = (float) (amplitude * (random.nextDouble() * 2.0 - 1.0));
    }
}

bool isFiniteAndBounded (const juce::AudioBuffer<float>& buffer, float limit)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (data[i]) || std::abs (data[i]) > limit)
                return false;
    }

    return true;
}

double rms (const juce::AudioBuffer<float>& buffer, int start, int count, int channel = 0)
{
    const auto* data = buffer.getReadPointer (channel);
    double sum = 0.0;

    for (int i = start; i < start + count; ++i)
        sum += (double) data[i] * data[i];

    return std::sqrt (sum / juce::jmax (1, count));
}

double toneAmplitude (const juce::AudioBuffer<float>& buffer, int start, int count,
                      double frequency, double sampleRate, int channel = 0)
{
    const auto* data = buffer.getReadPointer (channel);
    const auto increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;
    double real = 0.0, imaginary = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const auto phase = increment * (double) (start + i);
        real      += (double) data[start + i] * std::cos (phase);
        imaginary -= (double) data[start + i] * std::sin (phase);
    }

    return 2.0 * std::sqrt (real * real + imaginary * imaginary) / juce::jmax (1, count);
}

/** BRIGHT 0 출력과 지정 BRIGHT 출력의 차. 셸프 delta와 저차 배음을 모두 포함한다. */
juce::AudioBuffer<float> renderEffectDelta (double sampleRate, int blockSize, int channels,
                                            const juce::AudioBuffer<float>& input,
                                            float tune, float character, float width = 100.0f,
                                            float amount = 100.0f)
{
    juce::AudioBuffer<float> dryRun (input);
    {
        Rig rig (sampleRate, blockSize, channels);
        rig.setAll (tune, 0.0f, character, width, 0.0f);
        rig.settle();
        rig.render (dryRun);
    }

    juce::AudioBuffer<float> wetRun (input);
    {
        Rig rig (sampleRate, blockSize, channels);
        rig.setAll (tune, amount, character, width, 0.0f);
        rig.settle();
        rig.render (wetRun);
    }

    for (int channel = 0; channel < wetRun.getNumChannels(); ++channel)
        juce::FloatVectorOperations::subtract (wetRun.getWritePointer (channel),
                                               dryRun.getReadPointer (channel),
                                               wetRun.getNumSamples());

    return wetRun;
}

//==============================================================================
constexpr int fftOrder = 15;                 // 32768
constexpr int fftSize  = 1 << fftOrder;

/*  window = true 이면 Hann 창을 쓴다.

    빈 정렬된 톤만 있을 때는 직사각 창이 정확하지만, 빈에 맞지 않는 강한 성분이 있으면
    직사각 창의 누설(1/Δf로만 감쇠)이 다른 성분 주변에 가짜 치맛자락을 만든다.
    변조 측정처럼 "치맛자락의 크기" 자체가 판정 대상일 때는 반드시 창을 씌워야 한다. */
std::vector<float> magnitudeSpectrum (const float* data, int numSamples, bool window = false)
{
    jassert (numSamples >= fftSize);
    juce::ignoreUnused (numSamples);

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> scratch ((size_t) fftSize * 2, 0.0f);
    std::copy (data, data + fftSize, scratch.begin());

    if (window)
        for (int i = 0; i < fftSize; ++i)
            scratch[(size_t) i] *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                           * (float) i / (float) (fftSize - 1));

    fft.performFrequencyOnlyForwardTransform (scratch.data());

    std::vector<float> magnitudes ((size_t) fftSize / 2);

    for (size_t i = 0; i < magnitudes.size(); ++i)
        magnitudes[i] = scratch[i] * 2.0f / (float) fftSize;

    return magnitudes;
}

/** FFT 빈에 정확히 맞는 주파수를 고른다. 누설 없이 배음 크기를 읽기 위한 것이다. */
double binAlignedFrequency (double desiredHz, double sampleRate, int& binOut)
{
    binOut = juce::jmax (1, juce::roundToInt (desiredHz * fftSize / sampleRate));
    return binOut * sampleRate / fftSize;
}

double peakAroundBin (const std::vector<float>& magnitudes, int bin, int radius = 2)
{
    double peak = 0.0;

    for (int i = juce::jmax (0, bin - radius);
         i <= juce::jmin ((int) magnitudes.size() - 1, bin + radius); ++i)
        peak = juce::jmax (peak, (double) magnitudes[(size_t) i]);

    return peak;
}

//==============================================================================
// ITU-R BS.1770-4 게이팅 라우드니스. K-weighting 계수는 샘플레이트별로 계산한다.
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    double process (double x)
    {
        const auto y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

double integratedLoudness (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples  = buffer.getNumSamples();

    juce::AudioBuffer<double> weighted (numChannels, numSamples);

    constexpr auto shelfFrequency = 1681.974450955533;
    constexpr auto shelfGainDb    = 3.999843853973347;
    constexpr auto shelfQ         = 0.7071752369554196;
    const auto shelfK  = std::tan (juce::MathConstants<double>::pi * shelfFrequency / sampleRate);
    const auto shelfVh = std::pow (10.0, shelfGainDb / 20.0);
    const auto shelfVb = std::pow (shelfVh, 0.4996667741545416);
    const auto shelfA0 = 1.0 + shelfK / shelfQ + shelfK * shelfK;

    Biquad shelfTemplate;
    shelfTemplate.b0 = (shelfVh + shelfVb * shelfK / shelfQ + shelfK * shelfK) / shelfA0;
    shelfTemplate.b1 = 2.0 * (shelfK * shelfK - shelfVh) / shelfA0;
    shelfTemplate.b2 = (shelfVh - shelfVb * shelfK / shelfQ + shelfK * shelfK) / shelfA0;
    shelfTemplate.a1 = 2.0 * (shelfK * shelfK - 1.0) / shelfA0;
    shelfTemplate.a2 = (1.0 - shelfK / shelfQ + shelfK * shelfK) / shelfA0;

    constexpr auto highPassFrequency = 38.13547087602444;
    constexpr auto highPassQ         = 0.5003270373238773;
    const auto highPassK  = std::tan (juce::MathConstants<double>::pi * highPassFrequency / sampleRate);
    const auto highPassA0 = 1.0 + highPassK / highPassQ + highPassK * highPassK;

    Biquad highPassTemplate;
    highPassTemplate.b0 = 1.0;
    highPassTemplate.b1 = -2.0;
    highPassTemplate.b2 = 1.0;
    highPassTemplate.a1 = 2.0 * (highPassK * highPassK - 1.0) / highPassA0;
    highPassTemplate.a2 = (1.0 - highPassK / highPassQ + highPassK * highPassK) / highPassA0;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto shelf = shelfTemplate;
        auto highPass = highPassTemplate;

        const auto* source = buffer.getReadPointer (channel);
        auto* destination  = weighted.getWritePointer (channel);

        for (int i = 0; i < numSamples; ++i)
            destination[i] = highPass.process (shelf.process ((double) source[i]));
    }

    const auto blockSamples = (int) (0.4 * sampleRate);
    const auto stepSamples  = (int) (0.1 * sampleRate);

    if (numSamples < blockSamples)
        return -70.0;

    std::vector<std::vector<double>> blockPower;   // [block][channel]

    for (int start = 0; start + blockSamples <= numSamples; start += stepSamples)
    {
        std::vector<double> perChannel ((size_t) numChannels, 0.0);

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* data = weighted.getReadPointer (channel);
            double sum = 0.0;

            for (int i = start; i < start + blockSamples; ++i)
                sum += data[i] * data[i];

            perChannel[(size_t) channel] = sum / blockSamples;
        }

        blockPower.push_back (std::move (perChannel));
    }

    auto loudnessOf = [numChannels] (const std::vector<double>& power)
    {
        double sum = 0.0;

        for (int channel = 0; channel < numChannels; ++channel)
            sum += power[(size_t) channel];      // L/R 가중치 G = 1.0

        return sum > 0.0 ? -0.691 + 10.0 * std::log10 (sum) : -200.0;
    };

    // 절대 게이트 -70 LUFS
    std::vector<const std::vector<double>*> aboveAbsolute;

    for (const auto& power : blockPower)
        if (loudnessOf (power) > -70.0)
            aboveAbsolute.push_back (&power);

    if (aboveAbsolute.empty())
        return -70.0;

    auto meanPower = [numChannels] (const std::vector<const std::vector<double>*>& blocks)
    {
        std::vector<double> mean ((size_t) numChannels, 0.0);

        for (const auto* block : blocks)
            for (int channel = 0; channel < numChannels; ++channel)
                mean[(size_t) channel] += (*block)[(size_t) channel];

        for (auto& value : mean)
            value /= (double) blocks.size();

        return mean;
    };

    // 상대 게이트 -10 LU
    const auto relativeThreshold = loudnessOf (meanPower (aboveAbsolute)) - 10.0;

    std::vector<const std::vector<double>*> gated;

    for (const auto* block : aboveAbsolute)
        if (loudnessOf (*block) > relativeThreshold)
            gated.push_back (block);

    if (gated.empty())
        return -70.0;

    return loudnessOf (meanPower (gated));
}

//==============================================================================
void testStateAndSafety()
{
    section ("1. 안전성과 상태");

    {
        Rig rig (48000.0, 512, 2);
        report (rig.layoutAccepted, "스테레오 버스 구성 지원");
        report (rig.latency() > 0, "레이턴시 보고", "latency = " + juce::String (rig.latency()) + " samples");
    }

    {
        Rig rig (48000.0, 480, 1);
        report (rig.layoutAccepted, "모노 버스 구성 지원");

        juce::AudioBuffer<float> buffer (1, 48000);
        buffer.clear();
        rig.settle();
        rig.render (buffer);

        report (isFiniteAndBounded (buffer, 1.0e-6f), "무입력 → 무출력 (모노)",
                "peak = " + db (buffer.getMagnitude (0, buffer.getNumSamples())));
    }

    {
        // 큰 버스트 후 무음 구간에서 DC 폭주나 자발 발진이 없어야 한다.
        Rig rig (48000.0, 512, 2);
        rig.setAll (1000.0f, 100.0f, 50.0f, 100.0f, 0.0f);
        rig.settle();

        juce::AudioBuffer<float> buffer (2, 48000 * 3);
        buffer.clear();
        juce::AudioBuffer<float> burst (buffer.getArrayOfWritePointers(), 2, 0, 24000);
        fillNoise (burst, 0.9, 1234);
        rig.render (buffer);

        const auto tail = rms (buffer, 48000 * 2, 48000);
        report (tail < 1.0e-5, "버스트 후 3초 구간에서 잔류 신호 없음", "tail rms = " + db (tail));
    }

    {
        // 전 파라미터 극단 조합.
        const float tunes[]      { 500.0f, 3000.0f, 6000.0f };
        const float amounts[]    { 0.0f, 50.0f, 100.0f };
        const float characters[] { 0.0f, 50.0f, 100.0f };
        const float widths[]     { 0.0f, 100.0f, 200.0f };
        const float outputs[]    { -12.0f, 0.0f, 12.0f };

        bool allSafe = true;
        double worstPeak = 0.0;

        for (auto tune : tunes)
            for (auto amount : amounts)
                for (auto character : characters)
                    for (auto width : widths)
                        for (auto output : outputs)
                        {
                            Rig rig (48000.0, 256, 2);
                            rig.setAll (tune, amount, character, width, output);
                            rig.settle (0.1);

                            juce::AudioBuffer<float> buffer (2, 12000);
                            fillNoise (buffer, 0.95, 77);
                            rig.render (buffer);

                            worstPeak = juce::jmax (worstPeak,
                                                    (double) buffer.getMagnitude (0, buffer.getNumSamples()));

                            // BRIGHT 100 % + LEVEL +12 dB + 풀스케일 노이즈는 사용자가 고른
                            // 극단 조합이다. 여기서 요구하는 것은 "조용함"이 아니라
                            // 유한하고 유계이며 발산하지 않는 것이다 (개발 가이드 8장).
                            if (! isFiniteAndBounded (buffer, 32.0f))
                                allSafe = false;
                        }

        report (allSafe, "극단값 243조합에서 NaN/Inf 없음, 유계",
                "worst peak = " + db (worstPeak));
    }

    {
        /*  인서트 기본값의 안전성 (개발 가이드 6.3).

            풀스케일 화이트 노이즈는 익사이터에 가장 가혹한 입력이다. 스펙트럼이 평탄해
            여기 대역 에너지가 최대이고 크레스트도 높다. 실제 음악 소재의 기본값 피크 상승은
            --calibrate 실측 기준 BUS_01 +1.9 dB, DRM_01 +2.8 dB, VOX_01 -0.0 dB이며,
            아래 한계는 그 실측 대비 여유를 둔 최악 입력 기준이다. */
        double worstGain = 0.0;

        for (double amplitude : { 0.1, 0.5, 0.95 })
        {
            Rig rig (48000.0, 512, 2);
            rig.setAll (tbx::params::tuneDefaultHz, tbx::params::amountDefault,
                        tbx::params::characterDefault, tbx::params::widthDefault,
                        tbx::params::outputDefaultDb);
            rig.settle();

            juce::AudioBuffer<float> buffer (2, 48000);
            fillNoise (buffer, amplitude, 555);
            const auto inputPeak = buffer.getMagnitude (0, buffer.getNumSamples());
            rig.render (buffer);

            worstGain = juce::jmax (worstGain,
                                    (double) buffer.getMagnitude (0, buffer.getNumSamples()) / inputPeak);
        }

        report (worstGain < 2.0, "기본값 + 풀스케일 화이트 노이즈에서 피크 상승 ≤ +6 dB",
                "worst peak gain = " + db (worstGain));
    }

    {
        // DAW 상태 저장/복원.
        Rig rig (48000.0, 512, 2);
        rig.setAll (1234.0f, 71.5f, 88.0f, 125.0f, -4.5f);

        juce::MemoryBlock saved;
        rig.processor.getStateInformation (saved);

        rig.setAll (5000.0f, 10.0f, 5.0f, 40.0f, 9.0f);
        rig.processor.setStateInformation (saved.getData(), (int) saved.getSize());

        auto valueOf = [&rig] (const char* id)
        {
            return rig.processor.getValueTreeState().getRawParameterValue (id)->load();
        };

        const auto restored = std::abs (valueOf (tbx::params::tuneId)      - 1234.0f) < 0.5f
                           && std::abs (valueOf (tbx::params::amountId)    - 71.5f)   < 0.05f
                           && std::abs (valueOf (tbx::params::characterId) - 88.0f)   < 0.05f
                           && std::abs (valueOf (tbx::params::widthId)     - 125.0f)  < 0.6f
                           && std::abs (valueOf (tbx::params::outputId)    - (-4.5f)) < 0.05f;

        report (restored, "DAW 상태 저장 → 변경 → 복원이 전체 파라미터를 되돌림");

        // 알 수 없는 미래 스키마는 현재 상태를 바꾸지 않아야 한다.
        juce::XmlElement bogus ("TB_ExciterState");
        bogus.setAttribute ("stateSchemaVersion", 99);
        juce::MemoryBlock bogusBlock;
        rig.processor.copyXmlToBinary (bogus, bogusBlock);

        rig.processor.setStateInformation (bogusBlock.getData(), (int) bogusBlock.getSize());
        report (std::abs (valueOf (tbx::params::tuneId) - 1234.0f) < 0.5f,
                "알 수 없는 스키마 버전을 거부하고 현재 상태를 보존");
    }

    {
        bool catalogValid = tbx::presets::factory.size() == 30;

        for (size_t i = 0; i < tbx::presets::factory.size(); ++i)
        {
            const auto& preset = tbx::presets::factory[i];
            catalogValid = catalogValid
                           && preset.id == static_cast<int> (i) + 1
                           && juce::String (preset.name).isNotEmpty()
                           && preset.bright >= 0.0f && preset.bright <= 78.0f
                           && preset.clarity >= 0.0f && preset.clarity <= 100.0f
                           && preset.focusHz >= tbx::params::tuneMinHz
                           && preset.focusHz <= tbx::params::tuneMaxHz
                           && preset.width >= 0.0f && preset.width <= 200.0f
                           && preset.levelDb >= -1.8f && preset.levelDb <= 0.0f;

            for (size_t j = i + 1; j < tbx::presets::factory.size(); ++j)
                catalogValid = catalogValid
                               && preset.id != tbx::presets::factory[j].id
                               && juce::String (preset.name) != tbx::presets::factory[j].name;
        }

        report (catalogValid, "30 factory presets have stable unique IDs and safe ranges");

        TB_ExciterAudioProcessor processor;
        bool allPresetsApply = processor.getNumPrograms() == 1;

        auto valueOf = [&processor] (const char* id)
        {
            return processor.getValueTreeState().getRawParameterValue (id)->load();
        };

        for (const auto& preset : tbx::presets::factory)
        {
            allPresetsApply = allPresetsApply
                              && processor.applyFactoryPreset (preset.id)
                              && processor.getActiveFactoryPresetId() == preset.id
                              && processor.matchesFactoryPreset (preset.id)
                              && std::abs (valueOf (tbx::params::amountId) - preset.bright) < 0.051f
                              && std::abs (valueOf (tbx::params::characterId) - preset.clarity) < 0.051f
                              && std::abs (valueOf (tbx::params::tuneId) - preset.focusHz) < 0.51f
                              && std::abs (valueOf (tbx::params::widthId) - preset.width) < 0.51f
                              && std::abs (valueOf (tbx::params::outputId) - preset.levelDb) < 0.051f;
        }

        report (allPresetsApply,
                "all factory presets reach exact APVTS values without changing host program count");

        processor.applyFactoryPreset (13);
        auto* bright = processor.getValueTreeState().getParameter (tbx::params::amountId);
        bright->setValueNotifyingHost (bright->convertTo0to1 (55.0f));
        juce::MemoryBlock dirtyState;
        processor.getStateInformation (dirtyState);

        TB_ExciterAudioProcessor restored;
        restored.setStateInformation (dirtyState.getData(), static_cast<int> (dirtyState.getSize()));
        const auto dirtyRestored = restored.getActiveFactoryPresetId() == 13
                                   && ! restored.matchesFactoryPreset (13)
                                   && std::abs (restored.getValueTreeState()
                                                   .getRawParameterValue (tbx::params::amountId)->load()
                                               - 55.0f) < 0.051f;
        report (dirtyRestored, "preset identity and edited values survive state round-trip");
    }

    {
        Rig rig (48000.0, 512, 2);
        auto* focus = rig.processor.getValueTreeState().getParameter (tbx::params::tuneId);
        auto* width = rig.processor.getValueTreeState().getParameter (tbx::params::widthId);

        auto roundTrips = [] (juce::RangedAudioParameter& parameter, float actual, float tolerance)
        {
            const auto text = parameter.getText (parameter.convertTo0to1 (actual), 64);
            const auto parsed = parameter.convertFrom0to1 (parameter.getValueForText (text));
            return std::abs (parsed - actual) <= tolerance;
        };

        bool focusRoundTrip = focus != nullptr;
        for (auto value : { 500.0f, 999.0f, 1000.0f, 1800.0f, 3000.0f, 6000.0f })
            focusRoundTrip = focusRoundTrip && roundTrips (*focus, value, 0.51f);

        focusRoundTrip = focusRoundTrip
                      && std::abs (focus->convertFrom0to1 (focus->getValueForText ("3 kHz")) - 3000.0f) < 0.51f
                      && std::abs (focus->convertFrom0to1 (focus->getValueForText ("3000 Hz")) - 3000.0f) < 0.51f;

        bool widthRoundTrip = width != nullptr;
        for (auto value : { 0.0f, 1.0f, 100.0f, 199.0f, 200.0f })
            widthRoundTrip = widthRoundTrip && roundTrips (*width, value, 0.51f);

        report (focusRoundTrip, "FOCUS 표시 문자열을 값으로 다시 입력해도 같은 주파수");
        report (widthRoundTrip, "WIDTH의 Mono/Wide 표시 문자열이 0/200 %로 왕복");

        std::unique_ptr<juce::AudioProcessorEditor> editor (rig.processor.createEditor());
        int namedSliders = 0;
        int slidersWithoutTextBoxes = 0;
        juce::Slider* brightSlider = nullptr;
        for (int i = 0; i < editor->getNumChildComponents(); ++i)
            if (auto* slider = dynamic_cast<juce::Slider*> (editor->getChildComponent (i));
                slider != nullptr)
            {
                if (slider->getTitle().isNotEmpty())
                    ++namedSliders;

                if (slider->getTextBoxPosition() == juce::Slider::NoTextBox)
                    ++slidersWithoutTextBoxes;

                if (slider->getTitle() == "BRIGHT")
                    brightSlider = slider;
            }

        report (namedSliders == tbx::params::parameterCount,
                "공개 슬라이더 5개가 모두 스크린리더용 접근성 이름을 가짐",
                "named sliders = " + juce::String (namedSliders));

        const juce::String controlNames[] { "BRIGHT", "CLARITY", "FOCUS", "WIDTH", "LEVEL" };
        const juce::Rectangle<int> fieldBounds (160, 100, 270, 270);
        int labelsBelowKnobs = 0;
        int labelsOutsideField = 0;

        for (const auto& controlName : controlNames)
        {
            juce::Slider* matchingSlider = nullptr;
            juce::Label* matchingLabel = nullptr;

            for (int i = 0; i < editor->getNumChildComponents(); ++i)
            {
                auto* child = editor->getChildComponent (i);

                if (auto* slider = dynamic_cast<juce::Slider*> (child);
                    slider != nullptr && slider->getTitle() == controlName)
                    matchingSlider = slider;

                if (auto* label = dynamic_cast<juce::Label*> (child);
                    label != nullptr && label->getText() == controlName)
                    matchingLabel = label;
            }

            if (matchingSlider != nullptr && matchingLabel != nullptr)
            {
                if (matchingLabel->getY() >= matchingSlider->getBottom())
                    ++labelsBelowKnobs;

                if (! matchingLabel->getBounds().intersects (fieldBounds))
                    ++labelsOutsideField;
            }
        }

        report (slidersWithoutTextBoxes == tbx::params::parameterCount
                    && labelsBelowKnobs == tbx::params::parameterCount
                    && labelsOutsideField == tbx::params::parameterCount,
                "knob values stay in the central readout and labels sit below the knobs outside the field",
                "no text boxes = " + juce::String (slidersWithoutTextBoxes)
                    + ", labels below = " + juce::String (labelsBelowKnobs)
                    + ", labels outside = " + juce::String (labelsOutsideField));

        auto* presetBox = [&]() -> juce::ComboBox*
        {
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
                if (auto* combo = dynamic_cast<juce::ComboBox*> (editor->getChildComponent (i)))
                    return combo;

            return nullptr;
        }();

        report (editor->getWidth() == 590 && editor->getHeight() == 450,
                "editor uses compact 590x450 layout",
                juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()));
        report (presetBox != nullptr && presetBox->getNumItems() == 30
                    && presetBox->getTitle().isNotEmpty(),
                "preset selector exposes 30 accessible factory presets");

        bool dirtyPresetReloads = presetBox != nullptr && brightSlider != nullptr;

        if (dirtyPresetReloads)
        {
            presetBox->setSelectedId (13, juce::sendNotificationSync);
            brightSlider->setValue (55.0, juce::sendNotificationSync);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            dirtyPresetReloads = presetBox->getSelectedId() == 0
                                 && presetBox->getText().endsWithChar ('*');

            presetBox->setSelectedId (13, juce::sendNotificationSync);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            dirtyPresetReloads = dirtyPresetReloads
                                 && presetBox->getSelectedId() == 13
                                 && rig.processor.matchesFactoryPreset (13);
        }

        report (dirtyPresetReloads,
                "edited preset shows dirty marker and can reload the same factory preset");

        /*  중앙 리드아웃은 값이 바뀔 때 스스로 갱신되어야 한다(자체 타이머 없음).
            리드아웃 컴포넌트만 그려 값 변경 전후로 픽셀이 달라지는지 본다. Sparkle Field는
            BRIGHT에 따라 달라지므로 에디터 전체가 아니라 리드아웃만 스냅샷한다. */
        juce::Component* readout = nullptr;

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
            if (editor->getChildComponent (i)->getBounds() == juce::Rectangle<int> (200, 307, 190, 48))
                readout = editor->getChildComponent (i);

        report (readout != nullptr, "central readout sits inside the field at 200,307 190x48");

        if (brightSlider != nullptr && readout != nullptr)
        {
            auto readoutSignature = [&]
            {
                const auto image = readout->createComponentSnapshot (readout->getLocalBounds(),
                                                                     true, 1.0f);
                juce::uint64 sum = 0;

                for (int y = 0; y < image.getHeight(); ++y)
                    for (int x = 0; x < image.getWidth(); ++x)
                        sum = sum * 31 + (juce::uint64) image.getPixelAt (x, y).getARGB();

                return sum;
            };

            brightSlider->setValue (11.1, juce::sendNotificationSync);
            const auto before = readoutSignature();
            brightSlider->setValue (88.8, juce::sendNotificationSync);
            const auto after = readoutSignature();

            report (before != after,
                    "central readout follows the active knob value without its own timer");
        }
    }

    {
        Rig rig (48000.0, 64, 1);
        rig.setAll (1000.0f, 100.0f, 0.0f, 200.0f, 12.0f);

        juce::AudioBuffer<float> excitation (1, 256);
        fillNoise (excitation, 0.9, 8080);
        rig.render (excitation);

        rig.processor.reset();

        juce::AudioBuffer<float> silence (1, 512);
        silence.clear();
        rig.render (silence);

        report (silence.getMagnitude (0, silence.getNumSamples()) < 1.0e-7f,
                "호스트 reset 직후 delay/filter의 오래된 신호가 남지 않음");
    }

    {
        bool configurationsSafe = true;
        double worstLatencyMs = 0.0;

        for (auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (auto blockSize : { 64, 512, 2048 })
                for (auto channels : { 1, 2 })
                {
                    Rig rig (sampleRate, blockSize, channels);
                    rig.setAll (3000.0f, 70.0f, 45.0f, 130.0f, 0.0f);
                    rig.settle (0.03);

                    juce::AudioBuffer<float> buffer (channels, (int) (sampleRate * 0.05));
                    fillNoise (buffer, 0.5, 1776);
                    rig.render (buffer);

                    configurationsSafe = configurationsSafe
                                      && rig.layoutAccepted
                                      && isFiniteAndBounded (buffer, 8.0f);
                    worstLatencyMs = juce::jmax (worstLatencyMs,
                                                 1000.0 * (double) rig.latency() / sampleRate);
                }

        report (configurationsSafe && worstLatencyMs <= 2.0,
                "44.1~192 kHz, 64~2048 sample, mono/stereo 구성이 유한·유계",
                "worst latency = " + juce::String (worstLatencyMs, 3) + " ms");
    }

    {
        bool loudnessReferenceMatches = true;
        juce::String details;

        for (auto sampleRate : { 44100.0, 48000.0 })
        {
            juce::AudioBuffer<float> referenceTone (1, (int) (sampleRate * 3.0));
            fillSine (referenceTone, 1000.0, 0.1, sampleRate);
            const auto measured = integratedLoudness (referenceTone, sampleRate);
            loudnessReferenceMatches = loudnessReferenceMatches && std::abs (measured + 23.0) < 0.1;
            details << juce::String ((int) sampleRate) << " Hz=" << juce::String (measured, 3) << " LUFS  ";
        }

        report (loudnessReferenceMatches,
                "44.1/48 kHz BS.1770 라우드니스가 -23 LUFS 기준 톤과 일치",
                details);
    }

    {
        auto renderAutomation = [] (int blockSize)
        {
            Rig rig (48000.0, blockSize, 2);
            rig.setAll (500.0f, 0.0f, 0.0f, 100.0f, 0.0f);
            rig.settle();
            rig.setAll (6000.0f, 100.0f, 100.0f, 200.0f, 0.0f);

            juce::AudioBuffer<float> output (2, 8192);
            fillNoise (output, 0.4, 2048);
            rig.render (output);
            return output;
        };

        const auto reference = renderAutomation (32);

        double worstDifference = 0.0;

        for (auto blockSize : { 33, 257, 2048 })
        {
            const auto output = renderAutomation (blockSize);

            for (int channel = 0; channel < output.getNumChannels(); ++channel)
                for (int i = 0; i < output.getNumSamples(); ++i)
                    worstDifference = juce::jmax (
                        worstDifference,
                        (double) std::abs (reference.getSample (channel, i)
                                          - output.getSample (channel, i)));
        }

        report (worstDifference < 1.0e-6,
                "FOCUS/BRIGHT 자동화 결과가 32/33/257/2048 sample 호스트 블록 크기에 독립적",
                "worst difference = " + juce::String (worstDifference, 10));
    }
}

//==============================================================================
void testDryTransparency()
{
    section ("2. 원음 보존과 레이턴시 정렬");

    Rig rig (48000.0, 512, 2);
    rig.setAll (3000.0f, 0.0f, 30.0f, 50.0f, 0.0f);
    rig.settle();

    juce::AudioBuffer<float> input (2, 48000);
    fillNoise (input, 0.5, 909);

    juce::AudioBuffer<float> output (input);
    rig.render (output);

    const auto latency = rig.latency();
    double worstError = 0.0;

    for (int channel = 0; channel < 2; ++channel)
    {
        const auto* in  = input.getReadPointer (channel);
        const auto* out = output.getReadPointer (channel);

        for (int i = latency; i < input.getNumSamples(); ++i)
            worstError = juce::jmax (worstError, (double) std::abs (out[i] - in[i - latency]));
    }

    report (worstError < 1.0e-6, "BRIGHT 0 %에서 출력 = 지연 정렬된 드라이 (샘플 단위)",
            "worst error = " + juce::String (worstError, 10));

    {
        Rig bypassRig (48000.0, 512, 2);
        bypassRig.setAll (6000.0f, 100.0f, 100.0f, 200.0f, 12.0f);
        bypassRig.settle();

        juce::AudioBuffer<float> bypassInput (2, 48000);
        fillNoise (bypassInput, 0.5, 919);
        juce::AudioBuffer<float> bypassOutput (bypassInput);
        bypassRig.renderBypassed (bypassOutput);

        /*  settle()이 일반 처리로 돌았으므로 여기서 처리 → bypass 전환이 한 번 일어난다.
            전환 크로스페이드(10 ms)가 끝난 뒤의 정상상태를 본다. 전환 자체의 클릭과
            길이는 testHostTransitions가 따로 판정한다. */
        const auto steadyStart = bypassRig.latency() + (int) (0.015 * bypassRig.sampleRate);

        double bypassError = 0.0;
        for (int channel = 0; channel < bypassInput.getNumChannels(); ++channel)
        {
            const auto* in  = bypassInput.getReadPointer (channel);
            const auto* out = bypassOutput.getReadPointer (channel);

            for (int i = steadyStart; i < bypassInput.getNumSamples(); ++i)
                bypassError = juce::jmax (bypassError,
                                          (double) std::abs (out[i] - in[i - bypassRig.latency()]));
        }

        report (bypassError < 1.0e-6,
                "호스트 bypass도 보고 레이턴시만큼 정렬되고 LEVEL을 적용하지 않음",
                "worst error = " + juce::String (bypassError, 10));
    }

    {
        Rig offlineRig (48000.0, 512, 2);
        offlineRig.setAll (3000.0f, 0.0f, 45.0f, 100.0f, 0.0f);
        offlineRig.settle();

        const auto aligned = renderLatencyAligned (offlineRig, input);
        double alignmentError = 0.0;

        for (int channel = 0; channel < input.getNumChannels(); ++channel)
            for (int i = 0; i < input.getNumSamples(); ++i)
                alignmentError = juce::jmax (
                    alignmentError,
                    (double) std::abs (aligned.getSample (channel, i) - input.getSample (channel, i)));

        report (alignmentError < 1.0e-6,
                "오프라인 A/B 렌더가 플러그인 레이턴시를 제거하고 원래 길이를 보존",
                "worst error = " + juce::String (alignmentError, 10));
    }

    {
        Rig belowFocusRig (48000.0, 512, 1);
        belowFocusRig.setAll (6000.0f, 100.0f, 45.0f, 200.0f, 0.0f);
        belowFocusRig.settle();

        juce::AudioBuffer<float> belowFocusInput (1, 96000);
        fillSine (belowFocusInput, 100.0, 0.5, 48000.0);
        juce::AudioBuffer<float> belowFocusOutput (belowFocusInput);
        belowFocusRig.render (belowFocusOutput);

        const auto latencySamples = belowFocusRig.latency();
        const auto start = latencySamples + 4800;
        const auto period = 480;
        const auto count = (belowFocusOutput.getNumSamples() - start) / period * period;
        const auto end = start + count;
        const auto* in = belowFocusInput.getReadPointer (0);
        const auto* out = belowFocusOutput.getReadPointer (0);
        double inputEnergy = 0.0, quadratureEnergy = 0.0;
        double inPhaseCross = 0.0, quadratureCross = 0.0;

        for (int i = start; i < end; ++i)
        {
            const auto x = (double) in[i - latencySamples];
            const auto q = 0.5 * std::cos (juce::MathConstants<double>::twoPi
                                           * 100.0 * (i - latencySamples) / 48000.0);
            inputEnergy += x * x;
            quadratureEnergy += q * q;
            inPhaseCross += x * (double) out[i];
            quadratureCross += q * (double) out[i];
        }

        const auto inPhaseGain = inPhaseCross / juce::jmax (1.0e-30, inputEnergy);
        const auto quadratureGain = quadratureCross / juce::jmax (1.0e-30, quadratureEnergy);
        double residualEnergy = 0.0;

        for (int i = start; i < end; ++i)
        {
            const auto x = (double) in[i - latencySamples];
            const auto q = 0.5 * std::cos (juce::MathConstants<double>::twoPi
                                           * 100.0 * (i - latencySamples) / 48000.0);
            const auto error = (double) out[i] - inPhaseGain * x - quadratureGain * q;
            residualEnergy += error * error;
        }

        const auto fittedGain = std::sqrt (inPhaseGain * inPhaseGain
                                            + quadratureGain * quadratureGain);
        const auto gainDb = 20.0 * std::log10 (juce::jmax (1.0e-30, fittedGain));
        const auto phaseDegrees = std::atan2 (quadratureGain, inPhaseGain)
                                * 180.0 / juce::MathConstants<double>::pi;
        const auto residualDb = 10.0 * std::log10 (juce::jmax (
            1.0e-30, residualEnergy / juce::jmax (1.0e-30, inputEnergy)));

        /*  여기서 재는 위상은 BRIGHT 셸프(최소위상 2차)가 100 Hz에 남기는 선형 위상이다.
            RBJ 하이셸프 식으로 계산한 값과 실측이 소수 셋째 자리까지 같다:
              12 kHz / +5 dB   (2026-11-10까지, FOCUS 6 kHz)  0.153°
               6 kHz / +5 dB   (같은 시기의 기본 FOCUS 3 kHz)  0.370°
               9 kHz / +6.5 dB (2026-11-11 코너 상한 9 kHz)    0.299° = 약 8 µs
            이 검사는 FOCUS 6 kHz에서만 재므로 코너가 12 → 9 kHz로 내려온 만큼 값이 올랐다.
            이전 제품도 기본 FOCUS에서는 0.37°였으니 새로 생긴 위상 오차가 아니다.
            비선형 누설을 잡는 것은 gain과 잔류 항이고 그 둘의 바는 그대로 둔다. */
        report (std::abs (gainDb) <= 0.01 && std::abs (phaseDegrees) <= 0.35
                    && residualDb <= -70.0,
                "FOCUS 아래 원음은 gain 0.01 dB / phase 0.35° / 비선형 잔류 -70 dB 이하",
                "gain = " + juce::String (gainDb, 5)
                    + " dB / phase = " + juce::String (phaseDegrees, 3)
                    + "° / residual = " + juce::String (residualDb, 1) + " dB");
    }
}

//==============================================================================
void testTransparentHarmonicCeiling()
{
    section ("3. CLARITY 배음 구조와 투명성 상한");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr double inputAmplitude = 0.35;

    int fundamentalBin = 0;
    const auto frequency = binAlignedFrequency (700.0, sampleRate, fundamentalBin);
    const auto tune = 500.0f;

    juce::AudioBuffer<float> input (1, fftSize * 4);
    fillSine (input, frequency, inputAmplitude, sampleRate);

    auto measure = [&] (float character, float amount, double& evenEnergy, double& oddEnergy,
                        double& lowOrder, double& highOrder)
    {
        auto delta = renderEffectDelta (sampleRate, blockSize, 1, input, tune,
                                        character, 0.0f, amount);
        const auto offset = delta.getNumSamples() - fftSize;
        const auto magnitudes = magnitudeSpectrum (delta.getReadPointer (0) + offset, fftSize);

        double harmonic[13] {};
        for (int order = 2; order <= 12; ++order)
            harmonic[order] = peakAroundBin (magnitudes, fundamentalBin * order);

        evenEnergy = harmonic[2] * harmonic[2] + harmonic[4] * harmonic[4];
        oddEnergy  = harmonic[3] * harmonic[3] + harmonic[5] * harmonic[5];

        double lowEnergy = 0.0, highEnergy = 0.0;
        for (int order = 2; order <= 4; ++order)
            lowEnergy += harmonic[order] * harmonic[order];
        for (int order = 5; order <= 12; ++order)
            highEnergy += harmonic[order] * harmonic[order];

        lowOrder = std::sqrt (lowEnergy);
        highOrder = std::sqrt (highEnergy);

        std::cout << "        clarity=" << juce::String (character, 0)
                  << "  bright=" << juce::String (amount, 0)
                  << "  h2=" << db (harmonic[2]) << "  h3=" << db (harmonic[3])
                  << "  low/input=" << db (lowOrder / inputAmplitude)
                  << "  high/input=" << db (highOrder / inputAmplitude) << std::endl;
    };

    double warmEven = 0.0, warmOdd = 0.0, warmLow = 0.0, warmHigh = 0.0;
    double clearEven = 0.0, clearOdd = 0.0, clearLow = 0.0, clearHigh = 0.0;
    measure (0.0f, 100.0f, warmEven, warmOdd, warmLow, warmHigh);
    measure (100.0f, 100.0f, clearEven, clearOdd, clearLow, clearHigh);

    const auto warmRatio = warmEven / juce::jmax (1.0e-30, warmOdd);
    const auto clearRatio = clearEven / juce::jmax (1.0e-30, clearOdd);

    report (warmRatio > 1.0, "WARM은 짝수 배음 우세",
            "even/odd = " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, warmRatio)), 1) + " dB");
    report (clearRatio < 0.1, "CLEAR는 홀수 배음 우세",
            "even/odd = " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, clearRatio)), 1) + " dB");
    report (warmRatio / juce::jmax (1.0e-30, clearRatio) > 100.0,
            "CLARITY 양 끝의 짝수/홀수 구성이 반전됨",
            "separation = " + juce::String (10.0 * std::log10 (
                juce::jmax (1.0e-30, warmRatio / juce::jmax (1.0e-30, clearRatio))), 1) + " dB");

    const auto fullLowDb = 20.0 * std::log10 (juce::jmax (1.0e-30,
        juce::jmax (warmLow, clearLow) / inputAmplitude));
    const auto fullHighDb = 20.0 * std::log10 (juce::jmax (1.0e-30,
        juce::jmax (warmHigh, clearHigh) / inputAmplitude));

    report (fullLowDb <= -20.0, "BRIGHT 100 저차 배음이 입력 대비 -20 dBc 이하",
            "worst = " + juce::String (fullLowDb, 2) + " dBc");
    report (fullHighDb <= -40.0, "BRIGHT 100 고차 배음이 입력 대비 -40 dBc 이하",
            "worst = " + juce::String (fullHighDb, 2) + " dBc");

    double defaultEven = 0.0, defaultOdd = 0.0, defaultLow = 0.0, defaultHigh = 0.0;
    measure (45.0f, 40.0f, defaultEven, defaultOdd, defaultLow, defaultHigh);
    const auto defaultLowDb = 20.0 * std::log10 (juce::jmax (1.0e-30, defaultLow / inputAmplitude));
    const auto defaultHighDb = 20.0 * std::log10 (juce::jmax (1.0e-30, defaultHigh / inputAmplitude));

    report (defaultLowDb <= -30.0 && defaultHighDb <= -50.0,
            "기본값 배음은 저차 -30 dBc / 고차 -50 dBc 이하",
            "low = " + juce::String (defaultLowDb, 2)
                + " dBc / high = " + juce::String (defaultHighDb, 2) + " dBc");

    double quietestRatioDb = 1000.0, loudestRatioDb = -1000.0;
    for (double amplitude : { 0.05, 0.2, 0.8 })
    {
        juce::AudioBuffer<float> levelInput (1, fftSize * 4);
        fillSine (levelInput, frequency, amplitude, sampleRate);
        auto delta = renderEffectDelta (sampleRate, blockSize, 1, levelInput, tune,
                                        45.0f, 0.0f, 40.0f);
        const auto magnitudes = magnitudeSpectrum (
            delta.getReadPointer (0) + delta.getNumSamples() - fftSize, fftSize);
        const auto h2 = peakAroundBin (magnitudes, fundamentalBin * 2);
        const auto h3 = peakAroundBin (magnitudes, fundamentalBin * 3);
        const auto ratioDb = 20.0 * std::log10 (juce::jmax (
            1.0e-30, std::sqrt (h2 * h2 + h3 * h3) / amplitude));
        quietestRatioDb = juce::jmin (quietestRatioDb, ratioDb);
        loudestRatioDb = juce::jmax (loudestRatioDb, ratioDb);
    }

    report (loudestRatioDb <= -30.0 && loudestRatioDb - quietestRatioDb <= 2.0,
            "adaptive 배음 비율이 레벨에 무관하며 기본값 -30 dBc 이하",
            "worst = " + juce::String (loudestRatioDb, 2)
                + " dBc / spread = " + juce::String (loudestRatioDb - quietestRatioDb, 2) + " dB");
}

//==============================================================================
void testAliasing()
{
    section ("4. 앨리어싱");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    int fundamentalBin = 0;
    const auto frequency = binAlignedFrequency (2900.0, sampleRate, fundamentalBin);

    juce::AudioBuffer<float> input (1, fftSize * 4);
    fillSine (input, frequency, 0.5, sampleRate);

    auto wet = renderEffectDelta (sampleRate, blockSize, 1, input, 2000.0f, 50.0f, 0.0f);
    const auto offset = wet.getNumSamples() - fftSize;
    const auto magnitudes = magnitudeSpectrum (wet.getReadPointer (0) + offset, fftSize);

    const auto lowBin  = (int) (1000.0 * fftSize / sampleRate);
    const auto highBin = (int) (18000.0 * fftSize / sampleRate);

    double harmonicPeak = 0.0;
    double aliasPeak = 0.0;
    int aliasBin = 0;

    for (int bin = lowBin; bin <= highBin; ++bin)
    {
        bool nearHarmonic = false;
        const auto nearFundamental = std::abs (bin - fundamentalBin) <= 24;

        // ±35 Hz. 기음·배음 바로 옆의 저주파 변조 사이드밴드를 앨리어싱으로 오판하지 않는다.
        for (int order = 2; order <= 12; ++order)
            if (std::abs (bin - fundamentalBin * order) <= 24)
                nearHarmonic = true;

        const auto magnitude = (double) magnitudes[(size_t) bin];

        if (nearHarmonic)
        {
            harmonicPeak = juce::jmax (harmonicPeak, magnitude);
        }
        else if (! nearFundamental && magnitude > aliasPeak)
        {
            aliasPeak = magnitude;
            aliasBin = bin;
        }
    }

    const auto aliasHz = aliasBin * sampleRate / fftSize;

    report (aliasPeak < 1.0e-5, "비하모닉(앨리어싱) 성분 ≤ -100 dBFS",
            "alias peak = " + db (aliasPeak) + " @ " + juce::String (aliasHz, 0) + " Hz"
                + " / harmonic peak = " + db (harmonicPeak));

    const auto aliasToHarmonic = aliasPeak / juce::jmax (1.0e-30, harmonicPeak);
    report (aliasToHarmonic <= 0.0031623,
            "앨리어싱이 의도된 배음보다 -50 dB 이하",
            "alias/harmonic = " + juce::String (20.0 * std::log10 (
                juce::jmax (1.0e-30, aliasToHarmonic)), 1) + " dB");
}

//==============================================================================
void testTransientContainment()
{
    section ("5. 트랜지언트 배음 억제");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;
    constexpr double inputAmplitude = 0.5;

    const auto onset = (int) (sampleRate * 0.5);
    juce::AudioBuffer<float> input (1, (int) (sampleRate * 1.2));
    input.clear();
    fillSine (input, 2800.0, inputAmplitude, sampleRate, onset);

    struct Levels { double early = 0.0, late = 0.0; };

    auto measure = [&] (float character, float amount)
    {
        auto delta = renderEffectDelta (sampleRate, blockSize, 1, input, 2000.0f,
                                        character, 100.0f, amount);
        Rig probe (sampleRate, blockSize, 1);
        const auto latency = probe.latency();
        const auto earlyStart = onset + latency + (int) (sampleRate * 0.001);
        const auto lateStart  = onset + latency + (int) (sampleRate * 0.250);
        const auto window     = (int) (sampleRate * 0.014);

        auto harmonicLevel = [&] (int start)
        {
            const auto second = toneAmplitude (delta, start, window, 5600.0, sampleRate);
            const auto third  = toneAmplitude (delta, start, window, 8400.0, sampleRate);
            return std::sqrt (second * second + third * third);
        };

        Levels levels { harmonicLevel (earlyStart), harmonicLevel (lateStart) };
        std::cout << "        clarity=" << juce::String (character, 0)
                  << "  bright=" << juce::String (amount, 0)
                  << "  early/input=" << db (levels.early / inputAmplitude)
                  << "  late/input=" << db (levels.late / inputAmplitude)
                  << "  overshoot=" << juce::String (20.0 * std::log10 (
                      juce::jmax (1.0e-30, levels.early / juce::jmax (1.0e-30, levels.late))), 2)
                  << " dB" << std::endl;
        return levels;
    };

    const auto warm = measure (0.0f, 100.0f);
    const auto clear = measure (100.0f, 100.0f);
    const auto defaults = measure (45.0f, 40.0f);

    const auto fullEarlyDb = 20.0 * std::log10 (juce::jmax (
        1.0e-30, juce::jmax (warm.early, clear.early) / inputAmplitude));
    const auto defaultEarlyDb = 20.0 * std::log10 (juce::jmax (
        1.0e-30, defaults.early / inputAmplitude));
    const auto warmOvershoot = 20.0 * std::log10 (juce::jmax (
        1.0e-30, warm.early / juce::jmax (1.0e-30, warm.late)));
    const auto clearOvershoot = 20.0 * std::log10 (juce::jmax (
        1.0e-30, clear.early / juce::jmax (1.0e-30, clear.late)));
    const auto defaultOvershoot = 20.0 * std::log10 (juce::jmax (
        1.0e-30, defaults.early / juce::jmax (1.0e-30, defaults.late)));
    const auto worstOvershoot = juce::jmax (warmOvershoot,
                                             juce::jmax (clearOvershoot, defaultOvershoot));

    /*  이 절대 레벨 바는 배음이 지금보다 6 dB 낮던(제품이 사실상 EQ이던) 시점에 정해졌다.
        FOCUS 틸트로 에어 대역 배음을 의도적으로 올렸으므로 최대 설정의 절대치도 올라간다.

        트랜지언트 억제의 실질적 보증은 바로 아래의 **돌출 검사**(서스테인 대비 +3 dB 이내)이며
        그쪽은 +1.00 dB로 여유 있게 통과한다. 즉 onset이 유독 튀는 것이 아니라 전체 배음
        레벨이 올라간 것이다. 그래서 이 바만 -15 dBc로 조정한다. 돌출 바는 건드리지 않는다.

        2026-11-11: 청취 비교에서 사용자가 4_air_sparkle(높은 FOCUS 배음량 2.0 → 2.8배)을
        선택했다. 배음층이 통째로 약 2.6 dB 올라가 최대 설정 실측이 -15.12 → -12.54 dBc가
        됐고, 돌출은 1.93 → 1.86 dB로 그대로다. 같은 이유로 이 바만 -12 dBc로 옮긴다.
        기본값 바(-24 dBc, 실측 -24.62)와 돌출 바는 그대로 둔다. */
    report (fullEarlyDb <= -12.0, "BRIGHT 100 onset 배음이 입력 대비 -12 dBc 이하",
            "worst = " + juce::String (fullEarlyDb, 2) + " dBc");
    report (defaultEarlyDb <= -24.0, "기본값 onset 배음이 입력 대비 -24 dBc 이하",
            "level = " + juce::String (defaultEarlyDb, 2) + " dBc");
    report (worstOvershoot <= 3.0, "onset 배음 돌출이 +3 dB 이하",
            "worst = " + juce::String (worstOvershoot, 2) + " dB");
}

//==============================================================================
/*  두 톤을 넣고, 생성된 wet 중 "예측 가능한 합 성분"과 "그 외 잡음 구름"의 비를 잰다.

    메모리리스 비선형은 대역 안의 모든 성분 쌍에서 합·차음을 만들어 이산 배음이 아니라
    잡음 구름을 만든다. 그것이 청취에서 지적된 "초고역 지글거림"이다.
    해석신호 기반 배가는 차음과 DC가 원리적으로 없으므로 이 비가 낮아야 한다. */
void testIntermodulation()
{
    section ("6. 상호변조(IMD) — 지글거림 지표");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    const auto tune = 3000.0f;

    int bin1 = 0, bin2 = 0;
    const auto f1 = binAlignedFrequency (3400.0, sampleRate, bin1);
    const auto f2 = binAlignedFrequency (4300.0, sampleRate, bin2);

    juce::AudioBuffer<float> input (1, fftSize * 4);
    {
        auto* data = input.getWritePointer (0);
        const auto w1 = juce::MathConstants<double>::twoPi * f1 / sampleRate;
        const auto w2 = juce::MathConstants<double>::twoPi * f2 / sampleRate;

        for (int i = 0; i < input.getNumSamples(); ++i)
            data[i] = (float) (0.25 * (std::sin (w1 * i) + std::sin (w2 * i)));
    }

    // 3차까지의 합 성분만이 "예측 가능한" 결과다.
    const int cleanBins[]
    {
        2 * bin1, bin1 + bin2, 2 * bin2,
        3 * bin1, 2 * bin1 + bin2, bin1 + 2 * bin2, 3 * bin2
    };

    auto measure = [&] (float character)
    {
        auto wet = renderEffectDelta (sampleRate, blockSize, 1, input, tune, character, 0.0f);
        const auto offset = wet.getNumSamples() - fftSize;
        const auto magnitudes = magnitudeSpectrum (wet.getReadPointer (0) + offset, fftSize);

        const auto lowBin  = (int) (tune * 1.8 * fftSize / sampleRate);
        const auto highBin = (int) (18000.0 * fftSize / sampleRate);

        double cleanEnergy = 0.0, junkEnergy = 0.0;

        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            bool isClean = false;

            for (auto cleanBin : cleanBins)
                if (std::abs (bin - cleanBin) <= 6)
                    isClean = true;

            const auto energy = (double) magnitudes[(size_t) bin] * magnitudes[(size_t) bin];

            if (isClean)
                cleanEnergy += energy;
            else
                junkEnergy += energy;
        }

        const auto ratio = junkEnergy / juce::jmax (1.0e-30, cleanEnergy);

        std::cout << "        character=" << juce::String (character, 0)
                  << "  잡음구름/합성분 = "
                  << juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, ratio)), 1) << " dB"
                  << std::endl;

        return ratio;
    };

    const auto warmRatio = measure (0.0f);
    const auto clearRatio = measure (100.0f);

    report (clearRatio < 1.5849e-6,
            "CLEAR의 비조화성 IMD 구름이 예측 합 성분 대비 -58 dB 이하",
            "ratio = " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, clearRatio)), 1) + " dB");

    report (warmRatio < 1.5849e-6,
            "WARM의 비조화성 IMD 구름이 예측 합 성분 대비 -58 dB 이하",
            "ratio = " + juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, warmRatio)), 1) + " dB");
}

//==============================================================================
// bandRms는 캘리브레이션 절에 정의되어 있다.
double bandRms (const juce::AudioBuffer<float>& buffer, double sampleRate,
                double lowHz, double highHz);

//==============================================================================
//  제품 가치 게이트 (하한)
//
//  기존 검사는 전부 "결함이 없음"을 재는 지표라, 아무 일도 하지 않는 플러그인이
//  만점을 받는다. 실제로 청취 실패가 날 때마다 효과를 줄이는 방향으로 대응했고
//  그때마다 전 검사가 통과했다. 그 루프를 끊기 위한 **하한** 게이트다.
//==============================================================================

/*  급준 로우패스. Butterworth 2차를 여러 단 쌓아 대역제한 소재를 만든다.
    8단이면 96 dB/oct 이므로 컷오프의 1.2배 지점에서 약 25 dB 떨어진다. */
void applyCascadedLowPass (juce::AudioBuffer<float>& buffer, double sampleRate,
                           double cutoffHz, int sections = 8)
{
    constexpr auto q = juce::MathConstants<float>::sqrt2 * 0.5f;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, (float) cutoffHz, q);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        std::vector<juce::dsp::IIR::Filter<float>> filters ((size_t) sections);

        for (auto& filter : filters)
            filter.coefficients = coeffs;

        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto value = data[i];

            for (auto& filter : filters)
                value = filter.processSample (value);

            data[i] = value;
        }
    }
}

bool loadAsset (const juce::String& fileName, juce::AudioBuffer<float>& destination,
                double& sampleRate, int& numChannels)
{
    const juce::File file (juce::String ("E:/Plugin Projects/Guide MD/Assets/") + fileName);

    if (! file.existsAsFile())
        return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return false;

    sampleRate  = reader->sampleRate;
    numChannels = (int) juce::jmin ((unsigned int) 2, reader->numChannels);

    destination.setSize (numChannels, (int) reader->lengthInSamples);
    reader->read (&destination, 0, (int) reader->lengthInSamples, 0, true, numChannels > 1);
    return true;
}

/*  하이셸프 기준기. 플러그인과 같은 대역 변화량을 내도록 게인을 맞춘 뒤,
    "무엇이 더해졌는지"를 플러그인의 added 신호와 직접 비교한다. */
juce::AudioBuffer<float> makeShelfDelta (const juce::AudioBuffer<float>& source,
                                         double sampleRate, double cornerHz, double gainDb)
{
    juce::AudioBuffer<float> shelved (source);
    constexpr auto q = juce::MathConstants<float>::sqrt2 * 0.5f;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, (float) cornerHz, q, juce::Decibels::decibelsToGain ((float) gainDb));

    for (int channel = 0; channel < shelved.getNumChannels(); ++channel)
    {
        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = coeffs;
        auto* data = shelved.getWritePointer (channel);

        for (int i = 0; i < shelved.getNumSamples(); ++i)
            data[i] = filter.processSample (data[i]);

        juce::FloatVectorOperations::subtract (data, source.getReadPointer (channel),
                                               shelved.getNumSamples());
    }

    return shelved;
}

double normalisedCorrelation (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    double cross = 0.0, energyA = 0.0, energyB = 0.0;
    const auto n = juce::jmin (a.getNumSamples(), b.getNumSamples());

    for (int channel = 0; channel < juce::jmin (a.getNumChannels(), b.getNumChannels()); ++channel)
    {
        const auto* x = a.getReadPointer (channel);
        const auto* y = b.getReadPointer (channel);

        for (int i = 0; i < n; ++i)
        {
            cross   += (double) x[i] * y[i];
            energyA += (double) x[i] * x[i];
            energyB += (double) y[i] * y[i];
        }
    }

    return std::abs (cross) / juce::jmax (1.0e-30, std::sqrt (energyA * energyB));
}

void testProductValue()
{
    section ("제품 가치 게이트 (하한)");

    constexpr int blockSize = 512;
    constexpr double cutoffHz = 10000.0;
    constexpr double measureFromHz = 12000.0;   // 컷오프 위 여유

    juce::AudioBuffer<float> full;
    double sampleRate = 0.0;
    int numChannels = 0;

    if (! loadAsset ("BUS_01_fullmix.wav", full, sampleRate, numChannels))
    {
        report (false, "검증 소재 BUS_01_fullmix.wav 를 찾을 수 없음");
        return;
    }

    /*  게이트 A: 대역 확장.
        10 kHz에서 잘린 소재에서는 셸프가 올릴 것이 없으므로 원리적으로 아무것도 못 한다.
        이 구간을 채우는 것은 배음 생성만 할 수 있고 EQ는 경쟁 자체가 안 된다.
        익사이터가 EQ 대비 유일하게 우월한 지점이므로 여기를 제품의 하한으로 삼는다. */
    juce::AudioBuffer<float> limited (full);
    applyCascadedLowPass (limited, sampleRate, cutoffHz);

    /*  기준(분모)은 소재가 컷오프 위에 남긴 값이 아니라 **컷오프 바로 아래 옥타브**로 잡는다.
        컷오프 위 잔량은 테스트용 LPF의 누설일 뿐이라, 그걸 분모로 쓰면 제품이 아니라
        내 필터의 급준도를 재게 된다. 실제로 처음엔 그렇게 재고 있었다.

        "잘린 대역 바로 아래만큼의 정보를 그 위에 만들어 주는가"가 음악적으로 의미 있는
        질문이고, 이 값이 -15 dB 안쪽이면 대역이 실제로 확장된 것으로 들린다. */
    const auto sourceShoulder = bandRms (limited, sampleRate, 6000.0, cutoffHz);
    const auto sourceAboveCutoff = bandRms (limited, sampleRate, measureFromHz, 20000.0);

    for (float bright : { tbx::params::amountDefault, 100.0f })
    {
        auto added = renderEffectDelta (sampleRate, blockSize, numChannels, limited,
                                        tbx::params::tuneDefaultHz, tbx::params::characterDefault,
                                        tbx::params::widthDefault, bright);

        const auto createdAboveCutoff = bandRms (added, sampleRate, measureFromHz, 20000.0);
        const auto extensionDb = 20.0 * std::log10 (
            juce::jmax (1.0e-30, createdAboveCutoff / juce::jmax (1.0e-30, sourceShoulder)));

        std::cout << "        BRIGHT=" << juce::String (bright, 0)
                  << "   shoulder 6-10k = " << db (sourceShoulder)
                  << "   leakage 12k+ = " << db (sourceAboveCutoff)
                  << "   created 12k+ = " << db (createdAboveCutoff)
                  << "   extension = " << juce::String (extensionDb, 2) << " dB (어깨 기준)"
                  << std::endl;

        const auto floorDb = juce::approximatelyEqual (bright, 100.0f) ? -15.0 : -25.0;

        report (extensionDb > floorDb,
                "대역제한 소재의 컷오프 위에 어깨 대비 의미 있는 정보를 만든다 (BRIGHT "
                    + juce::String (bright, 0) + " %, 기준 " + juce::String (floorDb, 0) + " dB)",
                "extension = " + juce::String (extensionDb, 2) + " dB");
    }

    /*  게이트 B: 하이셸프와 구분되는가.
        더해진 성분이 하이셸프가 더하는 것과 사실상 같다면 이 제품은 EQ다.
        브리프의 실패 조건을 자동 판정으로 바꾼 것이다. */
    {
        auto added = renderEffectDelta (sampleRate, blockSize, numChannels, full,
                                        tbx::params::tuneDefaultHz, tbx::params::characterDefault,
                                        tbx::params::widthDefault, tbx::params::amountDefault);

        const auto targetBand = bandRms (added, sampleRate, 6000.0, 16000.0);
        /*  대조 셸프의 코너는 제품 셸프의 코너(3×FOCUS, 2026-11-11 보이싱)와 같아야 한다.
            2×FOCUS로 두면 코너가 다르다는 이유만으로 상관이 떨어져 이 게이트가 느슨해진다. */
        const auto corner = tbx::params::tuneDefaultHz * 3.0;

        // 셸프 게인은 단조 증가하므로 이분 탐색으로 충분하다. 선형 스캔 120회는 느리다.
        double low = 0.05, high = 14.0;

        for (int iteration = 0; iteration < 14; ++iteration)
        {
            const auto middle = 0.5 * (low + high);
            auto shelf = makeShelfDelta (full, sampleRate, corner, middle);

            if (bandRms (shelf, sampleRate, 6000.0, 16000.0) < targetBand)
                low = middle;
            else
                high = middle;
        }

        const auto bestGain = 0.5 * (low + high);

        auto shelfDelta = makeShelfDelta (full, sampleRate, corner, bestGain);

        /*  플러그인 출력은 보고 레이턴시만큼 밀려 있다. 정렬하지 않고 상관을 재면
            고역에서는 몇 샘플만 어긋나도 상관이 0으로 떨어져 무조건 통과해 버린다. */
        {
            Rig probe (sampleRate, blockSize, numChannels);
            const auto latency = probe.latency();

            if (latency > 0)
            {
                juce::AudioBuffer<float> aligned (shelfDelta.getNumChannels(), shelfDelta.getNumSamples());
                aligned.clear();

                for (int channel = 0; channel < shelfDelta.getNumChannels(); ++channel)
                    aligned.copyFrom (channel, latency, shelfDelta, channel, 0,
                                      shelfDelta.getNumSamples() - latency);

                shelfDelta = std::move (aligned);
            }
        }

        const auto likeness = normalisedCorrelation (added, shelfDelta);

        std::cout << "        matched shelf = +" << juce::String (bestGain, 1) << " dB @ "
                  << juce::String (corner / 1000.0, 1) << " kHz"
                  << "   correlation = " << juce::String (likeness, 3)
                  << "  (1.0 = 셸프와 동일, 0 = 완전히 새 정보)" << std::endl;

        /*  0.90은 측정 전에 임의로 잡은 값이었고, 이 제품에는 맞지 않는다.
            상관 0.90은 셸프와 배음이 거의 같은 에너지라는 뜻인데, 사용자가 명시적으로
            선택한 설계는 "셸프 위주 + 배음 소량"이다. 그 결정과 정면으로 모순된다.

            의미 있는 질문은 에너지 비중이 아니라 "셸프가 만들 수 없는 성분이 들릴 만큼
            있는가"이다. 비셸프 성분의 비율은 sqrt(1 - corr^2)이므로 상관 0.98이
            비셸프 -14 dB에 해당한다. 그 지점을 바로 삼는다.
            셸프로 대체 불가능한 정도는 대역제한 소재의 게이트 A가 별도로 보증한다. */
        report (likeness < 0.98,
                "더해진 성분에 셸프가 만들 수 없는 부분이 -14 dB 이상 있다 (상관 < 0.98)",
                "correlation = " + juce::String (likeness, 3)
                    + " / 비셸프 = " + juce::String (20.0 * std::log10 (
                        juce::jmax (1.0e-9, std::sqrt (juce::jmax (0.0, 1.0 - likeness * likeness)))), 1) + " dB");
    }
}

//==============================================================================
void testAmountMonotonicity()
{
    section ("7. BRIGHT 단조성과 캘리브레이션");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    juce::AudioBuffer<float> input (1, (int) sampleRate);
    fillNoise (input, 0.3, 4242);

    double previous = -1.0;
    bool monotonic = true;
    double defaultRatio = 0.0, fullRatio = 0.0;

    for (float amount : { 0.0f, 25.0f, 40.0f, 75.0f, 100.0f })
    {
        juce::AudioBuffer<float> dryRun (input);
        {
            Rig rig (sampleRate, blockSize, 1);
            rig.setAll (3000.0f, 0.0f, 30.0f, 50.0f, 0.0f);
            rig.settle();
            rig.render (dryRun);
        }

        juce::AudioBuffer<float> wetRun (input);
        {
            Rig rig (sampleRate, blockSize, 1);
            rig.setAll (3000.0f, amount, 30.0f, 50.0f, 0.0f);
            rig.settle();
            rig.render (wetRun);
        }

        juce::FloatVectorOperations::subtract (wetRun.getWritePointer (0),
                                               dryRun.getReadPointer (0),
                                               wetRun.getNumSamples());

        const auto level = rms (wetRun, 12000, 24000);
        const auto dryLevel = rms (dryRun, 12000, 24000);

        std::cout << "        amount=" << juce::String (amount, 0)
                  << "  effect delta rms=" << db (level)
                  << "  (dry rms=" << db (dryLevel) << ")" << std::endl;

        if (level < previous)
            monotonic = false;

        previous = level;

        if (amount == 40.0f)
            defaultRatio = level / juce::jmax (1.0e-30, dryLevel);
        if (amount == 100.0f)
            fullRatio = level / juce::jmax (1.0e-30, dryLevel);
    }

    report (monotonic, "BRIGHT 증가에 따라 추가되는 상단 성분 레벨이 단조 증가");
    const auto defaultDb = 20.0 * std::log10 (juce::jmax (1.0e-30, defaultRatio));
    const auto fullDb = 20.0 * std::log10 (juce::jmax (1.0e-30, fullRatio));
    report (defaultDb <= -10.0 && fullDb <= 0.0,
            "effect delta는 기본값 -10 dB / BRIGHT 100 0 dB 이하",
            "default = " + juce::String (defaultDb, 2)
                + " dB / full = " + juce::String (fullDb, 2) + " dB");
}

//==============================================================================
void testResonanceSafeShelf()
{
    section ("8. Resonance-safe BRIGHT shelf");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int numSamples = 96000;
    constexpr int measureStart = 48000;
    constexpr int measureCount = 32768;
    constexpr float focusHz = 500.0f;
    constexpr double centres[]
        { 1500.0, 2121.32, 3000.0, 4242.64, 6000.0, 8485.28, 12000.0, 16970.56 };

    auto addTone = [sampleRate] (juce::AudioBuffer<float>& buffer, int channel,
                                 double frequency, double amplitude)
    {
        auto* data = buffer.getWritePointer (channel);
        const auto increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] += (float) (amplitude * std::sin (increment * i));
    };

    auto renderAt = [sampleRate, blockSize, focusHz] (
                        const juce::AudioBuffer<float>& input, float bright)
    {
        Rig rig (sampleRate, blockSize, input.getNumChannels());
        rig.setAll (focusHz, bright, 100.0f, 100.0f, 0.0f);
        rig.settle();
        return renderLatencyAligned (rig, input);
    };

    auto gainDbAt = [measureStart, measureCount, sampleRate] (
                        const juce::AudioBuffer<float>& output,
                        const juce::AudioBuffer<float>& input,
                        double frequency, int channel = 0)
    {
        const auto inputAmplitude = toneAmplitude (input, measureStart, measureCount,
                                                   frequency, sampleRate, channel);
        const auto outputAmplitude = toneAmplitude (output, measureStart, measureCount,
                                                    frequency, sampleRate, channel);
        return 20.0 * std::log10 (juce::jmax (1.0e-12, outputAmplitude)
                                  / juce::jmax (1.0e-12, inputAmplitude));
    };

    bool monotonic = true;
    double smallestStep = 1000.0;
    double weakestFullGain = 1000.0;

    for (int band = 1; band < 8; ++band)
    {
        double previousGain = -1000.0;
        juce::AudioBuffer<float> resonanceTone (1, numSamples);
        resonanceTone.clear();
        addTone (resonanceTone, 0, centres[band], 0.04);

        for (float bright : { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f })
        {
            const auto output = renderAt (resonanceTone, bright);
            const auto gain = gainDbAt (output, resonanceTone, centres[band]);

            if (previousGain > -999.0)
            {
                const auto step = gain - previousGain;
                smallestStep = juce::jmin (smallestStep, step);
                monotonic = monotonic && step > 0.10;
            }

            previousGain = gain;
        }

        weakestFullGain = juce::jmin (weakestFullGain, previousGain);
    }

    report (monotonic && weakestFullGain > 3.0,
            "BRIGHT always increases strong resonances across the shelf instead of darkening them",
            "smallest step = " + juce::String (smallestStep, 2)
                + " dB / weakest full gain = " + juce::String (weakestFullGain, 2) + " dB");

    juce::AudioBuffer<float> resonantSpectrum (1, numSamples);
    resonantSpectrum.clear();

    for (auto frequency : centres)
        addTone (resonantSpectrum, 0, frequency, 0.002);

    addTone (resonantSpectrum, 0, centres[5], 0.078);
    const auto protectedOutput = renderAt (resonantSpectrum, 100.0f);
    const auto resonanceGain = gainDbAt (protectedOutput, resonantSpectrum, centres[5]);
    const auto lowerAirGain = gainDbAt (protectedOutput, resonantSpectrum, centres[4]);
    const auto upperAirGain = gainDbAt (protectedOutput, resonantSpectrum, centres[6]);
    const auto airGain = 0.5 * (lowerAirGain + upperAirGain);
    const auto slopeRatio = resonanceGain / juce::jmax (1.0e-9, airGain);

    report (resonanceGain > 3.0 && airGain > resonanceGain + 0.45
                && slopeRatio >= 0.72 && slopeRatio <= 0.90,
            "a narrow resonance receives a gentler but still-positive BRIGHT slope",
            "resonance = " + juce::String (resonanceGain, 2)
                + " dB / air = " + juce::String (airGain, 2)
                + " dB / ratio = " + juce::String (slopeRatio, 3));

    juce::AudioBuffer<float> topResonantSpectrum (1, numSamples);
    topResonantSpectrum.clear();

    for (auto frequency : centres)
        addTone (topResonantSpectrum, 0, frequency, 0.002);

    addTone (topResonantSpectrum, 0, centres[7], 0.078);
    const auto topOutput = renderAt (topResonantSpectrum, 100.0f);
    const auto topResonanceGain = gainDbAt (topOutput, topResonantSpectrum, centres[7]);
    const auto topAirGain = gainDbAt (topOutput, topResonantSpectrum, centres[6]);
    const auto topSlopeRatio = topResonanceGain / juce::jmax (1.0e-9, topAirGain);

    report (topResonanceGain > 3.0 && topAirGain > topResonanceGain + 0.25
                && topSlopeRatio >= 0.75 && topSlopeRatio <= 0.95,
            "the upper-air edge protects a narrow 17 kHz resonance without closing the shelf",
            "resonance = " + juce::String (topResonanceGain, 2)
                + " dB / air = " + juce::String (topAirGain, 2)
                + " dB / ratio = " + juce::String (topSlopeRatio, 3));

    juce::AudioBuffer<float> evenSpectrum (1, numSamples);
    evenSpectrum.clear();

    for (auto frequency : centres)
        addTone (evenSpectrum, 0, frequency, 0.008);

    const auto evenOutput = renderAt (evenSpectrum, 100.0f);
    double minimumCleanGain = 1000.0;
    double maximumCleanGain = -1000.0;

    for (int index : { 4, 5, 6 })
    {
        const auto gain = gainDbAt (evenOutput, evenSpectrum, centres[index]);
        minimumCleanGain = juce::jmin (minimumCleanGain, gain);
        maximumCleanGain = juce::jmax (maximumCleanGain, gain);
    }

    // 셸프 최대 게인 +6.5 dB(2026-11-11 보이싱) 기준. 허용 폭은 +5 dB 때와 같다.
    report (minimumCleanGain >= 6.05 && maximumCleanGain <= 6.75
                && maximumCleanGain - minimumCleanGain <= 0.35,
            "an even high-frequency spectrum keeps the full open-air shelf",
            "range = " + juce::String (minimumCleanGain, 2) + ".."
                + juce::String (maximumCleanGain, 2) + " dB");

    {
        Rig rig (sampleRate, blockSize, 1);
        report (rig.latency() == 61,
                "resonance protection adds no host latency",
                "latency = " + juce::String (rig.latency()) + " samples");
    }

    {
        Rig contaminated (sampleRate, blockSize, 1);
        contaminated.setAll (focusHz, 100.0f, 100.0f, 100.0f, 0.0f);
        contaminated.settle();
        juce::AudioBuffer<float> contaminant (1, 24000);
        fillSine (contaminant, centres[5], 0.8, sampleRate);
        contaminated.render (contaminant);
        contaminated.processor.reset();

        Rig fresh (sampleRate, blockSize, 1);
        fresh.setAll (focusHz, 100.0f, 100.0f, 100.0f, 0.0f);
        fresh.settle();
        fresh.processor.reset();

        juce::AudioBuffer<float> probeA (resonantSpectrum);
        juce::AudioBuffer<float> probeB (resonantSpectrum);
        contaminated.render (probeA);
        fresh.render (probeB);

        double worstError = 0.0;
        for (int i = 0; i < probeA.getNumSamples(); ++i)
            worstError = juce::jmax (worstError,
                                     std::abs ((double) probeA.getSample (0, i)
                                               - probeB.getSample (0, i)));

        report (worstError < 1.0e-6,
                "reset clears resonance detector and protector history",
                "worst error = " + juce::String (worstError, 10));
    }

    {
        juce::AudioBuffer<float> stereo (2, numSamples);
        stereo.clear();
        addTone (stereo, 0, centres[5], 0.08);
        addTone (stereo, 1, centres[5], 0.008);
        const auto stereoOutput = renderAt (stereo, 100.0f);
        const auto leftGain = gainDbAt (stereoOutput, stereo, centres[5], 0);
        const auto rightGain = gainDbAt (stereoOutput, stereo, centres[5], 1);

        report (std::abs (leftGain - rightGain) <= 0.15,
                "stereo-linked resonance protection preserves image balance",
                "L/R gain = " + juce::String (leftGain, 2) + "/"
                    + juce::String (rightGain, 2) + " dB");
    }
}

//==============================================================================
void testWidthBehaviour()
{
    section ("9. WIDTH added 경로");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    auto renderAtWidth = [sampleRate, blockSize] (const juce::AudioBuffer<float>& input, float width)
    {
        Rig rig (sampleRate, blockSize, input.getNumChannels());
        rig.setAll (3000.0f, 70.0f, 45.0f, width, 0.0f);
        rig.settle();
        return renderLatencyAligned (rig, input);
    };

    juce::AudioBuffer<float> stereoInput (2, 16384);
    fillNoise (stereoInput, 0.3, 8800);

    const auto width0   = renderAtWidth (stereoInput, 0.0f);
    const auto width100 = renderAtWidth (stereoInput, 100.0f);
    const auto width200 = renderAtWidth (stereoInput, 200.0f);

    double worstMidError = 0.0;
    double worstSideLinearityError = 0.0;
    double sideMovement = 0.0;

    for (int i = 0; i < stereoInput.getNumSamples(); ++i)
    {
        const auto mid0 = 0.5 * (width0.getSample (0, i) + width0.getSample (1, i));
        const auto mid1 = 0.5 * (width100.getSample (0, i) + width100.getSample (1, i));
        const auto mid2 = 0.5 * (width200.getSample (0, i) + width200.getSample (1, i));
        const auto side0 = 0.5 * (width0.getSample (0, i) - width0.getSample (1, i));
        const auto side1 = 0.5 * (width100.getSample (0, i) - width100.getSample (1, i));
        const auto side2 = 0.5 * (width200.getSample (0, i) - width200.getSample (1, i));

        worstMidError = juce::jmax (worstMidError,
                                    juce::jmax (std::abs (mid1 - mid0), std::abs (mid2 - mid0)));
        worstSideLinearityError = juce::jmax (worstSideLinearityError,
                                              std::abs ((side2 - side1) - (side1 - side0)));
        sideMovement = juce::jmax (sideMovement, std::abs (side2 - side0));
    }

    report (worstMidError < 1.0e-6 && worstSideLinearityError < 1.0e-6 && sideMovement > 1.0e-4,
            "WIDTH 0/100/200 %가 added Side만 선형 조절하고 Mid를 보존",
            "mid error = " + juce::String (worstMidError, 10)
                + " / side linearity error = " + juce::String (worstSideLinearityError, 10));

    juce::AudioBuffer<float> monoInput (1, 16384);
    fillNoise (monoInput, 0.3, 8810);
    const auto mono0   = renderAtWidth (monoInput, 0.0f);
    const auto mono200 = renderAtWidth (monoInput, 200.0f);

    double worstMonoError = 0.0;
    for (int i = 0; i < monoInput.getNumSamples(); ++i)
        worstMonoError = juce::jmax (worstMonoError,
                                     (double) std::abs (mono200.getSample (0, i) - mono0.getSample (0, i)));

    report (worstMonoError < 1.0e-6,
            "모노 입력은 WIDTH 설정과 무관",
            "worst error = " + juce::String (worstMonoError, 10));
}

//==============================================================================
/*  호스트가 재생 중에 일으키는 전환과 손상된 상태에 대한 방어.

    정상상태 검사는 "bypass가 켜져 있는 동안"만 본다. 실제 호스트는 재생 중에 bypass를
    토글하고 자동화하므로, 전환 순간의 불연속(클릭)은 따로 재야 한다
    (성능 가이드 4.4, 릴리즈 QA 가이드의 click-free 전환). */
void testHostTransitions()
{
    section ("10. 호스트 전환과 상태 방어");

    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 64;
        constexpr int period = 512;            // 93.75 Hz
        constexpr int settleSamples = 720;     // 전환 뒤 15 ms 안에는 정상상태여야 한다.

        Rig rig (sampleRate, blockSize, 2);
        rig.setAll (3000.0f, 100.0f, 45.0f, 200.0f, -12.0f);
        rig.settle();

        juce::AudioBuffer<float> input (2, period * 24);
        fillSine (input, sampleRate / period, 0.5, sampleRate);
        juce::AudioBuffer<float> output (input);

        const auto latency = rig.latency();

        /*  출력 i는 입력 i - latency에 대응한다. 전환을 출력 파형의 정점에 맞춰
            LEVEL -12 dB의 하드 스위치가 만드는 불연속이 최대가 되게 한다. */
        const auto firstToggle = ((period / 4 + latency + blockSize / 2) / blockSize) * blockSize
                               + period * 2;
        const int toggles[] { firstToggle, firstToggle + period * 4,
                              firstToggle + period * 8, firstToggle + period * 12 };

        int position = 0;
        bool bypassed = false;

        auto renderUntil = [&] (int end)
        {
            juce::AudioBuffer<float> view (output.getArrayOfWritePointers(), 2,
                                           position, end - position);
            if (bypassed)
                rig.renderBypassed (view);
            else
                rig.render (view);

            position = end;
        };

        for (auto toggle : toggles)
        {
            renderUntil (toggle);
            bypassed = ! bypassed;
        }

        renderUntil (output.getNumSamples());

        double worstStep = 0.0;
        double hardSwitchStep = 0.0;
        const auto levelGain = (double) juce::Decibels::decibelsToGain (-12.0f);

        for (int channel = 0; channel < 2; ++channel)
        {
            const auto* out = output.getReadPointer (channel);

            for (int i = latency + 1; i < output.getNumSamples(); ++i)
                worstStep = juce::jmax (worstStep, (double) std::abs (out[i] - out[i - 1]));

            for (auto toggle : toggles)
                hardSwitchStep = juce::jmax (
                    hardSwitchStep,
                    std::abs ((double) input.getSample (channel, toggle - latency))
                        * (1.0 - levelGain));
        }

        report (worstStep < 0.01 && hardSwitchStep > 0.3,
                "재생 중 bypass 토글이 클릭 없이 전환됨 (LEVEL -12 dB, BRIGHT 100)",
                "worst step = " + juce::String (worstStep, 5)
                    + " / 하드 스위치였다면 = " + juce::String (hardSwitchStep, 3));

        // bypass 구간은 전환이 끝난 뒤 지연 dry와 샘플 단위로 같아야 한다.
        double bypassError = 0.0;

        for (int segment = 0; segment < 4; segment += 2)
            for (int channel = 0; channel < 2; ++channel)
                for (int i = toggles[segment] + settleSamples; i < toggles[segment + 1]; ++i)
                    bypassError = juce::jmax (
                        bypassError,
                        (double) std::abs (output.getSample (channel, i)
                                           - input.getSample (channel, i - latency)));

        report (bypassError < 1.0e-6,
                "bypass 전환은 15 ms 안에 끝나고 이후 지연 dry와 샘플 단위로 일치",
                "worst error = " + juce::String (bypassError, 10));
    }

    {
        // bypass가 켜진 채로 세션을 열면 첫 블록부터 페이드 없이 dry여야 한다.
        Rig rig (48000.0, 512, 2);
        rig.setAll (6000.0f, 100.0f, 100.0f, 200.0f, 12.0f);

        juce::AudioBuffer<float> input (2, 8192);
        fillNoise (input, 0.5, 4242);
        juce::AudioBuffer<float> output (input);
        rig.renderBypassed (output);

        double worstError = 0.0;

        for (int channel = 0; channel < 2; ++channel)
            for (int i = rig.latency(); i < input.getNumSamples(); ++i)
                worstError = juce::jmax (
                    worstError,
                    (double) std::abs (output.getSample (channel, i)
                                       - input.getSample (channel, i - rig.latency())));

        report (worstError < 1.0e-6,
                "bypass 상태로 시작한 인스턴스는 첫 블록부터 지연 dry와 일치",
                "worst error = " + juce::String (worstError, 10));
    }

    {
        /*  손상된 상태. 값이 NaN/Inf인 세션을 받아도 현재 상태를 보존하고 출력이
            유한해야 한다 (호스트 통합 가이드 4.4, 5.1). */
        bool statesRejected = true;
        bool outputsFinite = true;

        for (const auto* poison : { "nan", "inf", "-inf" })
        {
            Rig rig (48000.0, 512, 2);
            rig.setAll (1234.0f, 71.5f, 88.0f, 125.0f, -4.5f);

            juce::MemoryBlock saved;
            rig.processor.getStateInformation (saved);
            auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
            bool poisoned = false;

            if (xml != nullptr)
                if (auto* parameters = xml->getChildByName ("PARAMETERS"))
                    for (auto* parameter : parameters->getChildIterator())
                        if (parameter->getStringAttribute ("id") == tbx::params::tuneId)
                        {
                            parameter->setAttribute ("value", poison);
                            poisoned = true;
                        }

            juce::MemoryBlock poisonedBlock;

            if (xml != nullptr)
                juce::AudioProcessor::copyXmlToBinary (*xml, poisonedBlock);

            rig.processor.setStateInformation (poisonedBlock.getData(), (int) poisonedBlock.getSize());

            const auto tune = rig.processor.getValueTreeState()
                                  .getRawParameterValue (tbx::params::tuneId)->load();
            statesRejected = statesRejected && poisoned && std::abs (tune - 1234.0f) < 0.5f;

            juce::AudioBuffer<float> buffer (2, 4800);
            fillNoise (buffer, 0.5, 6001);
            rig.render (buffer);
            outputsFinite = outputsFinite && isFiniteAndBounded (buffer, 8.0f);
        }

        report (statesRejected, "NaN/Inf 파라미터 값을 가진 상태를 거부하고 현재 상태를 보존");
        report (outputsFinite, "손상된 상태를 받은 뒤에도 출력이 유한·유계");
    }
}

//==============================================================================
/*  조밀한 여기 신호에서 배음층이 얼마나 "매끄러운가".

    정상 톤 IMD 검사는 정규화 엔벌로프가 상수라 생성기의 상한(니) 구간에 닿지 않는다.
    실제 음악처럼 크레스트가 있는 조밀한 신호는 피크마다 그 구간을 지나고, 거기의 곡선이
    거칠면 2·3차 배음 대역 주변으로 스펙트럼이 번진다(regrowth). 이것이 배음이
    "예쁜 공기"가 아니라 "지글거림"으로 들리는 경로다.

    방법: 3.0~3.6 kHz에만 성분이 있는 랜덤 위상 멀티톤을 넣는다. 깨끗한 Re(z²)는
    6.0~7.2 kHz, Re(z³)는 9.0~10.8 kHz에만 떨어진다. 그 사이와 위의 빈 구간
    (7.6~8.6 kHz, 11.4~17.5 kHz)에 생긴 에너지는 전부 번짐이다. */
void testDenseSmoothness()
{
    section ("11. 조밀한 여기 신호의 배음 번짐 (매끄러움)");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    juce::AudioBuffer<float> input (1, fftSize * 4);
    input.clear();
    {
        const auto firstBin = juce::roundToInt (3000.0 * fftSize / sampleRate);
        const auto lastBin  = juce::roundToInt (3600.0 * fftSize / sampleRate);
        constexpr int binStep = 12;
        const auto toneCount = (lastBin - firstBin) / binStep + 1;
        const auto amplitude = 0.12 * std::sqrt (2.0 / toneCount);   // 합계 RMS 0.12
        juce::Random random (20261110);
        auto* data = input.getWritePointer (0);

        for (int bin = firstBin; bin <= lastBin; bin += binStep)
        {
            const auto phase = random.nextDouble() * juce::MathConstants<double>::twoPi;
            const auto increment = juce::MathConstants<double>::twoPi * bin / fftSize;

            for (int i = 0; i < input.getNumSamples(); ++i)
                data[i] += (float) (amplitude * std::sin (increment * i + phase));
        }
    }

    double worstDb = -1000.0;

    for (float character : { 0.0f, 100.0f })
    {
        auto delta = renderEffectDelta (sampleRate, blockSize, 1, input, 3000.0f,
                                        character, 0.0f, 100.0f);
        const auto magnitudes = magnitudeSpectrum (
            delta.getReadPointer (0) + delta.getNumSamples() - fftSize, fftSize);

        auto bandEnergy = [&] (double lowHz, double highHz)
        {
            const auto lowBin  = (int) (lowHz * fftSize / sampleRate);
            const auto highBin = (int) (highHz * fftSize / sampleRate);
            double sum = 0.0;

            for (int bin = lowBin; bin <= highBin; ++bin)
                sum += (double) magnitudes[(size_t) bin] * magnitudes[(size_t) bin];

            return sum;
        };

        const auto intended = bandEnergy (6000.0, 7200.0) + bandEnergy (9000.0, 10800.0);
        const auto regrowth = bandEnergy (7600.0, 8600.0) + bandEnergy (11400.0, 17500.0);
        const auto ratioDb = 10.0 * std::log10 (juce::jmax (1.0e-30, regrowth)
                                                / juce::jmax (1.0e-30, intended));
        worstDb = juce::jmax (worstDb, ratioDb);

        std::cout << "        clarity=" << juce::String (character, 0)
                  << "  intended=" << juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, intended)), 2)
                  << " dB  regrowth=" << juce::String (10.0 * std::log10 (juce::jmax (1.0e-30, regrowth)), 2)
                  << " dB  regrowth/intended=" << juce::String (ratioDb, 2) << " dB" << std::endl;
    }

    /*  2026-11-10 실측 -59.1 dB(WARM) / -51.5 dB(CLEAR). 같은 날 매끄러운 니, 선행(true-peak)
        엔벌로프, 정규화 릴리즈 70~250 ms를 이 지표와 IMD·onset·가치 게이트로 함께 비교했고
        어느 것도 현행 조합을 손해 없이 이기지 못했다. 바는 그 실측에 3.5 dB 여유를 둔 값이다. */
    report (worstDb <= -48.0,
            "조밀한 여기 신호의 배음 번짐이 의도된 2·3차 대비 -48 dB 이하",
            "worst = " + juce::String (worstDb, 2) + " dB");
}

//==============================================================================
// --bench : 실제 Processor 경로의 CPU 비용을 잰다 (성능 가이드 13장).
//   wall-clock 측정이라 절대값은 장비에 따라 다르다. 같은 장비의 변경 전후 비교에 쓴다.
//==============================================================================
void runBenchmark()
{
    section ("CPU 벤치마크 (실제 Processor 경로, 단일 스레드)");

    struct BenchCase
    {
        const char* name;
        double sampleRate;
        int blockSize;
        float bright;
        bool automateFocus;
        bool bypassed;
    };

    const BenchCase cases[]
    {
        { "48k/512  기본값",              48000.0, 512,  tbx::params::amountDefault, false, false },
        { "48k/512  BRIGHT 100",          48000.0, 512,  100.0f,                     false, false },
        { "48k/64   기본값",              48000.0, 64,   tbx::params::amountDefault, false, false },
        { "48k/512  FOCUS 자동화(최악)",  48000.0, 512,  100.0f,                     true,  false },
        { "48k/512  bypass",              48000.0, 512,  tbx::params::amountDefault, false, true  },
        { "96k/512  기본값",              96000.0, 512,  tbx::params::amountDefault, false, false },
        { "192k/512 기본값",              192000.0, 512, tbx::params::amountDefault, false, false },
    };

    /*  짧게 여러 번 재고 최솟값을 쓴다. 케이스 사이에는 쉬어 준다.

        연속 부하로 재면 노트북 CPU의 부스트가 몇 초 만에 꺼져, 뒤에 오는 케이스가
        코드와 무관하게 정확히 2배 느리게 측정됐다(bypass가 일반 처리의 2배로 나왔다). */
    constexpr double seconds = 4.0;
    constexpr int repeats = 7;

    for (const auto& benchCase : cases)
    {
        juce::Thread::sleep (1500);

        Rig rig (benchCase.sampleRate, benchCase.blockSize, 2);
        rig.setAll (tbx::params::tuneDefaultHz, benchCase.bright, tbx::params::characterDefault,
                    tbx::params::widthDefault, tbx::params::outputDefaultDb);
        rig.settle();

        juce::AudioBuffer<float> source (2, (int) benchCase.sampleRate);
        fillNoise (source, 0.25, 9001);
        juce::AudioBuffer<float> work (2, benchCase.blockSize);
        juce::MidiBuffer midi;

        const auto totalBlocks = (int) (seconds * benchCase.sampleRate / benchCase.blockSize);
        double bestMs = 1.0e30;
        double checksum = 0.0;

        for (int repeat = 0; repeat < repeats; ++repeat)
        {
            int sourcePosition = 0;
            const auto start = juce::Time::getMillisecondCounterHiRes();

            for (int block = 0; block < totalBlocks; ++block)
            {
                if (sourcePosition + benchCase.blockSize > source.getNumSamples())
                    sourcePosition = 0;

                for (int channel = 0; channel < 2; ++channel)
                    work.copyFrom (channel, 0, source, channel, sourcePosition, benchCase.blockSize);

                sourcePosition += benchCase.blockSize;

                if (benchCase.automateFocus)
                    rig.set (tbx::params::tuneId, (block & 1) == 0 ? 1000.0f : 5000.0f);

                if (benchCase.bypassed)
                    rig.processor.processBlockBypassed (work, midi);
                else
                    rig.processor.processBlock (work, midi);

                checksum += (double) work.getSample (0, 0);
            }

            bestMs = juce::jmin (bestMs, juce::Time::getMillisecondCounterHiRes() - start);
            juce::Thread::sleep (150);
        }

        const auto load = bestMs / (seconds * 1000.0);

        std::cout << "  " << benchCase.name
                  << "   " << juce::String (load * 100.0, 3) << " % of one core"
                  << "   (" << juce::String (1.0 / juce::jmax (1.0e-9, load), 0) << "x realtime)"
                  << "   checksum " << juce::String (checksum, 6) << std::endl;
    }
}

//==============================================================================
// --diagnose : 출력 스펙트럼에 실제로 무엇이 들어 있는지 그대로 본다.
//   이론이 아니라 측정으로 지글거림의 출처를 특정하기 위한 모드다.
//==============================================================================

/*  엔진의 힐베르트 네트워크와 동일한 구조를 여기서 재현해 품질을 직접 검증한다.
    두 체인의 출력이 정말 90° 차이인지 확인하지 않고 넘어갔던 것을 되짚는다. */
void diagnoseHilbertNetwork()
{
    section ("A. 힐베르트 네트워크 품질 (엔진과 동일 계수)");

    struct Section
    {
        double c = 0.0, x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
        int form = 0;

        double process (double x)
        {
            const auto y = form == 0 ? c * (x + y2) - x2
                                     : c * (x - y2) + x2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    static constexpr double coefficientsA[4]
        { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
    static constexpr double coefficientsB[4]
        { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };

    // 엔진은 4x 오버샘플링 안에서 동작하므로 192 kHz 기준으로 확인한다.
    constexpr double rate = 192000.0;

    /*  올패스 부호 규약과 지연 위치를 추측하지 않고 네 조합을 모두 측정한다.
          form 0 : y = c*(x + y2) - x2   ->  A(z) = (c - z^-2)/(1 - c·z^-2), 극이 ±sqrt(c)
          form 1 : y = c*(x - y2) + x2   ->  A(z) = (c + z^-2)/(1 + c·z^-2), 극이 ±j·sqrt(c)
        힐베르트 쌍 계수는 극이 허수축에 놓이는 form 1을 전제로 설계된 값이다. */
    auto measureRipple = [&] (int form, bool delayOnB, bool squareCoefficients, double frequency)
    {
        Section chainA[4], chainB[4];

        for (int i = 0; i < 4; ++i)
        {
            chainA[i].c = squareCoefficients ? coefficientsA[i] * coefficientsA[i] : coefficientsA[i];
            chainB[i].c = squareCoefficients ? coefficientsB[i] * coefficientsB[i] : coefficientsB[i];
            chainA[i].form = form;
            chainB[i].form = form;
        }

        const auto omega = juce::MathConstants<double>::twoPi * frequency / rate;
        double previousInput = 0.0;
        double minMagnitude = 1.0e30, maxMagnitude = 0.0;

        for (int n = 0; n < 60000; ++n)
        {
            const auto x = std::sin (omega * n);

            auto a = delayOnB ? x : previousInput;
            for (auto& s : chainA) a = s.process (a);

            auto b = delayOnB ? previousInput : x;
            for (auto& s : chainB) b = s.process (b);

            previousInput = x;

            if (n > 40000)   // 과도 구간 제외
            {
                const auto magnitude = a * a + b * b;
                minMagnitude = juce::jmin (minMagnitude, magnitude);
                maxMagnitude = juce::jmax (maxMagnitude, magnitude);
            }
        }

        return 10.0 * std::log10 (maxMagnitude / juce::jmax (1.0e-30, minMagnitude));
    };

    const double testFrequencies[] { 1000.0, 3000.0, 6000.0, 12000.0, 18000.0, 30000.0 };

    int bestForm = 0;
    bool bestDelayOnB = true, bestSquared = false;
    double bestWorstRipple = 1.0e30;

    for (int form = 0; form < 2; ++form)
        for (int delayOnB = 1; delayOnB >= 0; --delayOnB)
            for (int squared = 0; squared < 2; ++squared)
            {
                double worst = 0.0;
                juce::String row;

                for (auto frequency : testFrequencies)
                {
                    const auto ripple = measureRipple (form, delayOnB != 0, squared != 0, frequency);
                    worst = juce::jmax (worst, ripple);
                    row << juce::String (ripple, 3) << "  ";
                }

                std::cout << "        form=" << form
                          << " delay=" << (delayOnB ? "B" : "A")
                          << " coef=" << (squared ? "a^2" : "a  ")
                          << "   1k/3k/6k/12k/18k/30k: " << row
                          << "  최악 " << juce::String (worst, 3) << std::endl;

                if (worst < bestWorstRipple)
                {
                    bestWorstRipple = worst;
                    bestForm = form;
                    bestDelayOnB = delayOnB != 0;
                    bestSquared = squared != 0;
                }
            }

    std::cout << "        => 최선: form=" << bestForm
              << ", delay=" << (bestDelayOnB ? "B" : "A")
              << ", coef=" << (bestSquared ? "a^2" : "a")
              << ", 최악 리플 " << juce::String (bestWorstRipple, 4) << " dB" << std::endl;

    // 0.15 dB 리플 ≈ 이미지 억압 -50 dB. 이 설계로 달성 가능한 실용 한계다.
    const auto productionConfiguration = bestForm == 0 && ! bestDelayOnB && bestSquared;
    report (productionConfiguration && bestWorstRipple < 0.15,
            "엔진이 실측 최선 힐베르트 조합을 사용하고 |z|^2 리플 < 0.15 dB",
            "최선 form=" + juce::String (bestForm) + " delay=" + (bestDelayOnB ? "B" : "A")
                + " coef=" + (bestSquared ? "a^2" : "a"));
}

/*  단일 톤을 넣고 전체 effect delta(셸프 + 배음)의 상위 피크를 그대로 출력한다.
    2f, 3f 외에 무엇이 얼마나 있는지 눈으로 확인하기 위한 것이다. */
void diagnoseSpectrum (float character, const juce::String& label)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    const auto tune = 3000.0f;

    int fundamentalBin = 0;
    const auto frequency = binAlignedFrequency (4000.0, sampleRate, fundamentalBin);

    juce::AudioBuffer<float> input (1, fftSize * 4);
    fillSine (input, frequency, 0.3, sampleRate);

    auto wet = renderEffectDelta (sampleRate, blockSize, 1, input, tune, character, 50.0f);
    const auto offset = wet.getNumSamples() - fftSize;
    const auto magnitudes = magnitudeSpectrum (wet.getReadPointer (0) + offset, fftSize);

    std::cout << "  [" << label << "] 입력 " << juce::String (frequency, 0)
              << " Hz 단일 톤 → effect-delta 스펙트럼 상위 12 피크" << std::endl;

    // 국소 최대만 추린다.
    std::vector<std::pair<double, int>> peaks;

    for (int bin = 2; bin < (int) magnitudes.size() - 2; ++bin)
        if (magnitudes[(size_t) bin] > magnitudes[(size_t) bin - 1]
            && magnitudes[(size_t) bin] >= magnitudes[(size_t) bin + 1])
            peaks.emplace_back ((double) magnitudes[(size_t) bin], bin);

    std::sort (peaks.begin(), peaks.end(), std::greater<>());

    for (size_t i = 0; i < juce::jmin ((size_t) 12, peaks.size()); ++i)
    {
        const auto binFrequency = peaks[i].second * sampleRate / fftSize;
        const auto order = binFrequency / frequency;

        juce::String tag;

        if (std::abs (order - std::round (order)) < 0.02 && std::round (order) >= 2.0)
            tag = "  <- " + juce::String ((int) std::round (order)) + "차 배음";

        std::cout << "        " << juce::String (binFrequency, 0) << " Hz"
                  << "   " << db (peaks[i].first)
                  << "   (x" << juce::String (order, 2) << ")" << tag << std::endl;
    }
}

void diagnoseDenseInput()
{
    section ("C. 조밀한 입력에서의 전체 effect delta");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    // 실제 음악처럼 대역 안에 성분이 많은 입력.
    juce::AudioBuffer<float> input (1, fftSize * 4);
    fillNoise (input, 0.3, 31337);

    for (float character : { 0.0f, 100.0f })
    {
        auto wet = renderEffectDelta (sampleRate, blockSize, 1, input, 3000.0f, character, 50.0f);
        const auto offset = wet.getNumSamples() - fftSize;
        const auto magnitudes = magnitudeSpectrum (wet.getReadPointer (0) + offset, fftSize);

        // 셸프 delta와 배음을 합친 전체 추가 경로의 대역별 에너지 분포다.
        auto bandEnergy = [&] (double lowHz, double highHz)
        {
            const auto lowBin  = (int) (lowHz * fftSize / sampleRate);
            const auto highBin = juce::jmin ((int) magnitudes.size() - 1,
                                             (int) (highHz * fftSize / sampleRate));
            double sum = 0.0;

            for (int bin = lowBin; bin <= highBin; ++bin)
                sum += (double) magnitudes[(size_t) bin] * magnitudes[(size_t) bin];

            return std::sqrt (sum);
        };

        std::cout << "        character=" << juce::String (character, 0)
                  << "   0.1-1k=" << db (bandEnergy (100.0, 1000.0))
                  << "   1-3k=" << db (bandEnergy (1000.0, 3000.0))
                  << "   3-6k=" << db (bandEnergy (3000.0, 6000.0))
                  << "   6-12k=" << db (bandEnergy (6000.0, 12000.0))
                  << "   12-20k=" << db (bandEnergy (12000.0, 20000.0))
                  << std::endl;
    }

    std::cout << "        (셸프 delta가 포함되므로 이 수치를 배음 누설 판정에 사용하지 않는다)"
              << std::endl;
}

/*  제어 신호가 오디오 레이트로 출력을 진폭변조하는지 본다.

    정상상태 톤만으로 하는 IMD 테스트는 엔벌로프 추종기가 상수라 이 결함을 보지 못한다.
    실제 음악은 레벨이 계속 변하므로, 정규화 엔벌로프(1/env)가 오디오 레이트로 흔들리면
    생성된 배음이 잡음으로 번진다. 그것이 "지속적인 지글거림"이다.

    방법: 완전히 정상적인 톤 A와, 강하게 진폭변조된 톤 B를 함께 넣는다.
    A는 흔들리지 않으므로 2×A 배음도 순수한 선이어야 한다. 그 주변에 치맛자락이 생기면
    제어 경로가 출력을 변조하고 있다는 뜻이다. */
void diagnoseControlModulation()
{
    section ("D. 제어 신호에 의한 변조 (동적 소재 결함)");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    int steadyBin = 0;
    const auto steady = binAlignedFrequency (4000.0, sampleRate, steadyBin);
    const auto disturber = 5500.0;
    const auto modulationHz = 7.0;

    juce::AudioBuffer<float> input (1, fftSize * 4);
    {
        auto* data = input.getWritePointer (0);
        const auto wSteady = juce::MathConstants<double>::twoPi * steady / sampleRate;
        const auto wDisturb = juce::MathConstants<double>::twoPi * disturber / sampleRate;
        const auto wMod = juce::MathConstants<double>::twoPi * modulationHz / sampleRate;

        for (int i = 0; i < input.getNumSamples(); ++i)
        {
            const auto envelope = 0.5 * (1.0 - std::cos (wMod * i));   // 0 .. 1
            data[i] = (float) (0.12 * std::sin (wSteady * i)
                             + 0.35 * envelope * std::sin (wDisturb * i));
        }
    }

    double worstRelativeDb = -1000.0;
    double worstDefaultInputDb = -1000.0;
    double worstFullInputDb = -1000.0;

    for (float amount : { 40.0f, 100.0f })
        for (float character : { 0.0f, 100.0f })
        {
            auto delta = renderEffectDelta (sampleRate, blockSize, 1, input, 3000.0f,
                                            character, 100.0f, amount);
            const auto offset = delta.getNumSamples() - fftSize;
            // Hann 창 필수: 방해 톤의 직사각 창 누설이 가짜 치맛자락으로 잡힌다.
            const auto magnitudes = magnitudeSpectrum (delta.getReadPointer (0) + offset,
                                                        fftSize, true);

            const auto centre = steadyBin * 2;
            const auto skirtBins = (int) (120.0 * fftSize / sampleRate);
            double peak = 0.0, skirt = 0.0;

            for (int bin = centre - skirtBins; bin <= centre + skirtBins; ++bin)
            {
                const auto energy = (double) magnitudes[(size_t) bin] * magnitudes[(size_t) bin];

                if (std::abs (bin - centre) <= 3)
                    peak += energy;
                else
                    skirt += energy;
            }

            const auto relativeDb = 10.0 * std::log10 (juce::jmax (
                1.0e-30, skirt / juce::jmax (1.0e-30, peak)));
            // Hann coherent gain은 0.5이므로 입력 대비 진폭에는 2를 곱한다.
            const auto inputDb = 20.0 * std::log10 (juce::jmax (
                1.0e-30, 2.0 * std::sqrt (skirt) / 0.12));

            worstRelativeDb = juce::jmax (worstRelativeDb, relativeDb);
            if (amount == 40.0f)
                worstDefaultInputDb = juce::jmax (worstDefaultInputDb, inputDb);
            else
                worstFullInputDb = juce::jmax (worstFullInputDb, inputDb);

            std::cout << "        bright=" << juce::String (amount, 0)
                      << "  clarity=" << juce::String (character, 0)
                      << "  skirt/main=" << juce::String (relativeDb, 1)
                      << " dB  skirt/input=" << juce::String (inputDb, 1) << " dBc"
                      << std::endl;
        }

    std::cout << "        (정상 톤의 배음은 순수한 선이어야 한다. 치맛자락 = 제어 경로 변조)"
              << std::endl;

    report (worstRelativeDb <= -15.0,
            "변조 치맛자락이 배음 본선 대비 -15 dB 이하",
            "worst skirt/main = " + juce::String (worstRelativeDb, 1) + " dB");
    /*  2026-11-11 보이싱(높은 FOCUS 배음량 2.0 → 2.8배)으로 배음 본선과 치맛자락이 함께
        약 2.9 dB 올라갔다(BRIGHT 100: -40.3 → -37.4 dBc, 기본값: -49.9 → -47.4 dBc).
        본선 대비 비율은 -17.2 dB로 변하지 않았으므로 제어 경로 변조가 나빠진 것이 아니다.
        BRIGHT 100 절대 바만 같은 여유(약 2.4 dB)를 유지하도록 -38 → -35 dBc로 옮긴다. */
    report (worstDefaultInputDb <= -45.0 && worstFullInputDb <= -35.0,
            "변조 치맛자락이 기본값 -45 dBc / BRIGHT 100 -35 dBc 이하",
            "default = " + juce::String (worstDefaultInputDb, 1)
                + " dBc / full = " + juce::String (worstFullInputDb, 1) + " dBc");
}

void runDiagnostics()
{
    diagnoseHilbertNetwork();

    section ("B. 단일 톤 effect-delta 스펙트럼");
    diagnoseSpectrum (100.0f, "CLEAR");
    diagnoseSpectrum (0.0f, "WARM");

    diagnoseDenseInput();
    diagnoseControlModulation();
}

//==============================================================================
// --calibrate : 실제 소재에서 BRIGHT가 만드는 고역 에너지 변화를 측정한다.
//   인서트 기본값(40 %)이 실제로 들릴 만한 변화를 만드는지 판정하는 근거
//   (TB_PLUGIN_DEVELOPMENT_GUIDE.md 6.3).
//==============================================================================
double bandRms (const juce::AudioBuffer<float>& buffer, double sampleRate,
                double lowHz, double highHz)
{
    constexpr auto q = juce::MathConstants<float>::sqrt2 * 0.5f;
    auto highPass = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, (float) lowHz, q);
    auto lowPass  = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, (float) juce::jmin (highHz, sampleRate * 0.45), q);

    double sum = 0.0;
    juce::int64 count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        juce::dsp::IIR::Filter<float> hp1, hp2, lp1, lp2;
        hp1.coefficients = highPass; hp2.coefficients = highPass;
        lp1.coefficients = lowPass;  lp2.coefficients = lowPass;

        const auto* data = buffer.getReadPointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto value = lp2.processSample (lp1.processSample (hp2.processSample (hp1.processSample (data[i]))));

            if (i > (int) (sampleRate * 0.05))   // 필터 과도 구간 제외
            {
                sum += (double) value * value;
                ++count;
            }
        }
    }

    return std::sqrt (sum / juce::jmax ((juce::int64) 1, count));
}

void calibrateAgainstAssets()
{
    section ("BRIGHT 캘리브레이션 (실제 소재의 6–16 kHz 에너지 변화)");

    const juce::File assets ("E:/Plugin Projects/Guide MD/Assets");
    const juce::StringArray sources { "BUS_01_fullmix.wav", "VOX_01_female.wav", "DRM_01_loop.wav" };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (const auto& sourceName : sources)
    {
        const auto file = assets.getChildFile (sourceName);

        if (! file.existsAsFile())
        {
            std::cout << "  (없음) " << sourceName << std::endl;
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
            continue;

        const auto sampleRate  = reader->sampleRate;
        const auto numChannels = (int) juce::jmin ((unsigned int) 2, reader->numChannels);
        const auto numSamples  = (int) reader->lengthInSamples;

        juce::AudioBuffer<float> original (numChannels, numSamples);
        reader->read (&original, 0, numSamples, 0, true, numChannels > 1);

        const auto dryBand = bandRms (original, sampleRate, 6000.0, 16000.0);
        const auto dryExcitation = bandRms (original, sampleRate, 3000.0, 6000.0);
        const auto dryLoudness = integratedLoudness (original, sampleRate);

        std::cout << "  " << file.getFileNameWithoutExtension()
                  << "   dry 3–6k(여기 대역) = " << db (dryExcitation)
                  << "   dry 6–16k = " << db (dryBand)
                  << "   LUFS = " << juce::String (dryLoudness, 2) << std::endl;

        Rig dryRig (sampleRate, 512, numChannels);
        dryRig.setAll (3000.0f, 0.0f, 30.0f, 50.0f, 0.0f);
        dryRig.settle();
        const auto aligned = renderLatencyAligned (dryRig, original);

        for (float amount : { 20.0f, tbx::params::amountDefault, 70.0f, 100.0f })
        {
            Rig rig (sampleRate, 512, numChannels);
            rig.setAll (3000.0f, amount, 30.0f, 50.0f, 0.0f);
            rig.settle();
            auto processed = renderLatencyAligned (rig, original);

            const auto bandAfter = bandRms (processed, sampleRate, 6000.0, 16000.0);
            const auto delta = 20.0 * std::log10 (juce::jmax (1.0e-30, bandAfter / juce::jmax (1.0e-30, dryBand)));
            const auto loudnessDelta = integratedLoudness (processed, sampleRate) - dryLoudness;

            juce::AudioBuffer<float> wetOnly (processed);

            for (int channel = 0; channel < wetOnly.getNumChannels(); ++channel)
                juce::FloatVectorOperations::subtract (wetOnly.getWritePointer (channel),
                                                       aligned.getReadPointer (channel),
                                                       wetOnly.getNumSamples());

            // 와이드 확인: 6–16 kHz의 Side 성분이 실제로 늘어나는지.
            juce::String widthInfo;

            if (numChannels == 2)
            {
                auto sideOf = [&] (const juce::AudioBuffer<float>& source)
                {
                    juce::AudioBuffer<float> side (1, source.getNumSamples());
                    const auto* left  = source.getReadPointer (0);
                    const auto* right = source.getReadPointer (1);
                    auto* out = side.getWritePointer (0);

                    for (int i = 0; i < source.getNumSamples(); ++i)
                        out[i] = (left[i] - right[i]) * 0.5f;

                    return bandRms (side, sampleRate, 6000.0, 16000.0);
                };

                const auto sideDelta = 20.0 * std::log10 (
                    juce::jmax (1.0e-30, sideOf (processed) / juce::jmax (1.0e-30, sideOf (original))));

                widthInfo = "   Side 6–16k Δ = " + juce::String (sideDelta, 2) + " dB";
            }

            const auto wetBand = bandRms (wetOnly, sampleRate, 6000.0, 16000.0);
            const auto wetToDry = 20.0 * std::log10 (juce::jmax (1.0e-30, wetBand / juce::jmax (1.0e-30, dryBand)));
            const auto wetToExcitation = 20.0 * std::log10 (juce::jmax (1.0e-30, wetBand / juce::jmax (1.0e-30, dryExcitation)));

            std::cout << "        amount=" << juce::String (amount, 0)
                      << "   wet 6–16k = " << db (wetBand)
                      << "   wet/여기대역 = " << juce::String (wetToExcitation, 2) << " dB"
                      << "   wet/dry = " << juce::String (wetToDry, 2) << " dB"
                      << "   band Δ = " << juce::String (delta, 2) << " dB"
                      << "   LUFS Δ = " << juce::String (loudnessDelta, 2) << " LU"
                      << "   peak Δ = "
                      << juce::String (juce::Decibels::gainToDecibels (
                             processed.getMagnitude (0, processed.getNumSamples())
                             / juce::jmax (1.0e-9f, original.getMagnitude (0, original.getNumSamples()))), 2)
                      << " dB"
                      << widthInfo
                      << std::endl;
        }

        // FOCUS 스윕: 소재마다 어느 시작 주파수가 맞는지 확인한다(프리셋 설계 근거).
        for (float tune : { 1000.0f, 1500.0f, 2000.0f, 3000.0f, 5000.0f })
        {
            Rig rig (sampleRate, 512, numChannels);
            rig.setAll (tune, 45.0f, 30.0f, 50.0f, 0.0f);
            rig.settle();
            auto processed = renderLatencyAligned (rig, original);

            const auto lowHz = juce::jmax (6000.0, (double) tune * 2.0);
            const auto dryAtTune = bandRms (original, sampleRate, lowHz, 16000.0);
            const auto wetAtTune = bandRms (processed, sampleRate, lowHz, 16000.0);
            const auto delta = 20.0 * std::log10 (juce::jmax (1.0e-30, wetAtTune / juce::jmax (1.0e-30, dryAtTune)));

            std::cout << "        [tune sweep @40%] tune=" << juce::String (tune, 0) << " Hz"
                      << "   " << juce::String (lowHz / 1000.0, 1) << "–16k Δ = "
                      << juce::String (delta, 2) << " dB" << std::endl;
        }
    }
}

//==============================================================================
// --render : Guide MD/Assets 소재로 레벨 매칭 A/B 파일 생성
//==============================================================================
struct RenderCase
{
    juce::String name;
    float tune, amount, character, width;
};

void renderComparisons()
{
    section ("A/B 렌더");

    const juce::File assets ("E:/Plugin Projects/Guide MD/Assets");
    const juce::File outputFolder ("E:/Plugin Projects/TB_Exciter/output/ab");
    outputFolder.createDirectory();

    const juce::StringArray sources { "BUS_01_fullmix.wav", "VOX_01_female.wav", "DRM_01_loop.wav" };

    const RenderCase cases[]
    {
        // 인서트 기본값
        { "default",   tbx::params::tuneDefaultHz, tbx::params::amountDefault,
                       tbx::params::characterDefault, tbx::params::widthDefault },
        // CLARITY 양 끝 — 같은 BRIGHT에서 배음 구조가 어떻게 달라지는지 확인한다.
        { "warm",      3000.0f, 70.0f,   0.0f, 160.0f },
        { "clear",     3000.0f, 70.0f, 100.0f, 160.0f },
        // WIDTH 양 끝 — 더해진 상단의 좌우 폭만 다르다
        { "width_mono", 3000.0f, 70.0f,  45.0f,   0.0f },
        { "width_wide", 3000.0f, 70.0f,  45.0f, 200.0f },
        // 여기 대역이 얇은 소재(보컬 등)용 설정 예시
        { "vocal",     1800.0f, 75.0f,  55.0f, 160.0f },
        // 최대치 — 어디까지 밀 수 있는지 상한 확인용
        { "max_air",   3500.0f, 100.0f, 70.0f, 150.0f },
    };

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (const auto& sourceName : sources)
    {
        const auto file = assets.getChildFile (sourceName);

        if (! file.existsAsFile())
        {
            report (false, "소재 없음: " + sourceName);
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr)
        {
            report (false, "소재를 읽을 수 없음: " + sourceName);
            continue;
        }

        const auto sampleRate = reader->sampleRate;
        const auto numChannels = (int) juce::jmin ((unsigned int) 2, reader->numChannels);
        const auto numSamples = (int) reader->lengthInSamples;

        juce::AudioBuffer<float> original (numChannels, numSamples);
        reader->read (&original, 0, numSamples, 0, true, numChannels > 1);

        const auto referenceLoudness = integratedLoudness (original, sampleRate);
        const auto stem = file.getFileNameWithoutExtension();

        auto write = [&] (const juce::String& suffix, const juce::AudioBuffer<float>& buffer)
        {
            const auto target = outputFolder.getChildFile (stem + "__" + suffix + ".wav");
            target.deleteFile();

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream (target.createOutputStream());

            if (stream == nullptr)
                return;

            const auto options = juce::AudioFormatWriterOptions {}
                                     .withSampleRate (sampleRate)
                                     .withNumChannels (buffer.getNumChannels())
                                     .withBitsPerSample (24);
            auto writer = wav.createWriterFor (stream, options);

            if (writer != nullptr)
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        };

        write ("00_bypass", original);

        /*  대역제한 A/B. 10 kHz에서 자른 소재는 하이셸프가 올릴 것이 없으므로 원리적으로
            아무것도 못 한다. 이 제품이 EQ로 불가능한 일을 하는지 귀로 확인하는 경우다.
            BUS_01에만 적용해 파일 수를 늘리지 않는다. */
        if (sourceName.startsWith ("BUS_01"))
        {
            juce::AudioBuffer<float> limited (original);
            applyCascadedLowPass (limited, sampleRate, 10000.0);
            const auto limitedReference = integratedLoudness (limited, sampleRate);

            write ("lp10k_00_bypass", limited);

            for (float bright : { tbx::params::amountDefault, 100.0f })
            {
                Rig rig (sampleRate, 512, numChannels);
                rig.setAll (tbx::params::tuneDefaultHz, bright, tbx::params::characterDefault,
                            tbx::params::widthDefault, 0.0f);
                rig.settle();
                auto processed = renderLatencyAligned (rig, limited);

                const auto loudness = integratedLoudness (processed, sampleRate);
                processed.applyGain ((float) juce::Decibels::decibelsToGain (limitedReference - loudness));

                write (juce::String ("lp10k_bright") + juce::String (juce::roundToInt (bright)), processed);
            }

            // 셸프 대조군: 같은 소재에 하이셸프만. 컷오프 위에는 아무것도 만들지 못한다.
            {
                juce::AudioBuffer<float> shelved (limited);
                auto delta = makeShelfDelta (limited, sampleRate, tbx::params::tuneDefaultHz * 2.0, 3.0);

                for (int channel = 0; channel < shelved.getNumChannels(); ++channel)
                    juce::FloatVectorOperations::add (shelved.getWritePointer (channel),
                                                      delta.getReadPointer (channel),
                                                      shelved.getNumSamples());

                const auto loudness = integratedLoudness (shelved, sampleRate);
                shelved.applyGain ((float) juce::Decibels::decibelsToGain (limitedReference - loudness));
                write ("lp10k_control_highshelf", shelved);
            }
        }

        for (const auto& renderCase : cases)
        {
            Rig rig (sampleRate, 512, numChannels);
            rig.setAll (renderCase.tune, renderCase.amount, renderCase.character, renderCase.width, 0.0f);
            rig.settle();
            auto processed = renderLatencyAligned (rig, original);

            const auto loudness = integratedLoudness (processed, sampleRate);
            const auto matchGain = juce::Decibels::decibelsToGain (referenceLoudness - loudness);
            processed.applyGain ((float) matchGain);

            write (renderCase.name, processed);

            std::cout << "  " << stem << " / " << renderCase.name
                      << "  LUFS " << juce::String (loudness, 2)
                      << " → matched to " << juce::String (referenceLoudness, 2)
                      << " (gain " << juce::String (juce::Decibels::gainToDecibels (matchGain), 2) << " dB)"
                      << std::endl;
        }

        // 밝기만 다른 대조군: +3 dB 하이셸프 @ 6 kHz, 같은 라우드니스로 매칭
        {
            juce::AudioBuffer<float> shelved (original);
            auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 6000.0f, juce::MathConstants<float>::sqrt2 * 0.5f,
                juce::Decibels::decibelsToGain (3.0f));

            for (int channel = 0; channel < shelved.getNumChannels(); ++channel)
            {
                juce::dsp::IIR::Filter<float> filter;
                filter.coefficients = coefficients;
                auto* data = shelved.getWritePointer (channel);

                for (int i = 0; i < shelved.getNumSamples(); ++i)
                    data[i] = filter.processSample (data[i]);
            }

            const auto loudness = integratedLoudness (shelved, sampleRate);
            shelved.applyGain ((float) juce::Decibels::decibelsToGain (referenceLoudness - loudness));
            write ("control_highshelf", shelved);
        }
    }

    std::cout << std::endl << "  출력 위치: " << outputFolder.getFullPathName() << std::endl;
}

//==============================================================================
// --render-voicing <label> : 지금 빌드된 엔진의 기본 설정을 청취 비교용으로 렌더한다.
//   보이싱 후보를 귀로 고르기 위한 모드다. 후보마다 엔진을 다르게 빌드(또는 설정)해
//   label만 바꿔 실행하면 output/ab_voicing 에 나란히 쌓인다. focusHz를 주면 그 FOCUS로
//   렌더하고 output/ab_voicing_focus<Hz> 폴더를 따로 쓴다. 모든 파일은 원본과
//   같은 통합 라우드니스로 맞춘다.
//==============================================================================
void renderVoicingAudition (const juce::String& label, float focusHz)
{
    section ("보이싱 청취 렌더: " + label + " @ FOCUS " + juce::String (focusHz, 0) + " Hz");

    // 기본 FOCUS가 아니면 폴더를 나눠, FOCUS가 다른 파일끼리 섞여 비교되지 않게 한다.
    const auto isDefaultFocus = juce::approximatelyEqual (focusHz, tbx::params::tuneDefaultHz);
    const juce::File outputFolder (juce::String ("E:/Plugin Projects/TB_Exciter/output/ab_voicing")
                                       + (isDefaultFocus ? juce::String()
                                                         : "_focus" + juce::String (juce::roundToInt (focusHz))));
    outputFolder.createDirectory();

    const juce::StringArray sources { "BUS_01_fullmix.wav", "VOX_01_female.wav", "DRM_01_loop.wav" };

    for (const auto& sourceName : sources)
    {
        juce::AudioBuffer<float> original;
        double sampleRate = 0.0;
        int numChannels = 0;

        if (! loadAsset (sourceName, original, sampleRate, numChannels))
        {
            report (false, "소재 없음: " + sourceName);
            continue;
        }

        const auto stem = juce::File (sourceName).getFileNameWithoutExtension();
        const auto referenceLoudness = integratedLoudness (original, sampleRate);

        auto write = [&] (const juce::String& suffix, const juce::AudioBuffer<float>& buffer)
        {
            const auto target = outputFolder.getChildFile (stem + "__" + suffix + ".wav");
            target.deleteFile();

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream (target.createOutputStream());

            if (stream == nullptr)
                return false;

            const auto options = juce::AudioFormatWriterOptions {}
                                     .withSampleRate (sampleRate)
                                     .withNumChannels (buffer.getNumChannels())
                                     .withBitsPerSample (24);
            auto writer = wav.createWriterFor (stream, options);

            return writer != nullptr
                && writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        };

        bool allWritten = write ("00_bypass", original);

        auto bandDelta = [&] (const juce::AudioBuffer<float>& processed, double lowHz, double highHz)
        {
            return 20.0 * std::log10 (
                juce::jmax (1.0e-30, bandRms (processed, sampleRate, lowHz, highHz))
                    / juce::jmax (1.0e-30, bandRms (original, sampleRate, lowHz, highHz)));
        };

        for (float bright : { tbx::params::amountDefault, 75.0f })
        {
            Rig rig (sampleRate, 512, numChannels);
            rig.setAll (focusHz, bright, tbx::params::characterDefault,
                        tbx::params::widthDefault, 0.0f);
            rig.settle();
            auto processed = renderLatencyAligned (rig, original);

            const auto loudness = integratedLoudness (processed, sampleRate);
            const auto peakDelta = juce::Decibels::gainToDecibels (
                processed.getMagnitude (0, processed.getNumSamples())
                    / juce::jmax (1.0e-9f, original.getMagnitude (0, original.getNumSamples())));

            std::cout << "  " << stem << "  BRIGHT " << juce::String (bright, 0)
                      << "   2-4k " << juce::String (bandDelta (processed, 2000.0, 4000.0), 2)
                      << "   4-8k " << juce::String (bandDelta (processed, 4000.0, 8000.0), 2)
                      << "   8-12k " << juce::String (bandDelta (processed, 8000.0, 12000.0), 2)
                      << "   12-20k " << juce::String (bandDelta (processed, 12000.0, 20000.0), 2)
                      << " dB   LUFS " << juce::String (loudness - referenceLoudness, 2)
                      << " LU   peak " << juce::String (peakDelta, 2) << " dB" << std::endl;

            processed.applyGain ((float) juce::Decibels::decibelsToGain (referenceLoudness - loudness));
            allWritten = write ("bright" + juce::String (juce::roundToInt (bright)) + "__" + label,
                                processed) && allWritten;
        }

        report (allWritten, stem + " 렌더 완료");
    }

    std::cout << std::endl << "  출력 위치: " << outputFolder.getFullPathName() << std::endl;
}

//==============================================================================
// --preset-levels : Factory 프리셋 30개가 실제 소재에서 만드는 변화를 표로 낸다.
//   보이싱 상수를 바꾼 뒤 프리셋의 BRIGHT/LEVEL이 여전히 맞는지 확인하는 근거다.
//   LUFS·peak는 프리셋의 LEVEL 트림까지 적용한 최종 출력 기준이다.
//==============================================================================
void measurePresetLevels()
{
    section ("Factory 프리셋 실소재 측정 (6-16k / LUFS / peak, LEVEL 포함)");

    const juce::StringArray sources { "BUS_01_fullmix.wav", "VOX_01_female.wav", "DRM_01_loop.wav" };

    struct Source
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;
        int numChannels = 0;
        double loudness = 0.0, band = 0.0;
        float peak = 0.0f;
    };

    std::vector<Source> loaded;

    for (const auto& sourceName : sources)
    {
        Source source;

        if (! loadAsset (sourceName, source.audio, source.sampleRate, source.numChannels))
        {
            report (false, "소재 없음: " + sourceName);
            return;
        }

        source.loudness = integratedLoudness (source.audio, source.sampleRate);
        source.band = bandRms (source.audio, source.sampleRate, 6000.0, 16000.0);
        source.peak = source.audio.getMagnitude (0, source.audio.getNumSamples());
        loaded.push_back (std::move (source));
    }

    std::cout << "  id  focus  bright   | BUS 6-16k  LUFS  peak | VOX 6-16k  LUFS  peak | DRM 6-16k  LUFS  peak"
              << std::endl;

    for (const auto& preset : tbx::presets::factory)
    {
        std::cout << "  " << juce::String (preset.id).paddedLeft (' ', 2)
                  << "  " << juce::String (preset.focusHz, 0).paddedLeft (' ', 5)
                  << "  " << juce::String (preset.bright, 0).paddedLeft (' ', 4) << "    ";

        for (const auto& source : loaded)
        {
            Rig rig (source.sampleRate, 512, source.numChannels);
            rig.setAll (preset.focusHz, preset.bright, preset.clarity, preset.width, preset.levelDb);
            rig.settle();
            const auto processed = renderLatencyAligned (rig, source.audio);

            const auto bandDb = 20.0 * std::log10 (
                juce::jmax (1.0e-30, bandRms (processed, source.sampleRate, 6000.0, 16000.0))
                    / juce::jmax (1.0e-30, source.band));
            const auto loudnessDelta = integratedLoudness (processed, source.sampleRate) - source.loudness;
            const auto peakDelta = juce::Decibels::gainToDecibels (
                processed.getMagnitude (0, processed.getNumSamples()) / juce::jmax (1.0e-9f, source.peak));

            std::cout << " | " << juce::String (bandDb, 2).paddedLeft (' ', 9)
                      << " " << juce::String (loudnessDelta, 2).paddedLeft (' ', 5)
                      << " " << juce::String (peakDelta, 2).paddedLeft (' ', 5);
        }

        std::cout << "   " << preset.name << std::endl;
    }
}

void renderEditorPreview()
{
    section ("UI preview render");

    TB_ExciterAudioProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::stereo());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    processor.setBusesLayout (layout);
    processor.setRateAndBufferSizeDetails (48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // Sparkle Field는 화면에 보이는 동안에만 움직인다. 실제 창으로 올려 애니메이션을 진행시킨다.
    editor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    editor->setVisible (true);
    editor->resized();

   #if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
   #endif

    auto image = editor->createComponentSnapshot (editor->getLocalBounds());

    const juce::File folder ("E:/Plugin Projects/TB_Exciter/output/ui");
    folder.createDirectory();
    const auto target = folder.getChildFile ("TB_Exciter.png");
    target.deleteFile();

    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream (target.createOutputStream());
    const auto written = stream != nullptr && png.writeImageToStream (image, *stream);
    const auto backgroundPixel = image.getPixelAt (10, 10);
    const auto hasPaintedContent = backgroundPixel.getRed() != 0
                                   || backgroundPixel.getGreen() != 0
                                   || backgroundPixel.getBlue() != 0;

    report (written && hasPaintedContent, "HTML-authoritative JUCE editor preview generated",
            target.getFullPathName() + "  " + juce::String (editor->getWidth())
                + "x" + juce::String (editor->getHeight()));
}

} // namespace

//==============================================================================
int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "TB_Exciter DSP Harness" << std::endl;

    const juce::String mode = argc > 1 ? juce::String (argv[1]) : juce::String();

    if (mode == "--render")
    {
        renderComparisons();
    }
    else if (mode == "--render-ui")
    {
        renderEditorPreview();
    }
    else if (mode == "--calibrate")
    {
        calibrateAgainstAssets();
    }
    else if (mode == "--diagnose")
    {
        runDiagnostics();
    }
    else if (mode == "--bench")
    {
        runBenchmark();
    }
    else if (mode == "--preset-levels")
    {
        measurePresetLevels();
    }
    else if (mode == "--render-voicing")
    {
        const auto focusHz = argc > 3 ? juce::String (argv[3]).getFloatValue() : tbx::params::tuneDefaultHz;

        renderVoicingAudition (argc > 2 ? juce::String (argv[2]) : juce::String ("current"),
                               juce::jlimit (tbx::params::tuneMinHz, tbx::params::tuneMaxHz, focusHz));
    }
    else
    {
        testStateAndSafety();
        testDryTransparency();
        testTransparentHarmonicCeiling();
        testAliasing();
        testTransientContainment();
        testIntermodulation();
        testProductValue();
        testAmountMonotonicity();
        testResonanceSafeShelf();
        testWidthBehaviour();
        testHostTransitions();
        testDenseSmoothness();
    }

    std::cout << std::endl
              << "검사 " << checkCount << "개 중 실패 " << failureCount << "개" << std::endl;

    return failureCount == 0 ? 0 : 1;
}
