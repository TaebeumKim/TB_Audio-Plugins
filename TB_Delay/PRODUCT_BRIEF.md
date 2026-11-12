# TB_Delay Product Brief

버전: 0.1  
작성일: 2026-07-27  
적용 공통 가이드: `E:\Plugin Projects\Guide MD`

## 1. 제품 기본 정보

```text
프로젝트명: TB_Delay
제품 표시명: TB_Delay
한 문장 정의: 좋은 스테레오 딜레이 위에서 매 반복의 색, 공간, 주파수와 입자 질감을 누적 변형해
              echo부터 comb, tape, spiral, grain cloud까지 연주하는 피드백 인스트루먼트.
플러그인 종류: Creative stereo delay / modulation effect
아날로그 복각 여부: 아니오
복각 대상: 해당 없음. Tape는 특정 기기 복각이 아닌 음악적 캐릭터 모드.
주요 대상 사용자: 음악 프로듀서, 믹싱 엔지니어, 사운드 디자이너
주요 대상 소스: 보컬, 기타, 신스·패드, 드럼 퍼커션
대표 사용 상황: 기본 echo, ping-pong 공간화, 12 ms급 comb/flange, worn tape echo,
                상승·하강 frequency-shift echo, granular cloud와 freeze throw
```

사용자 원문 요구:

> 일반적인 딜레이에 무슨 기능이 있어야 상업적 느낌의 재밌는 딜레이 효과를 줄 수 있을까?
> 콤필터링, 테이프, 핑퐁, 에코, 프리퀀시 시프팅 에코(상승/하강), 그레인즈 같은
> 복합적인 느낌들을 만들고 싶어.

> 필요한 노브의 종류와 개수, 제시된 음향 이펙트 실현은 포함되어있어야 하며 제시되지 않은
> 음향 이펙트를 프리셋으로 구현할 수 있도록 필요한 컨트롤러와 기능을 업데이트한 상태로 구현.

## 2. 해결하려는 음악적 문제

```text
사용자가 현재 겪는 문제:
  평범한 delay는 시간·feedback·mix 조절만으로 끝나고, 여러 외부 플러그인을 피드백처럼
  조합하지 않으면 반복이 시간에 따라 흥미롭게 변하지 않는다.

기존 작업 방식의 불편함:
  tape, frequency shifter, granular, diffusion, modulation과 ducking을 여러 인서트로 조합하면
  실제 feedback 내부 누적과 다르고, gain 및 폭주 관리와 preset recall이 복잡하다.

이 플러그인이 만드는 결과:
  한 개의 안정적인 stereo feedback 안에서 topology, character, motion, diffusion,
  frequency shift와 grain을 조합하고, 반복이 어두워지거나 흐려지거나 상승·하강·분해되게 한다.

처리 전 상태:
  정적이고 원음과 겹쳐 답답한 반복 또는 외부 체인 관리가 필요한 상태.

처리 후 기대 상태:
  원음은 ducking으로 선명하게 남고, 반복은 예측 가능한 방향으로 변화하며,
  일반 echo와 극단적인 sound-design preset 사이를 같은 조작 체계로 이동한다.
```

## 3. 제품 가치와 차별점

```text
이 플러그인을 선택할 한 문장 이유:
  반복될수록 변화하는 색·공간·스펙트럼·입자를 한 개의 안전한 feedback 악기로 연주할 수 있다.

일반적인 동종 처리와 다른 점:
  효과 종류를 서로 배타적인 거대 mode 목록으로 만들지 않고,
  Style × Character × Transform의 독립 축으로 조합한다.

기존 TB 플러그인과 겹치지 않는 점:
  시간축 feedback network와 tail 자체의 누적 변형이 제품 정체성이다.

이 플러그인만의 컬러링 축:
  매 반복의 bandwidth/비선형 밀도, stereo 이동, diffusion 밀도,
  signed-Hz spiral과 windowed-grain의 시간 분해.

의도적으로 하지 않을 것:
  특정 빈티지 기기 1:1 복각, 임의 순서의 자유 모듈러 그래프, 외부 IR,
  surround 처리, per-tap 16개 개별 편집기, 스펙트럴 오프라인 재합성.
```

## 4. 청감 A/B 성공 기준

```text
A 상태: 호스트 바이패스 또는 동일 레벨의 일반 clean delay
B 상태: TB_Delay Init 및 핵심 Factory preset
비교할 대상: 바이패스, Clean Echo, Tape, Spiral Up/Down, Grain Cloud
음량 매칭 방법: dry source peak/RMS를 고정하고 wet output을 통합 loudness 기준 ±1 dB 안에서 조정

반드시 구분되어야 할 청감 차이:
  - Tape는 밝기만이 아니라 반복의 saturation 밀도, bandwidth, 미세 시간 움직임이 달라야 한다.
  - Shift Up/Down은 매 반복의 dominant frequency가 해당 방향으로 누적 이동해야 한다.
  - Grain은 pitch, spray, reverse와 밀도 변화로 echo가 시간 입자로 분해되어야 한다.
  - Ping-Pong은 mono-summed seed가 L lane에서 시작해 cross-feedback으로 L-R-L 교대해야 한다.
  - Comb는 12 ms급 delay의 공진/notch가 정확하고 feedback polarity 차이가 들려야 한다.

차이를 표현할 음악적 단어:
  focused, bouncing, worn, melting, metallic, rising, falling, smeared, scattered, frozen

실패로 판정할 상태:
  레벨 또는 고역 밝기만 달라짐, shift mirror sideband가 주 신호만큼 큼, grain click,
  feedback 폭주·NaN·DC, automation click, ping-pong 채널 교대 실패, preset 간 중복.
```

## 5. 청감 A/B 사용 소재

Delay와 가장 가까운 `ParallelReverb`와 `InvertedFlanger`의 공통 목적을 바탕으로 다음 세 개를
고정한다. 모두 현재 공용 Assets 폴더에 존재한다.

| 역할 | 소재 | 선택 이유 |
| --- | --- | --- |
| 주 타깃 | `KEY_01` | 지속 화음에서 tape tail, diffusion, shift 누적과 grain cloud를 판단 |
| 대조 | `GTR_01` | 어택과 릴리즈가 있어 comb, ducking과 transient 보존을 판단 |
| 역방향 검증 | `DRM_01` | 과도한 smear·feedback·grain이 리듬을 무너뜨리는 상한을 판단 |

합성 검증에는 공용 `01_sine_220`, `03_saw_220`, `06_saw_440`, `07_white_stereo`,
`09_sweep_20_to_20k`와 Harness 내부 impulse를 사용한다.

## 6. 기능 범위

### 6.1 필수 기능

