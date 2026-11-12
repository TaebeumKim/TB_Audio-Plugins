# TB_Delay QA 기록

상태: **HOLD — Release x64 빌드·자동 검증·호스트 계약·그래픽 게이트 통과, 최종 출하 전 사람·DAW·성능 게이트 대기**

기록일: 2026-10-02  
제품 버전: 1.0.3  
대상: Windows x64 Standalone / VST3

3절부터 5절까지의 산출물 SHA-256과 UI 캡처 수치는 현재 로컬 `1.0.3` Release 후보를
기준으로 한다. 공개 카탈로그는 변경하지 않았고, 직관성 개선 후보의 상세 기록은 9.4를 따른다.

## 1. 제품 식별 정보

| 항목 | 값 |
| --- | --- |
| 제품명 | TB_Delay |
| Project ID | `TbDly1` |
| Plugin Code | `TbDl` |
| Manufacturer Code | `TbSd` |
| Bundle Identifier | `com.TeamImpulseImpact.TB_Delay` |
| VST3 Category | `Fx\|Delay` |
| JUCE | 8.0.12 |
| 빌드 도구 | Visual Studio 2022 Community, x64 MSBuild |
| 검증 OS | Windows 11 Pro 64-bit, 10.0.26200 |
| 검증 CPU | AMD Ryzen 7 260 w/ Radeon 780M Graphics |

## 2. 구현 범위 확인

- 공개 편집 화면: 실제 rotary body 13개 + 수평 슬라이더 2개
  - `REPEAT`: 통합 `DELAY TIME`, `FEEDBACK`, `ECHOES`, `DUCK`
  - `STEREO`: `STYLE`, `WIDTH`
  - `CHARACTER / SPACE`: `CHARACTER`, `TONE`, `DIFFUSION`
  - `TRANSFORM`: `SHIFT`, `GRAIN`, `PITCH`, `REVERSE`
  - 최하단 `MIX`, `OUTPUT`은 별도 패널이나 `FINAL` 문구 없는 filled horizontal slider
- 결과 셀렉터·토글:
  - dropdown `TIME MODE`: `FREE / SYNC / TUNE`
  - `TIME MODE` 아래 UI-only momentary `TAP`; 현재 모드를 유지한 채 Free/Tune은
    `time`, Sync는 host BPM 기준 최근접 `division`을 갱신
  - UI 결과명 `SHIFT ROUTE: LOOP / OUTPUT`; 호스트 choice metadata는 호환을 위해
    `Loop / Post` 유지
  - `FREEZE`와 `STEREO / CHARACTER·SPACE / TRANSFORM`의 작은 `BYP`
  - dropdown 2개, `TIME CHANGE`는 preset-only
- preset-only 상세 계수: 연속 15개 + `TIME CHANGE`
- 자동화·상태 파라미터: 38개
  - 기존 36개 뒤에 `color_macro`, `space_macro`를 append
  - 두 매크로는 `-100…+100`, `0=BASE`이며 BASE에서 기존 소리를 정확히 보존
- Factory preset: 120개, 12개 논리 폴더 × 10개
- 별도 utility: Factory/User 통합 preset browser, 이전/다음, Save, Undo/Redo
- 필수 효과:
  - 일반/멀티탭 echo
  - tuned comb와 signed feedback polarity
  - Tape/BBD/Digital/Clean character
  - Dual/Ping-Pong/Ratio topology
  - feedback-loop 누적 또는 Post frequency shift 상승·하강
  - 8 voice overlapping Hann grains, spray/pitch/reverse
  - diffusion, ducking, freeze
- 추가 조합 preset:
  - slap/ADT, chorus, flanger, resonator, dub throw, rhythmic multi-tap
  - Ping-Pong/Ratio stereo, Tape/BBD, Digital/Lo-Fi, diffuse delay
  - shimmer/falling harmony, barber spiral, reverse cloud, lo-fi stutter, broken tape

