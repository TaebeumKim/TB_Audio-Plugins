# TB XYZ Panner 1.5.0

## [1.5.0] - 2026-11-21

- Windows에서 공통 파트의 정적 GPU 이미지 캐시 때문에 플러그인 종료가 정지하는 문제를 재현했습니다. 캐시를 소프트웨어 이미지로 보관하도록 수정해 그림·배치·DSP는 유지하며 GPU 리소스 수명 문제를 없앱니다.

실제 근접 음장 경계와 0 기준 아크를 표시하도록 정리하고 공통 재질·서체를 적용했습니다. 공간 렌더링 DSP·배치는 유지합니다.

- 이번 배포: Windows x64 VST3, Hub ZIP 및 Windows 설치 파일. 가격은 기존 카탈로그 값을 유지합니다.
- 기존 플러그인 식별자·제품 파라미터·상태/사용자 프리셋 계약을 유지합니다. macOS 배포본은 변경하지 않습니다.
- 실제 DAW 청취·수동 GUI 조작과 설치 실행은 이번 자동 검증에 포함하지 않았습니다.

## Automated release checks

- Release x64 VST3/Standalone build and current bundle version: PASS.
- Native VST3 host load, metadata, parameter Set/Get and new-instance state round-trip: PASS.
- Existing processor/controller CIDs and product parameter metadata: preserved.
- TB_XYZPanner-editor: PASS.
- TB_XYZPanner-native-ui: PASS.
- TB_XYZPanner-spatial: PASS.
- ZIP structure, runtime/licence payload, SHA-256 and exact byte size: PASS.
- Installer compiled and version metadata checked; signature: NotSigned.
- NOT RUN: installer execution, manual interactive GUI testing, DAW listening and macOS builds.

## Downloads

- [Windows x64 VST3 ZIP](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-xyz-panner-windows-v1.5.0/TB_XYZPanner.zip)
- [Windows x64 installer](https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-xyz-panner-windows-v1.5.0/TB-XYZ-Panner-1.5.0-Windows-x64-VST3-Setup.exe)

ZIP SHA-256: `57202dd18385ca7d29c8b3a14e484aa098fabeec423571adf58f2602cad82000` (3485056 bytes).
Installer SHA-256: `b28ab8ec9e4e3ea6976c19d4956ab2bcf370fd540e00c56f457f8c50f0ceb72e` (4446606 bytes).
