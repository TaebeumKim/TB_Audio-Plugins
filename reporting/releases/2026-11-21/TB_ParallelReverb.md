# TB Parallel Reverb 3.2.0

## [3.2.0] - 2026-11-21

16라인 FDN·입력 디퓨저와 감쇠 보정을 적용하고 밀도·스테레오 폭 동작을 개선했습니다. 기존 100개를 보존하며 프리셋을 150개로 확장했습니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.
- 기존 100개 프리셋의 순서는 유지합니다. FDN·감쇠 모델 변경으로 같은 설정의 잔향 소리는 이전과 달라질 수 있습니다.

## Automated release checks

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_ParallelReverb-native-ui: PASS.
- TB_ParallelReverb-presets: PASS.
- TB_ParallelReverb-quality: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-parallel-reverb-windows-v3.2.0/TB_ParallelReverb.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-parallel-reverb-windows-v3.2.0/TB-Parallel-Reverb-3.2.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `0ff6905a4c942cfb48ca7a7ea94ff96ef8c05c716b23c4f6ad6a4d6e10c215e9` (5719957 bytes).
Installer SHA-256: `ce8b675f97b6a24cbb6478fcc71b76be272c41b076a30c5c2d48c6f44b95241a` (6870611 bytes).

## Reverb measurement boundary

The quality harness completed with finite output in all five 48 kHz reference cases. The measured 1 kHz T60 error was +3.6%, +3.9%, -0.9%, +2.0%, +5.8%. This is a measurement run, not a claim of equivalence to another reverb or that every aspirational quality target was met. The 150-preset audit is a separate pass/fail gate.
