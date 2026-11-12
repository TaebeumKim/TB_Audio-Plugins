# TB XYZ Panner 1.4.1 Windows release QA

Date: 2026-10-15, Asia/Seoul. The user requested Git publication of the verified
fixes. Patch version 1.4.1 was selected for this maintenance update and the
previously approved catalog price 2 is retained.

## Scope

- Source: `E:\Plugin Projects\TB_XYZPanner`, existing SVN working copy plus local
  changes. No source commit to SVN and no private source uploaded to GitHub.
- Distribution worktree: `E:\Plugin Projects\TB_Audio-Plugins-xyz-release-20261008`,
  branch `release/tb-xyz-panner-1.4.1-20261015`, base
  `9cb877ded2c7fe0ef683de276671b833ae83b300`.
- Windows x64 VST3. Catalog ID `tb_xyz_panner`, installer AppId
  `TeamImpulseImpact.TBPlugins.tb_xyz_panner`, price 2.
- Processor CID `ABCDEF019182FAEB4D616E755A6B387A`; controller CID
  `ABCDEF011234ABCD4D616E755A6B387A`.
- Eight source parameters plus host Bypass. No state/schema/parameter changes.
  Existing macOS 1.0.3 downloads, logos and other 34 catalog products preserved.

## Executed release validation

1. Updated version in `.jucer`, regenerated via Projucer and rebuilt Release x64
   SharedCode, Manifest Helper, VST3 and Standalone using AMD64 MSBuild.
   Generated files and moduleinfo were not manually edited. DLL, manifest, both
   class versions and Standalone report 1.4.1.
2. Production-linked DSP harness: **164/164 PASS**. Editor harness: **48/48 PASS**.
   Includes low-rate stability, mode re-entry, filter automation, both-plane
   painted-marker coordinates, pointer/value agreement, layout and Undo/Redo.
   Default/extreme/150-percent offscreen snapshots generated; default inspected.
   See `VALIDATION_2026-10-15.md` for measured pre/post defect evidence.
3. Actual VST3 probes on built, freshly extracted ZIP and installed bundles pass.
   Compared with pre-change installed 1.4.0, only `plugin.version` differs. Both
   CIDs, 9 metadata parameters, 22 Set/Get cases and 9 state comparisons match.
4. ZIP contains exactly one `TB_XYZPanner.vst3` bundle with module, manifest and
   KEMAR attribution only. All three extracted file hashes match staged files.
5. Inno Setup installer built with the existing AppId. Actual silent installation
   exited 0; log reports success and no restart. Installed three file hashes
   equal the stage. Previous local correction build preserved under QA-root
   `previous-install/TB_XYZPanner.vst3` (binary hash
   `9F0E98436D028F4F6C96B6831DB0A5C160C0151AB61823BE98FF4E241EDB7ECD`).
6. Catalog semantic comparison preserves all other 34 products, XYZ macOS,
   portfolio/description/category/price and root contract. ZIP/installer hashes
   and exact sizes match catalog metadata. Git changes are limited to
   `Hub/TB_XYZPanner.zip`, `catalog.json` and README update notes.

QA root: `E:\Plugin Projects\output\release-qa\TB_XYZPanner-1.4.1-20261015`.
Contains build logs, spatial/editor logs, three host-contract reports, installer
source/log/EXE, stage, ZIP extraction, checksums and publication notes.

## Final artifacts

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| VST3 module | 7097344 | `603cf00c568f9f70e42f69ca1fc8073a3bee7d73ed14bd4c4151a9a6f812d705` |
| moduleinfo.json | 1158 | `66f62cf275c6ef0ab0be502984e8b2756cd6d4e855476a48daa8ca5da07463f6` |
| KEMAR_LICENSE.md | 2607 | `739e1142f8b336d673aa089bc89f1ba272dfc0d6b48fab66f21511b4f7cea5bc` |
| TB_XYZPanner.zip | 3288408 | `df54abc5b3b1bb5477cba0b1aee23bb6a7b913569ed620b7bd1802d7aea9bad8` |
| TB-XYZ-Panner-1.4.1-Windows-x64-VST3-Setup.exe | 4297521 | `0db60d85c83475e50f99f359e1d4931c34e7f50ae0629f7156d4ecb09e2f157d` |

## Remaining validation boundaries

Manual DAW listening, live desktop interaction, OS DPI transitions,
long-duration/multi-instance playback, whole-callback allocation profiling and
code signing were not run. Existing JUCE C4819 encoding warnings were retained;
no vendored module changes were made.

Generic KEMAR limitations and existing ordinary Stereo antiphase hard-pan
behaviour remain. The separate legacy preset UI/bank is not merged; this release
does not claim preset-save/load recovery. DAW parameter/state recall remains
supported and tested. TB Hub's elevated installer workflow was not rerun; actual
standalone installer success does not certify Hub UAC handling.

## Publication status

Published stable Windows release:
https://github.com/TaebeumKim/TB_Audio-Plugins/releases/tag/tb-xyz-panner-windows-v1.4.1

- Distribution commit: `3e90ca5f0e113906bf3a80356311ff5777a2e33d`.
- PR: https://github.com/TaebeumKim/TB_Audio-Plugins/pull/81, merged to main as
  `1553544c83dc443dafd2819097615a5d5b41c212`.
- Published 2026-10-15 10:56:42 +09:00. Draft=false, prerelease=false. Tag points
  to the distribution commit, which is an ancestor of main. The repository's
  existing global latest macOS installer release is unchanged.
- All three published assets were redownloaded; SHA-256 and byte sizes equal
  local final assets. Raw main catalog, Hub ZIP and icon were redownloaded;
  version 1.4.1, price 2, all catalog fields, ZIP hash/size and canonical logo
  hash match the reviewed result.
- A fresh extraction of the remotely downloaded Hub ZIP contains the same
  three bundle file hashes. Actual host loading, metadata, Set/Get and state
  recall pass and its JSON equals the locally built contract.
- Evidence: QA-root `release-redownload`, `main-redownload`,
  `remote-extracted` and `remote-host-contract.json`. Distribution worktree
  clean after publication. Local installed VST3 is the same 1.4.1 build.

Result: **CONDITIONAL PASS**. All executed automated correctness, editor,
build, compatibility, packaging, installer and publication checks pass. Manual
listening and the other explicitly unperformed checks above remain unclaimed.
