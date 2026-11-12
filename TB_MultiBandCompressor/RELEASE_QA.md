# TB_MultiBandCompressor Release QA

판정: **CURRENT-SOURCE AUTOMATED ENGINEERING PASS / RELEASE HOLD**  
대상: 1.0.2 v4 Windows release  
현재 계약: 78 parameters / state schema 4 / preset format 4  
갱신일: 2026-10-03

## 판정 요약

현재 4-bus sidechain source의 automated engineering evidence는 fresh `PASS`이며, 실제 DAW Send/Aux 표시와 routing은 아직 수동/외부 검증 대상이다.

### 1.0.2 재검증

- 4채널 sidechain lane mapping과 mapped Mid/Side detector 경로를 포함한 current source에서 DSP Harness
  Debug/Release `59/59`, Processor Integration Debug/Release `11/11`을 통과했다.
- x64 Debug/Release full rebuild와 VST3 manifest 생성이 exit `0`으로 끝났고 module/controller
  version은 모두 `1.0.2`다. 현재 Release VST3 binary는 7,165,952 bytes,
  SHA-256 `7947A2A48E2F5A9429429AC5EE05C6A1E8FFA4DF5091E3E43A2261295447B842`이며
  설치된 `C:\Program Files\Common Files\VST3\TB_MultiBandCompressor.vst3`와 일치한다.
- 현재 Release host contract probe는 exit `0`으로 metadata/profile/set-get/text/state를 통과했다.
  네 bus declaration과 `Sidechain N→Band N` 처리는 Processor Integration과 설치 profile에서 확인했고,
  production DAW의 실제 Send/Aux 메뉴 표시는 별도 수동 게이트로 남긴다.
- 아래 1.0.1 상세 provenance 표의 source fingerprint와 artifact hash는 역사적 증거이며,
  1.0.2 산출물에는 이 재검증 행을 우선 적용한다. 수동 listening/DAW/longevity/rights/signing/
  installer는 계속 `HOLD`다.

- DSP Harness Debug/Release direct run: 각각 `59/59`; per-band lane isolation과 mapped Side detector 포함
- Processor Integration Debug/Release direct run: 각각 `11/11`; v4 round-trip/migration, single continuous live response, Range node Y, hidden LINK와 header 계약 포함. Negative/positive gain fixture에서 움직이는 active-band 중심의 0 dB 띠에 `#817C66` pixel이 남으면 실패하고 active-band 밖 baseline continuity를 확인하는 회귀 포함
- Debug/Release main Standalone+VST3 full rebuild: 각각 exit `0`; Release AMD64 VST3 manifest 성공. Debug build에는 32-bit MSBuild toolchain 경고가 있었지만 컴파일 오류는 없었다.
- Release VST3 host contract: current probe exit `0`; metadata/profile/set-get/text/state PASS. 네 bus의 실제 DAW Send/Aux 표시는 수동 게이트
- Debug component UI capture 10개 재생성: 85/100/150%, internal, SIDECHAIN On, Delta, bypass, MASTER, Linear+8X+Mid와 four-band 상태
- 이전 source+authored-tests manifest와 fingerprint는 four-bus routing 변경 전 provenance라 재사용하지 않는다. 현재 source/build/test 결과는 이 문서의 2026-10-02 실행 기록과 산출물 hash로 확인한다.

직전 v4 Range-overlay source의 PASS, capture와 hash는 아래 역사적 `STALE` snapshot에만 남기며 current 판정에 합산하지 않는다.

실제 Release Standalone native 화면·오디오·상호작용, level-matched listening, production DAW sidechain/PDC/recall/bounce, 장시간 안정성, 배포 권리·서명은 아직 미검증이다. installer와 Hub ZIP은 검증했고 사용자 지시에 따라 Windows 공개를 진행했으며 나머지 수동 게이트는 보류 상태다.

## 현재 v4 계약

- Project ID: `TbMbCp`
- Manufacturer / Plugin Code: `TbSd` / `TbMb`
- Bundle ID: `com.TeamImpulseImpact.TB_MultiBandCompressor`
- VST3 Processor CID: `ABCDEF019182FAEB5462536454624D62`
- VST3 Controller CID: `ABCDEF011234ABCD5462536454624D62`
- APVTS: 기존 71개 prefix + `phase_mode`, `oversampling`, `lookahead`, `band1..4_channel_mode` = 78개
- State schema / preset format: `4 / 4`
- Range: `-48…+12 dB`, default `-3 dB`; parameter count/ID/order 78은 유지
- Oversampling parameter ID/order 유지, choice append: `OFF / 2X / 4X / 8X`
- main bus: mono/stereo
- optional external sidechain: `Sidechain 1`–`Sidechain 4` optional mono-default aux bus 네 개; stereo layout은 L/R을 하나의 detector key로 downmix하고 각 bus는 `Sidechain N→Band N`으로 고정한다. JUCE fixed 5-bus contract로 구 mono/stereo/quad shared-key 세션 라우팅은 보장하지 않으며 업데이트 후 Send/Aux 재연결이 필요할 수 있다. Host parameter는 band별 `INT/EXT`, custom editor label은 항상 `SIDECHAIN`
- detector mode/frequency/bandwidth: 기존 ID·choice·raw value/state round-trip은 보존하되 current effective DSP는 BAND/FOLLOW 고정. Raw FREE와 detector coordinates는 DSP에 inert하며 이전 FREE 음향 결과는 보존하지 않음. 기존 detector Audition control/function은 보존하지만 current effective FOLLOW detector를 듣는 generic-host compatibility surface; custom editor UI hidden
- transient Delta Listen: panel `DELTA` click latch + enabled node right-button hold momentary override; hold-drag는 같은 band Frequency만 편집하고 Range/Delta 대상은 유지; normal release는 pre-hold latch 복원, lifecycle/external reset은 복원 없이 Off; exclusive Off/Band 1–4, Delta state는 parameter/state/preset/automation/Undo 밖
- band channel: Stereo/Mid/Side. Stereo Link parameter/attachment와 host/state/DSP는 호환성을 위해 유지하지만 custom editor의 `LINK` label/control, bounds와 hit target은 숨김
- 기본 latency: Dynamic + Off + 0 ms에서 0 samples
- 총 latency: Lookahead + Linear FIR/partition + 선택한 JUCE oversampling round trip
- Tail: 0 seconds
- Factory: ID 1–20, `START/SOURCE/BUS/MASTER/MOTION`; 모두 BAND/FOLLOW이고 detector coordinates=processing band; 모든 global Output `0 dB`
- Factory signed-direction fixtures: ID 12 `Transient Lift`와 ID 20 `Parallel Drum Punch`의 positive bands는 Compress/above-threshold boost, ID 19 `Keys Space` positive bands는 Expand/below-threshold detail lift
- global Output과 Band Output parameter/DSP/state/automation은 compatibility를 위해 유지하지만 custom editor UI에서는 숨김
- Dynamics: Compress는 threshold 위, Expand는 threshold 아래가 active side; `+Range`는 양의 gain, `-Range`는 음의 gain이며 meter/output 부호도 동일
- State migration: exact v3/v2 78 identity와 exact v1 71 contract만 허용한다. 각 legacy Range는 당시 `-24…+24 dB`로 먼저 검증한 뒤 v4 range로 clamp해 기존 `+24 dB`를 `+12 dB`로 바꾸며, 성공한 legacy data는 다음 저장 시 v4가 된다.
- Analyzer response: plot top/zero/bottom `y=38/188/338`, 각 half `150 px`; 왼쪽 tick `+12/+6/0/-24/-48 dB`. Plot 전폭의 dark pale-yellow `#817C66`, alpha `1.0`, `3.0 px` actual DSP stroke 하나만 사용하고 idle/outside-band 구간은 0 dB datum, active 구간만 위·아래로 변형한다. Range maximum은 node Y로만 표시하며 별도 datum/configured/red overlay, gain fill/shadow/gradient는 없다.
- Analyzer INPUT: engine 직전 main INPUT spectrum 하나를 response 축과 독립된 `0…-96 dBFS`로 plot 전체 높이에 매핑하고 상단 `0 dBFS` caption을 표시한다. UI-local `ANALYZER` toggle과 고정 `INPUT` label만 사용하며 숫자 debug readout, PRE/POST selector, processed spectrum overlay와 output analyzer tap/FIFO/API는 없다.
- Analyzer depth: outer glass는 유지하고 plot 내부만 `#20242C → #141820`으로 더 옅게 구분한다.

