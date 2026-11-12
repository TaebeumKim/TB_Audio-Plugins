# TB_MultiBandCompressor UI/UX 브리프

작성일: 2026-08-12  
현재 계약: 78 parameters / state schema 4 / preset format 4 / `RELEASE HOLD`

## 목표

- 가장 큰 조작 영역은 engine 직전의 실제 INPUT spectrum, 밴드 범위와 signed dynamics를 한 canvas에 겹쳐 보여 주는 Analyzer Hero다.
- 사용자는 graph에서 band node를 직접 움직이고 full-height edge로 width를 바꾸며, enabled node를 right-button으로 누르는 동안 그 band의 Delta Listen을 momentary audition하고 수평 drag로 같은 band의 frequency를 옮긴다.
- TB_Compressor의 어두운 recessed hardware, compact control density와 signal-trace 위계를 계승하되 화면·예약색·부품 배치를 복제하지 않는다.
- Pro-MB의 자유 band와 band별 sidechain workflow를 목표로 삼되 FabFilter의 화면, 자산, 색, 상표 또는 비공개 DSP를 픽셀 단위로 복제하지 않는다.
- APVTS는 기존 71개 prefix와 신규 7개를 유지한 78개다. Parameter count/ID/order와 `oversampling`의 OFF/2X/4X/8X choice는 유지하고 Range domain을 `-48…+12 dB`, state/preset을 v4로 올렸다. UI-local Analyzer toggle과 transient Delta Listen은 host parameter/state에 추가하지 않는다.

## 확인한 기준 화면

- 직접 실행·확인: `E:/Plugin Projects/TB_Compressor/TB_Compressor/TB_Compressor Mockup (standalone).html`.
- 참고 요소: 한 줄 header, 깊은 bezel, 명료한 signal trace, dotted knob arc와 compact control density.
- 제외 요소: 고정 crossover식 compressor 구조, transfer curve plate의 그대로 복제, TB_Compressor 예약색 `#E18476`/`#72B9D4`의 재사용.
- TB_EQ 코드 참고: `E:/Plugin Projects/TB_EQ/TB_EQ/Source/SpectrumAnalyzer.{h,cpp}`의 fixed FIFO/GUI FFT와 `PluginEditor.cpp`의 node select/drag/wheel 및 analyzer lifecycle.
- 제외 요소: TB_EQ 전체 SpectrumDisplayComponent, Freeze/tilt/speed/range controls, frame마다 vector를 만드는 path builder, prepared block보다 큰 입력에 안전하지 않은 scratch pattern.

## 정보 위계

`단일 live DSP-gain response/datum + Range node + 독립 dBFS INPUT spectrum + band edit > 선택 band 핵심값 > sidechain/Delta/channel > global quality > preset`.

- Primary: Analyzer Hero, Frequency, `WIDTH`, Threshold, Range, Mix.
- Standard: Ratio, Knee, Attack, Release, Compress/Expand, `SIDECHAIN` Off/On, Stereo/Mid/Side, Dynamic/Linear, Lookahead.
- Supporting: Delta Listen, Band On, Oversampling, actual latency, Bypass, preset, Undo/Redo.
- Compatibility-only: global Output, Band Output, Stereo Link, `band*_detector_mode`, detector frequency/bandwidth와 기존 detector Audition parameter. Band Output과 Stereo Link는 기존 host parameter/state/DSP 의미를 보존하지만 custom editor control, label, bounds와 hit target은 숨긴다. Detector mode/range는 raw host value/state round-trip만 보존하고 effective DSP는 FOLLOW로 고정한다. Raw FREE/range는 inert하며 이전 FREE 음향은 보존하지 않고, detector Audition control/function은 유지하지만 current effective FOLLOW detector를 듣는다. 현재 editor에는 이 control이나 label을 표시하지 않는다.
- Internal: FFT window, FIFO depth, smoothing coefficient, filter coefficient cadence, safety gain clamp.

## 정확한 레이아웃

