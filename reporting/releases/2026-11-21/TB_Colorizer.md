# TB Colorizer 3.0.0

## [3.0.0] - 2026-11-21

- Windows에서 공통 파트의 정적 GPU 이미지 캐시 때문에 플러그인 종료가 정지하는 문제를 재현했습니다. 캐시를 소프트웨어 이미지로 보관하도록 수정해 그림·배치·DSP는 유지하며 GPU 리소스 수명 문제를 없앱니다.

실제 응답에 기반한 Hero와 840×400 화면으로 개편하고 고정된 가상 소스·잘못된 배음 표시를 제거했습니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.

## Automated release checks

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_Colorizer-dsp: PASS.
- TB_Colorizer-native-ui: PASS.
- TB_Colorizer-ui: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-colorizer-windows-v3.0.0/TB_Colorizer.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-colorizer-windows-v3.0.0/TB-Colorizer-3.0.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `d9ae9f737f90f96e98a439f8020242389cd94650ebd092e71e3f9c85a07201c1` (3298924 bytes).
Installer SHA-256: `b2985ca05e4a66925ace47a338de5342025171175941ab5082e3ac724272bec3` (4399922 bytes).
