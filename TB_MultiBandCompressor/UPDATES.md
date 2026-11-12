# TB_MultiBandCompressor 업데이트 기록

카탈로그 ID: `tb_multiband_compressor`  
현재 공개 버전: `1.0.2`  
릴리스 상태: Windows 공개 / 수동 release validation 보류  
이전 이력: `1.0.1`, `1.0.0` historical baseline

## 1.0.2 — 2026-08-17

### 변경 내용

- 상태·프리셋의 숫자 문자열 파서를 JUCE의 UTF 문자 포인터 기반 strict parser로 교체해
  Visual Studio 외 표준 라이브러리 구현에서도 동일한 유한 실수 판정과 완전 소비 규칙을
  유지하도록 했다. 공개 parameter ID/order/range, state schema 4와 음향 동작은 바뀌지 않는다.
- 현재 소스로 DSP Harness, Processor Integration, x64 Release rebuild와 host contract를 다시
  검증했고, deterministic Hub ZIP과 Inno 설치 프로그램의 해시·버전도 확인했다. 사용자 지시에
  따라 Windows 공개를 진행했으며 수동 native interaction·레벨 매칭 청감·production DAW·
  장시간 안정성·배포 권리·코드 서명은 보류 상태로 기록한다.

## 1.0.1 — 2026-08-13

### 변경 내용

- Single-live-response UI를 current 1.0.1 artifact에 반영하고 DSP Harness Debug/Release `58/58`, Processor Integration Debug/Release `11/11`, Debug/Release main rebuild·manifest, Release host contract와 UI capture 10개를 fresh 재검증했다. 수동 native interaction·레벨 매칭 청감·production DAW·장시간 안정성·배포 권리·코드 서명·installer는 `RELEASE HOLD`다.

## 1.0.0 — 2026-08-13 (`HISTORICAL`, 1.0.1이 대체)

### 변경 내용

- 첫 공개 릴리스 후보. Release VST3를 1.0.0으로 정규화하고 DSP Harness 58/58, Processor Integration 11/11, 반복 host 계약 검증을 완료했다. 수동 native interaction·레벨 매칭 청감·production DAW·장시간 안정성·배포 권리·코드 서명·installer 검증은 `RELEASE HOLD`로 남긴다.

## 누적 개발 이력

아래 표는 누적 개발 이력이다. 마지막 v4 행들이 현재 계약이며, schema 1/2/3·12 preset·구형 layout과 이전 build를 설명하는 앞선 행은 역사적 baseline이다.

