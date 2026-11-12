# TB_Shimmer Release QA

작성일: 2026-08-03  
적용 UI 가이드: `E:\Plugin Projects\Guide MD\TB_PLUGIN_UI_DESIGN_GUIDE.md`  
적용 HTML 디자인: `E:\Plugin Projects\TB_Shimmer\TB Shimmer Grain Cloud (standalone).html` 최상단 `2a` / variant `d`  
현재 판정: **RELEASED — Windows x64 1.0.0 공개 승인; 자동 게이트 PASS, 수동 DAW/청감/성능 항목은 미실행으로 기록**

## 기준 정보

| 항목 | 값 |
| --- | --- |
| 제품명 | TB_Shimmer |
| 릴리스 버전 | 2.0.0 (개발 중) |
| Project ID | `TbShm1` |
| Manufacturer Code | `TbIi` |
| Plugin Code | `TbSh` |
| Bundle Identifier | `com.TeamImpulseImpact.TB_Shimmer` |
| State Schema / Preset Format | 4 / 4 (strict v1·v2·v3 migration 지원) |
| 공개 파라미터 | APVTS 17개: 기존 11개 + `level_var` → `position_rand` → `pitch_spray` → `freeze` → `sparkle` → `shatter` |
| VST3 Category | Fx / Delay |
| 형식 | Standalone, VST3 |
| 플랫폼 | Windows x64 |
| 레이턴시 / 보고 tail | 0 samples / 180 s |
| 검증 CPU | AMD Ryzen 7 260 w/ Radeon 780M Graphics, 8C/16T |

## 자동 검증 게이트

2026-08-03에 Visual Studio 2022 `amd64` MSBuild와 JUCE 8.0.12로 실행한 결과다.

