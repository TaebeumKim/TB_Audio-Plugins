# TB_MultiBandCompressor 제품 브리프

작성일: 2026-08-11  
상태: 1.0.1 v4 release candidate — single-live-response automated engineering `PASS`, 수동 release validation `HOLD`

## 제품 기본 정보

- 프로젝트·번들명: `TB_MultiBandCompressor`; custom editor 표시명: 정확히 `Multi Band Compressor`
- 한 문장 정의: 필요한 주파수 구간에만 자유 밴드를 배치하고, 각 밴드를 독립 검출기·외부 사이드체인·Stereo/Mid/Side 대상으로 압축·확장하며 Dynamic 또는 Linear 처리 품질을 고르는 멀티밴드 다이내믹스.
- 플러그인 종류: Stereo/Mono VST3 및 Standalone dynamics effect
- 주요 사용자: 믹싱·마스터링 엔지니어, 프로듀서
- 주요 소스: 드럼·보컬·베이스·버스·풀믹스
- 레퍼런스: FabFilter Pro-MB의 자유 밴드 워크플로와 밴드별 검출기 UX. 화면, 상표, 프리셋 또는 비공개 DSP는 복제하지 않는다.

## 해결하려는 문제와 선택 이유

전통적인 멀티밴드 컴프레서는 먼저 전체 스펙트럼을 크로스오버로 나눈 뒤 모든 구간을 재합성한다. 이 제품은 문제 구간에만 밴드를 켜고 나머지 주파수는 원신호로 남긴다. 선택 이유는 다음 세 가지다.

1. 전체 대역을 불필요하게 분할하지 않는 자유 배치 밴드.
2. Ratio와 별개인 signed `Range`로 최대 게인 변화를 제한하고, `+Range=증폭`·`-Range=감쇄`를 화면과 소리에서 그대로 일치시키는 동작.
3. 호스트에 4개의 독립적인 mono 보조 입력을 노출하고 각 입력을 해당 밴드의 외부 검출 키로 고정 연결하는 sidechain workflow. Custom editor의 버튼 글자는 항상 `SIDECHAIN`이며 Off는 내부 검출, On은 해당 외부 입력 검출을 뜻한다.

사용자가 말한 “각각 채널의 사이드체인”은 **DAW에 `Sidechain 1`부터 `Sidechain 4`까지 4개의 독립 optional mono-default 보조 입력을 표시하고, 각각을 Band 1부터 Band 4에 고정 연결하는 기능**으로 해석한다. 따라서 사용자는 네 개의 Send/Aux 선택지에서 각 밴드의 더킹 소스를 따로 고를 수 있다. 각 보조 입력은 해당 번호 밴드의 외부 검출 키가 되며, 선택한 버스가 없으면 그 밴드만 내부 검출기로 fallback한다. 각 버스는 stereo 연결도 허용하고 DSP에서 `(L+R)/2`로 mono detector key를 만든다. JUCE의 고정 5-bus 구성 때문에 기존 단일 shared mono/stereo/quad 세션 라우팅은 보장하지 않으며, 업데이트 후 Send/Aux를 다시 연결해야 할 수 있다. APVTS parameter, VST3 CID, state/preset schema와 ID/order는 변경하지 않는다. 기존 밴드별 Stereo Link parameter는 L/R 검출 보간, host/state와 DSP 호환성을 위해 유지하지만 custom editor에는 `LINK` control이나 label을 표시하지 않는다.

## 청감 A/B 성공 기준

- A: 바이패스 또는 동일 레벨의 원신호.
- B: 기본값 또는 해당 소스용 프리셋.
- 음량 매칭: DAW gain trim 또는 host generic parameter view의 compatibility Output으로 장기 RMS 차이를 약 0.5 dB 이내로 맞춘다. 현재 custom editor에는 global Output control이 없다.
- 반드시 들려야 할 차이: 드럼의 저역·고역 피크가 따로 안정되고 중역 어택은 유지됨, 보컬의 특정 바디/치찰 구간만 안정됨, 풀밴드 펌핑 없이 문제 대역의 움직임만 줄거나 강조됨.
- 실패: GR이 0인데 null이 깨짐, 외부 사이드체인이 출력에 누설됨, 결과가 단순 음량 차이뿐임, 인접 대역이 과도하게 함께 펌핑함.

### 청감 A/B 사용 소재

| 역할 | 소재 | 선택 이유 |
| --- | --- | --- |
| 주 타깃 | `DRM_01` | 트랜지언트와 저·중·고역이 동시에 있어 밴드 독립성과 펀치를 확인한다. |
| 대조 | `VOX_01` | 지속음과 자음에서 레벨링·치찰 제어의 자연스러움을 확인한다. |
| 역방향 검증 | `KEY_01` | 지속 화음에서 크로스밴드 펌핑과 위상성 착색의 상한을 찾는다. |

## 기능 범위

### 필수