## Gate 상태

| Gate | 상태 | 현재 근거 / 남은 범위 |
| --- | --- | --- |
| G0 범위·계약 | `PASS` | source·authored tests·제품 문서·preset 20-map과 fresh provenance가 single-live-response/78/v4 계약에 일치 |
| G1 DSP Harness | `PASS` | Debug/Release direct run 각각 `59/59`; current transcript와 Release 성능 수치 SHA 고정 |
| G2 Debug Processor | `PASS` | Debug `11/11`; single live response·Range node Y·hidden LINK·header·raster와 Debug capture 10개 확인 |
| G3 Release Processor | `PASS` | Release `11/11`; Debug과 동일한 current integration suite와 raster asset 검사 통과 |
| G4 Plugin build | `PASS / MANUAL HOLD` | Debug/Release Standalone+VST3 full rebuild exit `0`, Release AMD64 manifest 성공; Debug toolchain warning은 남아 있고 native interaction은 수동 HOLD |
| G5 VST3 host contract | `PASS / MANUAL DAW HOLD` | current Release probe exit `0`; processor/profile에서 네 `Sidechain 1–4` 선언·고정 매핑 확인, production DAW 메뉴는 미확인 |
| G6 Final UI automated | `PASS / MANUAL HOLD` | Debug capture 10개와 단일 response/header/hidden LINK/no-fill 계약 PASS; Release native interaction은 수동 HOLD |
| G7 Listening·DAW·distribution | `NOT RUN / HOLD` | level-matched listening, production DAW, longevity, ImageGen asset rights·기타 rights/signing/install 필요 |

## DSP 자동 결과 — current `PASS`

Current four-bus sidechain source와 authored Harness source의 Debug/Release direct run은 각각 `59/59`이다. Transcript와 binaries는 아래 current provenance에 고정했다.

- 48 kHz latency:
  - Dynamic + Off + 0 ms: `0 samples`
  - 10 ms Lookahead: `480 samples`
  - Linear: `1472 samples`
  - JUCE 2× / 4× / 8× round trip: `49 / 61 / 65 samples`
- Neutral impulse peak: 2× `49/49`, 4× `61/61`, 8× `65/65`.
- Linear neutral reconstruction error `0.000000000`; impulse symmetry error `0.00000001`.
- Linear+8×+20 ms: 44.1/192 kHz, mono/stereo, variable/oversize block finite pass.
- Default와 Linear+8× callback `new/delete`: `0`.
- Delta Listen 집중 검증: selected-band wet-minus-dry identity, 20 ms exclusive transfer, legacy detector Audition precedence, Mix 독립, Band/global Output, final Bypass, per-bus EXT non-leak, `Sidechain N→Band N` mapping과 Mid/Side 및 mono fallback, Linear+8×+Lookahead latency alignment.
- Signed Range 집중 검증: Range host/sanitise endpoint `-48/+12 dB`, Compress-above와 Expand-below, meter/output gain의 동일 부호와 cap을 확인했다.
- Current clean Release direct run, 48 kHz / 64 samples, p50 / p95 / p99 / max:
  - Default: `11.80 / 11.90 / 17.40 / 130.80 µs`
  - Worst Dynamic: `17.40 / 24.30 / 25.30 / 86.90 µs`
  - Linear+8×: `321.60 / 439.40 / 581.30 / 1815.60 µs`
  - 8 worst Dynamic instances: `139.50 / 155.00 / 253.60 / 1205.50 µs`

48 kHz/64-sample deadline은 `1333.33 µs`다. Default p99는 약 `1.3%`, Worst Dynamic p99는 약 `1.9%`, Linear+8X p99는 약 `43.6%`, eight-instance p99는 약 `19.0%`다. Linear+8X max는 약 `136.2%`로 occasional outlier가 deadline을 넘지만 p99는 deadline 안이며, 나머지 측정군의 p99도 모두 deadline 안이다. Callback `new/delete`는 Default와 Linear+8× 모두 0이다. Current clean-idle transcript SHA는 아래 provenance에 고정했다.

## Processor·build·host 자동 결과

Four-bus sidechain source의 Debug/Release Processor Integration은 각각 `11/11`, main Debug/Release full rebuild는 각각 exit `0`, current Release VST3 host probe는 exit `0`으로 통과했다. Suite는 다음 계약을 current source에서 증명했다.