| 게이트 | 성공 기준 | 상태 |
| --- | --- | --- |
| Projucer 생성 | 메인·테스트 `.jucer`가 VS2022 솔루션을 오류 없이 생성 | PASS |
| Debug 컴파일 | 엔진·processor·editor·test harness 전체 컴파일 | PASS |
| Release 컴파일 | x64 Release Standalone 및 VST3 생성 | PASS |
| 무음 안전성 | 44.1/48/96 kHz에서 무음이 finite zero 유지 | PASS |
| 블록 결정론 | 새 랜덤 3종 활성 상태에서 고정 512와 임의 `1/31/127/509/64/3/256` 분할 결과 오차 ≤ 1e-6 | PASS |
| 랜덤 reset 결정론 | 같은 엔진에서 all-random render → reset → 재render 결과 오차 ≤ 1e-6 | PASS |
| 독립 grain random | Level Var, Position Rand, Pitch Spray가 각각 출력을 독립적으로 바꾸고 finite 유지 | PASS |
| Hybrid OLA | Size 1/160/500 × Density 2/2000 grid에서 finite, RMS 급락/폭주 없음 | PASS — RMS 0.033713–0.201220 |
| Dry 계약 | Mix 0%가 sample-identical | PASS |
| Pitch 계약 | 220 Hz 입력, +12 st 결과의 우세 peak가 440 Hz ±20 Hz | PASS — 443 Hz |
| Feedback 안전성 | Feedback 88%와 새 랜덤 3종 포함 최대 grain, NaN/∞ input이 finite이고 절대값 ≤ 8.0001 | PASS — 48/192 kHz |
| Early diffusion | Feedback/Scatter/랜덤 0에서 4-stage all-pass가 최초 late FDN 도착 전 69–125 ms 구간을 stereo reflection으로 채우며 mono에서도 유효 | PASS — direct RMS 0.02184277, early RMS 0.00472750, side RMS 0.00413638, mono RMS 0.00349579, 1 ms bin 56개 |
| Diffuse one-pass | Feedback/Scatter 0에서도 direct grain 종료 뒤 dense stereo early reflection이 존재하고 안전하게 종료 | PASS — tail RMS 0.02446003, side RMS 0.01782265, 0.70–0.90 s RMS 0.0003320878, 1.20–1.40 s RMS 0.0000000183 |
| Diffuse decay | 0.35 s burst가 late wash로 이어지고 시간 순서대로 감쇠 | PASS — bloom 0.00064286, late 0.00002144, final 0 |
| Diffuse channel/reset | Width 0% L/R 동일, Width 160% side 존재, mono finite, reset 뒤 exact zero | PASS |
| Realtime storage/path | 192 kHz DSP storage < 16 MiB; callback allocation/lock/I/O/log 없음 | PASS — 약 12.83 MiB, source audit |
| State 계약 | 17개 round-trip, v1→v4 `0 / old Scatter / old Scatter×0.0035 / Freeze off / Sparkle·Shatter 0`와 v3→v4 `Sparkle·Shatter 0` 이관, malformed/future/mixed/tag 변조 원자적 거부 | PASS (2026-11-11 Release) |
| Preset 계약 | Factory 100개 전수 적용·고유 ID/이름·범위·상태 유일성, 최초 12개 golden 완전 보존, 99번 state round-trip, v1/v2 User save/load, 잘못된 index/제품 원자적 거부 | PASS |
| UI snapshot | 760×430 단일 PNG에 16개 노브를 8+8로 항상 함께 표시하고 focus order 1–15(Freeze 제외)·겹침 없음·패널 안쪽 배치 검증 | PASS (2026-11-11 Release, `TB_Shimmer_UI.png` / `TB_Shimmer_UI_Sparkle.png`) — flat matte knob·단위 표시·Grain Halo와 중앙+방사형 Team Impulse 원본 로고 육안 확인 |
| Audio QA | `output/audio-qa/TB_Shimmer_Default_Demo.wav`, 48 kHz stereo 24-bit | PASS — 12.0 s, peak 0.3862, 4–7 s tail RMS 0.00450314, max stereo diff 0.20743 |
| VST3 계약 | manifest 생성, moduleinfo/CID 일관성, 정확히 1개 bypass parameter | PASS — Debug/Release probe |
| Tail HF 유지 | late FDN의 unity-magnitude 보간이 9 kHz/1 kHz 상대 감쇠를 Tone이 결정하도록 보존 | PASS — retention 2.54e-02 (수정 전 9.31e-06) |
| Tail 레벨 독립성 | 40 dB 입력 범위에서 감쇠율 일치 | PASS — agreement 1.2119 |
| Tail Size 모프 | Size 500→40 모프 중에도 loop gain이 사용 중인 delay를 추종하며 감쇠 지속 | PASS — decay 0.0768 |
| 오디오레이트 경계 | 2000 gr/s × 500 ms에서 voice stealing 포화 시 finite/bounded | PASS — RMS 0.0327, bounded |
| 오디오레이트 피치 | 400·1200 gr/s randomised에서 shifted grain pitch 유지 | PASS — ratio 6.34 / 3.88 |
| 오디오레이트 결정론 | 900 gr/s × 4 ms에서 고정/임의 분할 오차 ≤ 1e-6 | PASS |
| Coherent 상쇄 경계 | cloud 범위(160 ms)는 randomisation 0에서도 shift 유지, 짧은 grain(40 ms 이하)은 상쇄 | PASS — ratio 1.28e+08 vs 0.0032 |
| 연속 Pitch | +7.5 st가 220 Hz를 339.3 Hz 부근으로 이동 (±3%) | PASS — 342.0 Hz |
| Freeze 유지 | 입력이 끝난 뒤에도 포착된 history로 grain이 계속 생성 | PASS — late RMS 0.0 → 0.1766 |
| Freeze dry | Freeze 중에도 Mix 0%가 sample-identical | PASS |
| Sparkle off 동일성 | Sparkle 0%면 Shatter 100%에서도 출력이 sample-identical | PASS (2026-11-12 Release) |
| Sparkle 레이어 | Sparkle 70% 차분 신호가 유한·가청이며 grain cloud보다 밝음 | PASS — layer RMS 0.026919 (cloud 0.046630), 1차 차분 밝기 0.01467 vs 0.00461 |
| Shatter burst | Shatter 100%는 attack 직후 0.4 s에 shard가 몰리고, 0%는 고른 drizzle | PASS — 517 / 47개, 0%에서 59 / 52개 |
| Sparkle 핑퐁 | L shard가 Time 뒤 R에서 재등장 | PASS — 상관 0.4346 (Time) vs 0.0203 (다른 lag) |
| Sparkle 결정론/reset/mono | 분할 불변 ≤ 1e-6, reset 후 exact zero, Sparkle 0 복귀 3 s 뒤 잔차 0, mono 유효 | PASS |
| Sparkle 폭 | Width 100%에서 shard 레이어 power의 1/4 이상이 side, Width 0%에서 L/R 동일 | PASS — side RMS 0.036465 / total 0.067482, L/R 차이 0 |
| Sparkle Audio QA | `output/audio-qa/TB_Shimmer_Sparkle_{Off,Dust,Shatter}_Demo.wav` 비클리핑 생성 | PASS — peak 0.620 / 0.647, 레이어 −7.2 / −0.3 dB(off 대비). Shatter 85% 데모 계측: 조각 평균 19.6 ms·6 ms 미만 0%, 에코 리미터 초과 0%, 게인 덕킹 0%. 청감 평가는 미실행 |
| 최악 콜백 비용 | 192 kHz / 32 samples / density 2000 / size 500 ms / feedback 88%, 64 voice 포화 | 측정 미완료 — 데드라인 166.7 µs 대비 median 51–136 µs(31–82%)로 실행마다 요동. 이 harness는 realtime thread가 아니므로 유휴 머신·실호스트 재측정 필요 |

