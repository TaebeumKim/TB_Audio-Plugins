# TB Delay 업데이트 기록

카탈로그 ID: `tb_delay`  
현재 공개 버전: `1.0.3`  
이전 이력: `1.0.2` · `1.0.1` · `1.0.0`

## 미출시 - 2026-10-02 Factory 확장 후보

릴리즈 판정: `로컬 Release 후보에서만 검증 · 공개 카탈로그 미변경`

### 변경 내용

- 기존 0–99 Factory program은 이름·순서·상태를 그대로 두고 100–119에 20개를
  append했습니다. Factory browser는 `PERFORMANCE & UTILITY`와 `HYBRID & EXTREME`
  두 논리 폴더를 추가해 총 12개 폴더 × 10개로 확장됩니다.
- 기본 음향 이론과 실전 용도를 보강했습니다. 1/32 grid, 3:2 polyrhythm, ducking,
  Haas와 true L/R ping, BBD clock, tuned bass comb, 장시간 feedback을 포함합니다.
- 창의적 조합도 별도 그룹으로 분리했습니다. negative comb, Tape Stop, Post/Loop
  frequency-shift throw, reverse bloom, shift+grain, micrograin spray, octave cloud,
  freeze bed를 추가했습니다.
- 새 20개는 COLOR/SPACE macro 방향을 preset에서 바로 들을 수 있게 했고, 기존 0–99의
  macro `BASE`·host recall·Freeze Off 정책은 보존했습니다.

### 검증

- Debug/Release VST3 scan·instantiate 포함 전체 하네스 각각 `30/30 PASS`
- Factory 120개 범위·고유성·finite/bounded render와 12개 폴더 UI snapshot 확인
- 공개 버전 `1.0.3`과 카탈로그 파일은 변경하지 않음

## 미출시 - 2026-10-01

릴리즈 판정: `로컬 Release 후보 생성 · Debug/Release 각 30/30 PASS · 공개 카탈로그 미변경`

### 변경 내용

- `TIME MODE` 바로 아래에 같은 폭의 momentary `TAP` 버튼을 추가했습니다.
- 첫 탭은 측정만 시작하고 두 번째부터 `100–3000 ms` 범위의 최근 최대 4개 간격을
  평균합니다. 너무 벗어난 간격은 새 탭 시퀀스로 다시 시작합니다.
- Free와 Tune에서는 측정한 간격을 기존 `time`에 적용하고, Sync에서는 실제 host BPM에서
  가장 가까운 기존 `division`을 선택합니다. 현재 `TIME MODE`는 바꾸지 않습니다.
- TAP은 저장·자동화 파라미터가 아닌 UI 명령입니다. 기존 38개 host parameter ID와 상태
  schema를 그대로 유지하고, 키보드 포커스와 DAW의 Space transport를 소비하지 않습니다.

### 검증

- Debug/Release VST3 직접 scan·instantiate 포함 전체 하네스 각각 `30/30 PASS`
- 실제 `220 ms` 탭의 Free 시간 반영과 120 BPM Sync의 `1/8` 최근접 선택 확인
- 800 × 700과 150% 캡처에서 TIME MODE와 같은 폭, 각각 6 px·9 px 하단 간격 확인
- Release Standalone hidden 3초 기동 smoke test 통과

## 미출시 - 2026-10-01 직관성 개선

### 변경 내용

- Delay Map을 원형 ripple 중심 표시에서 `10 ms–5 s` 선형 타임라인으로 바꿨습니다.
  입력 기준선을 왼쪽에 두고 위쪽 L·아래쪽 R lane을 분리해 첫 echo 위치를 바로 읽을 수 있습니다.
- 첫 echo는 10 ms에서도 넓은 세로 marker로 표시하고, 반복 감쇄는 오른쪽으로 줄어드는
  삼각형으로 표시합니다. grain은 시간 범위 band와 pitch stem/triangle로 표시하며,
  5초 경계에 걸친 cloud도 `>5 s` overflow로 알려줍니다.
