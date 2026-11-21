# TB Filter Table 2.0.0

## [2.0.0] - 2026-11-21

- Windows에서 공통 파트의 정적 GPU 이미지 캐시 때문에 플러그인 종료가 정지하는 문제를 재현했습니다. 캐시를 소프트웨어 이미지로 보관하도록 수정해 그림·배치·DSP는 유지하며 GPU 리소스 수명 문제를 없앱니다.

한 화면으로 UI를 개편하고 미배포 상태였던 LFO 템포 싱크·Division·입력 엔벨로프 Amount/Speed·Stereo Spread를 공개합니다. 기존 11개 파라미터를 보존하며 5개를 추가하고 이전 상태를 schema 6으로 읽어 올립니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.

## Automated release checks

Final full-suite performance checks were run twice at High process priority, with no build/compression jobs running: 197/197 passed in each run. At 48 kHz, stereo, 64 samples on a Ryzen 7 260, full-spread automation p99 was 449.7/449.0 us against the unchanged 470 us gate. The second run recorded one isolated maximum callback deadline miss; Normal-priority measurements also included failed runs (523.7/474.0 us), retained locally. These measurements are not a guarantee of dropout-free 64-sample operation in every DAW or on every system. Increase the buffer when necessary.

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_FilterTable-dsp: PASS.
- TB_FilterTable-final1: PASS.
- TB_FilterTable-final2: PASS.
- TB_FilterTable-native-ui: PASS.
- TB_FilterTable-quiet1: PASS.
- TB_FilterTable-ui: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-filter-table-windows-v2.0.0/TB_FilterTable.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-filter-table-windows-v2.0.0/TB-Filter-Table-2.0.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `501cb3d0edc9fb152bca6cad3219c898155ec049dfb2939c096c8bf4aad86bf0` (3284265 bytes).
Installer SHA-256: `9f92400877a234e3e87dba757daf0bb095b4d06311852dca38b1c5dfe4f94674` (4410375 bytes).
