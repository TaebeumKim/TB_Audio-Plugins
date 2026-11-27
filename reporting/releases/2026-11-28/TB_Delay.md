# TB Delay 1.2.2

## [1.2.2] - 2026-11-28

BBD·Digital 캐릭터에서 FEEDBACK이 높으면 echo가 마지막 한 단계에 걸려 끝나지 않던 문제를 고쳤습니다. 입력이 들어오는 동안의 음색은 1.2.1과 같습니다.

- BBD·Digital의 비트 축소는 새 입력이 들어오는 동안 지금처럼 가장 가까운 단계로 반올림하고, 입력이 반 단계 아래로 내려가면 0 쪽으로 버립니다. 그래서 FEEDBACK이 높아도 echo가 −36 ~ −90 dBFS의 마지막 단계에 걸려 남지 않습니다(DISTORT·WEAR 0에서도 일어났습니다).
- 음색: 1 kHz −20/−40 dBFS에서 BBD·Digital × WEAR 0/100 × DISTORT 0/100 16개 측정이 1.2.1과 같습니다.
- 들리는 차이: 일부 BBD·Digital 프리셋의 꼬리가 실제로 짧아집니다(예: Dark Bucket Trail 5.9 → 3.7 s). 끝에 걸려 남던 소리가 없어진 만큼입니다.
- 꼬리 보고: 프리셋 120개·무작위 944개에서 보고값보다 길게 울리는 설정이 없습니다. 단 120 s보다 오래 울리는 무작위 설정 24개는 보고 상한 120 s에 걸립니다(1.2.1과 같음).
- CID·40개 파라미터 ID·상태·프리셋 계약은 1.2.1과 같습니다. 가격과 macOS 다운로드는 유지합니다.
- Delay 테스트 `pass=40 fail=0 skip=0`, Host 계약(1.2.1과 비교), 네이티브 UI attach/paint/remove, 지연 0·bypass 복귀 잔류 없음을 확인했습니다.
- 실제 DAW 청취·설치 실행은 미실행이며 설치 파일은 서명되지 않았습니다.

## Downloads

- ZIP: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-delay-windows-v1.2.2/TB_Delay.zip
- Installer: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/download/tb-delay-windows-v1.2.2/TB-Delay-1.2.2-Windows-x64-VST3-Setup.exe

ZIP SHA-256: `f065a01ac04e72ba6dd33257c7abba27965f12bf770e8f7f9334f4cd42c41942` (2633942 bytes)

Installer SHA-256: `df7f4ada7c89b842a9c23abb38456622c78351af67fa7bde161b58534f3cb5d3` (4005989 bytes)
