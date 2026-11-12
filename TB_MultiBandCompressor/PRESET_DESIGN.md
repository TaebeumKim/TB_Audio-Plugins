# TB_MultiBandCompressor Factory Preset Design

버전: 0.1.0  
Preset format / state schema: 4 / 4  
Factory bank: 20 presets, 5 logical groups  
호환성: Factory ID·이름·그룹과 78개 parameter ID/order는 고정한다. v3/v2/v1 legacy Range `-24…+24 dB`는 exact validation 뒤 v4 `-48…+12 dB`로 clamp하며 기존 `+24 dB`는 `+12 dB`가 된다. 이번 development candidate에서는 signed Range 의미 수정에 맞춰 ID 12와 ID 20 positive band의 mode 값만 의도적으로 갱신

## 뱅크 원칙

- 20개 모두 78개 public parameter의 전체 상태를 명시한다.
- `Init`은 parameter default와 정확히 일치하는 안전한 3-band 시작점이다.
- 수를 채우기 위한 유사 프리셋을 추가하지 않고 서로 다른 사용 이유를 가진 20개만 유지한다.
- `START / SOURCE / BUS / MASTER / MOTION`의 2단계 메뉴를 사용한다.
- Factory recall 시 compatibility detector Audition은 항상 Off다. Transient Delta Listen은 preset 값이 아니며 recall은 panel latch, active right-hold와 pending pre-hold restore를 모두 취소해 복원 없이 Off로 reset한다.
- Output 차이만으로 프리셋을 구분하지 않는다.
- Global Output은 세션·자동화 호환성을 위해 남겨 둔 숨은 파라미터이며, Factory 1–20은 모두 unity 0 dB를 저장한다.
- Factory detector는 모두 `FOLLOW/BAND`이며, 검출 중심 주파수와 폭은 항상 해당 처리 밴드와 일치한다. `EXT`는 고정 external key mapping으로 `Sidechain 1–4`를 Band 1–4에 연결한다. 각 bus는 mono 기본이며 stereo 연결은 `(L+R)/2`로 downmix하고, 선택 bus가 없으면 해당 band만 internal fallback한다. 기존 단일 bus 세션은 업데이트 후 Send/Aux 재연결이 필요할 수 있다.
- Compress는 threshold 위, Expand는 threshold 아래를 활성 영역으로 사용한다. 모든 preset에서 positive Range는 실제 증폭, negative Range는 실제 감쇄다.
- 모든 Factory Range는 현재 host domain `-48…+12 dB` 안에 있어야 한다. Positive Range의 최대값은 `+12 dB`, negative Range의 최대 감쇄는 `-48 dB`다.
- Linear·Oversampling·Lookahead 프리셋은 실제 host latency를 발생시킨다.

## Factory 뱅크 맵

