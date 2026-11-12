# TB_Shimmer 업데이트 기록

카탈로그 ID: `tb_shimmer`  
현재 릴리스: `1.0.0`  
개발 후보: `2.0.0`  
공개 상태: **Windows x64 릴리스**

## [2.0.0] - 개발 중 (Unreleased)

이 빌드가 새 기준이다. 1.0.0의 계약 중 충돌하는 항목은 이 빌드에 맞춰 갱신했다.

### Tail 감쇠 물리 수정

- Late FDN의 루프 보간을 선형에서 1차 all-pass로 교체했다. 진폭 응답이 모든 주파수에서 정확히 unity가 되어
  tail의 고역 감쇠를 Tone만이 결정한다. 실측 HF/LF 유지율이 `9.31e-06`에서 `2.54e-02`로 개선됐다.
- `diffuseSoftBound`에 0.9 선형 임계값을 도입했다. knee 아래에서는 감쇠가 레벨과 무관하다. 유한성 상한은 유지된다.
- Late loop gain을 목표 delay가 아니라 현재 smoothing 중인 delay에서 계산한다. Size 모프 중에도 각 line이
  실제 사용 중인 길이에 맞는 T60을 갖는다. 정착 후에는 `pow` 호출이 없다.

### 오디오레이트 그래뉼레이션

- `Density` 범위를 2–24 gr/s에서 2–2000 gr/s로 확장하고 skew 중심을 24 gr/s에 두었다. 노브 하단 절반이
  기존 cloud 범위, 상단 절반이 새 synthesis 범위다.
- `Size` 범위를 40–500 ms에서 1–500 ms로 확장하고 표시 단위를 소수점 1자리로 바꿨다. skew 중심은 60 ms다.
- Grain voice pool을 32에서 64로 늘리고, 여유 voice가 없으면 envelope 끝에 가장 가까운 grain을 steal한다.
  spawn 그리드가 규칙적으로 유지되어 오디오레이트에서 grain rate가 스펙트럼 구조를 만든다.

### 연속 Pitch

- `Pitch`를 stepped `AudioParameterInt`에서 1 cent 해상도의 연속 `AudioParameterFloat`로 바꿨다.
  범위와 기본값(-12–+24 st, +12 st)은 그대로다. Grain 커널은 `ceil(pitch)`로 bin을 고르므로
  분수 semitone에서도 보수적인 anti-alias cutoff를 유지한다. +7.5 st 실측 342.0 Hz(기대 339.3 Hz).

### 의도한 계약 변경

- `Density`, `Size`의 정규화 매핑이 바뀌었고 `Pitch`가 stepped에서 continuous가 되었다.
  저장된 상태와 User 프리셋은 평문 값을 쓰므로 1.0.0 데이터가 그대로 복원된다(state schema 2 유지).
  다만 1.0.0에서 기록한 **호스트 오토메이션 레인은 다른 정규화 그리드로 재생된다.**
  2.0.0 빌드를 기준으로 삼기로 했으므로 이 변경은 수용하고 이전 문서·게이트를 갱신했다.

### Freeze

- `freeze` 파라미터(Off/On)를 추가했다. 켜면 history 기록과 write head 전진을 멈추므로 grain pool이
  포착된 구간을 계속 재생한다. Dry 경로와 diffuse tail은 영향을 받지 않는다.
- State schema / preset format을 **3**으로 올렸다. v1 → v3는 `Level Var 0 / Position Rand=기존 Scatter /
  Pitch Spray=기존 Scatter×0.0035 / Freeze off`, v2 → v3는 `Freeze off`로 이관한다.
- 에디터를 7+6에서 **7+7** 대칭 배치로 바꾸고 FREEZE를 14번째 셀에 넣었다. 값은 OFF/ON으로 표시한다.
- Factory preset 100개는 집합 초기화 규칙에 따라 Freeze off를 그대로 이어받으며 나머지 값은 변경 없다.

### Sparkle / Shatter (반짝임 딜레이 레이어)

- `sparkle`(0–100 %, 기본 0)과 `shatter`(0–100 %, 기본 0) 파라미터를 `freeze` 뒤에 append했다. 공개 파라미터는 17개다.
- Sparkle은 grain cloud와 별개인 **shard 레이어**다. 입력 history를 +12/+19/+24/+31/+36 st(배음 2·3·4·6·8)로
  올려 읽는 10–75 ms짜리 타악성 조각(1.5–3 ms smoothstep attack + 지수 감쇠)을 최대 32 voice로 뿌린다. 700 Hz 1-pole HPF를 거쳐
  Tone의 영향을 받지 않고 밝게 남는다.
