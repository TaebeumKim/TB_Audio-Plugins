# TB_Shimmer Product Brief

작성일: 2026-08-02  
상태: early+late diffuse swarm tail 및 독립 grain random controls 구현 기준  
프로젝트명 가정: 사용자가 별도 제품명을 말하지 않았으므로 현재 작업명과 저장소명인 `TB_Shimmer`를 적용한다.

## 1. 제품 기본 정보

```text
프로젝트명: TB_Shimmer
제품 표시명: TB_SHIMMER
한 문장 정의: 입력 오디오를 겹치는 시간영역 grain으로 재조합하고, pitch-shifted feedback과 early all-pass diffusion+late FDN을 통해 입자가 부드럽게 번지는 프리즘 공간 잔향을 만드는 granular shimmer effect.
플러그인 종류: Creative granular delay / pitch / spatial effect
아날로그 복각 여부: 아니오
복각 대상: 해당 없음
주요 대상 사용자: 프로듀서, 사운드 디자이너, 기타·신스·보컬 이펙트 사용자
주요 대상 소스: pad/keys, guitar, vocal, rhythmic loop
대표 사용 상황: 정적인 소스에 반짝이는 움직임과 상승하는 공간 꼬리를 추가하거나, reverse grain이 diffuse tail 안에서 bloom하는 입자 질감을 만드는 작업
```