| 날짜 | 영역 | 변경 내용 | 사용자 영향 | 호환성 영향 |
| --- | --- | --- | --- | --- |
| 2026-08-11 | Research / Docs | Pro-MB 공개 동작, 전통 crossover multiband, JUCE 필터 후보와 sidechain 호스트 관례를 비교하고 제품·UI·QA 계약을 정의 | 목표와 의도적 차이를 확인할 수 있음 | 없음 |
| 2026-08-12 | DSP | 4개의 자유 배치 TPT dynamic band, dry+delta 재합성, signed Range의 compress/expand 4방향, Band/Free detector, stereo link, audition과 20 ms soft bypass 구현 | 필요한 주파수만 동적으로 감쇠·증폭하며 중립 상태는 exact dry | 신규 제품 |
| 2026-08-12 | Sidechain | optional mono/stereo aux bus 1개와 밴드별 INT/EXT 선택, 미연결 시 deterministic INT fallback, 연결된 무음 존중, source 전환 smoothing 구현 | 각 밴드가 같은 외부 키에서 독립 detector 범위를 선택 | 신규 제품 |
| 2026-08-12 | State / Preset | 초기 71-parameter/schema 1 state, 12 Factory preset, User preset과 one-step Undo/Redo 계약 구현 | 후속 exact v1 migration의 legacy 입력 기준 제공 | v2/v3에서 exact migration 대상으로 보존 |
| 2026-08-12 | UI | 실제 detector/동적 gain 표시, graph 생성·선택·drag·wheel, 4-band selector, NO SC 상태, 85–150% responsive layout, canonical TB header와 상태 캡처 구현 | 밴드 위치와 동작을 화면에서 직접 편집하고 확인 | 신규 제품 |
| 2026-08-12 | Analyzer / UI | TB_Compressor HTML의 recessed hardware·signal-trace 위계를 적용하고 main bus의 실제 PRE/POST 2048-point FFT, UI-local Analyzer/PRE/POST toggle, TB_EQ식 node/edge/wheel 편집, selected-first hit-test를 구현 | 입력과 최종 출력의 스펙트럼을 보면서 밴드를 직접 만들고 이동·폭·Range를 편집 | Analyzer 표시 상태는 host state 밖에 유지 |
| 2026-08-12 | Analyzer lifecycle | fixed FIFO의 audio-thread sample tap과 message-thread FFT를 분리하고 demand/offline/visibility gate, hide·destroy gesture cleanup을 구현 | Analyzer Off 또는 창 숨김/닫힘에서 analyzer CPU 작업이 멈춤 | 오디오 출력·latency·tail 변화 없음 |
| 2026-08-12 | DSP / Quality | per-band Stereo/Mid/Side, Dynamic/Linear symmetric-FIR delta, 실제 Off/2×/4× oversampling과 전역 0–20 ms lookahead를 추가하고 전체 경로 latency를 host에 보고 | 믹싱용 zero-latency 기본 경로와 mastering용 linear-phase 선택, transient 예측과 고배율 처리 가능 | 신규 7 parameter; latency contract 변경 |
| 2026-08-12 | Sidechain / Channel | Stereo main의 Mid/Side detector·processing domain, mono main의 effective Stereo fallback, stereo-main + mono-EXT Side detector=0 계약을 추가 | 밴드별로 Stereo/Mid/Side 성분을 선택하고 mono/sidechain 예외를 예측 가능하게 처리 | 기존 INT/EXT bus 수와 routing은 유지 |
| 2026-08-12 | State / Preset | 기존 71개 parameter prefix 뒤에 품질 3개와 밴드별 Channel Mode 4개를 append해 총 78개, state/preset v2로 올리고 exact v1 71 migration을 정의 | 이전 v1 상태·프리셋의 기존 값은 보존되고 신규 값은 안전 기본값으로 채워짐 | count/ID/metadata가 정확한 v1만 수용; 미래·손상 데이터 atomic reject |
| 2026-08-12 | Analyzer / UI | Analyzer를 `976×456`으로 확장하고 단일 강한 0 dB선, 4색 Range, signed actual contour, temporal `0.78/0.24`와 two-pass log smoothing `0.42`, node-anchored `824×166` floating panel과 Details shelf를 추가 | 더 큰 smoothed analyzer에서 범위와 실제 증폭/감쇄를 중앙선 기준으로 편집·판독 | UI-local 상태만 추가 |
| 2026-08-12 | UI / Quality | 하단 quality strip에 Dynamic/Linear, Off/2×/4×, Lookahead와 actual latency를 배치하고 selected band row에 Stereo/Mid/Side를 추가 | 품질·레이턴시와 처리 채널을 한 화면에서 확인·변경 | latency-changing controls는 non-automatable, Channel Mode는 automatable |
| 2026-08-12 | Build / QA | 이전 71-parameter/schema 1 revision의 Debug/Release·DSP 28/28·processor 8/8·host snapshot evidence를 현재 revision에서 stale로 분류 | 현재 revision은 새 빌드·DSP·host·native UI·청감 QA 전까지 출시 후보로 간주하지 않음 | `RELEASE HOLD`; 현재 revision 검증 `NOT RUN` |
| 2026-08-12 | Preset / Content | Factory ID·이름·4개 그룹 12개를 보존하고 Vocal/Bass/Mix/Master/Motion 프리셋에 Mid/Side, Linear, 2X/4X, 1–5 ms Lookahead와 EXT/FREE를 목적별로 배치 | 기본 프리셋만 순회해도 저지연 작업·보컬/베이스·버스·마스터링·외부 키 모션을 비교 가능 | 전체 78-value state 유지; 상세 맵 `PRESET_DESIGN.md` |
| 2026-08-12 | Build / QA | v2 78-parameter source로 당시 Debug/Release 전체 빌드, DSP Harness 40/40, Processor Integration 11/11, 12개 Factory audio smoke, Release VST3 host contract와 8개 automated UI capture를 통과 | v2 engineering baseline 제공 | v3에서는 전부 STALE; 현재 release 판정에 합산하지 않음 |
| 2026-08-12 | DSP / Quality | 기존 `oversampling` parameter의 choice 끝에 실제 JUCE 8× path를 append하고 Off/2×/4×/8× quality를 구현 | 더 높은 internal rate를 명시적으로 선택 가능; latency와 CPU가 증가 | parameter count/ID/order 78 유지; 48 kHz OS latency 49/61/65 samples (2×/4×/8×) |
| 2026-08-12 | State / Preset | state schema와 preset format을 v3로 올리고 exact v2 78 identity 및 exact v1 71 migration을 구현 | v2 값은 그대로 유지되고 v1은 신규 7개 안전 기본값으로 확장 | v2의 OS 값은 Off/2×/4×만 허용; invalid/future data atomic reject |
| 2026-08-12 | Preset / Content | Factory bank를 20개와 `START/SOURCE/BUS/MASTER/MOTION` 5개 group으로 확장하고 모든 Factory global Output을 0 dB로 고정 | source·bus·master·motion use case 확대; hidden Output이 recall level을 바꾸지 않음 | 78-value full-state와 `PRESET_DESIGN.md` 20개 map 유지 |
| 2026-08-12 | Analyzer / UI | Analyzer를 `[12,54,976,499]`, internal plot `[42,38,892,443]`으로 확대하고 full-height band edge/수평 gradient, neutral shared 0 dB/configured/actual response를 적용 | spectrum과 band 영역이 더 큰 Hero에서 명확히 읽힘 | 이전 456/400 capture STALE |
| 2026-08-12 | Band UI | panel을 fixed `{88,400,824,120}`으로 바꾸고 pointer/Details를 제거; `BW (OCT)`, SC INT/EXT, FOLLOW/FREE, graph FREE bracket direct edit와 always-visible Stereo Link를 적용 | 선택 band 핵심값과 detector edit를 한 compact workflow로 통합 | Detector Frequency/Width는 graph-only direct edit; global Output UI hidden |
| 2026-08-12 | Global UI | bottom strip을 43 px로 축소하고 compact Dynamic/Linear, OFF/2X/4X/8X, Lookahead/latency, horizontal Mix를 배치 | Analyzer 면적을 확보하면서 quality controls 유지 | compatibility Output parameter/DSP/state는 유지하되 custom UI에서 숨김 |
| 2026-08-12 | UI / Preset | Bypass 중 preset recall 뒤 disabled button이 이전 SC/Detect 상태를 표시하던 문제를 APVTS 원본 재동기화로 수정하고 회귀 테스트 추가 | 오디오는 새 preset인데 화면만 이전 `SC EXT/FREE`로 남는 불일치 제거 | parameter/state 변경 없음 |
| 2026-08-12 | Build / QA | v3 DSP Debug/Release `40/40`, Processor Debug/Release `11/11`, Debug/Release Standalone+VST3 rebuild, Release host contract 78/1/207/state78, 당시 UI capture 9개를 통과 | 당시 v3 engineering baseline 제공 | 이후 Delta/UI source 변경으로 artifact·Release processor·host evidence는 STALE |
| 2026-08-12 | Sidechain / UI | Custom editor source 글자를 항상 `SIDECHAIN`으로 고정하고 toggle 상태로만 Off=INT/On=EXT를 표시; detector mode/range와 기존 detector Audition control을 숨김 | 한 버튼으로 sidechain 사용 여부를 읽고, 상세 legacy detector UI 혼잡을 제거 | INT/EXT와 detector Audition control/function은 유지하지만 Audition은 current effective FOLLOW detector를 들음; BAND/FREE·독립 range는 ID/choice/raw value/state round-trip만 보존하고 DSP에서 inert/effective FOLLOW이며 이전 FREE 음향은 보존하지 않음; 78/v3 불변 |
| 2026-08-12 | DSP / Monitoring | Band node 우클릭과 `DELTA` button이 공유하는 transient exclusive Delta Listen 추가; 선택 band wet-minus-dry를 20 ms transfer, Mix 독립, Band/global Output 적용, final Bypass 우선 | 문제 band가 실제로 더하거나 빼는 성분을 단독 청취하고 같은 band Off/다른 band transfer 가능 | parameter/state/preset/automation/Undo에 추가하지 않음; legacy detector Audition은 숨은 host 호환 surface로 유지 |
| 2026-08-12 | Analyzer / UI | Graph를 fixed neutral 0 dB + vermilion composite actual contour 하나로 단순화하고 configured response와 FREE bracket 제거; Range는 node Y와 full-height zone으로 표시 | 기준선, 설정 범위와 실제 동작의 역할이 덜 혼동됨 | detector raw parameters는 보존하지만 current Processor DSP는 FOLLOW로 고정 |
| 2026-08-12 | Preset / Content | Factory 20개 detector를 모두 BAND/FOLLOW로 고정하고 detector center/width를 processing band와 일치시킴 | 숨은 FREE 설정 없이 현재 UI에서 예측 가능한 recall | Factory ID/name/group, 78-value state와 global Output 0 dB 유지 |
| 2026-08-12 | Build / QA | Current Delta DSP Harness Debug/Release `50/50`, Processor Debug `11/11`과 Debug UI capture를 통과 | Delta identity/transfer/Output/bypass/non-leak/channel과 custom UI interaction을 자동 확인 | Release Processor, main Standalone/VST3 rebuild, host contract와 native interaction은 재실행 전 STALE; `RELEASE HOLD` |
| 2026-08-12 | Build / QA | Final current source로 Processor Debug/Release `11/11`, main Debug/Release Standalone+VST3 full rebuild·manifest, Release host contract `78/78/1/207/state78`, profile byte identity와 Debug UI capture 10개를 fresh 재검증 | Current automated engineering baseline을 source fingerprint와 artifact SHA로 고정 | 자동 gate `PASS`; Release native interaction·level-matched listening·production DAW·longevity·license/signing/installer는 계속 `RELEASE HOLD` |
| 2026-08-12 | UI / Monitoring | Node 우클릭을 right-button hold 동안의 momentary Delta로 확정하고 hold-drag transfer와 mouse-up/realtime button-loss release를 추가; panel `DELTA` click latch는 유지하며 정상 release는 pre-hold latch 복원, lifecycle/external reset은 복원 없이 Off | 빠른 hold-to-audition과 deliberate latch를 구분하고 reset 뒤 이전 monitor가 되살아나는 위험 제거 | 위 transient same-Off 우클릭 행을 supersede; parameter/state/preset/automation/Undo와 DSP 수식 불변 |
| 2026-08-12 | Analyzer / UI | Node 중심 `≤12 design px`를 full-height edge보다 우선하고 disabled ghost를 saved Range Y에 표시; 양방향 `-24/+24 dB`, frequency invariant와 방향별 one Undo 회귀 추가 | node/edge overlap 때문에 positive Range drag가 bandwidth edit로 잘못 선택되던 경로 제거 | 78/v3 불변; Processor interaction evidence 갱신 |
| 2026-08-12 | Build / QA | 위 interaction source로 DSP Debug/Release `50/50` 유지, Processor Debug/Release `11/11`, main Debug/Release full rebuild warning/error text 0·manifest와 Release VST3 host probe exit 0 재확인 | 최종 automated baseline을 current source와 새 logs/captures에 맞춤 | provenance를 새 source/build/test/UI SHA로 교체; 수동 gate는 계속 `RELEASE HOLD` |
| 2026-08-12 | Visual / Asset | 기존 orange-forward 고채도 palette와 vector-only 노브를 supersede하고 charcoal·Signal Red·Slate Blue·Console Green·Calibration Ivory의 저채도 studio palette, generated neutral bakelite raster body, Aged Brass tick과 procedural Ivory/Slate pointer를 적용 | 제공된 vintage compressor 이미지의 촉각적 재질 방향을 반영하면서 Analyzer Hero와 값 판독을 유지하고 주황 concept을 제거 | knob image는 회전하지 않고 pointer/value만 동적; DSP·78 ID·v3 불변; 공개 배포 전 similarity/license human review 필요 |
| 2026-08-12 | Analyzer / UI | PRE/POST source selector, processed spectrum overlay와 output analyzer tap/FIFO/API를 제거하고 engine 직전 main INPUT spectrum 하나와 단일 `ANALYZER` toggle만 유지 | fixed 0 dB 위/아래의 Signal Red actual contour가 실제 감쇄·증폭을 이미 보여 주므로 원본 spectrum 판독을 방해하는 중복 결과 overlay 제거 | 78/v3와 actual contour 불변; current Debug Processor `11/11` 및 새 Debug capture 10개 PASS, 이전 Release Processor/main build/VST3 host/provenance는 STALE이며 재생성 전 `RELEASE HOLD` |
| 2026-08-12 | Build / QA | 위 single-input source로 DSP Debug/Release `50/50`, Processor Debug/Release `11/11`, main Debug/Release Standalone+VST3 full rebuild·manifest, Release host contract `78/78/1/207/state78`, profile identity와 Debug capture 10개를 fresh 재검증 | Single INPUT analyzer와 78/v3 host·DSP 계약의 current automated baseline 확보 | Source fingerprint와 artifact SHA 갱신; Release native interaction·listening·production DAW·longevity·license/signing/installer는 계속 `RELEASE HOLD` |
| 2026-08-12 | Analyzer / UI | visible Analyzer 축을 `+24/0/-24 dB` 하나로 통합하고 plot `{42,38,892,300}`, 0 dB `y=188`을 확정; INPUT spectrum/node/actual이 동일 `dbToY`를 사용하고 구형 `-90…0 dBFS` visible axis를 제거 | 중앙 0 dB와 signed 변화량, 원본 spectrum의 위치를 하나의 눈금으로 읽음 | PRE-only 계약·DSP·78 ID/v3 불변; 새 10-capture로 검증 |
| 2026-08-12 | Visual / Asset | previous generated knob를 pointer-free broad-lobed Bakelite/large ivory-cap raster로 supersede하고 9개 rotary를 `{10+90i,40,84,76}`, 90 px pitch/6 px gap/10 px margins와 50 px renderer cap으로 고정 | 85% UI에서 노브 재질·값 판독성을 보존하면서 Analyzer Hero와 panel 밀도를 유지 | tint: FREQ/BW/OUT Ivory, THRESH/KNEE Green, RANGE/RATIO Blue, ATTACK/RELEASE Red; Aged Brass marker는 procedural; runtime SHA `13AA4B40…A88EEE` |
| 2026-08-12 | Build / QA | 위 centered-axis/pointer-free-knob source로 Processor Debug/Release `11/11`, main Debug/Release rebuild·manifest, Release host probe와 Debug UI capture 10개를 fresh 재검증 | latest UI·BinaryData·layout 계약의 current automated baseline 확보 | 자동 gate `PASS`; native/listening/DAW/longevity/similarity·ImageGen rights/signing/installer는 계속 `RELEASE HOLD` |
| 2026-08-12 | DSP / Dynamics | Compress는 threshold 위, Expand는 threshold 아래를 active side로 고정하고 signed Range 부호를 실제 meter/output gain 부호에 직접 연결 | `+Range`는 활성 구간을 증폭하고 `-Range`는 감쇄하므로 노브·그래프·소리가 같은 방향으로 움직임 | parameter ID/range·78/v3 불변; DSP Harness Debug/Release `54/54` |
| 2026-08-12 | Preset / Content | ID 12 `Transient Lift`와 ID 20 `Parallel Drum Punch`의 positive bands를 explicit Compress로 조정하고 ID 19 `Keys Space`는 Expand로 유지 | threshold 위 transient lift와 threshold 아래 detail lift를 signed Range의 새 의미에 맞게 명확히 구분 | Factory ID·이름·그룹은 유지; ID 12/20 mode 값만 development candidate에서 의도적으로 변경 |
| 2026-08-12 | Analyzer / UI | Y축 `+24/+12/0/-12/-24 dB` 숫자를 왼쪽에만 배치하고 우측 눈금을 제거; 상단 좌측 고정 `INPUT` source label을 추가하고 BAND/ACTUAL/INPUT 숫자 debug readout을 삭제 | 원본 analyzer source와 signed 축을 즉시 식별하고 분석 화면의 불필요한 진단 문자를 제거 | single-input FIFO/API·78/v3 불변 |
| 2026-08-12 | Band UI / Monitoring | `BW`를 `WIDTH`로 변경하고 Band Out knob를 숨긴 뒤 8개 rotary를 `{20+100i,40,84,76}`에 재배치; Threshold/Range/Ratio/Knee를 인접한 동일 Green 그룹으로 묶음. Node right-hold drag는 Delta band transfer 대신 처음 누른 band의 Frequency만 편집하고 Range를 보존 | 동적 핵심값의 순서와 색이 일관되고, 우클릭 청취 중 원하는 band 중심을 그대로 탐색 가능 | Band Output parameter/DSP/state와 panel Delta latch 유지; Frequency edit만 one Undo/host gesture |
| 2026-08-12 | Build / QA | 위 signed-Range·left-axis·8-control·same-band right-drag·Factory mode source로 DSP Debug/Release `54/54`, Processor Debug/Release `11/11`, UI capture 10개, main Debug/Release full rebuild·manifest와 Release host contract `78/78/1/207/state78`을 fresh 재검증 | 최신 DSP·UI·preset 의미와 host/state 계약의 동일-source automated baseline 확보 | source/artifact/UI SHA를 갱신; Release native/listening/production DAW/longevity/license/signing/installer는 계속 `RELEASE HOLD` |
| 2026-08-12 | State / Parameter | Range domain을 `-48…+12 dB`, state schema/preset format을 v4로 변경하고 exact v3/v2/v1 migration을 추가 | 더 깊은 mastering 감쇄와 제한된 upward headroom을 host 값·노브·그래프에서 동일하게 사용 | parameter count/ID/order 78 유지; legacy Range는 `-24…+24 dB`로 먼저 검증하고 v4로 clamp해 기존 `+24 dB`는 `+12 dB`가 됨 |
| 2026-08-12 | Analyzer / UI | plot top/zero/bottom `38/188/338`과 150 px 양 half를 유지하면서 response 눈금을 `+12/+6/0/-24/-48 dB`로 변경; fixed `#817C66` datum, `#C8C09A` configured Range contour와 thinner `#A83F46/#D26761/#9E3A48` actual DSP-gain gradient를 계층화하고 graph-only background를 `#20242C/#141820`으로 밝힘 | 중앙선에서 최대 설정량과 현재 동작량을 즉시 구분하고 Analyzer Hero의 depth를 더 선명하게 판독 | 구형 shared ±24 axis와 actual-only contour 계약을 supersede; response node/contours는 piecewise `-48…+12 dB` mapping 사용 |
| 2026-08-12 | Analyzer / UI | 원본 INPUT spectrum을 response 축에서 분리해 plot 전체 높이의 `0…-96 dBFS`로 매핑하고 상단 `0 dBFS` caption을 추가 | 원본 level이 증폭·감쇄 response scale에 왜곡되지 않고 full-scale 기준으로 읽힘 | engine 직전 INPUT 단일 tap과 no-processed-overlay 계약, 78 parameter는 유지 |
| 2026-08-12 | Build / QA | v4 Range-overlay source의 DSP/Processor, Debug/Release main build, Release host contract, UI capture와 source/artifact provenance를 전부 재생성 대상으로 전환 | 서로 다른 source revision의 이전 PASS/hash를 현재 근거로 오인하지 않게 함 | 당시 자동 gate `PENDING`; 바로 다음 current v4 PASS 행이 이 일시적 상태를 supersede, 최종 판정은 계속 `RELEASE HOLD` |
| 2026-08-12 | Build / QA | v4 Range-overlay current source로 DSP Debug/Release `58/58`, Processor Debug/Release `11/11`, Debug/Release Standalone+VST3 full rebuild·manifest, Release host contract와 Debug UI capture 10개를 fresh 재검증 | Range endpoint/migration, configured/actual contour, 독립 INPUT scale와 host metadata가 동일 source/artifact에서 일치함을 고정 | parameter/state/preset `78/4/4`, host `78/78/1/207/state78`, Range `-48/-3/+12 dB`, source/artifact/UI SHA 갱신; 수동 gate는 계속 `RELEASE HOLD` |
| 2026-08-13 | Analyzer / UI | 별도 fixed datum, configured Range contour, red actual overlay와 gain fill/shadow/gradient를 제거하고 `#817C66`, alpha `1.0`, `3.0 px`의 단일 actual DSP line으로 통합. Idle/outside-band 구간은 0 dB datum이고 active 구간만 위·아래로 변형하며 Range maximum은 node Y로만 표시 | 두 겹의 response와 채운 영역을 없애 실제 gain 움직임 하나로 깔끔하게 판독 | INPUT 독립 `0…-96 dBFS`, response `-48…+12 dB`, 78/v4 계약은 유지 |
| 2026-08-13 | Band UI / Header | Stereo Link parameter/attachment와 host/state/DSP는 호환성을 위해 유지하되 custom editor의 `LINK` label/control/bounds/hit target을 제거. Header 표시명을 정확히 `Multi Band Compressor`로 바꾸고 M/B/C를 `#7088B2/#4F805F/#C65350`, 나머지를 `#D9DAE1`로 표시 | 불필요한 custom control을 줄이고 제품명과 기능별 색의 연계를 명확히 함 | 내부 프로젝트·번들명과 parameter ID/order 불변; 직전 build/test/host/capture provenance는 `STALE`, fresh evidence는 `PENDING` |
| 2026-08-13 | Build / QA | Single-live-response source의 DSP/Processor, Debug/Release main build·manifest, Release host contract, UI capture와 source/artifact provenance를 fresh 재생성 대상으로 전환 | 이전 Range-overlay binary·capture·hash를 현재 증거로 오인하지 않게 함 | 현재 자동 gate `PENDING`; 새 결과와 SHA가 확정되기 전까지 `RELEASE HOLD` |
| 2026-08-13 | Build / QA | Single-live-response current source로 DSP Debug/Release direct `58/58`, Processor Debug/Release direct `11/11`, main Debug/Release Standalone+VST3 full rebuild exit `0`·manifest, Release host probe exit `0`과 Debug UI capture 10개를 fresh 재검증 | 단일 actual line·Range node Y·hidden LINK·header와 78/v4 DSP/state/host 계약을 동일 provenance로 고정 | host metadata/profile/set-get/text/state PASS, `78/78/1/207/state78`, source+tests manifest 16행/1,501 bytes와 current artifact SHA는 `RELEASE_QA.md`; automated engineering `PASS`, 수동 gate는 계속 `RELEASE HOLD` |
| 2026-10-02 | Sidechain / Routing | optional external aux를 `Sidechain 1–4` 네 개의 mono-default bus로 분리하고 `Sidechain N→Band N` 고정 매핑을 추가. 각 bus는 stereo 연결을 `(L+R)/2`로 downmix하고 선택 bus 부재 시 해당 band만 internal fallback | DAW의 네 Send/Aux 선택지에서 각 밴드를 독립 더킹하고 APVTS parameter·CID·state/preset identity는 유지 | 기존 단일 bus 세션 라우팅은 보장하지 않아 업데이트 후 Send/Aux 재연결이 필요할 수 있음; DSP/Processor isolation·동시 ducking·missing-bus fallback·connected silence·external non-leak과 실제 DAW의 네 bus 표시/routing을 검증 |