- Shard 버스는 `Time` 길이의 **핑퐁 에코**를 지난다. L에서 난 조각이 Time 뒤 R에서, 다시 Time 뒤 L에서 돌아온다.
  반복량은 `Feedback`×0.85, 반복마다 `Tone`이 damping한다. Feedback 0에서도 1회 반복(0.6)은 남는다.
  에코까지 합친 신호의 45%를 late FDN에 보내 반짝이는 잔향을 만든다. Granular history에는 재주입하지 않는다.
- `Shatter`는 조각이 흩어지는 방식을 바꾼다. 0%에서는 초당 약 10–48개의 고른 drizzle(반짝이 가루)이고, 올릴수록
  dry 입력의 attack(빠른/느린 envelope 비 2.2배)을 감지해 그 순간을 다시 읽는 burst를 터뜨린다. burst는
  120–500 ms에 걸쳐 지수적으로 성기어지고, 조각은 더 짧아지며(바닥 10 ms) 높은 배음·넓은 pan·수 cent의 detune으로 치우친다.
- 조각의 pan은 세제곱근 분포로 좌우 가장자리 쪽에 몰리게 뽑는다(범위 ±92–100%, 가운데 절반에 떨어지는 조각은 1/8).
  Width 100%에서 레이어 power의 31%가 side(side/mid −3.5 dB)이고, Width 170% 데모에서는 side/mid가 가루 +0.6 dB,
  부서짐 +1.4 dB, L/R 상관 −0.08 / −0.16이다. Width 0%에서는 L/R 차이 0으로 완전히 모노로 접힌다.
  Width를 150% 이상으로 올리면 side가 mid보다 커지므로 모노 합산 시 레이어가 그만큼 작아진다.
- 2026-11-12 조정: 최대 gain 2.2→3.2, drizzle 26→48개/s, burst 220→380개/s, 겹침 power 허용 2→4,
  배음 가중을 상위(+24 st 이상)로 이동, pan을 가장자리 쏠림으로 변경. 같은 데모에서 레이어 레벨이
  가루 −13.7→−7.2 dB, 부서짐 −6.4→−0.3 dB(Sparkle off 렌더 대비), side/mid가 가루 −7.2→+0.6 dB,
  부서짐 −2.4→+1.4 dB로 바뀌었다.
- 2026-11-12 조정 2 (Shatter의 디스토션/클리핑 같은 자작함 제거): 임시 계측으로 원인을 확인했다. Shatter 85% 데모에서
  에코 리미터 초과 0.00%, 겹침 게인 덕킹 0%로 **클리핑은 아니었고**, 조각 자체가 평균 9.5 ms(23%가 6 ms 미만)에
  attack 0.25–0.4 ms라 클릭에 가까웠으며 ±0.34 st detune이 거친 맥놀이를 더했다. 수정: 조각 길이 바닥 10 ms와
  완만한 단축(48−39·S → 48−22·S ms, 평균 19.6 ms·6 ms 미만 0%), attack 1.5–3 ms smoothstep, detune ±0.04–0.12 st,
  상위 배음 쏠림 절반, 겹침 정규화를 순간값 대신 1 ms/80 ms follower로 변경, 에코 라인 soft-bound 여유 2배.
  Shatter 연동 level trim(+1.2 → −2.1 dB)으로 레이어 레벨을 조정 전과 같게 맞췄다(가루 −7.2 dB, 부서짐 −0.3 dB,
  side/mid +0.6 / +1.3 dB). burst 밀도는 그대로다.
- Sparkle 0%에서는 레이어 전체를 건너뛰므로 출력이 이전 엔진과 **sample-identical**이다(Shatter 값과 무관).
  Sparkle을 0으로 되돌리면 에코가 다 울린 뒤 레이어가 스스로 꺼진다.
- State schema / preset format을 **4**로 올렸다. v1·v2·v3 데이터는 `Sparkle 0 / Shatter 0`으로 이관한다.
- Factory 100개 중 `SPARKLE` 그룹 8개(index 28–35)에만 Sparkle/Shatter 값을 넣었다. 기존 15개 값·ID·이름·순서는
  그대로이며 나머지 92개는 Sparkle 0이라 소리가 바뀌지 않는다.
- 에디터를 7+7에서 **8+8**로 바꿨다(셀 폭 96→88 px). `Reverse`를 1행 끝으로 옮기고 2행을
  `Pitch / Feedback / Tone / Sparkle / Shatter / Width / Mix / Freeze`로 배치했다. Grain Halo에는 엔진이 실제로
  생성한 shard 수만큼 별 모양 파편이 코어에서 튀어나와 떨어진다(오디오가 없으면 느린 preview drizzle).