- 기존 71 prefix + 신규 7개, 총 78 parameter와 host bypass
- mono/stereo main, fixed four optional mono-default sidechain buses, and stereo `(L+R)/2` detector downmix
- quality latency와 mono Channel Mode fallback
- bit-exact bypass/dry/neutral delta
- band별 `Sidechain N→Band N` isolation, four-key simultaneous ducking, missing-bus fallback/non-leak, connected silence
- 단일 INPUT analyzer demand/FFT/offline gate, output analyzer tap/FIFO/API 부재
- v4 state round-trip, exact v3/v2 78 identity, exact v1 71 migration, legacy Range validation·`+24→+12` clamp와 atomic rejection
- 20 Factory preset full recall/고유성/Undo/Redo/audio smoke
- Factory ID 12·20 positive band의 explicit Compress, ID 19 positive band의 Expand와 signed Range 방향 의도
- isolated User preset round-trip
- editor lifecycle/resize, SIDECHAIN invariant text/state, hidden detector/LINK UI, graph node-over-edge hit arbitration와 `-48…+12 dB`/frequency-invariant/one-Undo Range, Range maximum의 node-Y-only 표시, `38/188/338` response geometry, independent `0…-96 dBFS` INPUT mapping, disabled saved-Range ghost, 단일 `#817C66`/alpha 1/3 px actual path와 별도 datum/configured/red/fill/shadow/gradient 부재, Delta button latch/right-hold 중 same-band Frequency edit·Range invariant·button-loss/pre-hold restore/external-reset guard, legacy Audition preservation/lifecycle reset과 PNG path
- BinaryData generated knob의 byte size·512×512 RGBA decode·alpha/chroma 계약, titled rotary 8개의 `{20+100i,40,84,76}`/100 px pitch/16 px gap/20 px margins와 Frequency/Width/Threshold/Range/Ratio/Knee/Attack/Release 순서, 85/100/150% raster rendering, legacy high-saturation orange upper-bound, 정확한 header `Multi Band Compressor`와 M/B/C `#7088B2/#4F805F/#C65350`를 확인

Current Release VST3 host probe는 declared host-profile/state/preset `1/4/4`, parameters `78`, writable `78`, bypass `1`, cases `207`, state compared `78`을 통과했다. Host probe 자체는 Send/Aux bus 메뉴를 열지 않으므로 네 bus의 production DAW 표시·routing은 수동 게이트로 남긴다.

## Final UI 계약 — 자동 결과 `PASS`, native interaction `HOLD`

- Editor `1000×620`; Analyzer `[12,54,976,499]`; visible plot `{42,38,892,300}`; physical top/zero/bottom은 `y=38/188/338`이고 두 half는 각각 `150 px`다. 왼쪽 response tick `+12/+6/0/-24/-48 dB`는 `y=38/113/188/263/338`이며 오른쪽에는 response Y축 숫자가 없다.
- fixed centered panel `{88,400,824,120}`; node pointer와 Details shelf 없음.
- full-height coloured band edge와 center-emphasised horizontal gradient.
- plot 전폭을 잇는 dark pale-yellow `#817C66`, alpha `1.0`, `3.0 px` rounded actual DSP stroke 하나. Idle/outside-band 구간은 `y=188`의 0 dB datum이고 active 구간만 양수면 위, 음수면 아래로 같은 연속선이 변형된다. Range maximum은 node의 signed Y로만 표시한다. 별도 fixed datum, configured Range contour, red actual overlay, gain fill/wash/shadow/gradient는 없다. Node 중심 `≤12 design px`는 edge보다 우선하고 disabled ghost도 saved Range Y를 유지한다.
- Detector mode/range와 기존 detector Audition control, FREE bracket 없음. 기존 host/state raw value는 compatibility-only로 round-trip하지만 raw FREE/range는 DSP에 inert하고 effective FOLLOW다.
- 글자가 항상 `SIDECHAIN`인 Off/On toggle, `DELTA/DELTA ON`과 Stereo/Mid/Side. Stereo Link parameter/attachment와 host/state/DSP는 보존하지만 `LINK` label/control, bounds와 hit target은 custom editor에 없다.
- main label은 정확히 `WIDTH`다. 저장/동작 값은 기존 octave bandwidth이며 Q나 `BW`로 표기하지 않는다.
- bottom strip `[12,565,976,43]`; compact Bypass/Dynamic/Linear/OFF/2X/4X/8X/Lookahead와 horizontal Mix.
- global Output과 Band Output UI는 숨기되 compatibility parameter와 final DSP gain은 유지.
- Engine 직전 main INPUT spectrum 하나만 smoothed fill/line으로 표시하고 response 축과 독립된 `0…-96 dBFS` mapping을 사용한다. 상단 좌측 고정 `INPUT` source label, top `0 dBFS` caption과 우측의 단일 `ANALYZER` toggle을 둔다. Band/actual/input 숫자 debug readout, PRE/POST selector와 processed spectrum overlay는 표시하지 않는다.
- visual palette는 charcoal/graphite chassis, graph-only lighter `#20242C/#141820`, 단일 dark pale-yellow `#817C66` live response/datum, Signal Red Delta/Bypass, Slate Blue input spectrum/sidechain, muted Green general On, Oxide Red/Console Green/Enamel Blue/Calibration Ivory band와 제한된 Aged Brass tick/value marker로 고정한다. Header는 정확히 `Multi Band Compressor`이며 M/B/C는 `#7088B2/#4F805F/#C65350`, 나머지는 `#D9DAE1`이다. 주황색은 concept accent로 사용하지 않는다.
- `Source/Assets/tb_knob_bakelite.png`는 generated 512×512 RGBA pointer-free Bakelite body다. BinaryData에서 decode해 고정 방향으로 그리며 raster body를 parameter angle로 회전하지 않고 100% visible diameter는 최대 50 px다. Frequency/Width=Blue, 인접한 Threshold/Range/Ratio/Knee=Green, Attack/Release=Red cap tint와 Aged Brass ticks/value marker, 값 text만 procedural layer로 갱신한다. Band Output knob는 없다.
- Fresh Debug capture 10개를 85/100/150%, internal, `sidechain_on`, `delta`, bypass, MASTER, Linear/8X/Mid와 four-band 상태로 재생성해 자동 검증을 통과했다. 모든 상태에서 왼쪽 response tick, 고정 `INPUT` label과 top `0 dBFS`, 독립 INPUT spectrum, 단일 `#817C66` response/datum stroke와 Range node Y를 유지하고 `LINK` UI, 별도 datum/configured/red overlay, gain fill/shadow/gradient, 오른쪽 response 숫자, 좌상단 숫자 debug, POST/PRE source button과 output spectrum이 없음을 확인했다. 기본 negative fixture는 같은 선이 0 dB 아래, positive fixture는 위로 변형되고, `sidechain_on`은 invariant `SIDECHAIN` label, On visual state와 `NO SC INPUT`, `delta`는 `DELTA ON`과 node ring을 표시한다. 85/100/150% 각각에서 정확한 header와 8개 raster knob의 alpha/chroma box·crop 부재를 확인했다.
- 구형 Details 및 오인 이름의 capture는 `output/ui-qa/archive/`로 격리했다.