- Free/Sync/Tune 시간 모드와 straight/dotted/triplet division
- Single/Dual/Ping-Pong/Ratio stereo topology
- 1/2/4/8 tap shape, spacing와 swing
- 10 ms minimum Free delay와 12 ms급 comb/flange, feedback polarity를 포함하는 signed offset 동작
- Clean/Tape/BBD/Digital character
- feedback low-cut/high-cut, drive, age, delay modulation
- diffusion amount/size
- signed-Hz frequency shift와 stereo divergence
- overlapping Hann grain: amount, size, density, spray, pitch, reverse probability
- feedback-loop/post transform routing
- ducking, constant-power mix, width, output
- freeze
- 기술 계수를 파생하되 기존 값을 변경하지 않는 `COLOR`, `SPACE` host macro
- `STEREO`, `CHARACTER / SPACE`, `TRANSFORM`의 독립 모듈 바이패스
  - 현재 설정값은 보존하고 해당 모듈의 음향 기여만 약 15 ms로 중립 경로 전환
  - 기본 `REPEAT`, 최종 `MIX / OUTPUT`과 `FREEZE`는 항상 독립적으로 동작
- 현재 설정의 원본 IN, 첫 L/R 지점, echo/grain 시간·pitch·감쇄를 보여주는 parameter-prediction Delay Map
- APVTS automation/state, Undo/Redo
- `논리 폴더 > 프리셋` 2단계 Factory/User 통합 브라우저, 이전·다음 순환, 검증된 사용자 `.tbdelay` 저장·불러오기
- Mono→Mono, Mono→Stereo와 Stereo→Stereo, VST3와 Standalone

### 6.2 있으면 좋은 기능

- parameter lock을 가진 제한형 randomize
- external sidechain ducking
- tap별 pan/level 편집

핵심 DSP와 상태·preset 검증을 통과하기 전에는 추가하지 않는다.

### 6.3 제외 범위

- 임의 순서의 drag-and-drop modular routing
- 특정 tape machine의 물리 모델 1:1 복각
- AAX/AU/CLAP 및 surround
- MIDI note key tracking과 MIDI-triggered grain
- 16개 이상 tap의 per-tap automation
- 외부 sample/IR import

## 7. 컨트롤 계획

### 7.1 공개 연속 파라미터: 로터리 10개 + 수평 슬라이더 2개

일반 화면에는 이름만으로 결과를 예측할 수 있고 반복해서 조정할 가치가 높은 값만 둔다.
기존 기술 파라미터는 삭제하거나 의미를 바꾸지 않고 APVTS·호스트 자동화·상태·Factory/User
preset·DSP에 보존한다. `COLOR`와 `SPACE`는 여러 기술 계수가 만드는 하나의 음악적 행동을
각각 한 개의 안정된 host parameter로 표현한다. Hero는 큰 노브가 아니라 실제 설정이 만들
delay pattern을 반영하는 `Delay Map`이다.

| # | 공개 컨트롤 | 사용자에게 들리는 역할 | 범위·단위 | 기본값 | 타입 | 위계 |
| ---: | --- | --- | --- | ---: | --- | --- |
| 1 | DELAY TIME | Free ms / Sync 음표 / Tune 공진 주파수 | 10–8000 ms 또는 1/32–1/1 T/D | 1/8 D | 모드 통합 rotary | Primary |
| 2 | FEEDBACK | 반복 감쇠와 self-oscillation 접근 | 0–110 % | 48 | Unipolar rotary | Primary |
| 3 | WIDTH | wet stereo 폭 | 0–200 % | 130 | Unipolar rotary | Standard |
| 4 | DUCK | dry 입력에 반응해 wet을 뒤로 이동 | 0–100 % | 25 | Unipolar rotary | Standard |
| 5 | TONE (`COLOR` macro) | 깨끗한 반복 ↔ 프리셋 원형 ↔ 낡은 반복 | -100–100 % | 0 BASE | Bipolar macro rotary | Standard |
| 6 | DIFFUSION (`SPACE` macro) | 단단한 반복 ↔ 프리셋 원형 ↔ 퍼지는 반복 | -100–100 % | 0 BASE | Bipolar macro rotary | Standard |
| 7 | SHIFT | 반복마다 누적되는 signed Hz 이동 | -2000–2000 Hz | 0 | Bipolar rotary detent | Standard |
| 8 | GRAIN | normal echo와 grain layer 비율 | 0–100 % | 0 | Unipolar rotary | Standard |
| 9 | PITCH | grain 재생 음정 | -24–24 st | 0 | Bipolar rotary detent | Standard |
| 10 | REVERSE | grain별 reverse 확률 | 0–100 % | 0 | Unipolar rotary | Standard |
| 11 | MIX | constant-power dry/wet | 0–100 % | 30 | Filled horizontal slider | Primary |
| 12 | OUTPUT | 최종 출력 보정 | -18–6 dB | 0 | Bipolar filled horizontal slider, 0 dB detent | Standard |

메인 UI에서 숨기는 기존 상세 ID는 `OFFSET`, `SPACING`, `SWING`, `LOW CUT`, `HIGH CUT`,
`DRIVE`, `AGE`, `MOD RATE`, `MOD DEPTH`, `SMEAR`, `SIZE`, `DIVERGE`, `GRAIN SIZE`,
`DENSITY`, `SPRAY`다. 이 값은 preset-only이며 기존 세션의 host automation에서 계속 접근할
수 있다. `TIME CHANGE`도 같은 이유로 상태와 DSP에는 남기고 메인 UI에서는 숨긴다.

컨트롤 타입 합계:

- 공개 연속 로터리 10개와 Sync에서 같은 자리를 교대하는 `DIVISION` stepped rotary
- 공개 stepped rotary 3개: `STYLE`, `ECHOES`, `CHARACTER`
- 수평 슬라이더 2개: `MIX`, `OUTPUT`
- OUTPUT은 0 dB detent·marker와 양방향 amount fill로 부호를 표시

#### 7.1.1 매크로 파생 계약

두 매크로는 기존 상세 파라미터 값을 쓰거나 자동화하지 않는다. Processor→Engine 경계에서
오직 effective DSP coefficient만 계산하며 `0 BASE`에서는 기존 preset/state의 결과와 정확히
같다. Hz와 시간은 로그 보간하고 나머지는 선형 보간한다.

| 매크로 | -100 | 0 BASE | +100 |
| --- | --- | --- | --- |
| COLOR | Drive 0, Age 0, Low Cut 20 Hz, High Cut 20 kHz | 기존 Drive/Age/Low Cut/High Cut 그대로 | Drive `max(base,72)`, Age `max(base,82)`, Low Cut `max(base,320 Hz)`, High Cut `min(base,4.2 kHz)` |
| SPACE | Mod Depth 0, Mod Rate base, Smear 0, Size 2 ms | 기존 Mod Rate/Depth/Smear/Size 그대로 | Mod Rate `max(base,1.5 Hz)`, Mod Depth `min(20,max(base,4)+0.5×base) ms`, Smear 100, Size 120 ms |