상세 컨트롤 범위와 기본값은 `PRODUCT_BRIEF.md`, 알고리즘 근거와 제한은
`PROCESSING_RESEARCH.md`를 기준으로 한다.

## 3. 현재 Debug 및 Release 산출물

```text
Standalone:
Builds/VisualStudio2022/x64/Debug/Standalone Plugin/TB_Delay.exe

VST3 bundle:
Builds/VisualStudio2022/x64/Debug/VST3/TB_Delay.vst3

VST3 binary:
Builds/VisualStudio2022/x64/Debug/VST3/TB_Delay.vst3/Contents/x86_64-win/TB_Delay.vst3

Test harness:
Tests/Builds/VisualStudio2022/x64/Debug/ConsoleApp/TB_Delay_Tests.exe

Release Standalone:
Builds/VisualStudio2022/x64/Release/Standalone Plugin/TB_Delay.exe

Release VST3 bundle:
Builds/VisualStudio2022/x64/Release/VST3/TB_Delay.vst3

Release VST3 binary:
Builds/VisualStudio2022/x64/Release/VST3/TB_Delay.vst3/Contents/x86_64-win/TB_Delay.vst3

Release test harness:
Tests/Builds/VisualStudio2022/x64/Release/ConsoleApp/TB_Delay_Tests.exe
```

- Projucer 프로젝트 재저장 후 64-bit MSBuild 전체 Debug/Release 빌드 성공
- VST3 manifest helper 성공
- Release `moduleinfo.json` 제품명 `TB_Delay`, 버전 `1.0.3`, 카테고리 `Fx/Delay` 확인
  - Processor CID: `ABCDEF019182FAEB546253645462446C`
  - Controller CID: `ABCDEF011234ABCD546253645462446C`
- Debug/Release Standalone hidden 3초 기동 smoke test 통과

Release SHA-256:

| 파일 | SHA-256 |
| --- | --- |
| Standalone | `B57879BADEFF0E80258752FE03C6E57F935F92CBF823FB5AF77B35473FB3D71C` |
| VST3 내부 binary | `45B57B5FF89B9BFA271E2282DB9DB23B242377B07586FA0420504882238B6FFD` |
| `moduleinfo.json` | `AAABF075E493FBA092E11AB59E1A4C4A22E40BBB5779E85DB69555475B7ECA43` |
| Release harness | `67DDDA919C75FC95924A6EFC58D8E45FD883F98D7E7FFFB9CF5CBCC25EC4D69B` |

## 4. 자동 검증 결과

Debug와 Release 최종 실행:

```text
Debug TB_Delay_Tests.exe <Debug TB_Delay.vst3 bundle>
SUMMARY pass=30 fail=0 skip=0

Release TB_Delay_Tests.exe <Release TB_Delay.vst3 bundle>
SUMMARY pass=30 fail=0 skip=0
```

통과 항목:

- 38개 parameter ID·기본값·중복 검사와 `TIME` 10–8000 ms 범위
- `COLOR`, `SPACE`의 BASE exact-preserve, 방향별 파생 계수와 raw 상세값 비변경
- 120개 Factory preset의 범위·choice 정수, 이름·전체 상태·비레벨 차이 고유성,
  host program API 일치, cold-start Freeze 0개와 기존 0–99 macro BASE 보존
- 추가된 100–119 Factory preset은 의도적인 COLOR/SPACE macro 방향을 포함하며,
  Factory 120개 각각의 9.25초 독립 렌더가 finite/bounded, 최대 peak `0.334528`
  (`Vocal Pocket`)
- Clean/Dual impulse timing 오차 2 samples 이내
- 10 ms 최소 TIME의 첫 echo 480 samples, 선행 누설 없음과 Tape modulation floor 보장(48 kHz)
- `TIME CHANGE` Crossfade/Glide의 100→600 ms 자동화 finite/click bound와 경로 차이
- 중앙 stereo 입력과 Mono→Stereo 입력의 Ping-Pong L-R-L 교대
- `Classic Ping Pong` Factory preset의 3회 반복 채널 우세 13.98 dB
- Tune comb 주기와 feedback polarity
- ±100 Hz frequency shift 방향과 mirror rejection
  - 48/96/192 kHz에서 약 57–58 dB