제공된 vintage compressor 이미지는 저채도 studio hardware의 재질·palette 방향에만 사용했고 참조 이미지의 픽셀, 로고, 문자, meter, panel layout 또는 control arrangement를 포함하지 않는다. 이는 trade-dress 비침해나 배포 권리에 대한 법적 결론이 아니므로 공개 배포 전 human similarity review와 ImageGen asset-rights 확인은 수동 `HOLD`로 남긴다.

Component capture는 native hit-test를 대신하지 않는다. 실제 Release Standalone에서 node 중심 `≤12 px` 우선, edge/wheel, Range `-48…+12 dB` full axis/frequency invariant/one Undo, node right-hold 중 same-band Frequency 이동·Range 불변·mouse-up/button-loss release·pre-hold latch restore·external-reset no-restore·empty no-op, `DELTA` button latch/sync, SIDECHAIN Off/On, 네 mono `Sidechain 1–4` routing, text entry, preset/Undo/Redo와 hide reset을 수동 확인해야 한다.

## Source fingerprint와 artifact provenance — current `PASS`, previous Range-overlay `STALE`

Current UTF-8 manifest는 첫 행 `TB_MultiBandCompressor.jucer`, 뒤에는 `Source/` 아래 10개 파일을 `/` 상대 경로 ordinal 순서로 정렬하고 authored test/tooling 5개는 아래 재현 목록의 case-insensitive ordinal 순서로 붙인다. 각 행은 `relative-path|SHA-256`, 구분자는 LF, 마지막 LF는 없다. Current manifest는 `16`행/`1,501` bytes이고 source+tests fingerprint는 `70F281C8F15C9D9DA1C7CE3876544EF93894D5EA65C9B11766179D3B9AA617BC`다.

```text
paths = ["TB_MultiBandCompressor.jucer"]
      + ordinalSort(normalizeToForwardSlash(allRegularFilesRecursivelyUnder("Source")))
      + ordinalSortIgnoreCase([
          "Tests/build_processor_tests.ps1",
          "Tests/DSPHarness.cpp",
          "Tests/DSPHarness.jucer",
          "Tests/ProcessorIntegration/ProcessorIntegrationTests.jucer",
          "Tests/ProcessorIntegrationTests.cpp"
        ])
rows = [path + "|" + upperHex(SHA256(fileBytes(path))) for path in paths]
manifestBytes = UTF8_without_BOM(join(rows, LF))  # final LF 없음
sourceFingerprint = upperHex(SHA256(manifestBytes))
```

| Current source + authored tests | SHA-256 |
| --- | --- |
| `TB_MultiBandCompressor.jucer` | `8CC8AC4F6A0F8F2415B8B0B6634EAC11DAD1CE2684A793078BE0DA0341EB615B` |
| `Source/Assets/tb_effect_logo.png` | `B6E9389937AA1065EEC5BFB2CCB38AC633FD0F76499AB6F4309F9A5E2C57D3B4` |
| `Source/Assets/tb_knob_bakelite.png` | `13AA4B40E297DEDE8EFDAECE4B9234D33B78A4435C0FB67DEEE2A2E088A88EEE` |
| `Source/FactoryPresets.h` | `56D1FF6CCA23857AF9A8FEFB62429DFAFAE47A40DE98DC334EB26618214D1EC4` |
| `Source/MultibandEngine.cpp` | `6E21870300A6604A4316911964C87C74C2AFECAC64FAB1BEB030E8B3244F4FDD` |
| `Source/MultibandEngine.h` | `8E75F089FB106EFCFF2749EA13237007A474F7B64D644AD591FDA7BA461D0495` |
| `Source/Parameters.h` | `763804FDE76545FC80371C96218D381F55AAFD223DA2019DF5C6CD924D5F018B` |
| `Source/PluginEditor.cpp` | `F79574D175F3355C3EECE7A55CD5F8D2CC94330B76F84C8D9EFB66BBBE3C80AE` |
| `Source/PluginEditor.h` | `EA7F32E76364B2EB11C14E0F211EC14D0D5667DE254B74B9A64A6A53A9625B85` |
| `Source/PluginProcessor.cpp` | `67EC4EF21CC07ABA2FEA692AE5AA31F42C0F1EF3A39687745E275BB47903FBF0` |
| `Source/PluginProcessor.h` | `B7901183D10F5C34ED477FD6A9B3D366639B11FF75F726B1D8701651DB13ACB5` |
| `Tests/build_processor_tests.ps1` | `7A0D0BDD68D937F64ABD80369F952D921D26562DF73C0EAB8450EB1BE2E5CE34` |
| `Tests/DSPHarness.cpp` | `CF9E9FC906474C1C2C7F08F4DE787D09CB4A007846B034B778666509735AA611` |
| `Tests/DSPHarness.jucer` | `A2DB45DA77F256D15EEF91A6B9C431417B1B84AA5DD7F570E81C5979E48515F1` |
| `Tests/ProcessorIntegration/ProcessorIntegrationTests.jucer` | `FE395F4504F8A2EE80037C05A8E1A66ED250B6A04FF83A417F25ACA44D586442` |
| `Tests/ProcessorIntegrationTests.cpp` | `4C5DB067CE2202ECE033B68265DBCB03B0CDFDB0A82A13189E09703F688460BC` |

모든 상대 경로는 프로젝트 루트 기준이며 수정 시각은 KST다. `output/qa/dsp-harness-release-single-live-response.txt`가 current Release 성능 transcript이며, 역사적 `output/performance/release-dsp-harness.txt`는 current 표에 포함하지 않는다.