- Editor: `1000 × 620 px`, 고정 종횡비, `850 × 527`–`1500 × 930` resize.
- Header: `[0,0,1000,42]`. 좌측 표시명은 정확히 `Multi Band Compressor`이며 `M/B/C`는 각각 Slate Blue `#7088B2`, Control Green `#4F805F`, Signal Red `#C65350`, 나머지 문자는 `#D9DAE1`이다. 중앙에는 preset navigation/save/undo/redo, 우측에는 canonical `48 × 48 px` signature를 두고 양끝에는 rack screw를 둔다. 내부 프로젝트·번들명 `TB_MultiBandCompressor`는 바꾸지 않는다.
- Analyzer Hero component: `[12,54,976,499]`.
- Analyzer visible plot: component-local `{42,38,892,300}`. 물리 top/zero/bottom `y=38/188/338`과 위·아래 각각 `150 px` 폭은 유지한다. Response Y축은 `+12 / +6 / 0 / -24 / -48 dB`이고 각각 `y=38 / 113 / 188 / 263 / 338`에 놓는다. Mapping은 `db≥0`에서 `y=188-(db/12)×150`, `db<0`에서 `y=188+(-db/48)×150`이며 endpoint 밖은 clamp한다. 눈금 글자는 plot 왼쪽에만 두고 오른쪽에는 response 숫자를 그리지 않는다. INPUT spectrum은 이 response 좌표를 사용하지 않고 `y=38+(-clamp(db,-96,0)/96)×300`으로 독립 매핑하며 상단에 `0 dBFS` caption을 둔다. 상단 좌측에는 고정 `INPUT` source label `[42,6,64,24]`, 우측에는 기본 On인 단일 `[ANALYZER]` toggle `[884,6,82,24]`를 둔다. 밴드/actual/input의 숫자 debug readout과 PRE/POST source selector는 두지 않는다.
- Selected-band panel: editor 좌표 `{88,400,824,120}`에 고정한다. node 위치에 따라 이동하지 않고 pointer도 그리지 않는다.
- Panel compact row는 local `y=5`, 높이 `22`: Band 1–4 selector, Band On, Compress/Expand, `SIDECHAIN`, `DELTA`, Stereo/Mid/Side 순이다. Band On은 `[158,5,72,22]`, Compress/Expand는 `[252,5,96,22]`, `SIDECHAIN`은 `[370,5,132,22]`, `DELTA`는 `[524,5,82,22]`, Stereo/Mid/Side는 `[628,5,184,22]`다. Stereo Link는 parameter/attachment와 DSP에는 남지만 visible child나 hit target으로 만들지 않고 bounds도 주지 않는다.
- 8개 main slider는 local `{20+100i,40,84,76}`: Frequency, `WIDTH`, Threshold, Range, Ratio, Knee, Attack, Release 순이다. Threshold–Range–Ratio–Knee는 서로 인접한 한 그룹이며 같은 Console Green cap tint를 사용한다. centre pitch `100 px`, bounds gap `16 px`, 좌우 margin `20 px`이며 raster renderer는 100%에서 visible body를 최대 `50 px`로 제한한다. Band Output parameter와 DSP는 호환성을 위해 남기되 custom editor knob는 표시하지 않는다.
- 별도 `DETAILS` button/shelf, detector mode button, Detector Frequency/Width slider와 FREE bracket은 없다. 기존 detector parameter는 generic host/state compatibility surface로만 남는다.
- Bottom strip: `[12,565,976,43]`. Bypass `[24,578,72,22]`, Dynamic/Linear `[108,578,78,22]`와 `[188,578,78,22]`, OFF/2X/4X/8X는 `x=280+47i`, `45×22`, Lookahead `[480,578,200,22]`, horizontal Mix `[700,578,268,22]`다.
- 모든 하단 button은 compact `22 px` 높이를 유지한다. global Output UI는 숨기고 남는 폭을 Mix에 사용한다.

## Analyzer와 직접 편집

