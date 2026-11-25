# TB Compressor 2.2.0

## [2.2.0] - 2026-11-25

보고 지연과 호스트 bypass 정렬, OUTPUT 순서, 꼬리 보고를 고치고 오버샘플링을 선형 위상으로 바꿨으며 화면을 900 × 600의 TB 공통 시각 언어로 바꿨습니다.

- 보고 지연: 1×의 1샘플 오차를 고치고, 2×/4×/8× 오버샘플링을 선형 위상 FIR로 바꿔 보고 지연이 실제 지연과 같습니다(44.1–192 kHz).
- 호스트 bypass: 보고한 지연만큼 원음을 늦춰 내보내고, 복귀할 때 이어 붙입니다.
- OUTPUT: 새츄레이터 뒤의 마지막 트림이 되어 레벨만 바꿉니다.
- 꼬리: 새츄레이터가 켜지면 0.3 s, 꺼지면 0.02 s로 보고합니다.
- CID·파라미터 ID와 메타데이터는 공개된 2.1.0과 같습니다. 가격과 macOS 다운로드는 유지합니다.
- 측정 하니스, 기존 스모크 테스트 2종, Host 계약(2.1.0과 비교), 네이티브 UI attach/paint/remove를 통과했습니다.
- 실제 DAW 청취·수동 GUI/DPI 확인·설치 실행은 미실행이며 설치 파일은 서명되지 않았습니다.

## Known boundary

- **소리가 달라질 수 있습니다:** 새츄레이터가 켜져 있고 OUTPUT이 0 dB가 아닌 세션, 그리고 2×/4×/8× 오버샘플링을 쓰는 세션.
- 오버샘플링의 지연이 늘었습니다(48 kHz: 1× 960, 2× 1009, 4× 1020, 8× 1025샘플). 호스트가 보정합니다.
- 기본 창 크기가 900 × 510에서 900 × 600으로 바뀌었습니다.

## Downloads

- ZIP: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-compressor-windows-v2.2.0/TB_Compressor.zip
- Installer: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-compressor-windows-v2.2.0/TB-Compressor-2.2.0-Windows-x64-VST3-Setup.exe

ZIP SHA-256: `d4d3df301ea7a08b4939182849e3fbfeca582fc49bbd003cb0a63c1c30d33468` (3012983 bytes)

Installer SHA-256: `03b140c47471f83a9d596fd1aba97b23d32bb8f992ddb88997c6e5c9cbbcddef` (4279830 bytes)