| Current artifact / evidence | Bytes | Modified (KST) | SHA-256 |
| --- | ---: | --- | --- |
| `output/qa/main-build-debug-single-live-response.txt` | 4,170 | 2026-08-13 15:49:32.119 | `A4DCEAFF3E130CCB3B95C1C4CCF097D3E704A86CA73E445CE7EAB3B52F57C5DD` |
| `output/qa/main-build-release-single-live-response.txt` | 4,396 | 2026-08-13 15:52:24.318 | `89EB14844E1EE4E685E7B910341A38EBA38FB6D93FCAAB1F947A00D4415CD373` |
| `output/qa/dsp-harness-debug-single-live-response.txt` | 11,376 | 2026-08-13 15:46:24.322 | `DAAC214269E4E2AAB263B6ACBB1F71CDB39B11C18F7D03E80050AC1210B19B96` |
| `output/qa/dsp-harness-release-single-live-response.txt` | 11,316 | 2026-08-13 15:47:34.958 | `D7EF93FFD1B0CDB918E5EE803C4083E4F4D5EECC203CBD9BD5F4B9C242B1E58F` |
| `output/qa/processor-integration-debug-single-live-response.txt` | 1,210 | 2026-08-13 15:45:51.155 | `F1917AF670317634350336C9955297F320CC2EEAFC39092769EC214A50967B49` |
| `output/qa/processor-integration-release-single-live-response.txt` | 1,210 | 2026-08-13 15:46:06.373 | `F1917AF670317634350336C9955297F320CC2EEAFC39092769EC214A50967B49` |
| `Tests/Builds/VisualStudio2022/x64/Debug/ConsoleApp/TB_MultiBandCompressor_DSPHarness.exe` | 5,823,488 | 2026-08-12 22:16:16.602 | `6221C7D610659FA57CB5640AEBD7BEB7C32688620A8F7F372B145D7D5A9C0F32` |
| `Tests/Builds/VisualStudio2022/x64/Release/ConsoleApp/TB_MultiBandCompressor_DSPHarness.exe` | 1,810,944 | 2026-08-13 02:13:12.031 | `2CA20981C8F449EF0C45B64265BA327AD129B713390551E9CD7D507313FE1D0D` |
| `Tests/ProcessorIntegrationBuilds/VisualStudio2022/x64/Debug/ConsoleApp/TB_MultiBandCompressor_ProcessorIntegrationTests.exe` | 21,603,840 | 2026-08-13 15:41:34.273 | `12D7ACC42118F2FAAF02F17BEBF7DA0FBFDC247444B85006ABD27D2540A76572` |
| `Tests/ProcessorIntegrationBuilds/VisualStudio2022/x64/Release/ConsoleApp/TB_MultiBandCompressor_ProcessorIntegrationTests.exe` | 5,020,672 | 2026-08-13 15:44:04.257 | `89B0D07299DEBE56B8F8753C544179360C7E881C20890A3898D446C0803EDCD9` |
| `Builds/VisualStudio2022/x64/Debug/Standalone Plugin/TB_MultiBandCompressor.exe` | 23,238,656 | 2026-08-13 15:49:24.267 | `1AF7006653057FE4A747AF9DDE689C03C4BC7984D896F917812FAF2F6C53D023` |
| `Builds/VisualStudio2022/x64/Debug/VST3/TB_MultiBandCompressor.vst3/Contents/x86_64-win/TB_MultiBandCompressor.vst3` | 22,871,552 | 2026-08-13 15:49:29.939 | `2617D76772203F8210C4F7F73F4E2F1A4FCFD231F778ED6571B25E12CDD08EF4` |
| `Builds/VisualStudio2022/x64/Release/Standalone Plugin/TB_MultiBandCompressor.exe` | 7,408,128 | 2026-08-13 15:51:39.989 | `3736B6B01C88A32FFDC323287E3266D62BA9957AA7DE42FD3856A04BDBBF0DF2` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/x86_64-win/TB_MultiBandCompressor.vst3` | 7,172,608 | 2026-08-13 15:52:23.296 | `EE5CF272FC25A0BFF4E26C13074968A0BE103720254F29ED6F77663B0C8B7CE1` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/Resources/moduleinfo.json` | 1,190 | 2026-08-13 15:52:24.305 | `389D2783B986F68B0F39C8A8A55C80EF5A6ACCD8607B754DC134CA83D60B8297` |
| `Resources/TB_HOST_PROFILE.json` | 25,040 | 2026-08-12 22:18:44.539 | `5F63863D98A8C3A78D14B962D052B57249CDA61374523D7AFD6AFA45B8C33362` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/Resources/TB_HOST_PROFILE.json` | 25,040 | 2026-08-12 22:18:44.539 | `5F63863D98A8C3A78D14B962D052B57249CDA61374523D7AFD6AFA45B8C33362` |
| `../Guide MD/Tools/TBHostContractProbe/Builds/VisualStudio2022/x64/Release/ConsoleApp/TBHostContractProbe.exe` | 1,170,944 | 2026-08-13 15:10:56.641 | `AEBA309B92BDF609EF03EFCE4774E00105CDE8B88F51914CD387948B8956BD34` |
| `output/contract/release-host-contract.json` | 64,775 | 2026-08-13 15:52:39.112 | `5EEFDB34CE554F01BBE8F57FA631ECAB61408377EB00E377C3B2D650C3197EE7` |
| `output/qa/host-contract-single-live-response.txt` | 912 | 2026-08-13 15:52:39.352 | `2A028A9DA6ADD3BBEC8950CDAD36387FE4A8592D3DEF1A6574187EE49282CF09` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_85pct.png` | 65,384 | 2026-08-13 15:42:42.786 | `40F2001E4D46382152B9969252F6EBE9FF0400537FFEBB15A08E0068602C8196` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_150pct.png` | 120,022 | 2026-08-13 15:42:44.721 | `648741E43BDC6EB66B556C3A396F2888F2305FD1FB1725D3FF7EC122435F3FB2` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_100pct.png` | 76,701 | 2026-08-13 15:42:46.862 | `0DB1DEE8E533976B7E646D2E39E30CE59DC4BCEFF4F244CCC70A2AA47B9877F2` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_internal_band.png` | 72,341 | 2026-08-13 15:42:48.181 | `2E3405CB29AF0D5AC55F5BF7CD7899004BE7A299C890D574A260B00F5F79F757` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_sidechain_on.png` | 69,069 | 2026-08-13 15:42:49.800 | `B3368A55E1D181CFF423D0ABA81D051ED0251D92EEFAE9389BA17FB7D0775F01` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_delta.png` | 76,554 | 2026-08-13 15:42:51.333 | `8471C26D5C400337C11C65D495B6F89ADAF914EFDAA54E0CDADB74FB837A98C7` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_bypass.png` | 70,825 | 2026-08-13 15:42:52.105 | `907EE15C8404E1CAD2D948E94583983277FB36A043C6ABA1BF2309C93E3D032E` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_master_preset.png` | 73,731 | 2026-08-13 15:42:53.199 | `2B8D6674343BFC53692B905C765923EB3E9BFFFB7ACA0C926B6326EC94A0FA8E` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_positive_8x_mid.png` | 74,356 | 2026-08-13 15:42:54.120 | `E9A559D844293B0871EEF62DF927B526FC51B636313C1B4566CFBE4D101716F7` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_four_bands.png` | 76,968 | 2026-08-13 15:42:54.702 | `7FA28D36AFBCDD7CFFF94676F1CB712637A5A65745378113BF7A5E1F40332631` |
| `DesignAssets/tb_knob_bakelite_chroma.png` | 1,940,148 | 2026-08-12 17:30:49.914 | `927A66AE919C502E9CB36204185EDB4D37E3D7482C44FCF449C80143E33E8754` |
| `C:/Users/ktb70/.codex/generated_images/019ff131-87af-7ab0-b9d3-22e4ca132de2/exec-79df3586-165e-4898-ad6b-f20e6968f538.png` | 1,940,148 | 2026-08-12 17:30:49.914 | `927A66AE919C502E9CB36204185EDB4D37E3D7482C44FCF449C80143E33E8754` |

Automated current evidence는 위 source+tests fingerprint와 artifact rows에 대해 `PASS`다. 이 폴더에는 VCS revision metadata가 없으므로 이후 source/test 변경이나 재빌드가 있으면 같은 알고리즘으로 fingerprint와 artifact 표를 다시 생성해야 한다.

### Previous v4 Range-overlay snapshot — `STALE`

아래 UTF-8 manifest는 첫 행 `TB_MultiBandCompressor.jucer`, 뒤에는 당시 `Source/` 아래 10개 파일을 `/` 상대 경로 ordinal 순서로 정렬했다. 각 행은 `relative-path|SHA-256`, 구분자는 LF, 마지막 LF는 없다. Previous manifest는 11행/1005 bytes이고 source fingerprint는 `CAF1DED10C6048CA7B0C362F4E60752CC7371EAA45EB333DEB0E1A79E48387B8`이다.

재현 알고리즘은 다음과 같다. 경로와 hash는 표의 11개 row와 일치해야 한다.

```text
paths = ["TB_MultiBandCompressor.jucer"]
      + ordinalSort(normalizeToForwardSlash(allRegularFilesRecursivelyUnder("Source")))
