# TB_MultiBandCompressor Processing Research

확인일: 2026-08-12  
조사할 문제: 전체 스펙트럼을 항상 분할하지 않으면서 필요한 주파수 범위만 독립적으로 압축·확장하고, 밴드별 external sidechain·Stereo/Mid/Side, zero-latency Dynamic과 mastering용 Linear, 2×/4×/8× oversampling 및 0–20 ms lookahead를 하나의 실시간 구조로 제공하는 방법.

## 공식 참고 자료

- [FabFilter Pro-MB 제품 페이지](https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in) — 자유 배치 밴드, Dynamic/Linear/Minimum Phase, per-band 기능의 공개 제품 계약.
- [FabFilter Pro-MB Display and workflow](https://www.fabfilter.com/help/pro-mb/using/display) — 필요한 구간만 밴드를 만들고 나머지를 처리하지 않는 UX.
- [FabFilter Pro-MB Basic band controls](https://www.fabfilter.com/help/pro-mb/using/basicbandcontrols) — Threshold, signed Range, Ratio, Knee, timing과 네 dynamics 조합.
- [FabFilter Pro-MB Expert band controls](https://www.fabfilter.com/help/pro-mb/using/expertbandcontrols) — band별 In/Ext, Band/Free, Audition, Stereo Link.
- [FabFilter Pro-MB Processing mode](https://www.fabfilter.com/help/pro-mb/using/processingmode) — 공개된 Dynamic/Linear/Minimum Phase의 동작 차이. 비공개 계수나 구현은 공개하지 않는다.
- [FabFilter Pro-MB Oversampling](https://www.fabfilter.com/help/pro-mb/using/oversampling) — 공개된 2×/4× 품질 선택과 CPU trade-off. TB_MultiBandCompressor의 8×는 독자적으로 추가한 JUCE 경로이며 Pro-MB parity 주장이 아니다.
- [FabFilter Pro-MB External side chaining](https://www.fabfilter.com/help/pro-mb/support/externalsidechaining) — 호스트가 하나의 external key input을 연결하는 방식.
- [JUCE LinkwitzRileyFilter](https://docs.juce.com/master/classjuce_1_1dsp_1_1LinkwitzRileyFilter.html) — LR4 full-band separation과 flat magnitude recombination.
- [JUCE StateVariableTPTFilter](https://docs.juce.com/master/classjuce_1_1dsp_1_1StateVariableTPTFilter.html) — 빠른 modulation을 위한 12 dB/oct TPT low/band/high-pass.
- [W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/) — bandwidth와 Q 관계 및 biquad 배경.
- 로컬 JUCE 8.0.12 모듈은 AGPLv3와 상용 JUCE 라이선스의 이중 라이선스다. 이 프로젝트는 설치본 `C:/JUCE/LICENSE.md` 및 해당 JUCE 8 라이선스 조건을 따라 공개 API를 사용한다.

상용 레퍼런스는 청감·기능·UX 비교에만 사용한다. `Dynamic Phase`는 FabFilter의 고유 명칭이며 내부 알고리즘을 추정해 동일 구현이라고 표시하지 않는다.

## Pro-MB와 일반 멀티밴드의 핵심 차이

| 일반적 구조 | Pro-MB 공개 동작에서 중요한 점 |
| --- | --- |
| 전체 대역을 연속 crossover band로 분할 | 필요한 주파수 구간에만 band를 배치하고 gap은 untouched |
| Minimum Phase 또는 Linear Phase 한 방식 | 공개 문서상 zero-latency Dynamic Phase와 Linear/Minimum Phase를 선택 |
| ratio 중심의 downward compression | signed Range + Compress/Expand로 네 방향의 dynamics |
| 처리 band가 detector band | 공개 문서상 band별 INT/EXT와 BAND/FREE detector range, Audition; 이 제품은 기존 raw host value/state round-trip은 유지하되 DSP는 effective FOLLOW이고 custom UI는 고정 `SIDECHAIN` toggle과 Delta Listen만 노출 |
| 전역 sidechain switch | host에 보이는 네 개의 optional mono aux input(`Sidechain 1–4`)을 Band 1–4에 고정 연결하고 band별로 독립 선택 |

## 후보 A — 전 대역 Linkwitz-Riley 분할

- 원리: 3개의 LR4 crossover로 네 연속 밴드를 만들고 각 band compressor 뒤 재합성.
- 장점: 전통적 multiband 의미가 명확하고 대역 분리도가 좋다. JUCE 공식 클래스가 low+high flat magnitude 합을 제공한다.
- 위험: 모든 신호가 필터를 통과한다. cascade의 서로 다른 all-pass 경로, crossover 자동화와 재합성 phase를 별도 검증해야 한다. 자유로운 isolated band/gap UX와 맞지 않는다.
- 위상/레이턴시: IIR라 알고리즘 latency는 0이지만 crossover phase가 항상 존재한다.
- CPU/메모리: 낮음. 4 band × main/sidechain filter tree.
- 라이선스: JUCE 8 이중 라이선스(AGPLv3 또는 상용 JUCE 라이선스).
- 판정: 보류. 이후 전통적 crossover mode 후보.

## 후보 B — 자유 배치 TPT band-pass delta (Dynamic 선택)

- 원리: 각 자유 band에서 TPT band-pass `B_b(x)`를 만들고 detector가 계산한 gain `g_b`로 `y = x + Σ((g_b-1)B_b(x))`를 출력한다.
- 장점: band gain 1일 때 delta가 sample-exact zero이므로 나머지 spectrum과 static phase가 untouched다. band가 겹치거나 gap이 있어도 topology를 다시 만들지 않는다. center/width modulation에 맞는 TPT 필터를 사용할 수 있다.
- 위험: gain이 변할 때는 IIR band의 phase가 포함되며, 겹친 band의 delta가 합쳐져 의도보다 강해질 수 있다. 12 dB/oct band-pass는 전통적 steep crossover와 다르다. 강한 positive Range는 headroom 검증이 필요하다.
- 위상/레이턴시: neutral에서 phase change와 latency 0. active delta 구간에는 국소 phase 변화. pre-ringing 없음.
- CPU/메모리: 4 processing filters + INT/EXT detector filters, stereo sample processing. 고정 메모리, callback allocation 없음.
- 라이선스: JUCE 8 이중 라이선스(AGPLv3 또는 상용 JUCE 라이선스); delta 조합 코드는 이 프로젝트에서 작성.
- 판정: `Dynamic` 경로로 선택. 이 명칭은 제품의 일반 모드명이며 FabFilter의 proprietary `Dynamic Phase`와 동등성을 뜻하지 않는다.

## 후보 C — 대칭 FIR band-pass delta (Linear 선택)

- 원리: 중심 주파수와 폭에서 low/high cutoff를 계산하고, Blackman-windowed sinc 차로 홀수 길이 대칭 band-pass kernel을 만든다. 512-sample partitioned convolution으로 네 자유 band를 처리하고 `dry + Σ((g-1)B(x))`로 재합성한다.
- 장점: kernel이 대칭이므로 band delta의 위상은 linear하고, dry를 동일 group/partition delay로 정렬하면 neutral에서 delayed dry로 정확히 재구성할 수 있다. mastering에서 위상 일관성을 우선하는 명시적 선택지를 제공한다.
- 위험: pre-ringing 가능성, 고정 latency, Dynamic보다 높은 CPU와 메모리, center/width 변경 시 kernel 전환 아티팩트가 있다. `Linear`라는 이름은 이러한 trade-off를 숨기지 않는다.
- 완화: half impulse 20 ms, kernel 재설계 cadence 20 ms, target kernel 전환 50 ms, 고정 크기 FFT/partition/storage를 `prepare()`에서 준비한다.
- 위상/레이턴시: 대칭 FIR group delay + partition delay. oversampling과 lookahead를 더한 전체 base-rate latency를 host에 보고한다.
- 라이선스: windowed-sinc, FFT와 partitioned convolution 코드는 프로젝트 자체 구현; JUCE 기본 타입만 사용.
- 판정: `Linear` 경로로 선택. Pro-MB Linear mode와 feature parity 또는 동일 알고리즘을 주장하지 않는다.

## 후보 D — 고정 complementary all-pass 4-band tree

- 원리: `low=(x+A(x))/2`, `high=(x-A(x))/2` residual split을 tree로 연결해 sample-exact reconstruction 후 band gain을 적용.
- 장점: 모든 band unity에서 정확 복원되고 전 band 동일 gain도 정확하다. 로컬 `TB_SubLow`에 검증된 구조가 있다.
- 위험: 고정 crossover 중심 구조라 “think bands, not crossovers” UX를 충족하지 못한다. 1차 slope는 bleed가 크다.
- 위상/레이턴시: 합산 neutral exact, latency 0. band별 gain 차이 때 all-pass phase 영향.
- CPU/메모리: 매우 낮음.
- 라이선스: 저장소 자체 코드.
- 판정: 자유 band 구조가 청감적으로 실패할 때의 fallback.

## 선택과 현재 구현 계약

기본 경로는 후보 B, mastering용 명시적 Linear 선택은 후보 C를 사용한다. 두 경로 모두 동일한 자유 band, detector, signed Range와 dry+delta 재합성 계약을 공유한다. 후보 D와 LR crossover는 현 revision에서 제외한다.

- 4 fixed parameter slots, 자유 center/width, enable/delete에 해당하는 On/Off.
- Compress는 threshold 위, Expand는 threshold 아래를 활성 영역으로 고르고 signed Range의 부호가 실제 gain 부호를 직접 결정하는 네 조합.
- Host Range domain은 `-48…+12 dB`이며 default는 `-3 dB`다. 음수 endpoint는 mastering 감쇄 여유, 양수 endpoint는 upward headroom을 제한하는 현재 공개 범위다.
- Factory fixture는 이 의미를 직접 드러낸다. ID 12 `Transient Lift`와 ID 20 `Parallel Drum Punch`의 positive bands는 Compress로 threshold 위 transient를 올리고, ID 19 `Keys Space`의 positive bands는 Expand로 threshold 아래 detail을 올린다.
- host에 `Sidechain 1`–`Sidechain 4` optional mono-default aux bus 네 개를 선언한다. 각 bus는 같은 번호의 Band 1–4에 1:1 공급하고, stereo 연결은 DSP에서 `(L+R)/2`로 하나의 mono detector key로 downmix한다. 선택 bus가 없으면 해당 band만 internal로 fallback한다. 연결된 silence는 silence로 유지한다. 기존 단일 bus 세션 라우팅은 보장하지 않으며 업데이트 후 DAW Send/Aux 재연결이 필요할 수 있다. 각 선택 bus는 detector 양쪽 입력에 동일한 mono key로 복제한다.
- 기존 `BAND/FREE` detector mode·독립 center/width는 같은 ID·choice·raw value/state round-trip을 보존하지만 Processor가 effective mode를 BAND/FOLLOW로 강제한다. Raw FREE/range는 inert하며 이전 FREE 음향 결과는 보존하지 않는다. 기존 detector Audition control/function은 남지만 current effective FOLLOW detector를 듣는다. Custom editor에서는 이 네 control을 숨긴다.
- Custom editor의 `DELTA` button은 선택 band를 latch하고, enabled band node의 right-button hold는 그 latch를 momentary override한다. Hold-drag는 처음 누른 band의 Frequency만 움직이고 Range와 Delta 대상 band를 유지하며, 정상 release는 pre-hold latch를 복원한다. Delta 선택은 APVTS parameter, state/preset, automation 또는 Undo 항목이 아니고 Frequency edit만 host gesture/Undo transaction 하나를 만든다.
- band별 Stereo/Mid/Side processing/detector domain.
- Dynamic/Linear, Oversampling Off/2×/4×/8×, global Lookahead 0–20 ms.
- `Dynamic + Off + 0 ms`만 0 sample latency이며 나머지는 실제 지연을 합산 보고한다.

## Sidechain 결정

VST3/JUCE의 고정 5-bus 구성에서 optional mono-default aux input 네 개를 선언하며 이름은 `Sidechain 1`–`Sidechain 4`다. DAW는 각 버스를 별도의 Send/Aux 선택지로 표시하고, Band N은 같은 번호의 외부 key를 선택한다. 각 bus는 stereo 연결도 허용하며 내부에서 `(L+R)/2`로 downmix한다. 기존 단일 bus 세션의 라우팅은 보장하지 않으므로 업데이트 후 Send/Aux를 다시 연결해야 할 수 있다.

`MultibandEngine`의 direct single-buffer overload는 내부 legacy DSP tests 전용이며, DAW sidechain routing surface가 아니다.

- Source는 band별 automatable `INT/EXT` choice parameter다. Custom editor의 compact button 글자는 두 상태 모두 `SIDECHAIN`이고 toggle fill/outline과 접근성 상태로 Off=INT, On=EXT를 구분한다.
- `BAND/FREE`와 독립 detector center/width는 기존 host/state raw contract다. Custom editor에는 해당 control이 없고 Processor audio는 raw FREE 값과 무관하게 processing center/width를 사용한다. Generic host의 값과 이전 state는 정확히 round-trip하지만 이전 FREE 음향 결과는 의도적으로 보존하지 않는다.
- 선택한 `Sidechain N` bus가 없을 때만 해당 band의 DSP는 internal로 fallback하고 UI는 `NO SC INPUT`을 표시한다. 다른 Sidechain bus의 연결 여부는 그 band의 source를 바꾸지 않는다.
- 외부 bus가 연결됐지만 silence이면 silence를 존중한다. 에너지로 연결 여부를 추측하지 않는다.
- EXT signal은 detector와 기존 detector Audition 외에는 output에 더하지 않는다. Delta Listen에서도 외부 key는 gain을 결정할 뿐 carrier로 누설되지 않는다.
- Stereo main의 Mid/Side는 `M=0.5(L+R)`, `S=0.5(L-R)`로 processing band에 사용한다. 각 mono external key는 Stereo detector에서는 양쪽 입력에 복제하고 Mid/Side detector에서는 선택 domain의 key로 사용하므로 모든 mapped key가 유효하게 유지되며, 출력 delta만 main L/R에 재배치한다.
- Mono main에서는 raw Channel Mode 값을 state/automation에 보존하되 유효 DSP mode를 Stereo로 바꾼다. UI는 `MONO > STEREO`를 표시한다.
- External key가 mono인 것은 정상적인 mapped-lane 상태이며 연결 여부를 energy로 추측하지 않는다. 연결된 silence도 silence로 존중하고 `NO SC INPUT`으로 바꾸지 않는다.
- Mid/Side에서는 단일 domain envelope를 사용하므로 Stereo Link는 DSP 의미가 없다. 기존 parameter, host/state round-trip과 Stereo-mode DSP 보간은 보존하지만 custom editor에는 `LINK` label/control, bounds와 hit target을 두지 않는다.

## 품질, 오버샘플링과 레이턴시 결정

- Oversampling은 JUCE `dsp::Oversampling<float>`의 half-band FIR equiripple round trip을 실제 main audio, 4개 band의 L/R lanes와 네 external detector lanes에 적용한다. `Sidechain 1–4` source를 1:1로 유지하고 stereo source는 `(L+R)/2`로 downmix한다. Off/2×/4×/8×용 factor exponent `0/1/2/3` 객체와 scratch를 `prepare()`에서 미리 만들며 callback에서 할당하지 않는다.
- 48 kHz에서 현재 JUCE round-trip reported latency는 2× `49`, 4× `61`, 8× `65` base-rate samples이고 neutral impulse peak도 각각 같은 위치다. 이 값은 JUCE/filter revision과 sample rate의 함수이므로 고정 ABI 상수가 아니라 harness가 실제 객체에서 다시 검증하는 값이다.
- 48 kHz/64 samples current single-live-response DSP Engine/Harness clean-idle Release run의 p50/p95/p99/max는 Default `12.10/17.90/19.60/37.30`, Worst Dynamic `17.30/25.50/34.00/218.90`, Linear+8× `291.60/475.60/699.10/1289.70`, 8 worst Dynamic instances `142.00/201.20/252.60/667.70 µs`다. `1333.33 µs` block deadline 대비 p99는 각각 약 `1.5%/2.6%/52.4%/18.9%`이고 Linear+8× max는 약 `96.7%`다. 네 측정군의 max도 deadline 안이고 callback `new/delete`는 0이다. Debug/Release direct run은 각각 `58/58`; transcript와 final authored-tests source fingerprint는 `RELEASE_QA.md` current provenance에 고정했다.
- Lookahead는 main processing audio를 0–20 ms 늦추고 detector timeline은 앞에 유지한다. 기존 detector Audition, Delta Listen과 dry/bypass도 선택 경로의 총 latency에 맞춰 정렬한다.
- 전체 보고값은 `lookahead base samples + Linear(group delay + partition delay) + oversampling round-trip latency`다.
- Processing Mode, Oversampling, Lookahead는 latency를 바꾸므로 host parameter로 저장·표시하되 automatable이 아니다. APVTS listener는 `AsyncUpdater`를 통해 승인값과 `setLatencySamples()`를 message thread에서 함께 commit한다.
- 경로 변경 시 지연 버퍼와 detector/filter state를 reset하고 전환 출력을 제한한다. 실제 click-free 여부와 host의 delay-compensation 재협상은 새 revision QA 항목이다.

## Dynamics 수식

`d`는 threshold의 active side로 넘어간 거리다. Knee `K`에 대해 표준 soft-knee activation을 사용한다.

```text
activation(d, K) = 0                              if d <= -K/2
                 = (d + K/2)^2 / (2K)            if -K/2 < d < K/2
                 = d                              if d >= K/2
```

- Compress active side: `detector - threshold`, magnitude scale `1 - 1/ratio`.
- Expand active side: `threshold - detector`, magnitude scale `ratio - 1`.
- gain sign은 mode와 무관하게 signed Range가 직접 결정한다. `+Range`는 양의 gain, `-Range`는 음의 gain이다.
- 최종 gain dB는 signed Range를 넘지 않게 clamp한다.
- peak envelope는 attack/release one-pole smoothing, stereo detector는 unlinked level과 channel max를 `Stereo Link`로 보간한다.

## Delta Listen 계약

선택 band `k`의 contribution은 다음과 같다.

```text
D_k = (g_k - 1) × B_k(x)
DeltaOut = OutputGain × D_k
```

- Delta는 normal `dry + Mix × ΣD_b`가 아니라 선택 band contribution만 듣는다. 따라서 global Mix와 무관하지만 해당 Band Output과 hidden global Output은 반영한다.
- Off/Band 1–4는 lock-free transient processor atomic이며 한 band만 활성화된다. Active band가 바뀌거나 Off로 가면 20 ms equal-domain weight crossfade로 이전 weight에서 새 weight로 transfer한다.
- Panel `DELTA` click은 선택 band를 latch하며 같은 latched band click은 Off, 다른 band click은 latch transfer다. Enabled node right-button down은 pre-hold latch를 저장한 momentary override이고, hold-drag는 처음 누른 band의 Frequency만 바꾸며 Range와 momentary band를 고정한다. 정상 mouse-up 또는 realtime right-button-loss release는 processor 선택이 held band에 그대로 있을 때만 pre-hold latch를 복원한다.
- Delta가 활성화되면 기존 `band*_audition` detector signal보다 우선한다. Delta를 끄면 기존 Audition 상태와 신호로 복귀하며 legacy parameter 값을 변경하지 않는다.
- Final soft Bypass가 가장 나중에 적용되므로 bypass 중에는 latency-aligned dry로 돌아간다. EXT key는 gain control에만 쓰이고 Delta carrier에는 섞이지 않는다.
- prepare/reset/release, 성공한 state load, Factory/User preset recall, editor hide와 destruction은 active hold와 pending restore를 취소하고 선택을 복원 없이 Off로 reset한다. Hold 중 외부 reset도 우선하며 이후 mouse-up이 이전 latch를 되살리지 않는다. 적용 전에 거부된 손상·미래 state는 현재 transient 선택도 바꾸지 않는다.
- 이 상태는 monitoring 편의 기능이며 parameter/state/preset/automation/Undo surface가 아니다. 기존 78 parameter와 schema/preset v4에는 추가되지 않으며 Host Profile의 parameter 목록도 변경 대상이 아니다. Bundle profile identity는 current Release rebuild에서 다시 검증한다.

## Analyzer 표시 경로

오디오 출력 DSP와 독립된 표시 전용 후보를 비교했다.

- 후보 A(선택): TB_EQ의 공개 프로젝트 패턴처럼 audio callback은 engine 직전 main input만 단일 fixed SPSC FIFO에 복사하고, 기존 editor timer가 Hann window와 2048-point FFT를 수행한다. 처리 결과 spectrum은 idle에서 0 dB datum이 되고 active 구간만 변형되는 단일 signed actual DSP line과 정보가 중복되고 원본 판독을 흐리므로 별도 tap/FIFO를 두지 않는다.
- 후보 B(제외): 별도 analyzer worker thread와 triple buffer. GUI stall 분리는 가능하지만 thread lifecycle·동기화·테스트 범위가 커지고 현재 2048 FFT/최대 30 Hz 요구에는 불필요하다.
- 후보 C(제외): audio callback 안에서 FFT. callback CPU spike와 real-time contract 위반 위험 때문에 사용하지 않는다.

선택한 경로는 input용 고정 16,384-sample stereo FIFO 하나와 fixed FFT arrays만 사용한다. FIFO가 가득 차면 표시 sample만 bounded drop하며 오디오를 block하거나 buffer를 grow하지 않는다. Analyzer demand가 0이거나 offline render이면 producer도 실행하지 않는다. Sidechain 전체 host buffer가 아니라 engine 직전 main bus view만 전달하므로 external key와 처리 결과가 표시 spectrum에 섞이지 않는다. UI에는 단일 `ANALYZER` toggle만 두며 이 상태는 78개 host parameter에 포함하지 않는다.

표시 스펙트럼은 새 FFT frame마다 bin별 temporal smoothing을 적용한다. target이 상승할 때 `0.78`, 하강할 때 `0.24`를 사용하고, 로그 주파수로 샘플한 512 points를 전방·후방 두 번 `0.42 current + 0.58 neighbor`로 평활한다. 이는 사람이 형태를 읽기 위한 표시 필터이며 DSP 오디오나 정밀 계측 데이터에는 적용하지 않는다. 무신호 stale frame은 정해진 release 뒤 floor로 돌아간다.

Response와 INPUT은 같은 log-frequency X 좌표를 공유하지만 Y 좌표는 의미에 맞게 분리한다. Component-local plot `{42,38,892,300}`의 물리 top/zero/bottom은 `y=38/188/338`이며 위·아래 half는 각각 `150 px`로 유지한다. Response는 양수 `0…+12 dB`를 위 half, 음수 `0…-48 dB`를 아래 half에 piecewise 매핑하고, 왼쪽 눈금 `+12/+6/0/-24/-48 dB`를 같은 물리 간격 `38/113/188/263/338`에 둔다. 오른쪽 response Y축 숫자는 그리지 않는다.

INPUT FFT는 response의 signed ±값을 무시하고 `0…-96 dBFS`를 plot top `38`에서 bottom `338`까지 독립 선형 매핑한다. 상단의 `0 dBFS` caption이 이 원본 level scale를 명시한다. 따라서 중앙 `y=188`은 live response가 gain 0일 때 놓이는 0 dB 좌표지만 INPUT FFT에서는 -48 dBFS에 해당하며, 두 값의 단위/목적을 혼용하지 않는다. 이 분리는 표시 좌표 계약일 뿐 FFT 분석값, gain meter 또는 오디오 DSP를 다시 스케일하는 처리 단계가 아니다.

각 밴드의 frequency/width 영역은 저채도 band 고유색의 full-height 양 edge와 중앙이 진한 수평 gradient로 구분한다. Range 최대 증폭·감쇄는 node의 signed Y로만 표시하며 disabled ghost도 저장된 Range Y를 유지한다. Response는 plot 전폭을 잇는 dark pale-yellow `#817C66`, alpha `1.0`, `3.0 px` rounded stroke 하나다. DSP의 signed per-band gain meter를 현재 frequency/width의 raised-cosine shape로 합산하고, gain이 없거나 active band 밖인 구간은 `y=188`의 0 dB에 머물러 이 연속선 자체가 datum 역할을 한다. `+Range`가 활성화된 actual gain 구간은 위, `-Range`는 아래로 같은 선을 변형한다. 별도 fixed datum, configured Range contour, red actual overlay, gain fill, wash, shadow와 colour gradient는 그리지 않는다. 이 line은 실제 gain state의 시각화이지 측정 transfer curve가 아니다. Plot 내부 배경만 `#20242C → #141820`으로 외곽 glass보다 옅게 해 depth를 구분한다. 상단 좌측에는 고정 `INPUT` source label, 상단에는 `0 dBFS` caption만 두며 band/actual/input 숫자 debug readout은 그리지 않는다.

Generated pointer-free Bakelite knob raster는 고정된 UI material layer일 뿐 analyzer·meter·DSP state를 생성하지 않는다. 8개 control은 `{20+100i,40,84,76}` grid, `100 px` pitch, `16 px` gap과 `20 px` side margins를 사용하고 renderer의 body는 100%에서 최대 `50 px`다. 순서는 Frequency, Width, Threshold, Range, Ratio, Knee, Attack, Release다. Frequency/Width=Blue, 인접한 Threshold/Range/Ratio/Knee=Green, Attack/Release=Red의 낮은 alpha tint를 적용하며 값은 procedural Aged Brass tick/marker와 text로만 표시한다. Band Output parameter/DSP/state는 보존하지만 custom editor knob는 그리지 않는다. Raster body를 parameter angle로 회전하지 않아 조명 방향과 재질 결이 signal state처럼 오인되지 않게 한다.

Custom editor header는 정확히 `Multi Band Compressor`를 표시하고 M/B/C에 각각 `#7088B2/#4F805F/#C65350`, 나머지 문자에 `#D9DAE1`을 사용한다. 이는 표시 계약만 바꾸며 내부 프로젝트·번들명 `TB_MultiBandCompressor`와 parameter/state identity는 바꾸지 않는다.

Graph hit arbitration은 node 중심에서 `12 design px` 이내일 때 node를 full-height bandwidth edge보다 우선한다. Node 수직 drag는 X를 고정한 채 piecewise response 좌표의 signed Range `-48…+12 dB` 전체 축을 오가고 frequency를 보존하며, 각 drag는 균형 잡힌 host gesture와 Undo transaction 하나다.

## Parameter/state 결정

- 현재 APVTS는 78개다. v1의 첫 71개 ID와 순서는 유지하고 `phase_mode`, `oversampling`, `lookahead`, `band1..4_channel_mode`를 append한다.
- State schema와 preset format은 v4다. APVTS count/ID/order는 그대로 78개이며 Range metadata만 `-48…+12 dB`로 바뀐다.
- v4는 알려진 78개 ID/순서/value contract와 현재 choice/range를 exact validation한다.
- v3 migration은 정확한 78개 ID/순서와 당시 value contract를, v2 migration은 정확한 78개 ID/순서와 Off/2×/4× Oversampling contract를 검증한다. v1 migration은 정확히 71개 legacy ID/순서/value를 받고 신규 7개를 Dynamic/Off/0 ms/Stereo 기본값으로 채운다.
- v3/v2/v1의 각 Range 값은 legacy `-24…+24 dB`로 먼저 validation한 뒤 v4 domain으로 clamp한다. 기존 `+24 dB`는 `+12 dB`가 되고 음수 값과 `+12 dB` 이하는 보존되며, 성공한 데이터는 다음 저장 시 v4가 된다.
- v1에 신규 필드를 섞거나 count/ID/choice/legacy range가 다른 데이터, v2의 위조 8× 값, 미래 버전과 손상 데이터는 적용 전 전체를 거부한다. 부분 state mutation은 허용하지 않는다.
- `band*_detector_mode`와 detector frequency/bandwidth는 제거하지 않고 기존 ID·choice·raw value/state round-trip을 보존하되 current DSP에서는 inert/FOLLOW 고정이다. 이전 FREE 음향은 compatibility 대상이 아니다. `band*_audition`은 detector-listen control/function을 보존하며 current effective FOLLOW detector를 듣는다. Factory 20개는 모두 BAND/FOLLOW이며 detector center/width를 processing band와 일치시킨다.
- Delta Listen은 위 78개에 추가되지 않는 transient monitor state다. 성공한 state/preset 적용은 panel latch, active node hold와 pending pre-hold restore를 모두 취소해 Off로 만들고, 저장 데이터에는 기록하지 않는다.

## Harness

1. Dynamic/Linear 각각 neutral impulse/noise가 reported latency만큼 이동한 dry와 null하는지 확인한다.
2. Linear band impulse가 reported delay를 중심으로 대칭인지, 2×/4×/8×와 0/10/20 ms lookahead가 실제/보고 latency에 일치하는지 확인한다.
3. 1 kHz fixed level steps로 Compress=threshold 위, Expand=threshold 아래 활성 영역과 `+Range` 양의 gain/`-Range` 음의 gain, ratio/knee/cap, Attack/Release를 검증한다.
4. band center와 원거리 tone의 response/GR 비교, 네 band overlap의 finite/headroom 확인.
5. band별 INT/EXT, 네 optional mono-default `Sidechain 1–4` Send/Aux, stereo `(L+R)/2` downmix, `Sidechain N→Band N` isolation, 네 key 동시 ducking, 선택 bus fallback, connected silence, Stereo/Mid/Side detector domain, output leakage를 확인한다.
6. Stereo/Mid/Side 분리와 mono-main Stereo fallback, mode/OS/lookahead 전환 bound를 확인한다.
7. 44.1/48/96/192 kHz, block 0/1/3/63/65/257/expected 초과, mono/stereo와 callback allocation 0을 확인한다.
8. 78-value v4 state/preset round-trip, exact v3/v2 78 identity migration, exact v1 71 migration, legacy Range `-24…+24` validation과 `+24→+12` clamp, malformed/future atomic rejection과 host metadata를 확인한다.
9. Delta가 선택 band wet-minus-dry와 일치하는지, global Mix 독립·Band/global Output·final Bypass·20 ms exclusive transfer·legacy Audition precedence·EXT non-leak·M/S·mono fallback을 확인한다. UI integration은 panel latch, right-button hold 중 같은 band Frequency 이동·Range 불변·button-loss release, 정상 pre-hold restore와 state/preset/external-reset 뒤 미복원을 확인한다.
10. Graph interaction은 node 중심 `≤12 design px` 우선, 수직 drag의 Range `-48/+12 dB`, response top/zero/bottom `38/188/338`, frequency invariant와 방향별 one Undo, disabled saved-Range ghost를 확인한다. INPUT FFT의 별도 `0…-96 dBFS` mapping과 top `0 dBFS` caption도 response 축과 독립적으로 확인한다.

## 청감 A/B와 재검토 조건

- `DRM_01`, `VOX_01`, `KEY_01`에서 level-matched bypass/Init/preset 비교.
- 인접 대역 pumping, narrow-band ringing, strong upward range headroom을 집중 확인.
- neutral null 실패, 외부 SC 누설, Dynamic의 지속 pad phase coloration 또는 Linear의 pre-ringing/latency가 유용성보다 큰 경우 후보 D 또는 LR mode를 재검토한다.
- `KEY_01`과 full mix에서 Dynamic/Linear를 level-match해 mastering 선택 이유가 실제로 반복 구분되지 않으면 Linear의 출시 문구와 기본 사용처를 축소한다.
- 4 band 한계가 실제 workflow에서 반복 확인될 때만 6 band 확장을 검토한다.