- 동시에 최대 4개의 자유 배치 밴드; 비활성 슬롯은 원신호에 영향을 주지 않는다.
- 그래프에서 밴드 선택, 중심 주파수 이동, Width 조정, signed Range 조정.
- 밴드별 Threshold, Range, Ratio, Knee, Attack, Release, Compress/Expand. Band Output은 host/state/DSP 호환성을 위해 유지하지만 custom editor knob는 숨긴다.
- Compress는 threshold 위, Expand는 threshold 아래를 활성 영역으로 선택한다. 활성 영역에서 `+Range`는 양의 gain, `-Range`는 음의 gain이며 Range 절댓값이 최대 변화를 제한한다.
- 밴드별 `SIDECHAIN` Off/On. Stereo Link parameter와 attachment, host/state round-trip과 L/R detector DSP는 유지하지만 custom editor control, label, bounds와 hit target은 숨긴다. 기존 `BAND/FREE`와 detector center/width 값은 78-parameter/state raw round-trip을 위해 저장·복원하지만 current DSP는 항상 processing band를 따라간다. 이전 FREE 음향은 보존하지 않는다. 기존 detector Audition control의 detector-listen 기능은 generic host에 남지만 current effective FOLLOW detector를 듣고 custom editor에서는 숨긴다.
- 선택 밴드의 실제 wet-minus-dry contribution을 듣는 배타적 `Delta Listen`. Panel의 `DELTA` 버튼은 선택 band를 latch하고 같은 band를 다시 누르면 Off, 다른 band에서 누르면 latch를 transfer한다. Enabled node의 right-button down은 누르고 있는 동안만 momentary override하며, 누른 채 수평 drag하면 처음 누른 band의 Frequency만 이동하고 Range와 Delta 대상은 유지한다. mouse-up 또는 realtime button-loss에서 끝나고 정상 release는 hold 직전 panel latch를 복원하지만 외부 reset이 먼저 일어나면 복원하지 않는다. Delta 상태는 parameter/state/preset/automation/Undo에 포함하지 않고 Frequency edit만 one Undo/host gesture를 만든다.
- 밴드별 처리 채널 `STEREO / MID / SIDE`. Stereo main에서는 선택한 도메인으로 processing band와 detector를 함께 변환한다. Mono main에서는 저장된 선택값은 보존하되 유효 처리 도메인은 `STEREO`로 결정적으로 fallback한다.
- 외부 sidechain은 `Sidechain 1`–`Sidechain 4`의 optional mono-default bus 네 개다. 각 버스의 `EXT` detector는 같은 번호 밴드에 고정 매핑되고, stereo 연결은 DSP에서 `(L+R)/2`로 하나의 mono detector key로 downmix한다. 선택한 버스가 없으면 해당 밴드만 내부 검출기로 결정적으로 fallback한다. 미연결 bus일 때 UI에 `NO SC INPUT`을 표시하고 연결된 무음은 무음으로 존중하며 연결 여부를 에너지로 추측하지 않는다. 기존 단일 bus 세션은 업데이트 후 Send/Aux 재연결이 필요할 수 있다.
- 선택된 external mono bus는 Stereo detector의 양쪽 입력에 동일한 key로 복제하고, Mid/Side detector에서는 선택 domain의 key로 사용한다. 따라서 각 mapped key는 모든 processing mode에서 유효한 detector source로 유지되며, 연결된 mono key를 미연결로 오인해 내부 입력으로 바꾸지 않는다.
- 전역 `DYNAMIC / LINEAR` processing mode. Dynamic은 lookahead와 oversampling이 Off일 때 0 sample latency를 유지하고, Linear는 대칭 FIR band-delta와 정렬된 dry delay로 실제 linear-phase 경로를 제공한다.
- 전역 Oversampling `OFF / 2× / 4× / 8×`, Lookahead `0–20 ms`, 정확한 host latency 보고와 latency-aligned soft bypass.
- 글로벌 Mix와 host-recognized Bypass. global Output parameter는 기존 78-parameter/state/automation compatibility와 DSP를 위해 유지하지만 현재 editor control은 숨긴다.
- APVTS 자동화, Undo/Redo, 전체 DAW state, Factory/User preset.
- 실제 GR·detector level 피드백과 현재 품질 설정에 맞는 레이턴시 보고.
- engine 직전 main input 하나만 표시하는 실제 FFT analyzer와 graph 직접 편집. 시간 스무딩은 상승 `0.78`/하강 `0.24`, 표시용 로그 주파수 공간 스무딩은 전·후방 2-pass blend `0.42`를 사용한다. INPUT spectrum은 response 좌표와 분리된 `0…-96 dBFS` 축을 plot 상단에서 하단까지 사용하고 상단에 `0 dBFS` caption을 둔다. 처리 결과 spectrum은 겹치지 않으며, Analyzer가 꺼지거나 editor가 숨겨지면 input sample tap·FFT를 중단한다.
- 밴드별 full-height edge와 수평 gradient 위에 plot 전폭을 잇는 dark pale-yellow `#817C66`, alpha `1.0`, `3.0 px` actual DSP response stroke 하나만 표시한다. gain이 없거나 active band 밖인 구간은 물리적 중앙 `y=188`의 0 dB에 머물러 선 자체가 datum 역할을 하고, 실제 gain 구간만 같은 연속선이 위·아래로 변형된다. Range 최대 증폭·감쇄는 node의 signed Y로만 표시한다. 별도 datum, configured Range contour, red overlay, gain fill, shadow와 gradient는 없다. Node 중심 `12 design px` 이내는 full-height edge보다 node hit가 우선하고 disabled ghost도 저장된 signed Range Y를 유지한다. Live line은 실제 gain state의 시각화이며 측정 transfer response라고 주장하지 않는다.

### 이후 후보

- 6개 밴드, 밴드 snapping, 다중 선택 편집.
- 밴드별 lookahead, Minimum Phase와 전통적 crossover processing mode.

### 제외

- FabFilter의 `Dynamic Phase` 명칭 또는 비공개 알고리즘을 동일하다고 주장하는 것.
- 네트워크/AI/MCP 런타임. 현재 범위는 DAW에 노출되는 네 개의 mono sidechain Send/Aux 입력과 고정된 Band 1–4 매핑이다.
- AAX/AU/CLAP, 온라인 라이선스, 카탈로그 배포.

## 컨트롤과 호스트 계약