rows = [path + "|" + upperHex(SHA256(fileBytes(path))) for path in paths]
manifestBytes = UTF8_without_BOM(join(rows, LF))  # final LF 없음
sourceFingerprint = upperHex(SHA256(manifestBytes))
```

| Previous source (`STALE`) | SHA-256 |
| --- | --- |
| `TB_MultiBandCompressor.jucer` | `E244533D4A745838E533E3E9257978F2972C644B3017DC373409FEC64C788973` |
| `Source/Assets/tb_effect_logo.png` | `B6E9389937AA1065EEC5BFB2CCB38AC633FD0F76499AB6F4309F9A5E2C57D3B4` |
| `Source/Assets/tb_knob_bakelite.png` | `13AA4B40E297DEDE8EFDAECE4B9234D33B78A4435C0FB67DEEE2A2E088A88EEE` |
| `Source/FactoryPresets.h` | `56D1FF6CCA23857AF9A8FEFB62429DFAFAE47A40DE98DC334EB26618214D1EC4` |
| `Source/MultibandEngine.cpp` | `6E21870300A6604A4316911964C87C74C2AFECAC64FAB1BEB030E8B3244F4FDD` |
| `Source/MultibandEngine.h` | `8E75F089FB106EFCFF2749EA13237007A474F7B64D644AD591FDA7BA461D0495` |
| `Source/Parameters.h` | `763804FDE76545FC80371C96218D381F55AAFD223DA2019DF5C6CD924D5F018B` |
| `Source/PluginEditor.cpp` | `DFBD8BD1E323646620FBDB8C08D7EE56DF11BD45CFF9D16881EA601B4B638119` |
| `Source/PluginEditor.h` | `EA7F32E76364B2EB11C14E0F211EC14D0D5667DE254B74B9A64A6A53A9625B85` |
| `Source/PluginProcessor.cpp` | `67EC4EF21CC07ABA2FEA692AE5AA31F42C0F1EF3A39687745E275BB47903FBF0` |
| `Source/PluginProcessor.h` | `B7901183D10F5C34ED477FD6A9B3D366639B11FF75F726B1D8701651DB13ACB5` |

모든 경로는 프로젝트 루트 기준이며 수정 시각은 KST다.

| Previous artifact / evidence (`STALE`) | Bytes | Modified (KST) | SHA-256 |
| --- | ---: | --- | --- |
| `output/qa/main-build-debug-range-overlay-v4.txt` | 4,170 | 2026-08-12 22:35:41.435 | `A4DCEAFF3E130CCB3B95C1C4CCF097D3E704A86CA73E445CE7EAB3B52F57C5DD` |
| `output/qa/main-build-release-range-overlay-v4.txt` | 4,396 | 2026-08-12 22:39:00.552 | `89EB14844E1EE4E685E7B910341A38EBA38FB6D93FCAAB1F947A00D4415CD373` |
| `output/qa/dsp-harness-debug-range-overlay-v4.txt` | 11,378 | 2026-08-12 22:40:51.656 | `F8F1DBB35DBB5A2486B27DC46CADFACE3EAEB7799978D5365FCEFB85C798B8D4` |
| `output/qa/dsp-harness-release-range-overlay-v4.txt` | 11,316 | 2026-08-12 22:42:14.680 | `5FF59717FE59A7FFEA0F4811885D1CC1E61B370D6A3B138F6FA3252975C06D5F` |
| `output/performance/release-dsp-harness.txt` | 11,316 | 2026-08-12 22:42:14.680 | `5FF59717FE59A7FFEA0F4811885D1CC1E61B370D6A3B138F6FA3252975C06D5F` |
| `output/qa/processor-integration-debug-range-overlay-v4.txt` | 1,210 | 2026-08-12 22:40:04.061 | `F1917AF670317634350336C9955297F320CC2EEAFC39092769EC214A50967B49` |
| `output/qa/processor-integration-release-range-overlay-v4.txt` | 1,210 | 2026-08-12 22:40:17.642 | `F1917AF670317634350336C9955297F320CC2EEAFC39092769EC214A50967B49` |
| `Tests/Builds/VisualStudio2022/x64/Debug/ConsoleApp/TB_MultiBandCompressor_DSPHarness.exe` | 5,823,488 | 2026-08-12 22:16:16.602 | `6221C7D610659FA57CB5640AEBD7BEB7C32688620A8F7F372B145D7D5A9C0F32` |
| `Tests/Builds/VisualStudio2022/x64/Release/ConsoleApp/TB_MultiBandCompressor_DSPHarness.exe` | 1,810,944 | 2026-08-12 22:16:55.920 | `3D8D5FED98A05697812FBE8E515789F8E6E571B6D2F61A9A997EDAE3BC23D7BC` |
| `Tests/ProcessorIntegrationBuilds/VisualStudio2022/x64/Debug/ConsoleApp/TB_MultiBandCompressor_ProcessorIntegrationTests.exe` | 21,597,696 | 2026-08-12 22:31:39.681 | `0833DC59357047E80ED8FB694542B6EB0C09D7299583522F8D88CFF8F7F0F1FD` |
| `Tests/ProcessorIntegrationBuilds/VisualStudio2022/x64/Release/ConsoleApp/TB_MultiBandCompressor_ProcessorIntegrationTests.exe` | 5,018,624 | 2026-08-12 22:33:21.931 | `E06A05DEA81DFAF2C6915E6FAC6EAED83AACCCF458FCDDE348687464E777F8B1` |
| `Builds/VisualStudio2022/x64/Debug/Standalone Plugin/TB_MultiBandCompressor.exe` | 23,242,240 | 2026-08-12 22:35:36.887 | `73D3EE2A0B1B23426D9EAED12C6BDFC724DD1487267D1A2ADD3706703F5C6513` |
| `Builds/VisualStudio2022/x64/Debug/VST3/TB_MultiBandCompressor.vst3/Contents/x86_64-win/TB_MultiBandCompressor.vst3` | 22,874,112 | 2026-08-12 22:35:40.757 | `87D45061208A54F567CCB627E137A327123AAA97F476880B8997F44A77FB9EEA` |
| `Builds/VisualStudio2022/x64/Release/Standalone Plugin/TB_MultiBandCompressor.exe` | 7,410,176 | 2026-08-12 22:37:54.770 | `81FA50FCD68A0D7CD2608934DC4298B8223831F566BB62A672AD0BE0404BA748` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/x86_64-win/TB_MultiBandCompressor.vst3` | 7,174,144 | 2026-08-12 22:39:00.038 | `3C8BA7AEBA908BAAB4190033B299C2D270D4BB68008B2651E8A3D1070AA7AF07` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/Resources/moduleinfo.json` | 1,190 | 2026-08-12 22:39:00.547 | `7F565AFA7CBB18A0AD22530F086F6DFF671BEBB32EDAC16A5827A7DADE24C0C5` |
| `Resources/TB_HOST_PROFILE.json` | 25,040 | 2026-08-12 22:18:44.539 | `5F63863D98A8C3A78D14B962D052B57249CDA61374523D7AFD6AFA45B8C33362` |
| `Builds/VisualStudio2022/x64/Release/VST3/TB_MultiBandCompressor.vst3/Contents/Resources/TB_HOST_PROFILE.json` | 25,040 | 2026-08-12 22:18:44.539 | `5F63863D98A8C3A78D14B962D052B57249CDA61374523D7AFD6AFA45B8C33362` |
| `../Guide MD/Tools/TBHostContractProbe/Builds/VisualStudio2022/x64/Release/ConsoleApp/TBHostContractProbe.exe` | 1,170,944 | 2026-08-12 19:25:30.705 | `1201CC6C3B504450AA78820309D1F7F87663A68133384707708A622C2DCDDCD8` |
| `output/contract/release-host-contract.json` | 64,775 | 2026-08-12 22:39:21.355 | `447445BDCA3E35D8617FB4620D18DCEEDC8D10CF83541AB31F2D17C634F857B9` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_85pct.png` | 70,734 | 2026-08-12 22:39:56.802 | `1568E8A7C66A50BBFB4F60404F6BAA10DEE063D6142C0E49AC1318C341C431F1` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_150pct.png` | 129,589 | 2026-08-12 22:39:57.688 | `C37D49756038B9B9A077108031B4F64998E1EA07E8B3A5CB6044BCE8097F4E91` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_100pct.png` | 82,120 | 2026-08-12 22:39:58.388 | `3686424CFAA62187D39C320DAA98E9B345C355E5E9A99BB65E9E9F3525C805A2` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_internal_band.png` | 76,156 | 2026-08-12 22:39:59.011 | `7C900E89652960150398EAF735EECC0567AF560BEF72FCE502D5E66FC78E2133` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_sidechain_on.png` | 72,002 | 2026-08-12 22:39:59.623 | `589EEA10DC5F8132FFEFF31731B513A867923C87D506C0400081F9BC1C8AF448` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_delta.png` | 82,079 | 2026-08-12 22:40:00.344 | `A4A8E83CDBE69CDAC9644F4195BA984FA3821B6CA273027FF7BD22A5E5525055` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_bypass.png` | 75,925 | 2026-08-12 22:40:01.073 | `0A0373B1694AA701563C6105C2F6E74B13E82015E6AA2AEF29448002C297E59D` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_master_preset.png` | 76,786 | 2026-08-12 22:40:01.938 | `4EB9A2AB5302087C7B6531F83660C7B348E09B40C96D046D92ECF66220B8338D` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_positive_8x_mid.png` | 81,068 | 2026-08-12 22:40:02.874 | `C18647F480B255E018135F8117EC12C0B89433040AFAEBFDA9AD8BBE1830C8A8` |
| `output/ui-qa/TB_MultiBandCompressor_Debug_four_bands.png` | 82,535 | 2026-08-12 22:40:03.489 | `5917BBD4BADFD1BA77C53FBBC6A034321A361BAD4B0002EBCEA1952C94A000D3` |
| `DesignAssets/tb_knob_bakelite_chroma.png` | 1,940,148 | 2026-08-12 17:30:49.914 | `927A66AE919C502E9CB36204185EDB4D37E3D7482C44FCF449C80143E33E8754` |
| `C:/Users/ktb70/.codex/generated_images/019ff131-87af-7ab0-b9d3-22e4ca132de2/exec-79df3586-165e-4898-ad6b-f20e6968f538.png` | 1,940,148 | 2026-08-12 17:30:49.914 | `927A66AE919C502E9CB36204185EDB4D37E3D7482C44FCF449C80143E33E8754` |