- 표시 spectrum은 engine 직전 main input 하나다. Mix와 compatibility Output gain이 반영된 처리 결과에는 analyzer tap/FIFO를 두지 않으며 external sidechain bus도 spectrum에 섞지 않는다.
- spectrum trace는 실제 audio data만 사용한다. 데이터가 없으면 floor 상태이며 fake motion이나 synthetic shape를 그리지 않는다.
- INPUT spectrum은 상단 좌측의 고정 `INPUT` label, 상단 `0 dBFS` caption, 낮은 alpha fill과 얇은 Slate Blue line으로 표시한다. FFT 표시값은 signed response 값을 무시하고 `0…-96 dBFS`를 plot 전체 높이에 독립 매핑한다. Band node와 단일 live DSP-gain response는 spectrum보다 높은 계층이어야 한다.
- spectrum temporal smoothing은 target 상승 `0.78`, 하강 `0.24`다. log-frequency 512-point display path에는 전방·후방 2-pass 공간 smoothing `0.42 current + 0.58 neighbor`를 적용한다. 이 smoothing은 표시 전용이다.
- response는 plot 전폭을 잇는 dark pale-yellow `#817C66`, alpha `1.0`, `3.0 px` rounded/curved stroke 하나다. gain이 없거나 active band 밖인 구간은 물리적 중앙 `y=188`의 0 dB를 따라 이 선 자체가 datum 역할을 하고, 실제 DSP gain이 발생한 구간만 같은 연속선이 위·아래로 변형된다. Response의 0 dB는 INPUT dBFS의 0이 아니다.
- 각 enabled band의 frequency/width는 plot 전체 높이를 관통하는 고유색 양 edge와, edge에서 옅고 center에서 진한 수평 gradient로 나타낸다. 선택 band는 edge alpha/두께와 흰 node rim을 올린다.
- Range 최대 증폭·감쇄는 각 band node의 signed Y로만 표시한다. Disabled ghost도 저장된 Range Y를 유지하지만 configured Range contour나 envelope는 그리지 않는다.
- live response는 DSP의 signed per-band gain meter를 current frequency/width의 raised-cosine shape에 매핑해 합산한다. 양수 gain은 0 dB 위, 음수 gain은 아래로 같은 연속 path를 변형한다. 이 선은 실제 gain state의 시각화이며 측정 transfer curve나 output EQ curve라고 부르지 않는다.
- response path는 0 dB 기준으로 닫지 않는 stroke-only 표현이다. 별도 fixed datum, configured contour, 두 번째 actual overlay, gain 면적 fill, wash, shadow와 colour gradient를 모두 금지한다. 실제 gain이 0으로 돌아오면 해당 구간은 끊기거나 사라지지 않고 같은 선의 0 dB baseline으로 복귀한다.
- node drag: X=Frequency, Y=Range. Node 중심 `12 design px` 이내는 full-height edge보다 node hit를 우선한다. 수직 drag는 piecewise response 좌표의 `-48…+12 dB` 전체 Range를 오가면서 frequency를 보존한다.
- full-height band edge drag 또는 node 위 wheel: Bandwidth. 빈 공간 double-click은 첫 disabled slot의 값을 먼저 설정하고 Enable을 마지막에 켠다.
- Node right-button down은 enabled band에만 hit하고 누른 동안만 Delta를 momentary override한다. 누른 채 수평 drag하면 처음 누른 band의 Frequency만 이동하고 Range와 Delta 대상 band는 유지한다. mouse-up 또는 realtime right-button-loss에서 끝나며 정상 release는 pre-hold panel latch를 복원하고, hold 중 외부 reset이 일어났다면 복원하지 않는다. 빈 graph 우클릭은 no-op이다. Active node에는 비색상 ring을 더하되 별도 숫자 debug readout은 사용하지 않는다.
- Left click은 select만 수행한다. APVTS graph edit는 interaction당 Undo transaction 하나와 균형 잡힌 host gesture를 사용하고 mouse-up, hide와 editor destruction에서 열린 gesture를 종료한다. Delta 우클릭과 `DELTA` button은 transient monitor 조작이라 host gesture나 Undo transaction을 만들지 않는다.
- EXT가 선택됐지만 해당 `Sidechain N` aux input이 없을 때만 `NO SC INPUT`을 표시하고 DSP는 그 band만 internal detector로 deterministic fallback한다. `Sidechain 1–4`가 Band 1–4에 1:1로 매핑되며 stereo 연결은 DSP에서 `(L+R)/2`로 mono key에 downmix한다. 기존 단일 bus 세션 라우팅은 보장하지 않으며 업데이트 후 Send/Aux 재연결이 필요할 수 있다.