모든 밴드 파라미터는 `band{1..4}_<name>` 형식의 출시 고정 Source Parameter ID를 사용한다.
현재 계약은 기존 71개 순서를 그대로 앞에 보존하고 전역 품질 3개와 밴드별 Channel Mode 4개를 뒤에 추가한 총 78개다.

| 컨트롤 | 범위 / 기본값 | 역할 | 위계 |
| --- | --- | --- | --- |
| Band Enabled | Off/On / 밴드 1~3 On, 4 Off | 밴드 슬롯 활성화 | Standard |
| Frequency | 30 Hz–18 kHz / 밴드별 120 Hz, 1.2 kHz, 7 kHz, 12 kHz | 처리 중심 | Primary |
| Width | 0.25–4 oct / 1.5 oct | 처리 폭; custom UI 표기는 정확히 `WIDTH`, 저장 단위는 octave | Primary |
| Threshold | -60–0 dB / -18 dB | 동작 기준 | Primary |
| Range | -48–+12 dB / -3 dB | 최대 게인 변화와 방향 | Primary |
| Ratio | 1:1–20:1 / 2:1 | 변화 비율 | Standard |
| Knee | 0–18 dB / 6 dB | 임계점 부드러움 | Standard |
| Attack | 0.5–200 ms / 15 ms | 반응 속도 | Standard |
| Release | 20–2000 ms / 180 ms | 복귀 속도 | Standard |
| Band Output | -18–+18 dB / 0 dB | 밴드 정적 보정; custom editor knob hidden | Compatibility |
| Dynamics | Compress/Expand / Compress | 다이내믹 종류 | Standard |
| Source | INT/EXT / INT | 검출 입력. `EXT`는 해당 밴드 번호의 `Sidechain 1–4` mono bus를 사용하고 Custom editor 표기는 상태와 무관하게 `SIDECHAIN` | Standard |
| Detect | BAND/FREE / BAND | raw host/state choice round-trip; effective DSP는 BAND 고정, 이전 FREE 음향 미보존 | Compatibility |
| Detector Frequency | 30 Hz–18 kHz / 처리 중심 | raw host/state 값 보존; current DSP에서는 inert | Compatibility |
| Detector Bandwidth | 0.25–4 oct / 1.5 oct | raw host/state 값 보존; current DSP에서는 inert | Compatibility |
| Stereo Link | 0–100% / 100% | L/R 검출 연동; host/state/DSP 유지, custom editor UI hidden | Compatibility |
| Channel Mode | Stereo/Mid/Side / Stereo | 밴드가 실제로 바꾸는 stereo 성분 | Standard |
| Audition | Off/On / Off | 기존 detector signal 청취 파라미터; editor UI hidden | Compatibility |
| Delta Listen | Off/Band 1–4 / Off | Panel click latch + node right-button hold momentary wet-minus-dry monitor; host/state 밖 | Supporting |
| Processing Mode | Dynamic/Linear / Dynamic | zero-latency delta 또는 linear-phase FIR delta | Standard |
| Oversampling | Off/2×/4×/8× / Off | 빠른 gain modulation의 alias 저감 | Supporting |
| Lookahead | 0–20 ms / 0 ms | detector보다 오디오를 늦춰 transient 예측 | Standard |
| Mix | 0–100% / 100% | 전체 효과량 | Primary |
| Output | -18–+18 dB / 0 dB | 기존 host/state 호환용 최종 gain; editor UI hidden | Compatibility |
| Bypass | Off/On / Off | 전체 soft bypass | Supporting |

## 신호 구조와 기술 가정

```text
Main input ─┬─ INPUT analyzer tap (display demand가 있을 때만)
            ├─ latency-aligned dry/lookahead delay ───────────┐
            └─ 4 parallel Dynamic TPT / Linear FIR bands ─────┤
External SC [Sidechain 1..4] ── fixed Sidechain n→Band n, stereo `(L+R)/2` downmix ── per-band INT/EXT → processing-band detector filter ┘
                 → per-band Stereo/Mid/Side gain delta → Mix → Output
```

- 핵심 수식: `y = x + Σ((g_b - 1) × B_b(x))`.
- 선택 band `k`의 Delta Listen은 `D_k=(g_k-1)×B_k(x)`를 20 ms 교차 페이드해 단독 청취한다. Global Mix와 무관하고 Band Output과 hidden global Output을 적용하며, 최종 Bypass가 우선한다. Delta가 켜진 동안에는 기존 detector Audition보다 우선하고 해제하면 기존 Audition으로 돌아간다.
- Panel `DELTA`는 latched Off/Band 1–4 선택이다. Node right-button hold는 그 latch를 임시로 덮고, right-drag는 처음 누른 band의 Frequency만 이동하며 Range와 Delta 대상 band를 유지한다. 정상 mouse-up 또는 realtime button-loss release는 hold 직전 latch를 복원하되, hold 중 processor 선택이 외부에서 바뀌면 그 외부 reset을 존중하고 latch를 되살리지 않는다.
- Delta 선택과 pending latch restore는 prepare/reset/release, 성공한 state load, Factory/User preset recall, editor hide와 editor destruction에서 모두 Off/무효화된다. 손상·미래 state처럼 적용 전에 거부된 입력은 현재 runtime 선택도 바꾸지 않는다.
- 밴드 gain이 모두 1이면 delta가 정확히 0이므로 필터의 정적 위상 변화가 출력에 남지 않는다.
- 이 구조는 Pro-MB 내부 구현의 재현이 아니라, 공개된 “필요한 밴드만 처리하고 gain 변화가 없을 때 투명”이라는 목표를 위한 독자적 최소 구조다.
- 채널: main mono/stereo, optional mono-default sidechain bus 4개(`Sidechain 1–4`), output은 main과 동일. 각 bus는 같은 번호 Band 1–4에 1:1 매핑하고 stereo 연결은 `(L+R)/2`로 downmix한다. 기존 단일 bus 세션 라우팅은 보장하지 않으며 업데이트 뒤 Send/Aux 재연결이 필요할 수 있다.
- 레이턴시: `Dynamic + Oversampling Off + Lookahead 0 ms`는 0 samples. Linear의 대칭 FIR group delay와 partition delay, 선택한 oversampling round-trip latency, lookahead delay를 합산하여 host에 보고한다. 품질 변경은 비동기 승인 경로에서 DSP 선택값과 host latency를 함께 갱신한다. Tail은 0 seconds이며 reset 뒤 지연 버퍼가 남지 않는다.
- 오버샘플링: JUCE half-band FIR equiripple의 실제 Off/2×/4×/8× path를 main, 4 processing bands와 band-mapped detector lanes에 적용한다. factor exponent `0/1/2/3` 객체와 scratch는 `prepareToPlay()`에서 미리 만들며 callback에서 할당하지 않는다. 48 kHz 현재 measured round-trip latency는 2× `49`, 4× `61`, 8× `65` samples다.
- 외부 라이브러리: JUCE 8.0.12만 직접 사용하며, 제품 코드에 별도로 추가한 제3자 소스는 없음. JUCE 자체 의존성은 `THIRD_PARTY_NOTICES.md`를 따른다.