### 카탈로그 노트 후보

- 필요한 주파수에만 배치하는 4개의 독립 dynamic band.
- 네 개의 optional mono-default external aux에서 `Sidechain 1→Band 1`…`Sidechain 4→Band 4` key를 공급하고 각 밴드의 `SIDECHAIN` toggle로 내부/외부 key를 독립 선택. Stereo 연결은 DSP에서 `(L+R)/2`로 downmix하며 Factory는 모두 processing band를 따르는 detector를 사용.
- Compress/Expand로 threshold active side를 고르고 `+Range`는 증폭, `-Range`는 감쇄하도록 한 축으로 제어.
- 밴드별 Stereo/Mid/Side와 mono host의 명확한 Stereo fallback.
- zero-latency Dynamic 또는 mastering용 Linear, 2×/4×/8× oversampling과 0–20 ms lookahead.
- Smoothed main INPUT analyzer에서 독립 `0…-96 dBFS` 원본 level과 top `0 dBFS` caption, 왼쪽 `+12/+6/0/-24/-48 dB` response 축, idle에서 0 dB datum이 되고 active 구간만 변형되는 단일 dark pale-yellow actual gain line을 보며 직접 편집. Range maximum은 node Y로 확인한다.
- Node right-hold로 선택 band의 true wet-minus-dry contribution을 순간 청취하면서 같은 band Frequency를 이동하고, `DELTA` button으로 필요할 때 latch.