- Pitch kernel bin을 37→49개로 늘려 +36 st까지 덮는다. 기존 bin 0–36의 계수는 동일하다.
- 192 kHz DSP storage는 sparkle echo 2줄(약 1.84 MiB)과 kernel 확장(약 0.09 MiB)만큼 늘어 약 14.8 MiB(계산값)다.
- **미검증**: 청감 튜닝(레벨·밀도·조각 길이)은 합성 신호 수치로만 맞췄다. 실제 악기/DAW 청취와 VST3 Host Contract
  Probe(17 파라미터)는 아직 실행하지 않았다. +31/+36 st 조각은 8-tap kernel의 한계로 고역 입력에서 aliasing이 남는다.

### 성능

- `readHistory`의 8-tap 합성곱이 tap마다 `wrapIndex` while 루프를 돌던 것을 wrap 1회 계산 + 연속/경계
  분기로 바꿨다. 출력은 bit-identical이며 결정론 테스트가 이를 확인한다.
- 최악 조건(192 kHz / 32 samples / 64 voice 포화) 콜백 비용 측정을 harness에 추가했다.
  **아직 신뢰할 수 있는 수치가 아니다**: 데드라인 166.7 µs 대비 median이 실행마다 51–136 µs(31–82%)로
  요동한다. 이 harness는 realtime thread가 아니라 머신 부하가 그대로 섞인다. 유휴 머신과 실제 호스트에서
  재측정해야 하며, 프로젝트 예산(p99 20%)을 만족하는지는 아직 확인되지 않았다.

### 알려진 제약

- 짧은 grain(40 ms 이하)에서 randomisation을 모두 0으로 두면 인접 grain이 위상 상쇄를 일으켜
  pitch shift가 사라진다. cloud 범위(160 ms)에서는 발생하지 않는다. 회귀 테스트 2개로 두 거동을 고정했다.

## [1.0.0] - 2026-08-03 (Released)

### 구현 범위