- grain reset 결정성과 8초 history 경계 reverse-grain recent marker 누설 0
- 20초 feedback 극단 조합 finite/bounded
- history를 채운 뒤 10/100/500 ms Freeze 진입 변화 최대 -2.49 dB, 4초 유지 drift 0.00 dB
- Freeze On cold-start 연속음과 단발 impulse를 10/100/500 ms에서 capture하고 유지
- Smear/Size가 큰 상태에서도 full Freeze 중 diffusion 재누적 없이 drift 0.00 dB
- 내부 delay-memory generation 무효화 경로의 이전 tail 완전 제거
- mono/stereo, 44.1–192 kHz, block 1–1024 matrix
- DAW state round trip, 손상·미래 schema 거부, 36/33-parameter 상태를 38개로 이행
- 사용자 `.tbdelay` v3 round trip, v2/v1 이행, User→Custom 표시 상태 전이,
  손상·미래 버전 파일 거부
- Factory preset 1회 Undo/Redo transaction
- 세 모듈 바이패스의 극단값 중립 동등성, 자동화 왕복 click bound,
  `FREEZE` 독립 동작
- Delay Map 예측 모델의 L/R 시간, Ping-Pong 세대, tap spacing/swing,
  경로별 Build/Once shift, grain forward-safety·reverse support·pitch 누적,
  8초 grain trajectory 경계, 바이패스 중립 예측, 마지막 tap 기준 -60 dB tail,
  mono lane, 음수 feedback polarity, macro 파생값과 minimum-time modulation clamp
- 800×700 UI, 좌측 제품명·중앙 preset·우측 3중 delay 물고기 로고와 랙 나사의 독립 헤더,
  가이드 헤더 순서·폭·아이콘 타입·Load 제거, parameter-driven Delay Map의 10 ms–5 s 축과
  control board 일치 painted width, 첫 L/R anchor, 2×2로 배치한 4개 양각 패널,
  rotary body 13개 + slider 2개, 숨긴 15개 계수의 hit target 부재, 패널 없는 `MIX`/`OUTPUT`
- 공용 실제 음원 3종 Dry/Wet/RMS-matched 렌더
- VST3 scan, instantiate, 실제 main bus 2-in/2-out와 1-in/2-out, silence finite
- 기존 Release의 36 APVTS + wrapper Bypass/Program native ParamID·이름·단위·Choice·flags
  `38/38` 완전 일치, 새 Color/Space 2개만 충돌 없이 append
- host contract probe의 metadata/set-get/state round trip 통과. 종료 코드 1은 기존 `Low Cut`
  로그 범위 text 왕복의 `120.0000229→120.0000458` 부동소수 오차만 원인

JUCE의 VST3 description metadata는 동적 bus에 대해 `0→0`을 반환했지만, 생성된
실제 instance의 main bus는 기본 `2→2`, Mono→Stereo 전환 시 `1→2`로 확인했다.

## 5. UI QA

최종 software capture:

```text
Tests/output/ui-qa/TB_Delay_UI.png
800×700, 84,346 bytes

Tests/output/ui-qa/TB_Delay_UI_DelayMap_Active.png
800×700, 98,355 bytes
```

확인 완료:

- 독립 랙 헤더는 좌측 22 px bold `TB_DELAY`, 중앙 preset 이전/다음·저장·Undo/Redo,
  우측 3단 delay 물고기 로고와 양 끝 나사 순으로 위계가 분리됨
- 프리셋은 Factory/User 폴더를 먼저 고른 뒤 leaf를 선택하며, 별도 Load 버튼과
  Space 단축키 소비가 없음
- Delay Map과 control board의 painted x/width가 동일한 `924 px`; 축은 `10 ms–5.0 s`
- 입력 원점, 위 L/아래 R lane, 첫 echo의 넓은 anchor, pitch 높이, grain support,
  feedback decay triangle을 parameter preview로 표시