## Sidechain·channel 시맨틱

- Source button의 글자는 두 상태 모두 정확히 `SIDECHAIN`이다. 네 개의 optional host aux bus는 `Sidechain 1`–`Sidechain 4`라는 이름과 mono 기본 layout으로 노출되고 각각 Band 1–4에 고정 연결한다. 각 bus는 stereo 연결도 허용하며 DSP에서 `(L+R)/2`로 mono detector key에 downmix한다. 각 밴드는 자신의 `Sidechain N` key와 main input 중 하나를 독립 선택하고 Off=INT, On=EXT를 pressed fill·outline과 accessibility toggle state로 표시한다. 색만으로 상태를 구분하지 않는다.
- Detector mode/range와 기존 detector Audition은 custom editor에서 숨긴다. `BAND/FREE`와 독립 range는 host choice/raw value/state만 round-trip하고 effective DSP는 BAND/FOLLOW이며, 이전 FREE 음향은 보존하지 않는다. Detector Audition control/function은 유지하지만 current effective FOLLOW detector를 듣는다. Factory 20개는 모두 BAND/FOLLOW다.
- Channel은 `STEREO / MID / SIDE`. Mono main에서는 raw 선택값을 보존하고 `MONO > STEREO` effective fallback을 함께 표시한다.
- External key는 Stereo detector에서는 mapped mono channel을 양쪽 입력에 복제하고 Mid/Side detector에서는 선택 domain의 key로 사용하므로 모든 Quad processing mode에서 유효하다. 연결된 mono key나 silence를 `NO SC INPUT`으로 오인하지 않는다.
- Stereo Link는 Stereo detector에서 unlinked level과 channel max를 보간하는 기존 host parameter/state/DSP 계약을 유지한다. Custom editor에서는 `LINK` 글자, slider, bounds와 hit target을 모두 숨긴다.
- `DELTA/DELTA ON` click은 선택 band를 latch하고 같은 latched band click은 Off, 다른 band click은 latch transfer다. Node right-hold는 이 latch의 임시 override다. 모든 Off/Band transfer는 20 ms이며 Mix와 무관하고 Band/global Output을 적용한 wet-minus-dry contribution을 들려준다. Final Bypass가 우선한다.

## 시맨틱 팔레트

| token | HEX | 역할 |
|---|---:|---|
| canvas-top / bottom | `#25262D` / `#111218` | charcoal rack chassis |
| surface-top / bottom | `#2B2C34` / `#18191F` | recessed panel |
| display-top / bottom | `#12151C` / `#080A0E` | analyzer outer glass |
| graph-top / bottom | `#20242C` / `#141820` | plot 내부의 더 옅은 depth layer |
| text-primary / secondary | `#D9DAE1` / `#A6A7B0` | label·status |
| value-text | `#E6E4DB` | 수치와 선택 node의 교정색 |
| accent-primary | `#C65350` | Delta/Bypass action |
| accent-secondary | `#7088B2` | Slate Blue input spectrum·sidechain·analyzer action |
| control-active | `#4F805F` | 일반 On 상태 |
| band-1 | `#A94B49` | Oxide Red Band 1 zone/node |
| band-2 | `#3F7558` | Console Green Band 2 zone/node |
| band-3 | `#4D6FA8` | Enamel Blue Band 3 zone/node |
| band-4 / calibration-ivory | `#C7C1AD` | Band 4 zone/node·비활성 tick·노브 pointer |
| aged-brass | `#AA985D` | 현재값까지의 저채도 knob tick과 procedural value marker |
| live-gain-response / datum | `#817C66` | alpha `1.0`, `3.0 px`의 단일 continuous actual DSP stroke. idle/outside-band에서는 0 dB baseline이고 active 구간만 위·아래로 변형 |
| header M / B / C | `#7088B2` / `#4F805F` / `#C65350` | `Multi Band Compressor`의 세 머리글자. 나머지 문자는 `#D9DAE1` |
| outline / deep-shadow / highlight | `#4A4C56` / `#06070A` / `#7A7B86` | structure·depth |

