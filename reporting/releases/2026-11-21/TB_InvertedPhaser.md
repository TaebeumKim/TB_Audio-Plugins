# TB Inverted Phaser 4.0.0

## [4.0.0] - 2026-11-21

- Windows에서 공통 파트의 정적 GPU 이미지 캐시 때문에 플러그인 종료가 정지하는 문제를 재현했습니다. 캐시를 소프트웨어 이미지로 보관하도록 수정해 그림·배치·DSP는 유지하며 GPU 리소스 수명 문제를 없앱니다.

840×456 화면에서 실제 페이저 응답과 INVERT 상태를 표시하고 12개 조작부를 그룹별로 정리했습니다.

이전 공개 바이너리에는 없던 INVERT 조작부(자동화 가능한 bool 파라미터)를 이번에 공개합니다. 기존 상태에서 이 값이 없으면 이전과 같은 일반 극성(Off)을 적용합니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.

## Automated release checks

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_InvertedPhaser-dsp: PASS.
- TB_InvertedPhaser-native-ui: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-inverted-phaser-windows-v4.0.0/TB_InvertedPhaser.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-inverted-phaser-windows-v4.0.0/TB-Inverted-Phaser-4.0.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `dfe4190b11fbf85ebe7221b753c358b3213c7458a8a487a96dbc895a6cd38d5c` (2727485 bytes).
Installer SHA-256: `2697e92859d4aca7b7cf61a2f3fdbf5bb202efdc68761e0d9ae6c09094baa587` (4016396 bytes).