## 빌드 및 실행 명령

모든 Release 빌드는 Visual Studio 2022의 64-bit MSBuild를 사용한다.

```powershell
& 'C:\JUCE\Projucer.exe' --resave 'E:\Plugin Projects\TB_Shimmer\TB_Shimmer.jucer'
& 'C:\JUCE\Projucer.exe' --resave 'E:\Plugin Projects\TB_Shimmer\Tests\TB_Shimmer_Tests.jucer'

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'E:\Plugin Projects\TB_Shimmer\Builds\VisualStudio2022\TB_Shimmer.sln' `
  /m /p:Configuration=Release /p:Platform=x64

& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' `
  'E:\Plugin Projects\TB_Shimmer\Tests\Builds\VisualStudio2022\TB_Shimmer_Tests.sln' `
  /m /p:Configuration=Release /p:Platform=x64
```

Debug와 Release harness 모두 `48/48 PASS`다. VST3 Host Contract Probe는 두 빌드에서
메타데이터, Set/Get 174건, text/state round-trip, 14개 APVTS 파라미터와 JUCE `Program`을 합친
15개 writable VST3 parameter, 정확히 한 개의 bypass를 통과했다. Pitch는 `37` values / VST3
`stepCount 36`, Program은 `100` values / `stepCount 99`로 노출된다. 최초 12개 Program의 인덱스·이름·값은
보존되지만 12→100 확장으로 normalized Program automation grid는 달라진다. 생성된 단일 패널 PNG에서 13개 노브의 7+6 배치, flat matte knob, 값 단위,
Grain Halo와 방사형 canonical logo를 육안 확인했고 WAV는 48 kHz stereo 24-bit/12초 및 active/tail FFT·구간 RMS를 확인했다.
빌드와 harness stdout은 `output/build-qa/`에, Debug/Release host snapshot은
`output/host-qa/`에 보존했다. 저장된 컴파일 로그에는 warning/error marker가 없다.

## 수동 승인 게이트

다음 항목은 자동 테스트로 대체하지 않는다.