### 알려진 제한 사항

- 최대 4밴드다. Pro-MB의 6밴드와 proprietary Dynamic Phase 알고리즘을 복제하지 않는다.
- Processing mode는 Dynamic/Linear 두 가지다. Minimum Phase와 FabFilter proprietary Dynamic Phase는 포함하지 않으며 feature parity를 주장하지 않는다.
- 물리 external sidechain은 호스트에 노출되는 optional mono-default aux bus 4개이며 `Sidechain N`을 Band N에 고정 연결한다. 각 bus는 stereo 연결을 `(L+R)/2`로 downmix한다. 기존 단일 bus 세션은 업데이트 후 Send/Aux 재연결이 필요할 수 있다.
- 각 mapped external channel은 Stereo detector에서는 양쪽 입력에 같은 mono key로 공급되고 Mid/Side detector에서는 선택 domain의 key로 사용되므로 모든 Quad processing mode에서 유효한 external source로 유지된다. 연결된 silence를 미연결로 판정하지 않는다.
- 공개 배포 전 현재 revision의 Release native interaction, DRM_01/VOX_01/KEY_01 level-matched 청감, 실제 production DAW, 장시간 안정성, EULA·JUCE 배포 권리와 code signing을 완료해야 한다.
- Four-bus sidechain source의 DSP Debug/Release `59/59`, Processor Debug/Release `11/11`, Debug/Release main rebuild·manifest, current Release host probe와 Debug UI evidence는 fresh `PASS`다. Processor/profile에서 네 mono-default bus declaration과 `Sidechain N→Band N` 매핑을 확인했고, production DAW의 실제 Send/Aux 표시는 수동 게이트로 남긴다. Engine direct single-buffer overload는 내부 legacy DSP tests 전용이다. 수동 native/listening/DAW/license/signing/installer gate는 남아 있어 `RELEASE HOLD`다.