| ID | Group / Name | 단계 | 대상 | 품질·타이밍 | 채널·검출 | 음악적 목적 |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | START / Init | Simple | All | Dynamic, Off, 0 ms | Stereo, INT/BAND | 120 Hz·1.2 kHz·7 kHz의 안전한 3-band 기본 압축 시작점 |
| 2 | START / Gentle Balance | Simple | All | Dynamic, Off, 0 ms | Stereo, INT/BAND | Mix 80%와 약한 -1.5…-2 dB Range로 전체 균형을 가볍게 정돈 |
| 3 | START / Broad Control | Normal | All | Dynamic, Off, 0 ms | Stereo, INT/BAND | 4개 넓은 밴드로 저지연 broadband control 제공 |
| 4 | SOURCE / Vocal Focus | Normal | Vocal | Dynamic, 2X, 2 ms | active bands Mid, INT/BAND | 중앙 보컬의 저역·presence·치찰음을 분리 제어하고 presence는 upward 처리 |
| 5 | SOURCE / Bass Foundation | Simple | Bass | Dynamic, Off, 0 ms | active bands Mid, INT/BAND | 중앙 저역 70 Hz를 고정하고 700 Hz 존재감을 부드럽게 들어 올림 |
| 6 | SOURCE / Drum Tamer | Complex | Drums | Dynamic, 4X, 3 ms | Stereo, INT/BAND | 네 대역의 빠른 peak를 lookahead와 고배율 경로로 안정적으로 억제 |
| 7 | BUS / Mix Bus Balance | Complex | Mix Bus | Dynamic, 2X, 1 ms | Low/Low-mid Mid, High Side | 중앙 저역을 정돈하면서 측면 high-air의 과도한 움직임을 별도로 제어 |
| 8 | BUS / Drum Bus Glue | Normal | Drum Bus | Dynamic, Off, 0 ms | Stereo, INT/BAND | 레이턴시 없이 저·중·고역의 서로 다른 시간 반응으로 glue 제공 |
| 9 | BUS / Mastering Touch | Complex | Master | Linear, 2X, 5 ms | Low Mid, High Side | 작은 Range의 linear-phase mastering control과 M/S 균형 시작점 |
| 10 | MOTION / Kick Duck | Normal | Bass/Keys | Dynamic, Off, 3 ms | Stereo, EXT/FOLLOW (Sidechain 1→B1) | 외부 Sidechain 1 kick key의 90 Hz 처리 범위를 따라 해당 밴드만 선행 ducking |
| 11 | MOTION / Rhythmic Pump | Complex | All | Dynamic, Off, 0 ms | Stereo, 3-band EXT/FOLLOW (Sidechain 1–3→B1–B3) | 세 외부 key를 각 처리 밴드에 고정 매핑해 서로 다른 깊이의 pump 생성 |
| 12 | MOTION / Transient Lift | Complex | Drums/Mix | Dynamic, 4X, 1 ms | Low/Mid Mid, High Side | positive Range + Compress로 threshold 위의 중앙 punch와 측면 high transient를 upward 강조 |
| 13 | MASTER / Transparent Master | Complex | Master | Linear, 4X, 5 ms | Low/Low-mid Mid, High Side | 작은 Range와 긴 release로 전대역 균형을 투명하게 안정화 |
| 14 | MASTER / Loudness Guard | Complex | Master | Dynamic, 8X, 10 ms | Low/Low-mid Mid | 고역의 1 ms attack과 8X 경로로 순간 peak를 완화하되 limiter 대체를 의도하지 않음 |
| 15 | MASTER / De-Harsh Master | Complex | Master | Linear, 8X, 8 ms | Low-mid Mid, High Side | 2.2–12 kHz의 서로 다른 공진·거친 고역을 작은 Range로 세분화해 억제 |
| 16 | MASTER / Low End Anchor | Normal | Master | Linear, 4X, 6 ms | Low/Low-mid Mid | 48–250 Hz만 느리게 제어하고 상위 대역은 처리하지 않아 저역 흔들림을 고정 |
| 17 | MASTER / Stereo Focus | Complex | Master | Linear, 4X, 5 ms | Low/High Side, Low-mid Mid | low-side를 정리하고 상단 side 움직임을 제한해 스테레오 중심을 안정화 |
| 18 | SOURCE / Guitar Smooth | Complex | Guitar | Dynamic, 2X, 2 ms | Low/Low-mid Mid, High Side | body를 보존하면서 2.5 kHz pick과 5.5–10 kHz fizz를 분리 제어 |
| 19 | SOURCE / Keys Space | Complex | Keys | Dynamic, 4X, 1 ms | Low Mid, Presence Mid, Air Side | 저역을 정돈하고 positive Range + Expand로 threshold 아래 presence와 side air detail을 복원 |
| 20 | BUS / Parallel Drum Punch | Complex | Drum Bus | Dynamic, 8X, 1.5 ms | Low Mid, High Side | Mix 65% parallel 경로에서 low-mid는 감쇄하고 positive-Range Compress band는 threshold 위 low/high transient를 upward 강조 |

## 기능 커버리지

- Processing: Dynamic / Linear
- Oversampling: Off / 2X / 4X / 8X
- Lookahead: 0 / 1 / 1.5 / 2 / 3 / 5 / 6 / 8 / 10 ms
- Channel: Stereo / Mid / Side
- Detector source: INT / EXT; `Sidechain 1→Band1`, `Sidechain 2→Band2`, `Sidechain 3→Band3`, `Sidechain 4→Band4` 고정 mapping, mono 기본·stereo `(L+R)/2` downmix
- Detector scope: Factory는 FOLLOW/BAND만 사용하며 detector 주파수·폭은 처리 밴드와 동기화
- Dynamics: Compress-above와 Expand-below active side, negative-Range 감쇄와 positive-Range 증폭. ID 12와 ID 20의 positive bands는 explicit Compress, ID 19의 positive bands는 Expand다.
- 사용 범위: All, Vocal, Bass, Drums, Guitar, Keys, Mix Bus, Master, external-key Motion

## QA 계약

- 자동 검증: Init=defaults, 20개 ID·이름·그룹 스냅샷, 78-value v4 유효성, Range `-48/+12 dB` endpoint, exact v3/v2/v1 migration과 legacy `+24→+12` clamp, 모든 Factory의 FOLLOW/BAND 및 detector/processing 좌표 일치, pairwise distinct state, signed Range/active-side 기능 커버리지, ID 12·20 positive band의 explicit Compress와 ID 19 positive band의 Expand, Audition-Off, Factory recall, selection match, one-step Undo/Redo, 사용자 preset round-trip, `Sidechain N→Band N` isolation과 동시 ducking, stereo `(L+R)/2` downmix와 selected-bus fallback을 확인한다. 이번 development candidate에서는 signed Range 의미 수정에 맞춰 ID 12의 mode 값과 ID 20 positive band mode 값을 의도적으로 갱신했다.
- 오디오 스모크: 각 Factory preset을 stereo main + 네 mono-default external bus(필요 시 일부 미연결, stereo downmix fixture), 1/3/63/65/256/257/513 sample block에서 처리해 finite output·안전 peak·mapped-band-only response를 확인한다.
- 청감 기준 소스: `DRM_01_loop.wav`, `VOX_01_female.wav`, `KEY_01_pad.wav`.
- 공개 배포 전 남은 수동 gate: 위 세 소스의 level-matched preset/bypass A/B, 실제 production DAW의 네 mono sidechain Send/Aux 연결·recall·PDC·offline bounce, 프리셋 간 지각 음량·headroom 확인.