- Map은 raw 상세값이 아니라 DSP와 같은 effective macro 파생값을 사용하며,
  `DIFFUSION` 조작과 time/style/tap/shift/grain/bypass 변경에 즉시 반응
- 2×2 양각 패널은 `REPEAT / STEREO / CHARACTER·SPACE / TRANSFORM`으로 구성
- 실제 rotary body 13개와 `MIX`/`OUTPUT` slider 2개만 노출; 숨긴 상세 계수 15개와
  `TIME CHANGE`는 editor tree·bounds·hit target이 없음
- `TONE`: `CLEAN ↔ BASE ↔ WORN`, `DIFFUSION`: `TIGHT ↔ BASE ↔ WASH`로 방향을 표시하고
  2초 hover tooltip에서 조작 결과를 설명
- UI의 `DIVISION / ECHOES / CHARACTER / TONE / DIFFUSION / SHIFT / SHIFT ROUTE`는
  결과 중심 표현이며 호스트 parameter ID와 choice metadata는 그대로 유지
- 3개 module `BYP`는 제목선의 supporting utility이고, Freeze는 Transform bypass와 독립
- `FINAL` 패널·문구 없이 같은 규격의 9 px amount track을 갖는 `MIX`/`OUTPUT` footer
- vivid purple pixels `0`, 기본 크기에서 label/control clipping `0`
- 기본 캡처 SHA-256:
  `BC6738A0FBC6CBB8B65FF53A1703B67620D7FABB7EC521621B7EB99EE8C453AC`
- 활성 Delay Map 캡처 SHA-256:
  `4AED71F8BA9357BBA3F96116CA7B4663445BC84061A0196F471422C73078773B`

## 6. 청감 A/B 렌더

자동 검증에서는 세 pair 모두 active-region RMS 오차가 0.000 dB이며 NaN/Inf가 없다.
Dry·Wet·RMS-matched Wet 전체에 pair별 동일 gain을 적용해 저장 피크도 0.98 이하로
확보했다. 이 수치는 레벨 정렬과 무클리핑 파일만 증명하고 제품 가치나 음질을 대신
판정하지 않는다.

| 소스 | preset | Dry | 레벨 매칭 Wet | 자동 결과 |
| --- | --- | --- | --- | --- |
| `KEY_01_pad.wav` | Init | `KEY_01_pad__Init__Dry.wav` | `KEY_01_pad__Init__Wet_RMS_Matched.wav` | 0.000 dB |
| `GTR_01_acoustic.wav` | Spiral Ascender | `GTR_01_acoustic__Spiral_Ascender__Dry.wav` | `GTR_01_acoustic__Spiral_Ascender__Wet_RMS_Matched.wav` | -0.000 dB |
| `DRM_01_loop.wav` | Grain Cloud | `DRM_01_loop__Grain_Cloud__Dry.wav` | `DRM_01_loop__Grain_Cloud__Wet_RMS_Matched.wav` | -0.000 dB |

모든 파일은 `Tests/output/ab/`에 있다.

사람의 청감 확인 항목:

- [ ] Init: 반복이 원음을 불필요하게 흐리지 않고 상업적으로 쓸 수 있는가
- [ ] Spiral Ascender: 상승 shift가 명확하고 mirror/거친 고역이 거슬리지 않는가
- [ ] Grain Cloud: grain이 리듬을 완전히 무너뜨리지 않으면서 복합적인가
- [ ] 세 pair 모두 단순 loudness 차이가 아닌 질적 가치가 있는가
- [ ] click, zipper noise, 비정상 DC/runaway가 들리지 않는가

판정: **PENDING — 사람의 레벨 매칭 청감 A/B 필요**

## 7. 실시간 안전성과 수정 기록

- process 경로의 동적 allocation/lock 없음
- UI에 노출하지 않는 내부 진단 경로는 큰 delay buffer를 audio thread에서 clear하지 않고
  generation tag로 O(1) 무효화