메인 UI 표시명은 `TONE`과 `DIFFUSION`이며 호스트·상태 ID는 기존 `COLOR`/`SPACE` 매크로를
유지한다. 표시 문자열은 TONE이 `CLEAN n / BASE / WORN n`, DIFFUSION이
`TIGHT n / BASE / WASH n`이다.
모듈 bypass가 켜지면 파생된 결과도 기존 15 ms 중립 morph를 함께 따른다. Delay Map은
오디오 엔진과 같은 effective 파생값을 사용해 SPACE에 따른 시간 변동 폭과 감쇄 예측을 갱신한다.

### 7.2 공개 음향 셀렉터·토글·명령: 11개

| # | 컨트롤 | 선택지 | 기본값 | 역할 |
| ---: | --- | --- | --- | --- |
| 1 | TIME MODE | Free / Sync / Tune | Sync | 현재 시간 해석을 고르는 dropdown |
| 2 | TAP | Momentary | — | 최근 최대 4개 탭 간격의 평균을 현재 모드의 시간 입력에 적용 |
| 3 | DIVISION | 1/32–1/1, T/D 포함 | 1/8 D | Sync에서 동일 `DELAY TIME` 위치에 나타나는 stepped rotary |
| 4 | STYLE | Single / Dual / Ping-Pong / Ratio | Ping-Pong | channel/feedback topology |
| 5 | ECHOES | 1 / 2 / 4 / 8 | 1 | rhythmic density |
| 6 | CHARACTER | Clean / Tape / BBD / Digital | Tape | feedback color algorithm |
| 7 | SHIFT ROUTE | Loop / Output | Loop | shift/grain을 반복마다 누적하거나 출력에서 한 번 적용 |
| 8 | FREEZE | Off / On | Off | 새 입력 차단과 tail hold |
| 9 | STEREO BYPASS | Off / On | Off | topology·offset·width를 Single/0%/100% 중립으로 전환 |
| 10 | CHARACTER / SPACE BYPASS | Off / On | Off | character·filter·modulation·diffusion을 중립으로 전환 |
| 11 | TRANSFORM BYPASS | Off / On | Off | grain·frequency transform을 중립으로 전환 |

`SHIFT ROUTE`는 editor의 결과 중심 표시명이다. 기존 세션과 host automation metadata를
보존하기 위해 APVTS/VST3 Choice text는 `Loop / Post`를 유지한다.

`TAP`은 저장·자동화되는 38개 parameter에 추가되지 않는 UI 명령이다. 첫 탭은 측정만 시작하고
두 번째부터 100–3000 ms의 최근 최대 4개 간격을 평균한다. Free와 Tune은 기존 `time`을,
Sync는 실제 host BPM에서 가장 가까운 기존 `division`을 갱신하며 현재 TIME MODE는 유지한다.

통합 Preset browser, 이전/다음, Save 아이콘, Undo/Redo는 음향 컨트롤 개수에 포함하지 않는다.

## 8. 제시되지 않은 효과를 만드는 조합 능력

| 추가 효과 | 필요한 조합 |
| --- | --- |
| Slapback | 짧은 TIME, TAPS 1, 낮은 FEEDBACK, Tape |
| Chorus | 10–35 ms, Dual, 낮은 FEEDBACK, SPEED/WOBBLE, WIDTH |
| Flanger | 12 ms급 TIME, feedback, WOBBLE, signed OFFSET |
| Tuned resonator | TIME MODE Tune, 높은 FEEDBACK, LOW/HIGH CUT, DISTORT |
| Dub throw | Sync, Ping-Pong, HIGH CUT, DISTORT, DUCK, FREEZE |
| Rhythmic multi-tap | TAPS, SPACING, SWING, Sync division |
| Diffuse delay/reverb | TAPS 4/8, BLUR, SPACE, feedback damping |
| Shimmer / falling harmony | Grain PITCH ±12 st, Loop route, BLUR |
| Metallic barber spiral | signed SHIFT, DIVERGE, Loop route |
| Reverse cloud | GRAIN, SPRAY, REVERSE, BLUR, FREEZE |
| Lo-fi stutter | Digital/BBD, WEAR, 짧은 TIME, TAPS, FREEZE |
| Broken tape | Tape, DISTORT, WEAR, SPEED/WOBBLE, grain SPRAY |

## 9. 신호 구조와 기술 가정

```text
입력 형식: mono 또는 stereo float audio
출력 형식: mono 또는 stereo float audio. Mono 입력은 stereo 출력으로 확장 가능

신호 처리 순서:
  input trim/DC safety
  → dry detector
  → input injection + previous feedback
  → character record saturation
  → stereo circular buffer
  → fractional normal/multi-tap reads + granular reads
  → character playback loss
  → frequency shift
  → diffusion
  → feedback HP/LP
  → normalized stereo matrix
  → bounded soft safety
  → feedback write
  → post transform(선택)
  → wet width + duck + output
  → constant-power mix

Dry/Wet 또는 병렬 구조: constant-power Dry/Wet
지원 채널 구성: Mono→Mono, Mono→Stereo, Stereo→Stereo
예상 host 보고 레이턴시: 0 sample
내부 transform group delay: frequency shifter wet 경로에 샘플레이트 비례 FIR의 약 0.66 ms
                                group delay가 포함됨
레이턴시 허용 범위: 의도된 delay wet 신호 안에서 1 ms 미만
예상 CPU 등급: Medium. 최대 8 grain × stereo + short FIR + diffusion.
오버샘플링 필요 여부: prototype은 feedback bandwidth 제한으로 시작하고 alias QA가 실패할 때
                        character saturator만 2x 적용.
외부 파일·IR·모델 의존성: 없음
제3자 라이브러리: JUCE 8.0.12만 사용. 외부 DSP 코드는 복사하지 않음.
```

Stereo feedback matrix는 self↔cross morph 과정에서 gain이 커지지 않게 정규화한다.
Ping-Pong은 stereo 입력도 mono로 합성해 L lane 하나에 seed하고, cross-feedback으로
`L→R→L`을 만든다. 따라서 Mono→Stereo에서도 실제 좌우 교대가 생기지만, 입력 L/R의
anti-phase 성분은 mono 합성 단계에서 상쇄될 수 있다.
일반 feedback은 안전 범위로 제한하고, 100% 이상은 bounded soft saturation이 있는 의도적
runaway 영역으로 취급한다. Freeze는 새 input을 닫는 10 ms ramp와 feedback의 unity
recirculation 전환을 equal-power로 교차시키고 loop character/filter coloration을 우회해
이미 채워진 loop의 진입 레벨을 보존한다. 빈 history에서 Freeze가 이미 On이면 실제 첫
delay 신호가 loop에 도착할 때까지 capture-armed 상태로 입력을 열어 둔다.
`MOD DEPTH`의 실제 이동 폭은 가장 짧은 channel delay와 10 ms minimum read 사이의 headroom으로
clamp해 read head가 허용 지연보다 앞으로 가지 않게 한다.
Circular delay history는 8초다. TIME·SPACING으로 계산한 긴 multi-tap read가 이 범위를 넘으면
최대 history 지점으로 clamp되므로 여러 tap이 같은 시점에 겹칠 수 있다.
Pitch·Reverse로 이동하는 grain은 시작 지점을 전체 voice 궤적 기준으로 제한해 8초 경계를
원형 wrap하여 최근 오디오를 잘못 읽지 않게 한다.