## 외부 호스트 계약

- Manufacturer Code / Plugin Code: `TbSd` / `TbMb`.
- Project ID: `TbMbCp`.
- Bundle Identifier: `com.TeamImpulseImpact.TB_MultiBandCompressor`.
- VST3 Processor CID: `ABCDEF019182FAEB5462536454624D62`.
- VST3 Controller CID: `ABCDEF011234ABCD5462536454624D62`.
- State schema / preset format: 4 / 4.
- 현재 v4 state/preset은 알려진 78개 ID, 순서, 종류, 새 Range `-48…+12 dB`와 choice 정수 조건을 exact validation한 뒤에만 적용한다. Parameter count/ID/order는 v3와 동일하다.
- Schema/format 3은 **정확한 78개 identity contract**, schema/format 2는 **정확한 78개 identity와 당시 Off/2×/4× Oversampling contract**, schema/format 1은 **정확한 기존 71개 contract**만 허용한다. v1은 신규 7개를 `Dynamic / Oversampling Off / Lookahead 0 ms / 각 밴드 Stereo` 기본값으로 append한다.
- v3/v2/v1의 Range는 당시 `-24…+24 dB` 범위로 먼저 검증한 뒤 v4 `-48…+12 dB`로 clamp한다. 따라서 음수 값과 `+12 dB` 이하는 보존되고 기존 `+24 dB`는 `+12 dB`가 되며, 성공한 legacy 데이터는 다음 저장 시 v4가 된다.
- count/ID/choice/legacy range가 다른 데이터, v2의 8× 값, v1에 신규 필드를 섞은 데이터, 알 수 없는 ID·미래 버전·손상 데이터는 현재 상태를 일부도 바꾸지 않고 거부한다.
- Aether AI 제어: 이번 범위에서 해당 없음.
- 허용 표면: Raw Parameter + Factory/User Preset.
- 자동화: 밴드별 Channel Mode는 automatable이다. 레이턴시를 바꾸는 Processing Mode, Oversampling, Lookahead는 host에 공개하고 state/preset에 저장하지만 automatable이 아니다.
- `band*_detector_mode`와 detector frequency/bandwidth는 기존 ID·choice·raw 자동화/state round-trip만 보존하고 effective DSP에서는 BAND/FOLLOW로 강제한다. 따라서 이전 FREE 음색은 보존하지 않는다. `band*_audition`은 detector-listen control/function을 보존하는 generic-host compatibility surface이며 current effective FOLLOW detector를 듣는다. Custom editor의 Delta Listen은 APVTS 파라미터가 아니며 저장·자동화·Undo 대상이 아니다.

## UX와 시각 방향