- 해당 내부 경로의 diffuser 잔여 상태는 작은 고정 작업으로 분할 정리
- Tape wow phase wrap 불연속 제거
- feedback 100–110% 영역을 실제 unity 초과 영역으로 연결
- feedback polarity, `MOD DEPTH`, `SPACING`, `SWING` sample smoothing
- 사용자 modulation과 Tape/BBD wow/flutter의 합을 현재 active read head의
  10 ms minimum headroom 안으로 최종 제한
- Ping-Pong 입력을 mono-summed L seed로 통일하고 cross-feedback으로 실제 L-R-L 교대
- Freeze 입력 차단과 unity feedback을 equal-power로 교차하고 loop character/filter/diffusion을 우회
- 빈 loop에서 Freeze가 On이면 첫 delay가 도착할 때까지 입력을 capture-arm하고,
  첫 단발 transient가 나타나는 sample을 unity feedback으로 기록
- slow/reverse grain의 전체 read trajectory를 8초 history 안으로 제한
- sample-rate-scaled Hilbert FIR과 zero-shift bypass
- Stereo, Color/Space, Grain/Transform 바이패스의 click-bounded smoothing과
  내부 delay/filter/diffuser 상태 보존
- 비정상 parameter/sample-rate 값 finite fallback
- preset 적용과 state snapshot에서 APVTS 안전 경로 사용
- DAW `stateSchemaVersion=2`, user preset `presetFormatVersion=3`
- schema 1의 36개 및 legacy 33개 state/preset은 두 macro를 BASE로 채워 이행

## 8. 알려진 제한

- 8초는 tap별 길이가 아니라 전체 delay history다. 매우 긴 base time에서는 뒤쪽
  multi-tap이 최대 지연 위치에 모일 수 있다.
- Mono→Mono, Mono→Stereo와 Stereo→Stereo를 지원한다. Stereo→Mono는 지원하지 않는다.
- Ping-Pong은 명확한 단일 교대 경로를 위해 stereo 입력을 mono-sum하므로,
  anti-phase stereo content는 seed 단계에서 상쇄될 수 있다.
- Tape는 상업적으로 쓸 수 있는 경량 character 모델이며 특정 기기의 1:1 물리 복각,
  oversampling 기반 정밀 모델은 아니다.
- 현재 단계에서 AAX/AU/CLAP, surround, external sidechain은 범위 밖이다.

## 8.1 2026-08-19 재작업 이후 재검증 필요 항목

`1.0.2` 후보에서 UI 정체성과 feedback 안전 동작이 바뀌었으므로 아래는 이전 `1.0.1`
결과를 재사용할 수 없다.

- [x] 무입력 runaway 수정의 사람 청감 확인 (2026-08-19 사용자 확인: "오디오 체크 문제없음"). 자동 검증
      `runaway feedback decays once the input stops`는 통과했고, 감시를 비활성화한
      역검증에서 11초간 `-0.97 dB`(사실상 무한 지속) → 활성 시 `-11.40 dB`로 확인했다.
- [x] 새 바다 팔레트와 선형 Delay Map의 Standalone software capture 시각 검수
- [ ] 800 × 700 기본 크기와 70%·180% 끝값에서 label/control clipping 재확인
- [ ] `TIME MODE`·`SHIFT ROUTE` 드롭다운과 `TAP`의 호스트별 동작 확인
- [x] 5절 UI QA 캡처와 SHA-256 갱신

`FREEZE`는 무입력 hold가 설계 의도이며 루프 게인이 unity를 넘지 않으므로 이번 무입력
감시 대상에서 제외했다. 사용자가 자리를 비운 사이의 지속음이 `FREEZE`였을 가능성은
청감 확인 시 함께 판단한다.

## 9. 최종 출하 전 남은 항목

- [ ] 위 3개 레벨 매칭 A/B 사람 청감 PASS
- [ ] 실제 Standalone에서 독립 랙 헤더, 4개 양각 패널, 통합 `DELAY TIME`,
  `TIME MODE` dropdown·하단 `TAP`과 패널 없는 `MIX`/`OUTPUT` slider row의 상호작용 수동 확인