이 표의 main logs와 binaries는 당시 Range-overlay fingerprint source 뒤에 생성됐다. 당시 DSP Debug/Release transcript는 각각 `58/58`, Processor는 `11/11`, probe와 UI capture도 당시 revision에서는 통과했지만 single-live-response source에는 적용하지 않는다. Chroma master와 pointer-free 512×512 RGBA derivative의 asset provenance는 보존하며 current plug-in binary, source+tests fingerprint와 fresh evidence는 위 current `PASS` 표를 따른다.

## State·preset 계약

- v4 current data는 정확한 78개 ID/순서와 각 value의 type/range/choice 조건, Range `-48…+12 dB`를 모두 통과한 뒤 원자 적용한다.
- exact v3는 동일 78개 identity와 당시 value contract를, exact v2는 동일 78개 identity와 당시 Off/2×/4× Oversampling contract를 검증한다. exact v1은 첫 71개 legacy contract를 검증하고 신규 7개를 Dynamic/Off/0 ms/각 band Stereo로 채운다.
- v3/v2/v1 Range는 legacy `-24…+24 dB` 범위로 먼저 검증하고 v4 domain으로 clamp한다. 기존 `+24 dB`는 `+12 dB`가 되며 새 범위 안의 legacy 값은 보존된다.
- count/ID/order 불일치, duplicate/unknown ID, fractional choice, current/legacy out-of-range, future/corrupt data는 현재 상태를 일부도 바꾸지 않는다.
- User preset은 compatibility detector Audition Off 저장, full 78-value recall과 one-step Undo/Redo를 지킨다. Delta Listen은 저장하지 않으며 recall은 panel latch, active right-hold와 pending restore를 모두 취소해 복원 없이 Off로 만든다.
- Factory 20개는 ID/name/group가 고정되고 pairwise distinct, 모두 BAND/FOLLOW이며 detector coordinates=processing band이고 global Output은 모두 `0 dB`다.
- Processing Mode/Oversampling/Lookahead는 writable/non-automatable, Channel Mode는 writable/automatable이다.
- Delta Listen은 78-value state/preset 밖의 transient exclusive monitor다. Panel button은 latch, node right-hold는 momentary override다. 정상 release만 pre-hold latch를 복원하며 성공한 state/preset load, prepare/reset/release, editor hide/destroy와 외부 reset은 active hold/pending restore를 취소하고 복원 없이 Off로 만든다. Undo/automation 대상이 아니다.