- `TAPS`는 `ECHOES`, `TYPE`은 `CHARACTER`, `COLOR`는 `TONE`, `SPACE`는 `DIFFUSION`,
  `GRAIN MIX`는 `GRAIN`, `BUILD / ONCE`는 `SHIFT ROUTE`로 표시해 결과를 먼저 읽도록 했습니다.
  호스트 parameter ID·상태·DSP 계약은 유지합니다.
- TAPS 값은 `1 echo / 2 echoes / 4 echoes / 8 echoes`, shift route는 `LOOP / OUTPUT`으로
  표시하고 모듈 바이패스 버튼은 `BYP`로 명시했습니다.

## [1.0.3] - 2026-08-21

소스 리비전: `Git 미연결 · Release VST3 ZIP SHA-256 c783f8d67732fdf52396afb0eb701f2b954a5103d9da11f7983921bb004ab146`  
릴리즈 판정: `사용자 지시로 공개 · 자동 검증 Debug 28/28 · Release 29/29 PASS · 수동 QA 보류`

### 변경 내용

- 한글 툴팁이 깨져 보이던 문제를 수정했습니다. `juce::String`은 평범한 `const char*`를
  UTF-8이 아니라 Latin-1로 해석하므로, 소스의 UTF-8 리터럴이 노브 설명 38개와 프리셋
  오류 메시지 12개에서 모두 mojibake로 표시되고 있었습니다. 비ASCII 리터럴 58군데를
  `juce::String::fromUTF8`로 통과시켜 툴팁, 상태 문구, 저장 다이얼로그와 Delay Map
  푸터의 `·` 같은 문장부호까지 함께 정상화했습니다.
- Delay Map을 세로로 키우고 컨트롤 보드를 줄였습니다. 보드가 작업 영역에서 차지하는
  비율을 `0.56`에서 `0.50`으로 낮춰 기본 창 기준 보드 `356 → 280 px`,
  Delay Map `204 → 270 px`가 됐습니다.
- 기본 창을 `880 × 710`에서 `800 × 700`으로 좁혔습니다.
- 노브 이름을 `9.5 → 12 px`, 셀렉터 이름을 `9 → 11.5 px`, 값 표시를 `9.5 → 10 px`로
  키웠습니다.
- 네 섹션 패널 안쪽에 좌측 여백을 넣어 첫 컨트롤이 패널 테두리에 붙지 않게 했습니다.
- 창 크기 조정을 추가했습니다. 종횡비를 고정한 채 `70%–180%` 범위에서 폰트·노브·여백까지
  같은 배율로 확대·축소되고, 마지막 크기는 세션 상태의 `editorScale`로 저장·복원됩니다.

### 적용한 공통 가이드 예외

`00_READ_THIS_FIRST.md`의 "사용자의 최신 명시적 요청이 문서와 충돌하면 해당 요청을
우선하고 예외를 기록한다"에 따라 다음 3건을 사용자 지시로 적용했다. 1.0.2에서 기록한
6건의 예외는 그대로 유지된다.

| # | 가이드 기준 | 적용값 | 근거 |
| --- | --- | --- | --- |
| 1 | UI 가이드 Delay Map `900 × 200 px` 이상 | `764 × 270 px` | 창 폭을 더 좁히라는 지시. 세로는 가이드 기준을 상회한다 |
| 2 | UI 가이드 5.4 메인 라벨·값 `10–10.5 px` | 노브 이름 `12 px` | 노브 이름 폰트를 키우라는 지시 |
| 3 | UI 가이드 5.4 패널 외곽 패딩 `10–14 px` | 보드 좌측 `20 px` | 패널 좌측 여백을 늘리라는 지시 |

### 카탈로그 노트 후보

- 노브 위에 마우스를 올리면 나오는 한글 설명이 정상적으로 표시됩니다.
- Delay Map이 커지고 창을 원하는 크기로 조절할 수 있습니다.

### 알려진 제한 사항

- macOS 빌드는 별도 SVN 파이프라인에서 생성하므로 이번 릴리즈에서는 `1.0.2`를 유지합니다.
- 수동 QA(실 DAW 통합, 청감 A/B, pluginval, 성능, 설치·제거, DPI)는 이번에도 보류 상태로
  공개했습니다.