- [ ] 실제 DAW 2종에서 VST3 scan/load/save/reopen/automation/bypass/offline render
- [ ] automation sweep 중 click/zipper 청감 확인
- [ ] CPU·장시간 memory·다중 instance 측정
- [ ] pluginval 또는 동등한 validator 실행
- [x] production vector 그래픽과 800×700 100% software capture 검증
- [ ] 실제 호스트에서 지원 DPI별 스케일·선명도 재검증
- [x] DAW 상태용 독립 `stateSchemaVersion`과 미래 버전 거부·마이그레이션 fixture 추가
- [x] VST3 CID·ParamID·이름·단위·Choice·automation flags 호스트 계약 스냅샷 추가
- [x] Projucer 재저장 후 Release SharedCode → Harness → Standalone → VST3 빌드
- [x] Release manifest/version, 내부 binary와 SHA-256 확인
- [ ] 패키지 설치·제거 확인

현재 최종 판정: **HOLD**

2026-08-19 기준 HOLD 사유에 8.1의 재검증 항목이 추가됐다. UI 정체성 전면 교체와
feedback 안전 동작 변경이 포함되어 자동 검증만으로는 출하 판정을 내릴 수 없다.

HOLD 사유는 알려진 DSP 또는 Release 빌드 실패가 아니라, 최종 출하 전에 필요한 사람의 청감 A/B,
실제 DAW 통합, 성능, 설치 및 호스트별 DPI QA가 아직 수행되지 않았기 때문이다.

## 9.1 2026-08-19 공개 기록

`1.0.2`를 사용자의 명시적 지시에 따라 위 수동 QA 완료 전에 공개했다.

- 저장소: `TaebeumKim/TB_Audio-Plugins` · PR #69 · 병합 `9dafa2c`
- 배포 ZIP SHA-256: `891b654bd53ba21d703dfa3d1072bcc152cec50f341b6e15c8f06e0eefe82a00` (2,427,015 B)
- 자동 검증: Debug/Release `29/29`, 버전 `1.0.2` 정렬, VST3 manifest 생성 확인
- 미완료 상태로 공개한 항목: 레벨 매칭 청감 A/B, 실 DAW 통합, pluginval,
  성능·다중 인스턴스, 설치·제거, DPI 재검증
- `raw.githubusercontent.com`의 `catalog.json`은 병합 직후 약 5분간 이전 버전을
  캐시로 서빙한다. 이 구간에는 Hub가 새 ZIP을 옛 SHA-256으로 검증해 실패할 수 있으므로
  배포 직후 전파 확인이 필요하다.

## 9.2 2026-08-21 `1.0.3` 공개 기록

`1.0.3`을 사용자의 명시적 지시에 따라 위 수동 QA 완료 전에 공개했다.

- 저장소: `TaebeumKim/TB_Audio-Plugins` · PR #71 · 병합 `84b48bf` (squash)
- 배포 ZIP SHA-256: `c783f8d67732fdf52396afb0eb701f2b954a5103d9da11f7983921bb004ab146` (2,542,969 B)
- 자동 검증: Debug `28/28`, Release `29/29`(VST3 scan/instantiate 포함), 버전 `1.0.3` 정렬,
  Projucer 재저장 후 VST3 manifest `1.0.3` 재생성 확인
- 이번 릴리즈에서 닫은 항목: 한글 툴팁·상태 문구의 Latin-1 오해석(mojibake). 실제 창에서
  깨짐을 재현한 뒤 수정했고, 보이는 모든 노브·셀렉터 툴팁이 실제 한글 코드포인트를
  포함하고 Latin-1 보충 영역 문자가 0개인지 검사하는 자동 테스트를 추가했다.