## 10. UX와 시각 방향

```text
사용자의 첫 번째 편집: Factory preset 선택 또는 DELAY TIME → FEEDBACK 조정
가장 자주 사용하는 작업 흐름:
  preset 선택 → REPEAT과 STEREO 조정
  → CHARACTER / SPACE 매크로와 TRANSFORM 조정
  → REPEAT의 DUCK으로 원음과 반복의 관계 조정
  → 패널 없는 하단 MIX / OUTPUT 슬라이더에서 마무리

Hero Control/Display: 현재 파라미터가 만들 L/R echo·grain·pitch·feedback을 예측하는 Delay Map
Primary 컨트롤: DELAY TIME / FEEDBACK / MIX
Standard 컨트롤: 나머지 공개 로터리 11개, OUTPUT 슬라이더와 결과 중심 셀렉터·토글
Supporting 컨트롤: 2단계 논리 폴더 preset browser, 이전/다음, Save, Undo/Redo
Preset-only / Internal 컨트롤: 기존 15개 기술 계수와 TIME CHANGE
컨트롤 크기 예외: 보드의 13개 로터리 body는 41 px.
최하단 `MIX`와 `OUTPUT`은 33 px footer 안의 filled horizontal slider를 사용한다.

필수 컨트롤 그룹:
  상단 전체 폭 Delay Map
  하단 전체 폭 단일 보드의 2×2 사분면:
    좌상 REPEAT
    우상 STEREO
    좌하 CHARACTER / SPACE
    우하 TRANSFORM
  최하단 패널 없는 MIX / OUTPUT output row

모드 전환 방식: 페이지 전환 없이 2×2 보드 안의 `FREE / SYNC / TUNE` dropdown,
stepped rotary, binary toggle과 Freeze toggle. DELAY TIME은 Free/Tune raw time과
Sync DIVISION이 같은 bounds에서 교체된다. 메인 UI는 결과를 바로 읽도록
`ECHOES`, `CHARACTER`, `SHIFT ROUTE` 같은 표시명을 사용하고 호스트 ID는 보존한다.
시각화 등급: 파라미터 기반 예측 시각화. 오디오 입력을 분석하는 실시간 analyzer가 아님.
시각화 목적: 원본 L/R 기준점에서 각 echo/grain의 시간, pitch 방향과 명목 감쇄를 이해.
무신호 상태: 실제 입력 레벨과 무관하게 현재 설정이 만들 delay pattern을 계속 표시.
그래픽 메타포: 어두운 금속 섀시에 매입된 stereo delay timeline/map.
피해야 할 시각 표현: 가짜 스펙트럼, 자율 입자 애니메이션, 네온 진행 링, 중첩 카드.
예상 에디터 크기: 960 × 650 px, resizable.
```

### 10.1 시각·정보·조작 위계

TB_Delay는 텍스트, 공간과 행동의 위계를 서로 구분한다. 독립 랙 헤더의 좌상단
`TB_DELAY`는 가장 강한 **텍스트 신호**, Delay Map은 가장 큰 **공간 덩어리**,
`DELAY TIME`과 `FEEDBACK`은 가장 먼저 찾아야 하는 **편집 지점**이다. 중앙 preset은
Supporting, 우상단 Team Impulse Impact 물고기 로고는 세 개의 반투명 분신으로 Delay를
설명한다. `MIX`는 기능상 Primary지만 작업 순서상 최하단에서 마무리한다.

| 단계 | TB_Delay 적용 | 시각 규칙 |
| --- | --- | --- |
| 1. Hero Display | 상단 전체 폭 `Delay Map` | 최소 창에서 `900 × 200 px` 이상을 유지한다. 큰 면적을 가지되 베젤·그리드가 조작부보다 강하게 빛나지 않는다. |
| 2. Structure | 독립 랙 헤더, 하나의 2×2 control board와 패널 없는 output row | 네 섹션은 같은 광원·깊이의 얕은 양각 peer panel이며 Map은 매입형이다. |
| 3. Primary | `DELAY TIME`, `FEEDBACK`, `MIX` | 중요도는 큰 노브가 아니라 읽기 순서, 그룹 위치와 결과 피드백으로 표현한다. |
| 4. Standard | 나머지 가시 음향 파라미터와 셀렉터·토글 | 같은 라벨·값·노브 문법을 유지하고 기능별 행으로 비교 가능하게 배치한다. |
| 5. Supporting | 중앙 preset browser, 이전/다음, Save, Undo/Redo | 프리셋 중심을 에디터 중심에 고정하고 제품명·우측 로고보다 약하게 표현한다. |
| 6. Preset-only / Internal | 기존 기술 계수 15개와 `TIME CHANGE` | editor에는 표시하지 않되 APVTS ID, 상태 복원, Factory/User preset, host automation과 DSP 호환은 보존한다. |

컨트롤 노출과 밀도 규칙:

- 일반 화면에는 이름으로 들리는 결과를 예상할 수 있고 반복 조정 가치가 높은 컨트롤만 남긴다.
  구현 세부에 가깝거나 다른 매크로와 역할이 겹쳐 선택 피로·과처리 인상을 만드는 값은
  preset-only 후보로 검토한다.
- 빈 공간을 채우기 위한 노브는 추가하지 않는다. 먼저 들리는 역할에 맞춰 기존 컨트롤을
  재배치하고, 과밀한 섹션은 기능별 두 줄로 나누며, 성긴 섹션은 그룹을 중심 정렬한다.
- Stereo는 공간 결과를 직접 만드는 `STYLE`, `WIDTH`만 한 행에 둔다. `OFFSET`은 시간 비율과
  feedback polarity를 동시에 바꿔 이름만으로 결과를 예측하기 어려우므로 preset-only로 둔다.
- Character/Space는 `CHARACTER`, `TONE`, `DIFFUSION` 한 행으로 정리한다. 기술 계수 8개는 두 매크로의
  base가 되며 메인 UI에서 중복 노출하지 않는다.
- Transform은 `SHIFT`, `GRAIN`, `PITCH`, `REVERSE`만 노출한다. `SHIFT ROUTE`와
  `FREEZE`는 결과가 독립적이므로 합치거나 숨기지 않는다.
