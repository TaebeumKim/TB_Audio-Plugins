# TB Vocoder (Beta) 3.0.0

## [3.0.0] - 2026-11-23

MORPH 시작 끊김·RING 덮어쓰기·입력 레벨 의존 DRIVE를 고치고 MORPH·DRIVE를 모든 TYPE에서 쓰게 했으며, 프리셋 20개를 추가하고 화면을 TB 공통 시각 언어로 바꿨습니다.

- Windows x64 VST3 ZIP와 설치 파일을 갱신했습니다. 카탈로그 가격과 기존 macOS 다운로드는 유지합니다.
- 기존 CID·파라미터 ID·상태·프리셋 계약을 유지합니다.
- Release 빌드, 제품 하니스, Host 계약(이전 공개본과 비교)을 통과했습니다.
- 실제 DAW 청취·수동 GUI/DPI 확인·설치 실행·서명 검증은 미실행입니다.

## Known boundary

MORPH·DRIVE가 모든 TYPE에서 동작하고 DRIVE가 입력 레벨과 무관해져 기존 세션의 소리가 달라질 수 있습니다. 3.0.0에서 파라미터 5개(Drive·Morph·MIDI Carrier·Sharpness·Synthesis Type)가 추가되고 표시 이름 4개가 바뀌었습니다(Warp→Stretch, Floor→Gate, Whiten→Match, Carrier Count→Voices). 파라미터 ID는 그대로라 기존 자동화는 유지됩니다. 팩토리 프리셋은 120개에서 140개가 되었습니다. MIDI CC 프록시 파라미터가 많아 Host probe 실행에 약 6분이 걸리지만 모든 검사를 통과했습니다.

## Downloads

- ZIP: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-vocoder-windows-v3.0.0/TB_Vocoder.zip
- Installer: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-vocoder-windows-v3.0.0/TB-Vocoder-Beta-3.0.0-Windows-x64-VST3-Setup.exe

ZIP SHA-256: `2356eebc79b1d3d53cdc4fa1bc054f7b26462e81d1f98eb7602aace58c0b8d28` (3022726 bytes)

Installer SHA-256: `20e7979a8c361ccb39f28a0523f79a5f33706fe0aa7177895f4130f379f9e99f` (4289332 bytes)