- 새로 검증이 필요한 항목: `70%`·`100%`·`180%` 배율에서의 실기 시각 검수와 호스트별
  리사이즈 동작. 자동 검증은 `150%`에서 Delay Map·노브가 같은 배율로 커지는지와
  저장된 배율이 세션 복원 시 되돌아오는지까지만 확인한다.
- 미완료 상태로 공개한 항목: 레벨 매칭 청감 A/B, 실 DAW 통합, pluginval,
  성능·다중 인스턴스, 설치·제거, DPI 재검증
- 병합 직후 `raw.githubusercontent.com` 전파를 확인했다. `catalog.json`은 `1.0.3`을
  서빙했고 `?v=1.0.3` ZIP의 크기·SHA-256과 내부 `moduleinfo.json` 버전이 일치해
  9.1에서 겪은 캐시 불일치 구간은 이번에는 관측되지 않았다.
- macOS 배포물은 이번 릴리즈 대상이 아니며 카탈로그에서 `1.0.2`를 유지한다.

## 9.3 2026-08-22 미출시 TAP 후보 검증

`TIME MODE` 바로 아래에 UI-only momentary `TAP`을 추가한 로컬 Release 후보를 만들었다.
공개 버전과 TB Hub 카탈로그는 `1.0.3`을 유지한다.

- 탭 계약: 첫 탭은 파라미터 불변, `100–3000 ms`의 최근 최대 4개 유효 간격을 평균,
  35%를 넘는 이탈 간격은 새 시퀀스로 재시작
- 모드 소유권: Free/Tune은 기존 `time`, Sync는 실제 host BPM에서 최근접 기존
  `division`만 갱신하고 `timeMode`는 유지
- 호스트 계약: TAP은 저장·자동화 파라미터가 아니며 기존 38개 parameter ID, state schema,
  VST3 CID와 preset format은 변경 없음
- DAW 입력: 버튼은 키보드 포커스를 잡지 않고 editor의 Space 입력도 소비하지 않음
- 배치: 800 × 700에서 TIME MODE와 같은 폭·하단 6 px, 150%에서 같은 폭·하단 9 px
- 자동 검증: Debug/Release VST3 scan·instantiate 포함 각각 `30/30 PASS`
- 타이밍 표본: Free `220.5 ms → 220.5 ms`, Sync 120 BPM `250 ms → 1/8`
- Release Standalone hidden 3초 기동 smoke test 통과
- 기본 캡처: `Tests/output/ui-qa/TB_Delay_UI.png`, 800 × 700, 109,353 B,
  SHA-256 `78246459E4A16247CE8C838E369E94E94BE57AFC9342B61F7CA99EF64A39FDC6`
- 150% 캡처: `Tests/output/ui-qa/TB_Delay_UI_Scaled_150.png`, 1200 × 1050, 163,317 B,
  SHA-256 `DCCB8F4940B005E5737C110E56B9D0303B8CC6C035DD930ADFB83CEB9B20BA74`
- Release Standalone SHA-256:
  `D7CF18C5C70EDEC117771B6EAC83AB961D0004E7850E3844B99A0144C3AF5A67`
- Release VST3 내부 binary SHA-256:
  `67231BE0BAD72779C564CFC8941B1DF479CC776B09B1C30787A0474F3AA55EFC`
- Release `moduleinfo.json`은 제품명 `TB_Delay`, 버전 `1.0.3`, `Fx/Delay`를 유지하며
  SHA-256은 `AAABF075E493FBA092E11AB59E1A4C4A22E40BBB5779E85DB69555475B7ECA43`이다.

## 9.4 2026-10-02 미출시 직관성 개선 검증

현재 공개 카탈로그와 제품 버전은 `1.0.3`을 유지한다. 이번 후보는 parameter 수,
host ID, state/preset schema를 변경하지 않고 표시와 preview의 읽기 순서만 개선했다.