- 로컬 그룹 여백은 허용하지만 Standard 셀 하나가 들어갈 빈 띠를 장식으로 남기지 않으며,
  전체 control board의 사용 가능 가로·세로 영역은 공통 가이드의 85% 점유 기준을 유지한다.

컨트롤과 색상 위계:

- 보드의 Standard 외곽 눈금은 `41 px`다. 보이는 크기와 별개로 실제 마우스 조작 영역은
  충분히 유지한다.
- 모든 연속 로터리는 범위 시작점부터 현재값까지 Aged Brass energy arc로 채운다.
  표준 arc는 `2.6 px`다. 하단 MIX/OUTPUT은 `9 px` amount fill과 독립 핸들을 사용하며
  OUTPUT은 0 dB marker를 표시한다.
- warm graphite가 화면 대부분을 차지하고 Aged Brass는 활성 포인터·값 arc·L/forward
  기준점, Smoked Sage는 R/return 정보에만 쓴다. 같은 Brass를 쓰는 섹션 표식은 짧은
  선과 작은 면적으로 제한해 활성값처럼 보이지 않게 한다.
- 노브 몸체, 패널, 베젤과 그리드는 중성 재질 계층을 사용하며 네온 진행 링과 넓은 포인트
  컬러 면을 만들지 않는다.

Delay Map 내부의 정보 강도는 `원본 기준선과 첫 L/R anchor → 후속 echo event → decay
triangle과 grain support → lane guide와 grid` 순으로 낮춘다. 첫 지점은 가장 넓고 선명한
막대, 후속 반복은 세대별 낮은 alpha, decay는 marker보다 약한 삼각형 면으로 보여준다.

단일 보드 사분면별 컨트롤:

| 위치 | 사분면 | 연속 컨트롤 | 셀렉터·토글 |
| --- | --- | --- | --- |
| 좌상 | `REPEAT` | `DELAY TIME`, `FEEDBACK`, `ECHOES`, `DUCK` | `TIME MODE`, `TAP` |
| 우상 | `STEREO` | `WIDTH` | `STYLE`, `STEREO BYPASS` |
| 좌하 | `CHARACTER / SPACE` | `TONE`, `DIFFUSION` | `CHARACTER`, `CHARACTER / SPACE BYPASS` |
| 우하 | `TRANSFORM` | `SHIFT`, `GRAIN`, `PITCH`, `REVERSE` | `SHIFT ROUTE`, `FREEZE`, `TRANSFORM BYPASS` |
| 최하단 | 패널 없는 output row | `MIX`, `OUTPUT` 수평 슬라이더 | 항상 활성 |

네 사분면은 하나의 불투명 control board 안에서 같은 광원과 베벨을 가진 얕은 양각 peer
panel로 나눈다. 탭, 페이지 버튼 또는 추가 중첩 카드 없이 모든 음향 컨트롤을 동시에 표시한다.
`STEREO`, `CHARACTER / SPACE`, `TRANSFORM`의 바이패스는 같은 제목선 우측에 두고,
활성화 시 해당 컨트롤만 감광한다. `FREEZE`는 Transform 바이패스와 독립이다.
Delay Map은 바이패스된 모듈의 중립 결과를 즉시 예측한다.
두 매크로는 `BASE`와 방향 단어를 값 영역에 직접 표시하고, 노브·라벨·값을 2초간 호버하면
어떤 기술 계수가 어떤 방향으로 움직이는지 한국어 툴팁으로 설명한다. 숨긴 상세 컨트롤은
component나 hit target을 만들지 않는다.

Delay Map 표현 규칙:

- 세로 `IN` 기준선이 원본 소스 시점이고, 중앙 가로선의 오른쪽이 지연 시간이다.
- 위 lane은 L, 아래 lane은 R이며 Ping-Pong은 feedback 세대가 교차하는 lane으로 읽힌다.
- X축은 10 ms부터 5 s까지 첫 지점과 반복 간격을 직접 비교할 수 있는 선형 실제 시간축이다.
  5 s를 넘는 echo와 명목 tail은 우측 끝에 클램프하고 `>5 s`로 표시한다.
- Delay Map의 실제 그려진 외곽 베젤은 하단 control board의 그려진 외곽과 좌우 edge가
  정확히 일치한다. 컴포넌트 bounds만 같고 내부 inset 때문에 좁아지는 상태는 허용하지 않는다.
- 실제 출력 경로에서 계산한 첫 L/R event time은 feedback, tap, Grain 양과 무관하게 굵은
  handle·중심선·유효 ms label로 항상 표시한다. Ping-Pong의 첫 R anchor는 L seed가 R로
  교차된 시점이므로 두 lane delay의 합이다.
- 첫 L/R handle은 실제 event time을 중심선으로 유지하면서 5.5 px 폭의 세로 막대로 표시해
  작은 틱보다 빠르게 찾을 수 있게 한다. 외곽 shadow는 8 px를 넘지 않아 10 ms 반복을 가리지 않는다.
- `TIME MODE`는 `FREE / SYNC / TUNE` dropdown이다. 동일한 `DELAY TIME` tile에서 Free는
  raw ms, Sync는 `DIVISION` 음표, Tune은 양자화된 공진 Hz를 표시·조절한다. 모드별 값은
  독립 보존하며 Map 상단과 첫 지점에는 실제 host BPM 또는 튜닝 결과의 유효 ms를 표시한다.
- `TAP`은 TIME MODE dropdown 바로 아래 같은 폭의 momentary 버튼이다. Free/Tune에서는
  측정 interval을 `time`에, Sync에서는 host BPM 기준 가장 가까운 `division`에 적용하며,
  키보드 포커스와 Space 단축키를 소비하지 않는다.
- echo tick은 frequency shift의 상승/하강 방향을 보여준다. Grain의 `±st` 높이와
  signed-Hz 방향 glyph는 분리해, 반대 방향일 때 임의로 상쇄된 semitone처럼 보이지 않게 한다.
- grain은 정확한 점 이벤트로 단정하지 않는다. DSP의 forward read-safety,
  grain window, Spray, reverse 확률로 계산한 가능한 출력 시간 support를 반투명 cloud로
  표시한다. `PITCH`는 cloud의 중심 stem/triangle 높이에 반영하고, Loop에서는 세대별
  pitch/shift 누적, Post에서는 1회 transform을 반영한다. 5 s 경계를 넘는 cloud support는
  우측 `>5 s` overflow marker로 알린다.
- 각 echo 뒤의 삼각형은 실제 waveform 측정값이 아니라 마지막 tap의 effective delay와
  DSP feedback gain으로 계산한 명목 -60 dB tail이다. `FREEZE`는 `HOLD`,
  100% 초과는 `SAT`, 음수 feedback polarity는 `NEG`와 반전 marker로 표시한다.
