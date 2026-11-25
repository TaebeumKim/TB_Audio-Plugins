# TB Parallel Reverb 3.3.1

## [3.3.1] - 2026-11-26

호스트 bypass 복귀 때 잔향이 다시 튀어나오지 않도록 Glue·Bright·Space 엔진을 블록 단위로 안전하게 비우고, 활성 엔진·pre-delay·decay·shimmer를 반영해 tail 보고를 보정했습니다.

- 기존 43개 파라미터 ID, CID, 상태·프리셋 계약을 유지했습니다. 가격은 기존 4를 유지합니다.
- HostTimelineTests, ReverbQualityHarness, Host contract, 네이티브 UI 검사를 통과했습니다.
- 192 kHz/64 샘플 블록에서도 엔진 초기화를 분할해 실시간 블록 예산을 지킵니다.
- 실제 DAW 청취·설치 실행·서명 검증은 미실행입니다.

## Downloads

- ZIP: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-parallel-reverb-windows-v3.3.1/TB_ParallelReverb.zip
- Installer: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-parallel-reverb-windows-v3.3.1/TB-Parallel-Reverb-3.3.1-Windows-x64-VST3-Setup.exe

ZIP SHA-256: `04a31ee10935aca2b5d3068a4b6b6cfa24ef8133ddf193b0c96e958cf6910f9a` (3062966 bytes)

Installer SHA-256: `0d2edab8bc6ffdcad16d6746f3cca5517e85fec0dbcbf938f61a01b0394b74af` (4321735 bytes)