- 네 band colour는 full-height zone의 낮은 alpha gradient, edge와 node에 제한한다. response line과 text 판독을 침범하지 않는다.
- Signal Red/Slate Blue는 분석과 상태 역할에 제한하고, Aged Brass는 노브 눈금의 작은 교정 표식에만 쓴다. 주황색을 제품 concept accent로 사용하지 않는다.
- TB_Compressor식 recessed hardware와 눈금 위계만 차용하고 이 제품의 저채도 palette와 부품 비례를 사용한다.

## Raster 노브와 동적 레이어

- rotary body는 BinaryData에 포함한 `Source/Assets/tb_knob_bakelite.png` 512×512 RGBA를 사용한다. broad black Bakelite grip과 large cool-ivory cap의 재질 정보만 raster이며 image에는 pointer/index mark가 없다. image 자체를 parameter angle에 맞춰 회전하지 않고 renderer는 100%에서 visible diameter를 `50 px`로 cap한다.
- cap tint mapping은 Frequency/Width=`Enamel Blue`, Threshold/Range/Ratio/Knee=`Console Green`, Attack/Release=`Oxide Red`다. enabled alpha `0.62`, disabled alpha `0.12`이며 정확한 값은 text와 procedural marker로 읽는다.
- Aged Brass tick progression과 Aged Brass value marker는 JUCE procedural drawing이다. 따라서 조명과 재질 결은 고정되고 marker/value만 회전해 값 변화가 흐려지지 않는다.
- 원본은 built-in ImageGen으로 새로 생성한 단일 knob이며 제공된 vintage compressor 이미지는 어두운 studio hardware의 재질·배색 방향에만 사용했다. 참조 이미지의 로고, 문자, meter, panel layout, control arrangement 또는 픽셀을 포함하지 않는다.

## 시각·데이터 lifecycle

- audio callback은 demand atomic을 확인한 뒤 fixed FIFO에 main bus sample만 기록한다. allocation, lock, I/O, FFT는 하지 않는다.
- FFT 2048/Hann은 editor message thread에서 실행하며 fixed arrays를 재사용한다.
- Analyzer가 On이고 editor가 실제 showing일 때만 input analyzer demand를 올린다.
- Analyzer Off에서는 sample tap/FFT/repaint를 요청하지 않는다. GR/detector/parameter repaint는 독립 기능이다.
- editor hidden/closed에서는 analyzer demand 0, timer 0, repaint 0, 열린 gesture 0이다. 다시 보이면 30 Hz active timer를 재개한다.
- Delta Listen은 editor hide/close/destruction, prepare/reset/release, 성공한 state load와 Factory/User preset recall에서 active hold와 pending latch restore까지 취소하고 복원 없이 Off로 reset한다. 저장·자동화·Undo에는 포함하지 않는다.
- actual gain/detector 표시는 DSP envelope atomics를 그대로 따라가며 별도 GUI meter smoothing을 덧씌우지 않는다. Spectrum만 위 smoothing을 적용하고 새 frame일 때 path/cache를 갱신한다.
- offline render에서는 analyzer producer를 사용하지 않는다. DSP 결과에는 영향이 없다.

## 상태와 접근성