- 첫 행동: 그래프에서 밴드를 클릭하거나 빈 슬롯을 켠 뒤 중심·Width·Range를 직접 조정.
- Hero Display: 실제 main input spectrum 위 자유 band 영역, band별 full-height edge/수평 gradient와 단일 dark pale-yellow actual DSP response line. 이 `#817C66`, alpha `1.0`, `3.0 px` 연속선은 idle/outside-band에서 0 dB datum이고 active 구간만 실제 gain에 따라 위·아래로 변형된다. Range maximum은 node Y로만 읽는다. 왼쪽 response Y축에는 `+12/+6/0/-24/-48`을 같은 물리 간격으로 표시하고 상단 좌측의 고정 `INPUT` label과 상단 `0 dBFS` caption으로 원본 source/scale를 알린다. 처리 결과 spectrum, PRE/POST source selector, band/actual/input 숫자 debug readout, 별도 datum/configured/red overlay와 gain fill/shadow/gradient는 표시하지 않는다.
- 밴드별 sidechain: 선택 band panel의 고정 글자 `[SIDECHAIN]` toggle로 Off=INT, On=EXT를 나타낸다. Detector mode/range와 기존 detector Audition UI는 표시하지 않는다.
- Delta Listen: 선택 panel의 `[DELTA|DELTA ON]`은 band를 latch한다. Enabled node는 right-button down 동안 momentary override하고, 수평 hold-drag로 같은 band의 Frequency를 옮기며 Range와 Delta 대상은 바꾸지 않는다. 정상 release에서 pre-hold latch로 돌아가고 빈 graph 우클릭은 no-op이다. Active node ring은 유지하되 별도 `DELTA Bn` 숫자 readout은 두지 않는다.
- 핵심 메타포: 한 신호를 통째로 누르는 프레스가 아니라, 필요한 주파수 섬만 서로 독립적인 스프링 클램프로 잡는 장면.
- 에디터: `1000 × 620 px`. Header 아래 Analyzer는 `[12,54,976,499]`, visible plot은 component-local `{42,38,892,300}`이며 top/zero/bottom `y=38/188/338`과 위·아래 `150 px` 폭은 유지한다. Response 눈금 `+12 / +6 / 0 / -24 / -48 dB`는 각각 `y=38 / 113 / 188 / 263 / 338`에 놓고 왼쪽에만 둔다. Response 좌표는 양수 `0…+12 dB`와 음수 `0…-48 dB`를 각 150 px half에 piecewise 매핑하며, INPUT은 별도 `0…-96 dBFS`를 plot 전체 높이에 매핑한다. 선택 band control은 node를 가리키지 않는 fixed centered `{88,400,824,120}` panel이며 pointer와 Details shelf를 사용하지 않는다. Header 표시명은 정확히 `Multi Band Compressor`이고 M/B/C는 `#7088B2/#4F805F/#C65350`, 나머지 문자는 `#D9DAE1`이다. Stereo Link UI는 숨기고 main label은 `WIDTH`다.
- 하단 `[12,565,976,43]` strip에는 compact Bypass, Dynamic/Linear, OFF/2X/4X/8X, Lookahead/actual latency와 horizontal Mix를 둔다. global Output UI는 숨긴다.
- 시각화는 main bus의 실제 FFT와 actual DSP gain meter/state를 표시하고 가짜 파형을 사용하지 않는다. Range parameter의 최대치는 node Y로만 표시하며 response는 idle에서 datum이 되는 단일 live stroke로만 그린다.
- 디자인 기준은 `TB_Compressor/TB_Compressor/TB_Compressor Mockup (standalone).html`의 recessed hardware와 signal trace 위계다. 화면·예약색·기능은 복제하지 않고 TB_EQ의 analyzer 직접 편집 방식만 현재 4-slot graph에 축소 적용한다. 제공된 vintage compressor 이미지는 저채도 studio hardware의 재질·노브 감각만 참고했으며 로고, meter, panel layout과 control arrangement는 차용하지 않는다.
- 8개 rotary는 panel-local `{20+100i,40,84,76}`에 놓아 centre pitch `100 px`, bounds gap `16 px`, 좌우 margin `20 px`를 유지한다. 순서는 Frequency, Width, Threshold, Range, Ratio, Knee, Attack, Release이며 Threshold–Range–Ratio–Knee를 인접 배치하고 모두 Console Green으로 묶는다. generated `tb_knob_bakelite.png`의 pointer-free neutral Bakelite body는 고정 방향으로 그리며 renderer의 visible body는 100%에서 최대 `50 px`다. Frequency/Width는 Enamel Blue, Attack/Release는 Oxide Red로 cap을 tint하고 Aged Brass lit ticks·value marker와 text만 procedural layer로 갱신한다.

### 팔레트

| 토큰 | HEX | 역할 |
| --- | --- | --- |
| canvas-top / bottom | `#25262D` / `#111218` | charcoal rack 섀시 |
| surface-top / bottom | `#2B2C34` / `#18191F` | recessed control board |
| display-top / bottom | `#12151C` / `#080A0E` | Analyzer Hero outer glass |
| graph-top / bottom | `#20242C` / `#141820` | plot 내부의 더 옅은 depth layer |
| text-primary / secondary | `#D9DAE1` / `#A6A7B0` | label·status |
| value-text | `#E6E4DB` | 값·선택 node |
| accent-primary | `#C65350` | Delta/Bypass action |
| accent-secondary | `#7088B2` | Slate Blue input spectrum·sidechain·analyzer action |
| control-active | `#4F805F` | 일반 On 상태 |
| band-1 | `#A94B49` | Oxide Red Range·node |
| band-2 | `#3F7558` | Console Green Range·node |
| band-3 | `#4D6FA8` | Enamel Blue Range·node |
| band-4 / calibration-ivory | `#C7C1AD` | Band 4 Range·node·pointer·비활성 tick |
| aged-brass | `#AA985D` | 현재값까지의 작은 calibration tick과 procedural value marker |
| live-gain-response / datum | `#817C66` | alpha `1.0`, `3.0 px`의 단일 continuous actual DSP stroke; idle/outside-band에서 0 dB 기준 |
| header M / B / C | `#7088B2` / `#4F805F` / `#C65350` | `Multi Band Compressor`의 세 머리글자; 나머지 문자는 `#D9DAE1` |
| outline / deep-shadow / highlight | `#4A4C56` / `#06070A` / `#7A7B86` | 구조·재질 |

전체 concept은 charcoal, muted signal colours와 calibration ivory를 사용한다. Aged Brass는 작은 knob tick으로만 제한하고 주황색을 제품 concept accent로 사용하지 않는다.

### 확인한 공통 Mockup

`01_Portal.png`, `02_Wave_Shifter.png`, `03_Fresh_Air.png`, `04_Convolver.png`, `05_Magma_BB_Tubes.png`, `softube 예시.jpeg`, `softube 예시2.jpeg`, `가장 예쁜 디자인..png`.

- 유지: Portal의 큰 직접 조작 영역, Wave Shifter의 모듈 위계, Magma/Softube의 촉각적 부품, Atlas의 밀도와 값 판독.
- 수정: 레퍼런스의 배치·브랜드·네온을 복제하지 않고 TB 한 줄 헤더, compact selected-band 편집, 실제 상태 기반 표시로 재구성.