- [x] Release Standalone hidden smoke에서 6초 생존.
- [ ] 실제 화면에서 editor 열기/닫기, 760×430 고정 resize와 표시 중 Grain Field 30 fps 움직임/숨김 시 정지를 확인.
- 최소 2개 실제 DAW에서 VST3 scan, insert, automation, preset save/load, 프로젝트 재열기.
- vocal, guitar, pad, drum loop에서 dry/default/extreme A/B 청취.
- Pad의 개별 shimmer가 부드러운 evolving wash로 이어지는지, guitar/vocal attack과 자음은 보존되는지 확인.
- Drum loop의 groove가 상시 wash로 무너지지 않는지 확인.
- Feedback 88%, Pitch +24 st, Reverse 100%, Width 200%에서 클릭·폭주·과도한 aliasing 확인.
- Level Var 12 dB, Position Rand 100%, Pitch Spray 12 st의 단독/조합 A/B와 자동화 zipper를 확인.
- 높은 Feedback에서 고정 tonal ringing, 반복적인 watery wobble과 저역 누적이 없는지 확인.
- Bypass 전환 click, tail 유지, mono/stereo bus 확인.
- 48 kHz/64 callback p99·max와 1/8 instance dropout·steady memory를 전용 profiler로 측정.
- 긴 Factory/User preset 이름, tooltip 2초, save 성공 3.2초 toast, 지속 오류 표시 확인.
- Factory 100개가 15개 논리 그룹 submenu에 표시되고 이전/다음이 그룹 경계를 정상 통과하는지 확인.
- Output Portal, Soundtoys Crystallizer와 SpaceBlender는 청감/작업 흐름 비교에만 사용하고 UI·X/Y visualizer·preset·구현을 복제하지 않았는지 확인.

## 배포 산출물

공개 승인한 산출물의 아래 항목을 기록한다.

| 항목 | 값 |
| --- | --- |
| VST3 경로 | `Builds/VisualStudio2022/x64/Release/VST3/TB_Shimmer.vst3` |
| VST3 내부 binary SHA-256 | `72A9D412E175080A85ABACF8BEE65137186E65490E192B25EA2E768E54BA93D9` |
| VST3 moduleinfo SHA-256 | `C4DA9AA0D3D4B03F9E8A856EF1126E6AACD7207081C3D51D89970B1E530816D2` |
| Standalone 경로 / SHA-256 | `Builds/VisualStudio2022/x64/Release/Standalone Plugin/TB_Shimmer.exe` / `81CAEF21FF29A18022DF000A6830234A14FBDA3DAC28527ED0472E8F9D24E528` |
| Windows 코드 서명 | VST3/Standalone 모두 Authenticode `NotSigned` |
| Processor CID | `ABCDEF019182FAEB5462496954625368` |
| Controller CID | `ABCDEF011234ABCD5462496954625368` |
| Debug/Release CID·parameter 계약 일치 | PASS |
| Host Probe 로그 | `output/host-qa/TB_Shimmer-host-contract.json`, `TB_Shimmer-host-contract-debug.json` |
| Debug/Release Host snapshot SHA-256 | 양쪽 동일: `0316151FD54DAB1FA7025DDC65484743D29C6A9D1AABFC1A9D28D4C7D48385D0` |
| 단일 UI snapshot SHA-256 | `58FE0A9D12E8C7ACEDC62CC758C986F67641E37C15317F6B1356048E8E59D00B` |
| Audio QA WAV SHA-256 | `0C4078D10E97AF6F4EE8621F229DE35E19BF6D99972A825C2990FC2CB087A69B` |
| 배포 ZIP 경로/크기/SHA-256 | `output/release-candidate/TB_Shimmer-1.0.0-Windows-x64.zip` / 5,643,911 bytes / `C99F80FA43BA6E06C955EBCE0254BAF4484E4A0BE2D8D64C215B09369F4E1253` |
| 배포 저장소 main / PR | `5105AE04F957358C5C1FD756735BF02F88C30973` / [#29](https://github.com/TaebeumKim/TB_Audio-Plugins/pull/29) |
| GitHub Release | [`tb-shimmer-windows-v1.0.0`](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-shimmer-windows-v1.0.0), stable, `latest=false` |
| 공개 asset 재다운로드 검증 | 5,643,911 bytes / SHA-256 `C99F80FA43BA6E06C955EBCE0254BAF4484E4A0BE2D8D64C215B09369F4E1253` 일치 |

GitHub Release만 공개하며, 실제 DAW 청취와 성능 검증 전까지 TB Hub catalog 및 설치 위치는 갱신하지 않는다.