- Delay Map을 원형 ripple에서 입력 기준선 + 위 L/아래 R lane의 선형 시간축으로 변경
- 시간축 범위는 실제 최소 `10 ms`부터 `5.0 s`; 500 ms, 1–5 s 눈금과 `>5 s` overflow를 표시
- 첫 echo는 항상 넓은 anchor bar로 표시하고, L/R·피치 방향·grain support·감쇄 triangle을
  같은 시간 좌표에 표시하며, grain cloud도 pitch stem/triangle과 `>5 s` overflow를 표시
- 내부 용어를 결과 중심 UI 이름으로 정리: `DIVISION`, `ECHOES`, `CHARACTER`, `TONE`,
  `DIFFUSION`, `SHIFT`, `SHIFT ROUTE`; 기술 metadata와 automation contract는 보존
- `BYP` supporting button 폭을 34 px로 조정해 기본 크기에서 clipping 없이 표시
- Debug/Release VST3 scan·instantiate 포함 각각 `30/30 PASS`
- Release Standalone hidden 3초 기동 smoke test: 실행 후 3초 생존 확인, 종료는 QA 종료를
  위해 수행
- 기본 캡처: `Tests/output/ui-qa/TB_Delay_UI.png`, 800 × 700, 84,346 B,
  SHA-256 `BC6738A0FBC6CBB8B65FF53A1703B67620D7FABB7EC521621B7EB99EE8C453AC`
- 150% 캡처: `Tests/output/ui-qa/TB_Delay_UI_Scaled_150.png`, 1200 × 1050, 131,310 B,
  SHA-256 `5233AD9F2B7C91054586BDD98667B50CFAE294706220BC320BA324BDA3491C30`
- Release Standalone SHA-256:
  `53212D13B6F7C174CD1CE6CA47E841A638940B901B98833F6A78AD1388CC36F4`
- Release VST3 내부 binary SHA-256:
  `241E61CE71A3DA008E2C46D47052C29E28F3A0A1366A09FE96F815563ABA9128`
- Release `moduleinfo.json`은 제품명 `TB_Delay`, 버전 `1.0.3`, `Fx/Delay`를 유지하며
  SHA-256은 `AAABF075E493FBA092E11AB59E1A4C4A22E40BBB5779E85DB69555475B7ECA43`이다.

## 9.5 2026-10-02 Factory 확장 후보

현재 공개 카탈로그와 제품 버전 `1.0.3`은 유지한다. 이번 후보는 기존 0–99 Factory
program의 이름·순서·상태를 변경하지 않고 100–119에만 20개를 append한 로컬 후보이다.

- `PERFORMANCE & UTILITY` 10개: 실전 throw, 안전한 장시간 feedback, Haas·L/R 분리,
  1/32 grid, 3:2 polyrhythm, ducking, BBD clock, controlled self-osc, tuned bass comb
- `HYBRID & EXTREME` 10개: tape stop, negative comb, Post/Loop shift throw, reverse,
  shift+grain, micrograin, spray, octave cloud, freeze bed
- 기존 0–99는 macro `BASE` 정책과 host recall을 보존하고, 새 100–119는 COLOR/SPACE
  macro 방향을 preset 자체로 들을 수 있게 했다. 모든 새 preset도 cold-start `FREEZE=Off`다.
- Debug/Release 하네스, UI folder 구조, 9.25초 전체 Factory render를 120개 기준으로 다시
  확인한다. 공개 전에는 이 후보를 카탈로그에 반영하지 않는다.

## 10. 선행 공개 기록

- 2026-08-13: 복잡도 재설계와 엔진 경계 수정이 반영된 `1.0.1` Release candidate를
  Debug/Release `28/28`, native host contract `38/38` 일치 상태로 생성했다.
- 2026-07-29: 사용자의 명시적 승인에 따라 자동 검증 `26/26`을 다시 통과한 최신 Release
  빌드를 TB Hub 카탈로그 `1.0.0`으로 먼저 공개했다.
- GitHub 반영 리비전: `0b2a38f729293f90661fba71798d0e3b3377292a`
- 위 수동 검증 항목은 공개 후 진행 예정이며, 완료 전까지 이 문서의 QA 판정은 `HOLD`를 유지한다.