- Mono→Mono instance에서는 R lane을 만들지 않는다. Stereo output Ping-Pong은 mono-summed
  input seed 하나를 L lane에 두고 실제 `L→R→L` 교차 경로를 표시한다.
- `TIME/DIVISION/OFFSET`, `STYLE/TAPS/SPACING/SWING`, `FEEDBACK/FREEZE`,
  `SHIFT/DIVERGE/ROUTE`, `GRAIN/SIZE/DENSITY/SPRAY/PITCH/REVERSE`,
  `WOBBLE/BLUR`, `COLOR/SPACE` 매크로 변경이 UI timer 주기 안에 다시 계산되어 표시된다.

### 10.2 확인한 목업

가이드 표기 `E:\Plugin Projects\Guide MD\Mockup`은 현재 존재하지 않는다. 실제 위치
`E:\Plugin Projects\Guide MD\Tools\Mockup`의 다음 5개를 모두 확인했다.

| 파일 | 유지할 요소 | 수정할 요소 |
| --- | --- | --- |
| `01_Portal.png` | 한 개의 강한 Hero 장면, macro-first 흐름 | 거대한 장식 비율과 자율 입자는 배제하고 실제 delay 상태에 연결 |
| `02_Wave_Shifter.png` | 중앙 처리 방향 표시와 기능 그룹 | 작은 패널 중첩을 없애고 표준/compact 로터리 위계로 density를 정리 |
| `03_Fresh_Air.png` | 즉시 보이는 주요 조작과 넉넉한 판독성 | 밝은 소비자 제품 재질 대신 TB dark outboard로 재해석 |
| `04_Convolver.png` | 작은 창에서도 명확한 단위와 실용성 | 지나치게 좁은 폭과 host-native 인상은 배제 |
| `05_Magma_BB_Tubes.png` | 촉각적 노브, 매입 meter, 일관된 조명 | 빈티지 장비 복제와 과도한 장식은 배제 |

### 10.3 TB_Delay 시맨틱 팔레트

TB Hub의 넓은 중성 면과 제한된 warm/sage 포인트를 어두운 outboard 재질로
재해석한다. 섀시와 컨트롤은 warm graphite로 통일하고, Aged Brass는 조작 포인터와
L/forward 신호에, Smoked Sage는 R/return 애널라이저 신호에만 제한적으로 사용한다.

| 토큰 | HEX | 역할 |
| --- | --- | --- |
| `canvas-top` | `#292A27` | smoked warm-graphite chassis |
| `canvas-bottom` | `#111310` | chassis depth |
| `surface-top` | `#31312D` | raised graphite control panel |
| `surface-bottom` | `#1A1C19` | panel depth |
| `display-top` | `#18201C` | smoked Delay Map glass |
| `display-bottom` | `#090D0B` | display depth |
| `text-primary` | `#E9E4D8` | product name / primary text |
| `text-secondary` | `#AAA79D` | label / scale |
| `value-text` | `#D9CCB2` | values |
| `accent-primary` | `#BDAA7F` | Aged Brass pointer / L-forward signal |
| `accent-secondary` | `#748C72` | Smoked Sage R-return signal |
| `section-accent` | `#BDAA7F` | restrained short section line |
| `outline` | `#484B45` | panel and bezel |
| `deep-shadow` | `#060806` | contact shadow |
| `highlight` | `#74776F` | graphite reflection |

Palette는 TB Hub와 FabFilter Timeless 3, Valhalla Delay, EchoBoy, Efx Fragments의
실제 제품 화면을 비교하고 800×700 software capture에서 확정했다. 고채도 violet의
전체 화면 점유는 회귀 테스트에서 0.2% 미만으로 제한한다.

### 10.4 디자이너 제작 에셋 계획

```text
material-base: smoked warm-graphite chassis with fine mineral grain
depth-layer: Delay Map bezel/glass reflection
control-part: TB_Delay 전용 knob cap/rim master
state-variant: FREE/SYNC/TUNE segmented selector, mode-swapped DELAY TIME, Freeze pressed state
dynamic-layer: echo ticks, grain clouds, decay triangles (코드로 실제 상태에 연결)
brand-metaphor: 우상단 물고기 원형 + 왼쪽으로 이어지는 opacity 0.24/0.12/0.065 세 분신
visualization-overlay: L/R lanes, linear 10 ms–5 s grid, primary anchors, pitch/shift direction markers

마스터 위치: Source/Assets/Masters/
출력 위치: Source/Assets/
기준 배율: 1x 및 2x, transparent asset는 alpha 포함
```

## 11. Factory 프리셋 방향

```text
Init 상태:
  1/8 dotted Ping-Pong Tape, feedback 48%, mild age/modulation/smear,
  mix 30%, duck 25%. 삽입 즉시 움직이는 안전한 echo가 들리는 시작점.

기존 공개 program 0–99는 그대로 보존하고, 다음 후보에서는 2개 논리 폴더를 뒤에
append해 총 120개로 확장한다. 각 폴더는 기본 이론·실전 utility·독립 parameter corner·
창의 hybrid를 섞으며, 이름 바꾸기·Mix/Output만 다른 사실상 중복은 만들지 않음.
브라우저 표시: 12개 Factory 그룹과 USER를 1단계 논리 폴더로, 각 프리셋을
2단계 하위 항목으로 표시. Factory 120개를 루트에 평면 나열하지 않음.

대표 사용 사례:
  utility echo, slap/ADT, sync multi-tap groove, Ping-Pong/Ratio stereo,
  Tape/BBD, Digital/Lo-Fi, tuned comb/flange, frequency-shift spiral,
  diffuse ambient tail, pitched/reverse/freeze-ready grain.

Cold-start Freeze Factory preset:
  모든 Factory preset은 `FREEZE=Off`로 저장한다. `Freeze Ready Constellation`과
  `Freeze Ready Reverse Swell`도 source가 loop에 먼저 채워진 뒤 사용자가 Freeze를
  켜 기존 loop를 유지하는 준비 상태다.

사용자 프리셋 확장자: .tbdelay
랜덤 정책: grain PRNG는 reset/prepare와 offline render에서 고정 seed로 재현 가능.
```

Factory 폴더와 프리셋 카탈로그:

| 순서 | 논리 폴더 | 음악적 초점 | 프리셋 10개 |
| ---: | --- | --- | --- |
| 1 | `ESSENTIAL ECHO` | 안전한 기본 echo와 주요 topology | Init, Clean Utility, Vocal Pocket, Wide Dual Guitar, Classic Ping Pong, Golden Ratio, Tape Slap, Dub Throw, Mono Safe Quarter, Clean Dotted Eighth |
| 2 | `SLAP & DOUBLE` | 18–120 ms slap, ADT, chorus와 microshift | Studio Slap 80, Vintage Slap 120, Dark Vocal Slap, Bright Guitar Slap, Stereo Double 18, Loose Double 28, Microshift Double, Stereo Chorus Drift, Reverse Tail Slap, Wide ADT |
| 3 | `RHYTHM & MULTITAP` | division, tap 수, spacing과 swing | Straight Eighth Grid, Dotted Pulse Train, Triplet Relay, Sixteenth Cascade, Quarter Note Call, Late Swing Taps, Early Swing Taps, Contracting Steps, Expanding Steps, Eight Tap Machine |
| 4 | `PING PONG & STEREO` | Single/Dual/Ping-Pong/Ratio와 L/R 시간·폭 | Narrow Relay, Panoramic Relay, Lazy Right Return, Fast Right Return, Dual Counterstep, Ratio Crosscurrent, Opposed Orbit, Diffuse Ping Pong, Mono Input Expander, Stereo Dub Circle |
| 5 | `TAPE & ANALOG` | Tape/BBD 포화, wear, wow와 flutter | Fresh Tape Repeat, Warm Reel Echo, Worn Tape, Saturated Tape Loop, Slow Wow Memory, Fast Flutter Reel, Broken Cassette, BBD Haze, BBD Chorus Echo, Dark Bucket Trail |
| 6 | `DIGITAL & LO-FI` | 깨끗한 Digital부터 bit/sample-hold와 stutter | Pristine Digital, Digital Crunch, 12-Bit Repeat, 6-Bit Collapse, Clocked Ping Pong, Telephone Memory, Narrowband Pager, Lo-Fi Stutter, Buffer Skip, Pixel Dust |
| 7 | `COMB & RESONANCE` | 10–30 ms tuned resonance와 flange | Comb Bloom, Hollow Notch, Plucked Wire, Glass String, Bass Tube, Damped Resonator, Subtle Flange, Jet Flange, Stereo Comb Split, Digital Bell Bank |
| 8 | `SPIRAL & SHIFT` | Loop/Post signed shift와 L/R 반대 방향 | Spiral Ascender, Spiral Descender, Mirror Orbit, Metallic Barber Pole, Gentle Lift, Gentle Fall, Opposed Currents, Post Shift Halo, Ping Pong Helix, Grain Shift Dust |
| 9 | `SPACE & DIFFUSION` | multi-tap onset 뒤의 diffusion과 ambient tail | Small Room Bloom, Diffuse Chamber, Dark Hallway, Bright Plate Trail, Eight Tap Nebula, Wide Ambient Wash, Mono Fog, Rhythmic Bloom, Deep Space Return, Ducking Atmosphere |
| 10 | `GRAIN & FREEZE` | hidden Density/Spray를 활용한 pitch/reverse grain | Grain Cloud, Reverse Cloud, Freeze Ready Constellation, Scatter Echo, Micrograin Chorus, Tape Granules, Freeze Ready Reverse Swell, Shimmer Twelve, Falling Fifth, Two Octave Spark |
| 11 | `PERFORMANCE & UTILITY` | 1/32·폴리리듬·ducking·Haas·controlled feedback·저역 공진 | Vocal Throw Quarter, Long Feedback Safe, Mono Haas Bloom, True L/R Ping, Tight 1/32 Grid, 3:2 Polyrhythm, Ducking Drum Gate, Clean BBD Clock, Controlled Self-Osc, Bass Resonant Notch |
| 12 | `HYBRID & EXTREME` | Tape Stop·negative comb·Loop/Post shift·macro·grain pitch·Freeze 준비 | Tape Stop Throw, Negative Comb Mirror, Post Ascender Throw, Loop Descender Throw, Reverse Bloom, Shift+Grain Mirror, Tight Micrograin, Chaotic Spray, Octave Cloud, Wash Freeze Bed |

## 12. 검증 계획

| 검증 항목 | 방법 | 입력 | 통과 기준 |
| --- | --- | --- | --- |
| 기본 delay | Harness impulse | inline impulse | target time ±2 samples, early leak -90 dB 이하 |
| Dual/Ping-Pong | stereo 및 Mono→Stereo impulse | inline impulse | mono-summed L seed에서 L-R-L 반복 교대와 feedback 비율 일치 |
| Comb | impulse + sine | 500/750 Hz | 12 ms급 delay 위치·부호, 공진 차이 |
| Tape | sine + two-tone | 220 Hz, 500/8000 Hz | 배음 증가와 반복별 고역 감쇠 |
| Shift | sine burst | 1500 Hz | 매 echo ±shift Hz 누적, mirror sideband 억제 |
| Grain | sine/noise burst와 8초 history marker | 220 Hz | pitch 방향, spray 길이, reset 결정성, history wrap·click 없음 |
| Diffusion | impulse | inline impulse | onset 보존과 energy span 증가 |
| Feedback safety | 20 s corner sweep | impulse/noise | finite, bounded, DC와 후반 증가 없음 |
| Freeze | history prefill 및 10/100/500 ms cold-start Freeze | sine/isolated impulse | 진입 레벨 ±3 dB, 4초 drift 1.5 dB 이하, 단발음 자동 capture, hold 중 diffusion 재누적 없음 |
| 모듈 바이패스 | 극단값 중립 동등성 및 자동화 왕복 | impulse/sine | 세 중립 경로와 동등, click-bounded, Freeze 독립 |
| Factory bank | compile-time bank audit, APVTS program API, 9.25 s full-bank render, UI menu walk | 120 states, stereo impulses | 12×10 순서·이름·전체 상태 고유, 기존 0–99 program 보존, 범위·choice 정수 유효, 전부 Freeze Off, finite, peak 8.0 이하, 모든 폴더 경계 탐색 |
| UI 구조 | 800×700 및 150% software snapshot과 component contract | default/Free/Sync/Tune/active Map | 좌측 제품명·중앙 preset·우측 3-copy logo·랙 나사, 네 양각 panel, 13 rotary body+2 slider, 기술 계수 hit target 부재, mode별 DELAY TIME, 10 ms–5 s 선형 map |
| TAP | timed UI command와 전체 parameter snapshot | Free/Sync, 100%/150%, Space | 첫 탭 전체 상태 불변, 두 번째 Free interval 반영, Sync 최근접 division, mode 유지, TIME MODE와 동일 폭·6/9 px 간격, Space 비소비, host parameter 38개 유지 |
| 매크로 | 최소/BASE/최대 coefficient와 Map 응답 | COLOR/SPACE | BASE exact, 상세 APVTS 값 불변, 명시한 방향으로 단조 변화, SPACE와 Map의 effective 값 일치 |
| 상태/프리셋 | Save/restore/roundtrip와 legacy fixture | 38/36/33 parameter state | 전체 값 복원, schema 1과 legacy는 새 매크로 BASE로 이행, 손상·미래 버전에서 현재 상태 보존 |
| 청감 가치 | level-matched A/B | KEY_01, GTR_01, DRM_01 | 두 적합 소스에서 선택 이유, 드럼에서 상한 확인 |