상용 레퍼런스인 Output Portal, Soundtoys Crystallizer와 SpaceBlender는 공식 공개 자료에 나타난 청감 결과와 작업 흐름만 참고한다. UI, 컨트롤 체계, 파라미터 범위, 프리셋, 브랜드 자산과 비공개 구현은 복제하거나 추정하지 않는다. SpaceBlender 관련 근거는 [공식 제품 페이지](https://www.soundtoys.com/product/spaceBlender/)와 [공식 사용자 매뉴얼](https://www.soundtoys.com/wp-content/uploads/SpaceBlender-Manual.pdf)로 한정한다.

## 2. 해결하려는 음악적 문제

```text
사용자가 현재 겪는 문제: 일반 delay/reverb만으로는 음정이 올라가며 분해되는 반짝이는 질감과 입자 운동을 빠르게 만들기 어렵다.
기존 작업 방식의 불편함: pitch shifter, reverse delay, feedback, filter를 여러 플러그인으로 조합해야 하고 폭주와 레벨 관리가 번거롭다.
이 플러그인이 만드는 결과: 짧은 조각들이 원본 주위에 떠다니다가 early diffusion에서 먼저 흩어지고 독립적인 late tail을 excite하며, feedback 세대마다 음정이 상승하면서 개별 grain이 넓은 spatial wash로 이어지는 cloud.
처리 전 상태: 정적이거나 평면적인 단일 소스.
처리 후 기대 상태: 원본의 연주감을 남기면서 음정·시간·스테레오 위치가 분산된 반짝이는 입자와 부드럽게 진화하는 공간 꼬리.
```

## 3. 제품 가치와 차별점

```text
이 플러그인을 선택할 한 문장 이유: 복잡한 granular/reverb routing 없이도 +12 st feedback cloud와 부드러운 evolving tail을 안전한 시작점에서 바로 만들 수 있다.
일반적인 동종 처리와 다른 점: grain cloud의 직접 음정 이동과 피드백 세대의 누적 상승, reverse 확률, 독립적인 level·position·pitch random과 stereo scatter가 채널별 4-stage early diffuser를 거쳐 고유한 8-line late FDN을 excite한다. Diffuse 출력은 granular history로 되돌아가지 않는다.
기존 TB 플러그인과 겹치지 않는 점: TB_Delay의 범용 echo/tap 기능이 아니라 pitch-accumulating granular cloud와 그 뒤의 early+late diffuse tail에 집중한다.
이 플러그인만의 컬러링 축: grain 길이와 밀도가 만드는 smooth↔spark 질감, raw↔early-diffused grain과 direct↔late-tail 비율, pitch 누적, reverse 방향성, 독립적인 grain level·position·pitch 분산, scatter에 따른 size/pan 분산과 느린 late-tail motion.
의도적으로 하지 않을 것: 투명 보컬 보정, tempo sequencer, scale quantizer, 실재 room/plate 복각, convolution/IR, 독립 reverb UI, 다중 FX rack, 상용 레퍼런스 복제.
```

## 4. 청감 A/B 성공 기준

```text
A 상태: 호스트 bypass 또는 플러그인 Bypass.
B 상태: Init 기본값 및 대표 Factory preset.
비교할 대상 또는 바이패스: 동일 원본의 bypass.
음량 매칭 방법: dry와 wet 출력의 integrated RMS를 측정하고 wet 비교본만 ±0.1 dB 이내로 보정한다.
반드시 구분되어야 할 청감 차이: +12 st 입자가 원본 뒤로 피어나고, 반복 세대가 넓어진 뒤 개별 grain이 click 없이 diffuse late wash로 이어진다.
차이를 표현할 음악적 단어: shimmering, ascending, prismatic, drifting, diffuse, reverse bloom.
검증할 최소 소스 1: Guide MD/Assets/KEY_01_pad.wav — 지속음에서 pitch cloud와 tail의 주 타깃.
검증할 최소 소스 2: Guide MD/Assets/GTR_01_acoustic.wav — transient 보존과 grain splice 대조.
역방향 검증: Guide MD/Assets/DRM_01_loop.wav — 과도한 size/scatter가 리듬을 무너뜨리는 상한 확인.
실패로 판정할 상태: 밝기나 음량만 달라지고 pitch/grain movement가 식별되지 않음, tail의 고정 음정 ringing 또는 주기적 wobble, 저역 누적, click/pop, feedback 폭주, mono collapse, dry 0% 불일치.
```

## 5. 기능 범위

### 필수 기능

- 실시간 stereo/mono circular history와 고정 크기 grain pool.
- Hann-windowed granular playback, fractional interpolation, pitch, size, density, delay position, reverse 확률과 독립적인 per-grain level·position·pitch random.
- Grain wet을 입력으로 받는 채널별 fixed-memory 4-stage Schroeder all-pass early diffuser와 8-line orthogonal late FDN. IR, runtime allocation과 별도 host parameter는 사용하지 않는다.
- tone-filtered and softly bounded granular/tail feedback, stereo width, linear Dry/Wet, click-free host bypass.
- APVTS automation, DAW state, Undo/Redo, Factory/User preset.
- Debug/Release Harness, Standalone, VST3 및 실제 Release contract snapshot.

### 있으면 좋은 기능

- 없음. 필수 범위와 검증이 끝나기 전 추가하지 않는다.

### 제외 범위

- tempo sync, MIDI note/scale quantization, transient detector, spectral phase vocoder, formant preservation.
- modulation sequencer, XY macro assignment, 독립 reverb parameter block, compressor/multi-FX.
- Aether AI profile, MCP, LLM, network 기능.
- Output Portal/Soundtoys Crystallizer/SpaceBlender의 UI·프리셋·컨트롤 체계·비공개 구현 복제.

## 6. 컨트롤 계획

| 컨트롤 | Source Parameter ID | 사용자에게 들리는 역할 | 범위·단위 | 기본값 | 외부 제어 | 위계 | 필수 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Time | `time` | grain이 읽는 과거 위치 | 60–1200 ms | 240 ms | Host | Standard | 예 |
| Size | `size` | 각 grain의 길이, early-diffusion blend와 late-tail delay scale | 1–500 ms | 160 ms | Host | Standard | 예 |
| Density | `density` | 초당 grain 수, early-diffusion blend와 30–80% late-tail 가중치 | 2–2000 gr/s | 9 gr/s | Host | Standard | 예 |
| Scatter | `scatter` | grain size·stereo pan과 deterministic tail motion의 분산 | 0–100 % | 32 % | Host | Standard | 예 |
| Pitch | `pitch` | grain playback 음정(연속, 1 cent 해상도) | -12–+24 st | +12 st | Host | Primary | 예 |
| Reverse | `reverse` | grain이 역방향으로 재생될 확률 | 0–100 % | 18 % | Host | Standard | 예 |
| Feedback | `feedback` | pitch-shifted grain 반복과 독립 tail T60, 최대 45 s | 0–88 % | 42 % | Host | Primary | 예 |
| Tone | `tone` | granular wet의 밝기 상한과 tail loop damping | 1200–18000 Hz | 9000 Hz | Host | Standard | 예 |
| Width | `width` | grain+tail wet의 M/S stereo 폭 | 0–200 % | 135 % | Host | Standard | 예 |
| Freeze | `freeze` | history 기록을 멈춰 포착된 구간을 grain이 계속 재생 | Off/On | Off | Host | Standard | 예 |
| Sparkle | `sparkle` | 배음으로 올린 짧은 shard 레이어의 양(레벨+밀도). Time 간격 핑퐁 에코 포함, 0%면 레이어 off | 0–100 % | 0 % | Host | Primary | 예 |
| Shatter | `shatter` | shard를 고른 drizzle에서 attack에 맞춰 터지고 성기어지는 burst로 전환 | 0–100 % | 0 % | Host | Standard | 예 |
| Mix | `mix` | dry와 granular+tail wet의 비율 | 0–100 % | 48 % | Host | Primary | 예 |
| Bypass | `bypass` | VST3 effect bypass | Off/On | Off | Host | Supporting/host | 예 |
| Level Var | `level_var` | grain마다 0 dB부터 설정값까지 독립적으로 감쇠하는 레벨 편차 | 0–12 dB | 0 dB | Host | Random | 예 |
| Position Rand | `position_rand` | grain마다 history read position을 앞뒤로 분산 | 0–100 % | 32 % | Host | Random | 예 |
| Pitch Spray | `pitch_spray` | grain마다 기준 pitch 주위의 연속 bipolar 음정 분산 | 0–12 st | 0.112 st | Host | Random | 예 |

기본값은 pad와 guitar에서 효과가 바로 들리되 dry attack과 headroom을 유지하는 시작점이다. 완전 무처리 비교는 Bypass를 사용한다.

## 7. 신호 구조와 기술 가정

```text
입력 형식: mono 또는 stereo float audio.
출력 형식: 입력과 같은 mono 또는 stereo float audio.
신호 처리 순서: Input + bounded filtered Granular Feedback → 8 s Circular History → fixed-density-interval Hann Grains/Pitch/Reverse/Level·Position·Pitch Random/Scatter Size·Pan → hybrid OLA normalisation → HP/Tone → {pre-diffuser Granular History Return, channel-wise 4-stage Schroeder All-pass} → energy-normalised Raw↔Early-diffused blend → {Direct Wet, 8-line Orthogonal Late FDN Input} → Density-weighted energy-normalised Early+Late Sum → M/S Width → Linear Mix → click-free Bypass crossfade. Early/late diffuse 출력은 granular history에 재주입하지 않는다.
Dry/Wet 또는 병렬 구조: 병렬 linear mix; Mix 0%는 sample-exact dry, Mix 100%는 wet only.
지원 채널 구성: mono→mono, stereo→stereo.
예상 레이턴시: 0 samples. Time은 창의적 look-back delay이며 host latency가 아니다.
레이턴시 허용 범위: 0 samples 고정.
예상 CPU 등급: Medium. 최대 64 active grains, 채널별 4 early all-pass stages와 8 late FDN lines, sample당 고정 상한.
오버샘플링 필요 여부: 없음.
외부 파일·IR·모델 의존성: 없음.
제3자 라이브러리와 라이선스: JUCE만 사용; DSP와 UI 자산은 프로젝트 자체 작성.
Tail: 독립 diffuse loop의 최대 목표 T60은 45 s이며, feedback 88%와 grain history를 함께 고려해 host에는 보수적 180 s를 계속 보고한다.
Early diffusion: L `3.1/5.3/9.1/15.7 ms`, R `3.7/6.2/10.7/16.9 ms` 목표를 sample rate별 nearest-prime sample 수로 할당하고 coefficient `0.57–0.72`를 사용한다. Size+Density가 정한 `24–58%` blend는 80 ms smoothing하며 raw/diffused 합은 energy-normalised다.
Randomness: Density가 정한 고정 sample interval로 grain을 spawn한다. 각 grain은 Level Var, Position Rand, Pitch Spray에 서로 다른 random draw를 사용하고, Scatter는 size/pan 및 diffuse-tail motion만 제어한다. grain과 각 diffuse line은 고정 PRNG seed와 reset 규칙을 사용하므로 cold render가 결정적이며 사용자 선택 seed는 없다.
```

### 7.1 외부 호스트 계약

```text
외부 호스트 제어 대상 여부: 예, 표준 VST3.
Aether AI 제어 대상 여부: 아니오(이번 요청에서 지정되지 않음).
Public parameter 계약: 총 14개. 기존 11 ID와 순서를 보존하고 `level_var`, `position_rand`, `pitch_spray`를 마지막에 append한다.
Project ID: TbShm1
Plugin Manufacturer Code / Plugin Code: TbIi / TbSh
Bundle Identifier: com.TeamImpulseImpact.TB_Shimmer
VST3 Processor CID: Release 스캔 후 확정.
VST3 Controller CID: Release 스캔 후 확정.
상태 스키마 버전: 3
프리셋 포맷 버전: 3
호스트 프로필 계약 버전: 해당 없음.
호스트 프로필 소스 경로: 해당 없음.
허용 제어 표면: Preset / Raw Parameter.
AI 허용 프리셋·매크로: 해당 없음.
AI 쓰기 금지 파라미터: 전체(프로필 없음).
파라미터별 안전 범위: 위 컨트롤 범위와 동일.
레이턴시·tail·오프라인 렌더 조건: 0 samples / 180 s / cold-start deterministic.
이전 상태·프리셋 호환 정책: 유효한 schema/preset 1은 Level Var=0, Position Rand=기존 Scatter, Pitch Spray=기존 Scatter×0.0035로 schema/preset 2에 이관한다. Factory 100개 확장은 parameter/state 구조를 바꾸지 않으므로 schema/preset 2/2를 유지하며 malformed/future 데이터는 원자적으로 거부한다.
```

## 8. UX와 시각 방향

```text
사용자의 첫 번째 행동: 한 패널에서 Pitch와 Feedback으로 상승 높이와 꼬리를 정한 뒤 Size/Density로 grain 길이와 early/late diffuse 비율을 맞춘다.
가장 자주 사용하는 작업 흐름: Pitch/Feedback → Size/Density → Scatter/Reverse → Level Var/Position Rand/Pitch Spray → Tone/Width → Mix.
Hero Control 또는 Display: GRAIN HALO — 현재 파라미터로 수명·밀도·반경·색조를 계산해 중앙에서 방사되는 실시간 Grain Field.
Primary 컨트롤: Pitch, Feedback, Mix.
필수 컨트롤 그룹: 한 패널의 1행 `Time / Size / Density / Scatter / Level Var / Position Rand / Pitch Spray`, 2행 `Pitch / Reverse / Feedback / Tone / Width / Mix`.
모드 전환 방식: 없음. 13개 조절 컨트롤을 하나의 760×430 패널에 항상 표시한다.
애널라이저 필요 여부와 목적: 오디오 측정 analyzer/tap은 없음. 고정 seed의 UI 입자장만 사용해 grain count, size, scatter/width, reverse, feedback 수명, tone과 pitch 색조를 설명.
그래픽 메타포: 어두운 프리즘 유리의 밝은 중앙 코어에서 작은 grain이 방사되거나 reverse로 되감기는 장면.
피해야 할 시각 표현: Portal의 원형 XY 배치나 SpaceBlender의 X/Y space visualizer 복제, 가짜 스펙트럼/레벨 미터, 과도한 네온.
참고할 레퍼런스: Guide MD/Tools/Mockup의 8개 전체와 `TB Shimmer Grain Cloud (standalone).html`의 최상단 `2a`/variant `d`. Portal에서는 grain movement 가독성만, Wave Shifter에서는 모듈 위계, Fresh Air에서는 소수 핵심 컨트롤, Convolver에서는 compact 정보, Magma/Softube/Atlas에서는 촉각 재질과 계기 위계를 참고한다.
```

확인한 Mockup 파일:

- `01_Portal.png`
- `02_Wave_Shifter.png`
- `03_Fresh_Air.png`
- `04_Convolver.png`
- `05_Magma_BB_Tubes.png`
- `softube 예시.jpeg`
- `softube 예시2.jpeg`
- `가장 예쁜 디자인..png`

목업에서 유지할 요소는 핵심 효과가 먼저 읽히는 Hero, 촉각 컨트롤, compact header, 값 가독성이다. 그대로 복제하지 않을 요소는 Portal의 원형 XY, 특정 브랜드/로고, 기존 panel geometry와 preset 그래픽이다.

### 8.1 Semantic palette

| 토큰 | HEX | 역할 |
| --- | --- | --- |
| `canvas-top` | `#30343B` | 흑연 섀시의 밝은 면 |
| `canvas-bottom` | `#101217` | 섀시의 어두운 면 |
| `surface-top` | `#282D35` | 돌출 control board |
| `surface-bottom` | `#171B22` | panel depth |
| `display-top` | `#18202A` | prism glass 밝은 면 |
| `display-bottom` | `#090C12` | display 깊이 |
| `text-primary` | `#EEF3F7` | 제품명과 주요 값 |
| `text-secondary` | `#AEB8C4` | 라벨과 눈금 |
| `value-text` | `#DCEAF2` | 값 표시 |
| `accent-primary` | `#BCEBFF` | Ice Pearl 포인터와 첫 grain |
| `accent-secondary` | `#F7C5FF` | Opal Bloom 후속 grain/feedback |
| `section-accent` | `#7E769E` | 짧은 의미 구분선 |
| `outline` | `#4A5260` | panel/display 경계 |
| `deep-shadow` | `#05070A` | 홈과 접촉면 |
| `highlight` | `#697381` | metal/glass reflection |

예약색과 나란히 비교했으며 기존 cyan/violet보다 명도가 높은 pale pearl 조합으로 분리한다. 구현과 함께 중앙 UI 가이드 등록부에 `TB_Shimmer` 행을 추가한다.

### 8.2 레이아웃과 자산

```text
예상 에디터 크기: 760 × 430 px.
헤더: 760 × 52 px, 좌 TB_SHIMMER / 중앙 preset tools / 우측 중앙 Team Impulse 원본 로고와 8방향 2개 ring의 정적 분신 16개.
Hero display: 약 732 × 132 px.
Controls: 한 패널의 2 compact rows. 1행은 Time/Size/Density/Scatter/Level Var/Position Rand/Pitch Spray, 2행은 Pitch/Reverse/Feedback/Tone/Width/Mix이며 flat matte knob와 경계 없는 평문 값 표시를 사용한다.
Surface: Grain Field와 13개 컨트롤을 전환 없이 같은 760×430 화면에 유지한다.
Designer-authored material-base master: Design/Assets/shimmer_prism_glass_master.svg.
Designer-authored material-base output: Source/Assets/shimmer_prism_glass.png, alpha 포함.
Header logo canonical source: `E:/Plugin Projects/TB Hub/TBHub/Assets/Brand/signature-dark.png`.
Header logo embedded output: `Source/Assets/tb_effect_logo.png`, canonical transparent 845 px source와 byte-identical. 중앙 원본은 38 px로 그리고 상·하·좌·우·네 대각선에 1차 8개, 더 바깥에 2차 8개를 크기·투명도만 달리해 방사형으로 반복한다.
동적 텍스트·포인터·grain 위치는 static asset에 굽지 않는다.
```

Grain Field는 editor가 보일 때 UI message thread의 30 fps timer로만 갱신하고 숨겨지면 simulation/repaint를 멈춘다. worker와 audio tap은 사용하지 않으며 DSP와 UI 상태는 분리한다.

## 9. Factory preset 방향

```text
Init 상태: 기본 파라미터와 동일한 들리는 +12 st shimmer 시작점.
Factory 개수: 정확히 100.
Legacy 논리 폴더: START 3 / SHIMMER 3 / CLOUD 3 / REVERSE 3.
신규 논리 폴더: UTILITY / DIFFUSE / SPARKLE / RANDOM / PULSE / VOCAL / GUITAR / KEYS / PADS / DARK / EXPERIMENTAL, 각 8개.
대표 사용 사례: guitar ascension, pad aurora, vocal starlight, drum dust, reverse comet, dark orbit.
Factory 금지 초기 상태: Bypass On, Mix 0%, feedback 폭주.
사용자 프리셋 확장자: .tbshimmer
사용자 프리셋 폴더: Documents/TB_Shimmer/Presets/User
스캔: 비재귀, 파일명 대소문자 무시 오름차순.
저장 성공 토스트: 약 3.2초.
오류: 사용자가 닫을 때까지 유지되는 AlertWindow.
```

| 신규 그룹 | 개수 | 목적 |
| --- | ---: | --- |
| UTILITY | 8 | 저강도 insert, 기본 공간과 점검용 기준점 |
| DIFFUSE | 8 | grain 경계를 부드럽게 잇는 early/late wash |
| SPARKLE | 8 | 밝고 선명한 상승 shimmer |
| RANDOM | 8 | Level Var, Position Rand, Pitch Spray 조합 탐색 |
| PULSE | 8 | host tempo-sync를 주장하지 않는 free-time 반복과 움직임 |
| VOCAL | 8 | 보컬 명료도를 남기는 halo와 tail |
| GUITAR | 8 | 어택을 남긴 기타 bloom과 ascension |
| KEYS | 8 | piano/keys의 sustain과 harmonic lift |
| PADS | 8 | 길고 넓은 cloud와 배경 wash |
| DARK | 8 | 낮은 Tone, 하강 pitch와 reverse 계열 공간 |
| EXPERIMENTAL | 8 | 허용 범위 안의 극단적 granular 조합 |

신규 88개는 위 순서의 연속 그룹으로 기존 12개 뒤에 append한다. Factory bank는
`getNumPrograms()`를 통해 100개 모두 노출하므로 VST3 Program은 `valueCount=100`,
`stepCount=99`가 된다. 이 변경은 public parameter ID와 schema/preset 2/2를 바꾸지
않지만, 이전 12-program 버전과 normalized Program automation grid는 달라진다.
저장된 parameter state와 User preset은 호환되며 기존 Program automation lane은
재확인이 필요하다.

Diffuse tail 추가 뒤 `START` 3개는 기본 계약과 저강도 기준점으로 유지했다. 나머지 9개는
stable ID, 표시명과 그룹을 유지한 채 다음 값으로 리튜닝했다. 값 순서는
`Time / Size / Density / Scatter / Pitch / Reverse / Feedback / Tone / Width / Mix`다.

| Stable ID | 구현값 |
| --- | --- |
| `glass_halo` | `320 / 200 / 13 / 24 / 12 / 10 / 50 / 12000 / 145 / 50` |
| `guitar_ascension` | `260 / 140 / 10 / 18 / 12 / 8 / 38 / 10500 / 125 / 40` |
| `vocal_starlight` | `180 / 120 / 16 / 14 / 7 / 6 / 26 / 12000 / 120 / 34` |
| `pad_aurora` | `480 / 360 / 20 / 45 / 12 / 22 / 52 / 10000 / 160 / 54` |
| `slow_nebula` | `800 / 500 / 15 / 60 / 7 / 30 / 58 / 6000 / 175 / 60` |
| `wide_harmonic_fog` | `600 / 280 / 22 / 62 / 19 / 18 / 44 / 11000 / 185 / 54` |
| `reverse_comet` | `420 / 220 / 9 / 38 / 12 / 100 / 40 / 10500 / 155 / 54` |
| `falling_glass` | `360 / 180 / 12 / 52 / -7 / 80 / 35 / 7500 / 150 / 50` |
| `dark_orbit` | `900 / 420 / 10 / 58 / -12 / 60 / 54 / 3200 / 175 / 58` |

이 값은 SpaceBlender Factory preset에서 가져온 것이 아니라 새 자체 tail의 중첩을 보상하고
각 TB_Shimmer preset의 기존 역할을 유지하도록 조정한 값이다.

새 random 값은 최초 12개 legacy Factory preset 모두 기존 음색을 보존하는
migration-equivalent 값으로 추가한다. 각 preset에서 `Level Var = 0`,
`Position Rand = Scatter`, `Pitch Spray = Scatter × 0.0035`다. 이 최초 12개는
index 0–11에서 stable ID, 이름, 그룹, 14개 값과 순서를 byte-for-value로 유지하고,
신규 88개만 뒤에 추가한다.

## 10. 검증 계획과 성능 예산

| 검증 항목 | 방법 | 입력 소스 | 기대 결과 | 통과 기준 |
| --- | --- | --- | --- | --- |
| 핵심 청감 가치 | 레벨 매칭 A/B | KEY_01, GTR_01 | 상승하는 입자 cloud가 밝기만이 아닌 pitch/time movement로 들림 | 반복 구분 가능, click 없음 |
| Early diffusion | early-only window 분석 | deterministic stereo impulse/burst | late FDN보다 먼저 촘촘한 stereo reflection이 형성됨 | `69–125 ms` energy/side/populated bins > 0, 최초 late FDN 약 `131 ms` 이전, finite/bounded |
| Feedback 0 one-pass | input 뒤 silence의 구간 RMS | deterministic stereo tone/burst | early/late diffusion이 재순환 없이 자연스럽게 소멸 | early decay > 0, 1.2 s 이후 RMS < `1e-7`, finite/bounded |
| Diffuse tail | short chord/burst 뒤 silence | deterministic stereo burst | 개별 grain이 넓은 late wash로 이어지고 자연스럽게 감소 | 1.5 s bloom과 3–5 s late energy 존재, 8–10 s energy가 더 낮음, finite |
| Tail 공간성 | Width 0/160%, mono render | deterministic burst | stereo decorrelation과 mono 호환 | Width 0% L/R 오차 ≤ 1e-7, wide side energy와 mono late energy > 0 |
| 효과 정체성 | DSP Harness FFT/peak | 220 Hz sine | +12 st에서 440 Hz 부근 grain energy 우세 | peak 허용 오차 ±6% |
| Dry | sample diff | deterministic buffer | Mix 0%와 bypass가 입력과 같음 | max abs error ≤ 1e-7 |
| Feedback 안전 | 장시간 extreme render | impulse/noise | 유한하고 bounded | NaN/Inf 0, abs peak < 8 |
| State/Preset | save-change-load 및 v1 migration | 모든 parameter | v2 원자적 전체 복원, 유효한 v1의 고정 매핑 | tolerance 내 전부 일치, malformed/future 원자적 거부 |
| UX | 설명 없는 사용/UI snapshot 및 focus traversal | Standalone | Grain Field와 13개 컨트롤의 순서·단위가 한 화면에서 잘리지 않음 | 하나의 760×430 실제 캡처와 1행 7개/2행 6개 focus·layout 확인 |

성능 예산:

```text
기준 머신 / CPU: 현재 Windows 개발 머신, CPU 모델은 RELEASE_QA에서 측정 기록.
OS / 전원 모드: Windows x64 / Best performance.
호스트 또는 Harness: TB_Shimmer_Tests + Standalone/VST3 host probe.
기준 조건: Release, 48 kHz, 64 samples, stereo, Init.
허용 callback p99 / max: 266 µs(20% deadline) / 533 µs(40% deadline).
최악 조건: 192 kHz, 32 samples, density 2000, size 500 ms, feedback 88%, stereo.
메모리 예산: instance steady ≤ 16 MiB at 192 kHz.
editor 최초 open: ≤ 250 ms.
GUI: editor 표시 중 Grain Field 30 paint/s, 숨김 상태 0 paint/s, worker/task 0.
1/8 instance: dropout 0, steady memory growth 0.
오디오 callback allocation/lock/file I/O/log: 0.
```

## 11. 단계 승인과 알려진 미정

- [x] 한 문장 정의, 필수/제외 범위, A/B 소스와 기준을 작성했다.
- [x] 컨트롤, 신호 경로, 기본값, 식별자 후보와 host 범위를 정했다.
- [x] 전체 Mockup 파일을 확인하고 독자 UI 방향을 기록했다.
- [x] Early diffuser와 Factory 100개 전수 검증을 포함한 전체 DSP Harness의 Debug/Release `36/36 PASS` 결과를 `RELEASE_QA.md`에 확정했다.
- [x] Release VST3 Host Probe 뒤 Processor/Controller CID와 VST3 ParamID를 확정했다.
- [ ] 사람의 레벨 매칭 청감 판정은 자동 측정으로 대체하지 않으며 최종 사용자 검수가 필요하다.

## 12. 기획 변경 기록

| 날짜 | 변경 내용 | 변경 이유 | 영향 범위 |
| --- | --- | --- | --- |
| 2026-08-02 | 최초 작성 | granular shimmer effect 제작 요청을 성공 기준과 최소 제품 범위로 구체화 | 전체 |
| 2026-08-02 | 8-line diffuse swarm tail 추가 | granular shimmer 뒤에 부드럽고 진화하는 공간 잔향을 더하되 public parameter/state 계약을 유지 | DSP, preset, 청감·QA 기준 |
| 2026-08-03 | Level Var → Position Rand → Pitch Spray 추가 | level·history position·pitch random을 Scatter에서 분리하고 직접 제어 | DSP, 14-parameter host contract, schema/preset 2 migration |
| 2026-08-03 | 4-stage/channel early diffuser와 단일 control panel 추가 | dry한 grain/late-tail 사이를 early reflection으로 채우고 모든 random 제어를 즉시 비교 | DSP topology, early/one-pass tests, 760×430 UI/UX |
| 2026-08-03 | Factory bank를 정확히 100개로 확장 | 최초 12개 계약을 보존하면서 용도별 88개 음색 추가 | Factory index/Program contract, preset QA, 문서 |