## [1.0.2] - 2026-08-19

소스 리비전: `Git 미연결 · Release VST3 ZIP SHA-256 891b654bd53ba21d703dfa3d1072bcc152cec50f341b6e15c8f06e0eefe82a00`  
릴리즈 판정: `사용자 지시로 공개 · 자동 검증 29/29 PASS · 수동 QA 보류`

### 변경 내용

- 메인 보드를 `REPEAT / STEREO / CHARACTER·SPACE / TRANSFORM` 네 결과 중심 영역으로
  재편하고 기술 상세 컨트롤을 preset-only로 내려 선택 피로를 줄였습니다.
- 기존 36개 파라미터 뒤에 `COLOR`, `SPACE` 안정 매크로를 추가했습니다. 두 매크로의
  `BASE`는 기존 Factory/User preset과 DAW state의 소리를 그대로 보존합니다.
- State schema 2·preset format 3을 추가하고 기존 36개/33개 상태와 preset을 자동 이행합니다.
- 빈 history Freeze가 단발음을 놓치던 문제, Freeze 중 diffusion이 계속 누적되던 문제와
  10 ms Tape/BBD modulation의 read-head 비대칭을 수정했습니다.
- 기본 에디터를 960×650으로 줄이면서 Delay Map의 10 ms–5 s 가시 영역을 유지했습니다.
- Debug/Release 회귀 `28/28`, 기존 host parameter metadata `38/38` 보존과
  새 Color/Space 2개 append, VST3 scan/instantiate를 확인했습니다.

- 무입력 상태에서 `FEEDBACK` 100% 초과 구간이 스스로 발진을 유지하던 문제를 수정했습니다.
  입력이 약 2초 이상 없으면 루프 게인 상한을 unity 아래(`0.985`)로 되돌려 tail이 반드시
  감쇠합니다. 연주 중에는 개입하지 않고 `FREEZE`의 의도된 hold도 유지됩니다.
- 시각 정체성을 `물결치는 바다` 팔레트로 교체하고 그라디언트를 툰 셰이더식 단계 밴딩으로
  바꿨습니다. 좌하단에 백사장과 포말 해안선을 넣었습니다.
- Delay Map을 두 개의 원형 물결 필드로 재설계했습니다. 반지름이 딜레이 시간이고 링의
  개수·두께가 피드백과 양감을 나타내며, `WOBBLE`/`BLUR` 값으로 링이 울렁입니다.
- `TIME MODE`와 `BUILD / ONCE`를 드롭다운으로 바꾸고 네 패널을 모두 한 줄 구성으로
  맞춰 컨트롤 셀 폭을 통일했습니다.
- 노브를 41 px에서 50 px로 키우고 패널 제목을 17 px로, 모듈 바이패스를 `B` 28 px
  정사각형으로 키웠습니다. 기본 창은 880 × 710입니다.
- Delay Map의 표시 시간 범위를 `10 ms–5 s`에서 `10 ms–2.5 s`로 좁혔습니다.

### 적용한 공통 가이드 예외

`00_READ_THIS_FIRST.md`의 "사용자의 최신 명시적 요청이 문서와 충돌하면 해당 요청을
우선하고 예외를 기록한다"에 따라 다음 6건을 사용자 지시로 적용했다.

| # | 가이드 기준 | 적용값 | 근거 |
| --- | --- | --- | --- |
| 1 | UI 가이드 13.4 Delay Map 최소 폭 `900 px` | `840 px` | 사용자 지시로 창 폭을 880으로 축소 |
| 2 | UI 가이드 13.4 x축 `10 ms–5 s` | `10 ms–2.5 s` | 내용이 없는 구간을 좁히라는 지시 |
| 3 | PRODUCT_BRIEF 10 `네온 진행 링` 회피 | 원형 물결 시각화 | 애널라이저를 원형 물 파형으로 바꾸라는 지시 |
| 4 | UI 가이드 5.4 Standard 노브 `41 px` | `50 px` | 가독성을 위해 키우라는 지시 |
| 5 | 모듈 바이패스 `≤42 × 16 px` | `≤34 × 34 px` | 2배 키우고 여백을 주라는 지시 |
| 6 | UI 가이드 5.3 장식 배경 위 컨트롤 패널 | 백사장 위 패널 유지 | 좌하단 백사장 10% 지시 |