## 13. 식별자와 빌드

```text
회사명: Team Impulse Impact
버전: 1.0.1
Project ID: TbDly1
Plugin Code: TbDl
Manufacturer Code: TbSd
Bundle Identifier: com.TeamImpulseImpact.TB_Delay
VST3 Category: Fx|Delay
Formats: Standalone, VST3
Windows architecture: x64
JUCE: 8.0.12
State schema: 2, previous schema: 1, legacy schema: 0
Preset format: 3, previous format: 2, legacy format: 1
```

Plugin Code와 Parameter ID는 첫 공개 후 변경하지 않는다.

## 14. 변경 기록

| 날짜 | 변경 내용 | 변경 이유 | 영향 범위 |
| --- | --- | --- | --- |
| 2026-07-27 | 최초 작성, 25-knob/8-selector 구조와 필수 효과 확정 | 사용자 요구와 공통 가이드 반영 | 전체 제품 |
| 2026-07-27 | 8 rotary·17 slider를 단일 보드의 2×2 사분면으로 재구성 | 탭 제거 후 모든 컨트롤의 동시 가시성과 음악적 역할 구분 확보 | UX·UI |
| 2026-07-27 | 적은 선택지를 4 stepped rotary·2 binary toggle로 전환하고 DIVISION만 dropdown 유지 | 빈 selector 공간 제거와 직접 조작 가시성 향상 | UX·UI |
| 2026-07-27 | FINAL MIX footer와 3개 모듈 bypass, state schema 1/preset v2 migration 추가 | Stereo/Mix 역할 분리와 섹션별 비교·호환성 확보 | DSP·UI·상태 |
| 2026-07-27 | PANIC UI 제거, DUCK을 DELAY / RHYTHM으로 이동하고 COLOR / SPACE 표시명·2초 툴팁 정리 | 기능 소속과 용어의 직관성 개선 | UX·UI |
| 2026-07-27 | Delay Map 첫 L/R anchor를 중심 정렬된 5.5 px 막대로 확대 | 첫 딜레이 시점의 가독성과 여유 개선 | Analyzer UI |
| 2026-07-28 | violet 전면 강조를 warm graphite·Aged Brass·Smoked Sage 디자인 시스템으로 교체 | TB Hub의 중성 면·제한된 포인트와 상용 delay의 재질·위계를 dark outboard UI로 적용 | Visual design |
| 2026-07-28 | COLOR / SPACE의 LOW CUT·HIGH CUT·SPEED·WOBBLE·SPACE를 로터리로 전환 | 음색 조형 파트의 조작 방식과 한 줄 실루엣 통일 | UX·UI |
| 2026-07-28 | 모든 연속 슬라이더를 로터리로 전환하고 MIX·OUTPUT에 compact rotary 적용 | 전체 조작 문법 통일과 Final Mix 분리 유지 | UX·UI |
| 2026-07-29 | value arc를 두껍게 채우고 DIVERGE를 Stereo로 이동, Color 2-row와 Grain 5-knob로 재편 | 값의 에너지 가시성, 섹션 밀도 균형, 과처리 인상 완화 | UX·UI |
| 2026-07-29 | Hero→Structure→Primary→Standard→Supporting→Preset-only 위계와 노출·행 분리 기준을 명문화 | 크기만으로 중요도를 표현하지 않고 시선·작업 순서·과처리 억제를 일관되게 유지 | Design documentation |
| 2026-07-29 | Factory bank를 10개 논리 폴더 × 10개, 총 100개로 확장하고 전수 오디오 안전성 검증 추가 | 프리셋 탐색성, 상업적 활용 범위와 preset-only Grain 파라미터의 음악적 가치 확대 | Preset·QA |
| 2026-10-02 | 공개 0–99 program을 보존한 채 `PERFORMANCE & UTILITY`와 `HYBRID & EXTREME` 20개를 append | 1/32·폴리리듬·Haas·negative comb·controlled feedback·macro 방향·shift/grain hybrid를 실제 Factory에서 audition | Preset·호환성·QA |
| 2026-07-29 | FINAL panel·문구를 제거하고 MIX/OUTPUT을 filled horizontal slider로 전환 | 마무리 컨트롤의 선형 판독성과 화면 호흡 개선 | UX·UI |
| 2026-07-29 | TIME MODE를 FREE/SYNC/TUNE 분할 토글로 만들고 동일 DELAY TIME tile에 ms·음표·Hz 표시를 통합, TIME MOTION을 TIME CHANGE로 명확화 | 중복 컨트롤과 구현 용어를 줄이면서 Tune preset 호환 보존 | UX·상태 |
| 2026-07-29 | Delay Map을 10 ms–5 s로 확장하고 실제 베젤 폭을 control board와 일치시킴 | 긴 delay 가시성과 외곽 정렬 오류 수정 | Analyzer UI |
| 2026-07-29 | 독립 랙 헤더를 좌측 굵은 제품명·중앙 프리셋·우측 분신 물고기 로고·양끝 나사로 재구성 | 제품 식별, 탐색, 브랜드 은유의 위계를 분리 | Visual design |
| 2026-08-13 | 메인 UI를 4개 결과 중심 사분면과 13개 로터리 body로 축소하고, 상세 15개와 TIME CHANGE를 preset-only로 전환 | 구현 계수와 음악적 결정을 분리해 선택 피로와 과처리 인상을 줄임 | UX·UI·호환성 |
| 2026-08-13 | `COLOR`, `SPACE` 두 host macro를 append하고 BASE에서 기존 36개 상태의 소리를 보존하는 schema 2/preset v3 이행 추가 | 새 개발 가이드의 안정 매크로 원칙과 기존 자동화 호환을 함께 충족 | DSP·상태·Preset |
| 2026-08-13 | Freeze 단발 transient capture, hold 중 diffusion 재누적, 10 ms Tape/BBD modulation headroom 수정 | 실제 사용에서 Freeze가 비거나 변질되는 경계와 최소 딜레이 비대칭 방지 | DSP·QA |
| 2026-08-22 | TIME MODE 아래 UI-only `TAP` 명령 추가. Free/Tune은 time, Sync는 host BPM의 최근접 division을 갱신 | 모드를 바꾸거나 새 상태 계약을 만들지 않고 손으로 반복 간격을 입력 | UX·UI·호환성 |
| 2026-10-01 | Delay Map을 원형 ripple에서 10 ms–5 s 선형 L/R 타임라인으로 전환하고, grain pitch/overflow 표식과 TONE·DIFFUSION·ECHOES·SHIFT ROUTE 등 결과 중심 표시명을 적용 | 첫 echo 위치·긴 tail·반복 개수·라우팅·grain 음높이를 화면에서 즉시 읽고 기존 host ID는 유지 | UX·Analyzer·호환성 |