## Factory 프리셋 방향

- 기본값/Init은 세 밴드가 가볍게 동작하는 안전한 시작점이다.
- Factory 20개: `START`, `SOURCE`, `BUS`, `MASTER`, `MOTION` 5개 group.
- ID 1–20을 고정하고, bank 전체에서 Dynamic/Linear, Off/2X/4X/8X, Lookahead, Stereo/Mid/Side, INT/EXT, Compress/Expand active side와 positive/negative Range를 실제로 들려준다. `Transient Lift`와 `Parallel Drum Punch`의 positive bands는 threshold 위 transient를 올리는 Compress이고, `Keys Space`의 positive bands는 threshold 아래 detail을 올리는 Expand다. Factory 20개 detector mode는 모두 `BAND/FOLLOW`이며 detector center/width는 processing band와 일치한다.
- 모든 Factory preset은 global Output을 정확히 `0 dB`로 저장한다. 현재 editor에서 숨긴 compatibility gain이 preset recall로 음량을 바꾸지 않는 계약이다.
- 사용자 프리셋: Documents/TB_MultiBandCompressor/Presets/User, 확장자 `.tbmbc`.
- 프리셋은 모든 전역/밴드 값을 명시하고 compatibility detector Audition은 항상 Off로 저장한다. Delta Listen은 preset에 저장하지 않는다. Recall은 panel latch와 진행 중인 right-hold/pending restore를 모두 취소하고 복원 없이 Off로 만든다.
- 현재 canonical bank는 `Source/FactoryPresets.h`와 `PRESET_DESIGN.md`의 20개 table이다.

## 성능 예산

- 기준 머신: AMD Ryzen 7 260, Windows 11 Pro x64.
- 기준 조건: Shipping Release, 48 kHz / 64 samples / stereo, 1 instance.
- 목표: 기본 p99 < block deadline 10%, 최악 p99 < 20%, callback allocation/lock/I/O 0.
- 목표 메모리: instance steady < 32 MiB, editor 포함 peak < 64 MiB.
- GUI: active meter/analyzer 최대 30 Hz, FFT는 message thread, 값이 정지하면 반복 repaint 없음. Analyzer Off에서는 analyzer tap/FFT 요청 0, editor hidden/closed에서는 timer·demand·repaint 0.
- Current single-live-response DSP Engine/Harness direct run은 Debug/Release 각각 `58 checks, 0 failures`다. 48 kHz/64 samples clean-idle Release p50/p95/p99/max는 Default `12.10/17.90/19.60/37.30`, Worst Dynamic `17.30/25.50/34.00/218.90`, Linear+8× `291.60/475.60/699.10/1289.70`, 8 worst Dynamic instances `142.00/201.20/252.60/667.70 µs`이며 callback `new/delete`는 0이다. 네 측정군의 max도 `1333.33 µs` block deadline 안이다. Processor Debug/Release `11/11`, main Debug/Release rebuild·manifest, Release host contract와 Debug capture 10개도 final authored-tests revision에서 fresh PASS이며 source/test/artifact provenance는 `RELEASE_QA.md`에 고정했다.

## 검증 계획

| 항목 | 통과 기준 |
| --- | --- |
| Neutral null | 모든 band delta가 0, Mix 0%, bypass에서 sample error ≤ 1e-6 |
| Dynamics transfer | Compress=threshold 위, Expand=threshold 아래 활성 영역; `+Range`=양의 gain, `-Range`=음의 gain; Ratio, Knee와 signed Range cap이 수치와 일치 |
| Band locality | 중심 tone 반응이 인접/원거리 tone보다 유의하게 큼 |
| Sidechain / Channel | band별 INT/EXT 독립, `Sidechain 1–4` mono-default bus의 Band 1–4 고정 매핑, stereo `(L+R)/2` downmix, 모든 4개 key의 band-isolation·동시 ducking, 선택 bus 부재 fallback, 외부 신호 출력 누설 ≤ -120 dBFS, connected silence 존중, Stereo/Mid/Side 분리와 mono main Stereo fallback |
| Delta Listen | 선택 band wet-minus-dry identity, 20 ms exclusive transfer, panel latch, right-button hold 중 같은 band Frequency 이동·Range 불변·release와 pre-hold latch 복원, 외부 reset 후 미복원, Mix 독립, Band/global Output·final Bypass, EXT non-leak, M/S·mono fallback, legacy Audition precedence와 lifecycle reset |
| Timing / Latency | Attack/Release step response, 0/10/20 ms lookahead, Linear/2×/4×/8× 실제 지연과 host reported latency가 일치 |
| Quality | Dynamic/Linear neutral null, Linear impulse 대칭, Off/2×/4×/8× 실제 경로, 품질 전환 finite/click bound |
| Stability | 44.1/48/96/192 kHz, mono/stereo, 0/1/3/63/65/257/oversize block에서 finite |
| State/preset | 78-value v4 round-trip, exact v3/v2 78 identity와 v1 71 migration, legacy Range validation 뒤 `-48…+12 dB` clamp (`+24→+12`), 20 Factory full-state/Output 0, one-step Undo/Redo, invalid/future atomic rejection |
| UI | Fresh 85/100/150% 포함 Debug component capture 10개에서 잘림/겹침 없음; response plot의 top/zero/bottom은 `38/188/338`, 왼쪽 눈금은 `+12/+6/0/-24/-48 dB`; INPUT spectrum은 독립 `0…-96 dBFS` 좌표와 상단 `0 dBFS` caption 사용; Range 최대치는 node Y로 표시; `#817C66`, alpha `1.0`, `3.0 px` actual line 하나가 idle/outside-band에서 0 dB datum이고 active 구간만 위·아래로 변형; 정확한 `Multi Band Compressor`와 M/B/C 색; `LINK` custom UI·별도 datum·configured/red overlay·gain fill/shadow/gradient·오른쪽 response 숫자·숫자 debug·POST/PRE selector·output analyzer 부재; 8개 rotary의 `100 px` pitch·`16 px` gap·`20 px` margins, 순서/그룹색과 50 px renderer cap을 자동 확인해 PASS. 실제 Release Standalone native 화면의 fixed panel, full-height zones, SIDECHAIN Off/On과 Delta right-hold frequency edit·button latch·lifecycle은 수동 `HOLD` |