시각화 등급은 UI 가이드 7의 `파라미터 예측 시각화`를 유지한다. 원형 필드의 반지름은
DSP와 동일한 시간 계산에서 나오고, 울렁임 진폭은 `MOD DEPTH`와 `SMEAR`의 실제 값에서
파생되므로 원천 상태가 없는 자율 애니메이션이 아니다.

### 카탈로그 노트 후보

- 더 단순한 4영역 편집 화면과 COLOR/SPACE 매크로로 복합 딜레이를 빠르게 조형할 수 있습니다.
- 기존 프리셋과 세션은 BASE 상태로 호환되며 Freeze의 짧은 단발음 유지가 개선되었습니다.

### 알려진 제한 사항

- 숨긴 상세 파라미터는 기존 호스트 자동화와 preset 호환을 위해 플러그인 자체 UI가 아닌
  DAW의 파라미터 목록에는 계속 표시됩니다.

## [1.0.0] - 2026-07-29

소스 리비전: `Git 미연결 · Release VST3 SHA-256 265b47c092fffa4d3143aa3575e25b8e19853ce5170cfba525b7bd69202abe76`  
릴리즈 판정: `사용자 승인 · 자동 검증 PASS · 수동 검증 보류`

### 변경 내용

- 스테레오 피드백 딜레이와 Tape, BBD, Digital 캐릭터를 구현했습니다.
- 주파수 이동, 디퓨전, 그레인, Freeze와 모듈별 바이패스를 구현했습니다.
- 10개 폴더에 100개 Factory 프리셋을 구성했습니다.

### 카탈로그 노트

- 테이프, BBD, 디지털 캐릭터를 갖춘 스테레오 피드백 딜레이를 처음 공개했습니다.
- 주파수 이동, 디퓨전, 그레인과 Freeze로 반복음을 변형할 수 있습니다.

### 호환성과 알려진 제한 사항

- 자동 검증은 통과했지만 실제 DAW, 사람 청감 A/B, 성능, pluginval, 설치·제거와 호스트별 DPI 수동 검증은 추후 진행 예정입니다.

### 검증 근거

- `RELEASE_QA.md`
- Debug/Release 자동 테스트 `26/26` 통과

### 카탈로그 반영 (1.0.3)

- 상태: `반영 완료`
- 저장소: `TaebeumKim/TB_Audio-Plugins`
- PR: #71 — Release TB Delay 1.0.3
- 병합 커밋: `84b48bf46ee97356643041a6345225d2300d3138` (squash, 2026-08-21)
- 배포 ZIP: `Hub/TB_Delay.zip` · `2,542,969 B` · SHA-256 `c783f8d67732fdf52396afb0eb701f2b954a5103d9da11f7983921bb004ab146`
- 전파 확인: `raw.githubusercontent.com`의 `catalog.json`이 `1.0.3`을 서빙하고, `?v=1.0.3`
  URL에서 내려받은 ZIP의 크기·SHA-256과 내부 `moduleinfo.json` 버전이 모두 일치했다.
- macOS VST3/AU와 `webDownloads` 항목, 다른 플러그인의 카탈로그 객체와 ZIP은 변경하지 않았다.

### 카탈로그 반영 (1.0.2)

- 상태: `반영 완료`
- 저장소: `TaebeumKim/TB_Audio-Plugins`
- PR: #69 — Release TB Delay 1.0.2
- 병합 커밋: `9dafa2c1ba1addac19b6d7b9dd5b45a2aaba2a22`
- 배포 ZIP: `Hub/TB_Delay.zip` · `2,427,015 B`
- 비대상 34종의 카탈로그 객체와 ZIP Git 객체는 병합 전과 동일하다.

### 카탈로그 반영 (1.0.0)

- 상태: `반영 완료`
- 반영 소스 리비전: `0b2a38f729293f90661fba71798d0e3b3377292a`
