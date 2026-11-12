# TB XYZ Panner 1.4.0 Windows release QA

Date: 2026-10-08 (Asia/Seoul). User approved version 1.4.0 and catalog price 2.

Result: CONDITIONAL PASS. Automated correctness, host-contract and local packaging/install checks passed. Manual listening and GUI interaction are not claimed.

## Scope and source

- Source: E:\Plugin Projects\TB_XYZPanner, SVN working copy r86 files plus local changes; not committed to SVN by this release task.
- Distribution worktree: E:\Plugin Projects\TB_Audio-Plugins-xyz-release-20261008, based on Git 8d0ec1402b5b70975b1eaf03d06f5aa70191cb4c.
- Windows x64 VST3 only. Public Git repository is binaries-only; private source, test harness and build files are not uploaded.
- Catalog ID tb_xyz_panner, plug-in name TB_XYZPanner, installer AppId TeamImpulseImpact.TBPlugins.tb_xyz_panner unchanged.
- Processor CID ABCDEF019182FAEB4D616E755A6B387A; Controller CID ABCDEF011234ABCD4D616E755A6B387A.
- Eight source parameters plus host Bypass unchanged; choices, defaults and state/preset formats retained. macOS and other catalog products unchanged.

## Executed checks

1. Set version in .jucer and re-save with C:\JUCE\Projucer.exe. No manual generated-file edits.
2. Build Release x64 SharedCode, Standalone, VST3 and VST3 Manifest Helper with VS2022 AMD64 MSBuild.
3. Detected stale 1.3.0 manifest helper; rebuilt helper and reran the generated VST3 PostBuildEvent target. Manifest and both classes then report 1.4.0. DLL file/product version also 1.4.0.
4. Tests/Run-SpatialRegression.ps1: 128 passed, 0 failed. Tests/out/last-run.log records directions, original HRIR waveforms, bus mapping, stereo source retention, state, reset and automation at 44.1/48/96/192 kHz.
5. HostContractProbe on 1.4.0 and publicly distributed 1.3.0's exact binary: all JSON fields match except plugin.version. Nine metadata parameters, 22 Set/Get cases and nine fresh-instance state comparisons passed. No host profile present, unchanged.
6. Standalone product version 1.4.0; hidden startup survived six seconds and created a main window, then closed. This is not GUI interaction or listening validation.
7. Preserved previous installation in output\release-qa\TB_XYZPanner-1.4.0-20261008\previous-install and verified its previous binary hash 773CE4B84BAE9AFA9999EAE66F4F1BCE70DB2849C36BCA7DDFAC306A31A3DD27.
8. Built Inno Setup installer with the previously published XYZ AppId; silent actual install exited 0 and installation log reports success, no restart.
9. All three staged/installed bundle files have matching hashes. ZIP has exactly one TB_XYZPanner.vst3 bundle with those three files and no source/PDB/cache. Fresh extraction has identical files and a successful host scan/state roundtrip.
10. Official KEMAR attribution is copied automatically by VST3-only .jucer post-build settings. Source/installed/bundled notice SHA256 matches. Existing Hub/portfolio logos match the canonical 256px PNG; no image change required.

## Artifacts

QA root: E:\Plugin Projects\output\release-qa\TB_XYZPanner-1.4.0-20261008

Build bundle: Builds\VisualStudio2022\x64\Release\VST3\TB_XYZPanner.vst3

Installed bundle: C:\Program Files\Common Files\VST3\TB_Plugins\TB_XYZPanner.vst3

- Module SHA256: 9594DF0D1B4D7C2B4555B6EC503473A310B2A502DA1392C1598E89DB65DB2806.
- Manifest SHA256: 1FD55CE435D3C16F11ADBE3E561DDEAA985E78ADE1B0EDF740CAF10255203A15.
- KEMAR notice SHA256: 739E1142F8B336D673AA089BC89F1BA272DFC0D6B48FAB66F21511B4F7CEA5BC.
- Hub ZIP: 3,286,751 bytes; SHA256 70d69e42e6789cfd1c8e5f1556a7ffaced01885fe7973811b4b4ef25d615f172.
- Installer: 4,294,447 bytes; SHA256 a761ce20f455d9bbb4e286b0bf2a4869d4a39b99bf03bcf2ae0a51971dd5d38f.

## Performance and unverified gates

Final harness processing-time/audio-duration ratios, including test signal generation: 48kHz steady 2.58%, moving 4.65%; 192kHz steady 30.02%, moving 65.49%. These are one-run observations, not DAW CPU percentages or multi-instance guarantees. 192kHz measurements are not a performance pass gate.

Manual DAW listening, GUI interactions, multi-instance/long-duration playback, full audio-callback allocation profiling and code signing are NOT RUN. The computer-use skill's required node_repl/runtime was unavailable; no alternate UI automation was used. Pre-existing JUCE code-page warnings were observed; no dependency files were changed.

Known limitations: generic non-personalized KEMAR, elevation -40..+40 degrees, no height-speaker output, no instrument separation, existing ordinary STEREO antiphase hard-pan behavior. The improved binaural path is HEADPHONES and automatic multichannel-to-stereo rendering.

## Publication

Published stable Windows release: https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-xyz-panner-windows-v1.4.0

Distribution commit: 5888b40500ce91804523be4e551db6b6fb9c5898. PR #80 merged to main as 9cb877ded2c7fe0ef683de276671b833ae83b300. Local distribution worktree clean. No private source uploaded and no SVN source commit performed.

Release installer, ZIP and SHA256SUMS were redownloaded; all three hashes and sizes match. Raw main catalog, Hub ZIP and logo were redownloaded; version 1.4.0, price 2, both Windows asset metadata and icon hash match. Fresh remote ZIP extraction has the same three bundle-file hashes and identical live host contract. Independent comparison confirms all other 34 catalog products and XYZ macOS assets unchanged.

Real TB Hub 1.3.18 CLI test reached Downloading, Verifying, Extracting, Verifying, then Installing. Its Windows administrator request was canceled and the log says FAILED; despite Hub process exit code 0, this is NOT an installation PASS. The local validation command intentionally rejected that result. Hub-installed-state confirmation remains unverified; no administrator/security bypass was attempted. The successful installer-installed 1.4.0 bundle remains unchanged, with all three final hashes rechecked.

Evidence: QA-root release-redownload, main-redownload, remote-audit-host-contract.json, installer-smoke.log, hub-install.log, SHA256SUMS.txt and GITHUB-RELEASE-NOTES.md. The GUI and listening limits above remain applicable.