- Processing `[DYNAMIC | LINEAR]`, Oversampling `[OFF | 2X | 4X | 8X]`, Lookahead `0–20 ms`; strip label에 host reported latency samples를 표시한다.
- latency-changing 세 control은 state/preset에 저장되지만 automatable이 아니다. Channel Mode와 sidechain 및 compatibility-only detector parameter는 automatable이다. Delta Listen은 parameter가 아니다.
- global Output과 Band Output은 78-parameter compatibility contract와 DSP에는 남아 있지만 현재 editor에서 숨긴다. 모든 Factory preset의 global Output은 `0 dB`라 숨은 전역 gain offset을 만들지 않는다.
- Band Off와 global Bypass는 영향을 받는 controls만 dim/disable하고 값을 보존한다.
- 단일 Analyzer toggle은 main input spectrum을 표시한다는 tooltip과 명시적인 On/Off 시각 상태를 가진다.
- 정확한 값은 slider text box에서 입력할 수 있고 graph edit는 같은 APVTS parameter/UndoManager를 사용한다.
- 오류는 짧고 구체적인 AlertWindow, 성공은 3.2 s 이내 toast로 표시한다.

## QA와 현재 판정

- 기존 schema 1/2/3, 12-preset, 구형 shared ±24 axis, 별도 fixed datum과 vermilion actual contour를 사용한 구형 actual-only response, configured Range/red actual의 이중 contour, `976×456`, moving pointer panel/Details shelf와 global Output UI를 보여 주는 capture는 현재 v4 계약의 통과 근거로 재사용하지 않는다.
- 최종 automated UI suite는 `1000×620`, 85/100/150%에서 `[12,54,976,499]` Hero, fixed `{88,400,824,120}` panel, 43 px strip과 no-crop을 증명한다. Single-live-response 변경 뒤 Debug capture 10개를 fresh 재생성했고 자동 검증은 `PASS`다. Negative/positive fixture 모두에서 움직이는 active-band 중심의 0 dB 띠에 `#817C66` pixel이 남으면 실패하며, baseline continuity는 active-band 밖에서 확인한다. 이전 Range-overlay capture는 `STALE`이다.
- 상태 capture에는 85/100/150%, disabled/unselected/4 band zones, negative actual이 0 dB 아래인 기본 상태, positive actual이 0 dB 위인 fixture, SIDECHAIN Off/On과 no-input, Delta Off/On, bypass, Dynamic/Linear, OFF/2X/4X/8X, lookahead와 Stereo/Mid/Side가 포함돼야 한다. 왼쪽 `+12/+6/0/-24/-48` response 눈금과 `38/188/338` top/zero/bottom, 독립 `0…-96 dBFS` INPUT mapping과 top `0 dBFS` caption, idle에서 0 dB datum이 되는 단일 `#817C66` 3 px live stroke, Range node Y, graph-only lighter background, 정확한 `Multi Band Compressor`와 M/B/C 색, `LINK` UI·별도 datum·configured/red overlay·gain fill/shadow/gradient·오른쪽 response 숫자·숫자 debug readout 부재를 확인한다.
- Component capture만으로 native hit-test를 대체하지 않는다. 실제 Release Standalone에서 node 중심 `≤12 px` 우선, edge/wheel, Range `-48…+12 dB`와 frequency invariant/one Undo, node right-hold 중 같은 band Frequency 이동·Range 불변·button-loss release·pre-hold latch restore·external-reset no-restore·empty no-op, `DELTA` button latch/sync, compact buttons, slider text entry와 preset/Undo/Redo를 수동 확인한다.
- analyzer demand Off/On/Off, editor hide/destroy/recreate, Delta lifecycle reset, DSP atomic meter, actual latency label과 no-sidechain/selected-bus fallback 상태를 검증한다. Four-bus fixture는 `Sidechain 1–4`가 각 Band 1–4만 구동하는 isolation과 네 key 동시 ducking, stereo `(L+R)/2` downmix를 포함한다. 기존 단일 bus 세션은 Send/Aux 재연결이 필요한지 확인한다.
- 현재 four-bus sidechain source의 Debug/Release Processor Integration `11/11`, main Debug/Release plugin full rebuild·manifest exit `0`와 Debug component capture 10개는 fresh `PASS`다. DSP Debug/Release도 `59/59`이며 exact source/test/artifact provenance는 `RELEASE_QA.md`에 고정했다. Four-bus 변경 후 Release VST3 host probe와 Release Standalone native 화면·오디오·hit-test는 미검증이므로 최종 판정은 `RELEASE HOLD`다.
