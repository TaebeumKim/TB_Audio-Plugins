# TB Noise Remover 3.0.0

## [3.0.0] - 2026-11-21

Basic을 복원→FastEnhancer-B→음성 분리 게이트의 직렬 처리로 개선했습니다. REDUCTION 강도와 레이턴시가 변경되므로 기존 세션은 재조정이 필요할 수 있습니다. Noise Print·Focus Band는 유지합니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.
- 주의: 같은 REDUCTION 값이 이전보다 강하게 작동합니다. 48 kHz 보고 지연은 2048→5120 샘플(약 107 ms)입니다. 44.1/48 kHz는 버퍼 128 이상, 88.2/96 kHz는 256 이상을 권장하며 32 샘플 실시간 처리는 보장하지 않습니다.

## Automated release checks

Known scope: the neural Basic separation path prioritizes speech/singing and may retain pitched background instruments. Legacy space/resonance/comb controls are not used in that separation path; the spectral fallback and Noise Print path retain their existing behavior. Noise Print is session-only and must be captured again after re-preparing/changing sample rate. RX-style offline time-frequency resynthesis is not included.

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_NoiseRemover-native-ui: PASS.
- TB_NoiseRemover-wav: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-noise-remover-windows-v3.0.0/TB_NoiseRemover.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-noise-remover-windows-v3.0.0/TB-Noise-Remover-3.0.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `29ad41dacfb7719cd06a04f1c234263c5d10ec8faa7b24fbc403b17a21c4ea75` (9888291 bytes).
Installer SHA-256: `f8462a82754f384ae2e21de8af5a55838e06fd2a36e00de0f4b1f56e801e55f3` (8519014 bytes).