## 기획 변경 기록

| 날짜 | 변경 내용 | 변경 이유 | 영향 |
| --- | --- | --- | --- |
| 2026-08-11 | 최초 브리프. 4개의 자유 delta band와 공유 external bus + band별 INT/EXT 채택 | Pro-MB의 핵심 워크플로를 유지하면서 1차 구현과 QA 범위를 제한 | DSP, UI, 상태, 테스트 |
| 2026-08-12 | 초기 71-parameter/schema 1 DSP·상태·프리셋·UI와 Debug/Release/VST3 baseline evidence 작성 | 초기 구현 결과와 고정 CID를 브리프에 반영 | 현재 78-parameter revision에서는 stale baseline |
| 2026-08-12 | 4색 band Range, 강한 0 dB 중앙선, smoothing analyzer, floating/details 편집기와 Stereo/Mid/Side, Dynamic/Linear, Off/2×/4×, 0–20 ms lookahead를 v2 계약에 추가 | 당시 analyzer 중심 편집과 품질 선택 요청 반영 | v3에서 UI/8X/state 계약이 대체한 역사적 baseline |
| 2026-08-12 | v2 Debug/Release, DSP 40/40, processor 11/11, Release VST3 78-parameter host contract와 automated UI captures 재검증 | 당시 source와 산출물의 engineering baseline 고정 | v3에서는 STALE; release 판정에 합산하지 않음 |
| 2026-08-12 | v2 Factory ID·이름·그룹 12개를 Linear, 2X/4X, Lookahead, Mid/Side와 EXT/FREE 용도로 큐레이션 | 당시 주요 기능을 preset 탐색으로 확인 | v3의 20개 bank로 대체된 역사적 baseline |
| 2026-08-12 | Oversampling choice 끝에 8X를 append하고 state/preset v3, exact v2 78 identity와 v1 71 migration을 구현 | 기존 78 parameter identity를 유지하면서 실제 고배율 path와 호환 migration 제공 | 당시 DSP 40/40, Processor Debug/Release 11/11, Release VST3 host contract PASS; 이후 Delta/UI source에는 build/host evidence STALE |
| 2026-08-12 | Factory bank를 START/SOURCE/BUS/MASTER/MOTION 20개로 확장하고 전 preset Output 0을 고정 | source/bus/master/motion 탐색 확대와 hidden compatibility gain의 안전 recall | v3 full-state bank와 20개 recall/audio smoke PASS |
| 2026-08-12 | Analyzer를 `976×499`, fixed `824×120` panel, full-height colour zones, neutral shared response, FREE bracket direct edit와 43 px strip으로 재구성 | Analyzer Hero와 direct edit를 강화하고 pointer/Details/global Output UI를 제거 | 이후 fixed 0 dB/actual-only·hidden detector UI revision으로 대체된 역사적 baseline |
| 2026-08-12 | SIDECHAIN 고정 표기, detector/FREE·기존 Audition UI 숨김, fixed 0 dB+actual-only graph와 transient exclusive Delta Listen 구현 | 78 ID/v3 raw round-trip을 유지하면서 band monitoring과 graph 의미를 단순화; raw FREE는 inert/FOLLOW로 변경되어 이전 FREE 음향은 미보존 | DSP Debug/Release 50/50, Processor Debug/Release 11/11, main Debug/Release build, Release VST3 host contract, current UI 10개 automated PASS; native/listening/DAW/license는 HOLD |
| 2026-08-12 | Node 우클릭 Delta를 right-button hold 동안의 momentary override로 확정하고 panel `DELTA` click latch와 분리; 정상 release는 pre-hold latch 복원, lifecycle/external reset은 복원 없이 Off | hold-to-audition과 deliberate latch를 동시에 제공하고 preset/state/hide 뒤 낡은 monitor 상태가 되살아나는 것을 방지 | 앞 행의 우클릭 same-Off toggle 설명을 대체; 78 ID/v3 및 DSP 수식 불변 |
| 2026-08-12 | Graph hit arbitration에서 node 중심 `≤12 design px`를 edge보다 우선하고 disabled ghost를 saved Range Y에 표시; 양방향 ±24 dB·frequency invariant·각 one Undo 회귀를 고정 | node/edge overlap 때문에 positive Range drag가 bandwidth edit로 오인되던 경로를 제거 | parameter/state 불변; Processor Integration interaction 계약 강화 |
| 2026-08-12 | 기존 고채도 vermilion/blue/orchid/mint/amber UI를 charcoal·Signal Red·Slate Blue·Console Green·Calibration Ivory의 저채도 palette로 교체하고, generated neutral bakelite raster body 위 procedural tick/pointer/value layer를 적용 | 제공된 vintage studio hardware의 촉각적 밀도와 노브 판독성을 반영하되 특정 제품의 trade dress를 복제하지 않음 | DSP·78 ID·v3 불변; 기존 주황 concept과 vector-only 노브 계약을 supersede |
| 2026-08-12 | PRE/POST source selector와 처리 결과 spectrum을 제거하고 engine 직전 INPUT spectrum 하나와 ANALYZER toggle만 유지 | 감쇄·증폭은 fixed 0 dB 기준의 Signal Red actual contour가 이미 보여 주므로 원본 신호 판독을 방해하는 중복 overlay를 제거 | output analyzer FIFO/API·audio-thread POST tap 제거; current DSP Debug/Release `50/50`, Processor Debug/Release `11/11`, main Debug/Release rebuild·manifest, Release host contract와 Debug capture 10개 automated PASS; 수동 native/listening/DAW/license는 HOLD |
| 2026-08-12 | Analyzer visible 축을 `+24/0/-24 dB` 단일 좌표계로 통합하고 plot을 `{42,38,892,300}`, 0 dB를 `y=188`로 확정; INPUT spectrum/node/actual이 같은 `dbToY`를 사용하도록 정리 | 별도 `-90…0 dBFS` 축과 중앙선 좌표 불일치를 제거해 spectrum과 signed 변화량을 한 눈금에서 비교 | 9개 rotary도 `{10+90i,40,84,76}`, 90 px pitch/6 px gap/10 px margins, 50 px visible cap으로 고정; 최신 10-capture 및 integration에서 자동 확인 |
| 2026-08-12 | final pointer-free ImageGen Bakelite raster와 procedural Aged Brass marker, 기능별 cap tint를 적용 | 작은 85% UI에서도 촉각적 body와 값 방향을 분리해 판독하고 generated pointer 중복을 제거 | runtime PNG `271,104` bytes / SHA-256 `13AA4B40…A88EEE`; chroma master SHA-256 `927A66AE…E8754`; 배포 전 similarity/ImageGen rights는 수동 HOLD |
| 2026-08-12 | Range 부호를 실제 gain 부호로 고정하고, Analyzer 왼쪽 축/INPUT label/no-debug 계약과 8-control grouped panel, right-hold frequency edit를 확정 | Range `+`가 감쇄로 보이던 의미 충돌, source 식별 부재와 Band Out/노브 위계 혼잡을 제거 | 78 ID·v3·Band Output DSP/state는 보존; custom editor surface와 dynamics transfer 의미 갱신 |
| 2026-08-12 | Factory ID 12 `Transient Lift`와 ID 20 `Parallel Drum Punch`의 positive band를 explicit Compress로 조정하고 ID 19 `Keys Space`는 Expand로 유지 | above-threshold transient lift와 below-threshold detail lift가 새 mode/Range 의미에 맞게 구분됨 | Factory ID/name/group 유지; ID 12/20의 음악적 mode 값만 development candidate에서 갱신 |
| 2026-08-12 | Range domain을 `-48…+12 dB`, state/preset을 v4로 올리고 exact v3/v2/v1 migration에서 legacy `-24…+24 dB` Range를 검증 후 clamp하도록 변경 | mastering 감쇄 여유를 넓히면서 응답 축의 `+12/0/-48 dB` 판독과 실제 parameter endpoint를 일치 | parameter count/ID/order 78 유지; 기존 `+24 dB`는 `+12 dB`, 나머지 legacy 유효값은 새 범위 안에서 보존 |
| 2026-08-12 | 고정 0 dB datum, configured Range contour와 thinner actual DSP-gain gradient contour를 분리하고 INPUT spectrum을 독립 `0…-96 dBFS` 축으로 전환 | 최대 증폭·감쇄와 현재 동작량을 같은 중심선에서 구분하면서 원본 level은 response scale에 왜곡되지 않게 표시 | plot top/zero/bottom `38/188/338`과 150 px 양 half 유지; 이전 shared ±24 axis/actual-only 행을 supersede |
| 2026-08-13 | 별도 datum/configured/red overlay와 gain fill/shadow/gradient를 제거하고 `#817C66`, alpha `1.0`, `3.0 px`의 단일 actual DSP line이 idle에서는 0 dB datum, active에서는 위·아래로 변형되도록 통합. Range max는 node Y만 사용하고 LINK custom UI를 숨겼으며 header를 `Multi Band Compressor`와 M/B/C 색으로 교정 | response가 두 선이나 채운 영역으로 갈라져 보이지 않고 실제 gain 움직임 하나로 읽히게 함 | Stereo Link host/state/DSP와 78/v4 계약은 유지. 직전 build/test/host/capture provenance는 `STALE`, fresh evidence는 `PENDING` |
| 2026-08-13 | 위 single-live-response source로 DSP Debug/Release `58/58`, Processor Debug/Release `11/11`, main Debug/Release Standalone+VST3 full rebuild·manifest, Release host contract와 Debug UI capture 10개를 fresh 재검증 | 최종 UI·DSP·state·host 계약을 동일 source/test/artifact provenance에 고정 | automated engineering `PASS`; source+tests manifest와 artifact table은 `RELEASE_QA.md`, native/listening/DAW/longevity/license/signing/installer는 계속 `RELEASE HOLD` |
| 2026-10-02 | Sidechain을 DAW에 보이는 `Sidechain 1–4` 네 개의 optional mono-default bus로 분리하고 각각 Band 1–4에 고정 매핑. Stereo 연결은 DSP에서 `(L+R)/2`로 downmix하며 선택 bus 부재 시 해당 band만 internal fallback | 네 개의 Send/Aux 선택지에서 밴드별 더킹 소스를 직접 구성하고, APVTS parameter·CID·state/preset identity는 유지 | 4-bus isolation·동시 ducking·missing-bus fallback·external non-leak·실제 DAW Send 표시/라우팅을 새 host probe에서 확인해야 하며 기존 단일 bus 세션은 Send/Aux 재연결이 필요할 수 있음 |