## 역사적 STALE evidence

다음은 현재 single-live-response 판정에 합산하지 않는다.

- schema 1의 DSP `28/28`, processor `8/8`, 71-parameter host snapshot
- schema 2의 12 Factory bank와 OFF/2X/4X host/UI snapshot
- Analyzer `976×456`, moving pointer panel, Details shelf, visible global Output을 담은 구형 capture
- Single-input analyzer 변경 전 Release Processor, main Standalone/VST3, profile, host contract, source fingerprint와 artifact hash
- 현재 색·굵기·mapping과 다른 구형 configured response, visible FOLLOW/FREE bracket 또는 detector Listen을 담은 이전 v3 capture
- shared `+24…-24 dB` response/INPUT 축과 별도 fixed datum·vermilion actual contour를 사용한 구형 actual-only capture
- fixed datum/configured Range/red actual의 이중 contour와 gain fill/gradient를 담은 직전 v4 Range-overlay capture, source fingerprint와 build/test/host hash
- 고채도 orange-forward palette와 vector-only rotary를 담은 이전 capture
- `output/ui-qa/archive/`, `output/ui-qa/stale-v3/`, `output/qa/archive/`의 구형 evidence와 과거 artifact SHA 기록

## 수동 Release gate

- `DRM_01_loop`, `VOX_01_female`, `KEY_01_pad`의 level-matched bypass/Dynamic/Linear/quality A/B
- 강한 upward Range, 0.5 ms Attack, 8×/20 ms와 quality switching의 click/pop/zipper·headroom 확인
- 실제 Release Standalone native 화면, audio pass-through, node/edge/우클릭 Delta/DELTA button/SIDECHAIN/slider/preset/Undo interaction과 hide reset
- 서로 다른 production DAW에서 네 mono external sidechain `Sidechain 1–4` 연결/해제, per-band mapping, automation, session recall, PDC, offline bounce, multiple instances
- 고정 조건 Release CPU/메모리 반복 기록과 장시간 안정성
- 제품 EULA/소스 공개 방침, JUCE commercial 또는 AGPL 배포 선택, canonical TB signature 권리, generated knob의 human similarity review와 ImageGen 사용 조건·asset 배포 권리, code signing, installer/uninstaller

## Stop-Ship

다음 중 하나라도 있거나 수동 gate가 열려 있으면 `RELEASE HOLD`를 유지한다.

- crash, NaN/Inf, output runaway, external sidechain leak, wrong-channel cross-trigger 또는 state corruption
- actual latency와 host reported latency 불일치, dry/wet/audition/bypass alignment 실패
- v3/v2 78 identity 또는 v1 71 migration 실패, legacy Range `+24→+12` clamp 실패, invalid/future data partial mutation
- 기존 CID/Parameter ID/order 변경, 78 metadata/flags, Range `-48…+12 dB`, schema/preset 4/4 또는 appended 8X choice 불일치
- Factory가 20개가 아니거나 global Output이 0 dB가 아님
- Release VST3 scan/load/manifest/profile 실패
- 단일 INPUT spectrum 계약 위반, 독립 `0…-96 dBFS` mapping/top `0 dBFS` caption·왼쪽 `+12/+6/0/-24/-48` response tick·INPUT source label 누락, 오른쪽 response 숫자·숫자 debug readout·PRE/POST source selector·processed spectrum overlay·output analyzer tap/FIFO/API의 재등장, `38/188/338` geometry 오류, `#817C66`/alpha 1/3 px가 아닌 response 또는 full-width 연속성·signed 방향 오류, 별도 datum/configured/red overlay·gain fill/shadow/gradient의 재등장, Range maximum이 node Y 외의 contour로 중복 표시됨, `LINK` custom control/label/bounds/hit target 재등장, header가 정확한 `Multi Band Compressor`가 아니거나 M/B/C `#7088B2/#4F805F/#C65350`가 아님, FREE bracket·visible detector/Band Output controls의 재등장, node/edge hit 우선순위·`-48…+12 dB` Range·disabled ghost 오류, Delta hold frequency/Range/latch/restore/reset lifecycle 오류, 8-knob 순서·그룹색·raster alpha/chroma/scale 오류, legacy orange concept 재등장 또는 major UI crop/overlap
- level-matched listening에서 핵심 band workflow와 Dynamic/Linear/8X의 음악적 선택 이유 미확인
- ImageGen asset similarity/license를 포함한 rights/signing/install 조건 미확정

Four-bus sidechain source의 DSP Debug/Release `59/59`, Processor Debug/Release `11/11`, main Debug/Release rebuild·manifest exit `0`, current Release host probe exit `0`, Debug component UI capture 10개와 source+tests/artifact provenance는 fresh `PASS`다. 수동 Release native interaction·청감·production DAW·장시간 안정성·배포 권리/서명/installer gate는 열려 있으므로 최종 판정은 **CURRENT-SOURCE AUTOMATED ENGINEERING PASS / RELEASE HOLD**다.