- 고정 크기 grain pool과 원형 history buffer를 사용하는 stereo granular shimmer 엔진.
- 채널별 4-stage Schroeder all-pass early diffuser와 고정 메모리 8-line normalized Hadamard late FDN을 연결한 고유 diffuse swarm tail.
- 기존 `Time`, `Size`, `Density`, `Scatter`, `Pitch`, `Reverse`, `Feedback`, `Tone`, `Width`, `Mix`, `Bypass` 뒤에 `Level Var`, `Position Rand`, `Pitch Spray`를 3→1→2 순서로 append한 14개 자동화 파라미터.
- octave-up grain과 필터링된 feedback을 결합한 crystalline/shimmer 계열 사운드.
- Early diffuser는 L `3.1/5.3/9.1/15.7 ms`, R `3.7/6.2/10.7/16.9 ms`의 목표 지연을 sample rate별 nearest-prime sample 수로 할당하고 `0.57–0.72` all-pass coefficient를 사용한다. `Size`와 `Density`로 blend를 `24–58%`에서 정하고 80 ms smoothing과 raw↔diffused energy normalisation을 적용한다.
- Energy-normalised raw↔early-diffused blend는 direct wet과 late FDN input에 함께 들어가지만 granular feedback history에는 재주입하지 않는다. `Size`는 late delay scale, `Density`는 30–80% late-tail 가중치, `Scatter`는 grain size/pan과 tail motion, `Feedback`은 독립 late T60(최대 45초), `Tone`은 loop damping에 연동한다.
- Per-grain `Level Var`(0–12 dB downward variation), `Position Rand`(0–100% bipolar history offset), continuous `Pitch Spray`(0–12 st bipolar pitch offset)는 서로 다른 random draw를 사용한다. Density scheduler는 고정 sample interval로 동작한다.
- Hann×level weight의 `S1`/`S2`에서 `Gc=1/max(1,S1)`, `Gr=1/sqrt(max(1,S2))`를 구하고 `min(sqrt(Gc·Gr), sqrt(2)·Gc)`를 적용하는 hybrid OLA normalisation을 추가했다.
- Standalone 및 VST3 대상, Factory/User preset과 versioned state 계약.
- Public ID의 기존 11개 순서를 보존한 뒤 `level_var`(0–12 dB, default 0), `position_rand`(0–100%, default 32), `pitch_spray`(0–12 st continuous, default 0.112)를 append했다. State schema/preset format은 2이며 유효한 v1은 `Level=0`, `Position=old Scatter`, `Spray=old Scatter×0.0035`로 이관하고 malformed/future 데이터는 원자적으로 거부한다.
- Factory bank는 정확히 100개다. `START`, `SHIMMER`, `CLOUD`, `REVERSE`의 최초 12개는 index 0–11에서 stable ID·이름·그룹·14개 값·순서를 byte-for-value로 유지한다. 신규 88개는 `UTILITY`, `DIFFUSE`, `SPARKLE`, `RANDOM`, `PULSE`, `VOCAL`, `GUITAR`, `KEYS`, `PADS`, `DARK`, `EXPERIMENTAL`의 연속 그룹에 각 8개씩 append한다. `PULSE`는 free-time이며 host tempo-sync를 주장하지 않는다.
- Factory 확장 뒤에도 state schema/preset format은 2/2다. VST3 Program은 `valueCount=100`/`stepCount=99`로 바뀌므로 기존 12-program 버전과 normalized Program automation grid가 달라지는 호환 경계를 명시하며, parameter state와 User preset 호환은 유지한다.
- 업데이트된 HTML 최상단 `2a`/variant `d`를 반영한 760×430 단일 패널 에디터. Grain Field·flat matte knob·방사형 원본 로고/halo는 유지하고, 13개 컨트롤을 1행 `Time / Size / Density / Scatter / Level Var / Position Rand / Pitch Spray`, 2행 `Pitch / Reverse / Feedback / Tone / Width / Mix`로 항상 함께 표시한다.
- 결정론 DSP/상태/preset 테스트와 단일 UI PNG·기본 wet WAV QA 산출물 생성 경로. Early-only `69–125 ms` 구간(최초 late FDN 도착 약 `131 ms` 이전), feedback 0 one-pass decay, stereo/mono/reset과 단일 패널 focus/layout 검증을 포함한다.
- SpaceBlender는 Soundtoys [공식 제품 페이지](https://www.soundtoys.com/product/spaceBlender/)와 [공식 사용자 매뉴얼](https://www.soundtoys.com/wp-content/uploads/SpaceBlender-Manual.pdf)의 공개 청감 설명만 참고했다. 알고리즘, 컨트롤, UI, preset과 비공개 구현은 복제하거나 추정하지 않았다.

### 검증 상태

- 소스 및 프로젝트 스캐폴딩: 완료
- Early diffuser + late FDN diffuse swarm 구현, Factory 100개 뱅크와 최초 12개 호환값 보존: 완료
- Projucer resave: 메인·테스트 프로젝트 PASS
- Debug/Release 빌드: x64 Standalone, VST3, test harness PASS; VST3 manifest 생성 확인
- 자동 테스트: Debug/Release 모두 `36/36 PASS`. Early-only `69–125 ms`, feedback 0 one-pass decay, 단일 패널 focus/layout과 Factory 100개 전수 무결성·적용·고인덱스 state round-trip coverage를 포함하며 상세 수치는 `RELEASE_QA.md`에 기록했다.
- VST3 Host Contract Probe: public parameter 14개, `valueCount=100`/`stepCount=99`인 JUCE Program을 합친 VST3 15개 항목과 bypass 1개를 통과했다. Debug/Release snapshot은 동일하며 hash는 `RELEASE_QA.md`에 기록했다.
- UI/audio QA: 13개 컨트롤이 모두 보이는 하나의 760×430 canonical snapshot과 early+late diffuse tail을 포함한 48 kHz/stereo/24-bit WAV를 재생성하고 검증했다.
- Release candidate ZIP을 재패키징하고 내부 VST3/Standalone hash와 5개 entry 구조를 검증했다. 최종 SHA-256은 `RELEASE_QA.md`에 기록했다.
- 배포 저장소 PR [#29](https://github.com/TaebeumKim/TB_Audio-Plugins/pull/29)를 병합하고 `tb-shimmer-windows-v1.0.0` GitHub Release를 공개했다. 공개 asset을 다시 다운로드해 로컬 후보와 SHA-256이 같은지 확인했으며 저장소 Latest는 기존 Hub 릴리스를 유지했다.
- 실제 DAW 로드 및 청감 A/B: 미실행

자동 빌드·테스트·호스트 계약 게이트를 통과한 Windows x64 1.0.0을 공개 승인했다. 실제 DAW 2종 로드, 소스별 청감 A/B와 성능 프로파일링은 미실행 상태로 남기며 후속 검증 대상으로 관리한다.
